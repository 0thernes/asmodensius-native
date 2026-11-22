/* test_store.c - the creature store (core/az_store.c): one block of any size
 * per creature, inside the locked arena, behind a handle.
 *
 *   1. az_blk_units and the canonical form of a block (az_blk_ok)
 *   2. new, resize (in place and by moving), fit, release: contents kept,
 *      pads zero, the live count exact
 *   3. a model test: thousands of random operations against a shadow copy
 *   4. compaction: on demand when the bump pointer reaches the end, and by
 *      az_store_compact; blocks end in slot order; nothing is lost
 *   5. a full store refuses: false, counted, one ledger event, nothing
 *      damaged, and room given back is used again
 *   6. body parts: add, drop, the matter in them inside the audits; requests
 *      applied by az_store_commit; the long-term memory, of any length
 *   7. worlds: the same state hash for every store period and thread count,
 *      also when the store is too small for everyone (the same refusals)
 */
#include "az_test.h"

static u64 rs = 0x5707E5ull;
static u32 below(u32 n) { return (u32)(((az_test_rand64(&rs) >> 32) * (u64)n) >> 32); }

static u32 refusals_seen;
static void count_sink(void *ctx, const AzEvent *ev, u32 n)
{
    (void)ctx;
    for (u32 i = 0; i < n; i++)
        if (ev[i].kind == AZ_EV_REFUSED && ev[i].aux == AZ_REF_STORE) refusals_seen++;
}

static bool slot_ok(const AzWorld *w, u32 s) { return az_blk_ok(az_blk(w, s), az_blk(w, s)->units); }

static u64 live_units(const AzWorld *w)
{
    u64 u = 0;
    for (u32 s = 0; s < w->n_slots; s++) if (w->blk[s]) u += az_blk(w, s)->units;
    return u;
}

/* ---- 1. units and canonical form ---- */
static void test_units(void)
{
    u32 len[AZ_SEG_COUNT] = { 0 };
    CHECK_EQ(az_blk_units(len), 1);                       /* the header unit alone */
    len[AZ_SEG_GEN_A] = 1;
    CHECK_EQ(az_blk_units(len), 2);
    len[AZ_SEG_GEN_A] = 8; len[AZ_SEG_BRAIN] = 56;
    CHECK_EQ(az_blk_units(len), 2);                       /* 8 + 56 fill one unit exactly */
    len[AZ_SEG_BODY] = 1;
    CHECK_EQ(az_blk_units(len), 3);
    len[AZ_SEG_GEN_A] = 9;                                /* padded to 16: 16 + 56 + 8 = 80 */
    CHECK_EQ(az_blk_units(len), 3);
    for (u32 k = 0; k < AZ_SEG_COUNT; k++) len[k] = 0xFFFFFFF0u;
    CHECK_EQ(az_blk_units(len), 402653184u);              /* counted in 64 bits: six segments of 4 GB do not wrap */
}

/* ---- shadow model ---- */
enum { MODEL_SLOTS = 96, MODEL_MAX = 3000 };
typedef struct Shadow { bool has; u32 len[AZ_SEG_COUNT]; u8 data[AZ_SEG_COUNT][MODEL_MAX]; } Shadow;
static Shadow *shadow;

static void fill_seg(AzWorld *w, u32 s, u32 seg, u32 from)
{
    u32 bytes;
    u8 *p = (u8 *)az_seg(w, s, seg, &bytes);
    for (u32 i = from; i < bytes; i++) p[i] = shadow[s].data[seg][i] = (u8)(az_test_rand64(&rs) >> 24);
}

static u32 check_slot(const AzWorld *w, u32 s)
{
    u32 bad = 0;
    if (!shadow[s].has) return w->blk[s] != 0;
    if (!w->blk[s] || !slot_ok(w, s)) return 1;
    for (u32 seg = 0; seg < AZ_SEG_COUNT; seg++) {
        u32 bytes;
        const u8 *p = (const u8 *)az_seg(w, s, seg, &bytes);
        if (bytes != shadow[s].len[seg] || memcmp(p, shadow[s].data[seg], bytes) != 0) bad++;
    }
    return bad;
}

/* genes come in fours: az_blk_ok wants the two gene segments in whole genes */
static u32 seg_len(u32 seg, u32 most)
{
    u32 n = below(4) == 0 ? 0u : below(most + 1u);
    if (seg == AZ_SEG_GEN_A || seg == AZ_SEG_GEN_B) n &= ~3u;
    return n;
}

static void test_model(void)
{
    AzConfig cfg = az_test_config(3, 16);
    cfg.cap = 256;
    cfg.store_units = 1500;                               /* small on purpose: it fills up and packs itself often */
    AzTestWorld t;
    if (!az_test_world_ex(&t, &cfg, 0, AZ_TEST_NO_WORLDGEN)) { CHECK(!"world"); return; }
    AzWorld *w = t.w;
    shadow = (Shadow *)calloc(MODEL_SLOTS, sizeof(Shadow));
    if (!shadow) { CHECK(!"memory"); az_test_world_free(&t); return; }
    for (u32 i = 0; i < MODEL_SLOTS; i++) CHECK_EQ(az_spawn(w), i);
    CHECK_EQ(w->store_live, 0);
    /* a slot without a block reads as an empty block */
    {
        u32 bytes = 99, n = 99;
        CHECK(az_seg(w, 5, AZ_SEG_BRAIN, &bytes) != NULL || bytes == 0);
        CHECK_EQ(bytes, 0);
        (void)az_genes(w, 5, 0, &n);
        CHECK_EQ(n, 0);
        (void)az_parts(w, 5, &n);
        CHECK_EQ(n, 0);
        CHECK_EQ(az_body_matter(w, 5), 0);
    }
    u32 ops[6] = { 0 }, bad = 0, refused = 0, compactions = 0;
    const u64 denied0 = w->totals.store_denied;
    for (u32 it = 0; it < 30000; it++) {
        const u32 s = below(MODEL_SLOTS), op = below(100);
        Shadow *sh = &shadow[s];
        const u32 base0 = w->store_base;
        if (!sh->has && op < 70) {                        /* new */
            u32 len[AZ_SEG_COUNT];
            for (u32 k = 0; k < AZ_SEG_COUNT; k++) len[k] = seg_len(k, k == AZ_SEG_BRAIN ? MODEL_MAX : 400);
            const u64 d0 = w->totals.store_denied;
            if (az_blk_new(w, s, len)) {
                sh->has = true;
                /* a new block is zero-filled */
                u32 nz = 0;
                for (u32 k = 0; k < AZ_SEG_COUNT; k++) {
                    u32 bytes;
                    const u8 *p = (const u8 *)az_seg(w, s, k, &bytes);
                    if (bytes != len[k]) nz++;
                    for (u32 i = 0; i < bytes; i++) nz += p[i] != 0;
                    sh->len[k] = len[k];
                    fill_seg(w, s, k, 0);
                }
                bad += nz != 0;
                ops[0]++;
            } else {
                refused++;
                bad += w->blk[s] != 0 || w->totals.store_denied != d0 + 1;
            }
        } else if (sh->has && op < 45) {                  /* resize one segment */
            const u32 seg = below(AZ_SEG_COUNT), old = sh->len[seg];
            const u32 want = seg_len(seg, seg == AZ_SEG_BRAIN ? MODEL_MAX : 400);
            const u64 d0 = w->totals.store_denied;
            if (az_blk_resize(w, s, seg, want)) {
                sh->len[seg] = want;
                u32 bytes, nz = 0;
                const u8 *p = (const u8 *)az_seg(w, s, seg, &bytes);
                if (bytes != want) nz++;
                for (u32 i = old; i < bytes; i++) nz += p[i] != 0;        /* what a segment gains is zero */
                bad += nz != 0;
                if (want > old) fill_seg(w, s, seg, old);
                ops[1]++;
            } else {
                refused++;
                bad += want <= old || w->totals.store_denied != d0 + 1;   /* a shrink never fails */
            }
        } else if (sh->has && op < 60) {                  /* fit: several segments shrink at once */
            u32 len[AZ_SEG_COUNT];
            for (u32 k = 0; k < AZ_SEG_COUNT; k++) {
                len[k] = below(3) ? sh->len[k] : below(sh->len[k] + 1u);
                if (k == AZ_SEG_GEN_A || k == AZ_SEG_GEN_B) len[k] &= ~3u;
                sh->len[k] = len[k];
            }
            az_blk_fit(w, s, len);
            ops[2]++;
        } else if (sh->has && op < 85) {                  /* release */
            const u32 units = az_blk(w, s)->units, live = w->store_live;
            az_blk_release(w, s);
            bad += w->blk[s] != 0 || w->store_live != live - units;
            sh->has = false;
            ops[3]++;
        } else if (op >= 97) {                            /* pack now */
            az_store_compact(w);
            u32 last = 0, order_bad = 0;
            for (u32 q = 0; q < w->n_slots; q++) {
                if (!w->blk[q]) continue;
                if (w->blk[q] <= last) order_bad++;       /* slot order */
                last = w->blk[q];
            }
            bad += order_bad != 0 || w->store_top != w->store_live + 1u;
            ops[4]++;
        }
        compactions += w->store_base != base0;
        bad += check_slot(w, s);
        if ((it & 255u) == 0) {
            for (u32 q = 0; q < MODEL_SLOTS; q++) bad += check_slot(w, q);
            bad += live_units(w) != w->store_live;
        }
        if (bad) { printf("  model: first fault at operation %u\n", it); break; }
    }
    for (u32 q = 0; q < MODEL_SLOTS; q++) bad += check_slot(w, q);
    CHECK_EQ(bad, 0);
    CHECK_EQ(live_units(w), w->store_live);
    CHECK(ops[0] > 1000 && ops[1] > 1000 && ops[2] > 500 && ops[3] > 1000 && ops[4] > 100);
    CHECK(compactions > ops[4]);                          /* the store also packed itself when it ran out at the top */
    CHECK(refused > 0);                                   /* and it was full at times */
    CHECK_EQ(w->totals.store_denied - denied0, refused);
    printf("model: %u new, %u resized, %u fitted, %u released, %u packed by call, %u compactions in all, %u refusals; "
           "every block equal to its shadow\n", ops[0], ops[1], ops[2], ops[3], ops[4], compactions, refused);
    free(shadow);
    az_test_world_free(&t);
}

/* ---- 5. a full store ---- */
static void test_full(void)
{
    AzConfig cfg = az_test_config(5, 16);
    cfg.cap = 256;
    cfg.store_units = 600;
    AzTestWorld t;
    if (!az_test_world_ex(&t, &cfg, 1, AZ_TEST_NO_WORLDGEN)) { CHECK(!"world"); return; }
    AzWorld *w = t.w;
    t.host->sink = count_sink;
    refusals_seen = 0;
    u32 len[AZ_SEG_COUNT] = { 0 };
    len[AZ_SEG_BRAIN] = 64 * 9;                           /* ten units a block */
    u32 made = 0, first_refused = AZ_NONE;
    for (u32 i = 0; i < 100; i++) {
        const u32 s = az_spawn(w);
        if (az_blk_new(w, s, len)) { made++; memset(az_seg(w, s, AZ_SEG_BRAIN, NULL), (int)(s + 1), len[AZ_SEG_BRAIN]); }
        else if (first_refused == AZ_NONE) first_refused = s;
    }
    /* 600 units, unit 0 is the empty block: 59 blocks of ten fit */
    CHECK_EQ(made, 59);
    CHECK_EQ(w->totals.store_denied, 41);
    CHECK_EQ(w->store_live, 590);
    u32 damaged = 0;
    for (u32 s = 0; s < w->n_slots; s++) {
        if (!w->blk[s]) continue;
        u32 bytes;
        const u8 *p = (const u8 *)az_seg(w, s, AZ_SEG_BRAIN, &bytes);
        for (u32 i = 0; i < bytes; i++) damaged += p[i] != (u8)(s + 1);
        damaged += !slot_ok(w, s);
    }
    CHECK_EQ(damaged, 0);
    /* growing a block in a full store is refused too, and leaves it as it was */
    CHECK(!az_blk_resize(w, 0, AZ_SEG_BODY, 4000));
    CHECK(slot_ok(w, 0));
    /* the first refusal of the run is in the ledger, the others are counted only */
    az_ledger_flush(w);
    CHECK_EQ(refusals_seen, 1);
    /* room given back is used again */
    az_blk_release(w, 3); az_blk_release(w, 17);
    CHECK(first_refused != AZ_NONE && az_blk_new(w, first_refused, len));
    CHECK(slot_ok(w, first_refused));
    printf("full store: 59 blocks of ten units in 600, 41 refusals counted, %u ledger event\n", refusals_seen);
    t.host->sink = NULL;
    az_test_world_free(&t);
}

/* ---- 6. body parts ---- */
static void test_parts(void)
{
    AzConfig cfg = az_test_config(9, 200);
    AzTestWorld t;
    if (!az_test_world(&t, &cfg, 0)) { CHECK(!"world"); return; }
    AzWorld *w = t.w;
    u32 s = AZ_NONE;
    for (u32 q = 0; q < w->n_slots && s == AZ_NONE; q++) if (w->flags[q] & AZ_F_ALIVE) s = q;
    CHECK(s != AZ_NONE);
    if (s == AZ_NONE) { az_test_world_free(&t); return; }
    const u64 m0 = az_total_matter(w);
    const i64 e0 = az_total_energy(w);
    u32 n0, n;
    (void)az_parts(w, s, &n0);
    CHECK(n0 >= 1);
    /* a new part: at the end, empty, of the kind asked for */
    CHECK(az_part_add(w, s, AZ_PART_ARMOR));
    AzPart *p = az_parts(w, s, &n);
    CHECK_EQ(n, n0 + 1);
    CHECK(p[n - 1].kind == AZ_PART_ARMOR && p[n - 1].matter == 0 && p[n - 1].health == 255);
    CHECK(slot_ok(w, s));
    CHECK_EQ(az_total_matter(w), m0);
    /* matter moved from a pool into a part stays inside both audits (biomass either way) */
    const u32 moved = w->pool_a[s] / 2u;
    CHECK(moved > 0);
    w->pool_a[s] -= moved; p[n - 1].matter += moved;
    CHECK_EQ(az_body_matter(w, s) >= moved, 1);
    CHECK_EQ(az_total_matter(w), m0);
    CHECK_EQ(az_total_energy(w), e0);
    /* a part that holds matter cannot be dropped; an empty one can */
    CHECK(!az_part_drop(w, s, n - 1));
    CHECK(!az_part_drop(w, s, n + 5));
    p = az_parts(w, s, &n);
    p[n - 1].matter -= moved; w->pool_a[s] += moved;
    const u8 kind_first = p[0].kind;
    CHECK(az_part_drop(w, s, n - 1));
    p = az_parts(w, s, &n);
    CHECK_EQ(n, n0);
    CHECK_EQ(p[0].kind, kind_first);
    CHECK(slot_ok(w, s));
    /* a body may grow far past what one byte or sixteen bits could count */
    u32 grown = 0;
    for (u32 i = 0; i < 70000; i++) grown += az_part_add(w, s, 1u + i % (AZ_PART_KINDS - 1u));
    p = az_parts(w, s, &n);
    CHECK(grown > 66000 || w->totals.store_denied > 0);  /* the store is the only bound */
    CHECK_EQ(n, n0 + grown);
    CHECK(slot_ok(w, s));
    CHECK_EQ(az_total_matter(w), m0);
    printf("parts: one body grown to %u parts (%u units)\n", n, az_blk(w, s)->units);
    while (n > n0) { CHECK(az_part_drop(w, s, n - 1)); (void)az_parts(w, s, &n); if (n > n0 + 64) { (void)az_blk_resize(w, s, AZ_SEG_BODY, (n0 + 64) * 8); (void)az_parts(w, s, &n); } }

    /* requests: asked in a phase-3 hook, applied by az_store_commit */
    az_part_request(w, s, AZ_PART_SENSOR);
    az_store_commit(w);
    p = az_parts(w, s, &n);
    CHECK_EQ(n, n0 + 1);
    CHECK_EQ(p[n - 1].kind, AZ_PART_SENSOR);
    CHECK_EQ(w->pair[s].req & (AZ_RQ_GROW | AZ_RQ_SHED), 0);
    az_part_shed_request(w, s, n - 1);
    az_store_commit(w);
    (void)az_parts(w, s, &n);
    CHECK_EQ(n, n0);
    /* the dead ask for nothing */
    const u8 fl = w->flags[s];
    w->flags[s] = 0; w->pair[s].req |= AZ_RQ_GROW;
    az_memory_request(w, s, 3);
    az_store_commit(w);
    (void)az_parts(w, s, &n);
    CHECK_EQ(n, n0);
    w->flags[s] = fl; w->pair[s].req = 0; w->blk_req[(size_t)s * 3 + 2] = 0;

    /* the long-term memory: no length at birth, any length in life */
    {
        u32 words = 99;
        (void)az_memory(w, s, &words);
        CHECK_EQ(words, 0);
        az_memory_request(w, s, 5);
        (void)az_memory(w, s, &words);
        CHECK_EQ(words, 0);                               /* asked, not yet made */
        az_store_commit(w);
        u64 *m = az_memory(w, s, &words);
        CHECK_EQ(words, 5);
        u32 nz = 0;
        for (u32 i = 0; i < words; i++) { nz += m[i] != 0; m[i] = 0x1111111111111111ull * (i + 1u); }
        CHECK_EQ(nz, 0);                                  /* new words are zero */
        CHECK(slot_ok(w, s));
        const u64 hm = az_state_hash(w);
        m[2] ^= 1u;
        CHECK(az_state_hash(w) != hm);                    /* what a creature remembers is state */
        m[2] ^= 1u;
        az_memory_request(w, s, 70000);                   /* longer than 16 bits could count */
        az_store_commit(w);
        m = az_memory(w, s, &words);
        CHECK_EQ(words, 70000);
        nz = 0;
        for (u32 i = 0; i < 5; i++) nz += m[i] != 0x1111111111111111ull * (i + 1u);
        for (u32 i = 5; i < words; i++) nz += m[i] != 0;
        CHECK_EQ(nz, 0);                                  /* it kept what it knew */
        u32 parts_now;
        (void)az_parts(w, s, &parts_now);
        CHECK_EQ(parts_now, n0);                          /* and the body beside it is untouched */
        az_memory_request(w, s, 2);
        az_part_request(w, s, AZ_PART_FIN);               /* both kinds of request in one tick */
        az_store_commit(w);
        m = az_memory(w, s, &words);
        CHECK_EQ(words, 2);
        CHECK(m[0] == 0x1111111111111111ull && m[1] == 0x2222222222222222ull);
        (void)az_parts(w, s, &parts_now);
        CHECK_EQ(parts_now, n0 + 1);
        CHECK(slot_ok(w, s));
        /* a memory no store could hold is refused, counted, and nothing is lost */
        const u64 d0 = w->totals.store_denied;
        az_memory_request(w, s, 0xFFFFFFFFu);
        az_store_commit(w);
        m = az_memory(w, s, &words);
        CHECK_EQ(words, 2);
        CHECK_EQ(w->totals.store_denied, d0 + 1);
        CHECK(m[0] == 0x1111111111111111ull);
        az_memory_request(w, s, 0);                       /* forget everything */
        az_part_shed_request(w, s, n0);
        az_store_commit(w);
        (void)az_memory(w, s, &words);
        CHECK_EQ(words, 0);
        (void)az_parts(w, s, &parts_now);
        CHECK_EQ(parts_now, n0);
        CHECK_EQ(az_total_matter(w), m0);
        printf("memory: grown to 70000 words, cut to 2, a request for 2^32 words refused and counted, forgotten\n");
    }

    /* a living world grows bodies: matter is built into parts and the books stay exact */
    u64 built = 0;
    u32 most = 0;
    int exact = 1;
    for (u32 i = 0; i < 200; i++) {
        az_tick(w);
        if (az_total_matter(w) != m0 || az_total_energy(w) != e0) exact = 0;
    }
    for (u32 q = 0; q < w->n_slots; q++) {
        if (!(w->flags[q] & AZ_F_ALIVE)) continue;
        built += az_body_matter(w, q);
        (void)az_parts(w, q, &n);
        if (n > most) most = n;
    }
    CHECK(exact);
    CHECK(built > 0);
    printf("parts: after 200 ticks %" PRIu64 " mu are built into body parts, the largest body has %u parts; matter and "
           "energy exact at every tick\n", built, most);
    az_test_world_free(&t);
}

/* ---- 7. worlds ---- */
typedef struct Run { u64 hash, denied, births; u32 live; int ok; } Run;

static Run world_run(u32 threads, u8 reorder, u16 store_period, u32 store_units)
{
    Run r = { 0, 0, 0, 0, 0 };
    AzConfig cfg = az_test_config(31, 3000);
    cfg.reorder_period = reorder;
    cfg.store_period = store_period;
    cfg.store_units = store_units;
    AzTestWorld t;
    if (!az_test_world(&t, &cfg, threads)) { CHECK(!"world"); return r; }
    AzWorld *w = t.w;
    az_test_founders_bud(w);
    const u64 m0 = az_total_matter(w);
    const i64 e0 = az_total_energy(w);
    r.ok = 1;
    for (u32 i = 0; i < 100; i++) {
        az_tick(w);
        if (az_total_matter(w) != m0 || az_total_energy(w) != e0) r.ok = 0;
        if ((i % 25) == 24 && live_units(w) != w->store_live) r.ok = 0;
    }
    for (u32 s = 0; s < w->n_slots; s++)
        if ((w->flags[s] & AZ_F_ALIVE) && w->blk[s] && !slot_ok(w, s)) r.ok = 0;
    r.hash = az_state_hash(w); r.denied = w->totals.store_denied; r.births = w->totals.births; r.live = w->n_live;
    az_test_world_free(&t);
    return r;
}

static void test_worlds(void)
{
    /* room for everyone */
    const Run a = world_run(0, 1, 1, 0);
    const Run b = world_run(8, 7, 3, 0);
    const Run c = world_run(3, 31, 64, 0);
    const Run d = world_run(12, 255, 1000, 1200000);      /* another store size, packed only on demand */
    CHECK(a.ok && b.ok && c.ok && d.ok);
    CHECK_EQ(a.denied, 0);
    CHECK_EQ(d.denied, 0);
    CHECK(a.births > 100);
    CHECK_EQ(a.hash, b.hash); CHECK_EQ(a.hash, c.hash); CHECK_EQ(a.hash, d.hash);
    /* a store too small for the founders and their children: the same creatures are refused whatever the
     * thread count, slot order and packing schedule, and the books stay exact */
    const Run e = world_run(0, 1, 1, 100000);
    const Run f = world_run(8, 7, 3, 100000);
    const Run g = world_run(5, 31, 64, 100000);
    CHECK(e.ok && f.ok && g.ok);
    CHECK(e.denied > 0);
    CHECK_EQ(e.denied, f.denied); CHECK_EQ(e.denied, g.denied);
    CHECK_EQ(e.hash, f.hash); CHECK_EQ(e.hash, g.hash);
    CHECK(e.hash != a.hash);
    printf("worlds: %" PRIu64 " births, hash %016" PRIx64 " for four store schedules and thread counts; with a store of "
           "100000 units %" PRIu64 " blocks were refused in each of three runs, hash %016" PRIx64 "\n",
           a.births, a.hash, e.denied, e.hash);
}

int main(void)
{
    test_units();
    test_model();
    test_full();
    test_parts();
    test_worlds();
    return az_test_done();
}
