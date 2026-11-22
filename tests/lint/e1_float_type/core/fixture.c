/* lint self-test fixture: rule E1 (floating point types). */
#include "az_abi.h"

float fx_ratio(int a); /* PLANT */

static int fx_conv(int a)
{
    double d = a; /* PLANT */
    long double e = d; /* PLANT */
    return (int)e;
}

int fx_use(int a)
{
    return fx_conv(a);
}
