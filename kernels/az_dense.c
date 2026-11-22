/* az_dense.c - C reference of the dense fixed-size network (az_dense.h).
 *
 *   x[0..47]  = sens (u8, 0..127)      x[48..79] = hid (u8, 0..127)
 *   acc[h]    = b1[h] + sum_j w1[h][j] * x[j]                 (i32)
 *   a         = acc[h] >> shift1
 *   a         = ((a * gain[h & 7]) >> 6) + 2 * bias[h & 7]
 *   hid'[h]   = clamp(a, 0, 127)
 *   m[k]      = b2[k] + sum_h w2[k][h] * hid'[h]
 *   motor[k]  = clamp(m[k] >> shift2, -127, 127)
 *
 * Contract: sens and hid in 0..127, gain any u8, bias any i8, weights any
 * i8, |b1|, |b2| <= 2^20, shifts 0..15.
 */
#include "az_dense.h"

/* the twins address AzDenseNet by these offsets (kernels/az_brain_vnni.asm) */
_Static_assert(offsetof(AzDenseNet, w1) == 0, "AzDenseNet.w1 offset is fixed for the twins");
_Static_assert(offsetof(AzDenseNet, w2) == 2560, "AzDenseNet.w2 offset is fixed for the twins");
_Static_assert(offsetof(AzDenseNet, b1) == 3072, "AzDenseNet.b1 offset is fixed for the twins");
_Static_assert(offsetof(AzDenseNet, b2) == 3200, "AzDenseNet.b2 offset is fixed for the twins");
_Static_assert(offsetof(AzDenseNet, shift1) == 3264, "AzDenseNet.shift1 offset is fixed for the twins");
_Static_assert(offsetof(AzDenseNet, shift2) == 3265, "AzDenseNet.shift2 offset is fixed for the twins");
_Static_assert(AZ_DENSE_SENS == 48 && AZ_DENSE_HID == 32 && AZ_DENSE_MOT == 16 && AZ_DENSE_MODG == 8, "the twins are unrolled for 48 + 32 -> 32 -> 16");

#define BR_IN (AZ_DENSE_SENS + AZ_DENSE_HID)

void az_dense_step_c(const AzDenseNet *c, const u8 *sens, u8 *hid,
                     const u8 *gain, const i8 *bias, i8 *motor)
{
    u8 x[BR_IN];
    i32 acc[AZ_DENSE_HID];
    i32 m[AZ_DENSE_MOT];

    for (u32 j = 0; j < AZ_DENSE_SENS; j++) x[j] = sens[j];
    for (u32 j = 0; j < AZ_DENSE_HID; j++) x[AZ_DENSE_SENS + j] = hid[j];

    for (u32 h = 0; h < AZ_DENSE_HID; h++) acc[h] = c->b1[h];
    for (u32 g = 0; g < BR_IN / 4; g++) {
        const i8 *wg = &c->w1[(size_t)g * (AZ_DENSE_HID * 4)];
        const i32 x0 = x[g * 4], x1 = x[g * 4 + 1], x2 = x[g * 4 + 2], x3 = x[g * 4 + 3];
        for (u32 h = 0; h < AZ_DENSE_HID; h++) {
            acc[h] += (i32)wg[h * 4 + 0] * x0 + (i32)wg[h * 4 + 1] * x1
                    + (i32)wg[h * 4 + 2] * x2 + (i32)wg[h * 4 + 3] * x3;
        }
    }
    const u32 s1 = c->shift1;
    for (u32 h = 0; h < AZ_DENSE_HID; h++) {
        i32 a = acc[h] >> s1;
        a = ((a * (i32)gain[h & 7]) >> 6) + 2 * (i32)bias[h & 7];
        hid[h] = (u8)az_clamp_i32(a, 0, 127);
    }

    for (u32 k = 0; k < AZ_DENSE_MOT; k++) m[k] = c->b2[k];
    for (u32 g = 0; g < AZ_DENSE_HID / 4; g++) {
        const i8 *wg = &c->w2[(size_t)g * (AZ_DENSE_MOT * 4)];
        const i32 h0 = hid[g * 4], h1 = hid[g * 4 + 1], h2 = hid[g * 4 + 2], h3 = hid[g * 4 + 3];
        for (u32 k = 0; k < AZ_DENSE_MOT; k++) {
            m[k] += (i32)wg[k * 4 + 0] * h0 + (i32)wg[k * 4 + 1] * h1
                  + (i32)wg[k * 4 + 2] * h2 + (i32)wg[k * 4 + 3] * h3;
        }
    }
    const u32 s2 = c->shift2;
    for (u32 k = 0; k < AZ_DENSE_MOT; k++)
        motor[k] = (i8)az_clamp_i32(m[k] >> s2, -127, 127);
}

/* ---- development: neural program -> network ------------------------------
 * Neuron index space of the program: sensors 0..47, hidden 48..79, motors
 * 80..95. A synapse's weight is the wiring rule entry for its (post type,
 * pre type) pair plus a per-synapse variation that is a pure function of
 * (salt, layer, post, pre), so the same program always develops into the
 * same network. */
static i32 synapse_noise(const AzDenseNet *c, u32 layer, u32 post, u32 pre)
{
    u64 h = az_mix64(((u64)c->salt << 32) ^ ((u64)layer << 24) ^ ((u64)post << 12) ^ pre);
    i32 n = (i32)(h & 0xFF) - 128;                 /* -128..127 */
    return (n * (i32)c->var) >> 8;
}

void az_dense_develop(AzDenseNet *c)
{
    for (u32 h = 0; h < AZ_DENSE_HID; h++) {
        u32 tpost = c->ntype[AZ_DENSE_SENS + h] & 7;
        for (u32 j = 0; j < BR_IN; j++) {
            u32 tpre = c->ntype[j] & 7;
            i32 wv = (i32)c->theta[tpost * 8 + tpre] + synapse_noise(c, 1, h, j);
            c->w1[AZ_W1_IDX(h, j)] = (i8)az_clamp_i32(wv, -127, 127);
        }
        c->b1[h] = (i32)c->tbias[tpost] * 64;
    }
    for (u32 k = 0; k < AZ_DENSE_MOT; k++) {
        u32 tpost = c->ntype[AZ_DENSE_SENS + AZ_DENSE_HID + k] & 7;
        for (u32 h = 0; h < AZ_DENSE_HID; h++) {
            u32 tpre = c->ntype[AZ_DENSE_SENS + h] & 7;
            i32 wv = (i32)c->theta[tpost * 8 + tpre] + synapse_noise(c, 2, k, h);
            c->w2[AZ_W2_IDX(k, h)] = (i8)az_clamp_i32(wv, -127, 127);
        }
        c->b2[k] = (i32)c->tbias[tpost] * 16;
    }
    /* shifts always end in 1..15: 0 means "not set" and gets the default */
    if (c->shift1 == 0) c->shift1 = 7;
    if (c->shift2 == 0) c->shift2 = 6;
    if (c->shift1 > 15) c->shift1 = 15;
    if (c->shift2 > 15) c->shift2 = 15;
}

