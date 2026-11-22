/* tests/protocol/az_life.c - TEST-ONLY interacting module (protocol set).
 *
 * See tests/protocol/az_social.c for what the protocol set is and is not.
 * The reference life stub has births but no deaths (100,000 creatures, 60
 * ticks: 69,969 births, 0 deaths), so no determinism test ever saw a dead
 * slot, the free list, slot reuse by az_spawn or a clade losing members
 * (findings v31, v38, v71). This file keeps the stub's kill, budding and
 * serial commit as they are and adds what makes them run: a constant
 * mortality hazard, old age, growing up, and readiness to mate.
 */
#include "az_abi.h"

/* the reference stub, with its phase-3 pass under another name */
void pm_stub_life_range(AzWorld *w, u32 begin, u32 end, u32 worker);
#define az_life_range pm_stub_life_range
#include "../../core/stubs/az_life.c"
#undef az_life_range

#define PM_HAZARD     96u    /* one death in this many entity-ticks, by a draw */
#define PM_OLD_AGE    70u    /* ticks; plus 0..31 by uid                       */
#define PM_ADULT_AGE  6u     /* ticks a newborn stays juvenile                 */
#define PM_READY_EU   300    /* energy an adult needs to become receptive      */

void az_life_range(AzWorld *w, u32 begin, u32 end, u32 worker)
{
    for (u32 s = begin; s < end; s++) {
        u8 fl = w->flags[s];
        if (!(fl & AZ_F_ALIVE)) continue;
        const u64 uid = w->uid[s];
        const u64 age = w->tick - w->birth_tick[s];
        const u32 r = az_rand(w, uid, AZ_RS_LIFE, 0);
        if (r % PM_HAZARD == 0 || age > PM_OLD_AGE + (uid & 31u)) w->pair[s].req |= AZ_RQ_DIE;
        if ((fl & AZ_F_JUV) && age >= PM_ADULT_AGE) fl &= (u8)~AZ_F_JUV;
        /* others see this in the public record from the next tick on */
        if (!(fl & AZ_F_JUV) && w->energy[s] >= PM_READY_EU && ((r >> 8) & 3u) == 0) fl |= AZ_F_RECEPT;
        w->flags[s] = fl;
    }
    pm_stub_life_range(w, begin, end, worker);   /* the deaths asked for, then budding */
}
