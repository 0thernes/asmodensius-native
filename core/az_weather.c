/* az_weather.c - suns, light, heat, wind, storms and vents. Parameters: params/az_weather.def.
 *
 * The world's gradients are made here. Nothing in this file is a cycle table:
 * the two suns turn by their periods, and everything else follows from the
 * state of the world and of one small chaotic system that the suns drive.
 *
 * az_weather_global (serial, every tick, and once from worldgen) writes the
 * words gstate[AZ_G_WEATHER ..]:
 *   - The suns. Each stands at (its start + tick / its period) of its day and
 *     gives the positive half of a sine as light; AZ_G_SEASON is how far the
 *     two are apart, their slow beat. Sun 0 crosses the sky along x, sun 1
 *     along y: the light vector (lvec) is the sum of where they stand, each
 *     weighted by its light, and nothing at night.
 *   - The convection of the sky: the three variables of Lorenz (1963), one
 *     Euler step per tick in Q16. Its Rayleigh number rises with the suns'
 *     light and falls at night, so the day's turbulence starts from wherever
 *     the calm of the night left it. The start is drawn once from the world's
 *     stream (lab: the seed; wild: its entropy). Its first variable is the
 *     cloud cover, which dims the sky; its third, the strength of the
 *     overturning, is the chance of a storm; its first two carry the storm.
 *   - The storm: calm, growing to a drawn peak, fading. A start is an event
 *     (AZ_EV_STORM: a = the cell under its centre, b = its peak).
 *   - The vents: each brings mineral (NUT_X) up from the buried reservoir at a
 *     steady rate with an eruption now and then, never more than the
 *     reservoir holds, and the mineral comes up hot. Its chemical energy
 *     and its heat are external input (energy_in).
 *
 * az_weather_cell (stepped cells, after chemistry) moves heat and writes the
 * derived columns temp, light, lvec, wind and charge of its cell:
 *   1. Light. A cell of the top layer gets the sky's light; a cell below gets
 *      what the cell above lets through (Beer-Lambert: the optical depths of
 *      what is in it add; solvent dims a little, organic matter and waste a
 *      lot, bedrock all). What a cell does not let through it absorbs as heat.
 *   2. Radiation (Budyko 1969; Sellers 1969: outgoing radiation linear in
 *      temperature). Above the sky's temperature a cell radiates a share of
 *      the excess away (energy_out); below it, it takes a share of the lack in
 *      (energy_in). A lifeless world settles where the light it absorbs and
 *      this loss meet: warm and swinging with the day at the top, cool and
 *      steady in the deep.
 *   3. Conduction to every colder face neighbour, a share of the temperature
 *      difference, an eighth of the cell's heat at most per step (when that
 *      binds, every side gets its share of it). Nothing into or out of
 *      bedrock.
 *   4. Wind: along each axis from the warm side to the cold (in z this is
 *      convection: up when it is warmer below), plus the swirl round the
 *      centre of a storm.
 *   5. Charge decays, and is raised by radiotoxin in the cell, by the sky in
 *      the top layer and by a storm overhead. Where a storm stands and the
 *      charge has reached WEATHER_LIGHTNING_AT, lightning fixes mineral: q
 *      GAS becomes q NUT_X and the cell pays the chemical energy of the
 *      mineral in heat, when that heat is there.
 *
 * Contracts others rely on:
 *   - Every flux is computed from committed amounts and capped. Weather never
 *     takes more than half of a cell's committed heat or gas in one step: the
 *     other half is chemistry's, which steps the same cell just before (it
 *     moves at most a quarter of the gas and, with the matter it moves, less
 *     than a quarter of the heat).
 *   - light is 0 in bedrock and never above 255; temp is az_temp_of.
 *   - Under AZ_LESION_WEATHER the tick calls neither hook: every column and
 *     word keeps the value it had (worldgen still derives them once).
 *   - A reader sees derived values up to field_stride - 1 ticks old; light
 *     goes down one layer per tick.
 */
#include "az_abi.h"

AZ_INLINE u32 wp(const AzWorld *w, u32 id) { return w->param[id]; }

/* this module's words in gstate[AZ_G_WEATHER ..], after the seven the ABI names */
enum { G_X = 7, G_Y, G_Z,           /* the convection model, Q16                       */
       G_CLOUD, G_SKY,              /* cloud cover 0..255; light of the top layer now  */
       G_STAGE, G_PEAK, G_CX, G_CY, /* storm: 0 calm, 1 growing, 2 fading; centre, Q8  */
       G_LX, G_LY, G_LZ,            /* where the light comes from, unit vector * 127   */
       G_BEGUN, G_STORMS };         /* the model has its start; storms so far          */

static u64 cell_matter(const AzWorld *w, u32 c)
{
    const u32 *sp = &w->c_sp[(size_t)c * AZ_NSPECIES];
    u64 m = 0;
    for (u32 k = 0; k < AZ_NSPECIES; k++) m += sp[k];
    return m;
}

/* share of the light that passes through cell c, of 65536 */
static u32 pass_q16(const AzWorld *w, u32 c)
{
    const u32 solid = w->c_solid[c];
    if (solid == 255u) return 0;
    const u32 *sp = &w->c_sp[(size_t)c * AZ_NSPECIES];
    const u64 org = (u64)sp[AZ_SP_ORG_Y] + sp[AZ_SP_WST_W];
    const u64 other = cell_matter(w, c) - org - sp[AZ_SP_SOLV];
    const u64 depth = ((u64)sp[AZ_SP_SOLV] * wp(w, AZ_WP_WEATHER_DIM_SOLV) + org * wp(w, AZ_WP_WEATHER_DIM_ORG)
                       + other * wp(w, AZ_WP_WEATHER_DIM_OTHER)) >> 16;      /* 1/256 halvings */
    if (depth >= 4096u) return 0;
    return (u32)(((u64)az_exp2_q8(4096u - (u32)depth) * (255u - solid)) / 255u);
}

/* the light that reaches cell c now: the sky's, or what the cell above lets through */
static u32 light_in(const AzWorld *w, u32 c)
{
    if (w->c_solid[c] == 255u) return 0;
    const u32 up = az_cell_nb(w, c, 0, 0, 1);
    if (up == AZ_NONE) return (u32)w->gstate[AZ_G_WEATHER + G_SKY];
    return (u32)(((u64)w->c_light[up] * pass_q16(w, up)) >> 16);
}

/* strength of the storm over cell c, 0..255: all of it at the centre, none at its reach or
 * under its depth; *ox, *oy: where the cell lies from the centre, cells */
static u32 storm_over(const AzWorld *w, u32 c, i32 *ox, i32 *oy)
{
    const i32 *g = &w->gstate[AZ_G_WEATHER];
    const u32 s = (u32)g[AZ_G_STORM];
    *ox = 0; *oy = 0;
    if (!s || (u64)az_cell_z(w, c) + wp(w, AZ_WP_WEATHER_STORM_DEPTH) < w->gz) return 0;
    i32 dx = (i32)az_cell_x(w, c) - (g[G_CX] >> 8), dy = (i32)az_cell_y(w, c) - (g[G_CY] >> 8);
    const i32 hx = (i32)(w->gx / 2u), hy = (i32)(w->gy / 2u);
    if (dx > hx) dx -= (i32)w->gx; else if (dx < -hx) dx += (i32)w->gx;
    if (dy > hy) dy -= (i32)w->gy; else if (dy < -hy) dy += (i32)w->gy;
    const u64 reach = wp(w, AZ_WP_WEATHER_STORM_RADIUS);
    const u64 r2 = reach * reach, d2 = (u64)((i64)dx * dx + (i64)dy * dy);
    if (d2 >= r2) return 0;
    *ox = dx; *oy = dy;
    return (u32)((s * (r2 - d2)) / r2);
}

void az_weather_derive(AzWorld *w, u32 c)
{
    const i32 *g = &w->gstate[AZ_G_WEATHER];
    const u32 l = light_in(w, c);
    i8 *lv = &w->c_lvec[(size_t)c * 4];
    w->c_temp[c] = (i16)az_temp_of(w->c_heat[c], cell_matter(w, c));
    w->c_light[c] = (u8)(l > 255u ? 255u : l);
    lv[0] = l ? (i8)g[G_LX] : 0; lv[1] = l ? (i8)g[G_LY] : 0; lv[2] = l ? (i8)g[G_LZ] : 0; lv[3] = 0;
}

void az_weather_cell(AzWorld *w, u32 c, u32 worker)
{
    i8 *wind = &w->c_wind[(size_t)c * 3];
    if (w->c_solid[c] == 255u) {                           /* bedrock: nothing flows in or out */
        wind[0] = 0; wind[1] = 0; wind[2] = 0;
        w->c_charge[c] = 0;
        az_weather_derive(w, c);
        return;
    }
    const i32 *g = &w->gstate[AZ_G_WEATHER];
    const u32 *sp = &w->c_sp[(size_t)c * AZ_NSPECIES];
    const u64 matter = cell_matter(w, c), m16 = matter + 16u;
    const u32 heat = w->c_heat[c];
    const i32 tc = az_temp_of(heat, matter);
    AzTally *ty = az_tally(w, worker);
    u32 left = heat / 2u;                                  /* weather's half of the heat in this step */

    /* 1. the light it does not let through becomes heat */
    const u64 gain = ((u64)light_in(w, c) * (65536u - pass_q16(w, c)) * wp(w, AZ_WP_WEATHER_SUN_HEAT)) >> 16;
    if (gain) {
        az_cell_post_heat(w, worker, c, (i32)gain);
        ty->energy_in += (i64)gain;
    }

    /* 2. radiation: to the sky above its temperature, from it below */
    const u64 base = ((u64)wp(w, AZ_WP_WEATHER_SPACE_TEMP) * m16) >> 10;
    if (heat > base) {
        u64 out = ((heat - base) * wp(w, AZ_WP_WEATHER_COOL)) >> 16;
        if (out > left) out = left;
        if (out) {
            az_cell_post_heat(w, worker, c, -(i32)out);
            ty->energy_out += (i64)out;
            left -= (u32)out;
        }
    } else if (heat < base) {
        u64 in = ((base - heat) * wp(w, AZ_WP_WEATHER_COOL)) >> 16;
        if (in > 0x3FFFFFFFu) in = 0x3FFFFFFFu;
        if (in) {
            az_cell_post_heat(w, worker, c, (i32)in);
            ty->energy_in += (i64)in;
        }
    }

    /* 3. conduction to the colder face neighbours; tn[]: the temperatures on the six sides
     * (-x, +x, -y, +y, -z, +z), its own where there is no way through */
    i32 tn[6];
    u32 nb[6];
    u64 flow[6], sum = 0;
    const u64 budget = heat / 8u < left ? heat / 8u : left;
    for (u32 f = 0; f < 6u; f++) {
        const i32 d = (f & 1u) ? 1 : -1;
        nb[f] = az_cell_nb(w, c, f < 2u ? d : 0, (f >> 1) == 1u ? d : 0, f >= 4u ? d : 0);
        const bool open = nb[f] != AZ_NONE && w->c_solid[nb[f]] != 255u;
        tn[f] = open ? (i32)w->c_temp[nb[f]] : tc;
        flow[f] = 0;
        if (tn[f] >= tc) continue;
        flow[f] = ((((u64)(u32)(tc - tn[f]) * m16) >> 10) * wp(w, AZ_WP_WEATHER_CONDUCT)) >> 8;
        if (flow[f] > budget) flow[f] = budget;
        sum += flow[f];
    }
    for (u32 f = 0; f < 6u; f++) {                         /* over the budget every side gets its share of it */
        const u64 d = sum > budget ? (flow[f] * budget) / sum : flow[f];
        if (!d) continue;
        az_cell_post_heat(w, worker, c, -(i32)d);
        az_cell_post_heat(w, worker, nb[f], (i32)d);
        left -= (u32)d;
    }

    /* 4. wind: from the warm side to the cold, and round the storm */
    i32 sx, sy;
    const u32 over = storm_over(w, c, &sx, &sy);
    i64 v[3];
    for (u32 a = 0; a < 3u; a++) v[a] = ((i64)(tn[2u * a] - tn[2u * a + 1u]) * (i64)wp(w, AZ_WP_WEATHER_WIND)) / 256;
    if (over) {
        const i64 swirl = (i64)over * (i64)wp(w, AZ_WP_WEATHER_STORM_WIND);
        v[0] -= ((i64)sy * swirl) / 256;
        v[1] += ((i64)sx * swirl) / 256;
    }
    for (u32 a = 0; a < 3u; a++) wind[a] = (i8)az_clamp_i64(v[a], -127, 127);

    /* 5. charge, and lightning under a storm */
    u32 ch = w->c_charge[c];
    ch -= (ch * wp(w, AZ_WP_WEATHER_CHARGE_DECAY) + 255u) >> 8;
    u64 add = ((u64)sp[AZ_SP_TOX_Z] * wp(w, AZ_WP_WEATHER_CHARGE_TOX)) >> 8;
    if (az_cell_nb(w, c, 0, 0, 1) == AZ_NONE) add += ((u64)(u32)g[G_SKY] * wp(w, AZ_WP_WEATHER_CHARGE_SUN)) >> 8;
    add += ((u64)over * wp(w, AZ_WP_WEATHER_CHARGE_STORM)) >> 8;
    ch = ch + add > 255u ? 255u : (u32)(ch + add);
    if (over && ch >= wp(w, AZ_WP_WEATHER_LIGHTNING_AT)) {
        const i64 eps = (i64)az_eps_species[AZ_SP_NUT_X] - (i64)az_eps_species[AZ_SP_GAS];
        const u64 cost = eps > 0 ? (u64)eps : 0u;          /* heat one unit of mineral takes */
        u64 q = ((u64)(sp[AZ_SP_GAS] / 2u) * wp(w, AZ_WP_WEATHER_LIGHTNING_FIX)) >> 8;
        if (cost && q * cost > left) q = left / cost;
        if (q) {
            az_cell_post(w, worker, c, AZ_SP_GAS, -(i32)q);
            az_cell_post(w, worker, c, AZ_SP_NUT_X, (i32)q);
            if (cost) az_cell_post_heat(w, worker, c, -(i32)(q * cost));
            const u32 drop = wp(w, AZ_WP_WEATHER_LIGHTNING_DROP);
            ch -= drop < ch ? drop : ch;
        }
    }
    w->c_charge[c] = (u8)ch;

    az_weather_derive(w, c);
}

void az_weather_global(AzWorld *w)
{
    i32 *g = &w->gstate[AZ_G_WEATHER];

    /* the suns: where each stands, the light it gives, where the light comes from */
    u32 raw = 0;
    i64 lv[3] = { 0, 0, 0 };
    for (u32 k = 0; k < 2u; k++) {
        const u32 per = w->sun_period[k];
        u32 ph = 0, li = 0;
        if (per) {
            ph = (wp(w, k ? AZ_WP_WEATHER_SUN1_START : AZ_WP_WEATHER_SUN0_START) + (u32)(((w->tick % per) << 16) / per)) & 0xFFFFu;
            const i32 s = az_sin_q14(ph);
            if (s > 0) {
                li = ((u32)s * wp(w, k ? AZ_WP_WEATHER_SUN1_LIGHT : AZ_WP_WEATHER_SUN0_LIGHT)) >> 14;
                lv[k] += (i64)az_cos_q14(ph) * (i64)li;
                lv[2] += (i64)s * (i64)li;
            }
        }
        g[AZ_G_SUN0_PHASE + k] = (i32)ph;
        g[AZ_G_SUN0_LIGHT + k] = (i32)li;
        raw += li;
    }
    if (raw > 255u) raw = 255u;
    g[AZ_G_SEASON] = (i32)(((u32)g[AZ_G_SUN0_PHASE] - (u32)g[AZ_G_SUN1_PHASE]) & 0xFFFFu);
    const i64 len = (i64)az_isqrt_u64((u64)(lv[0] * lv[0] + lv[1] * lv[1] + lv[2] * lv[2]));
    for (u32 a = 0; a < 3u; a++) g[G_LX + a] = len ? (i32)((lv[a] * 127) / len) : 0;

    /* the convection of the sky (Lorenz 1963): x' = sigma (y - x), y' = x (rho - z) - y,
     * z' = x y - beta z; rho follows the light */
    if (!g[G_BEGUN]) {
        u32 r[4];
        az_rand4(w, 0, AZ_RS_WEATHER, 2, r);
        g[G_X] = (i32)(r[0] & 0xFFFFFu) - 0x80000;         /* within 8 of rest */
        g[G_Y] = (i32)(r[1] & 0xFFFFFu) - 0x80000;
        g[G_Z] = (i32)(0x100000u + (r[2] & 0xFFFFFu));     /* 16 to 32 */
        g[G_BEGUN] = 1;
    }
    const i64 x = g[G_X], y = g[G_Y], z = g[G_Z];
    const i64 rho = ((i64)wp(w, AZ_WP_WEATHER_RHO) + (((i64)raw - 128) * (i64)wp(w, AZ_WP_WEATHER_RHO_SUN)) / 128) * 256;
    const i64 pace = wp(w, AZ_WP_WEATHER_PACE), lim = (i64)1 << 24;
    const i64 dx = ((i64)wp(w, AZ_WP_WEATHER_SIGMA) * (y - x)) / 256;
    const i64 dy = (x * (rho - z)) / 65536 - y;
    const i64 dz = (x * y) / 65536 - ((i64)wp(w, AZ_WP_WEATHER_BETA) * z) / 256;
    i64 nx = az_clamp_i64(x + (dx * pace) / 65536, -lim, lim);
    const i64 ny = az_clamp_i64(y + (dy * pace) / 65536, -lim, lim);
    const i64 nz = az_clamp_i64(z + (dz * pace) / 65536, -lim, lim);
    if (!nx && !ny) nx = 1;                                /* the axis of rest is a trap: step off it */
    g[G_X] = (i32)nx; g[G_Y] = (i32)ny; g[G_Z] = (i32)nz;
    const u32 cloud = (u32)az_clamp_i64(128 + (nx * (i64)wp(w, AZ_WP_WEATHER_CLOUD_GAIN)) / 65536, 0, 255);
    g[G_CLOUD] = (i32)cloud;
    g[G_SKY] = (i32)((raw * (256u - ((cloud * wp(w, AZ_WP_WEATHER_CLOUD_DIM)) >> 8))) >> 8);

    /* the storm */
    u32 s = (u32)g[AZ_G_STORM], stage = (u32)g[G_STAGE];
    i64 cx = g[G_CX], cy = g[G_CY];
    const i64 wx = (i64)w->gx << 8, wy = (i64)w->gy << 8;
    if (!stage) {
        const u64 p = (u64)wp(w, AZ_WP_WEATHER_STORM_RATE) * (u64)az_clamp_i64(nz / 65536, 0, 64);   /* of 2^24 */
        if (p && (u64)(az_rand(w, 0, AZ_RS_WEATHER, 0) >> 8) < p) {
            u32 r[4];
            az_rand4(w, 0, AZ_RS_WEATHER, 1, r);
            cx = (i64)(r[0] % (u32)wx); cy = (i64)(r[1] % (u32)wy);
            g[G_PEAK] = (i32)(1u + r[2] % 255u);
            stage = 1; s = 0;
            g[G_STORMS]++;
            az_emit(w, 0, AZ_EV_STORM, 0, az_cell_xyz(w, (u32)(cx >> 8), (u32)(cy >> 8), w->gz - 1u), (u64)g[G_PEAK], 1);
        }
    }
    if (stage == 1u) {
        s += wp(w, AZ_WP_WEATHER_STORM_RISE);
        if (s >= (u32)g[G_PEAK]) { s = (u32)g[G_PEAK]; stage = 2; }
    } else if (stage == 2u) {
        const u32 fade = wp(w, AZ_WP_WEATHER_STORM_FADE);
        s -= fade < s ? fade : s;
        if (!s) stage = 0;
    }
    if (stage) {                                           /* the convection carries it */
        const i64 drift = wp(w, AZ_WP_WEATHER_STORM_DRIFT);
        cx = (((cx + (nx * drift) / 1048576) % wx) + wx) % wx;
        cy = (((cy + (ny * drift) / 1048576) % wy) + wy) % wy;
    }
    g[AZ_G_STORM] = (i32)s; g[G_STAGE] = (i32)stage;
    g[G_CX] = (i32)cx; g[G_CY] = (i32)cy;
    g[AZ_G_STORM_CELL] = (i32)az_cell_xyz(w, (u32)(cx >> 8), (u32)(cy >> 8), w->gz - 1u);

    /* the vents */
    AzTally *ty = az_tally(w, 0);
    const u32 n_vents = (u32)(sizeof w->vent_cell / sizeof w->vent_cell[0]);
    for (u32 i = 0; i < w->vent_count && i < n_vents; i++) {
        const u32 c = w->vent_cell[i];
        if (c >= w->n_cells || w->c_solid[c] == 255u) continue;
        u64 q = wp(w, AZ_WP_WEATHER_VENT_RATE);
        if ((az_rand(w, (u64)i + 1u, AZ_RS_WEATHER, 3) & 0xFFFFu) < wp(w, AZ_WP_WEATHER_VENT_PULSE_P)) q += wp(w, AZ_WP_WEATHER_VENT_PULSE);
        const i64 have = (i64)w->totals.reservoir_mu + ty->reservoir;
        if (have <= 0) q = 0; else if (q > (u64)have) q = (u64)have;
        if (q) {
            ty->reservoir -= (i64)q;
            az_cell_post(w, 0, c, AZ_SP_NUT_X, (i32)q);
            ty->energy_in += (i64)az_eps_species[AZ_SP_NUT_X] * (i64)q;
        }
        const u64 warm = q * wp(w, AZ_WP_WEATHER_VENT_HEAT);   /* it comes up hot */
        /* The configured mineral pulse can carry several signed posts of
         * thermal energy. Credit exactly the amount that enters the cell. */
        ty->energy_in += (i64)warm;
        u64 remaining = warm;
        while (remaining) {
            i32 amount = (i32)(remaining > INT32_MAX ? INT32_MAX : remaining);
            az_cell_post_heat(w, 0, c, amount);
            remaining -= (u32)amount;
        }
    }
}
