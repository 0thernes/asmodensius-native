/* tk_qsv.h - classical simulation of a 3-qubit register in fixed point.
 *
 * This is ordinary deterministic integer arithmetic: a norm-preserving linear
 * map on 8 complex numbers plus a sampling step that takes its one random
 * word from the caller. It is NOT a source of randomness and it gives no
 * speed-up of any kind; it is a small state-vector module, nothing more.
 *
 * Derived from the conventions of the Moonlab dense state-vector kernel
 * (tsotchke/moonlab, MIT License, Copyright (c) 2024-2026 tsotchke;
 * src/quantum/gates.c): little-endian qubit order, the gate matrices below,
 * CNOT(control, target), symmetric CZ. The code is a new integer port; no
 * upstream source line is included here. The upstream kernel is used only as
 * a test oracle (tests/upstream/moonlab/).
 *
 * State: 8 complex amplitudes, int16 Q1.14 (16384 = 1.0), interleaved:
 *   s[2*i] = Re a_i, s[2*i + 1] = Im a_i,  i = 0..7        (32 bytes)
 * Qubit q is bit q of the basis index i (q = 0 is the least significant).
 * This is the per-creature `qstate` column.
 *
 * Gate matrices (c = cos(theta/2), s = sin(theta/2)):
 *   H  = [[1, 1], [1, -1]] / sqrt(2)      X = [[0, 1], [1, 0]]
 *   Y  = [[0, -i], [i, 0]]                Z = diag(1, -1)
 *   RX = [[c, -i s], [-i s, c]]           RY = [[c, -s], [s, c]]
 *   RZ = diag(c - i s, c + i s)
 * X, Y, Z, CNOT and CZ are exact (permutations and sign changes). H and the
 * rotations round each component once to the nearest Q1.14 value.
 *
 * Angles: u32, 65536 = one turn (theta = 2 pi). The half angle passed to
 * az_cos_q14 / az_sin_q14 is angle >> 1, so the lowest bit is ignored and
 * angles are periodic with 131072 (a rotation by one turn is -1, a global
 * phase). The (cos, sin) pair is renormalised to a Q1.30 unit pair before
 * use, so a rotation stays norm-preserving up to output rounding whatever
 * the accuracy of the sine table behind az_sin_q14.
 *
 * Every function accepts any 16 int16 values and has defined behaviour for
 * every argument: a qubit index >= 3, or control == target, is a no-op;
 * every computed word goes through a clamp to -32767..32767 (an input word
 * of -32768 can only survive unchanged, moved by a swap or left untouched,
 * and tk_qsv_renorm removes it).
 *
 * Freestanding, integers only, no state outside the caller's 32 bytes.
 */
#ifndef TK_QSV_H
#define TK_QSV_H

#include "az_abi.h"

#define TK_QSV_QUBITS        3u
#define TK_QSV_DIM           8u       /* basis states                          */
#define TK_QSV_LEN           16u      /* int16 words: re, im interleaved       */
#define TK_QSV_ONE           16384    /* 1.0 in Q1.14                          */
#define TK_QSV_NORM2_ONE     (1u << 28)   /* tk_qsv_norm2 of a unit state      */
#define TK_QSV_PROB_ONE      65536u   /* tk_qsv_probs sums to exactly this     */
#define TK_QSV_SLOTS         8u       /* angle slots a program can address     */
#define TK_QSV_RENORM_PERIOD 16u      /* tk_qsv_run renormalises this often    */
#define TK_QSV_CS_ONE        (1 << 30)    /* 1.0 of a unit (cos, sin) pair     */

/* ---- state ---- */
void tk_qsv_reset(i16 s[TK_QSV_LEN]);                 /* |000>: a_0 = 1.0      */
u32  tk_qsv_norm2(const i16 s[TK_QSV_LEN]);           /* sum of re^2 + im^2; a unit state gives 2^28 */

/* ---- gates on qubit q (0..2) ---- */
void tk_qsv_h(i16 s[TK_QSV_LEN], u32 q);
void tk_qsv_x(i16 s[TK_QSV_LEN], u32 q);
void tk_qsv_y(i16 s[TK_QSV_LEN], u32 q);
void tk_qsv_z(i16 s[TK_QSV_LEN], u32 q);
void tk_qsv_rx(i16 s[TK_QSV_LEN], u32 q, u32 angle);
void tk_qsv_ry(i16 s[TK_QSV_LEN], u32 q, u32 angle);
void tk_qsv_rz(i16 s[TK_QSV_LEN], u32 q, u32 angle);

/* ---- two-qubit gates ---- */
void tk_qsv_cnot(i16 s[TK_QSV_LEN], u32 control, u32 target);
void tk_qsv_cz(i16 s[TK_QSV_LEN], u32 a, u32 b);

/* ---- rotation building blocks ---- */
enum { TK_QSV_RX = 0, TK_QSV_RY = 1, TK_QSV_RZ = 2 };
/* Scale any Q1.14 pair (c14, s14) to a Q1.30 unit pair cs = {cos, sin}.
 * (0, 0) gives the identity pair {1.0, 0}. */
void tk_qsv_unit(i32 c14, i32 s14, i32 cs[2]);
/* The unit pair of the half angle: tk_qsv_unit(az_cos_q14(angle >> 1), az_sin_q14(angle >> 1)). */
void tk_qsv_cs(u32 angle, i32 cs[2]);
/* Apply the rotation `kind` (TK_QSV_RX/RY/RZ) with a Q1.30 pair. Callers that
 * reuse one angle for many creatures compute the pair once. */
void tk_qsv_rot(i16 s[TK_QSV_LEN], u32 kind, u32 q, const i32 cs[2]);

/* ---- measurement side ---- */
/* Integer renormalisation: every component is scaled by 1/|psi| and rounded.
 * The all-zero vector cannot be scaled and becomes |000>. */
void tk_qsv_renorm(i16 s[TK_QSV_LEN]);
/* Eight probabilities in units of 1/65536 that sum to exactly 65536 for
 * every input. p[i] is within one unit of 65536 * |a_i|^2 / sum|a|^2. The
 * all-zero vector gives p[0] = 65536. */
void tk_qsv_probs(const i16 s[TK_QSV_LEN], u32 p[TK_QSV_DIM]);
/* Outcome 0..7 from one uniform 32-bit word r (the engine passes an az_rand
 * word of stream AZ_RS_QSV). Inverse CDF of tk_qsv_probs on r >> 16: exactly
 * p[i] of the 65536 values of r >> 16 give outcome i. The state is not
 * changed (no collapse). No randomness is produced here. */
u32  tk_qsv_sample(const i16 s[TK_QSV_LEN], u32 r);

/* ---- programs: one byte per op, so a genome can encode a circuit ----
 *
 *   bit   7 6 | 5 4 3 | 2   | 1 0
 *   0 0 = RX  | slot  | inv | qubit     rotate `qubit` by angles[slot];
 *   0 1 = RY  | slot  | inv | qubit     inv = 1 rotates by the negative angle;
 *   1 0 = RZ  | slot  | inv | qubit     qubit = 3 is a no-op
 *   1 1       | gate  |    arg
 *               0 = H, 1 = X, 2 = Y, 3 = Z : qubit = arg & 3 (3 is a no-op; bit 2 ignored)
 *               4 = CNOT : arg 0..5 = ordered pair (control, target):
 *                          0 (0,1)  1 (0,2)  2 (1,0)  3 (1,2)  4 (2,0)  5 (2,1); 6, 7 no-op
 *               5 = CZ   : arg & 3 = pair: 0 (0,1)  1 (0,2)  2 (1,2); 3 no-op (bit 2 ignored)
 *               6, 7     : reserved, no-op
 *
 * Every byte value is a valid op: 180 of the 256 values are gates (48 each
 * of RX, RY, RZ; 6 each of H, X, Y, Z, CNOT, CZ), the other 76 are no-ops.
 * 0xFF is the canonical no-op.
 */
enum {
    TK_QSV_G_NOP = 0, TK_QSV_G_H, TK_QSV_G_X, TK_QSV_G_Y, TK_QSV_G_Z,
    TK_QSV_G_RX, TK_QSV_G_RY, TK_QSV_G_RZ, TK_QSV_G_CNOT, TK_QSV_G_CZ,
    TK_QSV_G_COUNT
};

typedef struct TkQsvOp {
    u8 gate;   /* TK_QSV_G_*                                                  */
    u8 q0;     /* the qubit; the control of CNOT; the lower qubit of CZ       */
    u8 q1;     /* the target of CNOT; the higher qubit of CZ; else 0          */
    u8 slot;   /* angle slot of a rotation; else 0                            */
    u8 inv;    /* 1 = rotate by the negative angle; else 0                    */
} TkQsvOp;

TkQsvOp tk_qsv_decode(u8 op);

/* encoders (compile-time constants for valid arguments) */
#define TK_QSV_OP_ROT(kind, q, slot, inv) ((u8)(((kind) << 6) | ((slot) << 3) | ((inv) << 2) | (q)))
#define TK_QSV_OP_RX(q, slot)   TK_QSV_OP_ROT(TK_QSV_RX, q, slot, 0)
#define TK_QSV_OP_RY(q, slot)   TK_QSV_OP_ROT(TK_QSV_RY, q, slot, 0)
#define TK_QSV_OP_RZ(q, slot)   TK_QSV_OP_ROT(TK_QSV_RZ, q, slot, 0)
#define TK_QSV_OP_FIX(g, arg)   ((u8)(0xC0u | ((g) << 3) | (arg)))
#define TK_QSV_OP_H(q)          TK_QSV_OP_FIX(0, q)
#define TK_QSV_OP_X(q)          TK_QSV_OP_FIX(1, q)
#define TK_QSV_OP_Y(q)          TK_QSV_OP_FIX(2, q)
#define TK_QSV_OP_Z(q)          TK_QSV_OP_FIX(3, q)
#define TK_QSV_OP_CNOT(c, t)    TK_QSV_OP_FIX(4, (c) * 2 + ((t) > (c) ? (t) - 1 : (t)))
#define TK_QSV_OP_CZ(a, b)      TK_QSV_OP_FIX(5, (a) + (b) - 1)
#define TK_QSV_OP_NOP           0xFFu

/* Apply one op byte (no renormalisation). */
void tk_qsv_apply(i16 s[TK_QSV_LEN], u8 op, const u16 angles[TK_QSV_SLOTS]);
/* Run n_ops op bytes on the state. angles[k] is the rotation angle of slot k
 * (65536 = one turn, so a u16 covers one turn). The state is renormalised
 * after every TK_QSV_RENORM_PERIOD-th op byte and once more at the end if
 * ops remain since the last one. n_ops = 0 leaves the state untouched. */
void tk_qsv_run(i16 s[TK_QSV_LEN], const u8 *prog, u32 n_ops, const u16 angles[TK_QSV_SLOTS]);

#endif /* TK_QSV_H */
