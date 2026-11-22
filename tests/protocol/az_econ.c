/* tests/protocol/az_econ.c - TEST-ONLY interacting module (protocol set).
 *
 * See tests/protocol/az_social.c for what the protocol set is and is not.
 * This file gives the trade branch of the pair dispatch real traffic: an
 * entity sets a little of its pool aside as inventory (slot 1, organic
 * matter), offers it, and a trade swaps up to PM_LOT units each way. It is a
 * swap of one good, not an economy; it exists so that az_econ_score and
 * az_econ_resolve run, the offer's trade bytes are read on both sides, and
 * the receive cap and the exact trade tally are exercised.
 *
 * It also gives the colony's energy pool and its voice channels real traffic
 * (ABI 4: a colony is an organism with a pool of its own). A member is a
 * taker in the ticks where uid + tick is odd and a giver in the others, so
 * the rule reads the same in phase 1, where it asks, and in phase 3, where it
 * takes. What the takers ask alternates every 8 ticks between less and more
 * than the givers pay in, so a pool both fills (grants in full) and runs dry
 * (grants in part). Energy only moves between a member and its colony.
 */
#include "az_abi.h"
#define AZ_INV_MAX 60000u      /* this test bench's own limit of an inventory slot */

#define PM_LOT       16u    /* most units one trade moves each way            */
#define PM_SET_ASIDE 2u     /* units of pool_a put into inventory per tick    */
#define PM_POOL_KEEP 150u   /* ... only while the pool stays above this       */
#define PM_INV       1u     /* inventory slot 1 holds AZ_SP_ORG_Y             */
#define PM_GIVE      40     /* eu a giver pays into its colony's pool         */
#define PM_GIVE_KEEP 200    /* ... only while its own energy stays above this */

static bool pm_taker(const AzWorld *w, u32 slot) { return ((w->uid[slot] + w->tick) & 1u) != 0; }
static u32 pm_ask(const AzWorld *w) { return ((w->tick >> 3) & 1u) ? 64u : 16u; }

void az_econ_intent(AzWorld *w, u32 slot, const AzPercept *p, u32 worker)
{
    (void)worker;
    if (w->colony[slot] && pm_taker(w, slot)) az_colony_ask_energy(w, w->colony[slot], pm_ask(w));
    u8 *o = &w->offer[(size_t)slot * AZ_OFFER];
    const u32 have = w->inv[(size_t)slot * AZ_NINV + PM_INV];
    o[AZ_O_GIVE_RES] = PM_INV;
    o[AZ_O_WANT_RES] = PM_INV;
    az_offer_set_u32(o, AZ_O_GIVE_QTY, have);                  /* exact */
    az_offer_set_u32(o, AZ_O_PRICE, have ? 65536u : AZ_PRICE_NONE);    /* nothing to sell: not for sale */

    const u8 fl = w->flags[slot];
    if (!(fl & AZ_F_INIT) || (fl & (AZ_F_EGG | AZ_F_DORMANT)) || have == 0) return;
    const u32 r = az_rand(w, w->uid[slot], AZ_RS_ECON, 0);
    if (r & 1u) return;
    const u32 target = p->near_kin != AZ_NONE ? p->near_kin : p->near_prey;
    if (target == AZ_NONE || az_is_initiator(w, w->pub_id[w->spos[target]].uid)) return;
    const u32 urge = 1u + ((r >> 8) & 127u);
    AzPair *pr = &w->pair[slot];
    if (urge > pr->urge) { pr->act = AZ_ACT_TRADE; pr->peer = target; pr->urge = (u8)urge; }
}

i32 az_econ_score(const AzWorld *w, u32 r, u32 c, u32 act)
{
    if (act != AZ_ACT_TRADE) return -1;
    const u8 *oc = &w->offer[(size_t)c * AZ_OFFER];
    const u8 *orr = &w->offer[(size_t)r * AZ_OFFER];
    if (az_offer_u32(orr, AZ_O_PRICE) == AZ_PRICE_NONE) return -1;   /* the responder sells nothing */
    return (i32)az_min_u32(az_offer_u32(oc, AZ_O_GIVE_QTY), 0x7FFFu);   /* the best-stocked claimant */
}

void az_econ_resolve(AzWorld *w, u32 slot, u32 partner, u32 act, bool initiator, u32 worker)
{
    if (act != AZ_ACT_TRADE) return;
    const u8 *mine = &w->offer[(size_t)slot * AZ_OFFER];
    const u8 *theirs = &w->offer[(size_t)partner * AZ_OFFER];
    const u32 give = az_min_u32(az_offer_u32(mine, AZ_O_GIVE_QTY), PM_LOT);
    const u32 get = az_min_u32(az_offer_u32(theirs, AZ_O_GIVE_QTY), PM_LOT);
    u32 *inv = &w->inv[(size_t)slot * AZ_NINV + PM_INV];
    const u32 left = (u32)*inv - give;             /* the giver removes the full amount */
    const u32 room = AZ_INV_MAX - left;
    const u32 add = get < room ? get : room;       /* receive cap: the rest goes to my cell */
    *inv = left + add;
    if (get > add) az_cell_post(w, worker, w->cell[slot], az_inv_species[PM_INV], (i32)(get - add));
    az_tally(w, worker)->trade_units[PM_INV][PM_INV] += give;
    if (initiator) az_emit(w, worker, AZ_EV_TRADE, (u32)PM_INV, w->uid[slot], w->uid[partner], (i32)give);
}

void az_econ_range(AzWorld *w, u32 begin, u32 end, u32 worker)
{
    (void)worker;
    /* a stand-in for foraging: biomass set aside as stored organic matter.
     * Both forms carry 16 eu per mu, so the move books no energy. */
    for (u32 s = begin; s < end; s++) {
        const u8 fl = w->flags[s];
        if (!(fl & AZ_F_ALIVE)) continue;
        const u32 col = w->colony[s];
        if (col) {
            if (pm_taker(w, s)) w->energy[s] += (i32)az_colony_take_energy(w, col, pm_ask(w));
            else if (w->energy[s] > PM_GIVE_KEEP) { w->energy[s] -= PM_GIVE; az_colony_give_energy(w, col, PM_GIVE); }
            az_colony_say(w, col, (u32)w->uid[s] & 3u, (u32)w->energy[s] >> 4);
        }
        if (fl & (AZ_F_EGG | AZ_F_DORMANT)) continue;
        u32 *inv = &w->inv[(size_t)s * AZ_NINV + PM_INV];
        if (w->pool_a[s] <= PM_POOL_KEEP || *inv > AZ_INV_MAX - PM_SET_ASIDE) continue;
        w->pool_a[s] -= PM_SET_ASIDE;
        *inv += PM_SET_ASIDE;
    }
}

void az_econ_global(AzWorld *w)
{
    (void)w;
}
