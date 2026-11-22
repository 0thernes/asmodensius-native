/* az_econ.c (reference stub) - no trade. The offer's trade bytes stay zero
 * except the price, which says "not for sale". */
#include "../az_abi.h"

void az_econ_intent(AzWorld *w, u32 slot, const AzPercept *p, u32 worker)
{
    (void)p; (void)worker;
    az_offer_set_u32(&w->offer[(size_t)slot * AZ_OFFER], AZ_O_PRICE, AZ_PRICE_NONE);
}

i32 az_econ_score(const AzWorld *w, u32 r, u32 c, u32 act)
{
    (void)w; (void)r; (void)c; (void)act;
    return -1;
}

void az_econ_resolve(AzWorld *w, u32 slot, u32 partner, u32 act, bool initiator, u32 worker)
{
    (void)w; (void)slot; (void)partner; (void)act; (void)initiator; (void)worker;
}

void az_econ_range(AzWorld *w, u32 begin, u32 end, u32 worker)
{
    (void)w; (void)begin; (void)end; (void)worker;
}

void az_econ_global(AzWorld *w)
{
    (void)w;
}
