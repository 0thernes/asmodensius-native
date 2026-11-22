/* test_dense.c - the dense int8 network (kernels/az_dense.c) and its assembly
 * twins. The dense network was the engine's brain until ABI 4 gave every
 * creature a brain of its own (core/az_brain.c, tests/test_brain.c); it stays
 * the batch kernel of the GPU twin and of az_brainbench, so its reference and
 * its twins keep their test:
 *
 *   1. az_dense_step_c against an independent 64-bit oracle written from the
 *      formula with AZ_W1_IDX / AZ_W2_IDX, on hand-made cases with known
 *      answers and on > 1,000,000 random steps including extreme values.
 *   2. az_dense_step_avx2 and az_dense_step_vnni against the same oracle on
 *      the same steps, bit for bit, with guard bytes around every buffer.
 *   3. hid is updated in place; callee-saved registers survive the twins.
 *   7. beyond the contract: sensor bytes above 127.
 */
#include "az_test.h"
#include <math.h>
#include "../kernels/az_kernels.h"
#include "../kernels/az_dense.h"

typedef void (*BrainFn)(const AzDenseNet *c, const u8 *sens, u8 *hid,
                        const u8 *gain, const i8 *bias, i8 *motor);

#define BR_IN (AZ_DENSE_SENS + AZ_DENSE_HID)
#define B_MAX (1 << 20)

/* ---- test RNG (splitmix64): the test's own stream, not simulation state ---- */
static u64 t_state = 0x1234567ull;
static u64 t_rnd(void) { return az_test_rand64(&t_state); }
static u32 t_below(u32 n) { return (u32)(((t_rnd() >> 32) * (u64)n) >> 32); }
static i32 t_range(i32 lo, i32 hi) { return lo + (i32)t_below((u32)(hi - lo + 1)); }
static void t_bytes(void *p, size_t n)
{
    u8 *b = (u8 *)p;
    size_t i = 0;
    for (; i + 8 <= n; i += 8) { u64 v = t_rnd(); memcpy(b + i, &v, 8); }
    if (i < n) { u64 v = t_rnd(); memcpy(b + i, &v, n - i); }
}

/* ---- oracle: the formula of ARCHITECTURE section 9, in 64-bit arithmetic ---- */
static i64 o_clamp(i64 v, i64 lo, i64 hi) { return v < lo ? lo : (v > hi ? hi : v); }
static i64 o_floor_shift(i64 v, u32 s)       /* floor(v / 2^s) without relying on >> */
{
    i64 d = (i64)1 << s;
    i64 q = v / d;
    if ((v % d) != 0 && v < 0) q -= 1;
    return q;
}
static void oracle(const AzDenseNet *c, const u8 *sens, const u8 *hid_in, const u8 *gain,
                   const i8 *bias, u8 *hid_out, i8 *motor)
{
    u8 x[BR_IN];
    for (u32 j = 0; j < AZ_DENSE_SENS; j++) x[j] = sens[j];
    for (u32 j = 0; j < AZ_DENSE_HID; j++) x[AZ_DENSE_SENS + j] = hid_in[j];
    for (u32 h = 0; h < AZ_DENSE_HID; h++) {
        i64 acc = c->b1[h];
        for (u32 j = 0; j < BR_IN; j++) acc += (i64)c->w1[AZ_W1_IDX(h, j)] * (i64)x[j];
        i64 a = o_floor_shift(acc, c->shift1);
        a = o_floor_shift(a * (i64)gain[h & 7], 6) + 2 * (i64)bias[h & 7];
        hid_out[h] = (u8)o_clamp(a, 0, 127);
    }
    for (u32 k = 0; k < AZ_DENSE_MOT; k++) {
        i64 m = c->b2[k];
        for (u32 h = 0; h < AZ_DENSE_HID; h++) m += (i64)c->w2[AZ_W2_IDX(k, h)] * (i64)hid_out[h];
        motor[k] = (i8)o_clamp(o_floor_shift(m, c->shift2), -127, 127);
    }
}

/* ---- the implementations under test ---- */
static BrainFn fns[3];
static const char *fn_name[3];
static u32 n_fns = 0;
static u64 fn_steps[3];

static void add_fn(BrainFn f, const char *name) { fns[n_fns] = f; fn_name[n_fns] = name; n_fns++; }

/* Guarded work area: every buffer sits inside a block filled with a guard
 * pattern, at a varying misalignment, so a write outside the 32 hid bytes or
 * the 16 motor bytes, or any write to an input, is seen. */
#define GUARD 0xA5
typedef struct Arena {
    u8 sens[64 + AZ_DENSE_SENS + 64];
    u8 hid[64 + AZ_DENSE_HID + 64];
    u8 gain[64 + AZ_DENSE_MODG + 64];
    u8 bias[64 + AZ_DENSE_MODG + 64];
    u8 motor[64 + AZ_DENSE_MOT + 64];
} Arena;

static bool guard_ok(const u8 *blk, size_t total, size_t at, size_t len)
{
    for (size_t i = 0; i < total; i++)
        if ((i < at || i >= at + len) && blk[i] != GUARD) return false;
    return true;
}

static int g_reported = 0;

/* Run one step through every implementation and compare with the oracle.
 * Returns the oracle's hid' in hid_next (for chaining) and motor in mot_out. */
static void check_step(const AzDenseNet *c, const u8 *sens, const u8 *hid_in, const u8 *gain,
                       const i8 *bias, u8 *hid_next, i8 *mot_out, u32 mis)
{
    u8 eh[AZ_DENSE_HID];
    i8 em[AZ_DENSE_MOT];
    oracle(c, sens, hid_in, gain, bias, eh, em);
    for (u32 f = 0; f < n_fns; f++) {
        Arena a;
        memset(&a, GUARD, sizeof a);
        size_t o1 = 16 + ((mis + f) & 31), o2 = 16 + ((mis * 7 + 3) & 31), o3 = 16 + ((mis * 3 + 1) & 31);
        size_t o4 = 16 + ((mis * 5 + 2) & 31), o5 = 16 + ((mis * 11 + 5) & 31);
        memcpy(a.sens + o1, sens, AZ_DENSE_SENS);
        memcpy(a.hid + o2, hid_in, AZ_DENSE_HID);
        memcpy(a.gain + o3, gain, AZ_DENSE_MODG);
        memcpy(a.bias + o4, bias, AZ_DENSE_MODG);
        fns[f](c, a.sens + o1, a.hid + o2, a.gain + o3, (const i8 *)(a.bias + o4), (i8 *)(a.motor + o5));
        fn_steps[f]++;
        bool ok = memcmp(a.hid + o2, eh, AZ_DENSE_HID) == 0 && memcmp(a.motor + o5, em, AZ_DENSE_MOT) == 0;
        bool clean = guard_ok(a.hid, sizeof a.hid, o2, AZ_DENSE_HID) && guard_ok(a.motor, sizeof a.motor, o5, AZ_DENSE_MOT)
                  && guard_ok(a.sens, sizeof a.sens, o1, AZ_DENSE_SENS) && guard_ok(a.gain, sizeof a.gain, o3, AZ_DENSE_MODG)
                  && guard_ok(a.bias, sizeof a.bias, o4, AZ_DENSE_MODG)
                  && memcmp(a.sens + o1, sens, AZ_DENSE_SENS) == 0 && memcmp(a.gain + o3, gain, AZ_DENSE_MODG) == 0
                  && memcmp(a.bias + o4, bias, AZ_DENSE_MODG) == 0;
        CHECK(ok);
        CHECK(clean);
        if (!ok && g_reported < 5) {
            g_reported++;
            printf("  mismatch in %s (shift1 %u shift2 %u):\n", fn_name[f], c->shift1, c->shift2);
            for (u32 h = 0; h < AZ_DENSE_HID; h++)
                if (a.hid[o2 + h] != eh[h]) printf("    hid[%u] got %u want %u\n", h, a.hid[o2 + h], eh[h]);
            for (u32 k = 0; k < AZ_DENSE_MOT; k++)
                if ((i8)a.motor[o5 + k] != em[k]) printf("    motor[%u] got %d want %d\n", k, (i8)a.motor[o5 + k], em[k]);
        }
    }
    memcpy(hid_next, eh, AZ_DENSE_HID);
    memcpy(mot_out, em, AZ_DENSE_MOT);
}

/* the net under test lives at a varying 8-byte-aligned address: the twins
 * may not assume more than the alignment of the record itself (it holds
 * 64-bit fields since ABI 4, so 4-byte steps would be misaligned access,
 * which the UBSan build traps on) */
static _Alignas(64) u8 clade_mem[sizeof(AzDenseNet) + 64];
static AzDenseNet *clade_at(u32 k) { return (AzDenseNet *)(void *)(clade_mem + 8 * (k & 7)); }

static const u8 gain_one[AZ_DENSE_MODG] = { 64, 64, 64, 64, 64, 64, 64, 64 };
static const i8 bias_zero[AZ_DENSE_MODG] = { 0, 0, 0, 0, 0, 0, 0, 0 };

/* ---- 1. cases with answers known by hand ---- */
static void test_known(void)
{
    u8 sens[AZ_DENSE_SENS], hid[AZ_DENSE_HID], hn[AZ_DENSE_HID];
    i8 mot[AZ_DENSE_MOT];

    /* every layer-1 weight position: one weight, one input, one decoy input */
    for (u32 h = 0; h < AZ_DENSE_HID; h++) {
        for (u32 j = 0; j < BR_IN; j++) {
            AzDenseNet *c = clade_at(h + j);
            memset(c, 0, sizeof *c);
            c->w1[AZ_W1_IDX(h, j)] = 3;
            u8 x[BR_IN];
            memset(x, 0, sizeof x);
            x[j] = 41;
            x[(j + 1) % BR_IN] = 127;          /* decoy: its weight is zero */
            memcpy(sens, x, AZ_DENSE_SENS);
            memcpy(hid, x + AZ_DENSE_SENS, AZ_DENSE_HID);
            check_step(c, sens, hid, gain_one, bias_zero, hn, mot, h * 3 + j);
            bool ok = true;
            for (u32 q = 0; q < AZ_DENSE_HID; q++) ok = ok && hn[q] == (q == h ? 123 : 0);
            CHECK(ok);
        }
    }
    /* every layer-2 weight position, both signs; hid' is set through b1 */
    for (u32 k = 0; k < AZ_DENSE_MOT; k++) {
        for (u32 h = 0; h < AZ_DENSE_HID; h++) {
            for (i32 sgn = -1; sgn <= 1; sgn += 2) {
                AzDenseNet *c = clade_at(k + h);
                memset(c, 0, sizeof *c);
                c->b1[h] = 50;
                c->b1[(h + 1) % AZ_DENSE_HID] = 127;  /* decoy hidden unit */
                c->w2[AZ_W2_IDX(k, h)] = (i8)(2 * sgn);
                memset(sens, 0, sizeof sens);
                memset(hid, 0, sizeof hid);
                check_step(c, sens, hid, gain_one, bias_zero, hn, mot, k * 5 + h);
                bool ok = true;
                for (u32 q = 0; q < AZ_DENSE_MOT; q++) ok = ok && mot[q] == (q == k ? 100 * sgn : 0);
                CHECK(ok);
            }
        }
    }
    /* modulation: hidden h uses gain[h & 7] and bias[h & 7] */
    {
        AzDenseNet *c = clade_at(1);
        memset(c, 0, sizeof *c);
        c->shift1 = 3;
        for (u32 h = 0; h < AZ_DENSE_HID; h++) c->b1[h] = 1000 + (i32)h;      /* a = 125..128 */
        u8 gain[AZ_DENSE_MODG];
        i8 bias[AZ_DENSE_MODG];
        for (u32 i = 0; i < AZ_DENSE_MODG; i++) { gain[i] = (u8)(9 * i); bias[i] = (i8)(4 * (i32)i - 10); }
        memset(sens, 0, sizeof sens);
        memset(hid, 0, sizeof hid);
        check_step(c, sens, hid, gain, bias, hn, mot, 9);
        for (u32 h = 0; h < AZ_DENSE_HID; h++) {
            i32 a = (1000 + (i32)h) / 8;
            i32 e = (a * (9 * (i32)(h & 7))) / 64 + 2 * (4 * (i32)(h & 7) - 10);
            CHECK_EQ(hn[h], e < 0 ? 0 : (e > 127 ? 127 : e));
        }
    }
    /* clamps: hid to 0 and 127, motors to -127 (never -128) and +127 */
    {
        AzDenseNet *c = clade_at(2);
        memset(c, 0, sizeof *c);
        for (u32 h = 0; h < AZ_DENSE_HID; h++) c->b1[h] = (h & 1) ? B_MAX : -B_MAX;
        for (u32 k = 0; k < AZ_DENSE_MOT; k++) c->b2[k] = (k & 1) ? B_MAX : -B_MAX;
        memset(sens, 127, sizeof sens);
        memset(hid, 127, sizeof hid);
        check_step(c, sens, hid, gain_one, bias_zero, hn, mot, 4);
        for (u32 h = 0; h < AZ_DENSE_HID; h++) CHECK_EQ(hn[h], (h & 1) ? 127 : 0);
        for (u32 k = 0; k < AZ_DENSE_MOT; k++) CHECK_EQ(mot[k], (k & 1) ? 127 : -127);
        /* the most negative reachable sums: all weights -128, all inputs 127 */
        memset(c->w1, -128, sizeof c->w1);
        memset(c->w2, -128, sizeof c->w2);
        for (u32 h = 0; h < AZ_DENSE_HID; h++) c->b1[h] = B_MAX;       /* 1048576 - 1300480 < 0 */
        check_step(c, sens, hid, gain_one, bias_zero, hn, mot, 6);
        for (u32 h = 0; h < AZ_DENSE_HID; h++) CHECK_EQ(hn[h], 0);
        /* and brought back to exactly 1 by the bias: -1300480 + 1048576 = -251904 */
        const i8 bias_hi[AZ_DENSE_MODG] = { 127, 127, 127, 127, 127, 127, 127, 127 };
        c->shift1 = 10;                                           /* -251904 >> 10 = -246 */
        const u8 gain_g[AZ_DENSE_MODG] = { 66, 66, 66, 66, 66, 66, 66, 66 };  /* -246*66>>6 = -254 (floor of -253.7) */
        check_step(c, sens, hid, gain_g, bias_hi, hn, mot, 8);
        for (u32 h = 0; h < AZ_DENSE_HID; h++) CHECK_EQ(hn[h], 0);     /* -254 + 254 = 0 */
        c->shift1 = 11;                                           /* -251904 >> 11 = -123 */
        check_step(c, sens, hid, gain_g, bias_hi, hn, mot, 8);
        for (u32 h = 0; h < AZ_DENSE_HID; h++) CHECK_EQ(hn[h], 127);   /* -127 + 254 = 127 */
    }
}

/* ---- 3a. hid is updated in place: a rotation network ---- */
static void test_in_place(void)
{
    AzDenseNet *c = clade_at(3);
    memset(c, 0, sizeof *c);
    c->shift1 = 6;
    for (u32 h = 0; h < AZ_DENSE_HID; h++) c->w1[AZ_W1_IDX(h, AZ_DENSE_SENS + (h + 1) % AZ_DENSE_HID)] = 64;
    u8 sens[AZ_DENSE_SENS];
    memset(sens, 0, sizeof sens);
    for (u32 f = 0; f < n_fns; f++) {
        u8 hid[AZ_DENSE_HID];
        i8 mot[AZ_DENSE_MOT];
        for (u32 h = 0; h < AZ_DENSE_HID; h++) hid[h] = (u8)(5 + 3 * h);
        bool ok = true;
        for (u32 step = 1; step <= 70; step++) {
            fns[f](c, sens, hid, gain_one, bias_zero, mot);
            /* a unit written before its old value was read would break the rotation */
            for (u32 h = 0; h < AZ_DENSE_HID; h++) ok = ok && hid[h] == 5 + 3 * ((h + step) % AZ_DENSE_HID);
        }
        CHECK(ok);
    }
}

/* ---- 2. random steps, including extreme weights and inputs ---- */
static _Alignas(64) u8 clade_mem2[sizeof(AzDenseNet) + 64];
static u64 mix_h[3], mix_m[3];      /* outputs at the low clamp, inside, at the high clamp */

static i32 pick_b(void)
{
    switch (t_below(4)) {
    case 0: return B_MAX;
    case 1: return -B_MAX;
    case 2: return 0;
    default: return t_range(-B_MAX, B_MAX);
    }
}

static void test_random(u32 n_steps)
{
    AzDenseNet *c = clade_at(0);
    u8 sens[AZ_DENSE_SENS], hid[AZ_DENSE_HID], hn[AZ_DENSE_HID], gain[AZ_DENSE_MODG];
    i8 bias[AZ_DENSE_MODG], mot[AZ_DENSE_MOT];
    memset(hn, 0, sizeof hn);
    for (u32 i = 0; i < n_steps; i++) {
        u32 mode = i & 7;           /* 0..3 random, 4..5 calibrated, 6 extreme, 7 sparse */
        AzDenseNet *cc = c;
        if ((i & 15) == 0) {        /* a fresh network every 16 steps, at a new address */
            c = clade_at(i >> 4);
            t_bytes(c, sizeof *c);
            cc = c;
        } else {                    /* otherwise change 64 weights */
            for (u32 k = 0; k < 56; k++) c->w1[t_below(AZ_W1_LEN)] = (i8)t_rnd();
            for (u32 k = 0; k < 8; k++) c->w2[t_below(AZ_W2_LEN)] = (i8)t_rnd();
        }
        /* inputs: fresh, or the previous hid' fed back as in a running brain */
        t_bytes(sens, sizeof sens);
        for (u32 j = 0; j < AZ_DENSE_SENS; j++) sens[j] &= 127;
        if (i & 16) memcpy(hid, hn, AZ_DENSE_HID);
        else { t_bytes(hid, sizeof hid); for (u32 j = 0; j < AZ_DENSE_HID; j++) hid[j] &= 127; }
        t_bytes(gain, sizeof gain);
        t_bytes(bias, sizeof bias);
        for (u32 h = 0; h < AZ_DENSE_HID; h++) c->b1[h] = t_range(-B_MAX, B_MAX);
        for (u32 k = 0; k < AZ_DENSE_MOT; k++) c->b2[k] = t_range(-B_MAX, B_MAX);
        c->shift1 = (u8)t_below(16);
        c->shift2 = (u8)t_below(16);

        if (mode == 7) {            /* sparse: three inputs in four are zero */
            for (u32 j = 0; j < AZ_DENSE_SENS; j++) if (t_below(4)) sens[j] = 0;
            for (u32 j = 0; j < AZ_DENSE_HID; j++) if (t_below(4)) hid[j] = 0;
        } else if (mode == 6) {     /* extremes, on a second clade so `c` keeps evolving */
            cc = (AzDenseNet *)(void *)(clade_mem2 + 8 * (i & 7));
            u32 wk = t_below(4), xk = t_below(4);
            if (wk == 0) { memset(cc->w1, 127, sizeof cc->w1); memset(cc->w2, 127, sizeof cc->w2); }
            else if (wk == 1) { memset(cc->w1, -128, sizeof cc->w1); memset(cc->w2, -128, sizeof cc->w2); }
            else {
                t_bytes(cc->w1, sizeof cc->w1); t_bytes(cc->w2, sizeof cc->w2);
                if (wk == 2) {
                    for (u32 k = 0; k < AZ_W1_LEN; k++) cc->w1[k] = (cc->w1[k] & 1) ? 127 : -128;
                    for (u32 k = 0; k < AZ_W2_LEN; k++) cc->w2[k] = (cc->w2[k] & 1) ? 127 : -128;
                }
            }
            if (xk == 0) { memset(sens, 127, sizeof sens); memset(hid, 127, sizeof hid); }
            else if (xk == 1) { memset(sens, 0, sizeof sens); memset(hid, 0, sizeof hid); }
            else if (xk == 2) {
                for (u32 j = 0; j < AZ_DENSE_SENS; j++) sens[j] = (sens[j] & 1) ? 127 : 0;
                for (u32 j = 0; j < AZ_DENSE_HID; j++) hid[j] = (hid[j] & 1) ? 127 : 0;
            }
            for (u32 h = 0; h < AZ_DENSE_HID; h++) cc->b1[h] = pick_b();
            for (u32 k = 0; k < AZ_DENSE_MOT; k++) cc->b2[k] = pick_b();
            static const u8 sh[4] = { 0, 1, 15, 7 };
            cc->shift1 = sh[t_below(4)];
            cc->shift2 = sh[t_below(4)];
            for (u32 q = 0; q < AZ_DENSE_MODG; q++) {
                u32 gk = t_below(4), bk = t_below(4);
                if (gk == 0) gain[q] = 0; else if (gk == 1) gain[q] = 255; else if (gk == 2) gain[q] = 64;
                if (bk == 0) bias[q] = -128; else if (bk == 1) bias[q] = 127; else if (bk == 2) bias[q] = 0;
            }
        } else if (mode >= 4) {
            /* calibrated: biases chosen from the exact sums so that most
             * outputs land inside the clamps, where one wrong product shows */
            u8 x[BR_IN], eh[AZ_DENSE_HID];
            i8 em[AZ_DENSE_MOT];
            memcpy(x, sens, AZ_DENSE_SENS);
            memcpy(x + AZ_DENSE_SENS, hid, AZ_DENSE_HID);
            c->shift1 = (u8)t_below(5);
            c->shift2 = (u8)t_below(5);
            for (u32 q = 0; q < AZ_DENSE_MODG; q++) { gain[q] = (u8)t_range(56, 72); bias[q] = (i8)t_range(-6, 6); }
            for (u32 h = 0; h < AZ_DENSE_HID; h++) {
                i64 s = 0;
                for (u32 j = 0; j < BR_IN; j++) s += (i64)c->w1[AZ_W1_IDX(h, j)] * (i64)x[j];
                /* a product, not a shift: shifting a negative value left is undefined (UBSan trap build) */
                i64 b = -s + ((i64)t_range(-16, 143) * ((i64)1 << c->shift1)) + (i64)t_below(1u << c->shift1);
                c->b1[h] = (i32)o_clamp(b, -B_MAX, B_MAX);
            }
            oracle(c, sens, hid, gain, bias, eh, em);
            for (u32 k = 0; k < AZ_DENSE_MOT; k++) {
                i64 s = 0;
                for (u32 h = 0; h < AZ_DENSE_HID; h++) s += (i64)c->w2[AZ_W2_IDX(k, h)] * (i64)eh[h];
                i64 b = -s + ((i64)t_range(-140, 140) * ((i64)1 << c->shift2)) + (i64)t_below(1u << c->shift2);
                c->b2[k] = (i32)o_clamp(b, -B_MAX, B_MAX);
            }
        }
        check_step(cc, sens, hid, gain, bias, hn, mot, i);
        for (u32 h = 0; h < AZ_DENSE_HID; h++) mix_h[hn[h] == 0 ? 0 : (hn[h] == 127 ? 2 : 1)]++;
        for (u32 k = 0; k < AZ_DENSE_MOT; k++) mix_m[mot[k] == -127 ? 0 : (mot[k] == 127 ? 2 : 1)]++;
    }
}

/* negative control: an implementation with two weights swapped must be caught */
static void broken_step(const AzDenseNet *c, const u8 *sens, u8 *hid, const u8 *gain, const i8 *bias, i8 *motor)
{
    static AzDenseNet tmp;
    memcpy(&tmp, c, sizeof tmp);
    i8 t = tmp.w1[AZ_W1_IDX(5, 9)];
    tmp.w1[AZ_W1_IDX(5, 9)] = tmp.w1[AZ_W1_IDX(5, 10)];
    tmp.w1[AZ_W1_IDX(5, 10)] = t;
    az_dense_step_c(&tmp, sens, hid, gain, bias, motor);
}

static void test_control(void)
{
    u32 caught = 0;
    AzDenseNet *c = clade_at(5);
    for (u32 i = 0; i < 2000; i++) {
        u8 sens[AZ_DENSE_SENS], hid[AZ_DENSE_HID], eh[AZ_DENSE_HID], gh[AZ_DENSE_HID];
        i8 em[AZ_DENSE_MOT], gm[AZ_DENSE_MOT];
        t_bytes(c, sizeof *c);
        for (u32 h = 0; h < AZ_DENSE_HID; h++) c->b1[h] = t_range(-4000, 4000);
        for (u32 k = 0; k < AZ_DENSE_MOT; k++) c->b2[k] = t_range(-4000, 4000);
        c->shift1 = 7; c->shift2 = 7;
        t_bytes(sens, sizeof sens);
        t_bytes(hid, sizeof hid);
        for (u32 j = 0; j < AZ_DENSE_SENS; j++) sens[j] &= 127;
        for (u32 j = 0; j < AZ_DENSE_HID; j++) hid[j] &= 127;
        oracle(c, sens, hid, gain_one, bias_zero, eh, em);
        memcpy(gh, hid, AZ_DENSE_HID);
        broken_step(c, sens, gh, gain_one, bias_zero, gm);
        if (memcmp(gh, eh, AZ_DENSE_HID) != 0 || memcmp(gm, em, AZ_DENSE_MOT) != 0) caught++;
    }
    printf("negative control (two weights swapped): caught in %u of 2000 steps\n", caught);
    CHECK(caught > 200);
}

/* ---- 3b. callee-saved registers survive (Windows x64: rbx rbp rsi rdi r12..r15, xmm6..xmm15) ----
 * brain_abi_check loads sentinels into all of them, calls fn(a[0..5]) and
 * returns a mask of the registers that came back changed:
 *   bits 0..7 rbx rbp rsi rdi r12 r13 r14 r15, bits 8..17 xmm6..xmm15.
 * brain_abi_bad is the control: it destroys r12 and xmm9 (mask 0x810). */
u32 brain_abi_check(BrainFn fn, const void *const *a, const u64 *gpr, const u8 *xmm);
void brain_abi_bad(const AzDenseNet *c, const u8 *sens, u8 *hid, const u8 *gain, const i8 *bias, i8 *motor);

#define X_SAVE(n, off) "    movups [rsp+" #off "], xmm" #n "\n"
#define X_LOAD(n, off) "    movups xmm" #n ", [r9+" #off "]\n"
#define X_REST(n, off) "    movups xmm" #n ", [rsp+" #off "]\n"
#define X_CHK(n, off, bit) "    movups xmm0, [rdx+" #off "]\n    pcmpeqb xmm0, xmm" #n "\n    pmovmskb ecx, xmm0\n" \
                           "    cmp ecx, 0xFFFF\n    je 1f\n    or eax, " #bit "\n1:\n"
#define G_CHK(reg, off, bit) "    cmp " #reg ", [rdx+" #off "]\n    je 1f\n    or eax, " #bit "\n1:\n"

__asm__(
    "    .text\n"
    "    .intel_syntax noprefix\n"
    "    .p2align 4\n"
    "    .globl brain_abi_check\n"
    "brain_abi_check:\n"
    "    push rbx\n    push rbp\n    push rsi\n    push rdi\n"
    "    push r12\n    push r13\n    push r14\n    push r15\n"
    "    sub rsp, 232\n"                      /* 8 pushes + 232 + return address: rsp is 16-aligned */
    X_SAVE(6, 48) X_SAVE(7, 64) X_SAVE(8, 80) X_SAVE(9, 96) X_SAVE(10, 112)
    X_SAVE(11, 128) X_SAVE(12, 144) X_SAVE(13, 160) X_SAVE(14, 176) X_SAVE(15, 192)
    "    mov [rsp+208], r8\n"
    "    mov [rsp+216], r9\n"
    "    mov r10, rcx\n"
    "    mov r11, rdx\n"
    "    mov rbx, [r8]\n    mov rbp, [r8+8]\n    mov rsi, [r8+16]\n    mov rdi, [r8+24]\n"
    "    mov r12, [r8+32]\n    mov r13, [r8+40]\n    mov r14, [r8+48]\n    mov r15, [r8+56]\n"
    X_LOAD(6, 0) X_LOAD(7, 16) X_LOAD(8, 32) X_LOAD(9, 48) X_LOAD(10, 64)
    X_LOAD(11, 80) X_LOAD(12, 96) X_LOAD(13, 112) X_LOAD(14, 128) X_LOAD(15, 144)
    "    mov rax, [r11+32]\n    mov [rsp+32], rax\n"
    "    mov rax, [r11+40]\n    mov [rsp+40], rax\n"
    "    mov rcx, [r11]\n    mov rdx, [r11+8]\n    mov r8, [r11+16]\n    mov r9, [r11+24]\n"
    "    call r10\n"
    "    xor eax, eax\n"
    "    mov rdx, [rsp+208]\n"
    G_CHK(rbx, 0, 1) G_CHK(rbp, 8, 2) G_CHK(rsi, 16, 4) G_CHK(rdi, 24, 8)
    G_CHK(r12, 32, 16) G_CHK(r13, 40, 32) G_CHK(r14, 48, 64) G_CHK(r15, 56, 128)
    "    mov rdx, [rsp+216]\n"
    X_CHK(6, 0, 0x100) X_CHK(7, 16, 0x200) X_CHK(8, 32, 0x400) X_CHK(9, 48, 0x800) X_CHK(10, 64, 0x1000)
    X_CHK(11, 80, 0x2000) X_CHK(12, 96, 0x4000) X_CHK(13, 112, 0x8000) X_CHK(14, 128, 0x10000) X_CHK(15, 144, 0x20000)
    X_REST(6, 48) X_REST(7, 64) X_REST(8, 80) X_REST(9, 96) X_REST(10, 112)
    X_REST(11, 128) X_REST(12, 144) X_REST(13, 160) X_REST(14, 176) X_REST(15, 192)
    "    add rsp, 232\n"
    "    pop r15\n    pop r14\n    pop r13\n    pop r12\n"
    "    pop rdi\n    pop rsi\n    pop rbp\n    pop rbx\n"
    "    ret\n"
    "    .p2align 4\n"
    "    .globl brain_abi_bad\n"
    "brain_abi_bad:\n"
    "    xor r12d, r12d\n"
    "    pxor xmm9, xmm9\n"
    "    ret\n"
    "    .att_syntax prefix\n"
);

static void test_abi(void)
{
    static _Alignas(16) u64 gpr[8];
    static _Alignas(16) u8 xmm[160];
    for (u32 i = 0; i < 8; i++) gpr[i] = 0x1111111111111100ull * (i + 1) + i + 1;
    for (u32 i = 0; i < 160; i++) xmm[i] = (u8)(0xA0 + (i >> 4) * 7 + (i & 15));
    AzDenseNet *c = clade_at(7);
    u8 sens[AZ_DENSE_SENS], hid[AZ_DENSE_HID], gain[AZ_DENSE_MODG];
    i8 bias[AZ_DENSE_MODG], mot[AZ_DENSE_MOT];
    t_bytes(c, sizeof *c);
    for (u32 h = 0; h < AZ_DENSE_HID; h++) c->b1[h] = t_range(-B_MAX, B_MAX);
    for (u32 k = 0; k < AZ_DENSE_MOT; k++) c->b2[k] = t_range(-B_MAX, B_MAX);
    c->shift1 = 7; c->shift2 = 6;
    t_bytes(sens, sizeof sens);
    t_bytes(hid, sizeof hid);
    for (u32 j = 0; j < AZ_DENSE_SENS; j++) sens[j] &= 127;
    for (u32 j = 0; j < AZ_DENSE_HID; j++) hid[j] &= 127;
    t_bytes(gain, sizeof gain);
    t_bytes(bias, sizeof bias);
    const void *args[6] = { c, sens, hid, gain, bias, mot };
    for (u32 f = 0; f < n_fns; f++) {
        u8 h0[AZ_DENSE_HID], eh[AZ_DENSE_HID];
        i8 em[AZ_DENSE_MOT];
        memcpy(h0, hid, AZ_DENSE_HID);
        oracle(c, sens, h0, gain, bias, eh, em);
        u32 mask = brain_abi_check(fns[f], args, gpr, xmm);
        printf("abi: %-20s changed-register mask = 0x%05x\n", fn_name[f], mask);
        CHECK_EQ(mask, 0);
        /* and the call through the checker still computed the right step */
        CHECK(memcmp(hid, eh, AZ_DENSE_HID) == 0 && memcmp(mot, em, AZ_DENSE_MOT) == 0);
        memcpy(hid, h0, AZ_DENSE_HID);
    }
    u32 bad = brain_abi_check(brain_abi_bad, args, gpr, xmm);
    printf("abi: %-20s changed-register mask = 0x%05x (control, expected 0x00810)\n", "brain_abi_bad", bad);
    CHECK_EQ(bad, 0x810);
}

/* ---- 7. beyond the contract: sensor bytes above 127 ----
 * az_sense belongs to another module. If it ever produced a byte above 127,
 * the C reference and the VNNI twin (true u8 x s8 products) would still
 * agree; the AVX2 twin would not, because vpmaddubsw saturates. This is
 * measured here so that the difference is a known fact, not a surprise. */
static void test_wide_sensors(void)
{
    u32 n_keep = n_fns, avx2_diff = 0;
    BrainFn avx2 = NULL;
    for (u32 f = 0; f < n_fns; f++) if (!strcmp(fn_name[f], "az_dense_step_avx2")) avx2 = fns[f];
    /* compare only the C reference and the VNNI twin with the oracle */
    BrainFn keep_fn[3]; const char *keep_name[3];
    memcpy(keep_fn, fns, sizeof fns); memcpy(keep_name, fn_name, sizeof fn_name);
    n_fns = 0;
    for (u32 f = 0; f < n_keep; f++) if (keep_fn[f] != avx2) add_fn(keep_fn[f], keep_name[f]);
    AzDenseNet *c = clade_at(9);
    for (u32 i = 0; i < 50000; i++) {
        u8 sens[AZ_DENSE_SENS], hid[AZ_DENSE_HID], hn[AZ_DENSE_HID], gain[AZ_DENSE_MODG];
        i8 bias[AZ_DENSE_MODG], mot[AZ_DENSE_MOT];
        if ((i & 15) == 0) t_bytes(c, sizeof *c);
        for (u32 h = 0; h < AZ_DENSE_HID; h++) c->b1[h] = t_range(-B_MAX, B_MAX);
        for (u32 k = 0; k < AZ_DENSE_MOT; k++) c->b2[k] = t_range(-B_MAX, B_MAX);
        c->shift1 = (u8)t_below(16); c->shift2 = (u8)t_below(16);
        t_bytes(sens, sizeof sens);                 /* 0..255 */
        t_bytes(hid, sizeof hid);
        for (u32 j = 0; j < AZ_DENSE_HID; j++) hid[j] &= 127;
        t_bytes(gain, sizeof gain);
        t_bytes(bias, sizeof bias);
        check_step(c, sens, hid, gain, bias, hn, mot, i);
        if (avx2) {
            u8 h2[AZ_DENSE_HID]; i8 m2[AZ_DENSE_MOT];
            memcpy(h2, hid, AZ_DENSE_HID);
            avx2(c, sens, h2, gain, bias, m2);
            avx2_diff += memcmp(h2, hn, AZ_DENSE_HID) != 0 || memcmp(m2, mot, AZ_DENSE_MOT) != 0;
        }
    }
    if (avx2) printf("sensor bytes up to 255 (outside the contract): C and VNNI agree with the oracle; AVX2 differs in %u of 50000 steps\n", avx2_diff);
    n_fns = 0;
    for (u32 f = 0; f < n_keep; f++) add_fn(keep_fn[f], keep_name[f]);
}

int main(void)
{
    add_fn(az_dense_step_c, "az_dense_step_c");
#ifdef AZ_HAVE_DENSE_VNNI
    {
        u32 f = az_cpu_features();
        if (f & AZ_CPU_AVX2) add_fn(az_dense_step_avx2, "az_dense_step_avx2");
        else printf("note: no AVX2 on this CPU, az_dense_step_avx2 not tested\n");
        if (az_twin_ok_brain_vnni(f)) add_fn(az_dense_step_vnni, "az_dense_step_vnni");
        else printf("note: no AVX-VNNI on this CPU, az_dense_step_vnni not tested\n");
    }
#else
    printf("note: built without the assembly twins, only the C reference is tested\n");
#endif
    test_known();
    printf("known cases: %d checks so far, %d failures\n", az_checks, az_failures);
    test_in_place();
    test_abi();
    test_control();
    test_random(1000000);
    {
        u64 before[3];
        memcpy(before, fn_steps, sizeof before);
        test_wide_sensors();
        memcpy(fn_steps, before, sizeof before);    /* those steps are not contract steps */
    }
    for (u32 f = 0; f < n_fns; f++)
        printf("%-20s %" PRIu64 " steps compared with the oracle\n", fn_name[f], fn_steps[f]);
    printf("oracle output mix: hid %" PRIu64 " at 0, %" PRIu64 " inside, %" PRIu64 " at 127; motor %" PRIu64
           " at -127, %" PRIu64 " inside, %" PRIu64 " at 127\n",
           mix_h[0], mix_h[1], mix_h[2], mix_m[0], mix_m[1], mix_m[2]);
    CHECK(mix_h[0] > 1000000 && mix_h[1] > 1000000 && mix_h[2] > 1000000);
    CHECK(mix_m[0] > 500000 && mix_m[1] > 500000 && mix_m[2] > 500000);
    return az_test_done();
}
