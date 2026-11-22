/* az_worldgen.c - the alien world at tick 0 and its founder species.
 *
 * What it makes, in one serial pass (build/work/w2/worldgen/DESIGN.md):
 *   - terrain: a height field of integer value noise (Perlin 1985, value-noise
 *     form: a periodic lattice per octave, smoothstep interpolation, octaves
 *     summed with a persistence), so bedrock (solid 255) rises from z = 0 to a
 *     height that varies continuously in x and y; the surface cell above it is
 *     partly rock;
 *   - layers along z: a solvent ocean up to a sea level set by the mean terrain,
 *     carrier gas above it; every cell amount scattered around its layer value
 *     (patchiness), organic matter on the sea floor, waste dissolved in the
 *     ocean, a little of each medium in the other;
 *   - local features: vents on the sea floor with reduced mineral around them
 *     and the buried reservoir they will bring up, radiotoxin hot spots, reef
 *     structure on the floor;
 *   - heat from a target temperature field: warm surface, cold deep, warm
 *     equator of y, cold poles, hot vents (the plume of the nearest vent);
 *     heat = temperature * matter;
 *   - two suns whose days are made coprime, so their beat is long;
 *   - gravity as a field that bends: down everywhere, stronger over high
 *     terrain, leaning toward high ground (the mass excess of mountains) and
 *     fading with height, plus a low-order swirl the terrain does not explain;
 *     w->gravity is its world mean;
 *   - founder species 1..100 on a niche table: the guilds of chemosynthetic and
 *     photic-zone communities (producer on light, chemotroph on vent mineral,
 *     grazer and decomposer, scavenger, predator), each species with its own
 *     habitat band by depth, mating mode, size class, social style and life
 *     history (Hutchinson niche differentiation: species differ along several
 *     axes at once). A niche is a set of trait targets fitted into the species'
 *     genome (az_genome_fit), never behaviour code; founders start in habitat
 *     patches and differ from their species' prototype at a few loci. Each
 *     world draws its own mix of strategies (the share of egg layers, of few
 *     well-provisioned young, of grazers in mid water, the weight of each
 *     hunting band). The habitat targets cover every cell a founder may be
 *     placed in (the deepest pressure, the densest or lightest medium, the
 *     coldest and warmest cell), and the fit is repeated with the drawn rest and
 *     then style let go until the guild, the mode and the habitat hold. A
 *     founder holds what its body can hold (metab's PART_HOLD rule), no more.
 * Every number that is not a law of az_abi.h is a WORLDGEN_* world parameter
 * (core/params/az_worldgen.def). Every draw is az_rand / az_rand4 /
 * az_rand_below on AZ_RS_WORLDGEN (the idx labels WG_I_* below are this
 * module's own), or a draw of a function it calls (az_found, az_clade_found,
 * az_genetic_code_init, az_weather_global, each on its own stream). cfg.seed
 * and cfg.mode are never read: a lab world is a function of its seed, a wild
 * world of its entropy. Worldgen posts nothing: it writes amounts and heat
 * directly at tick 0, and holds vent_count at 0 during the one weather call so
 * the vents do not post either; the books after worldgen are the baseline.
 * Honest limits:
 *   - a founder's body parts follow its species' base genome (they are built
 *     by az_found before the genome is fitted and copied; no ABI call rebuilds
 *     them). The parts start empty, so the books are not affected;
 *   - the tick-0 field does not last: weather heats a cell by the light it
 *     traps (rock traps it, az_weather.c) and cools it in proportion to its
 *     matter, and rock is not matter, so a rocky sea floor under the nearly
 *     clear ocean bakes (at SURFACE_SOLID 160 the floor reached about 20000 and
 *     45 percent of the ocean boiled within 100 ticks; at the default 16 the
 *     ocean stays liquid but for a few cells that crowds of founders heat);
 *   - the structure is the brief's: the ocean always lies under the air, the
 *     deep is never warmer than the surface nor the poles than the equator
 *     (TEMP_DEPTH and TEMP_POLE start at 0), gravity always points down -z,
 *     producers live in the lit band and chemotrophs at vents. Shape numbers
 *     that stay fixed: each noise octave doubles the lattice (a periodic
 *     lattice on a power-of-two grid), the pull of the rock fades linearly to
 *     the top of the grid, and the pool scales with (SIZE + 16) / 144;
 *   - only predators may hunt in the air, and some worlds give that band no
 *     weight worth a species: with 16 species 4 of 20 worlds (seeds 1-20) start
 *     with the gas layer empty.
 */
#include "az_abi.h"

/* idx labels of stream AZ_RS_WORLDGEN (az_abi.h: a stream's owner decides what idx means) */
enum {
    WG_I_TERRAIN = 0x01, WG_I_SWIRL_X = 0x02, WG_I_SWIRL_Y = 0x03,
    WG_I_AMOUNT = 0x10,
    WG_I_VENT = 0x20, WG_I_TOX = 0x21, WG_I_REEF = 0x22,
    WG_I_NICHE_ROT = 0x30, WG_I_NICHE = 0x31,
    WG_I_PLACE = 0x40, WG_I_PLACE_SUB = 0x41,
    WG_I_PATCH = 0x100,    /* + patch number (< 64)  */
    WG_I_VARY = 0x200      /* + varied locus (< 76)  */
};

/* species and clade seed label base (the stub's), and the ABI's species range */
#define WG_SEED_BASE   0x10000u
#define WG_MAX_SPECIES 100u
#define WG_VENTS_MAX   ((u32)(sizeof(((AzWorld *)0)->vent_cell) / sizeof(u32)))
#define WG_TEMP_MAX    32767        /* the range of az_temp_of */
#define WG_AMOUNT_MAX  0x7FFFFFFFu  /* a cell amount stays inside the i32 range of pending deltas */
#define WG_EDGE        (1u << 16)   /* physics keeps a body this far off the z bounds */
#define WG_MODES       (AZ_MM_EUSOC + 1u)
#define WG_NEUTRAL     128u         /* the neutral trait value */
#define WG_Q16         65536u

#define WP(name) (w->param[AZ_WP_WORLDGEN_##name])

/* guilds and habitat bands */
enum { WG_G_PRODUCER = 0, WG_G_CHEMO, WG_G_GRAZER, WG_G_SCAV, WG_G_PRED, WG_GUILDS };
enum { WG_B_FLOOR = 0, WG_B_PHOTIC, WG_B_MID, WG_B_AIR, WG_B_VENTFLOOR };

typedef struct WgCtx {
    u32 gx, gy, gz, ncol, plane_bits;
    u32 S;          /* sea level: ocean is z < S                 */
    u32 Hmin, Dmax; /* lowest terrain; deepest ocean (S - Hmin)  */
    u32 V;          /* vents                                     */
    u32 n_ocean;    /* ocean columns                             */
    u32 reach;
    u32 nsp;
    u32 og, om;     /* niche rotation: guild and mode offsets    */
    /* this world's mix of strategies, drawn once per world (of 256): the share of species that
     * lay eggs, that raise few well-provisioned young, of grazers in mid water; and the weight
     * of each hunting band of the predators (air, lit band, mid water, floor) */
    u32 egg_share, k_share, mid_share, pred_w[4], pred_sum;
    bool has_mid;   /* some column has a mid-water band          */
} WgCtx;

typedef struct WgNiche { u32 guild, mode, band, sc, ss, egg, kstrat, pattern; } WgNiche;

static u32 wg_below(u32 r, u32 n) { return (u32)(((u64)r * n) >> 32); }

/* a clamp that changed a value: counted straight into the totals. Worldgen is serial and runs
 * before tick 1, so no tally is merged after it; a worker tally left non-empty would read as
 * "inside a tick" and a snapshot of the new world would be refused (az_snapshot.c) */
static void wg_sat(AzWorld *w) { w->totals.sat[AZ_SAT_FIELD]++; }

static u32 wg_add(AzWorld *w, u32 a, u64 b)
{
    const u64 s = (u64)a + b;
    if (s > WG_AMOUNT_MAX) { wg_sat(w); return WG_AMOUNT_MAX; }
    return (u32)s;
}

/* ---- 2.1 value noise ------------------------------------------------------ */

static u32 wg_smooth(u32 f) { return (u32)(((u64)f * f * (3u * WG_Q16 - 2u * f)) >> 32); }

static u32 wg_noise(const AzWorld *w, u32 x, u32 y, u32 octaves, u32 persist, u32 idx)
{
    u64 sum = 0, asum = 0, amp = WG_Q16;
    for (u32 o = 0; o < octaves; o++) {
        const u32 Lx = (2u << o) < w->gx ? (2u << o) : w->gx;
        const u32 Ly = (2u << o) < w->gy ? (2u << o) : w->gy;
        const u32 sx = w->gx / Lx, sy = w->gy / Ly;
        const u32 i0 = x / sx, i1 = (i0 + 1u) % Lx, j0 = y / sy, j1 = (j0 + 1u) % Ly;
        const u64 fx = wg_smooth(((x % sx) << 16) / sx), fy = wg_smooth(((y % sy) << 16) / sy);
        const u64 ob = (u64)o << 32;
        const u64 v00 = az_rand(w, ob | ((u64)j0 << 12) | i0, AZ_RS_WORLDGEN, idx) >> 16;
        const u64 v10 = az_rand(w, ob | ((u64)j0 << 12) | i1, AZ_RS_WORLDGEN, idx) >> 16;
        const u64 v01 = az_rand(w, ob | ((u64)j1 << 12) | i0, AZ_RS_WORLDGEN, idx) >> 16;
        const u64 v11 = az_rand(w, ob | ((u64)j1 << 12) | i1, AZ_RS_WORLDGEN, idx) >> 16;
        const u64 a = (v00 * (WG_Q16 - fx) + v10 * fx) >> 16;
        const u64 b = (v01 * (WG_Q16 - fx) + v11 * fx) >> 16;
        const u64 val = (a * (WG_Q16 - fy) + b * fy) >> 16;
        sum += amp * val;
        asum += amp;
        amp = (amp * persist) >> 8;
    }
    return (u32)(sum / asum);
}

/* ---- geometry ------------------------------------------------------------- */

static u32 wg_col_of(const WgCtx *x, u32 c) { return c & (x->ncol - 1u); }
static u32 wg_cell(const WgCtx *x, u32 col, u32 z) { return (z << x->plane_bits) | col; }

/* bedrock count of a column: bedrock is contiguous from z = 0 and the top cell is open */
static u32 wg_col_H(const AzWorld *w, const WgCtx *x, u32 col)
{
    u32 lo = 0, hi = x->gz - 1u;
    while (lo < hi) {
        const u32 mid = (lo + hi) >> 1;
        if (w->c_solid[wg_cell(x, col, mid)] == 255u) lo = mid + 1u; else hi = mid;
    }
    return lo;
}

/* the offsets of one wrapping axis within reach r, each wrapped cell once */
static void wg_axis(u32 r, u32 g, i32 *lo, i32 *hi)
{
    if (2u * r + 1u > g) { *lo = -(i32)((g - 1u) / 2u); *hi = (i32)(g / 2u); }
    else { *lo = -(i32)r; *hi = (i32)r; }
}

/* wrapped distance on an axis of size g */
static u32 wg_wdist(u32 a, u32 b, u32 g)
{
    const u32 d = a > b ? a - b : b - a;
    return d < g - d ? d : g - d;
}

/* add amount * fall(d) of species k to every open cell within reach of cell `centre` */
static void wg_spread(AzWorld *w, const WgCtx *x, u32 centre, u32 k, u32 amount)
{
    const u32 R = x->reach;
    const u32 cx = az_cell_x(w, centre), cy = az_cell_y(w, centre), cz = az_cell_z(w, centre);
    i32 xlo, xhi, ylo, yhi;
    wg_axis(R, x->gx, &xlo, &xhi);
    wg_axis(R, x->gy, &ylo, &yhi);
    for (i32 dz = -(i32)R; dz <= (i32)R; dz++) {
        const i32 z = (i32)cz + dz;
        if (z < 0 || z >= (i32)x->gz) continue;
        for (i32 dy = ylo; dy <= yhi; dy++)
            for (i32 dx = xlo; dx <= xhi; dx++) {
                const u32 c = az_cell_xyz(w, (cx + (u32)dx) & (x->gx - 1u), (cy + (u32)dy) & (x->gy - 1u), (u32)z);
                if (w->c_solid[c] == 255u) continue;
                u32 d = (u32)az_abs_i32(dx);
                if ((u32)az_abs_i32(dy) > d) d = (u32)az_abs_i32(dy);
                if ((u32)az_abs_i32(dz) > d) d = (u32)az_abs_i32(dz);
                u32 *sp = &w->c_sp[(size_t)c * AZ_NSPECIES + k];
                *sp = wg_add(w, *sp, ((u64)amount * (R + 1u - d)) / (R + 1u));
            }
    }
}

/* ---- 2.2, 2.3 terrain and sea level ---------------------------------------- */

static u32 wg_hrel(const AzWorld *w, const WgCtx *x)
{
    const u32 hcap = x->gz >= 3u ? x->gz - 2u : 0u;
    const u32 hrel = (u32)(((u64)x->gz * WP(RELIEF)) >> 8);
    return hrel < hcap ? hrel : hcap;
}

static void wg_terrain(AzWorld *w, WgCtx *x)
{
    const u32 hrel = wg_hrel(w, x), oct = WP(OCTAVES), per = WP(PERSIST);
    u64 hsum = 0;
    u32 hmin = 0xFFFFFFFFu;
    for (u32 col = 0; col < x->ncol; col++) {
        const u32 n = wg_noise(w, col & (x->gx - 1u), col >> w->gx_bits, oct, per, WG_I_TERRAIN);
        w->tmp_u32[col] = n;
        const u32 H = (u32)(((u64)n * (hrel + 1u)) >> 16);
        hsum += H;
        if (H < hmin) hmin = H;
    }
    const u32 hm = (u32)(hsum / x->ncol);
    u32 S = hm + (u32)((((u64)(x->gz - hm)) * WP(SEA_LEVEL) + 128u) >> 8);
    if (S < hmin + 1u) S = hmin + 1u;
    if (S > x->gz - 1u) S = x->gz - 1u;
    x->S = S;
    x->Hmin = hmin;
    x->Dmax = S - hmin;
}

/* ---- 2.4 layers and amounts -------------------------------------------------- */

static void wg_layers(AzWorld *w, WgCtx *x)
{
    const u32 hrel = wg_hrel(w, x), surf = WP(SURFACE_SOLID), dis = WP(DISSOLVE), patch = WP(PATCHINESS);
    const u32 ocean_solv = WP(OCEAN_SOLV);
    const u32 ocean_oxi = WP(OCEAN_OXI), ocean_wst = WP(OCEAN_WST), floor_org = WP(FLOOR_ORG);
    const u32 air_gas = WP(AIR_GAS), air_oxi = WP(AIR_OXI);
    x->n_ocean = 0;
    for (u32 col = 0; col < x->ncol; col++) {
        const u64 t = (u64)w->tmp_u32[col] * (hrel + 1u);
        const u32 H = (u32)(t >> 16), rem = (u32)(t & 0xFFFFu);
        if (H < x->S) x->n_ocean++;
        for (u32 z = 0; z < x->gz; z++) {
            const u32 c = wg_cell(x, col, z);
            u32 *sp = &w->c_sp[(size_t)c * AZ_NSPECIES];
            for (u32 k = 0; k < AZ_NSPECIES; k++) sp[k] = 0;
            w->c_heat[c] = 0;
            if (z < H) { w->c_solid[c] = 255; continue; }
            const u32 solid = z == H ? (u32)(((u64)rem * surf) >> 16) : 0u;
            w->c_solid[c] = (u8)solid;
            u64 a[AZ_NSPECIES] = { 0, 0, 0, 0, 0, 0, 0, 0 };
            if (z < x->S) {                                    /* ocean: the liquid medium */
                a[AZ_SP_SOLV] = ocean_solv;
                a[AZ_SP_GAS] = ((u64)air_gas * dis) >> 8;
                a[AZ_SP_OXI] = ocean_oxi;
                a[AZ_SP_ORG_Y] = z == H ? (u64)floor_org : ((u64)floor_org * dis) >> 8;
                a[AZ_SP_WST_W] = ocean_wst;
            } else {                                           /* air: the carrier gas */
                a[AZ_SP_SOLV] = ((u64)ocean_solv * dis) >> 8;
                a[AZ_SP_GAS] = air_gas;
                a[AZ_SP_OXI] = air_oxi;
            }
            u32 r[4];
            az_rand4(w, c, AZ_RS_WORLDGEN, WG_I_AMOUNT, r);
            for (u32 k = 0; k < AZ_NSPECIES; k++) {
                const i32 b = (i32)((r[k >> 2] >> (8u * (k & 3u))) & 255u);
                const i32 f = 256 + ((b - 128) * (i32)patch) / 128;
                u64 v = (a[k] * (u64)f) >> 8;
                if (solid) v = (v * (255u - solid)) / 255u;      /* rock takes room */
                sp[k] = wg_add(w, 0, v);
            }
        }
    }
}

/* ---- 2.5 vents, 2.4.3 toxin spots and reefs ------------------------------- */

static void wg_vents(AzWorld *w, WgCtx *x)
{
    u64 V = ((u64)WP(VENTS) * x->n_ocean + 1023u) / 1024u;
    const u32 vmax = x->n_ocean < WG_VENTS_MAX ? x->n_ocean : WG_VENTS_MAX;
    if (V < 1u) V = 1u;
    if (V > vmax) V = vmax;
    x->V = (u32)V;
    for (u32 v = 0; v < WG_VENTS_MAX; v++) w->vent_cell[v] = 0;
    for (u32 v = 0; v < x->V; v++) {
        u32 col = az_rand_below(w, v, AZ_RS_WORLDGEN, WG_I_VENT, x->ncol);
        for (u32 step = 0; step < x->ncol; step++, col = (col + 1u) % x->ncol) {
            const u32 H = wg_col_H(w, x, col);
            if (H >= x->S) continue;
            bool taken = false;
            for (u32 u = 0; u < v; u++) taken = taken || wg_col_of(x, w->vent_cell[u]) == col;
            if (taken) continue;
            w->vent_cell[v] = wg_cell(x, col, H);
            break;
        }
    }
    w->totals.reservoir_mu = (u64)WP(RESERVOIR) * x->V;
    const u32 nut = WP(VENT_NUT);
    for (u32 v = 0; v < x->V; v++) wg_spread(w, x, w->vent_cell[v], AZ_SP_NUT_X, nut);
}

static void wg_spots_reefs(AzWorld *w, WgCtx *x)
{
    const u32 spots = (u32)(((u64)WP(TOX_SPOTS) * x->ncol + 1023u) / 1024u);
    const u32 tox = WP(TOX_AMOUNT);
    for (u32 t = 0; t < spots; t++) {
        u32 r[4];
        az_rand4(w, t, AZ_RS_WORLDGEN, WG_I_TOX, r);
        const u32 col = wg_below(r[0], x->ncol);
        const u32 H = wg_col_H(w, x, col);
        wg_spread(w, x, wg_cell(x, col, H + wg_below(r[1], x->gz - H)), AZ_SP_TOX_Z, tox);
    }
    const u32 sites = (u32)(((u64)WP(REEF_SITES) * x->n_ocean + 1023u) / 1024u);
    const u32 str = WP(REEF_STR), R = x->reach;
    i32 xlo, xhi, ylo, yhi;
    wg_axis(R, x->gx, &xlo, &xhi);
    wg_axis(R, x->gy, &ylo, &yhi);
    for (u32 s = 0; s < sites; s++) {
        u32 col = wg_below(az_rand(w, s, AZ_RS_WORLDGEN, WG_I_REEF), x->ncol);
        for (u32 step = 0; step < x->ncol && wg_col_H(w, x, col) >= x->S; step++) col = (col + 1u) % x->ncol;
        const u32 cx = col & (x->gx - 1u), cy = col >> w->gx_bits;
        for (i32 dy = ylo; dy <= yhi; dy++)
            for (i32 dx = xlo; dx <= xhi; dx++) {
                const u32 nc = (((cy + (u32)dy) & (x->gy - 1u)) << w->gx_bits) | ((cx + (u32)dx) & (x->gx - 1u));
                const u32 H = wg_col_H(w, x, nc);
                if (H >= x->S) continue;                         /* floors of ocean columns only */
                u32 d = (u32)az_abs_i32(dx);
                if ((u32)az_abs_i32(dy) > d) d = (u32)az_abs_i32(dy);
                u32 *sp = &w->c_sp[(size_t)wg_cell(x, nc, H) * AZ_NSPECIES + AZ_SP_STR_S];
                *sp = wg_add(w, *sp, ((u64)str * (R + 1u - d)) / (R + 1u));
            }
    }
}

/* ---- 2.6 heat --------------------------------------------------------------- */

/* the target temperature of an open cell, before the clamp */
static i64 wg_target_temp(const AzWorld *w, const WgCtx *x, u32 c)
{
    const u32 y = az_cell_y(w, c), z = az_cell_z(w, c);
    const i64 mid = WP(TEMP_MID), depth = WP(TEMP_DEPTH), pole = WP(TEMP_POLE);
    i64 T;
    if (z < x->S) {
        const i64 d = (i64)(x->S - 1u - z), span = (i64)x->Dmax - 1;
        T = mid + (depth * (span - 2 * d)) / (2 * (span > 1 ? span : 1));
    } else {
        T = mid + depth / 2;
    }
    const i64 ey = az_abs_i32(2 * (i32)y + 1 - (i32)x->gy);
    T += (pole * ((i64)x->gy - 1 - 2 * ey)) / (2 * (i64)x->gy);
    /* the warmth of the nearest vent's plume: plumes of vents that lie close together do not
     * add up (summed, clustered vents boiled the ocean around them at tick 0) */
    const u32 cx = az_cell_x(w, c), R = x->reach;
    const i64 vent = WP(TEMP_VENT);
    i64 plume = 0;
    for (u32 v = 0; v < x->V; v++) {
        const u32 vc = w->vent_cell[v];
        const u32 vz = az_cell_z(w, vc), dz = vz > z ? vz - z : z - vz;
        if (dz > R) continue;
        u32 d = dz;
        const u32 ddx = wg_wdist(cx, az_cell_x(w, vc), x->gx), ddy = wg_wdist(y, az_cell_y(w, vc), x->gy);
        if (ddx > d) d = ddx;
        if (ddy > d) d = ddy;
        if (d <= R) {
            const i64 p = (vent * (i64)(R + 1u - d)) / (i64)(R + 1u);
            if (p > plume) plume = p;
        }
    }
    return T + plume;
}

static void wg_heat(AzWorld *w, const WgCtx *x)
{
    for (u32 c = 0; c < w->n_cells; c++) {
        if (w->c_solid[c] == 255u) continue;
        i64 T = wg_target_temp(w, x, c);
        if (T < 0) { T = 0; wg_sat(w); }
        if (T > WG_TEMP_MAX) { T = WG_TEMP_MAX; wg_sat(w); }
        const u32 *sp = &w->c_sp[(size_t)c * AZ_NSPECIES];
        u64 m = 0;
        for (u32 k = 0; k < AZ_NSPECIES; k++) m += sp[k];
        u64 heat = ((u64)T * (m + 16u)) >> 10;
        if (heat > 0xFFFFFFFFull) { heat = 0xFFFFFFFFull; wg_sat(w); }
        w->c_heat[c] = (u32)heat;
    }
}

/* ---- 2.8 gravity -------------------------------------------------------------- */

static i32 wg_clamp_count(AzWorld *w, i32 v, i32 lo, i32 hi)
{
    if (v < lo) { wg_sat(w); return lo; }
    if (v > hi) { wg_sat(w); return hi; }
    return v;
}

static void wg_gravity(AzWorld *w, const WgCtx *x)
{
    const i32 bend = (i32)WP(GRAV_BEND), swirl = (i32)WP(GRAV_SWIRL), grav = (i32)WP(GRAV);
    u64 down = 0;
    for (u32 col = 0; col < x->ncol; col++) {
        const u32 cx = col & (x->gx - 1u), cy = col >> w->gx_bits;
        const u32 xm = (cx + x->gx - 1u) & (x->gx - 1u), xp = (cx + 1u) & (x->gx - 1u);
        const u32 ym = (cy + x->gy - 1u) & (x->gy - 1u), yp = (cy + 1u) & (x->gy - 1u);
        const i32 n = (i32)w->tmp_u32[col];
        const i32 sx = (i32)w->tmp_u32[(cy << w->gx_bits) | xp] - (i32)w->tmp_u32[(cy << w->gx_bits) | xm];
        const i32 sy = (i32)w->tmp_u32[(yp << w->gx_bits) | cx] - (i32)w->tmp_u32[(ym << w->gx_bits) | cx];
        const i32 wx = ((i32)wg_noise(w, cx, cy, 1, 0, WG_I_SWIRL_X) - 32768) * swirl / 32768;
        const i32 wy = ((i32)wg_noise(w, cx, cy, 1, 0, WG_I_SWIRL_Y) - 32768) * swirl / 32768;
        const u32 H = wg_col_H(w, x, col);
        for (u32 z = 0; z < x->gz; z++) {
            const u32 c = wg_cell(x, col, z);
            const u32 za = z > H ? z - H : 0u;
            const i32 f = 256 - (i32)((256u * za) / x->gz);   /* the pull of the rock mass fades with height */
            i32 g0 = (sx * bend / 65536) * f / 256 + wx;
            i32 g1 = (sy * bend / 65536) * f / 256 + wy;
            i32 g2 = -(grav + ((n - 32768) * bend / 65536) * f / 256);
            g0 = wg_clamp_count(w, g0, -127, 127);
            g1 = wg_clamp_count(w, g1, -127, 127);
            g2 = wg_clamp_count(w, g2, -127, -1);
            i8 *g = &w->c_grav[(size_t)c * 4];
            g[0] = (i8)g0; g[1] = (i8)g1; g[2] = (i8)g2; g[3] = 0;
            down += (u64)(-g2);
        }
    }
    w->gravity = (u16)(down / w->n_cells);
}

/* ---- 2.7 suns ----------------------------------------------------------------- */

static u32 wg_gcd(u32 a, u32 b)
{
    while (b) { const u32 t = a % b; a = b; b = t; }
    return a;
}

static void wg_suns(AzWorld *w)
{
    const u32 p0 = WP(SUN0_DAY);
    u32 p1 = WP(SUN1_DAY);
    while (wg_gcd(p0, p1) != 1u) p1++;      /* terminates: p0 * k + 1 is coprime with p0 */
    w->sun_period[0] = p0;
    w->sun_period[1] = p1;
}

/* ---- 4.1 bands -------------------------------------------------------------------- */

/* the z range of a band in a column of height H; false when the band is empty there */
static bool wg_band_range(const AzWorld *w, const WgCtx *x, u32 H, u32 band, u32 *lo, u32 *hi)
{
    const u32 S = x->S;
    if (band == WG_B_AIR) { *lo = H > S ? H : S; *hi = x->gz - 1u; return true; }
    if (H >= S) return false;                            /* a land column has no water */
    if (band == WG_B_FLOOR || band == WG_B_VENTFLOOR) { *lo = H; *hi = H; return true; }
    const u32 D = S - H;
    u32 P = (u32)(((u64)D * WP(PHOTIC) + 255u) >> 8);
    if (P < 1u) P = 1u;
    if (P > D) P = D;
    if (band == WG_B_PHOTIC) { *lo = S - P; *hi = S - 1u; return true; }
    if (S - P < H + 2u) return false;                    /* MID: z in [H + 1, S - P - 1] */
    *lo = H + 1u; *hi = S - P - 1u;
    return true;
}

static u32 wg_band_cell(const AzWorld *w, const WgCtx *x, u32 col, u32 band, u32 r)
{
    const u32 H = wg_col_H(w, x, col);
    u32 lo, hi;
    if (!wg_band_range(w, x, H, band, &lo, &hi)) return AZ_NONE;
    const u32 z = lo + wg_below(r, hi - lo + 1u);
    return wg_cell(x, col, z);
}

static bool wg_find_mid(const AzWorld *w, const WgCtx *x)
{
    for (u32 col = 0; col < x->ncol; col++) {
        u32 lo, hi;
        if (wg_band_range(w, x, wg_col_H(w, x, col), WG_B_MID, &lo, &hi)) return true;
    }
    return false;
}

/* ---- 4.2 the niche table ------------------------------------------------------------ */

static void wg_niche(const AzWorld *w, const WgCtx *x, u32 k, WgNiche *n)
{
    u32 q[4];
    az_rand4(w, k, AZ_RS_WORLDGEN, WG_I_NICHE, q);
    const u32 styles = WP(STYLES);
    n->guild = (k + x->og) % WG_GUILDS;
    n->mode = (k + x->om) % WG_MODES;
    switch (n->guild) {
    case WG_G_PRODUCER: n->band = WG_B_PHOTIC; break;
    case WG_G_CHEMO:    n->band = WG_B_VENTFLOOR; break;
    case WG_G_GRAZER:   n->band = (q[0] & 255u) < x->mid_share ? WG_B_MID : WG_B_FLOOR; break;
    case WG_G_SCAV:     n->band = WG_B_FLOOR; break;
    default: {
        static const u8 pred_band[4] = { WG_B_AIR, WG_B_PHOTIC, WG_B_MID, WG_B_FLOOR };
        u32 r = wg_below(q[0], x->pred_sum), b = 0;
        while (b < 3u && r >= x->pred_w[b]) { r -= x->pred_w[b]; b++; }
        n->band = pred_band[b];
    }
    }
    if (n->band == WG_B_MID && !x->has_mid) n->band = WG_B_FLOOR;
    n->sc = wg_below(q[1], styles);
    if (n->guild == WG_G_PRED && n->sc + 1u < styles) n->sc++;
    n->ss = wg_below(q[2], styles);
    if (n->mode == AZ_MM_EUSOC) n->ss = styles - 1u;
    n->egg = ((q[3] >> 16) & 255u) < x->egg_share;
    n->kstrat = (q[3] >> 24) < x->k_share;
    n->pattern = (q[3] >> 8) & 255u;
}

/* ---- 4.3 patch centres, 4.7 the cells a founder may take ------------------------------------ */

static u32 wg_patch_centre(const AzWorld *w, const WgCtx *x, u32 k, const WgNiche *n, u32 j)
{
    u32 p[4];
    az_rand4(w, k, AZ_RS_WORLDGEN, (u64)WG_I_PATCH + j, p);
    if (n->band == WG_B_VENTFLOOR) return w->vent_cell[wg_below(p[0], x->V)];
    u32 col = wg_below(p[0], x->ncol);
    for (u32 step = 0; step < x->ncol; step++, col = (col + 1u) % x->ncol) {
        const u32 c = wg_band_cell(w, x, col, n->band, p[1]);
        if (c != AZ_NONE) return c;
    }
    return w->vent_cell[0];                                  /* not reached: FLOOR, PHOTIC and AIR exist */
}

/* the column at offset (dx, dy) from a patch centre and the z range of the band a founder of
 * the niche may take there; false when it takes the centre itself (the band is empty in that
 * column, or, at a vent, the floor there lies beyond the reach of the vent's mineral) */
static bool wg_place_range(const AzWorld *w, const WgCtx *x, u32 centre, const WgNiche *n, i32 dx, i32 dy,
                           u32 *col, u32 *lo, u32 *hi)
{
    *col = (((az_cell_y(w, centre) + (u32)dy) & (x->gy - 1u)) << w->gx_bits)
         | ((az_cell_x(w, centre) + (u32)dx) & (x->gx - 1u));
    const bool vent = n->band == WG_B_VENTFLOOR;
    if (!wg_band_range(w, x, wg_col_H(w, x, *col), vent ? (u32)WG_B_FLOOR : n->band, lo, hi)) return false;
    if (vent) {                                              /* a chemotroph lies where the mineral reaches */
        const u32 vz = az_cell_z(w, centre);
        if ((*lo > vz ? *lo - vz : vz - *lo) > x->reach) return false;
    }
    return true;
}

/* what the founders of a niche will meet, over every cell placement (4.7) can give them:
 * the coldest and the warmest cell, the lightest and densest medium and its mean, the highest
 * pressure */
typedef struct WgHab { i32 tmin, tmax; u32 dmin, dmax, dmean; u64 pmax; } WgHab;

static void wg_habitat(const AzWorld *w, const WgCtx *x, u32 k, const WgNiche *n, WgHab *h)
{
    const u32 patches = WP(PATCHES), R = x->reach;
    u64 dsum = 0, cols = 0;
    h->tmin = WG_TEMP_MAX; h->tmax = 0; h->dmin = 255; h->dmax = 0; h->pmax = 0;
    for (u32 j = 0; j < patches; j++) {
        const u32 centre = wg_patch_centre(w, x, k, n, j);
        for (i32 dy = -(i32)R; dy <= (i32)R; dy++)
            for (i32 dx = -(i32)R; dx <= (i32)R; dx++) {
                u32 col, lo, hi;
                if (!wg_place_range(w, x, centre, n, dx, dy, &col, &lo, &hi)) {
                    col = wg_col_of(x, centre); lo = hi = az_cell_z(w, centre);
                }
                u64 d = 0;
                for (u32 z = lo; z <= hi; z++) {             /* the z of a founder is drawn evenly in the band */
                    const u32 c = wg_cell(x, col, z), m = w->c_med[(size_t)c * 2];
                    const i32 t = w->c_temp[c];
                    d += m;
                    if (m < h->dmin) h->dmin = m;
                    if (m > h->dmax) h->dmax = m;
                    if (t < h->tmin) h->tmin = t;
                    if (t > h->tmax) h->tmax = t;
                }
                dsum += d / (hi - lo + 1u);
                cols++;
                const u64 p = az_cell_pressure(w, wg_cell(x, col, lo));   /* pressure grows with depth */
                if (p > h->pmax) h->pmax = p;
            }
    }
    h->dmean = (u32)(dsum / cols);
}

/* ---- 4.4, 4.5 targets and the prototype fit ------------------------------------------------- */

static u32 wg_level(const AzWorld *w, u32 i)
{
    const u32 lo = WP(NICHE_LO), hi = WP(NICHE_HI), styles = WP(STYLES);
    if (styles == 1u) return (lo + hi) / 2u;
    return lo + ((hi - lo) * i) / (styles - 1u);
}

/* what a target belongs to: its weight in the fit is that class's */
enum { WG_C_REST = 0, WG_C_CORE, WG_C_HABITAT, WG_C_STYLE };

static void wg_set(u8 *target, u8 *cls, u32 trait, u32 value, u32 c)
{
    target[trait] = (u8)(value > 255u ? 255u : value);
    cls[trait] = (u8)c;
}

/* what the niche asks of the body, and what it must reach: the pressure it must bear
 * (PRESS_TOL at least press), the side of its medium (DENSITY), the coldest and warmest
 * cell inside its comfortable band (TOPT and TTOL) */
typedef struct WgNeed { u32 press, dens; i32 side, tmin, tmax; } WgNeed;

static void wg_targets(const AzWorld *w, const WgCtx *x, u32 k, const WgNiche *n, u8 *target, u8 *cls, WgNeed *need)
{
    const u32 HI = WP(NICHE_HI), LO = WP(NICHE_LO), MID = (HI + LO) / 2u, styles = WP(STYLES);
    const u32 C = WG_C_CORE, H = WG_C_HABITAT, Y = WG_C_STYLE;
    for (u32 t = 0; t < AZ_TRAITS; t++) { target[t] = (u8)WG_NEUTRAL; cls[t] = WG_C_REST; }
    /* guild */
    switch (n->guild) {
    case WG_G_PRODUCER:
        wg_set(target, cls, AZ_T_DIET_LIGHT, HI, C); wg_set(target, cls, AZ_T_DIET_W, MID, C);
        wg_set(target, cls, AZ_T_DIET_X, LO, C); wg_set(target, cls, AZ_T_DIET_Y, LO, C);
        wg_set(target, cls, AZ_T_PREDATION, LO, C); wg_set(target, cls, AZ_T_AGGR, LO, C);
        wg_set(target, cls, AZ_T_WEAPON, LO, C);
        break;
    case WG_G_CHEMO:
        wg_set(target, cls, AZ_T_DIET_X, HI, C); wg_set(target, cls, AZ_T_OXI_USE, MID, C);
        wg_set(target, cls, AZ_T_DIET_LIGHT, LO, C); wg_set(target, cls, AZ_T_DIET_Y, LO, C);
        wg_set(target, cls, AZ_T_DIET_W, LO, C); wg_set(target, cls, AZ_T_PREDATION, LO, C);
        wg_set(target, cls, AZ_T_AGGR, LO, C); wg_set(target, cls, AZ_T_WEAPON, LO, C);
        break;
    case WG_G_GRAZER:
        wg_set(target, cls, AZ_T_DIET_Y, HI, C); wg_set(target, cls, AZ_T_OXI_USE, HI, C);
        wg_set(target, cls, AZ_T_DIET_LIGHT, LO, C); wg_set(target, cls, AZ_T_DIET_X, LO, C);
        wg_set(target, cls, AZ_T_DIET_W, LO, C); wg_set(target, cls, AZ_T_PREDATION, LO, C);
        wg_set(target, cls, AZ_T_AGGR, LO, C);
        break;
    case WG_G_SCAV:
        wg_set(target, cls, AZ_T_DIET_W, HI, C); wg_set(target, cls, AZ_T_DIET_Y, MID, C);
        wg_set(target, cls, AZ_T_DIET_LIGHT, LO, C); wg_set(target, cls, AZ_T_DIET_X, LO, C);
        wg_set(target, cls, AZ_T_PREDATION, LO, C); wg_set(target, cls, AZ_T_AGGR, LO, C);
        break;
    default:
        wg_set(target, cls, AZ_T_DIET_Y, HI, C); wg_set(target, cls, AZ_T_PREDATION, HI, C);
        wg_set(target, cls, AZ_T_AGGR, HI, C); wg_set(target, cls, AZ_T_WEAPON, HI, C);
        wg_set(target, cls, AZ_T_BOLDNESS, HI, C);
        wg_set(target, cls, AZ_T_DIET_LIGHT, LO, C); wg_set(target, cls, AZ_T_DIET_X, LO, C);
        wg_set(target, cls, AZ_T_DIET_W, LO, C);
        break;
    }
    /* mating mode: the centre of bin m (the mode is the trait >> 5) */
    wg_set(target, cls, AZ_T_MATING_MODE, (n->mode << 5) + (1u << 4), C);
    if (n->mode == AZ_MM_SELF) wg_set(target, cls, AZ_T_SELFING, HI, Y);
    if (n->mode == AZ_MM_HERMA) wg_set(target, cls, AZ_T_SELFING, LO, Y);
    /* habitat, over every cell the founders may be placed in */
    WgHab h;
    wg_habitat(w, x, k, n, &h);
    /* temperature: the optimum in the middle of the range the founders meet, a tolerance wide
     * enough for all of it (at least the niche's middle level) */
    const i32 tm = (h.tmin + h.tmax) / 2, half = (h.tmax - h.tmin + 1) / 2;
    wg_set(target, cls, AZ_T_TOPT, (u32)((tm >> 6) > 255 ? 255 : (tm >> 6)), H);
    const i32 tol = half > 256 ? (half - 256 + 15) / 16 : 0;
    wg_set(target, cls, AZ_T_TTOL, (u32)(tol > (i32)MID ? tol : (i32)MID), H);
    need->tmin = h.tmin; need->tmax = h.tmax;
    /* density: floor dwellers sink in the densest medium they may meet, the lit band and the
     * air float on the lightest, mid water hangs in its mean */
    const i64 off = (i64)WP(DENS_OFFSET);
    i64 dens;
    if (n->band == WG_B_FLOOR || n->band == WG_B_VENTFLOOR) { dens = (i64)h.dmax + off; need->side = 1; need->dens = h.dmax; }
    else if (n->band == WG_B_MID) { dens = (i64)h.dmean; need->side = 0; need->dens = h.dmean; }
    else { dens = (i64)h.dmin - off; need->side = -1; need->dens = h.dmin; }
    dens = dens < 0 ? 0 : dens > 255 ? 255 : dens;
    wg_set(target, cls, AZ_T_DENSITY, (u32)dens, H);
    /* pressure: a body bears (PRESS_TOL + 1) * METAB_PRESS_UNIT (az_metab.c) */
    const u64 unit = w->param[AZ_WP_METAB_PRESS_UNIT];
    const u64 bear = (h.pmax * (256u + WP(PRESS_MARGIN))) / 256u;
    u64 ptol = (bear + unit - 1u) / unit;
    ptol = ptol > 0u ? ptol - 1u : 0u;
    wg_set(target, cls, AZ_T_PRESS_TOL, (u32)(ptol > 255u ? 255u : ptol), H);
    u64 pneed = (h.pmax + unit - 1u) / unit;
    pneed = pneed > 0u ? pneed - 1u : 0u;
    need->press = (u32)(pneed > 255u ? 255u : pneed);
    if (n->band == WG_B_PHOTIC || n->band == WG_B_AIR) wg_set(target, cls, AZ_T_RAD_TOL, HI, H);
    /* style and life history */
    const u32 size = wg_level(w, n->sc), social = wg_level(w, n->ss);
    wg_set(target, cls, AZ_T_SIZE, size, Y);
    wg_set(target, cls, AZ_T_SEGMENT, size, Y);
    wg_set(target, cls, AZ_T_SOCIAL, social, Y);
    wg_set(target, cls, AZ_T_COLONIALITY, social, Y);
    wg_set(target, cls, AZ_T_ALTRUISM, social, Y);
    wg_set(target, cls, AZ_T_TERRITORIAL, wg_level(w, styles - 1u - n->ss), Y);
    wg_set(target, cls, AZ_T_EGG_TIME, n->egg ? HI : 0u, Y);    /* 0: born live */
    wg_set(target, cls, AZ_T_CLUTCH, n->kstrat ? LO : HI, Y);
    wg_set(target, cls, AZ_T_PROVISION, n->kstrat ? HI : LO, Y);
    wg_set(target, cls, AZ_T_CARE, n->kstrat ? HI : LO, Y);
    /* look: hues evenly spaced, so species tell kin from prey */
    wg_set(target, cls, AZ_T_HUE, (k * 256u) / x->nsp, Y);
    wg_set(target, cls, AZ_T_PATTERN, n->pattern, Y);
}

/* whether the niche holds. Its core: every core trait within half the gap between the niche's ends of
 * its target (so on the target's side of the middle), the mode bin exact. Its habitat (when it
 * is fitted at all): the pressure borne, the medium on the niche's side, every cell comfortable */
static bool wg_core_holds(const AzWorld *w, u32 s, const u8 *target, const u8 *cls, const WgNiche *n)
{
    const u8 *t = &w->trait[(size_t)s * AZ_TRAITS];
    const u32 gap = WP(NICHE_HI) - WP(NICHE_LO);
    if ((t[AZ_T_MATING_MODE] >> 5) != n->mode) return false;
    for (u32 i = 0; i < AZ_TRAITS; i++) {
        if (cls[i] != WG_C_CORE || i == AZ_T_MATING_MODE) continue;
        const u32 e = t[i] > target[i] ? t[i] - target[i] : target[i] - t[i];
        if (2u * e > gap) return false;
    }
    return true;
}

static bool wg_habitat_holds(const AzWorld *w, u32 s, const WgNeed *need)
{
    const u8 *t = &w->trait[(size_t)s * AZ_TRAITS];
    if (!WP(W_HABITAT)) return true;
    if (t[AZ_T_PRESS_TOL] < need->press) return false;
    if (need->side > 0 && need->dens < 255u && t[AZ_T_DENSITY] <= need->dens) return false;
    if (need->side < 0 && need->dens > 0u && t[AZ_T_DENSITY] >= need->dens) return false;
    const i32 opt = AZ_TEMP_OF_TOPT(t[AZ_T_TOPT]), band = AZ_TEMP_TOL(t[AZ_T_TTOL]);
    return need->tmin >= opt - band && need->tmax <= opt + band;
}

static void wg_prototype(AzWorld *w, const WgCtx *x, u32 s, u32 k, const WgNiche *n)
{
    u8 target[AZ_TRAITS], cls[AZ_TRAITS], weight[AZ_TRAITS];
    WgNeed need;
    wg_targets(w, x, k, n, target, cls, &need);
    /* the traits the niche does not name are held where the species drew them: the fit moves
     * shared families, and without this the rest would be pushed to 0 or 255 */
    const u8 *drawn = &w->trait[(size_t)s * AZ_TRAITS];
    for (u32 t = 0; t < AZ_TRAITS; t++)
        if (cls[t] == WG_C_REST) target[t] = drawn[t];
    const u32 iters = WP(FIT_ITERS);
    /* up to five fits, each only while the niche does not hold: all of the niche; the drawn rest
     * held half as hard; not held; style let go too; and last, only if the core still does not
     * hold, the core alone. The guild and the mode come first, then the habitat, then style,
     * then the drawn rest */
    for (u32 pass = 0; pass < 5u; pass++) {
        const bool core = pass == 0u || wg_core_holds(w, s, target, cls, n);
        if (pass > 0u && core && wg_habitat_holds(w, s, &need)) break;
        if (pass == 4u && core) break;
        const u32 wt[4] = { pass == 0u ? WP(W_REST) : pass == 1u ? WP(W_REST) / 2u : 0u, WP(W_CORE),
                            pass == 4u ? 0u : WP(W_HABITAT), pass >= 3u ? 0u : WP(W_STYLE) };
        for (u32 t = 0; t < AZ_TRAITS; t++) weight[t] = (u8)wt[cls[t]];
        (void)az_genome_fit(w, s, target, weight, iters);
    }
    if ((w->trait[(size_t)s * AZ_TRAITS + AZ_T_MATING_MODE] >> 5) != n->mode) {
        for (u32 t = 0; t < AZ_TRAITS; t++) weight[t] = 0;
        weight[AZ_T_MATING_MODE] = (u8)WP(W_CORE);
        (void)az_genome_fit(w, s, target, weight, iters);
    }
}

/* ---- 4.6, 4.7 founders -------------------------------------------------------------------------- */

static bool wg_is_marker(const AzWorld *w, u32 fam)
{
    for (u32 i = 0; i < 4u; i++)
        if (w->code.marker_locus[i] == fam) return true;
    return false;
}

static u32 wg_place(AzWorld *w, const WgCtx *x, u32 k, const WgNiche *n, u32 pos[3])
{
    u32 r[4], u[4];
    az_rand4(w, w->uid_next, AZ_RS_WORLDGEN, WG_I_PLACE, r);
    const u32 centre = wg_patch_centre(w, x, k, n, wg_below(r[0], WP(PATCHES)));
    const u32 R = x->reach;
    const i32 dx = (i32)wg_below(r[1], 2u * R + 1u) - (i32)R, dy = (i32)wg_below(r[2], 2u * R + 1u) - (i32)R;
    u32 col, lo, hi, cell = centre;
    if (wg_place_range(w, x, centre, n, dx, dy, &col, &lo, &hi)) cell = wg_cell(x, col, lo + wg_below(r[3], hi - lo + 1u));
    az_rand4(w, w->uid_next, AZ_RS_WORLDGEN, WG_I_PLACE_SUB, u);
    pos[0] = (az_cell_x(w, cell) << w->shift_x) | (u[0] & ((1u << w->shift_x) - 1u));
    pos[1] = (az_cell_y(w, cell) << w->shift_y) | (u[1] & ((1u << w->shift_y) - 1u));
    u32 pz = (az_cell_z(w, cell) << w->shift_z) | (u[2] & ((1u << w->shift_z) - 1u));
    if (pz < WG_EDGE) pz = WG_EDGE;
    if (pz > 0xFFFFFFFFu - WG_EDGE) pz = 0xFFFFFFFFu - WG_EDGE;
    pos[2] = pz;
    return az_rock_place(w, &pos[0], &pos[1], &pos[2]);
}

/* the founder takes the prototype's genome except its own marker genes (sex, kinship) */
static void wg_copy_genome(AzWorld *w, u32 s, u32 proto)
{
    for (u32 h = 0; h < 2u; h++) {
        u32 ls, lp;
        AzGene *g = (AzGene *)az_seg(w, s, AZ_SEG_GEN_A + h, &ls);
        const AzGene *p = (const AzGene *)az_seg(w, proto, AZ_SEG_GEN_A + h, &lp);
        if (ls != lp) return;                                   /* keep its own genome */
        for (u32 i = 0; i < ls / (u32)sizeof(AzGene); i++) {
            if (g[i].fam != p[i].fam || wg_is_marker(w, g[i].fam)) continue;
            g[i].allele = p[i].allele;
            g[i].expr = p[i].expr;
        }
    }
}

/* a few loci of its own, each in one haplotype */
static void wg_vary(AzWorld *w, u32 s)
{
    const u32 loci = WP(VARY_LOCI), size = WP(VARY_SIZE);
    for (u32 q = 0; q < loci; q++) {
        u32 v[4];
        az_rand4(w, w->uid[s], AZ_RS_WORLDGEN, (u64)WG_I_VARY + q, v);
        u32 la, lh;
        (void)az_seg(w, s, AZ_SEG_GEN_A, &la);
        const u32 h = v[1] & 1u;
        AzGene *g = (AzGene *)az_seg(w, s, AZ_SEG_GEN_A + h, &lh);
        const u32 i = wg_below(v[0], la / (u32)sizeof(AzGene));
        if (i >= lh / (u32)sizeof(AzGene) || wg_is_marker(w, g[i].fam)) continue;
        const i32 delta = (i32)wg_below(v[2], 2u * size + 1u) - (i32)size;
        g[i].allele = az_sat_u8((i32)g[i].allele + delta);
    }
}

/* a founder holds at most what its body can hold: PART_HOLD per part, at most CELL_HOLD, and
 * energy worth that matter (metab's rule, az_metab.c). What a body cannot hold metab turns into
 * the heat of its cell at once, and many founders in one cell would overflow what one cell
 * can take in a tick */
static void wg_pools(AzWorld *w, u32 s)
{
    const u32 size = w->trait[(size_t)s * AZ_TRAITS + AZ_T_SIZE];
    u32 np;
    (void)az_parts(w, s, &np);
    u64 hold = (u64)w->param[AZ_WP_PART_HOLD] * np;
    if (hold > w->param[AZ_WP_CELL_HOLD]) hold = w->param[AZ_WP_CELL_HOLD];
    const u64 e_hold = hold * AZ_EPS_BIO > 0x3FFFFFFFull ? 0x3FFFFFFFull : hold * AZ_EPS_BIO;
    const u64 e = WP(FOUNDER_ENERGY), pool = ((u64)WP(FOUNDER_POOL) * (size + 16u)) / (WG_NEUTRAL + 16u);
    w->energy[s] = (i32)(e < e_hold ? e : e_hold);
    w->pool_a[s] = (u32)(pool < hold ? pool : hold);
    w->pool_m[s] = 0;
    w->pool_t[s] = 0;
    w->mass[s] = w->pool_a[s] + w->pool_m[s] + w->pool_t[s];
    w->health[s] = (u16)w->param[AZ_WP_LIFE_BIRTH_HEALTH];
}

static void wg_founders(AzWorld *w, const WgCtx *x)
{
    u32 proto[WG_MAX_SPECIES], clade_of[WG_MAX_SPECIES];
    for (u32 k = 0; k < WG_MAX_SPECIES; k++) proto[k] = AZ_NONE;
    for (u32 k = 0; k < x->nsp; k++) clade_of[k] = az_clade_found(w, WG_SEED_BASE + k, k);
    const u32 N = w->cfg.n_initial;
    for (u32 i = 0; i < N; i++) {
        const u32 k = i % x->nsp;
        WgNiche n;
        wg_niche(w, x, k, &n);
        u32 pos[3];
        const u32 cell = wg_place(w, x, k, &n, pos);
        if (cell == AZ_NONE) {
            az_emit(w, 0, AZ_EV_REFUSED, AZ_REF_TERRAIN, w->uid_next, 0, 0);
            break;
        }
        const u32 s = az_spawn(w);
        if (s == AZ_NONE) break;
        w->pos_x[s] = pos[0]; w->pos_y[s] = pos[1]; w->pos_z[s] = pos[2];
        w->cell[s] = cell;
        if (!az_found(w, s, WG_SEED_BASE + k)) break;          /* no room: the slot stays dead */
        if (i < x->nsp) {
            wg_prototype(w, x, s, k, &n);
            proto[k] = s;
        } else {
            if (proto[k] != AZ_NONE) wg_copy_genome(w, s, proto[k]);   /* else it keeps its own genome */
            wg_vary(w, s);
            az_develop(w, s);
        }
        wg_pools(w, s);
        const u32 cl = clade_of[k];
        w->clade[s] = cl;
        w->clades[cl].refcount++;
        w->flags[s] = AZ_F_ALIVE;
        w->lineage[s] = k;
        w->birth_tick[s] = 0;
    }
    for (u32 k = 0; k < x->nsp; k++) {                          /* the copy source is varied last */
        if (proto[k] == AZ_NONE) continue;
        wg_vary(w, proto[k]);
        az_develop(w, proto[k]);
        wg_pools(w, proto[k]);
    }
}

/* ---- the pass ------------------------------------------------------------------------------------------ */

void az_worldgen(AzWorld *w)
{
    WgCtx x;
    az_memset(&x, 0, sizeof x);
    x.gx = w->gx; x.gy = w->gy; x.gz = w->gz;
    x.ncol = w->gx * w->gy;
    x.plane_bits = (u32)w->gx_bits + w->gy_bits;
    x.reach = WP(REACH);
    if (w->cfg.n_species > WG_MAX_SPECIES) {
        /* P406-WG-04 / DEMO-16: record explicit refusal rather than silently clamping */
        az_emit(w, 0, AZ_EV_REFUSED, AZ_REF_CLADE, w->cfg.n_species, WG_MAX_SPECIES, 0);
        x.nsp = WG_MAX_SPECIES;
        w->cfg.n_species = x.nsp;
    } else {
        x.nsp = w->cfg.n_species == 0u ? 1u : w->cfg.n_species;
    }

    az_genetic_code_init(w);
    wg_terrain(w, &x);
    wg_layers(w, &x);
    wg_vents(w, &x);
    wg_spots_reefs(w, &x);
    wg_heat(w, &x);
    wg_gravity(w, &x);                    /* the last use of tmp_u32 */
    wg_suns(w);
    w->vent_count = 0;                    /* the one weather call runs no vent: worldgen posts nothing */
    az_weather_global(w);
    w->vent_count = (u16)x.V;
    for (u32 c = w->n_cells; c-- > 0u;) az_weather_derive(w, c);   /* top layer first: light comes from above */
    for (u32 c = 0; c < w->n_cells; c++) az_chem_derive(w, c);

    u32 r[4];
    az_rand4(w, 0, AZ_RS_WORLDGEN, WG_I_NICHE_ROT, r);
    x.og = wg_below(r[0], WG_GUILDS);
    x.om = wg_below(r[1], WG_MODES);
    x.egg_share = r[2] & 255u;
    x.k_share = (r[2] >> 8) & 255u;
    x.mid_share = (r[2] >> 16) & 255u;
    x.pred_sum = 0;
    for (u32 b = 0; b < 4u; b++) { x.pred_w[b] = 1u + ((r[3] >> (8u * b)) & 255u); x.pred_sum += x.pred_w[b]; }
    x.has_mid = wg_find_mid(w, &x);
    wg_founders(w, &x);
    az_publish_pressure(w);
}
