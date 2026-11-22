/*
 * membench.c - per-core sequential memory throughput versus working-set size.
 *
 * Windows, C. Build:
 *     gcc -O2 -march=native -o membench.exe membench.c
 *
 * What it does
 *   1. Reads the processor topology (GetLogicalProcessorInformationEx):
 *      EfficiencyClass per logical processor (higher class = P core on a
 *      hybrid Intel part) and the data/unified cache sizes each one sees.
 *   2. Pins the thread to one logical processor (SetThreadAffinityMask).
 *   3. Allocates one page-aligned buffer (VirtualAlloc), touches every page.
 *   4. For working sets 16 KB .. 512 MB (powers of two) measures
 *        read  : sum of 64-bit words over the working set
 *        write : store a 64-bit constant over the working set
 *        copy  : copy the first half of the working set to the second half
 *      Each timed trial repeats the pass until it lasts >= min-time (0.2 s);
 *      the best of 5 trials is reported.
 *
 * Units: GB/s = 1e9 bytes per second.
 *   read, write : bytes of the working set traversed per second.
 *   copy        : bytes COPIED per second (working set / 2 per pass); a pass
 *                 touches the whole working set (reads half, writes half).
 *
 * Kernels use 256-bit AVX2 loads/stores when __AVX2__ is defined (aligned,
 * ordinary cached stores, no non-temporal stores, no rep movs/stos), else a
 * plain 64-bit scalar loop. The kernels carry an attribute that stops GCC
 * turning them into memset/memcpy calls.
 *
 * Usage:
 *   membench --topo                 print topology only
 *   membench --core P               first-choice P core (default)
 *   membench --core E               first-choice E core
 *   membench --lp N                 explicit logical processor
 *   options: --min-kb K (16) --max-kb K (524288) --min-time S (0.2)
 *            --trials N (5)
 *
 * Every run self-checks the checksums (buffer is all-ones words, so a read
 * pass must sum to the word count).
 */
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __AVX2__
#include <immintrin.h>
#endif

/* noipa: no inlining and no interprocedural analysis, so the compiler cannot
 * prove a kernel pure and fold N identical passes into one. */
#define NOINLINE __attribute__((noipa))
#if defined(__GNUC__) && !defined(__clang__)
#define NOIDIOM __attribute__((optimize("no-tree-loop-distribute-patterns")))
#else
#define NOIDIOM
#endif

#define MAX_LP 64

typedef struct {
    int present;
    int eff_class;       /* EfficiencyClass of the core */
    int core_index;      /* index of the physical core record */
    int smt_siblings;    /* logical processors in this core */
    unsigned l1d, l2, l3;        /* bytes */
    int l2_share, l3_share;      /* logical processors sharing that cache */
} lp_info;

static lp_info g_lp[MAX_LP];
static int g_nlp = 0, g_ncore = 0, g_max_class = 0, g_min_class = 255;

static int popcount64(uint64_t x) { return __builtin_popcountll(x); }

static int read_topology(void)
{
    DWORD len = 0;
    GetLogicalProcessorInformationEx(RelationAll, NULL, &len);
    if (len == 0) return -1;
    BYTE *buf = (BYTE *)malloc(len);
    if (!buf) return -1;
    if (!GetLogicalProcessorInformationEx(
            RelationAll, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)buf, &len)) {
        free(buf);
        return -1;
    }
    for (DWORD off = 0; off < len;) {
        PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX r =
            (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)(buf + off);
        if (r->Relationship == RelationProcessorCore) {
            uint64_t m = (uint64_t)r->Processor.GroupMask[0].Mask;
            int cls = r->Processor.EfficiencyClass;
            if (r->Processor.GroupMask[0].Group == 0) {
                for (int i = 0; i < MAX_LP; i++) {
                    if (m >> i & 1) {
                        g_lp[i].present = 1;
                        g_lp[i].eff_class = cls;
                        g_lp[i].core_index = g_ncore;
                        g_lp[i].smt_siblings = popcount64(m);
                        if (i + 1 > g_nlp) g_nlp = i + 1;
                    }
                }
                if (cls > g_max_class) g_max_class = cls;
                if (cls < g_min_class) g_min_class = cls;
            }
            g_ncore++;
        } else if (r->Relationship == RelationCache) {
            if (r->Cache.GroupMask.Group == 0 &&
                (r->Cache.Type == CacheData || r->Cache.Type == CacheUnified)) {
                uint64_t m = (uint64_t)r->Cache.GroupMask.Mask;
                int share = popcount64(m);
                for (int i = 0; i < MAX_LP; i++) {
                    if (!(m >> i & 1)) continue;
                    if (r->Cache.Level == 1) g_lp[i].l1d = r->Cache.CacheSize;
                    if (r->Cache.Level == 2) {
                        g_lp[i].l2 = r->Cache.CacheSize;
                        g_lp[i].l2_share = share;
                    }
                    if (r->Cache.Level == 3) {
                        g_lp[i].l3 = r->Cache.CacheSize;
                        g_lp[i].l3_share = share;
                    }
                }
            }
        }
        off += r->Size;
    }
    free(buf);
    return 0;
}

static void print_topology(void)
{
    int np = 0, ne = 0;
    printf("topology: %d logical processors, %d physical cores\n", g_nlp, g_ncore);
    printf("  LP core class kind  L1d      L2 (shared by)   L3 (shared by)\n");
    for (int i = 0; i < g_nlp; i++) {
        if (!g_lp[i].present) continue;
        int isP = (g_lp[i].eff_class == g_max_class);
        if (g_max_class == g_min_class) isP = 1;
        if (isP) np++; else ne++;
        printf("  %2d  %3d   %3d  %s  %4u KB  %6u KB (%2d)   %6u KB (%2d)\n",
               i, g_lp[i].core_index, g_lp[i].eff_class, isP ? "P" : "E",
               g_lp[i].l1d / 1024, g_lp[i].l2 / 1024, g_lp[i].l2_share,
               g_lp[i].l3 / 1024, g_lp[i].l3_share);
    }
    printf("  summary: %d P logical processors (class %d), %d E logical processors (class %d)\n",
           np, g_max_class, ne, g_min_class);
}

/* Pick the second logical processor of a class when there is one, so the
 * run avoids the first processor of the class (LP 0 takes most interrupts). */
static int pick_lp(int want_p)
{
    int cls = want_p ? g_max_class : g_min_class;
    int first = -1, seen = 0;
    for (int i = 0; i < g_nlp; i++) {
        if (!g_lp[i].present || g_lp[i].eff_class != cls) continue;
        if (first < 0) first = i;
        if (++seen == 2) return i;
    }
    return first;
}

/* ---------------- kernels ---------------- */

NOINLINE NOIDIOM static uint64_t k_read(const uint64_t *p, size_t nwords)
{
#ifdef __AVX2__
    __m256i a0 = _mm256_setzero_si256(), a1 = a0, a2 = a0, a3 = a0;
    for (size_t i = 0; i < nwords; i += 16) {
        a0 = _mm256_add_epi64(a0, _mm256_load_si256((const __m256i *)(p + i)));
        a1 = _mm256_add_epi64(a1, _mm256_load_si256((const __m256i *)(p + i + 4)));
        a2 = _mm256_add_epi64(a2, _mm256_load_si256((const __m256i *)(p + i + 8)));
        a3 = _mm256_add_epi64(a3, _mm256_load_si256((const __m256i *)(p + i + 12)));
    }
    a0 = _mm256_add_epi64(_mm256_add_epi64(a0, a1), _mm256_add_epi64(a2, a3));
    uint64_t t[4];
    _mm256_storeu_si256((__m256i *)t, a0);
    return t[0] + t[1] + t[2] + t[3];
#else
    uint64_t s0 = 0, s1 = 0, s2 = 0, s3 = 0;
    for (size_t i = 0; i < nwords; i += 4) {
        s0 += p[i]; s1 += p[i + 1]; s2 += p[i + 2]; s3 += p[i + 3];
    }
    return s0 + s1 + s2 + s3;
#endif
}

NOINLINE NOIDIOM static void k_write(uint64_t *p, size_t nwords, uint64_t v)
{
#ifdef __AVX2__
    __m256i x = _mm256_set1_epi64x((long long)v);
    for (size_t i = 0; i < nwords; i += 16) {
        _mm256_store_si256((__m256i *)(p + i), x);
        _mm256_store_si256((__m256i *)(p + i + 4), x);
        _mm256_store_si256((__m256i *)(p + i + 8), x);
        _mm256_store_si256((__m256i *)(p + i + 12), x);
    }
#else
    for (size_t i = 0; i < nwords; i += 4) {
        p[i] = v; p[i + 1] = v; p[i + 2] = v; p[i + 3] = v;
    }
#endif
}

NOINLINE NOIDIOM static void k_copy(uint64_t *d, const uint64_t *s, size_t nwords)
{
#ifdef __AVX2__
    for (size_t i = 0; i < nwords; i += 16) {
        _mm256_store_si256((__m256i *)(d + i),
                           _mm256_load_si256((const __m256i *)(s + i)));
        _mm256_store_si256((__m256i *)(d + i + 4),
                           _mm256_load_si256((const __m256i *)(s + i + 4)));
        _mm256_store_si256((__m256i *)(d + i + 8),
                           _mm256_load_si256((const __m256i *)(s + i + 8)));
        _mm256_store_si256((__m256i *)(d + i + 12),
                           _mm256_load_si256((const __m256i *)(s + i + 12)));
    }
#else
    for (size_t i = 0; i < nwords; i += 4) {
        d[i] = s[i]; d[i + 1] = s[i + 1]; d[i + 2] = s[i + 2]; d[i + 3] = s[i + 3];
    }
#endif
}

/* 32 dependent register adds per iteration: 32 cycles per iteration if the
 * add has one cycle of latency (true for every current x86 core). The loop
 * counter runs in parallel. Result: effective core clock in cycles/second. */
NOINLINE static uint64_t k_chain(uint64_t iters)
{
    uint64_t a = 0, d = 1;
#if defined(__x86_64__)
    __asm__ volatile(
        ".p2align 5\n"
        "1:\n"
        ".rept 32\n"
        "add %[d], %[a]\n"
        ".endr\n"
        "dec %[n]\n"
        "jnz 1b\n"
        : [a] "+r"(a), [n] "+r"(iters)
        : [d] "r"(d)
        : "cc");
#else
    for (uint64_t i = 0; i < iters * 32; i++) a += d;
#endif
    return a;
}

/* ---------------- timing ---------------- */

static double g_qpf;
static double now_s(void)
{
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / g_qpf;
}

enum { K_READ, K_WRITE, K_COPY };

static int g_check_fail = 0;

static double run_passes(int kind, uint64_t *buf, size_t ws_bytes, uint64_t reps)
{
    size_t nwords = ws_bytes / 8;
    uint64_t sum = 0;
    double t0 = now_s();
    switch (kind) {
    case K_READ:
        for (uint64_t r = 0; r < reps; r++) sum += k_read(buf, nwords);
        break;
    case K_WRITE:
        for (uint64_t r = 0; r < reps; r++) k_write(buf, nwords, 1);
        break;
    default:
        for (uint64_t r = 0; r < reps; r++)
            k_copy(buf + nwords / 2, buf, nwords / 2);
        break;
    }
    double t1 = now_s();
    if (kind == K_READ && sum != (uint64_t)nwords * reps) g_check_fail++;
    return t1 - t0;
}

/* best-of-trials throughput in GB/s (1e9 bytes) */
static double measure(int kind, uint64_t *buf, size_t ws_bytes, double min_time,
                      int trials)
{
    double per_pass_bytes = (kind == K_COPY) ? (double)ws_bytes / 2 : (double)ws_bytes;
    /* warm-up + calibration: double the pass count until a run lasts 5 ms
     * (a single small pass is shorter than the timer resolution), then scale
     * that to min_time. */
    uint64_t reps = 1;
    double t = run_passes(kind, buf, ws_bytes, reps);
    while (t < 0.005 && t < min_time) {
        reps *= 2;
        t = run_passes(kind, buf, ws_bytes, reps);
    }
    if (t < min_time) reps = (uint64_t)((double)reps * (min_time / t) * 1.05) + 1;
    double best = 0;
    for (int tr = 0; tr < trials; tr++) {
        double el;
        for (;;) {
            el = run_passes(kind, buf, ws_bytes, reps);
            if (el >= min_time) break;
            /* too short (clock went up, or calibration was slow): grow and redo */
            double f = (el > 0) ? (min_time / el) * 1.1 : 2.0;
            if (f < 1.05) f = 1.05;
            reps = (uint64_t)((double)reps * f) + 1;
        }
        double gbps = per_pass_bytes * (double)reps / el / 1e9;
        if (gbps > best) best = gbps;
    }
    return best;
}

static const char *level_of(const lp_info *li, size_t ws)
{
    if (li->l1d && ws <= li->l1d) return "L1d";
    if (li->l2 && ws <= li->l2) return "L2";
    if (li->l3 && ws <= li->l3) return "L3";
    return "DRAM";
}

static void fmt_size(size_t b, char *out, size_t n)
{
    if (b >= (1u << 20)) snprintf(out, n, "%zu MB", b >> 20);
    else snprintf(out, n, "%zu KB", b >> 10);
}

int main(int argc, char **argv)
{
    int lp = -1, want_p = 1, topo_only = 0, trials = 5;
    size_t min_kb = 16, max_kb = 512 * 1024;
    double min_time = 0.2;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--topo")) topo_only = 1;
        else if (!strcmp(argv[i], "--lp") && i + 1 < argc) lp = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--core") && i + 1 < argc)
            want_p = (argv[++i][0] == 'P' || argv[i][0] == 'p');
        else if (!strcmp(argv[i], "--min-kb") && i + 1 < argc) min_kb = (size_t)atoll(argv[++i]);
        else if (!strcmp(argv[i], "--max-kb") && i + 1 < argc) max_kb = (size_t)atoll(argv[++i]);
        else if (!strcmp(argv[i], "--min-time") && i + 1 < argc) min_time = atof(argv[++i]);
        else if (!strcmp(argv[i], "--trials") && i + 1 < argc) trials = atoi(argv[++i]);
        else {
            fprintf(stderr,
                    "usage: membench [--topo] [--core P|E | --lp N] [--min-kb K] "
                    "[--max-kb K] [--min-time S] [--trials N]\n");
            return 2;
        }
    }
    if (min_kb < 16) min_kb = 16;
    if (max_kb < min_kb) max_kb = min_kb;
    if (trials < 1) trials = 1;

    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    g_qpf = (double)f.QuadPart;

    if (read_topology() != 0) {
        fprintf(stderr, "GetLogicalProcessorInformationEx failed (%lu)\n", GetLastError());
        return 1;
    }
    printf("membench (compiler: %s, kernels: %s)\n", __VERSION__,
#ifdef __AVX2__
           "AVX2 256-bit"
#else
           "scalar 64-bit"
#endif
    );
    print_topology();
    if (topo_only) return 0;

    if (lp < 0) lp = pick_lp(want_p);
    if (lp < 0 || lp >= MAX_LP || !g_lp[lp].present) {
        fprintf(stderr, "no such logical processor: %d\n", lp);
        return 1;
    }
    if (!SetThreadAffinityMask(GetCurrentThread(), (DWORD_PTR)1 << lp)) {
        fprintf(stderr, "SetThreadAffinityMask failed (%lu)\n", GetLastError());
        return 1;
    }
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    Sleep(0); /* reschedule onto the chosen processor */
    const lp_info *li = &g_lp[lp];
    int isP = (li->eff_class == g_max_class);
    printf("pinned to LP %d (now on %lu): %s core, class %d, L1d %u KB, L2 %u KB "
           "(shared by %d), L3 %u KB (shared by %d)\n",
           lp, GetCurrentProcessorNumber(), isP ? "P" : "E", li->eff_class,
           li->l1d / 1024, li->l2 / 1024, li->l2_share, li->l3 / 1024, li->l3_share);

    /* effective clock from a dependent add chain (best of 5 x ~50 ms) */
    {
        uint64_t iters = 2000000;
        double best = 0;
        for (int tr = 0; tr < 5; tr++) {
            double t0 = now_s();
            uint64_t a = k_chain(iters);
            double t1 = now_s();
            if (a != iters * 32) g_check_fail++;
            double ghz = 32.0 * (double)iters / (t1 - t0) / 1e9;
            if (ghz > best) best = ghz;
            if (t1 - t0 < 0.05) iters *= 2;
        }
        printf("effective clock (32-add dependent chain, best of 5): %.2f GHz\n", best);
    }

    size_t max_bytes = max_kb * 1024;
    uint64_t *buf = (uint64_t *)VirtualAlloc(NULL, max_bytes, MEM_RESERVE | MEM_COMMIT,
                                             PAGE_READWRITE);
    if (!buf) {
        fprintf(stderr, "VirtualAlloc(%zu) failed (%lu)\n", max_bytes, GetLastError());
        return 1;
    }
    k_write(buf, max_bytes / 8, 1); /* touch every page; buffer = all-ones words */

    printf("min-time %.2f s per trial, best of %d trials, GB = 1e9 bytes\n", min_time, trials);
    printf("%10s  %-5s %12s %12s %12s\n", "work set", "fits", "read GB/s", "write GB/s",
           "copy GB/s");
    for (size_t kb = min_kb; kb <= max_kb; kb *= 2) {
        size_t ws = kb * 1024;
        char sz[32];
        fmt_size(ws, sz, sizeof sz);
        double r = measure(K_READ, buf, ws, min_time, trials);
        double w = measure(K_WRITE, buf, ws, min_time, trials);
        double c = measure(K_COPY, buf, ws, min_time, trials);
        printf("%10s  %-5s %12.1f %12.1f %12.1f\n", sz, level_of(li, ws), r, w, c);
        fflush(stdout);
    }
    /* final content check: every word must still be 1 */
    if (k_read(buf, max_bytes / 8) != max_bytes / 8) g_check_fail++;
    printf("checks: %s\n", g_check_fail ? "FAILED" : "ok");
    VirtualFree(buf, 0, MEM_RELEASE);
    return g_check_fail ? 1 : 0;
}
