/* test_wild.c - the two modes of a world (AzConfig.mode).
 *
 * Wild mode is the default of a living run: the world is made of fresh
 * entropy from the host and draws 16 more bytes at the start of every tick,
 * so no seed determines a run and no two runs are the same. Lab mode is the
 * seeded test bench. This test checks
 *
 *   1. the default: a zeroed configuration is wild; a wild world cannot be
 *      made without a host that gives entropy
 *   2. no two wild runs are the same (eight worlds from one configuration,
 *      real entropy from the operating system), and each keeps matter and
 *      energy exact at every tick
 *   3. entropy is the only thing that differs: one scripted entropy stream
 *      gives one world, whatever the thread count, the slot order and the
 *      store schedule; another stream gives another world. The journal gets
 *      one record per tick, in order, with the bytes the tick drew
 *   4. a host whose entropy fails in the middle of a run: the run goes on
 *      and stays exact, every tick without fresh entropy is counted, and the
 *      ledger holds one event for it
 *   5. a lab world never asks for entropy and never writes the journal
 *   6. a wild world can be saved and restored: the restored world equals the
 *      saved one, the same entropy gives both the same future, and fresh
 *      entropy gives the restored one a future of its own
 *
 * The scripted host of parts 3 to 6 is the test bench: it stands in for the
 * operating system so that the engine can be compared with itself. az_run
 * has no such switch; its wild runs use the operating system's generator.
 */
#include "az_test_locked_block.h"

/* ---- a host whose entropy is a script ---- */
typedef struct Wrap {
    AzHost host;            /* what the world sees */
    AzHost *inner;          /* a real pool, or NULL: the parts run here */
    AzWinInfo info;
    u64 script;             /* state of the scripted stream */
    u32 draws;              /* successful draws */
    u32 fail_after;         /* draws that succeed before the source runs dry */
    u32 logs;               /* journal records */
    u64 log_mix;            /* a hash of all of them */
    u64 log_next;           /* the tick the next record must carry */
    u32 log_bad;
    u32 no_entropy_events;  /* AZ_EV_REFUSED / AZ_REF_ENTROPY seen by the sink */
    void *block;
    AzArena arena;
    AzWorld *w;
} Wrap;

static u64 splitmix(u64 *x)
{
    *x += 0x9E3779B97F4A7C15ull;
    return az_mix64(*x);
}

static void wrap_pfor(void *ctx, AzWorld *w, AzJobFn fn, const u32 *splits, u32 n)
{
    Wrap *x = (Wrap *)ctx;
    if (x->inner) { x->inner->parallel_for(x->inner->ctx, w, fn, splits, n); return; }
    for (u32 k = 0; k < n; k++) fn(w, splits[k], splits[k + 1], 0);
}

static u64 wrap_now(void *ctx) { (void)ctx; return 0; }

static bool wrap_entropy(void *ctx, void *buf, u32 bytes)
{
    Wrap *x = (Wrap *)ctx;
    if (x->draws >= x->fail_after) return false;
    x->draws++;
    u8 *p = (u8 *)buf;
    for (u32 i = 0; i < bytes; i += 8) {
        const u64 v = splitmix(&x->script);
        memcpy(p + i, &v, bytes - i < 8 ? bytes - i : 8);
    }
    return true;
}

static void wrap_log(void *ctx, u64 tick, const u8 bytes[16])
{
    Wrap *x = (Wrap *)ctx;
    u64 a, b;
    memcpy(&a, bytes, 8); memcpy(&b, bytes + 8, 8);
    x->log_mix = az_mix64(x->log_mix ^ az_mix64(tick + 1) ^ a) + b;
    if (tick != x->log_next) x->log_bad++;
    x->log_next = tick + 1;
    x->logs++;
}

static void wrap_sink(void *ctx, const AzEvent *ev, u32 n)
{
    Wrap *x = (Wrap *)ctx;
    for (u32 i = 0; i < n; i++)
        if (ev[i].kind == AZ_EV_REFUSED && ev[i].aux == AZ_REF_ENTROPY) x->no_entropy_events++;
}

static void wrap_init(Wrap *x, u32 threads, u64 script)
{
    memset(x, 0, sizeof *x);
    x->script = script;
    x->fail_after = 0xFFFFFFFFu;
    if (threads) x->inner = az_win_host_create(threads, AZ_WIN_USE_ECORES, &x->info);
    x->host.ctx = x;
    x->host.n_workers = x->inner ? x->inner->n_workers : 1u;
    x->host.max_parts = x->inner ? x->inner->max_parts : 0u;
    x->host.parallel_for = wrap_pfor;
    x->host.now_ns = wrap_now;
    x->host.sink = wrap_sink;
    x->host.sink_ctx = x;
    x->host.entropy = wrap_entropy;
    x->host.entropy_log = wrap_log;
}

static bool wrap_arena(Wrap *x, const AzConfig *cfg)
{
    const u64 bytes = az_world_bytes(cfg);
    x->block = az_test_locked_block(bytes);
    if (!x->block) return false;
    x->arena.base = (u8 *)x->block; x->arena.size = bytes; x->arena.used = 0;
    return true;
}

static bool wrap_world(Wrap *x, const AzConfig *cfg)
{
    if (!wrap_arena(x, cfg)) return false;
    x->w = az_world_create(&x->arena, cfg, &x->host);
    if (!x->w) return false;
    az_worldgen(x->w);
    return true;
}

static void wrap_free(Wrap *x)
{
    if (x->inner) az_win_host_destroy(x->inner);
    if (x->block) az_test_locked_block_free(x->block);
    memset(x, 0, sizeof *x);
}

static AzConfig wild_config(u32 n)
{
    AzConfig c = az_test_config(1, n);
    c.mode = AZ_MODE_WILD;
    c.seed = 0;                 /* a wild world has no seed */
    return c;
}

/* ---- 1. the default, and what a wild world needs ---- */
static bool dry(void *ctx, void *buf, u32 bytes) { (void)ctx; (void)buf; (void)bytes; return false; }

static void test_needs_entropy(void)
{
    AzConfig z;
    memset(&z, 0, sizeof z);
    CHECK_EQ(z.mode, AZ_MODE_WILD);                       /* nobody has to ask for the wild */
    CHECK_EQ(az_test_config(1, 10).mode, AZ_MODE_LAB);    /* the tests ask for the bench */

    AzConfig cfg = wild_config(200);
    Wrap x;
    wrap_init(&x, 0, 5);
    CHECK(wrap_arena(&x, &cfg));
    CHECK(az_world_create(&x.arena, &cfg, NULL) == NULL);            /* no host */
    x.host.entropy = NULL;
    CHECK(az_world_create(&x.arena, &cfg, &x.host) == NULL);         /* a host with no source */
    x.host.entropy = dry;
    CHECK(az_world_create(&x.arena, &cfg, &x.host) == NULL);         /* a source that has nothing */
    CHECK_EQ(x.arena.used, 0);                                       /* and nothing was taken from the arena */
    x.host.entropy = wrap_entropy;
    AzWorld *w = az_world_create(&x.arena, &cfg, &x.host);
    CHECK(w != NULL);
    CHECK_EQ(x.draws, 1);                                            /* the world's origin: one draw of 16 bytes */
    if (w) CHECK_EQ(w->cfg.mode, AZ_MODE_WILD);
    /* the same configuration in lab mode needs no host at all */
    cfg.mode = AZ_MODE_LAB;
    x.arena.used = 0;
    CHECK(az_world_create(&x.arena, &cfg, NULL) != NULL);
    wrap_free(&x);
}

/* ---- 2. no two wild runs are the same ---- */
enum { WILD_RUNS = 8, WILD_TICKS = 40 };

static void test_never_the_same(void)
{
    u64 origin[WILD_RUNS], h0[WILD_RUNS], h1[WILD_RUNS], births[WILD_RUNS];
    AzConfig cfg = wild_config(1500);
    int exact = 1;
    for (u32 r = 0; r < WILD_RUNS; r++) {
        AzTestWorld t;
        if (!az_test_world(&t, &cfg, 1 + r % 4)) { CHECK(!"world"); return; }
        AzWorld *w = t.w;
        origin[r] = (u64)w->key[0] | (u64)w->key[1] << 32;
        h0[r] = az_state_hash(w);
        const u64 m0 = az_total_matter(w);
        const i64 e0 = az_total_energy(w);
        for (u32 k = 0; k < WILD_TICKS; k++) {
            az_tick(w);
            if (az_total_matter(w) != m0 || az_total_energy(w) != e0) exact = 0;
        }
        h1[r] = az_state_hash(w);
        births[r] = w->totals.births;
        CHECK_EQ(w->totals.entropy_missed, 0);            /* the operating system always had some */
        CHECK_EQ(w->totals.field_underflow, 0);
        az_test_world_free(&t);
    }
    CHECK(exact);
    u32 same = 0;
    for (u32 a = 0; a < WILD_RUNS; a++)
        for (u32 b = a + 1; b < WILD_RUNS; b++)
            same += origin[a] == origin[b] || h0[a] == h0[b] || h1[a] == h1[b];
    CHECK_EQ(same, 0);
    u64 lo = births[0], hi = births[0];
    for (u32 r = 1; r < WILD_RUNS; r++) { if (births[r] < lo) lo = births[r]; if (births[r] > hi) hi = births[r]; }
    printf("wild: %d worlds from one configuration, %d ticks each: all origins and all hashes differ (births %" PRIu64
           " to %" PRIu64 "); matter and energy exact at every tick\n", WILD_RUNS, WILD_TICKS, lo, hi);
}

/* ---- 3. entropy is the only thing that differs ---- */
enum { SCRIPT_TICKS = 60 };
typedef struct Trace { u64 h[SCRIPT_TICKS]; u64 log_mix, births, deaths; u32 logs, draws, log_bad; int ok; } Trace;

static Trace scripted(u64 script, u32 threads, u8 reorder, u16 store_period)
{
    Trace tr;
    memset(&tr, 0, sizeof tr);
    AzConfig cfg = wild_config(2500);
    cfg.reorder_period = reorder;
    cfg.store_period = store_period;
    Wrap x;
    wrap_init(&x, threads, script);
    if ((threads && !x.inner) || !wrap_world(&x, &cfg)) { CHECK(!"world"); wrap_free(&x); return tr; }
    AzWorld *w = x.w;
    const u64 m0 = az_total_matter(w);
    const i64 e0 = az_total_energy(w);
    tr.ok = 1;
    for (u32 k = 0; k < SCRIPT_TICKS; k++) {
        az_tick(w);
        tr.h[k] = az_state_hash(w);
        if (az_total_matter(w) != m0 || az_total_energy(w) != e0) tr.ok = 0;
    }
    tr.log_mix = x.log_mix; tr.logs = x.logs; tr.draws = x.draws; tr.log_bad = x.log_bad;
    tr.births = w->totals.births; tr.deaths = w->totals.deaths;
    if (w->totals.entropy_missed || x.no_entropy_events) tr.ok = 0;
    wrap_free(&x);
    return tr;
}

static void test_only_entropy_differs(void)
{
    const Trace a = scripted(0xA11CEull, 0, 1, 1);
    CHECK(a.ok);
    CHECK_EQ(a.draws, 1 + SCRIPT_TICKS);                  /* the origin, then 16 bytes for every tick */
    CHECK_EQ(a.logs, SCRIPT_TICKS);                       /* one journal record per tick */
    CHECK_EQ(a.log_bad, 0);                               /* in order, each with its own tick */
    CHECK(a.births > 50);
    static const u32 threads[4] = { 1, 8, 24, 5 };
    static const u8 reorder[4] = { 1, 7, 31, 255 };
    static const u16 store[4] = { 1, 3, 64, 1000 };
    for (u32 i = 0; i < 4; i++) {
        const Trace b = scripted(0xA11CEull, threads[i], reorder[i], store[i]);
        CHECK(b.ok);
        CHECK(memcmp(a.h, b.h, sizeof a.h) == 0);         /* tick by tick */
        CHECK_EQ(a.log_mix, b.log_mix);                   /* the same journal */
        CHECK_EQ(a.births, b.births);
        CHECK_EQ(a.deaths, b.deaths);
    }
    /* another stream, another world: from the first tick on, and never the same again */
    const Trace c = scripted(0xA11CFull, 8, 7, 3);
    CHECK(c.ok);
    u32 equal = 0;
    for (u32 k = 0; k < SCRIPT_TICKS; k++) equal += a.h[k] == c.h[k];
    CHECK_EQ(equal, 0);
    CHECK(a.log_mix != c.log_mix);
    printf("wild, scripted entropy: one stream gives hash %016" PRIx64 " after %d ticks serial and on 1, 8, 24 and 5 "
           "workers; another stream gives %016" PRIx64 "\n", a.h[SCRIPT_TICKS - 1], SCRIPT_TICKS, c.h[SCRIPT_TICKS - 1]);
}

/* ---- 4. the source runs dry ---- */
static void test_entropy_fails(void)
{
    AzConfig cfg = wild_config(1500);
    Wrap x;
    wrap_init(&x, 4, 77);
    x.fail_after = 1 + 10;                                /* the origin and ten ticks */
    if (!x.inner || !wrap_world(&x, &cfg)) { CHECK(!"world"); wrap_free(&x); return; }
    AzWorld *w = x.w;
    const u64 m0 = az_total_matter(w);
    const i64 e0 = az_total_energy(w);
    int exact = 1;
    u32 stuck = 0, key_stuck = 0;
    u64 last = az_state_hash(w), last_wild = 0;
    for (u32 k = 0; k < 30; k++) {
        az_tick(w);
        if (az_total_matter(w) != m0 || az_total_energy(w) != e0) exact = 0;
        const u64 h = az_state_hash(w);
        stuck += h == last; last = h;
        key_stuck += k > 0 && w->wild[0] == last_wild;    /* the per-tick words keep moving without the source */
        last_wild = w->wild[0];
    }
    CHECK(exact);
    CHECK_EQ(stuck, 0);
    CHECK_EQ(key_stuck, 0);
    CHECK_EQ(x.draws, 11);
    CHECK_EQ(w->totals.entropy_missed, 20);               /* every tick without fresh entropy is counted */
    CHECK_EQ(x.logs, 30);                                 /* the journal records those ticks too */
    az_ledger_flush(w);
    CHECK_EQ(x.no_entropy_events, 1);                     /* one ledger event, at the first of them */
    /* the source comes back: the count stops */
    x.fail_after = 0xFFFFFFFFu;
    for (u32 k = 0; k < 5; k++) az_tick(w);
    CHECK_EQ(w->totals.entropy_missed, 20);
    CHECK_EQ(x.no_entropy_events, 1);
    printf("wild, source dry after 10 ticks: 20 of 30 ticks counted as without fresh entropy, 1 ledger event, the run "
           "went on, matter and energy exact\n");
    wrap_free(&x);
}

/* ---- 5. a lab world never asks ---- */
static void test_lab_never_asks(void)
{
    AzConfig cfg = az_test_config(17, 1500);
    Wrap x;
    wrap_init(&x, 4, 99);
    AzTestWorld plain;
    if (!x.inner || !wrap_world(&x, &cfg) || !az_test_world(&plain, &cfg, 0)) { CHECK(!"world"); wrap_free(&x); return; }
    u32 differ = 0;
    for (u32 k = 0; k < 30; k++) {
        az_tick(x.w); az_tick(plain.w);
        differ += az_state_hash(x.w) != az_state_hash(plain.w);
    }
    CHECK_EQ(differ, 0);                                  /* the seed alone decides a lab world */
    CHECK_EQ(x.draws, 0);
    CHECK_EQ(x.logs, 0);
    CHECK_EQ(x.w->totals.entropy_missed, 0);
    CHECK_EQ(x.w->wild[0] | x.w->wild[1], 0);
    printf("lab: 30 ticks on a host with an entropy source: 0 draws, 0 journal records, hash %016" PRIx64
           " equal to the world made without a host\n", az_state_hash(x.w));
    az_test_world_free(&plain);
    wrap_free(&x);
}

/* ---- 6. save and restore a wild world ---- */
static void test_snapshot(void)
{
    AzConfig cfg = wild_config(1500);
    Wrap a, b, c;
    wrap_init(&a, 3, 4242);
    if (!a.inner || !wrap_world(&a, &cfg)) { CHECK(!"world"); wrap_free(&a); return; }
    for (u32 k = 0; k < 25; k++) az_tick(a.w);
    const u64 nb = az_snapshot_bytes(a.w);
    u8 *snap = (u8 *)malloc((size_t)nb + 8);
    CHECK(snap && nb && az_snapshot_save(a.w, snap, nb) == nb);
    if (!snap || !nb) { wrap_free(&a); free(snap); return; }
    const u64 h_saved = az_state_hash(a.w);

    /* a wild snapshot is a wild world: it cannot be loaded without a source of entropy */
    wrap_init(&b, 8, 1);
    wrap_init(&c, 0, 2);
    CHECK(b.inner && wrap_arena(&b, &cfg) && wrap_arena(&c, &cfg));
    CHECK(az_snapshot_load(&b.arena, snap, nb, NULL) == NULL);
    CHECK_EQ(b.arena.used, 0);
    b.w = az_snapshot_load(&b.arena, snap, nb, &b.host);
    c.w = az_snapshot_load(&c.arena, snap, nb, &c.host);
    CHECK(b.w != NULL && c.w != NULL);
    if (b.w && c.w) {
        CHECK_EQ(b.w->cfg.mode, AZ_MODE_WILD);
        CHECK_EQ(az_state_hash(b.w), h_saved);            /* the restored world is the saved world */
        CHECK_EQ(az_state_hash(c.w), h_saved);
        CHECK_EQ(b.w->tick, a.w->tick);
        CHECK(b.w->key[0] == a.w->key[0] && b.w->key[1] == a.w->key[1]);   /* its origin came with it */
        /* b gets the entropy a gets from here on: the same future, on another thread count */
        b.script = a.script;
        u32 differ_b = 0, same_c = 0;
        const u64 m0 = az_total_matter(a.w);
        const i64 e0 = az_total_energy(a.w);
        int exact = 1;
        for (u32 k = 0; k < 25; k++) {
            az_tick(a.w); az_tick(b.w); az_tick(c.w);
            differ_b += az_state_hash(a.w) != az_state_hash(b.w);
            same_c += az_state_hash(a.w) == az_state_hash(c.w);            /* c draws its own */
            if (az_total_matter(c.w) != m0 || az_total_energy(c.w) != e0) exact = 0;
            if (az_total_matter(b.w) != m0 || az_total_energy(b.w) != e0) exact = 0;
        }
        CHECK_EQ(differ_b, 0);
        CHECK_EQ(same_c, 0);
        CHECK(exact);
        printf("wild snapshot: restored hash %016" PRIx64 " equals the saved one; with the same entropy 25 more ticks stay "
               "equal on 8 workers, with entropy of its own the restored world differs from the first tick on\n", h_saved);
    }
    free(snap);
    wrap_free(&a); wrap_free(&b); wrap_free(&c);
}

int main(void)
{
    test_needs_entropy();
    test_never_the_same();
    test_only_entropy_differs();
    test_entropy_fails();
    test_lab_never_asks();
    test_snapshot();
    return az_test_done();
}
