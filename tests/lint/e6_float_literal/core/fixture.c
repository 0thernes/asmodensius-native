/* lint self-test fixture: rule E6 (floating literals). 1.5 here is a comment. */
#include "az_abi.h"

#define FX_HALF 0.5 /* PLANT */
static const int fx_a = (int)1.5; /* PLANT */
static const int fx_b = (int)1e3; /* PLANT */
static const int fx_c = (int)2.f; /* PLANT */
static const int fx_d = (int).25; /* PLANT */
static const int fx_e = (int)1.0L; /* PLANT */
static const int fx_f = (int)0x1p3; /* PLANT */
static const int fx_g = (int)0x1.8p1; /* PLANT */
static const int fx_h = (int)1E-3; /* PLANT */
static const unsigned long long fx_ints[7] = { 10u, 10UL, 0xFFull, 0xE5, 077, 1000000000LL, 0b11 };
static const char fx_s[] = "2.5e3";

int fx_sum(void)
{
    return fx_a + fx_b + fx_c + fx_d + fx_e + fx_f + fx_g + fx_h + (int)fx_ints[0] + fx_s[0];
}
