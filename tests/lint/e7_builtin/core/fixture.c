/* lint self-test fixture: rule E7 (builtins outside the integer allow-list). */
#include "az_abi.h"

#define FX_FLOOR(x) __builtin_floorf(x) /* PLANT */

unsigned fx_root(unsigned x)
{
    return (unsigned)__builtin_sqrt(x); /* PLANT */
}

unsigned fx_bits(unsigned long long x)
{
    return (unsigned)__builtin_popcountll(x) + (unsigned)__builtin_ctzll(x); /* PLANT */
}

void fx_trap(void)
{
    __builtin_trap(); /* PLANT */
}
