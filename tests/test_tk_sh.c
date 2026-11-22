/* test_tk_sh.c - integer real spherical harmonics (core/tk_sh.c) against
 * libirrep's binary64 implementation and against the mathematics.
 *
 * Oracle: tests/upstream/libirrep (MIT, see LICENSE and PROVENANCE.txt there),
 * compiled into this executable only. Two independent libirrep code paths
 * are used: the closed polynomials (irrep_solid_harm_cart_all) and the
 * Legendre recurrence (irrep_sph_harm_cart_all).
 *
 * Hosted C: floating point is allowed here, never in core/.
 */
#include "az_test.h"
#include "../core/tk_sh.h"
#include <math.h>

/* ---- the oracle (unity build of the vendored upstream files) ---- */
#define IRREP_EXPORT_H            /* static use: skip the dllimport macro of export.h */
#define IRREP_API
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#pragma GCC diagnostic ignored "-Wunused-function"
#include "upstream/libirrep/src/solid_harmonics.c"
#include "upstream/libirrep/src/spherical_harmonics.c"
/* test shim (not from libirrep): scalar dispatch table so that
 * spherical_harmonics.c links without libirrep's SIMD runtime */
static const irrep_dispatch_t sh_dispatch = { .sph_harm_cart_all_batch = irrep_sph_harm_cart_all_batch_scalar };
const irrep_dispatch_t *irrep_dispatch_get(void) { return &sh_dispatch; }
#pragma GCC diagnostic pop

#define LSB (1.0 / 16384.0)
#define PI_D 3.14159265358979323846

/* ---- deterministic test randomness (splitmix64 counter; not the sim RNG) ---- */
static u64 rng_ctr = 0x243F6A8885A308D3ull;
static u64 rnd64(void) { return az_test_rand64(&rng_ctr); }
static double rnd01(void) { return (double)(rnd64() >> 11) * (1.0 / 9007199254740992.0); }
static double rnd_sym(void) { return 2.0 * rnd01() - 1.0; }
static void rnd_unit(double d[3])
{
    for (;;) {
        d[0] = rnd_sym(); d[1] = rnd_sym(); d[2] = rnd_sym();
        double n2 = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
        if (n2 > 1e-4 && n2 <= 1.0) { double n = sqrt(n2); d[0] /= n; d[1] /= n; d[2] /= n; return; }
    }
}
/* random unit quaternion -> rotation matrix (uniform over SO(3)) */
static void rnd_rot(double R[3][3])
{
    double q[4], n2;
    do {
        n2 = 0;
        for (int i = 0; i < 4; i++) { q[i] = rnd_sym(); n2 += q[i] * q[i]; }
    } while (n2 > 1.0 || n2 < 1e-4);
    double n = sqrt(n2), w = q[0] / n, x = q[1] / n, y = q[2] / n, z = q[3] / n;
    R[0][0] = 1 - 2 * (y * y + z * z); R[0][1] = 2 * (x * y - w * z);     R[0][2] = 2 * (x * z + w * y);
    R[1][0] = 2 * (x * y + w * z);     R[1][1] = 1 - 2 * (x * x + z * z); R[1][2] = 2 * (y * z - w * x);
    R[2][0] = 2 * (x * z - w * y);     R[2][1] = 2 * (y * z + w * x);     R[2][2] = 1 - 2 * (x * x + y * y);
}
static void rot_i32(const double R[3][3], const i32 v[3], i32 out[3])
{
    for (int i = 0; i < 3; i++)
        out[i] = (i32)llround(R[i][0] * v[0] + R[i][1] * v[1] + R[i][2] * v[2]);
}

/* libirrep reference for the direction of an integer vector, both code paths */
static void ref_dir(const i32 v[3], double poly[16], double rec[16])
{
    double d[3] = { (double)v[0], (double)v[1], (double)v[2] };
    double n = sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    d[0] /= n; d[1] /= n; d[2] /= n;
    irrep_solid_harm_cart_all(3, poly, d);
    irrep_sph_harm_cart_all(3, rec, d);
}

/* ------------------------------------------------------------------------ */
static void test_constants(void)
{
    const struct { long long q30; double sq; } k[] = {
        { TK_SH_K0_Q30, 1.0 / (4 * PI_D) },   { TK_SH_K1_Q30, 3.0 / (4 * PI_D) },
        { TK_SH_K2A_Q30, 15.0 / (4 * PI_D) }, { TK_SH_K20_Q30, 5.0 / (16 * PI_D) },
        { TK_SH_K22_Q30, 15.0 / (16 * PI_D) }, { TK_SH_K33_Q30, 35.0 / (32 * PI_D) },
        { TK_SH_K3A_Q30, 105.0 / (4 * PI_D) }, { TK_SH_K31_Q30, 21.0 / (32 * PI_D) },
        { TK_SH_K30_Q30, 7.0 / (16 * PI_D) },  { TK_SH_K32_Q30, 105.0 / (16 * PI_D) },
    };
    for (u32 i = 0; i < AZ_LEN(k); i++) CHECK_EQ(k[i].q30, llround(sqrt(k[i].sq) * 1073741824.0));
    CHECK_EQ(TK_SH_Y00_Q14, llround(sqrt(1.0 / (4 * PI_D)) * 16384.0));
    CHECK_EQ(TK_SH_PC0_Q24, llround(18014398509481984.0 / (4.0 * TK_SH_Y00_Q14 * TK_SH_Y00_Q14)));
    CHECK_EQ(TK_SH_PC1_Q24, llround(4 * PI_D / 3 * 16777216.0));
    CHECK_EQ(TK_SH_PC2_Q24, llround(4 * PI_D / 5 * 16777216.0));
    CHECK_EQ(TK_SH_PC3_Q24, llround(4 * PI_D / 7 * 16777216.0));
    CHECK_EQ(TK_SH_IDX(0, 0), 0); CHECK_EQ(TK_SH_IDX(1, -1), 1); CHECK_EQ(TK_SH_IDX(2, 0), 6);
    CHECK_EQ(TK_SH_IDX(3, 3), 15);
}

/* 10,000 random integer vectors of every magnitude against libirrep */
static void test_accuracy(void)
{
    double max_poly = 0, max_rec = 0, max_paths = 0;
    u32 worst_abs = 0;
    for (u32 it = 0; it < 10000; it++) {
        i32 v[3];
        u32 bits = 1 + (u32)(rnd64() % 31);                  /* component range 2^1 .. 2^31 */
        do {
            for (int i = 0; i < 3; i++) {
                i64 span = (i64)1 << bits;
                v[i] = (i32)((i64)(rnd64() % (u64)(2 * span)) - span);   /* -2^bits .. 2^bits - 1 */
            }
        } while (v[0] == 0 && v[1] == 0 && v[2] == 0);
        i16 y[16];
        double poly[16], rec[16];
        tk_sh_eval(v, y);
        ref_dir(v, poly, rec);
        for (int i = 0; i < 16; i++) {
            double e1 = fabs(y[i] * LSB - poly[i]), e2 = fabs(y[i] * LSB - rec[i]);
            if (e1 > max_poly) max_poly = e1;
            if (e2 > max_rec) max_rec = e2;
            if (fabs(poly[i] - rec[i]) > max_paths) max_paths = fabs(poly[i] - rec[i]);
            u32 a = (u32)abs(y[i]);
            if (a > worst_abs) worst_abs = a;
        }
    }
    printf("accuracy  10000 random integer vectors (component ranges 2^1..2^31), 16 values each:\n"
           "          max |tk_sh - libirrep closed polynomial| = %.3e = %.4f LSB of Q1.14\n"
           "          max |tk_sh - libirrep Legendre recurrence| = %.3e = %.4f LSB\n"
           "          (the two libirrep paths differ by %.1e); largest |output| = %u\n",
           max_poly, max_poly / LSB, max_rec, max_rec / LSB, max_paths, worst_abs);
    CHECK(max_poly <= 0.5005 * LSB);
    CHECK(max_rec <= 0.5005 * LSB);
    CHECK(max_paths < 1e-12);
    CHECK(worst_abs <= 12228);

    /* directions that are NOT representable: a real unit direction rounded to
     * an integer offset of length one cell edge (AZ_SUB_ONE), compared with
     * libirrep on the unrounded direction */
    double max_q = 0;
    for (u32 it = 0; it < 10000; it++) {
        double d[3], ref[16];
        rnd_unit(d);
        i32 v[3] = { (i32)llround(d[0] * AZ_SUB_ONE), (i32)llround(d[1] * AZ_SUB_ONE), (i32)llround(d[2] * AZ_SUB_ONE) };
        i16 y[16];
        tk_sh_eval(v, y);
        irrep_sph_harm_cart_all(3, ref, d);
        for (int i = 0; i < 16; i++)
            if (fabs(y[i] * LSB - ref[i]) > max_q) max_q = fabs(y[i] * LSB - ref[i]);
    }
    printf("          10000 real directions rounded to offsets of length 2^14: max error = %.3e = %.3f LSB\n",
           max_q, max_q / LSB);
    CHECK(max_q <= 2.5 * LSB);
}

/* exact properties of the integer outputs */
static void test_exact(void)
{
    i16 y[16], yn[16], ys[16];
    const i32 zero[3] = { 0, 0, 0 };
    tk_sh_eval(zero, y);
    CHECK_EQ(y[0], TK_SH_Y00_Q14);
    for (int i = 1; i < 16; i++) CHECK_EQ(y[i], 0);

    /* the axes: closed-form values */
    const i32 ez[3] = { 0, 0, 7 }, ex[3] = { 1, 0, 0 }, ey[3] = { 0, -123456789, 0 };
    tk_sh_eval(ez, y);
    CHECK_EQ(y[TK_SH_IDX(1, 0)], llround(sqrt(3 / (4 * PI_D)) * 16384));
    CHECK_EQ(y[TK_SH_IDX(2, 0)], llround(sqrt(5 / (4 * PI_D)) * 16384));
    CHECK_EQ(y[TK_SH_IDX(3, 0)], llround(sqrt(7 / (4 * PI_D)) * 16384));
    CHECK_EQ(y[TK_SH_IDX(1, 1)], 0); CHECK_EQ(y[TK_SH_IDX(1, -1)], 0); CHECK_EQ(y[TK_SH_IDX(2, 2)], 0);
    tk_sh_eval(ex, y);
    CHECK_EQ(y[TK_SH_IDX(1, 1)], llround(sqrt(3 / (4 * PI_D)) * 16384));      /* +x: no Condon-Shortley sign */
    CHECK_EQ(y[TK_SH_IDX(2, 2)], llround(sqrt(15 / (16 * PI_D)) * 16384));
    CHECK_EQ(y[TK_SH_IDX(3, 3)], llround(sqrt(35 / (32 * PI_D)) * 16384));
    CHECK_EQ(y[TK_SH_IDX(2, 0)], -llround(sqrt(5 / (16 * PI_D)) * 16384));
    tk_sh_eval(ey, y);
    CHECK_EQ(y[TK_SH_IDX(1, -1)], -llround(sqrt(3 / (4 * PI_D)) * 16384));    /* -y */
    CHECK_EQ(y[TK_SH_IDX(2, 2)], -llround(sqrt(15 / (16 * PI_D)) * 16384));
    CHECK_EQ(y[TK_SH_IDX(3, -3)], llround(sqrt(35 / (32 * PI_D)) * 16384));   /* y(3xx-yy) at -y: +1 */

    u32 bad_parity = 0, bad_scale = 0, bad_mirror = 0;
    for (u32 it = 0; it < 20000; it++) {
        i32 v[3], n[3], s[3], mz[3];
        u32 bits = 1 + (u32)(rnd64() % 24);
        for (int i = 0; i < 3; i++) {
            v[i] = (i32)(rnd64() % (2u << bits)) - (i32)(1u << bits);
            n[i] = -v[i];
            s[i] = v[i] * 64;                    /* same direction, other length */
        }
        mz[0] = v[0]; mz[1] = v[1]; mz[2] = -v[2];
        tk_sh_eval(v, y); tk_sh_eval(n, yn); tk_sh_eval(s, ys);
        for (int l = 0; l <= 3; l++)
            for (int m = -l; m <= l; m++) {
                int i = TK_SH_IDX(l, m);
                if (yn[i] != ((l & 1) ? -y[i] : y[i])) bad_parity++;
                if (ys[i] != y[i]) bad_scale++;
            }
        tk_sh_eval(mz, yn);                      /* z -> -z: factor (-1)^(l+m) */
        for (int l = 0; l <= 3; l++)
            for (int m = -l; m <= l; m++) {
                int i = TK_SH_IDX(l, m);
                if (yn[i] != (((l + m) & 1) ? -y[i] : y[i])) bad_mirror++;
            }
    }
    CHECK_EQ(bad_parity, 0);
    CHECK_EQ(bad_scale, 0);
    CHECK_EQ(bad_mirror, 0);

    /* extreme components: no overflow, still accurate */
    const i32 ext[6][3] = {
        { INT32_MIN, INT32_MIN, INT32_MIN }, { INT32_MAX, INT32_MAX, INT32_MAX }, { INT32_MIN, INT32_MAX, 1 },
        { INT32_MIN, 0, 0 }, { 1, 1, 1 }, { 1073741824, -1073741823, 536870912 },
    };
    double max_e = 0;
    for (int k = 0; k < 6; k++) {
        double poly[16], rec[16];
        tk_sh_eval(ext[k], y);
        ref_dir(ext[k], poly, rec);
        for (int i = 0; i < 16; i++)
            if (fabs(y[i] * LSB - rec[i]) > max_e) max_e = fabs(y[i] * LSB - rec[i]);
    }
    printf("exact     parity, power-of-two scaling and z-mirror hold bit-exactly on 20000 vectors;\n"
           "          extreme i32 vectors: max error %.4f LSB\n", max_e / LSB);
    CHECK(max_e <= 0.5005 * LSB);
}

/* Gauss-Legendre nodes and weights on [-1, 1] by Newton iteration */
static void gauss_legendre(int n, double *x, double *w)
{
    for (int i = 0; i < n; i++) {
        double t = cos(PI_D * (i + 0.75) / (n + 0.5)), dp = 1;
        for (int it = 0; it < 50; it++) {
            double p0 = 1, p1 = t;
            for (int k = 2; k <= n; k++) { double p2 = ((2 * k - 1) * t * p1 - (k - 1) * p0) / k; p0 = p1; p1 = p2; }
            dp = n * (t * p1 - p0) / (t * t - 1);
            double dt = p1 / dp;
            t -= dt;
            if (fabs(dt) < 1e-15) break;
        }
        x[i] = t;
        w[i] = 2 / ((1 - t * t) * dp * dp);
    }
}

/* max |G - I| of the 16 x 16 Gram matrix of the INTEGER harmonics under a
 * product rule: n_t Gauss-Legendre nodes in cos(theta) times n_p equally
 * spaced phi (exact for polynomials of degree 2 n_t - 1 and n_p - 1) */
static double gram_error(int n_t, int n_p, double *wsum_out)
{
    double x[32], w[32], G[16][16] = { { 0 } }, wsum = 0;
    gauss_legendre(n_t, x, w);
    for (int a = 0; a < n_t; a++)
        for (int b = 0; b < n_p; b++) {
            double st = sqrt(1 - x[a] * x[a]), phi = 2 * PI_D * (b + 0.37) / n_p;
            const double scale = 16777216.0;      /* direction error of the integer vector < 5e-8 */
            i32 v[3] = { (i32)llround(st * cos(phi) * scale), (i32)llround(st * sin(phi) * scale), (i32)llround(x[a] * scale) };
            i16 y[16];
            double wk = w[a] / 2 / n_p;           /* weights sum to 1 */
            tk_sh_eval(v, y);
            wsum += wk;
            for (int i = 0; i < 16; i++)
                for (int j = 0; j < 16; j++) G[i][j] += 4 * PI_D * wk * (y[i] * LSB) * (y[j] * LSB);
        }
    double e = 0;
    for (int i = 0; i < 16; i++)
        for (int j = 0; j < 16; j++)
            if (fabs(G[i][j] - (i == j)) > e) e = fabs(G[i][j] - (i == j));
    if (wsum_out) *wsum_out = wsum;
    return e;
}

static void test_orthonormal(void)
{
    double ws = 0;
    double e_a = gram_error(8, 16, &ws);     /* 128 points, exact to degree 15 */
    double e_b = gram_error(16, 32, NULL);   /* 512 points */
    double e_c = gram_error(4, 7, NULL);     /* 28 points: the smallest rule exact to degree 6 */
    double e_bad = gram_error(2, 3, NULL);   /* 6 points: exact to degree 2 only, must fail */
    printf("orthonorm 16 x 16 Gram matrix of the integer harmonics, max |G - I|:\n"
           "          8 x 16 = 128 points %.3e, 16 x 32 = 512 points %.3e, 4 x 7 = 28 points %.3e\n"
           "          (weight sum %.15f); negative control 2 x 3 = 6 points: %.3f (must be large)\n",
           e_a, e_b, e_c, ws, e_bad);
    /* Tolerance = the analytic worst case of rounding every value by 0.5 LSB:
     * |dG_ij| <= 4 pi * 0.5 LSB * (mean|Y_i| + mean|Y_j|) <= 2 sqrt(4 pi) * 0.5 LSB = 2.164e-4.
     * (G_00 alone is off by 8.3e-5 because Y_0,0 = 4621.84 rounds to 4622.) */
    const double tol = 2.0 * sqrt(4 * PI_D) * 0.5 * LSB + 1e-6;
    CHECK(fabs(ws - 1) < 1e-13);
    CHECK(e_a < tol);
    CHECK(e_b < tol);
    CHECK(e_c < tol);
    CHECK(e_bad > 0.2);
}

/* addition theorem: one direction has power TK_SH_POWER_ONE in every degree */
static void test_single_direction(void)
{
    i32 lo[4] = { 1 << 30, 1 << 30, 1 << 30, 1 << 30 }, hi[4] = { 0, 0, 0, 0 };
    for (u32 it = 0; it < 10000; it++) {
        i32 v[3];
        do { for (int i = 0; i < 3; i++) v[i] = (i32)(rnd64() % 131073u) - 65536; } while (!(v[0] | v[1] | v[2]));
        i64 acc[16] = { 0 };
        u32 p[4];
        u32 wgt = 1 + (u32)(rnd64() % 255);
        tk_sh_accumulate(acc, v, (i32)wgt);
        tk_sh_power(acc, wgt, p);
        for (int l = 0; l < 4; l++) { if ((i32)p[l] < lo[l]) lo[l] = (i32)p[l]; if ((i32)p[l] > hi[l]) hi[l] = (i32)p[l]; }
    }
    printf("addition  one weighted direction, 10000 cases, power per degree (ideal 65536):\n"
           "          l=0 %d..%d, l=1 %d..%d, l=2 %d..%d, l=3 %d..%d\n",
           lo[0], hi[0], lo[1], hi[1], lo[2], hi[2], lo[3], hi[3]);
    /* Tolerance 16: the worst case of 0.5 LSB per value is a relative error of
     * 2 sqrt(4 pi) * 0.5 LSB = 2.164e-4 = 14.2 units, plus the two roundings in tk_sh_power. */
    CHECK_EQ(lo[0], 65536); CHECK_EQ(hi[0], 65536);
    for (int l = 1; l < 4; l++) { CHECK(lo[l] >= 65536 - 16); CHECK(hi[l] <= 65536 + 16); }
}

/* rotation invariance of the power spectrum */
static void test_rotation(void)
{
    i32 max_d[4] = { 0, 0, 0, 0 };
    double max_single = 0;
    u32 trials = 2000, n_total = 0;
    for (u32 t = 0; t < trials; t++) {
        u32 n = 1 + (u32)(rnd64() % 64);
        bool unit_w = (t & 1) != 0;
        double R[3][3];
        rnd_rot(R);
        i64 acc_a[16] = { 0 }, acc_b[16] = { 0 };
        u32 wsum = 0, pa[4], pb[4];
        for (u32 k = 0; k < n; k++) {
            i32 v[3], r[3];
            /* offsets in neighbour space: up to 4 cell edges, at least 1/16 of one */
            do {
                for (int i = 0; i < 3; i++) v[i] = (i32)(rnd64() % (8u * AZ_SUB_ONE + 1u)) - 4 * AZ_SUB_ONE;
            } while ((i64)v[0] * v[0] + (i64)v[1] * v[1] + (i64)v[2] * v[2] < (i64)(AZ_SUB_ONE / 16) * (AZ_SUB_ONE / 16));
            rot_i32(R, v, r);
            u32 wgt = unit_w ? 1u : 1u + (u32)(rnd64() % 255);
            tk_sh_accumulate(acc_a, v, (i32)wgt);
            tk_sh_accumulate(acc_b, r, (i32)wgt);
            wsum += wgt;
        }
        n_total += n;
        tk_sh_power(acc_a, wsum, pa);
        tk_sh_power(acc_b, wsum, pb);
        for (int l = 0; l < 4; l++) {
            i32 d = abs((i32)pa[l] - (i32)pb[l]);
            if (d > max_d[l]) max_d[l] = d;
        }
        /* negative control: one squared coefficient is not an invariant */
        double sa = (double)acc_a[TK_SH_IDX(2, 0)] / wsum * LSB, sb = (double)acc_b[TK_SH_IDX(2, 0)] / wsum * LSB;
        double ds = fabs(sa * sa - sb * sb) * 4 * PI_D / 5 * 65536;
        if (ds > max_single) max_single = ds;
    }
    printf("rotation  %u random sets (1..64 integer offsets, mean %.1f; unit and 1..255 weights), each rotated\n"
           "          by a random rotation and rounded back to integers. max |power - power(rotated)| in units\n"
           "          of 1/65536: l=0 %d, l=1 %d, l=2 %d, l=3 %d  (%.1e of full scale at worst)\n"
           "          negative control, the single term a_2,0^2 on the same scale: %.0f\n",
           trials, (double)n_total / trials, max_d[0], max_d[1], max_d[2], max_d[3],
           (double)az_max_i32(az_max_i32(max_d[1], max_d[2]), max_d[3]) / 65536.0, max_single);
    /* Stated tolerance: 24 / 65536 = 3.7e-4 of full scale for l >= 1, exact for l = 0.
     * Two error sources: the 0.5 LSB rounding of each harmonic (up to 14 units per
     * spectrum) and the rounding of the rotated offsets back to integers (a
     * direction change of up to 0.87 / length; lengths here are 1024 .. 113512). */
    CHECK_EQ(max_d[0], 0);
    for (int l = 1; l < 4; l++) CHECK(max_d[l] <= 24);
    CHECK(max_single > 20000);
}

/* a line, a plane and a ball of 64 points around the observer */
enum { SHAPE_LINE = 0, SHAPE_PLANE, SHAPE_BALL, SHAPE_RAY, SHAPE_COUNT };
static void shape_power(int shape, u32 p[4])
{
    double R[3][3];
    rnd_rot(R);
    i64 acc[16] = { 0 };
    i32 d[3];
    do { for (int i = 0; i < 3; i++) d[i] = (i32)(rnd64() % 201u) - 100; } while (!(d[0] | d[1] | d[2]));
    for (u32 k = 0; k < 64; k++) {
        i32 v[3] = { 0, 0, 0 }, r[3];
        if (shape == SHAPE_LINE || shape == SHAPE_RAY) {
            /* exactly collinear integer points on both sides (line) or one side (ray) */
            i32 s = 1 + (i32)(rnd64() % 1000);
            if (shape == SHAPE_LINE && (rnd64() & 1)) s = -s;
            for (int i = 0; i < 3; i++) r[i] = s * d[i];
        } else {
            double q[3];
            do {
                q[0] = rnd_sym(); q[1] = rnd_sym(); q[2] = shape == SHAPE_PLANE ? 0.0 : rnd_sym();
            } while (q[0] * q[0] + q[1] * q[1] + q[2] * q[2] > 1.0 || q[0] * q[0] + q[1] * q[1] + q[2] * q[2] < 1e-3);
            for (int i = 0; i < 3; i++) v[i] = (i32)llround(q[i] * 2 * AZ_SUB_ONE);
            rot_i32(R, v, r);
        }
        tk_sh_accumulate(acc, r, 1);
    }
    tk_sh_power(acc, 64, p);
}

static void test_shapes(void)
{
    static const char *name[SHAPE_COUNT] = { "line ", "plane", "ball ", "ray  " };
    u32 lo[SHAPE_COUNT][4], hi[SHAPE_COUNT][4];
    for (int s = 0; s < SHAPE_COUNT; s++)
        for (int l = 0; l < 4; l++) { lo[s][l] = 0xFFFFFFFFu; hi[s][l] = 0; }
    for (u32 t = 0; t < 500; t++)
        for (int s = 0; s < SHAPE_COUNT; s++) {
            u32 p[4];
            shape_power(s, p);
            for (int l = 0; l < 4; l++) { lo[s][l] = az_min_u32(lo[s][l], p[l]); hi[s][l] = az_max_u32(hi[s][l], p[l]); }
        }
    printf("shapes    64 points, 500 random orientations each; power per degree as min..max (65536 = 1):\n");
    for (int s = 0; s < SHAPE_COUNT; s++)
        printf("          %s l=0 %u..%u  l=1 %u..%u  l=2 %u..%u  l=3 %u..%u\n", name[s],
               lo[s][0], hi[s][0], lo[s][1], hi[s][1], lo[s][2], hi[s][2], lo[s][3], hi[s][3]);
    for (int s = 0; s < SHAPE_COUNT; s++) { CHECK_EQ(lo[s][0], 65536); CHECK_EQ(hi[s][0], 65536); }
    /* degree 2 separates the three shapes with wide gaps */
    CHECK(lo[SHAPE_LINE][2] >= 65000);
    CHECK(hi[SHAPE_PLANE][2] <= 30000);
    CHECK(lo[SHAPE_PLANE][2] >= 11000);
    CHECK(hi[SHAPE_BALL][2] <= 7000);
    CHECK(lo[SHAPE_LINE][2] > 2 * hi[SHAPE_PLANE][2]);
    CHECK(lo[SHAPE_PLANE][2] > hi[SHAPE_BALL][2] + 4000);
    /* a one-sided ray is full scale in every degree; the two-sided shapes are not */
    for (int l = 1; l < 4; l++) CHECK(lo[SHAPE_RAY][l] >= 65000);
    CHECK(hi[SHAPE_PLANE][1] < 20000 && hi[SHAPE_BALL][1] < 20000 && hi[SHAPE_LINE][1] < 20000);
    /* negative control (what dropping the degree >= 2 channels costs): the
     * degree-1 ranges of line, plane and ball overlap, so degrees 0 and 1
     * alone cannot tell the three shapes apart */
    CHECK(lo[SHAPE_LINE][1] <= hi[SHAPE_PLANE][1] && lo[SHAPE_PLANE][1] <= hi[SHAPE_LINE][1]);
    CHECK(lo[SHAPE_PLANE][1] <= hi[SHAPE_BALL][1] && lo[SHAPE_BALL][1] <= hi[SHAPE_PLANE][1]);
    CHECK(lo[SHAPE_LINE][1] <= hi[SHAPE_BALL][1] && lo[SHAPE_BALL][1] <= hi[SHAPE_LINE][1]);
}

/* tk_sh_power edge cases and determinism of accumulation order */
static void test_power_edges(void)
{
    i64 acc[16] = { 0 }, acc2[16] = { 0 };
    u32 p[4], p2[4];
    tk_sh_power(acc, 0, p);
    for (int l = 0; l < 4; l++) CHECK_EQ(p[l], 0);
    tk_sh_power(acc, 5, p);                                  /* nothing accumulated */
    for (int l = 0; l < 4; l++) CHECK_EQ(p[l], 0);

    /* large weights (sum 2^31): no overflow, power[0] exact; order does not matter */
    i32 vs[64][3];
    for (int k = 0; k < 64; k++)
        for (int i = 0; i < 3; i++) vs[k][i] = (i32)(rnd64() % 2000001u) - 1000000;
    for (int k = 0; k < 64; k++) tk_sh_accumulate(acc, vs[k], 1 << 25);
    for (int k = 63; k >= 0; k--) tk_sh_accumulate(acc2, vs[k], 1 << 25);
    tk_sh_power(acc, 0x80000000u, p);
    tk_sh_power(acc2, 0x80000000u, p2);
    CHECK_EQ(p[0], 65536);
    for (int l = 0; l < 4; l++) { CHECK_EQ(p[l], p2[l]); CHECK(p[l] <= 65536 + 16); }
    for (int i = 0; i < 16; i++) CHECK_EQ(acc[i], acc2[i]);

    /* the same set with unit weights gives the same spectrum up to rounding */
    i64 acc1[16] = { 0 };
    for (int k = 0; k < 64; k++) tk_sh_accumulate(acc1, vs[k], 1);
    tk_sh_power(acc1, 64, p2);
    for (int l = 0; l < 4; l++) CHECK(abs((i32)p[l] - (i32)p2[l]) <= 2);

    /* n far too small, and absurd accumulators: the result is clamped and fits u32 */
    tk_sh_power(acc, 1, p);
    for (int l = 0; l < 4; l++) CHECK(p[l] <= 7u * 13u * 65536u);
    for (int i = 0; i < 16; i++) acc[i] = (i & 1) ? INT64_MAX : INT64_MIN;
    tk_sh_power(acc, 1, p);
    for (int l = 0; l < 4; l++) CHECK(p[l] <= 7u * 13u * 65536u);
    CHECK(p[3] > 65536);

    /* negative weights are allowed: a +1 and a -1 in the same direction cancel */
    i64 accn[16] = { 0 };
    const i32 dv[3] = { 3, -4, 12 };
    tk_sh_accumulate(accn, dv, 1);
    tk_sh_accumulate(accn, dv, -1);
    for (int i = 0; i < 16; i++) CHECK_EQ(accn[i], 0);
}

int main(void)
{
    test_constants();
    test_accuracy();
    test_exact();
    test_orthonormal();
    test_single_direction();
    test_rotation();
    test_shapes();
    test_power_edges();
    return az_test_done();
}
