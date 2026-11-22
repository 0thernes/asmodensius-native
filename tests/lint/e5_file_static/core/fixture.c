/* lint self-test fixture: rule E5 (mutable file-scope statics). */
#include "az_abi.h"

static unsigned fx_calls; /* PLANT */
static unsigned fx_hist[16] = { 0u }; /* PLANT */
static _Alignas(64) unsigned fx_aligned[8]; /* PLANT */
static const unsigned char *fx_cursor; /* PLANT */
static void (*fx_hook)(unsigned); /* PLANT */
static unsigned /* PLANT */
    fx_multi;
static const unsigned fx_ok[2] = { 1u, 2u };
static unsigned const fx_ok2 = 3u;
static const unsigned char *const fx_ok3 = 0;

static unsigned fx_fn(void)
{
    return fx_ok[0] + fx_ok2 + (fx_ok3 != 0);
}

unsigned fx_pub(void)
{
    return fx_fn();
}
