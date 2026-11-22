#ifndef AZ_PARTNER_HOOKS_H
#define AZ_PARTNER_HOOKS_H
#include "az_abi.h"
/* Only native reporter builds link these callbacks. */
void azt_partner_resolved(const AzWorld *w, u32 slot, u32 peer);
void azt_partner_tick(const AzWorld *w);
void azt_partner_reorder(const AzWorld *w);
#endif
