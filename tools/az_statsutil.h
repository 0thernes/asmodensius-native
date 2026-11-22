/* az_statsutil.h - small statistics kit for the evidence tools (header-only).
 *
 * Hosted C: libc and doubles are allowed here (this is tooling, never part
 * of the simulation). Everything is deterministic: no rand(), no clock, no
 * global state; the bootstrap takes its seed as an argument.
 *
 *   azs_wsr_exact       exact paired Wilcoxon signed-rank test, <= 64 pairs,
 *                       ties by midranks, zeros dropped; counts are integers
 *   azs_holm            Holm step-down adjusted p-values for a family
 *   azs_cohen_d_paired  d_z = mean(a - b) / sd(a - b)
 *   azs_welch_t         Welch t and Welch-Satterthwaite degrees of freedom
 *   azs_boot_ci_paired  percentile bootstrap interval of the mean paired
 *                       difference, seeded (same seed, same interval)
 *   azs_quartiles       Q1, median, Q3 (linear interpolation, "type 7")
 *
 * The signed-rank kernel descends from the scratch kernel that reproduced
 * the 14 p-values recorded by the previous lab (docs/research/v40/gates.md,
 * section 8). It differs in three ways: 128-bit counts (so 64 pairs fit),
 * both tails and the two-sided value are returned, and bad input is refused
 * instead of trusted.
 */
#ifndef AZ_STATSUTIL_H
#define AZ_STATSUTIL_H

#include "az_sha256.h"
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define AZS_WSR_MAX 64   /* most pairs azs_wsr_exact accepts */

typedef unsigned __int128 azs_u128;

/* ---------------------------------------------------------------------- */
/* basics                                                                  */
/* ---------------------------------------------------------------------- */

static inline double azs_mean(const double *x, int n)
{
    if (n <= 0) return NAN;
    double s = 0.0;
    for (int i = 0; i < n; i++) s += x[i];
    return s / (double)n;
}

/* sample variance (divisor n - 1); NAN for n < 2 */
static inline double azs_var(const double *x, int n)
{
    if (n < 2) return NAN;
    double m = azs_mean(x, n), s = 0.0;
    for (int i = 0; i < n; i++) { double d = x[i] - m; s += d * d; }
    return s / (double)(n - 1);
}

static inline int azs_cmp_double(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

/* quantile of an ascending array, linear interpolation between order
 * statistics at h = (n - 1) q (Hyndman-Fan type 7, the R default) */
static inline double azs_quantile_sorted(const double *sorted, int n, double q)
{
    if (n <= 0) return NAN;
    if (q <= 0.0) return sorted[0];
    if (q >= 1.0) return sorted[n - 1];
    double h = (double)(n - 1) * q;
    int lo = (int)floor(h);
    if (lo >= n - 1) return sorted[n - 1];
    double f = h - (double)lo;
    return sorted[lo] + f * (sorted[lo + 1] - sorted[lo]);
}

/* Q1, median, Q3 of x[0..n-1] (x is not modified). Returns 0, or -1 when
 * n <= 0 or memory ran out (then the outputs are NAN). */
static inline int azs_quartiles(const double *x, int n, double *q1, double *med, double *q3)
{
    *q1 = *med = *q3 = NAN;
    if (n <= 0) return -1;
    double *s = (double *)malloc((size_t)n * sizeof *s);
    if (!s) return -1;
    memcpy(s, x, (size_t)n * sizeof *s);
    qsort(s, (size_t)n, sizeof *s, azs_cmp_double);
    *q1 = azs_quantile_sorted(s, n, 0.25);
    *med = azs_quantile_sorted(s, n, 0.50);
    *q3 = azs_quantile_sorted(s, n, 0.75);
    free(s);
    return 0;
}

static inline double azs_median(const double *x, int n)
{
    double q1, med, q3;
    (void)azs_quartiles(x, n, &q1, &med, &q3);
    return med;
}

/* decimal text of a 128-bit count; buf needs 40 bytes; returns buf */
static inline char *azs_u128_str(azs_u128 v, char buf[40])
{
    char tmp[40];
    int k = 0;
    do { tmp[k++] = (char)('0' + (int)(v % 10u)); v /= 10u; } while (v);
    for (int i = 0; i < k; i++) buf[i] = tmp[k - 1 - i];
    buf[k] = 0;
    return buf;
}

/* ---------------------------------------------------------------------- */
/* exact Wilcoxon signed-rank test                                         */
/* ---------------------------------------------------------------------- */

typedef struct AzsWsr {
    int n;            /* pairs given                                          */
    int n_nz;         /* pairs with a non-zero difference (the test size)     */
    int n_pos, n_neg; /* of those, a > b and a < b                            */
    int w2_obs;       /* twice W+: twice the sum of midranks of a > b pairs   */
    int w2_max;       /* twice the sum of all midranks = n_nz (n_nz + 1)      */
    azs_u128 total;   /* 2^n_nz sign assignments                              */
    azs_u128 tail_ge; /* assignments with W+ >= observed                      */
    azs_u128 tail_le; /* assignments with W+ <= observed                      */
    double p_greater; /* tail_ge / total: one-sided, a tends to exceed b      */
    double p_less;    /* tail_le / total                                      */
    double p_two;     /* min(1, 2 min(tail_ge, tail_le) / total)              */
} AzsWsr;

typedef struct AzsNz { double abs; int sign; } AzsNz;

static inline int azs_cmp_nz(const void *a, const void *b)
{
    double x = ((const AzsNz *)a)->abs, y = ((const AzsNz *)b)->abs;
    return (x > y) - (x < y);
}

static inline double azs_u128_ratio(azs_u128 num, int den_log2)
{
    return (double)ldexpl((long double)num, -den_log2);
}

/* Exact paired Wilcoxon signed-rank test of a against b.
 *
 * A pair with |a - b| <= tie_tol is dropped (the usual convention: the test
 * is conditional on the non-zero differences). Absolute differences within
 * tie_tol of the smallest member of their group share the midrank. Twice a
 * midrank is an integer, so the null distribution of W+ over the 2^n_nz
 * equally likely sign assignments is counted exactly by subset-sum dynamic
 * programming. With integer metrics pass tie_tol = 0.
 *
 * The null distribution is symmetric about w2_max / 2, so the two-sided
 * value is twice the smaller tail, capped at 1.
 *
 * Returns 0 on success; -1 (and a zeroed result with every p = NAN) when
 * n < 0, n > AZS_WSR_MAX, a value is not finite, tie_tol is negative or
 * memory ran out. */
static inline int azs_wsr_exact(const double *a, const double *b, int n, double tie_tol, AzsWsr *out)
{
    memset(out, 0, sizeof *out);
    out->p_greater = out->p_less = out->p_two = NAN;
    if (n < 0 || n > AZS_WSR_MAX || !(tie_tol >= 0.0)) return -1;
    AzsNz nz[AZS_WSR_MAX];
    int m = 0;
    for (int i = 0; i < n; i++) {
        double d = a[i] - b[i];
        if (!isfinite(a[i]) || !isfinite(b[i]) || !isfinite(d)) return -1;
        if (fabs(d) > tie_tol) { nz[m].abs = fabs(d); nz[m].sign = d > 0 ? 1 : -1; m++; }
    }
    out->n = n;
    out->n_nz = m;
    if (m == 0) {   /* nothing differs: one assignment, every p is 1 */
        out->total = out->tail_ge = out->tail_le = 1;
        out->p_greater = out->p_less = out->p_two = 1.0;
        return 0;
    }
    qsort(nz, (size_t)m, sizeof nz[0], azs_cmp_nz);
    int twice_rank[AZS_WSR_MAX];
    for (int i = 0; i < m;) {
        int end = i + 1;
        while (end < m && nz[end].abs - nz[i].abs <= tie_tol) end++;
        for (int k = i; k < end; k++) twice_rank[k] = i + 1 + end;   /* 2 * midrank of ranks i+1 .. end */
        i = end;
    }
    int w2_max = 0, w2_obs = 0;
    for (int i = 0; i < m; i++) {
        w2_max += twice_rank[i];
        if (nz[i].sign > 0) { w2_obs += twice_rank[i]; out->n_pos++; } else out->n_neg++;
    }
    azs_u128 *ways = (azs_u128 *)calloc((size_t)w2_max + 1, sizeof *ways);
    if (!ways) { memset(out, 0, sizeof *out); out->p_greater = out->p_less = out->p_two = NAN; return -1; }
    ways[0] = 1;
    int cur = 0;
    for (int i = 0; i < m; i++) {
        int r = twice_rank[i];
        for (int s = cur; s >= 0; s--) if (ways[s]) ways[s + r] += ways[s];
        cur += r;
    }
    azs_u128 ge = 0, le = 0;
    for (int s = w2_obs; s <= w2_max; s++) ge += ways[s];
    for (int s = 0; s <= w2_obs; s++) le += ways[s];
    free(ways);
    out->w2_obs = w2_obs;
    out->w2_max = w2_max;
    out->total = (azs_u128)1 << m;
    out->tail_ge = ge;
    out->tail_le = le;
    out->p_greater = azs_u128_ratio(ge, m);
    out->p_less = azs_u128_ratio(le, m);
    azs_u128 two = 2 * (ge < le ? ge : le);
    if (two > out->total) two = out->total;
    out->p_two = azs_u128_ratio(two, m);
    return 0;
}

/* ---------------------------------------------------------------------- */
/* Holm step-down                                                          */
/* ---------------------------------------------------------------------- */

typedef struct AzsRank { double p; int idx; } AzsRank;

static inline int azs_cmp_rank(const void *a, const void *b)
{
    const AzsRank *x = (const AzsRank *)a, *y = (const AzsRank *)b;
    if (x->p < y->p) return -1;
    if (x->p > y->p) return 1;
    return (x->idx > y->idx) - (x->idx < y->idx);   /* total order: the result never depends on qsort */
}

/* Holm adjusted p-values: sort ascending p(1) <= ... <= p(m), then
 *   adj(k) = max over j <= k of min(1, (m - j + 1) p(j)).
 * Rejecting where adj <= alpha controls the family-wise error rate at alpha
 * under any dependence between the tests. adj may alias nothing in p.
 * Returns 0, or -1 when m <= 0, a p-value is outside [0, 1] or not a number,
 * or memory ran out (then adj is filled with NAN). */
static inline int azs_holm(const double *p, int m, double *adj)
{
    if (m <= 0) return -1;
    for (int i = 0; i < m; i++) adj[i] = NAN;
    for (int i = 0; i < m; i++) if (!(p[i] >= 0.0 && p[i] <= 1.0)) return -1;
    AzsRank *r = (AzsRank *)malloc((size_t)m * sizeof *r);
    if (!r) return -1;
    for (int i = 0; i < m; i++) { r[i].p = p[i]; r[i].idx = i; }
    qsort(r, (size_t)m, sizeof *r, azs_cmp_rank);
    double run = 0.0;
    for (int k = 0; k < m; k++) {
        double c = r[k].p * (double)(m - k);
        if (c > 1.0) c = 1.0;
        if (c > run) run = c;
        adj[r[k].idx] = run;
    }
    free(r);
    return 0;
}

/* ---------------------------------------------------------------------- */
/* effect size and Welch t                                                 */
/* ---------------------------------------------------------------------- */

/* The Windows GCC/Clang tools have 80-bit long double. Its exponent range
 * holds all differences, squared offsets and int-sized sums of binary64
 * observations, including differences above DBL_MAX. */
_Static_assert(FLT_RADIX == 2 && LDBL_MAX_EXP > 2 * DBL_MAX_EXP + 64 &&
               LDBL_MIN_EXP < 2 * (DBL_MIN_EXP - DBL_MANT_DIG) - 64,
               "paired effects require extended exponent range");

/* Grow a nonoverlapping expansion using error-free addition. Low terms
 * survive cancellation between large observations of either sign. */
static inline int azs_dz_add(long double *parts, int count, long double x)
{
    int used = 0;
    for (int i = 0; i < count; i++) {
        long double y = parts[i];
        if (fabsl(x) < fabsl(y)) { long double t = x; x = y; y = t; }
        long double high = x + y;
        long double low = y - (high - x);
        if (low != 0) parts[used++] = low;
        x = high;
    }
    if (x != 0) parts[used++] = x;
    return used;
}

static inline long double azs_dz_sum(const long double *parts, int count)
{
    long double sum = 0;
    for (int i = 0; i < count; i++) sum += parts[i];
    return sum;
}

/* (a-b) - (anchor_a-anchor_b), including tiny differences on a large
 * common baseline. The four input terms are exactly representable here. */
static inline long double azs_dz_offset(double a, double b, double anchor_a, double anchor_b)
{
    if (b == anchor_b) return (long double)a - anchor_a;
    if (a == anchor_a) return (long double)anchor_b - b;
    long double parts[4];
    int n = azs_dz_add(parts, 0, a);
    n = azs_dz_add(parts, n, -b);
    n = azs_dz_add(parts, n, -anchor_a);
    n = azs_dz_add(parts, n, anchor_b);
    return azs_dz_sum(parts, n);
}

/* Input is finite and n >= 2. A NULL draw selects the observed pairs. */
static inline long double azs_dz_sample(const double *a, const double *b, int n,
                                          int sign, const int *draw)
{
    const int first = draw ? draw[0] : 0;
    /* Each surviving expansion term occupies a separate significand over
     * the binary64 exponent range plus at most int-width sum growth. */
    long double parts[4 + (DBL_MAX_EXP - DBL_MIN_EXP + DBL_MANT_DIG +
                            CHAR_BIT * sizeof(int)) / (LDBL_MANT_DIG - 1)];
    int count = 0;
    long double mean_offset = 0, ss = 0;
    for (int i = 0; i < n; i++) {
        int j = draw ? draw[i] : i;
        count = azs_dz_add(parts, count, a[j]);
        count = azs_dz_add(parts, count, -b[j]);
        long double offset = azs_dz_offset(a[j], b[j], a[first], b[first]);
        long double delta = offset - mean_offset;
        mean_offset += delta / (i + 1);
        ss += delta * (offset - mean_offset);
    }
    long double mean = azs_dz_sum(parts, count) / n;
    if (ss == 0) return mean == 0 ? 0 : copysignl(INFINITY, sign * mean);
    long double variance = ss / (n - 1);
    int exponent = ilogbl(variance) / 2;
    long double numerator = scalbnl(mean, -exponent);
    long double variance_scaled = scalbnl(variance, -2 * exponent);
    /* Retain the scaled double mean and sample-SD convention of the public
     * estimator. Use extended arithmetic near the output range boundaries. */
    if (fabsl(numerator) < 2 * (long double)DBL_MIN ||
        fabsl(numerator) > (long double)DBL_MAX / 2)
        return sign * (numerator / sqrtl(variance_scaled));
    return sign * ((long double)(double)numerator / sqrt((double)variance_scaled));
}

/* True constant differences retain their signed infinity. A finite statistic
 * outside the application's output type is unreportable in that type. */
static inline double azs_dz_output(long double value)
{
    return isfinite(value) && fabsl(value) > DBL_MAX ? NAN : (double)value;
}

/* Paired Cohen d (d_z): mean(a-b) divided by its sample SD. Returns NAN
 * for invalid inputs or a finite result beyond the double output range. */
static inline double azs_cohen_d_paired(const double *a, const double *b, int n)
{
    if (!a || !b || n < 2) return NAN;
    for (int i = 0; i < n; i++) if (!isfinite(a[i]) || !isfinite(b[i])) return NAN;
    return azs_dz_output(azs_dz_sample(a, b, n, 1, NULL));
}

typedef struct AzsWelch {
    double t;        /* (mean_a - mean_b) / sqrt(var_a / na + var_b / nb)     */
    double df;       /* Welch-Satterthwaite                                    */
    double mean_a, mean_b, var_a, var_b;
} AzsWelch;

/* Welch's unequal-variance t for two samples treated as independent. When
 * both variances are 0: t = 0 for equal means, otherwise +-INFINITY, and df
 * is NAN. Every member is NAN when a sample has fewer than 2 values. */
static inline AzsWelch azs_welch_t(const double *a, int na, const double *b, int nb)
{
    AzsWelch r;
    r.t = r.df = r.mean_a = r.mean_b = r.var_a = r.var_b = NAN;
    if (na < 2 || nb < 2) return r;
    r.mean_a = azs_mean(a, na); r.mean_b = azs_mean(b, nb);
    r.var_a = azs_var(a, na);   r.var_b = azs_var(b, nb);
    double sa = r.var_a / (double)na, sb = r.var_b / (double)nb;
    double se2 = sa + sb, diff = r.mean_a - r.mean_b;
    if (se2 <= 0.0) {
        r.t = diff == 0.0 ? 0.0 : (diff > 0.0 ? INFINITY : -INFINITY);
        return r;
    }
    r.t = diff / sqrt(se2);
    r.df = se2 * se2 / (sa * sa / (double)(na - 1) + sb * sb / (double)(nb - 1));
    return r;
}

/* ---------------------------------------------------------------------- */
/* deterministic paired bootstrap                                          */
/* ---------------------------------------------------------------------- */

/* SplitMix64 (Steele, Lea, Flood 2014): the whole generator state is *s */
static inline uint64_t azs_splitmix64(uint64_t *s)
{
    uint64_t z = (*s += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

typedef struct AzsBootCi {
    double point;    /* observed statistic (paired mean or signed d_z)                */
    double lo, hi;   /* percentile interval of the resampled statistic            */
    int resamples;
} AzsBootCi;

/* Percentile bootstrap interval of the mean paired difference a - b: draw n
 * pairs with replacement `resamples` times, take the (1 - conf) / 2 and
 * (1 + conf) / 2 quantiles (type 7) of the resampled means. Integer index
 * draws from SplitMix64 and sums in draw order, so the same (data, seed,
 * resamples, conf) gives the same doubles on every run and thread count.
 * Returns 0, or -1 for n < 1, resamples < 1, conf outside (0, 1) or no
 * memory (then lo, hi and point are NAN). */
static inline int azs_boot_ci_paired(const double *a, const double *b, int n, int resamples,
                                     uint64_t seed, double conf, AzsBootCi *out)
{
    out->point = out->lo = out->hi = NAN;
    out->resamples = 0;
    if (n < 1 || resamples < 1 || !(conf > 0.0 && conf < 1.0)) return -1;
    double *d = (double *)malloc((size_t)n * sizeof *d);
    double *means = (double *)malloc((size_t)resamples * sizeof *means);
    if (!d || !means) { free(d); free(means); return -1; }
    for (int i = 0; i < n; i++) d[i] = a[i] - b[i];
    uint64_t state = seed;
    for (int r = 0; r < resamples; r++) {
        double s = 0.0;
        for (int i = 0; i < n; i++) {
            uint64_t u = azs_splitmix64(&state);
            int k = (int)(((azs_u128)u * (azs_u128)(uint64_t)n) >> 64);   /* 0 .. n-1 */
            s += d[k];
        }
        means[r] = s / (double)n;
    }
    qsort(means, (size_t)resamples, sizeof *means, azs_cmp_double);
    out->point = azs_mean(d, n);
    out->lo = azs_quantile_sorted(means, resamples, (1.0 - conf) / 2.0);
    out->hi = azs_quantile_sorted(means, resamples, (1.0 + conf) / 2.0);
    out->resamples = resamples;
    free(d);
    free(means);
    return 0;
}

/* SB-05, protocol 07 sections 3.2 and 4.2. The first eight digest bytes
 * interpreted in network order are the numeric SplitMix64 seed. */
static inline uint64_t azs_boot_seed(const char *item, const char *arm)
{
    AzSha256 sha;
    unsigned char digest[32];
    az_sha256_init(&sha);
    az_sha256_update(&sha, item, strlen(item));
    az_sha256_update(&sha, "|", 1);
    az_sha256_update(&sha, arm, strlen(arm));
    az_sha256_final(&sha, digest);
    uint64_t seed = 0;
    for (int i = 0; i < 8; i++) seed = (seed << 8) | digest[i];
    return seed;
}

/* Arithmetic on extended-real effect sizes. Interpolation strictly between
 * opposite true infinities is undefined. Finite endpoints remain extended
 * until interpolation and narrowing to the public double result. */
static inline long double azs_dz_quantile(const long double *sorted, int n, double q)
{
    double h = (n - 1) * q;
    int i = (int)h;
    if (i == n - 1 || h == i || sorted[i] == sorted[i + 1]) return sorted[i];
    long double left = sorted[i], right = sorted[i + 1];
    if (isinf(left)) return isinf(right) ? NAN : left;
    if (isinf(right)) return right;
    return left + (h - i) * (right - left);
}

static inline int azs_dz_cmp(const void *a, const void *b)
{
    long double x = *(const long double *)a, y = *(const long double *)b;
    return (x > y) - (x < y);
}

/* A resample selects both members of each pair. A nonfinite observation,
 * an undefined interval or a finite output beyond DBL_MAX is refused. */
static inline int azs_boot_ci_dz(const double *a, const double *b, int n, int sign,
                                 int resamples, uint64_t seed, double conf, AzsBootCi *out)
{
    if (!out) return -1;
    *out = (AzsBootCi){NAN, NAN, NAN, 0};
    if (!a || !b || n < 2 || (sign != 1 && sign != -1) || resamples < 1 ||
        !(conf > 0 && conf < 1) || (size_t)n > SIZE_MAX / sizeof(int) ||
        (size_t)resamples > SIZE_MAX / sizeof(long double)) return -1;
    for (int i = 0; i < n; i++) if (!isfinite(a[i]) || !isfinite(b[i])) return -1;
    double point = azs_dz_output(azs_dz_sample(a, b, n, sign, NULL));
    if (isnan(point)) return -1;
    int *draw = (int *)malloc(sizeof *draw * (size_t)n);
    long double *effects = (long double *)malloc(sizeof *effects * (size_t)resamples);
    int rc = -1;
    if (draw && effects) {
        for (int r = 0; r < resamples; r++) {
            for (int i = 0; i < n; i++) {
                uint64_t u = azs_splitmix64(&seed);
                draw[i] = (int)(((azs_u128)u * (uint64_t)n) >> 64);
            }
            effects[r] = azs_dz_sample(a, b, n, sign, draw);
        }
        qsort(effects, (size_t)resamples, sizeof *effects, azs_dz_cmp);
        double lo = azs_dz_output(azs_dz_quantile(effects, resamples, (1 - conf) / 2));
        double hi = azs_dz_output(azs_dz_quantile(effects, resamples, (1 + conf) / 2));
        if (!isnan(lo) && !isnan(hi)) {
            *out = (AzsBootCi){point, lo, hi, resamples};
            rc = 0;
        }
    }
    free(draw);
    free(effects);
    return rc;
}

static inline double azs_iut_p(const double *p, int k)
{
    if (!p || k < 1) return NAN;
    double largest = 0;
    for (int i = 0; i < k; i++) {
        if (!(p[i] >= 0 && p[i] <= 1)) return NAN;
        if (p[i] > largest) largest = p[i];
    }
    return largest;
}

static inline double azs_min_p_two(int n_nz)
{
    if (n_nz < 0) return NAN;
    return n_nz < 2 ? 1.0 : ldexp(1.0, 1 - n_nz);
}

typedef enum AzsVerdict {
    AZS_INVALID_INPUT = -1, AZS_FAIL_INVARIANT, AZS_FAIL_CEILING,
    AZS_INCONCLUSIVE_N, AZS_EFFECT, AZS_REVERSED, AZS_NO_EFFECT,
    AZS_INCONCLUSIVE, AZS_INERT
} AzsVerdict;

typedef struct AzsArmStats {
    double dz, lo, hi, p;
    int n_nz;
} AzsArmStats;

/* E1 has one intact/lesioned hash comparison per registered seed. Unordered
 * rows are allowed. Reject ambiguous or foreign rows. Missing rows produce
 * valid incomplete evidence. Empty input cannot establish equality. */
typedef struct AzsHashPair { uint64_t seed, intact, lesioned; } AzsHashPair;
typedef struct AzsE1Evidence {
    int expected, observed, matching, all_equal;
} AzsE1Evidence;

static inline int azs_e1_hash_evidence(const uint64_t *expected, int count,
                                       const AzsHashPair *rows, int n, AzsE1Evidence *out)
{
    if (!out) return -1;
    *out = (AzsE1Evidence){0, 0, 0, 0};
    if (count < 0 || n < 0 || n > count || (count && !expected) || (n && !rows)) return -1;
    for (int i = 0; i < count; i++)
        for (int j = 0; j < i; j++) if (expected[i] == expected[j]) return -1;
    int matching = 0;
    for (int i = 0; i < n; i++) {
        int j = 0;
        while (j < count && expected[j] != rows[i].seed) j++;
        if (j == count) return -1;
        for (int k = 0; k < i; k++) if (rows[k].seed == rows[i].seed) return -1;
        if (rows[i].intact == rows[i].lesioned) matching++;
    }
    *out = (AzsE1Evidence){count, n, matching, count > 0 && matching == count};
    return 0;
}

/* First matching ordinary row from protocol 07 section 3.5. 'n' is the
 * usable paired count across required arms; n_nz is arm-specific. The
 * power gate can decide even when no effect/interval is estimable. */
static inline AzsVerdict azs_statistical_verdict(const AzsArmStats *arms, int k, double holm,
    int n, int n_min, int m, int bounded, double maximum, double intact_mean, int invariant_failed)
{
    if (invariant_failed) return AZS_FAIL_INVARIANT;
    /* With zero usable pairs there is no intact mean for the ceiling row. */
    if (bounded && (!isfinite(maximum) || (n > 0 && !isfinite(intact_mean))))
        return AZS_INVALID_INPUT;
    if (bounded && n > 0 && intact_mean > 0.95 * maximum) return AZS_FAIL_CEILING;
    if (n < 0 || n > AZS_WSR_MAX || n_min < 1 || m < 1 || k < 1) return AZS_INVALID_INPUT;
    if (n < n_min) return AZS_INCONCLUSIVE_N;
    if (!arms) return AZS_INVALID_INPUT;
    int underpowered = 0;
    for (int i = 0; i < k; i++) {
        if (arms[i].n_nz < 0 || arms[i].n_nz > n) return AZS_INVALID_INPUT;
        if (azs_min_p_two(arms[i].n_nz) * m >= 0.05) underpowered = 1;
    }
    if (underpowered) return AZS_INCONCLUSIVE_N;
    if (!(holm >= 0 && holm <= 1)) return AZS_INVALID_INPUT;
    int all_effect = 1, reversed = 0, ruled_out = 0;
    for (int i = 0; i < k; i++) {
        AzsArmStats a = arms[i];
        if (isnan(a.dz) || !(a.lo <= a.hi) || !(a.p >= 0 && a.p <= 1)) return AZS_INVALID_INPUT;
        if (a.dz < 0.8) all_effect = 0;
        if (a.dz <= -0.8 && a.p < 0.05 / m) reversed = 1;
        if (a.hi < 0.8) ruled_out = 1;
    }
    if (all_effect && holm < 0.05) return AZS_EFFECT;
    if (reversed) return AZS_REVERSED;
    if (ruled_out) return AZS_NO_EFFECT;
    return AZS_INCONCLUSIVE;
}

/* Pass NULL for ordinary items. For E1, pass the validated per-seed evidence
 * returned by azs_e1_hash_evidence. The separate ordinary result preserves
 * the literal statistical hierarchy even for equal final hashes and
 * different intermediate histories. No E1 override before usable n_min. */
static inline AzsVerdict azs_verdict(const AzsArmStats *arms, int k, double holm,
    int n, int n_min, int m, int bounded, double maximum, double intact_mean, int invariant_failed,
    const AzsE1Evidence *e1, AzsVerdict *ordinary)
{
    AzsVerdict result = azs_statistical_verdict(arms, k, holm, n, n_min, m,
                                                 bounded, maximum, intact_mean, invariant_failed);
    if (ordinary) *ordinary = result;
    if (e1 && (result == AZS_INCONCLUSIVE_N || result == AZS_EFFECT || result == AZS_REVERSED ||
               result == AZS_NO_EFFECT || result == AZS_INCONCLUSIVE) && n >= n_min &&
        e1->all_equal && e1->expected >= n && e1->observed == e1->expected &&
        e1->matching == e1->expected)
        return AZS_INERT;
    return result;
}

#endif /* AZ_STATSUTIL_H */
