/* tk_qgt.c - quantum geometric tensor in fixed point. See tk_qgt.h for the
 * definitions, formats, accuracy statement and provenance (QGTL diffgeo
 * kernels, MIT License, Copyright (c) 2024 Tsotchke).
 *
 * Scales used below:
 *   amplitudes            Q1.14   |v| <= 2^15
 *   inner products        Q28     |v| <= 8 * 2 * 2^30 = 2^34   (dim <= 8)
 *   product of two inner  Q56     |v| <= 2^69: does not fit 64 bits, so it is
 *   products                      never formed; tk_dot2_shr14 returns it
 *                                 already divided by 2^14 (Q42, |v| <= 2^55).
 * Every division by a power of two is a floor (arithmetic shift), and
 * floor(floor(x / a) / b) == floor(x / (a*b)), so chaining shifts loses
 * nothing: each output is one correctly rounded value.
 */
#include "tk_qgt.h"

/* <u|v> = sum conj(u_k) v_k of two Q1.14 vectors, exact, Q28. */
static void tk_inner(const i16 *u, const i16 *v, u32 dim, i64 *re, i64 *im)
{
    i64 sr = 0, si = 0;
    for (u32 k = 0; k < dim; k++) {
        i64 ur = u[2 * k], ui = u[2 * k + 1];
        i64 vr = v[2 * k], vi = v[2 * k + 1];
        sr += ur * vr + ui * vi;
        si += ur * vi - ui * vr;
    }
    *re = sr;
    *im = si;
}

/* Exact floor((x1*y1 + x2*y2) / 2^14) for |x|, |y| <= 2^34.
 * y = (y >> 14) * 2^14 + (y & 16383) holds for negative y too, so the sum is
 * hi * 2^14 + lo with |hi| <= 2^55 and |lo| <= 2^49. */
static i64 tk_dot2_shr14(i64 x1, i64 y1, i64 x2, i64 y2)
{
    i64 hi = x1 * (y1 >> 14) + x2 * (y2 >> 14);
    i64 lo = x1 * (y1 & 16383) + x2 * (y2 & 16383);
    return hi + (lo >> 14);
}

static i32 tk_narrow(i64 v)
{
    return (i32)az_clamp_i64(v, -(i64)INT32_MAX, (i64)INT32_MAX);
}

bool tk_qgt(const i16 *psi, const i16 *dpsi, u32 dim, u32 n, i32 *g, i32 *f)
{
    i64 xr[TK_QGT_MAX_N], xi[TK_QGT_MAX_N];

    if (dim == 0 || dim > TK_QGT_MAX_DIM || n == 0 || n > TK_QGT_MAX_N) return false;

    /* x_a = <psi|d_a psi>, once per parameter */
    for (u32 a = 0; a < n; a++) tk_inner(psi, dpsi + a * 2 * dim, dim, &xr[a], &xi[a]);

    for (u32 a = 0; a < n; a++) {
        const i16 *da = dpsi + a * 2 * dim;
        for (u32 b = a; b < n; b++) {
            i64 gr, gi;
            tk_inner(da, dpsi + b * 2 * dim, dim, &gr, &gi);   /* <d_a|d_b>, Q28 */

            /* <d_a|psi><psi|d_b> = conj(x_a) * x_b = pr + i*pi  (Q56, not formed)
             *   pr = xr_a*xr_b + xi_a*xi_b      pi = xr_a*xi_b - xi_a*xr_b
             * g = floor((gr*2^28 - pr + 2^39) / 2^40)
             * f = floor((pi - gi*2^28 + 2^38) / 2^39)        (= -2 Im Q)      */
            i64 neg_pr = tk_dot2_shr14(-xr[a], xr[b], -xi[a], xi[b]);
            i64 pos_pi = tk_dot2_shr14(xr[a], xi[b], -xi[a], xr[b]);
            i32 gq = tk_narrow((gr * 16384 + neg_pr + ((i64)1 << 25)) >> 26);
            i32 fq = tk_narrow((pos_pi - gi * 16384 + ((i64)1 << 24)) >> 25);

            g[a * n + b] = gq;
            g[b * n + a] = gq;
            f[a * n + b] = fq;
            f[b * n + a] = -fq;          /* a == b: fq is exactly 0 */
        }
    }
    return true;
}

u32 tk_fidelity_q16(const i16 *a, const i16 *b, u32 dim)
{
    i64 sr, si;

    if (dim == 0 || dim > TK_QGT_MAX_DIM) return 0;
    tk_inner(a, b, dim, &sr, &si);                       /* Q28 */
    i64 m = tk_dot2_shr14(sr, sr, si, si);               /* |<a|b>|^2, Q42, >= 0 */
    return (u32)az_clamp_i64((m + ((i64)1 << 25)) >> 26, 0, (i64)UINT32_MAX);
}

u32 tk_qgt_precondition(i32 step[8], const i32 raw[8], const i32 *g, u32 n, i32 floor_q16)
{
    u32 sat = 0;
    u32 count = az_min_u32(n, 8u);
    i64 lo = floor_q16 < 1 ? 1 : (i64)floor_q16;

    for (u32 a = 0; a < 8u; a++) {
        if (a >= count) { step[a] = 0; continue; }
        i64 d = g ? (i64)g[a * n + a] : (i64)TK_QGT_ONE;
        if (d < lo) d = lo;
        i64 num = (i64)raw[a] * TK_QGT_ONE;              /* |num| <= 2^47 */
        i64 mag = num < 0 ? -num : num;
        i64 q = (mag + d / 2) / d;                       /* nearest, ties away from 0 */
        if (q > (i64)INT32_MAX) { q = INT32_MAX; sat++; }
        step[a] = (i32)(num < 0 ? -q : q);
    }
    return sat;
}
