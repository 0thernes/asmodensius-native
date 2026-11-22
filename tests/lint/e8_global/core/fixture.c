/* lint self-test fixture: rule E8 (non-static file-scope variables). */
#include "az_abi.h"
#include "az_fixture.h"

unsigned az_hits; /* PLANT */
unsigned az_hist[16] = { 0u }; /* PLANT */
const unsigned char *az_cursor; /* PLANT */
void (*az_hook)(unsigned); /* PLANT */
struct fx_pair { unsigned a; unsigned b; } az_pair; /* PLANT */
unsigned az_a, az_b; /* PLANT */
_Alignas(64) unsigned az_line[16]; /* PLANT */
struct fx_multi { /* PLANT */
    unsigned a;
} az_multi;

extern unsigned az_extern_ok;
const unsigned az_const_ok = 1u;
unsigned az_fn(unsigned x);
struct fx_tag;
enum fx_e { FX_E0 };
typedef unsigned fx_t;
_Static_assert(sizeof(unsigned) == 4, "u32");

unsigned az_fn(unsigned x)
{
    return x + az_hits + az_const_ok;
}
