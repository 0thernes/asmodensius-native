/* lint self-test fixture: rule E3 (libc calls). */
#include "az_abi.h"

void fx_copy(void *d, const void *s, unsigned n)
{
    memcpy(d, s, n); /* PLANT */
}

int fx_abs(int x)
{
    return abs(x); /* PLANT */
}

void fx_fail(void)
{
    abort(); /* PLANT */
}
