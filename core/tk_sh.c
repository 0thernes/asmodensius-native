/* tk_sh.c - integer real spherical harmonics, degree 0..3.
 *
 * Derived from libirrep (tsotchke, MIT License), src/solid_harmonics.c:
 * the polynomials of solid_harm_l1_ .. solid_harm_l3_ and the constants of
 * init_constants_, re-expressed in 64-bit integer arithmetic. Conventions,
 * scaling and limits are documented in tk_sh.h.
 *
 * Arithmetic plan for tk_sh_eval:
 *   1. scale v by a power of two so its largest component has 30 significant
 *      bits (exact for |v| < 2^30, a rounded right shift above that);
 *   2. len = az_isqrt_u64(x^2 + y^2 + z^2)   (sum < 3 * 2^60, 2^29 <= len);
 *      one division inv = 2^60 / len; unit vector u = v * inv in Q30;
 *   3. evaluate the polynomials in Q30 with i64 products (every product is
 *      below 2^63, bounds next to each line), multiply by the Q30 constant
 *      and round once to Q14.
 * Every rounding is half away from zero, which makes the outputs exactly
 * odd / even under v -> -v. Intermediate error is below 2e-8, so the final
 * error is the Q14 rounding (0.5 LSB) plus less than 0.001 LSB.
 */
#include "az_abi.h"
#include "tk_sh.h"

/* v / 2^s rounded half away from zero, 1 <= s <= 62, |v| < 2^62.
 * Branch-free (the sign of a harmonic is not predictable): sg is 0 or -1,
 * (v ^ sg) - sg is |v|, and the same step puts the sign back. */
AZ_INLINE i64 sh_rs(i64 v, u32 s)
{
    i64 sg = v >> 63;
    i64 a = (v ^ sg) - sg;
    i64 r = (a + ((i64)1 << (s - 1))) >> s;
    return (r ^ sg) - sg;
}

/* Q30 * Q30 -> Q30 */
AZ_INLINE i64 sh_mul(i64 a, i64 b) { return sh_rs(a * b, 30); }

/* (Q30 polynomial value) * (Q30 constant) -> Q14, saturated to i16 for safety */
AZ_INLINE i16 sh_out(i64 t, i64 k) { return az_sat_i16((i32)sh_rs(t * k, 46)); }

void tk_sh_eval(const i32 v[3], i16 out[TK_SH_N])
{
    i64 x = v[0], y = v[1], z = v[2];
    u64 ax = (u64)(x < 0 ? -x : x), ay = (u64)(y < 0 ? -y : y), az = (u64)(z < 0 ? -z : z);
    u64 m = ax > ay ? ax : ay;
    if (az > m) m = az;

    out[0] = TK_SH_Y00_Q14;
    if (m == 0) {
        for (u32 i = 1; i < TK_SH_N; i++) out[i] = 0;
        return;
    }

    /* 1. bring the largest component to [2^29, 2^30) */
    u32 top = 63u - (u32)__builtin_clzll(m);        /* 0 .. 31 */
    if (top <= 29u) {
        i64 f = (i64)1 << (29u - top);
        x *= f; y *= f; z *= f;
    } else {
        u32 s = top - 29u;                          /* 1 or 2 */
        x = sh_rs(x, s); y = sh_rs(y, s); z = sh_rs(z, s);
    }

    /* 2. unit vector in Q30. |x|,|y|,|z| <= 2^30, sum of squares <= 3 * 2^60 */
    u32 len = az_isqrt_u64((u64)(x * x) + (u64)(y * y) + (u64)(z * z));   /* >= 2^29 */
    i64 inv = (i64)(((u64)1 << 60) / len);          /* <= 2^31 */
    x = sh_rs(x * inv, 30);                         /* product < 2^61 */
    y = sh_rs(y * inv, 30);
    z = sh_rs(z * inv, 30);

    /* 3. polynomials. On the unit sphere |x|,|y|,|z| <= 2^30 (+1 rounding). */
    i64 xx = sh_mul(x, x), yy = sh_mul(y, y), zz = sh_mul(z, z);   /* 0 .. 2^30 */

    /* l = 1: (y, z, x) */
    out[1] = sh_out(y, TK_SH_K1_Q30);
    out[2] = sh_out(z, TK_SH_K1_Q30);
    out[3] = sh_out(x, TK_SH_K1_Q30);

    /* l = 2. |xy| <= 2^29; |2zz - xx - yy| <= 2^31; K2A < 2^31: products < 2^62 */
    i64 xy = sh_mul(x, y);
    out[4] = sh_out(xy, TK_SH_K2A_Q30);
    out[5] = sh_out(sh_mul(y, z), TK_SH_K2A_Q30);
    out[6] = sh_out(2 * zz - xx - yy, TK_SH_K20_Q30);
    out[7] = sh_out(sh_mul(x, z), TK_SH_K2A_Q30);
    out[8] = sh_out(xx - yy, TK_SH_K22_Q30);

    /* l = 3. Inner factors are at most 4 * 2^30, so y * factor < 2^62; the
     * polynomial values are bounded on the sphere by 1, 0.193, 1.73, 2, 1.73,
     * 0.385, 1 (times 2^30), and value * K < 2^61 for each. */
    i64 q = 4 * zz - xx - yy;
    out[9]  = sh_out(sh_mul(y, 3 * xx - yy), TK_SH_K33_Q30);
    out[10] = sh_out(sh_mul(xy, z), TK_SH_K3A_Q30);
    out[11] = sh_out(sh_mul(y, q), TK_SH_K31_Q30);
    out[12] = sh_out(sh_mul(z, 2 * zz - 3 * xx - 3 * yy), TK_SH_K30_Q30);
    out[13] = sh_out(sh_mul(x, q), TK_SH_K31_Q30);
    out[14] = sh_out(sh_mul(z, xx - yy), TK_SH_K32_Q30);
    out[15] = sh_out(sh_mul(x, xx - 3 * yy), TK_SH_K33_Q30);
}

void tk_sh_accumulate(i64 acc[TK_SH_N], const i32 v[3], i32 weight)
{
    i16 y[TK_SH_N];
    tk_sh_eval(v, y);
    for (u32 i = 0; i < TK_SH_N; i++) acc[i] += (i64)weight * (i64)y[i];
}

void tk_sh_power(const i64 acc[TK_SH_N], u32 n, u32 power[TK_SH_LMAX + 1])
{
    static const u32 pc[TK_SH_LMAX + 1] = { TK_SH_PC0_Q24, TK_SH_PC1_Q24, TK_SH_PC2_Q24, TK_SH_PC3_Q24 };
    const i64 acc_lim = (i64)1 << 46;      /* keeps acc * 2^16 inside i64        */
    const i64 one_q30 = (i64)1 << 30;      /* mean coefficients clamp at +-1.0   */

    for (u32 l = 0; l <= TK_SH_LMAX; l++) {
        if (n == 0) { power[l] = 0; continue; }
        u64 sum = 0;                       /* Q60, at most 7 * 2^60              */
        for (u32 i = l * l; i < (l + 1) * (l + 1); i++) {
            i64 a = az_clamp_i64(acc[i], -acc_lim, acc_lim) * 65536 / (i64)n;   /* Q30, toward zero */
            a = az_clamp_i64(a, -one_q30, one_q30);
            sum += (u64)(a * a);
        }
        u64 s30 = (sum + ((u64)1 << 29)) >> 30;                 /* Q30, < 2^33          */
        power[l] = (u32)((s30 * pc[l] + ((u64)1 << 37)) >> 38); /* < 2^61 before shift */
    }
}
