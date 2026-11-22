/* lint self-test fixture: rule E9 (function-local mutable statics), in stubs/. */
#include "az_abi.h"

unsigned fx_count(void)
{
    static unsigned calls; /* PLANT */
    static const unsigned k = 3u;
    calls++;
    return calls + k;
}

unsigned fx_table(unsigned i)
{
    static unsigned short memo[8] = { 0 }; /* PLANT */
    static const unsigned char *last; /* PLANT */
    static const unsigned char *const tbl = 0;
    if (i < 8u) {
        static _Thread_local unsigned depth; /* PLANT */
        depth++;
        memo[i]++;
    }
    (void)last;
    (void)tbl;
    return memo[i & 7u];
}
