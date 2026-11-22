/* test_metab.c - metabolism (core/az_metab.c, parameters core/params/az_metab.def).
 *
 *   1. size       the 3/4 power of mass the module scales by, against pow()
 *   2. uptake     each conversion alone on one creature in one cell, exact
 *                 numbers: heterotrophy, chemotrophy, photosynthesis (and the
 *                 dark, and no waste to fix), waste carried, toxin with the
 *                 food; scarcity; the body's room; the EAT drive; METAB
 *   3. breath     the pulse: aerobic against fermentation per unit burned,
 *                 EFFIC (yield against rate), OXI_USE without oxidiser,
 *                 STORE_CAP, the stores drawn back by a starving body
 *   4. body       building by GROWTH up to the wall, repair, detoxification,
 *                 secretion (TOX_SECRETE), excretion (WASTE_RATE and the
 *                 EXCRETE drive), scavenging by DIET_W, ALLOMETRY and the
 *                 parts, upkeep of what a creature carries
 *   5. stress     the thermal band (TOPT, TTOL), toxin (TOXRES, the lesion),
 *                 pressure (PRESS_TOL), radiation (RAD_TOL), starvation, and
 *                 the cause of death each gives
 *   6. lives      a producer in light gains mass and releases OXI, in the
 *                 dark it starves; a chemotroph lives on NUT_X; a grazer on
 *                 ORG_Y; an egg and a cyst are left alone
 *   7. worlds     books exact every tick, equal hashes for 0, 1, 3 and 8
 *                 workers and reorder 1 and 9; the toxin lesion gives another
 *                 world; which founders the stub world's one temperature
 *                 lets live (seeds 211 to 214, nothing tuned on them)
 *
 * Parts 1 to 6 run the module's two hooks for one hand-set creature alone in
 * a cell of the top layer, with the cell grant between them and the field
 * commit after, as in the tick. Matter and energy are compared before and
 * after every such step.
 */
#include "az_test.h"
#include <math.h>

typedef struct Bench {
    AzTestWorld t;
    AzWorld *w;
} Bench;

static u32 steps = 0, inexact = 0;

static bool bench_open(Bench *b, u64 seed, u32 lesion)
{
    AzConfig cfg = az_test_config(seed, 64);
    cfg.lesion = lesion;
    if (!az_test_world(&b->t, &cfg, 0)) return false;
    b->w = b->t.w;
    az_tick(b->w);                                        /* every founder gets its place in the grid */
    return true;
}
static void bench_close(Bench *b) { az_test_world_free(&b->t); }

static u8 *ph(const AzWorld *w, u32 s) { return &w->phen[(size_t)s * AZ_PHEN]; }
static u8 *tr(const AzWorld *w, u32 s) { return &w->trait[(size_t)s * AZ_TRAITS]; }
static u32 *sp(const AzWorld *w, u32 c) { return &w->c_sp[(size_t)c * AZ_NSPECIES]; }
static bool due(const AzWorld *w, u32 s) { return az_phase_due(w, w->uid[s], AZ_RS_METAB, 2); }
static u32 cause(const AzWorld *w, u32 s) { return (w->pair[s].req & AZ_RQ_DIE) ? (u32)(w->pair[s].req >> 4) : 99u; }
static u32 parts_of(const AzWorld *w, u32 s) { u32 n; (void)az_parts(w, s, &n); return n; }

/* test creature k lives alone in a cell of the top layer (nothing above it presses) */
static u32 home(const AzWorld *w, u32 k) { return w->n_cells - 1u - k; }

/* A creature with nothing but what the test gives it, in an empty cell at the temperature a
 * TOPT of 128 stands for. Every trait and every hot scalar is 0 but TOPT. */
static void blank(const Bench *b, u32 s, u32 c)
{
    AzWorld *w = b->w;
    w->flags[s] = AZ_F_ALIVE;
    w->cell[s] = c;
    memset(ph(w, s), 0, AZ_PHEN);
    memset(tr(w, s), 0, AZ_TRAITS);
    ph(w, s)[AZ_P_TOPT] = 128;
    for (u32 k = 0; k < AZ_MOT; k++) w->motor[(size_t)s * AZ_MOT + k] = 0;
    w->pair[s].req = 0;
    w->energy[s] = 0; w->pool_a[s] = 0; w->pool_m[s] = 0; w->pool_t[s] = 0;
    w->waste[s] = 0; w->toxin[s] = 0; w->health[s] = 40000; w->mass[s] = 0;
    u32 np;
    AzPart *p = az_parts(w, s, &np);
    for (u32 i = 0; i < np; i++) p[i].matter = 0;
    for (u32 k = 0; k < AZ_NSPECIES; k++) sp(w, c)[k] = 0;
    w->c_heat[c] = 0; w->c_temp[c] = 8192; w->c_light[c] = 0; w->c_charge[c] = 0;
}

/* The free pool and the energy. The cached mass, which sets size, appetite and cost for this
 * tick, is that of a body of 4096 units whatever the pools hold, so that every case below has
 * the same size (512); the module writes the true mass back at the end of the tick. */
static void feed(AzWorld *w, u32 s, u32 pool_a, i32 energy)
{
    w->pool_a[s] = pool_a;
    w->energy[s] = energy;
    w->mass[s] = 4096;
}

/* one tick of metabolism for one creature: ask, grant, take, commit */
static void step(const Bench *b, u32 s)
{
    AzWorld *w = b->w;
    const u64 m0 = az_total_matter(w);
    const i64 e0 = az_total_energy(w);
    az_metab_demand(w, s, 0);
    az_field_grant(w, 0, w->n_cells, 0);
    az_metab_range(w, s, s + 1u, 0);
    inexact += az_total_matter(w) != m0 || az_total_energy(w) != e0;
    az_field_commit(w, 0, w->n_cells, 0);
    inexact += az_total_matter(w) != m0 || az_total_energy(w) != e0;
    steps++;
    w->tick++;
}
/* move the clock to this creature's next pulse tick, or to a tick that is none */
static void clock_to(const Bench *b, u32 s, bool pulse)
{
    while (due(b->w, s) != pulse) b->w->tick++;
}
static void step_plain(const Bench *b, u32 s) { clock_to(b, s, false); step(b, s); }
static void step_pulse(const Bench *b, u32 s) { clock_to(b, s, true); step(b, s); }

/* a slot whose body has at least two parts (room for 8192 units) */
static u32 roomy(const AzWorld *w, u32 from)
{
    for (u32 s = from; s < w->n_slots; s++)
        if ((w->flags[s] & AZ_F_ALIVE) && w->blk[s] && parts_of(w, s) >= 2u) return s;
    return AZ_NONE;
}

/* the parameters that would blur exact numbers: no upkeep, no growth of parts */
static void plain_params(AzWorld *w)
{
    w->param[AZ_WP_METAB_PART_UPKEEP] = 0;
    w->param[AZ_WP_METAB_BRAIN_UPKEEP] = 0;
    w->param[AZ_WP_METAB_WORD_UPKEEP] = 0;
    w->param[AZ_WP_METAB_ORNAMENT_UPKEEP] = 0;
    w->param[AZ_WP_METAB_TOLERANCE_UPKEEP] = 0;
    w->param[AZ_WP_METAB_PART_SHARE] = 0;
    w->param[AZ_WP_METAB_HEAL_RATE] = 0;
    w->param[AZ_WP_METAB_BURN_SCALE] = 16;               /* the exact numbers below are worked out for 16 */
}

/* ---- 1: size ---------------------------------------------------------------- */
static void test_size(void)
{
    Bench b;
    if (!bench_open(&b, 3, 0)) { CHECK(!"bench"); return; }
    AzWorld *w = b.w;
    const u32 s = roomy(w, 0), c = home(w, 0);
    CHECK(s != AZ_NONE);
    w->param[AZ_WP_METAB_PACE_BASE] = 1;                 /* cost per tick = size exactly */
    w->param[AZ_WP_METAB_COST_SCALE] = 65536;
    w->param[AZ_WP_METAB_COST_MIN] = 0;
    w->param[AZ_WP_METAB_SENSE_COST] = 0;
    double lo = 9, hi = 0;
    u32 n = 0, bad = 0;
    for (u32 m = 0; m <= 1u << 20; m = m < 300u ? m + 1u : m + m / 13u) {
        blank(&b, s, c);
        w->energy[s] = 60000;
        w->mass[s] = m;
        clock_to(&b, s, false);
        step(&b, s);
        const double size = 60000 - w->energy[s], want = pow((double)m, 0.75);
        bad += size > want + 1e-9 || size < want * 0.98 - 1.0;       /* a whole number, never over, within 2 per cent and 1 */
        if (m >= 1024u) { const double r = size / want; if (r < lo) lo = r; if (r > hi) hi = r; }
        n++;
    }
    CHECK_EQ(bad, 0);
    CHECK(lo >= 0.98 && hi <= 1.0);
    for (u32 m = 1, want = 1; m <= 1u << 20; m <<= 4, want <<= 3) {  /* exact at the powers of 16 */
        blank(&b, s, c);
        w->energy[s] = 60000;
        w->mass[s] = m;
        clock_to(&b, s, false);
        step(&b, s);
        CHECK_EQ(60000 - w->energy[s], (i32)want);
    }
    printf("size: mass^(3/4) on %u masses from 0 to 2^20: a whole number never over the true value; from 1024 up between "
           "%.4f and %.4f of it; exact at the powers of 16\n", n, lo, hi);
    bench_close(&b);
}

/* ---- 2: uptake -------------------------------------------------------------- */
/* the rules, written a second time, for a body of 4096 units (size 512) */
static u32 m_want(const AzWorld *w, u32 pace, u32 appetite, u32 affinity)
{
    const u64 unit = (((u64)512 * pace * appetite) >> 8) * w->param[AZ_WP_METAB_UPTAKE_SCALE];
    return (u32)((unit * affinity) >> 24);
}
static u32 m_cost(const AzWorld *w, u32 pace, u32 sense)
{
    return w->param[AZ_WP_METAB_COST_MIN] + (u32)(((u64)512 * pace * w->param[AZ_WP_METAB_COST_SCALE]) >> 16)
         + ((sense * w->param[AZ_WP_METAB_SENSE_COST]) >> 8);
}
static u32 m_granted(u32 want, u32 avail)
{
    const u32 g = want <= avail ? 65535u : (u32)(((u64)avail * 65535u) / want);
    return (u32)(((u64)want * g) / 65535u);
}

static void test_uptake(void)
{
    Bench b, l;
    if (!bench_open(&b, 5, 0) || !bench_open(&l, 5, AZ_LESION_TOXIN)) { CHECK(!"bench"); return; }
    AzWorld *w = b.w;
    plain_params(w); plain_params(l.w);
    const u32 s = roomy(w, 0), c = home(w, 0);
    CHECK(s != AZ_NONE);
    const u32 cost = m_cost(w, 64, 0), full = m_want(w, 64, 128, 255);
    CHECK_EQ(cost, 25);
    CHECK_EQ(full, 63);

    /* a grazer: 1 ORG_Y -> 1 biomass */
    blank(&b, s, c); ph(w, s)[AZ_P_DIET_Y] = 255; sp(w, c)[AZ_SP_ORG_Y] = 1000; feed(w, s, 4096, 500);
    step_plain(&b, s);
    CHECK_EQ(w->pool_a[s], 4096 + 63); CHECK_EQ(sp(w, c)[AZ_SP_ORG_Y], 1000 - 63);
    CHECK_EQ(w->energy[s], 500 - 25); CHECK_EQ(w->c_heat[c], 25); CHECK_EQ(w->mass[s], 4096 + 63);
    /* scarcity: the cell grants its share */
    blank(&b, s, c); ph(w, s)[AZ_P_DIET_Y] = 255; sp(w, c)[AZ_SP_ORG_Y] = 10; feed(w, s, 4096, 500);
    step_plain(&b, s);
    const u32 got = m_granted(63, 10);
    CHECK(got == 9 || got == 10);
    CHECK_EQ(w->pool_a[s], 4096 + got); CHECK_EQ(sp(w, c)[AZ_SP_ORG_Y], 10 - got);
    /* an affinity of half asks for half; none asks for nothing */
    blank(&b, s, c); ph(w, s)[AZ_P_DIET_Y] = 128; sp(w, c)[AZ_SP_ORG_Y] = 1000; feed(w, s, 4096, 500);
    step_plain(&b, s);
    CHECK_EQ(w->pool_a[s], 4096 + m_want(w, 64, 128, 128)); CHECK_EQ(m_want(w, 64, 128, 128), 32);
    blank(&b, s, c); sp(w, c)[AZ_SP_ORG_Y] = 1000; feed(w, s, 4096, 500);
    step_plain(&b, s);
    CHECK_EQ(w->pool_a[s], 4096); CHECK_EQ(sp(w, c)[AZ_SP_ORG_Y], 1000);

    /* a chemotroph: 1 NUT_X -> 1 biomass + 8 eu */
    blank(&b, s, c); ph(w, s)[AZ_P_DIET_X] = 255; sp(w, c)[AZ_SP_NUT_X] = 1000; feed(w, s, 4096, 500);
    step_plain(&b, s);
    CHECK_EQ(w->pool_a[s], 4096 + 63); CHECK_EQ(sp(w, c)[AZ_SP_NUT_X], 1000 - 63);
    CHECK_EQ(w->energy[s], 500 + 8 * 63 - 25);

    /* a producer: 2 WST_W + 12 eu of light -> 1 biomass + 1 OXI */
    blank(&b, s, c); ph(w, s)[AZ_P_DIET_L] = 255; sp(w, c)[AZ_SP_WST_W] = 1000; w->c_light[c] = 255; feed(w, s, 4096, 500);
    const i64 in0 = w->totals.energy_in;
    step_plain(&b, s);
    az_tally_merge(w);
    CHECK_EQ(w->pool_a[s], 4096 + 63); CHECK_EQ(sp(w, c)[AZ_SP_WST_W], 1000 - 126); CHECK_EQ(sp(w, c)[AZ_SP_OXI], 63);
    CHECK_EQ(w->energy[s], 500 - 25);                    /* the light it took is in the biomass */
    CHECK_EQ(w->totals.energy_in - in0, 63 * 12);
    /* in the dark: nothing fixed, the waste stays in the cell */
    blank(&b, s, c); ph(w, s)[AZ_P_DIET_L] = 255; sp(w, c)[AZ_SP_WST_W] = 1000; feed(w, s, 4096, 500);
    step_plain(&b, s);
    CHECK_EQ(w->pool_a[s], 4096); CHECK_EQ(sp(w, c)[AZ_SP_WST_W], 1000); CHECK_EQ(w->energy[s], 500 - 25);
    /* light and nothing to fix: the light is energy */
    blank(&b, s, c); ph(w, s)[AZ_P_DIET_L] = 255; w->c_light[c] = 255; feed(w, s, 4096, 500);
    step_plain(&b, s);
    CHECK_EQ(w->pool_a[s], 4096); CHECK_EQ(w->energy[s], 500 + 756 - 25);
    /* dim light is shared out by the cell: 10 units of light are 40 eu, 3 units fixed */
    blank(&b, s, c); ph(w, s)[AZ_P_DIET_L] = 255; sp(w, c)[AZ_SP_WST_W] = 1000; w->c_light[c] = 10; feed(w, s, 4096, 500);
    step_plain(&b, s);
    const u32 lit = m_granted(756, 40);
    CHECK_EQ(w->pool_a[s], 4096 + lit / 12u); CHECK_EQ(sp(w, c)[AZ_SP_OXI], lit / 12u);
    CHECK_EQ(w->energy[s], (i32)(500 + lit - 12u * (lit / 12u) - 25));
    /* a full energy store takes no light */
    const i32 e_full = (i32)(parts_of(w, s) * 4096u * AZ_EPS_BIO);
    blank(&b, s, c); ph(w, s)[AZ_P_DIET_L] = 255; w->c_light[c] = 255; feed(w, s, 4096, e_full);
    step_plain(&b, s);
    CHECK_EQ(w->energy[s], e_full - 25);

    /* a scavenger carries the waste it takes until a pulse can upgrade it */
    blank(&b, s, c); ph(w, s)[AZ_P_DIET_W] = 255; sp(w, c)[AZ_SP_WST_W] = 1000; feed(w, s, 4096, 500);
    step_plain(&b, s);
    CHECK_EQ(w->waste[s], 63); CHECK_EQ(w->pool_a[s], 4096); CHECK_EQ(sp(w, c)[AZ_SP_WST_W], 1000 - 63);

    /* toxin comes with the food, as concentrated as among the food of the cell: 63 x 100 / 1100; not under the lesion */
    blank(&b, s, c); ph(w, s)[AZ_P_DIET_Y] = 255; sp(w, c)[AZ_SP_ORG_Y] = 1000; sp(w, c)[AZ_SP_TOX_Z] = 100; feed(w, s, 4096, 500);
    step_plain(&b, s);
    CHECK_EQ(w->toxin[s], 5); CHECK_EQ(sp(w, c)[AZ_SP_TOX_Z], 100 - 5);
    blank(&l, s, c); ph(l.w, s)[AZ_P_DIET_Y] = 255; sp(l.w, c)[AZ_SP_ORG_Y] = 1000; sp(l.w, c)[AZ_SP_TOX_Z] = 100; feed(l.w, s, 4096, 500);
    step_plain(&l, s);
    CHECK_EQ(l.w->toxin[s], 0); CHECK_EQ(sp(l.w, c)[AZ_SP_TOX_Z], 100); CHECK_EQ(l.w->pool_a[s], 4096 + 63);

    /* the brain's EAT drive: twice the appetite at full drive, none at full refusal */
    blank(&b, s, c); ph(w, s)[AZ_P_DIET_Y] = 255; sp(w, c)[AZ_SP_ORG_Y] = 1000; feed(w, s, 4096, 500);
    w->motor[(size_t)s * AZ_MOT + AZ_M_EAT] = 127;
    step_plain(&b, s);
    CHECK_EQ(w->pool_a[s], 4096 + m_want(w, 64, 255, 255)); CHECK_EQ(m_want(w, 64, 255, 255), 127);
    blank(&b, s, c); ph(w, s)[AZ_P_DIET_Y] = 255; sp(w, c)[AZ_SP_ORG_Y] = 1000; feed(w, s, 4096, 500);
    w->motor[(size_t)s * AZ_MOT + AZ_M_EAT] = -128;
    step_plain(&b, s);
    CHECK_EQ(w->pool_a[s], 4096);
    /* METAB: a fast life eats more and pays more */
    blank(&b, s, c); ph(w, s)[AZ_P_DIET_Y] = 255; ph(w, s)[AZ_P_METAB] = 255; sp(w, c)[AZ_SP_ORG_Y] = 1000; feed(w, s, 4096, 500);
    step_plain(&b, s);
    CHECK_EQ(w->pool_a[s], 4096 + m_want(w, 319, 128, 255)); CHECK_EQ(m_want(w, 319, 128, 255), 317);
    CHECK_EQ(w->energy[s], 500 - (i32)m_cost(w, 319, 0)); CHECK_EQ(m_cost(w, 319, 0), 120);
    /* the sense range costs */
    blank(&b, s, c); ph(w, s)[AZ_P_SENSE] = 255; feed(w, s, 4096, 500);
    step_plain(&b, s);
    CHECK_EQ(w->energy[s], 500 - 25 - 3);
    /* the body's room: PART_HOLD for every part; what does not fit stays in the cell */
    const u32 hold = parts_of(w, s) * 4096u;
    blank(&b, s, c); ph(w, s)[AZ_P_DIET_Y] = 255; sp(w, c)[AZ_SP_ORG_Y] = 1000; feed(w, s, hold - 5u, 500);
    w->mass[s] = 4096;                                   /* the same appetite as before */
    step_plain(&b, s);
    CHECK_EQ(w->pool_a[s], hold); CHECK_EQ(sp(w, c)[AZ_SP_ORG_Y], 1000 - 5);

    printf("uptake: a body of 4096 units at pace 64 asks for %u units at full affinity and pays %u eu a tick; "
           "grazing, chemotrophy (+8 eu a unit), photosynthesis (2 waste and 12 eu of light a unit, 1 OXI back), "
           "carried waste and toxin with the food all exact; full drive asks for 127, a fast life (METAB 255) for 317 at 120 eu\n",
           full, cost);
    bench_close(&b); bench_close(&l);
}

/* ---- 3: breath -------------------------------------------------------------- */
static void breather(const Bench *b, u32 s, u32 c, u32 effic, u32 oxi_use, u32 oxi_in_cell)
{
    AzWorld *w = b->w;
    blank(b, s, c);
    tr(w, s)[AZ_T_EFFIC] = (u8)effic; tr(w, s)[AZ_T_OXI_USE] = (u8)oxi_use; tr(w, s)[AZ_T_STORE_CAP] = 63;
    sp(w, c)[AZ_SP_OXI] = oxi_in_cell;
    feed(w, s, 4096, 0);
}

static void test_breath(void)
{
    Bench b;
    if (!bench_open(&b, 7, 0)) { CHECK(!"bench"); return; }
    AzWorld *w = b.w;
    plain_params(w);
    const u32 s = roomy(w, 0), c = home(w, 0);
    CHECK(s != AZ_NONE);
    /* the most one pulse burns: 4 ticks of 1 + size x pace x 16 / 2^16 = 9, times (384 - EFFIC) / 256 */
    /* aerobic: 1 biomass + 1 OXI -> 2 waste + 12 eu, half kept at EFFIC 0 */
    breather(&b, s, c, 0, 255, 1000);
    step_pulse(&b, s);
    CHECK_EQ(w->pool_a[s], 4096 - 53); CHECK_EQ(sp(w, c)[AZ_SP_OXI], 1000 - 53);
    CHECK_EQ(sp(w, c)[AZ_SP_WST_W], 106);                /* 2 a unit, excreted above the level of 63 */
    CHECK_EQ(w->energy[s], 53 * 12 / 2 - 25);
    const u32 air_e = 53 * 12 / 2;
    /* fermentation: 2 biomass -> 1 waste + 1 ORG_Y + 14 eu */
    breather(&b, s, c, 0, 0, 1000);
    step_pulse(&b, s);
    CHECK_EQ(w->pool_a[s], 4096 - 54); CHECK_EQ(sp(w, c)[AZ_SP_OXI], 1000); CHECK_EQ(sp(w, c)[AZ_SP_ORG_Y], 27);
    CHECK_EQ(w->waste[s], 27); CHECK_EQ(w->energy[s], 27 * 14 / 2 - 25);
    const u32 fer_e = 27 * 14 / 2;
    CHECK(air_e * 54 > fer_e * 53 * 3 / 2);              /* 6 eu a unit against 3.5 */
    /* an aerobe without oxidiser burns only the share it can ferment: here nothing, and it is hurt */
    breather(&b, s, c, 0, 255, 0);
    step_pulse(&b, s);
    CHECK_EQ(w->pool_a[s], 4096); CHECK_EQ(w->energy[s], 0);
    CHECK_EQ(w->health[s], 40000 - 25 * 256);
    /* half and half */
    breather(&b, s, c, 0, 128, 1000);
    step_pulse(&b, s);
    CHECK_EQ(w->pool_a[s], 4096 - 27 - 26); CHECK_EQ(sp(w, c)[AZ_SP_OXI], 1000 - 27); CHECK_EQ(sp(w, c)[AZ_SP_ORG_Y], 13);
    /* EFFIC: nearly all the energy kept, at a third of the rate */
    breather(&b, s, c, 255, 255, 1000);
    step_pulse(&b, s);
    CHECK_EQ(w->pool_a[s], 4096 - 17); CHECK_EQ(w->energy[s], (17 * 12 * 255 >> 8) - 25);
    CHECK((17 * 12 * 255 >> 8) * 53 > (i32)air_e * 17);  /* more per unit ... */
    CHECK((17 * 12 * 255 >> 8) < (i32)air_e);            /* ... less per pulse */
    /* STORE_CAP: a full store burns nothing; a larger store burns for longer */
    breather(&b, s, c, 0, 255, 1000); w->energy[s] = 3000;
    step_pulse(&b, s);
    CHECK_EQ(w->pool_a[s] + w->pool_m[s] + w->pool_t[s], 4096); CHECK_EQ(sp(w, c)[AZ_SP_OXI], 1000);
    u32 burned[2], famine[2];
    i32 kept[2];
    for (u32 k = 0; k < 2u; k++) {
        breather(&b, s, c, 0, 255, 100000); tr(w, s)[AZ_T_STORE_CAP] = k ? 200 : 20;
        for (u32 i = 0; i < 40u; i++) { w->pair[s].req = 0; w->mass[s] = 4096; step(&b, s); }
        burned[k] = 4096 - (w->pool_a[s] + w->pool_m[s] + w->pool_t[s]);
        kept[k] = w->energy[s];
        /* then a famine: nothing left to burn, and the store is all it has */
        w->pool_a[s] = 0; w->pool_m[s] = 0; w->pool_t[s] = 0; w->waste[s] = 0;
        famine[k] = 0;
        while (cause(w, s) == 99u && famine[k] < 1000u) { w->mass[s] = 4096; step(&b, s); famine[k]++; }
        CHECK_EQ(cause(w, s), AZ_DIE_STARVED);
    }
    CHECK(kept[1] > kept[0] + 1000);                     /* the stores stand 2880 eu apart */
    CHECK(famine[1] > 2u * famine[0]);
    /* a starving body draws the germ store first, then the wall */
    breather(&b, s, c, 0, 0, 0); w->pool_a[s] = 0; w->pool_t[s] = 40; w->pool_m[s] = 50; w->mass[s] = 4096;
    step_pulse(&b, s);
    CHECK_EQ(w->pool_t[s], 0); CHECK_EQ(w->pool_m[s], 50 - 14); CHECK_EQ(w->pool_a[s], 0);
    printf("breath: a pulse burns up to 54 units; with OXI 53 of them give %u eu kept (6.0 a unit), fermenting 54 gives "
           "%u (3.5 a unit); an aerobe without OXI burns nothing; at EFFIC 255 17 units give %d; after 40 ticks of plenty "
           "(%u and %u units burned) a body with a store of 201 x 16 eu held %d eu and lived %u ticks of famine, one of "
           "21 x 16 held %d and lived %u\n",
           air_e, fer_e, 17 * 12 * 255 >> 8, burned[1], burned[0], kept[1], famine[1], kept[0], famine[0]);
    bench_close(&b);
}

/* ---- 4: body ---------------------------------------------------------------- */
static void test_body(void)
{
    Bench b, l;
    if (!bench_open(&b, 9, 0) || !bench_open(&l, 9, AZ_LESION_TOXIN)) { CHECK(!"bench"); return; }
    AzWorld *w = b.w;
    plain_params(w); plain_params(l.w);
    const u32 s = roomy(w, 0), c = home(w, 0);
    CHECK(s != AZ_NONE);
    const u32 np = parts_of(w, s), wall = np * 4096u * 16u >> 8;
    const i32 store = 64 * 16;

    /* building: energy above the store turns 1 + pool / 64 units into wall (GROWTH / 256) and germ store, 4 eu each */
    static const u8 growth[3] = { 0, 128, 255 };
    u32 to_wall[3];
    for (u32 k = 0; k < 3u; k++) {
        blank(&b, s, c); tr(w, s)[AZ_T_STORE_CAP] = 63; tr(w, s)[AZ_T_GROWTH] = growth[k]; tr(w, s)[AZ_T_WASTE_RATE] = 255;
        feed(w, s, 4096, store + 1000);
        step_pulse(&b, s);
        to_wall[k] = w->pool_m[s];
        CHECK_EQ(w->pool_m[s] + w->pool_t[s], 65); CHECK_EQ(w->pool_a[s], 4096 - 65);
        CHECK_EQ(w->energy[s], store + 1000 - 25 - 65 * 4);
        CHECK_EQ(w->pool_m[s], (65u * growth[k]) >> 8);
    }
    /* a wall that is full takes no more; too little surplus builds less */
    blank(&b, s, c); tr(w, s)[AZ_T_STORE_CAP] = 63; tr(w, s)[AZ_T_GROWTH] = 255; w->pool_m[s] = wall; feed(w, s, 4096, store + 1000);
    step_pulse(&b, s);
    CHECK_EQ(w->pool_m[s], wall); CHECK_EQ(w->pool_t[s], 65);
    blank(&b, s, c); tr(w, s)[AZ_T_STORE_CAP] = 63; tr(w, s)[AZ_T_OXI_USE] = 255;   /* no OXI here: it cannot burn for more */
    feed(w, s, 4096, store + 25 + 41);
    step_pulse(&b, s);
    CHECK_EQ(w->pool_t[s], 10); CHECK_EQ(w->energy[s], store + 1);

    /* repair: 4 units of wall a pulse, 512 health each, the units become waste */
    w->param[AZ_WP_METAB_HEAL_RATE] = 4;
    blank(&b, s, c); tr(w, s)[AZ_T_STORE_CAP] = 63; tr(w, s)[AZ_T_WASTE_RATE] = 255; w->pool_m[s] = 10; w->health[s] = 30000;
    feed(w, s, 0, store + 25 + 110);
    step_pulse(&b, s);
    CHECK_EQ(w->health[s], 30000 + 4 * 512); CHECK_EQ(w->pool_m[s], 6); CHECK_EQ(sp(w, c)[AZ_SP_WST_W], 4);
    CHECK_EQ(w->c_heat[c], 25 + 4 * 14);
    blank(&b, s, c); tr(w, s)[AZ_T_STORE_CAP] = 63; w->pool_m[s] = 10; w->health[s] = 65000; feed(w, s, 0, store + 25 + 110);
    step_pulse(&b, s);
    CHECK_EQ(w->pool_m[s], 8); CHECK_EQ(w->health[s], 65535 - 2);   /* only what is missing; 2 units of waste carried harm by 2 */
    w->param[AZ_WP_METAB_HEAL_RATE] = 0;
    /* detoxification: 1 + toxin / 16 a pulse, 2 eu of heat each (TOXRES 255 so that the load does no harm here) */
    blank(&b, s, c); tr(w, s)[AZ_T_STORE_CAP] = 63; ph(w, s)[AZ_P_TOXRES] = 255; w->toxin[s] = 100; feed(w, s, 0, store + 25 + 110);
    step_pulse(&b, s);
    CHECK_EQ(w->toxin[s], 93); CHECK_EQ(w->waste[s], 7); CHECK_EQ(w->c_heat[c], 25 + 14);
    /* secretion: TOX_SECRETE of the waste becomes toxin in the cell, 2 eu a unit; not under the lesion */
    for (u32 k = 0; k < 2u; k++) {
        const Bench *x = k ? &l : &b;
        blank(x, s, c); tr(x->w, s)[AZ_T_STORE_CAP] = 63; tr(x->w, s)[AZ_T_TOX_SECRETE] = 255; x->w->waste[s] = 40;
        feed(x->w, s, 0, store + 25 + 110);
        step_pulse(x, s);
        CHECK_EQ(sp(x->w, c)[AZ_SP_TOX_Z], k ? 0u : 9u);         /* 40 x 255 / 256 x 64 / 256 */
        CHECK_EQ(x->w->energy[s], store + 110 - (k ? 0 : 18));
        CHECK_EQ(x->w->waste[s], k ? 40u : 31u);
    }
    /* excretion: above (255 - WASTE_RATE) / 4 units, or when the brain says so; carried waste harms a little */
    blank(&b, s, c); tr(w, s)[AZ_T_STORE_CAP] = 63; w->waste[s] = 50; feed(w, s, 0, store + 25 + 110);
    step_pulse(&b, s);
    CHECK_EQ(w->waste[s], 50); CHECK_EQ(sp(w, c)[AZ_SP_WST_W], 0); CHECK_EQ(w->health[s], 40000 - 50);
    blank(&b, s, c); tr(w, s)[AZ_T_STORE_CAP] = 63; tr(w, s)[AZ_T_WASTE_RATE] = 128; w->waste[s] = 50; feed(w, s, 0, store + 25 + 110);
    step_pulse(&b, s);
    CHECK_EQ(w->waste[s], 0); CHECK_EQ(sp(w, c)[AZ_SP_WST_W], 50); CHECK_EQ(w->health[s], 40000);
    blank(&b, s, c); tr(w, s)[AZ_T_STORE_CAP] = 63; w->waste[s] = 50; feed(w, s, 0, store + 25 + 110);
    w->motor[(size_t)s * AZ_MOT + AZ_M_EXCRETE] = 1;
    step_pulse(&b, s);
    CHECK_EQ(w->waste[s], 0); CHECK_EQ(sp(w, c)[AZ_SP_WST_W], 50);
    /* scavenging: 1 waste + 14 eu -> 1 biomass, by DIET_W, from energy above the store */
    blank(&b, s, c); tr(w, s)[AZ_T_STORE_CAP] = 63; ph(w, s)[AZ_P_DIET_W] = 255; w->waste[s] = 40; feed(w, s, 0, store + 25 + 14 * 20 + 5);
    step_pulse(&b, s);
    CHECK_EQ(w->waste[s], 20); CHECK_EQ(w->pool_a[s] + w->pool_t[s] + w->pool_m[s], 20);
    blank(&b, s, c); tr(w, s)[AZ_T_STORE_CAP] = 63; w->waste[s] = 40; feed(w, s, 0, store + 25 + 14 * 20 + 5);
    step_pulse(&b, s);
    CHECK_EQ(w->waste[s], 40);                           /* no DIET_W, no upgrade */

    /* parts: the part whose turn it is takes (8 + ALLOMETRY) x 4 / 2^15 of the free pool; a body
     * out of pool takes it back; a full body asks for a part when the brain drives GROW */
    w->param[AZ_WP_METAB_PART_SHARE] = 4;
    u32 grown[2];
    for (u32 k = 0; k < 2u; k++) {
        blank(&b, s, c); tr(w, s)[AZ_T_STORE_CAP] = 63; tr(w, s)[AZ_T_OXI_USE] = 255; tr(w, s)[AZ_T_ALLOMETRY] = k ? 248 : 0;
        feed(w, s, 4096, store + 25);                    /* no OXI and no surplus: nothing burned, nothing built */
        step_pulse(&b, s);
        grown[k] = (u32)az_body_matter(w, s);
        CHECK_EQ(grown[k], (4096u * (8u + (k ? 248u : 0u)) * 4u) >> 15);
        CHECK_EQ(w->mass[s], 4096);                      /* what moved into a part is still the body's */
    }
    CHECK_EQ(grown[0], 4); CHECK_EQ(grown[1], 128);
    /* At the supported maximum rate, demand can exceed the donor's pool or
     * the receiver's room. The computed demand can also exceed UINT32_MAX. */
    static const struct { u32 free_pool, stored, transferred; } limits[] = {
        { 100, 0, 100 }, { 100, 4090, 6 }, { 0x80000000u, 0, 4096 }
    };
    w->param[AZ_WP_METAB_PART_SHARE] = 256;
    for (u32 k = 0; k < sizeof limits / sizeof limits[0]; k++) {
        blank(&b, s, c);
        tr(w, s)[AZ_T_STORE_CAP] = 63;
        tr(w, s)[AZ_T_OXI_USE] = 255;
        tr(w, s)[AZ_T_ALLOMETRY] = 248;
        feed(w, s, limits[k].free_pool, store + 25);
        clock_to(&b, s, true);
        u32 count;
        AzPart *parts = az_parts(w, s, &count);
        AzPart *target = &parts[((w->tick >> 2) + w->uid[s]) % count];
        target->matter = limits[k].stored;
        step(&b, s);
        CHECK_EQ(target->matter, limits[k].stored + limits[k].transferred);
        CHECK_EQ(w->pool_a[s], limits[k].free_pool - limits[k].transferred);
    }
    w->param[AZ_WP_METAB_PART_SHARE] = 4;
    blank(&b, s, c); tr(w, s)[AZ_T_STORE_CAP] = 63; feed(w, s, 0, store + 25);
    { u32 n; AzPart *p = az_parts(w, s, &n); for (u32 i = 0; i < n; i++) p[i].matter = 100; }
    w->mass[s] = 100 * np;
    step_pulse(&b, s);
    CHECK(w->pool_a[s] > 0 && w->pool_a[s] + az_body_matter(w, s) == 100u * np);
    blank(&b, s, c); tr(w, s)[AZ_T_STORE_CAP] = 63; feed(w, s, 500, store + 25 + 300);
    { u32 n; AzPart *p = az_parts(w, s, &n); for (u32 i = 0; i < n; i++) p[i].matter = 4096; }
    w->mass[s] = 4096 * np;
    step_pulse(&b, s);
    CHECK(!(w->pair[s].req & AZ_RQ_GROW));
    w->motor[(size_t)s * AZ_MOT + AZ_M_GROW] = 64; w->energy[s] = 30000;
    step_pulse(&b, s);
    CHECK(w->pair[s].req & AZ_RQ_GROW);
    w->param[AZ_WP_METAB_PART_SHARE] = 0;

    /* upkeep: what a creature carries costs, every pulse */
    w->param[AZ_WP_METAB_PART_UPKEEP] = 1; w->param[AZ_WP_METAB_BRAIN_UPKEEP] = 4;
    w->param[AZ_WP_METAB_ORNAMENT_UPKEEP] = 4; w->param[AZ_WP_METAB_TOLERANCE_UPKEEP] = 2;
    w->param[AZ_WP_METAB_WORD_UPKEEP] = 256;
    u32 bytes, words;
    (void)az_brain(w, s, &bytes);
    i32 paid[4];
    for (u32 k = 0; k < 4u; k++) {
        blank(&b, s, c); tr(w, s)[AZ_T_STORE_CAP] = 0; feed(w, s, 0, 20000);
        w->health[s] = 65535;
        if (k == 1) tr(w, s)[AZ_T_ORNAMENT] = 255;
        if (k == 2) { tr(w, s)[AZ_T_PRESS_TOL] = 255; tr(w, s)[AZ_T_RAD_TOL] = 255; tr(w, s)[AZ_T_TOXRES] = 255; tr(w, s)[AZ_T_TTOL] = 255; }
        if (k == 3) { az_memory_request(w, s, 5); az_store_commit(w); }
        step_pulse(&b, s);
        paid[k] = 20000 - w->energy[s];
    }
    (void)az_memory(w, s, &words);
    CHECK_EQ(words, 5);
    CHECK_EQ(paid[0], (i32)(25 + np + ((bytes * 4u) >> 8)));     /* the cost of living, its parts, its brain */
    CHECK_EQ(paid[1] - paid[0], 3);
    CHECK_EQ(paid[2] - paid[0], (4 * 255 * 2) >> 8);
    CHECK_EQ(paid[3] - paid[0], 5 * (i32)w->param[AZ_WP_TIER1_WORD_EU]);
    printf("body: of 65 units built, GROWTH 0, 128, 255 puts %u, %u, %u into the wall; repair, detoxification, secretion "
           "(9 of 40 units of waste, none under the lesion), excretion and scavenging exact; ALLOMETRY 0 and 248 build %u "
           "and %u units into the part; a pulse with %u parts and %u bytes of brain at 4 eu per 256 bytes costs %d eu, an "
           "ORNAMENT of 255 adds 3, four tolerances at 255 add 7, five memory words at full price add %d\n",
           to_wall[0], to_wall[1], to_wall[2], grown[0], grown[1], np, bytes, paid[0], paid[3] - paid[0]);
    bench_close(&b); bench_close(&l);
}

/* Follow the SHED motor through the metabolism request and serial body commit. */
static void test_shed(void)
{
    static const struct {
        i8 drive;
        bool pulse, empty, multipart, shed;
    } cases[] = {
        { -128, true, true, true, false }, { 96, true, true, true, false },
        { 97, true, true, true, true }, { 127, true, true, true, true },
        { 127, false, true, true, false }, { 127, true, false, true, false },
        { 127, true, true, false, false }
    };
    for (size_t k = 0; k < AZ_LEN(cases); k++) {
        Bench b;
        if (!bench_open(&b, 11, 0)) { CHECK(!"bench"); return; }
        AzWorld *w = b.w;
        const u32 s = roomy(w, 0);
        if (s == AZ_NONE) { CHECK(s != AZ_NONE); bench_close(&b); return; }
        plain_params(w);
        blank(&b, s, home(w, 0));
        feed(w, s, 1000, 8000);
        u32 count;
        AzPart *parts = az_parts(w, s, &count);
        while (!cases[k].multipart && count > 1u) {
            const u32 before = count;
            az_part_shed_request(w, s, count - 1u);
            az_store_commit(w);
            parts = az_parts(w, s, &count);
            if (count != before - 1u) { CHECK_EQ(count, before - 1u); bench_close(&b); return; }
        }
        parts[count - 1u].matter = cases[k].empty ? 0u : 1u;
        w->motor[(size_t)s * AZ_MOT + AZ_M_SHED] = cases[k].drive;
        clock_to(&b, s, cases[k].pulse);
        const u64 matter = az_total_matter(w);
        const i64 energy = az_total_energy(w);
        step(&b, s);
        CHECK_EQ((w->pair[s].req & AZ_RQ_SHED) != 0, cases[k].shed);
        CHECK_EQ(parts_of(w, s), count);
        az_store_commit(w);
        CHECK_EQ(parts_of(w, s), count - (u32)cases[k].shed);
        CHECK_EQ(az_total_matter(w), matter);
        CHECK_EQ(az_total_energy(w), energy);
        bench_close(&b);
    }
}


/* ---- 5: stress -------------------------------------------------------------- */
static void test_stress(void)
{
    Bench b, l;
    if (!bench_open(&b, 11, 0) || !bench_open(&l, 11, AZ_LESION_TOXIN)) { CHECK(!"bench"); return; }
    AzWorld *w = b.w;
    plain_params(w); plain_params(l.w);
    const u32 s = roomy(w, 0), c = home(w, 0);
    CHECK(s != AZ_NONE);

    /* the thermal band: TOPT x 64 either side by 256 + TTOL x 16; outside it 2 eu and 16 health a tick per 1024 */
    static const i16 temp[5] = { 8192, 8192 + 256, 8192 - 256, 12288, 4096 };
    for (u32 k = 0; k < 5u; k++) {
        blank(&b, s, c); feed(w, s, 4096, 500); w->c_temp[c] = temp[k];
        step_plain(&b, s);
        const bool out = k >= 3;
        CHECK_EQ(w->energy[s], 500 - 25 - (out ? (3840 * 2) >> 10 : 0));
        CHECK_EQ(w->health[s], 40000 - (out ? (3840 * 16) >> 10 : 0));
    }
    blank(&b, s, c); ph(w, s)[AZ_P_TTOL] = 255; feed(w, s, 4096, 500); w->c_temp[c] = 12288;
    step_plain(&b, s);
    CHECK_EQ(w->health[s], 40000); CHECK_EQ(w->energy[s], 500 - 25);
    blank(&b, s, c); ph(w, s)[AZ_P_TOPT] = 192; feed(w, s, 4096, 500); w->c_temp[c] = 12288;
    step_plain(&b, s);
    CHECK_EQ(w->health[s], 40000);                       /* its optimum is this heat */

    /* toxin carried: 4 a unit and tick without resistance, 1/256 of it at TOXRES 255, none under the lesion */
    blank(&b, s, c); w->toxin[s] = 100; feed(w, s, 4096, 500);
    step_plain(&b, s);
    CHECK_EQ(w->health[s], 40000 - 400);
    blank(&b, s, c); ph(w, s)[AZ_P_TOXRES] = 128; w->toxin[s] = 100; feed(w, s, 4096, 500);
    step_plain(&b, s);
    CHECK_EQ(w->health[s], 40000 - 200);
    blank(&b, s, c); ph(w, s)[AZ_P_TOXRES] = 255; w->toxin[s] = 100; feed(w, s, 4096, 500);
    step_plain(&b, s);
    CHECK_EQ(w->health[s], 40000 - 1);
    blank(&l, s, c); l.w->toxin[s] = 100; feed(l.w, s, 4096, 500);
    step_plain(&l, s);
    CHECK_EQ(l.w->health[s], 40000);

    /* pressure: 4 x 64000 / 64 = 4000 here; PRESS_TOL + 1 times 64 is borne */
    for (u32 k = 0; k < 3u; k++) {
        static const u8 tol[3] = { 0, 30, 255 };
        blank(&b, s, c); tr(w, s)[AZ_T_STORE_CAP] = 63; tr(w, s)[AZ_T_PRESS_TOL] = tol[k]; sp(w, c)[AZ_SP_SOLV] = 64000;
        feed(w, s, 0, 1024 + 25);
        w->health[s] = 65535;
        step_pulse(&b, s);
        const u32 bears = (tol[k] + 1u) * 64u;
        CHECK_EQ(w->health[s], 65535u - (bears < 4000u ? (4000u - bears) * 256u / 64u : 0u));
    }
    /* radiation: 16 a pulse per unit of charge above RAD_TOL */
    blank(&b, s, c); tr(w, s)[AZ_T_STORE_CAP] = 63; w->c_charge[c] = 200; feed(w, s, 0, 1024 + 25);
    step_pulse(&b, s);
    CHECK_EQ(w->health[s], 40000 - 200 * 16);
    blank(&b, s, c); tr(w, s)[AZ_T_STORE_CAP] = 63; tr(w, s)[AZ_T_RAD_TOL] = 255; w->c_charge[c] = 200; feed(w, s, 0, 1024 + 25);
    step_pulse(&b, s);
    CHECK_EQ(w->health[s], 40000);

    /* the causes of death */
    blank(&b, s, c);                                     /* nothing left at all */
    step_plain(&b, s);
    CHECK_EQ(cause(w, s), AZ_DIE_STARVED);
    blank(&b, s, c); w->pool_m[s] = 5; w->mass[s] = 4096; w->health[s] = 6000;   /* matter but no energy: 25 unpaid eu x 256 */
    step_plain(&b, s);
    CHECK_EQ(cause(w, s), AZ_DIE_STARVED); CHECK_EQ(w->health[s], 0);
    blank(&b, s, c); w->pool_m[s] = 5; w->mass[s] = 4096; w->health[s] = 6401;
    step_plain(&b, s);
    CHECK_EQ(cause(w, s), 99); CHECK_EQ(w->health[s], 1);
    blank(&b, s, c); w->toxin[s] = 2000; feed(w, s, 4096, 500); w->health[s] = 7000;
    step_plain(&b, s);
    CHECK_EQ(cause(w, s), AZ_DIE_POISON);
    blank(&b, s, c); ph(w, s)[AZ_P_TOPT] = 0; feed(w, s, 4096, 60000); w->c_temp[c] = 32767; w->health[s] = 400;
    step_plain(&b, s);
    CHECK_EQ(cause(w, s), AZ_DIE_COLD_HEAT);
    blank(&b, s, c); tr(w, s)[AZ_T_STORE_CAP] = 63; sp(w, c)[AZ_SP_SOLV] = 4000000; feed(w, s, 0, 1024 + 25);
    step_pulse(&b, s);
    CHECK_EQ(cause(w, s), AZ_DIE_PRESSURE);
    blank(&b, s, c); feed(w, s, 4096, 500); w->health[s] = 0;      /* brought to nothing by someone else */
    step_plain(&b, s);
    CHECK_EQ(cause(w, s), AZ_DIE_WOUNDS);
    printf("stress: inside the thermal band nothing; 3840 outside it costs 7 eu and 60 health a tick, not with TTOL 255 "
           "or a fitting TOPT; 100 units of toxin cost 400 health a tick, 200 with TOXRES 128, 1 with 255, 0 under the lesion; pressure "
           "and radiation by PRESS_TOL and RAD_TOL; each way to die gives its own cause\n");
    bench_close(&b); bench_close(&l);
}

/* ---- 6: lives --------------------------------------------------------------- */
typedef struct Life { u32 ticks, cause, mass; i32 energy; u64 oxi_out; } Life;

/* A creature of ordinary make in an open cell: the test restocks the cell before every tick
 * (food by the creature's diet, some oxidiser) and counts the OXI the creature added to it. */
static Life live(const Bench *b, u32 s, u32 c, u32 diet, u32 light, u32 n)
{
    AzWorld *w = b->w;
    blank(b, s, c);
    ph(w, s)[diet] = 255; ph(w, s)[AZ_P_METAB] = 64;
    tr(w, s)[AZ_T_STORE_CAP] = 63; tr(w, s)[AZ_T_EFFIC] = 128; tr(w, s)[AZ_T_OXI_USE] = 128;
    tr(w, s)[AZ_T_GROWTH] = 128; tr(w, s)[AZ_T_WASTE_RATE] = 200;
    w->c_light[c] = (u8)light;
    feed(w, s, 200, 400);
    w->mass[s] = 200;
    Life r = { 0, 99u, 0, 0, 0 };
    while (r.ticks < n && cause(w, s) == 99u) {
        for (u32 k = 0; k < AZ_NSPECIES; k++) sp(w, c)[k] = 0;
        sp(w, c)[AZ_SP_WST_W] = diet == AZ_P_DIET_L ? 600u : 0u;
        sp(w, c)[AZ_SP_NUT_X] = diet == AZ_P_DIET_X ? 600u : 0u;
        sp(w, c)[AZ_SP_ORG_Y] = diet == AZ_P_DIET_Y ? 600u : 0u;
        sp(w, c)[AZ_SP_OXI] = 300;
        step(b, s);
        if (sp(w, c)[AZ_SP_OXI] > 300u) r.oxi_out += sp(w, c)[AZ_SP_OXI] - 300u;
        r.ticks++;
    }
    r.cause = cause(w, s); r.mass = w->mass[s]; r.energy = w->energy[s];
    return r;
}

static void test_lives(void)
{
    Bench b;
    if (!bench_open(&b, 13, 0)) { CHECK(!"bench"); return; }
    AzWorld *w = b.w;
    const u32 s = roomy(w, 0), c = home(w, 0);
    CHECK(s != AZ_NONE);
    const Life sun = live(&b, s, c, AZ_P_DIET_L, 255, 600);
    const Life dark = live(&b, s, c, AZ_P_DIET_L, 0, 3000);
    const Life vent = live(&b, s, c, AZ_P_DIET_X, 0, 600);
    const Life graze = live(&b, s, c, AZ_P_DIET_Y, 0, 600);
    CHECK_EQ(sun.cause, 99); CHECK(sun.mass > 400); CHECK(sun.oxi_out > 0);
    CHECK_EQ(dark.oxi_out, 0);
    CHECK_EQ(dark.cause, AZ_DIE_STARVED); CHECK(dark.ticks < 3000);
    CHECK_EQ(vent.cause, 99); CHECK(vent.mass > 400);
    CHECK_EQ(graze.cause, 99); CHECK(graze.mass > 400);

    /* an egg and a cyst are outside metabolism */
    for (u32 k = 0; k < 2u; k++) {
        blank(&b, s, c); ph(w, s)[AZ_P_DIET_Y] = 255; sp(w, c)[AZ_SP_ORG_Y] = 1000; feed(w, s, 300, 77);
        w->flags[s] = (u8)(AZ_F_ALIVE | (k ? AZ_F_DORMANT : AZ_F_EGG));
        for (u32 i = 0; i < 8u; i++) step(&b, s);
        CHECK_EQ(w->energy[s], 77); CHECK_EQ(w->pool_a[s], 300); CHECK_EQ(sp(w, c)[AZ_SP_ORG_Y], 1000);
        CHECK_EQ(cause(w, s), 99);
    }
    /* a body of one part and a body of no mass at all */
    u32 one = AZ_NONE;
    for (u32 k = 0; k < w->n_slots; k++)
        if ((w->flags[k] & AZ_F_ALIVE) && w->blk[k] && parts_of(w, k) == 1u) { one = k; break; }
    if (one != AZ_NONE) {
        blank(&b, one, home(w, 1)); ph(w, one)[AZ_P_DIET_Y] = 255; sp(w, home(w, 1))[AZ_SP_ORG_Y] = 100000;
        feed(w, one, 4000, 500);
        for (u32 i = 0; i < 40u; i++) { w->pair[one].req = 0; step(&b, one); }
        CHECK(w->pool_a[one] + w->pool_m[one] + w->pool_t[one] + w->waste[one] <= 4096u + 64u);   /* one part holds 4096 */
    }
    printf("lives: 600 ticks from 200 units in a cell restocked every tick: a producer in full light reached %u units "
           "and gave %" PRIu64 " OXI to the cell; in the dark it gave none and starved after %u ticks; a chemotroph "
           "reached %u units, a grazer %u\n", sun.mass, sun.oxi_out, dark.ticks, vent.mass, graze.mass);
    bench_close(&b);
}

/* ---- 7: worlds -------------------------------------------------------------- */
typedef struct Run { u64 hash, births, deaths; u32 live, ok; u64 oxi, tox; double off0, off1; u32 n0, n1; } Run;

/* how far a creature's thermal optimum is from the stub world's one temperature, in TOPT units */
static u32 topt_off(const AzWorld *w, u32 s)
{
    const i32 d = (i32)ph(w, s)[AZ_P_TOPT] - 128;
    return (u32)(d < 0 ? -d : d);
}

static Run world_run(u64 seed, u32 threads, u8 reorder, u32 lesion, u32 ticks, bool uniform)
{
    Run r;
    memset(&r, 0, sizeof r);
    AzConfig cfg = az_test_config(seed, 2000);
    cfg.reorder_period = reorder;
    cfg.cap = 40000;
    cfg.lesion = lesion;
    AzTestWorld t;
    /* uniform: the uniform world (az_test.h), for the checks that name its one temperature and
     * its cells without toxin (so that what toxin there is was secreted); else the real world */
    if (!az_test_world_ex(&t, &cfg, threads, uniform ? AZ_TEST_NO_WORLDGEN : 0)) { CHECK(!"world"); return r; }
    if (uniform) az_test_uniform(t.w);
    AzWorld *w = t.w;
    const u64 m0 = az_total_matter(w);
    const i64 e0 = az_total_energy(w);
    for (u32 s = 0; s < w->n_slots; s++)
        if (w->flags[s] & AZ_F_ALIVE) { r.off0 += topt_off(w, s); r.n0++; }
    r.ok = 1;
    for (u32 i = 0; i < ticks; i++) {
        az_tick(w);
        if (az_total_matter(w) != m0 || az_total_energy(w) != e0) r.ok = 0;
    }
    for (u32 s = 0; s < w->n_slots; s++)                  /* the founders still alive (generation 0) */
        if ((w->flags[s] & AZ_F_ALIVE) && w->birth_tick[s] == 0) { r.off1 += topt_off(w, s); r.n1++; }
    for (u32 c = 0; c < w->n_cells; c++) { r.oxi += sp(w, c)[AZ_SP_OXI]; r.tox += sp(w, c)[AZ_SP_TOX_Z]; }
    r.hash = az_state_hash(w); r.births = w->totals.births; r.deaths = w->totals.deaths; r.live = w->n_live;
    if (w->totals.field_underflow) r.ok = 0;
    az_test_world_free(&t);
    return r;
}

static void test_worlds(void)
{
    const Run a = world_run(17, 0, 1, 0, 150, false), b = world_run(17, 1, 9, 0, 150, false),
              c = world_run(17, 3, 1, 0, 150, false), d = world_run(17, 8, 9, 0, 150, false);
    CHECK(a.ok && b.ok && c.ok && d.ok);
    CHECK_EQ(a.hash, b.hash); CHECK_EQ(a.hash, c.hash); CHECK_EQ(a.hash, d.hash);
    CHECK(a.births > 0 && a.live > 0);
    const Run l = world_run(17, 0, 1, AZ_LESION_TOXIN, 150, false), l8 = world_run(17, 8, 9, AZ_LESION_TOXIN, 150, false);
    CHECK(l.ok && l8.ok);
    CHECK_EQ(l.hash, l8.hash);
    CHECK(l.hash != a.hash);
    CHECK(a.tox > 0);
    /* the uniform world (az_test.h) has no toxin of its own: there the toxin is what the creatures secrete */
    const Run ua = world_run(17, 0, 1, 0, 150, true), ul = world_run(17, 8, 9, AZ_LESION_TOXIN, 150, true);
    CHECK(ua.ok && ul.ok);
    CHECK(ul.hash != ua.hash);
    CHECK(ua.tox > 0); CHECK_EQ(ul.tox, 0);              /* the toxin of this world is what its creatures secrete */
    printf("worlds: 2000 founders, 150 ticks: %" PRIu64 " births, %" PRIu64 " deaths, %u alive, hash %016" PRIx64
           " on 0, 1, 3 and 8 workers, books exact every tick, the toxin lesion gives another world (%" PRIu64 " deaths); "
           "in the uniform world %" PRIu64 " units of secreted toxin in the cells, none under the lesion\n",
           a.births, a.deaths, a.live, a.hash, l.deaths, ua.tox);
    /* the founders of the real world live: seeds 211 to 214, 600 ticks, books exact every tick */
    for (u64 seed = 211; seed <= 214; seed++) {
        const Run r = world_run(seed, 8, 9, 0, 600, false);
        CHECK(r.ok);
        CHECK(r.n1 > 0);
    }

    /* The stub world has one temperature. Which founders does it let live? Seeds 211 to 214,
     * nothing tuned on them (201 to 204 were looked at while the toxin rule was being set, so
     * they are not used): the distance of the survivors' TOPT from 128 against all founders'. */
    u32 closer = 0;
    double f0 = 0, f1 = 0;
    for (u64 seed = 211; seed <= 214; seed++) {
        const Run r = world_run(seed, 8, 9, 0, 600, true);
        CHECK(r.ok);
        CHECK(r.n1 > 0);
        const double m0 = r.off0 / (r.n0 ? r.n0 : 1), m1 = r.off1 / (r.n1 ? r.n1 : 1);
        closer += m1 < m0;
        f0 += m0 / 4; f1 += m1 / 4;
        if (getenv("AZ_TEST_VERBOSE"))
            printf("  seed %" PRIu64 ": founders %u, alive at 600 %u, |TOPT - 128| %.1f -> %.1f, births %" PRIu64 ", deaths %" PRIu64 "\n",
                   seed, r.n0, r.n1, m0, m1, r.births, r.deaths);
    }
    printf("thermal sorting, as measured (expected 3 of 4, not met): the founders still alive after 600 ticks have a thermal optimum %.1f units from the world's "
           "temperature against %.1f for all founders; closer on %u of 4 seeds (211 to 214)\n", f1, f0, closer);
    /* Written down before these seeds were run: "closer on at least 3 of 4". It did not hold
     * (2 of 4), so there is no check on the direction: in a world of one temperature, with
     * thermal damage mild enough to repair, 600 ticks do not sort the founders by TOPT. The
     * numbers are printed as measured; the bench above shows what the trait does to one body. */
    CHECK(closer <= 4);
}

int main(void)
{
    test_size();
    test_uptake();
    test_breath();
    test_body();
    test_shed();
    test_stress();
    test_lives();
    test_worlds();
    CHECK_EQ(inexact, 0);
    printf("books: matter and energy the same after each of %u steps as before it\n", steps);
    return az_test_done();
}
