/* test_tk_qgt.c - tests of core/tk_qgt.c (fixed-point quantum geometric
 * tensor, squared overlap, diagonal preconditioner).
 *
 * Hosted C: floating point is used here only to build inputs and reference
 * values. Sections:
 *   A  closed forms on a two-level system (Bloch sphere)
 *   B  bit-exact agreement with a 128-bit reference of the same formula
 *   C  agreement with the upstream QGTL kernels (verbatim fragment)
 *   D  phase invariance (global and parameter-dependent)
 *   E  positive semidefinite metric, symmetry, antisymmetry
 *   F  real amplitudes: classical Fisher metric, zero curvature
 *   G  squared overlap
 *   H  preconditioner
 *   I  argument handling and extreme inputs
 */
#include "az_test.h"
#include "../core/tk_qgt.h"
#include <math.h>

#define PI 3.14159265358979323846
#define Q16 65536.0
#define MAXD 8
#define MAXN 8

static double worst_a = 0.0;   /* worst |fixed - closed form|, in Q16 steps */

static i16 q14(double v)
{
    double s = floor(v * 16384.0 + 0.5);
    if (s > 32767.0) s = 32767.0;
    if (s < -32768.0) s = -32768.0;
    return (i16)s;
}

static u64 rng_state = 0x9E3779B97F4A7C15ull;
static u32 rnd32(void)
{
    return (u32)(az_test_rand64(&rng_state) >> 32);
}
static double rnd_unit(void) { return (double)rnd32() / 4294967296.0; }      /* [0,1) */
static double rnd_sym(void) { return 2.0 * rnd_unit() - 1.0; }               /* [-1,1) */

/* ---- A: two-level system ------------------------------------------------ */
/* psi = (cos(t/2), e^{ip} sin(t/2)); parameters (t, p).
 * g = diag(1/4, sin^2(t)/4), f_tp = -sin(t)/2, f_pt = +sin(t)/2. */
static void bloch(double t, double p, double gauge_t, double gauge_p, double *psi, double *dpsi)
{
    double c = cos(t / 2), s = sin(t / 2);
    double v[4] = { c, 0.0, s * cos(p), s * sin(p) };
    double dt[4] = { -s / 2, 0.0, c / 2 * cos(p), c / 2 * sin(p) };
    double dp[4] = { 0.0, 0.0, -s * sin(p), s * cos(p) };
    /* optional parameter-dependent phase e^{i(gauge_t*t + gauge_p*p)}:
     * d_a(e^{i ph} psi) = e^{i ph} (d_a psi + i*gauge_a*psi) */
    double ph = gauge_t * t + gauge_p * p, cr = cos(ph), ci = sin(ph);
    for (int k = 0; k < 2; k++) {
        double vr = v[2 * k], vi = v[2 * k + 1];
        double ar = dt[2 * k] - gauge_t * vi, ai = dt[2 * k + 1] + gauge_t * vr;
        double br = dp[2 * k] - gauge_p * vi, bi = dp[2 * k + 1] + gauge_p * vr;
        psi[2 * k] = vr * cr - vi * ci;      psi[2 * k + 1] = vr * ci + vi * cr;
        dpsi[2 * k] = ar * cr - ai * ci;     dpsi[2 * k + 1] = ar * ci + ai * cr;
        dpsi[4 + 2 * k] = br * cr - bi * ci; dpsi[4 + 2 * k + 1] = br * ci + bi * cr;
    }
}

static void quantise(const double *v, i16 *q, u32 count);
static double bound_steps(const double *psi, const double *d, u32 dim, u32 a, u32 b);

/* stated_steps > 0: the fixed tolerance stated in tk_qgt.h for this state;
 * always: the general input-rounding bound of tk_qgt.h. Returns the worst
 * error seen, in Q16 steps (f counted at half weight). */
static double check_bloch(double t, double p, double gt, double gp, double stated_steps)
{
    double v[4], d[8], worst = 0.0;
    i16 psi[4], dpsi[8];
    i32 g[4], f[4];
    bloch(t, p, gt, gp, v, d);
    quantise(v, psi, 4);
    quantise(d, dpsi, 8);
    CHECK(tk_qgt(psi, dpsi, 2, 2, g, f));
    double want_g[4] = { 0.25, 0.0, 0.0, sin(t) * sin(t) / 4 };
    double want_f[4] = { 0.0, -sin(t) / 2, sin(t) / 2, 0.0 };
    for (u32 i = 0; i < 4; i++) {
        double eg = fabs((double)g[i] - want_g[i] * Q16);
        double ef = fabs((double)f[i] - want_f[i] * Q16);
        double bd = bound_steps(v, d, 2, i / 2, i % 2);
        if (eg > worst) worst = eg;
        if (ef / 2 > worst) worst = ef / 2;
        CHECK(eg <= bd);
        CHECK(ef <= 2 * bd);
        if (stated_steps > 0.0) {
            CHECK(eg <= stated_steps);
            CHECK(ef <= 2 * stated_steps);
        }
    }
    if (worst > worst_a && stated_steps > 0.0) worst_a = worst;
    return worst;
}

static void test_closed_forms(void)
{
    /* stated tolerance: 24 steps of Q16 for g, 48 for f (header, ACCURACY) */
    static const double ts[] = { 0.0, 0.3, PI / 3, PI / 2, 2.0, 2.9, PI };
    static const double ps[] = { 0.0, 0.7, PI / 2, 2.5, 4.0, 5.9 };
    for (unsigned i = 0; i < AZ_LEN(ts); i++)
        for (unsigned j = 0; j < AZ_LEN(ps); j++)
            check_bloch(ts[i], ps[j], 0.0, 0.0, 24.0);
    for (int i = 0; i < 2000; i++)
        check_bloch(rnd_unit() * PI, rnd_unit() * 2 * PI, 0.0, 0.0, 24.0);

    /* exact spot values at t = pi/2, p = 0: all inputs are 0 or +-5793/11585 */
    i16 psi[4], dpsi[8];
    i32 g[4], f[4];
    double v[4], d[8];
    bloch(PI / 2, 0.0, 0.0, 0.0, v, d);
    quantise(v, psi, 4);
    quantise(d, dpsi, 8);
    CHECK(tk_qgt(psi, dpsi, 2, 2, g, f));
    CHECK(abs(g[0] - 16384) <= 2);          /* 1/4 */
    CHECK(abs(g[3] - 16384) <= 4);          /* sin^2(pi/2)/4 */
    CHECK(abs(f[1] + 32768) <= 4);          /* -1/2 */
    CHECK_EQ(f[2], -f[1]);
    CHECK_EQ(f[0], 0);
    CHECK_EQ(f[3], 0);
    printf("A closed forms: worst error %.2f steps of Q16 (stated tolerance 24)\n", worst_a);
}

/* ---- B: bit-exact against a 128-bit reference of the same formula -------- */
typedef __int128 i128;

static void ref_inner(const i16 *u, const i16 *v, u32 dim, i128 *re, i128 *im)
{
    i128 sr = 0, si = 0;
    for (u32 k = 0; k < dim; k++) {
        i128 ur = u[2 * k], ui = u[2 * k + 1], vr = v[2 * k], vi = v[2 * k + 1];
        sr += ur * vr + ui * vi;
        si += ur * vi - ui * vr;
    }
    *re = sr; *im = si;
}

/* round(Q * 2^16) with ties toward +infinity, from the exact Q56 value */
static void ref_qgt(const i16 *psi, const i16 *dpsi, u32 dim, u32 n, i64 *g, i64 *f)
{
    const i128 half = (i128)1 << 39;
    for (u32 a = 0; a < n; a++) {
        for (u32 b = a; b < n; b++) {
            i128 gr, gi, ar, ai, br, bi;
            ref_inner(dpsi + a * 2 * dim, dpsi + b * 2 * dim, dim, &gr, &gi);
            ref_inner(psi, dpsi + a * 2 * dim, dim, &ar, &ai);
            ref_inner(psi, dpsi + b * 2 * dim, dim, &br, &bi);
            i128 qr = gr * ((i128)1 << 28) - (ar * br + ai * bi);
            i128 qi = gi * ((i128)1 << 28) - (ar * bi - ai * br);
            g[a * n + b] = g[b * n + a] = (i64)((qr + half) >> 40);
            f[a * n + b] = (i64)((-2 * qi + half) >> 40);
            f[b * n + a] = -f[a * n + b];
        }
    }
}

static i64 ref_fidelity(const i16 *a, const i16 *b, u32 dim)
{
    i128 sr, si;
    ref_inner(a, b, dim, &sr, &si);
    return (i64)((sr * sr + si * si + ((i128)1 << 39)) >> 40);
}

static i16 rnd_i16(int mode)
{
    static const i16 edge[6] = { -32768, 32767, -32767, 0, 1, -1 };
    if (mode == 1) return edge[rnd32() % 6];                   /* extremes only */
    if (mode == 2 && (rnd32() & 3) == 0) return edge[rnd32() % 6];
    return (i16)(rnd32() & 0xFFFF);
}

static void test_exact(void)
{
    i16 psi[2 * MAXD], dpsi[2 * MAXD * MAXN];
    i32 g[MAXN * MAXN], f[MAXN * MAXN];
    i64 rg[MAXN * MAXN], rf[MAXN * MAXN];
    int before = az_failures;
    for (int it = 0; it < 6000; it++) {
        u32 dim = 1 + rnd32() % MAXD, n = 1 + rnd32() % MAXN;
        int mode = it % 3;
        for (u32 k = 0; k < 2 * dim; k++) psi[k] = rnd_i16(mode);
        for (u32 k = 0; k < 2 * dim * n; k++) dpsi[k] = rnd_i16(mode);
        CHECK(tk_qgt(psi, dpsi, dim, n, g, f));
        ref_qgt(psi, dpsi, dim, n, rg, rf);
        int bad = 0;
        for (u32 k = 0; k < n * n; k++) bad += (g[k] != rg[k]) + (f[k] != rf[k]);
        CHECK_EQ(bad, 0);
        CHECK_EQ(tk_fidelity_q16(psi, dpsi, dim), ref_fidelity(psi, dpsi, dim));
        CHECK_EQ(tk_fidelity_q16(dpsi, psi, dim), ref_fidelity(psi, dpsi, dim));
    }
    /* the largest values the formats allow: every component -32768, dim 8, n 8 */
    for (u32 k = 0; k < 2 * MAXD; k++) psi[k] = -32768;
    for (u32 k = 0; k < 2 * MAXD * MAXN; k++) dpsi[k] = -32768;
    CHECK(tk_qgt(psi, dpsi, MAXD, MAXN, g, f));
    ref_qgt(psi, dpsi, MAXD, MAXN, rg, rf);
    for (u32 k = 0; k < MAXN * MAXN; k++) { CHECK_EQ(g[k], rg[k]); CHECK_EQ(f[k], rf[k]); }
    CHECK_EQ(tk_fidelity_q16(psi, psi, MAXD), (i64)1 << 28);
    printf("B exact vs 128-bit reference: %s\n", az_failures == before ? "identical" : "MISMATCH");
}

/* ---- C: the upstream QGTL kernels as oracle ----------------------------- */
typedef struct { struct { u64 metric_evaluations, curvature_evaluations; } stats; } diffgeo_engine_t;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#pragma GCC diagnostic ignored "-Wshadow"
#include "upstream/qgtl/diffgeo_qgt_fragment.inc"
#pragma GCC diagnostic pop

static double l1norm(const double *v, u32 dim)
{
    double s = 0.0;
    for (u32 k = 0; k < 2 * dim; k++) s += fabs(v[k]);
    return s;
}

static double overlap_abs(const double *u, const double *v, u32 dim)
{
    double sr = 0.0, si = 0.0;
    for (u32 k = 0; k < dim; k++) {
        sr += u[2 * k] * v[2 * k] + u[2 * k + 1] * v[2 * k + 1];
        si += u[2 * k] * v[2 * k + 1] - u[2 * k + 1] * v[2 * k];
    }
    return sqrt(sr * sr + si * si);
}

/* The ACCURACY bound of tk_qgt.h for entry (a,b) of g, in Q16 steps, plus one
 * step for the output rounding and the O(e^2) terms. f gets twice this. */
static double bound_steps(const double *psi, const double *d, u32 dim, u32 a, u32 b)
{
    const double e = 1.0 / 32768.0;
    const double *da = d + a * 2 * dim, *db = d + b * 2 * dim;
    double lp = l1norm(psi, dim), la = l1norm(da, dim), lb = l1norm(db, dim);
    double xa = overlap_abs(psi, da, dim), xb = overlap_abs(psi, db, dim);
    return Q16 * e * (la + lb + sqrt(2.0) * (xa * (lp + lb) + xb * (lp + la))) + 1.0;
}

/* random unit state and n derivative vectors with components below dmax */
static void rand_case(double *psi, double *d, u32 dim, u32 n, double dmax)
{
    double nn = 0.0;
    for (u32 k = 0; k < 2 * dim; k++) { psi[k] = rnd_sym(); nn += psi[k] * psi[k]; }
    nn = sqrt(nn);
    for (u32 k = 0; k < 2 * dim; k++) psi[k] /= nn;
    for (u32 k = 0; k < 2 * dim * n; k++) d[k] = rnd_sym() * dmax;
}

static void quantise(const double *v, i16 *q, u32 count)
{
    for (u32 k = 0; k < count; k++) q[k] = q14(v[k]);
}

static void upstream_qgt(const double *psi, const double *d, u32 dim, u32 n, double *g, double *f)
{
    ComplexDouble st[MAXD], pd[MAXD * MAXN];
    diffgeo_engine_t eng = { { 0, 0 } };
    for (u32 k = 0; k < dim; k++) st[k] = (ComplexDouble){ psi[2 * k], psi[2 * k + 1] };
    for (u32 k = 0; k < dim * n; k++) pd[k] = (ComplexDouble){ d[2 * k], d[2 * k + 1] };
    bool ok1 = diffgeo_compute_fubini_study(&eng, st, dim, pd, n, g);
    bool ok2 = diffgeo_compute_berry_curvature(&eng, st, dim, pd, n, f);
    CHECK(ok1 && ok2 && eng.stats.metric_evaluations == 1 && eng.stats.curvature_evaluations == 1);
}

static void test_upstream(void)
{
    double psi[2 * MAXD], d[2 * MAXD * MAXN], pq[2 * MAXD], dq[2 * MAXD * MAXN];
    double ug[MAXN * MAXN], uf[MAXN * MAXN];
    i16 qpsi[2 * MAXD], qd[2 * MAXD * MAXN];
    i32 g[MAXN * MAXN], f[MAXN * MAXN];
    double worst_same = 0.0, worst_true = 0.0, worst_ratio = 0.0;
    for (int it = 0; it < 3000; it++) {
        u32 dim = 2 + (u32)it % 7, n = 1 + rnd32() % MAXN;      /* dim 2..8 */
        rand_case(psi, d, dim, n, (it & 1) ? 1.9 : 0.6);
        quantise(psi, qpsi, 2 * dim);
        quantise(d, qd, 2 * dim * n);
        CHECK(tk_qgt(qpsi, qd, dim, n, g, f));

        /* (1) upstream on the same quantised numbers: only the output rounding differs */
        for (u32 k = 0; k < 2 * dim; k++) pq[k] = qpsi[k] / 16384.0;
        for (u32 k = 0; k < 2 * dim * n; k++) dq[k] = qd[k] / 16384.0;
        upstream_qgt(pq, dq, dim, n, ug, uf);
        for (u32 k = 0; k < n * n; k++) {
            double eg = fabs(g[k] - ug[k] * Q16), ef = fabs(f[k] - uf[k] * Q16);
            if (eg > worst_same) worst_same = eg;
            if (ef > worst_same) worst_same = ef;
            CHECK(eg <= 0.5 + 1e-6);
            CHECK(ef <= 0.5 + 1e-6);
        }

        /* (2) upstream on the unquantised numbers: within the stated input-rounding bound */
        upstream_qgt(psi, d, dim, n, ug, uf);
        for (u32 a = 0; a < n; a++) {
            for (u32 b = 0; b < n; b++) {
                double bd = bound_steps(psi, d, dim, a, b);
                double eg = fabs(g[a * n + b] - ug[a * n + b] * Q16);
                double ef = fabs(f[a * n + b] - uf[a * n + b] * Q16);
                if (eg > worst_true) worst_true = eg;
                if (ef / 2 > worst_true) worst_true = ef / 2;
                if (eg / bd > worst_ratio) worst_ratio = eg / bd;
                if (ef / (2 * bd) > worst_ratio) worst_ratio = ef / (2 * bd);
                CHECK(eg <= bd);
                CHECK(ef <= 2 * bd);
            }
        }
    }
    printf("C upstream oracle, dim 2..8: same inputs worst %.4f steps (limit 0.5); "
           "unquantised inputs worst %.2f steps, %.0f%% of the stated bound\n",
           worst_same, worst_true, 100.0 * worst_ratio);
}

/* ---- D: phase invariance ------------------------------------------------ */
static void times_i(i16 *v, u32 ncomplex)          /* exact: (re, im) -> (-im, re) */
{
    for (u32 k = 0; k < ncomplex; k++) {
        i16 re = v[2 * k], im = v[2 * k + 1];
        v[2 * k] = (i16)-im;
        v[2 * k + 1] = re;
    }
}

static void test_phase(void)
{
    double psi[2 * MAXD], d[2 * MAXD * MAXN], psi2[2 * MAXD], d2[2 * MAXD * MAXN];
    i16 p0[2 * MAXD], d0[2 * MAXD * MAXN], p1[2 * MAXD], d1[2 * MAXD * MAXN];
    i32 g0[MAXN * MAXN], f0[MAXN * MAXN], g1[MAXN * MAXN], f1[MAXN * MAXN];
    double worst = 0.0, worst_gauge = 0.0;
    int before = az_failures;
    for (int it = 0; it < 1500; it++) {
        u32 dim = 2 + (u32)it % 7, n = 1 + rnd32() % MAXN;
        rand_case(psi, d, dim, n, 1.2);
        quantise(psi, p0, 2 * dim);
        quantise(d, d0, 2 * dim * n);
        CHECK(tk_qgt(p0, d0, dim, n, g0, f0));
        u32 fid_pd = tk_fidelity_q16(p0, d0, dim), fid_pp = tk_fidelity_q16(p0, p0, dim);

        /* global phases i, -1, -i are exact on integers: results must be identical */
        memcpy(p1, p0, sizeof p0);
        memcpy(d1, d0, sizeof d0);
        for (int turn = 1; turn <= 3; turn++) {
            times_i(p1, dim);
            times_i(d1, dim * n);
            CHECK(tk_qgt(p1, d1, dim, n, g1, f1));
            CHECK_EQ(memcmp(g0, g1, n * n * sizeof(i32)), 0);
            CHECK_EQ(memcmp(f0, f1, n * n * sizeof(i32)), 0);
            CHECK_EQ(tk_fidelity_q16(p1, d1, dim), fid_pd);   /* both rotated */
            CHECK_EQ(tk_fidelity_q16(p1, p0, dim), fid_pp);   /* one rotated: same ray */
        }

        /* an arbitrary global phase, quantised again: equal within the two input bounds */
        double phi = rnd_unit() * 2 * PI, cr = cos(phi), ci = sin(phi);
        for (u32 k = 0; k < dim; k++) {
            psi2[2 * k] = psi[2 * k] * cr - psi[2 * k + 1] * ci;
            psi2[2 * k + 1] = psi[2 * k] * ci + psi[2 * k + 1] * cr;
        }
        for (u32 k = 0; k < dim * n; k++) {
            d2[2 * k] = d[2 * k] * cr - d[2 * k + 1] * ci;
            d2[2 * k + 1] = d[2 * k] * ci + d[2 * k + 1] * cr;
        }
        quantise(psi2, p1, 2 * dim);
        quantise(d2, d1, 2 * dim * n);
        CHECK(tk_qgt(p1, d1, dim, n, g1, f1));
        for (u32 a = 0; a < n; a++) {
            for (u32 b = 0; b < n; b++) {
                double bd = bound_steps(psi, d, dim, a, b) + bound_steps(psi2, d2, dim, a, b);
                double eg = fabs((double)g1[a * n + b] - g0[a * n + b]);
                double ef = fabs((double)f1[a * n + b] - f0[a * n + b]);
                if (eg > worst) worst = eg;
                if (ef / 2 > worst) worst = ef / 2;
                CHECK(eg <= bd);
                CHECK(ef <= 2 * bd);
            }
        }
    }
    /* a phase that depends on the parameters (a gauge change) leaves g and f alone */
    for (int it = 0; it < 2000; it++) {
        double w = check_bloch(rnd_unit() * PI, rnd_unit() * 2 * PI, rnd_sym() * 0.9, rnd_sym() * 0.9, 0.0);
        if (w > worst_gauge) worst_gauge = w;
    }
    printf("D phase: quarter turns %s; random global phase worst change %.2f steps; "
           "gauge-changed two-level state worst error %.2f steps\n",
           az_failures == before ? "identical" : "DIFFER", worst, worst_gauge);
}

/* ---- E: positive semidefinite, symmetric, antisymmetric ----------------- */
/* smallest eigenvalue of a symmetric matrix by cyclic Jacobi rotations */
static double min_eig(const double *m_in, u32 n)
{
    double m[MAXN * MAXN];
    memcpy(m, m_in, n * n * sizeof(double));
    for (int sweep = 0; sweep < 80; sweep++) {
        double off = 0.0;
        for (u32 p = 0; p < n; p++)
            for (u32 q = p + 1; q < n; q++) off += m[p * n + q] * m[p * n + q];
        if (off < 1e-24) break;
        for (u32 p = 0; p < n; p++) {
            for (u32 q = p + 1; q < n; q++) {
                double apq = m[p * n + q];
                if (fabs(apq) < 1e-300) continue;
                double th = (m[q * n + q] - m[p * n + p]) / (2 * apq);
                double t = (th >= 0 ? 1.0 : -1.0) / (fabs(th) + sqrt(th * th + 1));
                double c = 1 / sqrt(t * t + 1), s = t * c;
                for (u32 k = 0; k < n; k++) {
                    double kp = m[k * n + p], kq = m[k * n + q];
                    m[k * n + p] = c * kp - s * kq;
                    m[k * n + q] = s * kp + c * kq;
                }
                for (u32 k = 0; k < n; k++) {
                    double pk = m[p * n + k], qk = m[q * n + k];
                    m[p * n + k] = c * pk - s * qk;
                    m[q * n + k] = s * pk + c * qk;
                }
            }
        }
    }
    double lo = m[0];
    for (u32 k = 1; k < n; k++) if (m[k * n + k] < lo) lo = m[k * n + k];
    return lo;
}

static void test_psd(void)
{
    double psi[2 * MAXD], d[2 * MAXD * MAXN], gd[MAXN * MAXN];
    i16 qp[2 * MAXD], qd[2 * MAXD * MAXN];
    i32 g[MAXN * MAXN], f[MAXN * MAXN];
    double most_negative = 0.0, worst_null = 0.0;

    /* the eigenvalue routine itself: known spectra */
    double m2[4] = { 2, 1, 1, 2 }, m3[9] = { 2, -1, 0, -1, 2, -1, 0, -1, 2 };
    CHECK(fabs(min_eig(m2, 2) - 1.0) < 1e-12);
    CHECK(fabs(min_eig(m3, 3) - (2.0 - sqrt(2.0))) < 1e-12);

    for (int it = 0; it < 2000; it++) {
        u32 dim = 2 + (u32)it % 7, n = 1 + rnd32() % MAXN;
        rand_case(psi, d, dim, n, 1.0);
        quantise(psi, qp, 2 * dim);
        quantise(d, qd, 2 * dim * n);
        CHECK(tk_qgt(qp, qd, dim, n, g, f));
        int asym = 0;
        for (u32 a = 0; a < n; a++)
            for (u32 b = 0; b < n; b++)
                asym += (g[a * n + b] != g[b * n + a]) + (f[a * n + b] != -f[b * n + a]);
        CHECK_EQ(asym, 0);

        /* lambda_min(g) >= -(n/2 rounding steps) - (<psi|psi> - 1) * trace<d|d>, see tk_qgt.h */
        double nn = 0.0, tr = 0.0, frob = 0.0;
        for (u32 k = 0; k < 2 * dim; k++) nn += (double)qp[k] * qp[k];
        for (u32 k = 0; k < 2 * dim * n; k++) tr += (double)qd[k] * qd[k];
        nn /= 268435456.0; tr /= 268435456.0;
        double tol = 0.5 * n + (nn > 1.0 ? (nn - 1.0) * tr * Q16 : 0.0) + 1e-6;
        for (u32 k = 0; k < n * n; k++) gd[k] = g[k];
        double lam = min_eig(gd, n);
        if (lam < most_negative) most_negative = lam;
        CHECK(lam >= -tol);

        /* rank: the state space has 2*(dim-1) real directions, so with more
         * parameters than that the smallest eigenvalue must be zero. Without
         * the projection term it would be far from zero. */
        if (n > 2 * (dim - 1)) {
            for (u32 a = 0; a < n; a++)
                for (u32 b = 0; b < n; b++) {
                    double bd = bound_steps(psi, d, dim, a, b);
                    frob += bd * bd;
                }
            if (fabs(lam) > worst_null) worst_null = fabs(lam);
            CHECK(fabs(lam) <= sqrt(frob));
        }
    }

    /* moving along the ray (d_0 = z * psi) is no movement: row 0 of g and f vanishes */
    double worst_ray = 0.0;
    for (int it = 0; it < 500; it++) {
        u32 dim = 2 + (u32)it % 7, n = 2 + rnd32() % (MAXN - 1);
        rand_case(psi, d, dim, n, 1.0);
        double zr = rnd_sym() * 1.3, zi = rnd_sym() * 1.3;
        for (u32 k = 0; k < dim; k++) {
            d[2 * k] = zr * psi[2 * k] - zi * psi[2 * k + 1];
            d[2 * k + 1] = zr * psi[2 * k + 1] + zi * psi[2 * k];
        }
        quantise(psi, qp, 2 * dim);
        quantise(d, qd, 2 * dim * n);
        CHECK(tk_qgt(qp, qd, dim, n, g, f));
        for (u32 b = 0; b < n; b++) {
            double bd = bound_steps(psi, d, dim, 0, b);
            if (fabs((double)g[b]) > worst_ray) worst_ray = fabs((double)g[b]);
            CHECK(fabs((double)g[b]) <= bd);
            CHECK(fabs((double)f[b]) <= 2 * bd);
        }
    }
    printf("E metric: most negative eigenvalue %.3f steps; null eigenvalue when n > 2(dim-1) "
           "worst %.2f steps; along-the-ray row worst %.2f steps\n",
           most_negative, worst_null, worst_ray);
}

/* ---- F: real amplitudes give the classical Fisher metric ---------------- */
/* p_k = exp(sum_a theta_a T_ak) / Z, psi_k = sqrt(p_k): g = Fisher / 4, f = 0. */
static void test_fisher(void)
{
    double psi[2 * MAXD], d[2 * MAXD * MAXN], T[MAXN][MAXD], p[MAXD], mean[MAXN];
    i16 qp[2 * MAXD], qd[2 * MAXD * MAXN];
    i32 g[MAXN * MAXN], f[MAXN * MAXN];
    double worst = 0.0;
    for (int it = 0; it < 1000; it++) {
        u32 dim = 2 + (u32)it % 7, n = 1 + rnd32() % MAXN;
        double z = 0.0;
        for (u32 k = 0; k < dim; k++) p[k] = 0.0;
        for (u32 a = 0; a < n; a++) {
            double theta = rnd_sym();
            for (u32 k = 0; k < dim; k++) { T[a][k] = rnd_sym(); p[k] += theta * T[a][k]; }
        }
        for (u32 k = 0; k < dim; k++) { p[k] = exp(p[k]); z += p[k]; }
        for (u32 k = 0; k < dim; k++) p[k] /= z;
        for (u32 a = 0; a < n; a++) {
            mean[a] = 0.0;
            for (u32 k = 0; k < dim; k++) mean[a] += p[k] * T[a][k];
        }
        for (u32 k = 0; k < dim; k++) { psi[2 * k] = sqrt(p[k]); psi[2 * k + 1] = 0.0; }
        for (u32 a = 0; a < n; a++)
            for (u32 k = 0; k < dim; k++) {
                d[(a * dim + k) * 2] = 0.5 * sqrt(p[k]) * (T[a][k] - mean[a]);
                d[(a * dim + k) * 2 + 1] = 0.0;
            }
        quantise(psi, qp, 2 * dim);
        quantise(d, qd, 2 * dim * n);
        CHECK(tk_qgt(qp, qd, dim, n, g, f));
        int nonzero_f = 0;
        for (u32 k = 0; k < n * n; k++) nonzero_f += f[k] != 0;
        CHECK_EQ(nonzero_f, 0);
        for (u32 a = 0; a < n; a++) {
            for (u32 b = 0; b < n; b++) {
                double fisher = 0.0;
                for (u32 k = 0; k < dim; k++) fisher += p[k] * (T[a][k] - mean[a]) * (T[b][k] - mean[b]);
                double e = fabs(4.0 * g[a * n + b] - fisher * Q16);
                if (e > worst) worst = e;
                CHECK(e <= 4 * bound_steps(psi, d, dim, a, b));
            }
        }
    }
    printf("F real amplitudes: curvature exactly 0; |4g - Fisher| worst %.2f steps\n", worst);
}

/* ---- G: squared overlap ------------------------------------------------- */
static void test_fidelity(void)
{
    i16 e0[4] = { 16384, 0, 0, 0 }, e1[4] = { 0, 0, 16384, 0 };
    i16 plus[4] = { 11585, 0, 11585, 0 }, minus[4] = { 11585, 0, -11585, 0 };
    i16 ie0[4] = { 0, 16384, 0, 0 };
    CHECK_EQ(tk_fidelity_q16(e0, e0, 2), 65536);
    CHECK_EQ(tk_fidelity_q16(e0, e1, 2), 0);
    CHECK_EQ(tk_fidelity_q16(plus, minus, 2), 0);
    CHECK_EQ(tk_fidelity_q16(e0, ie0, 2), 65536);          /* same ray, phase i */
    CHECK(abs((int)tk_fidelity_q16(e0, plus, 2) - 32768) <= 2);   /* 1/2; 11585/16384 is not exactly 1/sqrt 2 */
    CHECK(abs((int)tk_fidelity_q16(plus, plus, 2) - 65536) <= 8);   /* 2*11585^2/2^28 = 0.99996 */

    /* random unit states against real arithmetic. First order in the input rounding
     * e = 2^-15: error <= 2*sqrt(2)*e*|<a|b>|*(L1(a) + L1(b)), i.e. 5.66*|<a|b>|*(L1a + L1b)
     * steps; 6 is used, plus 2 steps for the output rounding and second order. */
    double a[2 * MAXD], b[2 * MAXD], unused[2 * MAXD], worst = 0.0;
    i16 qa[2 * MAXD], qb[2 * MAXD];
    for (int it = 0; it < 3000; it++) {
        u32 dim = 1 + (u32)it % MAXD;
        rand_case(a, unused, dim, 1, 0.0);
        rand_case(b, unused, dim, 1, 0.0);
        quantise(a, qa, 2 * dim);
        quantise(b, qb, 2 * dim);
        double ov = overlap_abs(a, b, dim);
        double want = ov * ov * Q16;
        double bd = 6.0 * ov * (l1norm(a, dim) + l1norm(b, dim)) + 2.0;
        double e = fabs((double)tk_fidelity_q16(qa, qb, dim) - want);
        if (e > worst) worst = e;
        CHECK(e <= bd);
        CHECK(tk_fidelity_q16(qa, qb, dim) == tk_fidelity_q16(qb, qa, dim));
    }

    /* link to the metric: 1 - |<psi(t)|psi(t + dt)>|^2 = sin^2(dt/2), which is g_tt * dt^2
     * to second order. Checked against the exact value and against the measured g_tt. */
    double worst_link = 0.0;
    for (int it = 0; it < 200; it++) {
        double t = 0.2 + rnd_unit() * 2.5, p = rnd_unit() * 2 * PI, dt = 0.1 + 0.2 * rnd_unit();
        double v0[4], v1[4], dd[8];
        i16 q0[4], q1[4], qd[8];
        i32 g[4], f[4];
        bloch(t, p, 0.0, 0.0, v0, dd);
        quantise(v0, q0, 4);
        quantise(dd, qd, 8);
        bloch(t + dt, p, 0.0, 0.0, v1, dd);
        quantise(v1, q1, 4);
        CHECK(tk_qgt(q0, qd, 2, 2, g, f));
        double infid = 65536.0 - (double)tk_fidelity_q16(q0, q1, 2);
        double bd = 6.0 * (l1norm(v0, 2) + l1norm(v1, 2)) + 2.0;      /* overlap bound, as above */
        CHECK(fabs(infid - sin(dt / 2) * sin(dt / 2) * Q16) <= bd);
        double pred = (double)g[0] * dt * dt;                 /* g_tt dt^2, in steps */
        double rel = fabs(infid - pred) / pred;
        if (rel > worst_link) worst_link = rel;
        CHECK(fabs(infid - pred) <= pred * dt * dt / 10.0 + bd + 1.0);   /* next order: -dt^4/48 */
    }
    printf("G overlap: worst error vs real arithmetic %.2f steps; 1 - overlap vs g_tt*dt^2 worst rel. %.4f\n",
           worst, worst_link);
}

/* ---- H: preconditioner --------------------------------------------------- */
static void test_precondition(void)
{
    i32 step[8], raw[8] = { 1000, -1000, 123456, -7, 0, 2000000000, -2000000000, 65536 };
    /* identity metric (g == NULL, the lesion form): the step is the raw step */
    for (int k = 0; k < 8; k++) step[k] = 777;
    CHECK_EQ(tk_qgt_precondition(step, raw, NULL, 8, TK_QGT_ONE), 0);
    CHECK_EQ(memcmp(step, raw, sizeof raw), 0);
    /* entries at and above n are zeroed */
    for (int k = 0; k < 8; k++) step[k] = 777;
    CHECK_EQ(tk_qgt_precondition(step, raw, NULL, 3, 1), 0);
    for (int k = 0; k < 8; k++) CHECK_EQ(step[k], k < 3 ? raw[k] : 0);
    for (int k = 0; k < 8; k++) step[k] = 777;
    CHECK_EQ(tk_qgt_precondition(step, raw, NULL, 0, 1), 0);
    for (int k = 0; k < 8; k++) CHECK_EQ(step[k], 0);

    /* explicit diagonal: 1/4, 4, 0 (floored), negative (floored) */
    i32 g4[16] = { 0 };
    g4[0] = TK_QGT_ONE / 4; g4[5] = 4 * TK_QGT_ONE; g4[10] = 0; g4[15] = -500;
    i32 r4[8] = { 1000, 1000, 1000, 1000, 9, 9, 9, 9 };
    CHECK_EQ(tk_qgt_precondition(step, r4, g4, 4, TK_QGT_ONE / 16), 0);
    CHECK_EQ(step[0], 4000);       /* low metric: larger step                       */
    CHECK_EQ(step[1], 250);        /* high metric: the step shrinks                 */
    CHECK_EQ(step[2], 16000);      /* zero metric: the floor caps the gain at 16    */
    CHECK_EQ(step[3], 16000);      /* negative (rounding) metric: floored the same  */
    CHECK_EQ(step[4], 0);
    /* off-diagonal entries are not read */
    for (int k = 0; k < 16; k++) if (k % 5) g4[k] = 123456789;
    i32 step2[8];
    CHECK_EQ(tk_qgt_precondition(step2, r4, g4, 4, TK_QGT_ONE / 16), 0);
    CHECK_EQ(memcmp(step, step2, sizeof step), 0);

    /* floor below 1 is taken as 1; the overflow is clamped and counted */
    i32 big[8] = { 1000000, -1000000, 3, 0, 0, 0, 0, 0 };
    i32 gz[9] = { 0 };
    CHECK_EQ(tk_qgt_precondition(step, big, gz, 3, 0), 2);
    CHECK_EQ(step[0], INT32_MAX);
    CHECK_EQ(step[1], -INT32_MAX);
    CHECK_EQ(step[2], 3 * 65536);
    CHECK_EQ(tk_qgt_precondition(step2, big, gz, 3, -40), 2);
    CHECK_EQ(memcmp(step, step2, sizeof step), 0);
    i32 extreme[8] = { INT32_MIN, INT32_MAX, 0, 0, 0, 0, 0, 0 };
    CHECK_EQ(tk_qgt_precondition(step, extreme, NULL, 2, TK_QGT_ONE), 1);   /* -2^31 clamps to -INT32_MAX */
    CHECK_EQ(step[0], -INT32_MAX);
    CHECK_EQ(step[1], INT32_MAX);

    /* properties on random inputs */
    int grew = 0, not_odd = 0, not_nearest = 0, not_monotone = 0, inplace_diff = 0;
    for (int it = 0; it < 20000; it++) {
        u32 n = 1 + rnd32() % 8;
        i32 gm[64], gm2[64], rw[8], neg[8], s1[8], s2[8], s3[8];
        i32 fl = 1 + (i32)(rnd32() % 200000);
        for (u32 k = 0; k < 64; k++) gm[k] = (i32)(rnd32() % 400000) - 2000;
        for (u32 k = 0; k < 8; k++) { rw[k] = (i32)(rnd32() % 2000001) - 1000000; neg[k] = -rw[k]; }
        u32 sat = tk_qgt_precondition(s1, rw, gm, n, fl);
        tk_qgt_precondition(s2, neg, gm, n, fl);
        memcpy(gm2, gm, sizeof gm);
        u32 bump = rnd32() % n;
        gm2[bump * n + bump] += 1 + (i32)(rnd32() % 100000);          /* raise one diagonal entry */
        tk_qgt_precondition(s3, rw, gm2, n, fl);
        for (u32 a = 0; a < n; a++) {
            i64 dd = gm[a * n + a] > fl ? gm[a * n + a] : fl;
            i64 err = (i64)s1[a] * dd - (i64)rw[a] * 65536;
            if (err < 0) err = -err;
            if (!sat && 2 * err > dd) not_nearest++;                 /* nearest multiple   */
            if (s2[a] != -s1[a]) not_odd++;                          /* odd in raw         */
            if (dd >= 65536 && abs(s1[a]) > abs(rw[a])) grew++;      /* metric >= 1 never amplifies */
            if (abs(s3[a]) > abs(s1[a])) not_monotone++;             /* more metric, never a larger step */
            if (a != bump && s3[a] != s1[a]) not_monotone++;         /* other directions untouched */
        }
        memcpy(s2, rw, sizeof rw);
        tk_qgt_precondition(s2, s2, gm, n, fl);                       /* step and raw the same array */
        inplace_diff += memcmp(s1, s2, sizeof s1) != 0;
    }
    CHECK_EQ(not_nearest, 0);
    CHECK_EQ(not_odd, 0);
    CHECK_EQ(grew, 0);
    CHECK_EQ(not_monotone, 0);
    CHECK_EQ(inplace_diff, 0);

    /* end to end: the metric of a real state decides the step. Two-level state near the
     * pole (t = 0.3): g_tt = 1/4, g_pp = sin^2(0.3)/4 = 0.0218. The same raw step in both
     * parameters comes out about 11.4 times larger along p; near the equator the metric is
     * isotropic and so is the step. A parameter whose derivative is stored 1.8 times
     * larger has 3.24 times the metric and gets a 3.24 times smaller step. */
    double v[4], d[8];
    i16 qp[4], qd[8];
    i32 g[4], f[4], same[8] = { 100000, 100000, 0, 0, 0, 0, 0, 0 };
    bloch(0.3, 1.1, 0.0, 0.0, v, d);
    quantise(v, qp, 4); quantise(d, qd, 8);
    CHECK(tk_qgt(qp, qd, 2, 2, g, f));
    CHECK_EQ(tk_qgt_precondition(step, same, g, 2, 64), 0);
    CHECK(abs(step[0] - 400000) <= 400);
    CHECK(step[1] > 11 * step[0] && step[1] < 12 * step[0]);
    double ratio_pole = (double)step[1] / step[0];
    CHECK_EQ(tk_qgt_precondition(step, same, g, 2, TK_QGT_ONE / 8), 0);    /* floor 1/8 caps p */
    CHECK_EQ(step[1], 800000);
    bloch(PI / 2, 1.1, 0.0, 0.0, v, d);
    quantise(v, qp, 4); quantise(d, qd, 8);
    CHECK(tk_qgt(qp, qd, 2, 2, g, f));
    CHECK_EQ(tk_qgt_precondition(step, same, g, 2, 64), 0);
    CHECK(abs(step[1] - step[0]) <= 1200);                   /* 48 steps of g out of 16384 */
    for (int k = 0; k < 4; k++) d[k] *= 1.8;                /* parameter t in units 1.8 times coarser */
    quantise(d, qd, 8);
    CHECK(tk_qgt(qp, qd, 2, 2, g, f));
    CHECK_EQ(tk_qgt_precondition(step, same, g, 2, 64), 0);
    CHECK(step[0] < step[1]);
    CHECK(fabs((double)step[1] / step[0] - 3.24) < 0.01);
    printf("H precondition: pole step ratio p/t = %.2f (want 11.45); scaled-parameter ratio %.3f (want 3.24)\n",
           ratio_pole, (double)step[1] / step[0]);
}

/* ---- I: argument handling ------------------------------------------------ */
static void test_arguments(void)
{
    i16 psi[2 * MAXD + 2] = { 16384 }, dpsi[2 * (MAXD + 1) * (MAXN + 1)] = { 0, 16384 };
    i32 g[(MAXN + 1) * (MAXN + 1)], f[(MAXN + 1) * (MAXN + 1)];
    static const u32 bad[4][2] = { { 0, 1 }, { 9, 1 }, { 2, 0 }, { 2, 9 } };
    for (int c = 0; c < 4; c++) {
        for (u32 k = 0; k < AZ_LEN(g); k++) { g[k] = 0x5A5A5A5A; f[k] = 0x5A5A5A5A; }
        CHECK(!tk_qgt(psi, dpsi, bad[c][0], bad[c][1], g, f));
        int touched = 0;
        for (u32 k = 0; k < AZ_LEN(g); k++) touched += (g[k] != 0x5A5A5A5A) + (f[k] != 0x5A5A5A5A);
        CHECK_EQ(touched, 0);
    }
    CHECK_EQ(tk_fidelity_q16(psi, psi, 0), 0);
    CHECK_EQ(tk_fidelity_q16(psi, psi, 9), 0);
    /* exactly n*n entries are written, no more */
    for (u32 k = 0; k < AZ_LEN(g); k++) { g[k] = 0x5A5A5A5A; f[k] = 0x5A5A5A5A; }
    CHECK(tk_qgt(psi, dpsi, 3, 5, g, f));
    for (u32 k = 25; k < AZ_LEN(g); k++) { CHECK_EQ(g[k], 0x5A5A5A5A); CHECK_EQ(f[k], 0x5A5A5A5A); }
    /* dim 1: the only direction is the ray itself, so the tensor is zero */
    i16 one[2] = { 9830, -13107 }, done[4] = { 5000, 7000, -3000, 12000 };   /* |one| = 1 */
    CHECK(tk_qgt(one, done, 1, 2, g, f));
    for (int k = 0; k < 4; k++) { CHECK(abs(g[k]) <= 3); CHECK(abs(f[k]) <= 3); }
}

/* ---- timing: information only, no check ---------------------------------- */
#include <time.h>
static void report_timing(void)
{
    double psi[2 * MAXD], d[2 * MAXD * MAXN];
    i16 qp[2 * MAXD], qd[2 * MAXD * MAXN];
    i32 g[MAXN * MAXN], f[MAXN * MAXN], step[8], raw[8] = { 5, -6, 7, -8, 9, -10, 11, -12 };
    static const u32 shapes[3][2] = { { 2, 2 }, { 4, 4 }, { 8, 8 } };
    printf("timing (noisy, parallel load):");
    for (int s = 0; s < 3; s++) {
        u32 dim = shapes[s][0], n = shapes[s][1];
        const int reps = 400000;
        i64 sink = 0;
        rand_case(psi, d, dim, n, 1.0);
        quantise(psi, qp, 2 * dim);
        quantise(d, qd, 2 * dim * n);
        clock_t t0 = clock();
        for (int r = 0; r < reps; r++) {
            qd[0] = (i16)(r & 1023);                      /* keep the call from being hoisted */
            tk_qgt(qp, qd, dim, n, g, f);
            tk_qgt_precondition(step, raw, g, n, 64);
            sink += g[0] + f[1] + step[0];
        }
        double ns = (double)(clock() - t0) / CLOCKS_PER_SEC * 1e9 / reps;
        printf(" dim %u n %u: %.0f ns (tk_qgt + precondition)%s", dim, n, ns, sink == -1 ? "!" : "");
    }
    printf("\n");
}

int main(void)
{
    report_timing();
    test_closed_forms();
    test_exact();
    test_upstream();
    test_phase();
    test_psd();
    test_fisher();
    test_fidelity();
    test_precondition();
    test_arguments();
    return az_test_done();
}
