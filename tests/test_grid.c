/* test_grid.c - az_grid.c against the reference stub, byte for byte.
 *
 * The reference (core/stubs/az_grid.c) is compiled into this file under the
 * names ref_grid_build / ref_grid_reorder. Two worlds get the same scrambled
 * state; one runs the reference serially, the other runs the module under
 * test with 0, 3 or 8 worker threads; then every entity column (all tiers,
 * full capacity), cell_start[], order[], pub[] and the scalars are compared
 * with memcmp.
 *
 *   test_grid                      correctness (this is what ctest runs)
 *   test_grid v                    the same, printing every case and its time
 *   test_grid bench [n] [threads]  timings of build and reorder
 *                                  (noisy, parallel load)
 *
 * Worker pools are created unpinned here: a pinned pool puts worker k of
 * every test process on the same core, and when several test programs run
 * at once a parallel region then waits for whole scheduler slices.
 */
#include "az_test.h"
#include <time.h>

void ref_grid_build(AzWorld *w);
void ref_grid_reorder(AzWorld *w);
#define az_grid_build   ref_grid_build
#define az_grid_reorder ref_grid_reorder
#include "../core/stubs/az_grid.c"
#undef az_grid_build
#undef az_grid_reorder

/* ---------------------------------------------------------------------- */
/* deterministic test randomness (not the simulation RNG) and a clock      */
/* ---------------------------------------------------------------------- */
static u64 rng_state;
static u64 rnd64(void) { return az_test_rand64(&rng_state); }
static u32 rnd_below(u32 n) { return (u32)(((rnd64() >> 32) * (u64)n) >> 32); }

static void rnd_fill(void *dst, size_t n)
{
    u8 *d = (u8 *)dst;
    while (n >= 8) { u64 v = rnd64(); memcpy(d, &v, 8); d += 8; n -= 8; }
    if (n) { u64 v = rnd64(); memcpy(d, &v, n); }
}

static u64 now_ns(void)
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (u64)ts.tv_sec * 1000000000ull + (u64)ts.tv_nsec;
}

static bool verbose;    /* "test_grid v" */

/* ---------------------------------------------------------------------- */
/* worlds                                                                  */
/* ---------------------------------------------------------------------- */
/* az_test_world_ex with an unpinned pool; worldgen is optional */
static bool make_world(AzTestWorld *t, const AzConfig *cfg, u32 threads, bool worldgen)
{
    if (!az_world_bytes(cfg)) { memset(t, 0, sizeof *t); return false; }
    return az_test_world_ex(t, cfg, threads, AZ_TEST_UNPINNED | (worldgen ? 0u : AZ_TEST_NO_WORLDGEN));
}

typedef struct Shape {
    u32 cap;
    u8  gx, gy, gz;
} Shape;

/* a world for the grid alone: columns and geometry, no fields, no founders */
static bool make_grid_world(AzTestWorld *t, const Shape *sh, u32 threads, u32 split_seed)
{
    AzConfig cfg = az_test_config(77, 16);
    cfg.cap = sh->cap;
    cfg.gx_bits = sh->gx; cfg.gy_bits = sh->gy; cfg.gz_bits = sh->gz;
    cfg.debug_split_seed = split_seed;
    return make_world(t, &cfg, threads, false);
}

enum { M_UNIFORM = 0, M_DUP_UID, M_ALL_DEAD, M_ALL_ALIVE };

/* Give world `w` n_slots slots of arbitrary state: every byte of every
 * column random (stale scratch included), then the fields the grid reads
 * made meaningful: about one slot in eight dead, uids a random permutation
 * of 1..n_slots (or, for M_DUP_UID, drawn from a small range so that many
 * are equal), `crowd` alive slots placed in one cell. */
static void scramble(AzWorld *w, u32 n_slots, u32 mode, u32 crowd, u64 seed)
{
    rng_state = seed * 0x100000001B3ull + 12345;
    const u32 ncol = az_n_columns();
    for (u32 k = 0; k < ncol; k++) {
        AzColDesc col;
        az_column(w, k, &col);
        rnd_fill(col.data, (size_t)n_slots * col.elem * col.per);
    }
    w->n_slots = n_slots;
    /* these worlds have no creature blocks: the handle column must say so
     * (a random handle would point outside the store) */
    for (u32 s = 0; s < w->cap; s++) w->blk[s] = 0;
    w->tick = (u32)(rnd64() & 0xFFFF);
    w->role_key = rnd64();
    w->free_head = AZ_NONE;

    for (u32 s = 0; s < n_slots; s++) w->uid[s] = s + 1;
    for (u32 s = n_slots; s > 1; s--) {             /* Fisher-Yates */
        u32 j = rnd_below(s);
        u32 t = w->uid[s - 1]; w->uid[s - 1] = w->uid[j]; w->uid[j] = t;
    }
    if (mode == M_DUP_UID)
        for (u32 s = 0; s < n_slots; s++) w->uid[s] = 1 + rnd_below(n_slots / 8 + 2);

    for (u32 s = 0; s < n_slots; s++) {
        u8 fl = w->flags[s];
        if (mode == M_ALL_DEAD) fl &= (u8)~AZ_F_ALIVE;
        else if (mode == M_ALL_ALIVE) fl |= AZ_F_ALIVE;
        else if (rnd_below(8) == 0) fl &= (u8)~AZ_F_ALIVE;
        else fl |= AZ_F_ALIVE;
        w->flags[s] = fl;
    }

    /* crowd: `crowd` alive slots, chosen at random, share one cell */
    if (crowd) {
        const u32 cx = rnd_below(w->gx), cy = rnd_below(w->gy), cz = rnd_below(w->gz);
        u32 placed = 0;
        for (u32 tries = 0; placed < crowd && tries < 64u * n_slots; tries++) {
            u32 s = rnd_below(n_slots);
            if (!(w->flags[s] & AZ_F_ALIVE)) continue;
            u32 x = (cx << w->shift_x) | (w->pos_x[s] & ((1u << w->shift_x) - 1u));
            u32 y = (cy << w->shift_y) | (w->pos_y[s] & ((1u << w->shift_y) - 1u));
            u32 z = (cz << w->shift_z) | (w->pos_z[s] & ((1u << w->shift_z) - 1u));
            if (w->pos_x[s] == x && w->pos_y[s] == y && w->pos_z[s] == z) continue;  /* already there */
            w->pos_x[s] = x; w->pos_y[s] = y; w->pos_z[s] = z;
            placed++;
        }
    }
}

/* copy the whole entity state and the grid outputs of `a` into `b` */
static void copy_world(AzWorld *b, const AzWorld *a)
{
    const u32 ncol = az_n_columns();
    for (u32 k = 0; k < ncol; k++) {
        AzColDesc ca, cb;
        az_column(a, k, &ca);
        az_column(b, k, &cb);
        memcpy(cb.data, ca.data, (size_t)a->cap * ca.elem * ca.per);
    }
    memcpy(b->cell_start, a->cell_start, ((size_t)a->n_cells + 1) * sizeof(u32));
    memcpy(b->order, a->order, (size_t)a->cap * sizeof(u32));
    memcpy(b->pub, a->pub, (size_t)a->cap * sizeof(AzPub));
    memcpy(b->pub_id, a->pub_id, (size_t)a->cap * sizeof(AzPubId));
    b->n_slots = a->n_slots; b->n_live = a->n_live; b->n_sorted = a->n_sorted;
    b->free_head = a->free_head; b->tick = a->tick; b->uid_next = a->uid_next;
    b->role_key = a->role_key;
}

/* number of differing outputs between the reference world and the other */
static u32 world_diff(const AzWorld *a, const AzWorld *b, bool print)
{
    u32 bad = 0;
    const u32 ncol = az_n_columns();
    for (u32 k = 0; k < ncol; k++) {
        AzColDesc ca, cb;
        az_column(a, k, &ca);
        az_column(b, k, &cb);
        if (memcmp(ca.data, cb.data, (size_t)a->cap * ca.elem * ca.per) != 0) {
            bad++;
            if (print) printf("  column %s differs\n", ca.name);
        }
    }
    if (memcmp(a->cell_start, b->cell_start, ((size_t)a->n_cells + 1) * sizeof(u32)) != 0) {
        bad++; if (print) printf("  cell_start differs\n");
    }
    if (memcmp(a->order, b->order, (size_t)a->cap * sizeof(u32)) != 0) {
        bad++; if (print) printf("  order differs\n");
    }
    if (memcmp(a->pub, b->pub, (size_t)a->cap * sizeof(AzPub)) != 0 ||
        memcmp(a->pub_id, b->pub_id, (size_t)a->cap * sizeof(AzPubId)) != 0) {
        bad++; if (print) printf("  pub differs\n");
    }
    if (a->n_slots != b->n_slots)     { bad++; if (print) printf("  n_slots %u vs %u\n", a->n_slots, b->n_slots); }
    if (a->n_live != b->n_live)       { bad++; if (print) printf("  n_live %u vs %u\n", a->n_live, b->n_live); }
    if (a->n_sorted != b->n_sorted)   { bad++; if (print) printf("  n_sorted %u vs %u\n", a->n_sorted, b->n_sorted); }
    if (a->free_head != b->free_head) { bad++; if (print) printf("  free_head %u vs %u\n", a->free_head, b->free_head); }
    return bad;
}

/* properties of a built grid, checked without the reference */
static void check_grid_invariants(const AzWorld *w)
{
    u32 bad_perm = 0, bad_cell = 0, bad_uid = 0, alive = 0, dead = 0;
    for (u32 s = 0; s < w->n_slots; s++) {
        if (w->flags[s] & AZ_F_ALIVE) {
            alive++;
            u32 p = w->spos[s];
            if (p >= w->n_live || w->order[p] != s) bad_perm++;
            if (w->cell[s] != az_cell_at(w, w->pos_x[s], w->pos_y[s], w->pos_z[s])) bad_cell++;
            if (!(w->flags[s] & AZ_F_INIT) != !az_is_initiator(w, w->uid[s])) bad_uid++;
        } else {
            dead++;
            if (w->cell[s] != AZ_NONE) bad_cell++;
        }
    }
    for (u32 c = 0; c < w->n_cells; c++) {
        if (w->cell_start[c] > w->cell_start[c + 1]) { bad_cell++; continue; }
        for (u32 p = w->cell_start[c]; p < w->cell_start[c + 1]; p++) {
            u32 s = w->order[p];
            if (w->cell[s] != c) bad_cell++;
            if (w->pub_id[p].uid != w->uid[s]) bad_uid++;
            if (p > w->cell_start[c] && w->uid[w->order[p - 1]] > w->uid[s]) bad_uid++;
        }
    }
    /* the free list: every dead slot once, from the highest down */
    u32 chained = 0, prev = AZ_NONE, bad_chain = 0;
    for (u32 s = w->free_head; s != AZ_NONE && chained <= w->n_slots; s = w->spos[s]) {
        if (s >= w->n_slots || (w->flags[s] & AZ_F_ALIVE)) { bad_chain++; break; }
        if (prev != AZ_NONE && s >= prev) bad_chain++;
        prev = s;
        chained++;
    }
    CHECK_EQ(bad_perm, 0);
    CHECK_EQ(bad_cell, 0);
    CHECK_EQ(bad_uid, 0);
    CHECK_EQ(bad_chain, 0);
    CHECK_EQ(chained, dead);
    CHECK_EQ(w->n_live, alive);
    CHECK_EQ(w->n_sorted, alive);
    CHECK_EQ(w->cell_start[0], 0);
    CHECK_EQ(w->cell_start[w->n_cells], alive);
}

static void expect_same(const AzWorld *a, const AzWorld *b, const char *what, const char *label)
{
    u32 bad = world_diff(a, b, false);
    CHECK_EQ(bad, 0);
    if (bad) { printf("  ^ %s, %s\n", label, what); (void)world_diff(a, b, true); }
}

/* small moves, some deaths and some newborns in appended slots: what a tick
 * does to the grid's inputs between two builds */
static void disturb(AzWorld *w, u64 seed)
{
    rng_state = seed ^ 0xD1B54A32D192ED03ull;
    const u32 n = w->n_slots;
    for (u32 s = 0; s < n; s++) {
        u32 r = (u32)rnd64();
        if ((r & 3u) == 0) {
            w->pos_x[s] += (u32)((i32)(rnd64() & 0x3FFFFFFu) - 0x2000000);
            w->pos_y[s] += (u32)((i32)(rnd64() & 0x3FFFFFFu) - 0x2000000);
            w->pos_z[s] ^= (u32)(rnd64() & 0x1FFFFFFu);
        }
        if (((r >> 8) & 31u) == 0) w->flags[s] &= (u8)~AZ_F_ALIVE;
        w->vel_x[s] = (i16)rnd64();
        w->mass[s] = (u16)rnd64();
    }
    u32 born = az_min_u32(w->cap - n, n / 16 + 3);
    const u32 ncol = az_n_columns();
    for (u32 k = 0; k < ncol; k++) {
        AzColDesc col;
        az_column(w, k, &col);
        size_t bytes = (size_t)col.elem * col.per;
        rnd_fill((u8 *)col.data + (size_t)n * bytes, (size_t)born * bytes);
    }
    for (u32 s = n; s < n + born; s++) {
        w->blk[s] = 0;                                  /* no creature block (see scramble) */
        w->flags[s] |= AZ_F_ALIVE;
        w->uid[s] = 0x40000000u + s + (u32)(seed & 0xFFFFu) * 0x10000u;
    }
    w->n_slots = n + born;
    w->tick++;
}

/* ---------------------------------------------------------------------- */
/* the differential test                                                   */
/* ---------------------------------------------------------------------- */
/* a = reference world (serial), b = world under test; same shape */
static void differential(AzWorld *a, AzWorld *b, u32 n_slots, u32 mode, u32 crowd, u64 seed)
{
    char label[200];
    snprintf(label, sizeof label, "cap %u cells %u slots %u mode %u crowd %u workers %u%s split_seed %u",
             b->cap, b->n_cells, n_slots, mode, crowd, b->n_workers, b->host ? "" : " (no host)",
             b->cfg.debug_split_seed);
    const u64 t_start = now_ns();
    CHECK(n_slots <= a->cap && a->cap == b->cap && a->n_cells == b->n_cells);

    scramble(a, n_slots, mode, crowd, seed);
    copy_world(b, a);
    CHECK_EQ(world_diff(a, b, true), 0);

    /* 1. build on arbitrary slots */
    ref_grid_build(a);
    az_grid_build(b);
    expect_same(a, b, "first build", label);
    check_grid_invariants(b);
    if (crowd) {                                    /* the crowd really is one cell */
        u32 biggest = 0;
        for (u32 c = 0; c < b->n_cells; c++)
            biggest = az_max_u32(biggest, b->cell_start[c + 1] - b->cell_start[c]);
        CHECK(biggest >= crowd);
    }

    /* the comparison can fail: one flipped bit in each kind of output is seen */
    if (b->n_live > 0) {
        b->pub_id[b->n_live - 1].partner ^= 0x00010000u;
        CHECK(world_diff(a, b, false) == 1);
        b->pub_id[b->n_live - 1].partner ^= 0x00010000u;
        b->order[b->n_live / 2] ^= 1u;
        CHECK(world_diff(a, b, false) == 1);
        b->order[b->n_live / 2] ^= 1u;
        b->cell_start[b->n_cells / 2] ^= 1u;
        CHECK(world_diff(a, b, false) == 1);
        b->cell_start[b->n_cells / 2] ^= 1u;
        b->spos[b->n_slots - 1] ^= 4u;
        CHECK(world_diff(a, b, false) == 1);
        b->spos[b->n_slots - 1] ^= 4u;
    }
    CHECK_EQ(world_diff(a, b, false), 0);

    /* 2. reorder, 3. build: slots are now sorted positions */
    for (u32 round = 0; round < 2; round++) {
        ref_grid_reorder(a);
        az_grid_reorder(b);
        expect_same(a, b, round ? "second reorder" : "reorder", label);
        CHECK_EQ(b->n_slots, b->n_live);
        CHECK_EQ(b->free_head, AZ_NONE);

        ref_grid_build(a);
        az_grid_build(b);
        expect_same(a, b, "build after reorder", label);
        u32 not_identity = 0;
        for (u32 p = 0; p < b->n_live; p++) not_identity += (b->order[p] != p) + (b->spos[p] != p);
        CHECK_EQ(not_identity, 0);
        check_grid_invariants(b);

        /* 4. what a tick does: moves, deaths, births; then build again */
        disturb(a, seed + round);
        disturb(b, seed + round);
        CHECK_EQ(world_diff(a, b, true), 0);
        ref_grid_build(a);
        az_grid_build(b);
        expect_same(a, b, "build after disturbance", label);
        check_grid_invariants(b);
    }
    if (verbose) printf("%s: %.1f ms\n", label, (double)(now_ns() - t_start) / 1e6);
}

typedef struct Case { u32 n_slots, mode, crowd; } Case;

static void run_shape(const Shape *sh, const Case *cases, u32 n_cases, u32 threads, u32 split_seed)
{
    AzTestWorld ta, tb = {0};
    bool ok = make_grid_world(&ta, sh, 0, 0) && make_grid_world(&tb, sh, threads, split_seed);
    CHECK(ok);
    if (ok) {
        for (u32 i = 0; i < n_cases; i++)
            differential(ta.w, tb.w, cases[i].n_slots, cases[i].mode, cases[i].crowd,
                         1000u * sh->cap + 10u * i + threads);
    } else {
        printf("  ^ could not create a world: cap %u threads %u\n", sh->cap, threads);
    }
    az_test_world_free(&ta);
    az_test_world_free(&tb);
}

/* ---------------------------------------------------------------------- */
/* whole ticks: state hash against thread count and reorder period         */
/* ---------------------------------------------------------------------- */
static u64 run_ticks(u32 n, u32 threads, u32 reorder_period, u32 split_seed, u32 ticks, bool check)
{
    AzTestWorld t;
    AzConfig cfg = az_test_config(53, n);  /* a tuning seed: 5001.. are held out */
    cfg.reorder_period = (u8)reorder_period;
    cfg.debug_split_seed = split_seed;
    const u64 t_start = now_ns();
    if (!make_world(&t, &cfg, threads, true)) { CHECK(false); az_test_world_free(&t); return 0; }
    const u64 m0 = az_total_matter(t.w);
    const i64 e0 = az_total_energy(t.w);
    for (u32 i = 0; i < ticks; i++) {
        az_tick(t.w);
        if (check) {
            CHECK_EQ(az_total_matter(t.w), m0);
            CHECK_EQ(az_total_energy(t.w), e0);
        }
    }
    if (check) {
        CHECK(t.w->n_live > 0);
        CHECK_EQ(t.w->totals.field_underflow, 0);
    }
    u64 h = az_state_hash(t.w);
    if (verbose)
        printf("ticks: n %u threads %u reorder %u split_seed %u live %u hash %016" PRIx64 ": %.1f ms\n",
               n, threads, reorder_period, split_seed, t.w->n_live, h, (double)(now_ns() - t_start) / 1e6);
    az_test_world_free(&t);
    return h;
}

/* ---------------------------------------------------------------------- */
/* timings                                                                 */
/* ---------------------------------------------------------------------- */
/* per-tick drift in x and y: up to 1/16 of a cell of the default grid */
static void drift(AzWorld *w, u64 seed)
{
    rng_state = seed;
    for (u32 s = 0; s < w->n_slots; s++) {
        u64 r = rnd64();
        w->pos_x[s] += (u32)(r & 0x3FFFFFu) - 0x200000u;
        w->pos_y[s] += (u32)((r >> 22) & 0x3FFFFFu) - 0x200000u;
    }
    w->tick++;
}

static void sort_u64(u64 *v, u32 n)
{
    for (u32 i = 1; i < n; i++) {
        u64 key = v[i]; u32 j = i;
        while (j > 0 && v[j - 1] > key) { v[j] = v[j - 1]; j--; }
        v[j] = key;
    }
}

static void bench(u32 n, u32 threads)
{
    enum { REPS = 9 };
    Shape sh = { n + 4096, 6, 6, 5 };
    AzTestWorld ta, tb = {0};
    if (!make_grid_world(&ta, &sh, 0, 0) || !make_grid_world(&tb, &sh, threads, 0)) {
        CHECK(!"could not create both benchmark worlds");
        az_test_world_free(&tb);
        az_test_world_free(&ta);
        printf("bench: could not create worlds for n = %u\n", n);
        return;
    }
    AzWorld *a = ta.w, *b = tb.w;
    scramble(a, n, M_UNIFORM, 0, 4242);
    copy_world(b, a);

    u64 t0, ref_ns[REPS], new_ns[REPS];
    t0 = now_ns(); ref_grid_build(a); u64 ref_cold = now_ns() - t0;
    t0 = now_ns(); az_grid_build(b);  u64 new_cold = now_ns() - t0;
    u32 bad = world_diff(a, b, false);
    t0 = now_ns(); ref_grid_reorder(a); u64 ref_rcold = now_ns() - t0;
    t0 = now_ns(); az_grid_reorder(b);  u64 new_rcold = now_ns() - t0;
    bad += world_diff(a, b, false);
    printf("n = %u slots, %u alive, %u cells, %u workers (noisy, parallel load)\n",
           n, b->n_live, b->n_cells, b->n_workers);
    printf("  ms, reference stub (serial) vs az_grid.c;  median / fastest of %d\n", REPS);
    printf("  shuffled slots, once : build    ref %8.3f              new %8.3f\n",
           (double)ref_cold / 1e6, (double)new_cold / 1e6);
    printf("  shuffled slots, once : reorder  ref %8.3f              new %8.3f\n",
           (double)ref_rcold / 1e6, (double)new_rcold / 1e6);

    for (u32 i = 0; i < REPS; i++) {                /* build, one tick of drift after a reorder */
        drift(a, 900 + i); drift(b, 900 + i);
        t0 = now_ns(); ref_grid_build(a); ref_ns[i] = now_ns() - t0;
        t0 = now_ns(); az_grid_build(b);  new_ns[i] = now_ns() - t0;
        bad += world_diff(a, b, false);
        ref_grid_reorder(a); az_grid_reorder(b);
    }
    sort_u64(ref_ns, REPS); sort_u64(new_ns, REPS);
    printf("  one tick after a reorder: build    ref %8.3f / %8.3f   new %8.3f / %8.3f\n",
           (double)ref_ns[REPS / 2] / 1e6, (double)ref_ns[0] / 1e6,
           (double)new_ns[REPS / 2] / 1e6, (double)new_ns[0] / 1e6);
    for (u32 i = 0; i < REPS; i++) {                /* reorder, one tick of drift after the last */
        drift(a, 1900 + i); drift(b, 1900 + i);
        t0 = now_ns(); ref_grid_reorder(a); ref_ns[i] = now_ns() - t0;
        t0 = now_ns(); az_grid_reorder(b);  new_ns[i] = now_ns() - t0;
        bad += world_diff(a, b, false);
    }
    sort_u64(ref_ns, REPS); sort_u64(new_ns, REPS);
    printf("  reorder every tick      : reorder  ref %8.3f / %8.3f   new %8.3f / %8.3f   (with its build)\n",
           (double)ref_ns[REPS / 2] / 1e6, (double)ref_ns[0] / 1e6,
           (double)new_ns[REPS / 2] / 1e6, (double)new_ns[0] / 1e6);
    printf("  outputs equal to the reference after every step: %s\n", bad ? "NO" : "yes");
    CHECK_EQ(bad, 0);
    az_test_world_free(&ta);
    az_test_world_free(&tb);
}

/* ---------------------------------------------------------------------- */
/* Each build replaces the published stake, including a slot without history.
 * Distinct private values and a prior nonzero public record reveal stale data. */
static void check_published_stake(void)
{
    AzConfig cfg = az_test_config(77, 0);
    cfg.cap = 16;
    AzArena arena = {0};
    AzWinInfo info = {0};
    bool locked = az_win_arena(&arena, az_world_bytes(&cfg), AZ_WIN_LOCK, &info);
    CHECK(locked && info.arena_locked);
    if (!locked) return;
    AzWorld *w = az_world_create(&arena, &cfg, NULL);
    CHECK(w != NULL);
    if (!w) { az_win_arena_free(&arena); return; }

    enum { COUNT = 3 };
    u32 lengths[AZ_SEG_COUNT] = {0};
    lengths[AZ_SEG_MIND] = AZ_SENS + 16u + sizeof(AzSenseMemory);
    w->n_slots = COUNT;
    for (u32 slot = 0; slot < COUNT; slot++) {
        w->flags[slot] = AZ_F_ALIVE;
        w->uid[slot] = COUNT - slot;
    }
    /* The third creature has no private block. */
    if (!az_blk_new(w, 0, lengths) || !az_blk_new(w, 1, lengths)) {
        CHECK(false);
        az_win_arena_free(&arena);
        return;
    }
    AzSenseMemory *first = az_sense_memory(w, 0);
    AzSenseMemory *second = az_sense_memory(w, 1);
    CHECK(first && second && !az_sense_memory(w, 2));
    if (!first || !second) { az_win_arena_free(&arena); return; }
    void (*const build[])(AzWorld *) = {ref_grid_build, az_grid_build};
    const u8 expected[][COUNT] = {{23, 91, 0}, {0, 17, 0}};
    for (u32 version = 0; version < AZ_LEN(build); version++) {
        for (u32 pass = 0; pass < AZ_LEN(expected); pass++) {
            first->stake = expected[pass][0];
            second->stake = expected[pass][1];
            for (u32 i = 0; i < COUNT; i++) w->pub[i].stake = 165;
            build[version](w);
            CHECK_EQ(w->n_live, COUNT);
            for (u32 slot = 0; slot < COUNT; slot++) {
                u32 position = w->spos[slot];
                CHECK(position < COUNT);
                if (position >= COUNT) continue;
                CHECK_EQ(w->pub_id[position].uid, w->uid[slot]);
                CHECK_EQ(w->pub[position].stake, expected[pass][slot]);
            }
        }
    }
    az_win_arena_free(&arena);
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc > 1 && strcmp(argv[1], "bench") == 0) {
        u32 n = argc > 2 ? (u32)strtoul(argv[2], NULL, 10) : 100000;
        u32 threads = argc > 3 ? (u32)strtoul(argv[3], NULL, 10) : 8;
        bench(n, threads);
        return az_test_done();
    }
    check_published_stake();
    verbose = argc > 1 && strcmp(argv[1], "v") == 0;

    static const Shape small  = { 4096,   4, 4, 3 };    /* 2048 cells   */
    static const Shape tiny   = { 8192,   2, 2, 1 };    /* 32 cells     */
    static const Shape medium = { 40000,  4, 4, 3 };
    static const Shape sparse = { 70016,  6, 6, 5 };    /* 131072 cells */
    static const Shape large  = { 200000, 5, 5, 4 };    /* 16384 cells  */
    static const Case small_cases[] = {
        { 0, M_UNIFORM, 0 }, { 1, M_UNIFORM, 0 }, { 700, M_UNIFORM, 0 },
        { 3000, M_UNIFORM, 300 },                       /* one cell holds 300          */
        { 2500, M_ALL_DEAD, 0 }, { 33, M_ALL_ALIVE, 0 },
    };
    static const Case tiny_cases[] = {
        { 6000, M_UNIFORM, 5000 },                      /* one cell holds 5000         */
        { 6000, M_DUP_UID, 0 },                         /* equal uids: stable order    */
    };
    static const Case medium_cases[] = {
        { 30000, M_UNIFORM, 0 }, { 30000, M_UNIFORM, 300 }, { 30000, M_UNIFORM, 5000 },
        { 30000, M_DUP_UID, 5000 }, { 36000, M_ALL_ALIVE, 0 }, { 20000, M_ALL_DEAD, 0 },
    };
    static const Case sparse_cases[] = { { 60000, M_UNIFORM, 300 }, { 9000, M_UNIFORM, 5000 } };
    static const Case large_cases[]  = { { 180000, M_UNIFORM, 5000 }, { 150000, M_UNIFORM, 300 } };

    /* threads x split seed. debug_split_seed on a serial world cuts every
     * region into seven random parts (so even the small cases run the
     * multi-part code); on a threaded world it moves the cut points. */
    static const u32 thread_counts[3] = { 0, 3, 8 };
    for (u32 ti = 0; ti < 3; ti++) {
        const u32 th = thread_counts[ti];
        for (u32 split_seed = 0; split_seed <= 91; split_seed += 91) {
            /* a threaded world without a split seed runs fewer than 4096 slots serially */
            if (th == 0 || split_seed != 0) run_shape(&small, small_cases, AZ_LEN(small_cases), th, split_seed);
            run_shape(&tiny, tiny_cases, AZ_LEN(tiny_cases), th, split_seed);
            run_shape(&medium, medium_cases, AZ_LEN(medium_cases), th, split_seed);
            run_shape(&sparse, sparse_cases, AZ_LEN(sparse_cases), th, split_seed);
            run_shape(&large, large_cases, AZ_LEN(large_cases), th, split_seed);
        }
    }
    printf("differential: %d checks, %d failures so far\n", az_checks, az_failures);

    /* whole ticks with this grid: both invariants every tick, and one state
     * hash for every thread count, reorder period and split */
    {
        const u32 n = 6000, ticks = 12;
        const u64 h = run_ticks(n, 0, 1, 0, ticks, true);
        CHECK_EQ(run_ticks(n, 3, 1, 0, ticks, true), h);
        CHECK_EQ(run_ticks(n, 8, 1, 0, ticks, false), h);
        CHECK_EQ(run_ticks(n, 0, 9, 0, ticks, true), h);
        CHECK_EQ(run_ticks(n, 3, 9, 0, ticks, false), h);
        CHECK_EQ(run_ticks(n, 8, 9, 0, ticks, true), h);
        CHECK_EQ(run_ticks(n, 0, 9, 1234, ticks, false), h);
        CHECK_EQ(run_ticks(n, 8, 1, 99, ticks, false), h);
        /* the hash can differ: one more tick changes it */
        CHECK(run_ticks(n, 0, 1, 0, ticks + 1, false) != h);
    }
    return az_test_done();
}
