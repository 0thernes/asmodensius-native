/* test_fx.c - core/az_fx.c against double precision and against the
 * reference stub (owner: fx).
 *
 *   1. az_isqrt_u64: exact on every edge and on 1,000,000 random values.
 *   2. az_sin_q14 / az_cos_q14: error at all 65536 angles, exact symmetries.
 *   3. az_log2_q8, az_exp2_q8, az_div_round: bounds, monotonicity, saturation.
 *   4. az_radius_code, az_tetra4: equal to the reference stub.
 *   5. rotors: norm, step, apply / apply_inv, mul against double precision.
 *   6. timings (printed only; noisy, parallel load).
 *
 * The stub is compiled into this file under stub_* names. */

/* gcc on mingw-w64 does not keep the stack 32-byte aligned, so AVX code that
 * spills a 256-bit temporary (a struct of four doubles passed by value)
 * faults under -march=x86-64-v3. Nothing in this test needs AVX. */
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC target("no-avx")
#endif

#include "az_test.h"
#include <math.h>
#include <time.h>

u32  stub_isqrt_u64(u64 x);
i32  stub_sin_q14(u32 angle);
i32  stub_cos_q14(u32 angle);
u32  stub_log2_q8(u32 x);
u32  stub_exp2_q8(u32 e);
u32  stub_div_round(u64 num, u32 den);
u32  stub_radius_code(u32 mass);
void stub_tetra4(const i32 v[3], u32 shift, u8 out[4]);
void stub_rotor_norm(i16 r[4]);
void stub_rotor_mul(i16 out[4], const i16 a[4], const i16 b[4]);
void stub_rotor_apply(i32 out[3], const i16 r[4], const i32 v[3]);
void stub_rotor_apply_inv(i32 out[3], const i16 r[4], const i32 v[3]);
void stub_rotor_step(i16 r[4], const i16 ang[3]);

#define az_isqrt_u64       stub_isqrt_u64
#define az_sin_q14         stub_sin_q14
#define az_cos_q14         stub_cos_q14
#define az_log2_q8         stub_log2_q8
#define az_exp2_q8         stub_exp2_q8
#define az_div_round       stub_div_round
#define az_radius_code     stub_radius_code
#define az_tetra4          stub_tetra4
#define az_rotor_norm      stub_rotor_norm
#define az_rotor_mul       stub_rotor_mul
#define az_rotor_apply     stub_rotor_apply
#define az_rotor_apply_inv stub_rotor_apply_inv
#define az_rotor_step      stub_rotor_step
#include "../core/stubs/az_fx.c"
#undef az_isqrt_u64
#undef az_sin_q14
#undef az_cos_q14
#undef az_log2_q8
#undef az_exp2_q8
#undef az_div_round
#undef az_radius_code
#undef az_tetra4
#undef az_rotor_norm
#undef az_rotor_mul
#undef az_rotor_apply
#undef az_rotor_apply_inv
#undef az_rotor_step

typedef unsigned __int128 u128;
#define TEST_PI 3.14159265358979323846

/* ---- deterministic test randomness (splitmix64) ---- */
static u64 rng_state = 0x243F6A8885A308D3ull;
static u64 r64(void) { return az_test_rand64(&rng_state); }
static u32 r32(void) { return (u32)(r64() >> 32); }
static i32 rrange(i32 lo, i32 hi) { return lo + (i32)(r64() % (u64)((i64)hi - (i64)lo + 1)); }
/* a u64 whose bit length is uniform in 0..64 */
static u64 r64_any_width(void) { u32 s = r32() % 65u; return s == 64 ? 0 : r64() >> s; }
static u32 r32_any_width(void) { u32 s = r32() % 33u; return s == 32 ? 0 : r32() >> s; }

/* ====================================================================== */
/* 1. isqrt                                                                */
/* ====================================================================== */
static int isqrt_exact(u64 x)
{
    u32 r = az_isqrt_u64(x);
    return (u128)r * r <= x && (u128)((u64)r + 1) * ((u64)r + 1) > x;
}

static void test_isqrt(void)
{
    for (u64 x = 0; x <= 70000; x++) CHECK(isqrt_exact(x));
    CHECK_EQ(az_isqrt_u64(0), 0); CHECK_EQ(az_isqrt_u64(1), 1); CHECK_EQ(az_isqrt_u64(3), 1);
    CHECK_EQ(az_isqrt_u64(4), 2); CHECK_EQ(az_isqrt_u64(0xFFFFFFFFFFFFFFFFull), 0xFFFFFFFFu);
    CHECK_EQ(az_isqrt_u64(0xFFFFFFFE00000001ull), 0xFFFFFFFFu);      /* (2^32-1)^2     */
    CHECK_EQ(az_isqrt_u64(0xFFFFFFFE00000000ull), 0xFFFFFFFEu);      /* (2^32-1)^2 - 1 */
    CHECK_EQ(az_isqrt_u64(1ull << 62), 1u << 31);
    CHECK_EQ(az_isqrt_u64((1ull << 62) - 1), (1u << 31) - 1);
    /* powers of two and their neighbours */
    for (u32 j = 0; j < 64; j++) {
        u64 p = 1ull << j;
        CHECK(isqrt_exact(p)); CHECK(isqrt_exact(p - 1)); CHECK(isqrt_exact(p + 1));
        CHECK(isqrt_exact(p + (p >> 1))); CHECK(isqrt_exact(p | (p - 1)));
    }
    /* perfect squares and their neighbours, roots of every bit length */
    for (u32 j = 0; j < 32; j++) {
        for (i32 d = -40; d <= 40; d++) {
            i64 k = (i64)(1ull << j) + d;
            if (k < 1 || k > 0xFFFFFFFFll) continue;
            u64 sq = (u64)k * (u64)k;
            CHECK_EQ(az_isqrt_u64(sq), k);
            CHECK_EQ(az_isqrt_u64(sq - 1), k - 1);
            CHECK(isqrt_exact(sq + 1));
            CHECK_EQ(az_isqrt_u64(sq + 2 * (u64)k), k);              /* (k+1)^2 - 1 */
        }
    }
    for (u32 d = 0; d < 3000; d++) {
        u64 k = 0xFFFFFFFFull - d, sq = k * k;
        CHECK_EQ(az_isqrt_u64(sq), k); CHECK_EQ(az_isqrt_u64(sq - 1), k - 1);
        CHECK(isqrt_exact(0xFFFFFFFFFFFFFFFFull - d));
    }
    /* random roots: k^2 - 1, k^2, k^2 + 2k */
    for (u32 n = 0; n < 200000; n++) {
        u64 k = r32_any_width();
        if (k == 0) k = 1;
        u64 sq = k * k;
        CHECK_EQ(az_isqrt_u64(sq), k); CHECK_EQ(az_isqrt_u64(sq - 1), k - 1);
        CHECK_EQ(az_isqrt_u64(sq + 2 * k), k);
    }
    /* 1,000,000 random values of every bit length; also equal to the stub */
    for (u32 n = 0; n < 1000000; n++) {
        u64 x = (n & 1) ? r64() : r64_any_width();
        CHECK(isqrt_exact(x));
        CHECK_EQ(az_isqrt_u64(x), stub_isqrt_u64(x));
    }
}

/* ====================================================================== */
/* 2. sine and cosine                                                      */
/* ====================================================================== */
static void test_trig(void)
{
    CHECK_EQ(az_sin_q14(0), 0);          CHECK_EQ(az_cos_q14(0), 16384);
    CHECK_EQ(az_sin_q14(16384), 16384);  CHECK_EQ(az_cos_q14(16384), 0);
    CHECK_EQ(az_sin_q14(32768), 0);      CHECK_EQ(az_cos_q14(32768), -16384);
    CHECK_EQ(az_sin_q14(49152), -16384); CHECK_EQ(az_cos_q14(49152), 0);
    CHECK_EQ(az_sin_q14(65536), 0);      CHECK_EQ(az_cos_q14(65536), 16384);

    double max_sin = 0, max_cos = 0, max_stub = 0;
    for (u32 a = 0; a < 65536; a++) {
        double x = 2.0 * TEST_PI * (double)a / 65536.0;
        i32 s = az_sin_q14(a), c = az_cos_q14(a);
        double es = fabs((double)s - 16384.0 * sin(x)), ec = fabs((double)c - 16384.0 * cos(x));
        double eb = fabs((double)stub_sin_q14(a) - 16384.0 * sin(x));
        if (es > max_sin) max_sin = es;
        if (ec > max_cos) max_cos = ec;
        if (eb > max_stub) max_stub = eb;
        CHECK(es <= 2.0);
        CHECK(ec <= 2.0);
        CHECK(s >= -16384 && s <= 16384);
        /* exact symmetries */
        CHECK_EQ(az_sin_q14(a + 32768u), -s);                /* half-turn antisymmetry */
        CHECK_EQ(c, az_sin_q14(a + 16384u));                 /* cosine is a shifted sine */
        CHECK_EQ(az_sin_q14(65536u - a), -s);                /* odd */
        CHECK_EQ(az_sin_q14(32768u - a), s);                 /* mirror about the quarter turn */
        CHECK_EQ(az_cos_q14(65536u - a), c);                 /* even */
        CHECK_EQ(az_cos_q14(a + 32768u), -c);
        CHECK_EQ(az_sin_q14(a + 16384u), az_cos_q14(a));
        CHECK_EQ(az_sin_q14(a + 0xABCD0000u), s);            /* only the low 16 bits count */
        /* sin^2 + cos^2 = 1 within the rounding of both terms */
        i64 pyth = (i64)s * s + (i64)c * c - ((i64)1 << 28);
        CHECK(pyth >= -4 * 16384 && pyth <= 4 * 16384);
    }
    for (u32 a = 0; a < 16384; a++) CHECK(az_sin_q14(a + 1) >= az_sin_q14(a));   /* monotone quarter */
    printf("sin/cos: max error %.3f / %.3f units of 1/16384 (stub parabola: %.2f)\n", max_sin, max_cos, max_stub);
    CHECK(max_sin <= 1.25);    /* table rounding 0.5 + interpolation rounding 0.5 + chord 0.08 */
    CHECK(max_cos <= 1.25);
}

/* ====================================================================== */
/* 3. log2, exp2, div_round                                                */
/* ====================================================================== */
static double max_log_err;

static void log_point(u32 x)
{
    u32 g = az_log2_q8(x);
    double e = fabs((double)g - 256.0 * log2((double)x));
    if (e > max_log_err) max_log_err = e;
    CHECK(e <= 2.0);
    CHECK_EQ(g >> 8, 31 - __builtin_clz(x));       /* integer part is floor(log2 x) */
}

static void test_log_exp(void)
{
    CHECK_EQ(az_log2_q8(0), 0);
    CHECK_EQ(az_log2_q8(1), 0);
    for (u32 k = 0; k < 32; k++) {
        CHECK_EQ(az_log2_q8(1u << k), 256 * k);     /* exact at powers of two */
        CHECK_EQ(az_exp2_q8(256 * k), 1u << k);
        log_point((1u << k) | ((1u << k) - 1));
        log_point((1u << k) + 1 > (1u << k) ? (1u << k) + 1 : 1u << k);
    }
    log_point(0xFFFFFFFFu); log_point(0xFFFFFFFEu); log_point(0x80000001u);
    /* exhaustive up to 2^20: error, integer part, monotone */
    u32 prev = 0;
    for (u32 x = 1; x <= (1u << 20); x++) {
        log_point(x);
        u32 g = az_log2_q8(x);
        CHECK(g >= prev);
        prev = g;
    }
    /* windows of consecutive large values: monotone across octave and table-step borders */
    for (u32 n = 0; n < 400; n++) {
        u32 base = n < 12 ? (1u << (20 + n)) - 2048u : r32_any_width();
        if (base > 0xFFFFFFFFu - 4096u) base = 0xFFFFFFFFu - 4096u;
        if (base == 0) base = 1;
        u32 p = az_log2_q8(base);
        for (u32 d = 1; d <= 4096; d++) {
            u32 g = az_log2_q8(base + d);
            CHECK(g >= p);
            p = g;
        }
    }
    /* random values and random ordered pairs */
    for (u32 n = 0; n < 1000000; n++) {
        u32 x = (n & 1) ? r32() : r32_any_width();
        if (x == 0) x = 1;
        log_point(x);
        u32 y = r32_any_width();
        if (y == 0) y = 1;
        if (x <= y) CHECK(az_log2_q8(x) <= az_log2_q8(y)); else CHECK(az_log2_q8(y) <= az_log2_q8(x));
    }
    printf("log2_q8: max error %.4f units of 1/256 bit\n", max_log_err);
    CHECK(max_log_err <= 1.0);

    /* exp2: every exponent up to past the saturation point */
    double max_abs = 0, max_rel_big = 0;
    u32 last = 0;
    for (u32 e = 0; e < 8192 + 1024; e++) {
        u32 g = az_exp2_q8(e);
        CHECK(g >= last);                           /* monotone */
        last = g;
        if (e >= 8192) { CHECK_EQ(g, 0xFFFFFFFFu); continue; }
        double v = pow(2.0, (double)e / 256.0);
        double err = fabs((double)g - v);
        /* nearest integer, up to the 2^-31 relative resolution of the table */
        CHECK(err <= 0.5 + v * 1e-9);
        if (err > max_abs && v < 1e6) max_abs = err;
        if (e >= 6 * 256) {                         /* value >= 64: the 1 % bound of the task */
            double rel = err / v;
            if (rel > max_rel_big) max_rel_big = rel;
            CHECK(rel <= 0.01);
        }
    }
    CHECK(az_exp2_q8(8191) < 0xFFFFFFFFu);
    CHECK_EQ(az_exp2_q8(8192), 0xFFFFFFFFu);
    CHECK_EQ(az_exp2_q8(0xFFFFFFFFu), 0xFFFFFFFFu);
    CHECK_EQ(az_exp2_q8(0x80000000u), 0xFFFFFFFFu);
    for (u32 n = 0; n < 100000; n++) CHECK_EQ(az_exp2_q8(8192u + r32_any_width()), 0xFFFFFFFFu);
    printf("exp2_q8: max abs error %.4f (values < 1e6), max relative error %.5f %% for e >= 1536\n",
           max_abs, 100.0 * max_rel_big);
    /* round trip exp2(log2(x)) = x within the 1/256-bit resolution of the log */
    for (u32 n = 0; n < 200000; n++) {
        u32 x = r32_any_width();
        if (x == 0) x = 1;
        double back = (double)az_exp2_q8(az_log2_q8(x));
        CHECK(fabs(back - (double)x) <= (double)x * 0.004 + 1.0);
    }

    /* div_round: round half up, saturating, no intermediate overflow */
    for (u32 n = 0; n < 500000; n++) {
        u64 num = r64_any_width();
        u32 den = r32_any_width();
        if (den == 0) den = 1;
        u128 q = ((u128)num * 2 + den) / ((u128)den * 2);
        u32 want = q > 0xFFFFFFFFu ? 0xFFFFFFFFu : (u32)q;
        CHECK_EQ(az_div_round(num, den), want);
        if (num < (1ull << 63)) CHECK_EQ(az_div_round(num, den), stub_div_round(num, den));
    }
    CHECK_EQ(az_div_round(0, 7), 0);  CHECK_EQ(az_div_round(3, 7), 0);  CHECK_EQ(az_div_round(4, 7), 1);
    CHECK_EQ(az_div_round(3, 6), 1);  CHECK_EQ(az_div_round(2, 6), 0);  CHECK_EQ(az_div_round(9, 6), 2);
    CHECK_EQ(az_div_round(1, 2), 1);  CHECK_EQ(az_div_round(1, 3), 0);  CHECK_EQ(az_div_round(5, 1), 5);
    CHECK_EQ(az_div_round(0xFFFFFFFFFFFFFFFFull, 1), 0xFFFFFFFFu);
    CHECK_EQ(az_div_round(0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFFu), 0xFFFFFFFFu);     /* 2^32 + 1 saturates */
    CHECK_EQ(az_div_round(0xFFFFFFFFFFFFFFFFull, 0x80000000u), 0xFFFFFFFFu);
    CHECK_EQ(az_div_round(0xFFFFFFFEFFFFFFFFull, 0xFFFFFFFFu), 0xFFFFFFFFu);     /* 2^32 - 1 + ... */
    CHECK_EQ(az_div_round(0x7FFFFFFFull, 0xFFFFFFFFu), 0);                       /* just under a half */
    CHECK_EQ(az_div_round(0x80000000ull, 0xFFFFFFFFu), 1);                       /* just over a half  */
    CHECK_EQ(az_div_round(5, 0), 0xFFFFFFFFu);                                   /* outside the contract: saturates */
    CHECK_EQ(az_div_round(0, 0), 0);
}

/* ====================================================================== */
/* 4. radius code and tetrahedral channels: equal to the stub              */
/* ====================================================================== */
static void tetra_same(const i32 v[3], u32 shift)
{
    u8 a[4], b[4];
    az_tetra4(v, shift, a);
    stub_tetra4(v, shift, b);
    CHECK(memcmp(a, b, 4) == 0);
}

static void test_stub_equal(void)
{
    for (u32 m = 0; m <= (1u << 17); m++) CHECK_EQ(az_radius_code(m), stub_radius_code(m));
    for (u32 k = 0; k < 32; k++) {
        u32 p = 1u << k;
        CHECK_EQ(az_radius_code(p - 2u), stub_radius_code(p - 2u));
        CHECK_EQ(az_radius_code(p - 1u), stub_radius_code(p - 1u));
        CHECK_EQ(az_radius_code(p), stub_radius_code(p));
        CHECK_EQ(az_radius_code(p + 1u), stub_radius_code(p + 1u));
    }
    CHECK_EQ(az_radius_code(0xFFFFFFFEu), stub_radius_code(0xFFFFFFFEu));
    CHECK_EQ(az_radius_code(0xFFFFFFFFu), stub_radius_code(0xFFFFFFFFu));
    CHECK_EQ(az_radius_code(0), 0); CHECK_EQ(az_radius_code(1), 1); CHECK_EQ(az_radius_code(2), 1);
    CHECK_EQ(az_radius_code(3), 2); CHECK_EQ(az_radius_code(32766), 14); CHECK_EQ(az_radius_code(32767), 15);
    CHECK_EQ(az_radius_code(0xFFFFFFFEu), 15);
    CHECK_EQ(az_radius_code(UINT32_MAX), 15);
    CHECK_EQ(stub_radius_code(UINT32_MAX), 15);
    u32 prev = 0;
    for (u32 n = 0; n < 100000; n++) {
        u32 m = (n & 1) ? r32() : r32_any_width();
        CHECK_EQ(az_radius_code(m), stub_radius_code(m));
        CHECK(az_radius_code(m) <= 15);
    }
    for (u32 m = 0; m < 70000; m++) { u32 c = az_radius_code(m); CHECK(c >= prev); prev = c; }   /* monotone */

    const i32 edge[] = { 0, 1, -1, 2, -2, 127, 128, -128, 255, 16384, -16384, 65536,
                         0x3FFFFFFF, -0x3FFFFFFF, 0x7FFFFFFF, -0x7FFFFFFF, (i32)0x80000000u };
    const u32 ne = (u32)(sizeof edge / sizeof edge[0]);
    for (u32 i = 0; i < ne; i++)
        for (u32 j = 0; j < ne; j++)
            for (u32 k = 0; k < ne; k++) {
                i32 v[3] = { edge[i], edge[j], edge[k] };
                tetra_same(v, 0); tetra_same(v, 7); tetra_same(v, 14); tetra_same(v, 25); tetra_same(v, 63);
            }
    for (u32 n = 0; n < 100000; n++) {
        i32 v[3];
        u32 kind = n % 4u;
        for (u32 k = 0; k < 3; k++)
            v[k] = kind == 0 ? (i32)r32() : kind == 1 ? rrange(-200, 200)
                 : kind == 2 ? rrange(-(1 << 20), 1 << 20) : (i32)r32_any_width() * ((r32() & 1) ? 1 : -1);
        tetra_same(v, r32() % 64u);
        tetra_same(v, r32() % 16u);
    }
    {   /* a shift of 64 or more is defined here (all channels 0); the stub leaves it undefined */
        const i32 v[3] = { 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF };
        u8 o[4] = { 9, 9, 9, 9 };
        az_tetra4(v, 64, o);
        CHECK(o[0] == 0 && o[1] == 0 && o[2] == 0 && o[3] == 0);
        az_tetra4(v, 0xFFFFFFFFu, o);
        CHECK(o[0] == 0 && o[1] == 0 && o[2] == 0 && o[3] == 0);
        const i32 f[3] = { 100, 100, 100 };              /* straight along axis 0 */
        az_tetra4(f, 2, o);
        CHECK(o[0] == 75 && o[1] == 0 && o[2] == 0 && o[3] == 0);
    }
}

/* ====================================================================== */
/* 5. rotors                                                               */
/* ====================================================================== */
typedef struct { double w, a, b, c; } Quat;

static Quat q_from(const i16 r[4]) { Quat q = { r[0], r[1], r[2], r[3] }; return q; }
static double q_len(Quat q) { return sqrt(q.w * q.w + q.a * q.a + q.b * q.b + q.c * q.c); }
static Quat q_unit(Quat q) { double n = q_len(q); Quat u = { q.w / n, q.a / n, q.b / n, q.c / n }; return u; }
static Quat q_mul(Quat p, Quat q)
{
    Quat o = { p.w * q.w - p.a * q.a - p.b * q.b - p.c * q.c,
               p.w * q.a + p.a * q.w + p.b * q.c - p.c * q.b,
               p.w * q.b - p.a * q.c + p.b * q.w + p.c * q.a,
               p.w * q.c + p.a * q.b - p.b * q.a + p.c * q.w };
    return o;
}
/* v' = q v q* for a unit q, by two quaternion products */
static void q_rot(Quat u, const double v[3], double out[3])
{
    Quat p = { 0, v[0], v[1], v[2] }, cj = { u.w, -u.a, -u.b, -u.c };
    Quat o = q_mul(q_mul(u, p), cj);
    out[0] = o.a; out[1] = o.b; out[2] = o.c;
}
/* angle between the rotations of two rotors, radians */
static double q_angle(Quat p, Quat q)
{
    p = q_unit(p); q = q_unit(q);
    double d = fabs(p.w * q.w + p.a * q.a + p.b * q.b + p.c * q.c);
    return 2.0 * acos(d > 1.0 ? 1.0 : d);
}
/* largest component difference between a rotor and 16384 * (unit q) */
static double q_diff(const i16 r[4], Quat q)
{
    q = q_unit(q);
    double d0 = fabs(r[0] - 16384.0 * q.w), d1 = fabs(r[1] - 16384.0 * q.a);
    double d2 = fabs(r[2] - 16384.0 * q.b), d3 = fabs(r[3] - 16384.0 * q.c);
    double m = d0 > d1 ? d0 : d1;
    if (d2 > m) m = d2;
    if (d3 > m) m = d3;
    return m;
}
static double rotor_len(const i16 r[4]) { return q_len(q_from(r)); }

static void random_unit_rotor(i16 r[4])
{
    do { for (u32 k = 0; k < 4; k++) r[k] = (i16)rrange(-16384, 16384); } while (rotor_len(r) < 2000.0);
    az_rotor_norm(r);
}

static double vec_len(const i32 v[3])
{
    return sqrt((double)v[0] * v[0] + (double)v[1] * v[1] + (double)v[2] * v[2]);
}

static void test_rotor_norm(void)
{
    i16 z[4] = { 0, 0, 0, 0 };
    az_rotor_norm(z);
    CHECK(z[0] == 16384 && z[1] == 0 && z[2] == 0 && z[3] == 0);     /* zero -> identity */
    const i16 fixed[][4] = {
        { 1, 0, 0, 0 }, { -1, 0, 0, 0 }, { 0, 0, 0, 1 }, { 1, 1, 0, 0 }, { 1, -1, 1, -1 }, { 3, 4, 0, 0 },
        { 32767, 32767, 32767, 32767 }, { -32768, -32768, -32768, -32768 }, { -32768, 0, 0, 0 },
        { 16384, 0, 0, 0 }, { 0, 16384, 0, 0 }, { 11585, 11585, 0, 0 }, { 8192, 8192, 8192, 8192 },
        { 32767, 1, 0, 0 }, { 1, 32767, -32768, 2 } };
    double max_d = 0, max_n = 0;
    for (u32 n = 0; n < 200000; n++) {
        i16 r[4], in[4];
        if (n < sizeof fixed / sizeof fixed[0]) memcpy(r, fixed[n], sizeof r);
        else {
            i32 lim = n % 3u == 0 ? 32767 : n % 3u == 1 ? 300 : 3;
            for (u32 k = 0; k < 4; k++) r[k] = (i16)rrange(-lim, lim);
            if (n % 7u == 0) r[r32() % 4u] = (i16)-32768;
        }
        if (!(r[0] | r[1] | r[2] | r[3])) continue;
        memcpy(in, r, sizeof in);
        az_rotor_norm(r);
        double d = q_diff(r, q_from(in)), len = fabs(rotor_len(r) - 16384.0);
        if (d > max_d) max_d = d;
        if (len > max_n) max_n = len;
        CHECK(d <= 0.5005);                      /* every component rounded to nearest */
        CHECK(len <= 1.0);
        /* odd symmetry (when no component is -32768, whose negation does not exist) */
        if (in[0] != -32768 && in[1] != -32768 && in[2] != -32768 && in[3] != -32768) {
            i16 m[4] = { (i16)-in[0], (i16)-in[1], (i16)-in[2], (i16)-in[3] };
            az_rotor_norm(m);
            CHECK(m[0] == -r[0] && m[1] == -r[1] && m[2] == -r[2] && m[3] == -r[3]);
        }
    }
    printf("rotor_norm: max component error %.4f, max |len - 16384| %.4f\n", max_d, max_n);
}

/* one step in double precision: normalise(q + (1/2) (0, ang) q), ang in radians */
static Quat step_ref(Quat q, const i16 ang[3])
{
    Quat om = { 0, ang[0] / 16384.0, ang[1] / 16384.0, ang[2] / 16384.0 };
    Quat d = q_mul(om, q);
    Quat o = { q.w + 0.5 * d.w, q.a + 0.5 * d.a, q.b + 0.5 * d.b, q.c + 0.5 * d.c };
    return o;
}

static void test_rotor_step(void)
{
    /* 100,000 random steps on one rotor: unit length and the double-precision step */
    i16 r[4] = { 16384, 0, 0, 0 };
    double max_dev = 0, max_d = 0;
    for (u32 n = 0; n < 100000; n++) {
        i16 ang[3], before[4];
        i32 lim = n % 3u == 0 ? 64 : n % 3u == 1 ? 2048 : 32767;
        for (u32 k = 0; k < 3; k++) ang[k] = (i16)rrange(-lim, lim);
        if (n % 1000u == 999u) { ang[0] = (i16)-32768; ang[1] = (i16)-32768; ang[2] = (i16)-32768; }
        memcpy(before, r, sizeof before);
        az_rotor_step(r, ang);
        double dev = fabs(rotor_len(r) / 16384.0 - 1.0);
        if (dev > max_dev) max_dev = dev;
        CHECK(dev <= 0.001);                                         /* the task's bound: 0.1 % */
        double d = q_diff(r, step_ref(q_from(before), ang));
        if (d > max_d) max_d = d;
        CHECK(d <= 0.5005);
    }
    printf("rotor_step: 100000 random steps, max |len/16384 - 1| = %.6f %%, max component error vs double %.4f\n",
           100.0 * max_dev, max_d);
    CHECK(max_dev <= 0.0001);                                        /* in fact within one unit: 0.006 % */

    /* a zero angular velocity leaves a unit rotor as its own normalisation */
    for (u32 n = 0; n < 2000; n++) {
        i16 a[4], b[4];
        const i16 zero[3] = { 0, 0, 0 };
        random_unit_rotor(a);
        memcpy(b, a, sizeof b);
        az_rotor_step(b, zero);
        az_rotor_norm(a);
        CHECK(memcmp(a, b, sizeof a) == 0);
    }

    /* constant angular velocity: compare with the exact rotation.
     * Per tick the rule turns by 2 atan(|w|/2); the true angle is |w|. */
    const i32 mags[] = { 20, 82, 328, 1638, 6554 };                  /* about 0.0012, 0.005, 0.02, 0.1, 0.4 rad per tick */
    for (u32 mi = 0; mi < 5; mi++) {
        const u32 trials = 100;
        double sum2 = 0, worst = 0, worst_true = 0, third = 0, rate = 0;
        u32 ticks = 0;
        for (u32 trial = 0; trial < trials; trial++) {
            i16 q0[4], q[4], ang[3];
            random_unit_rotor(q0);
            memcpy(q, q0, sizeof q);
            double ax[3] = { rrange(-1000, 1000), rrange(-1000, 1000), rrange(-1000, 1000) };
            double al = sqrt(ax[0] * ax[0] + ax[1] * ax[1] + ax[2] * ax[2]);
            if (al < 1.0) { ax[0] = 1; al = 1; }
            for (u32 k = 0; k < 3; k++) ang[k] = (i16)lrint(mags[mi] * ax[k] / al);
            double wl = sqrt((double)ang[0] * ang[0] + (double)ang[1] * ang[1] + (double)ang[2] * ang[2]) / 16384.0;
            u32 steps = (u32)(6.0 / wl);                             /* about one turn */
            for (u32 s = 0; s < steps; s++) az_rotor_step(q, ang);
            double th_rule = steps * 2.0 * atan(wl / 2.0), th_true = steps * wl;
            double sx = ang[0] / 16384.0 / wl, sy = ang[1] / 16384.0 / wl, sz = ang[2] / 16384.0 / wl;
            Quat e_rule = { cos(th_rule / 2), sin(th_rule / 2) * sx, sin(th_rule / 2) * sy, sin(th_rule / 2) * sz };
            Quat e_true = { cos(th_true / 2), sin(th_true / 2) * sx, sin(th_true / 2) * sy, sin(th_true / 2) * sz };
            double err_rule = q_angle(q_from(q), q_mul(e_rule, q_from(q0)));     /* world frame: left product */
            double err_true = q_angle(q_from(q), q_mul(e_true, q_from(q0)));
            /* Rounding moves the rotor by at most one unit (1/16384) per tick, which is a
             * rotation error of at most 2/16384 rad per tick. That is the hard bound; the
             * measured error (printed) is far below it. */
            double tol = (steps + 1.0) * 2.0 / 16384.0;
            CHECK(err_rule <= tol);
            CHECK(err_true <= tol + steps * wl * wl * wl / 12.0 * 1.01);
            sum2 += err_rule * err_rule;
            if (err_rule > worst) worst = err_rule;
            if (err_true > worst_true) worst_true = err_true;
            third = steps * wl * wl * wl / 12.0; rate = wl; ticks = steps;
        }
        printf("rotor_step: |w| = %.4f rad/tick, %5u ticks (about one turn), %u trials: error vs rule rms %.5f max %.5f rad;"
               " vs true rotation max %.5f rad (third-order term %.5f)\n",
               rate, ticks, trials, sqrt(sum2 / trials), worst, worst_true, third);
        /* measured accuracy that later changes must not lose: within 3 % of the turned angle
         * at the slowest rate, within 0.5 % from 0.005 rad per tick up */
        CHECK(worst <= (mi == 0 ? 0.03 : 0.005) * rate * ticks);
    }

    /* the angular velocity is in the world frame: a step about world z turns
     * every world-frame image about z, whatever the orientation */
    for (u32 n = 0; n < 2000; n++) {
        i16 q0[4], q1[4];
        const i16 ang[3] = { 0, 0, 1024 };                           /* 1/16 rad about world z */
        random_unit_rotor(q0);
        memcpy(q1, q0, sizeof q1);
        az_rotor_step(q1, ang);
        i32 v[3] = { rrange(-100000, 100000), rrange(-100000, 100000), rrange(-100000, 100000) }, w0[3], w1[3];
        az_rotor_apply(w0, q0, v);
        az_rotor_apply(w1, q1, v);
        double th = 2.0 * atan(1024.0 / 16384.0 / 2.0);
        double ex = cos(th) * w0[0] - sin(th) * w0[1], ey = sin(th) * w0[0] + cos(th) * w0[1];
        double tol = 2.0 + 2e-4 * vec_len(v);
        CHECK(fabs(w1[0] - ex) <= tol);
        CHECK(fabs(w1[1] - ey) <= tol);
        CHECK(fabs((double)w1[2] - (double)w0[2]) <= tol);
    }
}

/* The definition of az_rotor_apply in exact 128-bit integers:
 * round(M v / |q|^2), ties away from zero, saturated to an i32. */
static void apply_exact(i32 out[3], const i16 r[4], const i32 v[3], int inverse)
{
    __int128 w = r[0], a = r[1], b = r[2], c = r[3];
    if (inverse) { a = -a; b = -b; c = -c; }
    __int128 n2 = w * w + a * a + b * b + c * c;
    if (n2 == 0) { out[0] = v[0]; out[1] = v[1]; out[2] = v[2]; return; }
    const __int128 m[9] = {
        w * w + a * a - b * b - c * c, 2 * (a * b - w * c), 2 * (a * c + w * b),
        2 * (a * b + w * c), w * w - a * a + b * b - c * c, 2 * (b * c - w * a),
        2 * (a * c - w * b), 2 * (b * c + w * a), w * w - a * a - b * b + c * c };
    for (u32 i = 0; i < 3; i++) {
        __int128 s = m[3 * i] * v[0] + m[3 * i + 1] * v[1] + m[3 * i + 2] * v[2];
        __int128 mag = s < 0 ? -s : s;
        __int128 q = (2 * mag + n2) / (2 * n2);
        if (q > 0x7FFFFFFF) q = 0x7FFFFFFF;
        out[i] = (i32)(s < 0 ? -q : q);
    }
}

/* both directions equal the exact integer definition */
static void apply_same_as_exact(const i16 r[4], const i32 v[3])
{
    i32 got[3], want[3];
    az_rotor_apply(got, r, v);
    apply_exact(want, r, v, 0);
    CHECK(got[0] == want[0] && got[1] == want[1] && got[2] == want[2]);
    az_rotor_apply_inv(got, r, v);
    apply_exact(want, r, v, 1);
    CHECK(got[0] == want[0] && got[1] == want[1] && got[2] == want[2]);
}

static void apply_case(const i16 r[4], const i32 v[3], double *max_err, double *max_len, i32 *max_rt)
{
    i32 out[3], back[3], fwd[3], alias[3];
    apply_same_as_exact(r, v);
    az_rotor_apply(out, r, v);
    /* exact rotation by the rotor's direction, rounded to nearest */
    double dv[3] = { v[0], v[1], v[2] }, ref[3];
    q_rot(q_unit(q_from(r)), dv, ref);
    for (u32 k = 0; k < 3; k++) {
        double e = fabs(out[k] - ref[k]);
        if (e > *max_err) *max_err = e;
        CHECK(e <= 0.5 + 1e-6 + 1e-12 * fabs(ref[k]));
    }
    /* length */
    double lv = vec_len(v), lo = vec_len(out), dl = fabs(lo - lv);
    if (dl > *max_len) *max_len = dl;
    CHECK(dl <= 0.8661);                                    /* half a unit per component */
    if (lv >= 1024.0) CHECK(dl <= 0.002 * lv);              /* the task's bound: 0.2 % */
    /* apply then apply_inv, and the other order */
    az_rotor_apply_inv(back, r, out);
    az_rotor_apply_inv(fwd, r, v);
    az_rotor_apply(fwd, r, fwd);                            /* in place */
    for (u32 k = 0; k < 3; k++) {
        i32 d1 = az_abs_i32(back[k] - v[k]), d2 = az_abs_i32(fwd[k] - v[k]);
        if (d1 > *max_rt) *max_rt = d1;
        if (d2 > *max_rt) *max_rt = d2;
        CHECK(d1 <= 4);                                     /* the task's bound */
        CHECK(d2 <= 4);
        CHECK(d1 <= 1);                                     /* what the exact rotation gives */
        CHECK(d2 <= 1);
    }
    /* out may alias v */
    memcpy(alias, v, sizeof alias);
    az_rotor_apply(alias, r, alias);
    CHECK(memcmp(alias, out, sizeof out) == 0);
    /* odd symmetry */
    i32 neg[3] = { -v[0], -v[1], -v[2] }, nout[3];
    az_rotor_apply(nout, r, neg);
    CHECK(nout[0] == -out[0] && nout[1] == -out[1] && nout[2] == -out[2]);
}

static void test_rotor_apply(void)
{
    const i16 ident[4] = { 16384, 0, 0, 0 }, zero[4] = { 0, 0, 0, 0 };
    const i16 quarter_z[4] = { 11585, 0, 0, 11585 };         /* 90 degrees about z */
    const i16 half_x[4] = { 0, 16384, 0, 0 };                /* 180 degrees about x */
    {
        const i32 v[3] = { 1000000, -20000, 333 };
        i32 o[3];
        az_rotor_apply(o, ident, v);      CHECK(o[0] == v[0] && o[1] == v[1] && o[2] == v[2]);
        az_rotor_apply_inv(o, ident, v);  CHECK(o[0] == v[0] && o[1] == v[1] && o[2] == v[2]);
        az_rotor_apply(o, zero, v);       CHECK(o[0] == v[0] && o[1] == v[1] && o[2] == v[2]);
        az_rotor_apply(o, quarter_z, v);  CHECK(o[0] == 20000 && o[1] == 1000000 && o[2] == 333);   /* x -> y */
        az_rotor_apply_inv(o, quarter_z, v); CHECK(o[0] == -20000 && o[1] == -1000000 && o[2] == 333);
        az_rotor_apply(o, half_x, v);     CHECK(o[0] == 1000000 && o[1] == 20000 && o[2] == -333);
        /* body x (forward) of a creature yawed a quarter turn points along world y */
        const i32 fwd[3] = { 16384, 0, 0 };
        az_rotor_apply(o, quarter_z, fwd); CHECK(o[0] == 0 && o[1] == 16384 && o[2] == 0);
        /* saturation instead of wrap-around */
        const i32 big[3] = { (i32)0x80000000u, 0x7FFFFFFF, 0 };
        az_rotor_apply(o, ident, big);    CHECK(o[0] == -0x7FFFFFFF && o[1] == 0x7FFFFFFF && o[2] == 0);
        const i32 diag[3] = { 0x7FFFFFFF, 0x7FFFFFFF, 0 };
        const i16 eighth_z[4] = { 15137, 0, 0, 6270 };       /* 45 degrees about z: y would be 2^31 * 1.414 */
        az_rotor_apply(o, eighth_z, diag); CHECK(o[1] == 0x7FFFFFFF);
    }
    double max_err = 0, max_len = 0;
    i32 max_rt = 0;
    for (u32 n = 0; n < 100000; n++) {
        i16 r[4];
        i32 v[3];
        u32 kind = n % 5u;
        if (kind == 4) {             /* a rotor that is not unit length: still an exact rotation */
            do { for (u32 k = 0; k < 4; k++) r[k] = (i16)rrange(-16384, 16384); } while (rotor_len(r) < 3000.0);
        } else random_unit_rotor(r);
        i32 lim = kind == 0 ? 1 << 20 : kind == 1 ? 40000 : kind == 2 ? 300 : kind == 3 ? 1 << 30 : 1 << 20;
        for (u32 k = 0; k < 3; k++) v[k] = rrange(-lim, lim);
        apply_case(r, v, &max_err, &max_len, &max_rt);
    }
    /* Unit rotors, rotors a few units off unit length, rotors of any length;
     * vectors of the full i32 range (they reach the saturation). Every result
     * must equal the exact integer definition. */
    u32 n_fast = 0, n_div = 0;
    for (u32 n = 0; n < 400000; n++) {
        i16 r[4];
        i32 v[3];
        u32 kind = n % 4u;
        if (kind == 3) {             /* any length, any component, -32768 included */
            for (u32 k = 0; k < 4; k++) r[k] = (i16)rrange(-32768, 32767);
            /* keep |q|^2 <= 2^31, the documented exact range */
            while ((i64)r[0] * r[0] + (i64)r[1] * r[1] + (i64)r[2] * r[2] + (i64)r[3] * r[3] > ((i64)1 << 31))
                for (u32 k = 0; k < 4; k++) r[k] = (i16)(r[k] / 2);
        } else {
            random_unit_rotor(r);
            if (kind) for (u32 k = 0; k < 4; k++) r[k] = (i16)(r[k] + rrange(-3, 3));
        }
        i64 e = (i64)r[0] * r[0] + (i64)r[1] * r[1] + (i64)r[2] * r[2] + (i64)r[3] * r[3] - ((i64)1 << 28);
        if (e > -65536 && e < 65536) n_fast++; else n_div++;
        const i32 ends[8] = { 0x7FFFFFFF, 0x7FFFFFFE, (i32)0x80000000u, -0x7FFFFFFF, 0, 1, -1, 0x40000000 };
        u32 vk = (n / 4u) % 4u;
        for (u32 k = 0; k < 3; k++)
            v[k] = vk == 0 ? (i32)r32() : vk == 1 ? rrange(-(1 << 20), 1 << 20)
                 : vk == 2 ? (i32)(r32_any_width() >> 1) * ((r32() & 1) ? 1 : -1)
                 : ends[r32() % 8u];
        apply_same_as_exact(r, v);
    }
    printf("rotor_apply: exact-integer oracle on 400000 more cases (%u within two units of unit length, %u others)\n",
           n_fast, n_div);
    CHECK(n_fast > 50000 && n_div > 50000);
    {   /* lengths 16378 .. 16390 with small other components */
        for (i32 d = -6; d <= 6; d++)
            for (u32 n = 0; n < 2000; n++) {
                i16 r[4] = { (i16)(16384 + d), (i16)rrange(-40, 40), (i16)rrange(-40, 40), (i16)rrange(-40, 40) };
                i32 v[3] = { (i32)r32(), (i32)r32(), (i32)r32() };
                if (n & 1) { v[0] >>= 8; v[1] >>= 8; v[2] >>= 8; }
                apply_same_as_exact(r, v);
            }
    }
    printf("rotor_apply: max error vs double %.4f units, max length change %.4f units, max apply/apply_inv round trip %d units\n",
           max_err, max_len, max_rt);
}

static void test_rotor_mul(void)
{
    const i16 ident[4] = { 16384, 0, 0, 0 };
    double max_d = 0, max_c = 0;
    for (u32 n = 0; n < 100000; n++) {
        i16 a[4], b[4], out[4], al[4];
        random_unit_rotor(a);
        random_unit_rotor(b);
        az_rotor_mul(out, a, b);
        /* the double-precision product, normalised */
        double d = q_diff(out, q_mul(q_from(a), q_from(b)));
        if (d > max_d) max_d = d;
        CHECK(d <= 0.5005);
        CHECK(fabs(rotor_len(out) - 16384.0) <= 1.0);
        /* apply(out, v) = apply(a, apply(b, v)) up to the rotor's own resolution */
        i32 lim = n % 2u ? 1 << 20 : 30000;
        i32 v[3] = { rrange(-lim, lim), rrange(-lim, lim), rrange(-lim, lim) }, t[3], two[3], one[3];
        az_rotor_apply(t, b, v);
        az_rotor_apply(two, a, t);
        az_rotor_apply(one, out, v);
        double lv = vec_len(v);
        for (u32 k = 0; k < 3; k++) {
            double c = fabs((double)one[k] - (double)two[k]);
            if (lv > 1.0 && (c - 2.0) / lv > max_c) max_c = (c - 2.0) / lv;
            CHECK(c <= 2.0 + 1.3e-4 * lv);
        }
        /* out may alias either input */
        memcpy(al, a, sizeof al); az_rotor_mul(al, al, b); CHECK(memcmp(al, out, sizeof out) == 0);
        memcpy(al, b, sizeof al); az_rotor_mul(al, a, al); CHECK(memcmp(al, out, sizeof out) == 0);
        /* sign symmetry: (-a) b = -(a b) */
        i16 na[4] = { (i16)-a[0], (i16)-a[1], (i16)-a[2], (i16)-a[3] }, no[4];
        az_rotor_mul(no, na, b);
        CHECK(no[0] == -out[0] && no[1] == -out[1] && no[2] == -out[2] && no[3] == -out[3]);
        /* the identity is neutral up to renormalisation */
        i16 ia[4], ai[4], an[4];
        az_rotor_mul(ia, ident, a); az_rotor_mul(ai, a, ident);
        memcpy(an, a, sizeof an); az_rotor_norm(an);
        CHECK(memcmp(ia, an, sizeof an) == 0);
        CHECK(memcmp(ai, an, sizeof an) == 0);
        /* a times its conjugate is the identity */
        i16 cj[4] = { a[0], (i16)-a[1], (i16)-a[2], (i16)-a[3] }, pr[4];
        az_rotor_mul(pr, a, cj);
        CHECK(pr[0] == 16384 && pr[1] == 0 && pr[2] == 0 && pr[3] == 0);
    }
    /* raw (not unit) and extreme inputs still give a unit rotor that matches double precision */
    for (u32 n = 0; n < 50000; n++) {
        i16 a[4], b[4], out[4];
        i32 lim = n % 3u == 0 ? 32767 : n % 3u == 1 ? 500 : 2;
        for (u32 k = 0; k < 4; k++) { a[k] = (i16)rrange(-lim - 1, lim); b[k] = (i16)rrange(-lim - 1, lim); }
        if (n % 11u == 0) for (u32 k = 0; k < 4; k++) { a[k] = (i16)-32768; b[k] = (i16)(k & 1 ? 32767 : -32768); }
        if (!(a[0] | a[1] | a[2] | a[3]) || !(b[0] | b[1] | b[2] | b[3])) {
            az_rotor_mul(out, a, b);
            CHECK(out[0] == 16384 && out[1] == 0 && out[2] == 0 && out[3] == 0);   /* zero -> identity */
            continue;
        }
        az_rotor_mul(out, a, b);
        CHECK(q_diff(out, q_mul(q_from(a), q_from(b))) <= 0.5005);
    }
    printf("rotor_mul: max component error vs double %.4f; composition mismatch beyond 2 units: %.2e of |v|\n",
           max_d, max_c < 0 ? 0.0 : max_c);
}

/* ====================================================================== */
/* 6. timings (information only)                                           */
/* ====================================================================== */
static double now_ns(void)
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

#define TIME_LOOP(label, count, body)                                          \
    do {                                                                       \
        double t0 = now_ns();                                                  \
        for (u32 it = 0; it < (count); it++) { body; }                         \
        printf("  %-28s %7.2f ns/call\n", label, (now_ns() - t0) / (count));   \
    } while (0)

static void timings(void)
{
    enum { NV = 4096 };
    static u64 xs[NV];
    static i16 rot[NV][4], ang[NV][3];
    static i32 vec[NV][3];
    for (u32 i = 0; i < NV; i++) {
        xs[i] = r64_any_width();
        random_unit_rotor(rot[i]);
        for (u32 k = 0; k < 3; k++) { ang[i][k] = (i16)rrange(-2048, 2048); vec[i][k] = rrange(-(1 << 20), 1 << 20); }
    }
    volatile u64 sink = 0;
    u64 acc = 0;
    const u32 N = 2000000;
    printf("timings (noisy, parallel load; one thread, includes loop overhead):\n");
    TIME_LOOP("az_isqrt_u64", N, acc += az_isqrt_u64(xs[it & (NV - 1)]));
    TIME_LOOP("stub isqrt", N, acc += stub_isqrt_u64(xs[it & (NV - 1)]));
    TIME_LOOP("az_sin_q14", N, acc += (u64)az_sin_q14(it * 2654435761u));
    TIME_LOOP("stub sin", N, acc += (u64)stub_sin_q14(it * 2654435761u));
    TIME_LOOP("az_log2_q8", N, acc += az_log2_q8((u32)xs[it & (NV - 1)]));
    TIME_LOOP("az_exp2_q8", N, acc += az_exp2_q8(it & 8191u));
    TIME_LOOP("az_rotor_apply", N, { i32 o[3]; az_rotor_apply(o, rot[it & (NV - 1)], vec[it & (NV - 1)]); acc += (u64)o[0]; });
    TIME_LOOP("stub rotor_apply", N, { i32 o[3]; stub_rotor_apply(o, rot[it & (NV - 1)], vec[it & (NV - 1)]); acc += (u64)o[0]; });
    TIME_LOOP("az_rotor_mul", N, { i16 o[4]; az_rotor_mul(o, rot[it & (NV - 1)], rot[(it + 1) & (NV - 1)]); acc += (u64)o[0]; });
    TIME_LOOP("stub rotor_mul", N, { i16 o[4]; stub_rotor_mul(o, rot[it & (NV - 1)], rot[(it + 1) & (NV - 1)]); acc += (u64)o[0]; });
    TIME_LOOP("az_rotor_step", N, { i16 *q = rot[it & (NV - 1)]; az_rotor_step(q, ang[it & (NV - 1)]); acc += (u64)q[0]; });
    TIME_LOOP("stub rotor_step", N, { i16 *q = rot[it & (NV - 1)]; stub_rotor_step(q, ang[it & (NV - 1)]); acc += (u64)q[0]; });
    sink = acc;
    (void)sink;
}

int main(void)
{
    test_isqrt();
    printf("isqrt done: %d checks, %d failures so far\n", az_checks, az_failures);
    test_trig();
    test_log_exp();
    test_stub_equal();
    test_rotor_norm();
    test_rotor_step();
    test_rotor_apply();
    test_rotor_mul();
    timings();
    return az_test_done();
}
