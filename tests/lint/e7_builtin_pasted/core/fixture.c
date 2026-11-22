/* lint fixture: E7, a builtin name glued together by the preprocessor */
#include "az_abi.h"
#define AZ_BI(x) __builtin_##x /* PLANT */
u32 az_fixture_pasted(u32 n);
u32 az_fixture_pasted(u32 n) { return n; }
