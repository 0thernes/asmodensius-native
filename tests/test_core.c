/* test_core.c - the invariants every build must keep (owner: project lead).
 *   1. Philox4x32-10 matches the published known answers.
 *   2. Matter and the energy invariant are exact after every tick.
 *   3. The state hash does not depend on thread count.
 *   4. The state hash does not depend on the physical reorder period,
 *      also when the slot capacity binds and births are refused.
 *   5. The state hash does not depend on where the work is cut
 *      (debug_split_seed randomises every split).
 *   6. The state hash does depend on the seed (the hash is not vacuous).
 *   7. The state hash sees what waits in the cell accumulators at a tick
 *      boundary (finding v01): the life commit posts the refunds of refused
 *      births after the tick's last field commit, the audits and the
 *      snapshot count them, so the hash must too.
 *   8. The arena holds what the world uses (finding v60): az_world_bytes
 *      reserves no event buffers for workers the host does not have.
 *   9. az_birth_request refuses a provision AzBirth cannot hold (v07).
 *  10. Colony deposits and withdrawals no commit applied stay pending (v40).
 *  11. A host with more workers than AzWorld has tallies is refused (v44).
 *  12. A birth is refused while the only free slots died this tick (v42:
 *      this pins the behaviour the corrected documents now describe).
 *  13. A world moved by az_world_rebase keeps working kernels (v45).
 */
#include "az_test_locked_block.h"

typedef struct RunOut { u64 hash; u64 births, deaths, cap_denied; u32 live; } RunOut;

static RunOut run(u64 seed, u32 n, u32 cap, u32 ticks, u32 threads, u8 reorder, u32 split_seed, int audit)
{
    RunOut out;
    memset(&out, 0, sizeof out);
    AzConfig cfg = az_test_config(seed, n);
    if (cap) cfg.cap = cap;
    cfg.reorder_period = reorder;
    cfg.debug_split_seed = split_seed;
    AzTestWorld t;
    if (!az_test_world(&t, &cfg, threads)) { CHECK(!"world creation"); return out; }
    u64 m0 = az_total_matter(t.w);
    i64 e0 = az_total_energy(t.w);
    for (u32 i = 0; i < ticks; i++) {
        az_tick(t.w);
        if (audit) {
            CHECK_EQ(az_total_matter(t.w), m0);
            CHECK_EQ(az_total_energy(t.w), e0);
        }
    }
    CHECK_EQ(az_total_matter(t.w), m0);
    CHECK_EQ(az_total_energy(t.w), e0);
    CHECK_EQ(t.w->totals.field_underflow, 0);
    CHECK_EQ(t.w->totals.dropped_events, 0);
    CHECK(t.w->n_live > 0);
    out.hash = az_state_hash(t.w);
    out.births = t.w->totals.births;
    out.deaths = t.w->totals.deaths;
    out.cap_denied = t.w->totals.cap_denied;
    out.live = t.w->n_live;
    az_test_world_free(&t);
    return out;
}

static u64 zero_ns(void *ctx) { (void)ctx; return 0; }

static void arena_is_used(void)
{
    AzConfig cfg = az_test_config(42, 100000);           /* the 100k reference geometry */
    cfg.cap = 400000;
    cfg.gx_bits = 6; cfg.gy_bits = 6; cfg.gz_bits = 5;
    const u64 bytes = az_world_bytes(&cfg);
    static const u32 workers[] = { 1, 8, 24 };
    for (u32 i = 0; i < AZ_LEN(workers); i++) {
        AzHost host;
        memset(&host, 0, sizeof host);                   /* create reads only n_workers */
        host.n_workers = workers[i];
        host.now_ns = zero_ns;
        void *block = az_test_locked_block(bytes);
        if (!block) { CHECK(!"arena"); return; }
        AzArena arena;
        arena.base = (u8 *)block; arena.size = bytes; arena.used = 0;
        AzWorld *w = az_world_create(&arena, &cfg, &host);
        CHECK(w != NULL);
        const u64 unused = arena.size - arena.used;
        CHECK(unused * 100 < arena.size);                /* under 1 percent left over */
        printf("arena %u workers: %.1f MiB, unused %.2f MiB\n", workers[i],
               (double)arena.size / 1048576.0, (double)unused / 1048576.0);
        az_test_locked_block_free(block);
    }
}

static u32 first_alive(const AzWorld *w, u32 from)
{
    for (u32 s = from; s < w->n_slots; s++)
        if (w->flags[s] & AZ_F_ALIVE) return s;
    return AZ_NONE;
}

static void birth_request_range(void)
{
    AzConfig cfg = az_test_config(13, 500);
    AzTestWorld t;
    if (!az_test_world(&t, &cfg, 0)) { CHECK(!"world creation"); return; }
    AzWorld *w = t.w;
    const u32 s = first_alive(w, 0);
    const u32 s2 = s == AZ_NONE ? AZ_NONE : first_alive(w, s + 1);
    CHECK(s != AZ_NONE && s2 != AZ_NONE);
    if (s != AZ_NONE && s2 != AZ_NONE) {
        CHECK(!az_birth_request(w, s, AZ_NONE, 0, 0, 0, 70000u, 0));    /* no children: refused */
        CHECK_EQ(w->birth[s].n, 0);                                     /* nothing recorded */
        /* no brood size is refused for being large (the clutch cap is gone):
         * matter, energy, slots and store room are what limit a brood */
        CHECK(az_birth_request(w, s, AZ_NONE, 100000u, 0, 65536u, 0, 0));
        CHECK_EQ(w->birth[s].n, 100000u);
        w->birth[s].n = 0; w->birth[s].prov[0] = 0;
        /* a provision is not limited by 16 bits any more (finding v07 is gone
         * with the u16 fields): only matter and energy limit a brood */
        CHECK(az_birth_request(w, s2, AZ_NONE, 1, 0, 70000u, 3000000000u, 65536u));
        CHECK(!az_birth_request(w, s2, AZ_NONE, 1, 0, 1u, 1u, 1u));     /* a second call is refused */
        CHECK_EQ(w->birth[s2].prov[0], 70000u);
        CHECK_EQ(w->birth[s2].prov[1], 3000000000u);
        CHECK_EQ(w->birth[s2].prov[2], 65536u);
        w->birth[s2].n = 0;
    }
    az_test_world_free(&t);
}

static void colony_pending_is_kept(void)
{
    AzConfig cfg = az_test_config(11, 2000);
    AzTestWorld t;
    if (!az_test_world(&t, &cfg, 0)) { CHECK(!"world creation"); return; }
    AzWorld *w = t.w;
    const u32 s = first_alive(w, 0);
    CHECK(s != AZ_NONE);
    if (s == AZ_NONE) { az_test_world_free(&t); return; }
    /* setup, before the books are read: colony 1 holds 10 units of resource
     * 0, and the entity carries 100 units of resources 0 and 1 */
    w->n_colonies = 2;
    memset(&w->colonies[1], 0, sizeof w->colonies[1]);
    memset(w->colony_eph, 0, 2 * sizeof(AzColonyEph));
    w->colonies[1].store[0] = 10;
    w->inv[(size_t)s * AZ_NINV + 0] = 100;
    w->inv[(size_t)s * AZ_NINV + 1] = 100;
    const u64 m0 = az_total_matter(w);
    const i64 e0 = az_total_energy(w);
    /* what modules could do: take 20 from colony 1 (which holds 10), and
     * deposit 7 into colony 0 ("no colony"), which nothing commits */
    w->inv[(size_t)s * AZ_NINV + 0] += 20;
    az_atomic_add_u64(&w->colony_eph[1].taken[0], 20);
    w->inv[(size_t)s * AZ_NINV + 1] -= 7;
    az_atomic_add_u64(&w->colony_eph[0].dep[1], 7);
    CHECK_EQ(az_total_matter(w), m0);
    CHECK_EQ(az_total_energy(w), e0);
    const u64 under0 = w->totals.field_underflow;
    az_tick(w);
    CHECK_EQ(az_total_matter(w), m0);              /* nothing vanished with the tick */
    CHECK_EQ(az_total_energy(w), e0);
    CHECK(w->totals.field_underflow > under0);     /* the refused commit is counted */
    CHECK_EQ(w->colony_eph[1].taken[0], 20);
    CHECK_EQ(w->colony_eph[0].dep[1], 7);
    CHECK_EQ(az_snapshot_bytes(w), 0);             /* no snapshot while changes are pending */
    const u64 h = az_state_hash(w);
    w->colony_eph[0].dep[1] = 0;
    CHECK(az_state_hash(w) != h);                  /* the hash sees a pending deposit */
    w->colony_eph[0].dep[1] = 7;
    CHECK_EQ(az_state_hash(w), h);
    printf("colony pending after tick %" PRIu64 ": taken %" PRIu64 ", deposit %" PRIu64 ", field_underflow %" PRIu64 "\n",
           w->tick, w->colony_eph[1].taken[0], w->colony_eph[0].dep[1], w->totals.field_underflow);
    az_test_world_free(&t);
}

static void host_too_large(void)
{
    AzConfig cfg = az_test_config(5, 100);
    const u64 bytes = az_world_bytes(&cfg);
    static const u32 counts[] = { AZ_MAX_WORKERS, AZ_MAX_WORKERS + 1, 96 };
    for (u32 i = 0; i < AZ_LEN(counts); i++) {
        AzHost host;
        memset(&host, 0, sizeof host);
        host.n_workers = counts[i];
        host.now_ns = zero_ns;
        void *block = az_test_locked_block(bytes);
        if (!block) { CHECK(!"arena"); return; }
        AzArena arena;
        arena.base = (u8 *)block; arena.size = bytes; arena.used = 0;
        AzWorld *w = az_world_create(&arena, &cfg, &host);
        if (counts[i] <= AZ_MAX_WORKERS) CHECK(w != NULL);
        else CHECK(w == NULL);                     /* worker tokens past workers[63] */
        az_test_locked_block_free(block);
    }
}

static void dead_slots_wait_for_grid(void)
{
    AzConfig cfg = az_test_config(17, 256);
    cfg.cap = 256;                                 /* every slot is used by a founder */
    AzTestWorld t;
    if (!az_test_world(&t, &cfg, 0)) { CHECK(!"world creation"); return; }
    AzWorld *w = t.w;
    CHECK_EQ(w->n_slots, w->cap);
    u32 killed = 0;
    for (u32 s = 0; s < w->n_slots && killed < 10; s++)
        if (w->flags[s] & AZ_F_ALIVE) { az_life_kill(w, s, 0); killed++; }
    u32 asked = 0;
    for (u32 s = 0; s < w->n_slots && asked < 10; s++)
        if ((w->flags[s] & AZ_F_ALIVE) && az_birth_request(w, s, AZ_NONE, 1, 0, 0, 0, 0)) asked++;
    const u64 births0 = w->totals.births, denied0 = w->totals.cap_denied;
    az_life_commit(w);
    CHECK_EQ(killed, 10);
    CHECK_EQ(asked, 10);
    CHECK_EQ(w->totals.births - births0, 0);       /* 10 slots are dead, yet every birth is refused */
    CHECK_EQ(w->totals.cap_denied - denied0, 10);
    az_grid_build(w);                              /* the dead slots join the free list here */
    const u32 m = first_alive(w, 0);
    CHECK(m != AZ_NONE && az_birth_request(w, m, AZ_NONE, 1, 0, 0, 0, 0));
    az_life_commit(w);
    CHECK_EQ(w->totals.births - births0, 1);
    az_tally_merge(w);
    az_ledger_flush(w);
    az_test_world_free(&t);
}

static void rebase_then_tick(void)
{
    AzConfig cfg = az_test_config(7, 3000);
    AzTestWorld a, b;
    if (!az_test_world(&a, &cfg, 0) || !az_test_world(&b, &cfg, 0)) { CHECK(!"world creation"); return; }
    az_tick(a.w);
    az_tick(b.w);
    az_world_rebase(a.w, 0);                       /* same base: every column stays valid */
    CHECK(a.w->k.brain_step != NULL);
    if (a.w->k.brain_step) {                       /* the old code crashed in the next tick */
        az_tick(a.w);
        az_tick(b.w);
        CHECK_EQ(az_state_hash(a.w), az_state_hash(b.w));
    }
    az_test_world_free(&a);
    az_test_world_free(&b);
}

static void pending_acc_is_hashed(void)
{
    AzConfig cfg = az_test_config(7, 3000);
    cfg.cap = 3000 + 192;                       /* binding: births are refused and refunded */
    AzTestWorld t;
    if (!az_test_world(&t, &cfg, 0)) { CHECK(!"world creation"); return; }
    AzWorld *w = t.w;
    u32 c = AZ_NONE;
    for (u32 i = 0; i < 200 && c == AZ_NONE; i++) {
        az_tick(w);
        for (u32 k = 0; k < w->n_cells; k++)
            if (w->c_acc[k].dheat || w->c_acc[k].delta[AZ_SP_ORG_Y]) { c = k; break; }
    }
    CHECK(c != AZ_NONE);
    if (c != AZ_NONE) {
        const AzCellAcc keep = w->c_acc[c];
        const u64 h = az_state_hash(w);
        memset(&w->c_acc[c], 0, sizeof keep);
        CHECK(az_state_hash(w) != h);           /* the pending refund is part of the state */
        w->c_acc[c] = keep;
        CHECK_EQ(az_state_hash(w), h);
        w->c_acc[c].dmark[0] ^= 1u;             /* every member counts, the marks too */
        CHECK(az_state_hash(w) != h);
        w->c_acc[c] = keep;
        printf("pending acc at tick %" PRIu64 ", cell %u: org %d heat %d; hash %016" PRIx64 "\n",
               w->tick, c, keep.delta[AZ_SP_ORG_Y], keep.dheat, h);
    }
    az_test_world_free(&t);
}

static u32 pf_calls;   /* parallel_for calls seen by the one-worker host below */

static void counting_pf(void *ctx, AzWorld *w, AzJobFn fn, const u32 *splits, u32 n_parts)
{
    (void)ctx;
    pf_calls++;
    for (u32 k = 0; k < n_parts; k++) fn(w, splits[k], splits[k + 1], 0);
}

/* 14. A one-worker host runs the serial path (finding v51): its parallel_for
 *     is never called, and with debug_split_seed the world hashes like the
 *     same run with host == NULL. */
static void one_worker_host_is_serial(void)
{
    AzConfig cfg = az_test_config(9, 2000);
    cfg.debug_split_seed = 77;
    AzHost host;
    memset(&host, 0, sizeof host);
    host.n_workers = 1;
    host.max_parts = 1024;
    host.parallel_for = counting_pf;
    host.now_ns = zero_ns;
    const u64 bytes = az_world_bytes(&cfg);
    void *block = az_test_locked_block(bytes);
    AzTestWorld ref;
    if (!block || !az_test_world(&ref, &cfg, 0)) { CHECK(!"world creation"); az_test_locked_block_free(block); return; }
    AzArena arena;
    arena.base = (u8 *)block; arena.size = bytes; arena.used = 0;
    AzWorld *w = az_world_create(&arena, &cfg, &host);
    CHECK(w != NULL);
    if (w) {
        az_worldgen(w);
        pf_calls = 0;
        for (u32 t = 0; t < 12; t++) { az_tick(w); az_tick(ref.w); }
        CHECK_EQ(pf_calls, 0);
        CHECK_EQ(az_state_hash(w), az_state_hash(ref.w));
        printf("one-worker host: %u parallel_for calls in 12 ticks; hash %016" PRIx64 "\n", pf_calls,
               az_state_hash(w));
    }
    az_test_world_free(&ref);
    az_test_locked_block_free(block);
}

/* 15. P4-CORE-02: a refused world creation restores arena->used so a smaller valid retry succeeds. */
static void failed_create_restores_arena_cursor(void)
{
    AzConfig small_cfg = az_test_config(9, 256);
    const u64 small_bytes = az_world_bytes(&small_cfg);
    const u64 pad = 4096u;
    void *block = az_test_locked_block(small_bytes + pad);
    if (!block) { CHECK(!"locked block"); return; }

    AzConfig big_cfg = az_test_config(9, 100000);
    big_cfg.cap = 200000;
    AzArena arena;
    arena.base = (u8 *)block;
    arena.size = small_bytes + pad;
    arena.used = pad;

    AzWorld *bad = az_world_create(&arena, &big_cfg, NULL);
    CHECK(bad == NULL);
    CHECK_EQ(arena.used, pad);

    AzWorld *ok = az_world_create(&arena, &small_cfg, NULL);
    CHECK(ok != NULL);
    if (ok) {
        az_worldgen(ok);
        az_tick(ok);
        CHECK_EQ(ok->totals.field_underflow, 0);
    }
    az_test_locked_block_free(block);
}

/* 16. P4-CORE-03: reusing a dead slot that still owns a store block releases its units cleanly. */
static void reused_slot_releases_store_block(void)
{
    AzConfig cfg = az_test_config(5, 64);
    AzTestWorld t;
    if (!az_test_world(&t, &cfg, 0)) { CHECK(!"world creation"); return; }
    AzWorld *w = t.w;
    const u32 s0 = 0;
    CHECK(w->blk[s0] != 0);
    const u32 live_before = w->store_live;
    const u32 old_blk = w->blk[s0];
    w->flags[s0] = 0;
    w->spos[s0] = w->free_head;
    w->free_head = s0;
    const u32 s_new = az_spawn(w);
    CHECK_EQ(s_new, s0);
    CHECK_EQ(w->blk[s_new], 0u);
    CHECK(old_blk != 0u && w->store_live < live_before);
    az_test_world_free(&t);
}

int main(int argc, char **argv)
{
    /* --quick: the invariant checks once each, without the slow thread,
     * reorder and split variants (owners' request; the full run is the gate) */
    const bool quick = argc > 1 && !strcmp(argv[1], "--quick");

    /* 1. known answers from the Random123 distribution (kat_vectors) */
    {
        u32 out[4];
        const u32 c0[4] = { 0, 0, 0, 0 }, k0[2] = { 0, 0 };
        az_philox4x32(out, c0, k0);
        CHECK_EQ(out[0], 0x6627e8d5u); CHECK_EQ(out[1], 0xe169c58du);
        CHECK_EQ(out[2], 0xbc57ac4cu); CHECK_EQ(out[3], 0x9b00dbd8u);
        const u32 c1[4] = { 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu }, k1[2] = { 0xffffffffu, 0xffffffffu };
        az_philox4x32(out, c1, k1);
        CHECK_EQ(out[0], 0x408f276du); CHECK_EQ(out[1], 0x41c83b0eu);
        CHECK_EQ(out[2], 0xa20bc7c6u); CHECK_EQ(out[3], 0x6d5451fdu);
        const u32 c2[4] = { 0x243f6a88u, 0x85a308d3u, 0x13198a2eu, 0x03707344u }, k2[2] = { 0xa4093822u, 0x299f31d0u };
        az_philox4x32(out, c2, k2);
        CHECK_EQ(out[0], 0xd16cfe09u); CHECK_EQ(out[1], 0x94fdccebu);
        CHECK_EQ(out[2], 0x5001e420u); CHECK_EQ(out[3], 0x24126ea1u);
    }

    /* loose capacity: nothing is refused */
    const u32 N = 3000, T = 80;
    RunOut a = run(7, N, 0, T, 0, 1, 0, 1);
    printf("loose cap   serial      %016" PRIx64 "  births %" PRIu64 " denied %" PRIu64 " live %u\n",
           a.hash, a.births, a.cap_denied, a.live);
    CHECK(a.births > 0);
    CHECK_EQ(a.cap_denied, 0);
    if (quick) {                                             /* one variant of each kind    */
        CHECK(a.hash == run(7, N, 0, T, 5, 7, 4242, 0).hash);
        CHECK(a.hash != run(8, N, 0, T, 0, 1, 0, 0).hash);
    } else {
    CHECK(a.hash == run(7, N, 0, T, 0, 1, 0, 0).hash);       /* same run twice             */
    CHECK(a.hash == run(7, N, 0, T, 2, 1, 0, 0).hash);       /* thread-count independence  */
    CHECK(a.hash == run(7, N, 0, T, 5, 1, 0, 0).hash);
    CHECK(a.hash == run(7, N, 0, T, 12, 1, 0, 0).hash);
    CHECK(a.hash == run(7, N, 0, T, 0, 7, 0, 0).hash);       /* reorder-period independence */
    CHECK(a.hash == run(7, N, 0, T, 3, 31, 0, 0).hash);
    CHECK(a.hash == run(7, N, 0, T, 0, 1, 12345, 0).hash);   /* split independence, serial  */
    CHECK(a.hash == run(7, N, 0, T, 0, 9, 777, 0).hash);
    CHECK(a.hash == run(7, N, 0, T, 6, 5, 4242, 0).hash);    /* split independence, threads */
    CHECK(a.hash != run(8, N, 0, T, 0, 1, 0, 0).hash);       /* the hash sees the seed      */
    }

    /* binding capacity: births are refused; the outcome must still not depend
     * on how often dead slots are compacted or on the thread count */
    const u32 CAP = N + 192;
    RunOut b = run(7, N, CAP, T, 0, 1, 0, 1);
    printf("binding cap serial      %016" PRIx64 "  births %" PRIu64 " denied %" PRIu64 " live %u\n",
           b.hash, b.births, b.cap_denied, b.live);
    CHECK(b.cap_denied > 0);
    CHECK(b.hash != a.hash);
    CHECK(b.hash == run(7, N, CAP, T, 4, 13, 99, 0).hash);
    if (!quick) {
        CHECK(b.hash == run(7, N, CAP, T, 0, 7, 0, 0).hash);
        CHECK(b.hash == run(7, N, CAP, T, 0, 31, 0, 0).hash);
        CHECK(b.hash == run(7, N, CAP, T, 0, 255, 0, 0).hash);
        CHECK(b.hash == run(7, N, CAP, T, 5, 3, 0, 0).hash);
    }

    pending_acc_is_hashed();
    arena_is_used();
    birth_request_range();
    colony_pending_is_kept();
    host_too_large();
    dead_slots_wait_for_grid();
    one_worker_host_is_serial();
    rebase_then_tick();
    failed_create_restores_arena_cursor();
    reused_slot_releases_store_block();
    if (quick) printf("quick mode: one thread/reorder/split variant per capacity case (the full run is the gate)\n");
    return az_test_done();
}
