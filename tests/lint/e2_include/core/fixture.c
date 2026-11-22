/* lint self-test fixture: rule E2 (non-freestanding includes). */
#include "az_abi.h"
#include <stdint.h>
#include <string.h> /* PLANT */
#include <windows.h> /* PLANT */
#include "helper.h" /* PLANT */

int fx_one(void)
{
    return 1;
}
