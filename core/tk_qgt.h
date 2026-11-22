/* tk_qgt.h - quantum geometric tensor of a small state vector, fixed point.
 *
 * WHAT IT IS
 *   For a complex state psi(theta) of dimension dim <= 8 and n <= 8 derivative
 *   vectors d_a psi = d psi / d theta_a, the tensor
 *
 *       Q_ab = <d_a psi|d_b psi> - <d_a psi|psi><psi|d_b psi>
 *
 *   splits into a real symmetric part and an imaginary antisymmetric part:
 *
 *       g_ab = Re Q_ab          the Fubini-Study metric (symmetric)
 *       f_ab = -2 Im Q_ab       the Berry curvature     (antisymmetric)
 *
 *   so that Q = g - (i/2) f. With the connection A_a = i <psi|d_a psi> this is
 *   f_ab = d_a A_b - d_b A_a, the convention of the upstream "diffgeo"
 *   kernels. Check value: psi = (cos(t/2), e^{ip} sin(t/2)) has
 *   g = diag(1/4, sin^2(t)/4) and f_tp = -sin(t)/2.
 *
 * NAMING, HONESTLY
 *   For real amplitudes psi_k = sqrt(p_k) this is nothing but the classical
 *   Fisher information metric: g_ab = (1/4) sum_k (d_a p_k)(d_b p_k) / p_k,
 *   and f is identically zero. "Quantum" in the name is the textbook name of
 *   the formula for complex amplitudes. This is ordinary integer linear
 *   algebra on a vector of at most 8 complex numbers; it runs on a classical
 *   CPU at classical cost and nothing here is faster for being named so.
 *
 * PROVENANCE
 *   Derived from quantum_geometric_tensor (QGTL), HEAD 770994a (2026-01-09),
 *   src/quantum_geometric/distributed/differential_geometry.c:2819-2906
 *   (diffgeo_compute_fubini_study, diffgeo_compute_berry_curvature).
 *   MIT License, Copyright (c) 2024 Tsotchke. The verbatim fragment with its
 *   permission notice is kept as the test oracle in
 *   tests/upstream/qgtl/diffgeo_qgt_fragment.inc.
 *   Changes against upstream: integers instead of doubles; no engine handle,
 *   no counters; <psi|d_b psi> is computed once per parameter and each pair
 *   (a,b) once; the result is rounded once per entry.
 *
 * FORMATS (one fixed-point format per quantity)
 *   psi    i16, Q1.14 (16384 = 1.0), interleaved: psi[2k] = Re, psi[2k+1] = Im.
 *          The caller keeps psi at unit norm (as it does for rotors); the
 *          formula above is not divided by <psi|psi>.
 *   dpsi   i16, Q1.14, same layout, vector a at dpsi + a*2*dim. The value is
 *          the derivative per ONE unit of the parameter. If the caller stores
 *          s_a * d_a psi instead (to use the i16 range better, or because its
 *          parameter is counted in other units) then g_ab and f_ab come out
 *          multiplied by s_a * s_b; that is the caller's bookkeeping.
 *   g, f   i32, Q16.16 (TK_QGT_ONE = 65536 = 1.0), row-major [a*n + b].
 *   fidelity  u32, Q16 (65536 = identical rays, 0 = orthogonal).
 *
 * ACCURACY
 *   Every entry of g and f, and the fidelity, is the EXACT rational value of
 *   the formula on the given integers, rounded once to the nearest Q16 step
 *   (ties toward +infinity; f_ba is stored as -f_ab, g_ba as g_ab). So against
 *   real arithmetic on the same quantised inputs the error is at most half a
 *   step (2^-17). Against the unquantised state the error is dominated by the
 *   Q1.14 input rounding: with e = 2^-15 per real component and L1(v) the sum
 *   of |Re| + |Im| over the components of v,
 *       |dg_ab| <= e*(L1(d_a) + L1(d_b))
 *                + sqrt(2)*e*(|x_a|*(L1(psi) + L1(d_b)) + |x_b|*(L1(psi) + L1(d_a)))
 *                + O(e^2),            x_a = <psi|d_a psi>,
 *   and twice that for f. For the two-level check state this is below 24
 *   steps of Q16 (3.7e-4); the test measures what actually occurs (about 5).
 *   Because psi is only unit to Q1.14 precision, g is positive semidefinite
 *   up to a tolerance: its smallest eigenvalue is at least
 *       -(n/2 steps) - max(0, <psi|psi> - 1) * sum_a <d_a psi|d_a psi>.
 *   tk_qgt_precondition floors the diagonal, so this never reaches a step.
 *
 * All functions are pure: no global state, no allocation, 64-bit integer
 * accumulators only, defined for every i16 input (no overflow for dim <= 8).
 */
#ifndef TK_QGT_H
#define TK_QGT_H

#include "az_abi.h"

#define TK_QGT_MAX_DIM 8u
#define TK_QGT_MAX_N   8u
#define TK_QGT_ONE     65536          /* 1.0 in the Q16.16 outputs            */
#define TK_QGT_AMP_ONE 16384          /* 1.0 in the Q1.14 amplitudes          */

/* g and f receive n*n entries each. Returns false and writes nothing when
 * dim is 0 or > TK_QGT_MAX_DIM, or n is 0 or > TK_QGT_MAX_N. */
bool tk_qgt(const i16 *psi, const i16 *dpsi, u32 dim, u32 n, i32 *g, i32 *f);

/* |<a|b>|^2 in Q16. 65536 for the same ray (any global phase), 0 for
 * orthogonal states. Not divided by the norms: for inputs that are not unit
 * vectors the value scales with <a|a><b|b> (it cannot exceed 2^28).
 * Returns 0 when dim is 0 or > TK_QGT_MAX_DIM. */
u32 tk_fidelity_q16(const i16 *a, const i16 *b, u32 dim);

/* Natural-gradient style step with the diagonal of the metric:
 *
 *     step[a] = raw[a] * TK_QGT_ONE / max(g[a*n + a], floor_q16)  a < n
 *     step[a] = 0                                                 n <= a < 8
 *
 * rounded to nearest (ties away from zero, so the map is odd in raw) and
 * clamped to +-INT32_MAX. A direction in which the state moves a lot per
 * unit of parameter (large g_aa) gets a smaller step. `floor_q16` is in
 * Q16.16 and bounds the amplification at TK_QGT_ONE / floor_q16; values
 * below 1 are taken as 1. With g == NULL the metric is the identity (step =
 * raw when floor_q16 <= TK_QGT_ONE): that is the lesion form on the same
 * code path. g is an n*n matrix as written by tk_qgt; when n > 8 only the
 * first 8 parameters are used. step may be the same array as raw. Returns
 * the number of entries that were clamped, for the caller's saturation
 * tally. */
u32 tk_qgt_precondition(i32 step[8], const i32 raw[8], const i32 *g, u32 n, i32 floor_q16);

#endif /* TK_QGT_H */
