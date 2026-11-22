/* tests/protocol/az_mate.c - TEST-ONLY interacting module (protocol set).
 *
 * See tests/protocol/az_social.c for what the protocol set is and is not.
 * This file gives the mate branch of the pair dispatch real traffic and makes
 * sexual births happen: an initiator courts the nearest receptive adult of
 * its percept; a receptive responder picks the showiest claimant and refuses
 * some; the responder then bears one or two young (az_birth_request with the
 * initiator as father), which az_life_commit creates through az_genome_cross.
 *
 * It is also the last of the three intent hooks, so it counts the proposals
 * that stand after all three (in the worker tally's AZ_SAT_OTHER counter:
 * this set's own use, a real module has none). Proposals minus resolved
 * pairs is the number of claims that were refused or lost to a rival.
 */
#include "az_abi.h"

#define PM_COURT_COST 8      /* eu the suitor pays as heat                    */
#define PM_PROV_MIN   20     /* a mother this poor bears no young             */

void az_mate_intent(AzWorld *w, u32 slot, const AzPercept *p, u32 worker)
{
    u8 *o = &w->offer[(size_t)slot * AZ_OFFER];
    const u8 fl = w->flags[slot];
    o[AZ_O_ORNAMENT] = (u8)(w->tag[slot] >> 8);
    if (fl & AZ_F_RECEPT) o[AZ_O_DISP] |= AZ_OD_RECEPTIVE;
    if (w->phen[(size_t)slot * AZ_PHEN + AZ_P_MATE] & 8u) o[AZ_O_DISP] |= AZ_OD_SEX;

    AzPair *pr = &w->pair[slot];
    if ((fl & AZ_F_INIT) && !(fl & (AZ_F_EGG | AZ_F_DORMANT | AZ_F_JUV)) && p->best_mate != AZ_NONE) {
        const u32 r = az_rand(w, w->uid[slot], AZ_RS_MATE, 0);
        const u32 target = p->best_mate;
        const u32 urge = 1u + ((r >> 8) & 127u);
        if ((r & 1u) && !az_is_initiator(w, w->pub_id[w->spos[target]].uid) && urge > pr->urge) {
            pr->act = AZ_ACT_MATE; pr->peer = target; pr->urge = (u8)urge;
        }
    }
    if ((fl & AZ_F_INIT) && pr->act != AZ_ACT_NONE) az_tally(w, worker)->sat[AZ_SAT_OTHER]++;
}

i32 az_mate_score(const AzWorld *w, u32 r, u32 c, u32 act)
{
    if (act != AZ_ACT_MATE) return -1;
    const u8 *oc = &w->offer[(size_t)c * AZ_OFFER];
    const u8 *orr = &w->offer[(size_t)r * AZ_OFFER];
    if (!(orr[AZ_O_DISP] & AZ_OD_RECEPTIVE)) return -1;
    /* choice: a responder refuses a suitor of its own display class and
     * otherwise prefers the showiest */
    if (((orr[AZ_O_ORNAMENT] ^ oc[AZ_O_ORNAMENT]) & 3u) == 0) return -1;
    return (i32)oc[AZ_O_ORNAMENT];
}

void az_mate_resolve(AzWorld *w, u32 slot, u32 partner, u32 act, bool initiator, u32 worker)
{
    if (act != AZ_ACT_MATE) return;
    if (initiator) {
        /* the cost of courtship, paid as heat (rule C3) */
        const i32 pay = az_min_i32(PM_COURT_COST, w->energy[slot] > 0 ? w->energy[slot] : 0);
        w->energy[slot] -= pay;
        az_cell_post_heat(w, worker, w->cell[slot], pay);
        az_emit(w, worker, AZ_EV_MATE, 0, w->uid[slot], w->uid[partner], 0);
        return;
    }
    /* the responder bears the young: a quarter of her energy and of her pool */
    const i32 e = w->energy[slot] > 0 ? w->energy[slot] : 0;
    const i32 prov_e = e / 4;
    const u32 prov_a = (u32)w->pool_a[slot] / 4u;
    if (prov_e < PM_PROV_MIN || prov_a < (u32)PM_PROV_MIN) return;
    const u32 n = 1u + (az_rand(w, w->uid[slot], AZ_RS_MATE, 1) & 1u);
    if (az_birth_request(w, slot, partner, n, prov_e, prov_a, 0, 0)) {
        w->energy[slot] -= prov_e;
        w->pool_a[slot] -= prov_a;
        w->flags[slot] &= (u8)~AZ_F_RECEPT;
    }
}
