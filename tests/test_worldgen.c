/* test_worldgen.c - the alien world at tick 0 and its founders (core/az_worldgen.c).
 *
 *   A. the world   (seed 91, 2000 founders, 16 species, 16x16x8): the terrain against an
 *                  oracle of the noise, the layer order, nothing in bedrock, amount bounds,
 *                  nothing posted, the temperature field, the derived columns current, the
 *                  vents and the reservoir, the suns, the gravity field and its mean
 *   B. founders    (same world): counts per species, every founder alive and open, developed,
 *                  clades exact, every guild and mating mode present, the fit within the
 *                  measured tolerance, variation, habitat bands, pools
 *   C. modes       one seed one world, two seeds two worlds, wild worlds follow entropy and
 *                  not cfg.seed, a snapshot round trip, one hash on 0 and 3 workers
 *   D. books       200 ticks exact every tick, the ocean still liquid; the gravity lesion gives
 *                  another world
 *   E. edges       1 and 100 species, the smallest and a tall grid, a store that refuses,
 *                  founders that lack genes, crowds of the richest founders in one cell, grids
 *                  that are not square, 0 and 150 species, a small lineage table, every
 *                  parameter at both ends of its range (the contracts, the layer recipe, a
 *                  snapshot at tick 0, the bands, the niche table), and every parameter acts
 *
 * Seeds 91 to 98 (the tuning block of this module). With --measure-fit the program prints
 * the fit errors over seeds 1 to 99 with 100 species (how FIT_TOL and FIT_MEAN were set).
 */
#include "az_test_locked_block.h"

/* az_worldgen.c's idx labels of AZ_RS_WORLDGEN, written a second time on purpose: the test
 * pins the stream and the ids */
enum { I_TERRAIN = 0x01, I_AMOUNT = 0x10, I_NICHE_ROT = 0x30, I_NICHE = 0x31 };
enum { G_PRODUCER = 0, G_CHEMO, G_GRAZER, G_SCAV, G_PRED, N_GUILDS };
enum { B_FLOOR = 0, B_PHOTIC, B_MID, B_AIR, B_VENTFLOOR };

/* the fit tolerances: 1.25 times the worst case measured with --measure-fit over seeds 1-99,
 * 100 species, no variation (worst core trait 53 from its target, worst mean 14.000 per core
 * trait of one prototype, the traits the niche leaves free 62.095 from their drawn values on
 * average over the worst prototype; build/work/w2/worldgen/PROGRESS.md, fixer step). FIT_TOL
 * stays below half the gap between the niche's ends (84 at the defaults), and side_bad checks
 * that gap itself */
#define FIT_TOL         66u      /* 1.25 * 53     */
#define FIT_MEAN_X1000  17500u   /* 1.25 * 14.000 */
#define REST_MEAN_X1000 77618u   /* 1.25 * 62.095 */

static u32 P(const AzWorld *w, u32 id) { return w->param[id]; }
#define WP(name) P(w, AZ_WP_WORLDGEN_##name)
static u32 *sp(const AzWorld *w, u32 c) { return &w->c_sp[(size_t)c * AZ_NSPECIES]; }
static u32 below(u32 r, u32 n) { return (u32)(((u64)r * n) >> 32); }
static u32 ncol(const AzWorld *w) { return w->gx * w->gy; }
static u32 cellz(const AzWorld *w, u32 col, u32 z) { return (z << (w->gx_bits + w->gy_bits)) | col; }
static u32 colof(const AzWorld *w, u32 c) { return c & (ncol(w) - 1u); }

/* ---- worlds ---- */
typedef struct Ov { u32 id, v; } Ov;

static bool gen_ex(AzTestWorld *t, u64 seed, u32 n, u32 nsp, u32 gxb, u32 gyb, u32 gzb,
                   const Ov *ov, u32 nov, u32 threads, u32 store_units, u32 lesion)
{
    AzConfig cfg = az_test_config(seed, n);
    cfg.n_species = nsp;
    cfg.gx_bits = (u8)gxb; cfg.gy_bits = (u8)gyb; cfg.gz_bits = (u8)gzb;
    cfg.store_units = store_units;
    cfg.lesion = lesion;
    if (!az_test_world_ex(t, &cfg, threads, AZ_TEST_NO_WORLDGEN)) return false;
    for (u32 i = 0; i < nov; i++)
        if (!az_param_set(t->w, ov[i].id, ov[i].v)) return false;
    az_worldgen(t->w);
    return true;
}
static bool gen(AzTestWorld *t, u64 seed, u32 n, u32 nsp, const Ov *ov, u32 nov)
{
    return gen_ex(t, seed, n, nsp, 4, 4, 3, ov, nov, 0, 0, 0);
}

static u64 mixin(u64 h, u64 v) { return az_mix64(h ^ v) + 0x9E3779B97F4A7C15ull; }

/* what worldgen made, without the parameters (az_state_hash hashes them) */
static u64 digest(const AzWorld *w)
{
    u64 h = 1;
    for (u32 c = 0; c < w->n_cells; c++) {
        for (u32 k = 0; k < AZ_NSPECIES; k++) h = mixin(h, sp(w, c)[k]);
        h = mixin(h, w->c_heat[c]);
        h = mixin(h, w->c_solid[c]);
        u32 g; memcpy(&g, &w->c_grav[(size_t)c * 4], 4); h = mixin(h, g);
    }
    h = mixin(h, w->vent_count);
    for (u32 v = 0; v < 64u; v++) h = mixin(h, w->vent_cell[v]);
    h = mixin(h, w->sun_period[0]); h = mixin(h, w->sun_period[1]);
    h = mixin(h, w->gravity); h = mixin(h, w->totals.reservoir_mu);
    for (u32 s = 0; s < w->n_slots; s++) {
        if (!(w->flags[s] & AZ_F_ALIVE)) continue;
        h = mixin(h, w->pos_x[s]); h = mixin(h, w->pos_y[s]); h = mixin(h, w->pos_z[s]);
        for (u32 t = 0; t < AZ_TRAITS; t++) h = mixin(h, w->trait[(size_t)s * AZ_TRAITS + t]);
        for (u32 hp = 0; hp < 2u; hp++) {
            u32 bytes;
            const u8 *g = (const u8 *)az_seg(w, s, AZ_SEG_GEN_A + hp, &bytes);
            for (u32 i = 0; i < bytes; i++) h = mixin(h, g[i]);
        }
        h = mixin(h, (u32)w->energy[s]); h = mixin(h, w->pool_a[s]); h = mixin(h, w->pool_m[s]);
        h = mixin(h, w->pool_t[s]); h = mixin(h, w->mass[s]); h = mixin(h, w->health[s]);
        h = mixin(h, w->lineage[s]); h = mixin(h, w->clade[s]);
    }
    return h;
}

static u32 col_H(const AzWorld *w, u32 col)
{
    u32 n = 0;
    for (u32 z = 0; z < w->gz; z++) n += w->c_solid[cellz(w, col, z)] == 255u;
    return n;
}

/* ---- the oracle of 2.1 to 2.3 ---- */
static u32 o_smooth(u32 f) { return (u32)(((u64)f * f * (3u * 65536u - 2u * f)) >> 32); }

static u32 oracle_noise(const AzWorld *w, u32 x, u32 y, u32 oct, u32 per, u32 idx)
{
    u64 sum = 0, asum = 0, amp = 65536;
    for (u32 o = 0; o < oct; o++) {
        const u32 Lx = (2u << o) < w->gx ? (2u << o) : w->gx, Ly = (2u << o) < w->gy ? (2u << o) : w->gy;
        const u32 sx = w->gx / Lx, sy = w->gy / Ly;
        const u32 i0 = x / sx, i1 = (i0 + 1) % Lx, j0 = y / sy, j1 = (j0 + 1) % Ly;
        const u64 fx = o_smooth(((x % sx) << 16) / sx), fy = o_smooth(((y % sy) << 16) / sy);
        u64 v[2][2];
        for (u32 a = 0; a < 2; a++)
            for (u32 b = 0; b < 2; b++)
                v[a][b] = az_rand(w, ((u64)o << 32) | ((u64)(b ? j1 : j0) << 12) | (a ? i1 : i0), AZ_RS_WORLDGEN, idx) >> 16;
        const u64 ra = (v[0][0] * (65536 - fx) + v[1][0] * fx) >> 16, rb = (v[0][1] * (65536 - fx) + v[1][1] * fx) >> 16;
        sum += amp * ((ra * (65536 - fy) + rb * fy) >> 16);
        asum += amp;
        amp = (amp * per) >> 8;
    }
    return (u32)(sum / asum);
}

typedef struct Terrain { u32 H[4096]; u32 rem[4096]; u32 S, Hmin, Dmax; } Terrain;

static void oracle_terrain(const AzWorld *w, Terrain *o)
{
    const u32 gz = w->gz, hcap = gz >= 3 ? gz - 2 : 0;
    u32 hrel = (u32)(((u64)gz * WP(RELIEF)) >> 8);
    if (hrel > hcap) hrel = hcap;
    u64 hsum = 0;
    o->Hmin = 0xFFFFFFFFu;
    for (u32 col = 0; col < ncol(w) && col < 4096u; col++) {
        const u32 n = oracle_noise(w, col % w->gx, col / w->gx, WP(OCTAVES), WP(PERSIST), I_TERRAIN);
        const u64 t = (u64)n * (hrel + 1);
        o->H[col] = (u32)(t >> 16); o->rem[col] = (u32)(t & 0xFFFF);
        hsum += o->H[col];
        if (o->H[col] < o->Hmin) o->Hmin = o->H[col];
    }
    const u32 hm = (u32)(hsum / ncol(w));
    u32 S = hm + (u32)((((u64)(gz - hm)) * WP(SEA_LEVEL) + 128) >> 8);
    if (S < o->Hmin + 1) S = o->Hmin + 1;
    if (S > gz - 1) S = gz - 1;
    o->S = S; o->Dmax = S - o->Hmin;
}

/* S as the world shows it: the open cells of the lowest column that hold more solvent than gas */
static u32 world_S(const AzWorld *w)
{
    u32 best = 0xFFFFFFFFu, bcol = 0;
    for (u32 col = 0; col < ncol(w); col++) { const u32 H = col_H(w, col); if (H < best) { best = H; bcol = col; } }
    u32 z = best;
    while (z < w->gz && sp(w, cellz(w, bcol, z))[AZ_SP_SOLV] > sp(w, cellz(w, bcol, z))[AZ_SP_GAS]) z++;
    return z;
}

/* ---- the oracle of the niche table (4.1, 4.2) ---- */
typedef struct Niche { u32 guild, mode, band; } Niche;

static bool has_mid(const AzWorld *w, u32 S)
{
    for (u32 col = 0; col < ncol(w); col++) {
        const u32 H = col_H(w, col);
        if (H >= S) continue;
        const u32 D = S - H;
        u32 Pp = (u32)(((u64)D * WP(PHOTIC) + 255) >> 8);
        if (Pp < 1) Pp = 1;
        if (Pp > D) Pp = D;
        if (S - Pp >= H + 2) return true;
    }
    return false;
}

static Niche oracle_niche(const AzWorld *w, u32 k, bool mid)
{
    u32 r[4], q[4];
    az_rand4(w, 0, AZ_RS_WORLDGEN, I_NICHE_ROT, r);
    az_rand4(w, k, AZ_RS_WORLDGEN, I_NICHE, q);
    Niche n;
    n.guild = (k + below(r[0], 5)) % 5;
    n.mode = (k + below(r[1], 8)) % 8;
    /* the world's own mix: grazers in mid water by its share r[2] byte 2; predators over the four
     * bands by the weights 1 + the bytes of r[3] */
    static const u32 pred[4] = { B_AIR, B_PHOTIC, B_MID, B_FLOOR };
    u32 pw[4], psum = 0;
    for (u32 b = 0; b < 4; b++) { pw[b] = 1 + ((r[3] >> (8 * b)) & 255); psum += pw[b]; }
    u32 pr = below(q[0], psum), pb = 0;
    while (pb < 3 && pr >= pw[pb]) { pr -= pw[pb]; pb++; }
    n.band = n.guild == G_PRODUCER ? B_PHOTIC : n.guild == G_CHEMO ? B_VENTFLOOR
           : n.guild == G_GRAZER ? ((q[0] & 255) < ((r[2] >> 16) & 255) ? B_MID : B_FLOOR) : n.guild == G_SCAV ? B_FLOOR : pred[pb];
    if (n.band == B_MID && !mid) n.band = B_FLOOR;
    return n;
}

/* the core (W_CORE) targets of a guild: trait and target */
static u32 core_targets(const AzWorld *w, u32 guild, u32 *trait, u32 *target)
{
    const u32 HI = WP(NICHE_HI), LO = WP(NICHE_LO), MID = (HI + LO) / 2;
    u32 n = 0;
#define T(tr, v) do { trait[n] = (tr); target[n] = (v); n++; } while (0)
    switch (guild) {
    case G_PRODUCER: T(AZ_T_DIET_LIGHT, HI); T(AZ_T_DIET_W, MID); T(AZ_T_DIET_X, LO); T(AZ_T_DIET_Y, LO);
                     T(AZ_T_PREDATION, LO); T(AZ_T_AGGR, LO); T(AZ_T_WEAPON, LO); break;
    case G_CHEMO:    T(AZ_T_DIET_X, HI); T(AZ_T_OXI_USE, MID); T(AZ_T_DIET_LIGHT, LO); T(AZ_T_DIET_Y, LO);
                     T(AZ_T_DIET_W, LO); T(AZ_T_PREDATION, LO); T(AZ_T_AGGR, LO); T(AZ_T_WEAPON, LO); break;
    case G_GRAZER:   T(AZ_T_DIET_Y, HI); T(AZ_T_OXI_USE, HI); T(AZ_T_DIET_LIGHT, LO); T(AZ_T_DIET_X, LO);
                     T(AZ_T_DIET_W, LO); T(AZ_T_PREDATION, LO); T(AZ_T_AGGR, LO); break;
    case G_SCAV:     T(AZ_T_DIET_W, HI); T(AZ_T_DIET_Y, MID); T(AZ_T_DIET_LIGHT, LO); T(AZ_T_DIET_X, LO);
                     T(AZ_T_PREDATION, LO); T(AZ_T_AGGR, LO); break;
    default:         T(AZ_T_DIET_Y, HI); T(AZ_T_PREDATION, HI); T(AZ_T_AGGR, HI); T(AZ_T_WEAPON, HI);
                     T(AZ_T_BOLDNESS, HI); T(AZ_T_DIET_LIGHT, LO); T(AZ_T_DIET_X, LO); T(AZ_T_DIET_W, LO); break;
    }
#undef T
    return n;
}

/* the guild a body's traits show: a predator by its predation, otherwise its main food */
static u32 guild_of(const AzWorld *w, u32 s)
{
    const u8 *t = &w->trait[(size_t)s * AZ_TRAITS];
    const u32 MID = (WP(NICHE_HI) + WP(NICHE_LO)) / 2;
    if (t[AZ_T_PREDATION] > MID) return G_PRED;
    const u32 d[4] = { t[AZ_T_DIET_LIGHT], t[AZ_T_DIET_X], t[AZ_T_DIET_Y], t[AZ_T_DIET_W] };
    static const u32 g[4] = { G_PRODUCER, G_CHEMO, G_GRAZER, G_SCAV };
    u32 best = 0;
    for (u32 i = 1; i < 4; i++) if (d[i] > d[best]) best = i;
    return g[best];
}

static u32 first_of(const AzWorld *w, u32 k)          /* the prototype: lowest uid of the species */
{
    u32 best = AZ_NONE;
    for (u32 s = 0; s < w->n_slots; s++)
        if ((w->flags[s] & AZ_F_ALIVE) && w->lineage[s] == k && (best == AZ_NONE || w->uid[s] < w->uid[best])) best = s;
    return best;
}

/* fit errors of the prototypes: worst core error, mean core error (of 1); false on a wrong mode */
static bool fit_errors(const AzWorld *w, u32 nsp, u32 *worst, u32 *worst_mean_x1000)
{
    bool modes_ok = true;
    const u32 S = world_S(w);
    const bool mid = has_mid(w, S);
    *worst = 0; *worst_mean_x1000 = 0;
    for (u32 k = 0; k < nsp; k++) {
        const u32 s = first_of(w, k);
        if (s == AZ_NONE) continue;
        const Niche n = oracle_niche(w, k, mid);
        const u8 *t = &w->trait[(size_t)s * AZ_TRAITS];
        if ((t[AZ_T_MATING_MODE] >> 5) != n.mode) modes_ok = false;
        u32 tr[16], tg[16];
        const u32 m = core_targets(w, n.guild, tr, tg);
        u32 sum = 0;
        for (u32 i = 0; i < m; i++) {
            const u32 e = (u32)abs((int)t[tr[i]] - (int)tg[i]);
            sum += e;
            if (e > *worst) *worst = e;
        }
        const u32 e = (u32)abs((int)t[AZ_T_MATING_MODE] - (int)((n.mode << 5) + 16));
        sum += e;
        if (e > *worst) *worst = e;
        const u32 mean = (sum * 1000u) / (m + 1u);
        if (mean > *worst_mean_x1000) *worst_mean_x1000 = mean;
    }
    return modes_ok;
}

/* core traits of the prototypes farther from their target than half the gap between the niche's
 * ends: on the wrong side of the middle (the mating mode is judged by its bin, in fit_errors) */
static u32 side_bad(const AzWorld *w, u32 nsp)
{
    const u32 S = world_S(w), gap = WP(NICHE_HI) - WP(NICHE_LO);
    const bool mid = has_mid(w, S);
    u32 bad = 0;
    for (u32 k = 0; k < nsp; k++) {
        const u32 s = first_of(w, k);
        if (s == AZ_NONE) continue;
        u32 tr[16], tg[16];
        const u32 m = core_targets(w, oracle_niche(w, k, mid).guild, tr, tg);
        for (u32 i = 0; i < m; i++) bad += 2u * (u32)abs((int)w->trait[(size_t)s * AZ_TRAITS + tr[i]] - (int)tg[i]) > gap;
    }
    return bad;
}

/* the habitat a founder meets in its cell at tick 0: the pressure it bears, the side of its
 * medium (floor dwellers denser, the lit band and the air lighter), its comfortable band */
static void habitat_fit(const AzWorld *w, u32 *press, u32 *dens, u32 *comfort)
{
    const u32 S = world_S(w), unit = P(w, AZ_WP_METAB_PRESS_UNIT);
    const bool mid = has_mid(w, S);
    *press = *dens = *comfort = 0;
    for (u32 s = 0; s < w->n_slots; s++) {
        if (!(w->flags[s] & AZ_F_ALIVE)) continue;
        const u8 *t = &w->trait[(size_t)s * AZ_TRAITS];
        const u32 c = w->cell[s], med = w->c_med[(size_t)c * 2], band = oracle_niche(w, w->lineage[s], mid).band;
        *press += az_cell_pressure(w, c) > (u64)(t[AZ_T_PRESS_TOL] + 1u) * unit;
        if (band == B_FLOOR || band == B_VENTFLOOR) *dens += med < 255 && t[AZ_T_DENSITY] <= med;
        if (band == B_PHOTIC || band == B_AIR) *dens += med > 0 && t[AZ_T_DENSITY] >= med;
        *comfort += abs((int)w->c_temp[c] - AZ_TEMP_OF_TOPT(t[AZ_T_TOPT])) > AZ_TEMP_TOL(t[AZ_T_TTOL]);
    }
}

/* the traits of the species' genomes as az_found draws them (before any fit), in a world of the
 * same seed */
static bool drawn_traits(u64 seed, u32 nsp, u8 *out)
{
    AzTestWorld t;
    AzConfig cfg = az_test_config(seed, 0);
    cfg.n_species = nsp;
    if (!az_test_world_ex(&t, &cfg, 0, AZ_TEST_NO_WORLDGEN)) return false;
    az_genetic_code_init(t.w);
    bool ok = true;
    for (u32 k = 0; k < nsp && ok; k++) {
        const u32 s = az_spawn(t.w);
        ok = s != AZ_NONE && az_found(t.w, s, 0x10000u + k);
        if (ok) memcpy(&out[(size_t)k * AZ_TRAITS], &t.w->trait[(size_t)s * AZ_TRAITS], AZ_TRAITS);
    }
    az_test_world_free(&t);
    return ok;
}

/* what the fit did to the traits the niche does not name: the worst mean shift of those traits
 * of one prototype from the species' drawn value (of 1000), and the traits at 0 or 255 and the
 * sense ranges below 16, fitted and drawn */
typedef struct Collateral { u32 worst_rest_x1000, ext_fit, ext_drawn, sense_fit, sense_drawn; } Collateral;

static Collateral collateral(const AzWorld *w, u32 nsp, const u8 *drawn)
{
    static const u8 named[] = { AZ_T_DIET_LIGHT, AZ_T_DIET_W, AZ_T_DIET_X, AZ_T_DIET_Y, AZ_T_PREDATION, AZ_T_AGGR,
        AZ_T_WEAPON, AZ_T_OXI_USE, AZ_T_BOLDNESS, AZ_T_MATING_MODE, AZ_T_SELFING, AZ_T_TOPT, AZ_T_TTOL, AZ_T_DENSITY,
        AZ_T_PRESS_TOL, AZ_T_RAD_TOL, AZ_T_SIZE, AZ_T_SEGMENT, AZ_T_SOCIAL, AZ_T_COLONIALITY, AZ_T_ALTRUISM,
        AZ_T_TERRITORIAL, AZ_T_EGG_TIME, AZ_T_CLUTCH, AZ_T_PROVISION, AZ_T_CARE, AZ_T_HUE, AZ_T_PATTERN };
    bool is_named[AZ_TRAITS] = { false };
    for (u32 i = 0; i < sizeof named; i++) is_named[named[i]] = true;
    Collateral r = { 0, 0, 0, 0, 0 };
    for (u32 k = 0; k < nsp; k++) {
        const u32 s = first_of(w, k);
        if (s == AZ_NONE) continue;
        const u8 *f = &w->trait[(size_t)s * AZ_TRAITS], *d = &drawn[(size_t)k * AZ_TRAITS];
        u32 sum = 0, n = 0;
        for (u32 t = 0; t < AZ_TRAITS; t++) {
            r.ext_fit += f[t] == 0 || f[t] == 255;
            r.ext_drawn += d[t] == 0 || d[t] == 255;
            if (is_named[t]) continue;
            sum += (u32)abs((int)f[t] - (int)d[t]);
            n++;
        }
        const u32 mean = n ? (sum * 1000u) / n : 0u;
        if (mean > r.worst_rest_x1000) r.worst_rest_x1000 = mean;
        r.sense_fit += f[AZ_T_SENSE_RANGE] < 16;
        r.sense_drawn += d[AZ_T_SENSE_RANGE] < 16;
    }
    return r;
}

static bool near_vent(const AzWorld *w, u32 c, u32 R, bool xy_only)
{
    for (u32 v = 0; v < w->vent_count; v++) {
        const u32 vc = w->vent_cell[v];
        u32 dx = (u32)abs((int)az_cell_x(w, c) - (int)az_cell_x(w, vc)); if (w->gx - dx < dx) dx = w->gx - dx;
        u32 dy = (u32)abs((int)az_cell_y(w, c) - (int)az_cell_y(w, vc)); if (w->gy - dy < dy) dy = w->gy - dy;
        u32 dz = xy_only ? 0 : (u32)abs((int)az_cell_z(w, c) - (int)az_cell_z(w, vc));
        u32 d = dx > dy ? dx : dy; if (dz > d) d = dz;
        if (d <= R) return true;
    }
    return false;
}

static u32 gcd(u32 a, u32 b) { while (b) { u32 t = a % b; a = b; b = t; } return a; }

static u32 live_count(const AzWorld *w)
{
    u32 n = 0;
    for (u32 s = 0; s < w->n_slots; s++) n += (w->flags[s] & AZ_F_ALIVE) != 0;
    return n;
}

/* B9 at any parameters: a founder holds what its body can hold and no more: PART_HOLD per part
 * (at most CELL_HOLD) of matter, and energy worth that matter (metab's rule) */
static u32 pools_bad(const AzWorld *w)
{
    u32 bad = 0;
    for (u32 s = 0; s < w->n_slots; s++) {
        if (!(w->flags[s] & AZ_F_ALIVE)) continue;
        u32 np;
        (void)az_parts(w, s, &np);
        u64 hold = (u64)P(w, AZ_WP_PART_HOLD) * np;
        if (hold > P(w, AZ_WP_CELL_HOLD)) hold = P(w, AZ_WP_CELL_HOLD);
        const u64 e_hold = hold * AZ_EPS_BIO > 0x3FFFFFFFull ? 0x3FFFFFFFull : hold * AZ_EPS_BIO;
        const u64 e = WP(FOUNDER_ENERGY);
        const u64 pool = ((u64)WP(FOUNDER_POOL) * (w->trait[(size_t)s * AZ_TRAITS + AZ_T_SIZE] + 16u)) / 144u;
        bad += np == 0;
        bad += (u64)w->energy[s] != (e < e_hold ? e : e_hold);
        bad += w->pool_a[s] != (pool < hold ? pool : hold);
        bad += w->mass[s] != w->pool_a[s] + w->pool_m[s] + w->pool_t[s];
    }
    return bad;
}

/* ---- contracts that hold for any parameters (A2-A5, A8-A10, B2) ---- */
static u32 oracle_S(const AzWorld *w);

/* A2 at any parameters: every open cell holds its layer's recipe (2.4.2) of the five species no
 * feature adds: the ocean's below S, the air's from S up, scattered by its own patchiness draw
 * and squeezed by the rock of a surface cell */
static u32 layer_bad(const AzWorld *w, u32 S)
{
    static const u32 kinds[5] = { AZ_SP_SOLV, AZ_SP_GAS, AZ_SP_OXI, AZ_SP_ORG_Y, AZ_SP_WST_W };
    const u64 dis = WP(DISSOLVE);
    const i32 patch = (i32)WP(PATCHINESS);
    u32 bad = 0;
    for (u32 col = 0; col < ncol(w); col++) {
        const u32 H = col_H(w, col);
        for (u32 z = H; z < w->gz; z++) {
            const u32 c = cellz(w, col, z), solid = w->c_solid[c];
            u64 a[AZ_NSPECIES] = { 0 };
            if (z < S) {
                a[AZ_SP_SOLV] = WP(OCEAN_SOLV); a[AZ_SP_GAS] = ((u64)WP(AIR_GAS) * dis) >> 8; a[AZ_SP_OXI] = WP(OCEAN_OXI);
                a[AZ_SP_ORG_Y] = z == H ? (u64)WP(FLOOR_ORG) : ((u64)WP(FLOOR_ORG) * dis) >> 8; a[AZ_SP_WST_W] = WP(OCEAN_WST);
            } else {
                a[AZ_SP_SOLV] = ((u64)WP(OCEAN_SOLV) * dis) >> 8; a[AZ_SP_GAS] = WP(AIR_GAS); a[AZ_SP_OXI] = WP(AIR_OXI);
            }
            u32 r[4];
            az_rand4(w, c, AZ_RS_WORLDGEN, I_AMOUNT, r);
            for (u32 i = 0; i < 5; i++) {
                const u32 k = kinds[i];
                const i32 b = (i32)((r[k >> 2] >> (8u * (k & 3u))) & 255u);
                u64 v = (a[k] * (u64)(256 + ((b - 128) * patch) / 128)) >> 8;
                if (solid) v = (v * (255u - solid)) / 255u;
                if (v > 0x7FFFFFFFu) v = 0x7FFFFFFFu;
                bad += sp(w, c)[k] != v;
            }
        }
    }
    return bad;
}

/* a snapshot of the new world saves, loads and gives the same world */
static bool snap_round(const AzWorld *w)
{
    const u64 need = az_snapshot_bytes(w);
    if (!need) return false;
    u8 *buf = (u8 *)malloc((size_t)need);
    bool ok = false;
    if (buf && az_snapshot_save(w, buf, need) == need) {
        const u64 bytes = az_world_bytes(&w->cfg);
        void *blk = az_test_locked_block(bytes);
        AzArena ar = { (u8 *)blk, bytes, 0 };
        AzWorld *l = blk ? az_snapshot_load(&ar, buf, need, NULL) : NULL;
        ok = l && az_state_hash(l) == az_state_hash(w);
        az_test_locked_block_free(blk);
    }
    free(buf);
    return ok;
}

/* B8: every founder in its band: FLOOR and VENTFLOOR on z == H, PHOTIC, MID and AIR inside the
 * band of its own column (a founder whose column lacks the band takes its patch centre, which
 * lies in the band of the centre's column); a chemotroph within REACH of a vent in x, y and z,
 * in a cell with vent mineral */
static u32 bands_bad(const AzWorld *w, u32 S, u32 *chemo, u32 *floor_n, u32 *air_n)
{
    const bool mid = has_mid(w, S);
    const u32 R = WP(REACH);
    u32 bad = 0;
    *chemo = *floor_n = *air_n = 0;
    for (u32 s = 0; s < w->n_slots; s++) {
        if (!(w->flags[s] & AZ_F_ALIVE)) continue;
        const Niche n = oracle_niche(w, w->lineage[s], mid);
        const u32 c = w->cell[s], z = az_cell_z(w, c), H = col_H(w, colof(w, c));
        const u32 D = H < S ? S - H : 0u;
        u32 Pp = (u32)(((u64)D * WP(PHOTIC) + 255) >> 8);
        if (Pp < 1) Pp = 1;
        if (Pp > D) Pp = D;
        if (n.guild == G_CHEMO) {
            (*chemo)++;
            bad += !near_vent(w, c, R, false);
            bad += WP(VENT_NUT) >= R + 1 && sp(w, c)[AZ_SP_NUT_X] == 0;
        }
        switch (n.band) {
        case B_FLOOR: case B_VENTFLOOR: (*floor_n)++; bad += z != H || H >= S; break;
        case B_AIR: (*air_n)++; bad += z < S; break;
        case B_PHOTIC: bad += H >= S || z < S - Pp || z > S - 1; break;
        default: bad += H >= S || z < H + 1 || z + Pp + 1 > S; break;
        }
    }
    return bad;
}

static u32 oracle_S(const AzWorld *w)
{
    Terrain *o = (Terrain *)calloc(1, sizeof *o);
    if (!o) return 0;
    oracle_terrain(w, o);
    const u32 S = o->S;
    free(o);
    return S;
}

static void contracts(const AzWorld *w)
{
    const u32 S = oracle_S(w);
    CHECK_EQ(layer_bad(w, S), 0);
    u32 chemo, floor_n, air_n;
    CHECK_EQ(bands_bad(w, S, &chemo, &floor_n, &air_n), 0);
    u64 down = 0;
    u32 bad_bed = 0, bad_amount = 0, bad_press = 0, bad_temp = 0, bad_acc = 0, bad_grav = 0, bad_lat = 0;
    const i32 lat = (i32)(WP(GRAV_BEND) + WP(GRAV_SWIRL));
    for (u32 col = 0; col < ncol(w); col++) {
        const u32 H = col_H(w, col);
        for (u32 z = 0; z < w->gz; z++) {
            const u32 c = cellz(w, col, z);
            if ((w->c_solid[c] == 255u) != (z < H)) bad_bed++;     /* bedrock contiguous from the bottom */
            if (w->c_solid[c] == 255u) {
                for (u32 k = 0; k < AZ_NSPECIES; k++) bad_bed += sp(w, c)[k] != 0;
                bad_bed += w->c_heat[c] != 0;
            }
            for (u32 k = 0; k < AZ_NSPECIES; k++) bad_amount += sp(w, c)[k] >= 0x80000000u;
            bad_amount += w->c_heat[c] == 0xFFFFFFFFu;
            bad_press += az_cell_pressure(w, c) == 0xFFFFFFFFu;
            bad_temp += w->c_temp[c] < 0;
            const AzCellAcc *a = &w->c_acc[c];
            for (u32 k = 0; k < AZ_NSPECIES; k++) bad_acc += a->delta[k] != 0;
            bad_acc += a->dheat != 0;
            const i8 *g = &w->c_grav[(size_t)c * 4];
            bad_grav += g[2] > -1 || g[3] != 0;
            bad_lat += abs(g[0]) > lat || abs(g[1]) > lat;
            down += (u64)(-(i32)g[2]);
        }
    }
    CHECK_EQ(bad_bed, 0); CHECK_EQ(bad_amount, 0); CHECK_EQ(bad_press, 0); CHECK_EQ(bad_temp, 0);
    CHECK_EQ(bad_acc, 0); CHECK_EQ(bad_grav, 0); CHECK_EQ(bad_lat, 0);
    const AzTally *ty = &w->workers[0].tally;
    CHECK_EQ(ty->reservoir, 0); CHECK_EQ(ty->energy_in, 0); CHECK_EQ(ty->energy_out, 0);
    AzTally zero;
    memset(&zero, 0, sizeof zero);
    u32 busy = 0;                                             /* no tally waits for a merge: the world is between ticks */
    for (u32 k = 0; k < AZ_MAX_WORKERS; k++) busy += memcmp(&w->workers[k].tally, &zero, sizeof zero) != 0;
    CHECK_EQ(busy, 0);
    CHECK(snap_round(w));
    CHECK(w->gravity >= 1);
    CHECK_EQ(w->gravity, down / w->n_cells);
    /* A8 vents */
    CHECK(w->vent_count >= 1 && w->vent_count <= 64);
    u32 n_ocean = 0;
    for (u32 col = 0; col < ncol(w); col++) n_ocean += col_H(w, col) < S;
    u64 V = ((u64)WP(VENTS) * n_ocean + 1023) / 1024;
    if (V < 1) V = 1;
    if (V > n_ocean) V = n_ocean;
    if (V > 64) V = 64;
    CHECK_EQ(w->vent_count, V);
    u32 bad_vent = 0;
    for (u32 v = 0; v < w->vent_count; v++) {
        const u32 c = w->vent_cell[v], col = colof(w, c);
        bad_vent += c >= w->n_cells || w->c_solid[c] == 255u || az_cell_z(w, c) != col_H(w, col) || col_H(w, col) >= S;
        bad_vent += sp(w, c)[AZ_SP_NUT_X] < WP(VENT_NUT);
        for (u32 u = 0; u < v; u++) bad_vent += w->vent_cell[u] == c;
    }
    CHECK_EQ(bad_vent, 0);
    CHECK_EQ(w->totals.reservoir_mu, (u64)WP(RESERVOIR) * w->vent_count);
    /* A9 suns */
    CHECK(w->sun_period[0] >= 256 && w->sun_period[1] >= 256);
    CHECK_EQ(gcd(w->sun_period[0], w->sun_period[1]), 1);
    CHECK_EQ(w->sun_period[0], WP(SUN0_DAY));
    /* B2 founders */
    u32 bad_f = 0;
    for (u32 s = 0; s < w->n_slots; s++) {
        if (!(w->flags[s] & AZ_F_ALIVE)) continue;
        bad_f += w->flags[s] != AZ_F_ALIVE || w->blk[s] == 0;
        bad_f += w->cell[s] != az_cell_at(w, w->pos_x[s], w->pos_y[s], w->pos_z[s]);
        bad_f += w->c_solid[w->cell[s]] == 255u;
        bad_f += w->birth_tick[s] != 0 || w->parent_a[s] != 0;
        bad_f += w->pos_z[s] < (1u << 16) || w->pos_z[s] > 0xFFFFFFFFu - (1u << 16);
    }
    CHECK_EQ(bad_f, 0);
}

static bool run_books(AzWorld *w, u32 ticks)
{
    const u64 m0 = az_total_matter(w);
    const i64 e0 = az_total_energy(w);
    bool ok = true;
    for (u32 i = 0; i < ticks; i++) {
        az_tick(w);
        if (az_total_matter(w) != m0 || az_total_energy(w) != e0) ok = false;
    }
    return ok;
}

/* ---- A. the world ---- */
static void part_a(AzWorld *w)
{
    Terrain *o = (Terrain *)calloc(1, sizeof *o);
    if (!o) { CHECK(!"oracle"); return; }
    oracle_terrain(w, o);
    const u32 S = o->S, R = WP(REACH);
    /* A1 */
    u32 bad_h = 0, bad_surf = 0, bad_s = 0;
    for (u32 col = 0; col < ncol(w); col++) {
        const u32 H = col_H(w, col);
        bad_h += H != o->H[col];
        bad_surf += w->c_solid[cellz(w, col, H)] != (u8)(((u64)o->rem[col] * WP(SURFACE_SOLID)) >> 16);
        u32 wet = 0;
        for (u32 z = H; z < w->gz; z++) wet += sp(w, cellz(w, col, z))[AZ_SP_SOLV] > sp(w, cellz(w, col, z))[AZ_SP_GAS];
        bad_s += wet != (H < S ? S - H : 0u);
    }
    CHECK_EQ(bad_h, 0); CHECK_EQ(bad_surf, 0); CHECK_EQ(bad_s, 0);
    CHECK_EQ(world_S(w), S);
    u32 distinct_h = 0;
    for (u32 col = 1; col < ncol(w); col++) distinct_h += o->H[col] != o->H[0];
    CHECK(distinct_h > 0);                                    /* a terrain, not a plain */
    /* A2 layer order */
    u32 bad_layer = 0, ocean = 0, air = 0;
    for (u32 col = 0; col < ncol(w); col++) {
        const u32 H = col_H(w, col);
        for (u32 z = H; z < w->gz; z++) {
            const u32 c = cellz(w, col, z);
            bad_layer += w->c_solid[c] == 255u;
            if (z < S) { ocean++; bad_layer += AZ_PHASE_OF(w->c_phase[c], AZ_SP_SOLV) != AZ_PH_LIQUID; }
            else { air++; bad_layer += sp(w, c)[AZ_SP_SOLV] >= sp(w, c)[AZ_SP_GAS]; }
        }
    }
    CHECK_EQ(bad_layer, 0);
    CHECK(ocean > 0 && air > 0);
    /* A2 on larger grids, where vents lie closer together: the ocean is liquid there too */
    for (u32 g = 0; g < 2; g++) {
        AzTestWorld big;
        if (gen_ex(&big, 91, 64, 16, 5 + g, 5 + g, 4 + g, NULL, 0, 0, 0, 0)) {
            const u32 bS = oracle_S(big.w);
            u32 boil = 0, wet = 0;
            for (u32 c = 0; c < big.w->n_cells; c++) {
                if (big.w->c_solid[c] == 255u || az_cell_z(big.w, c) >= bS) continue;
                wet++;
                boil += AZ_PHASE_OF(big.w->c_phase[c], AZ_SP_SOLV) != AZ_PH_LIQUID;
            }
            CHECK(wet > 0);
            CHECK_EQ(boil, 0);
        } else CHECK(!"big world");
        az_test_world_free(&big);
    }
    /* A3-A5, A8-A10 contracts; A4: no saturated temperature */
    contracts(w);
    u32 hot = 0;
    for (u32 c = 0; c < w->n_cells; c++) hot += w->c_temp[c] == 32767;
    CHECK_EQ(hot, 0);
    /* A6 temperature field: depth, latitude and the plume of the nearest vent, falling off over
     * the reach by the Chebyshev distance (x and y wrap) */
    u32 bad_t = 0, nmid = 0, plumed = 0;
    i64 midsum = 0;
    for (u32 c = 0; c < w->n_cells; c++) {
        if (w->c_solid[c] == 255u) continue;
        const u32 y = az_cell_y(w, c), z = az_cell_z(w, c);
        i64 plume = 0;
        for (u32 v = 0; v < w->vent_count; v++) {
            const u32 vc = w->vent_cell[v];
            u32 dx = (u32)abs((int)az_cell_x(w, c) - (int)az_cell_x(w, vc)); if (w->gx - dx < dx) dx = w->gx - dx;
            u32 dy = (u32)abs((int)y - (int)az_cell_y(w, vc)); if (w->gy - dy < dy) dy = w->gy - dy;
            u32 d = (u32)abs((int)z - (int)az_cell_z(w, vc));
            if (dx > d) d = dx;
            if (dy > d) d = dy;
            if (d > R) continue;
            const i64 pv = ((i64)WP(TEMP_VENT) * (i64)(R + 1 - d)) / (i64)(R + 1);
            if (pv > plume) plume = pv;
        }
        plumed += plume > 0;
        i64 T;
        const i64 d = (i64)S - 1 - (i64)z, span = (i64)o->Dmax - 1;
        if (z < S) T = (i64)WP(TEMP_MID) + ((i64)WP(TEMP_DEPTH) * (span - 2 * d)) / (2 * (span > 1 ? span : 1));
        else T = (i64)WP(TEMP_MID) + (i64)WP(TEMP_DEPTH) / 2;
        const i64 ey = llabs(2 * (i64)y + 1 - (i64)w->gy);
        const i64 lat = ((i64)WP(TEMP_POLE) * ((i64)w->gy - 1 - 2 * ey)) / (2 * (i64)w->gy);
        T += lat + plume;
        if (T < 0) T = 0;
        if (T > 32767) T = 32767;
        u64 m = 0;
        for (u32 k = 0; k < AZ_NSPECIES; k++) m += sp(w, c)[k];
        if (llabs((i64)w->c_temp[c] - T) > 1 + (i64)(1024 / (m + 16))) bad_t++;
        if (z < S && llabs(2 * d - span) <= 1 && !plume) { midsum += (i64)w->c_temp[c] - lat; nmid++; }
    }
    CHECK_EQ(bad_t, 0);
    CHECK(plumed > w->vent_count);
    CHECK(nmid > 0);
    if (nmid) CHECK(llabs(midsum / (i64)nmid - (i64)WP(TEMP_MID)) <= 256);
    u32 warm = 0;
    for (u32 v = 0; v < w->vent_count; v++) {
        const u32 vc = w->vent_cell[v], z = az_cell_z(w, vc);
        i64 t = 0; u32 n = 0;
        for (u32 col = 0; col < ncol(w); col++) {
            const u32 c = cellz(w, col, z);
            if (w->c_solid[c] != 255u) { t += w->c_temp[c]; n++; }
        }
        warm += n && (i64)w->c_temp[vc] * n > t;
    }
    CHECK_EQ(warm, w->vent_count);
    /* A7 the derived columns are current */
    const size_t nc = w->n_cells;
    u8 *light = (u8 *)malloc(nc); i16 *temp = (i16 *)malloc(nc * 2); u16 *phase = (u16 *)malloc(nc * 2);
    u8 *med = (u8 *)malloc(nc * 2); i8 *lvec = (i8 *)malloc(nc * 4);
    if (light && temp && phase && med && lvec) {
        memcpy(light, w->c_light, nc); memcpy(temp, w->c_temp, nc * 2); memcpy(phase, w->c_phase, nc * 2);
        memcpy(med, w->c_med, nc * 2); memcpy(lvec, w->c_lvec, nc * 4);
        for (u32 c = w->n_cells; c-- > 0u;) az_weather_derive(w, c);
        for (u32 c = 0; c < w->n_cells; c++) az_chem_derive(w, c);
        CHECK(memcmp(light, w->c_light, nc) == 0); CHECK(memcmp(temp, w->c_temp, nc * 2) == 0);
        CHECK(memcmp(phase, w->c_phase, nc * 2) == 0); CHECK(memcmp(med, w->c_med, nc * 2) == 0);
        CHECK(memcmp(lvec, w->c_lvec, nc * 4) == 0);
    } else CHECK(!"buffers");
    free(light); free(temp); free(phase); free(med); free(lvec);
    u32 bad_rock = 0, lit_top = 0;
    for (u32 c = 0; c < w->n_cells; c++)
        if (w->c_solid[c] == 255u) bad_rock += w->c_med[(size_t)c * 2] != 255 || w->c_med[(size_t)c * 2 + 1] != 255 || w->c_phase[c] != 0;
    for (u32 col = 0; col < ncol(w); col++) lit_top += w->c_light[cellz(w, col, w->gz - 1)] > 0;
    CHECK_EQ(bad_rock, 0);
    CHECK_EQ(lit_top, ncol(w));                               /* sun 0 is up at tick 0: the sky lights the top layer */
    /* A10 gravity is a field */
    u32 g0v[256] = { 0 }, g2v[256] = { 0 }, n0 = 0, n2 = 0;
    for (u32 c = 0; c < w->n_cells; c++) {
        const i8 *g = &w->c_grav[(size_t)c * 4];
        if (!g0v[(u8)g[0]]++) n0++;
        if (!g2v[(u8)g[2]]++) n2++;
    }
    CHECK(n0 >= 2 && n2 >= 2);
    u32 hmax = 0;
    for (u32 col = 0; col < ncol(w); col++) if (o->H[col] > hmax) hmax = o->H[col];
    printf("world: 16x16x8, sea level %u, terrain %u..%u, %u vents, reservoir %llu mu, suns %u and %u ticks, "
           "gravity %u with %u lateral and %u vertical values\n",
           S, o->Hmin, hmax, w->vent_count, (unsigned long long)w->totals.reservoir_mu,
           w->sun_period[0], w->sun_period[1], w->gravity, n0, n2);
    free(o);
    /* A10 without bend and swirl the lateral pull is 0 */
    AzTestWorld f;
    const Ov flat[2] = { { AZ_WP_WORLDGEN_GRAV_BEND, 0 }, { AZ_WP_WORLDGEN_GRAV_SWIRL, 0 } };
    if (gen(&f, 91, 64, 8, flat, 2)) {
        u32 lateral = 0;
        for (u32 c = 0; c < f.w->n_cells; c++) lateral += f.w->c_grav[(size_t)c * 4] != 0 || f.w->c_grav[(size_t)c * 4 + 1] != 0;
        CHECK_EQ(lateral, 0);
        CHECK_EQ(f.w->gravity, WP(GRAV));
    } else CHECK(!"flat world");
    az_test_world_free(&f);
}

/* ---- B. founders ---- */
static void part_b(AzWorld *w)
{
    const u32 N = w->cfg.n_initial, nsp = w->cfg.n_species;
    const u32 S = world_S(w), R = WP(REACH);
    const bool mid = has_mid(w, S);
    /* B1 */
    CHECK_EQ(live_count(w), N);
    u32 per[100] = { 0 }, sum = 0;
    for (u32 s = 0; s < w->n_slots; s++)
        if ((w->flags[s] & AZ_F_ALIVE) && w->lineage[s] < 100u) per[w->lineage[s]]++;
    for (u32 k = 0; k < nsp; k++) { CHECK_EQ(per[k], N / nsp + (k < N % nsp)); sum += per[k]; }
    CHECK_EQ(sum, live_count(w));
    /* B2 in contracts(); B3 developed */
    u32 changed = 0;
    u8 row[AZ_TRAITS], ph[AZ_PHEN];
    for (u32 s = 0; s < w->n_slots; s++) {
        if (!(w->flags[s] & AZ_F_ALIVE)) continue;
        memcpy(row, &w->trait[(size_t)s * AZ_TRAITS], AZ_TRAITS);
        memcpy(ph, &w->phen[(size_t)s * AZ_PHEN], AZ_PHEN);
        const u16 tag = w->tag[s];
        az_develop(w, s);
        changed += memcmp(row, &w->trait[(size_t)s * AZ_TRAITS], AZ_TRAITS) != 0 || memcmp(ph, &w->phen[(size_t)s * AZ_PHEN], AZ_PHEN) != 0
                || tag != w->tag[s];
    }
    CHECK_EQ(changed, 0);
    /* B4 clades */
    u64 refs = 0;
    for (u32 c = 0; c < w->n_clades; c++) refs += w->clades[c].refcount;
    CHECK_EQ(refs, live_count(w));
    u32 bad_clade = 0;
    for (u32 k = 0; k < nsp; k++) {
        const u32 p = first_of(w, k);
        if (p == AZ_NONE) { bad_clade++; continue; }
        const u32 cl = w->clade[p];
        u32 n = 0;
        for (u32 s = 0; s < w->n_slots; s++)
            if ((w->flags[s] & AZ_F_ALIVE) && w->lineage[s] == k) { n++; bad_clade += w->clade[s] != cl; }
        bad_clade += w->clades[cl].refcount != n || w->clades[cl].species != k;
    }
    CHECK_EQ(bad_clade, 0);
    /* B5 guilds and modes */
    u32 modes = 0, guilds = 0;
    for (u32 k = 0; k < nsp; k++) {
        u32 mc[8] = { 0 };
        for (u32 s = 0; s < w->n_slots; s++)
            if ((w->flags[s] & AZ_F_ALIVE) && w->lineage[s] == k) mc[w->phen[(size_t)s * AZ_PHEN + AZ_P_MATE] & 7]++;
        u32 best = 0;
        for (u32 m = 1; m < 8; m++) if (mc[m] > mc[best]) best = m;
        modes |= 1u << best;
        const u32 p = first_of(w, k);
        if (p != AZ_NONE) guilds |= 1u << guild_of(w, p);
    }
    CHECK_EQ(modes, 0xFF);
    CHECK_EQ(guilds, 0x1F);
    /* B7 variation (VARY_LOCI default 3): some two founders of one species differ outside the
     * markers; both are copies (not the prototype, which is varied last), so each carries
     * variation of its own */
    u32 differ = 0;
    const u32 proto0 = first_of(w, 0);
    u32 p0 = AZ_NONE;
    for (u32 s = 0; s < w->n_slots; s++)
        if ((w->flags[s] & AZ_F_ALIVE) && w->lineage[s] == 0 && s != proto0 && (p0 == AZ_NONE || w->uid[s] < w->uid[p0])) p0 = s;
    for (u32 s = 0; s < w->n_slots && p0 != AZ_NONE; s++) {
        if (!(w->flags[s] & AZ_F_ALIVE) || w->lineage[s] != 0 || s == p0 || s == proto0) continue;
        for (u32 h = 0; h < 2; h++) {
            u32 la, lb;
            const AzGene *a = (const AzGene *)az_seg(w, s, AZ_SEG_GEN_A + h, &la);
            const AzGene *b = (const AzGene *)az_seg(w, p0, AZ_SEG_GEN_A + h, &lb);
            for (u32 i = 0; i < la / 4 && i < lb / 4; i++) {
                bool marker = false;
                for (u32 q = 0; q < 4; q++) marker = marker || w->code.marker_locus[q] == a[i].fam;
                differ += !marker && a[i].allele != b[i].allele;
            }
        }
    }
    CHECK(differ > 0);
    u32 bad_ratio = 0, judged = 0;
    for (u32 k = 0; k < nsp; k++) {
        u32 n = 0, one = 0;
        for (u32 s = 0; s < w->n_slots; s++)
            if ((w->flags[s] & AZ_F_ALIVE) && w->lineage[s] == k) { n++; one += w->sex[s]; }
        if (n < 64) continue;
        judged++;
        bad_ratio += one * 4 < n || one * 4 > 3 * n;
    }
    CHECK(judged > 0);
    CHECK_EQ(bad_ratio, 0);
    /* B8 habitat bands (every band, chemotrophs by a vent in three dimensions) */
    u32 chemo, floor_n, air_n;
    CHECK_EQ(bands_bad(w, S, &chemo, &floor_n, &air_n), 0);
    CHECK(chemo > 0 && floor_n > 0);
    (void)R; (void)mid;
    /* B9 pools */
    u32 bad_pool = 0;
    for (u32 s = 0; s < w->n_slots; s++) {
        if (!(w->flags[s] & AZ_F_ALIVE)) continue;
        const u32 size = w->trait[(size_t)s * AZ_TRAITS + AZ_T_SIZE];
        bad_pool += w->energy[s] != (i32)WP(FOUNDER_ENERGY);
        bad_pool += w->pool_a[s] != (u32)(((u64)WP(FOUNDER_POOL) * (size + 16)) / 144);
        bad_pool += w->mass[s] != w->pool_a[s] + w->pool_m[s] + w->pool_t[s];
        bad_pool += w->health[s] != P(w, AZ_WP_LIFE_BIRTH_HEALTH);
    }
    CHECK_EQ(bad_pool, 0);
    CHECK_EQ(pools_bad(w), 0);
    printf("founders: %u of %u, %u species, all 8 modes and all 5 guilds present; %u chemotrophs by a vent, "
           "%u on the floor, %u in the air\n", live_count(w), N, nsp, chemo, floor_n, air_n);

    /* B6 the fit (and B7 without variation): prototypes unvaried, 4 founders per species */
    AzTestWorld f;
    const Ov still[1] = { { AZ_WP_WORLDGEN_VARY_LOCI, 0 } };
    if (gen(&f, 91, 400, 100, still, 1)) {
        u32 worst, mean;
        CHECK(fit_errors(f.w, 100, &worst, &mean));
        CHECK(worst <= FIT_TOL);
        CHECK(mean <= FIT_MEAN_X1000);
        u32 diff = 0;
        for (u32 k = 0; k < 100; k++) {
            const u32 p = first_of(f.w, k);
            for (u32 s = 0; s < f.w->n_slots && p != AZ_NONE; s++) {
                if (!(f.w->flags[s] & AZ_F_ALIVE) || f.w->lineage[s] != k) continue;
                for (u32 h = 0; h < 2; h++) {
                    u32 la, lb;
                    const AzGene *a = (const AzGene *)az_seg(f.w, s, AZ_SEG_GEN_A + h, &la);
                    const AzGene *b = (const AzGene *)az_seg(f.w, p, AZ_SEG_GEN_A + h, &lb);
                    for (u32 i = 0; i < la / 4 && i < lb / 4; i++) {
                        bool marker = false;
                        for (u32 q = 0; q < 4; q++) marker = marker || f.w->code.marker_locus[q] == a[i].fam;
                        diff += !marker && (a[i].allele != b[i].allele || a[i].expr != b[i].expr);
                    }
                }
            }
        }
        CHECK_EQ(diff, 0);
        CHECK_EQ(side_bad(f.w, 100), 0);                      /* no core trait on the wrong side of the middle */
        u32 press, dens, comfort;                              /* every founder fits the cells it was put in */
        habitat_fit(f.w, &press, &dens, &comfort);
        CHECK_EQ(press, 0); CHECK_EQ(dens, 0); CHECK_EQ(comfort, 0);
        {   /* the same without the pressure margin, so the margin cannot hide a habitat measured too
             * narrowly (the deepest cell a founder may be put in, not only the patch centres) */
            AzTestWorld e;
            const Ov bare[2] = { { AZ_WP_WORLDGEN_VARY_LOCI, 0 }, { AZ_WP_WORLDGEN_PRESS_MARGIN, 0 } };
            if (gen(&e, 91, 2000, 16, bare, 2)) {
                u32 p2, d2, c2;
                habitat_fit(e.w, &p2, &d2, &c2);
                CHECK_EQ(p2, 0); CHECK_EQ(d2, 0); CHECK_EQ(c2, 0);
            } else CHECK(!"bare world");
            az_test_world_free(&e);
        }
        u8 *drawn = (u8 *)malloc((size_t)100 * AZ_TRAITS);   /* the fit leaves the rest of the genome be */
        Collateral col = { 0, 0, 0, 0, 0 };
        if (drawn && drawn_traits(91, 100, drawn)) {
            col = collateral(f.w, 100, drawn);
            CHECK(col.worst_rest_x1000 <= REST_MEAN_X1000);
            CHECK(col.ext_fit <= col.ext_drawn);
        } else CHECK(!"drawn genomes");
        free(drawn);
        printf("fit: 100 prototypes, worst core trait %u from its target, worst mean %u.%03u (tolerances %u and %u.%03u); "
               "the founders bear their pressure, sit on their side of the medium and inside their comfort; the traits the "
               "niche leaves free moved %u.%03u at worst (tolerance %u.%03u), %u at 0 or 255 against %u drawn, sense range "
               "below 16 in %u against %u drawn\n",
               worst, mean / 1000, mean % 1000, FIT_TOL, FIT_MEAN_X1000 / 1000, FIT_MEAN_X1000 % 1000,
               col.worst_rest_x1000 / 1000, col.worst_rest_x1000 % 1000, REST_MEAN_X1000 / 1000, REST_MEAN_X1000 % 1000,
               col.ext_fit, col.ext_drawn, col.sense_fit, col.sense_drawn);
    } else CHECK(!"still world");
    az_test_world_free(&f);
}

/* ---- C. modes ---- */
typedef struct Wrap {
    AzHost host; u64 script; void *block; AzArena arena; AzWorld *w;
} Wrap;
static void wrap_pfor(void *ctx, AzWorld *w, AzJobFn fn, const u32 *splits, u32 n)
{
    (void)ctx;
    for (u32 k = 0; k < n; k++) fn(w, splits[k], splits[k + 1], 0);
}
static u64 wrap_now(void *ctx) { (void)ctx; return 0; }
static bool wrap_entropy(void *ctx, void *buf, u32 bytes)
{
    Wrap *x = (Wrap *)ctx;
    u8 *p = (u8 *)buf;
    for (u32 i = 0; i < bytes; i += 8) {
        const u64 v = az_test_rand64(&x->script);
        memcpy(p + i, &v, bytes - i < 8 ? bytes - i : 8);
    }
    return true;
}
static u64 wild_digest(u64 script, u64 seed)
{
    Wrap x;
    memset(&x, 0, sizeof x);
    x.script = script;
    x.host.ctx = &x; x.host.n_workers = 1; x.host.parallel_for = wrap_pfor; x.host.now_ns = wrap_now;
    x.host.entropy = wrap_entropy;
    AzConfig cfg = az_test_config(seed, 300);
    cfg.mode = AZ_MODE_WILD;
    cfg.n_species = 16;
    const u64 bytes = az_world_bytes(&cfg);
    x.block = az_test_locked_block(bytes);
    u64 d = 0;
    if (x.block) {
        x.arena.base = (u8 *)x.block; x.arena.size = bytes; x.arena.used = 0;
        x.w = az_world_create(&x.arena, &cfg, &x.host);
        if (x.w) { az_worldgen(x.w); d = digest(x.w); }
        az_test_locked_block_free(x.block);
    }
    CHECK(d != 0);
    return d;
}

static void part_c(void)
{
    AzTestWorld a = {0}, b = {0};
    /* C1, C2 */
    if (!gen(&a, 91, 500, 16, NULL, 0)) {
        CHECK(!"world 91");
        az_test_world_free(&a);
        return;
    }
    if (gen(&b, 91, 500, 16, NULL, 0)) {
        CHECK_EQ(az_state_hash(a.w), az_state_hash(b.w));
        CHECK_EQ(digest(a.w), digest(b.w));
    } else CHECK(!"worlds");
    az_test_world_free(&b);
    if (gen(&b, 92, 500, 16, NULL, 0)) {
        CHECK(digest(a.w) != digest(b.w));
        u32 differ = 0;
        for (u32 col = 0; col < ncol(a.w); col++) differ += col_H(a.w, col) != col_H(b.w, col);
        CHECK(differ > 0);
    } else CHECK(!"world 92");
    az_test_world_free(&b);
    /* C4 snapshot */
    const u64 need = az_snapshot_bytes(a.w);
    u8 *buf = (u8 *)malloc((size_t)need);
    if (buf && az_snapshot_save(a.w, buf, need) == need) {
        const u64 bytes = az_world_bytes(&a.w->cfg);
        void *blk = az_test_locked_block(bytes);
        AzArena ar = { (u8 *)blk, bytes, 0 };
        AzWorld *l = blk ? az_snapshot_load(&ar, buf, need, NULL) : NULL;
        CHECK(l != NULL);
        if (l) { CHECK_EQ(az_state_hash(l), az_state_hash(a.w)); CHECK_EQ(digest(l), digest(a.w)); }
        az_test_locked_block_free(blk);
    } else CHECK(!"snapshot");
    free(buf);
    az_test_world_free(&a);
    /* C3 wild */
    const u64 s1 = wild_digest(0x1111, 5), s2 = wild_digest(0x2222, 5), s3 = wild_digest(0x1111, 6);
    CHECK(s1 != s2);
    CHECK_EQ(s1, s3);
    /* C5 threads */
    u64 h[2] = { 0, 0 };
    for (u32 k = 0; k < 2; k++) {
        AzTestWorld t;
        if (gen_ex(&t, 94, 2000, 16, 4, 4, 3, NULL, 0, k ? 3u : 0u, 0, 0)) {
            for (u32 i = 0; i < 50; i++) az_tick(t.w);
            h[k] = az_state_hash(t.w);
        } else CHECK(!"thread world");
        az_test_world_free(&t);
    }
    CHECK(h[0] != 0);
    CHECK_EQ(h[0], h[1]);
    printf("modes: one seed one world, two seeds two terrains, wild worlds follow their entropy and not the seed, "
           "a snapshot round trip, one hash on 0 and 3 workers after 50 ticks\n");
}

/* ---- D. books ---- */
static void part_d(void)
{
    u64 h[2] = { 0, 0 };
    u64 live = 0;
    u32 ocean = 0, boiled = 0;
    for (u32 k = 0; k < 2; k++) {
        AzTestWorld t;
        if (!gen_ex(&t, 93, 2000, 16, 4, 4, 3, NULL, 0, 0, 0, k ? AZ_LESION_GRAVITY : 0u)) { CHECK(!"world"); continue; }
        AzWorld *w = t.w;
        const u64 m0 = az_total_matter(w);
        const i64 e0 = az_total_energy(w);
        for (u32 i = 0; i < 200; i++) {
            az_tick(w);
            CHECK_EQ(az_total_matter(w), m0);
            CHECK_EQ(az_total_energy(w), e0);
        }
        CHECK_EQ(w->totals.field_underflow, 0);
        CHECK_EQ(w->totals.dropped_events, 0);
        h[k] = az_state_hash(w);
        if (!k) {
            live = w->n_live;
            /* the sea stays a sea: the floor under the lit ocean is not baked by light that rock
             * traps (rock holds no heat, az_weather.c), and the heat of the crowds boils only a few
             * cells (measured 0 to 3 percent on seeds 1-6; the sun-baked floor boiled 45 percent) */
            const u32 S = oracle_S(w);
            for (u32 c = 0; c < w->n_cells; c++) {
                if (w->c_solid[c] == 255u || az_cell_z(w, c) >= S) continue;
                ocean++;
                boiled += AZ_PHASE_OF(w->c_phase[c], AZ_SP_SOLV) != AZ_PH_LIQUID;
            }
            CHECK(ocean > 0);
            CHECK(boiled * 16u <= ocean);
        }
        az_test_world_free(&t);
    }
    CHECK(h[0] != h[1]);
    printf("books: 200 ticks exact every tick, %llu alive at the end, the solvent liquid in %u of %u ocean cells; the "
           "gravity lesion gives another world\n", (unsigned long long)live, ocean - boiled, ocean);
}

/* ---- E. edges ---- */
static void part_e(void)
{
    AzTestWorld t;
    /* E1 */
    if (gen(&t, 95, 64, 1, NULL, 0)) {
        contracts(t.w);
        CHECK_EQ(live_count(t.w), 64);
        u32 bad = 0;
        for (u32 s = 0; s < t.w->n_slots; s++) bad += (t.w->flags[s] & AZ_F_ALIVE) && t.w->lineage[s] != 0;
        CHECK_EQ(bad, 0);
        CHECK_EQ(t.w->clades[t.w->clade[0]].refcount, 64);
    } else CHECK(!"one species");
    az_test_world_free(&t);
    if (gen(&t, 95, 1000, 100, NULL, 0)) {
        contracts(t.w);
        CHECK_EQ(live_count(t.w), 1000);
        u32 modes = 0, guilds = 0;
        for (u32 k = 0; k < 100; k++) {
            const u32 p = first_of(t.w, k);
            if (p == AZ_NONE) continue;
            modes |= 1u << (t.w->phen[(size_t)p * AZ_PHEN + AZ_P_MATE] & 7);
            guilds |= 1u << guild_of(t.w, p);
        }
        CHECK_EQ(modes, 0xFF); CHECK_EQ(guilds, 0x1F);
        u64 refs = 0;
        for (u32 c = 0; c < t.w->n_clades; c++) refs += t.w->clades[c].refcount;
        CHECK_EQ(refs, 1000);
    } else CHECK(!"100 species");
    az_test_world_free(&t);
    /* E2 the smallest grid */
    if (gen_ex(&t, 96, 64, 8, 2, 2, 1, NULL, 0, 0, 0, 0)) {
        contracts(t.w);
        u32 bad = 0;
        for (u32 col = 0; col < ncol(t.w); col++) bad += col_H(t.w, col) != 0;
        CHECK_EQ(bad, 0);
        CHECK_EQ(world_S(t.w), 1);
        for (u32 v = 0; v < t.w->vent_count; v++) CHECK_EQ(az_cell_z(t.w, t.w->vent_cell[v]), 0);
        CHECK(run_books(t.w, 20));
    } else CHECK(!"small grid");
    az_test_world_free(&t);
    /* E3 a tall grid */
    if (gen_ex(&t, 96, 64, 8, 2, 2, 9, NULL, 0, 0, 0, 0)) {
        contracts(t.w);
        u32 bad = 0;
        const u32 S = world_S(t.w);
        for (u32 col = 0; col < ncol(t.w); col++)
            for (u32 z = col_H(t.w, col); z < t.w->gz; z++) {
                const u32 c = cellz(t.w, col, z);
                bad += (z < S) != (sp(t.w, c)[AZ_SP_SOLV] > sp(t.w, c)[AZ_SP_GAS]);
            }
        CHECK_EQ(bad, 0);
        CHECK(run_books(t.w, 20));
    } else CHECK(!"tall grid");
    az_test_world_free(&t);
    /* E4 a store that refuses */
    u32 units = 0;
    if (gen(&t, 97, 64, 8, NULL, 0)) units = t.w->store_live / live_count(t.w);
    az_test_world_free(&t);
    if (units && gen_ex(&t, 97, 200, 8, 4, 4, 3, NULL, 0, 0, units * 10, 0)) {
        AzWorld *w = t.w;
        CHECK(w->totals.store_denied > 0);
        const u32 live = live_count(w);
        CHECK(live > 0 && live < 200);
        u32 bad = 0;
        for (u32 s = 0; s < w->n_slots; s++) bad += (w->flags[s] & AZ_F_ALIVE) && w->blk[s] == 0;
        CHECK_EQ(bad, 0);
        u64 refs = 0;
        for (u32 c = 0; c < w->n_clades; c++) refs += w->clades[c].refcount;
        CHECK_EQ(refs, live);
        contracts(w);
        CHECK(run_books(w, 20));
    } else CHECK(!"refusing store");
    az_test_world_free(&t);
    /* E5 founders that lack genes, and the smallest brains */
    const Ov loss[2] = { { AZ_WP_FOUNDER_GENE_LOSS, 128 }, { AZ_WP_BRAIN_FOUND_HID, 1 } };
    if (gen(&t, 98, 200, 16, loss, 2)) {
        AzWorld *w = t.w;
        contracts(w);
        CHECK_EQ(live_count(w), 200);
        u32 shorter = 0;
        for (u32 s = 0; s < w->n_slots; s++) {
            if (!(w->flags[s] & AZ_F_ALIVE)) continue;
            u32 la; (void)az_seg(w, s, AZ_SEG_GEN_A, &la);
            shorter += la < AZ_CORE_FAMS * 4u;
        }
        CHECK(shorter > 0);
        u64 refs = 0;
        for (u32 c = 0; c < w->n_clades; c++) refs += w->clades[c].refcount;
        CHECK_EQ(refs, 200);
        CHECK(run_books(w, 20));
    } else CHECK(!"gene loss");
    az_test_world_free(&t);
    /* E8 crowds: founders packed into few cells with the most energy and pool a founder may be
     * given hold what their bodies hold, so no cell takes a flood of heat or matter at once */
    const Ov rich[2] = { { AZ_WP_WORLDGEN_FOUNDER_ENERGY, az_param_desc(AZ_WP_WORLDGEN_FOUNDER_ENERGY)->hi },
                         { AZ_WP_WORLDGEN_FOUNDER_POOL, az_param_desc(AZ_WP_WORLDGEN_FOUNDER_POOL)->hi } };
    if (gen_ex(&t, 97, 20000, 16, 2, 2, 1, rich, 2, 0, 0, 0)) {
        CHECK_EQ(live_count(t.w), 20000);
        CHECK_EQ(pools_bad(t.w), 0);
        CHECK(run_books(t.w, 5));
        CHECK_EQ(t.w->totals.field_underflow, 0);
    } else CHECK(!"crowded world");
    az_test_world_free(&t);
    const Ov heap[4] = { rich[0], rich[1], { AZ_WP_WORLDGEN_REACH, 0 }, { AZ_WP_WORLDGEN_PATCHES, 1 } };
    if (gen(&t, 97, 4000, 1, heap, 4)) {
        u32 same = 0;
        for (u32 s = 0; s < t.w->n_slots; s++) same += (t.w->flags[s] & AZ_F_ALIVE) && t.w->cell[s] == t.w->cell[first_of(t.w, 0)];
        CHECK_EQ(same, 4000);                                 /* one species, one patch, no reach: one cell */
        CHECK_EQ(pools_bad(t.w), 0);
        CHECK(run_books(t.w, 5));
        CHECK_EQ(t.w->totals.field_underflow, 0);
    } else CHECK(!"heaped world");
    az_test_world_free(&t);
    /* E9 grids that are not square, n_species 0 and above 100, a lineage table smaller than the
     * species */
    for (u32 g = 0; g < 2; g++) {
        if (gen_ex(&t, 96, 300, 8, g ? 2 : 4, g ? 4 : 2, 3, NULL, 0, 0, 0, 0)) {
            contracts(t.w);
            Terrain *o = (Terrain *)calloc(1, sizeof *o);
            if (o) {
                oracle_terrain(t.w, o);
                u32 bad = 0;
                for (u32 col = 0; col < ncol(t.w); col++) bad += col_H(t.w, col) != o->H[col];
                CHECK_EQ(bad, 0);
            }
            free(o);
            CHECK(run_books(t.w, 10));
        } else CHECK(!"oblong world");
        az_test_world_free(&t);
    }
    for (u32 g = 0; g < 2; g++) {
        if (gen(&t, 95, g ? 1000 : 64, g ? 150 : 0, NULL, 0)) {
            contracts(t.w);
            CHECK_EQ(live_count(t.w), g ? 1000 : 64);
            u32 top = 0;
            for (u32 s = 0; s < t.w->n_slots; s++)
                if ((t.w->flags[s] & AZ_F_ALIVE) && t.w->lineage[s] > top) top = t.w->lineage[s];
            CHECK_EQ(top, g ? 99 : 0);                             /* 0 species is 1, more than 100 is 100 */
        } else CHECK(!"species count world");
        az_test_world_free(&t);
    }
    {
        AzConfig cfg = az_test_config(95, 200);
        cfg.n_species = 8;
        cfg.max_clades = 4;
        if (az_test_world_ex(&t, &cfg, 0, AZ_TEST_NO_WORLDGEN)) {
            az_worldgen(t.w);
            contracts(t.w);
            CHECK_EQ(live_count(t.w), 200);
            u64 refs = 0;
            for (u32 c = 0; c < t.w->n_clades; c++) refs += t.w->clades[c].refcount;
            CHECK_EQ(refs, 200);
            CHECK(run_books(t.w, 10));
        } else CHECK(!"small lineage table");
        az_test_world_free(&t);
    }
    /* E6 every parameter at both ends; E7 every parameter acts */
    u64 base = 0;
    if (gen(&t, 91, 200, 16, NULL, 0)) base = digest(t.w);
    az_test_world_free(&t);
    u32 params = 0, inert = 0;
    for (u32 id = 0; id < AZ_WP_COUNT; id++) {
        const AzParamDesc *d = az_param_desc(id);
        if (!d || strncmp(d->name, "WORLDGEN_", 9) != 0) continue;
        params++;
        for (u32 e = 0; e < 2; e++) {
            const Ov ov[1] = { { id, e ? d->hi : d->lo } };
            if (gen_ex(&t, 91, 64, 8, 2, 2, 3, ov, 1, 0, 0, 0)) {
                contracts(t.w);
                CHECK_EQ(pools_bad(t.w), 0);
                if (id == AZ_WP_WORLDGEN_GRAV && e) CHECK(t.w->totals.sat[AZ_SAT_FIELD] > 0);   /* clamps are counted */
                const bool ok = run_books(t.w, 10);
                CHECK(ok);
                if (!ok) printf("  books not exact with %s = %u\n", d->name, ov[0].v);
            } else CHECK(!"range world");
            az_test_world_free(&t);
            /* the niche table at this end, on unvaried founders (the variation is VARY_LOCI's own
             * business): every mode and guild among 8 species, no core trait past the middle */
            if (id == AZ_WP_WORLDGEN_VARY_LOCI) continue;
            const Ov still[2] = { { id, e ? d->hi : d->lo }, { AZ_WP_WORLDGEN_VARY_LOCI, 0 } };
            if (gen_ex(&t, 91, 64, 8, 2, 2, 3, still, 2, 0, 0, 0)) {
                u32 modes = 0, guilds = 0;
                for (u32 k = 0; k < 8; k++) {
                    const u32 p = first_of(t.w, k);
                    if (p == AZ_NONE) continue;
                    modes |= 1u << (t.w->trait[(size_t)p * AZ_TRAITS + AZ_T_MATING_MODE] >> 5);
                    guilds |= 1u << guild_of(t.w, p);
                }
                const u32 side = side_bad(t.w, 8);
                CHECK_EQ(modes, 0xFF); CHECK_EQ(guilds, 0x1F); CHECK_EQ(side, 0);
                if (modes != 0xFF || guilds != 0x1F || side)
                    printf("  niche table not whole with %s = %u (modes %02x, guilds %02x, %u past the middle)\n", d->name,
                           still[0].v, modes, guilds, side);
            } else CHECK(!"still range world");
            az_test_world_free(&t);
        }
        AzTestWorld r;
        AzConfig cfg = az_test_config(91, 0);
        if (az_test_world_ex(&r, &cfg, 0, AZ_TEST_NO_WORLDGEN)) {
            if (d->lo > 0) CHECK(!az_param_set(r.w, id, d->lo - 1));
            if (d->hi < 0xFFFFFFFFu) CHECK(!az_param_set(r.w, id, d->hi + 1));
            CHECK(az_param_set(r.w, id, d->lo) && az_param_set(r.w, id, d->hi));
        }
        az_test_world_free(&r);
        const Ov other[1] = { { id, d->hi != d->def ? d->hi : d->lo } };
        if (gen(&t, 91, 200, 16, other, 1)) {
            const bool acts = digest(t.w) != base;
            CHECK(acts);
            if (!acts) { inert++; printf("  %s = %u moves nothing\n", d->name, other[0].v); }
        } else CHECK(!"act world");
        az_test_world_free(&t);
    }
    CHECK(params >= 45);
    CHECK_EQ(inert, 0);
    printf("edges: 1 and 100 species, 4x4x2 and 4x4x512, a refusing store, gene loss, 20000 rich founders in 32 cells and "
           "4000 in one, 16x4 and 4x16, 0 and 150 species, 4 lineages for 8 species; %u parameters at both ends with "
           "books exact and the niche table whole, every one of them moves the world\n", params);
}

/* ---- the measurement of the fit tolerances (not a check) ---- */
static int measure_fit(int argc, char **argv)
{
    u32 worst = 0, worst_mean = 0, bad_modes = 0, side = 0, worst_rest = 0;
    u64 ext_fit = 0, ext_drawn = 0, sense_fit = 0, sense_drawn = 0;
    Ov ov[8] = { { AZ_WP_WORLDGEN_VARY_LOCI, 0 } };
    u32 nov = 1;
    for (int i = 2; i < argc && nov < 8; i++) {           /* NAME=value: another parameter for the measurement */
        const char *eq = strchr(argv[i], '=');
        for (u32 id = 0; eq && id < AZ_WP_COUNT; id++) {
            const char *nm = az_param_desc(id)->name;
            if (strlen(nm) == (size_t)(eq - argv[i]) && strncmp(nm, argv[i], (size_t)(eq - argv[i])) == 0) {
                ov[nov].id = id; ov[nov].v = (u32)strtoul(eq + 1, NULL, 10); nov++;
            }
        }
    }
    u8 *drawn = (u8 *)malloc((size_t)100 * AZ_TRAITS);
    if (!drawn) return 1;
    for (u64 seed = 1; seed <= 99; seed++) {
        AzTestWorld t;
        if (!gen(&t, seed, 100, 100, ov, nov) || !drawn_traits(seed, 100, drawn)) {
            printf("seed %llu: no world\n", (unsigned long long)seed); free(drawn); return 1;
        }
        u32 wo, me;
        bad_modes += !fit_errors(t.w, 100, &wo, &me);
        side += side_bad(t.w, 100);
        const Collateral c = collateral(t.w, 100, drawn);
        ext_fit += c.ext_fit; ext_drawn += c.ext_drawn; sense_fit += c.sense_fit; sense_drawn += c.sense_drawn;
        if (c.worst_rest_x1000 > worst_rest) worst_rest = c.worst_rest_x1000;
        if (wo > worst) worst = wo;
        if (me > worst_mean) worst_mean = me;
        printf("seed %llu: worst %u mean %u.%03u, free traits moved %u.%03u\n", (unsigned long long)seed, wo, me / 1000, me % 1000,
               c.worst_rest_x1000 / 1000, c.worst_rest_x1000 % 1000);
        az_test_world_free(&t);
    }
    free(drawn);
    printf("fit over seeds 1-99, 100 species: worst core error %u, worst mean %u.%03u, core traits past the middle %u, "
           "seeds with a wrong mode %u; the traits the niche leaves free moved %u.%03u at worst (mean over one prototype); "
           "traits at 0 or 255: %llu fitted against %llu drawn (%.2f and %.2f per prototype); SENSE_RANGE below 16: %llu "
           "fitted against %llu drawn, of 9900\n",
           worst, worst_mean / 1000, worst_mean % 1000, side, bad_modes, worst_rest / 1000, worst_rest % 1000,
           (unsigned long long)ext_fit, (unsigned long long)ext_drawn, (double)ext_fit / 9900.0, (double)ext_drawn / 9900.0,
           (unsigned long long)sense_fit, (unsigned long long)sense_drawn);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--measure-fit") == 0) return measure_fit(argc, argv);
    AzTestWorld a;
    if (gen(&a, 91, 2000, 16, NULL, 0)) {
        part_a(a.w);
        part_b(a.w);
    } else CHECK(!"world A");
    az_test_world_free(&a);
    part_c();
    part_d();
    part_e();
    return az_test_done();
}
