/* az_econ.c - the pantry and the barter market. Parameters: params/az_econ.def.
 *
 * Nobody sets a price here. A creature holds goods, shows what it holds most
 * of and what it lacks, and two creatures that each hold what the other lacks
 * exchange at a rate between the two rates each would still accept.
 *
 * THE PANTRY (az_econ_range, one tick in four per creature: the only place
 * outside a pair where the trait row is read). The inventory holds four
 * resources as matter (az_inv_species: NUT_X, ORG_Y, STR_S, OXI), counted in
 * the matter and energy totals at their chemical energy. Two of them a body
 * makes and uses itself, both exactly:
 *     1 biomass            <-> 1 ORG_Y   (16 eu = 16 eu)
 *     1 biomass + 8 eu     <-> 1 NUT_X   (16 + 8 = 24 eu)
 *   - Setting aside: free pool above its ease (ECON_KEEP of its own body mass)
 *     is stored, (HOARD x ECON_STORE_SHARE) / 65536 of the surplus per pulse,
 *     up to what the body carries ((CARRY_CAP + 1) / 256 of ECON_CARRY of
 *     PART_HOLD per part). The form
 *     is by its valuations: (VAL_0 + 1) of (VAL_0 + VAL_1 + 2) as NUT_X, as
 *     far as it has energy above its store (STORE_CAP, the same store the
 *     metabolism refills) to pack with it; the rest as ORG_Y.
 *   - Opening: with the free pool under half its ease it takes ORG_Y back,
 *     then NUT_X; with energy under ECON_OPEN_SHARE of its store it opens
 *     NUT_X for the energy.
 *   STR_S and OXI can be held, carried and stolen, and offered by one who
 *   holds most of them; nobody asks for them, so they are never traded: no
 *   module gives them to a body yet, and no body has a use for them.
 *
 * THE OFFER (az_econ_intent; HOT state only). A creature with goods is in the
 * market: GIVE_RES the resource it holds most of, GIVE_QTY all of it (exact),
 * WANT_RES the usable resource other than that one it holds least of, and
 * PRICE = (1 + held of WANT) / (1 + held of GIVE) in Q16.16: with a value of
 * log(1 + held) for each good, that is the least it can take per unit given
 * without losing. Everybody else has PRICE = AZ_PRICE_NONE.
 *   An initiator in the market proposes a trade to somebody near (kin,
 *   smaller or larger, by a draw) with urge ECON_URGE / 256 of TRADE_DRIVE x the lopsidedness of
 *   its holdings (in doublings, to 8) / 8. It cannot see the other's offer
 *   before it asks: asking costs it the tick's one pair.
 *
 * THE DEAL (az_econ_score and az_econ_resolve; both sides compute it from the
 * two frozen offers and the two trait rows, so they agree). The claimant c
 * gives A, the responder r gives B.
 *   - Double coincidence of wants (Jevons 1875): c gives what r wants and r
 *     gives what c wants; else no deal.
 *   - Each side's reservation price is its PRICE times (VAL_give + 1) /
 *     (VAL_want + 1): the valuations are heritable and private. With pc the
 *     least B per A that c takes and hi = 1 / pr the most B per A that r
 *     gives, there is a deal only if pc <= hi (Myerson and Satterthwaite
 *     1983: with private values some gainful trades must fail).
 *   - The rate is the geometric middle sqrt(pc x hi), the bilateral barter
 *     rule of Epstein and Axtell 1996 (Sugarscape: the geometric mean of the
 *     two marginal rates of substitution): the split of the difference
 *     (Chatterjee and Samuelson 1983, k = 1/2) taken on the ratio scale, which
 *     is the same whichever good is called the price of the other.
 *   - Quantities are whole units: qa = min(c's lot, r's lot / rate) and
 *     qb = floor(qa x rate), a lot being ECON_LOT_SHARE of what is offered. If
 *     rounding down would give c less than pc there is no deal, so neither
 *     side ever gets less than its own reservation price, in integers.
 *   - What does not fit the receiver's pantry falls to its cell.
 *   The responder's score is the width of the bargain (hi / pc): of several
 *   claimants it takes the one with whom there is most to gain.
 *
 * THE MARKET'S MEMORY (az_econ_global, serial; gstate):
 *   AZ_G_VOLUME + k   units of k given per tick, smoothed (Q8.8)
 *   AZ_G_PRICE + k    what a unit of k has fetched since the world began, in
 *                     Q8.8 eu with ORG_Y at its chemical 16 eu: 16 x (ORG_Y
 *                     given for k) / (k given for ORG_Y); its chemical energy
 *                     until it has been traded both ways
 *   AZ_G_ECON + 8 + k the units of k given up to the last tick (31 bits)
 *   No creature senses these words (the ABI has no sense for a price): they
 *   are a record for whoever looks at the world.
 *
 * Lesion TRADE: nobody is in the market, nothing is proposed or accepted (the
 * tick does not call az_econ_global). The pantry stays.
 *
 * THE COLONY'S POOLS (members only; the colony itself is core/az_colony.c).
 * A member deals with its colony on its pantry tick, after the pantry:
 *   - Asking (phase 1, colony_ask). In want it asks its colony before it
 *     starves: energy, when it is under the share of its own store at which
 *     it opens its pantry (ECON_OPEN_SHARE), for what it lacks; matter, when
 *     its free pool is under half its ease, for what it lacks and its pantry
 *     has room for, ORG_Y first, never more than the store holds. The asking
 *     is bent by the brain: (255 - drive) / 128 of the lack, where the drive
 *     is 128 + its GIVE motor, so a brain that pushes GIVE asks for nothing
 *     and one that pulls asks for twice its lack. The asks are kept in the
 *     scratch column gather[] ([0], [1] units of NUT_X and ORG_Y, [3] energy,
 *     [2] the colony asked), written by every creature on its pantry tick, so
 *     what is taken in phase 3 is what was asked of this colony in this tick.
 *   - Taking (phase 3). It gets the granted fraction of each ask (the colony
 *     module's and the engine's grant: a pool never gives what it does not
 *     hold), matter into its pantry, energy into its own.
 *   - Giving (phase 3, when it took nothing). Of each good in its pantry
 *     STORE_SHARE x drive x ECON_GIVE_SHARE / 2^24 goes to the colony's store,
 *     and of its energy above its own store (STORE_CAP) COMMONS x drive x
 *     ECON_GIVE_SHARE / 2^24 goes to the colony's energy pool. A FORAGER gives
 *     ECON_FORAGER_SHARE / 256 times that: bringing in is what the role is.
 *     Nothing is given to a pool that holds CELL_HOLD.
 *   - Building. A BUILDER lays the NEST mark of its cell, ECON_NEST x (128 +
 *     its BUILD motor) / 256 per pulse: the colony's call for builders falls
 *     where the nest is marked (az_colony.c reads the mark under its centre).
 *   - The voice (every tick). 128 + its COLONY_SIG motor goes to the colony's
 *     channel 0; the members sense the mean of the voices (AZ_S_COLONY).
 *   These are public-goods moves with heritable strategies (STORE_SHARE,
 *   COMMONS) that the brain adjusts in life (the GIVE motor): one that gives
 *   nothing still draws on the pool in want. Nothing here rewards giving.
 *   The TRADE lesion leaves all of it; it ends with the colonies (COLONY).
 * Not here: the TRAIL mark (nothing senses marks yet). Carrying costs nothing
 * yet: the physics module is to weigh what is carried.
 */
#include "az_abi.h"
#ifdef AZ_COMMONS_OBSERVER
#include "az_intake_hooks.h"
#endif
#ifdef AZ_BUILDER_OBSERVER
#include "az_builder_hooks.h"
#endif

#define USABLE     0x3u            /* the resources a body can open: NUT_X (bit 0), ORG_Y (bit 1) */
#define PACK_EU    (az_eps_species[AZ_SP_NUT_X] - AZ_EPS_BIO)   /* 8 eu packed with a unit of NUT_X */
#define PRICE_TOP  0x80000000ull   /* 32768 units for one */

AZ_INLINE u32 wp(const AzWorld *w, u32 id) { return w->param[id]; }
AZ_INLINE const u8 *trow(const AzWorld *w, u32 s) { return &w->trait[(size_t)s * AZ_TRAITS]; }
AZ_INLINE const u8 *orow(const AzWorld *w, u32 s) { return &w->offer[(size_t)s * AZ_OFFER]; }

static u32 hold_mu(const AzWorld *w, u32 slot)
{
    u32 np;
    (void)az_parts(w, slot, &np);
    const u64 h = (u64)wp(w, AZ_WP_PART_HOLD) * np;
    return h > wp(w, AZ_WP_CELL_HOLD) ? wp(w, AZ_WP_CELL_HOLD) : (u32)h;
}
/* what its pantry holds: ECON_CARRY of what the body holds, by its own CARRY_CAP (read on pair paths and pulses only) */
static u32 carry_mu(const AzWorld *w, u32 slot)
{
    return (u32)(((u64)hold_mu(w, slot) * wp(w, AZ_WP_ECON_CARRY) * (trow(w, slot)[AZ_T_CARRY_CAP] + 1u)) >> 16);
}

/* ---- the colony's pools ------------------------------------------------------ */
/* the trait bent by the brain: 128 with the GIVE motor at rest, 1 pulled back, 255 pushed */
AZ_INLINE u32 give_drive(const AzWorld *w, u32 s)
{
    return (u32)az_clamp_i32(128 + (i32)w->motor[(size_t)s * AZ_MOT + AZ_M_GIVE], 0, 255);
}

/* phase 1, on its pantry tick: what a member in want asks of its colony */
static void colony_ask(AzWorld *w, u32 s)
{
    if (!az_phase_due(w, w->uid[s], AZ_RS_ECON, 2)) return;
    u32 *ask = &w->gather[(size_t)s * AZ_NINV];
    ask[0] = 0; ask[1] = 0; ask[2] = 0; ask[3] = 0;        /* [2]: the colony asked; none */
    const u32 col = w->colony[s];
    if (!col || col >= w->n_colonies) return;
    ask[2] = col;
    const u8 *t = trow(w, s);
    const u32 pull = 255u - give_drive(w, s);
    const i64 store = (i64)(t[AZ_T_STORE_CAP] + 1u) * wp(w, AZ_WP_METAB_STORE_EU);
    const i64 low = (store * wp(w, AZ_WP_ECON_OPEN_SHARE)) >> 8;
    if ((i64)w->energy[s] < low) {
        const i64 want = ((low - (i64)w->energy[s]) * (i64)pull) >> 7;
        ask[3] = (u32)az_clamp_i64(want, 0, 0x3FFFFFFF);
        az_colony_ask_energy(w, col, ask[3]);
    }
    const u32 keep = (u32)(((u64)w->mass[s] * wp(w, AZ_WP_ECON_KEEP)) >> 8);
    if (w->pool_a[s] < keep / 2u) {
        const u32 *inv = &w->inv[(size_t)s * AZ_NINV];
        const u64 held = (u64)inv[0] + inv[1] + inv[2] + inv[3];
        const u32 carry = carry_mu(w, s);
        const u64 room = carry > held ? carry - held : 0u;
        u64 want = ((u64)(keep / 2u - w->pool_a[s]) * pull) >> 7;
        if (want > room) want = room;
        for (u32 k = 2; k-- > 0u && want;) {               /* ORG_Y, then NUT_X */
            const u32 have = w->colonies[col].store[k];
            const u32 q = want < have ? (u32)want : have;
            ask[k] = q; want -= q;
            if (q) az_atomic_add_u64(&w->colony_eph[col].dem[k], q);
        }
    }
}

/* phase 3, on its pantry tick, after the pantry: take what was granted, or give; build */
static void colony_traffic(AzWorld *w, u32 s, u32 col, u32 worker)
{
    const u8 *t = trow(w, s);
    u32 *inv = &w->inv[(size_t)s * AZ_NINV];
    const u32 *ask = &w->gather[(size_t)s * AZ_NINV];
    AzColonyEph *e = &w->colony_eph[col];
    const AzColony *k = &w->colonies[col];
    const u32 hold = wp(w, AZ_WP_CELL_HOLD);
    bool took = false;
#ifdef AZ_COMMONS_OBSERVER
    bool retained = false, contributed = false, eligible = false;
#endif
    if (ask[2] == col) {
        const i64 room_e = 0x3FFFFFFFll - (w->energy[s] > 0 ? (i64)w->energy[s] : 0);
        const u32 got_e = az_colony_take_energy(w, col, (u32)az_clamp_i64(ask[3], 0, room_e));
        w->energy[s] += (i32)got_e;
        took = got_e != 0u;
#ifdef AZ_COMMONS_OBSERVER
        retained = took;
#endif
        for (u32 r = 0; r < 2u; r++) {
            const u32 got = (u32)(((u64)ask[r] * e->grant[r]) / 65535u);
            if (!got) continue;
            az_atomic_add_u64(&e->taken[r], got);
            const u64 held = (u64)inv[0] + inv[1] + inv[2] + inv[3];
            const u32 carry = carry_mu(w, s);
            const u32 room = carry > held ? (u32)(carry - held) : 0u;
            const u32 add = got < room ? got : room;
            inv[r] += add;
#ifdef AZ_COMMONS_OBSERVER
            if (add) retained = true;
#endif
            if (got > add) az_cell_post(w, worker, w->cell[s], az_inv_species[r], (i32)(got - add));
            took = true;
        }
    }
    if (!took) {
        u64 rate = (u64)give_drive(w, s) * wp(w, AZ_WP_ECON_GIVE_SHARE);
        if (w->role[s] == AZ_ROLE_FORAGER) rate = (rate * wp(w, AZ_WP_ECON_FORAGER_SHARE)) >> 8;
#ifdef AZ_COMMONS_OBSERVER
        /* Same settlement and same traits, allowing the highest legal GIVE motor. */
        u64 possible_rate = UINT64_C(255) * wp(w, AZ_WP_ECON_GIVE_SHARE);
        if (w->role[s] == AZ_ROLE_FORAGER)
            possible_rate = (possible_rate * wp(w, AZ_WP_ECON_FORAGER_SHARE)) >> 8;
#endif
        for (u32 r = 0; r < AZ_NINV; r++) {
            u64 q = ((u64)inv[r] * t[AZ_T_STORE_SHARE] * rate) >> 24;
            const u64 room = k->store[r] < hold ? hold - k->store[r] : 0u;
#ifdef AZ_COMMONS_OBSERVER
            if (room && ((u64)inv[r] * t[AZ_T_STORE_SHARE] * possible_rate >> 24)) eligible = true;
#endif
            if (q > inv[r]) q = inv[r];
            if (q > room) q = room;
            if (!q) continue;
            inv[r] -= (u32)q;
            az_atomic_add_u64(&e->dep[r], (u32)q);
#ifdef AZ_COMMONS_OBSERVER
            contributed = true;
#endif
        }
        const i64 store = (i64)(t[AZ_T_STORE_CAP] + 1u) * wp(w, AZ_WP_METAB_STORE_EU);
        const i64 above = (i64)w->energy[s] - store;
        if (above > 0 && k->energy < (i64)hold) {
            u64 q = ((u64)above * t[AZ_T_COMMONS] * rate) >> 24;
            if (q > (u64)above) q = (u64)above;
#ifdef AZ_COMMONS_OBSERVER
            if ((u64)above * t[AZ_T_COMMONS] * possible_rate >> 24) eligible = true;
            if (q) contributed = true;
#endif
            if (q) { w->energy[s] -= (i32)q; az_colony_give_energy(w, col, (u32)q); }
        }
    }
#ifdef AZ_COMMONS_OBSERVER
    if (k->alive && !(w->cfg.lesion & AZ_LESION_COLONY) && (retained || contributed || eligible))
        azt_commons_pulse(w, s, k, retained, contributed, eligible);
#endif
    if (w->role[s] == AZ_ROLE_BUILDER) {
        const u32 push = (u32)az_clamp_i32(128 + (i32)w->motor[(size_t)s * AZ_MOT + AZ_M_BUILD], 0, 255);
        const u32 m = (wp(w, AZ_WP_ECON_NEST) * push) >> 8;
        if (m) {
            az_cell_mark(w, worker, w->cell[s], AZ_MK_NEST, m, col);
#ifdef AZ_BUILDER_OBSERVER
            azt_builder_post(w, s, worker, w->cell[s], m);
#endif
        }
    }
}

/* ---- the offer --------------------------------------------------------------- */
void az_econ_intent(AzWorld *w, u32 slot, const AzPercept *p, u32 worker)
{
    (void)worker;
    colony_ask(w, slot);
    u8 *o = &w->offer[(size_t)slot * AZ_OFFER];
    az_offer_set_u32(o, AZ_O_PRICE, AZ_PRICE_NONE);
    const u8 fl = w->flags[slot];
    if ((w->cfg.lesion & AZ_LESION_TRADE) || (fl & (AZ_F_EGG | AZ_F_DORMANT))) return;
    const u32 *inv = &w->inv[(size_t)slot * AZ_NINV];
    u32 g = 0, want = AZ_NINV;
    for (u32 k = 1; k < AZ_NINV; k++) if (inv[k] > inv[g]) g = k;
    if (inv[g] == 0) return;                              /* nothing to give: not in the market */
    for (u32 k = 0; k < AZ_NINV; k++)
        if (k != g && ((USABLE >> k) & 1u) && (want == AZ_NINV || inv[k] < inv[want])) want = k;
    if (want == AZ_NINV) return;
    u64 price = ((u64)(1u + inv[want]) << 16) / (1ull + inv[g]);
    if (price < 1) price = 1;
    o[AZ_O_GIVE_RES] = (u8)g; o[AZ_O_WANT_RES] = (u8)want;
    az_offer_set_u32(o, AZ_O_GIVE_QTY, inv[g]);
    az_offer_set_u32(o, AZ_O_PRICE, (u32)price);          /* at most 65536: it gives what it holds most of */

    if (!(fl & AZ_F_INIT)) return;
    const u32 nb[3] = { p->near_kin, p->near_prey, p->near_threat };
    const u32 r = az_rand(w, w->uid[slot], AZ_RS_ECON, 0);
    u32 target = AZ_NONE;
    for (u32 k = 0; k < 3u && target == AZ_NONE; k++) target = nb[(r + k) % 3u];
    if (target == AZ_NONE || az_is_initiator(w, w->pub_id[w->spos[target]].uid)) return;
    const u32 lop = az_log2_q8(1u + inv[g]) - az_log2_q8(1u + inv[want]);     /* doublings, Q8 */
    const u32 urge = (u32)(((u64)trow(w, slot)[AZ_T_TRADE_DRIVE] * az_min_u32(lop, 2048u) * wp(w, AZ_WP_ECON_URGE)) >> 19);
    AzPair *pr = &w->pair[slot];
    if (urge > pr->urge) { pr->act = AZ_ACT_TRADE; pr->peer = target; pr->urge = (u8)urge; }
}

/* ---- the deal ---------------------------------------------------------------- */
typedef struct Deal { u32 a, b, qa, qb, gain; } Deal;

/* the least it takes of what it wants per unit of what it gives, Q16.16: holdings and valuations */
static u64 reserve(const AzWorld *w, u32 s)
{
    const u8 *o = orow(w, s), *t = trow(w, s);
    const u64 p = (u64)az_offer_u32(o, AZ_O_PRICE) * (t[AZ_T_VAL_0 + (o[AZ_O_GIVE_RES] & 3u)] + 1u)
                / (t[AZ_T_VAL_0 + (o[AZ_O_WANT_RES] & 3u)] + 1u);
    return p < 1 ? 1 : p > PRICE_TOP ? PRICE_TOP : p;
}

static bool deal(const AzWorld *w, u32 c, u32 r, Deal *d)
{
    const u8 *oc = orow(w, c), *orr = orow(w, r);
    if (az_offer_u32(oc, AZ_O_PRICE) == AZ_PRICE_NONE || az_offer_u32(orr, AZ_O_PRICE) == AZ_PRICE_NONE) return false;
    d->a = oc[AZ_O_GIVE_RES] & 3u; d->b = orr[AZ_O_GIVE_RES] & 3u;
    if (d->a == d->b || (oc[AZ_O_WANT_RES] & 3u) != d->b || (orr[AZ_O_WANT_RES] & 3u) != d->a) return false;
    const u64 pc = reserve(w, c);
    const u64 hi = (1ull << 32) / reserve(w, r);          /* the most B the responder gives for one A */
    if (pc > hi) return false;                            /* no rate suits both */
    const u64 rate = az_isqrt_u64(pc * hi);               /* pc <= rate <= hi */
    const u64 lot_c = ((u64)az_offer_u32(oc, AZ_O_GIVE_QTY) * wp(w, AZ_WP_ECON_LOT_SHARE)) >> 8;
    const u64 lot_r = ((u64)az_offer_u32(orr, AZ_O_GIVE_QTY) * wp(w, AZ_WP_ECON_LOT_SHARE)) >> 8;
    u64 qa = (lot_r << 16) / rate;
    if (qa > lot_c) qa = lot_c;
    const u64 qb = (qa * rate) >> 16;
    if (!qa || !qb || (qb << 16) < qa * pc) return false; /* whole units would give c less than it takes */
    d->qa = (u32)qa; d->qb = (u32)qb;
    const u64 width = (hi << 8) / pc;                     /* 256 = no gain at all */
    d->gain = width > 32256u ? 32000u : (u32)width - 256u;
    return true;
}

i32 az_econ_score(const AzWorld *w, u32 r, u32 c, u32 act)
{
    Deal d;
    if (act != AZ_ACT_TRADE || (w->cfg.lesion & AZ_LESION_TRADE) || !deal(w, c, r, &d)) return -1;
    return 1 + (i32)d.gain;
}

void az_econ_resolve(AzWorld *w, u32 slot, u32 partner, u32 act, bool initiator, u32 worker)
{
    Deal d;
    if (act != AZ_ACT_TRADE || !deal(w, initiator ? slot : partner, initiator ? partner : slot, &d)) return;
    const u32 gres = initiator ? d.a : d.b, wres = initiator ? d.b : d.a;
    const u32 give = initiator ? d.qa : d.qb, get = initiator ? d.qb : d.qa;
    u32 *inv = &w->inv[(size_t)slot * AZ_NINV];
    inv[gres] -= give;                                    /* a lot is a part of what it held at the start of the tick */
    const u64 held = (u64)inv[0] + inv[1] + inv[2] + inv[3];
    const u32 carry = carry_mu(w, slot);
    const u32 room = carry > held ? (u32)(carry - held) : 0u;
    const u32 add = get < room ? get : room;
    inv[wres] += add;
    if (get > add) az_cell_post(w, worker, w->cell[slot], az_inv_species[wres], (i32)(get - add));
    az_tally(w, worker)->trade_units[gres][wres] += give;
    az_stat_bump(w, slot, AZ_ST_TRADES, 1);
    w->flags[slot] |= AZ_F_COOP;
    if (initiator) az_emit(w, worker, AZ_EV_TRADE, (gres << 8) | wres, w->uid[slot], w->uid[partner], (i32)give);
}

/* ---- the pantry -------------------------------------------------------------- */
static void pantry(AzWorld *w, u32 s)
{
    const u8 *t = trow(w, s);
    u32 *inv = &w->inv[(size_t)s * AZ_NINV];
    const u32 hold = hold_mu(w, s);
    const u32 keep = (u32)(((u64)w->mass[s] * wp(w, AZ_WP_ECON_KEEP)) >> 8);
    const i64 store = (i64)(t[AZ_T_STORE_CAP] + 1u) * wp(w, AZ_WP_METAB_STORE_EU);
    const i64 low = (store * wp(w, AZ_WP_ECON_OPEN_SHARE)) >> 8;
    u32 pa = w->pool_a[s];
    i64 e = w->energy[s];

    /* opening: matter first as ORG_Y, then NUT_X; NUT_X also for its energy */
    if (pa < keep / 2u) {
        const u32 q = az_min_u32(inv[1], keep / 2u - pa);
        inv[1] -= q; pa += q;
    }
    {
        u64 need = pa < keep / 2u ? keep / 2u - pa : 0u;
        if (e < low && (u64)((low - e + PACK_EU - 1) / PACK_EU) > need) need = (u64)((low - e + PACK_EU - 1) / PACK_EU);
        const u32 space = pa < hold ? hold - pa : 0u;
        u32 q = need > inv[0] ? inv[0] : (u32)need;
        if (q > space) q = space;
        if (e + (i64)q * PACK_EU > 0x3FFFFFFFll) q = e < 0x3FFFFFFFll ? (u32)((0x3FFFFFFFll - e) / PACK_EU) : 0u;
        inv[0] -= q; pa += q; e += (i64)q * PACK_EU;
    }
    /* setting aside */
    const u64 held = (u64)inv[0] + inv[1] + inv[2] + inv[3];
    const u32 carry = carry_mu(w, s);
    if (pa > keep && carry > held) {
        u64 q = ((u64)(pa - keep) * t[AZ_T_HOARD] * wp(w, AZ_WP_ECON_STORE_SHARE)) >> 16;
        if (q > carry - held) q = carry - held;
        u64 q0 = q * (t[AZ_T_VAL_0] + 1u) / (t[AZ_T_VAL_0] + t[AZ_T_VAL_1] + 2u);
        const u64 can = e > store ? (u64)((e - store) / PACK_EU) : 0u;
        if (q0 > can) q0 = can;
        pa -= (u32)q; e -= (i64)q0 * PACK_EU;
        inv[0] += (u32)q0; inv[1] += (u32)(q - q0);
    }
    w->pool_a[s] = pa;
    w->energy[s] = (i32)e;
}

void az_econ_range(AzWorld *w, u32 begin, u32 end, u32 worker)
{
    for (u32 s = begin; s < end; s++) {
        const u8 fl = w->flags[s];
        if (!(fl & AZ_F_ALIVE) || (fl & (AZ_F_EGG | AZ_F_DORMANT))) continue;
        const u32 col = w->colony[s] < w->n_colonies ? w->colony[s] : 0u;
        if (col) az_colony_say(w, col, 0, (u32)(128 + (i32)w->motor[(size_t)s * AZ_MOT + AZ_M_COLONY_SIG]));
        if (!az_phase_due(w, w->uid[s], AZ_RS_ECON, 2)) continue;
        pantry(w, s);
        if (col) colony_traffic(w, s, col, worker);
    }
}

/* ---- the market's memory ----------------------------------------------------- */
void az_econ_global(AzWorld *w)
{
    for (u32 k = 0; k < AZ_NINV; k++) {
        u64 given = 0;
        for (u32 j = 0; j < AZ_NINV; j++) given += w->totals.trade_units[k][j];
        const u32 now = (u32)(given & 0x7FFFFFFFu);
        const u32 d = (now - (u32)w->gstate[AZ_G_ECON + 8 + k]) & 0x7FFFFFFFu;
        w->gstate[AZ_G_ECON + 8 + k] = (i32)now;
        const i64 seen = d > 0x3FFFFFu ? 0x3FFFFF00ll : (i64)d << 8;
        const i64 v = w->gstate[AZ_G_VOLUME + k];
        w->gstate[AZ_G_VOLUME + k] = (i32)(v + (seen - v) / (i64)(1u << wp(w, AZ_WP_ECON_SMOOTH)));
        const u64 out = w->totals.trade_units[k][1], in = w->totals.trade_units[1][k];
        u64 price = (u64)az_eps_species[az_inv_species[k]] << 8;
        if (k != 1u && out && in) {
            price = (in > (1ull << 40) ? (in / out) << 12 : (in << 12) / out);
            if (price > 0x3FFFFFFFull) price = 0x3FFFFFFFull;
        }
        w->gstate[AZ_G_PRICE + k] = (i32)price;
    }
}
