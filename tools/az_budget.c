/* az_budget.c - byte budget of the world, read from the column registry.
 *
 *   az_budget [--workers 8] [--cap-mode run|exact] [--csv file]
 *
 * Prints, from az_column / az_cell_column on a tiny world:
 *   - bytes per entity by tier (HOT, WARM, EPH), the public record, bytes per cell
 *   - for N = 10k .. 1M: hot MB, hot + scratch + public MB, whole arena MB
 *     (az_world_bytes), the share per performance core, and the smallest cache
 *     level that share fits in
 *   - the population from which the hot tier no longer fits the private L2 of
 *     its core, and from which it no longer fits all cache together.
 *
 * Nothing here is measured: it is arithmetic on the registry and on the
 * cache sizes. MB means MiB (1048576 bytes) everywhere, KB means KiB.
 * Hosted C (floats allowed); reads the world, never ticks it.
 */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "../core/az_abi.h"
#include "../platform/win/az_host_win.h"
#include "az_tools.h"

#define KIB 1024.0
#define MIB 1048576.0

/* Cache sizes of the target CPU (Intel Core Ultra 9 275HX), bytes. AzWinInfo
 * carries core counts but no cache sizes, so these are constants; the tool
 * prints what Windows reports next to them as a cross-check. */
#define C_P_L0D   (48u * 1024u)            /* per P core, first-level data        */
#define C_P_L1D   (192u * 1024u)           /* per P core, second-level data       */
#define C_P_L2    (3u * 1024u * 1024u)     /* per P core, private L2              */
#define C_E_L1D   (32u * 1024u)            /* per E core                          */
#define C_E_L2    (4u * 1024u * 1024u)     /* shared by a cluster of four E cores */
#define C_L3      (36u * 1024u * 1024u)    /* shared by every core                */
#define C_N_P     8u
#define C_N_ECLU  4u
#define C_ALL     ((u64)C_N_P * C_P_L2 + (u64)C_N_ECLU * C_E_L2 + C_L3)   /* 76 MB */

static const u32 pops[] = { 10000u, 50000u, 100000u, 250000u, 500000u, 1000000u };
#define N_POPS (sizeof pops / sizeof pops[0])

static const char *level_of(double bytes)
{
    if (bytes <= C_P_L0D) return "L0 48 KB";
    if (bytes <= C_P_L1D) return "L1 192 KB";
    if (bytes <= C_P_L2)  return "L2 3 MB";
    return NULL;
}

/* where does a per-core share live: a private level, or only with shared help */
static const char *fit_text(double share, double total, char *buf, size_t cap)
{
    const char *lv = level_of(share);
    if (lv) return lv;
    if (total <= (double)C_N_P * C_P_L2 + C_L3)
        snprintf(buf, cap, "L2 + L3 (%.0f%% in L2)", 100.0 * C_P_L2 / share);
    else if (total <= (double)C_ALL)
        snprintf(buf, cap, "all cache (%.0f%%)", 100.0 * total / (double)C_ALL);
    else
        snprintf(buf, cap, "DRAM (%.1fx all cache)", total / (double)C_ALL);
    return buf;
}

/* What Windows says about the caches: one line per distinct (level, type, size). */
static void print_os_caches(void)
{
    DWORD len = 0;
    GetLogicalProcessorInformationEx(RelationCache, NULL, &len);
    if (!len) { printf("  (Windows reported no cache information)\n"); return; }
    BYTE *buf = (BYTE *)malloc(len);
    if (!buf) return;
    if (!GetLogicalProcessorInformationEx(RelationCache, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)buf, &len)) {
        printf("  (GetLogicalProcessorInformationEx failed, error %lu)\n", (unsigned long)GetLastError());
        free(buf);
        return;
    }
    struct { u32 level, type, size, count, lps; } seen[32];
    u32 n_seen = 0;
    for (DWORD off = 0; off < len;) {
        PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX e = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)(buf + off);
        if (e->Relationship == RelationCache) {
            u32 lps = (u32)__builtin_popcountll((unsigned long long)e->Cache.GroupMask.Mask);
            u32 k = 0;
            for (; k < n_seen; k++)
                if (seen[k].level == e->Cache.Level && seen[k].type == (u32)e->Cache.Type &&
                    seen[k].size == e->Cache.CacheSize && seen[k].lps == lps) break;
            if (k == n_seen && n_seen < 32) {
                seen[k].level = e->Cache.Level; seen[k].type = (u32)e->Cache.Type;
                seen[k].size = e->Cache.CacheSize; seen[k].lps = lps; seen[k].count = 0;
                n_seen++;
            }
            if (k < n_seen) seen[k].count++;
        }
        off += e->Size;
    }
    free(buf);
    static const char *tname[] = { "unified", "instruction", "data", "trace" };
    for (u32 k = 0; k < n_seen; k++)
        printf("  L%u %-11s %8.0f KB  x %2u  (each shared by %u logical processor%s)\n",
               seen[k].level, tname[seen[k].type & 3], seen[k].size / KIB, seen[k].count,
               seen[k].lps, seen[k].lps == 1 ? "" : "s");
}

int main(int argc, char **argv)
{
    /* strict options (az_tools.h): anything else is refused with exit code 2 */
    static const char *const with_value[] = { "--workers", "--cap-mode", "--csv", NULL };
    static const char *const flags[] = { "--help", "-h", NULL };
    if (!azt_known(argc, argv, 1, with_value, flags)) return 2;
    bool help = false, help_h = false;
    if (!azt_flag(argc, argv, 1, "--help", &help) || !azt_flag(argc, argv, 1, "-h", &help_h)) return 2;
    if (help || help_h) {
        printf("az_budget [--workers 8] [--cap-mode run|exact] [--csv file]\n"
               "  --workers W    workers the hot tier is split over, 1 to %u (default 8 = the P cores)\n"
               "  --cap-mode M   run: arena for cap = n + n/2 + 4096 (what az_run and az_bench allocate)\n"
               "                 exact: arena for cap = n\n"
               "  --csv file     also write the population table as CSV\n", (unsigned)AZ_MAX_WORKERS);
        return 0;
    }
    u32 workers = C_N_P;
    const char *cap_mode = "run", *csv_path = NULL;
    if (!azt_u32(argc, argv, 1, "--workers", 1, AZ_MAX_WORKERS, &workers) ||
        !azt_str(argc, argv, 1, "--cap-mode", &cap_mode) ||
        !azt_str(argc, argv, 1, "--csv", &csv_path))
        return 2;
    if (strcmp(cap_mode, "run") != 0 && strcmp(cap_mode, "exact") != 0) {
        fprintf(stderr, "option --cap-mode: '%s' is not run or exact\n", cap_mode);
        return 2;
    }
    bool cap_exact = !strcmp(cap_mode, "exact");

    /* a tiny world, only so that the registry can be asked through its API */
    AzConfig cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.mode = AZ_MODE_LAB;
    cfg.seed = 1; cfg.cap = 64; cfg.n_initial = 0; cfg.n_species = 1;
    cfg.gx_bits = 2; cfg.gy_bits = 2; cfg.gz_bits = 1;
    cfg.reorder_period = 1; cfg.field_stride = 2;
    u64 tiny = az_world_bytes(&cfg);
    AzArena arena;
    arena.base = (u8 *)_aligned_malloc((size_t)tiny, 4096);
    arena.size = tiny; arena.used = 0;
    AzWorld *w = arena.base ? az_world_create(&arena, &cfg, NULL) : NULL;
    if (!w) { fprintf(stderr, "could not create the tiny world (%" PRIu64 " bytes)\n", tiny); return 2; }

    u32 tier_bytes[5] = { 0, 0, 0, 0, 0 }, tier_cols[5] = { 0, 0, 0, 0, 0 };
    static const char *tier_name[5] = { "HOT", "WARM", "EPH", "FIELD", "HND" };

    printf("asmodensius-native byte budget  (abi %u; MB = MiB, KB = KiB; arithmetic, not a measurement)\n\n",
           AZ_ABI_VERSION);
    printf("entity columns (%u):\n", az_n_columns());
    printf("  %-11s %-5s %5s %4s %6s\n", "column", "tier", "elem", "per", "bytes");
    for (u32 k = 0; k < az_n_columns(); k++) {
        AzColDesc d;
        az_column(w, k, &d);
        u32 b = d.elem * d.per;
        if (d.tier < 5) { tier_bytes[d.tier] += b; tier_cols[d.tier]++; }
        printf("  %-11s %-5s %5u %4u %6u\n", d.name, d.tier < 5 ? tier_name[d.tier] : "?", d.elem, d.per, b);
    }
    u32 hot = tier_bytes[AZ_TIER_HOT], warm = tier_bytes[AZ_TIER_WARM], eph = tier_bytes[AZ_TIER_EPH];
    u32 hnd = tier_bytes[AZ_TIER_HND];
    u32 pub = (u32)sizeof(AzPub), ord = 4;
    u32 traffic = hot + eph + pub + ord;

    u32 cell_field = 0, cell_eph = 0;
    printf("\ncell columns (%u):\n", az_n_cell_columns());
    printf("  %-11s %-5s %5s %4s %6s\n", "column", "tier", "elem", "per", "bytes");
    for (u32 k = 0; k < az_n_cell_columns(); k++) {
        AzColDesc d;
        az_cell_column(w, k, &d);
        u32 b = d.elem * d.per;
        if (d.tier == AZ_TIER_EPH) cell_eph += b; else cell_field += b;
        printf("  %-11s %-5s %5u %4u %6u\n", d.name, d.tier < 5 ? tier_name[d.tier] : "?", d.elem, d.per, b);
    }

    printf("\nbytes per entity:\n");
    printf("  HOT   %4u   (%u columns; read or written every tick)\n", hot, tier_cols[AZ_TIER_HOT]);
    printf("  WARM  %4u   (%u columns; events, birth, death, Tier 1)\n", warm, tier_cols[AZ_TIER_WARM]);
    printf("  EPH   %4u   (%u columns; per-tick scratch)\n", eph, tier_cols[AZ_TIER_EPH]);
    printf("  HND   %4u   (%u column; the handle of the creature's block in the store)\n", hnd, tier_cols[AZ_TIER_HND]);
    printf("  fixed record read every tick = HOT + HND = %u (bookkeeping only: no genome, brain or body in it)\n",
           hot + hnd);
    printf("  public record %2u, sorted order entry %u\n", pub, ord);
    printf("  per tick traffic = HOT + EPH + public + order = %u\n", traffic);
    printf("  persistent state = HOT + WARM + HND = %u, plus the creature's own block (any size; az_run prints\n"
           "    the measured spread)\n", hot + warm + hnd);
    printf("  reorder scratch (tmp) 128 per slot of capacity\n");
    printf("bytes per cell:\n");
    printf("  FIELD %4u   EPH %4u   total %u   (plus 4 for cell_start)\n", cell_field, cell_eph,
           cell_field + cell_eph);
    printf("shared tables: lineage record %u B, colony %u B + %u B scratch\n",
           (u32)sizeof(AzClade), (u32)sizeof(AzColony), (u32)sizeof(AzColonyEph));
    printf("creature store: one block per creature behind the handle `blk`, in units of %u B: genome (4 B per\n"
           "  gene), brain (64 B header, 32 B per tile of connections), mind, body (8 B per part). A brain step\n"
           "  reads the creature's own block. az_run prints the measured spread of block sizes.\n", (u32)AZ_UNIT);

    AzWinInfo info;
    memset(&info, 0, sizeof info);
    az_win_topology(&info);
    printf("\ncaches used for the table (constants for the Core Ultra 9 275HX; az_win_topology has no cache sizes):\n");
    printf("  P core: L0 data %u KB, L1 data %u KB, private L2 %u MB   (x %u)\n", C_P_L0D / 1024u,
           C_P_L1D / 1024u, C_P_L2 >> 20, C_N_P);
    printf("  E core: L1 data %u KB, L2 %u MB shared by four          (x %u clusters)\n", C_E_L1D / 1024u,
           C_E_L2 >> 20, C_N_ECLU);
    printf("  L3 %u MB shared; all cache together %.0f MB\n", C_L3 >> 20, (double)C_ALL / MIB);
    printf("this machine (az_win_topology): %u logical, %u P cores, %u E cores%s\n", info.n_logical,
           info.n_pcores, info.n_ecores,
           (info.n_pcores == C_N_P && info.n_ecores == 16) ? "" : "   <-- NOT the CPU the constants describe");
    printf("this machine (Windows cache report, for comparison):\n");
    print_os_caches();

    FILE *csv = NULL;
    if (csv_path) {
        csv = fopen(csv_path, "w");
        if (!csv) { fprintf(stderr, "cannot write %s\n", csv_path); return 2; }
        fprintf(csv, "n,cap,grid_bits_x,grid_bits_y,grid_bits_z,cells,per_cell,hot_mb,traffic_mb,cells_mb,"
                     "arena_mb,workers,hot_share_mb,hot_fits,traffic_share_mb,traffic_fits\n");
    }

    printf("\npopulation table: %u workers; arena for cap = %s; grid = az_bench rule (1 to 2 per cell)\n",
           workers, cap_exact ? "n" : "n + n/2 + 4096");
    printf("  %8s %-9s %8s %9s %9s %9s | %9s %-22s | %9s %-22s\n", "n", "grid", "hot MB", "h+s+p MB",
           "cells MB", "arena MB", "hot/core", "hot fits", "h+s+p/core", "h+s+p fits");
    for (u32 i = 0; i < N_POPS; i++) {
        u32 n = pops[i], bits[3];
        azt_grid_for(n, bits);   /* the az_bench rule, 1 to 2 per cell (az_tools.h) */
        AzConfig c;
        memset(&c, 0, sizeof c);
        c.mode = AZ_MODE_LAB;
        c.cap = cap_exact ? n : n + n / 2 + 4096;
        c.n_initial = n;
        c.gx_bits = (u8)bits[0]; c.gy_bits = (u8)bits[1]; c.gz_bits = (u8)bits[2];
        u64 arena_b = az_world_bytes(&c);
        u64 cells = 1ull << (bits[0] + bits[1] + bits[2]);
        double hot_b = (double)n * hot, tr_b = (double)n * traffic;
        double cells_b = (double)cells * (cell_field + cell_eph + 4);
        char g[24], f1[48], f2[48];
        snprintf(g, sizeof g, "%u,%u,%u", bits[0], bits[1], bits[2]);
        const char *hf = fit_text(hot_b / workers, hot_b, f1, sizeof f1);
        const char *tf = fit_text(tr_b / workers, tr_b, f2, sizeof f2);
        printf("  %8u %-9s %8.2f %9.2f %9.2f %9.1f | %6.3f MB %-22s | %6.3f MB %-22s\n", n, g, hot_b / MIB,
               tr_b / MIB, cells_b / MIB, (double)arena_b / MIB, hot_b / workers / MIB, hf,
               tr_b / workers / MIB, tf);
        if (csv)
            fprintf(csv, "%u,%u,%u,%u,%u,%" PRIu64 ",%.3f,%.3f,%.3f,%.3f,%.3f,%u,%.4f,%s,%.4f,%s\n", n, c.cap,
                    bits[0], bits[1], bits[2], cells, (double)n / (double)cells, hot_b / MIB, tr_b / MIB,
                    cells_b / MIB, (double)arena_b / MIB, workers, hot_b / workers / MIB, hf,
                    tr_b / workers / MIB, tf);
    }
    if (csv) { fclose(csv); printf("  (table written to %s)\n", csv_path); }

    /* the thresholds, in creatures */
    u64 n_l0 = (u64)workers * C_P_L0D / hot, n_l1 = (u64)workers * C_P_L1D / hot;
    u64 n_l2 = (u64)workers * C_P_L2 / hot, n_all = C_ALL / hot;
    u64 t_l2 = (u64)workers * C_P_L2 / traffic, t_all = C_ALL / traffic;
    printf("\nthresholds (hot tier of %u bytes split evenly over %u P cores):\n", hot, workers);
    printf("  fits the first-level data caches (%u KB each) up to %" PRIu64 " creatures\n", C_P_L0D / 1024u, n_l0);
    printf("  fits the second-level data caches (%u KB each) up to %" PRIu64 " creatures\n", C_P_L1D / 1024u, n_l1);
    printf("  The hot tier no longer fits the private L2 of its core from %" PRIu64 " creatures on.\n", n_l2 + 1);
    printf("  The hot tier no longer fits all cache together (%.0f MB) from %" PRIu64 " creatures on.\n",
           (double)C_ALL / MIB, n_all + 1);
    printf("  With scratch and public record (%u bytes per creature) the same two limits are %" PRIu64
           " and %" PRIu64 ".\n", traffic, t_l2 + 1, t_all + 1);
    printf("  Workers on P cores reach only their own L2 and the L3 (%.0f MB, not the E-core L2): hot tier up to\n"
           "  %" PRIu64 " creatures, with scratch and public record up to %" PRIu64 ".\n",
           ((double)C_N_P * C_P_L2 + C_L3) / MIB, ((u64)C_N_P * C_P_L2 + C_L3) / hot,
           ((u64)C_N_P * C_P_L2 + C_L3) / traffic);
    printf("  One %u-slot block: %u B hot, %u B with scratch and public record (first-level data cache: %u B).\n",
           (u32)AZ_CHUNK, AZ_CHUNK * hot, AZ_CHUNK * traffic, C_P_L0D);
    printf("notes: the limits ignore the cell fields, the clade weight tiles and the WARM tier, which share the\n"
           "       same caches; L3 is shared with every other process; \"fits\" is capacity, not a hit rate.\n");

    _aligned_free(arena.base);
    return 0;
}
