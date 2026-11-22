/* tk_sh.h - integer real spherical harmonics, degree 0..3 (16 values).
 *
 * Derived from libirrep (tsotchke, MIT License): the closed polynomial forms
 * and normalisation constants of src/solid_harmonics.c (solid_harm_l0_ ..
 * solid_harm_l3_, init_constants_). libirrep itself is only a test oracle
 * (tests/upstream/libirrep); nothing of it is linked into the simulation.
 * The constants below are printed by tools/gen/gen_sh.py and checked against
 * libm in tests/test_tk_sh.c.
 *
 * CONVENTION (identical to libirrep irrep_sph_harm_cart_all and
 * irrep_solid_harm_cart_all on a unit vector):
 *   - real harmonics, orthonormal on the unit sphere: the integral of
 *     Y_lm * Y_l'm' over the sphere is 1 if (l, m) == (l', m'), else 0.
 *     So Y_0,0 = sqrt(1 / (4 pi)) = 0.28209...
 *   - index = l*l + l + m, m = -l .. +l  (TK_SH_IDX). m < 0 are the
 *     sin(|m| phi) functions, m > 0 the cos(m phi) functions.
 *   - no Condon-Shortley phase: the degree-1 block is +sqrt(3/(4 pi)) * (y, z, x)
 *     for m = (-1, 0, +1); every listed polynomial has a positive constant:
 *       l=2: xy, yz, (2zz-xx-yy), xz, (xx-yy)
 *       l=3: y(3xx-yy), xyz, y(4zz-xx-yy), z(2zz-3xx-3yy), x(4zz-xx-yy),
 *            z(xx-yy), x(xx-3yy)
 *   - parity: Y_lm(-v) = (-1)^l Y_lm(v), and this holds EXACTLY in the
 *     integer outputs (all rounding is half away from zero).
 *
 * FIXED POINT: outputs are Q1.14 (value * 16384), |out| <= 12228 (0.7464).
 * Against libirrep in binary64 on the same integer vector the measured
 * maximum error is 0.50 LSB (see tests/test_tk_sh.c), i.e. the outputs are
 * the correctly rounded values up to ties.
 *
 * Freestanding, integers only, pure functions, no tables, no state.
 */
#ifndef TK_SH_H
#define TK_SH_H

#include "az_abi.h"

#define TK_SH_LMAX   3
#define TK_SH_N      16                           /* (TK_SH_LMAX + 1)^2 */
#define TK_SH_IDX(l, m) ((l) * (l) + (l) + (m))   /* m = -l .. +l */
#define TK_SH_ONE    16384                        /* Q1.14 */

/* normalisation constants, Q30 (tools/gen/gen_sh.py consts) */
#define TK_SH_K0_Q30   302896976ll   /* sqrt(1/(4 pi))    = 0.282094791774  Y_0,0                      */
#define TK_SH_K1_Q30   524632952ll   /* sqrt(3/(4 pi))    = 0.488602511903  Y_1,m  * {y, z, x}         */
#define TK_SH_K2A_Q30  1173114945ll  /* sqrt(15/(4 pi))   = 1.092548430592  Y_2,-2 Y_2,-1 Y_2,1 * {xy, yz, xz} */
#define TK_SH_K20_Q30  338649115ll   /* sqrt(5/(16 pi))   = 0.315391565253  Y_2,0  * (2zz - xx - yy)   */
#define TK_SH_K22_Q30  586557472ll   /* sqrt(15/(16 pi))  = 0.546274215296  Y_2,2  * (xx - yy)         */
#define TK_SH_K33_Q30  633554480ll   /* sqrt(35/(32 pi))  = 0.590043589927  Y_3,-3 Y_3,3               */
#define TK_SH_K3A_Q30  3103770403ll  /* sqrt(105/(4 pi))  = 2.890611442641  Y_3,-2 * xyz               */
#define TK_SH_K31_Q30  490749190ll   /* sqrt(21/(32 pi))  = 0.457045799464  Y_3,-1 Y_3,1               */
#define TK_SH_K30_Q30  400695036ll   /* sqrt(7/(16 pi))   = 0.373176332590  Y_3,0                      */
#define TK_SH_K32_Q30  1551885201ll  /* sqrt(105/(16 pi)) = 1.445305721320  Y_3,2  * z(xx - yy)        */

#define TK_SH_Y00_Q14  4622          /* out[0] of every evaluation */

/* power scale: TK_SH_POWER_ONE = "all weight in one direction" (see tk_sh_power) */
#define TK_SH_POWER_ONE 65536u
#define TK_SH_PC0_Q24  210814215u    /* 2^54 / (4 * 4622^2): 4 pi calibrated to the rounded Y_0,0 */
#define TK_SH_PC1_Q24  70276238u     /* 4 pi / 3 */
#define TK_SH_PC2_Q24  42165743u     /* 4 pi / 5 */
#define TK_SH_PC3_Q24  30118388u     /* 4 pi / 7 */

/* The 16 real harmonics of degree 0..3 for the DIRECTION of the integer
 * vector v (any i32 components, any length; only the direction matters).
 * out[TK_SH_IDX(l, m)] in Q1.14. The zero vector has no direction: it gives
 * out[0] = TK_SH_Y00_Q14 and 0 for every l >= 1 (isotropic). */
void tk_sh_eval(const i32 v[3], i16 out[TK_SH_N]);

/* acc[i] += weight * Y_i(direction of v), exact (no rounding beyond
 * tk_sh_eval). acc is Q14 times the weight unit. |acc[i]| stays below
 * 12228 * (sum of |weight|), so i64 cannot overflow for any weight sum
 * below 2^49. Start from acc[] = 0. */
void tk_sh_accumulate(i64 acc[TK_SH_N], const i32 v[3], i32 weight);

/* Rotation-invariant power per degree of the accumulated distribution.
 * n = the sum of the weights that were accumulated (the number of
 * directions if every weight was 1); n = 0 gives all zeros.
 *
 *   a_lm     = acc_lm / n                     (mean coefficient)
 *   power[l] = TK_SH_POWER_ONE * (4 pi / (2l+1)) * sum_m a_lm^2
 *
 * By the addition theorem this is TK_SH_POWER_ONE * the weighted mean of
 * P_l(cos angle) over all pairs of accumulated directions, so with
 * non-negative weights 0 <= power[l] <= TK_SH_POWER_ONE, up to at most 16
 * units for l >= 1 (the Q1.14 rounding of the harmonics: 2 sqrt(4 pi) * 0.5
 * LSB = 2.2e-4 relative):
 *   power[0] == TK_SH_POWER_ONE exactly (non-negative weights, n = their sum)
 *   everything in one direction  -> power[l] ~ 65536 for every l
 *   a symmetric line (both ways) -> power ~ (65536, 0, 65536, 0)
 *   a full plane through me      -> power ~ (65536, 0, 16384, 0)
 *   an isotropic ball            -> power ~ (65536, 0, 0, 0)
 * n is a u32, so the weight sum must be below 2^32. If the caller passes an
 * n smaller than the weight sum the mean coefficients are clamped to +-1.0,
 * so the result still fits u32 (at most 7 * 12.6 * 65536). Division is
 * toward zero, so reversing every direction leaves the spectrum unchanged
 * exactly. Under a rotation of all the directions power[0] is unchanged
 * exactly and power[1..3] change only by fixed-point rounding: measured at
 * most 13 units, tested against a tolerance of 24 (3.7e-4 of full scale) in
 * tests/test_tk_sh.c. Never treat the invariance as exact in hashed state. */
void tk_sh_power(const i64 acc[TK_SH_N], u32 n, u32 power[TK_SH_LMAX + 1]);

#endif /* TK_SH_H */
