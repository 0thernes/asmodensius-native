/* az_mate.c - who mates with whom.
 *
 * Mating is a game between two creatures, not a table. One side proposes
 * (phase 1), the other scores every suitor it got through its own preference
 * genes and takes the best one that passes its own threshold (phase 2), and
 * the pair is carried out by both, each for itself (phase 3). The bearing
 * side asks for the brood through az_life_brood; nobody else creates births.
 *
 * What a creature can be in a mating comes from its kind (MATING_MODE, in
 * phen[AZ_P_MATE]) and its mating type (the sex locus, bit 3 of the same
 * byte):
 *   budder, selfer        breed alone (az_life.c); never propose, never accept
 *   hermaphrodite         can bear and can sire
 *   the two-sex kinds     mating type 0 bears, mating type 1 sires
 *   eusocial kind         as two sexes, and inside a colony only the breeder
 *                         caste mates (a solitary one may: it founds)
 * Two creatures can breed when one can bear and the other can sire. Between
 * two hermaphrodites each is ready to bear by its own SEX_BIAS: a draw both
 * sides compute alike, with the odds SEX_BIAS + 1 to SEX_BIAS + 1.
 *
 * Phase 1 (HOT state only). Offer byte AZ_O_ORNAMENT is the display the
 * brain puts into courtship now: twice its drive on AZ_M_REPRO. The bits:
 * RECEPTIVE (ripe and active), SEX, BONDED (my partner is beside me) and
 * BREEDER (may mate under its kind's rule). A ripe initiator of a mating
 * kind whose brain drives AZ_M_REPRO proposes AZ_ACT_MATE with that drive as
 * its urge: to its bonded partner if the partner is beside it and ripe, else
 * to the nearest ripe adult of its percept. With the partner beside it but
 * not ripe it stays with a chance of bond / 256.
 *
 * Phase 2, the chooser's side (pure). What the responder sees of the suitor:
 *   show   its ORNAMENT, multiplied by its SHEEN, as far as it displays now,
 *          plus the signals it emits; all through MATE_SHOW_GAIN
 *   size   the size code of its public record
 *   like   how alike the two are in PATTERN (the low byte of the tag) and in
 *          body plan (SHAPE_M); close kin count as most alike
 *   rank   the rank it offers
 * Her quality scale is the mean of the four weighted by her own PREF_SIGNAL,
 * PREF_SIZE, PREF_SIMILAR and PREF_RANK (Lande 1981 and Kirkpatrick 1982: a
 * preference gene and a display gene, both heritable). Below CHOOSY *
 * MATE_CHOOSY_SCALE / 256 she refuses and waits for a better one (the fixed
 * threshold of sequential search: Real 1990); among the suitors of one tick
 * the best wins (the pair matcher keeps the highest score). A chooser with no
 * preference gene has no scale and takes whoever comes.
 *   Her bonded partner is taken without a second look, and its GUARD adds to
 * its claim, so a guarding partner outbids a rival of higher quality. With
 * the partner beside her she refuses anyone else with a chance of her own
 * FIDELITY / 256; a widow's partner is never beside her, so she is free.
 *
 * Phase 3, each side for itself. The bearing side calls az_life_brood and
 * emits MATE (aux 1: it also bound itself). The siring side pays for its
 * display as heat (MATE_COURT_EU for a full one). Bonds: the kinds that pair
 * up (hermaphrodite, monogamous, eusocial; in the polygynous kind only the
 * bearing side) bind themselves to the mate with a chance of BOND / 256; a
 * mating with the partner strengthens the bond by 1 + BOND * MATE_BOND_STEP /
 * 256, one outside it halves it. The promiscuous and the lekking kinds form
 * none. An initiator that sired emits BOND when its bond formed or broke (an
 * entity emits one pair event per tick).
 *
 * AZ_LESION_MATING drops mate pairs in the tick: budding and selfing go on.
 */
#include "az_abi.h"
#ifdef AZ_PARTNER_OBSERVER
#include "az_partner_hooks.h"
#endif

AZ_INLINE u32 wp(const AzWorld *w, u32 id) { return w->param[id]; }

enum { CAN_BEAR = 1, CAN_SIRE = 2 };

static u32 capacity(const AzWorld *w, u32 s)
{
    const u32 pm = w->phen[(size_t)s * AZ_PHEN + AZ_P_MATE];
    const u32 mode = pm & 7u;
    if (mode < AZ_MM_HERMA) return 0;
    if (mode == AZ_MM_HERMA) return CAN_BEAR | CAN_SIRE;
    return (pm & 8u) ? CAN_SIRE : CAN_BEAR;
}

/* which of the two bears; AZ_NONE if they cannot breed together. The same
 * answer whichever side asks (uid, phen and trait are constant in phase 3). */
static u32 bearer(const AzWorld *w, u32 a, u32 b)
{
    const u32 ca = capacity(w, a), cb = capacity(w, b);
    const bool a_can = (ca & CAN_BEAR) && (cb & CAN_SIRE), b_can = (cb & CAN_BEAR) && (ca & CAN_SIRE);
    if (!a_can || !b_can) return a_can ? a : b_can ? b : AZ_NONE;
    const u32 lo = w->uid[a] < w->uid[b] ? a : b, hi = lo == a ? b : a;
    const u32 wl = (u32)w->trait[(size_t)lo * AZ_TRAITS + AZ_T_SEX_BIAS] + 1u;
    const u32 wh = (u32)w->trait[(size_t)hi * AZ_TRAITS + AZ_T_SEX_BIAS] + 1u;
    const u32 r = az_rand(w, w->uid[lo] ^ az_mix64(w->uid[hi]), AZ_RS_MATE, 0);
    return (r % (wl + wh)) < wl ? lo : hi;
}

void az_mate_intent(AzWorld *w, u32 slot, const AzPercept *p, u32 worker)
{
    (void)worker;
    u8 *o = &w->offer[(size_t)slot * AZ_OFFER];
    const u8 fl = w->flags[slot];
    const u32 mode = w->phen[(size_t)slot * AZ_PHEN + AZ_P_MATE] & 7u;
    const i32 drive = w->motor[(size_t)slot * AZ_MOT + AZ_M_REPRO];
    const bool active = !(fl & (AZ_F_EGG | AZ_F_JUV | AZ_F_DORMANT));

    o[AZ_O_ORNAMENT] = (active && drive > 0) ? (u8)(drive * 2) : 0;
    u8 d = o[AZ_O_DISP];
    if ((fl & AZ_F_RECEPT) && active) d |= AZ_OD_RECEPTIVE;
    if (w->phen[(size_t)slot * AZ_PHEN + AZ_P_MATE] & 8u) d |= AZ_OD_SEX;
    if (w->bond[slot] && p->bond != AZ_NONE) d |= AZ_OD_BONDED;
    if (mode != AZ_MM_EUSOC || w->colony[slot] == 0 || w->role[slot] == AZ_ROLE_BREEDER) d |= AZ_OD_BREEDER;
    o[AZ_O_DISP] = d;

    if (!(fl & AZ_F_INIT) || !(d & AZ_OD_RECEPTIVE) || !(d & AZ_OD_BREEDER) || mode < AZ_MM_HERMA || drive <= 0)
        return;
    u32 target = p->best_mate;
    if (d & AZ_OD_BONDED) {
        const u8 rf = w->pub[w->spos[p->bond]].rf;
        if ((rf & (AZ_PF_RECEPT | AZ_PF_PASSIVE | AZ_PF_JUV)) == AZ_PF_RECEPT) target = p->bond;
        else if ((az_rand(w, w->uid[slot], AZ_RS_MATE, 1) & 255u) < w->bond[slot]) return;
    }
    if (target == AZ_NONE || az_is_initiator(w, w->pub_id[w->spos[target]].uid)) return;
    AzPair *pr = &w->pair[slot];
    if ((u32)drive > pr->urge) { pr->act = AZ_ACT_MATE; pr->peer = target; pr->urge = (u8)drive; }
}

i32 az_mate_score(const AzWorld *w, u32 r, u32 c, u32 act)
{
    if (act != AZ_ACT_MATE) return -1;
    const u8 *orr = &w->offer[(size_t)r * AZ_OFFER], *oc = &w->offer[(size_t)c * AZ_OFFER];
    const u32 need = AZ_OD_RECEPTIVE | AZ_OD_BREEDER;
    if ((orr[AZ_O_DISP] & need) != need || (oc[AZ_O_DISP] & need) != need) return -1;
    if (bearer(w, r, c) == AZ_NONE) return -1;

    const u8 *tr = &w->trait[(size_t)r * AZ_TRAITS], *tc = &w->trait[(size_t)c * AZ_TRAITS];
    const AzPub *pc = &w->pub[w->spos[c]];
    const AzPubId *ic = &w->pub_id[w->spos[c]], *ir = &w->pub_id[w->spos[r]];

    /* what she sees of the suitor */
    const u32 gain = wp(w, AZ_WP_MATE_SHOW_GAIN);
    const u32 orn = ((u32)tc[AZ_T_ORNAMENT] * (256u + (((u32)tc[AZ_T_SHEEN] * wp(w, AZ_WP_MATE_SHEEN_GAIN)) >> 7))) >> 8;
    const u32 show = (u32)(((u64)(((orn * oc[AZ_O_ORNAMENT]) >> 8) + (((u32)pc->sig_a + pc->sig_b) >> 2)) * gain) >> 8);
    const u32 size = AZ_PUB_SIZE(pc->rf) * 17u;
    const bool kin = (ic->parent_a && (ic->parent_a == ir->parent_a || ic->parent_a == ir->parent_b ||
                                       ic->parent_a == ir->uid)) ||
                     (ic->parent_b && (ic->parent_b == ir->parent_a || ic->parent_b == ir->parent_b ||
                                       ic->parent_b == ir->uid)) ||
                     ir->parent_a == ic->uid || ir->parent_b == ic->uid;
    const u32 unlike = ((u32)az_abs_i32((i32)(w->tag[r] & 255u) - (i32)(pc->tag & 255u)) +
                        (u32)az_abs_i32((i32)tr[AZ_T_SHAPE_M] - (i32)tc[AZ_T_SHAPE_M])) >> 1;   /* PATTERN, SHAPE_M */
    const u32 like = kin ? 255u : 255u - unlike;
    const u32 rank = oc[AZ_O_RANK];
    const u32 ps = tr[AZ_T_PREF_SIGNAL], pz = tr[AZ_T_PREF_SIZE], pl = tr[AZ_T_PREF_SIMILAR], pk = tr[AZ_T_PREF_RANK];
    const u32 wsum = ps + pz + pl + pk;
    const u64 q64 = wsum ? ((u64)ps * show + (u64)pz * size + (u64)pl * like + (u64)pk * rank) / wsum : 0u;
    const u32 q = q64 > 0x3FFFFFFFull ? 0x3FFFFFFFu : (u32)q64;

    if (w->bond[r] && w->partner[r] == ic->uid) {
        const u64 g = q + 1u + (((u64)tc[AZ_T_GUARD] * wp(w, AZ_WP_MATE_GUARD_GAIN)) >> 4);
        return g > 0x7FFFFFFFull ? 0x7FFFFFFF : (i32)g;
    }
    if (orr[AZ_O_DISP] & AZ_OD_BONDED) {
        if ((az_rand(w, ir->uid ^ az_mix64(ic->uid), AZ_RS_MATE, 2) & 255u) < tr[AZ_T_FIDELITY]) return -1;
    }
    if (wsum && q < (u32)(((u64)tr[AZ_T_CHOOSY] * wp(w, AZ_WP_MATE_CHOOSY_SCALE)) >> 8)) return -1;
    return (i32)q;
}

void az_mate_resolve(AzWorld *w, u32 slot, u32 partner, u32 act, bool initiator, u32 worker)
{
    if (act != AZ_ACT_MATE) return;
    const u32 b = bearer(w, slot, partner);
    if (b == AZ_NONE) return;
    const u8 *t = &w->trait[(size_t)slot * AZ_TRAITS];
    const u64 mate_uid = w->uid[partner];
    const u32 mode = w->phen[(size_t)slot * AZ_PHEN + AZ_P_MATE] & 7u;
    u32 young = 0;
    if (b == slot) {
        young = az_life_brood(w, slot, partner, worker);
    } else {
        /* the siring side pays for its display, as heat */
        const u32 show = w->offer[(size_t)slot * AZ_OFFER + AZ_O_ORNAMENT];
        const i32 have = w->energy[slot] > 0 ? w->energy[slot] : 0;
        const u64 ask = ((u64)show * wp(w, AZ_WP_MATE_COURT_EU)) >> 8;
        const i32 pay = ask > (u64)(u32)have ? have : (i32)ask;
        if (pay > 0) { w->energy[slot] -= pay; az_cell_post_heat(w, worker, w->cell[slot], pay); }
    }

    bool formed = false, broken = false;
    const bool binds = mode == AZ_MM_HERMA || mode == AZ_MM_MONOG || mode == AZ_MM_EUSOC ||
                       (mode == AZ_MM_POLYGYN && b == slot);
    if (binds) {
        const u64 step = 1u + (((u64)t[AZ_T_BOND] * wp(w, AZ_WP_MATE_BOND_STEP)) >> 8);
        if (w->bond[slot] && w->partner[slot] == mate_uid) {
            const u64 s2 = w->bond[slot] + step;
            w->bond[slot] = (u8)(s2 > 255u ? 255u : s2);
        } else if ((az_rand(w, w->uid[slot], AZ_RS_MATE, 3) & 255u) < t[AZ_T_BOND]) {
            broken = w->bond[slot] != 0;                 /* the old bond gives way to the new one */
            w->partner[slot] = mate_uid;
            w->bond[slot] = (u8)(step > 255u ? 255u : step);
            formed = true;
        } else if (w->bond[slot]) {
            w->bond[slot] = (u8)(w->bond[slot] / 2u);    /* a mating outside the bond wears it */
            if (!w->bond[slot]) { w->partner[slot] = 0; broken = true; }
        }
    }
#ifdef AZ_PARTNER_OBSERVER
    azt_partner_resolved(w, slot, partner);
#endif
    az_stat_bump(w, slot, AZ_ST_MATES, 1);
    if (formed) az_stat_bump(w, slot, AZ_ST_PARTNERS, 1);
    if (b == slot && young)
        az_emit(w, worker, AZ_EV_MATE, formed ? 1u : 0u, w->uid[slot], mate_uid, (i64)young);
    else if (initiator && (formed || broken))
        az_emit(w, worker, AZ_EV_BOND, formed ? 1u : 0u, w->uid[slot], mate_uid, 0);
}
