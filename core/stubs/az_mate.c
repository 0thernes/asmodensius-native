/* az_mate.c (reference stub) - no sexual reproduction. */
#include "../az_abi.h"

void az_mate_intent(AzWorld *w, u32 slot, const AzPercept *p, u32 worker)
{
    (void)w; (void)slot; (void)p; (void)worker;
}

i32 az_mate_score(const AzWorld *w, u32 r, u32 c, u32 act)
{
    (void)w; (void)r; (void)c; (void)act;
    return -1;
}

void az_mate_resolve(AzWorld *w, u32 slot, u32 partner, u32 act, bool initiator, u32 worker)
{
    (void)w; (void)slot; (void)partner; (void)act; (void)initiator; (void)worker;
}
