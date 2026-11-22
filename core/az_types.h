/* az_types.h - base types for the Asmodensius-Native core.
 *
 * The core is FREESTANDING C23: it includes only <stdint.h>, <stddef.h> and
 * <stdbool.h>, calls no libc function and no OS function, and uses no floating
 * point. It runs inside a Windows program (the owner's rule: Windows only, from
 * Z:, no boot image) and, as integer algorithms, on the GPU. Freestanding keeps
 * the hot code small, fast and auditable. Compilers: gcc >= 13 or clang >= 16.
 */
#ifndef AZ_TYPES_H
#define AZ_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

#define AZ_CACHELINE 64u

#define AZ_INLINE    static inline __attribute__((always_inline))
#define AZ_NOINLINE  __attribute__((noinline))
#define AZ_LIKELY(x)   __builtin_expect(!!(x), 1)
#define AZ_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define AZ_RESTRICT  __restrict__
#define AZ_ALIGNED(n) __attribute__((aligned(n)))
#define AZ_LEN(a) ((u32)(sizeof(a) / sizeof((a)[0])))

/* ---- integer helpers (all behaviour is defined: no signed overflow, no
 *      shifts of negative values to the left, arithmetic right shift assumed
 *      and checked by a static assert below) ---- */

_Static_assert((-8 >> 1) == -4, "arithmetic right shift of negative ints is required");
_Static_assert(sizeof(void *) == 8, "64-bit target required");

AZ_INLINE i32 az_min_i32(i32 a, i32 b) { return a < b ? a : b; }
AZ_INLINE i32 az_max_i32(i32 a, i32 b) { return a > b ? a : b; }
AZ_INLINE u32 az_min_u32(u32 a, u32 b) { return a < b ? a : b; }
AZ_INLINE u32 az_max_u32(u32 a, u32 b) { return a > b ? a : b; }
AZ_INLINE i64 az_min_i64(i64 a, i64 b) { return a < b ? a : b; }
AZ_INLINE i64 az_max_i64(i64 a, i64 b) { return a > b ? a : b; }
AZ_INLINE i32 az_clamp_i32(i32 v, i32 lo, i32 hi) { return v < lo ? lo : (v > hi ? hi : v); }
AZ_INLINE i64 az_clamp_i64(i64 v, i64 lo, i64 hi) { return v < lo ? lo : (v > hi ? hi : v); }
AZ_INLINE i32 az_abs_i32(i32 v) { return v < 0 ? -v : v; }

/* saturating narrowing */
AZ_INLINE u8  az_sat_u8(i32 v)  { return (u8)az_clamp_i32(v, 0, 255); }
AZ_INLINE i8  az_sat_i8(i32 v)  { return (i8)az_clamp_i32(v, -127, 127); }
AZ_INLINE u16 az_sat_u16(i32 v) { return (u16)az_clamp_i32(v, 0, 65535); }
AZ_INLINE i16 az_sat_i16(i32 v) { return (i16)az_clamp_i32(v, -32767, 32767); }

/* wrap-around add for unsigned position math is plain unsigned arithmetic */

/* 64-bit mixer (splitmix64 finaliser); used for hashing state, never as the
 * simulation RNG (that is Philox4x32-10, see az_abi.h). */
AZ_INLINE u64 az_mix64(u64 x)
{
    x ^= x >> 30; x *= 0xBF58476D1CE4E5B9ull;
    x ^= x >> 27; x *= 0x94D049BB133111EBull;
    x ^= x >> 31;
    return x;
}

#endif /* AZ_TYPES_H */
