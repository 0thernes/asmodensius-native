/* test_life.c - life history (core/az_life.c, parameters core/params/az_life.def).
 *
 *   1. walk       48 creatures with traits of their own go egg, juvenile,
 *                 adult, senescent and die at exactly the ticks a model
 *                 written here from the rules gives them; again with the
 *                 clock across 2^32
 *   2. broods     the books of a brood for every clutch size, share and
 *                 store: the record, what the mother gives, what every child
 *                 gets (never 0 mu), parents and flags; budding, selfing and
 *                 a pair; the refusals; a brood of 65,281 that the slots
 *                 cut short and one that the creature store cuts short, with
 *                 the rest refunded
 *   3. modes      who breeds alone: a budder every period, a selfer at every
 *                 check, a hermaphrodite by its SELFING trait, the mating
 *                 kinds never; REPRO_THRESH and SIZE decide who is ripe
 *   4. dormancy   entered by rest and want, left by food, by chance and by
 *                 request
 *   5. mortality  deaths by age on seeds 1 to 24 against the doubling hazard
 *   6. traits     SIZE, DISPERSAL, DEV_PLAST and the founders' ages (the
 *                 other owned traits change an outcome in parts 1 to 4)
 *   7. edges      creatures with no gene of the family, with every trait and
 *                 every scale at its highest, with one body part
 *   8. worlds     whole ticks: both invariants at every tick; equal hashes
 *                 for 1, 3 and 8 workers and reorder periods 1 and 9, also
 *                 when the slots bind and when the creature store binds; the
 *                 life table of one run from its ledger
 *
 * Parts 1 to 7 drive the module directly on a serial lab world (the bench):
 * the life stage of a tick for chosen slots, then the birth commit. The other
 * modules do not run there, so every state is the one the test set.
 */
#include "az_test_locked_block.h"

/* ---- the bench: a serial lab world whose ledger the test reads ---- */
typedef struct Bench {
    AzHost host;
    void *block;
    AzArena arena;
    AzWorld *w;
    u64 m0;
    i64 e0;
    u32 births, eggs, hatches, stages, deaths, cause[16];
    AzEvent last;               /* the last life event */
} Bench;

static void bench_pfor(void *ctx, AzWorld *w, AzJobFn fn, const u32 *splits, u32 n)
{
    (void)ctx;
    for (u32 k = 0; k < n; k++) fn(w, splits[k], splits[k + 1], 0);
}

static u64 bench_now(void *ctx) { (void)ctx; return 0; }

static void bench_sink(void *ctx, const AzEvent *ev, u32 n)
{
    Bench *b = (Bench *)ctx;
    for (u32 i = 0; i < n; i++) {
        switch (ev[i].kind) {
        case AZ_EV_BIRTH: b->births++; b->eggs += ev[i].aux & 1u; break;
        case AZ_EV_HATCH: b->hatches++; break;
        case AZ_EV_STAGE: b->stages++; break;
        case AZ_EV_DEATH: b->deaths++; b->cause[ev[i].aux & 15u]++; break;
        default: continue;
        }
        b->last = ev[i];
    }
}

static bool bench_open(Bench *b, const AzConfig *cfg)
{
    memset(b, 0, sizeof *b);
    b->host.ctx = b; b->host.n_workers = 1;
    b->host.parallel_for = bench_pfor; b->host.now_ns = bench_now;
    b->host.sink = bench_sink; b->host.sink_ctx = b;
    const u64 bytes = az_world_bytes(cfg);
    b->block = az_test_locked_block(bytes);
    if (!b->block) return false;
    b->arena.base = (u8 *)b->block; b->arena.size = bytes; b->arena.used = 0;
    b->w = az_world_create(&b->arena, cfg, &b->host);
    if (!b->w) return false;
    az_worldgen(b->w);
    return true;
}

/* the bench on the uniform world (az_test.h): for the parts whose checks assume its
 * founders and its open space (a mother whose genome carries an egg time, room on every
 * side of her for a wide brood) */
static bool bench_open_uniform(Bench *b, const AzConfig *cfg)
{
    memset(b, 0, sizeof *b);
    b->host.ctx = b; b->host.n_workers = 1;
    b->host.parallel_for = bench_pfor; b->host.now_ns = bench_now;
    b->host.sink = bench_sink; b->host.sink_ctx = b;
    const u64 bytes = az_world_bytes(cfg);
    b->block = az_test_locked_block(bytes);
    if (!b->block) return false;
    b->arena.base = (u8 *)b->block; b->arena.size = bytes; b->arena.used = 0;
    b->w = az_world_create(&b->arena, cfg, &b->host);
    if (!b->w) return false;
    az_test_uniform(b->w);
    return true;
}

static void bench_close(Bench *b)
{
    if (b->block) az_test_locked_block_free(b->block);
    memset(b, 0, sizeof *b);
}

static void books(Bench *b) { b->m0 = az_total_matter(b->w); b->e0 = az_total_energy(b->w); }
static bool exact(const Bench *b) { return az_total_matter(b->w) == b->m0 && az_total_energy(b->w) == b->e0; }

/* the ledger of what the bench did so far */
static void drain(Bench *b) { az_tally_merge(b->w); az_ledger_flush(b->w); }

/* the life stage of one tick for slots [s0, s1), the birth commit, the ledger, the clock */
static void life_step(Bench *b, u32 s0, u32 s1)
{
    AzWorld *w = b->w;
    az_life_range(w, s0, s1, 0);
    az_life_commit(w);
    drain(b);
    for (u32 s = s0; s < s1; s++) w->pair[s].req = 0;
    w->tick++;
}

static u32 P(const AzWorld *w, u32 id) { return w->param[id]; }
static u8 *traits(const AzWorld *w, u32 s) { return &w->trait[(size_t)s * AZ_TRAITS]; }
static void set_motor(AzWorld *w, u32 s, u32 m, int v) { w->motor[(size_t)s * AZ_MOT + m] = (i8)v; }

static void set_mode(AzWorld *w, u32 s, u32 mode)
{
    u8 *p = &w->phen[(size_t)s * AZ_PHEN + AZ_P_MATE];
    *p = (u8)((*p & ~7u) | mode);
}

#define ELSEWHERE 0xFFFFFFFFFFFFFFF0ull   /* the uid of a mother who is not in the world */

/* A founder remade as a creature of known state: born now of a mother (so it
 * has no founder's age), still (every motor 0), with the stage flags given. */
static void remake(AzWorld *w, u32 s, u32 flags)
{
    w->flags[s] = (u8)(AZ_F_ALIVE | flags);
    w->birth_tick[s] = w->tick;
    w->parent_a[s] = ELSEWHERE;
    w->pair[s].req = 0;
    w->birth[s].n = 0;
    for (u32 k = 0; k < AZ_MOT; k++) set_motor(w, s, k, 0);
}

/* ---- the rules, written a second time ---- */
static u32 m_scaled(u32 trait, u32 scale)
{
    const u64 v = ((u64)trait * scale) / 16u;
    return v > 0xFFFFFFFFull ? 0xFFFFFFFFu : (u32)v;
}

/* chance in 2^20 per check, `past` ticks into senescence: doubles every
 * LIFE_SENESC_DOUBLING ticks, linear inside a doubling */
static u32 m_age_hazard(const AzWorld *w, u64 past)
{
    const u64 dbl = P(w, AZ_WP_LIFE_SENESC_DOUBLING), k = past / dbl, f = past % dbl;
    u64 h = P(w, AZ_WP_LIFE_SENESC_HAZARD);
    if (h == 0) return 0;
    if (k >= 21u) return 1u << 20;
    h <<= k;
    h += h * f / dbl;
    return h > (1u << 20) ? 1u << 20 : (u32)h;
}

enum { ST_EGG, ST_JUV, ST_ADULT, ST_OLD, ST_DEAD };

static u32 stage_of(const AzWorld *w, u32 s)
{
    const u8 f = w->flags[s];
    if (!(f & AZ_F_ALIVE)) return ST_DEAD;
    if (f & AZ_F_EGG) return ST_EGG;
    if (f & AZ_F_JUV) return ST_JUV;
    return (f & AZ_F_SENESC) ? ST_OLD : ST_ADULT;
}

/* ---- 1. walk ---- */
static void test_walk(u64 base)
{
    enum { NW = 48 };
    AzConfig cfg = az_test_config(11, 64);
    Bench b;
    if (!bench_open(&b, &cfg)) { CHECK(!"bench"); return; }
    AzWorld *w = b.w;
    w->tick = base;
    w->param[AZ_WP_LIFE_CHECK_LOG2] = 3;
    w->param[AZ_WP_LIFE_SENESC_DOUBLING] = 32;
    u64 rs = 0xA11CEull + base;
    u32 st[NW], moves[5] = { 0, 0, 0, 0, 0 }, by_age = 0, by_hazard = 0;
    for (u32 i = 0; i < NW; i++) {
        remake(w, i, AZ_F_EGG);
        u8 *t = traits(w, i);
        t[AZ_T_EGG_TIME] = (u8)(az_test_rand64(&rs) % 200u);   /* 0 to 49 ticks  */
        t[AZ_T_MATURITY] = (u8)(az_test_rand64(&rs) % 200u);   /* 0 to 99 ticks  */
        t[AZ_T_LIFESPAN] = (u8)(az_test_rand64(&rs) % 40u);    /* 0 to 312 ticks */
        t[AZ_T_SIZE] = (u8)(az_test_rand64(&rs) % 64u);        /* at most 63 mu  */
        st[i] = ST_EGG;
    }
    books(&b);
    u32 alive = NW, wrong = 0, inexact = 0;
    const u32 log2p = P(w, AZ_WP_LIFE_CHECK_LOG2);
    for (u32 n = 0; n < 4000 && alive; n++) {
        /* the model's verdict for this tick */
        for (u32 i = 0; i < NW; i++) {
            if (st[i] == ST_DEAD || !az_phase_due(w, w->uid[i], AZ_RS_LIFE, log2p)) continue;
            const u8 *t = traits(w, i);
            const u64 age = w->tick - base;
            const u32 r0 = az_rand(w, w->uid[i], AZ_RS_LIFE, 0), r1 = az_rand(w, w->uid[i], AZ_RS_LIFE, 1);
            const u32 onset = m_scaled(t[AZ_T_LIFESPAN], P(w, AZ_WP_LIFE_SPAN_SCALE));
            const u32 was = st[i];
            if ((r0 & 0xFFFFFu) < P(w, AZ_WP_LIFE_HAZARD)) { st[i] = ST_DEAD; by_hazard++; }
            else if (st[i] == ST_EGG) {
                if (age >= m_scaled(t[AZ_T_EGG_TIME], P(w, AZ_WP_LIFE_EGG_SCALE))) st[i] = ST_JUV;
            } else if (st[i] == ST_JUV) {
                if (age >= m_scaled(t[AZ_T_MATURITY], P(w, AZ_WP_LIFE_MATURITY_SCALE)) &&
                    w->mass[i] >= m_scaled(t[AZ_T_SIZE], P(w, AZ_WP_LIFE_SIZE_MU))) st[i] = ST_ADULT;
            } else {
                if (st[i] == ST_ADULT && age >= onset) st[i] = ST_OLD;
                if (st[i] == ST_OLD && (r1 & 0xFFFFFu) < m_age_hazard(w, age >= onset ? age - onset : 0u)) {
                    st[i] = ST_DEAD; by_age++;
                }
            }
            if (st[i] != was) { moves[st[i]]++; if (st[i] == ST_DEAD) alive--; }
            /* an adult that grows old and dies at one check passed through the stage */
            if (was == ST_ADULT && st[i] == ST_DEAD && age >= onset &&
                !((r0 & 0xFFFFFu) < P(w, AZ_WP_LIFE_HAZARD))) moves[ST_OLD]++;
        }
        life_step(&b, 0, NW);
        for (u32 i = 0; i < NW; i++) wrong += stage_of(w, i) != st[i];
        inexact += !exact(&b);
    }
    CHECK_EQ(wrong, 0);
    CHECK_EQ(inexact, 0);
    CHECK_EQ(alive, 0);
    CHECK_EQ(b.hatches, moves[ST_JUV]);
    CHECK_EQ(b.stages, moves[ST_ADULT] + moves[ST_OLD]);
    CHECK_EQ(b.deaths, NW);
    CHECK_EQ(b.cause[AZ_DIE_AGE], by_age);
    CHECK_EQ(b.cause[AZ_DIE_HAZARD], by_hazard);
    CHECK(moves[ST_JUV] > 30 && moves[ST_ADULT] > 30 && moves[ST_OLD] > 30 && by_age > 30);
    printf("walk from tick %" PRIu64 ": %u creatures, %u hatched, %u grew up, %u grew old, %u died of age and %u of "
           "the base hazard, each at the tick the model gives; last death at tick %" PRIu64 "\n",
           base, (u32)NW, moves[ST_JUV], moves[ST_ADULT], moves[ST_OLD], by_age, by_hazard, w->tick - 1u);
    bench_close(&b);
}

/* ---- 2. broods ---- */
typedef struct Kids { u32 n, eggs, bad, min_mass, first; u64 a, t; i64 e; } Kids;

/* the children of mother slot m born at `tick`, and whether each is as a newborn must be */
static Kids kids_of(const AzWorld *w, u32 m, u64 tick)
{
    Kids k;
    memset(&k, 0, sizeof k);
    k.min_mass = 0xFFFFFFFFu; k.first = AZ_NONE;
    for (u32 s = 0; s < w->n_slots; s++) {
        if (!(w->flags[s] & AZ_F_ALIVE) || w->parent_a[s] != w->uid[m] || w->birth_tick[s] != tick) continue;
        if (k.first == AZ_NONE) k.first = s;
        k.n++; k.a += w->pool_a[s]; k.t += w->pool_t[s]; k.e += w->energy[s];
        if (w->mass[s] < k.min_mass) k.min_mass = w->mass[s];
        const bool egg = m_scaled(traits(w, s)[AZ_T_EGG_TIME], P(w, AZ_WP_LIFE_EGG_SCALE)) > 0;
        k.eggs += egg;
        if (w->flags[s] != (u8)(AZ_F_ALIVE | (egg ? AZ_F_EGG : AZ_F_JUV)) ||
            w->mass[s] != w->pool_a[s] + w->pool_m[s] + w->pool_t[s] || w->pool_m[s] != 0 ||
            w->generation[s] != w->generation[m] + 1u || w->lineage[s] != w->lineage[m] ||
            w->colony[s] != w->colony[m] || w->health[s] != P(w, AZ_WP_LIFE_BIRTH_HEALTH) || w->blk[s] == 0 ||
            w->c_solid[w->cell[s]] == 255u)
            k.bad++;
    }
    return k;
}

static u32 brood_cases = 0, brood_kids = 0, brood_most = 0;

static void brood_case(Bench *b, u32 m, u32 father, u32 clutch, u32 prov, u32 pt, u32 pa, i32 energy)
{
    AzWorld *w = b->w;
    u8 *t = traits(w, m);
    t[AZ_T_CLUTCH] = (u8)clutch; t[AZ_T_PROVISION] = (u8)prov;
    w->flags[m] = AZ_F_ALIVE | AZ_F_RECEPT;
    w->pool_t[m] = pt; w->pool_a[m] = pa; w->energy[m] = energy;
    w->pair[m].req = 0;
    books(b);
    /* 1 + CLUTCH * scale / 256 young share (PROVISION + 1) / 512 of her free stores, tissue first */
    const u64 share = prov + 1u, give = (((u64)pt + pa) * share) / 512u;
    u64 n = 1u + ((u64)clutch * P(w, AZ_WP_LIFE_CLUTCH_SCALE)) / 256u;
    if (give < n) n = give;
    const i32 e = energy > 0 ? (i32)(((u64)(u32)energy * share) / 512u) : 0;
    const u32 from_t = (u32)(give < pt ? give : pt), from_a = (u32)(give - from_t);
    const u32 births0 = b->births;
    brood_cases++;
    CHECK_EQ(az_life_brood(w, m, father, 0), n);
    if (!n) {
        CHECK(w->birth[m].n == 0 && w->pool_t[m] == pt && w->pool_a[m] == pa && w->energy[m] == energy);
        CHECK(w->flags[m] & AZ_F_RECEPT);
        return;
    }
    CHECK(w->birth[m].n == n && w->birth[m].father == father && w->birth[m].energy == e &&
          w->birth[m].prov[0] == from_a && w->birth[m].prov[1] == 0 && w->birth[m].prov[2] == from_t);
    CHECK(w->pool_t[m] == pt - from_t && w->pool_a[m] == pa - from_a && w->energy[m] == energy - e);
    CHECK(!(w->flags[m] & AZ_F_RECEPT));
    CHECK_EQ(az_life_brood(w, m, father, 0), 0);        /* one brood at a time */
    az_life_commit(w);
    drain(b);
    const Kids k = kids_of(w, m, w->tick);
    CHECK_EQ(k.n, n); CHECK_EQ(k.a, from_a); CHECK_EQ(k.t, from_t); CHECK_EQ(k.e, e);
    CHECK(k.min_mass >= 1u);
    CHECK_EQ(k.bad, 0);
    CHECK_EQ(b->births - births0, n);
    CHECK_EQ(w->birth[m].n, 0);
    if (k.first != AZ_NONE)
        CHECK_EQ(w->parent_b[k.first], (father != AZ_NONE && father != m) ? w->uid[father] : 0u);
    CHECK(exact(b));
    brood_kids += k.n;
    if (k.n > brood_most) brood_most = k.n;
    w->tick++;
}

static void test_broods(void)
{
    AzConfig cfg = az_test_config(3, 64);
    cfg.cap = 8192;
    Bench b;
    if (!bench_open_uniform(&b, &cfg)) { CHECK(!"bench"); return; }
    AzWorld *w = b.w;
    const u32 m = 0, f = 1;
    u64 rs = 0xB100Dull;
    remake(w, m, 0);
    /* every value of the CLUTCH trait at the default scale: buds, selfed broods and broods of a pair */
    for (u32 c = 0; c < 256u; c++) {
        const u32 prov = c == 0 ? 0u : c == 255u ? 255u : (u32)(az_test_rand64(&rs) & 255u);
        const u32 father = (c % 3u) == 0 ? AZ_NONE : (c % 3u) == 1 ? m : f;
        brood_case(&b, m, father, c, prov, (u32)(az_test_rand64(&rs) % 300u), (u32)(az_test_rand64(&rs) % 2000u),
                   (i32)(az_test_rand64(&rs) % 100000u));
    }
    /* every clutch size a scale of 256 gives to these traits: 1 + CLUTCH young */
    w->param[AZ_WP_LIFE_CLUTCH_SCALE] = 256;
    static const u8 some[] = { 0, 1, 2, 3, 7, 8, 15, 16, 31, 63, 100, 127, 128, 200, 254, 255 };
    for (u32 i = 0; i < sizeof some; i++)
        brood_case(&b, m, AZ_NONE, some[i], 255, 40000, 100000, 3000000);
    /* a poor mother has fewer young, and none without 1 mu of its own: stores of 0 to 11 mu */
    for (u32 pt = 0; pt < 4u; pt++)
        for (u32 pa = 0; pa < 9u; pa++) {
            brood_case(&b, m, AZ_NONE, 255, 255, pt, pa, 77);
            brood_case(&b, m, AZ_NONE, 255, 0, pt, pa, 0);
        }
    brood_case(&b, m, AZ_NONE, 3, 255, 1, 5, -40);      /* in debt: no energy to give, the matter still goes */
    /* the unit of the egg time is a parameter: with 0, every child is born live */
    w->param[AZ_WP_LIFE_EGG_SCALE] = 0;
    brood_case(&b, m, AZ_NONE, 63, 255, 500, 5000, 9000);
    CHECK_EQ(kids_of(w, m, w->tick - 1u).eggs, 0);
    w->param[AZ_WP_LIFE_EGG_SCALE] = 65536;
    w->param[AZ_WP_LIFE_BIRTH_HEALTH] = 123;
    brood_case(&b, m, AZ_NONE, 63, 255, 500, 5000, 9000);
    const Kids eggs = kids_of(w, m, w->tick - 1u);
    CHECK(eggs.eggs > 0);                                /* its children carry the gene */
    CHECK_EQ(w->health[eggs.first], 123);
    w->param[AZ_WP_LIFE_EGG_SCALE] = 4;
    w->param[AZ_WP_LIFE_BIRTH_HEALTH] = 40000;

    /* who is refused, and that a refusal takes nothing */
    static const u8 unfit[] = { 0, AZ_F_ALIVE, AZ_F_ALIVE | AZ_F_RECEPT | AZ_F_EGG, AZ_F_ALIVE | AZ_F_RECEPT | AZ_F_JUV,
                                AZ_F_ALIVE | AZ_F_RECEPT | AZ_F_DORMANT };
    for (u32 i = 0; i < sizeof unfit; i++) {
        w->flags[m] = unfit[i]; w->pool_a[m] = 5000; w->energy[m] = 5000;
        CHECK_EQ(az_life_brood(w, m, AZ_NONE, 0), 0);
        CHECK(w->birth[m].n == 0 && w->pool_a[m] == 5000 && w->energy[m] == 5000 && w->flags[m] == unfit[i]);
    }
    w->flags[m] = AZ_F_ALIVE | AZ_F_RECEPT;
    az_request_death(w, m, AZ_DIE_WOUNDS);               /* the dying do not breed */
    CHECK_EQ(az_life_brood(w, m, AZ_NONE, 0), 0);
    w->pair[m].req = 0;

    /* nothing caps a brood but her matter, the slots and the store: 65,281 young asked, the slots run out */
    w->param[AZ_WP_LIFE_CLUTCH_SCALE] = 65536;
    traits(w, m)[AZ_T_CLUTCH] = 255; traits(w, m)[AZ_T_PROVISION] = 255;
    w->flags[m] = AZ_F_ALIVE | AZ_F_RECEPT;
    w->pool_t[m] = 0; w->pool_a[m] = 2000000; w->energy[m] = 1000000;
    books(&b);
    const u64 denied0 = w->totals.cap_denied;
    const u32 free0 = cfg.cap - w->n_slots;              /* no slot has been freed on this bench */
    CHECK_EQ(az_life_brood(w, m, AZ_NONE, 0), 65281);
    CHECK_EQ(w->pool_a[m], 1000000);
    az_life_commit(w);
    drain(&b);
    const Kids cut = kids_of(w, m, w->tick);
    CHECK(cut.n > 0 && cut.n < 65281u);
    CHECK_EQ(cut.n, free0);                              /* every slot there was */
    CHECK(w->totals.cap_denied > denied0);
    CHECK(cut.a < 1000000u && cut.min_mass >= 1u);
    CHECK_EQ(cut.bad, 0);
    CHECK(exact(&b));                                    /* the rest went back to her cell */
    printf("broods: %u cases, %u children, the largest brood %u; then 65281 young asked with %u slots free: %u "
           "born with %" PRIu64 " mu, the other %" PRIu64 " mu and the energy refunded, books exact\n",
           brood_cases, brood_kids, brood_most, free0, cut.n, cut.a, 1000000u - cut.a);
    bench_close(&b);

    /* the creature store runs out first */
    cfg = az_test_config(3, 64);
    cfg.store_units = 6000;
    if (!bench_open(&b, &cfg)) { CHECK(!"bench"); return; }
    w = b.w;
    remake(w, m, AZ_F_RECEPT);
    w->param[AZ_WP_LIFE_CLUTCH_SCALE] = 512;
    traits(w, m)[AZ_T_CLUTCH] = 255; traits(w, m)[AZ_T_PROVISION] = 255;
    w->pool_a[m] = 100000; w->energy[m] = 100000;
    books(&b);
    CHECK_EQ(az_life_brood(w, m, AZ_NONE, 0), 511);
    az_life_commit(w);
    drain(&b);
    const Kids few = kids_of(w, m, w->tick);
    CHECK(few.n > 0 && few.n < 511u);
    CHECK(w->totals.store_denied > 0);
    CHECK_EQ(few.bad, 0);
    CHECK(exact(&b));
    printf("broods: with a creature store of 6000 units, 511 young asked, %u born, the rest refunded, books exact\n",
           few.n);
    bench_close(&b);
}

/* ---- 3. modes ---- */
/* broods the creature starts by itself in `ticks` ticks; each is undone so that she stays as she was */
static u32 broods_in(Bench *b, u32 s, u32 ticks, u32 *father)
{
    AzWorld *w = b->w;
    const u32 pt = w->pool_t[s], pa = w->pool_a[s];
    const i32 e = w->energy[s];
    u32 n = 0;
    for (u32 i = 0; i < ticks; i++) {
        az_life_range(w, s, s + 1u, 0);
        if (w->birth[s].n) {
            n++;
            if (father) *father = w->birth[s].father;
            w->birth[s].n = 0;
            w->pool_t[s] = pt; w->pool_a[s] = pa; w->energy[s] = e;
        }
        w->pair[s].req = 0;
        w->tick++;
    }
    drain(b);
    return n;
}

/* a fresh adult of the kind given, with stores to breed from and the drive given */
static void ready(AzWorld *w, u32 s, u32 mode, int repro)
{
    remake(w, s, 0);
    set_mode(w, s, mode);
    set_motor(w, s, AZ_M_REPRO, repro);
    w->pool_a[s] = 5000; w->pool_t[s] = 0; w->energy[s] = 100000;
}

static void test_modes(void)
{
    AzConfig cfg = az_test_config(13, 64);
    Bench b;
    if (!bench_open(&b, &cfg)) { CHECK(!"bench"); return; }
    AzWorld *w = b.w;
    w->param[AZ_WP_LIFE_HAZARD] = 0;
    const u32 s = 2, T = 64u << P(w, AZ_WP_LIFE_CHECK_LOG2);      /* 64 checks */
    u8 *t = traits(w, s);
    t[AZ_T_LIFESPAN] = 255; t[AZ_T_REPRO_THRESH] = 16; t[AZ_T_SIZE] = 16; t[AZ_T_DORMANCY] = 0;
    t[AZ_T_CLUTCH] = 0; t[AZ_T_PROVISION] = 127;
    u32 father = 12345;

    /* a budder whose brain drives REPRO buds once per check period; not without the drive */
    ready(w, s, AZ_MM_BUD, 50);
    books(&b);
    const u32 buds = broods_in(&b, s, T, &father);
    CHECK(buds >= 63u && buds <= 64u);
    CHECK_EQ(father, AZ_NONE);
    CHECK(exact(&b));
    ready(w, s, AZ_MM_BUD, 0);
    CHECK_EQ(broods_in(&b, s, T, NULL), 0);
    ready(w, s, AZ_MM_BUD, -90);
    CHECK_EQ(broods_in(&b, s, T, NULL), 0);

    /* a selfer at every check, with itself as the father */
    ready(w, s, AZ_MM_SELF, 1);
    const u32 selfed = broods_in(&b, s, T, &father);
    CHECK_EQ(selfed, 64);
    CHECK_EQ(father, s);
    CHECK(exact(&b));

    /* a hermaphrodite alone selfs as often as its SELFING trait says */
    u32 by_selfing[3];
    static const u8 selfing[3] = { 0, 128, 255 };
    for (u32 i = 0; i < 3u; i++) {
        ready(w, s, AZ_MM_HERMA, 1);
        t[AZ_T_SELFING] = selfing[i];
        by_selfing[i] = broods_in(&b, s, T, NULL);
    }
    CHECK_EQ(by_selfing[0], 0);
    CHECK(by_selfing[1] >= 16u && by_selfing[1] <= 48u);
    CHECK(by_selfing[2] >= 60u);

    /* the kinds that need a mate never breed alone (that is the mating module's call of az_life_brood) */
    for (u32 mode = AZ_MM_PROMISC; mode <= AZ_MM_EUSOC; mode++) {
        ready(w, s, mode, 127);
        CHECK_EQ(broods_in(&b, s, T, NULL), 0);
    }

    /* ripe is her own threshold against her own stores: REPRO_THRESH (energy) and SIZE (free biomass) */
    static const struct { u8 thresh, size; i32 energy; u32 pool_a; u32 ripe; } rp[] = {
        { 100, 16,  500,  100, 1 },     /* 400 eu needed, 500 held          */
        { 200, 16,  500,  100, 0 },     /* 800 eu needed                    */
        { 100, 50,  500,  100, 1 },     /* 50 mu needed, 100 held           */
        { 100, 200, 500,  100, 0 },     /* 200 mu needed                    */
        { 0,   0,   0,    0,   1 },     /* no gene for either: always ripe  */
        { 0,   0,   -1,   0,   0 },     /* but not in debt                  */
    };
    for (u32 i = 0; i < sizeof rp / sizeof rp[0]; i++) {
        ready(w, s, AZ_MM_EUSOC, 127);
        t[AZ_T_REPRO_THRESH] = rp[i].thresh; t[AZ_T_SIZE] = rp[i].size;
        w->energy[s] = rp[i].energy; w->pool_a[s] = rp[i].pool_a;
        (void)broods_in(&b, s, 1u << P(w, AZ_WP_LIFE_CHECK_LOG2), NULL);
        CHECK_EQ((w->flags[s] & AZ_F_RECEPT) != 0, rp[i].ripe);
    }
    /* ripeness is lost again when the stores fall */
    ready(w, s, AZ_MM_EUSOC, 127);
    w->energy[s] = 500; w->pool_a[s] = 100; t[AZ_T_REPRO_THRESH] = 100; t[AZ_T_SIZE] = 16;
    (void)broods_in(&b, s, 1u << P(w, AZ_WP_LIFE_CHECK_LOG2), NULL);
    CHECK(w->flags[s] & AZ_F_RECEPT);
    w->energy[s] = 399;
    (void)broods_in(&b, s, 1u << P(w, AZ_WP_LIFE_CHECK_LOG2), NULL);
    CHECK(!(w->flags[s] & AZ_F_RECEPT));

    /* how often a life is checked is a parameter */
    u32 per[2];
    for (u32 i = 0; i < 2u; i++) {
        w->param[AZ_WP_LIFE_CHECK_LOG2] = i ? 6u : 2u;
        ready(w, s, AZ_MM_SELF, 1);
        t[AZ_T_REPRO_THRESH] = 0; t[AZ_T_SIZE] = 0;
        per[i] = broods_in(&b, s, 1024, NULL);
    }
    CHECK_EQ(per[0], 256);
    CHECK_EQ(per[1], 16);
    printf("modes: in 64 checks a budder budded %u times, a selfer selfed %u times, a hermaphrodite %u, %u and %u "
           "times with SELFING 0, 128 and 255, the five mating kinds never; one check per 4 ticks gives %u broods "
           "in 1024 ticks, one per 64 ticks gives %u\n",
           buds, selfed, by_selfing[0], by_selfing[1], by_selfing[2], per[0], per[1]);
    bench_close(&b);
}

/* ---- 4. dormancy ---- */
static u32 step_until(Bench *b, u32 s, u32 flag, bool set, u32 limit)
{
    AzWorld *w = b->w;
    for (u32 i = 0; i < limit; i++) {
        life_step(b, s, s + 1u);
        if (((w->flags[s] & flag) != 0) == set) return i + 1u;
    }
    return 0;
}

static void test_dormancy(void)
{
    AzConfig cfg = az_test_config(17, 64);
    Bench b;
    if (!bench_open(&b, &cfg)) { CHECK(!"bench"); return; }
    AzWorld *w = b.w;
    w->param[AZ_WP_LIFE_HAZARD] = 0;
    const u32 period = 1u << P(w, AZ_WP_LIFE_CHECK_LOG2), s = 5;
    u32 *food = &w->c_sp[(size_t)w->cell[s] * AZ_NSPECIES + AZ_SP_ORG_Y];
    u8 *t = traits(w, s);
    t[AZ_T_LIFESPAN] = 255; t[AZ_T_REPRO_THRESH] = 255;

    /* entered: a resting creature below its own energy level (DORMANCY 200: 200 eu) */
    remake(w, s, 0);
    t[AZ_T_DORMANCY] = 200;
    set_motor(w, s, AZ_M_REST, 100);
    w->energy[s] = 50;
    books(&b);
    const u32 took = step_until(&b, s, AZ_F_DORMANT, true, 64u * period);
    CHECK(took > 0);
    CHECK(!(w->flags[s] & AZ_F_RECEPT));
    CHECK(b.last.kind == AZ_EV_STAGE && (b.last.aux & AZ_F_DORMANT) && b.last.a == w->uid[s]);
    CHECK(exact(&b));
    /* not entered: no gene, no want, no rest, or the brain drives against rest */
    static const struct { u8 dorm; i32 energy; int rest; } no[] = {
        { 0, 0, 127 }, { 200, 200, 127 }, { 200, 50, 0 }, { 200, 50, -100 } };
    for (u32 i = 0; i < sizeof no / sizeof no[0]; i++) {
        remake(w, s, 0);
        t[AZ_T_DORMANCY] = no[i].dorm; w->energy[s] = no[i].energy;
        set_motor(w, s, AZ_M_REST, no[i].rest);
        CHECK_EQ(step_until(&b, s, AZ_F_DORMANT, true, 64u * period), 0);
    }

    /* left by food: a cyst in a barren cell sleeps on, organic matter wakes it at its next check */
    t[AZ_T_DORMANCY] = 200;
    w->param[AZ_WP_LIFE_WAKE_CHANCE] = 0;
    remake(w, s, AZ_F_DORMANT);
    *food = 0;
    books(&b);
    CHECK_EQ(step_until(&b, s, AZ_F_DORMANT, false, 40u * period), 0);
    *food = P(w, AZ_WP_LIFE_WAKE_FOOD);
    books(&b);
    const u32 woke = step_until(&b, s, AZ_F_DORMANT, false, period);
    CHECK(woke > 0);
    CHECK(b.last.kind == AZ_EV_STAGE && !(b.last.aux & AZ_F_DORMANT));
    CHECK(exact(&b));

    /* left by chance: sure for a creature with no dormancy gene, one check in three for DORMANCY 255 */
    *food = 0;
    books(&b);
    w->param[AZ_WP_LIFE_WAKE_CHANCE] = 43691;
    u32 up0 = 0, up255 = 0;
    for (u32 i = 8; i < 40u; i++) {
        remake(w, i, AZ_F_DORMANT);
        traits(w, i)[AZ_T_DORMANCY] = (i & 1u) ? 255 : 0;
        w->c_sp[(size_t)w->cell[i] * AZ_NSPECIES + AZ_SP_ORG_Y] = 0;
    }
    books(&b);
    for (u32 k = 0; k < period; k++) life_step(&b, 8, 40);
    for (u32 i = 8; i < 40u; i++)
        if (!(w->flags[i] & AZ_F_DORMANT)) { if (i & 1u) up255++; else up0++; }
    CHECK_EQ(up0, 16);
    CHECK(up255 >= 1u && up255 <= 12u);
    CHECK(exact(&b));

    /* left by request (AZ_RQ_HATCH, from any module): a cyst opens and an egg hatches at once */
    w->param[AZ_WP_LIFE_WAKE_CHANCE] = 0;
    remake(w, s, AZ_F_DORMANT);
    w->pair[s].req = AZ_RQ_HATCH;
    life_step(&b, s, s + 1u);
    CHECK_EQ(w->flags[s], AZ_F_ALIVE);
    remake(w, s, AZ_F_EGG);
    t[AZ_T_EGG_TIME] = 255;
    const u32 h0 = b.hatches;
    w->pair[s].req = AZ_RQ_HATCH;
    life_step(&b, s, s + 1u);
    CHECK_EQ(w->flags[s], AZ_F_ALIVE | AZ_F_JUV);
    CHECK_EQ(b.hatches, h0 + 1u);
    printf("dormancy: encysted after %u ticks of rest and want, slept through 40 checks in a barren cell, woke %u "
           "ticks after food came; by chance in one period 16 of 16 without the gene and %u of 16 with DORMANCY 255\n",
           took, woke, up255);
    bench_close(&b);
}

/* ---- 5. mortality by age ---- */
static void test_mortality(void)
{
    enum { N = 1500, MARKS = 3 };
    static double p[N];
    u64 got[MARKS] = { 0, 0, 0 };
    double want[MARKS] = { 0, 0, 0 };
    u32 outside = 0;
    for (u64 seed = 1; seed <= 24u; seed++) {
        AzConfig cfg = az_test_config(seed, N);
        Bench b;
        if (!bench_open(&b, &cfg)) { CHECK(!"bench"); return; }
        AzWorld *w = b.w;
        w->param[AZ_WP_LIFE_HAZARD] = 0;
        const u32 log2p = P(w, AZ_WP_LIFE_CHECK_LOG2), dbl = P(w, AZ_WP_LIFE_SENESC_DOUBLING);
        for (u32 s = 0; s < N; s++) {
            remake(w, s, 0);
            traits(w, s)[AZ_T_LIFESPAN] = 0;             /* old from the first check: age is time in senescence */
            p[s] = 1.0;
        }
        books(&b);
        u32 inexact = 0;
        for (u32 mark = 0; mark < MARKS; mark++) {
            for (u32 k = 0; k < dbl; k++) {
                const double keep = 1.0 - (double)m_age_hazard(w, w->tick) / 1048576.0;
                for (u32 s = 0; s < N; s++)
                    if (az_phase_due(w, w->uid[s], AZ_RS_LIFE, log2p)) p[s] *= keep;
                life_step(&b, 0, N);
                inexact += !exact(&b);
            }
            u32 alive = 0;
            double mu = 0, var = 0;
            for (u32 s = 0; s < N; s++) {
                alive += (w->flags[s] & AZ_F_ALIVE) != 0;
                mu += p[s]; var += p[s] * (1.0 - p[s]);
            }
            const double d = (double)alive - mu;
            if (d * d > 16.0 * var + 1.0) outside++;     /* four standard deviations */
            got[mark] += alive; want[mark] += mu;
        }
        CHECK_EQ(inexact, 0);
        CHECK_EQ(b.cause[AZ_DIE_AGE], b.deaths);
        bench_close(&b);
    }
    CHECK_EQ(outside, 0);
    /* the hazard doubles: each doubling time takes a larger share of those still alive */
    const double q1 = 1.0 - (double)got[0] / (24.0 * N), q2 = 1.0 - (double)got[1] / (double)got[0],
                 q3 = 1.0 - (double)got[2] / (double)got[1];
    CHECK(q2 > 1.6 * q1 && q3 > 1.6 * q2);
    printf("mortality: 24 seeds x %u creatures, alive after 1, 2 and 3 doubling times %" PRIu64 ", %" PRIu64 ", %"
           PRIu64 " (the doubling hazard gives %.1f, %.1f, %.1f); every seed within four standard deviations; "
           "share dying per doubling time %.3f, %.3f, %.3f\n",
           (u32)N, got[0], got[1], got[2], want[0], want[1], want[2], q1, q2, q3);
}

/* ---- 6. traits ---- */
typedef struct Spread { u32 n, at_mother, far; u32 max_dx, max_dy; } Spread;

static u32 udist(u32 a, u32 b) { const u32 d = a - b; return d < 0x80000000u ? d : 0u - d; }

/* one brood of 200 buds from mother m with the DISPERSAL trait given: where the children are */
static Spread brood_spread(Bench *b, u32 m, u32 dispersal, u32 near)
{
    AzWorld *w = b->w;
    Spread r = { 0, 0, 0, 0, 0 };
    remake(w, m, AZ_F_RECEPT);
    traits(w, m)[AZ_T_CLUTCH] = 255; traits(w, m)[AZ_T_PROVISION] = 255; traits(w, m)[AZ_T_DISPERSAL] = (u8)dispersal;
    w->pool_a[m] = 100000; w->energy[m] = 100000;
    books(b);
    CHECK_EQ(az_life_brood(w, m, AZ_NONE, 0), 200);
    az_life_commit(w);
    drain(b);
    CHECK(exact(b));
    const u64 reach = ((u64)dispersal + 1u) * P(w, AZ_WP_LIFE_DISPERSAL_SCALE);
    const u32 half_x = (u32)(((((u64)1 << w->shift_x) * reach) / 128u | 1u) / 2u) + 1u;
    const u32 half_y = (u32)(((((u64)1 << w->shift_y) * reach) / 128u | 1u) / 2u) + 1u;
    for (u32 s = 0; s < w->n_slots; s++) {
        if (!(w->flags[s] & AZ_F_ALIVE) || w->parent_a[s] != w->uid[m] || w->birth_tick[s] != w->tick) continue;
        const u32 dx = udist(w->pos_x[s], w->pos_x[m]), dy = udist(w->pos_y[s], w->pos_y[m]);
        r.n++;
        if (dx > r.max_dx) r.max_dx = dx;
        if (dy > r.max_dy) r.max_dy = dy;
        r.at_mother += w->pos_x[s] == w->pos_x[m] && w->pos_y[s] == w->pos_y[m] && w->pos_z[s] == w->pos_z[m];
        r.far += dx > near || dy > near;
        CHECK(dx <= half_x && dy <= half_y);             /* inside her box */
        CHECK(w->c_solid[w->cell[s]] != 255u);
    }
    w->tick++;
    return r;
}

static u32 kind_count[2][AZ_PART_KINDS];

#ifdef AZ_TEST_DEVELOPED_SENSE_TRAITS
enum { TRAIT_FOUNDERS = 16 };
typedef struct SenseOutcome {
    u64 uid;
    u32 pos[3];
    i8 motor[3];
    u8 receptor[4], trait;
} SenseOutcome;

/* A noncore family with weight 16 addresses one trait with unit increments.
 * Choose expression using the developed trait alone, before running a tick. */
static bool develop_single_allele(AzWorld *w, u32 s, u32 trait, u8 target)
{
    u32 family = AZ_CORE_FAMS;
    u64 mapping = 0;
    for (; family <= UINT16_MAX; family++) {
        mapping = az_mix64(w->code.latent_key ^ ((u64)family * UINT64_C(0x9E3779B97F4A7C15)));
        if (mapping % AZ_TRAITS == trait && ((mapping >> 32) % 17u) == 8u) break;
    }
    u32 bytes;
    (void)az_seg(w, s, AZ_SEG_GEN_A, &bytes);
    if (family > UINT16_MAX || !az_blk_resize(w, s, AZ_SEG_GEN_A, bytes + 4u * sizeof(AzGene))) return false;
    u32 length;
    AzGene *genes = az_seg(w, s, AZ_SEG_GEN_A, &length);
    if (length != bytes + 4u * sizeof(AzGene)) return false;
    u8 prior[AZ_TRAITS];
    memcpy(prior, traits(w, s), sizeof prior);
    i32 low = -4 * 126, high = 4 * 126;
    while (low <= high) {
        const i32 dose = low + (high - low) / 2;
        i32 left = (mapping >> 63) ? -dose : dose;
        for (u32 i = bytes / sizeof(AzGene); i < length / sizeof(AzGene); i++) {
            const i32 part = az_clamp_i32(left, -126, 126);
            const u8 allele = (u8)(128 + part + (part > 0) - (part < 0));
            genes[i] = (AzGene){ (u16)family, allele, 255 };
            left -= part;
        }
        az_develop(w, s);
        if (traits(w, s)[trait] == target) break;
        if (traits(w, s)[trait] < target) low = dose + 1;
        else high = dose - 1;
    }
    for (u32 i = 0; i < AZ_TRAITS; i++)
        if (i != trait && prior[i] != traits(w, s)[i]) return false;
    return traits(w, s)[trait] == target;
}

static bool run_sense_allele(u32 trait, u8 target, u32 lesion, bool floor_contact, bool thin, SenseOutcome out[TRAIT_FOUNDERS], u64 hash[2])
{
    AzConfig cfg = az_test_config(19, TRAIT_FOUNDERS);
    cfg.cap = 96;
    cfg.lesion = lesion;
    AzArena arena = {0};
    AzWinInfo info = {0};
    if (!az_win_arena(&arena, az_world_bytes(&cfg), AZ_WIN_LOCK, &info)) return false;
    AzWorld *w = az_world_create(&arena, &cfg, NULL);
    bool ok = w != NULL && info.arena_locked;
    /* The original uniform substrate saturates viscosity and can quantize
     * a changed motor command back to exactly the same velocity. Predeclare
     * a frictionless substrate in both matched arms to measure transmission.
     * Keep the original viscosity arms below for comparison. */
    if (ok && thin) {
        ok = az_param_set(w, AZ_WP_CHEM_VISC_LIQUID, 0) &&
             az_param_set(w, AZ_WP_CHEM_VISC_SOLID, 0) &&
             az_param_set(w, AZ_WP_CHEM_VISC_GAS, 0);
    }
    if (ok) {
        az_test_uniform(w);
        ok = w->n_slots == TRAIT_FOUNDERS;
    }
    for (u32 c = 0; ok && c < w->n_cells; c++) {
        u32 *sp = &w->c_sp[(size_t)c * AZ_NSPECIES];
        sp[AZ_SP_ORG_Y] += az_cell_x(w, c) * 4096u;
        sp[AZ_SP_TOX_Z] = 64;
        az_chem_derive(w, c);
    }
    /* The previous reference physics applied thrust in free space. A real
     * body needs swim or crawl coupling. Keep the original free-space trial
     * as a control and use the same level floor for both supported arms. */
    if (floor_contact) {
        for (u32 slot = 0; ok && slot < TRAIT_FOUNDERS; ++slot) {
            w->pos_z[slot] = 65536u;
            az_place(w, slot);
        }
    }
    if (ok) hash[0] = az_state_hash(w);
    for (u32 s = 0; ok && s < TRAIT_FOUNDERS; s++)
        ok = develop_single_allele(w, s, trait, target);
    if (ok) {
        hash[1] = az_state_hash(w);
        const u64 matter = az_total_matter(w);
        const i64 energy = az_total_energy(w);
        az_tick(w);
        CHECK_EQ(az_total_matter(w), matter); CHECK_EQ(az_total_energy(w), energy);
        CHECK_EQ(w->totals.field_underflow, 0); CHECK_EQ(w->totals.dropped_events, 0);
        for (u32 s = 0; s < TRAIT_FOUNDERS; s++) {
            u32 bytes;
            const u8 *mind = az_seg(w, s, AZ_SEG_MIND, &bytes);
            if (!(w->flags[s] & AZ_F_ALIVE) || bytes < AZ_SENS) { ok = false; break; }
            SenseOutcome *r = &out[s];
            r->uid = w->uid[s]; r->trait = traits(w, s)[trait];
            r->pos[0] = w->pos_x[s]; r->pos[1] = w->pos_y[s]; r->pos[2] = w->pos_z[s];
            memcpy(r->motor, &w->motor[(size_t)s * AZ_MOT + AZ_M_FWD], sizeof r->motor);
            if (trait == AZ_T_TAXIS) memcpy(r->receptor, &mind[AZ_S_FOOD], 4);
            else r->receptor[0] = mind[AZ_S_TOXIN];
        }
    }
    az_win_arena_free(&arena);
    return ok;
}

static void test_developed_sense_traits(void)
{
    enum { SUPPORTED, SENSORY_MASK, THRUST_MASK, ORIGINAL_SUBSTRATE, VISCOUS_FLOOR };
    u32 supported_links[2] = {0};
    const u32 owned[] = { AZ_T_TAXIS, AZ_T_CURVE };
#ifdef AZ_TEST_DEVELOPED_PHYSICS_TRAITS
    const u32 cases = 5;
#else
    const u32 cases = 2;
#endif
    for (u32 k = 0; k < AZ_LEN(owned); k++) for (u32 mode = 0; mode < cases; mode++) {
        SenseOutcome low[TRAIT_FOUNDERS] = {0}, high[TRAIT_FOUNDERS] = {0};
        u64 low_hash[2], high_hash[2];
        const bool supported_fixture = cases == 5 && mode != ORIGINAL_SUBSTRATE;
        const bool thin = cases == 5 && mode < ORIGINAL_SUBSTRATE;
        const bool masked = mode == SENSORY_MASK;
        const u32 lesion = masked ? (k == 0 ? AZ_LESION_TAXIS : AZ_LESION_SENSES) :
                           mode == THRUST_MASK ? AZ_LESION_THRUST : 0;
        if (!run_sense_allele(owned[k], 16, lesion, supported_fixture, thin, low, low_hash) ||
            !run_sense_allele(owned[k], 240, lesion, supported_fixture, thin, high, high_hash)) {
            CHECK(!"locked developed trait fixture"); return;
        }
        CHECK_EQ(low_hash[0], high_hash[0]);
        CHECK(low_hash[1] != high_hash[1]);
        u32 receptor = 0, motor = 0, moved = 0, linked = 0;
        for (u32 i = 0; i < TRAIT_FOUNDERS; i++) {
            CHECK_EQ(low[i].uid, high[i].uid);
            CHECK_EQ(low[i].trait, 16); CHECK_EQ(high[i].trait, 240);
            const bool r = memcmp(low[i].receptor, high[i].receptor, sizeof low[i].receptor) != 0;
            const bool m = memcmp(low[i].motor, high[i].motor, sizeof low[i].motor) != 0;
            const bool p = memcmp(low[i].pos, high[i].pos, sizeof low[i].pos) != 0;
            receptor += r; motor += m; moved += p; linked += r && m && p;
        }
        printf("developed %s lesion=%u founders=%u floor=%u thin=%u receptor=%u motor=%u position=%u linked=%u edited=%016" PRIx64 "/%016" PRIx64 "\n",
               k ? "CURVE" : "TAXIS", lesion, TRAIT_FOUNDERS, supported_fixture ? 1u : 0u, thin ? 1u : 0u, receptor, motor, moved, linked, low_hash[1], high_hash[1]);
        if (masked) {
            CHECK_EQ(receptor, 0); CHECK_EQ(motor, 0); CHECK_EQ(moved, 0);
        } else if (mode == THRUST_MASK || mode == ORIGINAL_SUBSTRATE) {
            CHECK(receptor > 0); CHECK(motor > 0); CHECK_EQ(moved, 0);
        } else if (mode == VISCOUS_FLOOR) {
            CHECK(receptor > 0); CHECK(motor > 0); CHECK(linked <= supported_links[k]);
        } else {
            CHECK(linked > 0);
            supported_links[k] = linked;
        }
    }
}
#endif

#ifdef AZ_TEST_DEVELOPED_PHYSICS_TRAITS
typedef struct PhysTraitOutcome {
    i32 motion[TRAIT_FOUNDERS], paid[TRAIT_FOUNDERS];
} PhysTraitOutcome;

/* The same native founders receive a different dose of the gene for exactly
 * one trait. Alternating signs expose directional bias. Each physical mask
 * removes the causal path tested by its matching positive control. */
static bool run_physics_allele(u32 trait, u8 target, bool masked, PhysTraitOutcome *out)
{
    AzConfig cfg = az_test_config(19, TRAIT_FOUNDERS);
    cfg.cap = 96;
    if (masked && trait == AZ_T_DENSITY) cfg.lesion |= AZ_LESION_GRAVITY;
    if (masked && trait == AZ_T_THRUST) cfg.lesion |= AZ_LESION_THRUST;
    AzArena arena = {0};
    AzWinInfo info = {0};
    if (!az_win_arena(&arena, az_world_bytes(&cfg), AZ_WIN_LOCK, &info)) return false;
    AzWorld *w = az_world_create(&arena, &cfg, NULL);
    bool ok = w && info.arena_locked;
    if (ok) az_worldgen(w);
    AzPercept contact = {0};
    for (u32 s = 0; ok && s < TRAIT_FOUNDERS; ++s) {
        ok = develop_single_allele(w, s, trait, target);
        if (!ok) break;
        const u8 hot = trait == AZ_T_DENSITY ? AZ_P_DENS : trait == AZ_T_DRAG ? AZ_P_DRAG : AZ_P_THRUST;
        CHECK_EQ(w->phen[(size_t)s * AZ_PHEN + hot], target);
        const i32 direction = (s & 1u) ? -1 : 1;
        w->flags[s] = AZ_F_ALIVE;
        w->pos_x[s] = w->pos_y[s] = 0x70800000u;
        w->pos_z[s] = trait == AZ_T_THRUST ? 65536u : 0x70800000u;
        az_place(w, s);
        const u32 cell = w->cell[s];
        w->c_solid[cell] = 0;
        w->c_med[(size_t)cell * 2] = 128;
        w->c_med[(size_t)cell * 2 + 1] = trait == AZ_T_DRAG && !masked ? 255 : 0;
        for (u32 k = 0; k < 3; k++) {
            w->c_grav[(size_t)cell * 4 + k] = 0;
            w->c_wind[(size_t)cell * 3 + k] = 0;
        }
        if (trait == AZ_T_DENSITY) w->c_grav[(size_t)cell * 4 + 2] = (i8)(16 * direction);
        w->rot_w[s] = 16384;
        w->rot_a[s] = w->rot_b[s] = w->rot_c[s] = 0;
        w->ang_a[s] = w->ang_b[s] = w->ang_c[s] = 0;
        w->vel_x[s] = trait == AZ_T_DRAG ? (i16)(2048 * direction) : 0;
        w->vel_y[s] = w->vel_z[s] = 0;
        i8 *m = w->motor + (size_t)s * AZ_MOT;
        memset(m, 0, AZ_MOT);
        if (trait == AZ_T_THRUST) m[AZ_M_FWD] = (i8)(100 * direction);
        const i32 before = w->energy[s];
        const u64 matter = az_total_matter(w);
        const i64 energy = az_total_energy(w);
        az_physics_steer(w, s, &contact, 0);
        az_physics_range(w, s, s + 1u, 0);
        out->paid[s] = before - w->energy[s];
        out->motion[s] = direction * (trait == AZ_T_DENSITY ? w->vel_z[s] : w->vel_x[s]);
        CHECK_EQ(az_total_matter(w), matter);
        CHECK_EQ(az_total_energy(w), energy);
        CHECK_EQ(w->totals.field_underflow, 0);
        CHECK_EQ(w->totals.dropped_events, 0);
    }
    az_win_arena_free(&arena);
    return ok;
}

static void test_developed_physics_traits(void)
{
    const u32 trait[] = {AZ_T_DENSITY, AZ_T_DRAG, AZ_T_THRUST};
    const char *name[] = {"DENSITY", "DRAG", "THRUST"};
    for (u32 k = 0; k < AZ_LEN(trait); ++k) {
        for (u32 masked = 0; masked <= 1; ++masked) {
            PhysTraitOutcome lo, hi;
            if (!run_physics_allele(trait[k], 16, masked != 0, &lo) ||
                !run_physics_allele(trait[k], 240, masked != 0, &hi)) {
                CHECK(!"locked physics allele fixture");
                return;
            }
            u32 directed = 0, cost = 0;
            for (u32 s = 0; s < TRAIT_FOUNDERS; ++s) {
                directed += k == 1 ? hi.motion[s] < lo.motion[s] : hi.motion[s] > lo.motion[s];
                cost += hi.paid[s] > lo.paid[s];
                if (masked) {
                    CHECK_EQ(hi.motion[s], lo.motion[s]);
                    CHECK_EQ(hi.paid[s], lo.paid[s]);
                }
            }
            printf("developed %s mask=%u signed-motion=%u/%u energy-cost=%u/%u\n",
                   name[k], masked, directed, TRAIT_FOUNDERS, cost, TRAIT_FOUNDERS);
            CHECK_EQ(directed, masked ? 0 : TRAIT_FOUNDERS);
            if (trait[k] == AZ_T_THRUST) CHECK_EQ(cost, masked ? 0 : TRAIT_FOUNDERS);
        }
    }
}
#endif

static void test_traits(void)
{
#ifdef AZ_TEST_DEVELOPED_SENSE_TRAITS
    test_developed_sense_traits();
#endif
#ifdef AZ_TEST_DEVELOPED_PHYSICS_TRAITS
    test_developed_physics_traits();
#endif
    AzConfig cfg = az_test_config(19, 64);
    cfg.cap = 4096;
    Bench b;
    if (!bench_open_uniform(&b, &cfg)) { CHECK(!"bench"); return; }
    AzWorld *w = b.w;
    w->param[AZ_WP_LIFE_HAZARD] = 0;
    const u32 period = 1u << P(w, AZ_WP_LIFE_CHECK_LOG2);

    /* SIZE: a young one of age stays young until its body has the matter its own SIZE asks */
    const u32 j = 4;
    remake(w, j, AZ_F_JUV);
    traits(w, j)[AZ_T_MATURITY] = 0; traits(w, j)[AZ_T_SIZE] = 255; traits(w, j)[AZ_T_LIFESPAN] = 255;
    w->param[AZ_WP_LIFE_SIZE_MU] = 16u * (w->mass[j] / 255u + 1u);        /* 255 units are more than it weighs */
    books(&b);
    CHECK_EQ(step_until(&b, j, AZ_F_JUV, false, 20u * period), 0);
    traits(w, j)[AZ_T_SIZE] = 1;
    CHECK(step_until(&b, j, AZ_F_JUV, false, period) > 0);
    CHECK(b.last.kind == AZ_EV_STAGE && b.last.aux == 0 && b.last.a == w->uid[j]);
    w->param[AZ_WP_LIFE_SIZE_MU] = 16;

    /* DISPERSAL: how far from her the young are placed */
    w->param[AZ_WP_LIFE_CLUTCH_SCALE] = 200;             /* 1 + 255 * 200 / 256 = 200 young */
    const u32 m = 3;
    const u32 cell_x = 1u << w->shift_x, cell_y = 1u << w->shift_y;
    const Spread near = brood_spread(&b, m, 0, cell_x / 32u);
    const Spread wide = brood_spread(&b, m, 255, cell_x / 32u);
    CHECK_EQ(near.n, 200); CHECK_EQ(wide.n, 200);
    CHECK_EQ(near.far, 0);
    CHECK(wide.far > 100u);
    CHECK(wide.max_dx > cell_x && wide.max_dy > cell_y);
    /* never in bedrock: with rock everywhere but in her own cell, every child is in her cell */
    for (u32 c = 0; c < w->n_cells; c++)
        if (c != w->cell[m]) w->c_solid[c] = 255;
    const Spread rock = brood_spread(&b, m, 255, cell_x / 32u);
    CHECK_EQ(rock.n, 200);
    CHECK(rock.at_mother > 100u);
    u32 out = 0;
    for (u32 s = 0; s < w->n_slots; s++)
        out += (w->flags[s] & AZ_F_ALIVE) && w->parent_a[s] == w->uid[m] && w->birth_tick[s] == w->tick - 1u &&
               w->cell[s] != w->cell[m];
    CHECK_EQ(out, 0);
    printf("traits: DISPERSAL 0 puts 200 young within %u of 2^%u position units of her, DISPERSAL 255 up to %u "
           "(%u beyond 1/32 of a cell); with rock all around, %u of 200 are set down at her side\n",
           near.max_dx > near.max_dy ? near.max_dx : near.max_dy, w->shift_x,
           wide.max_dx > wide.max_dy ? wide.max_dx : wide.max_dy, wide.far, rock.at_mother);
    bench_close(&b);

    /* DEV_PLAST: the same 200 children born in two places. Twin worlds, the same mother; in the first every
     * cell is rich in food, lit, clean and still, in the second barren, dark, poisoned and windy. */
    Bench tw[2];
    u32 bent = 0, plastic = 0, fixed = 0, fixed_same = 1, cores = 1, lowest = 0;
    for (u32 k = 0; k < 2u; k++) {
        cfg = az_test_config(19, 64);
        cfg.lesion = AZ_LESION_MUTATION;                 /* the children are their mother's clones */
        if (!bench_open(&tw[k], &cfg)) { CHECK(!"bench"); return; }
        AzWorld *v = tw[k].w;
        u32 mom = 0, nop = 0;                            /* the most and the least plastic founder */
        for (u32 q = 1; q < 64u; q++) {
            if (traits(v, q)[AZ_T_DEV_PLAST] > traits(v, mom)[AZ_T_DEV_PLAST]) mom = q;
            if (traits(v, q)[AZ_T_DEV_PLAST] < traits(v, nop)[AZ_T_DEV_PLAST]) nop = q;
        }
        CHECK(traits(v, mom)[AZ_T_DEV_PLAST] > 0);
        lowest = traits(v, nop)[AZ_T_DEV_PLAST];
        for (u32 c = 0; c < v->n_cells; c++) {
            v->c_sp[(size_t)c * AZ_NSPECIES + AZ_SP_ORG_Y] = k ? 0u : 1u << 24;
            v->c_sp[(size_t)c * AZ_NSPECIES + AZ_SP_TOX_Z] = k ? 1u << 24 : 0u;
            v->c_light[c] = k ? 0 : 255;
            for (u32 a = 0; a < 3u; a++) v->c_wind[(size_t)c * 3 + a] = k ? 100 : 0;
        }
        v->param[AZ_WP_LIFE_CLUTCH_SCALE] = 200;
        (void)brood_spread(&tw[k], mom, 0, 0xFFFFFFFFu);
        (void)brood_spread(&tw[k], nop, 0, 0xFFFFFFFFu);
        for (u32 q = 0; q < v->n_slots; q++) {
            if (!(v->flags[q] & AZ_F_ALIVE) || v->birth_tick[q] != v->tick - 2u || v->parent_a[q] != v->uid[mom])
                continue;
            u32 np;
            const AzPart *part = az_parts(v, q, &np);
            for (u32 i = 1; i < np; i++) kind_count[k][part[i].kind]++;
        }
    }
    {
        const AzWorld *a = tw[0].w, *c = tw[1].w;
        CHECK_EQ(a->n_slots, c->n_slots);
        for (u32 q = 64; q < a->n_slots && q < c->n_slots; q++) {
            u32 na, nc;
            const AzPart *pa = az_parts(a, q, &na), *pc = az_parts(c, q, &nc);
            if (a->uid[q] != c->uid[q] || na != nc ||
                traits(a, q)[AZ_T_DEV_PLAST] != traits(c, q)[AZ_T_DEV_PLAST]) { fixed_same = 0; continue; }
            u32 differ = 0;
            for (u32 i = 1; i < na; i++) differ += pa[i].kind != pc[i].kind;
            if (na && pa[0].kind != pc[0].kind) cores = 0;
            if (traits(a, q)[AZ_T_DEV_PLAST] == 0) { fixed++; if (differ) fixed_same = 0; }
            else { plastic++; bent += differ != 0; }
        }
    }
    CHECK(fixed_same);                                   /* no plasticity gene: the genome's body, wherever born */
    CHECK(cores);                                        /* the first part is never bent */
    CHECK(plastic > 0 && bent > 0);
    CHECK(kind_count[0][AZ_PART_GUT] > kind_count[1][AZ_PART_GUT]);
    CHECK(kind_count[1][AZ_PART_STORE] > kind_count[0][AZ_PART_STORE]);
    CHECK(kind_count[1][AZ_PART_SENSOR] > kind_count[0][AZ_PART_SENSOR]);
    CHECK(kind_count[1][AZ_PART_ARMOR] > kind_count[0][AZ_PART_ARMOR]);
    CHECK(kind_count[1][AZ_PART_FIN] > kind_count[0][AZ_PART_FIN]);
    printf("traits: DEV_PLAST: twin worlds, the same mothers. %u of %u children with the gene have another body "
           "in the other place, the %u children of the least plastic founder (DEV_PLAST %u) %s; parts of the "
           "plastic brood, rich/lit/clean/still vs barren/dark/poisoned/windy: gut %u/%u, store %u/%u, sensor "
           "%u/%u, armour %u/%u, fin %u/%u\n", bent, plastic, lowest ? 0u : fixed, lowest,
           lowest ? "are plastic too" : "have the same body in both",
           kind_count[0][AZ_PART_GUT], kind_count[1][AZ_PART_GUT], kind_count[0][AZ_PART_STORE],
           kind_count[1][AZ_PART_STORE], kind_count[0][AZ_PART_SENSOR], kind_count[1][AZ_PART_SENSOR],
           kind_count[0][AZ_PART_ARMOR], kind_count[1][AZ_PART_ARMOR], kind_count[0][AZ_PART_FIN],
           kind_count[1][AZ_PART_FIN]);
    bench_close(&tw[0]);
    bench_close(&tw[1]);

    /* the founders are not one cohort: each has an age of its own, up to a share of its time to senescence */
    u32 old[2] = { 0, 0 }, old_late = 0;
    for (u32 k = 0; k < 2u; k++) {
        cfg = az_test_config(23, 400);
        if (!bench_open(&b, &cfg)) { CHECK(!"bench"); return; }
        w = b.w;
        w->param[AZ_WP_LIFE_HAZARD] = 0;
        w->param[AZ_WP_LIFE_SENESC_HAZARD] = 0;
        w->param[AZ_WP_LIFE_FOUNDER_SPREAD] = k ? 256u : 0u;
        for (u32 s = 0; s < 400u; s++) {
            w->flags[s] = AZ_F_ALIVE;
            traits(w, s)[AZ_T_LIFESPAN] = 64;            /* 512 ticks */
            for (u32 q = 0; q < AZ_MOT; q++) set_motor(w, s, q, 0);
        }
        books(&b);
        for (u32 i = 0; i < 256u; i++) life_step(&b, 0, 400);
        for (u32 s = 0; s < 400u; s++) old[k] += (w->flags[s] & AZ_F_SENESC) != 0;
        if (!k) {
            for (u32 i = 0; i < 256u + 16u; i++) life_step(&b, 0, 400);
            for (u32 s = 0; s < 400u; s++) old_late += (w->flags[s] & AZ_F_SENESC) != 0;
        }
        CHECK(exact(&b));
        bench_close(&b);
    }
    CHECK_EQ(old[0], 0);
    CHECK_EQ(old_late, 400);
    CHECK(old[1] > 140u && old[1] < 260u);
    printf("traits: 400 founders with 512 ticks to senescence, after 256 ticks: %u old with a founder spread of 0, "
           "%u with a spread of the whole span\n", old[0], old[1]);
}

/* ---- 7. edges ---- */
static void test_edges(void)
{
    AzConfig cfg = az_test_config(29, 64);
    Bench b;
    if (!bench_open(&b, &cfg)) { CHECK(!"bench"); return; }
    AzWorld *w = b.w;
    const u32 period = 1u << P(w, AZ_WP_LIFE_CHECK_LOG2);
    w->param[AZ_WP_LIFE_HAZARD] = 0;

    /* a creature with no gene of the family (every trait reads 0) lives by the same rules: no egg time, so
     * an egg hatches at its first check; grown at the next; old at once at the one after; one child a brood */
    const u32 z = 6;
    remake(w, z, AZ_F_EGG);
    memset(traits(w, z), 0, AZ_TRAITS);
    w->param[AZ_WP_LIFE_SENESC_HAZARD] = 0;
    w->pool_a[z] = 2000; w->pool_t[z] = 0; w->energy[z] = 5000;
    books(&b);
    for (u32 k = 0; k < period; k++) life_step(&b, z, z + 1u);
    CHECK_EQ(w->flags[z], AZ_F_ALIVE | AZ_F_JUV);
    for (u32 k = 0; k < period; k++) life_step(&b, z, z + 1u);
    CHECK_EQ(w->flags[z], AZ_F_ALIVE);
    for (u32 k = 0; k < period; k++) life_step(&b, z, z + 1u);
    CHECK_EQ(w->flags[z], AZ_F_ALIVE | AZ_F_SENESC | AZ_F_RECEPT);
    CHECK_EQ(az_life_brood(w, z, AZ_NONE, 0), 1);        /* no clutch gene: one child */
    CHECK_EQ(w->birth[z].prov[0], 2000u / 512u);         /* no provision gene: 1/512 of her stores */
    az_life_commit(w);
    drain(&b);
    CHECK(exact(&b));
    set_motor(w, z, AZ_M_REST, 127);                     /* no dormancy gene: never a cyst */
    w->energy[z] = 0;
    books(&b);
    CHECK_EQ(step_until(&b, z, AZ_F_DORMANT, true, 32u * period), 0);
    w->param[AZ_WP_LIFE_SENESC_HAZARD] = 1u << 20;       /* and old age takes it at the next check */
    CHECK(step_until(&b, z, AZ_F_ALIVE, false, period) > 0);
    CHECK(b.last.kind == AZ_EV_DEATH && b.last.aux == AZ_DIE_AGE && b.last.a == w->uid[z]);
    CHECK(exact(&b));

    /* every trait at 255 and every scale at its highest: the arithmetic holds, the creature is an egg for
     * 255 * 65536 / 16 ticks, and it still hatches on request and walks on */
    static const u16 scales[] = { AZ_WP_LIFE_EGG_SCALE, AZ_WP_LIFE_MATURITY_SCALE, AZ_WP_LIFE_SIZE_MU,
                                  AZ_WP_LIFE_SPAN_SCALE, AZ_WP_LIFE_REPRO_EU, AZ_WP_LIFE_CLUTCH_SCALE,
                                  AZ_WP_LIFE_DORMANT_EU, AZ_WP_LIFE_WAKE_FOOD, AZ_WP_LIFE_WAKE_CHANCE,
                                  AZ_WP_LIFE_DISPERSAL_SCALE, AZ_WP_LIFE_SENESC_DOUBLING, AZ_WP_LIFE_FOUNDER_SPREAD };
    static const u32 highest[] = { 65536, 65536, 65536, 1048576, 1048576, 65536, 1048576, 1073741824, 65536, 4096,
                                   1048576, 256 };
    for (u32 i = 0; i < sizeof scales / sizeof scales[0]; i++) w->param[scales[i]] = highest[i];
    w->param[AZ_WP_LIFE_SENESC_HAZARD] = 8192;
    const u32 x = 7;
    remake(w, x, AZ_F_EGG);
    memset(traits(w, x), 255, AZ_TRAITS);
    set_motor(w, x, AZ_M_REPRO, 127);
    set_motor(w, x, AZ_M_REST, 127);
    books(&b);
    for (u32 k = 0; k < 8u * period; k++) life_step(&b, x, x + 1u);
    CHECK_EQ(w->flags[x], AZ_F_ALIVE | AZ_F_EGG);
    w->pair[x].req = AZ_RQ_HATCH;
    life_step(&b, x, x + 1u);
    CHECK_EQ(w->flags[x], AZ_F_ALIVE | AZ_F_JUV);
    w->birth_tick[x] = 0;
    w->tick = 0x4000000000ull;                           /* 2^38 ticks later */
    for (u32 k = 0; k < 8u * period; k++) life_step(&b, x, x + 1u);
    CHECK_EQ(w->flags[x], AZ_F_ALIVE | AZ_F_JUV | AZ_F_DORMANT);          /* rest and want: a young cyst */
    set_motor(w, x, AZ_M_REST, 0);
    w->pair[x].req = AZ_RQ_HATCH;
    life_step(&b, x, x + 1u);
    for (u32 k = 0; k < 2u * period; k++) life_step(&b, x, x + 1u);
    CHECK_EQ(w->flags[x], AZ_F_ALIVE | AZ_F_JUV);        /* of age, but its body is too small for its SIZE */
    w->param[AZ_WP_LIFE_SIZE_MU] = 0;
    for (u32 k = 0; k < period; k++) life_step(&b, x, x + 1u);
    CHECK(!(w->flags[x] & AZ_F_JUV));
    w->flags[x] |= AZ_F_RECEPT;
    w->pool_a[x] = 0x7FFFFFFFu; w->pool_t[x] = 0x7FFFFFFFu; w->energy[x] = 0x7FFFFFFF;
    CHECK_EQ(az_life_brood(w, x, AZ_NONE, 0), 1u + 255u * 256u);
    CHECK_EQ(w->birth[x].prov[0] + (u64)w->birth[x].prov[2], 0x7FFFFFFFull);    /* half of 2^32 - 2 */
    CHECK_EQ(w->birth[x].energy, 0x3FFFFFFF);
    w->pool_a[x] += w->birth[x].prov[0]; w->pool_t[x] += w->birth[x].prov[2]; w->energy[x] += w->birth[x].energy;
    w->birth[x].n = 0;

    /* a body of one part: shed down to the core, then killed; everything it held is back in the world */
    for (u32 i = 0; i < sizeof scales / sizeof scales[0]; i++) w->param[scales[i]] = 0;
    w->param[AZ_WP_LIFE_SPAN_SCALE] = 128; w->param[AZ_WP_LIFE_SENESC_DOUBLING] = 128;
    const u32 o = 9;
    remake(w, o, 0);
    u32 np;
    (void)az_parts(w, o, &np);
    books(&b);
    for (u32 guard = 0; np > 1u && guard < 100000u; guard++) {
        az_part_shed_request(w, o, np - 1u);
        az_store_commit(w);
        w->pair[o].req = 0;
        (void)az_parts(w, o, &np);
    }
    CHECK_EQ(np, 1);
    CHECK(exact(&b));
    w->flags[o] |= AZ_F_RECEPT;
    w->pool_a[o] = 5000; w->energy[o] = 100;
    traits(w, o)[AZ_T_CLUTCH] = 0; traits(w, o)[AZ_T_PROVISION] = 255;
    books(&b);
    CHECK_EQ(az_life_brood(w, o, AZ_NONE, 0), 1);        /* it breeds like any other */
    az_life_commit(w);
    drain(&b);
    CHECK(exact(&b));
    const u64 cell_before = w->c_sp[(size_t)w->cell[o] * AZ_NSPECIES + AZ_SP_ORG_Y];
    const u32 d0 = b.deaths;
    az_request_death(w, o, AZ_DIE_KILLED);
    life_step(&b, o, o + 1u);
    CHECK_EQ(w->flags[o], 0);
    CHECK(w->pool_a[o] == 0 && w->pool_t[o] == 0 && w->energy[o] == 0 && w->mass[o] == 0);
    CHECK(b.deaths == d0 + 1u && b.last.kind == AZ_EV_DEATH && b.last.aux == AZ_DIE_KILLED);
    CHECK(exact(&b));
    (void)cell_before;
    /* the dead are not walked again */
    const u32 d1 = b.deaths;
    life_step(&b, o, o + 1u);
    CHECK_EQ(b.deaths, d1);

    /* the base hazard takes any stage, an egg and a cyst too */
    w->param[AZ_WP_LIFE_HAZARD] = 1u << 20;
    static const u8 stages[] = { AZ_F_EGG, AZ_F_JUV, 0, AZ_F_SENESC, AZ_F_DORMANT };
    for (u32 i = 0; i < sizeof stages; i++) remake(w, 20u + i, stages[i]);
    const u32 h0 = b.cause[AZ_DIE_HAZARD];
    books(&b);
    for (u32 k = 0; k < period; k++) life_step(&b, 20, 25);
    for (u32 i = 0; i < sizeof stages; i++) CHECK_EQ(w->flags[20u + i], 0);
    CHECK_EQ(b.cause[AZ_DIE_HAZARD], h0 + 5u);
    CHECK(exact(&b));
    printf("edges: a creature with no life-history gene, one with every trait and scale at its highest, a body of "
           "one part, and the base hazard at every stage: books exact throughout\n");
    bench_close(&b);
}

/* ---- 8. worlds ---- */
typedef struct Run { u64 hash, births, deaths, cap_denied, store_denied; u32 live; int ok; } Run;

static Run world_run(u32 threads, u8 reorder, u32 cap, u32 store_units)
{
    Run r;
    memset(&r, 0, sizeof r);
    AzConfig cfg = az_test_config(7, 2000);
    cfg.reorder_period = reorder;
    if (cap) cfg.cap = cap;
    cfg.store_units = store_units;
    AzTestWorld t;
    if (!az_test_world(&t, &cfg, threads)) { CHECK(!"world"); return r; }
    AzWorld *w = t.w;
    az_test_founders_bud(w);
    const u64 m0 = az_total_matter(w);
    const i64 e0 = az_total_energy(w);
    r.ok = 1;
    for (u32 i = 0; i < 120u; i++) {
        az_tick(w);
        if (az_total_matter(w) != m0 || az_total_energy(w) != e0) r.ok = 0;
    }
    r.hash = az_state_hash(w);
    r.births = w->totals.births; r.deaths = w->totals.deaths;
    r.cap_denied = w->totals.cap_denied; r.store_denied = w->totals.store_denied;
    r.live = w->n_live;
    az_test_world_free(&t);
    return r;
}

static void test_worlds(void)
{
    const Run a = world_run(0, 1, 40000, 0), b = world_run(1, 9, 40000, 0), c = world_run(3, 1, 40000, 0),
              d = world_run(8, 9, 40000, 0);
    CHECK(a.ok && b.ok && c.ok && d.ok);
    CHECK(a.births > 1000u && a.deaths > 0);
    CHECK_EQ(a.cap_denied, 0); CHECK_EQ(a.store_denied, 0);
    CHECK_EQ(a.hash, b.hash); CHECK_EQ(a.hash, c.hash); CHECK_EQ(a.hash, d.hash);
    CHECK(a.births == d.births && a.deaths == d.deaths && a.live == d.live);
    /* the slots bind */
    const Run e = world_run(0, 1, 2600, 0), f = world_run(3, 9, 2600, 0), g = world_run(8, 1, 2600, 0);
    CHECK(e.ok && f.ok && g.ok);
    CHECK(e.cap_denied > 0);
    CHECK_EQ(e.hash, f.hash); CHECK_EQ(e.hash, g.hash);
    CHECK_EQ(e.cap_denied, g.cap_denied);
    CHECK(e.hash != a.hash);
    /* the creature store binds */
    const Run h = world_run(0, 1, 40000, 150000), i = world_run(8, 9, 40000, 150000), j = world_run(3, 9, 40000, 150000);
    CHECK(h.ok && i.ok && j.ok);
    CHECK(h.store_denied > 0);
    CHECK_EQ(h.hash, i.hash); CHECK_EQ(h.hash, j.hash);
    CHECK_EQ(h.store_denied, i.store_denied);
    printf("worlds: 2000 founders, 120 ticks: %" PRIu64 " births, %" PRIu64 " deaths, %u alive, hash %016" PRIx64
           " on 0, 1, 3 and 8 workers; %" PRIu64 " births refused for want of a slot with 2600 slots and %" PRIu64
           " blocks refused with a store of 150000 units, the same on every thread count\n",
           a.births, a.deaths, a.live, a.hash, e.cap_denied, h.store_denied);

    /* the life table of one run, read from its ledger */
    AzConfig cfg = az_test_config(5, 1500);
    Bench t;
    if (!bench_open(&t, &cfg)) { CHECK(!"bench"); return; }
    AzWorld *w = t.w;
    az_test_founders_bud(w);
    books(&t);
    u32 inexact = 0;
    for (u32 k = 0; k < 300u; k++) { az_tick(w); inexact += !exact(&t); }
    CHECK_EQ(inexact, 0);
    CHECK_EQ(t.births, w->totals.births);
    CHECK_EQ(t.deaths, w->totals.deaths);
    CHECK(t.births > 0 && t.deaths > 0);
    CHECK(t.hatches <= t.eggs);
    u32 stage[5] = { 0, 0, 0, 0, 0 }, cysts = 0;
    for (u32 s = 0; s < w->n_slots; s++) {
        stage[stage_of(w, s)]++;
        cysts += (w->flags[s] & AZ_F_ALIVE) && (w->flags[s] & AZ_F_DORMANT);
    }
    printf("life table, 1500 founders, 300 ticks: %u births (%u as eggs, %u hatched), %u stage changes, %u deaths "
           "(by cause 0..8: %u %u %u %u %u %u %u %u %u; 3 is age, 8 the base hazard); alive at the end: %u eggs, "
           "%u young, %u adults, %u old (%u of them cysts)\n",
           t.births, t.eggs, t.hatches, t.stages, t.deaths, t.cause[0], t.cause[1], t.cause[2], t.cause[3],
           t.cause[4], t.cause[5], t.cause[6], t.cause[7], t.cause[8],
           stage[ST_EGG], stage[ST_JUV], stage[ST_ADULT], stage[ST_OLD], cysts);
    bench_close(&t);
}

int main(void)
{
    test_walk(0);
    test_walk(0xFFFFFFF0ull);
    test_broods();
    test_modes();
    test_dormancy();
    test_mortality();
    test_traits();
    test_edges();
    test_worlds();
    return az_test_done();
}
