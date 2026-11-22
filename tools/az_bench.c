/* az_bench.c - throughput benchmark of the tick pipeline.
 *
 *   az_bench [--n 10000,50000,100000] [--threads 1,P,PE] [--kernels 0]
 *            [--ticks 200] [--warmup 50] [--repeats 1] [--seed 42] [--csv file]
 *            [--max-s 0] [--audit-every 0] [--grid x,y,z] [--species 100]
 *            [--reorder 16] [--k1 32] [--k2 256] [--stride 8] [--lesion 0x0]
 *            [--no-lock] [--allow-pageable] [--large-pages] [--no-pin] [--quiet]
 *
 * For every population, kernel setting and thread setting it builds a fresh
 * world (grid chosen so that a cell holds 1 to 2 creatures), runs the warm-up
 * ticks, then times every measured tick on its own. It reports ticks per
 * second, nanoseconds per creature-tick, milliseconds per pipeline stage
 * (w->prof_ns), the arena size and whether it is page-locked, and it checks
 * the two conservation invariants. docs/BENCH.md says how to read the output
 * and what makes a measurement valid.
 *
 * Hosted C (floats allowed). The benchmark only observes: it calls az_tick
 * and reads the world between ticks.
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
#include "../kernels/az_kernels.h"
#include "az_tools.h"
#include "az_statsutil.h"

#define MIB 1048576.0
#define MAX_LIST 16

/* ---------------------------------------------------------------------- */
/* command line                                                            */
/* ---------------------------------------------------------------------- */
/* Split "a,b,c" into at most MAX_LIST items of at most 15 characters. An
 * empty list, an empty item ("a,,b", a leading or trailing comma), an item
 * that is too long and too many items are refused (one line on stderr). */
static bool split_list(const char *opt, const char *s, char out[MAX_LIST][16], u32 *n_out)
{
    u32 n = 0;
    const char *p = s;
    for (;;) {
        u32 len = 0;
        while (p[len] && p[len] != ',') len++;
        if (len == 0 || len >= 16 || n == MAX_LIST) {
            fprintf(stderr, "option %s: '%s' is not a list of 1 to %u comma-separated items of 1 to 15 characters\n",
                    opt, s, MAX_LIST);
            return false;
        }
        memcpy(out[n], p, len);
        out[n][len] = 0;
        n++;
        if (!p[len]) break;
        p += len + 1;
    }
    *n_out = n;
    return true;
}

/* every item a whole number in [lo, hi] */
static bool list_numbers(const char *opt, char items[MAX_LIST][16], u32 n, u64 lo, u64 hi, u32 out[MAX_LIST])
{
    for (u32 i = 0; i < n; i++) {
        u64 v = 0;
        if (!azt_parse_u64(items[i], &v) || v < lo || v > hi) {
            fprintf(stderr, "option %s: item '%s' is not a whole number from %llu to %llu\n", opt, items[i],
                    (unsigned long long)lo, (unsigned long long)hi);
            return false;
        }
        out[i] = (u32)v;
    }
    return true;
}

/* every item P, p, PE, pe, all, or a worker count from 1 to AZ_MAX_WORKERS */
static bool list_threads(char items[MAX_LIST][16], u32 n)
{
    for (u32 i = 0; i < n; i++) {
        const char *t = items[i];
        if (!strcmp(t, "P") || !strcmp(t, "p") || !strcmp(t, "PE") || !strcmp(t, "pe") || !strcmp(t, "all"))
            continue;
        u64 v = 0;
        if (!azt_parse_u64(t, &v) || v < 1 || v > AZ_MAX_WORKERS) {
            fprintf(stderr, "option --threads: item '%s' is not P, PE, all or a worker count from 1 to %u\n", t,
                    (unsigned)AZ_MAX_WORKERS);
            return false;
        }
    }
    return true;
}

/* ---------------------------------------------------------------------- */
/* one configuration, one run                                              */
/* ---------------------------------------------------------------------- */
typedef struct Setup {
    u32 n, kernels;
    char threads[16];        /* "1", "P", "PE" or a worker count                */
    u32 ticks, warmup, audit_every;
    u64 seed;
    u32 species, reorder, k1, k2, stride, lesion;
    u32 grid[3];             /* 0,0,0 = pick by the 1-to-2-per-cell rule        */
    double max_s;            /* stop measuring after this many seconds; 0 = off */
    bool lock, large, pin, allow_pageable;
} Setup;

typedef struct Result {
    bool ok;                 /* the run happened (allocation, creation)         */
    u32 workers, twins, bits[3], cells, cap;
    u32 warm_done, ticks_done, live_start, live_end;
    bool truncated;          /* a phase hit --max-s before its tick count       */
    double other_cores;      /* CPU the rest of the machine used meanwhile      */
    double total_s, tps, ms_mean, ms_median, ms_p95, ms_max, ns_per_ct;
    double stage_ms[AZ_PROF_COUNT], stage_sum_ms;
    double arena_mb;
    u32 locked, large;
    bool matter_ok, energy_ok;
    u32 audits, audit_fail;
    u64 underflow, births, deaths, cap_denied, hash;
    char note[160];
} Result;

/* CPU time in 100 ns units: the whole machine busy, and this process */
static u64 ft64(FILETIME f) { return ((u64)f.dwHighDateTime << 32) | f.dwLowDateTime; }
static void cpu_times(u64 *machine_busy, u64 *self)
{
    FILETIME idle, kern, user, c, e, pk, pu;
    *machine_busy = 0; *self = 0;
    if (GetSystemTimes(&idle, &kern, &user)) *machine_busy = ft64(kern) + ft64(user) - ft64(idle);
    if (GetProcessTimes(GetCurrentProcess(), &c, &e, &pk, &pu)) *self = ft64(pk) + ft64(pu);
}

static void run_one(const Setup *s, Result *r)
{
    memset(r, 0, sizeof *r);

    AzWinInfo info;
    memset(&info, 0, sizeof info);
    az_win_topology(&info);
    u32 want = 0, hflags = s->pin ? 0 : AZ_WIN_NO_PIN;
    if (!strcmp(s->threads, "P") || !strcmp(s->threads, "p")) want = 0;
    else if (!strcmp(s->threads, "PE") || !strcmp(s->threads, "pe") || !strcmp(s->threads, "all")) {
        want = info.n_pcores + info.n_ecores;
        hflags |= AZ_WIN_USE_ECORES;
    } else {
        u64 v = 1;                     /* the same parser list_threads checked it with */
        if (!azt_parse_u64(s->threads, &v) || v < 1 || v > AZ_MAX_WORKERS) v = 1;
        want = (u32)v;
        if (want > info.n_pcores) hflags |= AZ_WIN_USE_ECORES;
    }
    AzHost *host = az_win_host_create(want, hflags, &info);
    if (!host) { snprintf(r->note, sizeof r->note, "host creation failed"); return; }
    r->workers = host->n_workers;

    AzConfig cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.mode = AZ_MODE_LAB;     /* a measurement is taken on a world that can be repeated */
    cfg.seed = s->seed;
    cfg.n_initial = s->n;
    cfg.cap = s->n + s->n / 2 + 4096;
    cfg.n_species = (u16)s->species;
    if (s->grid[0]) memcpy(r->bits, s->grid, sizeof r->bits);
    else azt_grid_for(s->n, r->bits);   /* 1 to 2 creatures per cell (az_tools.h) */
    cfg.gx_bits = (u8)r->bits[0]; cfg.gy_bits = (u8)r->bits[1]; cfg.gz_bits = (u8)r->bits[2];
    cfg.reorder_period = (u8)s->reorder;
    cfg.tier1_period = (u8)s->k1;
    cfg.tier2_period = (u16)s->k2;
    cfg.field_stride = (u8)s->stride;
    cfg.lesion = s->lesion;
    cfg.kernels = s->kernels;

    u64 bytes = az_world_bytes(&cfg);
    AzArena arena;
    u32 aflags = (s->lock ? AZ_WIN_LOCK : 0) | (s->large ? AZ_WIN_LARGE_PAGES : 0) |
                 (s->allow_pageable ? AZ_WIN_ALLOW_PAGEABLE : 0);
    if (!bytes || !az_win_arena(&arena, bytes, aflags, &info)) {
        if (info.arena_refused)
            snprintf(r->note, sizeof r->note, "arena not locked, run refused (--allow-pageable runs it): %.100s",
                     info.note);
        else
            snprintf(r->note, sizeof r->note, "arena of %" PRIu64 " bytes failed (error %u)", bytes, info.last_error);
        az_win_host_destroy(host);
        return;
    }
    AzWorld *w = az_world_create(&arena, &cfg, host);
    double *dur = (double *)malloc((size_t)(s->ticks ? s->ticks : 1) * sizeof(double));   /* ns, exact below 2^53 */
    if (!w || !dur) {
        snprintf(r->note, sizeof r->note, "world creation failed");
        free(dur);
        az_win_arena_free(&arena);
        az_win_host_destroy(host);
        return;
    }
    r->twins = cfg.kernels ? az_kernels_install(w, AZ_K_ALL) : 0;
    az_worldgen(w);

    r->cells = w->n_cells;
    r->cap = w->cap;
    r->arena_mb = (double)arena.size / MIB;
    r->locked = info.arena_locked;
    r->large = info.arena_large;
    snprintf(r->note, sizeof r->note, "%s", info.note);

    const u64 matter0 = az_total_matter(w);
    const i64 energy0 = az_total_energy(w);

    const u64 limit_ns = (u64)(s->max_s * 1e9);
    const u64 warm_begin = host->now_ns(host->ctx);
    for (; r->warm_done < s->warmup; ) {
        az_tick(w);
        r->warm_done++;
        if (limit_ns && host->now_ns(host->ctx) - warm_begin >= limit_ns) break;
    }
    r->audits++;
    if (az_total_matter(w) != matter0 || az_total_energy(w) != energy0) r->audit_fail++;
    r->live_start = w->n_live;

    u64 prof0[AZ_PROF_COUNT];
    for (u32 k = 0; k < AZ_PROF_COUNT; k++) prof0[k] = w->prof_ns[k];

    u64 total_ns = 0, creature_ticks = 0;
    u64 busy0, self0, busy1, self1;
    cpu_times(&busy0, &self0);
    const u64 wall0 = host->now_ns(host->ctx);
    u32 done = 0;
    for (; done < s->ticks; done++) {
        u64 t0 = host->now_ns(host->ctx);
        az_tick(w);
        u64 t1 = host->now_ns(host->ctx);
        dur[done] = (double)(t1 - t0);
        total_ns += t1 - t0;
        creature_ticks += w->n_live;      /* counted by this tick's grid build */
        if (s->audit_every && ((done + 1) % s->audit_every) == 0) {
            r->audits++;
            if (az_total_matter(w) != matter0 || az_total_energy(w) != energy0) r->audit_fail++;
        }
        if (limit_ns && total_ns >= limit_ns) { done++; break; }
    }
    const u64 wall1 = host->now_ns(host->ctx);
    cpu_times(&busy1, &self1);
    if (wall1 > wall0) {
        double other = (double)(busy1 - busy0) - (double)(self1 - self0);   /* 100 ns units */
        r->other_cores = other > 0.0 ? other * 100.0 / (double)(wall1 - wall0) : 0.0;
    }
    r->ticks_done = done;
    r->truncated = (r->warm_done < s->warmup) || (done < s->ticks);
    r->live_end = w->n_live;

    const u64 matter1 = az_total_matter(w);
    const i64 energy1 = az_total_energy(w);
    r->audits++;
    r->matter_ok = (matter1 == matter0);
    r->energy_ok = (energy1 == energy0);
    if (!r->matter_ok || !r->energy_ok) r->audit_fail++;
    r->underflow = w->totals.field_underflow;
    r->births = w->totals.births;
    r->deaths = w->totals.deaths;
    r->cap_denied = w->totals.cap_denied;
    r->hash = az_state_hash(w);

    if (done) {
        r->total_s = (double)total_ns / 1e9;
        r->tps = (double)done / r->total_s;
        r->ms_mean = 1e3 * r->total_s / done;
        r->ns_per_ct = creature_ticks ? (double)total_ns / (double)creature_ticks : 0.0;
        for (u32 k = 0; k < AZ_PROF_COUNT; k++) {
            r->stage_ms[k] = (double)(w->prof_ns[k] - prof0[k]) / 1e6 / done;
            r->stage_sum_ms += r->stage_ms[k];
        }
        /* quantiles of the single tick times: az_statsutil.h, type 7 */
        qsort(dur, done, sizeof(double), azs_cmp_double);
        r->ms_median = azs_quantile_sorted(dur, (int)done, 0.50) / 1e6;
        r->ms_p95 = azs_quantile_sorted(dur, (int)done, 0.95) / 1e6;
        r->ms_max = azs_quantile_sorted(dur, (int)done, 1.0) / 1e6;
    }
    r->ok = true;

    free(dur);
    az_win_host_destroy(host);
    az_win_arena_free(&arena);
}

static bool result_clean(const Result *r)
{
    return r->ok && r->matter_ok && r->energy_ok && r->audit_fail == 0 && r->underflow == 0;
}

/* Other processes using more than this many cores during a run make it
 * unfit as a measurement (see docs/BENCH.md). */
#define LOAD_LIMIT_CORES 0.5

/* "ok", or one letter per reason the run is not a valid measurement:
 * L other load, B on battery, U arena not locked, T truncated by --max-s */
static const char *validity(const Result *r, int mains, char buf[8])
{
    u32 n = 0;
    if (r->other_cores >= LOAD_LIMIT_CORES) buf[n++] = 'L';
    if (mains == 0) buf[n++] = 'B';
    if (!r->locked) buf[n++] = 'U';
    if (r->truncated) buf[n++] = 'T';
    if (n == 0) { buf[0] = 'o'; buf[1] = 'k'; n = 2; }
    buf[n] = 0;
    return buf;
}

typedef struct Cfg {
    Setup s;
    Result *rep;          /* one per repeat                                   */
    u32 n_ok, med;        /* usable repeats; index of the median repeat       */
    double tps_min, tps_max, spread_pct;
} Cfg;

static void csv_header(FILE *f)
{
    fprintf(f, "row,n,threads,workers,pinned,kernels,twin_mask,seed,grid_bits_x,grid_bits_y,grid_bits_z,cells,"
               "per_cell,cap,warmup_ticks,ticks,total_s,ticks_per_s,tps_min,tps_max,spread_pct,"
               "ns_per_creature_tick,ms_tick_mean,ms_tick_median,ms_tick_p95,ms_tick_max");
    for (u32 k = 0; k < AZ_PROF_COUNT; k++) fprintf(f, ",ms_%s", azt_prof_name(k));
    fprintf(f, ",ms_stage_sum,arena_mb,locked,large_pages,live_start,live_end,births,deaths,cap_denied,"
               "matter_ok,energy_ok,field_underflow,audits,audit_failures,other_load_cores,on_mains,"
               "valid,state_hash\n");
}

static void csv_row(FILE *f, const char *label, const Cfg *c, const Result *r, int mains, bool summary)
{
    char v[8];
    fprintf(f, "%s,%u,%s,%u,%u,%u,0x%x,%" PRIu64 ",%u,%u,%u,%u,%.4f,%u,%u,%u,%.6f,%.3f,", label, c->s.n,
            c->s.threads, r->workers, c->s.pin ? 1u : 0u, c->s.kernels, r->twins, c->s.seed, r->bits[0],
            r->bits[1], r->bits[2], r->cells, r->cells ? (double)c->s.n / r->cells : 0.0, r->cap, r->warm_done,
            r->ticks_done, r->total_s, r->tps);
    if (summary) fprintf(f, "%.3f,%.3f,%.2f,", c->tps_min, c->tps_max, c->spread_pct);
    else fprintf(f, ",,,");
    fprintf(f, "%.2f,%.4f,%.4f,%.4f,%.4f", r->ns_per_ct, r->ms_mean, r->ms_median, r->ms_p95, r->ms_max);
    for (u32 k = 0; k < AZ_PROF_COUNT; k++) fprintf(f, ",%.4f", r->stage_ms[k]);
    fprintf(f, ",%.4f,%.1f,%u,%u,%u,%u,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%u,%u,%" PRIu64 ",%u,%u,%.2f,%d,%s,%016" PRIx64 "\n",
            r->stage_sum_ms, r->arena_mb, r->locked, r->large, r->live_start, r->live_end, r->births, r->deaths,
            r->cap_denied, r->matter_ok ? 1u : 0u, r->energy_ok ? 1u : 0u, r->underflow, r->audits,
            r->audit_fail, r->other_cores, mains, validity(r, mains, v), r->hash);
}

static void usage(void)
{
    printf("az_bench [options]\n"
           "  --n LIST          populations, e.g. 10000,100000,1000000   (default 10000,50000,100000)\n"
           "  --threads LIST    thread settings: 1, P (all P cores), PE (P plus E cores) or a count\n"
           "                    (default 1,P,PE)\n"
           "  --kernels LIST    0 = C reference, 1 = assembly twins; e.g. 0,1   (default 0)\n"
           "  --ticks T         measured ticks per run (default 200)\n"
           "  --warmup T        ticks run before measuring (default 50)\n"
           "  --repeats R       runs per configuration, 1 to 64; the table shows the run closest to\n"
           "                    the median ticks/s (default 1)\n"
           "  --max-s S         end the warm-up and the measurement early after S whole seconds each\n"
           "                    (0 to 86400); such a run is marked T (truncated); 0 = no limit (default 30)\n"
           "  --audit-every K   also check the invariants every K measured ticks (outside the timing)\n"
           "  --grid x,y,z      grid bits for every population (default: 1 to 2 creatures per cell)\n"
           "  --seed S  --species N  --reorder P  --k1 K  --k2 K  --stride S  --lesion MASK\n"
           "  --no-lock         do not page-lock the arena     --large-pages   ask for 2 MB pages\n"
           "  --allow-pageable  run even when the lock fails (such runs are marked U)\n"
           "  --no-pin          no ideal processors (the OS places the workers)\n"
           "  --quiet           no per-run lines\n"
           "  --csv FILE        write every run and every median as CSV\n");
}

int main(int argc, char **argv)
{
    /* Strict options (az_tools.h, findings v34 and v56): unknown options,
     * stray values, garbage, partial and repeated values and values out of
     * range are refused with one line and exit code 2. */
    static const char *const with_value[] = {
        "--n", "--threads", "--kernels", "--ticks", "--warmup", "--repeats", "--seed", "--csv", "--max-s",
        "--audit-every", "--grid", "--species", "--reorder", "--k1", "--k2", "--stride", "--lesion", NULL
    };
    static const char *const flags[] = {
        "--no-lock", "--allow-pageable", "--large-pages", "--no-pin", "--quiet", "--help", "-h", NULL
    };
    if (!azt_known(argc, argv, 1, with_value, flags)) return 2;
    bool help = false, help_h = false;
    if (!azt_flag(argc, argv, 1, "--help", &help) || !azt_flag(argc, argv, 1, "-h", &help_h)) return 2;
    if (help || help_h) { usage(); return 0; }

    Setup base;
    memset(&base, 0, sizeof base);
    base.ticks = 200; base.warmup = 50; base.audit_every = 0; base.seed = 42; base.species = 100;
    base.reorder = 16; base.k1 = 32; base.k2 = 256; base.stride = 8; base.lesion = 0;
    u32 repeats = 1;
    double max_s = 30.0;
    bool no_lock = false, no_pin = false, quiet = false;
    const char *csv_path = NULL;
    const char *n_list = "10000,50000,100000", *th_list = "1,P,PE", *k_list = "0";
    const u32 lesion_all = AZ_LESION_ALL;
    bool grid_given = false;
    if (!azt_u32(argc, argv, 1, "--ticks", 1, 1000000, &base.ticks) ||
        !azt_u32(argc, argv, 1, "--warmup", 0, 1000000, &base.warmup) ||
        !azt_u32(argc, argv, 1, "--audit-every", 0, 1000000, &base.audit_every) ||
        !azt_u64(argc, argv, 1, "--seed", 0, UINT64_MAX, &base.seed) ||
        !azt_u32(argc, argv, 1, "--species", 1, 512, &base.species) ||   /* 512 = default clade limit */
        !azt_u32(argc, argv, 1, "--reorder", 0, 255, &base.reorder) ||   /* AzConfig u8; 0 = core default 1 */
        !azt_u32(argc, argv, 1, "--k1", 0, 255, &base.k1) ||             /* AzConfig u8; 0 = core default 32 */
        !azt_u32(argc, argv, 1, "--k2", 0, 65535, &base.k2) ||           /* AzConfig u16; 0 = core default 256 */
        !azt_u32(argc, argv, 1, "--stride", 0, 128, &base.stride) ||
        !azt_u32(argc, argv, 1, "--lesion", 0, lesion_all, &base.lesion) ||
        !azt_f64(argc, argv, 1, "--max-s", 0.0, 86400.0, &max_s) ||       /* seconds, fractions allowed */
        !azt_u32(argc, argv, 1, "--repeats", 1, 64, &repeats) ||         /* the median search holds 64 */
        !azt_flag(argc, argv, 1, "--no-lock", &no_lock) ||
        !azt_flag(argc, argv, 1, "--large-pages", &base.large) ||
        !azt_flag(argc, argv, 1, "--no-pin", &no_pin) ||
        !azt_flag(argc, argv, 1, "--allow-pageable", &base.allow_pageable) ||
        !azt_flag(argc, argv, 1, "--quiet", &quiet) ||
        !azt_str(argc, argv, 1, "--csv", &csv_path) ||
        !azt_str(argc, argv, 1, "--n", &n_list) ||
        !azt_str(argc, argv, 1, "--threads", &th_list) ||
        !azt_str(argc, argv, 1, "--kernels", &k_list) ||
        !azt_grid(argc, argv, 1, "--grid", 12, base.grid, &grid_given))
        return 2;
    if (base.stride != 0 && (base.stride < 2 || (base.stride & (base.stride - 1)) != 0)) {
        fprintf(stderr, "option --stride: '%u' is not 0 or a power of two from 2 to 128\n", base.stride);
        return 2;
    }
    if (grid_given && (base.grid[0] < 2 || base.grid[1] < 2 || base.grid[0] + base.grid[1] + base.grid[2] > 26)) {
        fprintf(stderr, "option --grid: x and y need at least 2 bits and x + y + z at most 26\n");
        return 2;
    }
    if (!grid_given) base.grid[0] = base.grid[1] = base.grid[2] = 0;
    base.max_s = max_s;
    base.lock = !no_lock;
    base.pin = !no_pin;

    char ns[MAX_LIST][16], ths[MAX_LIST][16], ks[MAX_LIST][16];
    u32 n_ns = 0, n_th = 0, n_ks = 0, nv[MAX_LIST], kv[MAX_LIST];
    if (!split_list("--n", n_list, ns, &n_ns) || !list_numbers("--n", ns, n_ns, 1, 100000000, nv) ||
        !split_list("--threads", th_list, ths, &n_th) || !list_threads(ths, n_th) ||
        !split_list("--kernels", k_list, ks, &n_ks) || !list_numbers("--kernels", ks, n_ks, 0, 1, kv))
        return 2;

    const u32 n_cfg = n_ns * n_ks * n_th;
    Cfg *cfgs = (Cfg *)calloc(n_cfg, sizeof(Cfg));
    Result *all = (Result *)calloc((size_t)n_cfg * repeats, sizeof(Result));
    if (!cfgs || !all) { fprintf(stderr, "out of memory\n"); return 2; }
    for (u32 a = 0; a < n_ns; a++)
        for (u32 b = 0; b < n_ks; b++)
            for (u32 c = 0; c < n_th; c++) {
                Cfg *cf = &cfgs[(a * n_ks + b) * n_th + c];
                cf->s = base;
                cf->s.n = nv[a];
                cf->s.kernels = kv[b];
                memcpy(cf->s.threads, ths[c], sizeof cf->s.threads);
                cf->rep =&all[(size_t)((a * n_ks + b) * n_th + c) * repeats];
            }

    AzWinInfo info;
    memset(&info, 0, sizeof info);
    az_win_topology(&info);
    SYSTEM_POWER_STATUS sps;
    int mains = -1;                        /* 1 mains, 0 battery, -1 unknown */
    if (GetSystemPowerStatus(&sps) && sps.ACLineStatus != 255) mains = sps.ACLineStatus ? 1 : 0;

    printf("az_bench  abi=%u  cpu: %u logical, %u P cores, %u E cores, features 0x%x  power: %s\n",
           AZ_ABI_VERSION, info.n_logical, info.n_pcores, info.n_ecores, az_cpu_features(),
           mains == 1 ? "mains" : mains == 0 ? "BATTERY" : "unknown");
    printf("ticks %u after %u warm-up, %u repeat%s, seed %" PRIu64 ", workers %s, arena %s, max %.0f s per phase\n",
           base.ticks, base.warmup, repeats, repeats == 1 ? "" : "s", base.seed,
           base.pin ? "ideal processors" : "OS placement", base.lock ? "locked" : "not locked", base.max_s);

    /* Repeats are the outer loop inside a population, so the runs of one
     * configuration are spread in time and a slow drift (heat, a background
     * task) does not land on one configuration only. */
    int rc = 0;
    for (u32 a = 0; a < n_ns; a++)
        for (u32 rep = 0; rep < repeats; rep++)
            for (u32 bc = 0; bc < n_ks * n_th; bc++) {
                Cfg *cf = &cfgs[a * n_ks * n_th + bc];
                Result *r = &cf->rep[rep];
                run_one(&cf->s, r);
                if (!r->ok) {
                    printf("  run n=%u thr=%s k=%u rep %u/%u FAILED: %s\n", cf->s.n, cf->s.threads,
                           cf->s.kernels, rep + 1, repeats, r->note);
                    rc = 2;
                } else {
                    char v[8];
                    if (!result_clean(r)) rc = 1;
                    if (!quiet)
                        printf("  run n=%u thr=%s wk=%u k=%u rep %u/%u: %u ticks, %.1f ticks/s, %.1f ns/creature-tick, "
                               "other load %.1f cores [%s]%s\n", cf->s.n, cf->s.threads, r->workers,
                               cf->s.kernels, rep + 1, repeats, r->ticks_done, r->tps, r->ns_per_ct,
                               r->other_cores, validity(r, mains, v), result_clean(r) ? "" : "  INVARIANT VIOLATED");
                }
                fflush(stdout);
            }

    /* Median and spread of every configuration (az_statsutil.h, type 7).
     * The table and the stage breakdown show one whole run, so the shown run
     * is the usable repeat whose ticks/s is closest to the median ticks/s
     * (the slower one on a tie, which for an even count is the lower middle). */
    for (u32 i = 0; i < n_cfg; i++) {
        Cfg *cf = &cfgs[i];
        double tps[64];
        u32 n = 0;
        for (u32 rep = 0; rep < repeats; rep++)   /* repeats <= 64 (option range) */
            if (cf->rep[rep].ok && cf->rep[rep].ticks_done) tps[n++] = cf->rep[rep].tps;
        cf->n_ok = n;
        if (!n) continue;
        qsort(tps, n, sizeof(double), azs_cmp_double);
        const double med = azs_quantile_sorted(tps, (int)n, 0.5);
        double best = -1.0;
        for (u32 rep = 0; rep < repeats; rep++) {
            const Result *r = &cf->rep[rep];
            if (!r->ok || !r->ticks_done) continue;
            double d = r->tps > med ? r->tps - med : med - r->tps;
            if (best < 0.0 || d < best || (d == best && r->tps < cf->rep[cf->med].tps)) { best = d; cf->med = rep; }
        }
        cf->tps_min = azs_quantile_sorted(tps, (int)n, 0.0);
        cf->tps_max = azs_quantile_sorted(tps, (int)n, 1.0);
        cf->spread_pct = 100.0 * (cf->tps_max - cf->tps_min) / med;
    }

    bool any_invalid = false;
    printf("\nresults (per configuration the run of %u repeat%s closest to the median ticks/s; spread = (max - min)\n"
           "         / median of ticks/s; med ms and p95 ms = quantiles 0.5 and 0.95 of the single tick times of\n"
           "         that run; every median and quantile: linear interpolation between order statistics at\n"
           "         h = (n - 1) q (Hyndman-Fan type 7, the R default))\n",
           repeats, repeats == 1 ? "" : "s");
    printf("%9s %-4s %3s %2s %-8s %10s %8s %10s %9s %9s %9s %6s %9s %-10s %-5s\n", "n", "thr", "wk", "k",
           "grid", "ticks/s", "spread%", "ns/c-tick", "med ms", "p95 ms", "arena MB", "locked", "live",
           "invariants", "valid");
    for (u32 i = 0; i < n_cfg; i++) {
        const Cfg *cf = &cfgs[i];
        if (!cf->n_ok) { printf("%9u %-4s  no usable run\n", cf->s.n, cf->s.threads); continue; }
        const Result *r = &cf->rep[cf->med];
        char g[24], v[8], vall[8];
        snprintf(g, sizeof g, "%u,%u,%u", r->bits[0], r->bits[1], r->bits[2]);
        /* a configuration is valid only if every repeat is; invariants likewise */
        bool clean = true, valid = true;
        for (u32 rep = 0; rep < repeats; rep++) {
            if (!result_clean(&cf->rep[rep])) clean = false;
            if (strcmp(validity(&cf->rep[rep], mains, v), "ok") != 0) { valid = false; memcpy(vall, v, sizeof v); }
        }
        if (valid) memcpy(vall, "ok", 3); else any_invalid = true;
        printf("%9u %-4s %3u %2u %-8s %10.1f %8.1f %10.1f %9.3f %9.3f %9.1f %6s %9u %-10s %-5s\n", cf->s.n,
               cf->s.threads, r->workers, cf->s.kernels, g, r->tps, cf->spread_pct, r->ns_per_ct, r->ms_median,
               r->ms_p95, r->arena_mb, r->locked ? "yes" : "NO", r->live_end, clean ? "exact" : "VIOLATED", vall);
    }

    printf("\nstages of the median run (ms per tick; sum = the ten stages, tick = whole az_tick, mean)\n");
    printf("%9s %-4s %2s", "n", "thr", "k");
    for (u32 k = 0; k < AZ_PROF_COUNT; k++) printf(" %8s", azt_prof_name(k));
    printf(" %8s %8s\n", "sum", "tick");
    for (u32 i = 0; i < n_cfg; i++) {
        const Cfg *cf = &cfgs[i];
        if (!cf->n_ok) continue;
        const Result *r = &cf->rep[cf->med];
        printf("%9u %-4s %2u", cf->s.n, cf->s.threads, cf->s.kernels);
        for (u32 k = 0; k < AZ_PROF_COUNT; k++) printf(" %8.3f", r->stage_ms[k]);
        printf(" %8.3f %8.3f\n", r->stage_sum_ms, r->ms_mean);
    }

    /* Determinism for free: with the same seed and tick count every complete
     * run of one population must end in the same state, whatever the thread
     * setting, the kernels or the repeat. */
    printf("\nstate hash after %u ticks, per population (threads, kernels and repeats must agree):\n",
           base.warmup + base.ticks);
    for (u32 a = 0; a < n_ns; a++) {
        u64 ref = 0;
        u32 n_full = 0, n_diff = 0;
        for (u32 j = 0; j < n_ks * n_th * repeats; j++) {
            const Result *r = &all[(size_t)a * n_ks * n_th * repeats + j];
            if (!r->ok || r->truncated) continue;
            if (n_full == 0) ref = r->hash; else if (r->hash != ref) n_diff++;
            n_full++;
        }
        const u32 pop = cfgs[a * n_ks * n_th].s.n;
        if (n_full == 0) printf("  n=%u: no complete run\n", pop);
        else if (n_diff == 0) printf("  n=%u: %016" PRIx64 "  %u complete run%s agree\n", pop, ref, n_full,
                                     n_full == 1 ? " (nothing to compare)" : "s");
        else { printf("  n=%u: %u of %u complete runs DIFFER from %016" PRIx64 "\n", pop, n_diff, n_full, ref); rc = 1; }
    }

    printf("\nvalid: ok, or L = other processes used >= %.1f cores, B = on battery, U = arena not locked, "
           "T = truncated by --max-s\n", LOAD_LIMIT_CORES);
    if (any_invalid) printf("NOT a valid measurement (see the valid column): label these numbers \"noisy\".\n");
    if (repeats < 3) printf("fewer than three repeats: report these numbers as a single sample, not as a result.\n");
    printf("invariants: %s\n", rc == 1 ? "VIOLATED or state hashes differ (exit code 1)" :
                               rc == 2 ? "some runs failed to start (exit code 2)" : "matter and energy exact in every run");

    if (csv_path) {
        FILE *f = fopen(csv_path, "w");
        if (!f) { fprintf(stderr, "cannot write %s\n", csv_path); return 2; }
        csv_header(f);
        for (u32 i = 0; i < n_cfg; i++) {
            const Cfg *cf = &cfgs[i];
            for (u32 rep = 0; rep < repeats; rep++) {
                char label[16];
                snprintf(label, sizeof label, "rep%u", rep + 1);
                if (cf->rep[rep].ok) csv_row(f, label, cf, &cf->rep[rep], mains, false);
            }
            if (cf->n_ok) csv_row(f, "median", cf, &cf->rep[cf->med], mains, true);
        }
        fclose(f);
        printf("csv written to %s\n", csv_path);
    }
    free(all);
    free(cfgs);
    return rc;
}

