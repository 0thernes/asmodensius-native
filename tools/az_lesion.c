/* az_lesion.c - lesion evidence tool (review finding v27, 2026-11-17).
 *
 *   az_lesion [--seeds 5101-5124] [--n 3000] [--ticks 64] [--grid 4,4,3]
 *             [--species 8] [--threads 0] [--ecores] [--no-pin] [--kernels 0]
 *             [--only NAME,NAME] [--csv FILE] [--no-lock] [--allow-pageable]
 *             [--list] [--quiet]
 *
 * For every AZ_LESION_* switch (one row of the table below) and every seed
 * the tool runs two worlds that differ only in that bit: the intact world and
 * the lesioned one, with the same seed, configuration and thread count. At
 * the last tick it reads seven outcome measures and the state hash. Intact
 * and lesioned are compared per seed (paired by seed) with tools/az_statsutil.h:
 * the exact two-sided Wilcoxon signed-rank test (ties by midranks, zero
 * differences dropped), Holm's step-down adjustment over the family of
 * lesions, the paired effect size d_z = mean(lesioned - intact) /
 * sd(lesioned - intact), and a seeded bootstrap interval of the mean
 * difference.
 *
 * Every row names its primary measure in advance. Only the primary measure
 * enters the Holm family; the other measures are printed as description. Do
 * not change a row's primary measure after looking at results (that would
 * turn the test into a search); a module owner who makes a mechanism real
 * picks its measure before running the tool on reporting seeds.
 *
 * Verdict per lesion, by the owner's rule (docs/ARCHITECTURE.md sections 10
 * and 11, handoff plan E1):
 *   EFFECT     |d_z| >= 0.8 and Holm-adjusted p < 0.05 on the primary measure
 *   INERT      the lesioned world ends in exactly the intact state (same state
 *              hash) on every seed: nothing reads the bit yet, or the
 *              mechanism never acted in these runs
 *   NO EFFECT  the final state differs on some seed, but the primary measure
 *              does not pass the rule
 * Seeds: tune on 1..99; report on the preregistered blocks 5001..5024 and
 * 5101..5124 only. The default is 5101..5124 (24 pairs; the smallest exact
 * two-sided p is 2 / 2^24 = 1.19e-7). Other seeds are marked exploratory.
 *
 * tests/test_statsutil.c checks that every AZ_LESION_* in core/az_abi.h has a
 * row here: a line indented by four spaces that starts with LESION(NAME), and
 * no other line written that way. The tool itself checks at start that the
 * rows hold distinct single bits.
 *
 * Hosted C (floats allowed). The tool only observes: it creates worlds, ticks
 * them and reads them. Exit codes: 0 done, 1 an invariant failed in some run,
 * 2 bad arguments or setup failed, 3 the arena could not be locked.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>
#include "../core/az_abi.h"
#include "../platform/win/az_host_win.h"
#include "../kernels/az_kernels.h"
#include "az_statsutil.h"

/* ---------------------------------------------------------------------- */
/* outcome measures, read at the last tick                                 */
/* ---------------------------------------------------------------------- */
enum { M_LIVE, M_BIRTHS, M_DEATHS, M_ENERGY, M_GEN, M_CLADES, M_INTER, M_COUNT };
static const char *const measure_name[M_COUNT] = {
    "live", "births", "deaths", "energy", "generation", "clades", "interactions"
};
static const char *const measure_what[M_COUNT] = {
    "live creatures", "births so far", "deaths so far", "mean energy per live creature",
    "deepest generation", "clades in use", "pair interactions so far"
};

typedef struct LesionRow {
    const char *name;
    u32 bit;
    u32 measure;          /* primary measure, chosen before any reporting run */
    const char *what;
} LesionRow;

/* LESION(name) opens a row with the switch's name and bit; the row then
 * gives the primary measure and what the switch turns off. */
#define LESION(name) { #name, AZ_LESION_##name,
static const LesionRow lesions[] = {
    LESION(BRAIN)      M_ENERGY, "motor outputs forced to zero" },
    LESION(RECURRENCE) M_ENERGY, "hidden state cleared every tick" },
    LESION(PLASTICITY) M_ENERGY, "no lifetime learning" },
    LESION(SOCIAL)     M_INTER,  "no pair interactions at all" },
    LESION(TRADE)      M_INTER,  "trade proposals discarded" },
    LESION(MATING)     M_BIRTHS, "no sexual reproduction" },
    LESION(MUTATION)   M_CLADES, "no mutation" },
    LESION(WEATHER)    M_ENERGY, "weather off" },
    LESION(CHEM)       M_ENERGY, "cell chemistry off" },
    LESION(SIGNALS)    M_INTER,  "signals off" },
    LESION(COLONY)     M_LIVE,   "colony pass off" },
    LESION(TIER1_Q)    M_ENERGY, "classical state-vector module off" },
    LESION(TIER1_SH)   M_ENERGY, "spherical-harmonic descriptors off" },
    LESION(MEMORY)     M_ENERGY, "associative memory off" },
    LESION(GRAVITY)    M_ENERGY, "gravity off" },
    LESION(NOISE)      M_ENERGY, "intrinsic noise input off" },
    LESION(KIN)        M_INTER,  "kin recognition off" },
    LESION(TOXIN)      M_DEATHS, "toxins off" },
    LESION(METRIC)     M_ENERGY, "plasticity without its metric preconditioner" },
    LESION(CARE)       M_DEATHS, "no parental care" },
    LESION(SENSES)     M_ENERGY, "all exteroceptive inputs zero" },
    LESION(TAXIS)      M_ENERGY, "field-gradient receptor gain set to zero" },
    LESION(THRUST)     M_ENERGY, "powered translation suppressed" },
};
#undef LESION
#define N_LESIONS ((u32)(sizeof lesions / sizeof lesions[0]))

#define MAX_SEEDS AZS_WSR_MAX
#define RULE_D    0.8
#define RULE_P    0.05

/* ---------------------------------------------------------------------- */
/* command line: every option is checked; a bad value is an error          */
/* ---------------------------------------------------------------------- */
typedef struct Opts {
    u64 seeds[MAX_SEEDS];
    u32 n_seeds;
    u32 n, ticks, species, threads, kernels;
    unsigned grid[3];
    bool ecores, no_pin, no_lock, allow_pageable, list, quiet;
    bool only[N_LESIONS];
    bool any_only;
    const char *csv;
} Opts;

static void usage(void)
{
    printf("az_lesion: paired intact-versus-lesioned runs for every AZ_LESION_* switch\n"
           "  --seeds A-B[,C-D|,S]  seeds (default 5101-5124; at most %d, at least 2)\n"
           "  --n N                 initial creatures (default 3000)\n"
           "  --ticks T             ticks per run (default 64)\n"
           "  --grid x,y,z          grid bits (default 4,4,3)\n"
           "  --species S           founder species (default 8)\n"
           "  --threads K           workers, 0 = one per P core (default 0); --ecores --no-pin\n"
           "  --kernels 0|1         assembly twins (default 0)\n"
           "  --only NAME,NAME      only these lesions (names as in --list)\n"
           "  --csv FILE            write every run (seed, lesion, measures, hash)\n"
           "  --no-lock, --allow-pageable   as in az_run\n"
           "  --list                print the table and exit\n"
           "  --quiet               no per-seed progress\n", MAX_SEEDS);
}

static bool parse_u64(const char *s, u64 lo, u64 hi, u64 *out)
{
    if (!s || !*s) return false;
    char *end = NULL;
    unsigned long long v = strtoull(s, &end, 0);
    if (*end || v < lo || v > hi || s[0] == '-') return false;
    *out = v;
    return true;
}

static bool parse_u32(const char *s, u32 lo, u32 hi, u32 *out)
{
    u64 v;
    if (!parse_u64(s, lo, hi, &v)) return false;
    *out = (u32)v;
    return true;
}

static bool parse_seeds(const char *s, Opts *o)
{
    char buf[512];
    if (strlen(s) >= sizeof buf) return false;
    strcpy(buf, s);
    o->n_seeds = 0;
    for (char *tok = strtok(buf, ","); tok; tok = strtok(NULL, ",")) {
        char *dash = strchr(tok, '-');
        u64 a, b;
        if (dash) {
            *dash = 0;
            if (!parse_u64(tok, 0, UINT64_MAX, &a) || !parse_u64(dash + 1, 0, UINT64_MAX, &b) || b < a) return false;
        } else {
            if (!parse_u64(tok, 0, UINT64_MAX, &a)) return false;
            b = a;
        }
        for (u64 v = a; v <= b; v++) {
            if (o->n_seeds >= MAX_SEEDS) return false;
            o->seeds[o->n_seeds++] = v;
            if (v == UINT64_MAX) break;
        }
    }
    return o->n_seeds >= 2;
}

static int find_lesion(const char *name)
{
    for (u32 i = 0; i < N_LESIONS; i++) if (!strcmp(lesions[i].name, name)) return (int)i;
    return -1;
}

static bool parse_only(const char *s, Opts *o)
{
    char buf[512];
    if (strlen(s) >= sizeof buf) return false;
    strcpy(buf, s);
    for (char *tok = strtok(buf, ","); tok; tok = strtok(NULL, ",")) {
        int i = find_lesion(tok);
        if (i < 0) { fprintf(stderr, "unknown lesion '%s' (see --list)\n", tok); return false; }
        o->only[i] = true;
        o->any_only = true;
    }
    return o->any_only;
}

static bool parse_args(int argc, char **argv, Opts *o)
{
    memset(o, 0, sizeof *o);
    parse_seeds("5101-5124", o);
    o->n = 3000; o->ticks = 64; o->species = 8;
    o->grid[0] = 4; o->grid[1] = 4; o->grid[2] = 3;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        const char *v = i + 1 < argc ? argv[i + 1] : NULL;
        bool ok = true, takes = true;
        if (!strcmp(a, "--seeds")) ok = v && parse_seeds(v, o);
        else if (!strcmp(a, "--n")) ok = parse_u32(v, 1, 1u << 26, &o->n);
        else if (!strcmp(a, "--ticks")) ok = parse_u32(v, 1, 1u << 24, &o->ticks);
        else if (!strcmp(a, "--species")) ok = parse_u32(v, 1, 1000, &o->species);
        else if (!strcmp(a, "--threads")) ok = parse_u32(v, 0, AZ_MAX_WORKERS, &o->threads);
        else if (!strcmp(a, "--kernels")) ok = parse_u32(v, 0, 1, &o->kernels);
        else if (!strcmp(a, "--grid")) {
            char tail = 0;
            ok = v && sscanf(v, "%u,%u,%u%c", &o->grid[0], &o->grid[1], &o->grid[2], &tail) == 3;
        }
        else if (!strcmp(a, "--only")) ok = v && parse_only(v, o);
        else if (!strcmp(a, "--csv")) { ok = v != NULL; o->csv = v; }
        else {
            takes = false;
            if (!strcmp(a, "--ecores")) o->ecores = true;
            else if (!strcmp(a, "--no-pin")) o->no_pin = true;
            else if (!strcmp(a, "--no-lock")) o->no_lock = true;
            else if (!strcmp(a, "--allow-pageable")) o->allow_pageable = true;
            else if (!strcmp(a, "--list")) o->list = true;
            else if (!strcmp(a, "--quiet")) o->quiet = true;
            else if (!strcmp(a, "--help") || !strcmp(a, "-h")) { usage(); exit(0); }
            else { fprintf(stderr, "unknown option '%s' (--help lists them)\n", a); return false; }
        }
        if (!ok) { fprintf(stderr, "bad or missing value for %s\n", a); return false; }
        if (takes) i++;
    }
    return true;
}

/* ---------------------------------------------------------------------- */
/* one run                                                                 */
/* ---------------------------------------------------------------------- */
typedef struct Run {
    double m[M_COUNT];
    u64 hash;
    bool conserved;
    u64 underflow, dropped;
} Run;

static AzConfig make_config(const Opts *o, u64 seed, u32 lesion)
{
    AzConfig cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.mode = AZ_MODE_LAB;     /* a lesion contrast compares two runs of the same world */
    cfg.seed = seed;
    cfg.n_initial = o->n;
    cfg.cap = o->n + o->n / 2 + 4096;
    cfg.n_species = (u16)o->species;
    cfg.gx_bits = (u8)o->grid[0]; cfg.gy_bits = (u8)o->grid[1]; cfg.gz_bits = (u8)o->grid[2];
    cfg.reorder_period = 16;
    cfg.tier1_period = 32;
    cfg.tier2_period = 256;
    cfg.field_stride = 8;
    cfg.lesion = lesion;
    cfg.kernels = o->kernels;
    return cfg;
}

static bool run_world(const Opts *o, AzArena *arena, AzHost *host, u64 seed, u32 lesion, Run *r)
{
    memset(r, 0, sizeof *r);
    AzConfig cfg = make_config(o, seed, lesion);
    arena->used = 0;                       /* every world starts from zeroed columns */
    AzWorld *w = az_world_create(arena, &cfg, host);
    if (!w) return false;
    if (cfg.kernels) az_kernels_install(w, AZ_K_ALL);
    az_worldgen(w);
    const u64 matter0 = az_total_matter(w);
    const i64 energy0 = az_total_energy(w);
    for (u32 t = 0; t < o->ticks; t++) az_tick(w);
    AzStats st;
    az_stats_collect(w, &st);
    u64 inter = 0;
    for (u32 a = 0; a < AZ_ACT_COUNT; a++) inter += w->totals.interactions[a];
    r->m[M_LIVE] = (double)st.n_live;
    r->m[M_BIRTHS] = (double)w->totals.births;
    r->m[M_DEATHS] = (double)w->totals.deaths;
    r->m[M_ENERGY] = (double)st.mean_energy_q8 / 256.0;
    r->m[M_GEN] = (double)st.max_generation;
    r->m[M_CLADES] = (double)st.n_clades;
    r->m[M_INTER] = (double)inter;
    r->hash = az_state_hash(w);
    r->conserved = az_total_matter(w) == matter0 && az_total_energy(w) == energy0;
    r->underflow = w->totals.field_underflow;
    r->dropped = w->totals.dropped_events;
    return true;
}

/* ---------------------------------------------------------------------- */
/* report                                                                  */
/* ---------------------------------------------------------------------- */
static bool reporting_seed(u64 s) { return (s >= 5001 && s <= 5024) || (s >= 5101 && s <= 5124); }

static void fmt_d(char *buf, size_t n, double d)
{
    if (isnan(d)) snprintf(buf, n, "nan");
    else if (isinf(d)) snprintf(buf, n, "%sinf", d > 0 ? "+" : "-");
    else snprintf(buf, n, "%+.3f", d);
}

static bool table_ok(void)
{
    for (u32 i = 0; i < N_LESIONS; i++) {
        const u32 b = lesions[i].bit;
        if (b == 0 || (b & (b - 1)) != 0 || lesions[i].measure >= M_COUNT) return false;
        for (u32 j = 0; j < i; j++) if (lesions[j].bit == b) return false;
    }
    return true;
}

int main(int argc, char **argv)
{
    Opts o;
    if (!parse_args(argc, argv, &o)) return 2;
    if (!table_ok()) { fprintf(stderr, "internal error: the lesion table has a repeated or malformed bit\n"); return 2; }

    if (o.list) {
        printf("%-11s %-9s %-13s %s\n", "lesion", "bit", "primary", "switches off");
        for (u32 i = 0; i < N_LESIONS; i++)
            printf("%-11s 0x%06x  %-13s %s\n", lesions[i].name, lesions[i].bit,
                   measure_name[lesions[i].measure], lesions[i].what);
        printf("%u switches; measures:", N_LESIONS);
        for (u32 m = 0; m < M_COUNT; m++) printf(" %s = %s;", measure_name[m], measure_what[m]);
        printf("\n");
        return 0;
    }

    u32 sel[N_LESIONS], n_sel = 0;
    for (u32 i = 0; i < N_LESIONS; i++) if (!o.any_only || o.only[i]) sel[n_sel++] = i;

    AzWinInfo info;
    memset(&info, 0, sizeof info);
    u32 hflags = (o.ecores ? AZ_WIN_USE_ECORES : 0) | (o.no_pin ? AZ_WIN_NO_PIN : 0);
    AzHost *host = az_win_host_create(o.threads, hflags, &info);
    if (!host) { fprintf(stderr, "host creation failed\n"); return 2; }

    AzConfig probe = make_config(&o, o.seeds[0], 0);
    const u64 bytes = az_world_bytes(&probe);
    if (!bytes) { fprintf(stderr, "bad world configuration (grid %u,%u,%u)\n", o.grid[0], o.grid[1], o.grid[2]); return 2; }
    AzArena arena;
    u32 aflags = (o.no_lock ? 0 : AZ_WIN_LOCK) | (o.allow_pageable ? AZ_WIN_ALLOW_PAGEABLE : 0);
    if (!az_win_arena(&arena, bytes, aflags, &info)) {
        fprintf(stderr, "%s: %s\n", info.arena_refused ? "error: the arena could not be locked in RAM; refusing to run "
                "(--allow-pageable runs anyway)" : "arena allocation failed", info.note);
        az_win_host_destroy(host);
        return info.arena_refused ? 3 : 2;
    }
    if (!info.arena_locked) fprintf(stderr, "warning: the arena is pageable (%s)\n", info.note);

    bool all_reporting = true, any_tuning = false;
    for (u32 s = 0; s < o.n_seeds; s++) {
        if (!reporting_seed(o.seeds[s])) all_reporting = false;
        if (o.seeds[s] >= 1 && o.seeds[s] <= 99) any_tuning = true;
    }
    printf("az_lesion  abi=%u  %u seeds (%" PRIu64 "..%" PRIu64 ")%s  n %u  ticks %u  grid %u,%u,%u  species %u\n",
           AZ_ABI_VERSION, o.n_seeds, o.seeds[0], o.seeds[o.n_seeds - 1],
           all_reporting ? ", preregistered reporting seeds" : any_tuning ? ", TUNING seeds: exploratory" :
           ", not the reporting blocks: exploratory", o.n, o.ticks, o.grid[0], o.grid[1], o.grid[2], o.species);
    printf("workers %u%s, kernels %s, arena %.1f MB %s, %u lesions x %u seeds + %u intact runs\n", host->n_workers,
           o.no_pin ? " (OS placement)" : "", o.kernels ? "assembly twins" : "C reference",
           (double)arena.size / 1048576.0, info.arena_locked ? "locked" : "PAGEABLE", n_sel, o.n_seeds, o.n_seeds);

    /* runs: intact[s], lesioned[k][s] */
    Run *intact = (Run *)calloc(o.n_seeds, sizeof(Run));
    Run *les = (Run *)calloc((size_t)n_sel * o.n_seeds, sizeof(Run));
    if (!intact || !les) { fprintf(stderr, "out of memory\n"); return 2; }
    FILE *csv = NULL;
    if (o.csv) {
        csv = fopen(o.csv, "wb");
        if (!csv) { fprintf(stderr, "cannot write %s\n", o.csv); return 2; }
        fprintf(csv, "seed,lesion,bit");
        for (u32 m = 0; m < M_COUNT; m++) fprintf(csv, ",%s", measure_name[m]);
        fprintf(csv, ",state_hash,conserved\n");
    }
    int rc = 0;
    const u64 t0 = host->now_ns(host->ctx);
    for (u32 s = 0; s < o.n_seeds; s++) {
        for (u32 k = 0; k <= n_sel; k++) {
            const u32 bit = k == 0 ? 0 : lesions[sel[k - 1]].bit;
            Run *r = k == 0 ? &intact[s] : &les[(size_t)(k - 1) * o.n_seeds + s];
            if (!run_world(&o, &arena, host, o.seeds[s], bit, r)) {
                fprintf(stderr, "world creation failed (seed %" PRIu64 ")\n", o.seeds[s]);
                return 2;
            }
            if (!r->conserved || r->underflow) {
                fprintf(stderr, "INVARIANT FAILED: seed %" PRIu64 " lesion %s: matter/energy %s, field_underflow %" PRIu64 "\n",
                        o.seeds[s], k ? lesions[sel[k - 1]].name : "none", r->conserved ? "exact" : "VIOLATED",
                        r->underflow);
                rc = 1;
            }
            if (csv) {
                fprintf(csv, "%" PRIu64 ",%s,0x%x", o.seeds[s], k ? lesions[sel[k - 1]].name : "INTACT", bit);
                for (u32 m = 0; m < M_COUNT; m++) fprintf(csv, ",%.6f", r->m[m]);
                fprintf(csv, ",%016" PRIx64 ",%d\n", r->hash, r->conserved ? 1 : 0);
            }
        }
        if (!o.quiet) {
            printf("  seed %" PRIu64 " done (%u of %u): intact live %.0f births %.0f energy %.2f hash %016" PRIx64 "\n",
                   o.seeds[s], s + 1, o.n_seeds, intact[s].m[M_LIVE], intact[s].m[M_BIRTHS], intact[s].m[M_ENERGY],
                   intact[s].hash);
            fflush(stdout);
        }
    }
    const double secs = (double)(host->now_ns(host->ctx) - t0) / 1e9;
    if (csv) fclose(csv);

    /* statistics on the primary measure of each lesion */
    const int ns = (int)o.n_seeds;
    double *p = (double *)malloc((size_t)n_sel * sizeof(double));
    double *padj = (double *)malloc((size_t)n_sel * sizeof(double));
    double *a = (double *)malloc((size_t)ns * sizeof(double));
    double *b = (double *)malloc((size_t)ns * sizeof(double));
    double *dz = (double *)malloc((size_t)n_sel * sizeof(double));
    u32 *differs = (u32 *)calloc(n_sel, sizeof(u32));
    if (!p || !padj || !a || !b || !dz || !differs) { fprintf(stderr, "out of memory\n"); return 2; }
    for (u32 k = 0; k < n_sel; k++) {
        const u32 m = lesions[sel[k]].measure;
        for (int s = 0; s < ns; s++) {
            const Run *r = &les[(size_t)k * o.n_seeds + (u32)s];
            a[s] = r->m[m];
            b[s] = intact[s].m[m];
            if (r->hash != intact[s].hash) differs[k]++;
        }
        AzsWsr wsr;
        p[k] = azs_wsr_exact(a, b, ns, 0.0, &wsr) == 0 ? wsr.p_two : 1.0;
        dz[k] = azs_cohen_d_paired(a, b, ns);
    }
    if (azs_holm(p, (int)n_sel, padj) != 0) for (u32 k = 0; k < n_sel; k++) padj[k] = 1.0;

    printf("\nprimary measure per lesion, paired by seed (diff = lesioned - intact); Holm over %u lesions; %.1f s\n",
           n_sel, secs);
    printf("%-11s %-13s %12s %12s %11s %21s %8s %9s %9s %8s  %s\n", "lesion", "measure", "intact", "lesioned",
           "mean diff", "95% boot interval", "d_z", "p exact", "p Holm", "differs", "verdict");
    u32 n_effect = 0, n_inert = 0, n_none = 0;
    for (u32 k = 0; k < n_sel; k++) {
        const u32 m = lesions[sel[k]].measure;
        for (int s = 0; s < ns; s++) { a[s] = les[(size_t)k * o.n_seeds + (u32)s].m[m]; b[s] = intact[s].m[m]; }
        AzsBootCi ci;
        if (azs_boot_ci_paired(a, b, ns, 2000, 0x5EED0000ull + sel[k], 0.95, &ci) != 0) ci.lo = ci.hi = ci.point = NAN;
        const char *verdict;
        if (differs[k] == 0) { verdict = "INERT"; n_inert++; }
        else if (fabs(dz[k]) >= RULE_D && padj[k] < RULE_P) { verdict = "EFFECT"; n_effect++; }
        else { verdict = "NO EFFECT"; n_none++; }
        char dbuf[16], ibuf[32], dfs[16];
        fmt_d(dbuf, sizeof dbuf, dz[k]);
        snprintf(ibuf, sizeof ibuf, "[%.3g, %.3g]", ci.lo, ci.hi);
        snprintf(dfs, sizeof dfs, "%u/%d", differs[k], ns);
        printf("%-11s %-13s %12.4g %12.4g %+11.4g %21s %8s %9.3g %9.3g %8s  %s\n", lesions[sel[k]].name,
               measure_name[m], azs_mean(b, ns), azs_mean(a, ns), ci.point, ibuf, dbuf, p[k], padj[k], dfs, verdict);
    }
    printf("%u EFFECT, %u NO EFFECT, %u INERT. EFFECT needs |d_z| >= %.1f and Holm p < %.2f on the row's primary "
           "measure; INERT = the same final state hash as the intact world on every seed; differs = seeds whose "
           "final state differs.%s\n", n_effect, n_none, n_inert, RULE_D, RULE_P,
           all_reporting ? "" : " These are NOT reporting seeds: exploratory only.");

    printf("\nall measures, mean lesioned - intact (description only, not tested):\n%-11s", "lesion");
    for (u32 m = 0; m < M_COUNT; m++) printf(" %13s", measure_name[m]);
    printf("\n");
    for (u32 k = 0; k < n_sel; k++) {
        printf("%-11s", lesions[sel[k]].name);
        for (u32 m = 0; m < M_COUNT; m++) {
            double sum = 0;
            for (int s = 0; s < ns; s++) sum += les[(size_t)k * o.n_seeds + (u32)s].m[m] - intact[s].m[m];
            printf(" %+13.4g", sum / ns);
        }
        printf("\n");
    }

    free(p); free(padj); free(a); free(b); free(dz); free(differs); free(intact); free(les);
    az_win_host_destroy(host);
    az_win_arena_free(&arena);
    return rc;
}
