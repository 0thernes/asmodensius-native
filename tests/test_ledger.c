/* test_ledger.c - the cold ledger: canonical order, hash chain, SPSC sink,
 * file round trip.
 *
 *   test_ledger.exe                 run everything
 *   test_ledger.exe --keep <path>   also leave one ledger file at <path> and
 *                                   print its chain (input for az_ledgerdump)
 *   --keep-resumed <path>           also leave the ledger of a world resumed
 *                                   from a snapshot (finding s2) at <path>
 *   --keep-old <path>               also leave the same records under a header
 *                                   of the old format version 2 at <path> (a
 *                                   file az_ledgerdump must refuse)
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#include "az_test_locked_block.h"
#include "../platform/win/az_sink_win.h"
#include "../tools/az_tools.h"

/* ---------------------------------------------------------------------- */
/* helpers                                                                 */
/* ---------------------------------------------------------------------- */
static u64 rng_state = 0x1234567ull;
static u32 rnd(void) { return (u32)(az_test_rand64(&rng_state) >> 32); }

/* the documented canonical order: (kind, a, b, aux, v signed), then tick */
static int ev_cmp(const void *px, const void *py)
{
    const AzEvent *x = (const AzEvent *)px, *y = (const AzEvent *)py;
    if (x->kind != y->kind) return x->kind < y->kind ? -1 : 1;
    if (x->a != y->a) return x->a < y->a ? -1 : 1;
    if (x->b != y->b) return x->b < y->b ? -1 : 1;
    if (x->aux != y->aux) return x->aux < y->aux ? -1 : 1;
    if (x->v != y->v) return x->v < y->v ? -1 : 1;
    if (x->tick != y->tick) return x->tick < y->tick ? -1 : 1;
    return 0;
}

static u64 fold_all(u64 c, const AzEvent *e, u64 n)
{
    for (u64 i = 0; i < n; i++) c = az_ledger_fold(c, &e[i]);
    return c;
}

/* Temporary ledger files go in a folder of their own under the working
 * directory (the build folder on Z:), never in %TEMP% (which is on C:).
 * main creates the folder first and removes it when the test is done. */
static char temp_dir[64];

static bool temp_dir_create(void)
{
    snprintf(temp_dir, sizeof temp_dir, "az_ledger_tmp_%d", (int)_getpid());
    return CreateDirectoryA(temp_dir, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

static void temp_path(char *out, size_t cap, const char *tag)
{
    snprintf(out, cap, "%s\\az_ledger_test_%d_%s.bin", temp_dir, (int)_getpid(), tag);
}

/* read a whole ledger file; returns false if it cannot be read */
static bool read_ledger(const char *path, AzLedgerHeader *hdr, AzEvent **ev, u64 *n)
{
    *ev = NULL; *n = 0;
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    bool ok = fread(hdr, sizeof(*hdr), 1, f) == 1;
    if (ok) {
        _fseeki64(f, 0, SEEK_END);
        long long size = _ftelli64(f);
        _fseeki64(f, (long long)sizeof(*hdr), SEEK_SET);
        u64 body = (u64)size - sizeof(*hdr);
        ok = body % sizeof(AzEvent) == 0;
        *n = body / sizeof(AzEvent);
        *ev = (AzEvent *)malloc((size_t)(*n ? *n : 1) * sizeof(AzEvent));
        ok = ok && *ev && fread(*ev, sizeof(AzEvent), (size_t)*n, f) == (size_t)*n;
    }
    fclose(f);
    return ok;
}

/* ---------------------------------------------------------------------- */
/* 1. canonical order and chain, events emitted by hand                    */
/* ---------------------------------------------------------------------- */
typedef struct Capture { AzEvent *ev; u32 n, cap, calls; } Capture;

static void capture_sink(void *ctx, const AzEvent *ev, u32 n)
{
    Capture *c = (Capture *)ctx;
    c->calls++;
    if (c->n + n > c->cap) { n = c->cap - c->n; }
    memcpy(c->ev + c->n, ev, (size_t)n * sizeof(AzEvent));
    c->n += n;
}
static u64 zero_ns(void *ctx) { (void)ctx; return 0; }

static AzEvent random_event(u32 tick, u32 spread)
{
    AzEvent e;
    e.tick = tick;
    e.kind = (u16)(1 + rnd() % 11);
    e.pad = 0;
    /* spread 0: tiny ranges, so most fields tie and the later keys decide;
     * spread 1: full 64-bit values, so both halves of a, b and v take part */
    e.a = spread ? (((u64)rnd() << 32) | rnd()) : rnd() % 3;
    e.b = spread ? (((u64)(rnd() % 3) << 32) | rnd()) : rnd() % 3;
    e.aux = spread ? rnd() : rnd() % 2;
    e.v = spread ? (i64)(((u64)rnd() << 32) | rnd()) : (i64)(rnd() % 5) - 2;
    return e;
}

static void test_order_and_chain(void)
{
    AzConfig cfg = az_test_config(7, 16);
    cfg.cap = 64;                       /* tmp = 8192 bytes = 409 records */
    Capture cap;
    memset(&cap, 0, sizeof cap);
    cap.cap = 4096;
    cap.ev = (AzEvent *)malloc(cap.cap * sizeof(AzEvent));
    AzHost host;
    memset(&host, 0, sizeof host);
    /* 64 workers and no parallel_for: every worker buffer exists, the core
     * still runs serially, and the test decides which buffer gets what */
    host.ctx = &cap; host.n_workers = AZ_MAX_WORKERS; host.now_ns = zero_ns; host.sink = capture_sink; host.sink_ctx = &cap;

    u64 bytes = az_world_bytes(&cfg);
    void *block = az_test_locked_block(bytes);
    AzArena arena = { (u8 *)block, bytes, 0 };
    AzWorld *w = az_world_create(&arena, &cfg, &host);
    CHECK(w != NULL);
    if (!w) return;
    az_worldgen(w);
    CHECK_EQ(w->cap, 64);
    az_ledger_flush(w);                 /* whatever worldgen emitted */

    const u32 room = (w->cap * 128u) / (u32)sizeof(AzEvent);   /* 204 */
    u32 alive_now = 0;                  /* nothing is born or dies in this test */
    for (u32 s = 0; s < w->n_slots; s++) alive_now += (w->flags[s] & AZ_F_ALIVE) ? 1u : 0u;
    CHECK_EQ(alive_now, 16);
    static const u32 sizes[] = { 0, 1, 2, 17, 75, 101, 102, 150, 203 };
    static const u32 nworkers[] = { 1, 3, 8, 64 };
    AzEvent *ref = (AzEvent *)malloc(2048 * sizeof(AzEvent));
    u32 order_matters = 0;

    for (u32 trial = 0; trial < 216; trial++) {
        u32 n = sizes[trial % AZ_LEN(sizes)];
        u32 spread = (trial / AZ_LEN(sizes)) & 1u;
        w->tick = 100 + trial;
        for (u32 i = 0; i < n; i++) ref[i] = random_event(w->tick, spread);
        if (n >= 4 && !spread) {         /* signed order of v, all else equal */
            for (u32 i = 0; i < 4; i++) { ref[i] = ref[0]; }
            ref[0].v = 3; ref[1].v = INT32_MIN; ref[2].v = -5; ref[3].v = INT32_MAX;
        }
        u64 raw_chain = fold_all(0, ref, n);

        u64 chain_by_workers[AZ_LEN(nworkers)];
        for (u32 d = 0; d < AZ_LEN(nworkers); d++) {
            /* same multiset, different distribution over worker buffers */
            for (u32 i = 0; i < n; i++) {
                const AzEvent *e = &ref[i];
                az_emit(w, rnd() % nworkers[d], e->kind, e->aux, e->a, e->b, e->v);
            }
            w->ledger_chain = 0;
            u64 count0 = w->ledger_count;
            cap.n = 0; cap.calls = 0;
            az_ledger_flush(w);
            chain_by_workers[d] = w->ledger_chain;

            CHECK_EQ(cap.calls, 1);
            CHECK_EQ(cap.n, n + 1);
            CHECK_EQ(w->ledger_count - count0, n + 1);
            CHECK_EQ(w->ledger_chain, fold_all(0, cap.ev, cap.n));
            for (u32 k = 0; k < AZ_MAX_WORKERS; k++) CHECK_EQ(w->workers[k].n_events, 0);

            const AzEvent *s = &cap.ev[n];          /* the summary */
            CHECK_EQ(s->kind, AZ_EV_TICK);
            CHECK_EQ(s->tick, w->tick);
            CHECK_EQ(s->a, n);
            CHECK_EQ(s->b, alive_now);              /* alive at the end of the tick (v11), not n_live */
            CHECK_EQ(s->aux, 0);
            CHECK_EQ((u64)s->v, fold_all(0, cap.ev, n));
        }
        /* the reference order, by libc qsort with the documented comparator */
        qsort(ref, n, sizeof(AzEvent), ev_cmp);
        CHECK(memcmp(ref, cap.ev, (size_t)n * sizeof(AzEvent)) == 0);
        for (u32 d = 1; d < AZ_LEN(nworkers); d++) CHECK_EQ(chain_by_workers[d], chain_by_workers[0]);
        /* the chain is order-sensitive: folding the unsorted events gives
         * another value (so sorting is what makes the chains above agree) */
        if (n >= 17 && raw_chain != fold_all(0, ref, n)) order_matters++;
    }
    CHECK(order_matters >= 100);

    /* v is ordered as a signed number, and each earlier key outranks the later ones */
    {
#define EV(kind_, aux_, a_, b_, v_) { .tick = 0, .a = (a_), .b = (b_), .v = (v_), .aux = (aux_), .kind = (kind_), .pad = 0 }
        static const AzEvent in[] = {
            EV(AZ_EV_MATE,  9, 5, 5, 3),
            EV(AZ_EV_MATE,  9, 5, 5, INT64_MAX),
            EV(AZ_EV_MATE,  9, 5, 5, INT64_MIN),
            EV(AZ_EV_MATE,  9, 5, 5, -5),
            EV(AZ_EV_MATE,  8, 5, 5, 100),           /* aux below 9          */
            EV(AZ_EV_MATE, 10, 5, 4, 100),           /* b below 5 beats aux  */
            EV(AZ_EV_MATE, 10, 4, 9, 100),           /* a below 5 beats b    */
            EV(AZ_EV_DEATH, 10, 0xFFFFFFFFFFFFFFFFull, 9, 100),   /* kind beats a */
            EV(AZ_EV_MATE,  9, 0x100000005ull, 5, 0),             /* the high half of a counts */
            EV(AZ_EV_MATE,  9, 5, 0x100000000ull, 0),             /* the high half of b counts */
            EV(AZ_EV_MATE,  9, 5, 5, 0x100000000ll),              /* the high half of v counts */
        };
#undef EV
        /* death first; then the mates by (a, b, aux, v): a = 4; a = 5 with
         * b = 4; a = 5, b = 5 with aux 8, then aux 9 by v ascending (the
         * minimum, -5, 3, 2^32, the maximum); b = 2^32; a = 2^32 + 5 */
        static const u8 want[] = { 7, 6, 5, 4, 2, 3, 0, 10, 1, 9, 8 };
        w->tick = 8000;
        for (u32 i = 0; i < AZ_LEN(in); i++) az_emit(w, AZ_LEN(in) - 1 - i, in[i].kind, in[i].aux, in[i].a, in[i].b, in[i].v);
        cap.n = 0;
        az_ledger_flush(w);
        CHECK_EQ(cap.n, AZ_LEN(in) + 1);
        for (u32 i = 0; i < AZ_LEN(in); i++) {
            AzEvent e = in[want[i]];
            e.tick = w->tick;
            CHECK(memcmp(&e, &cap.ev[i], sizeof e) == 0);
        }
    }
    CHECK_EQ(w->totals.dropped_events, 0);

    /* regime 3: more events than w->tmp can hold are counted, not lost silently */
    {
        u32 n = 1000, keep = room - 1;
        w->tick = 9000;
        for (u32 i = 0; i < n; i++) {
            AzEvent e = random_event(w->tick, 1);
            az_emit(w, i % 5, e.kind, e.aux, e.a, e.b, e.v);
        }
        cap.n = 0;
        w->ledger_chain = 0;
        az_ledger_flush(w);
        CHECK_EQ(cap.n, room);
        CHECK_EQ(w->totals.dropped_events, n - keep);
        CHECK_EQ(cap.ev[keep].kind, AZ_EV_TICK);
        CHECK_EQ(cap.ev[keep].a, keep);
        CHECK_EQ(cap.ev[keep].aux, n - keep);
        u32 unsorted = 0;
        for (u32 i = 1; i < keep; i++) if (ev_cmp(&cap.ev[i - 1], &cap.ev[i]) > 0) unsorted++;
        CHECK_EQ(unsorted, 0);
        for (u32 k = 0; k < AZ_MAX_WORKERS; k++) CHECK_EQ(w->workers[k].n_events, 0);
    }

    /* a worker buffer overflow (az_emit) shows up in the next summary */
    {
        w->totals.dropped_events = 0;
        w->workers[2].dropped_events = 70000;
        cap.n = 0;
        az_ledger_flush(w);
        CHECK_EQ(cap.n, 1);
        CHECK_EQ(cap.ev[0].aux, 70000);
        w->workers[2].dropped_events = 0;
        w->totals.dropped_events = 0x100000005ull;   /* more than the flag holds: it saturates */
        cap.n = 0;
        az_ledger_flush(w);
        CHECK_EQ(cap.n, 1);
        CHECK_EQ(cap.ev[0].aux, 0xFFFFFFFFu);
        w->totals.dropped_events = 0;
    }

    /* no host and no sink: the chain and the count still advance */
    {
        w->host = NULL;
        u64 c0 = w->ledger_chain, n0 = w->ledger_count;
        az_emit(w, 0, AZ_EV_STORM, 1, 2, 3, -4);
        az_ledger_flush(w);
        CHECK(w->ledger_chain != c0);
        CHECK_EQ(w->ledger_count, n0 + 2);
    }
    free(ref);
    free(cap.ev);
    az_test_locked_block_free(block);
}

/* ---------------------------------------------------------------------- */
/* 2. whole worlds: thread counts, reorder periods, seeds, file round trip */
/* ---------------------------------------------------------------------- */
typedef struct RunOut { u64 chain, count, hash, dropped; u64 kinds[16]; bool ok; } RunOut;

/* threads = 0 and path = NULL: host == NULL, the purely serial core.
 * path != NULL: the file sink wraps the pool (or stands alone). */
static RunOut run_world(u64 seed, u32 n, u32 ticks, u32 threads, u8 reorder, u32 split_seed,
                        const char *path)
{
    RunOut r;
    memset(&r, 0, sizeof r);
    AzConfig cfg = az_test_config(seed, n);
    cfg.reorder_period = reorder;
    cfg.debug_split_seed = split_seed;  /* non-zero: every work split is randomised */
    AzWinInfo info;
    AzHost *pool = NULL;
    if (threads) {
        /* not pinned: with every agent's tests pinned to the same P cores a
         * 3-thread run of 40 ticks took 67 s here; the thread count is what
         * this test varies, not the placement */
        pool = az_win_host_create(threads, AZ_WIN_USE_ECORES | AZ_WIN_NO_PIN, &info);
        CHECK(pool != NULL);
        if (!pool) return r;
        CHECK_EQ(pool->n_workers, threads);
    }
    AzSink *sink = NULL;
    AzHost *host = pool;
    if (path) {
        sink = az_sink_open(path, seed, 1u << 16);
        CHECK(sink != NULL);
        if (!sink) return r;
        host = az_sink_host(sink, pool);
    }
    u64 bytes = az_world_bytes(&cfg);
    void *block = az_test_locked_block(bytes);
    AzArena arena = { (u8 *)block, bytes, 0 };
    AzWorld *w = az_world_create(&arena, &cfg, host);
    CHECK(w != NULL);
    if (w) {
        az_worldgen(w);
        CHECK_EQ(w->n_workers, threads ? threads : 1);
        u64 m0 = az_total_matter(w);
        i64 e0 = az_total_energy(w);
        for (u32 t = 0; t < ticks; t++) {
            /* The stub world has no deaths this early, and deaths are the
             * events that come from the parallel workers. Cull a few
             * entities per tick, chosen by uid and tick only (so the choice
             * is the same for every thread count and slot order). */
            for (u32 sl = 0; sl < w->n_slots; sl++) {
                if (!(w->flags[sl] & AZ_F_ALIVE)) continue;
                if (az_mix64(((u64)w->uid[sl] << 32) | t) % 61u == 0) w->health[sl] = 0;
            }
            az_tick(w);
            CHECK_EQ(az_total_matter(w), m0);
            CHECK_EQ(az_total_energy(w), e0);
            /* the sink must never make the simulation wait: let the writer
             * drain between ticks so this small ring cannot fill */
            if (sink) while (az_sink_pending(sink) > (1u << 15)) SwitchToThread();
        }
        CHECK_EQ(w->totals.field_underflow, 0);
        r.chain = w->ledger_chain;
        r.count = w->ledger_count;
        r.hash = az_state_hash(w);
        r.dropped = w->totals.dropped_events;
        r.ok = true;
    }
    if (sink) {
        AzSinkStats st;
        bool closed = az_sink_close(sink, &st);
        CHECK(closed);
        CHECK_EQ(st.dropped, 0);
        CHECK_EQ(st.written, r.count);
        CHECK_EQ(st.chain, r.chain);
    }
    if (pool) az_win_host_destroy(pool);
    az_test_locked_block_free(block);
    return r;
}

/* structure of a ledger written by real ticks: blocks sorted canonically,
 * each closed by a summary that counts the block and checkpoints the chain */
static void check_file(const char *path, u64 seed, const RunOut *r, u32 ticks, u64 *kinds)
{
    AzLedgerHeader h;
    AzEvent *ev;
    u64 n;
    bool ok = read_ledger(path, &h, &ev, &n);
    CHECK(ok);
    if (!ok) { free(ev); return; }
    CHECK_EQ(h.magic, AZ_LEDGER_MAGIC);
    CHECK_EQ(h.version, AZ_LEDGER_VERSION);
    CHECK_EQ(h.abi, AZ_ABI_VERSION);
    CHECK_EQ(h.seed, seed);
    CHECK_EQ(h.header_bytes, sizeof(AzLedgerHeader));   /* 128 */
    CHECK_EQ(h.record_bytes, 40);                       /* format version 3 */
    CHECK_EQ(h.start_chain, 0);                         /* a fresh world starts the chain at 0 */
    CHECK_EQ(h.start_count, 0);
    CHECK_EQ(h.closed, 1);
    CHECK_EQ(h.write_errors, 0);
    CHECK_EQ(h.n_dropped, 0);
    CHECK_EQ(h.n_records, n);
    CHECK_EQ(n, r->count);
    CHECK_EQ(h.chain, r->chain);
    CHECK_EQ(fold_all(0, ev, n), r->chain);

    u64 c = 0, start = 0, unsorted = 0, bad_tick = 0, bad_summary = 0, blocks = 0;
    for (u64 i = 0; i < n; i++) {
        const AzEvent *e = &ev[i];
        if (e->kind < 16) kinds[e->kind]++;
        if (e->tick != blocks) bad_tick++;
        if (e->kind == AZ_EV_TICK) {
            if (e->a != i - start || (u64)e->v != c || e->aux != 0) bad_summary++;
            start = i + 1;
            blocks++;
        } else if (i > start && ev_cmp(&ev[i - 1], e) > 0) {
            unsorted++;
        }
        c = az_ledger_fold(c, e);
    }
    CHECK_EQ(unsorted, 0);
    CHECK_EQ(bad_tick, 0);
    CHECK_EQ(bad_summary, 0);
    CHECK_EQ(blocks, ticks);
    CHECK_EQ(start, n);                 /* the file ends with a summary */
    free(ev);
}

static void test_worlds(const char *keep_path)
{
    const u64 seed = 41, other_seed = 42;   /* tuning seeds (5001+ are held out) */
    const u32 n = 6000, ticks = 40;
    static const u32 threads[] = { 0, 3, 8 };
    static const u8 reorders[] = { 1, 9 };

    RunOut base = run_world(seed, n, ticks, 0, 1, 0, NULL);
    CHECK(base.ok);
    CHECK_EQ(base.dropped, 0);          /* precondition of every equality below */
    CHECK(base.count > ticks);          /* real events, not only summaries      */
    printf("seed %llu: chain %016llx count %llu hash %016llx\n", (unsigned long long)seed,
           (unsigned long long)base.chain, (unsigned long long)base.count, (unsigned long long)base.hash);

    for (u32 i = 0; i < AZ_LEN(threads); i++) {
        for (u32 j = 0; j < AZ_LEN(reorders); j++) {
            /* plain splits, then randomised splits (which events land in
             * which worker buffer changes; the ledger must not) */
            for (u32 split = 0; split < 2; split++) {
                ULONGLONG ms = GetTickCount64();
                RunOut r = run_world(seed, n, ticks, threads[i], reorders[j], split ? 77u + i : 0, NULL);
                ms = GetTickCount64() - ms;
                CHECK(r.ok);
                CHECK_EQ(r.chain, base.chain);
                CHECK_EQ(r.count, base.count);
                CHECK_EQ(r.hash, base.hash);
                CHECK_EQ(r.dropped, 0);
                printf("  threads %u reorder %u split %u: chain %016llx count %llu  (%llu ms)\n", threads[i],
                       reorders[j], split, (unsigned long long)r.chain, (unsigned long long)r.count,
                       (unsigned long long)ms);
            }
        }
    }

    RunOut other = run_world(other_seed, n, ticks, 0, 1, 0, NULL);
    CHECK(other.ok);
    CHECK(other.chain != base.chain);
    printf("seed %llu: chain %016llx count %llu\n", (unsigned long long)other_seed,
           (unsigned long long)other.chain, (unsigned long long)other.count);

    /* file round trip, with the sink around an 8-thread pool and alone */
    char path[512];
    for (u32 pass = 0; pass < 2; pass++) {
        u32 th = pass ? 0 : 8;
        u8 reorder = pass ? 9 : 1;
        if (pass == 0 && keep_path) snprintf(path, sizeof path, "%s", keep_path);
        else temp_path(path, sizeof path, pass ? "serial" : "pool");
        RunOut r = run_world(seed, n, ticks, th, reorder, 0, path);
        CHECK(r.ok);
        CHECK_EQ(r.chain, base.chain);
        CHECK_EQ(r.count, base.count);
        CHECK_EQ(r.hash, base.hash);
        u64 kinds[16];
        memset(kinds, 0, sizeof kinds);
        check_file(path, seed, &r, ticks, kinds);
        CHECK_EQ(kinds[AZ_EV_TICK], ticks);
        CHECK(kinds[AZ_EV_DEATH] > 1000);   /* events from the parallel stage */
        CHECK(kinds[AZ_EV_BIRTH] > 0);
        if (pass == 0) {
            printf("  file: births %llu deaths %llu summaries %llu\n", (unsigned long long)kinds[AZ_EV_BIRTH],
                   (unsigned long long)kinds[AZ_EV_DEATH], (unsigned long long)kinds[AZ_EV_TICK]);
            if (keep_path)
                printf("kept %s  chain 0x%016llx  records %llu\n", path, (unsigned long long)r.chain,
                       (unsigned long long)r.count);
        }
        if (!(pass == 0 && keep_path)) remove(path);
    }
}

/* ---------------------------------------------------------------------- */
/* 2b. resumed worlds (finding s2) and the old file format                 */
/* ---------------------------------------------------------------------- */

/* a ledger file through the shared reader: header decode + structure check
 * from the header's start chain, i.e. exactly what az_ledgerdump runs */
typedef struct FileCheck { AzLedgerHeader h; int body; AzLedgerCheck ck; u64 chain_from_zero, bad_from_zero; bool ok; } FileCheck;

static FileCheck check_ledger_file(const char *path)
{
    FileCheck r;
    memset(&r, 0, sizeof r);
    FILE *f = fopen(path, "rb");
    if (!f) return r;
    u8 raw[sizeof(AzLedgerHeader)];
    size_t got = fread(raw, 1, sizeof raw, f);
    r.body = az_ledger_header_decode(raw, got, &r.h);
    if (r.body > 0 && _fseeki64(f, r.body, SEEK_SET) == 0) {
        AzLedgerCheck zero;
        az_ledger_check_begin(&r.ck, r.h.start_chain);
        az_ledger_check_begin(&zero, 0);           /* what the old tool did */
        AzEvent buf[4096];
        size_t n;
        while ((n = fread(buf, sizeof(AzEvent), AZ_LEN(buf), f)) > 0) {
            az_ledger_check_step(&r.ck, buf, n);
            az_ledger_check_step(&zero, buf, n);
        }
        r.chain_from_zero = zero.chain;
        r.bad_from_zero = zero.bad_summary;
        r.ok = true;
    }
    fclose(f);
    return r;
}

typedef struct ResumeOut {
    u64 snap_chain, snap_count;     /* the world when it was saved (tick 5)        */
    u64 ref_chain, ref_count;       /* the same world run on without a break        */
    u64 chain, count;               /* the resumed world after 5 more ticks         */
    bool set_ok, late_set_refused;
    AzSinkStats st;
    bool closed, ok;
} ResumeOut;

/* deaths so the blocks hold real events: by uid and tick only, as in run_world */
static void cull(AzWorld *w, u32 t)
{
    for (u32 sl = 0; sl < w->n_slots; sl++)
        if ((w->flags[sl] & AZ_F_ALIVE) && az_mix64(((u64)w->uid[sl] << 32) | t) % 61u == 0) w->health[sl] = 0;
}

/* A world with no host runs 5 ticks and is saved; the snapshot is loaded
 * under a sink host (with or without az_sink_set_start) and runs 5 more. */
static ResumeOut resumed_run(u64 seed, u32 n, bool set_start, const char *path)
{
    ResumeOut o;
    memset(&o, 0, sizeof o);
    AzConfig cfg = az_test_config(seed, n);
    const u64 bytes = az_world_bytes(&cfg);
    AzTestWorld ta;                     /* the saved world: serial, with worldgen */
    const bool made = az_test_world_ex(&ta, &cfg, 0, 0);
    void *block_b = az_test_locked_block(bytes);
    AzArena arena_b = { (u8 *)block_b, bytes, 0 };
    AzWorld *a = (made && block_b) ? ta.w : NULL;
    CHECK(a != NULL);
    u8 *snap = NULL;
    AzSink *sink = NULL;
    if (!a) goto out;
    for (u32 t = 0; t < 5; t++) { cull(a, t); az_tick(a); }
    const u64 snap_n = az_snapshot_bytes(a);
    snap = (u8 *)malloc((size_t)(snap_n ? snap_n : 1));
    CHECK(snap_n > 0 && snap != NULL);
    if (!snap_n || !snap) goto out;
    CHECK_EQ(az_snapshot_save(a, snap, snap_n), snap_n);
    o.snap_chain = a->ledger_chain;
    o.snap_count = a->ledger_count;
    for (u32 t = 5; t < 10; t++) { cull(a, t); az_tick(a); }
    o.ref_chain = a->ledger_chain;
    o.ref_count = a->ledger_count;

    sink = az_sink_open(path, seed, 1u << 16);
    CHECK(sink != NULL);
    if (!sink) goto out;
    AzHost *host = az_sink_host(sink, NULL);
    AzWorld *b = az_snapshot_load(&arena_b, snap, snap_n, host);
    CHECK(b != NULL);
    if (!b) goto out;
    CHECK_EQ(b->ledger_chain, o.snap_chain);
    CHECK_EQ(b->ledger_count, o.snap_count);
    if (set_start) o.set_ok = az_sink_set_start(sink, b->ledger_chain, b->ledger_count);
    for (u32 t = 5; t < 10; t++) { cull(b, t); az_tick(b); }
    o.chain = b->ledger_chain;
    o.count = b->ledger_count;
    o.late_set_refused = !az_sink_set_start(sink, b->ledger_chain, b->ledger_count);
    o.ok = true;
out:
    if (sink) o.closed = az_sink_close(sink, &o.st);
    free(snap);
    az_test_world_free(&ta);
    az_test_locked_block_free(block_b);
    return o;
}

static void test_resumed(const char *keep_path)
{
    const u64 seed = 23;
    const u32 n = 3000;
    char path[512];

    /* with the start set: the file checks clean and matches the world */
    if (keep_path) snprintf(path, sizeof path, "%s", keep_path);
    else temp_path(path, sizeof path, "resumed");
    ResumeOut r = resumed_run(seed, n, true, path);
    CHECK(r.ok);
    CHECK(r.set_ok);
    CHECK(r.late_set_refused);                 /* after the first push it is too late */
    CHECK(r.closed);
    CHECK(r.snap_chain != 0);                  /* otherwise this test proves nothing */
    CHECK_EQ(r.chain, r.ref_chain);            /* resuming changed nothing in the world */
    CHECK_EQ(r.count, r.ref_count);
    CHECK_EQ(r.st.dropped, 0);
    CHECK_EQ(r.st.chain, r.chain);             /* the sink's chain equals w->ledger_chain */
    CHECK_EQ(r.st.written, r.count - r.snap_count);
    FileCheck fc = check_ledger_file(path);
    CHECK(fc.ok);
    CHECK_EQ(fc.body, sizeof(AzLedgerHeader));
    CHECK_EQ(fc.h.version, AZ_LEDGER_VERSION);
    CHECK_EQ(fc.h.start_chain, r.snap_chain);
    CHECK_EQ(fc.h.start_count, r.snap_count);
    CHECK_EQ(fc.h.closed, 1);
    CHECK_EQ(fc.h.chain, r.chain);
    CHECK_EQ(fc.h.n_records, r.st.written);
    CHECK(az_ledger_check_clean(&fc.ck));
    CHECK_EQ(fc.ck.n, r.st.written);
    CHECK_EQ(fc.ck.blocks, 5);
    CHECK(fc.ck.counts[AZ_EV_DEATH] > 50);     /* real events, not only summaries */
    CHECK_EQ(fc.ck.chain, r.chain);
    CHECK_EQ(fc.bad_from_zero, 5);             /* the old reader, from 0, failed every tick */
    printf("resumed (start set): snapshot chain %016llx count %llu; after 5 ticks chain %016llx, "
           "file records %llu, %llu bad summaries from the start chain, %llu from 0\n",
           (unsigned long long)r.snap_chain, (unsigned long long)r.snap_count, (unsigned long long)r.chain,
           (unsigned long long)fc.ck.n, (unsigned long long)fc.ck.bad_summary,
           (unsigned long long)fc.bad_from_zero);
    if (keep_path)
        printf("kept %s  chain 0x%016llx  records %llu\n", path, (unsigned long long)r.chain,
               (unsigned long long)fc.ck.n);
    else
        remove(path);

    /* the same scenario without az_sink_set_start: the old mismatch */
    temp_path(path, sizeof path, "resumed_nostart");
    ResumeOut u = resumed_run(seed, n, false, path);
    CHECK(u.ok);
    CHECK(u.closed);
    CHECK_EQ(u.chain, r.chain);                /* the world is the same ... */
    CHECK(u.st.chain != u.chain);              /* ... but the sink folded from 0 */
    FileCheck fu = check_ledger_file(path);
    CHECK(fu.ok);
    CHECK_EQ(fu.h.start_chain, 0);
    CHECK_EQ(fu.h.chain, u.st.chain);
    CHECK(!az_ledger_check_clean(&fu.ck));
    CHECK_EQ(fu.ck.bad_summary, 5);
    CHECK(fu.ck.chain != u.chain);
    printf("resumed (no start):  sink chain %016llx vs world %016llx, %llu of 5 summaries fail\n",
           (unsigned long long)u.st.chain, (unsigned long long)u.chain, (unsigned long long)fu.ck.bad_summary);
    remove(path);
}

/* A fresh ledger in the current format reads clean; the same records under a
 * header of an older format version are refused (their records were 20
 * bytes with 32-bit ticks and uids; nothing reads those any more). */
static void test_formats(const char *keep_old)
{
    char path[512], path1[512];
    temp_path(path, sizeof path, "fresh");
    RunOut r = run_world(41, 3000, 5, 0, 1, 0, path);
    CHECK(r.ok);
    FileCheck f3 = check_ledger_file(path);
    CHECK(f3.ok);
    CHECK_EQ(f3.body, 128);
    CHECK_EQ(f3.h.version, 3);
    CHECK_EQ(f3.h.start_chain, 0);
    CHECK_EQ(f3.h.start_count, 0);
    CHECK(az_ledger_check_clean(&f3.ck));
    CHECK_EQ(f3.ck.chain, r.chain);
    CHECK_EQ(f3.ck.chain, f3.chain_from_zero);

    AzLedgerHeader h;
    AzEvent *ev;
    u64 n;
    bool ok = read_ledger(path, &h, &ev, &n);
    CHECK(ok);
    remove(path);
    if (!ok) { free(ev); return; }
    /* a header that says "version 2, 20-byte records" over the same bytes */
    AzLedgerHeader h2 = h;
    h2.version = 2;
    h2.record_bytes = 20;
    if (keep_old) snprintf(path1, sizeof path1, "%s", keep_old);
    else temp_path(path1, sizeof path1, "old");
    FILE *f = fopen(path1, "wb");
    CHECK(f != NULL);
    if (f) {
        CHECK_EQ(fwrite(&h2, sizeof h2, 1, f), 1);
        CHECK_EQ(fwrite(ev, sizeof(AzEvent), (size_t)n, f), n);
        fclose(f);
    }
    FileCheck f2 = check_ledger_file(path1);
    CHECK(!f2.ok);                                   /* refused */
    if (keep_old)
        printf("kept %s (a format version 2 header: must be refused)  records %llu\n", path1,
               (unsigned long long)n);
    else
        remove(path1);
    free(ev);

    /* what the decoder refuses */
    u8 raw[sizeof(AzLedgerHeader)];
    AzLedgerHeader d;
    memcpy(raw, &h, sizeof raw);
    CHECK_EQ(az_ledger_header_decode(raw, sizeof raw, &d), 128);
    CHECK_EQ(az_ledger_header_decode(raw, 64, &d), -1);          /* cut inside the header */
    CHECK_EQ(az_ledger_header_decode(raw, 63, &d), 0);           /* too short to be a header */
    CHECK_EQ(az_ledger_header_decode(NULL, 128, &d), 0);
    AzLedgerHeader bad = h;
    bad.magic ^= 1;
    memcpy(raw, &bad, sizeof raw);
    CHECK_EQ(az_ledger_header_decode(raw, sizeof raw, &d), 0);
    bad = h; bad.version = 4;                                    /* a version from the future */
    memcpy(raw, &bad, sizeof raw);
    CHECK_EQ(az_ledger_header_decode(raw, sizeof raw, &d), -1);
    bad = h; bad.version = 2;                                    /* the old versions, even with */
    memcpy(raw, &bad, sizeof raw);                               /* today's record size         */
    CHECK_EQ(az_ledger_header_decode(raw, sizeof raw, &d), -1);
    bad = h; bad.version = 1; bad.header_bytes = 64;
    memcpy(raw, &bad, sizeof raw);
    CHECK_EQ(az_ledger_header_decode(raw, sizeof raw, &d), -1);
    bad = h; bad.record_bytes = 20;
    memcpy(raw, &bad, sizeof raw);
    CHECK_EQ(az_ledger_header_decode(raw, sizeof raw, &d), -1);
    bad = h; bad.record_bytes = 24;
    memcpy(raw, &bad, sizeof raw);
    CHECK_EQ(az_ledger_header_decode(raw, sizeof raw, &d), -1);
}

/* v53: the kind names az_ledgerdump prints come from tools/az_tools.h, and
 * its order check is the core's az_event_less */
static void test_kind_names(void)
{
    u32 named = 0;
    for (u32 k = 0; k < AZ_EV_COUNT + 4; k++) {
        const char *s = azt_ev_name(k);
        if (!s) continue;
        named++;
        CHECK(k >= 1 && k < AZ_EV_COUNT);
        CHECK_EQ(azt_parse_kind(s), (int)k);               /* the name maps back to its number */
        for (u32 j = 1; j < k; j++) CHECK(azt_ev_name(j) && strcmp(s, azt_ev_name(j)) != 0);
    }
    CHECK_EQ(named, AZ_EV_COUNT - 1);
    CHECK(azt_ev_name(0) == NULL);
    CHECK(azt_ev_name(AZ_EV_COUNT) == NULL);
    CHECK_EQ(azt_parse_kind("tick"), AZ_EV_TICK);
    CHECK_EQ(azt_parse_kind("refused"), AZ_EV_REFUSED);
    CHECK_EQ(azt_parse_kind("nosuch"), -1);
    CHECK_EQ(azt_parse_kind("0"), -1);

    /* az_event_less is the documented order (ev_cmp) on random pairs */
    u32 disagree = 0;
    for (u32 i = 0; i < 200000; i++) {
        AzEvent x = random_event(rnd() % 3, i & 1), y = random_event(rnd() % 3, i & 1);
        if (i % 7 == 0) y = x;
        if (az_event_less(&x, &y) != (ev_cmp(&x, &y) < 0)) disagree++;
    }
    CHECK_EQ(disagree, 0);
}

/* ---------------------------------------------------------------------- */
/* 3. ring stress                                                          */
/* ---------------------------------------------------------------------- */
#define STRESS_N 5000000u

static AzEvent stress_event(u32 i)
{
    AzEvent e;
    e.tick = i;
    e.kind = (u16)(1 + i % 11);
    e.pad = 0;
    e.aux = i * 7u;
    e.a = i;
    e.b = ~(u64)i;
    e.v = -(i64)i;
    return e;
}

/* compare a file with the events whose indices are idx[0..n) */
static void check_stress_file(const char *path, const u32 *idx, u64 n, u64 dropped)
{
    AzLedgerHeader h;
    AzEvent *ev;
    u64 got;
    bool ok = read_ledger(path, &h, &ev, &got);
    CHECK(ok);
    if (ok) {
        CHECK_EQ(got, n);
        CHECK_EQ(h.closed, 1);
        CHECK_EQ(h.n_records, n);
        CHECK_EQ(h.n_dropped, dropped);
        CHECK_EQ(h.write_errors, 0);
        u64 bad = 0, c = 0;
        u64 m = got < n ? got : n;
        for (u64 i = 0; i < m; i++) {
            AzEvent e = stress_event(idx ? idx[i] : (u32)i);
            if (memcmp(&e, &ev[i], sizeof e) != 0) bad++;
            c = az_ledger_fold(c, &e);
        }
        CHECK_EQ(bad, 0);
        CHECK_EQ(h.chain, c);
    }
    free(ev);
    remove(path);
}

static void test_ring(void)
{
    char path[512];
    AzEvent *batch = (AzEvent *)malloc(8192 * sizeof(AzEvent));
    AzSinkStats st;

    /* (a) the consumer keeps up: the producer never offers more than fits */
    {
        temp_path(path, sizeof path, "keepup");
        AzSink *s = az_sink_open(path, 11, 1u << 16);
        CHECK(s != NULL);
        if (s) {
            u32 i = 0;
            while (i < STRESS_N) {
                u32 n = 1 + rnd() % 4096;
                if (n > STRESS_N - i) n = STRESS_N - i;
                for (u32 k = 0; k < n; k++) batch[k] = stress_event(i + k);
                while ((1u << 16) - az_sink_pending(s) < n) SwitchToThread();
                az_sink_push(s, batch, n);
                i += n;
            }
            bool ok = az_sink_close(s, &st);
            CHECK(ok);
            CHECK_EQ(st.capacity, 1u << 16);
            CHECK_EQ(st.offered, STRESS_N);
            CHECK_EQ(st.dropped, 0);
            CHECK_EQ(st.accepted, STRESS_N);
            CHECK_EQ(st.written, STRESS_N);
            check_stress_file(path, NULL, STRESS_N, 0);
            printf("ring keep-up: offered %llu dropped %llu written %llu\n", (unsigned long long)st.offered,
                   (unsigned long long)st.dropped, (unsigned long long)st.written);
        }
    }

    /* (b) the consumer cannot keep up at all: writer held, ring of 4096 */
    {
        temp_path(path, sizeof path, "held");
        AzSink *s = az_sink_open(path, 12, 4000);     /* rounds up to 4096 */
        CHECK(s != NULL);
        if (s) {
            az_sink_pause(s, true);
            u32 i = 0;
            while (i < STRESS_N) {
                u32 n = 1 + rnd() % 3000;
                if (n > STRESS_N - i) n = STRESS_N - i;
                for (u32 k = 0; k < n; k++) batch[k] = stress_event(i + k);
                az_sink_push(s, batch, n);
                i += n;
            }
            az_sink_stats(s, &st);
            CHECK_EQ(st.capacity, 4096);
            CHECK_EQ(st.offered, STRESS_N);
            CHECK_EQ(st.accepted, 4096);
            CHECK_EQ(st.dropped, STRESS_N - 4096);
            CHECK_EQ(st.written, 0);
            CHECK_EQ(az_sink_pending(s), 4096);
            az_sink_pause(s, false);
            bool ok = az_sink_close(s, &st);
            CHECK(ok);
            CHECK_EQ(st.written, 4096);
            CHECK_EQ(st.dropped, STRESS_N - 4096);
            check_stress_file(path, NULL, 4096, STRESS_N - 4096);   /* the first 4096, in order */
            printf("ring held: offered %llu dropped %llu written %llu\n", (unsigned long long)st.offered,
                   (unsigned long long)st.dropped, (unsigned long long)st.written);
        }
    }

    /* (c) mixed: a live writer that is held now and then. The test learns
     * from the counters which prefix of each batch got in, and the file
     * must hold exactly those events in order. */
    {
        temp_path(path, sizeof path, "mixed");
        u32 *idx = (u32 *)malloc((size_t)STRESS_N * sizeof(u32));
        enum { MIXED_CAPACITY = 1024 };
        AzSink *s = az_sink_open(path, 13, MIXED_CAPACITY);
        CHECK(s != NULL && idx != NULL);
        if (s && idx) {
            u64 n_in = 0, n_out = 0, bad_account = 0;
            u32 i = 0, j = 0;
            while (i < STRESS_N) {
                /* The held first push must fill the ring and refuse an event. */
                u32 n = j == 0 ? MIXED_CAPACITY + 1u : 1u + rnd() % 700u;
                if (n > STRESS_N - i) n = STRESS_N - i;
                for (u32 k = 0; k < n; k++) batch[k] = stress_event(i + k);
                if (j % 64 == 0) az_sink_pause(s, true);
                AzSinkStats before, after;
                az_sink_stats(s, &before);
                az_sink_push(s, batch, n);
                az_sink_stats(s, &after);
                u64 in = after.accepted - before.accepted, out = after.dropped - before.dropped;
                if (in + out != n || after.offered - before.offered != n) bad_account++;
                for (u64 k = 0; k < in; k++) idx[n_in + k] = i + (u32)k;
                n_in += in; n_out += out;
                if (j % 64 == 7) {
                    az_sink_pause(s, false);
                    /* Observe actual writer progress in the first live window.
                     * Later windows still race normally with producer traffic. */
                    if (j == 7) {
                        const ULONGLONG started = GetTickCount64();
                        while (GetTickCount64() - started < 10000u && az_sink_pending(s)) Sleep(1);
                        const u32 pending = az_sink_pending(s);
                        if (pending) {
                            fprintf(stderr, "mixed ring: writer did not drain after release (%u pending)\n", pending);
                            _Exit(EXIT_FAILURE); /* close waits for the writer */
                        }
                    }
                }
                i += n; j++;
            }
            bool ok = az_sink_close(s, &st);
            CHECK(ok);
            CHECK_EQ(bad_account, 0);
            CHECK_EQ(st.offered, STRESS_N);
            CHECK_EQ(st.accepted, n_in);
            CHECK_EQ(st.dropped, n_out);
            CHECK_EQ(st.accepted + st.dropped, STRESS_N);
            CHECK_EQ(st.written, n_in);
            CHECK(n_out > 0);            /* the held phases overflow a 1024 ring */
            CHECK(n_in > MIXED_CAPACITY); /* and the live phases move events      */
            check_stress_file(path, idx, n_in, n_out);
            printf("ring mixed: offered %llu dropped %llu written %llu\n", (unsigned long long)st.offered,
                   (unsigned long long)st.dropped, (unsigned long long)st.written);
        }
        free(idx);
    }

    /* an unusable path fails cleanly */
    CHECK(az_sink_open("Q:\\no\\such\\dir\\x.bin", 1, 64) == NULL);
    CHECK(az_sink_open(NULL, 1, 64) == NULL);
    free(batch);
}

/* v04: a tick with more births than one worker buffer used to hold (the
 * serial life commit emits every birth on worker 0), then as many deaths on
 * one worker. No event may be lost, at any thread count, and the summary
 * must count them all. */
static void test_big_tick(void)
{
    const u32 n = 70000;
    AzConfig cfg = az_test_config(7, n);
    cfg.cap = 2 * n + 1024;
    /* without mutation no child founds a new lineage, so a birth is exactly
     * one event (a new lineage would add an AZ_EV_CLADE record of its own) */
    cfg.lesion = AZ_LESION_MUTATION;
    AzTestWorld t;
    if (!az_test_world(&t, &cfg, 0)) { CHECK(!"world creation"); return; }
    AzWorld *w = t.w;
    u32 alive = 0;
    for (u32 s = 0; s < w->n_slots; s++)
        if (w->flags[s] & AZ_F_ALIVE) { CHECK(az_birth_request(w, s, AZ_NONE, 1, 0, 0, 0, 0)); alive++; }
    const u64 births0 = w->totals.births, count0 = w->ledger_count;
    az_life_commit(w);
    az_tally_merge(w);
    az_ledger_flush(w);
    az_tally_merge(w);
    const u64 births = w->totals.births - births0;
    CHECK_EQ(alive, n);
    CHECK_EQ(births, n);
    CHECK_EQ(w->totals.dropped_events, 0);
    CHECK_EQ(w->ledger_count - count0, births + 1);          /* every birth and the summary */

    u32 killed = 0;
    for (u32 s = 0; s < w->n_slots && killed < n; s++)
        if (w->flags[s] & AZ_F_ALIVE) { az_life_kill(w, s, 0); killed++; }
    const u64 count1 = w->ledger_count;
    az_tally_merge(w);
    az_ledger_flush(w);
    az_tally_merge(w);
    CHECK_EQ(killed, n);
    CHECK_EQ(w->totals.dropped_events, 0);
    CHECK_EQ(w->ledger_count - count1, (u64)killed + 1);     /* every death and the summary */
    printf("big tick: %" PRIu64 " births then %u deaths on one worker, dropped_events %" PRIu64 "\n",
           births, killed, w->totals.dropped_events);
    az_test_world_free(&t);
}

/* v11: the AZ_EV_TICK summary of a tick counts the living at the end of that
 * tick (it used to repeat the count of the tick's grid build, so every tick
 * with a birth or a death reported the previous population). */
static void test_summary_alive(void)
{
    AzConfig cfg = az_test_config(11, 3000);
    Capture cap;
    memset(&cap, 0, sizeof cap);
    cap.cap = 1u << 16;
    cap.ev = (AzEvent *)malloc(cap.cap * sizeof(AzEvent));
    AzHost host;
    memset(&host, 0, sizeof host);
    host.ctx = &cap; host.n_workers = 1; host.now_ns = zero_ns; host.sink = capture_sink; host.sink_ctx = &cap;
    const u64 bytes = az_world_bytes(&cfg);
    void *block = az_test_locked_block(bytes);
    AzArena arena = { (u8 *)block, bytes, 0 };
    AzWorld *w = block ? az_world_create(&arena, &cfg, &host) : NULL;
    CHECK(w != NULL);
    if (!w || !cap.ev) { free(cap.ev); az_test_locked_block_free(block); return; }
    az_worldgen(w);
    u32 wrong = 0, changed = 0;
    for (u32 t = 0; t < 60; t++) {
        const u64 b0 = w->totals.births, d0 = w->totals.deaths;
        cap.n = 0;
        az_tick(w);
        u32 alive = 0;
        for (u32 s = 0; s < w->n_slots; s++) alive += (w->flags[s] & AZ_F_ALIVE) ? 1u : 0u;
        const AzEvent *sum = cap.n ? &cap.ev[cap.n - 1] : NULL;
        if (!sum || sum->kind != AZ_EV_TICK || sum->b != alive) wrong++;
        if (w->totals.births != b0 || w->totals.deaths != d0) changed++;
    }
    CHECK_EQ(wrong, 0);
    CHECK(changed > 0);                         /* the population moved in some ticks */
    printf("tick summary: %u of 60 ticks report a wrong alive count; %u ticks had births or deaths\n",
           wrong, changed);
    free(cap.ev);
    az_test_locked_block_free(block);
}

/* v18: a chunk whose write fails partway is cut back to the last whole record,
 * so every later record reads back exactly (before the fix the fragment stayed
 * in the file and shifted every later record by its length). When the cut-back
 * fails too, the sink appends nothing more: the file keeps its whole records
 * and the fragment, which az_ledgerdump reports as a file that ends inside a
 * record. */
static bool wait_drained(const AzSink *s)
{
    for (u32 i = 0; i < 5000 && az_sink_pending(s); i++) Sleep(1);
    return az_sink_pending(s) == 0;
}

static void partial_write_case(bool cut_fails)
{
    char path[260];
    temp_path(path, sizeof path, cut_fails ? "partial_cutfail" : "partial");
    AzSink *s = az_sink_open(path, 42, 4096);
    CHECK(s != NULL);
    if (!s) return;
    AzEvent a[50], b[50], c[50];
    for (u32 i = 0; i < 50; i++) { a[i] = stress_event(i); b[i] = stress_event(1000 + i); c[i] = stress_event(2000 + i); }
    az_sink_push(s, a, 50);
    CHECK(wait_drained(s));
    az_sink_fail_next_write(s, 7, cut_fails);   /* 7 bytes of b reach the file, then the write fails */
    az_sink_push(s, b, 50);
    CHECK(wait_drained(s));
    az_sink_push(s, c, 50);
    AzSinkStats st;
    CHECK(!az_sink_close(s, &st));              /* a write failed, and close says so */
    const u64 want_n = cut_fails ? 50 : 100;
    CHECK_EQ(st.write_errors, cut_fails ? 2u : 1u);
    CHECK_EQ(st.written, want_n);

    FILE *f = fopen(path, "rb");
    CHECK(f != NULL);
    if (f) {
        AzLedgerHeader h;
        AzEvent got[100];
        CHECK(fread(&h, sizeof h, 1, f) == 1);
        _fseeki64(f, 0, SEEK_END);
        const long long size = _ftelli64(f);
        CHECK_EQ((u64)size, (u64)sizeof h + want_n * sizeof(AzEvent) + (cut_fails ? 7u : 0u));
        _fseeki64(f, (long long)sizeof h, SEEK_SET);
        const bool read_all = fread(got, sizeof(AzEvent), (size_t)want_n, f) == (size_t)want_n;
        CHECK(read_all);
        if (read_all) {
            CHECK(memcmp(got, a, sizeof a) == 0);
            if (!cut_fails) CHECK(memcmp(got + 50, c, sizeof c) == 0);
            CHECK_EQ(h.chain, fold_all(0, got, want_n));
        }
        CHECK_EQ(h.closed, 1);
        CHECK_EQ(h.n_records, want_n);
        CHECK_EQ(h.write_errors, st.write_errors);
        fclose(f);
    }
    printf("partial write%s: write_errors %u, %llu whole records, %s\n", cut_fails ? " and failed cut" : "",
           st.write_errors, (unsigned long long)want_n,
           cut_fails ? "then the 7-byte fragment (nothing appended after it)" : "no fragment (a then c)");
    CHECK(DeleteFileA(path));
}

static void test_partial_write(void)
{
    partial_write_case(false);
    partial_write_case(true);
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);   /* progress stays visible if a run is cut short */
    const bool have_dir = temp_dir_create();
    CHECK(have_dir);
    test_partial_write();
    test_big_tick();
    test_summary_alive();
    const char *keep = NULL, *keep_resumed = NULL, *keep_old = NULL;
    for (int i = 1; i + 1 < argc; i++) {
        if (!strcmp(argv[i], "--keep")) keep = argv[i + 1];
        if (!strcmp(argv[i], "--keep-resumed")) keep_resumed = argv[i + 1];
        if (!strcmp(argv[i], "--keep-old")) keep_old = argv[i + 1];
    }
    test_kind_names();
    test_resumed(keep_resumed);
    test_formats(keep_old);
    ULONGLONG t0 = GetTickCount64();
    test_order_and_chain();
    ULONGLONG t1 = GetTickCount64();
    test_worlds(keep);
    ULONGLONG t2 = GetTickCount64();
    test_ring();
    ULONGLONG t3 = GetTickCount64();
    printf("wall time (noisy, parallel load): order %llu ms, worlds %llu ms, ring %llu ms\n",
           (unsigned long long)(t1 - t0), (unsigned long long)(t2 - t1), (unsigned long long)(t3 - t2));
    /* every temporary file was removed where it was used; the folder is empty */
    if (have_dir) CHECK(RemoveDirectoryA(temp_dir));
    return az_test_done();
}
