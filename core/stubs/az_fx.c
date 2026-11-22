/* az_fx.c (reference stub) - fixed-point helpers and rotor math.
 * Simple, correct-by-construction versions. The module owner replaces this
 * file with table-based, tested versions that keep the same signatures. */
#include "../az_abi.h"

u32 az_isqrt_u64(u64 x)
{
    u64 r = 0, bit = 1ull << 62;
    while (bit > x) bit >>= 2;
    while (bit) {
        if (x >= r + bit) { x -= r + bit; r = (r >> 1) + bit; }
        else r >>= 1;
        bit >>= 2;
    }
    return (u32)r;
}

/* sine by a corrected parabola: exact at 0, 1/4 and 1/2 turn, error < 0.1% of full scale */
i32 az_sin_q14(u32 angle)
{
    u32 a = angle & 0xFFFFu;
    i32 sign = 1;
    if (a >= 32768u) { a -= 32768u; sign = -1; }
    i64 p = (i64)(((u64)a * (32768u - a)) >> 14);          /* 0..16384 */
    i64 s = (p * (12697 + ((3687 * p) >> 14))) >> 14;
    return sign * (i32)s;
}

i32 az_cos_q14(u32 angle) { return az_sin_q14(angle + 16384u); }

u32 az_log2_q8(u32 x)
{
    if (x == 0) return 0;
    u32 msb = 31u - (u32)__builtin_clz(x);
    u32 frac = msb >= 8 ? (x >> (msb - 8)) & 0xFFu : (x << (8 - msb)) & 0xFFu;
    return msb * 256u + frac;
}

u32 az_exp2_q8(u32 e)
{
    u32 ip = e >> 8, frac = e & 255u;
    if (ip >= 31) return 0xFFFFFFFFu;
    u32 base = 1u << ip;
    return base + (u32)(((u64)base * frac) >> 8);
}

u32 az_div_round(u64 num, u32 den)
{
    u64 q = (num + den / 2) / den;
    return q > 0xFFFFFFFFull ? 0xFFFFFFFFu : (u32)q;
}

u32 az_radius_code(u32 mass)
{
    const u64 units = (u64)mass + 1u;
    return units >= (1u << 15) ? 15u : az_log2_q8((u32)units) >> 8;
}

void az_tetra4(const i32 v[3], u32 shift, u8 out[4])
{
    i64 x = v[0], y = v[1], z = v[2];
    i64 d[4] = { x + y + z, x - y - z, -x + y - z, -x - y + z };
    for (u32 k = 0; k < 4; k++) {
        i64 r = d[k] > 0 ? d[k] >> shift : 0;
        out[k] = (u8)(r > 127 ? 127 : r);
    }
}

/* The rotor is stored as a unit quaternion (w, a, b, c), Q1.14. */
void az_rotor_norm(i16 r[4])
{
    i64 n2 = (i64)r[0] * r[0] + (i64)r[1] * r[1] + (i64)r[2] * r[2] + (i64)r[3] * r[3];
    u32 n = az_isqrt_u64((u64)n2);
    if (n == 0) { r[0] = 1 << 14; r[1] = r[2] = r[3] = 0; return; }
    for (u32 k = 0; k < 4; k++) {
        i32 v = (i32)(((i64)r[k] * 16384) / (i64)n);
        r[k] = (i16)az_clamp_i32(v, -16384, 16384);
    }
}

void az_rotor_mul(i16 out[4], const i16 a[4], const i16 b[4])
{
    i32 w = (a[0] * b[0] - a[1] * b[1] - a[2] * b[2] - a[3] * b[3]) >> 14;
    i32 x = (a[0] * b[1] + a[1] * b[0] + a[2] * b[3] - a[3] * b[2]) >> 14;
    i32 y = (a[0] * b[2] - a[1] * b[3] + a[2] * b[0] + a[3] * b[1]) >> 14;
    i32 z = (a[0] * b[3] + a[1] * b[2] - a[2] * b[1] + a[3] * b[0]) >> 14;
    out[0] = (i16)az_clamp_i32(w, -16384, 16384);
    out[1] = (i16)az_clamp_i32(x, -16384, 16384);
    out[2] = (i16)az_clamp_i32(y, -16384, 16384);
    out[3] = (i16)az_clamp_i32(z, -16384, 16384);
    az_rotor_norm(out);
}

/* v' = v + 2w (u x v) + 2 u x (u x v), u = (a, b, c) */
static void rotate(i32 out[3], i32 w, i32 a, i32 b, i32 c, const i32 v[3])
{
    i64 cx = ((i64)b * v[2] - (i64)c * v[1]) >> 14;
    i64 cy = ((i64)c * v[0] - (i64)a * v[2]) >> 14;
    i64 cz = ((i64)a * v[1] - (i64)b * v[0]) >> 14;
    i64 dx = ((i64)b * cz - (i64)c * cy) >> 14;
    i64 dy = ((i64)c * cx - (i64)a * cz) >> 14;
    i64 dz = ((i64)a * cy - (i64)b * cx) >> 14;
    out[0] = (i32)(v[0] + ((2 * (i64)w * cx) >> 14) + 2 * dx);
    out[1] = (i32)(v[1] + ((2 * (i64)w * cy) >> 14) + 2 * dy);
    out[2] = (i32)(v[2] + ((2 * (i64)w * cz) >> 14) + 2 * dz);
}

void az_rotor_apply(i32 out[3], const i16 r[4], const i32 v[3])
{
    rotate(out, r[0], r[1], r[2], r[3], v);
}

void az_rotor_apply_inv(i32 out[3], const i16 r[4], const i32 v[3])
{
    rotate(out, r[0], -r[1], -r[2], -r[3], v);
}

/* R <- normalise(R + (1/2) * (0, ang) * R) */
void az_rotor_step(i16 r[4], const i16 ang[3])
{
    i32 w = r[0], a = r[1], b = r[2], c = r[3];
    i32 dw = (-(ang[0] * a) - ang[1] * b - ang[2] * c) >> 15;
    i32 da = (ang[0] * w + ang[1] * c - ang[2] * b) >> 15;
    i32 db = (-(ang[0] * c) + ang[1] * w + ang[2] * a) >> 15;
    i32 dc = (ang[0] * b - ang[1] * a + ang[2] * w) >> 15;
    r[0] = (i16)az_clamp_i32(w + dw, -32767, 32767);
    r[1] = (i16)az_clamp_i32(a + da, -32767, 32767);
    r[2] = (i16)az_clamp_i32(b + db, -32767, 32767);
    r[3] = (i16)az_clamp_i32(c + dc, -32767, 32767);
    az_rotor_norm(r);
}
