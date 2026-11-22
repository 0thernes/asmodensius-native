/* test_statsutil.c - checks of tools/az_statsutil.h (owner: stats-tool).
 *
 *   1. The exact signed-rank counts equal brute-force enumeration of every
 *      sign assignment for n <= 12, with ties, zeros and a tie tolerance.
 *   2. Hand-derived cases, the symmetry of the null distribution, 64 pairs
 *      without overflow, refusal of bad input.
 *   3. Holm on a worked example, clamping, monotonicity, order independence.
 *   4. Cohen d, Welch t, quartiles on hand-computed data.
 *   5. The bootstrap is deterministic, depends on its seed, and brackets the
 *      mean of well-separated data.
 *   6. Seam check: every AZ_LESION_* in core/az_abi.h is in the table of
 *      tools/az_lesion.c (a lesion added to the ABI must not be skipped
 *      silently by the evidence tool).
 */
#include "az_test.h"
#include "az_statsutil.h"

#define CHECK_NEAR(a, b, tol) CHECK(fabs((double)(a) - (double)(b)) <= (tol))

/* ---- brute force: enumerate all sign assignments with independently
 * computed midranks (O(n^2) counting, in doubled integers) ---- */
typedef struct Brute { int n_nz, w2_obs, w2_max; u64 ge, le, total; } Brute;

static Brute brute_wsr(const double *a, const double *b, int n, double tol)
{
    Brute r;
    memset(&r, 0, sizeof r);
    double ad[16];
    int sg[16], m = 0;
    for (int i = 0; i < n; i++) {
        double d = a[i] - b[i];
        if (fabs(d) > tol) { ad[m] = fabs(d); sg[m] = d > 0; m++; }
    }
    r.n_nz = m;
    r.total = 1ull << m;
    if (m == 0) { r.ge = r.le = 1; return r; }
    int tr[16];
    for (int i = 0; i < m; i++) {
        int less = 0, equal = 0;
        for (int j = 0; j < m; j++) {
            if (ad[j] < ad[i]) less++;
            else if (ad[j] == ad[i]) equal++;      /* includes i itself */
        }
        tr[i] = 2 * less + equal + 1;              /* 2 * (less + (equal + 1) / 2) */
        r.w2_max += tr[i];
        if (sg[i]) r.w2_obs += tr[i];
    }
    for (u64 mask = 0; mask < r.total; mask++) {
        int w = 0;
        for (int i = 0; i < m; i++) if (mask >> i & 1) w += tr[i];
        if (w >= r.w2_obs) r.ge++;
        if (w <= r.w2_obs) r.le++;
    }
    return r;
}

static u64 lcg_state = 0x243F6A8885A308D3ull;
static u32 lcg(void)
{
    lcg_state = lcg_state * 6364136223846793005ull + 1442695040888963407ull;
    return (u32)(lcg_state >> 33);
}

static void compare_with_brute(const double *a, const double *b, int n, double tol)
{
    AzsWsr w;
    Brute r = brute_wsr(a, b, n, tol);
    CHECK_EQ(azs_wsr_exact(a, b, n, tol, &w), 0);
    CHECK_EQ(w.n, n);
    CHECK_EQ(w.n_nz, r.n_nz);
    CHECK_EQ(w.w2_obs, r.w2_obs);
    CHECK_EQ(w.w2_max, r.w2_max);
    CHECK_EQ(w.w2_max, r.n_nz * (r.n_nz + 1));
    CHECK((u64)w.total == r.total && (u64)(w.total >> 64) == 0);
    CHECK((u64)w.tail_ge == r.ge && (u64)(w.tail_ge >> 64) == 0);
    CHECK((u64)w.tail_le == r.le && (u64)(w.tail_le >> 64) == 0);
    /* n <= 12: the fractions are exact doubles, so compare with == */
    CHECK(w.p_greater == (double)r.ge / (double)r.total);
    CHECK(w.p_less == (double)r.le / (double)r.total);
    u64 two = 2 * (r.ge < r.le ? r.ge : r.le);
    if (two > r.total) two = r.total;
    CHECK(w.p_two == (double)two / (double)r.total);
    CHECK_EQ(w.n_pos + w.n_neg, w.n_nz);
}

static void test_wsr_brute(void)
{
    double a[16], b[16];
    int cases = 0;
    /* value ranges from "almost all tied" to "almost no ties"; b = 0 half the time */
    static const u32 range[] = { 2, 3, 5, 9, 40, 1000000 };
    for (int n = 0; n <= 12; n++) {
        for (u32 ri = 0; ri < sizeof range / sizeof range[0]; ri++) {
            for (int rep = 0; rep < 12; rep++) {
                for (int i = 0; i < n; i++) {
                    a[i] = (double)(lcg() % range[ri]);
                    b[i] = (rep & 1) ? (double)(lcg() % range[ri]) : (double)(range[ri] / 2);
                }
                compare_with_brute(a, b, n, 0.0);
                cases++;
            }
        }
    }
    /* a shifted sample: mostly one sign, the case the tool meets */
    for (int n = 2; n <= 12; n++) {
        for (int rep = 0; rep < 10; rep++) {
            for (int i = 0; i < n; i++) { b[i] = (double)(lcg() % 7); a[i] = b[i] + (double)(lcg() % 6) - 1.0; }
            compare_with_brute(a, b, n, 0.0);
            cases++;
        }
    }
    /* tie tolerance: differences on a 0.5 grid, tol 0.25 leaves the groups
     * unchanged (the brute force applies the same drop rule, exact ties) */
    for (int rep = 0; rep < 20; rep++) {
        for (int i = 0; i < 11; i++) { a[i] = 0.5 * (double)(lcg() % 9); b[i] = 0.5 * (double)(lcg() % 9); }
        AzsWsr w0, w1;
        CHECK_EQ(azs_wsr_exact(a, b, 11, 0.0, &w0), 0);
        CHECK_EQ(azs_wsr_exact(a, b, 11, 0.25, &w1), 0);
        CHECK(w0.tail_ge == w1.tail_ge && w0.tail_le == w1.tail_le && w0.n_nz == w1.n_nz);
        cases++;
    }
    printf("signed-rank vs brute force: %d data sets\n", cases);
}

static void test_wsr_known(void)
{
    AzsWsr w;
    double z[64] = { 0 }, a[64], b[64];

    /* nothing differs: p = 1 on every side, test size 0 */
    for (int i = 0; i < 10; i++) a[i] = 3.0;
    CHECK_EQ(azs_wsr_exact(a, a, 10, 0.0, &w), 0);
    CHECK_EQ(w.n_nz, 0);
    CHECK(w.p_greater == 1.0 && w.p_less == 1.0 && w.p_two == 1.0);
    CHECK_EQ(azs_wsr_exact(a, a, 0, 0.0, &w), 0);
    CHECK(w.p_two == 1.0);

    /* hand-derived, with a tie group: d = 1, 1, -1, 2 -> midranks 2, 2, 2, 4;
     * W+ = 8 of 10. Of 16 assignments 4 reach W+ >= 8 and 15 have W+ <= 8. */
    {
        const double d[4] = { 1, 1, -1, 2 };
        CHECK_EQ(azs_wsr_exact(d, z, 4, 0.0, &w), 0);
        CHECK_EQ(w.w2_obs, 16); CHECK_EQ(w.w2_max, 20);
        CHECK_EQ((u64)w.tail_ge, 4); CHECK_EQ((u64)w.tail_le, 15);
        CHECK(w.p_greater == 0.25 && w.p_less == 0.9375 && w.p_two == 0.5);
        CHECK_EQ(w.n_pos, 3); CHECK_EQ(w.n_neg, 1);
    }
    /* hand-derived, no ties: ranks 1..9 with ranks 2 and 3 negative, W+ = 40.
     * Subsets of {1..9} with sum <= 5: {}, 1, 2, 3, 4, 5, 12, 13, 14, 23 = 10. */
    {
        const double d[9] = { 1, -2, -3, 4, 5, 6, 7, 8, 9 };
        CHECK_EQ(azs_wsr_exact(d, z, 9, 0.0, &w), 0);
        CHECK_EQ(w.w2_obs, 80);
        CHECK_EQ((u64)w.tail_ge, 10);
        CHECK(w.p_greater == 10.0 / 512.0);
        CHECK(w.p_two == 20.0 / 512.0);
        /* W+ <= 40 means W- >= 5. Only 7 subsets have W- <= 4: {}, 1, 2, 3, 4,
         * 12, 13. So 512 - 7 = 505; the three subsets with sum 5 ({5}, {1,4},
         * {2,3}) give W+ == 40 and count in both tails (the review's
         * enumeration of all 512 assignments agrees, finding v28). */
        CHECK_EQ((u64)w.tail_le, 512 - 7);
    }
    /* zeros are dropped: the same data plus three zero pairs */
    {
        const double d[12] = { 0, 1, -2, 0, -3, 4, 5, 6, 0, 7, 8, 9 };
        CHECK_EQ(azs_wsr_exact(d, z, 12, 0.0, &w), 0);
        CHECK_EQ(w.n, 12); CHECK_EQ(w.n_nz, 9);
        CHECK_EQ((u64)w.tail_ge, 10);
    }
    /* every difference positive and distinct: the smallest attainable p */
    for (int n = 1; n <= 64; n++) {
        for (int i = 0; i < n; i++) { a[i] = (double)(i + 1); b[i] = 0.0; }
        CHECK_EQ(azs_wsr_exact(a, b, n, 0.0, &w), 0);
        CHECK(w.tail_ge == 1);
        CHECK(w.tail_le == w.total);
        CHECK(w.total == (azs_u128)1 << n);
        CHECK(w.p_greater == ldexp(1.0, -n));
        CHECK(w.p_two == ldexp(1.0, 1 - n));
        CHECK(w.p_less == 1.0);
        /* swap the samples: the tails swap */
        AzsWsr v;
        CHECK_EQ(azs_wsr_exact(b, a, n, 0.0, &v), 0);
        CHECK(v.tail_le == 1 && v.tail_ge == v.total && v.p_two == w.p_two);
    }
    /* 64 pairs, mixed signs and ties: the counts must not overflow. Both
     * tails together cover every assignment, the observed value twice. */
    for (int rep = 0; rep < 50; rep++) {
        for (int i = 0; i < 64; i++) { a[i] = (double)(lcg() % 30); b[i] = (double)(lcg() % 30); }
        AzsWsr v;
        CHECK_EQ(azs_wsr_exact(a, b, 64, 0.0, &w), 0);
        CHECK_EQ(azs_wsr_exact(b, a, 64, 0.0, &v), 0);
        CHECK(w.tail_ge + w.tail_le > w.total);                 /* overlap = ways[w_obs] >= 1 */
        CHECK(w.tail_ge <= w.total && w.tail_le <= w.total);
        CHECK(w.tail_ge == v.tail_le && w.tail_le == v.tail_ge);   /* symmetry under swapping */
        CHECK_EQ(w.w2_obs + v.w2_obs, w.w2_max);
        CHECK(w.p_two == v.p_two && w.p_two > 0.0 && w.p_two <= 1.0);
    }
    /* a symmetric data set sits in the middle: two-sided p = 1 */
    {
        const double d[6] = { 1, -1, 2, -2, 3, -3 };
        CHECK_EQ(azs_wsr_exact(d, z, 6, 0.0, &w), 0);
        CHECK_EQ(2 * w.w2_obs, w.w2_max);
        CHECK(w.p_two == 1.0);
    }
    /* bad input is refused, not trusted */
    a[0] = NAN; b[0] = 0.0;
    CHECK_EQ(azs_wsr_exact(a, b, 1, 0.0, &w), -1);
    CHECK(isnan(w.p_two));
    a[0] = INFINITY;
    CHECK_EQ(azs_wsr_exact(a, b, 1, 0.0, &w), -1);
    a[0] = 1.0;
    CHECK_EQ(azs_wsr_exact(a, b, 65, 0.0, &w), -1);
    CHECK_EQ(azs_wsr_exact(a, b, -1, 0.0, &w), -1);
    CHECK_EQ(azs_wsr_exact(a, b, 1, -1.0, &w), -1);
    CHECK_EQ(azs_wsr_exact(a, b, 1, NAN, &w), -1);

    char buf[40];
    CHECK(strcmp(azs_u128_str((azs_u128)1 << 64, buf), "18446744073709551616") == 0);
    CHECK(strcmp(azs_u128_str(0, buf), "0") == 0);
}

static void test_holm(void)
{
    /* worked example, m = 4. Sorted: 0.005, 0.01, 0.03, 0.04.
     *   0.005 * 4 = 0.02
     *   0.01  * 3 = 0.03
     *   0.03  * 2 = 0.06
     *   0.04  * 1 = 0.04 -> raised to the running maximum 0.06 */
    {
        const double p[4] = { 0.01, 0.04, 0.03, 0.005 };
        double adj[4];
        CHECK_EQ(azs_holm(p, 4, adj), 0);
        CHECK_NEAR(adj[0], 0.03, 1e-15);
        CHECK_NEAR(adj[1], 0.06, 1e-15);
        CHECK_NEAR(adj[2], 0.06, 1e-15);
        CHECK_NEAR(adj[3], 0.02, 1e-15);
        /* at alpha 0.05 Holm rejects two of four; Bonferroni would also reject 0.005 and 0.01 only */
        int rejected = 0;
        for (int i = 0; i < 4; i++) if (adj[i] < 0.05) rejected++;
        CHECK_EQ(rejected, 2);
    }
    /* clamped at 1 */
    {
        const double p[3] = { 0.5, 0.6, 0.9 };
        double adj[3];
        CHECK_EQ(azs_holm(p, 3, adj), 0);
        CHECK(adj[0] == 1.0 && adj[1] == 1.0 && adj[2] == 1.0);
    }
    /* one test: unchanged. Equal p-values: equal adjusted values. */
    {
        double one = 0.0371, adj1;
        CHECK_EQ(azs_holm(&one, 1, &adj1), 0);
        CHECK(adj1 == one);
        const double p[5] = { 0.01, 0.01, 0.01, 0.01, 0.01 };
        double adj[5];
        CHECK_EQ(azs_holm(p, 5, adj), 0);
        for (int i = 0; i < 5; i++) CHECK_NEAR(adj[i], 0.05, 1e-15);
    }
    /* properties on random families: adj >= p, adj <= min(1, m p), same order
     * as p, and independent of the order the family is presented in */
    for (int rep = 0; rep < 200; rep++) {
        double p[40], adj[40], q[40], adjq[40];
        int m = 1 + (int)(lcg() % 40);
        for (int i = 0; i < m; i++) p[i] = (double)(lcg() % 2001) / 2000.0 * ((lcg() & 1) ? 0.05 : 1.0);
        CHECK_EQ(azs_holm(p, m, adj), 0);
        int ok = 1;
        for (int i = 0; i < m; i++) {
            double bonf = p[i] * m > 1.0 ? 1.0 : p[i] * m;
            if (!(adj[i] >= p[i] && adj[i] <= bonf && adj[i] <= 1.0)) ok = 0;
            for (int j = 0; j < m; j++) if (p[i] < p[j] && adj[i] > adj[j]) ok = 0;
        }
        CHECK(ok);
        for (int i = 0; i < m; i++) q[i] = p[m - 1 - i];
        CHECK_EQ(azs_holm(q, m, adjq), 0);
        int same = 1;
        for (int i = 0; i < m; i++) if (adjq[i] != adj[m - 1 - i]) same = 0;
        CHECK(same);
    }
    /* refusals */
    {
        double p[2] = { 0.5, 1.5 }, adj[2];
        CHECK_EQ(azs_holm(p, 2, adj), -1);
        CHECK(isnan(adj[0]) && isnan(adj[1]));
        p[1] = NAN;
        CHECK_EQ(azs_holm(p, 2, adj), -1);
        p[1] = -0.1;
        CHECK_EQ(azs_holm(p, 2, adj), -1);
        CHECK_EQ(azs_holm(p, 0, adj), -1);
    }
}

static void test_effect(void)
{
    /* differences 1, 2, 3, 4, 5: mean 3, sd sqrt(2.5), d = 1.8973665961 */
    {
        const double a[5] = { 11, 13, 15, 17, 19 }, b[5] = { 10, 11, 12, 13, 14 };
        CHECK_NEAR(azs_cohen_d_paired(a, b, 5), 3.0 / sqrt(2.5), 1e-14);
        CHECK_NEAR(azs_cohen_d_paired(b, a, 5), -3.0 / sqrt(2.5), 1e-14);
    }
    /* degenerate cases are decided on the data */
    {
        const double a[4] = { 5, 6, 7, 8 }, b[4] = { 4, 5, 6, 7 }, c[4] = { 6, 7, 8, 9 };
        CHECK(azs_cohen_d_paired(a, a, 4) == 0.0);
        CHECK(isinf(azs_cohen_d_paired(a, b, 4)) && azs_cohen_d_paired(a, b, 4) > 0);
        CHECK(isinf(azs_cohen_d_paired(a, c, 4)) && azs_cohen_d_paired(a, c, 4) < 0);
        CHECK(isnan(azs_cohen_d_paired(a, b, 1)));
    }
    /* Welch: a = 1..5 (mean 3, var 2.5), b = 2, 4, 6, 8, 10 (mean 6, var 10).
     * se^2 = 0.5 + 2 = 2.5, t = -3 / sqrt(2.5) = -1.8973665961,
     * df = 2.5^2 / (0.25 / 4 + 4 / 4) = 6.25 / 1.0625 = 5.8823529412 */
    {
        const double a[5] = { 1, 2, 3, 4, 5 }, b[5] = { 2, 4, 6, 8, 10 };
        AzsWelch r = azs_welch_t(a, 5, b, 5);
        CHECK_NEAR(r.mean_a, 3.0, 1e-15); CHECK_NEAR(r.mean_b, 6.0, 1e-15);
        CHECK_NEAR(r.var_a, 2.5, 1e-15);  CHECK_NEAR(r.var_b, 10.0, 1e-14);
        CHECK_NEAR(r.t, -3.0 / sqrt(2.5), 1e-14);
        CHECK_NEAR(r.df, 6.25 / 1.0625, 1e-13);
        /* unequal sizes: a = 1, 2, 3 (var 1), b = 4, 6 (var 2): se^2 = 1/3 + 1 */
        const double c[3] = { 1, 2, 3 }, d[2] = { 4, 6 };
        r = azs_welch_t(c, 3, d, 2);
        CHECK_NEAR(r.t, (2.0 - 5.0) / sqrt(1.0 / 3.0 + 1.0), 1e-14);
        CHECK_NEAR(r.df, (4.0 / 3.0) * (4.0 / 3.0) / ((1.0 / 9.0) / 2.0 + 1.0 / 1.0), 1e-13);
        /* zero variance on both sides */
        const double e[3] = { 2, 2, 2 }, f[3] = { 5, 5, 5 };
        r = azs_welch_t(e, 3, e, 3);
        CHECK(r.t == 0.0);
        r = azs_welch_t(e, 3, f, 3);
        CHECK(isinf(r.t) && r.t < 0 && isnan(r.df));
        r = azs_welch_t(e, 1, f, 3);
        CHECK(isnan(r.t));
    }
    /* quartiles, type 7: 1..8 -> 2.75, 4.5, 6.25; 1..5 -> 2, 3, 4; unsorted input */
    {
        const double x[8] = { 8, 3, 1, 6, 2, 7, 5, 4 };
        double q1, med, q3;
        CHECK_EQ(azs_quartiles(x, 8, &q1, &med, &q3), 0);
        CHECK(q1 == 2.75 && med == 4.5 && q3 == 6.25);
        CHECK(x[0] == 8.0);   /* input untouched */
        const double y[5] = { 5, 1, 4, 2, 3 };
        CHECK_EQ(azs_quartiles(y, 5, &q1, &med, &q3), 0);
        CHECK(q1 == 2.0 && med == 3.0 && q3 == 4.0);
        CHECK(azs_median(y, 5) == 3.0);
        const double one = 7.0;
        CHECK_EQ(azs_quartiles(&one, 1, &q1, &med, &q3), 0);
        CHECK(q1 == 7.0 && med == 7.0 && q3 == 7.0);
        CHECK_EQ(azs_quartiles(&one, 0, &q1, &med, &q3), -1);
        CHECK(isnan(med));
        CHECK(isnan(azs_mean(&one, 0)) && isnan(azs_var(&one, 1)));
        CHECK_NEAR(azs_var(y, 5), 2.5, 1e-15);
    }
}

static void test_bootstrap(void)
{
    double a[48], b[48];
    for (int i = 0; i < 48; i++) { b[i] = (double)(lcg() % 100); a[i] = b[i] + 10.0 + (double)(lcg() % 9) - 4.0; }
    AzsBootCi c1, c2, c3, c4;
    CHECK_EQ(azs_boot_ci_paired(a, b, 48, 2000, 0x5EEDull, 0.95, &c1), 0);
    CHECK_EQ(azs_boot_ci_paired(a, b, 48, 2000, 0x5EEDull, 0.95, &c2), 0);
    /* deterministic: bit-identical doubles on a second call. Member by member:
     * the struct has four bytes of tail padding, which hold whatever the stack
     * held, so a memcmp of the whole struct failed in the clang build */
    CHECK(memcmp(&c1.point, &c2.point, sizeof c1.point) == 0 && memcmp(&c1.lo, &c2.lo, sizeof c1.lo) == 0 &&
          memcmp(&c1.hi, &c2.hi, sizeof c1.hi) == 0 && c1.resamples == c2.resamples);
    CHECK(c1.lo == c2.lo && c1.hi == c2.hi && c1.point == c2.point);
    /* the seed matters (the interval is not a constant) */
    CHECK_EQ(azs_boot_ci_paired(a, b, 48, 2000, 0x5EEEull, 0.95, &c3), 0);
    CHECK(c3.lo != c1.lo || c3.hi != c1.hi);
    CHECK(c3.point == c1.point);
    /* sane: the interval holds the observed mean, lies near 10, and a wider
     * confidence level gives a wider interval from the same resamples */
    CHECK(c1.lo < c1.point && c1.point < c1.hi);
    CHECK(c1.lo > 8.0 && c1.hi < 12.0);
    CHECK_EQ(azs_boot_ci_paired(a, b, 48, 2000, 0x5EEDull, 0.99, &c4), 0);
    CHECK(c4.lo <= c1.lo && c4.hi >= c1.hi && c4.hi - c4.lo > c1.hi - c1.lo);
    /* constant differences: a degenerate interval at the constant */
    for (int i = 0; i < 48; i++) a[i] = b[i] + 3.0;
    CHECK_EQ(azs_boot_ci_paired(a, b, 48, 500, 1, 0.95, &c1), 0);
    CHECK(c1.lo == 3.0 && c1.hi == 3.0 && c1.point == 3.0);
    /* the generator itself: SplitMix64 from state 0 (first outputs of the
     * published reference implementation) */
    u64 s = 0;
    CHECK(azs_splitmix64(&s) == 0xE220A8397B1DCDAFull);
    CHECK(azs_splitmix64(&s) == 0x6E789E6AA1B965F4ull);
    CHECK(azs_splitmix64(&s) == 0x06C45D188009454Full);
    /* refusals */
    CHECK_EQ(azs_boot_ci_paired(a, b, 0, 100, 1, 0.95, &c1), -1);
    CHECK_EQ(azs_boot_ci_paired(a, b, 48, 0, 1, 0.95, &c1), -1);
    CHECK_EQ(azs_boot_ci_paired(a, b, 48, 100, 1, 1.0, &c1), -1);
    CHECK(isnan(c1.lo));
}

/* ---- seam check: the lesion table of the tool against the ABI header ---- */
static char *slurp(const char *dir, const char *rel)
{
    char path[1024];
    snprintf(path, sizeof path, "%s%s", dir, rel);
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)n + 1);
    if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); buf = NULL; }
    if (buf) buf[n] = 0;
    fclose(f);
    return buf;
}

static void test_lesion_table(void)
{
    /* the directory of this source file, as the compiler was given it */
    char dir[1024];
    snprintf(dir, sizeof dir, "%s", __FILE__);
    char *cut = dir;
    for (char *p = dir; *p; p++) if (*p == '/' || *p == '\\') cut = p + 1;
    *cut = 0;
    char *abi = slurp(dir, "../core/az_abi.h");
    char *tool = slurp(dir, "../tools/az_lesion.c");
    CHECK(abi != NULL);
    CHECK(tool != NULL);
    if (!abi || !tool) { printf("note: sources not found from %s; lesion table not checked\n", dir); free(abi); free(tool); return; }
    int in_abi = 0, missing = 0;
    for (const char *p = abi; (p = strstr(p, "AZ_LESION_")) != NULL;) {
        char name[64];
        int k = 0;
        while (k < 63 && (p[k] == '_' || (p[k] >= 'A' && p[k] <= 'Z') || (p[k] >= '0' && p[k] <= '9'))) { name[k] = p[k]; k++; }
        name[k] = 0;
        const char *q = p + k;
        while (*q == ' ') q++;
        p += k;
        if (*q != '=' || k <= 10) continue;       /* only the defining lines "AZ_LESION_X = 1u << n" */
        in_abi++;
        char want[96];
        snprintf(want, sizeof want, "LESION(%s)", name + 10);
        if (!strstr(tool, want)) { missing++; printf("lesion %s is in az_abi.h but not in tools/az_lesion.c\n", name); }
    }
    int in_tool = 0;
    for (const char *p = tool; (p = strstr(p, "    LESION(")) != NULL; p += 11) in_tool++;
    printf("lesion table: %d switches in az_abi.h, %d rows in az_lesion.c\n", in_abi, in_tool);
    CHECK(in_abi >= 21);
    CHECK_EQ(missing, 0);
    CHECK_EQ(in_tool, in_abi);
    free(abi);
    free(tool);
}

/* Alternating signs for untied ranks 1..n. Coefficients of
 * (1+x)(1+x^2)...(1+x^n) count subsets at each W+. Sum both inclusive tails. */
static void test_sb05_tables(void)
{
    const double a[8] = {1,-2,3,-4,5,-6,7,-8}, z[8] = {0};
    const unsigned ge[8] = {1,3,3,11,13,42,52,161};
    const unsigned le[8] = {2,2,6,7,22,27,84,108};
    for (int n = 1; n <= 8; n++) {
        AzsWsr w;
        CHECK_EQ(azs_wsr_exact(a,z,n,0,&w), 0);
        CHECK(w.tail_ge == ge[n-1] && w.tail_le == le[n-1]);
        double tail = (double)(ge[n-1] < le[n-1] ? ge[n-1] : le[n-1]);
        CHECK(w.p_two == fmin(1, 2 * tail / (1u << n)));
    }
    const double p[4] = {.01,.04,.03,.005};
    double h[4];
    CHECK_EQ(azs_holm(p,4,h), 0);
    CHECK(h[0] == .03 && h[1] == .06 && h[2] == .06 && h[3] == .02);
    CHECK(azs_iut_p(p,4) == .04 && azs_iut_p(p,1) == .01);
    const double bad[3] = {.4,NAN,.9}, overshoot = 1.001;
    CHECK(isnan(azs_iut_p(bad,3)) && isnan(azs_iut_p(&overshoot,1)) && isnan(azs_iut_p(p,0)));
    CHECK(azs_min_p_two(0) == 1 && azs_min_p_two(1) == 1 && azs_min_p_two(2) == .5);
    CHECK(azs_min_p_two(64) == 0x1p-63 && isnan(azs_min_p_two(-1)));
}

static void check_sha_vector(const void *msg, size_t n, const char *hex)
{
    unsigned char one[32], split[32];
    char rendered[65];
    az_sha256(msg, n, one);
    for (int i = 0; i < 32; i++) snprintf(rendered + 2*i, 3, "%02x", (unsigned)one[i]);
    CHECK(strcmp(rendered, hex) == 0);
    for (size_t i = 0; i <= n; i++) {
        AzSha256 sha;
        az_sha256_init(&sha);
        az_sha256_update(&sha, msg, i);
        az_sha256_update(&sha, (const unsigned char *)msg + i, n-i);
        az_sha256_final(&sha, split);
        CHECK(memcmp(split, one, sizeof one) == 0);
    }
}

static void test_sb05_sha(void)
{
    const char *long_msg = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    check_sha_vector("", 0, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    check_sha_vector("abc", 3, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    check_sha_vector(long_msg, 56, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    unsigned char bytes[127];
    for (int i = 0; i < 127; i++) bytes[i] = (unsigned char)i;
    check_sha_vector(bytes,55,"463eb28e72f82e0a96c0a4cc53690c571281131f672aa229e0d45ae59b598b59");
    check_sha_vector(bytes,63,"29af2686fd53374a36b0846694cc342177e428d1647515f078784d69cdb9e488");
    check_sha_vector(bytes,64,"fdeab9acf3710362bd2658cdc9a29e8f9c757fcf9811603a8c447cd1d9151108");
    check_sha_vector(bytes,65,"4bfd2c8b6f1eec7a2afeb48b934ee4b2694182027e6d0fc075074f2fabb31781");
    check_sha_vector(bytes,127,"92ca0fa6651ee2f97b884b7246a562fa71250fedefe5ebf270d31c546bfea976");
    CHECK(azs_boot_seed("fixture-item","fixture-arm") == UINT64_C(0xd64dc67c779de512));
}

static void test_sb05_bootstrap(void)
{
    /* Multiple baselines, signed differences -5,1,2,1,8,0,-1. */
    const double a[] = {4,13,1,6,19,13,4}, b[] = {9,12,-1,5,11,13,5};
    uint64_t seed = azs_boot_seed("fixture-item", "fixture-arm");
    AzsBootCi c, repeat, reverse;
    CHECK_EQ(azs_boot_ci_dz(a,b,7,1,10000,seed,.95,&c), 0);
    CHECK_EQ(azs_boot_ci_dz(a,b,7,1,10000,seed,.95,&repeat), 0);
    CHECK_EQ(azs_boot_ci_dz(a,b,7,-1,10000,seed,.95,&reverse), 0);
    CHECK(!memcmp(&c.point,&repeat.point,sizeof(double)) && !memcmp(&c.lo,&repeat.lo,sizeof(double)) &&
          !memcmp(&c.hi,&repeat.hi,sizeof(double)) && c.resamples == 10000 && repeat.resamples == c.resamples);
    CHECK_NEAR(c.point, .22026692946430568, 1e-14);
    CHECK_NEAR(c.lo, -.6032931878409815, 1e-14);
    CHECK_NEAR(c.hi, 1.124636991375145, 1e-14);
    CHECK(reverse.point == -c.point);
    CHECK_NEAR(reverse.lo, -c.hi, 1e-14);
    CHECK_NEAR(reverse.hi, -c.lo, 1e-14);
    const double z[] = {0,0}, two[] = {2,2}, mixed[] = {-1,1}, invalid[] = {NAN,1};
    CHECK_EQ(azs_boot_ci_dz(two,z,2,-1,10,seed,.95,&c),0);
    CHECK(c.point == -INFINITY && c.lo == -INFINITY && c.hi == -INFINITY);
    CHECK_EQ(azs_boot_ci_dz(two,z,2,1,10,seed,.95,&c),0);
    CHECK(c.point == INFINITY && c.lo == INFINITY && c.hi == INFINITY);
    CHECK_EQ(azs_boot_ci_dz(two,two,2,1,10,seed,.95,&c),0);
    CHECK(c.point == 0 && c.lo == 0 && c.hi == 0);
    CHECK_EQ(azs_boot_ci_dz(mixed,z,2,1,100,seed,.95,&c),0);
    CHECK(c.point == 0 && c.lo == -INFINITY && c.hi == INFINITY);
    /* Identical signed effect at widely different physical units. */
    const double huge[] = {0x1p+1000,0x1p+1001}, tiny[] = {0x1p-1000,0x1p-999};
    CHECK_EQ(azs_boot_ci_dz(huge,z,2,1,100,seed,.95,&c),0);
    CHECK_NEAR(c.point, 3 / sqrt(2.0), 1e-14);
    CHECK_EQ(azs_boot_ci_dz(tiny,z,2,1,100,seed,.95,&repeat),0);
    CHECK(c.point == repeat.point && c.lo == repeat.lo && c.hi == repeat.hi);
    /* Adjacent integers have SD sqrt(0.5) whichever physical origin they use. */
    const double near[] = {0x1p52, 0x1p52 + 1};
    const double ideal = (double)((0x1p52L + .5L) * sqrtl(2));
    CHECK_EQ(azs_boot_ci_dz(near,z,2,1,10000,seed,.95,&c),0);
    CHECK(fabs(c.point / ideal - 1) < 4 * DBL_EPSILON);
    CHECK(c.lo == c.point && c.hi == INFINITY);
    CHECK_EQ(azs_cohen_d_paired(near,z,2),c.point);
    const double larger[] = {DBL_MAX, DBL_MAX / 2}, negative[] = {-DBL_MAX, -DBL_MAX / 2};
    CHECK_EQ(azs_boot_ci_dz(larger,negative,2,-1,1000,seed,.95,&c),0);
    CHECK_NEAR(c.point,-3/sqrt(2.),1e-14);
    CHECK(c.lo == -INFINITY && c.hi == c.point);
    /* Difference of the paired differences can live entirely below the
     * precision of their common offset, even in long double. */
    const double common[] = {0x1p80,0x1p80}, baseline[] = {1,2};
    CHECK_EQ(azs_boot_ci_dz(common,baseline,2,1,1000,seed,.95,&c),0);
    CHECK(fabs(c.point / (0x1p80 * sqrt(2.)) - 1) < 4 * DBL_EPSILON);
    CHECK(c.lo == c.point && c.hi == INFINITY);
    const double extreme[] = {DBL_MAX,DBL_MAX}, sub[] = {0,DBL_TRUE_MIN};
    CHECK_EQ(azs_boot_ci_dz(extreme,sub,2,1,1000,seed,.95,&c),-1);
    CHECK(isnan(c.point) && isnan(c.lo) && isnan(c.hi) && c.resamples == 0);
    CHECK(isnan(azs_cohen_d_paired(extreme,sub,2)));
    CHECK_EQ(azs_boot_ci_dz(extreme,z,2,1,1000,seed,.95,&c),0);
    CHECK(c.point == INFINITY && c.lo == INFINITY && c.hi == INFINITY);
    const double multi[] = {0x1p500,0x1p200,1,-0x1p500,-0x1p200};
    const double zeros[] = {0,0,0,0,0};
    CHECK(azs_cohen_d_paired(multi,zeros,5) > 0);
    const long double outer[] = {-2.0L*DBL_MAX,2.0L*DBL_MAX};
    CHECK(azs_dz_quantile(outer,2,.5) == 0);
    const long double both[] = {-INFINITY, INFINITY}, ext[] = {-INFINITY, 2, INFINITY};
    CHECK(isnan(azs_dz_quantile(both,2,.25)) && azs_dz_quantile(ext,3,.5) == 2);
    CHECK(azs_dz_quantile(ext,3,.1) == -INFINITY && azs_dz_quantile(ext,3,.9) == INFINITY);
    CHECK_EQ(azs_boot_ci_dz(invalid,z,2,1,10,seed,.95,&c),-1);
    CHECK_EQ(azs_boot_ci_dz(z,z,1,1,10,seed,.95,&c),-1);
    CHECK_EQ(azs_boot_ci_dz(z,z,2,0,10,seed,.95,&c),-1);
    CHECK_EQ(azs_boot_ci_dz(z,z,2,1,0,seed,.95,&c),-1);
    CHECK_EQ(azs_boot_ci_dz(z,z,2,1,10,seed,1,&c),-1);
    CHECK(isnan(c.point) && isnan(c.lo) && isnan(c.hi) && c.resamples == 0);
}

static void test_sb05_verdict(void)
{
    AzsArmStats arms[] = {{.8, .3, 1.2, .001, 8}, {.8, .2, 1.0, .002, 8}};
    double holm = nextafter(.05, 0), mean = 95;
    int n = 8, minimum = 8, bound = 1, invariant = 0;
    const AzsE1Evidence *e1 = NULL;
    AzsVerdict ordinary;
#define SB05_DECISION() azs_verdict(arms,2,holm,n,minimum,2,bound,100,mean,invariant,e1,&ordinary)
    CHECK_EQ(SB05_DECISION(), AZS_EFFECT);
    invariant = 1;
    mean = nextafter(95, INFINITY);
    CHECK_EQ(SB05_DECISION(), AZS_FAIL_INVARIANT);
    invariant = 0;
    CHECK_EQ(SB05_DECISION(), AZS_FAIL_CEILING);
    bound = 0;
    CHECK_EQ(SB05_DECISION(), AZS_EFFECT);
    bound = 1;
    mean = 95;
    minimum++;
    CHECK_EQ(SB05_DECISION(), AZS_INCONCLUSIVE_N);
    minimum--;
    arms[1].n_nz = 6; /* attainable p times two = .0625 */
    CHECK_EQ(SB05_DECISION(), AZS_INCONCLUSIVE_N);
    arms[1].n_nz++;
    CHECK_EQ(SB05_DECISION(), AZS_EFFECT);
    holm = .05;
    CHECK_EQ(SB05_DECISION(), AZS_INCONCLUSIVE);
    holm = nextafter(.05, 0);
    arms[0].dz = nextafter(.8, 0);
    CHECK_EQ(SB05_DECISION(), AZS_INCONCLUSIVE);
    arms[0].dz = -.8;
    arms[0].p = nextafter(.025, 0);
    arms[1].dz = .2;
    arms[1].hi = nextafter(.8, 0);
    CHECK_EQ(SB05_DECISION(), AZS_REVERSED);
    arms[0].p = .025;
    CHECK_EQ(SB05_DECISION(), AZS_NO_EFFECT);
    arms[1].hi = .8;
    CHECK_EQ(SB05_DECISION(), AZS_INCONCLUSIVE);
    arms[0].dz = nextafter(-.8, 0);
    arms[0].p = .001;
    CHECK_EQ(SB05_DECISION(), AZS_INCONCLUSIVE);
    arms[1].dz = NAN;
    CHECK_EQ(SB05_DECISION(), AZS_INVALID_INPUT);
    arms[1].dz = 1.2;
    arms[0].dz = 1.2;
    /* Section 3.6 does not require the registered maximum to be positive. */
    CHECK_EQ(azs_verdict(arms,2,holm,n,minimum,2,1,0,0,0,NULL,NULL),AZS_EFFECT);
    uint64_t seeds[] = {10,11,12,13,14,15,16,17};
    AzsHashPair rows[8];
    for (int i = 0; i < 8; i++) rows[i] = (AzsHashPair){seeds[7-i],(uint64_t)i,(uint64_t)i};
    AzsE1Evidence evidence;
    e1 = &evidence;
    CHECK_EQ(azs_e1_hash_evidence(seeds,8,rows,7,&evidence),0);
    CHECK(!e1->all_equal && e1->observed == 7 && e1->matching == 7);
    CHECK_EQ(SB05_DECISION(), AZS_EFFECT);
    rows[2].intact++;
    CHECK_EQ(azs_e1_hash_evidence(seeds,8,rows,8,&evidence),0);
    CHECK(!e1->all_equal && e1->matching == 7);
    CHECK_EQ(SB05_DECISION(), AZS_EFFECT);
    rows[2].intact--;
    CHECK_EQ(azs_e1_hash_evidence(seeds,8,rows,8,&evidence),0);
    CHECK(e1->all_equal && e1->observed == 8 && e1->matching == 8);
    CHECK_EQ(SB05_DECISION(), AZS_INERT);  /* equal final hashes, measure from differing histories */
    CHECK_EQ(ordinary, AZS_EFFECT);
    arms[1].n_nz = 0;
    CHECK_EQ(SB05_DECISION(), AZS_INERT);
    CHECK_EQ(ordinary, AZS_INCONCLUSIVE_N);
    n = 0;
    CHECK_EQ(SB05_DECISION(), AZS_INCONCLUSIVE_N);
    mean = NAN;
    CHECK_EQ(SB05_DECISION(), AZS_INCONCLUSIVE_N);
    n = 7;
    mean = 95;
    CHECK_EQ(SB05_DECISION(), AZS_INCONCLUSIVE_N);
    n = 8;
    mean = 96;
    CHECK_EQ(SB05_DECISION(), AZS_FAIL_CEILING);
    invariant = 1;
    CHECK_EQ(SB05_DECISION(), AZS_FAIL_INVARIANT);
    CHECK_EQ(azs_e1_hash_evidence(NULL,0,NULL,0,&evidence),0);
    CHECK(!e1->all_equal);
    rows[0].seed = rows[1].seed;
    CHECK_EQ(azs_e1_hash_evidence(seeds,8,rows,8,&evidence),-1);
    rows[0].seed = 99;
    CHECK_EQ(azs_e1_hash_evidence(seeds,8,rows,8,&evidence),-1);
    seeds[1] = seeds[0];
    CHECK_EQ(azs_e1_hash_evidence(seeds,8,NULL,0,&evidence),-1);
#undef SB05_DECISION
}

int main(void)
{
    test_sb05_tables();
    test_sb05_sha();
    test_sb05_bootstrap();
    test_sb05_verdict();
    test_wsr_brute();
    test_wsr_known();
    test_holm();
    test_effect();
    test_bootstrap();
    test_lesion_table();
    return az_test_done();
}
