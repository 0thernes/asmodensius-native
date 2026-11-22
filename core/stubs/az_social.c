/* az_social.c (reference stub) - writes a valid offer, never interacts. */
#include "../az_abi.h"

void az_social_intent(AzWorld *w, u32 slot, const AzPercept *p, u32 worker)
{
    (void)p; (void)worker;
    u8 *o = &w->offer[(size_t)slot * AZ_OFFER];
    for (u32 i = 0; i < 16; i++) o[i] = 0;
    o[AZ_O_STRENGTH] = (u8)az_clamp_i32(w->mass[slot] >> 6, 0, 255);
    o[AZ_O_SIZE] = (u8)az_radius_code(w->mass[slot]);
    o[AZ_O_RANK] = 128;
}

i32 az_social_score(const AzWorld *w, u32 r, u32 c, u32 act)
{
    (void)w; (void)r; (void)c; (void)act;
    return -1;
}

void az_social_resolve(AzWorld *w, u32 slot, u32 partner, u32 act, bool initiator, u32 worker)
{
    (void)w; (void)slot; (void)partner; (void)act; (void)initiator; (void)worker;
}

void az_social_range(AzWorld *w, u32 begin, u32 end, u32 worker)
{
    (void)w; (void)begin; (void)end; (void)worker;
}
