/* tk_qsv.c - classical simulation of a 3-qubit register in fixed point.
 *
 * See tk_qsv.h for the conventions, the state layout and the op encoding.
 * Deterministic integer arithmetic only: this is a classical state-vector
 * module, not a source of randomness and not a speed-up.
 *
 * Derived from the conventions of the Moonlab dense state-vector kernel
 * (tsotchke/moonlab, MIT License, Copyright (c) 2024-2026 tsotchke); new
 * integer code, no upstream source line.
 */
#include "tk_qsv.h"

#define QSV_F       30                       /* fractional bits of gate coefficients */
#define QSV_HALF    ((i64)1 << (QSV_F - 1))
#define QSV_RSQRT2  759250125                /* round(2^30 / sqrt(2)) */

/* one rounding per output component, then the clamp every narrowing needs */
AZ_INLINE i16 qsv_rnd(i64 v)
{
    return (i16)az_clamp_i64((v + QSV_HALF) >> QSV_F, -32767, 32767);
}

AZ_INLINE i16 qsv_neg(i16 v) { return az_sat_i16(-(i32)v); }

/* round-to-nearest signed division, den > 0 */
AZ_INLINE i64 qsv_div_round(i64 num, i64 den)
{
    return num >= 0 ? (num + den / 2) / den : -((-num + den / 2) / den);
}

/* squared magnitudes; the total is at most 2^34 */
AZ_INLINE u64 qsv_weights(const i16 *s, u32 w[TK_QSV_DIM])
{
    u64 tot = 0;
    for (u32 i = 0; i < TK_QSV_DIM; i++) {
        i32 r = s[2 * i], m = s[2 * i + 1];
        w[i] = (u32)(r * r) + (u32)(m * m);
        tot += w[i];
    }
    return tot;
}

void tk_qsv_reset(i16 s[TK_QSV_LEN])
{
    for (u32 i = 0; i < TK_QSV_LEN; i++) s[i] = 0;
    s[0] = TK_QSV_ONE;
}

u32 tk_qsv_norm2(const i16 s[TK_QSV_LEN])
{
    u32 w[TK_QSV_DIM];
    u64 tot = qsv_weights(s, w);
    return tot > 0xFFFFFFFFull ? 0xFFFFFFFFu : (u32)tot;
}

/* ---------------------------------------------------------------------- */
/* single-qubit gates: the four index pairs (i, i | bit) with bit q clear  */
/* ---------------------------------------------------------------------- */

void tk_qsv_x(i16 s[TK_QSV_LEN], u32 q)
{
    if (q >= TK_QSV_QUBITS) return;
    const u32 bit = 1u << q;
    for (u32 i = 0; i < TK_QSV_DIM; i++) {
        if (i & bit) continue;
        i16 *a = s + 2 * i, *b = s + 2 * (i | bit);
        i16 r = a[0], m = a[1];
        a[0] = b[0]; a[1] = b[1];
        b[0] = r;    b[1] = m;
    }
}

void tk_qsv_y(i16 s[TK_QSV_LEN], u32 q)
{
    if (q >= TK_QSV_QUBITS) return;
    const u32 bit = 1u << q;
    for (u32 i = 0; i < TK_QSV_DIM; i++) {
        if (i & bit) continue;
        i16 *a = s + 2 * i, *b = s + 2 * (i | bit);
        i16 r0 = a[0], m0 = a[1], r1 = b[0], m1 = b[1];
        a[0] = m1;            a[1] = qsv_neg(r1);     /* a0' = -i a1 */
        b[0] = qsv_neg(m0);   b[1] = r0;              /* a1' =  i a0 */
    }
}

void tk_qsv_z(i16 s[TK_QSV_LEN], u32 q)
{
    if (q >= TK_QSV_QUBITS) return;
    const u32 bit = 1u << q;
    for (u32 i = 0; i < TK_QSV_DIM; i++) {
        if (!(i & bit)) continue;
        s[2 * i] = qsv_neg(s[2 * i]);
        s[2 * i + 1] = qsv_neg(s[2 * i + 1]);
    }
}

void tk_qsv_h(i16 s[TK_QSV_LEN], u32 q)
{
    if (q >= TK_QSV_QUBITS) return;
    const u32 bit = 1u << q;
    for (u32 i = 0; i < TK_QSV_DIM; i++) {
        if (i & bit) continue;
        i16 *a = s + 2 * i, *b = s + 2 * (i | bit);
        i64 r0 = a[0], m0 = a[1], r1 = b[0], m1 = b[1];
        a[0] = qsv_rnd((r0 + r1) * QSV_RSQRT2);
        a[1] = qsv_rnd((m0 + m1) * QSV_RSQRT2);
        b[0] = qsv_rnd((r0 - r1) * QSV_RSQRT2);
        b[1] = qsv_rnd((m0 - m1) * QSV_RSQRT2);
    }
}

/* ---------------------------------------------------------------------- */
/* rotations                                                               */
/* ---------------------------------------------------------------------- */

/* 2^36 / sqrt(n2 / 2^28) for a squared length n2 within 1/64 of 2^28, by the
 * binomial series of (1 + eps)^(-1/2) to the fourth order (the fifth-order
 * term is below 2.4e-10 in that window). Returns 0 outside the window. This
 * is the common case (a Q1.14 sine pair, a state that drifted a little) and
 * it needs no square root and no division. */
AZ_INLINE i64 qsv_rsqrt_near_one(u64 n2)
{
    const i64 e = (i64)n2 - ((i64)1 << 28);
    if (e <= -((i64)1 << 22) || e >= ((i64)1 << 22)) return 0;
    const i64 e1 = e * 256;                             /* eps in Q36, |e1| < 2^30 */
    const i64 e2 = (e1 * e1) >> 36;
    const i64 e3 = (e2 * e1) >> 36;
    const i64 e4 = (e2 * e2) >> 36;
    return ((i64)1 << 36) - e1 / 2 + (3 * e2) / 8 - (5 * e3) / 16 + (35 * e4) / 128;
}

void tk_qsv_unit(i32 c14, i32 s14, i32 cs[2])
{
    /* Dividing by the pair's own length removes the error of the sine table
     * from the norm: only the direction (the angle) keeps that error. */
    i64 c = az_clamp_i32(c14, -32767, 32767), sn = az_clamp_i32(s14, -32767, 32767);
    u64 n2 = (u64)(c * c + sn * sn);                    /* < 2^31 */
    if (n2 == 0) { cs[0] = TK_QSV_CS_ONE; cs[1] = 0; return; }
    const i64 f = qsv_rsqrt_near_one(n2);
    if (f) {                                            /* |c * f| < 2^52 */
        cs[0] = (i32)az_clamp_i64((c * f + ((i64)1 << 19)) >> 20, -TK_QSV_CS_ONE, TK_QSV_CS_ONE);
        cs[1] = (i32)az_clamp_i64((sn * f + ((i64)1 << 19)) >> 20, -TK_QSV_CS_ONE, TK_QSV_CS_ONE);
        return;
    }
    /* Shift n2 by an even count into [2^61, 2^63) so the root keeps 31 bits
     * for short pairs too. |c| <= |pair|, so c << (30 + sh/2) < 2^62. */
    const u32 sh = ((u32)__builtin_clzll(n2) - 1u) & ~1u;
    const i64 len = (i64)az_isqrt_u64(n2 << sh);        /* |pair| * 2^(sh/2), in [2^30.5, 2^31.5) */
    const u32 up = 30u + sh / 2u;
    cs[0] = (i32)az_clamp_i64(qsv_div_round(c * ((i64)1 << up), len), -TK_QSV_CS_ONE, TK_QSV_CS_ONE);
    cs[1] = (i32)az_clamp_i64(qsv_div_round(sn * ((i64)1 << up), len), -TK_QSV_CS_ONE, TK_QSV_CS_ONE);
}

void tk_qsv_cs(u32 angle, i32 cs[2])
{
    u32 half = angle >> 1;
    tk_qsv_unit(az_cos_q14(half), az_sin_q14(half), cs);
}

void tk_qsv_rot(i16 s[TK_QSV_LEN], u32 kind, u32 q, const i32 cs[2])
{
    if (q >= TK_QSV_QUBITS || kind > TK_QSV_RZ) return;
    const u32 bit = 1u << q;
    const i64 c = az_clamp_i32(cs[0], -TK_QSV_CS_ONE, TK_QSV_CS_ONE);
    const i64 sn = az_clamp_i32(cs[1], -TK_QSV_CS_ONE, TK_QSV_CS_ONE);
    for (u32 i = 0; i < TK_QSV_DIM; i++) {
        if (i & bit) continue;
        i16 *a = s + 2 * i, *b = s + 2 * (i | bit);
        i64 r0 = a[0], m0 = a[1], r1 = b[0], m1 = b[1];
        if (kind == TK_QSV_RX) {            /* [[c, -i s], [-i s, c]] */
            a[0] = qsv_rnd(c * r0 + sn * m1);
            a[1] = qsv_rnd(c * m0 - sn * r1);
            b[0] = qsv_rnd(c * r1 + sn * m0);
            b[1] = qsv_rnd(c * m1 - sn * r0);
        } else if (kind == TK_QSV_RY) {     /* [[c, -s], [s, c]] */
            a[0] = qsv_rnd(c * r0 - sn * r1);
            a[1] = qsv_rnd(c * m0 - sn * m1);
            b[0] = qsv_rnd(sn * r0 + c * r1);
            b[1] = qsv_rnd(sn * m0 + c * m1);
        } else {                            /* diag(c - i s, c + i s) */
            a[0] = qsv_rnd(c * r0 + sn * m0);
            a[1] = qsv_rnd(c * m0 - sn * r0);
            b[0] = qsv_rnd(c * r1 - sn * m1);
            b[1] = qsv_rnd(c * m1 + sn * r1);
        }
    }
}

void tk_qsv_rx(i16 s[TK_QSV_LEN], u32 q, u32 angle)
{
    i32 cs[2];
    tk_qsv_cs(angle, cs);
    tk_qsv_rot(s, TK_QSV_RX, q, cs);
}

void tk_qsv_ry(i16 s[TK_QSV_LEN], u32 q, u32 angle)
{
    i32 cs[2];
    tk_qsv_cs(angle, cs);
    tk_qsv_rot(s, TK_QSV_RY, q, cs);
}

void tk_qsv_rz(i16 s[TK_QSV_LEN], u32 q, u32 angle)
{
    i32 cs[2];
    tk_qsv_cs(angle, cs);
    tk_qsv_rot(s, TK_QSV_RZ, q, cs);
}

/* ---------------------------------------------------------------------- */
/* two-qubit gates                                                         */
/* ---------------------------------------------------------------------- */

void tk_qsv_cnot(i16 s[TK_QSV_LEN], u32 control, u32 target)
{
    if (control >= TK_QSV_QUBITS || target >= TK_QSV_QUBITS || control == target) return;
    const u32 cm = 1u << control, tm = 1u << target;
    for (u32 i = 0; i < TK_QSV_DIM; i++) {
        if (!(i & cm) || (i & tm)) continue;
        i16 *a = s + 2 * i, *b = s + 2 * (i | tm);
        i16 r = a[0], m = a[1];
        a[0] = b[0]; a[1] = b[1];
        b[0] = r;    b[1] = m;
    }
}

void tk_qsv_cz(i16 s[TK_QSV_LEN], u32 a, u32 b)
{
    if (a >= TK_QSV_QUBITS || b >= TK_QSV_QUBITS || a == b) return;
    const u32 mask = (1u << a) | (1u << b);
    for (u32 i = 0; i < TK_QSV_DIM; i++) {
        if ((i & mask) != mask) continue;
        s[2 * i] = qsv_neg(s[2 * i]);
        s[2 * i + 1] = qsv_neg(s[2 * i + 1]);
    }
}

/* ---------------------------------------------------------------------- */
/* renormalisation, probabilities, sampling                                */
/* ---------------------------------------------------------------------- */

void tk_qsv_renorm(i16 s[TK_QSV_LEN])
{
    u32 w[TK_QSV_DIM];
    u64 tot = qsv_weights(s, w);
    if (tot == 0) { tk_qsv_reset(s); return; }
    const i64 f = qsv_rsqrt_near_one(tot);
    if (f) {                                            /* |s[i] * f| < 2^52 */
        for (u32 i = 0; i < TK_QSV_LEN; i++) {
            i64 v = ((i64)s[i] * f + ((i64)1 << 35)) >> 36;
            s[i] = (i16)az_clamp_i64(v, -32767, 32767);
        }
        return;
    }
    /* Far from unit length (a state set from outside): exact root and one
     * division per word. tot is shifted by an even count into [2^61, 2^63);
     * |s[i]| <= |psi|, so s[i] << (14 + sh/2) stays below 2^46. */
    const u32 sh = ((u32)__builtin_clzll(tot) - 1u) & ~1u;
    const i64 len = (i64)az_isqrt_u64(tot << sh);       /* |psi| * 2^(sh/2) */
    const u32 up = 14u + sh / 2u;
    for (u32 i = 0; i < TK_QSV_LEN; i++) {
        i64 v = qsv_div_round((i64)s[i] * ((i64)1 << up), len);
        s[i] = (i16)az_clamp_i64(v, -32767, 32767);
    }
}

/* Cumulative rounding: cum_i = round(65536 * (w_0 + .. + w_i) / total). The
 * last one is exactly 65536 and the sequence never decreases, so the
 * differences are non-negative and sum exactly. */
AZ_INLINE u32 qsv_cum(u64 acc, u64 tot)
{
    return (u32)((acc * TK_QSV_PROB_ONE + tot / 2) / tot);
}

void tk_qsv_probs(const i16 s[TK_QSV_LEN], u32 p[TK_QSV_DIM])
{
    u32 w[TK_QSV_DIM];
    u64 tot = qsv_weights(s, w);
    if (tot == 0) {
        for (u32 i = 0; i < TK_QSV_DIM; i++) p[i] = 0;
        p[0] = TK_QSV_PROB_ONE;
        return;
    }
    u64 acc = 0;
    u32 prev = 0;
    for (u32 i = 0; i < TK_QSV_DIM; i++) {
        acc += w[i];
        u32 cum = qsv_cum(acc, tot);
        p[i] = cum - prev;
        prev = cum;
    }
}

u32 tk_qsv_sample(const i16 s[TK_QSV_LEN], u32 r)
{
    u32 w[TK_QSV_DIM];
    u64 tot = qsv_weights(s, w);
    if (tot == 0) return 0;
    /* u < cum_i  <=>  (u + 1) * total <= 65536 * acc + total / 2, so the
     * inverse CDF needs no division. Both sides stay below 2^51. */
    const u64 need = ((u64)(r >> 16) + 1u) * tot;
    u64 acc = 0;
    for (u32 i = 0; i < TK_QSV_DIM - 1; i++) {
        acc += w[i];
        if (need <= acc * TK_QSV_PROB_ONE + tot / 2) return i;
    }
    return TK_QSV_DIM - 1;
}

/* ---------------------------------------------------------------------- */
/* programs                                                                */
/* ---------------------------------------------------------------------- */

TkQsvOp tk_qsv_decode(u8 op)
{
    TkQsvOp d = { TK_QSV_G_NOP, 0, 0, 0, 0 };
    const u32 cls = (u32)op >> 6;
    if (cls < 3u) {
        const u32 q = op & 3u;
        if (q == 3u) return d;
        d.gate = (u8)(TK_QSV_G_RX + cls);
        d.q0 = (u8)q;
        d.slot = (u8)((op >> 3) & 7u);
        d.inv = (u8)((op >> 2) & 1u);
        return d;
    }
    const u32 g = (op >> 3) & 7u, arg = op & 7u;
    if (g < 4u) {
        if ((arg & 3u) == 3u) return d;
        d.gate = (u8)(TK_QSV_G_H + g);
        d.q0 = (u8)(arg & 3u);
    } else if (g == 4u) {
        if (arg >= 6u) return d;
        const u32 c = arg >> 1, k = arg & 1u;
        d.gate = TK_QSV_G_CNOT;
        d.q0 = (u8)c;
        d.q1 = (u8)(k + (k >= c ? 1u : 0u));    /* the k-th qubit that is not the control */
    } else if (g == 5u) {
        const u32 pair = arg & 3u;
        if (pair == 3u) return d;
        d.gate = TK_QSV_G_CZ;
        d.q0 = (u8)(pair >> 1);
        d.q1 = (u8)(pair == 0u ? 1u : 2u);
    }
    return d;
}

void tk_qsv_apply(i16 s[TK_QSV_LEN], u8 op, const u16 angles[TK_QSV_SLOTS])
{
    const TkQsvOp d = tk_qsv_decode(op);
    switch (d.gate) {
    case TK_QSV_G_H: tk_qsv_h(s, d.q0); break;
    case TK_QSV_G_X: tk_qsv_x(s, d.q0); break;
    case TK_QSV_G_Y: tk_qsv_y(s, d.q0); break;
    case TK_QSV_G_Z: tk_qsv_z(s, d.q0); break;
    case TK_QSV_G_RX:
    case TK_QSV_G_RY:
    case TK_QSV_G_RZ: {
        i32 cs[2];
        tk_qsv_cs(angles[d.slot], cs);
        if (d.inv) cs[1] = -cs[1];
        tk_qsv_rot(s, (u32)d.gate - TK_QSV_G_RX, d.q0, cs);
        break;
    }
    case TK_QSV_G_CNOT: tk_qsv_cnot(s, d.q0, d.q1); break;
    case TK_QSV_G_CZ: tk_qsv_cz(s, d.q0, d.q1); break;
    default: break;
    }
}

void tk_qsv_run(i16 s[TK_QSV_LEN], const u8 *prog, u32 n_ops, const u16 angles[TK_QSV_SLOTS])
{
    for (u32 k = 0; k < n_ops; k++) {
        tk_qsv_apply(s, prog[k], angles);
        if ((k + 1u) % TK_QSV_RENORM_PERIOD == 0u) tk_qsv_renorm(s);
    }
    if (n_ops % TK_QSV_RENORM_PERIOD != 0u) tk_qsv_renorm(s);
}
