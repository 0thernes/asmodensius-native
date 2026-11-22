/* az_social.c - what two creatures do to and for each other, and what one
 * shows the others: attack, steal, share, groom, teach, recruit; signals,
 * marks, rank and the memory of who did what.
 *
 * Nothing here is a payoff matrix. A pair moves energy, matter, toxin, health
 * and rank that the two already hold, exactly; what a creature risks and what
 * it gives are its own genes (STAKE, ALTRUISM, CARE, ...) and its own brain
 * (the drives AGON, AFFIL, STAKE). The books balance whoever wins.
 *
 * INTENT (phase 1, every creature; HOT state and the percept). The offer is
 * zeroed, then: STRENGTH (mass in units of SOCIAL_STRENGTH_MU, scaled by
 * health), SIZE, WEALTH (log of energy and inventory), RANK (128 = neutral),
 * the commitments ENERGY and MATTER (SOCIAL_STAKE_SHARE of its energy and of
 * its free pool: the most this tick can cost it), the RETALIATE bit (a draw
 * against its AGGR and arousal) and the SUBMIT bit (a draw against how low
 * its rank and health are). An active initiator whose brain drives AGON
 * (weighed by AGGR) or AFFIL (weighed by SOCIAL) then proposes one act to a
 * responder; only on that branch is the trait row read:
 *   agonistic   toward its nearest prey, or with a draw against BOLDNESS its
 *               nearest threat; STEAL with a draw against its STEAL trait,
 *               else ATTACK; more urgent by TERRITORIAL on its own colony's
 *               mark against one who is not of the colony
 *   affiliative toward its child (CARE; not under AZ_LESION_CARE), else its
 *               bonded partner, else its nearest kin: SHARE, GROOM, TEACH or
 *               RECRUIT by a draw weighted with ALTRUISM (CARE for a child),
 *               SOCIAL, ALTRUISM / 2 and, for a colony member facing a
 *               solitary one, 32 + TERRITORIAL / 4 (never under
 *               AZ_LESION_COLONY)
 *
 * SCORE (phase 2, pure). ATTACK and STEAL: the strongest claimant has the
 * target. The others need consent: refused when the responder remembers harm
 * from the claimant's tag (harm x RECIPROCITY / 256 >= SOCIAL_GRUDGE); TEACH
 * by a draw against the learner's IMITATE, RECRUIT by a draw against its
 * SOCIAL (kin are heard more readily) and only by a solitary one; kin, a
 * good memory and rank raise the score.
 *
 * RESOLVE (phase 3; each side changes only itself; both compute the same
 * outcome from the two offers, the two trait rows, the frozen drives, the
 * uids and one shared draw):
 *   ATTACK   The stake level of a side is its STAKE trait plus its brain's
 *            STAKE drive (0..255); its stake is that share of its
 *            commitments. The defender may notice in time (SENSE_ACUITY
 *            against the attacker's SAT) and nothing happens; or, with the
 *            SUBMIT bit and a draw against SUBMISSION, yield
 *            SOCIAL_YIELD_SHARE of its stake without a fight. Otherwise the
 *            attacker wins with probability Fa / (Fa + Fd),
 *              F = (1 + STRENGTH) x (256 + WEAPON + SHAPE_N / 2 [+ ARMOR for
 *                  the defender]) x (64 + stake level):
 *            staking more wins more often and loses more (a hawk-dove game
 *            with a continuous strategy: Maynard Smith and Price 1973). The
 *            loser loses its stake and SOCIAL_WOUND x (64 + the winner's
 *            WEAPON) / (64 + its ARMOR) of health; the winner takes the
 *            energy and, of the matter, the share its PREDATION can use (the
 *            rest falls to its cell). Each side pays for the fight by its own
 *            stake level. The defender's SPIKES wound the attacker either
 *            way; a defender with the RETALIATE bit wounds it by PUNISH and
 *            pays for that. Rank: the winner gains by DOMINANCE, the loser
 *            drops and lays an ALARM mark. A creature brought to no health
 *            asks for its death (KILLED).
 *   STEAL    The thief takes (STEAL + 1) / 256 of what the victim's offer
 *            holds for trade (GIVE_RES, GIVE_QTY: az_econ.c writes them),
 *            unless the victim notices (as above); a victim that notices and
 *            retaliates wounds the thief by PUNISH.
 *   SHARE    The giver gives (ALTRUISM + 1) / 256 of its commitments, energy
 *            and matter, to a child of its own by the larger of ALTRUISM and
 *            CARE (Hamilton 1964; not under AZ_LESION_CARE).
 *   GROOM    The groomed one drops SOCIAL_GROOM_SHARE of its toxin into its
 *            cell; the groomer pays SOCIAL_GROOM_EU.
 *   TEACH    The learner moves the gain and bias of its own brain toward the
 *            teacher's by its own IMITATE.
 *   RECRUIT  The recruit takes the recruiter's colony.
 * Both sides then remember the other's tag (rep_tag, rep_val: help raises,
 * harm lowers by SOCIAL_MEMORY_STEP; reciprocity and image scoring: Trivers
 * 1971, Nowak and Sigmund 1998), set or clear AZ_F_COOP and count the act;
 * the initiator emits the event.
 *
 * RANGE (phase 3, one tick in four per creature, behind az_phase_due):
 * signals A and B from the brain's drives by SIG_A_GAIN and SIG_B_GAIN, tied
 * to true condition (health) by SIG_HONESTY, plus the passive GLOW on A, paid
 * for as heat (a signal that cannot be paid for is not sent; honest
 * signalling by cost: Zahavi 1975, Grafen 1990); aimed by SIG_AIM and the
 * brain's SIG_AIM drive it costs down to half (the scan does not yet narrow
 * what others receive: see the limits in PROGRESS.md). Under
 * AZ_LESION_SIGNALS the column stays 0 and nothing is paid. A colony member
 * lays a TERRITORY mark by TERRITORIAL. Remembered harm fades by FORGIVE.
 *
 * AZ_S_PAYOFF: the ABI has no column for it and the sense stub reports 64.
 * The outcome reaches the creature through its energy (Tier 1's reward), its
 * health, its rank and its memory of the other's tag.
 */
#include "az_abi.h"
#ifdef AZ_PREDATION_OBSERVER
#include "az_intake_hooks.h"
#endif
#ifdef AZ_HERMIT_OBSERVER
#include "az_hermit_hooks.h"
#endif

AZ_INLINE u32 wp(const AzWorld *w, u32 id) { return w->param[id]; }
AZ_INLINE const u8 *trow(const AzWorld *w, u32 s) { return &w->trait[(size_t)s * AZ_TRAITS]; }
AZ_INLINE const u8 *orow(const AzWorld *w, u32 s) { return &w->offer[(size_t)s * AZ_OFFER]; }

/* ---- what a body can take ------------------------------------------------ */
static u32 hold_mu(const AzWorld *w, u32 slot)
{
    u32 np;
    (void)az_parts(w, slot, &np);
    const u64 h = (u64)wp(w, AZ_WP_PART_HOLD) * np;
    return h > wp(w, AZ_WP_CELL_HOLD) ? wp(w, AZ_WP_CELL_HOLD) : (u32)h;
}

static void post_heat(AzWorld *w, u32 worker, u32 cell, u64 q)
{
    while (q > 0x40000000ull) { az_cell_post_heat(w, worker, cell, 0x40000000); q -= 0x40000000ull; }
    if (q) az_cell_post_heat(w, worker, cell, (i32)q);
}

/* q eu into my store; what does not fit is heat in my cell */
static void take_energy(AzWorld *w, u32 slot, u32 worker, u64 q)
{
    const i64 cap = (i64)hold_mu(w, slot) * AZ_EPS_BIO > 0x3FFFFFFFll ? 0x3FFFFFFFll : (i64)hold_mu(w, slot) * AZ_EPS_BIO;
    const i64 e = w->energy[slot];
    const i64 room = e < cap ? cap - e : 0;
    const i64 add = (i64)q < room ? (i64)q : room;
    w->energy[slot] = (i32)(e + add);
    post_heat(w, worker, w->cell[slot], q - (u64)add);
}

/* q mu of biomass comes my way: `usable` of it into my free pool as far as there is room,
 * the rest stays in my cell as organic matter (16 eu per mu either way) */
static u32 take_matter(AzWorld *w, u32 slot, u32 worker, u32 q, u32 usable)
{
    const u64 carried = (u64)w->pool_a[slot] + w->pool_m[slot] + w->pool_t[slot] + w->waste[slot] + w->toxin[slot];
    const u32 hold = hold_mu(w, slot);
    const u32 room = carried < hold ? (u32)(hold - carried) : 0u;
    const u32 add = az_min_u32(az_min_u32(usable, q), room);
    w->pool_a[slot] += add;
    if (q > add) az_cell_post(w, worker, w->cell[slot], AZ_SP_ORG_Y, (i32)(q - add));
    return add;
}

/* pay up to q eu as heat; returns what was paid */
static u32 pay_heat(AzWorld *w, u32 slot, u32 worker, u64 q)
{
    const i32 e = w->energy[slot];
    const u32 pay = e > 0 ? (q < (u64)e ? (u32)q : (u32)e) : 0u;
    if (pay) { w->energy[slot] = e - (i32)pay; az_cell_post_heat(w, worker, w->cell[slot], (i32)pay); }
    return pay;
}

static void wound(AzWorld *w, u32 slot, u64 q)
{
    const u32 h = w->health[slot];
    w->health[slot] = (u16)(q >= h ? 0u : h - (u32)q);
    if (w->health[slot] == 0) az_request_death(w, slot, AZ_DIE_KILLED);
}

static void rank_add(AzWorld *w, u32 slot, i32 d)
{
    w->rank[slot] = az_sat_i16((i32)w->rank[slot] + d);
}

/* ---- the memory of others ------------------------------------------------ */
static i32 recall(const AzWorld *w, u32 slot, u16 tag)
{
    const u16 *t = &w->rep_tag[(size_t)slot * 4];
    const i8 *v = &w->rep_val[(size_t)slot * 4];
    for (u32 k = 0; k < 4u; k++)
        if (t[k] == tag && v[k]) return v[k];
    return 0;
}

static void remember(AzWorld *w, u32 slot, u16 tag, i32 delta)
{
    u16 *t = &w->rep_tag[(size_t)slot * 4];
    i8 *v = &w->rep_val[(size_t)slot * 4];
    u32 k = 0;
    while (k < 4u && !(t[k] == tag && v[k])) k++;
    if (k == 4u) {                                        /* the faintest memory makes room */
        k = 0;
        for (u32 i = 1; i < 4u; i++)
            if (az_abs_i32(v[i]) < az_abs_i32(v[k])) k = i;
        t[k] = tag; v[k] = 0;
    }
    v[k] = (i8)az_clamp_i32((i32)v[k] + delta, -127, 127);
}

static bool is_kin(const AzPubId *a, const AzPubId *b)
{
    return (a->parent_a && (a->parent_a == b->parent_a || a->parent_a == b->parent_b || a->parent_a == b->uid)) ||
           (a->parent_b && (a->parent_b == b->parent_a || a->parent_b == b->parent_b || a->parent_b == b->uid)) ||
           b->parent_a == a->uid || b->parent_b == a->uid;
}
static bool is_child_of(const AzPubId *child, const AzPubId *parent)
{
    return child->parent_a == parent->uid || child->parent_b == parent->uid;
}

AZ_INLINE u32 stake_level(const AzWorld *w, u32 s, u32 innate)
{
    return (u32)az_clamp_i32((i32)innate + w->motor[(size_t)s * AZ_MOT + AZ_M_STAKE], 0, 255);
}

/* ---- phase 1 ------------------------------------------------------------- */
void az_social_intent(AzWorld *w, u32 slot, const AzPercept *p, u32 worker)
{
    (void)worker;
    u8 *o = &w->offer[(size_t)slot * AZ_OFFER];
    for (u32 i = 0; i < AZ_OFFER; i++) o[i] = 0;
    AzSenseMemory *history = az_sense_memory(w,slot);
    if (history) history->stake = 0;
    const u8 fl = w->flags[slot];
    if (!(fl & AZ_F_ALIVE)) return;
    const u8 *ph = &w->phen[(size_t)slot * AZ_PHEN];
    const u32 health = w->health[slot] >> 8;
    const u32 body = az_min_u32(255u, w->mass[slot] / wp(w, AZ_WP_SOCIAL_STRENGTH_MU));
    o[AZ_O_STRENGTH] = (u8)((body * (1u + health)) >> 8);
    o[AZ_O_SIZE] = (u8)az_radius_code(w->mass[slot]);
    const u32 e = w->energy[slot] > 0 ? (u32)w->energy[slot] : 0u;
    u64 wealth = (u64)e + 1u;
    for (u32 k = 0; k < AZ_NINV; k++) wealth += (u64)w->inv[(size_t)slot * AZ_NINV + k] * AZ_EPS_BIO;
    o[AZ_O_WEALTH] = (u8)az_min_u32(255u, az_log2_q8(wealth > 0xFFFFFFFFull ? 0xFFFFFFFFu : (u32)wealth) >> 5);
    const u32 rank = (u32)(128 + (w->rank[slot] >> 8));
    o[AZ_O_RANK] = (u8)rank;
    if (fl & (AZ_F_EGG | AZ_F_DORMANT)) return;           /* a passive body commits nothing and shows no temper */
    const u32 share = wp(w, AZ_WP_SOCIAL_STAKE_SHARE);
    az_offer_set_u32(o, AZ_O_ENERGY, (u32)(((u64)e * share) >> 8));
    az_offer_set_u32(o, AZ_O_MATTER, (u32)(((u64)w->pool_a[slot] * share) >> 8));
    if (history) {
        const u64 fraction = stake_level(w,slot,history->innate_stake)+1u;
        const u64 eu = ((u64)az_offer_u32(o,AZ_O_ENERGY)*fraction)>>8;
        const u64 mu = ((u64)az_offer_u32(o,AZ_O_MATTER)*fraction)>>8;
        const u64 committed_stake_value = eu+AZ_EPS_BIO*mu;
        history->stake = (u8)(az_log2_q8(committed_stake_value >= UINT32_MAX ? UINT32_MAX : (u32)committed_stake_value+1u)>>6);
    }
    const u32 r = az_rand(w, w->uid[slot], AZ_RS_SOCIAL, 0);
    u8 disp = 0;
    if ((r & 255u) < ((u32)ph[AZ_P_AGGR] + w->arousal[slot]) >> 1) disp |= AZ_OD_RETALIATE;
    if (((r >> 8) & 255u) < ((255u - rank) + (255u - health)) >> 2) disp |= AZ_OD_SUBMIT;
    o[AZ_O_DISP] = disp;

    if (!(fl & AZ_F_INIT)) return;
    const i8 *mo = &w->motor[(size_t)slot * AZ_MOT];
    const u32 agon = mo[AZ_M_AGON] > 0 ? ((u32)mo[AZ_M_AGON] * (ph[AZ_P_AGGR] + 1u)) >> 7 : 0u;
    const u32 affil = mo[AZ_M_AFFIL] > 0 ? ((u32)mo[AZ_M_AFFIL] * (ph[AZ_P_SOCIAL] + 1u)) >> 7 : 0u;
    if (!agon && !affil) return;
    const u8 *tr = trow(w, slot);                        /* the proposing branch: the trait row may be read */
    const u32 lesion = w->cfg.lesion;
    const u32 mine = w->colony[slot];
    u32 target = AZ_NONE, act = AZ_ACT_NONE, urge = 0;
    if (agon >= affil) {
        if (p->near_prey != AZ_NONE) target = p->near_prey;
        else if (p->near_threat != AZ_NONE && ((r >> 16) & 255u) < tr[AZ_T_BOLDNESS]) target = p->near_threat;
        if (target == AZ_NONE) return;
        act = ((r >> 24) & 255u) < tr[AZ_T_STEAL] ? AZ_ACT_STEAL : AZ_ACT_ATTACK;
        urge = 1u + (agon >> 1);
        const u32 c = w->cell[slot];
        if (mine && w->c_mark_id[c] == mine && w->c_mark[(size_t)c * AZ_NMARK + AZ_MK_TERRITORY] &&
            w->pub[w->spos[target]].colony != mine)
            urge += tr[AZ_T_TERRITORIAL] >> 2;               /* an intruder on its own ground */
    } else {
        bool child = false;
        if (p->child != AZ_NONE && tr[AZ_T_CARE] && !(lesion & AZ_LESION_CARE)) { target = p->child; child = true; }
        else if (p->bond != AZ_NONE) target = p->bond;
        else if (p->near_kin != AZ_NONE) target = p->near_kin;
        if (target == AZ_NONE) return;
        const u32 w_share = child ? (u32)az_max_i32(tr[AZ_T_ALTRUISM], tr[AZ_T_CARE]) : tr[AZ_T_ALTRUISM];
        const u32 w_groom = ph[AZ_P_SOCIAL];
        const u32 w_teach = tr[AZ_T_ALTRUISM] >> 1;
        const u32 w_recruit = (mine && !(lesion & AZ_LESION_COLONY) && w->pub[w->spos[target]].colony == 0)
                              ? 32u + (tr[AZ_T_TERRITORIAL] >> 2) : 0u;
        const u32 total = w_share + w_groom + w_teach + w_recruit;
        if (!total) return;
        u32 pick = az_rand(w, w->uid[slot], AZ_RS_SOCIAL, 1) % total;
        if (pick < w_share) act = AZ_ACT_SHARE;
        else if ((pick -= w_share) < w_groom) act = AZ_ACT_GROOM;
        else if ((pick -= w_groom) < w_teach) act = AZ_ACT_TEACH;
        else act = AZ_ACT_RECRUIT;
        urge = 1u + (affil >> 1);
    }
    if (az_is_initiator(w, w->pub_id[w->spos[target]].uid)) return;    /* only a responder can be asked */
    if (urge > 255u) urge = 255u;
    AzPair *pr = &w->pair[slot];
    if (urge > pr->urge) { pr->act = (u8)act; pr->peer = target; pr->urge = (u8)urge; }
}

/* ---- phase 2 ------------------------------------------------------------- */
i32 az_social_score(const AzWorld *w, u32 r, u32 c, u32 act)
{
    const u8 *oc = orow(w, c);
    if (act == AZ_ACT_ATTACK || act == AZ_ACT_STEAL) return 1 + (i32)oc[AZ_O_STRENGTH];   /* priority, not consent */
    if (act != AZ_ACT_SHARE && act != AZ_ACT_GROOM && act != AZ_ACT_TEACH && act != AZ_ACT_RECRUIT) return -1;
    const u8 *tr = trow(w, r);
    const AzPub *pc = &w->pub[w->spos[c]], *pr = &w->pub[w->spos[r]];
    const i32 mem = recall(w, r, pc->tag);
    if (mem < 0 && (((u32)(-mem) * tr[AZ_T_RECIPROCITY]) >> 8) >= wp(w, AZ_WP_SOCIAL_GRUDGE)) return -1;
    const bool kin = is_kin(&w->pub_id[w->spos[c]], &w->pub_id[w->spos[r]]);
    const u32 d = az_rand(w, w->uid[r], AZ_RS_SOCIAL, 2u + (w->uid[c] << 2)) & 255u;
    if (act == AZ_ACT_TEACH && d >= (u32)tr[AZ_T_IMITATE] + (kin ? 64u : 0u)) return -1;
    if (act == AZ_ACT_RECRUIT) {
        if ((w->cfg.lesion & AZ_LESION_COLONY) || pr->colony != 0 || pc->colony == 0) return -1;
        if (d >= (u32)w->phen[(size_t)r * AZ_PHEN + AZ_P_SOCIAL] + (kin ? 64u : 0u)) {
#ifdef AZ_HERMIT_OBSERVER
            return AZ_RECRUIT_UNWILLING;
#else
            return -1;
#endif
        }
    }
    return 1 + (kin ? 128 : 0) + (mem > 0 ? mem : 0) + (i32)(oc[AZ_O_RANK] >> 2);
}

/* ---- phase 3: the pair ----------------------------------------------------- */
/* chance of 256 that `victim` notices `actor` in time */
AZ_INLINE u32 notice(const AzWorld *w, u32 victim, u32 actor)
{
    return (u32)(((u64)trow(w, victim)[AZ_T_SENSE_ACUITY] * (64u + trow(w, actor)[AZ_T_SAT]) * wp(w, AZ_WP_SOCIAL_EVADE)) >> 16);
}

/* one contest, seen from `slot`; a = the attacker, d = the defender */
static AZ_NOINLINE void contest(AzWorld *w, u32 slot, u32 partner, bool initiator, u32 worker)
{
    const u32 a = initiator ? slot : partner, d = initiator ? partner : slot;
    const u8 *oa = orow(w, a), *od = orow(w, d), *ta = trow(w, a), *td = trow(w, d);
    const u64 ua = w->uid[a], ud = w->uid[d];
    u32 r[4];
    az_rand4(w, ua < ud ? ua : ud, AZ_RS_CONTEST, ua < ud ? ud : ua, r);
    const u32 step = wp(w, AZ_WP_SOCIAL_MEMORY_STEP);
    w->flags[slot] &= (u8)~AZ_F_COOP;
    if (initiator) az_stat_bump(w, slot, AZ_ST_ATTACKS, 1);

    if ((r[0] & 255u) < notice(w, d, a)) {               /* seen coming: nothing happens but the effort */
        if (initiator) {
            (void)pay_heat(w, slot, worker, wp(w, AZ_WP_SOCIAL_FIGHT_EU));
            az_emit(w, worker, AZ_EV_ATTACK, 0, ua, ud, 0);
        } else remember(w, slot, w->tag[partner], -(i32)(step >> 1));
        return;
    }
    const u32 la = stake_level(w, a,ta[AZ_T_STAKE]), ld = stake_level(w, d,td[AZ_T_STAKE]);
    const bool yields = (od[AZ_O_DISP] & AZ_OD_SUBMIT) && ((r[0] >> 8) & 255u) < td[AZ_T_SUBMISSION];
    bool a_wins = true;
    if (!yields) {
        const u64 fa = (u64)(1u + oa[AZ_O_STRENGTH]) * (256u + ta[AZ_T_WEAPON] + (ta[AZ_T_SHAPE_N] >> 1)) * (64u + la);
        const u64 fd = (u64)(1u + od[AZ_O_STRENGTH]) * (256u + td[AZ_T_WEAPON] + (td[AZ_T_SHAPE_N] >> 1) + td[AZ_T_ARMOR]) * (64u + ld);
        a_wins = (u64)r[1] * (fa + fd) < (fa << 32);
    }
    const u32 loser = a_wins ? d : a, winner = a_wins ? a : d;
    const u8 *ol = orow(w, loser);
    u64 qe = ((u64)az_offer_u32(ol, AZ_O_ENERGY) * ((a_wins ? ld : la) + 1u)) >> 8;
    u64 qm = ((u64)az_offer_u32(ol, AZ_O_MATTER) * ((a_wins ? ld : la) + 1u)) >> 8;
    if (yields) {
        qe = (qe * wp(w, AZ_WP_SOCIAL_YIELD_SHARE)) >> 8;
        qm = (qm * wp(w, AZ_WP_SOCIAL_YIELD_SHARE)) >> 8;
    }
    const bool i_win = slot == winner;
    if (i_win) {
        take_energy(w, slot, worker, qe);
        const u32 assimilated = take_matter(w, slot, worker, (u32)qm, (u32)((qm * (trow(w, slot)[AZ_T_PREDATION] + 1u)) >> 8));
#ifdef AZ_PREDATION_OBSERVER
        if (initiator && assimilated) azt_predation_settled(w, slot, assimilated);
#else
        (void)assimilated;
#endif
        rank_add(w, slot, (i32)((wp(w, AZ_WP_SOCIAL_RANK_STEP) * (64u + trow(w, slot)[AZ_T_DOMINANCE])) >> 7));
        az_stat_bump(w, slot, AZ_ST_WINS, 1);
    } else {
        w->energy[slot] -= (i32)qe;
        w->pool_a[slot] -= (u32)qm;
        rank_add(w, slot, -(i32)wp(w, AZ_WP_SOCIAL_RANK_STEP));
        az_stat_bump(w, slot, AZ_ST_LOSSES, 1);
        if (wp(w, AZ_WP_SOCIAL_ALARM)) az_cell_mark(w, worker, w->cell[slot], AZ_MK_ALARM, wp(w, AZ_WP_SOCIAL_ALARM), 0);
    }
    u64 hurt = 0;
    if (!yields) {
        (void)pay_heat(w, slot, worker, ((u64)wp(w, AZ_WP_SOCIAL_FIGHT_EU) * ((slot == a ? la : ld) + 1u)) >> 6);
        if (!i_win)
            hurt += ((u64)wp(w, AZ_WP_SOCIAL_WOUND) * (64u + trow(w, winner)[AZ_T_WEAPON])) / (64u + trow(w, slot)[AZ_T_ARMOR]);
        if (slot == a) {
            hurt += ((u64)td[AZ_T_SPIKES] * wp(w, AZ_WP_SOCIAL_SPIKE_WOUND)) >> 8;
            if (od[AZ_O_DISP] & AZ_OD_RETALIATE) hurt += ((u64)td[AZ_T_PUNISH] * wp(w, AZ_WP_SOCIAL_PUNISH_WOUND)) >> 8;
        } else if (od[AZ_O_DISP] & AZ_OD_RETALIATE) {
            (void)pay_heat(w, slot, worker, ((u64)td[AZ_T_PUNISH] * wp(w, AZ_WP_SOCIAL_PUNISH_EU)) >> 8);
        }
    }
    if (hurt) wound(w, slot, hurt);
    if (slot == d) remember(w, slot, w->tag[partner], -(i32)step);
    else if (!yields && (od[AZ_O_DISP] & AZ_OD_RETALIATE)) remember(w, slot, w->tag[partner], -(i32)(step >> 1));
    if (initiator) az_emit(w, worker, AZ_EV_ATTACK, (u32)a_wins, ua, ud, (i32)(qm > 0x7FFFFFFFull ? 0x7FFFFFFF : (i32)qm));
}

/* a theft, seen from `slot`; t = the thief (the initiator), v = the victim */
static AZ_NOINLINE void theft(AzWorld *w, u32 slot, u32 partner, bool initiator, u32 worker)
{
    const u32 t = initiator ? slot : partner, v = initiator ? partner : slot;
    const u8 *ov = orow(w, v), *tt = trow(w, t), *tv = trow(w, v);
    const u64 ut = w->uid[t], uv = w->uid[v];
    u32 r[4];
    az_rand4(w, ut < uv ? ut : uv, AZ_RS_CONTEST, ut < uv ? uv : ut, r);
    const u32 step = wp(w, AZ_WP_SOCIAL_MEMORY_STEP);
    w->flags[slot] &= (u8)~AZ_F_COOP;
    const bool caught = (r[0] & 255u) < notice(w, v, t);
    const u32 res = ov[AZ_O_GIVE_RES] % AZ_NINV;
    const u32 q = caught ? 0u : (u32)(((u64)az_offer_u32(ov, AZ_O_GIVE_QTY) * (tt[AZ_T_STEAL] + 1u)) >> 8);
    if (slot == t) {
        if (q) {
            u32 *mine = &w->inv[(size_t)slot * AZ_NINV + res];
            const u64 room = *mine < wp(w, AZ_WP_CELL_HOLD) ? wp(w, AZ_WP_CELL_HOLD) - *mine : 0u;
            const u32 add = q < room ? q : (u32)room;
            *mine += add;                                    /* what it cannot carry falls to its cell */
            if (q > add) az_cell_post(w, worker, w->cell[slot], az_inv_species[res], (i32)(q - add));
            az_stat_bump(w, slot, AZ_ST_STOLEN, q);
        }
        if (caught && (ov[AZ_O_DISP] & AZ_OD_RETALIATE)) {
            wound(w, slot, ((u64)tv[AZ_T_PUNISH] * wp(w, AZ_WP_SOCIAL_PUNISH_WOUND)) >> 8);
            remember(w, slot, w->tag[partner], -(i32)(step >> 1));
        }
        az_emit(w, worker, AZ_EV_STEAL, (u32)caught, ut, uv, (i32)q);
    } else {
        if (q) w->inv[(size_t)slot * AZ_NINV + res] -= q;
        if (caught && (ov[AZ_O_DISP] & AZ_OD_RETALIATE))
            (void)pay_heat(w, slot, worker, ((u64)tv[AZ_T_PUNISH] * wp(w, AZ_WP_SOCIAL_PUNISH_EU)) >> 8);
        remember(w, slot, w->tag[partner], -(i32)step);
    }
}

void az_social_resolve(AzWorld *w, u32 slot, u32 partner, u32 act, bool initiator, u32 worker)
{
    if (act == AZ_ACT_ATTACK) { contest(w, slot, partner, initiator, worker); return; }
    if (act == AZ_ACT_STEAL) { theft(w, slot, partner, initiator, worker); return; }
    if (act != AZ_ACT_SHARE && act != AZ_ACT_GROOM && act != AZ_ACT_TEACH && act != AZ_ACT_RECRUIT) return;
    const u32 giver = initiator ? slot : partner, taker = initiator ? partner : slot;
    const u32 step = wp(w, AZ_WP_SOCIAL_MEMORY_STEP);
    const u64 ug = w->uid[giver], ut = w->uid[taker];
    w->flags[slot] |= AZ_F_COOP;
    remember(w, slot, w->tag[partner], initiator ? (i32)(step >> 2) : (i32)step);

    if (act == AZ_ACT_SHARE) {
        const u8 *og = orow(w, giver), *tg = trow(w, giver);
        u32 g = tg[AZ_T_ALTRUISM];
        if (!(w->cfg.lesion & AZ_LESION_CARE) && tg[AZ_T_CARE] > g &&
            is_child_of(&w->pub_id[w->spos[taker]], &w->pub_id[w->spos[giver]])) g = tg[AZ_T_CARE];
        const u64 qe = ((u64)az_offer_u32(og, AZ_O_ENERGY) * (g + 1u)) >> 8;
        const u64 qm = ((u64)az_offer_u32(og, AZ_O_MATTER) * (g + 1u)) >> 8;
        if (initiator) {
            w->energy[slot] -= (i32)qe;
            w->pool_a[slot] -= (u32)qm;
            az_stat_bump(w, slot, AZ_ST_SHARES, 1);
            az_emit(w, worker, AZ_EV_SHARE, 0, ug, ut, (i32)(qe > 0x7FFFFFFFull ? 0x7FFFFFFF : (i32)qe));
        } else {
            take_energy(w, slot, worker, qe);
            (void)take_matter(w, slot, worker, (u32)qm, (u32)qm);
            az_stat_bump(w, slot, AZ_ST_RECEIVED, 1);
        }
    } else if (act == AZ_ACT_GROOM) {
        if (initiator) {
            (void)pay_heat(w, slot, worker, wp(w, AZ_WP_SOCIAL_GROOM_EU));
            az_stat_bump(w, slot, AZ_ST_SHARES, 1);
            az_emit(w, worker, AZ_EV_SHARE, 1, ug, ut, 0);
        } else if (w->toxin[slot]) {
            const u32 t = az_min_u32(w->toxin[slot], 1u + (u32)(((u64)w->toxin[slot] * wp(w, AZ_WP_SOCIAL_GROOM_SHARE)) >> 8));
            w->toxin[slot] -= t;
            az_cell_post(w, worker, w->cell[slot], AZ_SP_TOX_Z, (i32)t);
        }
    } else if (act == AZ_ACT_TEACH) {
        if (initiator) az_emit(w, worker, AZ_EV_TEACH, 0, ug, ut, 0);
        else {
            u32 nb, nt;
            AzBrain *mine = az_brain(w, slot, &nb);
            const AzBrain *theirs = az_brain(w, partner, &nt);
            const i32 im = trow(w, slot)[AZ_T_IMITATE];
            if (nb >= sizeof(AzBrain) && nt >= sizeof(AzBrain) && im)
                for (u32 l = 0; l < AZ_MODG; l++) {
                    mine->gain[l] = (u8)az_clamp_i32((i32)mine->gain[l] + ((((i32)theirs->gain[l] - mine->gain[l]) * im) / 256), 0, 255);
                    mine->bias[l] = (i8)az_clamp_i32((i32)mine->bias[l] + ((((i32)theirs->bias[l] - mine->bias[l]) * im) / 256), -127, 127);
                }
        }
    } else {                                              /* AZ_ACT_RECRUIT */
        const u32 col = w->pub[w->spos[giver]].colony;
        if (initiator) az_emit(w, worker, AZ_EV_RECRUIT, col, ug, ut, 0);
        else if (col && !(w->cfg.lesion & AZ_LESION_COLONY)) w->colony[slot] = col;
    }
}

/* ---- phase 3: what is not a pair ------------------------------------------ */
void az_social_range(AzWorld *w, u32 begin, u32 end, u32 worker)
{
    const bool silent = (w->cfg.lesion & AZ_LESION_SIGNALS) != 0;
    for (u32 s = begin; s < end; s++) {
        const u8 fl = w->flags[s];
        if (!(fl & AZ_F_ALIVE)) continue;
        if (!az_phase_due(w, w->uid[s], AZ_RS_SOCIAL, 2)) continue;
        u8 *sig = &w->sig[(size_t)s * 2];
        if (silent || (fl & (AZ_F_EGG | AZ_F_DORMANT))) { sig[0] = 0; sig[1] = 0; continue; }
        const u8 *tr = trow(w, s);
        const i8 *mo = &w->motor[(size_t)s * AZ_MOT];
        /* a signal is the drive times the gain; the honest part of it scales with true condition */
        const u32 cond = w->health[s] >> 8;
        const u32 tie = (255u - tr[AZ_T_SIG_HONESTY]) * 256u + (u32)tr[AZ_T_SIG_HONESTY] * cond;      /* of 65280 */
        const u32 da = mo[AZ_M_SIG_A] > 0 ? (u32)mo[AZ_M_SIG_A] * 2u : 0u, db = mo[AZ_M_SIG_B] > 0 ? (u32)mo[AZ_M_SIG_B] * 2u : 0u;
        u32 a = (u32)((((u64)da * tr[AZ_T_SIG_A_GAIN]) >> 8) * tie >> 16) + (tr[AZ_T_GLOW] >> 2);
        u32 b = (u32)((((u64)db * tr[AZ_T_SIG_B_GAIN]) >> 8) * tie >> 16);
        if (a > 255u) a = 255u;
        if (b > 255u) b = 255u;
        if (a | b) {
            const u32 aim = ((u32)az_abs_i32(mo[AZ_M_SIG_AIM]) * 2u * tr[AZ_T_SIG_AIM]) >> 8;          /* 0..253 */
            const u64 cost = ((u64)(a + b) * wp(w, AZ_WP_SOCIAL_SIGNAL_EU) * (512u - aim)) >> 17;
            if (w->energy[s] > 0 && (u64)w->energy[s] >= cost) {
                (void)pay_heat(w, s, worker, cost);
                az_stat_bump(w, s, AZ_ST_SIGNALS, 1);
            } else { a = 0; b = 0; }                         /* a signal that cannot be paid for is not sent */
        }
        sig[0] = (u8)a; sig[1] = (u8)b;
        /* territory */
        if (w->colony[s] && tr[AZ_T_TERRITORIAL]) {
            const u32 m = ((u32)tr[AZ_T_TERRITORIAL] * wp(w, AZ_WP_SOCIAL_MARK)) >> 8;
            if (m) az_cell_mark(w, worker, w->cell[s], AZ_MK_TERRITORY, m, w->colony[s]);
        }
        /* old harm fades */
        const i32 fade = (i32)(((u32)tr[AZ_T_FORGIVE] * wp(w, AZ_WP_SOCIAL_FORGIVE_RATE)) >> 8);
        if (fade) {
            i8 *v = &w->rep_val[(size_t)s * 4];
            for (u32 k = 0; k < 4u; k++)
                if (v[k] < 0) v[k] = (i8)az_min_i32(0, (i32)v[k] + fade);
        }
    }
}
