/* test_social.c - social acts (core/az_social.c, parameters core/params/az_social.def).
 *
 *   1. offer     what a creature shows and commits; the RETALIATE and SUBMIT
 *                bits by temper and by weakness
 *   2. contest   every number of a fight for both sides against the rule
 *                written a second time, over 400 draws; the odds by strength,
 *                WEAPON, ARMOR and the stake (STAKE and the brain's drive);
 *                noticing (SENSE_ACUITY against SAT); yielding (SUBMISSION);
 *                SPIKES, PUNISH, PREDATION, DOMINANCE; death by wounds
 *   3. theft     STEAL takes its share of what is held for trade; a victim
 *                that notices keeps it and may punish; the robbed remember
 *   4. help      SHARE by ALTRUISM, by CARE toward one's own young (not under
 *                the lesion); GROOM; TEACH by the learner's IMITATE; RECRUIT
 *                (not under the lesion); consent, grudges (RECIPROCITY),
 *                forgiving (FORGIVE)
 *   5. intent    which act a creature proposes, to whom, how urgently
 *   6. signals   drive, gains, honesty, GLOW, the cost, the aim; the lesion;
 *                territory marks
 *   7. worlds    books exact, equal hashes for 0, 1, 3 and 8 workers and
 *                reorder 1 and 9; every act occurs, RECRUIT too (colonies
 *                are founded by core/az_colony.c); the SIGNALS and CARE
 *                lesions give other worlds
 *
 * Parts 1 to 6 call the module's hooks for hand-made creatures; matter and
 * energy are compared before and after every resolved pair.
 */
#include "az_test_locked_block.h"
#include <math.h>

typedef struct Bench {
    AzHost host;
    void *block;
    AzArena arena;
    AzWorld *w;
    u32 n_ev[32];
    AzEvent last;
} Bench;

static void bench_pfor(void *ctx, AzWorld *w, AzJobFn fn, const u32 *splits, u32 n)
{
    (void)ctx;
    for (u32 k = 0; k < n; k++) fn(w, splits[k], splits[k + 1], 0);
}
static u64 bench_now(void *ctx) { (void)ctx; return 0; }
static void bench_sink(void *ctx, const AzEvent *ev, u32 n)
{
    Bench *b = (Bench *)ctx;
    for (u32 i = 0; i < n; i++) {
        if (ev[i].kind < 32u) b->n_ev[ev[i].kind]++;
        if (ev[i].kind == AZ_EV_ATTACK || ev[i].kind == AZ_EV_STEAL || ev[i].kind == AZ_EV_SHARE ||
            ev[i].kind == AZ_EV_TEACH || ev[i].kind == AZ_EV_RECRUIT) b->last = ev[i];
    }
}
static bool bench_open(Bench *b, u64 seed, u32 lesion, u32 *slot, u32 want)
{
    AzConfig cfg = az_test_config(seed, 64);
    cfg.lesion = lesion;
    memset(b, 0, sizeof *b);
    b->host.ctx = b; b->host.n_workers = 1;
    b->host.parallel_for = bench_pfor; b->host.now_ns = bench_now;
    b->host.sink = bench_sink; b->host.sink_ctx = b;
    const u64 bytes = az_world_bytes(&cfg);
    b->block = az_test_locked_block(bytes);
    if (!b->block) return false;
    b->arena.base = (u8 *)b->block; b->arena.size = bytes; b->arena.used = 0;
    b->w = az_world_create(&b->arena, &cfg, &b->host);
    if (!b->w) return false;
    az_worldgen(b->w);
    az_tick(b->w);                                        /* every founder gets its public record */
    u32 n = 0;
    for (u32 s = 0; s < b->w->n_slots && n < want; s++)
        if ((b->w->flags[s] & AZ_F_ALIVE) && b->w->spos[s] != AZ_NONE && b->w->blk[s]) slot[n++] = s;
    return n == want;
}
static void bench_close(Bench *b) { if (b->block) az_test_locked_block_free(b->block); memset(b, 0, sizeof *b); }
static void drain(Bench *b) { az_tally_merge(b->w); az_ledger_flush(b->w); }

static u32 P(const AzWorld *w, u32 id) { return w->param[id]; }
static u8 *tr(const AzWorld *w, u32 s) { return &w->trait[(size_t)s * AZ_TRAITS]; }
static u8 *ph(const AzWorld *w, u32 s) { return &w->phen[(size_t)s * AZ_PHEN]; }
static u8 *offer(const AzWorld *w, u32 s) { return &w->offer[(size_t)s * AZ_OFFER]; }
static i8 *motor(const AzWorld *w, u32 s) { return &w->motor[(size_t)s * AZ_MOT]; }
static AzPub *pub(const AzWorld *w, u32 s) { return &w->pub[w->spos[s]]; }
static AzPubId *pub_id(const AzWorld *w, u32 s) { return &w->pub_id[w->spos[s]]; }
static u32 cause(const AzWorld *w, u32 s) { return (w->pair[s].req & AZ_RQ_DIE) ? (u32)(w->pair[s].req >> 4) : 99u; }

static u32 pairs = 0, inexact = 0;

/* A founder remade with no trait, no temper, no drive, no memory, no kin and no colony: 2,000
 * units of free pool, 10,000 eu, full health, its own tag. The case then gives it what it is about. */
static void make(AzWorld *w, u32 s)
{
    memset(tr(w, s), 0, AZ_TRAITS);
    ph(w, s)[AZ_P_AGGR] = 0; ph(w, s)[AZ_P_SOCIAL] = 0;
    memset(motor(w, s), 0, AZ_MOT);
    w->flags[s] = (u8)(AZ_F_ALIVE | (w->flags[s] & AZ_F_INIT));
    w->energy[s] = 10000; w->pool_a[s] = 2000; w->pool_m[s] = 0; w->pool_t[s] = 0;
    w->waste[s] = 0; w->toxin[s] = 0; w->health[s] = 65535; w->mass[s] = 2000;
    w->rank[s] = 0; w->colony[s] = 0; w->arousal[s] = 0;
    w->tag[s] = (u16)(0x1000u + s);
    for (u32 k = 0; k < 4u; k++) { w->rep_tag[(size_t)s * 4 + k] = 0; w->rep_val[(size_t)s * 4 + k] = 0; }
    for (u32 k = 0; k < AZ_NINV; k++) w->inv[(size_t)s * AZ_NINV + k] = 0;
    u32 np;
    AzPart *part = az_parts(w, s, &np);
    for (u32 i = 0; i < np; i++) part[i].matter = 0;
    AzPair *pr = &w->pair[s];
    pr->claim = 0; pr->peer = AZ_NONE; pr->act = AZ_ACT_NONE; pr->urge = 0; pr->req = 0;
    memset(offer(w, s), 0, AZ_OFFER);
    AzPub *pb = pub(w, s);
    pb->tag = w->tag[s]; pb->colony = 0;
    AzPubId *id = pub_id(w, s);
    id->uid = w->uid[s]; id->parent_a = 0; id->parent_b = 0; id->partner = 0;
}

static AzPercept nobody(void)
{
    AzPercept p;
    memset(&p, 0, sizeof p);
    p.near_prey = p.near_threat = p.near_kin = p.best_mate = p.bond = p.parent = p.child = AZ_NONE;
    return p;
}
static void intent(AzWorld *w, u32 s, const AzPercept *p)
{
    AzPair *pr = &w->pair[s];
    pr->claim = 0; pr->peer = AZ_NONE; pr->act = AZ_ACT_NONE; pr->urge = 0;
    az_social_intent(w, s, p, 0);
}
/* both sides of a pair, a the initiator; the books must be the same after as before */
static void resolve(Bench *b, u32 a, u32 d, u32 act)
{
    AzWorld *w = b->w;
    const u64 m0 = az_total_matter(w);
    const i64 e0 = az_total_energy(w);
    az_social_resolve(w, a, d, act, true, 0);
    az_social_resolve(w, d, a, act, false, 0);
    drain(b);
    inexact += az_total_matter(w) != m0 || az_total_energy(w) != e0;
    pairs++;
}
/* the offers of two creatures as phase 1 writes them (nobody near, so nothing is proposed) */
static void offers(AzWorld *w, u32 a, u32 d)
{
    const AzPercept p = nobody();
    intent(w, a, &p);
    intent(w, d, &p);
}

/* ---- 1: offer --------------------------------------------------------------- */
static void test_offer(void)
{
    Bench b;
    u32 sl[2];
    if (!bench_open(&b, 3, 0, sl, 2)) { CHECK(!"bench"); return; }
    AzWorld *w = b.w;
    const u32 s = sl[0];
    const AzPercept p = nobody();
    make(w, s);
    intent(w, s, &p);
    const u8 *o = offer(w, s);
    CHECK_EQ(o[AZ_O_STRENGTH], (2000 / 64) * 256 >> 8);   /* 31 at full health */
    CHECK_EQ(o[AZ_O_SIZE], az_radius_code(2000));
    CHECK_EQ(o[AZ_O_RANK], 128);
    CHECK_EQ(az_offer_u32(o, AZ_O_ENERGY), 5000); CHECK_EQ(az_offer_u32(o, AZ_O_MATTER), 1000);
    const u32 wealth0 = o[AZ_O_WEALTH];
    make(w, s); w->health[s] = 32767; w->energy[s] = 40000; w->rank[s] = -12800; w->mass[s] = 100000;
    intent(w, s, &p);
    CHECK_EQ(o[AZ_O_STRENGTH], (255 * 128) >> 8);         /* the body counts up to 255 units; half health halves it */
    CHECK_EQ(o[AZ_O_RANK], 128 - 50);
    CHECK(o[AZ_O_WEALTH] > wealth0);
    make(w, s); w->flags[s] |= AZ_F_DORMANT;
    intent(w, s, &p);
    CHECK_EQ(az_offer_u32(o, AZ_O_ENERGY), 0); CHECK_EQ(o[AZ_O_DISP], 0);
    /* temper and weakness, over 2,000 ticks each */
    u32 ret[3] = { 0, 0, 0 }, sub[3] = { 0, 0, 0 };
    for (u32 k = 0; k < 3u; k++) {
        for (u32 i = 0; i < 2000u; i++) {
            make(w, s);
            ph(w, s)[AZ_P_AGGR] = (u8)(k * 127); w->arousal[s] = (u8)(k * 127);
            w->health[s] = (u16)(65535u - k * 32000u); w->rank[s] = (i16)(-(i32)k * 16000);
            w->tick++;
            intent(w, s, &p);
            ret[k] += (o[AZ_O_DISP] & AZ_OD_RETALIATE) != 0;
            sub[k] += (o[AZ_O_DISP] & AZ_OD_SUBMIT) != 0;
        }
    }
    CHECK_EQ(ret[0], 0); CHECK(ret[1] > 900 && ret[1] < 1100); CHECK(ret[2] > 1900);
    CHECK(sub[0] < sub[1] && sub[1] < sub[2]); CHECK(sub[0] < 300);
    printf("offer: 2,000 units at full health show a strength of 31 and commit 5,000 of 10,000 eu and 1,000 of 2,000 "
           "units; the RETALIATE bit is set in %u, %u, %u of 2,000 ticks at temper 0, 127, 254, the SUBMIT bit in %u, %u, %u "
           "as rank and health fall\n", ret[0], ret[1], ret[2], sub[0], sub[1], sub[2]);
    bench_close(&b);
}

/* ---- 2: contest ------------------------------------------------------------- */
typedef struct Side { i32 e; u32 pa, health; i32 rank; } Side;
static Side side(const AzWorld *w, u32 s) { Side x = { w->energy[s], w->pool_a[s], w->health[s], w->rank[s] }; return x; }

static u32 m_level(const AzWorld *w, u32 s) { return (u32)az_clamp_i32((i32)tr(w, s)[AZ_T_STAKE] + motor(w, s)[AZ_M_STAKE], 0, 255); }
static u32 m_notice(const AzWorld *w, u32 v, u32 a)
{
    return (u32)(((u64)tr(w, v)[AZ_T_SENSE_ACUITY] * (64u + tr(w, a)[AZ_T_SAT]) * P(w, AZ_WP_SOCIAL_EVADE)) >> 16);
}
static u64 m_force(const AzWorld *w, u32 s, bool defends)
{
    return (u64)(1u + offer(w, s)[AZ_O_STRENGTH]) * (256u + tr(w, s)[AZ_T_WEAPON] + (tr(w, s)[AZ_T_SHAPE_N] >> 1) +
           (defends ? tr(w, s)[AZ_T_ARMOR] : 0u)) * (64u + m_level(w, s));
}

typedef struct Tally { u32 n, seen, yielded, a_won, checked, wrong; } Tally;

/* one contest between a (attacker) and d, with the offers phase 1 wrote; every number of both
 * sides is checked against the rule written here a second time */
static void fight(Bench *b, u32 a, u32 d, Tally *t)
{
    AzWorld *w = b->w;
    const Side a0 = side(w, a), d0 = side(w, d);
    u32 r[4];
    const u64 ua = w->uid[a], ud = w->uid[d];
    az_rand4(w, ua < ud ? ua : ud, AZ_RS_CONTEST, ua < ud ? ud : ua, r);
    const bool seen = (r[0] & 255u) < m_notice(w, d, a);
    const u32 la = m_level(w, a), ld = m_level(w, d);
    const bool yields = !seen && (offer(w, d)[AZ_O_DISP] & AZ_OD_SUBMIT) && ((r[0] >> 8) & 255u) < tr(w, d)[AZ_T_SUBMISSION];
    const u64 fa = m_force(w, a, false), fd = m_force(w, d, true);
    const bool a_wins = yields || (u64)r[1] * (fa + fd) < (fa << 32);
    const bool punish = (offer(w, d)[AZ_O_DISP] & AZ_OD_RETALIATE) != 0;
    const u32 n_att = b->n_ev[AZ_EV_ATTACK];
    resolve(b, a, d, AZ_ACT_ATTACK);
    const Side a1 = side(w, a), d1 = side(w, d);
    t->n++; t->seen += seen; t->yielded += yields; t->a_won += !seen && a_wins; t->checked++;
    u32 bad = b->n_ev[AZ_EV_ATTACK] != n_att + 1u;
    if (seen) {
        bad += a1.e != a0.e - (i32)P(w, AZ_WP_SOCIAL_FIGHT_EU) || a1.pa != a0.pa || a1.health != a0.health;
        bad += d1.e != d0.e || d1.pa != d0.pa || d1.health != d0.health || d1.rank != d0.rank;
        bad += b->last.aux != 0 || b->last.v != 0;
        t->wrong += bad;
        return;
    }
    const u32 lo = a_wins ? d : a, wi = a_wins ? a : d;
    const Side l0 = a_wins ? d0 : a0, w0 = a_wins ? a0 : d0, l1 = a_wins ? d1 : a1, w1 = a_wins ? a1 : d1;
    u64 qe = ((u64)az_offer_u32(offer(w, lo), AZ_O_ENERGY) * ((a_wins ? ld : la) + 1u)) >> 8;
    u64 qm = ((u64)az_offer_u32(offer(w, lo), AZ_O_MATTER) * ((a_wins ? ld : la) + 1u)) >> 8;
    if (yields) { qe = (qe * P(w, AZ_WP_SOCIAL_YIELD_SHARE)) >> 8; qm = (qm * P(w, AZ_WP_SOCIAL_YIELD_SHARE)) >> 8; }
    const i32 cost_a = yields ? 0 : (i32)((P(w, AZ_WP_SOCIAL_FIGHT_EU) * (la + 1u)) >> 6);
    const i32 cost_d = yields ? 0 : (i32)((P(w, AZ_WP_SOCIAL_FIGHT_EU) * (ld + 1u)) >> 6)
                     + (!yields && punish ? (i32)((tr(w, d)[AZ_T_PUNISH] * P(w, AZ_WP_SOCIAL_PUNISH_EU)) >> 8) : 0);
    /* energy: the loser loses its stake, the winner gains it (these bodies have room), each pays its own costs */
    bad += l1.e != l0.e - (i32)qe - (lo == a ? cost_a : cost_d);
    bad += w1.e != w0.e + (i32)qe - (wi == a ? cost_a : cost_d);
    /* matter: the winner's PREDATION share into its pool, the rest into its cell */
    bad += l1.pa != l0.pa - (u32)qm;
    bad += w1.pa != w0.pa + (u32)((qm * (tr(w, wi)[AZ_T_PREDATION] + 1u)) >> 8);
    /* health */
    u64 hurt_l = yields ? 0 : ((u64)P(w, AZ_WP_SOCIAL_WOUND) * (64u + tr(w, wi)[AZ_T_WEAPON])) / (64u + tr(w, lo)[AZ_T_ARMOR]);
    u64 hurt_a = yields ? 0 : (((u64)tr(w, d)[AZ_T_SPIKES] * P(w, AZ_WP_SOCIAL_SPIKE_WOUND)) >> 8)
               + (punish ? ((u64)tr(w, d)[AZ_T_PUNISH] * P(w, AZ_WP_SOCIAL_PUNISH_WOUND)) >> 8 : 0);
    const u64 ha = hurt_a + (lo == a ? hurt_l : 0), hd = lo == d ? hurt_l : 0;
    bad += a1.health != (ha >= a0.health ? 0u : a0.health - (u32)ha);
    bad += d1.health != (hd >= d0.health ? 0u : d0.health - (u32)hd);
    if (a1.health == 0) bad += cause(w, a) != AZ_DIE_KILLED;
    /* rank */
    bad += l1.rank != az_clamp_i32(l0.rank - (i32)P(w, AZ_WP_SOCIAL_RANK_STEP), -32767, 32767);
    bad += w1.rank != az_clamp_i32(w0.rank + (i32)((P(w, AZ_WP_SOCIAL_RANK_STEP) * (64u + tr(w, wi)[AZ_T_DOMINANCE])) >> 7), -32767, 32767);
    bad += b->last.aux != (u32)a_wins || b->last.v != (i64)qm || b->last.a != ua || b->last.b != ud;
    t->wrong += bad;
}

/* the same two fighters again and again, restored before each fight; the clock moves so the draw does */
static Tally bout(Bench *b, u32 a, u32 d, u32 n, void (*dress)(AzWorld *, u32, u32))
{
    AzWorld *w = b->w;
    Tally t;
    memset(&t, 0, sizeof t);
    for (u32 i = 0; i < n; i++) {
        make(w, a); make(w, d);
        dress(w, a, d);
        w->tick++;
        const u8 keep = offer(w, d)[AZ_O_DISP];
        (void)keep;
        offers(w, a, d);
        fight(b, a, d, &t);
    }
    return t;
}
static u8 g_lvl_a, g_lvl_d, g_weapon, g_armor;
static void dress_plain(AzWorld *w, u32 a, u32 d) { tr(w, a)[AZ_T_STAKE] = g_lvl_a; tr(w, d)[AZ_T_STAKE] = g_lvl_d; }
static void dress_arms(AzWorld *w, u32 a, u32 d)
{
    tr(w, a)[AZ_T_STAKE] = 128; tr(w, d)[AZ_T_STAKE] = 128;
    tr(w, a)[AZ_T_WEAPON] = g_weapon; tr(w, d)[AZ_T_ARMOR] = g_armor;
    tr(w, a)[AZ_T_PREDATION] = 127; tr(w, d)[AZ_T_PREDATION] = 30; tr(w, a)[AZ_T_DOMINANCE] = 200;
    tr(w, d)[AZ_T_SPIKES] = 64; tr(w, d)[AZ_T_PUNISH] = 100; ph(w, d)[AZ_P_AGGR] = 200; w->arousal[d] = 100;
    motor(w, a)[AZ_M_STAKE] = 40; motor(w, d)[AZ_M_STAKE] = -40;
}
static void dress_seen(AzWorld *w, u32 a, u32 d) { tr(w, d)[AZ_T_SENSE_ACUITY] = g_weapon; tr(w, a)[AZ_T_SAT] = g_armor; tr(w, a)[AZ_T_STAKE] = 128; }
static void dress_meek(AzWorld *w, u32 a, u32 d)
{
    tr(w, a)[AZ_T_STAKE] = 255; tr(w, d)[AZ_T_STAKE] = 255; tr(w, d)[AZ_T_SUBMISSION] = g_weapon;
    w->rank[d] = -32000; w->health[d] = 20000;
}
static void dress_small(AzWorld *w, u32 a, u32 d) { tr(w, a)[AZ_T_STAKE] = 128; tr(w, d)[AZ_T_STAKE] = 128; w->mass[d] = 500; }
static void dress_kill(AzWorld *w, u32 a, u32 d) { tr(w, a)[AZ_T_STAKE] = 255; tr(w, a)[AZ_T_WEAPON] = 255; w->health[d] = 300; w->mass[d] = 64; }

static void test_contest(void)
{
    Bench b;
    u32 sl[2];
    if (!bench_open(&b, 5, 0, sl, 2)) { CHECK(!"bench"); return; }
    AzWorld *w = b.w;
    const u32 a = sl[0], d = sl[1];
    u32 wrong = 0, checked = 0;
    /* equal bodies: the stake is the strategy. Odds Fa / (Fa + Fd) with F = 32 x 256 x (64 + level). */
    static const u8 lv[4][2] = { { 0, 0 }, { 255, 0 }, { 0, 255 }, { 128, 128 } };
    u32 won[4];
    for (u32 k = 0; k < 4u; k++) {
        g_lvl_a = lv[k][0]; g_lvl_d = lv[k][1];
        const Tally t = bout(&b, a, d, 2000, dress_plain);
        won[k] = t.a_won; wrong += t.wrong; checked += t.checked;
        const double odds = (64.0 + lv[k][0]) / (128.0 + lv[k][0] + lv[k][1]);
        CHECK(fabs(t.a_won / 2000.0 - odds) < 0.035);        /* 3 standard errors of 2,000 draws */
        CHECK_EQ(t.seen, 0); CHECK_EQ(t.yielded, 0);
    }
    /* arms: WEAPON raises the attacker's odds, ARMOR the defender's; SPIKES, PUNISH, PREDATION, DOMINANCE in the numbers */
    u32 arms[3];
    for (u32 k = 0; k < 3u; k++) {
        g_weapon = k == 1 ? 255 : 0; g_armor = k == 2 ? 255 : 0;
        const Tally t = bout(&b, a, d, 1500, dress_arms);
        arms[k] = t.a_won; wrong += t.wrong; checked += t.checked;
    }
    CHECK(arms[1] > arms[0] + 100 && arms[2] + 100 < arms[0]);
    /* a larger body wins more often */
    const Tally small = bout(&b, a, d, 1500, dress_small);
    wrong += small.wrong; checked += small.checked;
    CHECK(small.a_won > 1050);                            /* strength 31 against 7: odds 32 / 40 */
    /* noticing: SENSE_ACUITY against the attacker's SAT */
    u32 seen[3];
    for (u32 k = 0; k < 3u; k++) {
        g_weapon = k ? 255 : 0; g_armor = k == 2 ? 192 : 0;
        const Tally t = bout(&b, a, d, 2000, dress_seen);
        seen[k] = t.seen; wrong += t.wrong; checked += t.checked;
    }
    CHECK_EQ(seen[0], 0);
    CHECK(fabs(seen[1] / 2000.0 - 31.0 / 256.0) < 0.03);     /* 255 x 64 x 128 >> 16 = 31 */
    CHECK(fabs(seen[2] / 2000.0 - 127.0 / 256.0) < 0.04);    /* 255 x 256 x 128 >> 16 = 127 */
    /* yielding: a weak one of low rank with SUBMISSION gives up a quarter of its stake and is not hurt */
    u32 meek[2];
    for (u32 k = 0; k < 2u; k++) {
        g_weapon = k ? 255 : 0;
        const Tally t = bout(&b, a, d, 1500, dress_meek);
        meek[k] = t.yielded; wrong += t.wrong; checked += t.checked;
    }
    CHECK_EQ(meek[0], 0); CHECK(meek[1] > 300);
    /* death by wounds */
    const Tally kill = bout(&b, a, d, 200, dress_kill);
    wrong += kill.wrong; checked += kill.checked;
    make(w, a); make(w, d); dress_kill(w, a, d);
    u32 died = 0;
    for (u32 i = 0; i < 50u && !died; i++) {
        make(w, a); make(w, d); dress_kill(w, a, d);
        w->tick++;
        offers(w, a, d);
        resolve(&b, a, d, AZ_ACT_ATTACK);
        if (b.last.aux == 1u) { died = 1; CHECK_EQ(cause(w, d), AZ_DIE_KILLED); CHECK_EQ(w->health[d], 0); }
    }
    CHECK(died);
    CHECK_EQ(wrong, 0);
    printf("contest: %u fights, every number of both sides as the rule gives it; equal bodies, attacker's wins in 2,000 "
           "at stakes 0:0, 255:0, 0:255, 128:128 = %u, %u, %u, %u (odds 0.50, 0.83, 0.17, 0.50); with WEAPON 255 %u of 1,500 "
           "against %u plain and %u against ARMOR 255; four times the body wins %u of 1,500; seen coming %u, %u, %u of "
           "2,000 (ACUITY 0, 255, 255 against SAT 192); yielded %u of 1,500 with SUBMISSION 255, never without\n",
           checked, won[0], won[1], won[2], won[3], arms[1], arms[0], arms[2], small.a_won, seen[0], seen[1], seen[2], meek[1]);
    bench_close(&b);
}

/* ---- 3: theft, 4: help ------------------------------------------------------ */
static void hold_for_trade(AzWorld *w, u32 v, u32 res, u32 q)
{
    w->inv[(size_t)v * AZ_NINV + res] = q;
    offer(w, v)[AZ_O_GIVE_RES] = (u8)res;
    az_offer_set_u32(offer(w, v), AZ_O_GIVE_QTY, q);      /* as az_econ.c writes it */
}

static void test_theft_and_help(void)
{
    Bench b, lc, lk;
    u32 sl[3], sc[3], sk[3];
    if (!bench_open(&b, 7, 0, sl, 3) || !bench_open(&lc, 7, AZ_LESION_CARE, sc, 3) ||
        !bench_open(&lk, 7, AZ_LESION_COLONY, sk, 3)) { CHECK(!"bench"); return; }
    AzWorld *w = b.w;
    const u32 t = sl[0], v = sl[1], x = sl[2];
    /* theft: (STEAL + 1) / 256 of what is held for trade */
    static const u8 skill[3] = { 0, 127, 255 };
    u32 took[3];
    for (u32 k = 0; k < 3u; k++) {
        make(w, t); make(w, v); tr(w, t)[AZ_T_STEAL] = skill[k];
        offers(w, t, v); hold_for_trade(w, v, 1, 400);
        resolve(&b, t, v, AZ_ACT_STEAL);
        took[k] = w->inv[(size_t)t * AZ_NINV + 1];
        CHECK_EQ(took[k], (400u * (skill[k] + 1u)) >> 8);
        CHECK_EQ(w->inv[(size_t)v * AZ_NINV + 1], 400u - took[k]);
        CHECK_EQ(b.last.kind, AZ_EV_STEAL); CHECK_EQ(b.last.v, (i64)took[k]); CHECK_EQ(b.last.aux, 0);
        CHECK(!(w->flags[t] & AZ_F_COOP));
    }
    /* the robbed remember the thief's tag, and refuse its gift by RECIPROCITY */
    CHECK_EQ(w->rep_val[(size_t)v * 4], -(i32)P(w, AZ_WP_SOCIAL_MEMORY_STEP)); CHECK_EQ(w->rep_tag[(size_t)v * 4], w->tag[t]);
    tr(w, v)[AZ_T_RECIPROCITY] = 255;
    CHECK(az_social_score(w, v, t, AZ_ACT_SHARE) < 0);
    CHECK(az_social_score(w, v, x, AZ_ACT_SHARE) > 0);       /* a stranger's gift is taken */
    tr(w, v)[AZ_T_RECIPROCITY] = 0;
    CHECK(az_social_score(w, v, t, AZ_ACT_SHARE) > 0);       /* one who keeps no accounts takes it too */
    /* forgiving: the harm fades by FORGIVE, one social pulse in four ticks */
    tr(w, v)[AZ_T_RECIPROCITY] = 255; tr(w, v)[AZ_T_FORGIVE] = 255;
    u32 ticks = 0;
    while (az_social_score(w, v, t, AZ_ACT_SHARE) < 0 && ticks < 400u) { az_social_range(w, v, v + 1u, 0); w->tick++; ticks++; }
    CHECK(ticks > 8 && ticks < 80);
    const u32 forgiven = ticks;
    tr(w, v)[AZ_T_FORGIVE] = 0; w->rep_val[(size_t)v * 4] = -48;
    for (u32 i = 0; i < 200u; i++) { az_social_range(w, v, v + 1u, 0); w->tick++; }
    CHECK_EQ(w->rep_val[(size_t)v * 4], -48);
    /* a victim that notices keeps it; one that retaliates wounds the thief by PUNISH and pays for it */
    u32 caught = 0, hurt = 0;
    for (u32 i = 0; i < 600u; i++) {
        make(w, t); make(w, v); tr(w, t)[AZ_T_STEAL] = 255; tr(w, v)[AZ_T_SENSE_ACUITY] = 255; tr(w, t)[AZ_T_SAT] = 192;
        tr(w, v)[AZ_T_PUNISH] = 128; ph(w, v)[AZ_P_AGGR] = 255; w->arousal[v] = 255;
        w->tick++;
        offers(w, t, v); hold_for_trade(w, v, 2, 100);
        resolve(&b, t, v, AZ_ACT_STEAL);
        if (b.last.aux) {
            caught++;
            CHECK_EQ(w->inv[(size_t)v * AZ_NINV + 2], 100); CHECK_EQ(w->inv[(size_t)t * AZ_NINV + 2], 0);
            if (offer(w, v)[AZ_O_DISP] & AZ_OD_RETALIATE) {
                hurt++;
                CHECK_EQ(w->health[t], 65535 - ((128 * 4096) >> 8)); CHECK_EQ(w->energy[v], 10000 - ((128 * 32) >> 8));
            }
        } else CHECK_EQ(w->inv[(size_t)t * AZ_NINV + 2], 100);
    }
    CHECK(caught > 240 && caught < 360); CHECK(hurt > 200);

    /* sharing: (ALTRUISM + 1) / 256 of the commitments, energy and matter */
    make(w, t); make(w, v); tr(w, t)[AZ_T_ALTRUISM] = 127;
    offers(w, t, v);
    resolve(&b, t, v, AZ_ACT_SHARE);
    CHECK_EQ(w->energy[t], 10000 - 2500); CHECK_EQ(w->energy[v], 10000 + 2500);
    CHECK_EQ(w->pool_a[t], 2000 - 500); CHECK_EQ(w->pool_a[v], 2000 + 500);
    CHECK_EQ(b.last.kind, AZ_EV_SHARE); CHECK_EQ(b.last.v, 2500);
    CHECK((w->flags[t] & AZ_F_COOP) && (w->flags[v] & AZ_F_COOP));
    CHECK(w->rep_val[(size_t)v * 4] > 0);
    /* a taker with no room: the gift is not lost, it falls to its cell */
    make(w, t); make(w, v); tr(w, t)[AZ_T_ALTRUISM] = 255;
    { u32 np; (void)az_parts(w, v, &np); w->pool_m[v] = np * 4096u; }
    offers(w, t, v);
    const u32 org0 = w->c_acc[w->cell[v]].delta[AZ_SP_ORG_Y];
    resolve(&b, t, v, AZ_ACT_SHARE);
    CHECK_EQ(w->pool_a[v], 2000); CHECK_EQ(w->c_acc[w->cell[v]].delta[AZ_SP_ORG_Y] - org0, 1000);
    w->pool_m[v] = 0;
    /* care: toward its own young a parent gives by CARE; not under the lesion */
    i32 fed[2];
    for (u32 k = 0; k < 2u; k++) {
        Bench *bb = k ? &lc : &b;
        const u32 par = k ? sc[0] : t, kid = k ? sc[1] : v;
        make(bb->w, par); make(bb->w, kid);
        tr(bb->w, par)[AZ_T_CARE] = 255;
        pub_id(bb->w, kid)->parent_a = bb->w->uid[par];
        offers(bb->w, par, kid);
        resolve(bb, par, kid, AZ_ACT_SHARE);
        fed[k] = bb->w->energy[kid] - 10000;
    }
    CHECK_EQ(fed[0], 5000); CHECK_EQ(fed[1], 5000 >> 8);     /* without care: its ALTRUISM of 0, 1/256 */
    make(w, t); make(w, v); tr(w, t)[AZ_T_CARE] = 255;       /* not its child: CARE does not count */
    offers(w, t, v);
    resolve(&b, t, v, AZ_ACT_SHARE);
    CHECK_EQ(w->energy[v], 10000 + (5000 >> 8));

    /* grooming: the groomed one drops a quarter of its toxin and 1 unit; the groomer pays 2 eu */
    make(w, t); make(w, v); w->toxin[v] = 200;
    offers(w, t, v);
    resolve(&b, t, v, AZ_ACT_GROOM);
    CHECK_EQ(w->toxin[v], 200 - 51); CHECK_EQ(w->energy[t], 10000 - 2); CHECK_EQ(b.last.aux, 1);
    /* teaching: the learner's gain and bias move toward the teacher's by its own IMITATE */
    static const u8 im[3] = { 0, 128, 255 };
    i32 gained[3];
    for (u32 k = 0; k < 3u; k++) {
        make(w, t); make(w, v); tr(w, v)[AZ_T_IMITATE] = im[k];
        u32 n;
        AzBrain *bt = az_brain(w, t, &n), *bv = az_brain(w, v, &n);
        for (u32 l = 0; l < AZ_MODG; l++) { bt->gain[l] = 164; bt->bias[l] = -100; bv->gain[l] = 64; bv->bias[l] = 0; }
        offers(w, t, v);
        resolve(&b, t, v, AZ_ACT_TEACH);
        gained[k] = bv->gain[3] - 64;
        CHECK_EQ(bv->gain[3], 64 + (100 * im[k]) / 256); CHECK_EQ(bv->bias[5], (-100 * im[k]) / 256);
        CHECK_EQ(bt->gain[3], 164);
        CHECK_EQ(b.last.kind, AZ_EV_TEACH);
    }
    /* consent to be taught: a draw against IMITATE */
    u32 heard[2] = { 0, 0 };
    for (u32 k = 0; k < 2u; k++)
        for (u32 i = 0; i < 1000u; i++) {
            make(w, t); make(w, v); tr(w, v)[AZ_T_IMITATE] = k ? 128 : 0;
            w->tick++;
            heard[k] += az_social_score(w, v, t, AZ_ACT_TEACH) > 0;
        }
    CHECK_EQ(heard[0], 0); CHECK(heard[1] > 430 && heard[1] < 570);
    /* recruiting: a solitary one takes the recruiter's colony; not under the lesion; a member is not asked twice */
    for (u32 k = 0; k < 2u; k++) {
        Bench *bb = k ? &lk : &b;
        const u32 rc = k ? sk[0] : t, nw = k ? sk[1] : v;
        make(bb->w, rc); make(bb->w, nw);
        bb->w->colony[rc] = 7; pub(bb->w, rc)->colony = 7; ph(bb->w, nw)[AZ_P_SOCIAL] = 255;
        CHECK((az_social_score(bb->w, nw, rc, AZ_ACT_RECRUIT) > 0) == (k == 0));
        offers(bb->w, rc, nw);
        resolve(bb, rc, nw, AZ_ACT_RECRUIT);
        CHECK_EQ(bb->w->colony[nw], k ? 0u : 7u);
    }
    pub(w, v)->colony = 7;
    CHECK(az_social_score(w, v, t, AZ_ACT_RECRUIT) < 0);
    /* kin are preferred */
    make(w, t); make(w, v); make(w, x);
    pub_id(w, t)->parent_a = 12345; pub_id(w, v)->parent_b = 12345;
    CHECK(az_social_score(w, v, t, AZ_ACT_SHARE) > az_social_score(w, v, x, AZ_ACT_SHARE) + 100);
    printf("theft: STEAL 0, 127, 255 takes %u, %u, %u of 400 units held for trade; an alert victim caught %u of 600 and "
           "punished %u; the robbed refuse the thief's gift until the harm has faded (%u ticks at FORGIVE 255, never at 0)\n",
           took[0], took[1], took[2], caught, hurt, forgiven);
    printf("help: ALTRUISM 127 gives 2,500 of 5,000 eu and 500 of 1,000 units committed; a parent with CARE 255 gives its "
           "young %d eu, %d under the lesion; grooming drops 51 of 200 units of toxin; a learner with IMITATE 0, 128, 255 "
           "moves its gain by %d, %d, %d of the 100 between it and the teacher and is heard out %u of 1,000 times at 128\n",
           fed[0], fed[1], gained[0], gained[1], gained[2], heard[1]);
    bench_close(&b); bench_close(&lc); bench_close(&lk);
}

/* ---- 5: intent -------------------------------------------------------------- */
static void test_intent(void)
{
    Bench b, lc;
    u32 sl[40], sc[40];
    if (!bench_open(&b, 9, 0, sl, 40) || !bench_open(&lc, 9, AZ_LESION_CARE, sc, 40)) { CHECK(!"bench"); return; }
    AzWorld *w = b.w;
    u32 ini = AZ_NONE, r1 = AZ_NONE, r2 = AZ_NONE, ini2 = AZ_NONE;
    for (u32 i = 0; i < 40u; i++) {
        if (az_is_initiator(w, w->uid[sl[i]])) { if (ini == AZ_NONE) ini = sl[i]; else if (ini2 == AZ_NONE) ini2 = sl[i]; }
        else if (r1 == AZ_NONE) r1 = sl[i];
        else if (r2 == AZ_NONE) r2 = sl[i];
    }
    if (ini == AZ_NONE || r1 == AZ_NONE || r2 == AZ_NONE || ini2 == AZ_NONE) { CHECK(!"roles"); return; }
    make(w, ini); make(w, r1); make(w, r2); make(w, ini2);
    w->flags[ini] |= AZ_F_INIT;
    AzPercept p = nobody();
    AzPair *pr = &w->pair[ini];
    /* no drive, no proposal */
    p.near_prey = r1; p.near_kin = r2;
    intent(w, ini, &p);
    CHECK_EQ(pr->act, AZ_ACT_NONE);
    /* the AGON drive weighed by AGGR: ATTACK toward the prey; with STEAL 255, STEAL */
    motor(w, ini)[AZ_M_AGON] = 100; ph(w, ini)[AZ_P_AGGR] = 255;
    intent(w, ini, &p);
    CHECK_EQ(pr->act, AZ_ACT_ATTACK); CHECK_EQ(pr->peer, r1); CHECK_EQ(pr->urge, 1 + 100);
    ph(w, ini)[AZ_P_AGGR] = 127;
    intent(w, ini, &p);
    CHECK_EQ(pr->urge, 1 + 50);
    ph(w, ini)[AZ_P_AGGR] = 0; motor(w, ini)[AZ_M_AGON] = 127;
    intent(w, ini, &p);
    CHECK_EQ(pr->act, AZ_ACT_NONE);                       /* a drive of 127 on an AGGR of 0 is under one unit */
    ph(w, ini)[AZ_P_AGGR] = 255; tr(w, ini)[AZ_T_STEAL] = 255; motor(w, ini)[AZ_M_AGON] = 100;
    intent(w, ini, &p);
    CHECK_EQ(pr->act, AZ_ACT_STEAL);
    tr(w, ini)[AZ_T_STEAL] = 0;
    /* a larger one is dared only by BOLDNESS; an initiator is never asked */
    p = nobody(); p.near_threat = r1;
    u32 dared[2] = { 0, 0 };
    for (u32 k = 0; k < 2u; k++)
        for (u32 i = 0; i < 400u; i++) {
            tr(w, ini)[AZ_T_BOLDNESS] = k ? 128 : 0;
            w->tick++;
            if (!az_is_initiator(w, w->uid[ini]) || az_is_initiator(w, w->uid[r1])) continue;
            intent(w, ini, &p);
            dared[k] += pr->act == AZ_ACT_ATTACK;
        }
    CHECK_EQ(dared[0], 0); CHECK(dared[1] > 20);
    while (!az_is_initiator(w, w->uid[ini]) || az_is_initiator(w, w->uid[r1]) || !az_is_initiator(w, w->uid[ini2])) w->tick++;
    p = nobody(); p.near_prey = ini2;
    intent(w, ini, &p);
    CHECK_EQ(pr->act, AZ_ACT_NONE);
    /* its own ground: a colony member on its colony's mark is more urgent against an outsider by TERRITORIAL / 4 */
    p = nobody(); p.near_prey = r1;
    w->colony[ini] = 5; tr(w, ini)[AZ_T_TERRITORIAL] = 200;
    intent(w, ini, &p);
    const u32 plain = pr->urge;
    w->c_mark_id[w->cell[ini]] = 5; w->c_mark[(size_t)w->cell[ini] * AZ_NMARK + AZ_MK_TERRITORY] = 10;
    intent(w, ini, &p);
    CHECK_EQ(pr->urge, plain + 50);
    pub(w, r1)->colony = 5;
    intent(w, ini, &p);
    CHECK_EQ(pr->urge, plain);                            /* one of its own is no intruder */
    pub(w, r1)->colony = 0; w->colony[ini] = 0; w->c_mark_id[w->cell[ini]] = 0;
    w->c_mark[(size_t)w->cell[ini] * AZ_NMARK + AZ_MK_TERRITORY] = 0;
    /* the AFFIL drive weighed by SOCIAL: toward kin, by the weights of its own traits */
    motor(w, ini)[AZ_M_AGON] = 0; motor(w, ini)[AZ_M_AFFIL] = 100; ph(w, ini)[AZ_P_SOCIAL] = 255;
    tr(w, ini)[AZ_T_ALTRUISM] = 255; ph(w, ini)[AZ_P_SOCIAL] = 255;
    p = nobody(); p.near_kin = r2;
    u32 acts[AZ_ACT_COUNT];
    memset(acts, 0, sizeof acts);
    u32 n = 0;
    for (u32 i = 0; i < 3000u; i++) {
        w->tick++;
        if (!az_is_initiator(w, w->uid[ini]) || az_is_initiator(w, w->uid[r2])) continue;
        intent(w, ini, &p);
        CHECK_EQ(pr->peer, r2); CHECK_EQ(pr->urge, 1 + 100);
        acts[pr->act]++; n++;
    }
    CHECK(n > 300);
    /* weights 255 (share), 255 (groom), 127 (teach), 0 (recruit: it has no colony) */
    CHECK(fabs(acts[AZ_ACT_SHARE] / (double)n - 255.0 / 637.0) < 0.08);
    CHECK(fabs(acts[AZ_ACT_TEACH] / (double)n - 127.0 / 637.0) < 0.07);
    CHECK_EQ(acts[AZ_ACT_RECRUIT], 0); CHECK_EQ(acts[AZ_ACT_ATTACK], 0);
    /* its own young come first, by CARE; not under the lesion */
    while (!az_is_initiator(w, w->uid[ini]) || az_is_initiator(w, w->uid[r1]) || az_is_initiator(w, w->uid[r2])) w->tick++;
    tr(w, ini)[AZ_T_CARE] = 200;
    p = nobody(); p.near_kin = r2; p.child = r1;
    intent(w, ini, &p);
    CHECK_EQ(pr->peer, r1);
    tr(w, ini)[AZ_T_CARE] = 0;
    intent(w, ini, &p);
    CHECK_EQ(pr->peer, r2);
    {
        AzWorld *x = lc.w;
        u32 xi = AZ_NONE, xa = AZ_NONE, xb = AZ_NONE;
        for (u32 i = 0; i < 40u; i++) {
            if (az_is_initiator(x, x->uid[sc[i]])) { if (xi == AZ_NONE) xi = sc[i]; }
            else if (xa == AZ_NONE) xa = sc[i];
            else if (xb == AZ_NONE) xb = sc[i];
        }
        if (xi == AZ_NONE || xa == AZ_NONE || xb == AZ_NONE) { CHECK(!"roles"); return; }
        make(x, xi); make(x, xa); make(x, xb);
        x->flags[xi] |= AZ_F_INIT;
        motor(x, xi)[AZ_M_AFFIL] = 100; ph(x, xi)[AZ_P_SOCIAL] = 255; tr(x, xi)[AZ_T_CARE] = 200; tr(x, xi)[AZ_T_ALTRUISM] = 255;
        AzPercept q = nobody();
        q.child = xa; q.near_kin = xb;
        intent(x, xi, &q);
        CHECK_EQ(x->pair[xi].peer, xb);                   /* the lesion: no care proposal toward its young */
    }
    /* a stronger proposal already standing is kept */
    pr->urge = 0;
    p = nobody(); p.near_kin = r2;
    az_social_intent(w, ini, &p, 0);
    pr->act = AZ_ACT_MATE; pr->urge = 250; pr->peer = r1;
    const AzPair held = *pr;
    {
        u8 keep[AZ_OFFER];
        memcpy(keep, offer(w, ini), AZ_OFFER);
        az_social_intent(w, ini, &p, 0);
        CHECK_EQ(pr->act, held.act); CHECK_EQ(pr->peer, held.peer); CHECK_EQ(pr->urge, 250);
    }
    printf("intent: AGON 100 on AGGR 255 proposes an attack on the prey with urge 101 (51 on AGGR 127, a theft with STEAL "
           "255); a larger one is dared %u of the times with BOLDNESS 128, never with 0; on its own colony's mark an "
           "outsider is 50 more urgent at TERRITORIAL 200; AFFIL toward kin chose share %u, groom %u, teach %u of %u; its "
           "own young first by CARE, not under the lesion\n", dared[1], acts[AZ_ACT_SHARE], acts[AZ_ACT_GROOM], acts[AZ_ACT_TEACH], n);
    bench_close(&b); bench_close(&lc);
}

/* ---- 6: signals ------------------------------------------------------------- */
static void pulse(Bench *b, u32 s)
{
    AzWorld *w = b->w;
    while (!az_phase_due(w, w->uid[s], AZ_RS_SOCIAL, 2)) w->tick++;
    const u64 m0 = az_total_matter(w);
    const i64 e0 = az_total_energy(w);
    az_social_range(w, s, s + 1u, 0);
    inexact += az_total_matter(w) != m0 || az_total_energy(w) != e0;
    pairs++;
    w->tick++;
}

static void test_signals(void)
{
    Bench b, ls;
    u32 sl[1], sx[1];
    if (!bench_open(&b, 11, 0, sl, 1) || !bench_open(&ls, 11, AZ_LESION_SIGNALS, sx, 1)) { CHECK(!"bench"); return; }
    AzWorld *w = b.w;
    const u32 s = sl[0];
    u8 *sig = &w->sig[(size_t)s * 2];
    /* drive x gain: 100 x 2 x 255 / 256 = 199, and 255 / 256 of it with no tie to condition = 198 on A;
     * 8 eu for a full signal, so 198 x 8 x 512 >> 17 = 6 eu */
    make(w, s); motor(w, s)[AZ_M_SIG_A] = 100; tr(w, s)[AZ_T_SIG_A_GAIN] = 255;
    pulse(&b, s);
    CHECK_EQ(sig[0], 198); CHECK_EQ(sig[1], 0); CHECK_EQ(w->energy[s], 10000 - 6);
    make(w, s); motor(w, s)[AZ_M_SIG_A] = 100; tr(w, s)[AZ_T_SIG_A_GAIN] = 128; motor(w, s)[AZ_M_SIG_B] = 50; tr(w, s)[AZ_T_SIG_B_GAIN] = 255;
    pulse(&b, s);
    CHECK_EQ(sig[0], 99); CHECK_EQ(sig[1], 98);           /* (100 x 65280) >> 16 and (99 x 65280) >> 16 */
    make(w, s); motor(w, s)[AZ_M_SIG_A] = -100; tr(w, s)[AZ_T_SIG_A_GAIN] = 255;
    pulse(&b, s);
    CHECK_EQ(sig[0], 0); CHECK_EQ(w->energy[s], 10000);
    /* honesty ties the signal to true condition: at half health an honest signal is half as strong */
    u32 shown[3];
    for (u32 k = 0; k < 3u; k++) {
        make(w, s); motor(w, s)[AZ_M_SIG_A] = 100; tr(w, s)[AZ_T_SIG_A_GAIN] = 255; tr(w, s)[AZ_T_SIG_HONESTY] = (u8)(k * 127);
        w->health[s] = 32767;
        pulse(&b, s);
        shown[k] = sig[0];
    }
    CHECK(shown[0] > 190 && shown[1] > 140 && shown[1] < 160 && shown[2] > 95 && shown[2] < 105);
    /* GLOW shines without a drive and costs; a signal that cannot be paid for is not sent */
    make(w, s); tr(w, s)[AZ_T_GLOW] = 200;
    pulse(&b, s);
    CHECK_EQ(sig[0], 50); CHECK_EQ(w->energy[s], 10000 - 1);
    make(w, s); motor(w, s)[AZ_M_SIG_A] = 100; tr(w, s)[AZ_T_SIG_A_GAIN] = 255; w->energy[s] = 5;
    pulse(&b, s);
    CHECK_EQ(sig[0], 0); CHECK_EQ(w->energy[s], 5);
    /* aimed, it costs down to half */
    make(w, s); motor(w, s)[AZ_M_SIG_A] = 127; tr(w, s)[AZ_T_SIG_A_GAIN] = 255; motor(w, s)[AZ_M_SIG_B] = 127; tr(w, s)[AZ_T_SIG_B_GAIN] = 255;
    pulse(&b, s);
    const i32 wide = 10000 - w->energy[s];
    make(w, s); motor(w, s)[AZ_M_SIG_A] = 127; tr(w, s)[AZ_T_SIG_A_GAIN] = 255; motor(w, s)[AZ_M_SIG_B] = 127; tr(w, s)[AZ_T_SIG_B_GAIN] = 255;
    motor(w, s)[AZ_M_SIG_AIM] = 127; tr(w, s)[AZ_T_SIG_AIM] = 255;
    pulse(&b, s);
    const i32 aimed = 10000 - w->energy[s];
    /* each channel (253 x 65280) >> 16 = 252; the aim 127 x 2 x 255 >> 8 = 253 of 512 comes off the cost */
    CHECK_EQ(wide, (504 * 8 * 512) >> 17); CHECK_EQ(aimed, (504 * 8 * (512 - 253)) >> 17); CHECK(aimed < wide);
    /* the lesion: the column stays 0 and nothing is paid */
    make(ls.w, sx[0]); motor(ls.w, sx[0])[AZ_M_SIG_A] = 100; tr(ls.w, sx[0])[AZ_T_SIG_A_GAIN] = 255; tr(ls.w, sx[0])[AZ_T_GLOW] = 255;
    ls.w->sig[(size_t)sx[0] * 2] = 77;
    pulse(&ls, sx[0]);
    CHECK_EQ(ls.w->sig[(size_t)sx[0] * 2], 0); CHECK_EQ(ls.w->energy[sx[0]], 10000);
    /* territory: a colony member lays TERRITORIAL x 64 / 256 on the mark of its cell, with its colony's id */
    make(w, s); w->colony[s] = 9; tr(w, s)[AZ_T_TERRITORIAL] = 200;
    const u32 m0 = w->c_acc[w->cell[s]].dmark[AZ_MK_TERRITORY];
    pulse(&b, s);
    CHECK_EQ(w->c_acc[w->cell[s]].dmark[AZ_MK_TERRITORY] - m0, 50);
    make(w, s); tr(w, s)[AZ_T_TERRITORIAL] = 200;
    const u32 m1 = w->c_acc[w->cell[s]].dmark[AZ_MK_TERRITORY];
    pulse(&b, s);
    CHECK_EQ(w->c_acc[w->cell[s]].dmark[AZ_MK_TERRITORY], m1);       /* a solitary one has no territory */
    printf("signals: drive 100 at gain 255 sends 198 for 6 eu a pulse; at half health HONESTY 0, 127, 254 shows %u, %u, %u; "
           "GLOW 200 shines 50; unpaid, nothing is sent; two full channels cost %d eu, aimed %d; nothing under the lesion; "
           "a colony member with TERRITORIAL 200 marks 50\n", shown[0], shown[1], shown[2], wide, aimed);
    bench_close(&b); bench_close(&ls);
}

/* ---- 7: worlds -------------------------------------------------------------- */
typedef struct Run { u64 hash, births, deaths, act[AZ_ACT_COUNT]; u32 live, ok; u64 sig; } Run;

static Run world_run(u32 threads, u8 reorder, u32 lesion)
{
    Run r;
    memset(&r, 0, sizeof r);
    AzConfig cfg = az_test_config(19, 2000);
    cfg.reorder_period = reorder;
    cfg.cap = 40000;
    cfg.lesion = lesion;
    AzTestWorld t;
    if (!az_test_world(&t, &cfg, threads)) { CHECK(!"world"); return r; }
    AzWorld *w = t.w;
    const u64 m0 = az_total_matter(w);
    const i64 e0 = az_total_energy(w);
    r.ok = 1;
    for (u32 i = 0; i < 150u; i++) {
        az_tick(w);
        if (az_total_matter(w) != m0 || az_total_energy(w) != e0) r.ok = 0;
    }
    for (u32 s = 0; s < w->n_slots; s++)
        if (w->flags[s] & AZ_F_ALIVE) r.sig += w->sig[(size_t)s * 2] + w->sig[(size_t)s * 2 + 1];
    for (u32 k = 0; k < AZ_ACT_COUNT; k++) r.act[k] = w->totals.interactions[k];
    r.hash = az_state_hash(w); r.births = w->totals.births; r.deaths = w->totals.deaths; r.live = w->n_live;
    if (w->totals.field_underflow) r.ok = 0;
    az_test_world_free(&t);
    return r;
}

static void test_worlds(void)
{
    const Run a = world_run(0, 1, 0), b = world_run(1, 9, 0), c = world_run(3, 1, 0), d = world_run(8, 9, 0);
    CHECK(a.ok && b.ok && c.ok && d.ok);
    CHECK_EQ(a.hash, b.hash); CHECK_EQ(a.hash, c.hash); CHECK_EQ(a.hash, d.hash);
    CHECK(a.act[AZ_ACT_ATTACK] > 0 && a.act[AZ_ACT_STEAL] > 0 && a.act[AZ_ACT_SHARE] > 0 && a.act[AZ_ACT_GROOM] > 0 &&
          a.act[AZ_ACT_TEACH] > 0);
    CHECK(a.act[AZ_ACT_RECRUIT] > 0);                     /* colonies are founded (az_colony.c), so members recruit */
    CHECK(a.sig > 0);
    const Run ls = world_run(0, 1, AZ_LESION_SIGNALS), ls8 = world_run(8, 9, AZ_LESION_SIGNALS);
    const Run lc = world_run(0, 1, AZ_LESION_CARE), lc8 = world_run(8, 9, AZ_LESION_CARE);
    const Run lo = world_run(0, 1, AZ_LESION_SOCIAL);
    CHECK(ls.ok && ls8.ok && lc.ok && lc8.ok && lo.ok);
    CHECK_EQ(ls.hash, ls8.hash); CHECK_EQ(lc.hash, lc8.hash);
    CHECK(ls.hash != a.hash); CHECK(lc.hash != a.hash); CHECK(lo.hash != a.hash);
    CHECK_EQ(ls.sig, 0);
    CHECK_EQ(lo.act[AZ_ACT_ATTACK] + lo.act[AZ_ACT_SHARE], 0);
    printf("worlds: 2000 founders, 150 ticks: attack %" PRIu64 ", steal %" PRIu64 ", share %" PRIu64 ", groom %" PRIu64
           ", teach %" PRIu64 ", recruit %" PRIu64 "; %" PRIu64 " births, %" PRIu64 " deaths (%" PRIu64 " with no pair "
           "acts at all), %u alive, hash %016" PRIx64 " on 0, 1, 3 and 8 workers, books exact every tick; the SIGNALS and "
           "CARE lesions give other worlds, the same on 0 and 8 workers\n",
           a.act[AZ_ACT_ATTACK], a.act[AZ_ACT_STEAL], a.act[AZ_ACT_SHARE], a.act[AZ_ACT_GROOM], a.act[AZ_ACT_TEACH],
           a.act[AZ_ACT_RECRUIT], a.births, a.deaths, lo.deaths, a.live, a.hash);
}

int main(void)
{
    test_offer();
    test_contest();
    test_theft_and_help();
    test_intent();
    test_signals();
    test_worlds();
    CHECK_EQ(inexact, 0);
    printf("books: matter and energy the same after each of %u pairs and pulses as before it\n", pairs);
    return az_test_done();
}
