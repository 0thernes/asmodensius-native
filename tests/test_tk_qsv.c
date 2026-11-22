/* test_tk_qsv.c - tests of core/tk_qsv.c, the classical fixed-point
 * simulation of a 3-qubit register.
 *
 * References used here (hosted code, doubles allowed in tests/):
 *   1. an independent double-precision state-vector reference written in
 *      this file (generic 2x2 complex matrix application, table decoder);
 *   2. the upstream Moonlab dense kernel, unmodified, built into this
 *      translation unit through tests/upstream/moonlab/moonlab_oracle.h.
 *
 * Stated bounds (every one is a CHECK below):
 *   B1 arithmetic: against a double reference that uses the kernel's own
 *      (cos, sin) pair, the amplitude vector is within
 *      2.05 LSB * (rounding ops + renormalisations) * 1.01 in L2 norm
 *      (1 LSB = 1/16384); a start from a rounded random state adds 2.05 LSB.
 *   B2 end to end: against true double sin/cos the bound is B1 plus
 *      dmax per rotation, where dmax is the largest half-angle error of the
 *      linked az_sin_q14 / az_cos_q14, measured over all 65536 half angles
 *      and required to be <= 0.004 rad (the stub and a rounded table both
 *      satisfy this).
 *   B3 observed caps over all random circuits of 1..32 ops: max |dP| <= 1.0e-3
 *      against the same-pair reference, and <= 0.01 against true sin/cos.
 *   B4 norm: | |psi| - 1 | <= 0.02 after every one of 10,000 random gates
 *      when the state is renormalised every 16 gates.
 */
#include "upstream/moonlab/moonlab_oracle.h"
#include "az_test.h"
#include "../core/tk_qsv.h"
#include <math.h>
#include <time.h>

#define TWO_PI 6.283185307179586476925286766559
#define Q14    16384.0
#define Q30    1073741824.0

/* ---- test-side random numbers (splitmix64; not the simulation RNG) ---- */
static u64 rng_state;
static u64 rnd64(void) { return az_test_rand64(&rng_state); }
static u32 rnd32(void) { return (u32)(rnd64() >> 32); }
static u32 rnd_below(u32 n) { return (u32)(((u64)rnd32() * n) >> 32); }
static double rnd_unit(void) { return (double)(rnd64() >> 11) * (1.0 / 9007199254740992.0); }

/* ---- independent double reference ---- */
typedef struct DState { double re[8], im[8]; } DState;

static void d_reset(DState *d)
{
    memset(d, 0, sizeof *d);
    d->re[0] = 1.0;
}

/* m = { m00.re, m00.im, m01.re, m01.im, m10.re, m10.im, m11.re, m11.im } */
static void d_gate1(DState *d, int q, const double m[8])
{
    for (int i = 0; i < 8; i++) {
        if ((i >> q) & 1) continue;
        int j = i | (1 << q);
        double ar = d->re[i], ai = d->im[i], br = d->re[j], bi = d->im[j];
        d->re[i] = m[0] * ar - m[1] * ai + m[2] * br - m[3] * bi;
        d->im[i] = m[0] * ai + m[1] * ar + m[2] * bi + m[3] * br;
        d->re[j] = m[4] * ar - m[5] * ai + m[6] * br - m[7] * bi;
        d->im[j] = m[4] * ai + m[5] * ar + m[6] * bi + m[7] * br;
    }
}

static void d_cnot(DState *d, int c, int t)
{
    DState o = *d;
    for (int i = 0; i < 8; i++) {
        int src = ((i >> c) & 1) ? (i ^ (1 << t)) : i;
        d->re[i] = o.re[src];
        d->im[i] = o.im[src];
    }
}

static void d_cz(DState *d, int a, int b)
{
    for (int i = 0; i < 8; i++)
        if (((i >> a) & 1) && ((i >> b) & 1)) { d->re[i] = -d->re[i]; d->im[i] = -d->im[i]; }
}

static void d_fixed_gate(DState *d, int gate, int q)
{
    const double r = sqrt(0.5);
    const double mh[8] = { r, 0, r, 0, r, 0, -r, 0 };
    const double mx[8] = { 0, 0, 1, 0, 1, 0, 0, 0 };
    const double my[8] = { 0, 0, 0, -1, 0, 1, 0, 0 };
    const double mz[8] = { 1, 0, 0, 0, 0, 0, -1, 0 };
    d_gate1(d, q, gate == TK_QSV_G_H ? mh : gate == TK_QSV_G_X ? mx : gate == TK_QSV_G_Y ? my : mz);
}

static void d_rot(DState *d, int gate, int q, double c, double s)
{
    const double rx[8] = { c, 0, 0, -s, 0, -s, c, 0 };
    const double ry[8] = { c, 0, -s, 0, s, 0, c, 0 };
    const double rz[8] = { c, -s, 0, 0, 0, 0, c, s };
    d_gate1(d, q, gate == TK_QSV_G_RX ? rx : gate == TK_QSV_G_RY ? ry : rz);
}

static double d_norm(const DState *d)
{
    double t = 0;
    for (int i = 0; i < 8; i++) t += d->re[i] * d->re[i] + d->im[i] * d->im[i];
    return sqrt(t);
}

/* ---- decoder written from the table in tk_qsv.h, independent of the kernel ---- */
typedef struct RefOp { int gate, q0, q1, slot, inv; } RefOp;

static RefOp ref_decode(unsigned op)
{
    static const int cnot_pair[6][2] = { { 0, 1 }, { 0, 2 }, { 1, 0 }, { 1, 2 }, { 2, 0 }, { 2, 1 } };
    static const int cz_pair[3][2] = { { 0, 1 }, { 0, 2 }, { 1, 2 } };
    static const int rot_gate[3] = { TK_QSV_G_RX, TK_QSV_G_RY, TK_QSV_G_RZ };
    static const int fix_gate[4] = { TK_QSV_G_H, TK_QSV_G_X, TK_QSV_G_Y, TK_QSV_G_Z };
    RefOp r = { TK_QSV_G_NOP, 0, 0, 0, 0 };
    unsigned top = (op >> 6) & 3u, mid = (op >> 3) & 7u, low = op & 7u;
    if (top != 3u) {
        if ((low & 3u) == 3u) return r;
        r.gate = rot_gate[top]; r.q0 = (int)(low & 3u); r.slot = (int)mid; r.inv = (int)(low >> 2);
    } else if (mid < 4u) {
        if ((low & 3u) == 3u) return r;
        r.gate = fix_gate[mid]; r.q0 = (int)(low & 3u);
    } else if (mid == 4u) {
        if (low >= 6u) return r;
        r.gate = TK_QSV_G_CNOT; r.q0 = cnot_pair[low][0]; r.q1 = cnot_pair[low][1];
    } else if (mid == 5u) {
        if ((low & 3u) == 3u) return r;
        r.gate = TK_QSV_G_CZ; r.q0 = cz_pair[low & 3u][0]; r.q1 = cz_pair[low & 3u][1];
    }
    return r;
}

/* ---- comparison helpers ---- */
static double amp_l2_lsb(const i16 *s, const DState *d)
{
    double t = 0;
    for (int i = 0; i < 8; i++) {
        double a = s[2 * i] - d->re[i] * Q14, b = s[2 * i + 1] - d->im[i] * Q14;
        t += a * a + b * b;
    }
    return sqrt(t);
}

static double max_dp(const i16 *s, const DState *d)
{
    u32 p[8];
    double m = 0;
    tk_qsv_probs(s, p);
    for (int i = 0; i < 8; i++) {
        double e = fabs(p[i] / 65536.0 - (d->re[i] * d->re[i] + d->im[i] * d->im[i]));
        if (e > m) m = e;
    }
    return m;
}

static double fixed_norm(const i16 *s) { return sqrt(tk_qsv_norm2(s) / (double)TK_QSV_NORM2_ONE); }

static void random_unit_state(i16 *s, DState *d)
{
    double n;
    do {
        for (int i = 0; i < 8; i++) { d->re[i] = rnd_unit() * 2 - 1; d->im[i] = rnd_unit() * 2 - 1; }
        n = d_norm(d);
    } while (n < 0.1);
    for (int i = 0; i < 8; i++) {
        d->re[i] /= n; d->im[i] /= n;
        s[2 * i] = (i16)lround(d->re[i] * Q14);
        s[2 * i + 1] = (i16)lround(d->im[i] * Q14);
    }
}

static void random_any_state(i16 *s)
{
    u32 kind = rnd_below(4);
    for (int i = 0; i < 16; i++) {
        u32 r = rnd32();
        s[i] = kind == 0 ? (i16)r                         /* any 16 bits, includes -32768 */
             : kind == 1 ? (i16)((i32)(r % 7u) - 3)       /* tiny */
             : kind == 2 ? ((r & 7u) ? 0 : (i16)(r >> 16))/* sparse */
             : (i16)((i32)(r % 23171u) - 11585);          /* about unit scale */
    }
}

/* ====================================================================== */
/* unit tests                                                             */
/* ====================================================================== */

static void test_reset_and_exact_gates(void)
{
    i16 s[16], t[16];
    memset(s, 0x55, sizeof s);
    tk_qsv_reset(s);
    CHECK_EQ(s[0], 16384);
    for (int i = 1; i < 16; i++) CHECK_EQ(s[i], 0);
    CHECK_EQ(tk_qsv_norm2(s), TK_QSV_NORM2_ONE);

    /* X on qubit q moves |000> to basis index 1 << q (little-endian order) */
    for (u32 q = 0; q < 3; q++) {
        tk_qsv_reset(s);
        tk_qsv_x(s, q);
        for (int i = 0; i < 8; i++) CHECK_EQ(s[2 * i], i == (1 << q) ? 16384 : 0);
    }

    /* the exact gates against the double reference, bit for bit, and their
     * involutions on random states */
    rng_state = 11;
    for (int rep = 0; rep < 2000; rep++) {
        DState d;
        random_unit_state(s, &d);
        for (int i = 0; i < 8; i++) { d.re[i] = s[2 * i]; d.im[i] = s[2 * i + 1]; }   /* integers in doubles */
        memcpy(t, s, sizeof t);
        u32 q = rnd_below(3), q2 = (q + 1 + rnd_below(2)) % 3;
        int g = (int)rnd_below(5);
        for (int twice = 0; twice < 2; twice++) {
            switch (g) {
            case 0: tk_qsv_x(s, q); if (!twice) d_fixed_gate(&d, TK_QSV_G_X, (int)q); break;
            case 1: tk_qsv_y(s, q); if (!twice) d_fixed_gate(&d, TK_QSV_G_Y, (int)q); break;
            case 2: tk_qsv_z(s, q); if (!twice) d_fixed_gate(&d, TK_QSV_G_Z, (int)q); break;
            case 3: tk_qsv_cnot(s, q, q2); if (!twice) d_cnot(&d, (int)q, (int)q2); break;
            default: tk_qsv_cz(s, q, q2); if (!twice) d_cz(&d, (int)q, (int)q2); break;
            }
            if (!twice) {
                int same = 1;
                for (int i = 0; i < 8; i++) same &= (s[2 * i] == d.re[i]) && (s[2 * i + 1] == d.im[i]);
                CHECK(same);
            }
        }
        /* X, Z, CNOT, CZ are involutions; Y*Y = 1 as well */
        CHECK(memcmp(s, t, sizeof t) == 0);
    }

    /* invalid arguments are no-ops */
    random_any_state(s);
    memcpy(t, s, sizeof t);
    tk_qsv_h(s, 3); tk_qsv_x(s, 3); tk_qsv_y(s, 7); tk_qsv_z(s, 0xFFFFFFFFu);
    tk_qsv_rx(s, 3, 1234); tk_qsv_ry(s, 4, 1234); tk_qsv_rz(s, 99, 1234);
    tk_qsv_cnot(s, 1, 1); tk_qsv_cnot(s, 3, 0); tk_qsv_cnot(s, 0, 3); tk_qsv_cz(s, 2, 2); tk_qsv_cz(s, 0, 5);
    { i32 cs[2] = { 0, TK_QSV_CS_ONE }; tk_qsv_rot(s, 3, 0, cs); tk_qsv_rot(s, TK_QSV_RX, 3, cs); }
    CHECK(memcmp(s, t, sizeof t) == 0);

    /* Every function accepts any 16 words. A gate never writes -32768: that
     * value can only survive as an input word a swap moved or a gate did not
     * touch, so an input without it gives an output without it; and a
     * renormalisation always removes it. */
    rng_state = 12;
    for (int rep = 0; rep < 20000; rep++) {
        random_any_state(s);
        if (rep < 16) for (int i = 0; i < 16; i++) s[i] = (i16)(((rep >> (i & 3)) & 1) ? -32768 : 32767);
        int had_min = 0;
        for (int i = 0; i < 16; i++) had_min |= (s[i] == -32768);
        u8 op = (u8)rnd32();
        u16 ang[8];
        for (int i = 0; i < 8; i++) ang[i] = (u16)rnd32();
        tk_qsv_apply(s, op, ang);
        int has_min = 0;
        for (int i = 0; i < 16; i++) has_min |= (s[i] == -32768);
        CHECK(had_min || !has_min);
        /* scale some states to lengths around 1.0, on both sides of the window
         * in which the kernel replaces the square root by a series */
        if (rep % 4 == 1) {
            DState du;
            random_unit_state(s, &du);
            double len = 1.0 + (rnd_unit() - 0.5) * 0.06;
            if (rep % 16 == 1) len = sqrt(1.0 + ((rep & 16) ? 1.0 : -1.0) * (1.0 / 64.0 + (rnd_unit() - 0.5) * 4e-4));
            for (int i = 0; i < 16; i++) s[i] = (i16)lround(s[i] * len);
        }
        double want[16], nrm = 0;
        for (int i = 0; i < 16; i++) nrm += (double)s[i] * s[i];
        nrm = sqrt(nrm);
        for (int i = 0; i < 16; i++) want[i] = nrm > 0 ? Q14 * s[i] / nrm : (i == 0 ? Q14 : 0.0);
        tk_qsv_renorm(s);
        has_min = 0;
        for (int i = 0; i < 16; i++) has_min |= (s[i] == -32768);
        CHECK(!has_min);
        /* every word is the exactly normalised value rounded to nearest */
        double worst = 0;
        for (int i = 0; i < 16; i++) worst = fmax(worst, fabs(s[i] - want[i]));
        CHECK(worst <= 0.5 + 1e-3);
        /* so the result is a unit vector to within rounding */
        CHECK(fabs(fixed_norm(s) - 1.0) < 3e-4);
    }
    /* the all-zero vector becomes |000> */
    memset(s, 0, sizeof s);
    tk_qsv_renorm(s);
    tk_qsv_reset(t);
    CHECK(memcmp(s, t, sizeof t) == 0);
}

static void test_textbook_states(void)
{
    i16 s[16];
    u32 p[8];

    /* Bell state (|00> + |11>) / sqrt(2) on qubits 0 and 1 */
    tk_qsv_reset(s);
    tk_qsv_h(s, 0);
    CHECK_EQ(s[0], 11585); CHECK_EQ(s[2], 11585);        /* round(16384 / sqrt(2)) */
    tk_qsv_cnot(s, 0, 1);
    tk_qsv_probs(s, p);
    for (int i = 0; i < 8; i++) CHECK_EQ(p[i], (i == 0 || i == 3) ? 32768 : 0);

    /* the other two Bell pairs */
    tk_qsv_reset(s); tk_qsv_h(s, 2); tk_qsv_cnot(s, 2, 0); tk_qsv_probs(s, p);
    for (int i = 0; i < 8; i++) CHECK_EQ(p[i], (i == 0 || i == 5) ? 32768 : 0);
    tk_qsv_reset(s); tk_qsv_h(s, 1); tk_qsv_cnot(s, 1, 2); tk_qsv_probs(s, p);
    for (int i = 0; i < 8; i++) CHECK_EQ(p[i], (i == 0 || i == 6) ? 32768 : 0);

    /* GHZ state (|000> + |111>) / sqrt(2), as gates and as a program */
    tk_qsv_reset(s);
    tk_qsv_h(s, 0); tk_qsv_cnot(s, 0, 1); tk_qsv_cnot(s, 1, 2);
    tk_qsv_probs(s, p);
    for (int i = 0; i < 8; i++) CHECK_EQ(p[i], (i == 0 || i == 7) ? 32768 : 0);
    {
        const u8 prog[3] = { TK_QSV_OP_H(0), TK_QSV_OP_CNOT(0, 1), TK_QSV_OP_CNOT(0, 2) };
        const u16 ang[8] = { 0 };
        i16 g[16];
        tk_qsv_reset(g);
        tk_qsv_run(g, prog, 3, ang);
        tk_qsv_probs(g, p);
        for (int i = 0; i < 8; i++) CHECK_EQ(p[i], (i == 0 || i == 7) ? 32768 : 0);
        CHECK(memcmp(g, s, sizeof g) == 0);
        /* GHZ sampling: only 000 and 111, exactly half of the r >> 16 values each */
        u32 n0 = 0, n7 = 0, other = 0;
        for (u32 u = 0; u < 65536; u++) {
            u32 o = tk_qsv_sample(g, (u << 16) | (u * 40503u & 0xFFFFu));
            if (o == 0) n0++; else if (o == 7) n7++; else other++;
        }
        CHECK_EQ(n0, 32768); CHECK_EQ(n7, 32768); CHECK_EQ(other, 0);
    }

    /* uniform superposition: H on all three qubits gives 1/8 each */
    tk_qsv_reset(s);
    tk_qsv_h(s, 0); tk_qsv_h(s, 1); tk_qsv_h(s, 2);
    tk_qsv_probs(s, p);
    for (int i = 0; i < 8; i++) CHECK_EQ(p[i], 8192);

    /* CZ only changes a sign: H H CZ keeps 1/4 on each of the four states,
     * and the amplitude of |11> is negative */
    tk_qsv_reset(s);
    tk_qsv_h(s, 0); tk_qsv_h(s, 1); tk_qsv_cz(s, 0, 1);
    tk_qsv_probs(s, p);
    for (int i = 0; i < 8; i++) CHECK_EQ(p[i], i < 4 ? 16384 : 0);
    CHECK(s[2 * 3] < 0 && s[0] > 0 && s[2] > 0 && s[4] > 0);

    /* H is its own inverse up to one rounding step per application */
    rng_state = 13;
    for (int rep = 0; rep < 2000; rep++) {
        i16 t[16];
        DState d;
        random_unit_state(s, &d);
        memcpy(t, s, sizeof t);
        u32 q = rnd_below(3);
        tk_qsv_h(s, q); tk_qsv_h(s, q);
        int ok = 1;
        for (int i = 0; i < 16; i++) ok &= abs(s[i] - t[i]) <= 2;
        CHECK(ok);
    }
}

/* ====================================================================== */
/* the (cos, sin) pair                                                    */
/* ====================================================================== */

static double g_dmax;        /* largest half-angle error of the linked az_sin_q14 / az_cos_q14 (rad) */
static double g_dmax_ideal;  /* the same for a table of round(16384 * sin) */

static double wrap_pi(double x)
{
    while (x > TWO_PI / 2) x -= TWO_PI;
    while (x < -TWO_PI / 2) x += TWO_PI;
    return x;
}

static void ideal_cs(u32 angle, i32 cs[2])
{
    double th = TWO_PI * (double)((angle >> 1) & 0xFFFFu) / 65536.0;
    tk_qsv_unit((i32)lround(Q14 * cos(th)), (i32)lround(Q14 * sin(th)), cs);
}

static void test_trig_and_unit(void)
{
    double max_table_err = 0, max_n2_err = 0, max_dir_err = 0;
    int low_bit_ignored = 1;
    g_dmax = 0; g_dmax_ideal = 0;
    for (u32 h = 0; h < 65536; h++) {
        double th = TWO_PI * h / 65536.0;
        i32 c14 = az_cos_q14(h), s14 = az_sin_q14(h), cs[2], cs1[2], id[2];
        max_table_err = fmax(max_table_err, fmax(fabs(c14 - Q14 * cos(th)), fabs(s14 - Q14 * sin(th))));
        tk_qsv_cs(2 * h, cs);
        tk_qsv_cs(2 * h + 1, cs1);
        low_bit_ignored &= (cs[0] == cs1[0] && cs[1] == cs1[1]);
        max_n2_err = fmax(max_n2_err, fabs(((double)cs[0] * cs[0] + (double)cs[1] * cs[1]) / (Q30 * Q30) - 1.0));
        /* the pair keeps the direction of what the sine table returned */
        max_dir_err = fmax(max_dir_err, fabs(wrap_pi(atan2(cs[1], cs[0]) - atan2(s14, c14))));
        g_dmax = fmax(g_dmax, fabs(wrap_pi(atan2(cs[1], cs[0]) - th)));
        ideal_cs(2 * h, id);
        g_dmax_ideal = fmax(g_dmax_ideal, fabs(wrap_pi(atan2(id[1], id[0]) - th)));
    }
    printf("sine in this build: max |az_sin_q14 - 16384 sin| = %.3f LSB; half-angle error dmax = %.3e rad"
           " (rounded table would give %.3e)\n", max_table_err, g_dmax, g_dmax_ideal);
    printf("unit pair: max |c^2 + s^2 - 1| = %.3e, max direction change = %.3e rad\n", max_n2_err, max_dir_err);
    CHECK(low_bit_ignored);
    CHECK(max_n2_err <= 4e-9);          /* two Q1.30 roundings */
    CHECK(max_dir_err <= 2e-9);
    CHECK(g_dmax <= 0.004);             /* B2 precondition on the linked sine */
    CHECK(g_dmax_ideal <= 1e-4);

    /* exact pairs at the quarter points; periodic with two turns */
    i32 cs[2], cs2[2];
    tk_qsv_cs(0, cs);      CHECK_EQ(cs[0], TK_QSV_CS_ONE);  CHECK_EQ(cs[1], 0);
    tk_qsv_cs(32768, cs);  CHECK_EQ(cs[0], 0);              CHECK_EQ(cs[1], TK_QSV_CS_ONE);
    tk_qsv_cs(65536, cs);  CHECK_EQ(cs[0], -TK_QSV_CS_ONE); CHECK_EQ(cs[1], 0);
    tk_qsv_cs(98304, cs);  CHECK_EQ(cs[0], 0);              CHECK_EQ(cs[1], -TK_QSV_CS_ONE);
    tk_qsv_cs(12345, cs); tk_qsv_cs(12345u + 131072u, cs2);
    CHECK(cs[0] == cs2[0] && cs[1] == cs2[1]);

    /* tk_qsv_unit on arbitrary pairs: unit length, same direction; the zero
     * pair gives the identity */
    tk_qsv_unit(0, 0, cs); CHECK_EQ(cs[0], TK_QSV_CS_ONE); CHECK_EQ(cs[1], 0);
    rng_state = 21;
    for (int rep = 0; rep < 150000; rep++) {
        i32 c = (i32)rnd_below(65535) - 32767, s = (i32)rnd_below(65535) - 32767;
        if (rep % 3 == 0) { c /= 4096; s /= 4096; }          /* short pairs */
        if (rep % 3 == 1) {
            /* lengths within 3% of 1.0: both sides of the window in which the
             * kernel replaces the square root by a series */
            double len = 1.0 + (rnd_unit() - 0.5) * 0.06, th = TWO_PI * rnd_unit();
            if (rep % 12 == 1) len = 1.0 + ((rep & 16) ? 1.0 : -1.0) * (1.0 / 128.0 + (rnd_unit() - 0.5) * 2e-4);
            c = (i32)lround(Q14 * len * cos(th)); s = (i32)lround(Q14 * len * sin(th));
        }
        if (c == 0 && s == 0) continue;
        tk_qsv_unit(c, s, cs);
        double n2 = ((double)cs[0] * cs[0] + (double)cs[1] * cs[1]) / (Q30 * Q30);
        CHECK(fabs(n2 - 1.0) <= 4e-9 && fabs(wrap_pi(atan2(cs[1], cs[0]) - atan2(s, c))) <= 2e-9);
    }
    /* inputs outside Q1.14 are clamped, not undefined */
    tk_qsv_unit(1000000, 0, cs);   CHECK_EQ(cs[0], TK_QSV_CS_ONE);  CHECK_EQ(cs[1], 0);
    tk_qsv_unit(0, -1000000, cs);  CHECK_EQ(cs[0], 0);              CHECK_EQ(cs[1], -TK_QSV_CS_ONE);

    /* textbook rotations from |000>, within the half-angle error of the sine */
    const double tol = 2.0 * g_dmax + 4.0 / 65536.0;
    i16 st[16];
    u32 p[8];
    for (u32 q = 0; q < 3; q++) {
        /* RX and RY by half a turn (theta = pi) flip the qubit exactly */
        tk_qsv_reset(st); tk_qsv_rx(st, q, 32768); tk_qsv_probs(st, p); CHECK_EQ(p[1u << q], 65536);
        CHECK_EQ(st[2 * (1 << q) + 1], -16384);                         /* RX(pi)|0> = -i|1> */
        tk_qsv_reset(st); tk_qsv_ry(st, q, 32768); tk_qsv_probs(st, p); CHECK_EQ(p[1u << q], 65536);
        CHECK_EQ(st[2 * (1 << q)], 16384);                              /* RY(pi)|0> = |1> */
        /* RZ leaves |0> probabilities alone and is a phase: RZ(pi)|0> = -i|0> */
        tk_qsv_reset(st); tk_qsv_rz(st, q, 32768); CHECK_EQ(st[0], 0); CHECK_EQ(st[1], -16384);
        /* RY(theta)|0>: P(1) = sin^2(theta / 2) */
        for (u32 a = 0; a < 65536; a += 257) {
            double th = TWO_PI * a / 65536.0;
            tk_qsv_reset(st); tk_qsv_ry(st, q, a); tk_qsv_probs(st, p);
            CHECK(fabs(p[1u << q] / 65536.0 - sin(th / 2) * sin(th / 2)) <= tol);
            tk_qsv_reset(st); tk_qsv_rx(st, q, a); tk_qsv_probs(st, p);
            CHECK(fabs(p[1u << q] / 65536.0 - sin(th / 2) * sin(th / 2)) <= tol);
            /* H RZ(theta) H |0>: P(1) = sin^2(theta / 2) (interference of the two phases) */
            tk_qsv_reset(st); tk_qsv_h(st, q); tk_qsv_rz(st, q, a); tk_qsv_h(st, q); tk_qsv_probs(st, p);
            CHECK(fabs(p[1u << q] / 65536.0 - sin(th / 2) * sin(th / 2)) <= tol);
        }
    }
}

/* Each output word of H and of a rotation is the exact value rounded to
 * nearest (ties upward). The exact value is computed here in doubles from
 * the integer inputs: every product is below 2^46, so that arithmetic is
 * exact and the comparison is bit for bit. This pins the rounding rule. */
static void test_single_gate_rounding(void)
{
    const double k = (double)llround(Q30 * sqrt(0.5));
    rng_state = 51;
    for (int rep = 0; rep < 40000; rep++) {
        i16 s[16], t[16];
        DState d;
        i32 cs[2];
        random_unit_state(s, &d);
        memcpy(t, s, sizeof t);
        u32 q = rnd_below(3), kind = rnd_below(4);
        tk_qsv_cs(rnd32() & 0x1FFFFu, cs);
        if (kind == 3) tk_qsv_h(s, q); else tk_qsv_rot(s, kind, q, cs);
        int ok = 1;
        for (int i = 0; i < 8; i++) {
            if ((i >> q) & 1) continue;
            int j = i | (1 << q);
            double r0 = t[2 * i], m0 = t[2 * i + 1], r1 = t[2 * j], m1 = t[2 * j + 1], c = cs[0], sn = cs[1];
            double e[4];
            if (kind == 3)               { e[0] = (r0 + r1) * k; e[1] = (m0 + m1) * k; e[2] = (r0 - r1) * k; e[3] = (m0 - m1) * k; }
            else if (kind == TK_QSV_RX)  { e[0] = c * r0 + sn * m1; e[1] = c * m0 - sn * r1; e[2] = c * r1 + sn * m0; e[3] = c * m1 - sn * r0; }
            else if (kind == TK_QSV_RY)  { e[0] = c * r0 - sn * r1; e[1] = c * m0 - sn * m1; e[2] = sn * r0 + c * r1; e[3] = sn * m0 + c * m1; }
            else                         { e[0] = c * r0 + sn * m0; e[1] = c * m0 - sn * r0; e[2] = c * r1 - sn * m1; e[3] = c * m1 + sn * r1; }
            ok &= s[2 * i] == floor(e[0] / Q30 + 0.5) && s[2 * i + 1] == floor(e[1] / Q30 + 0.5);
            ok &= s[2 * j] == floor(e[2] / Q30 + 0.5) && s[2 * j + 1] == floor(e[3] / Q30 + 0.5);
        }
        CHECK(ok);
    }
}

/* ====================================================================== */
/* probabilities and sampling                                             */
/* ====================================================================== */

static void test_probs_and_sampling(void)
{
    i16 s[16];
    u32 p[8];
    DState d;

    /* exact sum and one-unit accuracy on every kind of input */
    rng_state = 31;
    for (int rep = 0; rep < 200000; rep++) {
        if (rep & 1) random_unit_state(s, &d); else random_any_state(s);
        tk_qsv_probs(s, p);
        u64 tot = 0, w[8];
        u32 sum = 0;
        for (int i = 0; i < 8; i++) {
            w[i] = (u64)((i64)s[2 * i] * s[2 * i] + (i64)s[2 * i + 1] * s[2 * i + 1]);
            tot += w[i];
            sum += p[i];
        }
        CHECK_EQ(sum, 65536);
        int ok = 1;
        for (int i = 0; i < 8; i++)
            ok &= tot == 0 ? (p[i] == (i == 0 ? 65536u : 0u))
                           : (fabs((double)p[i] - 65536.0 * (double)w[i] / (double)tot) < 1.0 + 1e-6);
        CHECK(ok);
    }
    memset(s, 0, sizeof s);
    tk_qsv_probs(s, p);
    CHECK_EQ(p[0], 65536);
    CHECK_EQ(tk_qsv_sample(s, 0xFFFFFFFFu), 0);

    /* sampling is the exact inverse CDF: over the 65536 values of r >> 16,
     * outcome i occurs exactly p[i] times, whatever the low 16 bits are */
    rng_state = 32;
    for (int rep = 0; rep < 100; rep++) {
        if (rep % 4 == 3) random_any_state(s); else random_unit_state(s, &d);
        u32 cnt[8] = { 0 };
        tk_qsv_probs(s, p);
        int in_range = 1;
        for (u32 u = 0; u < 65536; u++) {
            u32 o = tk_qsv_sample(s, (u << 16) | (rnd32() & 0xFFFFu));
            in_range &= (o < 8);
            cnt[o & 7]++;
        }
        CHECK(in_range);
        for (int i = 0; i < 8; i++) CHECK_EQ(cnt[i], p[i]);
    }

    /* sampling frequencies from an independent stream of random words match
     * the probabilities (5 sigma; the seed is fixed, so this is repeatable) */
    for (int rep = 0; rep < 20; rep++) {
        const u32 N = 200000;
        u32 cnt[8] = { 0 };
        random_unit_state(s, &d);
        tk_qsv_probs(s, p);
        for (u32 k = 0; k < N; k++) cnt[tk_qsv_sample(s, rnd32()) & 7]++;
        for (int i = 0; i < 8; i++) {
            double pi = p[i] / 65536.0, pd = d.re[i] * d.re[i] + d.im[i] * d.im[i];
            CHECK(fabs(cnt[i] - N * pi) <= 5.0 * sqrt(N * pi * (1 - pi)) + 1.0);
            /* and they match the probabilities of the double state it was rounded from */
            CHECK(fabs(cnt[i] / (double)N - pd) <= 5.0 * sqrt(pd * (1 - pd) / N) + 2e-4);
        }
    }

    /* sampling does not change the state */
    i16 t[16];
    random_unit_state(s, &d);
    memcpy(t, s, sizeof t);
    (void)tk_qsv_sample(s, rnd32());
    tk_qsv_probs(s, p);
    CHECK(memcmp(s, t, sizeof t) == 0);
}

/* ====================================================================== */
/* programs: decoder, interpreter, differential tests                     */
/* ====================================================================== */

static u8 random_gate_byte(void)
{
    u32 g = rnd_below(9), q = rnd_below(3), q2 = (q + 1 + rnd_below(2)) % 3;
    u32 slot = rnd_below(8), inv = rnd_below(2);
    switch (g) {
    case 0: return TK_QSV_OP_H(q);
    case 1: return TK_QSV_OP_X(q);
    case 2: return TK_QSV_OP_Y(q);
    case 3: return TK_QSV_OP_Z(q);
    case 4: return TK_QSV_OP_ROT(TK_QSV_RX, q, slot, inv);
    case 5: return TK_QSV_OP_ROT(TK_QSV_RY, q, slot, inv);
    case 6: return TK_QSV_OP_ROT(TK_QSV_RZ, q, slot, inv);
    case 7: return TK_QSV_OP_CNOT(q, q2);
    default: return TK_QSV_OP_CZ(q, q2);
    }
}

static void random_angles(u16 ang[8])
{
    static const u16 special[8] = { 0, 16384, 32768, 49152, 65535, 1, 2, 8192 };
    u32 mode = rnd_below(8);
    for (int i = 0; i < 8; i++) ang[i] = mode == 0 ? special[rnd_below(8)] : (u16)rnd32();
}

static void test_decode(void)
{
    u32 count[TK_QSV_G_COUNT] = { 0 };
    for (unsigned op = 0; op < 256; op++) {
        TkQsvOp k = tk_qsv_decode((u8)op);
        RefOp r = ref_decode(op);
        CHECK(k.gate == r.gate && k.q0 == r.q0 && k.q1 == r.q1 && k.slot == r.slot && k.inv == r.inv);
        CHECK(k.gate < TK_QSV_G_COUNT && k.q0 < 3 && k.q1 < 3 && k.slot < 8 && k.inv < 2);
        count[k.gate < TK_QSV_G_COUNT ? k.gate : 0]++;
    }
    /* 180 of the 256 byte values are gates */
    CHECK_EQ(count[TK_QSV_G_NOP], 76);
    CHECK_EQ(count[TK_QSV_G_H], 6); CHECK_EQ(count[TK_QSV_G_X], 6);
    CHECK_EQ(count[TK_QSV_G_Y], 6); CHECK_EQ(count[TK_QSV_G_Z], 6);
    CHECK_EQ(count[TK_QSV_G_RX], 48); CHECK_EQ(count[TK_QSV_G_RY], 48); CHECK_EQ(count[TK_QSV_G_RZ], 48);
    CHECK_EQ(count[TK_QSV_G_CNOT], 6); CHECK_EQ(count[TK_QSV_G_CZ], 6);
    CHECK_EQ(tk_qsv_decode(TK_QSV_OP_NOP).gate, TK_QSV_G_NOP);

    /* the encoders */
    for (u32 q = 0; q < 3; q++) {
        TkQsvOp k = tk_qsv_decode(TK_QSV_OP_H(q)); CHECK(k.gate == TK_QSV_G_H && k.q0 == q);
        k = tk_qsv_decode(TK_QSV_OP_X(q)); CHECK(k.gate == TK_QSV_G_X && k.q0 == q);
        k = tk_qsv_decode(TK_QSV_OP_Y(q)); CHECK(k.gate == TK_QSV_G_Y && k.q0 == q);
        k = tk_qsv_decode(TK_QSV_OP_Z(q)); CHECK(k.gate == TK_QSV_G_Z && k.q0 == q);
        for (u32 slot = 0; slot < 8; slot++) {
            k = tk_qsv_decode(TK_QSV_OP_RX(q, slot)); CHECK(k.gate == TK_QSV_G_RX && k.q0 == q && k.slot == slot && !k.inv);
            k = tk_qsv_decode(TK_QSV_OP_RY(q, slot)); CHECK(k.gate == TK_QSV_G_RY && k.q0 == q && k.slot == slot && !k.inv);
            k = tk_qsv_decode(TK_QSV_OP_RZ(q, slot)); CHECK(k.gate == TK_QSV_G_RZ && k.q0 == q && k.slot == slot && !k.inv);
            k = tk_qsv_decode(TK_QSV_OP_ROT(TK_QSV_RY, q, slot, 1)); CHECK(k.gate == TK_QSV_G_RY && k.inv == 1);
        }
        for (u32 t = 0; t < 3; t++) {
            if (t == q) continue;
            k = tk_qsv_decode(TK_QSV_OP_CNOT(q, t)); CHECK(k.gate == TK_QSV_G_CNOT && k.q0 == q && k.q1 == t);
            k = tk_qsv_decode(TK_QSV_OP_CZ(q, t));
            CHECK(k.gate == TK_QSV_G_CZ && k.q0 == (q < t ? q : t) && k.q1 == (q < t ? t : q));
        }
    }
}

enum { TRIG_PAIR = 0, TRIG_TRUE = 1 };

/* one op on the double reference. Returns 1 if the fixed-point gate rounds
 * (H or a rotation); *is_rot is set for rotations. */
static int ref_apply(DState *d, unsigned op, const u16 *ang, int mode, int *is_rot)
{
    RefOp r = ref_decode(op);
    *is_rot = 0;
    switch (r.gate) {
    case TK_QSV_G_H: d_fixed_gate(d, r.gate, r.q0); return 1;
    case TK_QSV_G_X: case TK_QSV_G_Y: case TK_QSV_G_Z: d_fixed_gate(d, r.gate, r.q0); return 0;
    case TK_QSV_G_CNOT: d_cnot(d, r.q0, r.q1); return 0;
    case TK_QSV_G_CZ: d_cz(d, r.q0, r.q1); return 0;
    case TK_QSV_G_RX: case TK_QSV_G_RY: case TK_QSV_G_RZ: {
        double c, s;
        if (mode == TRIG_PAIR) {
            i32 cs[2];
            tk_qsv_cs(ang[r.slot], cs);
            c = cs[0] / Q30; s = cs[1] / Q30;
        } else {
            double half = TWO_PI * (double)(ang[r.slot] >> 1) / 65536.0;
            c = cos(half); s = sin(half);
        }
        d_rot(d, r.gate, r.q0, c, r.inv ? -s : s);
        *is_rot = 1;
        return 1;
    }
    default: return 0;
    }
}

/* Test-side interpreter built from the public gate functions and the
 * independent decoder, with the renormalisation cadence tk_qsv.h documents.
 * ideal = 0 uses the linked az_sin_q14 (must equal tk_qsv_run bit for bit);
 * ideal = 1 injects a rounded exact table, to show the bounds also hold for
 * a table-based az_sin_q14. */
static void t_run(i16 *s, const u8 *prog, u32 n, const u16 *ang, int ideal)
{
    for (u32 k = 0; k < n; k++) {
        RefOp r = ref_decode(prog[k]);
        switch (r.gate) {
        case TK_QSV_G_H: tk_qsv_h(s, (u32)r.q0); break;
        case TK_QSV_G_X: tk_qsv_x(s, (u32)r.q0); break;
        case TK_QSV_G_Y: tk_qsv_y(s, (u32)r.q0); break;
        case TK_QSV_G_Z: tk_qsv_z(s, (u32)r.q0); break;
        case TK_QSV_G_CNOT: tk_qsv_cnot(s, (u32)r.q0, (u32)r.q1); break;
        case TK_QSV_G_CZ: tk_qsv_cz(s, (u32)r.q0, (u32)r.q1); break;
        case TK_QSV_G_RX: case TK_QSV_G_RY: case TK_QSV_G_RZ: {
            i32 cs[2];
            if (ideal) ideal_cs(ang[r.slot], cs); else tk_qsv_cs(ang[r.slot], cs);
            if (r.inv) cs[1] = -cs[1];
            tk_qsv_rot(s, (u32)(r.gate - TK_QSV_G_RX), (u32)r.q0, cs);
            break;
        }
        default: break;
        }
        if ((k + 1) % TK_QSV_RENORM_PERIOD == 0) tk_qsv_renorm(s);
    }
    if (n % TK_QSV_RENORM_PERIOD) tk_qsv_renorm(s);
}

#define N_CIRCUITS 12000
#define MAX_OPS    32

static void test_random_circuits(void)
{
    double max_l2_pair = 0, max_l2_true = 0, max_l2_ideal = 0, max_use = 0;
    double max_dp_pair = 0, max_dp_true = 0, max_dp_ideal = 0, sum_dp_pair = 0, sum_dp_true = 0;
    u64 total_ops = 0, total_gates = 0;
    rng_state = 41;
    for (int c = 0; c < N_CIRCUITS; c++) {
        u8 prog[MAX_OPS];
        u16 ang[8];
        i16 s[16], s0[16], st[16], si[16];
        DState d0, dp, dt;
        u32 n = 1 + rnd_below(MAX_OPS);
        for (u32 k = 0; k < n; k++) prog[k] = (c & 1) ? random_gate_byte() : (u8)rnd32();
        random_angles(ang);
        double budget = 0;
        if (c % 3 == 2) { random_unit_state(s0, &d0); budget = 2.05; }
        else { tk_qsv_reset(s0); d_reset(&d0); }

        memcpy(s, s0, sizeof s);
        tk_qsv_run(s, prog, n, ang);

        /* tk_qsv_run is exactly: decode, gate, renormalise every 16 ops and at the end */
        memcpy(st, s0, sizeof st);
        t_run(st, prog, n, ang, 0);
        CHECK(memcmp(s, st, sizeof s) == 0);

        dp = d0; dt = d0;
        u32 n_round = 0, n_rot = 0;
        for (u32 k = 0; k < n; k++) {
            int is_rot, dummy;
            n_round += (u32)ref_apply(&dp, prog[k], ang, TRIG_PAIR, &is_rot);
            n_rot += (u32)is_rot;
            (void)ref_apply(&dt, prog[k], ang, TRIG_TRUE, &dummy);
            total_gates += ref_decode(prog[k]).gate != TK_QSV_G_NOP;
        }
        total_ops += n;
        const u32 n_renorm = n / TK_QSV_RENORM_PERIOD + (n % TK_QSV_RENORM_PERIOD != 0);
        budget = (budget + 2.05 * (n_round + n_renorm)) * 1.01;

        /* B1: same-pair reference */
        double e = amp_l2_lsb(s, &dp), dpp = max_dp(s, &dp);
        CHECK(e <= budget);
        max_l2_pair = fmax(max_l2_pair, e); max_use = fmax(max_use, e / budget);
        max_dp_pair = fmax(max_dp_pair, dpp); sum_dp_pair += dpp;

        /* B2: true sin/cos reference, with the linked sine */
        e = amp_l2_lsb(s, &dt); dpp = max_dp(s, &dt);
        CHECK(e <= budget + 1.01 * g_dmax * n_rot * Q14);
        max_l2_true = fmax(max_l2_true, e);
        max_dp_true = fmax(max_dp_true, dpp); sum_dp_true += dpp;

        /* B2 again with an injected rounded table instead of the linked sine */
        memcpy(si, s0, sizeof si);
        t_run(si, prog, n, ang, 1);
        e = amp_l2_lsb(si, &dt); dpp = max_dp(si, &dt);
        CHECK(e <= budget + 1.01 * g_dmax_ideal * n_rot * Q14);
        max_l2_ideal = fmax(max_l2_ideal, e);
        max_dp_ideal = fmax(max_dp_ideal, dpp);

        /* the result is a unit vector to within rounding, and probabilities sum exactly */
        u32 p[8], sum = 0;
        tk_qsv_probs(s, p);
        for (int i = 0; i < 8; i++) sum += p[i];
        CHECK_EQ(sum, 65536);
        CHECK(fabs(fixed_norm(s) - 1.0) <= 3e-4);
    }
    printf("random circuits: %d circuits, %llu op bytes, %llu gates (1..%d ops each)\n",
           N_CIRCUITS, (unsigned long long)total_ops, (unsigned long long)total_gates, MAX_OPS);
    printf("  vs same-pair double reference: max amp L2 = %.2f LSB (%.0f%% of bound B1), max |dP| = %.3e, mean max|dP| = %.3e\n",
           max_l2_pair, 100.0 * max_use, max_dp_pair, sum_dp_pair / N_CIRCUITS);
    printf("  vs true sin/cos, linked sine : max amp L2 = %.2f LSB, max |dP| = %.3e, mean max|dP| = %.3e\n",
           max_l2_true, max_dp_true, sum_dp_true / N_CIRCUITS);
    printf("  vs true sin/cos, rounded table: max amp L2 = %.2f LSB, max |dP| = %.3e\n", max_l2_ideal, max_dp_ideal);
    /* B3: observed caps */
    CHECK(max_dp_pair <= 1.0e-3);
    CHECK(max_dp_ideal <= 1.0e-3);
    CHECK(max_dp_true <= 0.01);
    /* n_ops = 0 leaves the state untouched, whatever it holds */
    i16 a[16], b[16];
    u16 ang0[8] = { 0 };
    random_any_state(a);
    memcpy(b, a, sizeof b);
    tk_qsv_run(a, (const u8 *)"", 0, ang0);
    CHECK(memcmp(a, b, sizeof a) == 0);
}

/* ====================================================================== */
/* B4: norm over long gate sequences                                      */
/* ====================================================================== */

static void test_norm(void)
{
    i16 s[16];
    u16 ang[8];
    double worst = 0;
    for (int seed = 0; seed < 20; seed++) {
        rng_state = 1 + (u64)seed;   /* tuning seeds 1..20; 5001..5024/5101..5124 are reserved */
        tk_qsv_reset(s);
        random_angles(ang);
        for (int g = 0; g < 10000; g++) {
            if (g % 97 == 0) random_angles(ang);
            tk_qsv_apply(s, (seed & 1) ? random_gate_byte() : (u8)rnd32(), ang);
            if ((g + 1) % 16 == 0) tk_qsv_renorm(s);
            double dev = fabs(fixed_norm(s) - 1.0);
            worst = fmax(worst, dev);
            CHECK(dev <= 0.02);
        }
    }
    printf("norm: 20 x 10,000 random gates, renorm every 16: max | |psi| - 1 | = %.3e (bound 0.02)\n", worst);

    /* the same through tk_qsv_run with one 10,000-byte program */
    static u8 prog[10000];
    rng_state = 91;
    for (int k = 0; k < 10000; k++) prog[k] = random_gate_byte();
    random_angles(ang);
    tk_qsv_reset(s);
    tk_qsv_run(s, prog, 10000, ang);
    CHECK(fabs(fixed_norm(s) - 1.0) <= 3e-4);

    /* systematic case: the same rounding gate 16 times in a row without a
     * renormalisation, for every rotation kind and 1024 angles, and H */
    double worst_rep = 0;
    DState d;
    rng_state = 92;
    for (u32 a = 0; a < 65536; a += 64) {
        for (u32 kind = 0; kind < 4; kind++) {
            random_unit_state(s, &d);
            u32 q = rnd_below(3);
            for (int k = 0; k < 16; k++) {
                if (kind == 0) tk_qsv_rx(s, q, a); else if (kind == 1) tk_qsv_ry(s, q, a);
                else if (kind == 2) tk_qsv_rz(s, q, a); else tk_qsv_h(s, q);
            }
            worst_rep = fmax(worst_rep, fabs(fixed_norm(s) - 1.0));
        }
    }
    printf("norm: one rounding gate repeated 16 times, no renorm: max | |psi| - 1 | = %.3e\n", worst_rep);
    CHECK(worst_rep <= 0.02);

    /* information only: the drift without any renormalisation */
    rng_state = 93;
    tk_qsv_reset(s);
    random_angles(ang);
    for (int g = 0; g < 10000; g++) tk_qsv_apply(s, random_gate_byte(), ang);
    printf("norm: 10,000 random gates with no renorm at all: |psi| - 1 = %+.3e (not checked)\n", fixed_norm(s) - 1.0);
}

/* ====================================================================== */
/* cross-check against the upstream Moonlab dense kernel                  */
/* ====================================================================== */

#define N_UPSTREAM 400

static void test_upstream_moonlab(void)
{
    quantum_state_t ms;
    int ok_init = quantum_state_init(&ms, 3) == QS_SUCCESS;
    CHECK(ok_init);
    if (!ok_init) return;
    CHECK_EQ(ms.state_dim, 8);

    double max_ref_diff = 0, max_ref_diff_pair = 0, max_l2_pair = 0, max_dp_pair = 0, max_dp_true = 0;
    u64 gates = 0;
    rng_state = 61;
    for (int c = 0; c < N_UPSTREAM; c++) {
        const int mode = (c & 1) ? TRIG_TRUE : TRIG_PAIR;
        u8 prog[MAX_OPS];
        u16 ang[8];
        i16 s[16];
        DState d0, dr, dm;
        u32 n = 1 + rnd_below(MAX_OPS);
        for (u32 k = 0; k < n; k++) prog[k] = (c & 2) ? random_gate_byte() : (u8)rnd32();
        random_angles(ang);
        double budget = 0;
        quantum_state_reset(&ms);
        if (c % 3 == 2) {
            random_unit_state(s, &d0);
            budget = 2.05;
            for (int i = 0; i < 8; i++) moonlab_oracle_set(&ms, i, d0.re[i], d0.im[i]);
        } else {
            tk_qsv_reset(s);
            d_reset(&d0);
        }
        tk_qsv_run(s, prog, n, ang);

        dr = d0;
        u32 n_round = 0, n_rot = 0;
        int rc_ok = 1;
        for (u32 k = 0; k < n; k++) {
            RefOp r = ref_decode(prog[k]);
            double theta = 0;
            if (r.gate == TK_QSV_G_RX || r.gate == TK_QSV_G_RY || r.gate == TK_QSV_G_RZ) {
                if (mode == TRIG_PAIR) {
                    i32 cs[2];
                    tk_qsv_cs(ang[r.slot], cs);
                    theta = 2.0 * atan2(cs[1], cs[0]);
                } else {
                    theta = 2.0 * TWO_PI * (double)(ang[r.slot] >> 1) / 65536.0;
                }
                if (r.inv) theta = -theta;
            }
            switch (r.gate) {
            case TK_QSV_G_H: rc_ok &= gate_hadamard(&ms, r.q0) == QS_SUCCESS; break;
            case TK_QSV_G_X: rc_ok &= gate_pauli_x(&ms, r.q0) == QS_SUCCESS; break;
            case TK_QSV_G_Y: rc_ok &= gate_pauli_y(&ms, r.q0) == QS_SUCCESS; break;
            case TK_QSV_G_Z: rc_ok &= gate_pauli_z(&ms, r.q0) == QS_SUCCESS; break;
            case TK_QSV_G_RX: rc_ok &= gate_rx(&ms, r.q0, theta) == QS_SUCCESS; break;
            case TK_QSV_G_RY: rc_ok &= gate_ry(&ms, r.q0, theta) == QS_SUCCESS; break;
            case TK_QSV_G_RZ: rc_ok &= gate_rz(&ms, r.q0, theta) == QS_SUCCESS; break;
            case TK_QSV_G_CNOT: rc_ok &= gate_cnot(&ms, r.q0, r.q1) == QS_SUCCESS; break;
            case TK_QSV_G_CZ: rc_ok &= gate_cz(&ms, r.q0, r.q1) == QS_SUCCESS; break;
            default: break;
            }
            int is_rot;
            n_round += (u32)ref_apply(&dr, prog[k], ang, mode, &is_rot);
            n_rot += (u32)is_rot;
            gates += r.gate != TK_QSV_G_NOP;
        }
        CHECK(rc_ok);
        double ref_diff = 0;
        for (int i = 0; i < 8; i++) {
            dm.re[i] = moonlab_oracle_re(&ms, i);
            dm.im[i] = moonlab_oracle_im(&ms, i);
            ref_diff = fmax(ref_diff, fmax(fabs(dm.re[i] - dr.re[i]), fabs(dm.im[i] - dr.im[i])));
        }
        /* The reference written in this file and the upstream kernel agree
         * amplitude by amplitude, so the gate conventions are the same. With
         * nominal angles both use the same double sin/cos. In pair mode the
         * reference multiplies by the kernel's Q1.30 pair, whose length is
         * within 2e-9 of 1 (checked in test_trig_and_unit), while upstream
         * gets the pair's angle: allow that much per rotation. */
        CHECK(ref_diff <= 1e-12 + (mode == TRIG_PAIR ? 2e-9 * n_rot : 0.0));
        if (mode == TRIG_TRUE) max_ref_diff = fmax(max_ref_diff, ref_diff);
        else max_ref_diff_pair = fmax(max_ref_diff_pair, ref_diff);

        /* the fixed-point kernel against the upstream kernel: B1 / B2 */
        const u32 n_renorm = n / TK_QSV_RENORM_PERIOD + (n % TK_QSV_RENORM_PERIOD != 0);
        budget = (budget + 2.05 * (n_round + n_renorm)) * 1.01;
        double e = amp_l2_lsb(s, &dm), dpp = max_dp(s, &dm);
        if (mode == TRIG_PAIR) {
            CHECK(e <= budget);
            max_l2_pair = fmax(max_l2_pair, e);
            max_dp_pair = fmax(max_dp_pair, dpp);
        } else {
            CHECK(e <= budget + 1.01 * g_dmax * n_rot * Q14);
            max_dp_true = fmax(max_dp_true, dpp);
        }
    }
    quantum_state_free(&ms);
    printf("upstream Moonlab kernel: %d circuits, %llu gates; own reference vs upstream max |d amp| = %.2e"
           " (nominal angles), %.2e (pair angles)\n",
           N_UPSTREAM, (unsigned long long)gates, max_ref_diff, max_ref_diff_pair);
    printf("  fixed point vs upstream, same angles as the kernel's pair: max amp L2 = %.2f LSB, max |dP| = %.3e\n",
           max_l2_pair, max_dp_pair);
    printf("  fixed point vs upstream, nominal angles (linked sine)    : max |dP| = %.3e\n", max_dp_true);
    CHECK(max_dp_pair <= 1.0e-3);
    CHECK(max_dp_true <= 0.01);
}

/* ====================================================================== */
/* golden hash: pins the integer arithmetic bit for bit                   */
/* ====================================================================== */

/* pinned 2026-11-17 (findings v13, v29): gcc 16.1 -O2 and clang 21.1.8 -O2
 * for x86_64-w64-windows-gnu, which evaluates call arguments in the opposite
 * order, both print this value. Before the two draws below were split into
 * separate statements, that clang build printed 76fea5652eb1e62a. */
#define GOLDEN_HASH 0x8e989569204d8c1full

/* A fixed pseudo-random sequence of gates with integer-generated unit pairs
 * (no az_sin_q14, no libm), renormalised every 16 gates, with probabilities
 * and samples folded into an FNV-1a hash. The value must be the same for
 * every compiler, optimisation level and sine implementation. */
static void test_golden(void)
{
    i16 s[16];
    u64 h = 1469598103934665603ull;
    rng_state = 81;
    tk_qsv_reset(s);
    for (u32 k = 0; k < 100000; k++) {
        u32 g = rnd_below(9), q = rnd_below(3), q2 = (q + 1 + rnd_below(2)) % 3;
        i32 cs[2];
        /* Two draws, made one statement at a time: C leaves the order in which
         * a call's arguments are evaluated unspecified (finding v29). The y
         * value is drawn first, the order gcc 16 used when both draws were
         * arguments, so the hash that build printed stays the golden value. */
        const i32 uy = (i32)rnd_below(32768) - 16384;
        const i32 ux = (i32)rnd_below(32768) - 16384;
        tk_qsv_unit(ux, uy, cs);
        switch (g) {
        case 0: tk_qsv_h(s, q); break;
        case 1: tk_qsv_x(s, q); break;
        case 2: tk_qsv_y(s, q); break;
        case 3: tk_qsv_z(s, q); break;
        case 4: case 5: case 6: tk_qsv_rot(s, g - 4, q, cs); break;
        case 7: tk_qsv_cnot(s, q, q2); break;
        default: tk_qsv_cz(s, q, q2); break;
        }
        if ((k & 15u) == 15u) tk_qsv_renorm(s);
        if ((k & 255u) == 255u) {
            u32 p[8];
            tk_qsv_probs(s, p);
            for (int i = 0; i < 8; i++) { h ^= p[i]; h *= 1099511628211ull; }
            for (int i = 0; i < 16; i++) { h ^= (u16)s[i]; h *= 1099511628211ull; }
            h ^= tk_qsv_sample(s, rnd32()); h *= 1099511628211ull;
        }
    }
    printf("golden hash: %016llx\n", (unsigned long long)h);
    CHECK(h == GOLDEN_HASH);
}

/* ====================================================================== */
/* timing, information only                                               */
/* ====================================================================== */

static double now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

static void report_timing(void)
{
    enum { NPROG = 64, REPS = 4000 };
    static u8 prog[NPROG][16];
    static u16 ang[NPROG][8];
    i16 s[16];
    u32 p[8], sink = 0;
    rng_state = 71;
    for (int i = 0; i < NPROG; i++) {
        for (int k = 0; k < 16; k++) prog[i][k] = random_gate_byte();
        random_angles(ang[i]);
    }
    tk_qsv_reset(s);
    double t0 = now_ns();
    for (int r = 0; r < REPS; r++)
        for (int i = 0; i < NPROG; i++) { tk_qsv_run(s, prog[i], 16, ang[i]); sink += (u32)s[0]; }
    double t1 = now_ns();
    for (int r = 0; r < REPS * NPROG; r++) sink += tk_qsv_sample(s, (u32)r * 2654435761u);
    double t2 = now_ns();
    for (int r = 0; r < REPS * NPROG; r++) { tk_qsv_probs(s, p); sink += p[r & 7]; s[0] = (i16)(s[0] ^ (r & 1)); }
    double t3 = now_ns();
    printf("timing (noisy, parallel load, one thread): run of 16 gates + renorm %.0f ns, sample %.1f ns, probs %.1f ns  [%u]\n",
           (t1 - t0) / (REPS * NPROG), (t2 - t1) / (REPS * NPROG), (t3 - t2) / (REPS * NPROG), sink & 1u);
}

int main(void)
{
    test_reset_and_exact_gates();
    test_textbook_states();
    test_trig_and_unit();
    test_single_gate_rounding();
    test_probs_and_sampling();
    test_decode();
    test_random_circuits();
    test_norm();
    test_upstream_moonlab();
    test_golden();
    report_timing();
    return az_test_done();
}
