/* test_brain.c - the per-creature brain: a sparse int8 network of any size
 * in the creature's own block (core/az_brain.c), its assembly twins
 * (kernels/az_brain_vnni.asm), founders, inheritance and lineages.
 *
 *   1. az_brain_step_c against an independent 64-bit oracle written from the
 *      layout and the formula in az_abi.h, on cases with answers known by
 *      hand and on 120,000 random brains of every size (0 hidden neurons
 *      included, extreme weights, biases that wrap), three steps each.
 *   2. az_brain_step_vnni and az_brain_step_avx2 on the same steps, bit for
 *      bit, with guard bytes around the mind and the motors.
 *   3. Callee-saved registers survive the twins.
 *   4. az_brain_ok: what it must refuse; whatever it accepts keeps the three
 *      implementations inside their buffers (the gate a snapshot goes through).
 *   5. Founder brains: a function of the world, the species and the BRAIN_*
 *      parameters, the same at any tick.
 *   6. Inheritance: a copy under the mutation lesion; with mutation, weights
 *      move and tiles and blocks of neurons come and go, one edit per birth,
 *      at rates the mother's own traits scale. Lineages: a child whose brain
 *      differs in structure founds one; a full table refuses and says so.
 *   7. A running world gives the same hash with the reference and each twin.
 */
#include "az_test.h"
#include <math.h>
#include "../kernels/az_kernels.h"

typedef void (*BrainFn)(const AzBrain *b, u8 *mind, i8 *motor);

#define SG (AZ_SENS / 4u)       /* sensor groups */
#define MB (AZ_MOT / 8u)        /* motor blocks  */

/* ---- test RNG (splitmix64): the test's own stream, not simulation state ---- */
static u64 t_state = 0x1234567ull;
static u64 t_rnd(void) { return az_test_rand64(&t_state); }
static u32 t_below(u32 n) { return (u32)(((t_rnd() >> 32) * (u64)n) >> 32); }
static void t_bytes(void *p, size_t n)
{
    u8 *b = (u8 *)p;
    for (size_t i = 0; i < n; i++) b[i] = (u8)(t_rnd() >> 24);
}

static u32 pad8u(u32 n) { return (n + 7u) & ~7u; }

/* ---- oracle: the text of AzBrain in az_abi.h, in 64-bit arithmetic ---- */
static i64 wrap32(i64 v) { return (i64)(i32)(u32)(u64)v; }
static i64 o_clamp(i64 v, i64 lo, i64 hi) { return v < lo ? lo : (v > hi ? hi : v); }
static i64 o_floor_shift(i64 v, u32 s)       /* floor(v / 2^s) without relying on >> */
{
    i64 d = (i64)1 << s;
    i64 q = v / d;
    if (v % d != 0 && v < 0) q--;
    return q;
}

static u32 rd32u(const u8 *p) { u32 v; memcpy(&v, p, 4); return v; }
static i32 rd32s(const u8 *p) { i32 v; memcpy(&v, p, 4); return v; }
static u32 rd16u(const u8 *p) { u16 v; memcpy(&v, p, 2); return v; }

/* mind_in = sens ++ hid; writes the next hidden state and the motors */
static void oracle(const u8 *br, const u8 *mind_in, u8 *hid2, i8 *motor)
{
    const u32 n_hb = rd16u(br + 0), n_mb = rd16u(br + 2);
    const u32 shift1 = br[12], shift2 = br[13];
    const u8 *gain = br + 16;
    const i8 *bias = (const i8 *)br + 24;
    const u8 *b1 = br + rd32u(br + 32), *b2 = br + rd32u(br + 36), *c1 = br + rd32u(br + 40), *c2 = br + rd32u(br + 44);
    const u8 *in1 = br + rd32u(br + 48), *in2 = br + rd32u(br + 52);
    const i8 *w1 = (const i8 *)br + rd32u(br + 56), *w2 = (const i8 *)br + rd32u(br + 60);
    u32 t = 0;
    for (u32 hb = 0; hb < n_hb; hb++) {
        i64 acc[8];
        for (u32 k = 0; k < 8; k++) acc[k] = rd32s(b1 + 4 * (hb * 8 + k));
        for (u32 q = rd16u(c1 + 2 * hb); q; q--, t++) {
            const u32 g = rd16u(in1 + 2 * t);
            for (u32 k = 0; k < 8; k++)
                for (u32 j = 0; j < 4; j++) acc[k] += (i64)w1[(size_t)t * 32 + k * 4 + j] * (i64)mind_in[4 * g + j];
        }
        for (u32 k = 0; k < 8; k++) {
            i64 a = o_floor_shift(wrap32(acc[k]), shift1);
            a = o_floor_shift(wrap32(a * (i64)gain[k]), 6);
            a = wrap32(a + 2 * (i64)bias[k]);
            hid2[hb * 8 + k] = (u8)o_clamp(a, 0, 127);
        }
    }
    t = 0;
    for (u32 mb = 0; mb < n_mb; mb++) {
        i64 acc[8];
        for (u32 k = 0; k < 8; k++) acc[k] = rd32s(b2 + 4 * (mb * 8 + k));
        for (u32 q = rd16u(c2 + 2 * mb); q; q--, t++) {
            const u32 g = rd16u(in2 + 2 * t);
            for (u32 k = 0; k < 8; k++)
                for (u32 j = 0; j < 4; j++) acc[k] += (i64)w2[(size_t)t * 32 + k * 4 + j] * (i64)hid2[4 * g + j];
        }
        for (u32 k = 0; k < 8; k++)
            motor[mb * 8 + k] = (i8)o_clamp(o_floor_shift(wrap32(acc[k]), shift2), -127, 127);
    }
}

/* ---- the implementations under test ---- */
static BrainFn fns[3];
static const char *fn_name[3];
static u32 n_fns = 0;
static u64 fn_steps[3];
static void add_fn(BrainFn f, const char *name) { fns[n_fns] = f; fn_name[n_fns] = name; n_fns++; }

/* ---- building a brain by the published layout ---- */
#define MAX_HB 48u
#define BRAIN_MAX (64u + 32u * MAX_HB + 4u * AZ_MOT + 2u * MAX_HB + 16u + 2u * 34u * (SG + 2u * MAX_HB) * (MAX_HB + MB) + 64u)

typedef struct Plan { u32 n_hb, n_t1, n_t2, bytes, mind; } Plan;

static _Alignas(64) u8 brain_mem[BRAIN_MAX + 128];
static u8 group_on[(MAX_HB + MB) * (SG + 2u * MAX_HB)];

/* A random brain at brain_mem + at: n_hb hidden blocks, every possible tile
 * present with chance `dens` of 256. kind: 0 ordinary values, 1 extremes. */
static u8 *make_brain(u32 n_hb, u32 dens, u32 kind, u32 at, Plan *pl)
{
    const u32 g1 = SG + 2u * n_hb, g2 = 2u * n_hb;
    u32 t1 = 0, t2 = 0;
    for (u32 ob = 0; ob < n_hb; ob++)
        for (u32 g = 0; g < g1; g++) t1 += (group_on[ob * g1 + g] = (u8)(t_below(256) < dens));
    u8 *on2 = group_on + n_hb * g1;
    for (u32 ob = 0; ob < MB; ob++)
        for (u32 g = 0; g < g2; g++) t2 += (on2[ob * g2 + g] = (u8)(t_below(256) < dens));
    u8 *br = brain_mem + at;
    AzBrain h;
    memset(&h, 0, sizeof h);
    h.n_hb = (u16)n_hb; h.n_mb = (u16)MB; h.n_t1 = t1; h.n_t2 = t2; h.n_sg = (u8)SG;
    h.shift1 = (u8)t_below(16); h.shift2 = (u8)t_below(16);
    for (u32 k = 0; k < AZ_MODG; k++) {
        h.gain[k] = kind ? (u8)(t_below(4) ? t_rnd() >> 13 : 255) : (u8)(32 + t_below(64));
        h.bias[k] = kind ? (i8)(t_rnd() >> 17) : (i8)((i32)t_below(33) - 16);
    }
    h.off_b1 = 64;
    h.off_b2 = h.off_b1 + 32u * n_hb;
    h.off_c1 = h.off_b2 + 4u * AZ_MOT;
    h.off_c2 = h.off_c1 + pad8u(2u * n_hb);
    h.off_in1 = h.off_c2 + pad8u(2u * MB);
    h.off_in2 = h.off_in1 + pad8u(2u * t1);
    h.off_w1 = h.off_in2 + pad8u(2u * t2);
    h.off_w2 = h.off_w1 + 32u * t1;
    pl->n_hb = n_hb; pl->n_t1 = t1; pl->n_t2 = t2;
    pl->bytes = h.off_w2 + 32u * t2;
    pl->mind = AZ_SENS + 16u * n_hb + sizeof(AzSenseMemory);
    memset(br, 0, pl->bytes);
    memcpy(br, &h, sizeof h);
    /* biases: ordinary ones keep the sums in range; the extreme kind uses every i32 so that sums wrap */
    for (u32 i = 0; i < 8u * n_hb + AZ_MOT; i++) {
        const i32 v = kind && t_below(3) == 0 ? (i32)(u32)t_rnd() : (i32)t_below(40001) - 20000;
        memcpy(br + h.off_b1 + 4u * i, &v, 4);
    }
    u32 n = 0;
    for (u32 ob = 0; ob < n_hb; ob++) {
        u16 c = 0;
        for (u32 g = 0; g < g1; g++)
            if (group_on[ob * g1 + g]) { const u16 gg = (u16)g; memcpy(br + h.off_in1 + 2u * n, &gg, 2); n++; c++; }
        memcpy(br + h.off_c1 + 2u * ob, &c, 2);
    }
    n = 0;
    for (u32 ob = 0; ob < MB; ob++) {
        u16 c = 0;
        for (u32 g = 0; g < g2; g++)
            if (on2[ob * g2 + g]) { const u16 gg = (u16)g; memcpy(br + h.off_in2 + 2u * n, &gg, 2); n++; c++; }
        memcpy(br + h.off_c2 + 2u * ob, &c, 2);
    }
    i8 *wt = (i8 *)br + h.off_w1;
    const u32 nw = 32u * (t1 + t2);
    if (kind == 0) for (u32 i = 0; i < nw; i++) wt[i] = (i8)((i32)t_below(61) - 30);
    else for (u32 i = 0; i < nw; i++) { const u32 r = t_below(8); wt[i] = (i8)(r == 0 ? -128 : r == 1 ? 127 : r == 2 ? -127 : (i32)t_below(256) - 128); }
    return br;
}

/* ---- guarded step: every implementation against the oracle ---- */
#define GUARD 0xA5
#define MIND_MAX (AZ_SENS + 16u * MAX_HB + sizeof(AzSenseMemory))
static u8 mind_area[64 + MIND_MAX + 64];
static u8 motor_area[64 + AZ_MOT + 64];
static int g_reported = 0;
static u64 mix_h[3], mix_m[3];      /* outputs at the low clamp, inside, at the high clamp */

static bool guards_ok(const u8 *area, size_t total, size_t at, size_t len)
{
    for (size_t i = 0; i < at; i++) if (area[i] != GUARD) return false;
    for (size_t i = at + len; i < total; i++) if (area[i] != GUARD) return false;
    return true;
}

/* mind0 = sens ++ hid (++ anything): one step of every implementation; on
 * return mind0's hid2 part holds the oracle's next hidden state */
static void check_step(const u8 *br, const Plan *pl, u8 *mind0, bool count)
{
    static u8 want_h[8 * MAX_HB];
    i8 want_m[AZ_MOT];
    const u32 nh = 8u * pl->n_hb;
    oracle(br, mind0, want_h, want_m);
    if (count) {
        for (u32 i = 0; i < nh; i++) mix_h[want_h[i] == 0 ? 0 : want_h[i] == 127 ? 2 : 1]++;
        for (u32 i = 0; i < AZ_MOT; i++) mix_m[want_m[i] == -127 ? 0 : want_m[i] == 127 ? 2 : 1]++;
    }
    for (u32 f = 0; f < n_fns; f++) {
        const u32 mis = 1u + (u32)(t_rnd() & 15u), mis2 = 1u + (u32)(t_rnd() & 15u);
        memset(mind_area, GUARD, sizeof mind_area);
        memset(motor_area, GUARD, sizeof motor_area);
        u8 *mind = mind_area + 24 + mis;
        i8 *motor = (i8 *)motor_area + 24 + mis2;
        memcpy(mind, mind0, AZ_SENS + nh);
        t_bytes(mind + AZ_SENS + nh, nh);                  /* hid2 holds anything before a step */
        fns[f]((const AzBrain *)(const void *)br, mind, motor);
        const bool ok = memcmp(mind, mind0, AZ_SENS + nh) == 0 &&            /* inputs untouched */
                        memcmp(mind + AZ_SENS + nh, want_h, nh) == 0 &&
                        memcmp(motor, want_m, AZ_MOT) == 0 &&
                        guards_ok(mind_area, sizeof mind_area, 24 + mis, pl->mind) &&
                        guards_ok(motor_area, sizeof motor_area, 24 + mis2, AZ_MOT);
        if (!ok && g_reported < 5) {
            g_reported++;
            printf("  mismatch: %s, %u hidden blocks, tiles %u + %u, shifts %u %u\n", fn_name[f], pl->n_hb, pl->n_t1,
                   pl->n_t2, br[12], br[13]);
        }
        CHECK(ok);
        fn_steps[f]++;
    }
    memcpy(mind0 + AZ_SENS + nh, want_h, nh);
}

/* ---- 1. cases with answers known by hand ---- */
static void test_known(void)
{
    Plan pl;
    static u8 mind[MIND_MAX];
    /* no hidden neurons at all: the motors are their biases, shifted and clamped */
    u8 *br = make_brain(0, 0, 0, 8, &pl);
    CHECK_EQ(pl.bytes, (u32)az_brain_bytes(0, 0, 0));
    CHECK_EQ(pl.mind, az_mind_bytes(0));
    CHECK(az_brain_ok((const AzBrain *)(const void *)br, pl.bytes, pl.mind));
    br[13] = 2;                                           /* shift2 */
    for (u32 k = 0; k < AZ_MOT; k++) { const i32 v = (i32)k * 64 - 500; memcpy(br + rd32u(br + 36) + 4 * k, &v, 4); }
    memset(mind, 0, sizeof mind);
    for (u32 f = 0; f < n_fns; f++) {
        i8 motor[AZ_MOT];
        fns[f]((const AzBrain *)(const void *)br, mind, motor);
        u32 bad = 0;
        /* floor((64 k - 500) / 4) = 16 k - 125, clamped to -127..127 */
        for (u32 k = 0; k < AZ_MOT; k++) { const i32 e = (i32)k * 16 - 125; bad += motor[k] != (e > 127 ? 127 : e); }
        CHECK_EQ(bad, 0);
    }
    check_step(br, &pl, mind, false);

    /* one block of neurons: hidden k copies sensor k (k < 4) through an
     * identity tile, plus its bias; a motor reads twice hidden unit k */
    br = make_brain(1, 0, 0, 16, &pl);
    AzBrain h;
    memcpy(&h, br, sizeof h);
    h.n_t1 = 1; h.n_t2 = 1; h.shift1 = 0; h.shift2 = 0;
    for (u32 k = 0; k < 8; k++) { h.gain[k] = 64; h.bias[k] = 0; }
    h.off_in2 = h.off_in1 + 8; h.off_w1 = h.off_in2 + 8; h.off_w2 = h.off_w1 + 32;
    pl.n_t1 = 1; pl.n_t2 = 1; pl.bytes = h.off_w2 + 32;
    memset(br, 0, pl.bytes);
    memcpy(br, &h, sizeof h);
    { const u16 one = 1, zero = 0; memcpy(br + h.off_c1, &one, 2); memcpy(br + h.off_c2, &one, 2);
      memcpy(br + h.off_in1, &zero, 2); memcpy(br + h.off_in2, &zero, 2); }
    for (u32 k = 0; k < 4; k++) { br[h.off_w1 + k * 4 + k] = 1; br[h.off_w2 + k * 4 + k] = 2; }
    { const i32 b3 = -5, b0 = 100; memcpy(br + h.off_b1 + 12, &b3, 4); memcpy(br + h.off_b1, &b0, 4); }
    CHECK_EQ(pl.bytes, (u32)az_brain_bytes(1, 1, 1));
    CHECK(az_brain_ok((const AzBrain *)(const void *)br, pl.bytes, pl.mind));
    memset(mind, 0, sizeof mind);
    mind[0] = 10; mind[1] = 20; mind[2] = 30; mind[3] = 40;
    for (u32 f = 0; f < n_fns; f++) {
        static u8 m2[MIND_MAX];
        i8 motor[AZ_MOT];
        memcpy(m2, mind, sizeof m2);
        fns[f]((const AzBrain *)(const void *)br, m2, motor);
        const u8 *h2 = m2 + AZ_SENS + 8;
        /* hidden: 10 + 100 = 110, 20, 30, 40 - 5 = 35, the rest 0; motors: twice that, clamped at 127 */
        CHECK(h2[0] == 110 && h2[1] == 20 && h2[2] == 30 && h2[3] == 35 && h2[4] == 0 && h2[7] == 0);
        CHECK(motor[0] == 127 && motor[1] == 40 && motor[2] == 60 && motor[3] == 70 && motor[4] == 0 && motor[AZ_MOT - 1] == 0);
    }
    check_step(br, &pl, mind, false);
}

/* ---- 2. random brains ---- */
static void test_random(u32 n_brains)
{
    static u8 mind[MIND_MAX];
    u32 sizes[4] = { 0, 0, 0, 0 };
    u64 tiles = 0;
    for (u32 i = 0; i < n_brains; i++) {
        const u32 pick = t_below(100);
        const u32 n_hb = pick < 6 ? 0u : pick < 70 ? 1u + t_below(6) : pick < 97 ? 7u + t_below(10) : 17u + t_below(MAX_HB - 16u);
        const u32 dens = n_hb > 16 ? 8u + t_below(40) : (t_below(8) == 0 ? 256u : 16u + t_below(200));
        const u32 kind = t_below(3) == 0;
        Plan pl;
        const u8 *br = make_brain(n_hb, dens, kind, 8u * t_below(8), &pl);
        sizes[n_hb == 0 ? 0 : n_hb < 7 ? 1 : n_hb < 17 ? 2 : 3]++;
        tiles += pl.n_t1 + pl.n_t2;
        if ((i & 63u) == 0) {
            CHECK_EQ(pl.bytes, (u32)az_brain_bytes(pl.n_hb, pl.n_t1, pl.n_t2));
            CHECK(az_brain_ok((const AzBrain *)(const void *)br, pl.bytes, pl.mind));
        }
        /* the senses and the hidden state of a creature are 0..127 */
        const u32 nh = 8u * n_hb;
        const u32 mode = t_below(8);
        for (u32 j = 0; j < AZ_SENS + nh; j++)
            mind[j] = (u8)(mode == 0 ? 127 : mode == 1 ? 0 : (t_rnd() >> 20) & 127u);
        for (u32 step = 0; step < 3; step++) {
            check_step(br, &pl, mind, true);
            memcpy(mind + AZ_SENS, mind + AZ_SENS + nh, nh);        /* the caller copies hid2 over hid */
            for (u32 j = 0; j < AZ_SENS; j++) mind[j] = (u8)((t_rnd() >> 20) & 127u);
        }
    }
    printf("random: %u brains (%u without neurons, %u small, %u medium, %u large), %.1f tiles on average\n",
           n_brains, sizes[0], sizes[1], sizes[2], sizes[3], (double)tiles / n_brains);
}

/* ---- negative control: an implementation with one wrong rounding is caught ---- */
static void broken_step(const AzBrain *b, u8 *mind, i8 *motor)
{
    az_brain_step_c(b, mind, motor);
    for (u32 k = 0; k < AZ_MOT; k++) if (motor[k] == -127) motor[k] = -128;     /* forgets the lower clamp */
}

static void test_control(void)
{
    /* the oracle disagrees with the broken step on some random brain */
    u32 caught = 0;
    static u8 mind[MIND_MAX], want_h[8 * MAX_HB];
    for (u32 i = 0; i < 2000 && !caught; i++) {
        Plan pl;
        const u8 *br = make_brain(1u + t_below(6), 128, 1, 8, &pl);
        i8 want_m[AZ_MOT], got[AZ_MOT];
        for (u32 j = 0; j < pl.mind; j++) mind[j] = (u8)((t_rnd() >> 20) & 127u);
        oracle(br, mind, want_h, want_m);
        broken_step((const AzBrain *)(const void *)br, mind, got);
        caught += memcmp(got, want_m, AZ_MOT) != 0;
    }
    CHECK(caught);
}

/* ---- 3. callee-saved registers survive (Windows x64: rbx rbp rsi rdi r12..r15, xmm6..xmm15) ----
 * brain_abi_check loads sentinels into all of them, calls fn(a[0..5]) and
 * returns a mask of the registers that came back changed:
 *   bits 0..7 rbx rbp rsi rdi r12 r13 r14 r15, bits 8..17 xmm6..xmm15.
 * brain_abi_bad is the control: it destroys r12 and xmm9 (mask 0x810). */
u32 brain_abi_check(BrainFn fn, const void *const *a, const u64 *gpr, const u8 *xmm);
void brain_abi_bad(const AzBrain *b, u8 *mind, i8 *motor);

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
    static u8 mind[MIND_MAX], want_h[8 * MAX_HB];
    for (u32 i = 0; i < 8; i++) gpr[i] = 0x1111111111111100ull * (i + 1) + i + 1;
    for (u32 i = 0; i < 160; i++) xmm[i] = (u8)(0xA0 + (i >> 4) * 7 + (i & 15));
    Plan pl;
    const u8 *br = make_brain(5, 160, 0, 8, &pl);
    for (u32 j = 0; j < pl.mind; j++) mind[j] = (u8)((t_rnd() >> 20) & 127u);
    i8 mot[AZ_MOT], want_m[AZ_MOT];
    oracle(br, mind, want_h, want_m);
    const void *args[6] = { br, mind, mot, NULL, NULL, NULL };
    for (u32 f = 0; f < n_fns; f++) {
        memset(mot, 0, sizeof mot);
        u32 mask = brain_abi_check(fns[f], args, gpr, xmm);
        printf("abi: %-20s changed-register mask = 0x%05x\n", fn_name[f], mask);
        CHECK_EQ(mask, 0);
        /* and the call through the checker still computed the right step */
        CHECK(memcmp(mind + AZ_SENS + 40, want_h, 40) == 0 && memcmp(mot, want_m, AZ_MOT) == 0);
    }
    u32 bad = brain_abi_check(brain_abi_bad, args, gpr, xmm);
    printf("abi: %-20s changed-register mask = 0x%05x (control, expected 0x00810)\n", "brain_abi_bad", bad);
    CHECK_EQ(bad, 0x810);
}

/* ---- 4. az_brain_ok ---- */
static void test_ok(void)
{
    static u8 copy[BRAIN_MAX], mind[MIND_MAX];
    Plan pl;
    u8 *br = make_brain(3, 200, 0, 8, &pl);
    const AzBrain *b = (const AzBrain *)(const void *)br;
    CHECK(pl.n_t1 >= 4 && pl.n_t2 >= 2);
    CHECK(az_brain_ok(b, pl.bytes, pl.mind));
    memcpy(copy, br, pl.bytes);
#define REFUSED(stmt, what) do { stmt; if (az_brain_ok(b, pl.bytes, pl.mind)) { printf("  accepted: %s\n", what); CHECK(!"refused"); } \
                                 memcpy(br, copy, pl.bytes); CHECK(az_brain_ok(b, pl.bytes, pl.mind)); } while (0)
    AzBrain h;
    memcpy(&h, br, sizeof h);
    CHECK(!az_brain_ok(b, pl.bytes - 8, pl.mind));
    CHECK(!az_brain_ok(b, pl.bytes + 8, pl.mind));
    CHECK(!az_brain_ok(b, 32, pl.mind));
    CHECK(!az_brain_ok(b, pl.bytes, pl.mind + 16));
    CHECK(!az_brain_ok(b, pl.bytes, pl.mind - 16));
    REFUSED(br[2] ^= 1, "another number of motor blocks");
    REFUSED(br[14] ^= 1, "another number of sensor groups");
    REFUSED(br[12] = 16, "shift1 above 15");
    REFUSED(br[13] = 200, "shift2 above 15");
    REFUSED(br[15] = 1, "pad not zero");
    REFUSED(br[0] += 1, "one hidden block more than the arrays hold");
    REFUSED(br[4] += 1, "one tile more than the arrays hold");
    REFUSED(br[8] -= 1, "one layer-2 tile fewer");
    for (u32 off = 32; off < 64; off += 4) REFUSED(br[off] += 8, "a moved array");
    REFUSED(br[h.off_c1] += 1, "a block with one tile more than there are");
    REFUSED(br[h.off_c1 + 2] -= 1, "a block with one tile fewer");
    REFUSED(br[h.off_c2] += 1, "a motor block with one tile more");
    { const u16 big = (u16)(SG + 2u * 3u); REFUSED(memcpy(br + h.off_in1, &big, 2), "an input group outside the mind"); }
    { const u16 big = (u16)(2u * 3u); REFUSED(memcpy(br + h.off_in2, &big, 2), "a hidden group outside the hidden state"); }
    {   /* the first block has at least two tiles: make them equal, then descending */
        u16 c0; memcpy(&c0, br + h.off_c1, 2);
        if (c0 >= 2) {
            REFUSED(memcpy(br + h.off_in1 + 2, br + h.off_in1, 2), "the same input group twice in a block");
            u16 a, z; memcpy(&a, br + h.off_in1, 2); memcpy(&z, br + h.off_in1 + 2, 2);
            REFUSED((memcpy(br + h.off_in1, &z, 2), memcpy(br + h.off_in1 + 2, &a, 2)), "input groups out of order");
        }
    }
#undef REFUSED

    /* whatever the gate lets through, the three implementations stay inside the mind and the motors */
    u32 accepted = 0, refused = 0;
    for (u32 i = 0; i < 60000; i++) {
        br = make_brain(t_below(5), 64 + t_below(192), 0, 8, &pl);
        b = (const AzBrain *)(const void *)br;
        const u32 where = t_below(4) == 0 ? t_below(pl.bytes) : t_below(pl.bytes < 160 ? pl.bytes : 160);
        const u8 old = br[where];
        br[where] = (u8)(t_below(3) == 0 ? old ^ (1u << t_below(8)) : t_rnd() >> 24);
        if (br[where] == old) continue;
        if (!az_brain_ok(b, pl.bytes, pl.mind)) { refused++; continue; }
        accepted++;
        for (u32 j = 0; j < pl.mind; j++) mind[j] = (u8)((t_rnd() >> 20) & 127u);
        const u32 hb = rd16u(br);
        if (hb != pl.n_hb) { CHECK(!"the gate let another neuron count through"); continue; }
        check_step(br, &pl, mind, false);
    }
    CHECK(accepted > 1000 && refused > 1000);
    printf("az_brain_ok: of the damaged brains %u were refused; the %u accepted ones ran inside their buffers and "
           "equal the oracle\n", refused, accepted);
}

/* ---- 5, 6. founders, inheritance, lineages ---- */
static u32 founder(AzWorld *w, u32 species_seed)
{
    u32 s = az_spawn(w);
    if (s == AZ_NONE) return AZ_NONE;
    if (!az_found(w, s, species_seed)) return AZ_NONE;
    return s;
}

/* the brain of a slot without the innate gain and bias (those are the genome's) */
static u64 brain_sum(const AzWorld *w, u32 s, u32 *bytes_out)
{
    u32 bytes;
    const u8 *b = (const u8 *)az_brain(w, s, &bytes);
    u64 h = bytes;
    for (u32 i = 0; i < bytes; i++) if (i < 16 || i >= 32) h = az_mix64(h + b[i]);
    if (bytes_out) *bytes_out = bytes;
    return h;
}

static bool slot_brain_ok(const AzWorld *w, u32 s)
{
    u32 bb, mb;
    const AzBrain *b = az_brain(w, s, &bb);
    (void)az_seg(w, s, AZ_SEG_MIND, &mb);
    return bb >= sizeof(AzBrain) && az_brain_ok(b, bb, mb) && az_blk_ok(az_blk(w, s), az_blk(w, s)->units);
}

static void test_founders(u64 seed)
{
    AzConfig cfg = az_test_config(seed, 64);
    AzTestWorld t;
    if (!az_test_world(&t, &cfg, 0)) { CHECK(!"world creation"); return; }
    AzWorld *w = t.w;
    /* two founders of a species have the same brain; another species another; the tick does not matter */
    const u32 a = founder(w, 0x70001u), b = founder(w, 0x70001u), c = founder(w, 0x70002u);
    w->tick = 4242; az_tick_keys(w);
    const u32 d = founder(w, 0x70001u);
    w->tick = 0; az_tick_keys(w);
    CHECK(a != AZ_NONE && b != AZ_NONE && c != AZ_NONE && d != AZ_NONE);
    if (a == AZ_NONE || b == AZ_NONE || c == AZ_NONE || d == AZ_NONE) { az_test_world_free(&t); return; }
    CHECK(slot_brain_ok(w, a) && slot_brain_ok(w, c));
    CHECK_EQ(brain_sum(w, a, NULL), brain_sum(w, b, NULL));
    CHECK_EQ(brain_sum(w, a, NULL), brain_sum(w, d, NULL));
    CHECK(brain_sum(w, a, NULL) != brain_sum(w, c, NULL));
    /* the parameters: hidden blocks and the share of possible tiles a founder has */
    u32 bb;
    const AzBrain *br = az_brain(w, a, &bb);
    CHECK_EQ(br->n_hb, w->param[AZ_WP_BRAIN_FOUND_HID]);
    u64 have = 0, possible = 0;
    u32 not_ok = 0;
    for (u32 i = 0; i < 150; i++) {
        const u32 s = founder(w, 0x71000u + i);
        if (s == AZ_NONE) { CHECK(!"founder"); break; }
        br = az_brain(w, s, &bb);
        not_ok += !slot_brain_ok(w, s);
        have += br->n_t1 + br->n_t2;
        possible += (u64)br->n_hb * (SG + 2u * br->n_hb) + (u64)MB * 2u * br->n_hb;
    }
    CHECK_EQ(not_ok, 0);
    const double share = (double)have / (double)possible, want = w->param[AZ_WP_BRAIN_DENSITY] / 256.0;
    CHECK(fabs(share - want) < 0.03);                     /* about 18,000 possible tiles: sigma 0.0036 */
    w->param[AZ_WP_BRAIN_DENSITY] = 256; w->param[AZ_WP_BRAIN_FOUND_HID] = 9;
    const u32 e = founder(w, 0x70003u);
    CHECK(e != AZ_NONE);
    if (e != AZ_NONE) {
        br = az_brain(w, e, &bb);
        CHECK_EQ(br->n_hb, 9);
        CHECK_EQ(br->n_t1, 9u * (SG + 18u));
        CHECK_EQ(br->n_t2, MB * 18u);
        CHECK(slot_brain_ok(w, e));
    }
    w->param[AZ_WP_BRAIN_FOUND_HID] = 1; w->param[AZ_WP_BRAIN_DENSITY] = 1;
    const u32 f = founder(w, 0x70004u);
    CHECK(f != AZ_NONE && slot_brain_ok(w, f));
    printf("founders seed %" PRIu64 ": tile share %.4f for BRAIN_DENSITY %.4f\n", seed, share, want);
    az_test_world_free(&t);
}

typedef struct Shape { u32 n_hb, n_t1, n_t2; } Shape;
static Shape shape_of(const AzWorld *w, u32 s)
{
    u32 bb;
    const AzBrain *b = az_brain(w, s, &bb);
    Shape sh = { b->n_hb, b->n_t1, b->n_t2 };
    return sh;
}

/* Every sensing and hidden byte starts clear. The final five bytes contain
 * the three developed receptor/strategy traits and the two zero histories. */
static bool newborn_mind_ok(const u8 *mind,u32 bytes,u32 n_hb,const u8 *traits)
{
    u32 state_bytes=AZ_SENS+16u*n_hb;
    if(bytes!=state_bytes+5u) return false;
    for(u32 i=0;i<state_bytes;i++) if(mind[i]) return false;
    const AzSenseMemory *h=(const AzSenseMemory *)(const void *)(mind+state_bytes);
    return h->payoff==0 && h->stake==0 && h->taxis==traits[AZ_T_TAXIS] &&
           h->curve==traits[AZ_T_CURVE] && h->innate_stake==traits[AZ_T_STAKE];
}

static void test_inherit(u64 seed)
{
    AzConfig cfg = az_test_config(seed, 64);
    cfg.max_clades = 40;
    AzTestWorld t;
    if (!az_test_world(&t, &cfg, 0)) { CHECK(!"world creation"); return; }
    AzWorld *w = t.w;
    u32 m = AZ_NONE;
    for (u32 s = 0; s < w->n_slots && m == AZ_NONE; s++) if (w->flags[s] & AZ_F_ALIVE) m = s;
    const u32 ch = az_spawn(w);
    CHECK(m != AZ_NONE && ch != AZ_NONE);
    if (m == AZ_NONE || ch == AZ_NONE) { az_test_world_free(&t); return; }
    const Shape ms = shape_of(w, m);
    u32 mbytes;
    const u64 msum = brain_sum(w, m, &mbytes);

    /* the mutation lesion: the child's brain is its mother's, its mind is empty */
    w->cfg.lesion |= AZ_LESION_MUTATION;
    u32 diff = 0, mind_bad = 0;
    for (u32 i = 0; i < 200; i++) {
        if (w->blk[ch]) az_blk_release(w, ch);
        w->uid[ch] = 5000000u + i;
        CHECK(az_conceive(w, ch, m, AZ_NONE));
        u32 cb, mb;
        diff += brain_sum(w, ch, &cb) != msum || cb != mbytes;
        u8 *mind = az_seg(w,ch,AZ_SEG_MIND,&mb);
        const u8 *child_traits=&w->trait[(size_t)ch*AZ_TRAITS];
        mind_bad+=!newborn_mind_ok(mind,mb,ms.n_hb,child_traits);
        mind_bad+=!newborn_mind_ok(mind,mb,ms.n_hb,&w->trait[(size_t)m*AZ_TRAITS]);
        if(i==0 && newborn_mind_ok(mind,mb,ms.n_hb,child_traits)) {
            const u32 state=mb-5u;
            const u32 offsets[]={0,AZ_SENS-1,AZ_SENS,state-1,state,state+1,state+2,state+3,state+4};
            for(u32 j=0;j<AZ_LEN(offsets);j++) {
                mind[offsets[j]]^=1;
                CHECK(!newborn_mind_ok(mind,mb,ms.n_hb,child_traits));
                mind[offsets[j]]^=1;
            }
            CHECK(!newborn_mind_ok(mind,mb-1,ms.n_hb,child_traits));
            CHECK(!newborn_mind_ok(mind,mb+1,ms.n_hb,child_traits));
        }
    }
    CHECK_EQ(diff, 0);
    CHECK_EQ(mind_bad, 0);
    w->cfg.lesion &= ~AZ_LESION_MUTATION;

    /* with mutation: one edit of the structure per birth at most; all four kinds happen; weights move */
    w->param[AZ_WP_NEURO_RATE] = 256;
    u8 *mt = &w->trait[(size_t)m * AZ_TRAITS];
    mt[AZ_T_NEURO_MUT] = 255; mt[AZ_T_NEURO_GROW] = 255; mt[AZ_T_NEURO_PRUNE] = 255;
    const u32 n = 20000;
    u32 not_ok = 0, shape_bad = 0, add_tile = 0, del_tile = 0, add_blk = 0, del_blk = 0, moved = 0, too_big = 0;
    for (u32 i = 0; i < n; i++) {
        if (w->blk[ch]) az_blk_release(w, ch);
        w->uid[ch] = 6000000u + i;
        u32 room_b, room_m, cb;
        az_brain_child_size(w, m, &room_b, &room_m);
        CHECK(az_conceive(w, ch, m, AZ_NONE));
        not_ok += !slot_brain_ok(w, ch);
        const Shape cs = shape_of(w, ch);
        u32 mind_len; const u8 *mind=az_seg(w,ch,AZ_SEG_MIND,&mind_len);
        not_ok+=!newborn_mind_ok(mind,mind_len,cs.n_hb,&w->trait[(size_t)ch*AZ_TRAITS]);
        const i32 dh = (i32)cs.n_hb - (i32)ms.n_hb, dt = (i32)(cs.n_t1 + cs.n_t2) - (i32)(ms.n_t1 + ms.n_t2);
        const u64 csum = brain_sum(w, ch, &cb);
        if (cb > room_b) too_big++;
        if (dh == 0 && dt == 1) add_tile++;
        else if (dh == 0 && dt == -1) del_tile++;
        else if (dh == 1 && dt >= 0 && dt <= 4) add_blk++;
        else if (dh == -1 && dt <= 0) del_blk++;
        else if (dh != 0 || dt != 0) shape_bad++;
        else moved += csum != msum;
    }
    CHECK_EQ(not_ok, 0); CHECK_EQ(shape_bad, 0); CHECK_EQ(too_big, 0);
    /* chances out of 2^18: grow = prune = NEURO_RATE * (8 + trait) = 256 * 263 */
    const double p = 256.0 * 263.0 / 262144.0;
    CHECK(fabs((double)add_tile / n - p * 0.63) < 0.25 * p + 0.02);      /* a new tile needs a free place */
    CHECK(fabs((double)del_tile / n - p) < 0.02);
    CHECK(add_blk > 0 && del_blk > 0);
    CHECK(fabs((double)(add_blk + del_blk) / n - 2.0 * p / 8.0) < 0.04);
    CHECK(moved > n / 4);
    printf("inherit seed %" PRIu64 ": of %u children %u gained a tile, %u lost one, %u gained a block of neurons, %u lost "
           "one, %u others had weights moved\n", seed, n, add_tile, del_tile, add_blk, del_blk, moved);

    /* the mother's own rate traits: at 0 the edits fall to 8/263 of that */
    mt[AZ_T_NEURO_GROW] = 0; mt[AZ_T_NEURO_PRUNE] = 0;
    u32 edits = 0;
    for (u32 i = 0; i < n; i++) {
        if (w->blk[ch]) az_blk_release(w, ch);
        w->uid[ch] = 7000000u + i;
        CHECK(az_conceive(w, ch, m, AZ_NONE));
        const Shape cs = shape_of(w, ch);
        edits += cs.n_hb != ms.n_hb || cs.n_t1 != ms.n_t1 || cs.n_t2 != ms.n_t2;
    }
    CHECK((double)edits / n < 0.05);                      /* about 0.015 */
    CHECK(edits > 0);

    /* lineages: a child whose brain differs in structure founds one, until the table is full */
    mt[AZ_T_NEURO_GROW] = 255; mt[AZ_T_NEURO_PRUNE] = 255;
    const u32 clades0 = w->n_clades, home = w->clade[m];
    const u64 denied0 = w->totals.clade_denied;
    u32 founded = 0, kept = 0, wrong = 0;
    for (u32 i = 0; i < 400; i++) {
        const u32 k = az_spawn(w);
        if (k == AZ_NONE) break;
        w->uid[k] = 8000000u + i;
        if (!az_conceive(w, k, m, AZ_NONE)) break;
        const Shape cs = shape_of(w, k);
        const bool differs = cs.n_hb != ms.n_hb || cs.n_t1 != ms.n_t1 || cs.n_t2 != ms.n_t2;
        const u32 before = w->clades[home].refcount;
        az_clade_assign(w, k, m);
        if (w->clade[k] == home) {
            kept++;
            if (w->clades[home].refcount != before + 1) wrong++;
            if (differs && w->totals.clade_denied == denied0) wrong++;       /* only a full table keeps a new shape at home */
        } else {
            founded++;
            const AzClade *c = &w->clades[w->clade[k]];
            if (!differs || c->refcount != 1 || !c->alive || c->founder_uid != w->uid[k] ||
                c->parent_uid != w->clades[home].founder_uid) wrong++;
        }
    }
    CHECK_EQ(wrong, 0);
    CHECK(founded > 0 && kept > 0);
    CHECK(w->n_clades <= cfg.max_clades);
    CHECK(w->totals.clade_denied > denied0);              /* 40 records were not enough, and the world says so */
    printf("lineages: %u children founded one (table %u -> %u of %u), %u stayed in the mother's, %" PRIu64 " refused\n",
           founded, clades0, w->n_clades, cfg.max_clades, kept, w->totals.clade_denied - denied0);
    az_test_world_free(&t);
}

/* ---- 7. the brain inside a running world ---- */
typedef struct WorldRun { u64 hash; u64 births; u32 twins; int ok; } WorldRun;

static WorldRun world_run(u64 seed, u32 threads, u8 reorder, u32 lesion, u32 want_twins)
{
    WorldRun r = { 0, 0, 0, 0 };
    AzConfig cfg = az_test_config(seed, 2000);
    cfg.reorder_period = reorder;
    cfg.lesion = lesion;
    AzTestWorld t;
    if (!az_test_world(&t, &cfg, threads)) { CHECK(!"world creation"); return r; }
    AzWorld *w = t.w;
    w->param[AZ_WP_NEURO_RATE] = 512;                     /* brains of many shapes within the run */
    if (want_twins) r.twins = az_kernels_install(w, want_twins);
    const u64 m0 = az_total_matter(w);
    const i64 e0 = az_total_energy(w);
    r.ok = 1;
    for (u32 i = 0; i < 120; i++) {
        az_tick(w);
        if ((i % 20) == 19 && (az_total_matter(w) != m0 || az_total_energy(w) != e0)) r.ok = 0;
    }
    for (u32 s = 0; s < w->n_slots; s++)
        if ((w->flags[s] & AZ_F_ALIVE) && !slot_brain_ok(w, s)) r.ok = 0;
    r.hash = az_state_hash(w);
    r.births = w->totals.births;
    az_test_world_free(&t);
    return r;
}

static void test_world(void)
{
    const u64 seed = 37;
    const WorldRun ref = world_run(seed, 0, 1, 0, 0);
    CHECK(ref.ok);
    CHECK(ref.births > 100);
    const WorldRun par = world_run(seed, 8, 7, 0, 0);
    CHECK(par.ok);
    CHECK_EQ(par.hash, ref.hash);
#ifdef AZ_HAVE_BRAIN_VNNI
    const u32 feat = az_cpu_features();
    if (az_twin_ok_brain_vnni(feat)) {
        const WorldRun v = world_run(seed, 8, 7, 0, AZ_K_BRAIN_VNNI);
        CHECK(v.ok); CHECK(v.twins & AZ_K_BRAIN_VNNI);
        CHECK_EQ(v.hash, ref.hash);
    }
    if (az_twin_ok_avx2(feat)) {
        const WorldRun v = world_run(seed, 5, 31, 0, AZ_K_BRAIN_AVX2);
        CHECK(v.ok); CHECK(v.twins & AZ_K_BRAIN_AVX2);
        CHECK_EQ(v.hash, ref.hash);
    }
#endif
    /* the brain matters: with the motors forced to zero the world is another world */
    const WorldRun les = world_run(seed, 8, 7, AZ_LESION_BRAIN, 0);
    CHECK(les.ok);
    CHECK(les.hash != ref.hash);
    printf("world: %" PRIu64 " births, hash %016" PRIx64 " with the reference on 1 and 8 threads and with each twin; "
           "brain lesion %016" PRIx64 "\n", ref.births, ref.hash, les.hash);
}

int main(int argc, char **argv)
{
    u64 seeds[2] = { 23, 67 };
    u32 n_seeds = 2;
    if (argc > 1) { seeds[0] = strtoull(argv[1], NULL, 10); n_seeds = 1; }
    add_fn(az_brain_step_c, "az_brain_step_c");
#ifdef AZ_HAVE_BRAIN_VNNI
    {
        u32 f = az_cpu_features();
        if (f & AZ_CPU_AVX2) add_fn(az_brain_step_avx2, "az_brain_step_avx2");
        else printf("note: no AVX2 on this CPU, az_brain_step_avx2 not tested\n");
        if (az_twin_ok_brain_vnni(f)) add_fn(az_brain_step_vnni, "az_brain_step_vnni");
        else printf("note: no AVX-VNNI on this CPU, az_brain_step_vnni not tested\n");
    }
#else
    printf("note: built without the assembly twins, only the C reference is tested\n");
#endif
    test_known();
    printf("known cases: %d checks so far, %d failures\n", az_checks, az_failures);
    test_abi();
    test_control();
    test_ok();
    for (u32 i = 0; i < n_seeds; i++) {
        test_founders(seeds[i]);
        test_inherit(seeds[i]);
    }
    test_world();
    test_random(120000);
    for (u32 f = 0; f < n_fns; f++)
        printf("%-20s %" PRIu64 " steps compared with the oracle\n", fn_name[f], fn_steps[f]);
    printf("oracle output mix: hid %" PRIu64 " at 0, %" PRIu64 " inside, %" PRIu64 " at 127; motor %" PRIu64
           " at -127, %" PRIu64 " inside, %" PRIu64 " at 127\n", mix_h[0], mix_h[1], mix_h[2], mix_m[0], mix_m[1], mix_m[2]);
    CHECK(mix_h[0] && mix_h[1] && mix_h[2] && mix_m[0] && mix_m[1] && mix_m[2]);
    return az_test_done();
}
