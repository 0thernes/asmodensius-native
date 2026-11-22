/* az_colony.c (reference stub) - the store plumbing is real (it keeps the
 * books exact); founding, membership, roles and the Tier 2 pass are not. */
#include "../az_abi.h"

#include "../az_colony_store.h"

void az_colony_grant(AzWorld *w)
{
    az_grant_colony_stores(w);
}

void az_colony_commit(AzWorld *w)
{
    az_commit_colony_stores(w);
}

void az_colony_tier2(AzWorld *w)
{
    (void)w;
}

u32 az_colony_found(AzWorld *w, u32 founder_slot)
{
    (void)w; (void)founder_slot;
    return 0;
}
