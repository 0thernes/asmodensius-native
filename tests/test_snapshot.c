/* test_snapshot.c - exact save and restore (core/az_snapshot.c).
 *
 *   1. Save at a tick with dead slots present and reorder period 7, load:
 *      every saved byte is equal at once, the hash is equal at once and after
 *      100 more ticks in both worlds, and both invariants hold every tick.
 *   2. Saving the loaded world gives identical bytes.
 *   3. A snapshot taken single-threaded continues identically with 3 and 8
 *      threads, with reorder periods 1 and 9, and with scrambled work splits.
 *   4. Truncated buffers and 200 random single-byte corruptions are rejected,
 *      and so is every single-byte corruption of the header.
 *   5. With a valid checksum, a changed layout, version, count or index is
 *      still rejected (the checks behind the checksum).
 *   6. Matter, heat and marks pending in the cell accumulator at the tick
 *      boundary survive the round trip.
 *   7. A world inside a tick is not saved; a short buffer is not written.
 *   8. The scratch of a loaded world: cells, free list, az_spawn.
 *   9. Property: many seeds, tick counts, reorder periods and lesions.
 */
#include "az_test_locked_block.h"

/* layout of snapshot format 2 (see the head of az_snapshot.c) */
#define PAD8(n) (((u64)(n) + 7u) & ~(u64)7u)
enum { OFF_MAGIC = 0, OFF_ABI = 8, OFF_FORMAT = 12, OFF_TOTAL = 16, OFF_SZ_CONFIG = 24, OFF_SZ_CLADE = 28,
       OFF_NCOLS = 52, OFF_NSLOTS = 56, OFF_NCELLS = 60, OFF_NCLADES = 64, OFF_NCOLONIES = 68,
       OFF_NLIVE = 72, OFF_CONFIG = 80, COL_REC = 16, SCALARS = 64 };
#define OFF_TABLE (OFF_CONFIG + PAD8(sizeof(AzConfig)))

/* ---- helpers ------------------------------------------------------------ */
static u64 rng_state = 0x1234567ull;
static u64 rnd(void) { return az_test_rand64(&rng_state); }          /* finding v58 */

static u32 rd32(const u8 *p) { u32 v; memcpy(&v, p, 4); return v; }
static void wr32(u8 *p, u32 v) { memcpy(p, &v, 4); }
static u64 rd64(const u8 *p) { u64 v; memcpy(&v, p, 8); return v; }
static void wr64(u8 *p, u64 v) { memcpy(p, &v, 8); }

/* the checksum of the format, written again here from its description */
static u64 ref_sum(const u8 *p, u64 n)
{
    u64 lane[4] = { 0x243F6A8885A308D3ull ^ n, 0x13198A2E03707344ull, 0xA4093822299F31D0ull, 0x082EFA98EC4E6C89ull };
    for (u64 i = 0; i < n / 8; i++) lane[i & 3] = az_mix64(lane[i & 3] ^ rd64(p + i * 8)) + 0x9E3779B97F4A7C15ull;
    u64 acc = az_mix64(n);
    for (u32 k = 0; k < 4; k++) acc = az_mix64(acc ^ lane[k]) + 0x9E3779B97F4A7C15ull;
    return acc;
}
static void refix(u8 *buf, u64 n) { wr64(buf + n - 8, ref_sum(buf, n - 8)); }

typedef struct Loaded {
    AzArena arena;
    void *block;
    AzHost *host;
    AzWinInfo info;
    AzWorld *w;
} Loaded;

static bool load_world(Loaded *l, const AzConfig *cfg, const u8 *buf, u64 n, u32 threads)
{
    memset(l, 0, sizeof *l);
    if (threads) {
        /* not pinned: a pinned worker stalls the whole pool whenever another
         * process holds its core, and the results do not depend on pinning */
        l->host = az_win_host_create(threads, AZ_WIN_USE_ECORES | AZ_WIN_NO_PIN, &l->info);
        if (!l->host) return false;
    }
    u64 bytes = az_world_bytes(cfg);
    l->block = az_test_locked_block(bytes);
    if (!l->block) return false;
    l->arena.base = (u8 *)l->block; l->arena.size = bytes; l->arena.used = 0;
    l->w = az_snapshot_load(&l->arena, buf, n, l->host);
    return l->w != NULL;
}

static void loaded_free(Loaded *l)
{
    if (l->host) az_win_host_destroy(l->host);
    if (l->block) az_test_locked_block_free(l->block);
    memset(l, 0, sizeof *l);
}

static u8 *save_world(const AzWorld *w, u64 *n_out)
{
    u64 need = az_snapshot_bytes(w);
    u8 *buf = (u8 *)malloc((size_t)need + 16);
    *n_out = 0;
    if (!buf || !need) { CHECK(!"snapshot buffer"); return buf; }
    memset(buf, 0xEE, (size_t)need + 16);
    u64 n = az_snapshot_save(w, buf, need);
    CHECK_EQ(n, need);
    CHECK(buf[need] == 0xEE && buf[need + 15] == 0xEE);   /* nothing written past the end */
    *n_out = n;
    return buf;
}

static u32 dead_slots(const AzWorld *w)
{
    u32 d = 0;
    for (u32 s = 0; s < w->n_slots; s++) if (!(w->flags[s] & AZ_F_ALIVE)) d++;
    return d;
}

/* run at least `min_ticks`, then on until dead slots are present (bounded) */
static void run_until_dead_slots(AzWorld *w, u32 min_ticks)
{
    for (u32 i = 0; i < min_ticks; i++) az_tick(w);
    for (u32 i = 0; i < 400 && dead_slots(w) == 0; i++) az_tick(w);
}

/* number of saved things that differ between two worlds, byte for byte */
static u32 state_diffs(const AzWorld *a, const AzWorld *b)
{
    AzColDesc ca, cb;
    u32 diffs = 0;
    if (a->n_slots != b->n_slots || a->n_cells != b->n_cells || a->n_clades != b->n_clades ||
        a->n_colonies != b->n_colonies) return 1000;
    for (u32 k = 0; k < az_n_columns(); k++) {
        az_column(a, k, &ca); az_column(b, k, &cb);
        if (ca.tier == AZ_TIER_EPH) continue;
        if (ca.tier == AZ_TIER_HND) continue;        /* where a block lies is not state; what it holds is (below) */
        if (memcmp(ca.data, cb.data, (size_t)a->n_slots * ca.elem * ca.per)) diffs++;
    }
    {   /* every creature's block, byte for byte */
        u32 blocks = 0;
        for (u32 s = 0; s < a->n_slots; s++) {
            const AzBlk *ba = az_blk(a, s), *bb = az_blk(b, s);
            if ((a->blk[s] != 0) != (b->blk[s] != 0) || ba->units != bb->units ||
                memcmp(ba, bb, (size_t)ba->units * AZ_UNIT)) blocks++;
        }
        diffs += blocks != 0;
    }
    for (u32 k = 0; k < az_n_cell_columns(); k++) {
        az_cell_column(a, k, &ca); az_cell_column(b, k, &cb);
        if (!strcmp(ca.name, "need") || !strcmp(ca.name, "grant")) continue;
        if (memcmp(ca.data, cb.data, (size_t)a->n_cells * ca.elem * ca.per)) diffs++;
    }
    diffs += memcmp(a->clades, b->clades, (size_t)a->n_clades * sizeof(AzClade)) != 0;
    diffs += memcmp(a->colonies, b->colonies, (size_t)a->n_colonies * sizeof(AzColony)) != 0;
    diffs += memcmp(&a->totals, &b->totals, sizeof(AzTotals)) != 0;
    diffs += memcmp(&a->code, &b->code, sizeof(AzGeneticCode)) != 0;
    diffs += memcmp(a->gstate, b->gstate, sizeof a->gstate) != 0;
    diffs += memcmp(a->vent_cell, b->vent_cell, sizeof a->vent_cell) != 0;
    diffs += memcmp(a->sun_period, b->sun_period, sizeof a->sun_period) != 0;
    diffs += memcmp(&a->cfg, &b->cfg, sizeof(AzConfig)) != 0;
    diffs += a->tick != b->tick;
    diffs += a->uid_next != b->uid_next;
    diffs += a->key[0] != b->key[0] || a->key[1] != b->key[1];
    diffs += a->role_key != b->role_key;
    diffs += a->gravity != b->gravity || a->vent_count != b->vent_count;
    diffs += a->ledger_chain != b->ledger_chain || a->ledger_count != b->ledger_count;
    diffs += a->n_live != b->n_live;
    diffs += a->cap != b->cap;
    return diffs;
}

/* a reusable arena for loads that are expected to fail */
static AzArena rej_arena;
static bool rej_open(const AzConfig *cfg)
{
    rej_arena.size = az_world_bytes(cfg) * 2;
    rej_arena.base = (u8 *)az_test_locked_block(rej_arena.size);
    rej_arena.used = 0;
    return rej_arena.base != NULL;
}
static void rej_close(void)
{
    az_test_locked_block_free(rej_arena.base);
    memset(&rej_arena, 0, sizeof rej_arena);
}
static bool rejected(const u8 *buf, u64 n)
{
    rej_arena.used = 0;
    AzWorld *w = az_snapshot_load(&rej_arena, buf, n, NULL);
    bool clean = rej_arena.used == 0;                    /* a failed load takes nothing */
    return w == NULL && clean;
}

/* file offset of the entity column `name` (0 if it is not a saved column) */
static u64 column_offset(const AzWorld *w, const u8 *buf, const char *name)
{
    u64 at = OFF_TABLE + (u64)rd32(buf + OFF_NCOLS) * COL_REC + SCALARS + sizeof(w->vent_cell) +
             PAD8(sizeof(w->gstate)) + PAD8(sizeof(AzTotals)) + PAD8(sizeof(AzGeneticCode)) +
             PAD8(sizeof(w->param));
    AzColDesc d;
    for (u32 k = 0; k < az_n_columns(); k++) {
        az_column(w, k, &d);
        if (d.tier == AZ_TIER_EPH || d.tier == AZ_TIER_HND) continue;     /* neither is saved */
        if (!strcmp(d.name, name)) return at;
        at += PAD8((u64)w->n_slots * d.elem * d.per);
    }
    return 0;
}

/* ---- 1, 2, 4: the round trip, the second save, damage -------------------- */
static void t_roundtrip(u64 seed)
{
    AzConfig cfg = az_test_config(seed, 3000);
    cfg.reorder_period = 7;
    AzTestWorld a;
    if (!az_test_world(&a, &cfg, 0)) { CHECK(!"world a"); return; }
    u64 m0 = az_total_matter(a.w);
    i64 e0 = az_total_energy(a.w);
    run_until_dead_slots(a.w, 45);
    u32 dead = dead_slots(a.w);
    CHECK(dead > 0);
    CHECK(a.w->n_live > 0);
    CHECK_EQ(az_total_matter(a.w), m0);
    CHECK_EQ(az_total_energy(a.w), e0);

    u64 n = 0;
    u8 *buf = save_world(a.w, &n);
    u64 h_save = az_state_hash(a.w);
    printf("roundtrip seed %" PRIu64 ": tick %" PRIu64 ", slots %u (dead %u), clades %u, snapshot %" PRIu64 " bytes\n",
           seed, a.w->tick, a.w->n_slots, dead, a.w->n_clades, n);

    Loaded b;
    if (!n || !load_world(&b, &a.w->cfg, buf, n, 0)) { CHECK(!"load b"); free(buf); az_test_world_free(&a); return; }
    CHECK_EQ(az_state_hash(b.w), h_save);
    CHECK_EQ(state_diffs(a.w, b.w), 0);
    CHECK_EQ(az_total_matter(b.w), m0);
    CHECK_EQ(az_total_energy(b.w), e0);
    CHECK_EQ(dead_slots(b.w), dead);

    /* 2. the loaded world saves to the same bytes */
    u64 n2 = 0;
    u8 *buf2 = save_world(b.w, &n2);
    CHECK_EQ(n2, n);
    CHECK(n2 == n && memcmp(buf, buf2, (size_t)n) == 0);
    free(buf2);

    /* 4. damage: truncation */
    if (!rej_open(&a.w->cfg)) { CHECK(!"reject arena"); return; }
    CHECK(!rejected(buf, n));                            /* the helper can tell a good one */
    CHECK(rejected(buf, 0));
    CHECK(rejected(buf, 8));
    CHECK(rejected(buf, n - 1));
    CHECK(rejected(buf, n - 8));
    CHECK(rejected(buf, n / 2));
    for (u32 i = 0; i < 64; i++) CHECK(rejected(buf, rnd() % n));
    {   /* a longer buffer is not this snapshot either */
        u8 *ext = (u8 *)calloc(1, (size_t)n + 8);
        memcpy(ext, buf, (size_t)n);
        CHECK(rejected(ext, n + 8));
        free(ext);
    }
    /* 200 random single-byte corruptions */
    u32 passed = 0;
    for (u32 i = 0; i < 200; i++) {
        u64 at = rnd() % n;
        u8 flip = (u8)(1 + rnd() % 255);
        buf[at] ^= flip;
        if (!rejected(buf, n)) passed++;
        buf[at] ^= flip;
    }
    CHECK_EQ(passed, 0);
    /* every byte of the header and the column table, and of the checksum */
    u64 head = OFF_TABLE + (u64)rd32(buf + OFF_NCOLS) * COL_REC;
    for (u64 at = 0; at < head; at++) {
        buf[at] ^= (u8)(1u << (at & 7));
        if (!rejected(buf, n)) passed++;
        buf[at] ^= (u8)(1u << (at & 7));
    }
    for (u64 at = n - 8; at < n; at++) {
        buf[at] ^= 0x80;
        if (!rejected(buf, n)) passed++;
        buf[at] ^= 0x80;
    }
    CHECK_EQ(passed, 0);
    CHECK(!rejected(buf, n));                            /* and the buffer is whole again */
    rej_close();

    /* 1. both worlds continue identically */
    for (u32 i = 0; i < 100; i++) {
        az_tick(a.w); az_tick(b.w);
        CHECK_EQ(az_total_matter(a.w), m0);
        CHECK_EQ(az_total_energy(a.w), e0);
        CHECK_EQ(az_total_matter(b.w), m0);
        CHECK_EQ(az_total_energy(b.w), e0);
        if (i % 10 == 9) CHECK_EQ(az_state_hash(a.w), az_state_hash(b.w));
    }
    CHECK(az_state_hash(a.w) != h_save);                 /* the worlds did move */
    CHECK_EQ(state_diffs(a.w, b.w), 0);                  /* same slots, same bytes, 100 ticks later */
    CHECK_EQ(a.w->totals.field_underflow, 0);
    CHECK_EQ(b.w->totals.field_underflow, 0);
    CHECK_EQ(a.w->n_live, b.w->n_live);
    CHECK_EQ(a.w->uid_next, b.w->uid_next);

    free(buf);
    loaded_free(&b);
    az_test_world_free(&a);
}

/* ---- 3: thread counts, reorder periods, work splits ----------------------- */
static void t_threads(u64 seed)
{
    AzConfig cfg = az_test_config(seed, 6000);
    cfg.reorder_period = 7;
    AzTestWorld a;
    if (!az_test_world(&a, &cfg, 0)) { CHECK(!"world a"); return; }       /* single-threaded */
    u64 m0 = az_total_matter(a.w);
    i64 e0 = az_total_energy(a.w);
    run_until_dead_slots(a.w, 31);
    CHECK(dead_slots(a.w) > 0);
    u64 n = 0;
    u8 *buf = save_world(a.w, &n);
    u64 h_save = az_state_hash(a.w);
    const u32 threads[5] = { 8, 3, 0, 0, 5 };
    const u8 period[5] = { 7, 7, 1, 9, 31 };
    const u32 split[5] = { 0, 0, 0, 0, 0xC0FFEEu };
    Loaded l[5];
    bool ok = n != 0;
    for (u32 k = 0; k < 5 && ok; k++) {
        ok = load_world(&l[k], &a.w->cfg, buf, n, threads[k]);
        if (!ok) break;
        CHECK_EQ(az_state_hash(l[k].w), h_save);
        CHECK_EQ(l[k].w->n_workers, threads[k] ? threads[k] : 1);
        l[k].w->cfg.reorder_period = period[k];          /* how the slots are packed must not matter */
        l[k].w->cfg.debug_split_seed = split[k];         /* nor where the work is cut */
    }
    if (!ok) { CHECK(!"load with threads"); free(buf); az_test_world_free(&a); return; }
    for (u32 i = 0; i < 100; i++) {
        az_tick(a.w);
        for (u32 k = 0; k < 5; k++) az_tick(l[k].w);
        if (i % 20 == 19 || i == 0)
            for (u32 k = 0; k < 5; k++) CHECK_EQ(az_state_hash(l[k].w), az_state_hash(a.w));
    }
    for (u32 k = 0; k < 5; k++) {
        CHECK_EQ(az_total_matter(l[k].w), m0);
        CHECK_EQ(az_total_energy(l[k].w), e0);
        CHECK_EQ(l[k].w->totals.field_underflow, 0);
    }
    printf("threads seed %" PRIu64 ": saved serial at tick %" PRIu64 ", 100 more ticks with 8, 3, 1, 1, 5 threads: live %u, hash %016" PRIx64 "\n",
           seed, a.w->tick - 100, a.w->n_live, az_state_hash(a.w));
    CHECK(a.w->n_live >= 2048);                          /* enough entities for parallel splits */
    for (u32 k = 0; k < 5; k++) loaded_free(&l[k]);
    free(buf);
    az_test_world_free(&a);
}

/* Cells below `below` with something pending in the accumulator. A module that posts after the
 * last field commit of a tick leaves such records (Tier 1 does when it pays for a long-term word
 * as heat), so a world saved between ticks need not have an empty pending section. */
static u64 pending_cells(const AzWorld *w, u32 below)
{
    static const AzCellAcc zero;
    u64 k = 0;
    for (u32 c = 0; c < w->n_cells && c < below; c++) k += memcmp(&w->c_acc[c], &zero, sizeof zero) != 0;
    return k;
}

/* ---- 5: the checks behind the checksum ----------------------------------- */
static void mutate32(u8 *buf, u64 n, u64 at, u32 value, const char *what)
{
    u32 old = rd32(buf + at);
    wr32(buf + at, value);
    refix(buf, n);
    if (!rejected(buf, n)) { CHECK(!"accepted a changed snapshot"); printf("  accepted: %s\n", what); }
    else CHECK(1);
    wr32(buf + at, old);
    refix(buf, n);
}

/* Format 8 appends a sparse high-limb section after the ordinary accumulator.
 * Count its entries from the independent source world before locating the
 * old-section mutations. */
static u64 species_carry_section_bytes(const AzWorld *w)
{
    u64 bytes = 8;
    for (u32 c = 0; c < w->n_cells; c++) {
        const u32 *carry = &w->c_species_carry[(size_t)c * AZ_NSPECIES];
        for (u32 k = 0; k < AZ_NSPECIES; k++) {
            if (carry[k]) {
                bytes += 8 + AZ_NSPECIES * sizeof(*carry);
                break;
            }
        }
    }
    return bytes;
}

static void t_structure(u64 seed)
{
    AzConfig cfg = az_test_config(seed, 800);
    cfg.reorder_period = 7;
    AzTestWorld a;
    if (!az_test_world(&a, &cfg, 0)) { CHECK(!"world a"); return; }
    run_until_dead_slots(a.w, 20);
    u64 n = 0;
    u8 *buf = save_world(a.w, &n);
    if (!n || !rej_open(&a.w->cfg)) { CHECK(!"setup"); return; }
    const u64 sum0 = rd64(buf + n - 8);
    refix(buf, n);
    CHECK_EQ(rd64(buf + n - 8), sum0);                   /* the checksum is the documented one */
    CHECK(!rejected(buf, n));

    const u32 n_cols = rd32(buf + OFF_NCOLS);
    const u64 scal = OFF_TABLE + (u64)n_cols * COL_REC;
    CHECK(n_cols >= 10);
    mutate32(buf, n, OFF_MAGIC, rd32(buf + OFF_MAGIC) ^ 1u, "magic");
    mutate32(buf, n, OFF_ABI, AZ_ABI_VERSION + 1, "ABI version");
    mutate32(buf, n, OFF_ABI, AZ_ABI_VERSION - 1, "older ABI version");
    mutate32(buf, n, OFF_FORMAT, rd32(buf + OFF_FORMAT) + 1, "format version");
    mutate32(buf, n, OFF_TOTAL, (u32)n + 8, "total size");
    mutate32(buf, n, OFF_SZ_CONFIG, (u32)sizeof(AzConfig) + 4, "sizeof(AzConfig)");
    mutate32(buf, n, OFF_SZ_CLADE, (u32)sizeof(AzClade) + 8, "sizeof(AzClade)");
    mutate32(buf, n, OFF_SZ_CLADE + 4, (u32)sizeof(AzColony) - 4, "sizeof(AzColony)");
    mutate32(buf, n, OFF_SZ_CLADE + 8, (u32)sizeof(AzTotals) + 8, "sizeof(AzTotals)");
    mutate32(buf, n, OFF_SZ_CLADE + 12, (u32)sizeof(AzGeneticCode) + 1, "sizeof(AzGeneticCode)");
    mutate32(buf, n, OFF_SZ_CLADE + 16, AZ_NGLOBAL + 1, "AZ_NGLOBAL");
    mutate32(buf, n, OFF_NCOLS, n_cols - 1, "one column fewer");
    mutate32(buf, n, OFF_NCOLS, n_cols + 1, "one column more");
    mutate32(buf, n, OFF_NCOLS, 0x7FFFFFFFu, "absurd column count");
    mutate32(buf, n, OFF_NSLOTS, a.w->n_slots + 1, "one slot more");
    mutate32(buf, n, OFF_NSLOTS, a.w->n_slots - 1, "one slot fewer");
    mutate32(buf, n, OFF_NSLOTS, a.w->cap + 64, "more slots than capacity");
    mutate32(buf, n, OFF_NSLOTS, 0xFFFFFFFFu, "absurd slot count");
    mutate32(buf, n, OFF_NCELLS, a.w->n_cells * 2, "cell count");
    mutate32(buf, n, OFF_NCLADES, a.w->n_clades + 1, "one clade more");
    mutate32(buf, n, OFF_NCLADES, a.w->cfg.max_clades + 1, "too many clades");
    mutate32(buf, n, OFF_NCOLONIES, 0, "no colony 0");
    mutate32(buf, n, OFF_NCOLONIES, a.w->cfg.max_colonies + 1, "too many colonies");
    mutate32(buf, n, OFF_NLIVE, a.w->n_slots + 1, "more alive than slots");
    /* the config: another grid, a value az_world_create would normalise away, an invalid one */
    {
        u64 g = OFF_CONFIG + offsetof(AzConfig, gx_bits);   /* gx, gy, gz, reorder_period */
        u32 old = rd32(buf + g);
        mutate32(buf, n, g, old + 1, "gx_bits + 1");
        mutate32(buf, n, g, old & 0x00FFFFFFu, "reorder_period 0");
        mutate32(buf, n, g, (old & 0xFFFFFF00u) | 1u, "gx_bits 1");
    }
    /* the column table: a column that changed its element size, its count per entity, its name, its tier */
    for (u32 k = 0; k < n_cols; k += 5) {
        u64 r = OFF_TABLE + (u64)k * COL_REC;
        mutate32(buf, n, r, rd32(buf + r) ^ 0x10u, "column name");
        mutate32(buf, n, r + 8, rd32(buf + r + 8) * 2, "column element size");
        mutate32(buf, n, r + 12, rd32(buf + r + 12) + 1, "column elements per entity");
        mutate32(buf, n, r + 12, rd32(buf + r + 12) ^ 0x00030000u, "column kind");
    }
    {   /* two columns swapped in the .def file */
        u8 t[COL_REC];
        u8 *r0 = buf + OFF_TABLE, *r1 = buf + OFF_TABLE + COL_REC;
        memcpy(t, r0, COL_REC); memcpy(r0, r1, COL_REC); memcpy(r1, t, COL_REC);
        refix(buf, n);
        CHECK(rejected(buf, n));
        memcpy(t, r0, COL_REC); memcpy(r0, r1, COL_REC); memcpy(r1, t, COL_REC);
        refix(buf, n);
    }
    /* indices the tick would follow without a range check */
    mutate32(buf, n, scal + 56, (rd32(buf + scal + 56) & 0xFFFFu) | (65u << 16), "vent_count 65");
    if (a.w->vent_count) mutate32(buf, n, scal + SCALARS, a.w->n_cells, "vent cell outside the grid");
    {
        u64 at = column_offset(a.w, buf, "clade");
        CHECK(at != 0);
        u32 old = rd32(buf + at);
        CHECK_EQ(old, a.w->clade[0]);                    /* the offset really is the clade column */
        mutate32(buf, n, at, a.w->cfg.max_clades, "clade index outside the table");
        at = column_offset(a.w, buf, "colony");
        CHECK(at != 0);
        mutate32(buf, n, at, a.w->cfg.max_colonies, "colony index outside the table");
    }
    /* The first pending section contains one count, then one record for every cell that has
     * something pending (a world with nothing pending pays the 8 bytes of a zero count) */
    {
        const u64 k = pending_cells(a.w, a.w->n_cells);
        const u64 at = n - 8 - species_carry_section_bytes(a.w) - k * (8 + sizeof(AzCellAcc)) - 8;
        CHECK_EQ(rd64(buf + at), k);
        mutate32(buf, n, at, (u32)k + 1, "pending count without entries");
    }

    CHECK(!rejected(buf, n));                            /* all mutations were undone */
    CHECK_EQ(rd64(buf + n - 8), sum0);
    rej_close();
    free(buf);
    az_test_world_free(&a);
}

/* ---- 6: what is pending in the cell accumulator --------------------------- */
static void t_pending(u64 seed)
{
    AzConfig cfg = az_test_config(seed, 1500);
    cfg.reorder_period = 7;
    AzTestWorld a;
    if (!az_test_world(&a, &cfg, 0)) { CHECK(!"world a"); return; }
    u64 m0 = az_total_matter(a.w);
    i64 e0 = az_total_energy(a.w);
    for (u32 i = 0; i < 24; i++) az_tick(a.w);
    AzWorld *w = a.w;
    const u64 n_plain = az_snapshot_bytes(w);
    CHECK(n_plain > 0);

    /* what a module does when it posts after the last field commit of a tick:
     * an entity pays 10 eu as heat and sheds 5 mu of biomass into its cell,
     * and a colony marks the next cell */
    u32 s = AZ_NONE;
    const u64 k0 = pending_cells(w, w->n_cells);         /* what the world itself left pending */
    for (u32 k = 0; k < w->n_slots; k++) {
        if (!(w->flags[k] & AZ_F_ALIVE) || w->energy[k] < 10 || w->pool_a[k] < 5) continue;
        const u32 ck = az_cell_at(w, w->pos_x[k], w->pos_y[k], w->pos_z[k]);
        const u32 ck2 = (ck + 1) % w->n_cells;           /* two cells with nothing pending yet */
        if (pending_cells(w, ck + 1) != pending_cells(w, ck) || pending_cells(w, ck2 + 1) != pending_cells(w, ck2)) continue;
        s = k;
        break;
    }
    CHECK(s != AZ_NONE);
    if (s == AZ_NONE) { az_test_world_free(&a); return; }
    const u32 c = az_cell_at(w, w->pos_x[s], w->pos_y[s], w->pos_z[s]);
    const u32 c2 = (c + 1) % w->n_cells;
    w->energy[s] -= 10;
    az_cell_post_heat(w, 0, c, 10);
    w->pool_a[s] = (u16)(w->pool_a[s] - 5);
    az_cell_post(w, 0, c, AZ_SP_ORG_Y, 5);
    az_cell_mark(w, 0, c2, 0, 9, 3);
    CHECK_EQ(az_total_matter(w), m0);                    /* the audits count what is pending */
    CHECK_EQ(az_total_energy(w), e0);

    u64 n = 0;
    u8 *buf = save_world(w, &n);
    CHECK_EQ(n, n_plain + 2 * (8 + sizeof(AzCellAcc)));  /* two cells, one record each */
    CHECK_EQ(pending_cells(w, w->n_cells), k0 + 2);
    Loaded b, z;
    if (!n || !load_world(&b, &w->cfg, buf, n, 0) || !load_world(&z, &w->cfg, buf, n, 0)) { CHECK(!"load"); return; }
    CHECK_EQ(az_total_matter(b.w), m0);
    CHECK_EQ(az_total_energy(b.w), e0);
    CHECK_EQ(state_diffs(w, b.w), 0);
    CHECK_EQ(b.w->c_acc[c].dheat, 10);
    CHECK_EQ(b.w->c_acc[c].delta[AZ_SP_ORG_Y], 5);
    CHECK_EQ(b.w->c_acc[c2].dmark[0], 9);
    CHECK_EQ(b.w->c_acc[c2].dmark_id, (9ull << 32) | 3u);
    CHECK_EQ(az_state_hash(b.w), az_state_hash(w));

    /* control: without the pending records the restored world would have lost
     * matter and energy. Until finding v01 was fixed the state hash alone did
     * not show it (this check held that limit); now the hash covers pending
     * accumulators, so the damaged world must hash differently. */
    memset(z.w->c_acc, 0, (size_t)z.w->n_cells * sizeof(AzCellAcc));
    CHECK(az_total_matter(z.w) != m0);
    CHECK(az_total_energy(z.w) != e0);
    CHECK(az_state_hash(z.w) != az_state_hash(w));

    /* damaged pending records, with a valid checksum */
    if (rej_open(&w->cfg)) {
        const u64 rec = 8 + sizeof(AzCellAcc);
        const u32 lo = c < c2 ? c : c2, hi = c < c2 ? c2 : c;
        const u64 at_count = n - 8 - species_carry_section_bytes(w) - (k0 + 2) * rec - 8;     /* the records are in ascending cell order */
        const u64 at_e0 = at_count + 8 + pending_cells(w, lo) * rec, at_e1 = at_count + 8 + pending_cells(w, hi) * rec;
        CHECK_EQ(rd64(buf + at_count), k0 + 2);
        CHECK_EQ(rd64(buf + at_e0), lo);
        CHECK_EQ(rd64(buf + at_e1), hi);
        mutate32(buf, n, at_count, (u32)k0 + 3, "pending count too large");
        mutate32(buf, n, at_count, (u32)k0 + 1, "pending count too small");
        mutate32(buf, n, at_e1, rd32(buf + at_e1 - rec), "pending cells not ascending");
        mutate32(buf, n, at_e1, w->n_cells, "pending cell outside the grid");
        u8 keep[sizeof(AzCellAcc)];
        memcpy(keep, buf + at_e1 + 8, sizeof keep);
        memset(buf + at_e1 + 8, 0, sizeof keep);
        refix(buf, n);
        CHECK(rejected(buf, n));                         /* an all-zero record is not canonical */
        memcpy(buf + at_e1 + 8, keep, sizeof keep);
        refix(buf, n);
        CHECK(!rejected(buf, n));
        rej_close();
    }

    for (u32 i = 0; i < 30; i++) {
        az_tick(w); az_tick(b.w); az_tick(z.w);
        CHECK_EQ(az_total_matter(b.w), m0);
        CHECK_EQ(az_total_energy(b.w), e0);
        CHECK_EQ(az_state_hash(b.w), az_state_hash(w));
    }
    CHECK_EQ(w->totals.field_underflow, 0);
    /* the control, at the same tick (finding v30): the world that lost its
     * pending records has diverged from the one that kept them */
    CHECK_EQ(z.w->tick, w->tick);
    CHECK(az_state_hash(z.w) != az_state_hash(w));
    free(buf);
    loaded_free(&b); loaded_free(&z);
    az_test_world_free(&a);
}

/* ---- 7, 8: refusals; the scratch of a loaded world ------------------------ */
static void t_refuse_and_scratch(u64 seed)
{
    AzConfig cfg = az_test_config(seed, 1200);
    cfg.reorder_period = 31;
    AzTestWorld a;
    if (!az_test_world(&a, &cfg, 0)) { CHECK(!"world a"); return; }
    run_until_dead_slots(a.w, 12);
    AzWorld *w = a.w;
    const u32 dead = dead_slots(w);
    CHECK(dead > 0);
    u64 n = 0;
    u8 *buf = save_world(w, &n);
    if (!n) { az_test_world_free(&a); return; }

    /* 7. a short buffer is left alone; a world inside a tick is not saved */
    u8 *small = (u8 *)malloc((size_t)n);
    memset(small, 0x5A, (size_t)n);
    CHECK_EQ(az_snapshot_save(w, small, n - 1), 0);
    CHECK_EQ(az_snapshot_save(w, small, 0), 0);
    u64 touched = 0;
    for (u64 i = 0; i < n; i++) touched += small[i] != 0x5A;
    CHECK_EQ(touched, 0);
    CHECK_EQ(az_snapshot_save(w, NULL, n), 0);
    az_tally(w, 0)->energy_in = 7;                       /* an unmerged tally: mid-tick */
    CHECK_EQ(az_snapshot_bytes(w), 0);
    CHECK_EQ(az_snapshot_save(w, small, n), 0);
    az_tally(w, 0)->energy_in = 0;
    const u64 hash = az_state_hash(w);
    w->c_need[0].dem[AZ_D_SOLV] = 1ull << 32;
    w->c_grant[AZ_D_SOLV] = 12345;
    CHECK_EQ(az_state_hash(w), hash);
    CHECK_EQ(az_snapshot_bytes(w), n);
    CHECK_EQ(az_snapshot_save(w, small, n), n);
    CHECK(memcmp(small, buf, (size_t)n) == 0);
    free(small);

    /* garbage and missing arguments */
    AzArena tiny;
    Loaded b;
    memset(&tiny, 0, sizeof tiny);
    CHECK(az_snapshot_load(NULL, buf, n, NULL) == NULL);
    CHECK(az_snapshot_load(&tiny, NULL, n, NULL) == NULL);
    CHECK(az_snapshot_load(&tiny, buf, n, NULL) == NULL);         /* an arena with no room */
    CHECK_EQ(tiny.used, 0);
    {
        u8 junk[4096];
        for (u32 i = 0; i < sizeof junk; i++) junk[i] = (u8)rnd();
        if (rej_open(&w->cfg)) {
            for (u32 len = 0; len <= sizeof junk; len += 248) CHECK(rejected(junk, len));
            memset(junk, 0, sizeof junk);
            CHECK(rejected(junk, sizeof junk));
            rej_close();
        }
    }

    /* 8. scratch: every alive slot knows its cell, the dead ones are the free list */
    if (!load_world(&b, &w->cfg, buf, n, 0)) { CHECK(!"load b"); free(buf); az_test_world_free(&a); return; }
    AzWorld *v = b.w;
    CHECK_EQ(v->c_need[0].dem[AZ_D_SOLV], 0);
    CHECK_EQ(v->c_grant[AZ_D_SOLV], 0);
    u32 bad = 0, chain = 0, prev = AZ_NONE;
    for (u32 s = 0; s < v->n_slots; s++) {
        if (v->flags[s] & AZ_F_ALIVE) {
            bad += v->cell[s] != az_cell_at(v, v->pos_x[s], v->pos_y[s], v->pos_z[s]);
            bad += v->spos[s] != AZ_NONE;
        } else {
            bad += v->cell[s] != AZ_NONE;
        }
        bad += v->pair[s].peer != AZ_NONE;
        bad += v->birth[s].n != 0;
    }
    CHECK_EQ(bad, 0);
    for (u32 s = v->free_head; s != AZ_NONE && chain <= v->n_slots; s = v->spos[s], chain++) {
        bad += s >= v->n_slots || (v->flags[s] & AZ_F_ALIVE) != 0;
        bad += prev != AZ_NONE && s >= prev;             /* from the highest slot down */
        if (s >= v->n_slots) break;
        prev = s;
    }
    CHECK_EQ(bad, 0);
    CHECK_EQ(chain, dead);
    CHECK_EQ(v->n_sorted, 0);

    /* az_spawn works at once in the loaded world: it reuses the highest dead slot */
    const u32 head = v->free_head, uid = v->uid_next, slots = v->n_slots;
    u32 s = az_spawn(v);
    CHECK_EQ(s, head);
    CHECK_EQ(v->uid[s], uid);
    CHECK_EQ(v->n_slots, slots);
    CHECK_EQ(v->flags[s], 0);                            /* still dead: the test only took the slot */
    v->uid[s] = 0;                                       /* hand it back as it was: a dead, zeroed slot */
    v->rot_w[s] = 0;
    v->uid_next = uid;

    /* an empty world, and worldgen in the world that was loaded from it */
    AzTestWorld e;
    bool e_ok = az_test_world_ex(&e, &cfg, 0, AZ_TEST_NO_WORLDGEN);   /* finding v52 */
    CHECK(e_ok && e.w != NULL);
    if (e_ok && e.w) {
        u64 ne = 0;
        u8 *be = save_world(e.w, &ne);
        Loaded f;
        if (ne && load_world(&f, &e.w->cfg, be, ne, 0)) {
            CHECK_EQ(f.w->n_slots, 0);
            CHECK_EQ(f.w->free_head, AZ_NONE);
            CHECK_EQ(az_state_hash(f.w), az_state_hash(e.w));
            az_worldgen(e.w); az_worldgen(f.w);
            for (u32 i = 0; i < 10; i++) { az_tick(e.w); az_tick(f.w); }
            CHECK(f.w->n_live > 0);
            CHECK_EQ(az_state_hash(f.w), az_state_hash(e.w));
            CHECK_EQ(az_total_matter(f.w), az_total_matter(e.w));
            CHECK_EQ(az_total_energy(f.w), az_total_energy(e.w));
            loaded_free(&f);
        } else CHECK(!"empty world round trip");
        free(be);
    }
    az_test_world_free(&e);

    free(buf);
    loaded_free(&b);
    az_test_world_free(&a);
}

/* ---- 9: property ----------------------------------------------------------- */
static void t_property(void)
{
    const u8 periods[3] = { 1, 7, 31 };
    const u32 lesions[4] = { 0, AZ_LESION_SOCIAL, AZ_LESION_WEATHER | AZ_LESION_CHEM, AZ_LESION_BRAIN | AZ_LESION_MUTATION };
    u32 runs = 0, with_dead = 0;
    for (u64 seed = 25; seed <= 36; seed++) {          /* same residues mod 12 as the old 5101..5112 */
        AzConfig cfg = az_test_config(seed, 300 + (u32)(rnd() % 500));
        cfg.reorder_period = periods[seed % 3];
        cfg.lesion = lesions[seed % 4];
        if (seed & 1) { cfg.gx_bits = 3; cfg.gy_bits = 5; cfg.gz_bits = 2; }
        AzTestWorld a;
        if (!az_test_world(&a, &cfg, 0)) { CHECK(!"world"); continue; }
        u64 m0 = az_total_matter(a.w);
        i64 e0 = az_total_energy(a.w);
        u32 ticks = 1 + (u32)(rnd() % 70);
        if (seed % 3 == 0) run_until_dead_slots(a.w, ticks);   /* every third run is saved with dead slots */
        else for (u32 i = 0; i < ticks; i++) az_tick(a.w);
        with_dead += dead_slots(a.w) > 0;
        u64 n = 0, n2 = 0;
        u8 *buf = save_world(a.w, &n);
        Loaded b;
        if (!n || !load_world(&b, &a.w->cfg, buf, n, (seed % 4 == 3) ? 3 : 0)) { CHECK(!"load"); free(buf); az_test_world_free(&a); continue; }
        CHECK_EQ(state_diffs(a.w, b.w), 0);
        CHECK_EQ(az_state_hash(b.w), az_state_hash(a.w));
        CHECK_EQ(b.w->cfg.lesion, cfg.lesion);
        u8 *buf2 = save_world(b.w, &n2);
        CHECK(n2 == n && memcmp(buf, buf2, (size_t)n) == 0);
        if (seed % 2) b.w->cfg.debug_split_seed = (u32)seed;
        for (u32 i = 0; i < 25; i++) {
            az_tick(a.w); az_tick(b.w);
            CHECK_EQ(az_total_matter(b.w), m0);
            CHECK_EQ(az_total_energy(b.w), e0);
        }
        CHECK_EQ(az_state_hash(b.w), az_state_hash(a.w));
        runs++;
        free(buf); free(buf2);
        loaded_free(&b);
        az_test_world_free(&a);
    }
    printf("property: %u round trips, %u of them saved with dead slots present\n", runs, with_dead);
    CHECK_EQ(runs, 12);
    CHECK(with_dead > 0);
}

/* no argument: everything. One argument 1..6: only that group (for debugging). */
int main(int argc, char **argv)
{
    int only = argc > 1 ? atoi(argv[1]) : 0;
    /* tuning seeds 1-99 only: 5001-5024 and 5101-5124 are reserved for the
     * preregistered milestone-8 reporting runs */
    if (!only || only == 1) { t_roundtrip(11); t_roundtrip(12); }
    if (!only || only == 2) t_threads(13);
    if (!only || only == 3) t_structure(14);
    if (!only || only == 4) t_pending(15);
    if (!only || only == 5) t_refuse_and_scratch(16);
    if (!only || only == 6) t_property();
    return az_test_done();
}
