/* lint self-test fixture: everything here is allowed in core/.
   Floats in comments are fine: 1.5 2.0f 0x1p3 1e3. */
#include "az_abi.h"
#include <stdint.h>

typedef unsigned int u32x;
typedef uint64_t u64x;
typedef u32x v8x __attribute__((vector_size(32)));
struct fx_pair { u32x a; u32x b; };
enum fx_kind { FX_A = 1, FX_B = 0x10u };
union fx_bits { u32x w; unsigned char b[4]; };
typedef struct { u32x n; } FxCount;
struct fx_tag;
_Static_assert(sizeof(struct fx_pair) == 8, "pair 1.5 size");
extern u32x fx_external;
u32x fx_proto(u32x x);
u32x fx_rows(const u32x *w, int (*off)[3], u32x n);
u32x fx_sum(u32x n, const u32x a[static 4]);
void fx_apply(void (*fn)(u32x), u32x x);
static const u32x fx_table[4] = { 10u, 0xFFu, 077, 1UL };
static const char *const fx_names[2] = { "a.b", "1.5e3" };
static u32x const fx_after = 3u;
static const struct fx_pair fx_zero = { 0u, 0u };
static void (*const fx_hook)(u32x) = 0;
static u32x fx_helper(u32x x) __attribute__((unused));

static u32x fx_helper(u32x x)
{
    static const u64x k = 0x9E3779B97F4A7C15ull;
    static const char *const msg = "static u32 calls; 2.5";
    u32x n = (u32x)__builtin_popcountll(k) + (u32x)__builtin_clz(x | 1u);
    (void)msg;
    if (__builtin_expect(x == 0, 0)) {
        return 0;
    }
    return n + fx_table[x & 3u] + 1000000u + 0xE5u + 0b101 + 12LL + fx_after;
}

u32x fx_proto(u32x x)
{
    struct fx_pair p = { x, 2u };
    char c = '.';
    (void)c;
    (void)fx_hook;
    return fx_helper(p.a) + p.b + (u32x)sizeof(fx_names) + fx_zero.a;
}
