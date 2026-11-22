/* test_econ.c - the pantry and the barter market (core/az_econ.c, parameters core/params/az_econ.def).
 *
 *   1. pantry   setting aside by HOARD, the form by VAL_0 and VAL_1 and by the energy above its store,
 *               the carrying limit, opening for matter (ORG_Y first) and for energy; egg and cyst
 *   2. offer    who is in the market, what it gives and wants, its price from its holdings
 *   3. deal     20,000 random pairs against the rule written a second time: a deal exactly when the
 *               wants coincide and the two reservation prices leave room; the units moved; neither
 *               side ever gets less than its own reservation price; valuations move the rate;
 *               what does not fit falls to the cell
 *   4. intent   who proposes a trade, to whom, how urgently; the lesion
 *   5. memory   the market's volume and reference price after trades
 *   6. worlds   books exact, equal hashes for 0, 1, 3 and 8 workers and reorder 1 and 9; trades happen;
 *               lesion TRADE gives another world without any
 *
 * Parts 1 to 5 call the module's hooks for hand-made creatures; matter and energy are compared
 * before and after every pulse and every resolved pair.
 */
#include "az_test_locked_block.h"
#include <math.h>

typedef struct Bench {
    AzHost host;
    void *block;
    AzArena arena;
    AzWorld *w;
    u32 n_trade;
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
    for (u32 i = 0; i < n; i++) if (ev[i].kind == AZ_EV_TRADE) { b->n_trade++; b->last = ev[i]; }
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

static u32 P(const AzWorld *w, u32 id) { return w->param[id]; }
static u8 *tr(const AzWorld *w, u32 s) { return &w->trait[(size_t)s * AZ_TRAITS]; }
static u8 *offer(const AzWorld *w, u32 s) { return &w->offer[(size_t)s * AZ_OFFER]; }
static u32 *inv(const AzWorld *w, u32 s) { return &w->inv[(size_t)s * AZ_NINV]; }
static u32 carry(const AzWorld *w, u32 s)
{
    u32 np;
    (void)az_parts(w, s, &np);
    return (u32)(((u64)P(w, AZ_WP_PART_HOLD) * np * P(w, AZ_WP_ECON_CARRY) * (tr(w, s)[AZ_T_CARRY_CAP] + 1u)) >> 16);
}

static u32 steps = 0, inexact = 0;

/* A founder remade with no trait and nothing stored: a body of 2,000 units with 2,000 in its free
 * pool and 10,000 eu. The case then gives it what it is about. */
static void make(AzWorld *w, u32 s)
{
    memset(tr(w, s), 0, AZ_TRAITS);
    tr(w, s)[AZ_T_CARRY_CAP] = 255;                       /* a full pantry: a quarter of what the body holds */
    memset(&w->motor[(size_t)s * AZ_MOT], 0, AZ_MOT);
    w->flags[s] = (u8)(AZ_F_ALIVE | (w->flags[s] & AZ_F_INIT));
    w->energy[s] = 10000; w->pool_a[s] = 2000; w->pool_m[s] = 0; w->pool_t[s] = 0;
    w->waste[s] = 0; w->toxin[s] = 0; w->health[s] = 65535; w->mass[s] = 2000; w->colony[s] = 0;
    for (u32 k = 0; k < AZ_NINV; k++) inv(w, s)[k] = 0;
    u32 np;
    AzPart *part = az_parts(w, s, &np);
    for (u32 i = 0; i < np; i++) part[i].matter = 0;
    AzPair *pr = &w->pair[s];
    pr->claim = 0; pr->peer = AZ_NONE; pr->act = AZ_ACT_NONE; pr->urge = 0; pr->req = 0;
    memset(offer(w, s), 0, AZ_OFFER);
    w->pub_id[w->spos[s]].uid = w->uid[s];
}
static AzPercept nobody(void)
{
    AzPercept p;
    memset(&p, 0, sizeof p);
    p.near_prey = p.near_threat = p.near_kin = p.best_mate = p.bond = p.parent = p.child = AZ_NONE;
    return p;
}
/* phase 1 as the tick runs it: the social offer first (it zeroes the 32 bytes), then the trade bytes */
static void intent(AzWorld *w, u32 s, const AzPercept *p)
{
    AzPair *pr = &w->pair[s];
    pr->claim = 0; pr->peer = AZ_NONE; pr->act = AZ_ACT_NONE; pr->urge = 0;
    az_social_intent(w, s, p, 0);
    az_econ_intent(w, s, p, 0);
}
static void pulse(Bench *b, u32 s)
{
    AzWorld *w = b->w;
    while (!az_phase_due(w, w->uid[s], AZ_RS_ECON, 2)) w->tick++;
    const u64 m0 = az_total_matter(w);
    const i64 e0 = az_total_energy(w);
    az_econ_range(w, s, s + 1u, 0);
    inexact += az_total_matter(w) != m0 || az_total_energy(w) != e0;
    steps++;
    w->tick++;
}
static void resolve(Bench *b, u32 c, u32 r)
{
    AzWorld *w = b->w;
    const u64 m0 = az_total_matter(w);
    const i64 e0 = az_total_energy(w);
    az_econ_resolve(w, c, r, AZ_ACT_TRADE, true, 0);
    az_econ_resolve(w, r, c, AZ_ACT_TRADE, false, 0);
    az_tally_merge(w); az_ledger_flush(w);
    inexact += az_total_matter(w) != m0 || az_total_energy(w) != e0;
    steps++;
}

/* ---- 1: pantry -------------------------------------------------------------- */
static void test_pantry(void)
{
    Bench b;
    u32 sl[1];
    if (!bench_open(&b, 3, 0, sl, 1)) { CHECK(!"bench"); return; }
    AzWorld *w = b.w;
    const u32 s = sl[0];
    u32 *v = inv(w, s);
    w->param[AZ_WP_ECON_STORE_SHARE] = 64;                 /* the arithmetic below is written for a share of 64 */
    /* a body of 2,000 is at ease with 250 in its free pool; HOARD 255 sets aside 1750 x 255 x 64 >> 16 = 435 */
    const u32 q = (1750u * 255u * 64u) >> 16;
    make(w, s);
    pulse(&b, s);
    CHECK_EQ(v[0] + v[1], 0); CHECK_EQ(w->pool_a[s], 2000);          /* HOARD 0 keeps nothing */
    make(w, s); tr(w, s)[AZ_T_HOARD] = 255; tr(w, s)[AZ_T_VAL_1] = 255;
    pulse(&b, s);
    CHECK_EQ(v[0], q / 257u); CHECK_EQ(v[1], q - q / 257u); CHECK_EQ(w->pool_a[s], 2000 - q);
    CHECK_EQ(w->energy[s], 10000 - 8 * (i32)(q / 257u));
    make(w, s); tr(w, s)[AZ_T_HOARD] = 255; tr(w, s)[AZ_T_VAL_0] = 255;
    pulse(&b, s);
    CHECK_EQ(v[0], q * 256u / 257u); CHECK_EQ(v[1], q - q * 256u / 257u);
    CHECK_EQ(w->energy[s], 10000 - 8 * (i32)(q * 256u / 257u));       /* 8 eu packed with every unit of NUT_X */
    const u32 as_nut = v[0];
    make(w, s); tr(w, s)[AZ_T_HOARD] = 255; tr(w, s)[AZ_T_VAL_0] = 255; w->energy[s] = 16 + 80;
    pulse(&b, s);
    CHECK_EQ(v[0], 10); CHECK_EQ(v[1], q - 10); CHECK_EQ(w->energy[s], 16);   /* only energy above its store (16 eu) is packed */
    make(w, s); tr(w, s)[AZ_T_HOARD] = 128;
    pulse(&b, s);
    CHECK_EQ(v[0] + v[1], (1750u * 128u * 64u) >> 16);
    /* the carrying limit: the four resources together */
    make(w, s); tr(w, s)[AZ_T_HOARD] = 255; tr(w, s)[AZ_T_VAL_1] = 255; v[2] = carry(w, s) - 10u;
    pulse(&b, s);
    CHECK_EQ(v[0] + v[1], 10); CHECK_EQ(w->pool_a[s], 1990);
    {   /* the size of the pantry is the creature's own: CARRY_CAP 0 carries 1 / 256 of what 255 does */
        make(w, s); tr(w, s)[AZ_T_HOARD] = 255;
        const u32 full = carry(w, s);
        tr(w, s)[AZ_T_CARRY_CAP] = 0;
        pulse(&b, s);
        CHECK(full >= 1024); CHECK_EQ(carry(w, s), full / 256u); CHECK_EQ(v[0] + v[1], full / 256u);
        make(w, s); tr(w, s)[AZ_T_HOARD] = 255; tr(w, s)[AZ_T_CARRY_CAP] = 127; v[2] = full / 2u - 7u;
        pulse(&b, s);
        CHECK_EQ(v[0] + v[1], 7);
    }
    make(w, s); tr(w, s)[AZ_T_HOARD] = 255; w->param[AZ_WP_ECON_CARRY] = 0;
    pulse(&b, s);
    CHECK_EQ(v[0] + v[1], 0);
    w->param[AZ_WP_ECON_CARRY] = 64;
    /* opening for matter: under half its ease (125) it takes ORG_Y back, then NUT_X with its 8 eu */
    make(w, s); w->pool_a[s] = 50; v[1] = 40; v[0] = 100;
    pulse(&b, s);
    CHECK_EQ(v[1], 0); CHECK_EQ(v[0], 65); CHECK_EQ(w->pool_a[s], 125); CHECK_EQ(w->energy[s], 10000 + 35 * 8);
    make(w, s); w->pool_a[s] = 100; v[1] = 400;
    pulse(&b, s);
    CHECK_EQ(v[1], 375); CHECK_EQ(w->pool_a[s], 125);
    /* opening for energy: store 100 x 16 eu, a quarter of it is 400; at 1 eu it opens 50 units */
    make(w, s); w->pool_a[s] = 200; w->energy[s] = 1; tr(w, s)[AZ_T_STORE_CAP] = 99; v[0] = 80;
    pulse(&b, s);
    CHECK_EQ(v[0], 30); CHECK_EQ(w->energy[s], 401); CHECK_EQ(w->pool_a[s], 250);
    make(w, s); w->pool_a[s] = 200; w->energy[s] = 400; tr(w, s)[AZ_T_STORE_CAP] = 99; v[0] = 80;
    pulse(&b, s);
    CHECK_EQ(v[0], 80);                                    /* at ease: the pantry stays shut */
    /* an egg and a cyst do nothing; a tick that is not its pulse does nothing */
    make(w, s); tr(w, s)[AZ_T_HOARD] = 255; w->flags[s] |= AZ_F_DORMANT;
    pulse(&b, s);
    CHECK_EQ(v[0] + v[1], 0);
    make(w, s); tr(w, s)[AZ_T_HOARD] = 255; w->flags[s] |= AZ_F_EGG;
    pulse(&b, s);
    CHECK_EQ(v[0] + v[1], 0);
    make(w, s); tr(w, s)[AZ_T_HOARD] = 255;
    u32 acted = 0;
    for (u32 i = 0; i < 400u; i++) {
        const u32 before = w->pool_a[s];
        az_econ_range(w, s, s + 1u, 0);
        acted += w->pool_a[s] != before;
        w->pool_a[s] = 2000; v[0] = v[1] = 0;
        w->tick++;
    }
    CHECK_EQ(acted, 100);
    printf("pantry: a body of 2,000 with HOARD 255 sets aside %u of the 1,750 above its ease in a pulse, %u of them as NUT_X "
           "at VAL_0 255 (8 eu each) and %u at VAL_1 255, 10 with 80 eu above its store; never beyond what it carries; hungry, "
           "it takes 40 ORG_Y and 35 NUT_X back; at 1 eu it opens 50 NUT_X for 400 eu; one tick in four\n", q, as_nut, q / 257u);
    bench_close(&b);
}

/* ---- 2: offer --------------------------------------------------------------- */
static void test_offer(void)
{
    Bench b, lt;
    u32 sl[1], sx[1];
    if (!bench_open(&b, 5, 0, sl, 1) || !bench_open(&lt, 5, AZ_LESION_TRADE, sx, 1)) { CHECK(!"bench"); return; }
    AzWorld *w = b.w;
    const u32 s = sl[0];
    const AzPercept p = nobody();
    const u8 *o = offer(w, s);
    make(w, s);
    intent(w, s, &p);
    CHECK_EQ(az_offer_u32(o, AZ_O_PRICE), AZ_PRICE_NONE); CHECK_EQ(az_offer_u32(o, AZ_O_GIVE_QTY), 0);
    make(w, s); inv(w, s)[0] = 5; inv(w, s)[1] = 99;
    intent(w, s, &p);
    CHECK_EQ(o[AZ_O_GIVE_RES], 1); CHECK_EQ(o[AZ_O_WANT_RES], 0); CHECK_EQ(az_offer_u32(o, AZ_O_GIVE_QTY), 99);
    CHECK_EQ(az_offer_u32(o, AZ_O_PRICE), (6u << 16) / 100u);
    make(w, s); inv(w, s)[2] = 7;
    intent(w, s, &p);
    CHECK_EQ(o[AZ_O_GIVE_RES], 2); CHECK_EQ(o[AZ_O_WANT_RES], 0); CHECK_EQ(az_offer_u32(o, AZ_O_PRICE), 65536u / 8u);
    make(w, s); inv(w, s)[2] = 7; inv(w, s)[0] = 3;
    intent(w, s, &p);
    CHECK_EQ(o[AZ_O_WANT_RES], 1);                         /* it wants the usable good it holds least of */
    make(w, s); inv(w, s)[0] = 9; inv(w, s)[1] = 9;
    intent(w, s, &p);
    CHECK_EQ(o[AZ_O_GIVE_RES], 0); CHECK_EQ(o[AZ_O_WANT_RES], 1); CHECK_EQ(az_offer_u32(o, AZ_O_PRICE), 65536);
    make(w, s); inv(w, s)[1] = 99; w->flags[s] |= AZ_F_DORMANT;
    intent(w, s, &p);
    CHECK_EQ(az_offer_u32(o, AZ_O_PRICE), AZ_PRICE_NONE);
    make(lt.w, sx[0]); inv(lt.w, sx[0])[1] = 99;
    intent(lt.w, sx[0], &p);
    CHECK_EQ(az_offer_u32(offer(lt.w, sx[0]), AZ_O_PRICE), AZ_PRICE_NONE);
    CHECK_EQ(az_offer_u32(offer(lt.w, sx[0]), AZ_O_GIVE_QTY), 0);
    bench_close(&b); bench_close(&lt);
}

/* ---- 3: deal ---------------------------------------------------------------- */
typedef struct MDeal { bool ok; u32 a, b; u64 qa, qb, pc, pr; } MDeal;

static bool m_market(const AzWorld *w, u32 s, u32 *g, u32 *want)
{
    const u32 *v = inv(w, s);
    u32 best = 0;
    for (u32 k = 1; k < 4u; k++) if (v[k] > v[best]) best = k;
    if (!v[best]) return false;
    *g = best;
    *want = best == 0 ? 1u : best == 1 ? 0u : v[1] < v[0] ? 1u : 0u;
    return true;
}
static u64 m_reserve(const AzWorld *w, u32 s, u32 g, u32 want)
{
    u64 hp = ((u64)(1u + inv(w, s)[want]) << 16) / (1ull + inv(w, s)[g]);
    if (!hp) hp = 1;
    const u64 p = hp * (tr(w, s)[AZ_T_VAL_0 + g] + 1u) / (tr(w, s)[AZ_T_VAL_0 + want] + 1u);
    return p < 1 ? 1 : p > 0x80000000ull ? 0x80000000ull : p;
}
static u64 m_root(u64 x)
{
    u64 r = (u64)sqrtl((long double)x);
    while (r * r > x) r--;
    while ((r + 1) * (r + 1) <= x) r++;
    return r;
}
static MDeal m_deal(const AzWorld *w, u32 c, u32 r)
{
    MDeal d;
    memset(&d, 0, sizeof d);
    u32 gc, wc, gr, wr;
    if (!m_market(w, c, &gc, &wc) || !m_market(w, r, &gr, &wr) || gc != wr || gr != wc) return d;
    d.a = gc; d.b = gr; d.pc = m_reserve(w, c, gc, wc); d.pr = m_reserve(w, r, gr, wr);
    const u64 hi = 4294967296ull / d.pr;
    if (d.pc > hi) return d;
    const u64 rate = m_root(d.pc * hi);
    const u64 lot_c = ((u64)inv(w, c)[gc] * P(w, AZ_WP_ECON_LOT_SHARE)) >> 8;
    const u64 lot_r = ((u64)inv(w, r)[gr] * P(w, AZ_WP_ECON_LOT_SHARE)) >> 8;
    d.qa = (lot_r << 16) / rate;
    if (d.qa > lot_c) d.qa = lot_c;
    d.qb = (d.qa * rate) >> 16;
    d.ok = d.qa && d.qb && (d.qb << 16) >= d.qa * d.pc;
    return d;
}
static u32 g_x = 2463534242u;
static u32 rnd(void) { g_x ^= g_x << 13; g_x ^= g_x >> 17; g_x ^= g_x << 5; return g_x; }

static void test_deal(void)
{
    Bench b;
    u32 sl[2];
    if (!bench_open(&b, 7, 0, sl, 2)) { CHECK(!"bench"); return; }
    AzWorld *w = b.w;
    const u32 c = sl[0], r = sl[1];
    const AzPercept p = nobody();
    /* one holds 99 ORG_Y, the other 99 NUT_X, equal valuations: each would take 1 for 100; they meet at 1 for 1 */
    make(w, c); make(w, r); inv(w, c)[1] = 99; inv(w, r)[0] = 99;
    intent(w, c, &p); intent(w, r, &p);
    CHECK(az_econ_score(w, r, c, AZ_ACT_TRADE) > 0);
    CHECK(az_econ_score(w, r, c, AZ_ACT_SHARE) < 0);
    resolve(&b, c, r);
    CHECK_EQ(inv(w, c)[1], 50); CHECK_EQ(inv(w, c)[0], 48); CHECK_EQ(inv(w, r)[0], 51); CHECK_EQ(inv(w, r)[1], 49);
    CHECK_EQ(b.n_trade, 1); CHECK_EQ(b.last.aux, (1u << 8) | 0u); CHECK_EQ(b.last.v, 49);
    CHECK_EQ(b.last.a, w->uid[c]); CHECK_EQ(b.last.b, w->uid[r]);
    CHECK((w->flags[c] & AZ_F_COOP) && (w->flags[r] & AZ_F_COOP));
    CHECK_EQ(w->stat[(size_t)c * AZ_NSTAT + AZ_ST_TRADES], 1); CHECK_EQ(w->stat[(size_t)r * AZ_NSTAT + AZ_ST_TRADES], 1);
    /* both hold the same good: no coincidence of wants, no deal, nothing moves */
    make(w, c); make(w, r); inv(w, c)[1] = 99; inv(w, r)[1] = 50;
    intent(w, c, &p); intent(w, r, &p);
    CHECK(az_econ_score(w, r, c, AZ_ACT_TRADE) < 0);
    resolve(&b, c, r);
    CHECK_EQ(inv(w, c)[1], 99); CHECK_EQ(inv(w, r)[1], 50); CHECK_EQ(b.n_trade, 1);
    /* a claimant that values what it gives 256 times what it wants gets a far better rate from one in need */
    make(w, c); make(w, r); inv(w, c)[1] = 99; inv(w, r)[0] = 99; tr(w, c)[AZ_T_VAL_1] = 255;
    intent(w, c, &p); intent(w, r, &p);
    CHECK(az_econ_score(w, r, c, AZ_ACT_TRADE) > 0);
    resolve(&b, c, r);
    const u32 gave = 99 - inv(w, c)[1], got = inv(w, c)[0];
    CHECK(gave >= 1 && got > 10 * gave);
    /* both value what they hold: no rate suits both */
    make(w, c); make(w, r); inv(w, c)[1] = 99; inv(w, r)[0] = 99; tr(w, c)[AZ_T_VAL_1] = 255; tr(w, r)[AZ_T_VAL_0] = 255;
    intent(w, c, &p); intent(w, r, &p);
    CHECK(az_econ_score(w, r, c, AZ_ACT_TRADE) < 0);
    /* what does not fit the pantry falls to the cell */
    make(w, c); make(w, r); inv(w, c)[1] = 99; inv(w, r)[0] = 99;
    intent(w, c, &p); intent(w, r, &p);
    w->param[AZ_WP_ECON_CARRY] = 0;
    const i32 nut0 = w->c_acc[w->cell[c]].delta[AZ_SP_NUT_X], org0 = w->c_acc[w->cell[r]].delta[AZ_SP_ORG_Y];
    resolve(&b, c, r);
    CHECK_EQ(inv(w, c)[0], 0); CHECK_EQ(inv(w, c)[1], 50);
    if (w->cell[c] != w->cell[r]) {
        CHECK_EQ(w->c_acc[w->cell[c]].delta[AZ_SP_NUT_X] - nut0, 48); CHECK_EQ(w->c_acc[w->cell[r]].delta[AZ_SP_ORG_Y] - org0, 49);
    }
    w->param[AZ_WP_ECON_CARRY] = 64;
    /* 20,000 random pairs against the rule written a second time */
    u32 deals = 0, wrong = 0, both_ways = 0;
    u64 moved = 0;
    for (u32 i = 0; i < 20000u; i++) {
        make(w, c); make(w, r);
        for (u32 side = 0; side < 2u; side++) {
            const u32 s = side ? r : c;
            const u32 x = rnd();
            inv(w, s)[0] = (x & 3u) ? (x >> 8) % 300u : 0u;
            inv(w, s)[1] = (x & 12u) ? (x >> 18) % 300u : 0u;
            if ((x & 0xF0u) == 0) inv(w, s)[2 + ((x >> 30) & 1u)] = rnd() % 60u;
            const u32 y = rnd();
            for (u32 k = 0; k < 4u; k++) tr(w, s)[AZ_T_VAL_0 + k] = (u8)((i & 1u) ? (y >> (8 * k)) : (y >> (8 * k)) & 31u);
        }
        intent(w, c, &p); intent(w, r, &p);
        const MDeal m = m_deal(w, c, r);
        u32 c0[4], r0[4];
        memcpy(c0, inv(w, c), sizeof c0); memcpy(r0, inv(w, r), sizeof r0);
        const u32 n0 = b.n_trade;
        wrong += (az_econ_score(w, r, c, AZ_ACT_TRADE) > 0) != m.ok;
        resolve(&b, c, r);
        if (!m.ok) {
            wrong += memcmp(c0, inv(w, c), sizeof c0) != 0 || memcmp(r0, inv(w, r), sizeof r0) != 0 || b.n_trade != n0;
            continue;
        }
        deals++; moved += m.qa + m.qb;
        wrong += inv(w, c)[m.a] != c0[m.a] - m.qa || inv(w, c)[m.b] != c0[m.b] + m.qb;
        wrong += inv(w, r)[m.b] != r0[m.b] - m.qb || inv(w, r)[m.a] != r0[m.a] + m.qa;
        wrong += (m.qb << 16) < m.qa * m.pc;              /* the claimant gets at least its reservation price */
        wrong += (m.qa << 16) < m.qb * m.pr;              /* and so does the responder */
        wrong += b.n_trade != n0 + 1u || b.last.aux != ((m.a << 8) | m.b) || b.last.v != (i64)m.qa;
        both_ways += m.a < 2u && m.b < 2u;
    }
    CHECK_EQ(wrong, 0);
    CHECK(deals > 1000 && deals < 19000);
    printf("deal: 99 ORG_Y against 99 NUT_X at equal valuations: 49 for 48; a claimant that values its own good 256 times "
           "gives %u for %u; 20,000 random pairs: %u deals (%u between the two goods a body makes), %" PRIu64 " units moved, "
           "each as the rule gives it, and no side ever under its own reservation price\n", gave, got, deals, both_ways, moved);
    bench_close(&b);
}

/* ---- 4: intent -------------------------------------------------------------- */
static void test_intent(void)
{
    Bench b, lt;
    u32 sl[40], sx[40];
    if (!bench_open(&b, 9, 0, sl, 40) || !bench_open(&lt, 9, AZ_LESION_TRADE, sx, 40)) { CHECK(!"bench"); return; }
    AzWorld *w = b.w;
    u32 ini = AZ_NONE, ini2 = AZ_NONE, rsp = AZ_NONE;
    for (u32 i = 0; i < 40u; i++) {
        if (az_is_initiator(w, w->uid[sl[i]])) { if (ini == AZ_NONE) ini = sl[i]; else if (ini2 == AZ_NONE) ini2 = sl[i]; }
        else if (rsp == AZ_NONE) rsp = sl[i];
    }
    if (ini == AZ_NONE || ini2 == AZ_NONE || rsp == AZ_NONE) { CHECK(!"roles"); return; }
    make(w, ini); make(w, ini2); make(w, rsp);
    w->flags[ini] |= AZ_F_INIT;
    AzPair *pr = &w->pair[ini];
    AzPercept p = nobody();
    p.near_prey = rsp;
    w->param[AZ_WP_ECON_URGE] = 128;                      /* the arithmetic below is written for an urge of 128 */
    /* TRADE_DRIVE x lopsidedness: 255 units against none is 8 doublings */
    inv(w, ini)[1] = 255; tr(w, ini)[AZ_T_TRADE_DRIVE] = 255;
    intent(w, ini, &p);
    CHECK_EQ(pr->act, AZ_ACT_TRADE); CHECK_EQ(pr->peer, rsp); CHECK_EQ(pr->urge, (255 * 2048 * 128) >> 19);
    tr(w, ini)[AZ_T_TRADE_DRIVE] = 128;
    intent(w, ini, &p);
    CHECK_EQ(pr->urge, 64);
    inv(w, ini)[1] = 3; tr(w, ini)[AZ_T_TRADE_DRIVE] = 255;
    intent(w, ini, &p);
    CHECK_EQ(pr->urge, (255 * 512 * 128) >> 19);
    tr(w, ini)[AZ_T_TRADE_DRIVE] = 0;
    intent(w, ini, &p);
    CHECK_EQ(pr->act, AZ_ACT_NONE);                       /* no drive, no proposal */
    tr(w, ini)[AZ_T_TRADE_DRIVE] = 255; inv(w, ini)[1] = 0;
    intent(w, ini, &p);
    CHECK_EQ(pr->act, AZ_ACT_NONE);                       /* nothing to give */
    inv(w, ini)[1] = 255;
    p = nobody();
    intent(w, ini, &p);
    CHECK_EQ(pr->act, AZ_ACT_NONE);                       /* nobody near */
    p.near_threat = ini2;
    intent(w, ini, &p);
    CHECK_EQ(pr->act, AZ_ACT_NONE);                       /* an initiator is not asked */
    p = nobody(); p.near_kin = rsp;
    intent(w, ini, &p);
    CHECK_EQ(pr->peer, rsp);
    /* a stronger proposal standing is kept; a responder proposes nothing */
    az_social_intent(w, ini, &p, 0);
    pr->act = AZ_ACT_MATE; pr->urge = 200; pr->peer = rsp;
    az_econ_intent(w, ini, &p, 0);
    CHECK_EQ(pr->act, AZ_ACT_MATE);
    inv(w, rsp)[1] = 255; tr(w, rsp)[AZ_T_TRADE_DRIVE] = 255;
    p = nobody(); p.near_prey = ini;
    intent(w, rsp, &p);
    CHECK_EQ(w->pair[rsp].act, AZ_ACT_NONE);
    {   /* the lesion: nobody proposes */
        AzWorld *x = lt.w;
        u32 xi = AZ_NONE, xr = AZ_NONE;
        for (u32 i = 0; i < 40u; i++) {
            if (az_is_initiator(x, x->uid[sx[i]])) { if (xi == AZ_NONE) xi = sx[i]; }
            else if (xr == AZ_NONE) xr = sx[i];
        }
        if (xi == AZ_NONE || xr == AZ_NONE) { CHECK(!"roles"); return; }
        make(x, xi); make(x, xr);
        x->flags[xi] |= AZ_F_INIT;
        inv(x, xi)[1] = 255; tr(x, xi)[AZ_T_TRADE_DRIVE] = 255; inv(x, xr)[0] = 255;
        AzPercept q = nobody();
        q.near_prey = xr;
        intent(x, xi, &q); intent(x, xr, &q);
        CHECK_EQ(x->pair[xi].act, AZ_ACT_NONE);
        CHECK(az_econ_score(x, xr, xi, AZ_ACT_TRADE) < 0);
    }
    bench_close(&b); bench_close(&lt);
}

/* ---- 5: memory -------------------------------------------------------------- */
static void test_memory(void)
{
    Bench b;
    u32 sl[2];
    if (!bench_open(&b, 11, 0, sl, 2)) { CHECK(!"bench"); return; }
    AzWorld *w = b.w;
    const u32 c = sl[0], r = sl[1];
    const AzPercept p = nobody();
    CHECK_EQ(w->totals.trade_units[0][1] + w->totals.trade_units[1][0], 0);
    CHECK_EQ(w->gstate[AZ_G_PRICE + 0], 24 << 8); CHECK_EQ(w->gstate[AZ_G_PRICE + 1], 16 << 8);
    make(w, c); make(w, r); inv(w, c)[1] = 99; inv(w, r)[0] = 99;
    intent(w, c, &p); intent(w, r, &p);
    resolve(&b, c, r);                                    /* 49 ORG_Y for 48 NUT_X */
    az_econ_global(w);
    CHECK_EQ(w->gstate[AZ_G_VOLUME + 1], (49 << 8) / 8); CHECK_EQ(w->gstate[AZ_G_VOLUME + 0], (48 << 8) / 8);
    CHECK_EQ(w->gstate[AZ_G_PRICE + 1], 4096);            /* ORG_Y is the measure: its chemical 16 eu */
    CHECK_EQ(w->gstate[AZ_G_PRICE + 0], (49 << 12) / 48); /* a unit of NUT_X fetched 49 / 48 of one */
    CHECK_EQ(w->gstate[AZ_G_PRICE + 2], 0); CHECK_EQ(w->gstate[AZ_G_VOLUME + 2], 0);
    const i32 v1 = w->gstate[AZ_G_VOLUME + 1];
    az_econ_global(w);                                    /* a tick without a trade: the volume fades, the price stays */
    CHECK_EQ(w->gstate[AZ_G_VOLUME + 1], v1 - v1 / 8); CHECK_EQ(w->gstate[AZ_G_PRICE + 0], (49 << 12) / 48);
    for (u32 i = 0; i < 200u; i++) az_econ_global(w);
    CHECK(w->gstate[AZ_G_VOLUME + 1] < 8);
    bench_close(&b);
}

/* ---- 6: worlds -------------------------------------------------------------- */
typedef struct Run { u64 hash, births, deaths, trades, units[2], holders, stored[2]; u32 live, ok; i32 price; } Run;

static Run world_run(u32 threads, u8 reorder, u32 lesion, u64 seed, u32 ticks, i32 share)
{
    Run r;
    memset(&r, 0, sizeof r);
    AzConfig cfg = az_test_config(seed, 2000);
    cfg.reorder_period = reorder;
    cfg.cap = 40000;
    cfg.lesion = lesion;
    AzTestWorld t;
    if (!az_test_world(&t, &cfg, threads)) { CHECK(!"world"); return r; }
    AzWorld *w = t.w;
    if (share >= 0) w->param[AZ_WP_ECON_STORE_SHARE] = (u32)share;
    const u64 m0 = az_total_matter(w);
    const i64 e0 = az_total_energy(w);
    r.ok = 1;
    for (u32 i = 0; i < ticks; i++) {
        az_tick(w);
        if (az_total_matter(w) != m0 || az_total_energy(w) != e0) r.ok = 0;
    }
    for (u32 s = 0; s < w->n_slots; s++) {
        if (!(w->flags[s] & AZ_F_ALIVE)) continue;
        r.holders += (inv(w, s)[0] + inv(w, s)[1]) != 0;
        r.stored[0] += inv(w, s)[0]; r.stored[1] += inv(w, s)[1];
    }
    r.trades = w->totals.interactions[AZ_ACT_TRADE];
    r.units[0] = w->totals.trade_units[0][1]; r.units[1] = w->totals.trade_units[1][0];
    r.price = w->gstate[AZ_G_PRICE + 0];
    r.hash = az_state_hash(w); r.births = w->totals.births; r.deaths = w->totals.deaths; r.live = w->n_live;
    if (w->totals.field_underflow) r.ok = 0;
    az_test_world_free(&t);
    return r;
}

static void test_worlds(void)
{
    const Run a = world_run(0, 1, 0, 23, 150, -1), b = world_run(1, 9, 0, 23, 150, -1), c = world_run(3, 1, 0, 23, 150, -1),
              d = world_run(8, 9, 0, 23, 150, -1);
    CHECK(a.ok && b.ok && c.ok && d.ok);
    CHECK_EQ(a.hash, b.hash); CHECK_EQ(a.hash, c.hash); CHECK_EQ(a.hash, d.hash);
    CHECK(a.trades > 0 && a.units[0] > 0 && a.units[1] > 0);
    CHECK(a.stored[0] > 0 && a.stored[1] > 0);
    const Run lt = world_run(0, 1, AZ_LESION_TRADE, 23, 150, -1), lt8 = world_run(8, 9, AZ_LESION_TRADE, 23, 150, -1);
    CHECK(lt.ok && lt8.ok);
    CHECK_EQ(lt.hash, lt8.hash); CHECK(lt.hash != a.hash);
    CHECK_EQ(lt.trades, 0); CHECK(lt.stored[1] > 0);       /* the pantry stays under the lesion */
    static const i32 dose[5] = { 0, 4, 16, 64, 256 };
    for (u32 k = 0; k < 5u; k++) {
        const Run x = world_run(8, 9, 0, 23, 150, dose[k]);
        CHECK(x.ok);
        printf("  store share %3d: %" PRIu64 " births, %" PRIu64 " deaths, %" PRIu64 " trades, %" PRIu64 " + %" PRIu64 " units stored, %u alive\n",
               dose[k], x.births, x.deaths, x.trades, x.stored[0], x.stored[1], x.live);
    }
    /* seeds 23 and 29 were looked at while the two defaults (store share 16, urge 48) were set; seed 31 was
     * first run after both were set, no default has been changed since, and it is printed as measured */
    const Run f0 = world_run(8, 9, 0, 31, 150, 0), f1 = world_run(8, 9, 0, 31, 150, -1);
    CHECK(f0.ok && f1.ok);
    printf("  seed 31, not looked at before: %" PRIu64 " births and %" PRIu64 " deaths with the pantry, %" PRIu64 " and %" PRIu64
           " without; %" PRIu64 " trades\n", f1.births, f1.deaths, f0.births, f0.deaths, f1.trades);
    printf("worlds: 2000 founders, 150 ticks: %" PRIu64 " trades moved %" PRIu64 " NUT_X for %" PRIu64 " ORG_Y (a unit of "
           "NUT_X fetched %.2f eu with ORG_Y at 16); %" PRIu64 " of %u alive hold goods (%" PRIu64 " NUT_X, %" PRIu64
           " ORG_Y); %" PRIu64 " births, %" PRIu64 " deaths (%" PRIu64 " and %" PRIu64 " with no trade), hash %016" PRIx64
           " on 0, 1, 3 and 8 workers, books exact every tick\n",
           a.trades, a.units[0], a.units[1], a.price / 256.0, a.holders, a.live, a.stored[0], a.stored[1], a.births, a.deaths,
           lt.births, lt.deaths, a.hash);
}

int main(void)
{
    test_pantry();
    test_offer();
    test_deal();
    test_intent();
    test_memory();
    test_worlds();
    CHECK_EQ(inexact, 0);
    printf("books: matter and energy the same after each of %u pulses and pairs as before it\n", steps);
    return az_test_done();
}
