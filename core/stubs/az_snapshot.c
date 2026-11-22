/* az_snapshot.c (reference stub) - not implemented: save returns 0, load NULL. */
#include "../az_abi.h"

u64 az_snapshot_bytes(const AzWorld *w)
{
    (void)w;
    return 0;
}

u64 az_snapshot_save(const AzWorld *w, u8 *buf, u64 cap)
{
    (void)w; (void)buf; (void)cap;
    return 0;
}

bool az_snapshot_config(const u8 *buf, u64 n, AzConfig *out)
{
    (void)buf; (void)n; (void)out;
    return false;
}

AzWorld *az_snapshot_load(AzArena *a, const u8 *buf, u64 n, AzHost *host)
{
    (void)a; (void)buf; (void)n; (void)host;
    return NULL;
}
