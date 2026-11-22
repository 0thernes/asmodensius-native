/* az_run.c - command-line runner: run N ticks, print timings, hashes and
 * the conservation audit.
 *
 *   az_run [--n 100000] [--ticks 1000] [--lab [--seed 42]] [--threads 0] [--ecores]
 *          [--grid x,y,z] [--cap 0] [--species 100] [--reorder 16] [--k1 32]
 *          [--k2 256] [--stride 8] [--max-clades 0] [--split-seed 0]
 *          [--hash-every 0] [--audit-every 0] [--lesion 0x0] [--kernels 0]
 *          [--save FILE] [--load FILE] [--ledger FILE] [--entropy-journal FILE]
 *          [--param name=value,...] [--store-units 0] [--store-period 0]
 *          [--no-lock] [--allow-pageable] [--large-pages] [--no-pin] [--quiet]
 *
 * A run is WILD unless --lab is given: the world is made of fresh entropy
 * from the operating system and draws more at the start of every tick, so no
 * seed determines it and no two runs are the same (matter and energy still
 * balance exactly). --lab is the seeded, repeatable test bench (--seed, 42 by
 * default): the tests, the reference hashes and the lesion contrasts use it.
 * --seed without --lab is refused: a wild run has no seed.
 * --entropy-journal FILE (wild runs) writes one 24-byte record per tick: the
 * tick (u64, little endian) and the 16 bytes it drew. It is a record to look
 * into a surprising event afterwards; nothing reads it back into a run.
 * Every option is checked (tools/az_tools.h): a value must be a whole number
 * inside its range, and an unknown, repeated or incomplete option is refused
 * with one line on stderr, a short usage text and exit code 2.
 * Without --grid the grid follows the population (azt_grid_for, finding s4);
 * the world line says which grid was chosen and why.
 * --save FILE writes the world (az_snapshot_save) after the last tick.
 * --load FILE takes the world from a snapshot instead of worldgen; the thread
 * options and --ticks may differ from the run that saved it, but an option that
 * defines the world (--n, --seed, --cap, --grid, --species, --reorder, --k1,
 * --k2, --stride, --lesion, --max-clades, --split-seed, --kernels) is refused
 * unless it equals the snapshot's value.
 * --ledger FILE writes every event of the run to FILE through the ledger sink
 * (platform/win/az_sink_win.h; read it with az_ledgerdump). With --load the
 * file starts from the snapshot's ledger chain and count (az_sink_set_start,
 * finding s2), so its chain continues the saved run's. The last line says how
 * many records were written and whether the file's chain equals the world's.
 * The arena is locked in RAM. If the lock fails, az_run refuses to run
 * (exit code 3) unless --allow-pageable is given; --no-lock skips the lock.
 * A run on pageable memory always prints a warning on stderr, --quiet or not.
 * --no-pin leaves thread placement to the OS (no ideal processors).
 * Exit codes: 0 ok, 1 an invariant failed, the snapshot was not written or the
 * ledger is incomplete (events dropped, a write failed, or its chain differs),
 * 2 bad options or setup failed (a snapshot that cannot be loaded included),
 * 3 the arena could not be locked.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "../core/az_abi.h"
#include "../platform/win/az_host_win.h"
#include "../platform/win/az_sink_win.h"
#include "../kernels/az_kernels.h"
#include "az_tools.h"
#include "az_stats_report.h"
#include "az_teaching_observer.h"

/* bytes of the fixed per-creature record: the HOT tier and the store handle */
enum {
#define AZ_TIER_BYTES_HOT(type, per)  + (sizeof(type) * (per))
#define AZ_TIER_BYTES_HND(type, per)  + (sizeof(type) * (per))
#define AZ_TIER_BYTES_WARM(type, per)
#define AZ_TIER_BYTES_EPH(type, per)
#define AZ_COL(name, type, per, tier) AZ_TIER_BYTES_##tier(type, per)
    AZ_FIXED_RECORD_BYTES = 0
#include "az_columns.def"
#undef AZ_COL
#undef AZ_TIER_BYTES_HOT
#undef AZ_TIER_BYTES_HND
#undef AZ_TIER_BYTES_WARM
#undef AZ_TIER_BYTES_EPH
};

#define CAP_MAX    AZ_CAP_MAX
#define LESION_ALL AZ_LESION_ALL

static const char *const opt_value[] = {
    "--n", "--ticks", "--seed", "--threads", "--grid", "--cap", "--species", "--reorder", "--k1", "--k2",
    "--stride", "--lesion", "--max-clades", "--split-seed", "--kernels", "--hash-every", "--audit-every",
    "--save", "--load", "--ledger", "--param", "--store-units", "--store-period", "--entropy-journal", NULL
};
static const char *const opt_flag[] = {
    "--ecores", "--no-lock", "--allow-pageable", "--large-pages", "--no-pin", "--quiet", "--lab", NULL
};

static int usage(void)
{
    printf("usage: az_run [--n N] [--ticks T] [--lab [--seed S]] [--threads 0..%u] [--ecores] [--no-pin]\n"
           "              [--grid x,y,z] [--cap C] [--species 1..100] [--reorder 0..255] [--k1 0..255]\n"
           "              [--k2 0..65535] [--stride 0|2..128] [--lesion 0..0x%x] [--max-clades 0..%u]\n"
           "              [--split-seed S] [--kernels 0|1] [--hash-every K] [--audit-every K]\n"
           "              [--save FILE] [--load FILE] [--ledger FILE] [--entropy-journal FILE]\n"
           "              [--param name=value,...] [--store-units U] [--store-period K]\n"
           "              [--no-lock] [--allow-pageable] [--large-pages] [--quiet]\n"
           "  A run is wild (fresh entropy every tick, no seed, never the same twice) unless --lab is given;\n"
           "  --lab is the seeded, repeatable test bench, and --seed needs it.\n"
           "  0 means the default for --cap (n + n/2 + 4096), --reorder (1), --k1 (32), --k2 (256), --stride (8).\n"
           "  Decimal values may not start with 0 (an old 010 meant octal 8); write 8 or 0x8.\n",
           AZ_MAX_WORKERS, LESION_ALL, 1u << 24);
    return 2;
}

/* the entropy journal of a wild run: tick and the 16 bytes it drew */
static FILE *g_journal;
static u64 g_journal_records;

static void journal_log(void *ctx, u64 tick, const u8 bytes[16])
{
    (void)ctx;
    u8 rec[24];
    for (u32 i = 0; i < 8; i++) rec[i] = (u8)(tick >> (8 * i));
    memcpy(rec + 8, bytes, 16);
    if (g_journal && fwrite(rec, 1, sizeof rec, g_journal) == sizeof rec) g_journal_records++;
}

/* The grid order records the population at binning time, before births and deaths. */
static u32 live_at_tick_end(const AzWorld *w)
{
    u32 count = 0;
    for (u32 s = 0; s < w->n_slots; s++) count += (w->flags[s] & AZ_F_ALIVE) != 0;
    return count;
}

/* the whole file in a malloc'd buffer; NULL on any error */
static u8 *read_file(const char *path, u64 *n_out)
{
    *n_out = 0;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    u8 *buf = NULL;
    long long len = -1;
    if (_fseeki64(f, 0, SEEK_END) == 0) len = _ftelli64(f);
    if (len > 0 && _fseeki64(f, 0, SEEK_SET) == 0 && (buf = malloc((size_t)len)) != NULL &&
        fread(buf, 1, (size_t)len, f) != (size_t)len) {
        free(buf);
        buf = NULL;
    }
    fclose(f);
    if (buf) *n_out = (u64)len;
    return buf;
}

/* --load with a world option: refuse it unless it equals the snapshot's */
static bool same_as_snapshot(int argc, char **argv, const char *name, u64 given, u64 snap)
{
    if (azt_find(argc, argv, 1, name) < 0 || given == snap) return true;
    fprintf(stderr, "option %s %" PRIu64 " differs from the snapshot's %" PRIu64
            "; --load takes the world from the snapshot\n", name, given, snap);
    return false;
}

int main(int argc, char **argv)
{
    u32 n = 100000, ticks = 1000, cap = 0, species = 100, reorder = 16, k1 = 32, k2 = 256, stride = 8;
    u32 lesion = 0, max_clades = 0, split = 0, kernels = 0, threads = 0, hash_every = 0, audit_every = 0;
    u64 seed = 42;
    u32 grid[3] = { 6, 6, 5 };
    bool grid_given = false, ecores = false, no_pin = false, no_lock = false, pageable = false;
    bool large = false, quiet = false, lab = false;
    const char *save = NULL, *load = NULL, *ledger = NULL, *params = NULL, *journal = NULL;
    u32 store_units = 0, store_period = 0;

    bool ok = azt_known(argc, argv, 1, opt_value, opt_flag) &&
              azt_u32(argc, argv, 1, "--n", 1, CAP_MAX, &n) &&
              azt_u32(argc, argv, 1, "--ticks", 0, 0xFFFFFFFFu, &ticks) &&
              azt_u64(argc, argv, 1, "--seed", 0, UINT64_MAX, &seed) &&
              azt_u32(argc, argv, 1, "--threads", 0, AZ_MAX_WORKERS, &threads) &&
              azt_grid(argc, argv, 1, "--grid", 12, grid, &grid_given) &&
              azt_u32(argc, argv, 1, "--cap", 0, CAP_MAX, &cap) &&
              azt_u32(argc, argv, 1, "--species", 1, 100, &species) &&
              azt_u32(argc, argv, 1, "--reorder", 0, 255, &reorder) &&
              azt_u32(argc, argv, 1, "--k1", 0, 255, &k1) &&
              azt_u32(argc, argv, 1, "--k2", 0, 65535, &k2) &&
              azt_u32(argc, argv, 1, "--stride", 0, 128, &stride) &&
              azt_u32(argc, argv, 1, "--lesion", 0, LESION_ALL, &lesion) &&
              azt_u32(argc, argv, 1, "--max-clades", 0, 1u << 24, &max_clades) &&
              azt_u32(argc, argv, 1, "--split-seed", 0, 0xFFFFFFFFu, &split) &&
              azt_u32(argc, argv, 1, "--kernels", 0, 1, &kernels) &&
              azt_u32(argc, argv, 1, "--hash-every", 0, 0xFFFFFFFFu, &hash_every) &&
              azt_u32(argc, argv, 1, "--audit-every", 0, 0xFFFFFFFFu, &audit_every) &&
              azt_str(argc, argv, 1, "--save", &save) &&
              azt_str(argc, argv, 1, "--load", &load) &&
              azt_str(argc, argv, 1, "--ledger", &ledger) &&
              azt_str(argc, argv, 1, "--param", &params) &&
              azt_u32(argc, argv, 1, "--store-units", 0, 0x40000000u, &store_units) &&
              azt_u32(argc, argv, 1, "--store-period", 0, 65535, &store_period) &&
              azt_flag(argc, argv, 1, "--ecores", &ecores) &&
              azt_flag(argc, argv, 1, "--no-pin", &no_pin) &&
              azt_flag(argc, argv, 1, "--no-lock", &no_lock) &&
              azt_flag(argc, argv, 1, "--allow-pageable", &pageable) &&
              azt_flag(argc, argv, 1, "--large-pages", &large) &&
              azt_flag(argc, argv, 1, "--quiet", &quiet) &&
              azt_flag(argc, argv, 1, "--lab", &lab) &&
              azt_str(argc, argv, 1, "--entropy-journal", &journal);
    if (ok && !lab && !load && azt_find(argc, argv, 1, "--seed") >= 0) {
        fprintf(stderr, "--seed needs --lab: a wild run has no seed (wild is the default; --lab is the seeded "
                "test bench)\n");
        ok = false;
    }
    if (ok && lab && journal) {
        fprintf(stderr, "--entropy-journal needs a wild run: a lab run draws no entropy\n");
        ok = false;
    }
    if (!ok) return usage();
    if (stride == 1 || (stride & (stride - 1))) {
        fprintf(stderr, "option --stride: '%u' is not 0 or a power of two from 2 to 128\n", stride);
        return usage();
    }
    if (grid_given && (grid[0] < 2 || grid[1] < 2 || grid[0] + grid[1] + grid[2] > 26)) {
        fprintf(stderr, "option --grid: %u,%u,%u needs x and y of at least 2 bits and at most 26 bits in all\n",
                grid[0], grid[1], grid[2]);
        return usage();
    }
    if (!cap) {
        u64 c = (u64)n + n / 2 + 4096;
        if (c > CAP_MAX) {
            fprintf(stderr, "option --n: %u makes the default cap %" PRIu64 " larger than %u; give --cap\n", n, c,
                    CAP_MAX);
            return usage();
        }
        cap = (u32)c;
    }
    const char *grid_why = grid_given ? "given" : "auto for --n";
    if (!grid_given) azt_grid_for(n, grid);

    AzConfig cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.mode = lab ? AZ_MODE_LAB : AZ_MODE_WILD;
    cfg.seed = lab ? seed : 0;
    cfg.n_initial = n;
    cfg.cap = cap;
    cfg.n_species = species;
    cfg.gx_bits = (u8)grid[0]; cfg.gy_bits = (u8)grid[1]; cfg.gz_bits = (u8)grid[2];
    cfg.reorder_period = (u8)reorder;
    cfg.tier1_period = (u8)k1;
    cfg.tier2_period = (u16)k2;
    cfg.field_stride = (u8)stride;
    cfg.lesion = lesion;
    cfg.max_clades = max_clades;
    cfg.store_units = store_units;
    cfg.store_period = (u16)store_period;
    cfg.debug_split_seed = split;
    cfg.kernels = kernels;

    /* --load: the world config is the snapshot's */
    u8 *snap = NULL;
    u64 snap_n = 0;
    if (load) {
        snap = read_file(load, &snap_n);
        if (!snap) { fprintf(stderr, "--load: cannot read '%s'\n", load); return 2; }
        AzConfig sc;
        if (!az_snapshot_config(snap, snap_n, &sc)) {
            fprintf(stderr, "--load: '%s' is not a valid az snapshot (header, version or checksum)\n", load);
            free(snap);
            return 2;
        }
        if (grid_given && (grid[0] != sc.gx_bits || grid[1] != sc.gy_bits || grid[2] != sc.gz_bits)) {
            fprintf(stderr, "option --grid %u,%u,%u differs from the snapshot's %u,%u,%u; --load takes the world "
                    "from the snapshot\n", grid[0], grid[1], grid[2], sc.gx_bits, sc.gy_bits, sc.gz_bits);
            free(snap);
            return usage();
        }
        if (lab && sc.mode != AZ_MODE_LAB) {
            fprintf(stderr, "option --lab: the snapshot is of a wild world; --load takes the world from the snapshot\n");
            free(snap);
            return usage();
        }
        if (journal && sc.mode == AZ_MODE_LAB) {
            fprintf(stderr, "--entropy-journal needs a wild run: the snapshot is of a lab world\n");
            free(snap);
            return usage();
        }
        ok = same_as_snapshot(argc, argv, "--n", n, sc.n_initial) &&
             same_as_snapshot(argc, argv, "--seed", seed, sc.seed) &&
             same_as_snapshot(argc, argv, "--cap", cap, sc.cap) &&
             same_as_snapshot(argc, argv, "--species", species, sc.n_species) &&
             same_as_snapshot(argc, argv, "--reorder", reorder, sc.reorder_period) &&
             same_as_snapshot(argc, argv, "--k1", k1, sc.tier1_period) &&
             same_as_snapshot(argc, argv, "--k2", k2, sc.tier2_period) &&
             same_as_snapshot(argc, argv, "--stride", stride, sc.field_stride) &&
             same_as_snapshot(argc, argv, "--lesion", lesion, sc.lesion) &&
             same_as_snapshot(argc, argv, "--max-clades", max_clades ? max_clades : 4096u, sc.max_clades) &&
             same_as_snapshot(argc, argv, "--split-seed", split, sc.debug_split_seed) &&
             same_as_snapshot(argc, argv, "--kernels", kernels, sc.kernels);
        if (!ok) { free(snap); return usage(); }
        cfg = sc;
        n = cfg.n_initial;
        grid_why = "from the snapshot";
    }

    u32 hflags = 0;
    if (ecores) hflags |= AZ_WIN_USE_ECORES;
    if (no_pin) hflags |= AZ_WIN_NO_PIN;
    u32 aflags = no_lock ? 0u : AZ_WIN_LOCK;
    if (pageable) aflags |= AZ_WIN_ALLOW_PAGEABLE;
    if (large) aflags |= AZ_WIN_LARGE_PAGES;

    AzWinInfo info;
    memset(&info, 0, sizeof info);
    AzHost *host = az_win_host_create(threads, hflags, &info);
    if (!host) { fprintf(stderr, "host creation failed\n"); free(snap); return 2; }
    if (journal) {
        g_journal = fopen(journal, "wb");
        if (!g_journal) {
            fprintf(stderr, "cannot write the entropy journal %s\n", journal);
            az_win_host_destroy(host);
            free(snap);
            return 2;
        }
        host->entropy_log = journal_log;
    }

    u64 bytes = az_world_bytes(&cfg);
    if (!bytes) {
        fprintf(stderr, "the world config is invalid\n");
        az_win_host_destroy(host);
        free(snap);
        return 2;
    }
    AzArena arena;
    AztTeaching teaching;
    if (!azt_teaching_arena(&arena, bytes, cfg.cap, aflags, &info, true, &teaching)) {
        free(snap);
        if (info.arena_refused) {
            fprintf(stderr, "error: the %.1f MB arena could not be locked in RAM (%s); refusing to run on "
                    "pageable memory. --allow-pageable runs anyway, --no-lock skips the lock.\n",
                    (double)bytes / 1048576.0, info.note);
            az_win_host_destroy(host);
            return 3;
        }
        fprintf(stderr, "arena allocation of %" PRIu64 " bytes failed (error %u): %s\n", bytes, info.last_error,
                info.note);
        az_win_host_destroy(host);
        return 2;
    }
    if (!info.arena_locked)
        fprintf(stderr, "warning: the %.1f MB arena is pageable (%s); the run may touch the pagefile\n",
                (double)arena.size / 1048576.0, info.note);

    /* --ledger: the sink wraps the pool before the world is made, because the
     * world keeps its host; the start chain is set once the world is loaded */
    AzSink *sink = NULL;
    AzHost *run_host = host;
    if (ledger) {
        sink = az_sink_open(ledger, cfg.seed, 0);
        if (!sink) {
            fprintf(stderr, "--ledger: cannot open '%s' for writing\n", ledger);
            free(snap);
            az_win_host_destroy(host);
            az_win_arena_free(&arena);
            return 2;
        }
        run_host = az_sink_host(sink, host);
    }
    AzArena world_arena = arena;
    world_arena.size = bytes;
    AzWorld *w = snap ? az_snapshot_load(&world_arena, snap, snap_n, run_host) : az_world_create(&world_arena, &cfg, run_host);
    free(snap);
    if (w && sink && load && !az_sink_set_start(sink, w->ledger_chain, w->ledger_count)) {
        fprintf(stderr, "--ledger: could not record the snapshot's start chain in '%s'\n", ledger);
        w = NULL;
    } else if (!w) {
        if (load) fprintf(stderr, "--load: the snapshot '%s' was refused (damaged, or another ABI)\n", load);
        else fprintf(stderr, "world creation failed (bad config or arena too small)\n");
    }
    if (!w) {
        if (sink) az_sink_close(sink, NULL);
        az_win_host_destroy(host);
        az_win_arena_free(&arena);
        return 2;
    }
    if (w->rock_repaired_on_load) fprintf(stderr, "--load: repaired %u live positions in bedrock\n", w->rock_repaired_on_load);
    u32 twins = cfg.kernels ? az_kernels_install(w, AZ_K_ALL) : 0;
    if (params && (load || !azt_params(w, params))) {
        if (load) fprintf(stderr, "--param: a loaded world keeps the parameters it was saved with\n");
        if (sink) az_sink_close(sink, NULL);
        az_win_host_destroy(host);
        az_win_arena_free(&arena);
        return 2;
    }
    if (!load) az_worldgen(w);

    u64 matter0 = az_total_matter(w);
    i64 energy0 = az_total_energy(w);

    if (!quiet) {
        printf("asmodensius-native  abi=%u\n", AZ_ABI_VERSION);
        char smt[48] = "";
        if (info.n_smt) snprintf(smt, sizeof smt, " (+%u SMT threads, not used)", info.n_smt);
        printf("cpu: %u logical, %u P cores, %u E cores%s; workers=%u%s, max_parts %u%s%s\n", info.n_logical,
               info.n_pcores, info.n_ecores, smt, host->n_workers,
               (hflags & AZ_WIN_NO_PIN) ? " (OS placement, --no-pin)" : " (ideal processors)", host->max_parts,
               info.pool_note[0] ? "; " : "", info.pool_note);
        printf("arena: %.1f MB, locked=%u, large_pages=%u %s\n", (double)arena.size / 1048576.0,
               info.arena_locked, info.arena_large, info.note);
        /* name what runs (finding v35): the brain step runs every tick; no stage
         * calls bulk Philox yet, so its twin is installed but idle */
        printf("kernels: brain step %s; bulk Philox %s, not called by any stage yet; horizontal physics %s (twin mask 0x%x, cpu "
               "features 0x%x)\n",
               (twins & AZ_K_BRAIN_VNNI) ? "AVX-VNNI twin" : (twins & AZ_K_BRAIN_AVX2) ? "AVX2 twin" : "C reference",
               (twins & AZ_K_PHILOX_AVX2) ? "AVX2 twin installed" : "C reference",
               (twins & AZ_K_KIN_AVX2) ? "AVX2 twin installed" : "C reference", twins, az_cpu_features());
        if (w->cfg.mode == AZ_MODE_WILD)
            printf("mode: WILD - made of fresh entropy from the operating system, 16 more bytes drawn every tick; no "
                   "seed, never the same twice (origin %08x%08x)%s%s\n", w->key[1], w->key[0],
                   journal ? "; entropy journal " : "", journal ? journal : "");
        else
            printf("mode: LAB - the seeded, repeatable test bench (seed %" PRIu64 ")\n", w->cfg.seed);
        printf("world: grid %ux%ux%u (%u cells), cap %u, initial %u, species %u; grid bits %u,%u,%u %s\n",
               w->gx, w->gy, w->gz, w->n_cells, w->cap, n, cfg.n_species,
               (u32)w->gx_bits, (u32)w->gy_bits, (u32)w->gz_bits, grid_why);
        if (load)
            printf("loaded: %s at tick %" PRIu64 ", live %u, hash %016" PRIx64 "\n", load, w->tick, live_at_tick_end(w),
                   az_state_hash(w));
    }

    azt_teaching_attach(&teaching, w, load != NULL);
#ifdef AZ_INTAKE_OBSERVER
    (void)azt_intake_attach(w, !load, teaching.intake_tail, teaching.intake_bytes);
#ifdef AZ_PARTNER_OBSERVER
    (void)azt_partner_attach(w, !load, teaching.partner_tail, teaching.partner_bytes);
#endif
#ifdef AZ_HERMIT_OBSERVER
#ifdef AZ_BUILDER_OBSERVER
    (void)azt_builder_attach(w, teaching.builder_tail, teaching.builder_bytes);
#endif
    (void)azt_hermit_attach(w, teaching.hermit_tail, teaching.hermit_bytes, azt_record_refusal, &teaching);
#endif
#endif
    u64 t_begin = host->now_ns(host->ctx);
    u32 audit_fail = 0;
    for (u32 t = 0; t < ticks; t++) {
        az_tick(w);
        if (audit_every && (w->tick % audit_every) == 0) {
            if (az_total_matter(w) != matter0 || az_total_energy(w) != energy0) audit_fail++;
        }
        if (hash_every && (w->tick % hash_every) == 0)
            printf("tick %" PRIu64 " live %u hash %016" PRIx64 "\n", w->tick, live_at_tick_end(w), az_state_hash(w));
    }
    u64 t_end = host->now_ns(host->ctx);

    double secs = (double)(t_end - t_begin) / 1e9;
    u64 matter1 = az_total_matter(w);
    i64 energy1 = az_total_energy(w);
    AzStats st;
    const u64 census_start = host->now_ns(host->ctx);
    az_stats_collect(w, &st);
#ifdef AZ_INTAKE_OBSERVER
#ifdef AZ_PARTNER_OBSERVER
    azt_partner_prepare(w);
#endif
    azt_intake_apply(w, &st);
    AztTeachingSummary teaching_summary = azt_teaching_apply(&teaching, w, &st, azt_intake_fill);
#else
    AztTeachingSummary teaching_summary = azt_teaching_apply(&teaching, w, &st, NULL);
#endif
    printf("census_ms %.6f\n", (double)(host->now_ns(host->ctx) - census_start) / 1e6);
    printf("census ");
    azt_census_fields(stdout, &st);
    azt_teaching_json(stdout, &teaching_summary);
#ifndef AZ_INTAKE_OBSERVER
    fputs(",\"waste_conversion\":{\"available\":false}",stdout);
#endif
#ifdef AZ_INTAKE_OBSERVER
    azt_intake_json(stdout, w);
#ifdef AZ_COMMONS_OBSERVER
    azt_commons_json(stdout, w);
#endif
#ifdef AZ_PARTNER_OBSERVER
    azt_partner_json(stdout, w);
#endif
#ifdef AZ_HERMIT_OBSERVER
#ifdef AZ_BUILDER_OBSERVER
    azt_builder_json(stdout, w, teaching_summary.builder_covered, teaching_summary.builder_partial,
                     teaching_summary.builder_positive, teaching_summary.builder_exact);
#endif
    azt_hermit_json(stdout, w, teaching_summary.refusal_covered, teaching_summary.refusal_partial,
                    teaching_summary.refusal_positive, teaching_summary.refusal_exact);
#endif
#endif
    fputc('}', stdout);
    putchar('\n');

    if (ticks == 0) {
        printf("ticks 0 in %.3f s; live %u (slots %u); no simulation steps timed\n",
               secs, st.n_live, w->n_slots);
    } else {
        printf("ticks %u in %.3f s = %.1f ticks/s; %.3f ms/tick; live %u (slots %u); %.1f ns per live entity per tick\n",
               ticks, secs, secs > 0.0 ? ticks / secs : 0.0, 1e3 * secs / ticks, st.n_live, w->n_slots,
               st.n_live ? 1e9 * secs / ticks / st.n_live : 0.0);
        printf("stages (ms/tick):");
        for (u32 k = 0; k < AZ_PROF_COUNT; k++)
            printf(" %s=%.3f", azt_prof_name(k), (double)w->prof_ns[k] / 1e6 / ticks);
        printf("\n");
    }
    printf("births %" PRIu64 " deaths %" PRIu64 " cap_denied %" PRIu64 " clades %u colonies %u max_generation %u\n",
           w->totals.births, w->totals.deaths, w->totals.cap_denied, st.n_clades, st.n_colonies, st.max_generation);
    {   /* how unlike each other the living have become: nothing here has a fixed size */
        u64 nl = 0, sg = 0, sh = 0, sp = 0, su = 0, sm = 0;
        u32 gmin = ~0u, gmax = 0, hmin = ~0u, hmax = 0, pmin = ~0u, pmax = 0, umin = ~0u, umax = 0, tmax = 0;
        for (u32 s = 0; s < w->n_slots; s++) {
            if (!(w->flags[s] & AZ_F_ALIVE) || !w->blk[s]) continue;
            u32 ga, gb, bb, np;
            (void)az_genes(w, s, 0, &ga); (void)az_genes(w, s, 1, &gb);
            const AzBrain *br = az_brain(w, s, &bb);
            (void)az_parts(w, s, &np);
            const u32 g = ga + gb, h = bb >= sizeof(AzBrain) ? 8u * br->n_hb : 0u, u = az_blk(w, s)->units;
            const u32 t = bb >= sizeof(AzBrain) ? br->n_t1 + br->n_t2 : 0u;
            nl++; sg += g; sh += h; sp += np; su += u; sm += az_body_matter(w, s);
            if (g < gmin) gmin = g;
            if (g > gmax) gmax = g;
            if (h < hmin) hmin = h;
            if (h > hmax) hmax = h;
            if (np < pmin) pmin = np;
            if (np > pmax) pmax = np;
            if (u < umin) umin = u;
            if (u > umax) umax = u;
            if (t > tmax) tmax = t;
        }
        if (nl)
            printf("creatures: genes %u..%u (mean %.1f), neurons %u..%u (mean %.1f, most tiles %u), body parts %u..%u "
                   "(mean %.2f, matter in parts %" PRIu64 "), block %u..%u units (mean %.1f = %.0f bytes), fixed record %u bytes\n",
                   gmin, gmax, (double)sg / (double)nl, hmin, hmax, (double)sh / (double)nl, tmax, pmin, pmax,
                   (double)sp / (double)nl, sm, umin, umax, (double)su / (double)nl, 64.0 * (double)su / (double)nl,
                   (unsigned)AZ_FIXED_RECORD_BYTES);
    }
    if (w->cfg.mode == AZ_MODE_WILD)
        printf("entropy: %" PRIu64 " ticks drew fresh entropy, %" PRIu64 " had none%s\n",
               (u64)ticks - (w->totals.entropy_missed < ticks ? w->totals.entropy_missed : ticks),
               w->totals.entropy_missed, journal ? " (journal written)" : "");
    if (g_journal) { fclose(g_journal); g_journal = NULL; }
    printf("store: %u of %u units live (%.1f MB of %.1f MB per half), denied: store %" PRIu64 " clade %" PRIu64
           " colony %" PRIu64 "\n",
           w->store_live, w->store_units, (double)w->store_live * AZ_UNIT / 1048576.0,
           (double)w->store_units * AZ_UNIT / 1048576.0,
           w->totals.store_denied, w->totals.clade_denied, w->totals.colony_denied);
    /* resolved pair interactions by intent, counted on the initiator's side
     * (the reference stubs propose nothing, so every count is 0 until wave 2) */
    u64 pairs = 0;
    printf("pairs:");
    for (u32 a = 1; a < AZ_ACT_COUNT; a++) {
        printf(" %s %" PRIu64, azt_act_name(a), w->totals.interactions[a]);
        pairs += w->totals.interactions[a];
    }
    printf(" (total %" PRIu64 ")\n", pairs);
    printf("matter: %" PRIu64 " -> %" PRIu64 " (%s)   energy invariant: %" PRId64 " -> %" PRId64 " (%s)\n",
           matter0, matter1, matter0 == matter1 ? "exact" : "VIOLATED",
           energy0, energy1, energy0 == energy1 ? "exact" : "VIOLATED");
    printf("field_underflow %" PRIu64 " dropped_events %" PRIu64 " audit_failures %u\n",
           w->totals.field_underflow, w->totals.dropped_events, audit_fail);
    const u64 h_final = az_state_hash(w);
    printf("final hash %016" PRIx64 "\n", h_final);

    int rc = (matter0 == matter1 && energy0 == energy1 && !audit_fail && !w->totals.field_underflow) ? 0 : 1;
#ifdef AZ_INTAKE_OBSERVER
#ifdef AZ_HERMIT_OBSERVER
#ifdef AZ_BUILDER_OBSERVER
    azt_builder_detach();
#endif
    azt_hermit_detach();
#endif
#ifdef AZ_PARTNER_OBSERVER
    azt_partner_detach();
#endif
    azt_intake_detach();
#endif
    azt_teaching_detach(&teaching);
    if (sink) {
        AzSinkStats ls;
        const bool closed = az_sink_close(sink, &ls);    /* drains the ring first */
        sink = NULL;
        const bool same = ls.chain == w->ledger_chain;
        printf("ledger: %" PRIu64 " records written to %s, %" PRIu64 " dropped, %u write errors; chain %016" PRIx64
               " %s the world's %016" PRIx64 "\n",
               ls.written, ledger, ls.dropped, ls.write_errors, ls.chain, same ? "equals" : "DIFFERS FROM",
               w->ledger_chain);
        if (!closed || ls.dropped || ls.write_errors || !same) {
            fprintf(stderr, "--ledger: the ledger '%s' is incomplete\n", ledger);
            rc = 1;
        }
    }
    if (save) {
        u64 need = az_snapshot_bytes(w), got = 0;
        u8 *buf = need ? malloc((size_t)need) : NULL;
        if (buf) got = az_snapshot_save(w, buf, need);
        FILE *f = (got == need && got) ? fopen(save, "wb") : NULL;
        bool written = f && fwrite(buf, 1, (size_t)got, f) == (size_t)got;
        if (f && fclose(f) != 0) written = false;
        free(buf);
        if (written) {
            printf("snapshot: %" PRIu64 " bytes written to %s at tick %" PRIu64 ", final hash %016" PRIx64 "\n", got, save,
                   w->tick, h_final);
        } else {
            fprintf(stderr, "--save: the snapshot was NOT written to '%s'\n", save);
            rc = 1;
        }
    }
    az_win_host_destroy(host);
    az_win_arena_free(&arena);
    return rc;
}
