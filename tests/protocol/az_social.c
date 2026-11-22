/* tests/protocol/az_social.c - TEST-ONLY interacting module (protocol set).
 *
 * Not part of the engine. It is compiled into a second core library
 * (az_core_proto) in place of the social module and linked only into
 * test_protocol and az_run_proto, so that the pair-matching protocol of the
 * tick (claims, settle, resolve, tallies) carries real traffic under the
 * determinism and conservation checks (findings s1, v31, v38, v71). With the
 * reference stubs every score is -1 and no pair ever forms.
 *
 * The behaviour is crude on purpose. What it must do is follow every rule a
 * real module follows (docs/ARCHITECTURE.md sections 4, 6 and 7):
 *   - only an initiator proposes, and only to a responder;
 *   - a proposal replaces the standing one only with a strictly larger urge;
 *   - quantities in the offer are exact and capped by what the entity holds;
 *   - both sides compute the outcome from the two offers, the uids and one
 *     shared draw, and each changes only itself;
 *   - the giver removes the full amount, the receiver applies the receive cap
 *     and posts the rest to its own cell in the same block.
 *
 * Acts: ATTACK (a contest; the loser hands its energy and matter stakes to
 * the winner) and SHARE (the initiator gives its energy stake to a poorer
 * responder, who may refuse).
 */
#include "az_abi.h"
#define AZ_POOL_MAX 12000      /* this test bench's own limits of a pool and of the energy store */
#define AZ_ENERGY_MAX 1000000

#define PM_STAKE_MAX 60000u   /* an offer holds 16 bits */

/* add q eu to my energy store; what does not fit becomes heat in my cell */
static void pm_receive_energy(AzWorld *w, u32 slot, u32 cell, u32 q, u32 worker)
{
    const i32 room = AZ_ENERGY_MAX - w->energy[slot];
    const i32 add = az_min_i32((i32)q, room > 0 ? room : 0);
    w->energy[slot] += add;
    if ((i32)q > add) az_cell_post_heat(w, worker, cell, (i32)q - add);
}

void az_social_intent(AzWorld *w, u32 slot, const AzPercept *p, u32 worker)
{
    (void)worker;
    u8 *o = &w->offer[(size_t)slot * AZ_OFFER];
    for (u32 i = 0; i < 16; i++) o[i] = 0;
    const u32 mass = w->mass[slot];
    o[AZ_O_STRENGTH] = (u8)az_clamp_i32((i32)(mass >> 6), 0, 255);
    o[AZ_O_SIZE] = (u8)az_radius_code(mass);
    o[AZ_O_RANK] = 128;
    /* stakes: a quarter of what I hold now. Nothing changes my energy or my
     * pool between this hook and my pair's resolve, so they cannot overdraw. */
    const u32 e = w->energy[slot] > 0 ? (u32)w->energy[slot] : 0u;
    az_offer_set_u32(o, AZ_O_ENERGY, az_min_u32(e / 4u, PM_STAKE_MAX));
    az_offer_set_u32(o, AZ_O_MATTER, (u32)w->pool_a[slot] / 4u);

    const u8 fl = w->flags[slot];
    if (!(fl & AZ_F_INIT) || (fl & (AZ_F_EGG | AZ_F_DORMANT))) return;
    const u32 r = az_rand(w, w->uid[slot], AZ_RS_SOCIAL, 0);
    u32 target, act;
    switch (r & 3u) {
    case 0:  target = p->near_prey;   act = AZ_ACT_ATTACK; break;
    case 1:  target = p->near_kin;    act = AZ_ACT_SHARE;  break;
    case 2:  target = p->near_threat; act = AZ_ACT_ATTACK; break;
    default: return;
    }
    if (target == AZ_NONE) return;
    /* the role of the target, from its public uid (the percept is role-blind) */
    if (az_is_initiator(w, w->pub_id[w->spos[target]].uid)) return;
    const u32 urge = 1u + ((r >> 8) & 127u);
    AzPair *pr = &w->pair[slot];
    if (urge > pr->urge) { pr->act = (u8)act; pr->peer = target; pr->urge = (u8)urge; }
}

i32 az_social_score(const AzWorld *w, u32 r, u32 c, u32 act)
{
    const u8 *oc = &w->offer[(size_t)c * AZ_OFFER];
    const u8 *orr = &w->offer[(size_t)r * AZ_OFFER];
    /* coercive: the responder's priority among attackers, not consent */
    if (act == AZ_ACT_ATTACK) return (i32)oc[AZ_O_STRENGTH];
    if (act == AZ_ACT_SHARE) {
        /* consent: a gift is taken only from a richer claimant */
        const u32 give = az_offer_u32(oc, AZ_O_ENERGY), have = az_offer_u32(orr, AZ_O_ENERGY);
        return give > have ? (i32)(give - have) : -1;
    }
    return -1;
}

void az_social_resolve(AzWorld *w, u32 slot, u32 partner, u32 act, bool initiator, u32 worker)
{
    const u8 *mine = &w->offer[(size_t)slot * AZ_OFFER];
    const u8 *theirs = &w->offer[(size_t)partner * AZ_OFFER];
    const u32 cell = w->cell[slot];

    if (act == AZ_ACT_SHARE) {
        const u32 q = az_offer_u32(initiator ? mine : theirs, AZ_O_ENERGY);   /* the initiator's gift */
        if (initiator) w->energy[slot] -= (i32)q;
        else pm_receive_energy(w, slot, cell, q, worker);
        return;
    }
    if (act != AZ_ACT_ATTACK) return;

    /* a contest: strength plus one draw both sides share (smaller uid as id,
     * larger in idx); a tie goes to the lower uid */
    const u64 my_uid = w->uid[slot], their_uid = w->uid[partner];
    const u64 lo = my_uid < their_uid ? my_uid : their_uid;
    const u64 hi = my_uid < their_uid ? their_uid : my_uid;
    const u32 d = az_rand(w, lo, AZ_RS_CONTEST, hi);
    const u8 *oi = initiator ? mine : theirs;
    const u8 *orr = initiator ? theirs : mine;
    const u64 uid_i = initiator ? my_uid : their_uid;
    const u64 uid_r = initiator ? their_uid : my_uid;
    const u32 si = (u32)oi[AZ_O_STRENGTH] + (d & 63u);
    const u32 sr = (u32)orr[AZ_O_STRENGTH] + ((d >> 8) & 63u);
    const bool init_wins = si > sr || (si == sr && uid_i < uid_r);
    const bool i_win = initiator == init_wins;
    const u8 *loser = i_win ? theirs : mine;
    const u32 qe = az_offer_u32(loser, AZ_O_ENERGY);
    const u32 qm = az_offer_u32(loser, AZ_O_MATTER);

    if (i_win) {
        pm_receive_energy(w, slot, cell, qe, worker);
        /* biomass into my pool; what does not fit stays in my cell as organic
         * matter (16 eu per mu either way, so no energy is booked) */
        const u32 room = AZ_POOL_MAX - w->pool_a[slot];
        const u32 add = qm < room ? qm : room;
        w->pool_a[slot] += add;
        if (qm > add) az_cell_post(w, worker, cell, AZ_SP_ORG_Y, (i32)(qm - add));
    } else {
        w->energy[slot] -= (i32)qe;
        w->pool_a[slot] -= qm;
    }
    if (initiator) az_emit(w, worker, AZ_EV_ATTACK, (u32)init_wins, my_uid, their_uid, (i32)qe);
}

void az_social_range(AzWorld *w, u32 begin, u32 end, u32 worker)
{
    (void)w; (void)begin; (void)end; (void)worker;
}
