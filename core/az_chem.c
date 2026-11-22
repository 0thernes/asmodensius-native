/* az_chem.c - reactions, burial, movement between cells, the fading of marks,
 * phases and the medium.
 * Parameters: params/az_chem.def.
 *
 * az_chem_cell (stepped cells, before weather). Everything is a post from
 * committed amounts. A bedrock cell is skipped.
 *   1. Reactions, each exact in matter and in energy (az_eps_species): what
 *      a reaction frees goes to the heat of the cell.
 *        slow oxidation   1 ORG_Y + 1 OXI -> 2 WST_W        (12 eu per unit)
 *        toxin decay      1 TOX_Z -> 1 WST_W                ( 2 eu)
 *        precipitation    1 NUT_X -> 1 STR_S                (24 eu)
 *      Oxidation runs at a rate that doubles with every CHEM_DOUBLE of
 *      temperature (an Arrhenius curve read as a power of two). The toxin
 *      decays at one rate whatever the temperature, as a radioactive thing
 *      does. Mineral settles out as structure in a cell that is hot, or
 *      stands beside a hot one: the chimneys that vents build.
 *      The amount that reacts is share x pool: its whole part, and one unit
 *      more with the chance of the rest, by a draw keyed by the cell and the
 *      tick (streams AZ_RS_CHEM and AZ_RS_DECAY), so that small pools still
 *      react, one unit at a time.
 *   2. Burial. In the lowest open cell of a column a small share of the
 *      waste, the organic matter and the structure goes into the buried
 *      reservoir (the tally); its chemical energy stays in the cell as heat.
 *      This closes the cycle the vents open (az_weather.c).
 *   3. Movement to the six face neighbours, push only (the neighbour gets a
 *      positive post, this cell the negative one). Nothing into bedrock.
 *        diffusion  to every neighbour that holds less of a species: a share
 *                   of the difference, by the phase the species has in this
 *                   cell at its temperature (CHEM_DIFFUSE_SOLID, _LIQUID,
 *                   _GAS; plasma moves as gas): gas spreads fast, grains
 *                   creep
 *        weight     by the cell's own gravity vector (cell column grav), on
 *                   each axis: what is solid here sinks with the pull
 *                   (CHEM_SETTLE per unit of gravity), what is gas or plasma
 *                   rises against it (CHEM_RISE), what is liquid is the
 *                   medium and only diffuses. Which species sinks and which
 *                   rises is nowhere written down: it follows from the
 *                   melting and boiling points and the temperature of the
 *                   place, so a hot cell lifts what a cold one drops.
 *      All flows of one species together take at most a quarter of what the
 *      cell holds of it; when that binds every side gets its share. A flow
 *      is its whole part and one unit more with the chance of the rest (one
 *      draw per species, stream AZ_RS_CHEM), so small differences still
 *      even out. Heat goes with the matter: moved x heat / (matter + 16) to
 *      the same neighbour, so the cell that gives keeps its temperature.
 *   4. Every mark of the cell fades: a share and one unit, in place.
 *   5. az_chem_derive.
 *
 * az_chem_derive writes the derived columns of one cell and posts nothing:
 *   phase   two bits per species, from the cell's temperature against the
 *           melting and boiling points of the species (world parameters);
 *           what is gas is plasma at CHEM_PLASMA_CHARGE of charge
 *   med     [0] density and [1] viscosity of the medium, 0..255, from how
 *           much of the cell is liquid, solid grains and gas, and its rock:
 *           a cell of solvent is dense and viscous, a cell of gas is thin,
 *           structure and detritus thicken it; bedrock is 255, 255
 *
 * Budgets, so that nothing can go below 0 whatever weather posts on the same
 * cell in the same pass: reactions take at most half of the organic matter,
 * the oxidiser, the toxin and the mineral of a cell in a step, burial a
 * quarter of what it buries, movement a quarter of every species and, with
 * it, less than a quarter of the heat (every reaction here frees heat).
 * Weather takes at most half of the heat and of the gas.
 *
 * Under AZ_LESION_CHEM the tick does not call az_chem_cell: amounts stay
 * where creatures and weather put them, nothing diffuses, sinks or rises,
 * marks stop fading, phase and med keep their values.
 * Under AZ_LESION_GRAVITY matter has no weight: nothing sinks or rises
 * (diffusion and everything else go on).
 */
#include "az_abi.h"

AZ_INLINE u32 wp(const AzWorld *w, u32 id) { return w->param[id]; }

_Static_assert(AZ_WP_CHEM_MELT_STR_S == AZ_WP_CHEM_MELT_SOLV + AZ_SP_STR_S &&
               AZ_WP_CHEM_MELT_ORG_Y == AZ_WP_CHEM_MELT_SOLV + AZ_SP_ORG_Y &&
               AZ_WP_CHEM_BOIL_STR_S == AZ_WP_CHEM_BOIL_SOLV + AZ_SP_STR_S &&
               AZ_WP_CHEM_BOIL_ORG_Y == AZ_WP_CHEM_BOIL_SOLV + AZ_SP_ORG_Y,
               "params/az_chem.def: the melting and boiling points stand in the order of the species");

static u64 cell_matter(const AzWorld *w, u32 c)
{
    const u32 *sp = &w->c_sp[(size_t)c * AZ_NSPECIES];
    u64 m = 0;
    for (u32 k = 0; k < AZ_NSPECIES; k++) m += sp[k];
    return m;
}

/* units of `pool` that react in this step at `share` of 65536, `factor` of 256: the whole
 * part, and one more with the chance of the rest; never more than `most` */
static u32 react(const AzWorld *w, u32 c, u32 stream, u32 idx, u64 pool, u64 share, u64 factor, u64 most)
{
    const u64 x = pool * share * factor;                   /* of 2^24 */
    u64 n = x >> 24;
    if ((x & 0xFFFFFFu) && (u64)(az_rand(w, c, stream, idx) >> 8) < (x & 0xFFFFFFu)) n++;
    return (u32)(n > most ? most : n);
}

/* rate of a reaction at this temperature, of 256 at the worldgen temperature */
static u32 pace_of(const AzWorld *w, i32 temp)
{
    const i64 e = 2048 + (((i64)temp - AZ_TEMP_OF_TOPT(128)) * 256) / (i64)wp(w, AZ_WP_CHEM_DOUBLE);
    return az_exp2_q8((u32)az_clamp_i64(e, 0, 4096));
}

/* the phase of species k at this temperature; what is gas is plasma where the charge is high */
static u32 phase_of(const AzWorld *w, u32 k, i32 temp, bool plasma)
{
    return temp < (i32)wp(w, AZ_WP_CHEM_MELT_SOLV + k) ? (u32)AZ_PH_SOLID
         : temp < (i32)wp(w, AZ_WP_CHEM_BOIL_SOLV + k) ? (u32)AZ_PH_LIQUID
         : plasma ? (u32)AZ_PH_PLASMA : (u32)AZ_PH_GAS;
}

/* Local reactions can move more than a signed post, even when the final stock fits u32.
 * Keep the net signed amount until it reaches the posting API. */
static void post_species_delta(AzWorld *w, u32 worker, u32 c, u32 species, i64 delta)
{
    while (delta > INT32_MAX) {
        az_cell_post(w, worker, c, species, INT32_MAX);
        delta -= INT32_MAX;
    }
    while (delta < INT32_MIN) {
        az_cell_post(w, worker, c, species, INT32_MIN);
        delta -= (i64)INT32_MIN;
    }
    az_cell_post(w, worker, c, species, (i32)delta);
}

/* Each of the eight u32 species gives at most a quarter. A face can thus
 * carry 33 bits of matter and its product with u32 heat can need 65 bits.
 * Split that product before the multiply; the remainder is below divisor
 * and heat is below divisor on the wide path (divisor >= 4*moved + 16). */
static u64 transported_heat(u64 moved, u32 heat, u64 divisor)
{
    if (moved <= UINT32_MAX) return moved * heat / divisor;
    const u64 half = (moved >> 1) * heat;
    const u64 tail = 2u * (half % divisor) + (moved & 1u) * heat;
    return 2u * (half / divisor) + (tail >= divisor) + (tail >= 2u * divisor);
}

/* movement to the six face neighbours (-x, +x, -y, +y, -z, +z): diffusion and weight. What
 * leaves the cell is taken from d[]; what a neighbour gets is posted here, with its heat. */
static void spread(AzWorld *w, u32 c, u32 worker, const u32 *sp, i32 temp, u64 matter, i64 d[AZ_NSPECIES])
{
    u32 nb[6];
    bool open[6], any = false;
    for (u32 f = 0; f < 6u; f++) {
        const i32 s = (f & 1u) ? 1 : -1;
        nb[f] = az_cell_nb(w, c, f < 2u ? s : 0, (f >> 1) == 1u ? s : 0, f >= 4u ? s : 0);
        open[f] = nb[f] != AZ_NONE && w->c_solid[nb[f]] != 255u;
        any = any || open[f];
    }
    if (!any) return;
    const bool plasma = w->c_charge[c] >= wp(w, AZ_WP_CHEM_PLASMA_CHARGE);
    const bool weight = !(w->cfg.lesion & AZ_LESION_GRAVITY);
    const i8 *g = &w->c_grav[(size_t)c * 4];
    u64 moved[6] = { 0, 0, 0, 0, 0, 0 };
    for (u32 k = 0; k < AZ_NSPECIES; k++) {
        const u32 a = sp[k];
        if (a < 4u) continue;                              /* a quarter of it is less than one unit */
        const u32 ph = phase_of(w, k, temp, plasma);
        const u64 rate = wp(w, ph == AZ_PH_SOLID ? AZ_WP_CHEM_DIFFUSE_SOLID
                             : ph == AZ_PH_LIQUID ? AZ_WP_CHEM_DIFFUSE_LIQUID : AZ_WP_CHEM_DIFFUSE_GAS);
        const u64 drift = (!weight || ph == AZ_PH_LIQUID) ? 0u
                        : wp(w, ph == AZ_PH_SOLID ? AZ_WP_CHEM_SETTLE : AZ_WP_CHEM_RISE);
        u64 x[6], sum = 0;                                 /* the flows, of 65536 */
        for (u32 f = 0; f < 6u; f++) {
            x[f] = 0;
            if (!open[f]) continue;
            const u32 b = w->c_sp[(size_t)nb[f] * AZ_NSPECIES + k];
            if (b < a) x[f] = (u64)(a - b) * rate;
            /* this side lies with the pull when the sign of gravity on its axis is its own;
             * a solid goes with the pull, a gas against it */
            const i32 gi = g[f >> 1];
            if (drift && gi && ((gi > 0) == ((f & 1u) != 0u)) == (ph == AZ_PH_SOLID))
                x[f] += (u64)a * (u64)(u32)(gi < 0 ? -gi : gi) * drift;
            sum += x[f];
        }
        if (!sum) continue;
        const u64 budget = (u64)(a / 4u) << 16;
        const u64 scale = sum > budget ? (budget << 16) / sum : 65536u;    /* of 65536 */
        u32 left = a / 4u, r[4] = { 0, 0, 0, 0 };
        bool drawn = false;
        for (u32 f = 0; f < 6u && left; f++) {
            if (!x[f]) continue;
            const u64 xf = scale < 65536u ? ((x[f] >> 8) * scale) >> 8 : x[f];
            u64 n = xf >> 16;
            const u32 rest = (u32)(xf & 0xFFFFu);
            if (rest) {                                    /* one unit more with the chance of the rest */
                if (!drawn) { az_rand4(w, c, AZ_RS_CHEM, 8u + k, r); drawn = true; }
                if (((r[f >> 1] >> (16u * (f & 1u))) & 0xFFFFu) < rest) n++;
            }
            if (n > left) n = left;
            if (!n) continue;
            left -= (u32)n;
            d[k] -= (i64)n;
            az_cell_post(w, worker, nb[f], k, (i32)n);
            moved[f] += n;
        }
    }
    const u32 heat = w->c_heat[c];
    for (u32 f = 0; f < 6u; f++) {                         /* heat goes with the matter */
        const u64 h = transported_heat(moved[f], heat, matter + 16u);
        if (!h) continue;
        az_cell_post_heat(w, worker, c, -(i32)h);
        az_cell_post_heat(w, worker, nb[f], (i32)h);
    }
}

void az_chem_derive(AzWorld *w, u32 c)
{
    u8 *med = &w->c_med[(size_t)c * 2];
    const u32 rock = w->c_solid[c];
    if (rock == 255u) {
        w->c_phase[c] = 0;
        med[0] = 255; med[1] = 255;
        return;
    }
    const u32 *sp = &w->c_sp[(size_t)c * AZ_NSPECIES];
    const i32 temp = az_temp_of(w->c_heat[c], cell_matter(w, c));
    const bool plasma = w->c_charge[c] >= wp(w, AZ_WP_CHEM_PLASMA_CHARGE);
    u64 of[3] = { 0, 0, 0 };                               /* solid, liquid, gas or plasma */
    u32 phase = 0;
    for (u32 k = 0; k < AZ_NSPECIES; k++) {
        const u32 ph = phase_of(w, k, temp, plasma);
        phase |= ph << (2u * k);
        of[ph == AZ_PH_SOLID ? 0 : ph == AZ_PH_LIQUID ? 1 : 2] += sp[k];
    }
    w->c_phase[c] = (u16)phase;
    const u64 dens = ((of[1] * wp(w, AZ_WP_CHEM_DENS_LIQUID) + of[0] * wp(w, AZ_WP_CHEM_DENS_SOLID) + of[2] * wp(w, AZ_WP_CHEM_DENS_GAS)) >> 16) + rock;
    const u64 visc = ((of[1] * wp(w, AZ_WP_CHEM_VISC_LIQUID) + of[0] * wp(w, AZ_WP_CHEM_VISC_SOLID) + of[2] * wp(w, AZ_WP_CHEM_VISC_GAS)) >> 16) + rock;
    med[0] = (u8)(dens > 255u ? 255u : dens);
    med[1] = (u8)(visc > 255u ? 255u : visc);
}

void az_chem_cell(AzWorld *w, u32 c, u32 worker)
{
    if (w->c_solid[c] == 255u) { az_chem_derive(w, c); return; }
    const u32 *sp = &w->c_sp[(size_t)c * AZ_NSPECIES];
    const u64 matter = cell_matter(w, c);
    const i32 temp = az_temp_of(w->c_heat[c], matter);
    const i32 *eps = az_eps_species;
    i64 d[AZ_NSPECIES] = { 0 };                            /* what this step changes in the cell */
    i64 warm = 0;                                          /* the heat it frees */

    /* slow oxidation: 1 ORG_Y + 1 OXI -> 2 WST_W */
    const i64 e_ox = (i64)eps[AZ_SP_ORG_Y] + eps[AZ_SP_OXI] - 2 * (i64)eps[AZ_SP_WST_W];
    const u32 fuel = sp[AZ_SP_ORG_Y] < sp[AZ_SP_OXI] ? sp[AZ_SP_ORG_Y] : sp[AZ_SP_OXI];
    if (fuel && e_ox >= 0) {
        const u32 n = react(w, c, AZ_RS_CHEM, 0, fuel, wp(w, AZ_WP_CHEM_OXIDATION), pace_of(w, temp), fuel / 2u);
        d[AZ_SP_ORG_Y] -= (i64)n; d[AZ_SP_OXI] -= (i64)n; d[AZ_SP_WST_W] += 2 * (i64)n;
        warm += e_ox * (i64)n;
    }

    /* toxin decay: 1 TOX_Z -> 1 WST_W */
    const i64 e_tox = (i64)eps[AZ_SP_TOX_Z] - eps[AZ_SP_WST_W];
    const u32 tox = sp[AZ_SP_TOX_Z];
    if (tox && e_tox >= 0) {
        const u32 n = react(w, c, AZ_RS_DECAY, 0, tox, wp(w, AZ_WP_CHEM_TOX_DECAY), 256u, tox / 2u + (tox & 1u));
        d[AZ_SP_TOX_Z] -= (i64)n; d[AZ_SP_WST_W] += (i64)n;
        warm += e_tox * (i64)n;
    }

    /* precipitation: 1 NUT_X -> 1 STR_S in a hot cell or beside one */
    const i64 e_pr = (i64)eps[AZ_SP_NUT_X] - eps[AZ_SP_STR_S];
    const u32 nut = sp[AZ_SP_NUT_X];
    if (nut && e_pr >= 0) {
        const i32 hot_at = (i32)wp(w, AZ_WP_CHEM_PRECIP_TEMP);
        bool hot = temp >= hot_at;
        for (u32 f = 0; f < 6u && !hot; f++) {
            const i32 s = (f & 1u) ? 1 : -1;
            const u32 n = az_cell_nb(w, c, f < 2u ? s : 0, (f >> 1) == 1u ? s : 0, f >= 4u ? s : 0);
            hot = n != AZ_NONE && w->c_solid[n] != 255u && w->c_temp[n] >= hot_at;
        }
        if (hot) {
            const u32 n = react(w, c, AZ_RS_CHEM, 1, nut, wp(w, AZ_WP_CHEM_PRECIP), 256u, nut / 2u);
            d[AZ_SP_NUT_X] -= (i64)n; d[AZ_SP_STR_S] += (i64)n;
            warm += e_pr * (i64)n;
        }
    }

    /* burial from the lowest open cell of the column */
    const u32 below = az_cell_nb(w, c, 0, 0, -1);
    if (below == AZ_NONE || w->c_solid[below] == 255u) {
        const u32 sinks[3] = { AZ_SP_WST_W, AZ_SP_ORG_Y, AZ_SP_STR_S };
        for (u32 i = 0; i < 3u; i++) {
            const u32 k = sinks[i];
            const u32 q = react(w, c, AZ_RS_CHEM, 2u + i, sp[k], wp(w, AZ_WP_CHEM_BURIAL), 256u, sp[k] / 4u);
            if (!q || eps[k] < 0) continue;
            d[k] -= (i64)q;
            az_tally(w, worker)->reservoir += (i64)q;
            warm += (i64)eps[k] * (i64)q;
        }
    }

    /* diffusion, sinking and rising */
    spread(w, c, worker, sp, temp, matter, d);

    for (u32 k = 0; k < AZ_NSPECIES; k++)
        if (d[k]) post_species_delta(w, worker, c, k, d[k]);
    for (; warm > 0; warm -= warm < 0x40000000 ? warm : 0x40000000)
        az_cell_post_heat(w, worker, c, (i32)(warm < 0x40000000 ? warm : 0x40000000));

    /* the marks fade */
    u8 *mark = &w->c_mark[(size_t)c * AZ_NMARK];
    for (u32 ch = 0; ch < AZ_NMARK; ch++) {
        const u32 m = mark[ch], lose = ((m * wp(w, AZ_WP_CHEM_MARK_DECAY)) >> 8) + 1u;
        mark[ch] = (u8)(m > lose ? m - lose : 0u);
    }

    az_chem_derive(w, c);
}
