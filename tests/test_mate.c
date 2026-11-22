/* test_mate.c - who mates with whom (core/az_mate.c, parameters core/params/az_mate.def).
 *
 *   1. kinds     every pair of kind and mating type: who can breed together
 *                and who bears; between hermaphrodites by SEX_BIAS
 *   2. eyes      what a chooser sees through each preference gene: ORNAMENT
 *                and SHEEN as far as the suitor displays, emitted signals,
 *                size, likeness in PATTERN and SHAPE_M (kin most alike),
 *                rank; the mean of the four by her own weights
 *   3. choosy    her own threshold; none without a preference
 *   4. bonds     formed by BOND in the kinds that pair up, strengthened by
 *                the partner, worn by others; FIDELITY refuses others while
 *                the partner is beside her; GUARD outbids a better rival
 *   5. intent    the display, the offer bits, and who proposes to whom
 *   6. worlds    whole ticks: books exact, equal hashes for 0, 1, 3 and 8
 *                workers and reorder 1 and 9, sexual births happen, and
 *                under AZ_LESION_MATING only budding and selfing go on
 *   7. signal    twin worlds: in one every founder carries the preference
 *                for display (PREF_SIGNAL) and is choosy, in the other none
 *                has a preference. With the preference the children of
 *                matings carry more ORNAMENT. The historical gate compares
 *                maternal-generation-one offspring to all active adult founders.
 *                A second readout compares two-founder offspring to receptive
 *                sire-capable founders (tuned on seeds 1
 *                to 8, AZ_TEST_TUNE; reported on seeds 201 to 208; a seed
 *                with fewer than 30 such children in either world is left
 *                out and counted)
 *
 * Parts 1 to 5 call the three hooks directly on a serial lab world whose
 * creatures the test sets by hand.
 */
#include "az_test_locked_block.h"

typedef struct Bench {
    AzHost host;
    void *block;
    AzArena arena;
    AzWorld *w;
    u64 m0;
    i64 e0;
    u32 mates, mates_bound, bonds_formed, bonds_broken, births;
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
        switch (ev[i].kind) {
        case AZ_EV_MATE: b->mates++; b->mates_bound += ev[i].aux & 1u; break;
        case AZ_EV_BOND: if (ev[i].aux & 1u) b->bonds_formed++; else b->bonds_broken++; break;
        case AZ_EV_BIRTH: b->births++; continue;
        default: continue;
        }
        b->last = ev[i];
    }
}

static bool bench_open(Bench *b, const AzConfig *cfg)
{
    memset(b, 0, sizeof *b);
    b->host.ctx = b; b->host.n_workers = 1;
    b->host.parallel_for = bench_pfor; b->host.now_ns = bench_now;
    b->host.sink = bench_sink; b->host.sink_ctx = b;
    const u64 bytes = az_world_bytes(cfg);
    b->block = az_test_locked_block(bytes);
    if (!b->block) return false;
    b->arena.base = (u8 *)b->block; b->arena.size = bytes; b->arena.used = 0;
    b->w = az_world_create(&b->arena, cfg, &b->host);
    if (!b->w) return false;
    az_worldgen(b->w);
    return true;
}

static void bench_close(Bench *b)
{
    if (b->block) az_test_locked_block_free(b->block);
    memset(b, 0, sizeof *b);
}

static void books(Bench *b) { b->m0 = az_total_matter(b->w); b->e0 = az_total_energy(b->w); }
static bool exact(const Bench *b) { return az_total_matter(b->w) == b->m0 && az_total_energy(b->w) == b->e0; }
static void drain(Bench *b) { az_tally_merge(b->w); az_ledger_flush(b->w); }

static u32 P(const AzWorld *w, u32 id) { return w->param[id]; }
static u8 *traits(const AzWorld *w, u32 s) { return &w->trait[(size_t)s * AZ_TRAITS]; }
static u8 *offer(const AzWorld *w, u32 s) { return &w->offer[(size_t)s * AZ_OFFER]; }
static AzPub *pub(const AzWorld *w, u32 s) { return &w->pub[w->spos[s]]; }
static AzPubId *pub_id(const AzWorld *w, u32 s) { return &w->pub_id[w->spos[s]]; }

/* stores to breed from, no standing request */
static void restock(AzWorld *w, u32 s)
{
    w->flags[s] = AZ_F_ALIVE | AZ_F_RECEPT;
    w->pool_a[s] = 5000; w->pool_t[s] = 0; w->energy[s] = 100000;
    w->pair[s].req = 0;
    w->birth[s].n = 0;
}

/* A founder remade as a ripe adult of the kind and mating type given, with no
 * trait at all, unbonded, unrelated, of middling size and rank, displaying
 * nothing. The test then gives it what the case is about. */
static void make(AzWorld *w, u32 s, u32 mode, u32 sex)
{
    memset(traits(w, s), 0, AZ_TRAITS);
    w->phen[(size_t)s * AZ_PHEN + AZ_P_MATE] = (u8)(mode | (sex << 3));
    restock(w, s);
    w->bond[s] = 0; w->partner[s] = 0;
    w->colony[s] = 0; w->role[s] = AZ_ROLE_NONE;
    w->tag[s] = 0x4040;
    u8 *o = offer(w, s);
    memset(o, 0, AZ_OFFER);
    o[AZ_O_DISP] = (u8)(AZ_OD_RECEPTIVE | AZ_OD_BREEDER | (sex ? AZ_OD_SEX : 0));
    o[AZ_O_RANK] = 128;
    AzPub *pb = pub(w, s);
    pb->rf = (u8)((8u << 4) | AZ_PF_RECEPT); pb->tag = 0x4040; pb->sig_a = pb->sig_b = 0;
    AzPubId *id = pub_id(w, s);
    id->uid = w->uid[s]; id->parent_a = id->parent_b = 0; id->partner = 0;
}

static i32 score(const AzWorld *w, u32 r, u32 c) { return az_mate_score(w, r, c, AZ_ACT_MATE); }

/* both sides carry out the pair, a the initiator; returns the slot that bears, AZ_NONE if none */
static u32 mate(AzWorld *w, u32 a, u32 b)
{
    az_mate_resolve(w, a, b, AZ_ACT_MATE, true, 0);
    az_mate_resolve(w, b, a, AZ_ACT_MATE, false, 0);
    const bool ba = w->birth[a].n != 0, bb = w->birth[b].n != 0;
    CHECK(!(ba && bb));
    if (ba) CHECK_EQ(w->birth[a].father, b);
    if (bb) CHECK_EQ(w->birth[b].father, a);
    return ba ? a : bb ? b : AZ_NONE;
}

/* a bench after one whole tick, so that every founder has its public record; the slots the test may use */
static bool open_ticked(Bench *b, u64 seed, u32 *slot, u32 want)
{
    AzConfig cfg = az_test_config(seed, 64);
    if (!bench_open(b, &cfg)) return false;
    az_tick(b->w);
    u32 n = 0;
    for (u32 s = 0; s < b->w->n_slots && n < want; s++)
        if ((b->w->flags[s] & AZ_F_ALIVE) && b->w->spos[s] != AZ_NONE && b->w->blk[s]) slot[n++] = s;
    return n == want;
}

enum { BEAR = 1, SIRE = 2 };
static u32 can(u32 mode, u32 sex)
{
    if (mode < AZ_MM_HERMA) return 0;
    if (mode == AZ_MM_HERMA) return BEAR | SIRE;
    return sex ? SIRE : BEAR;
}

/* ---- 1. kinds ---- */
static void test_kinds(void)
{
    Bench b;
    u32 sl[2];
    if (!open_ticked(&b, 3, sl, 2)) { CHECK(!"bench"); return; }
    AzWorld *w = b.w;
    const u32 r = sl[0], c = sl[1];
    u32 pairs = 0;
    for (u32 k = 0; k < 256u; k++) {
        const u32 mr = k & 7u, sr = (k >> 3) & 1u, mc = (k >> 4) & 7u, sc = (k >> 7) & 1u;
        make(w, r, mr, sr); make(w, c, mc, sc);
        const u32 cr = can(mr, sr), cc = can(mc, sc);
        const bool r_bears = (cr & BEAR) && (cc & SIRE), c_bears = (cc & BEAR) && (cr & SIRE);
        CHECK_EQ(score(w, r, c) >= 0, r_bears || c_bears);
        CHECK_EQ(score(w, c, r) >= 0, r_bears || c_bears);
        const u32 who = mate(w, c, r);
        CHECK_EQ(who != AZ_NONE, r_bears || c_bears);
        if (r_bears != c_bears) CHECK_EQ(who, r_bears ? r : c);
        pairs += who != AZ_NONE;
        w->tick++;
    }
    /* a pair is not ripe, or not of the breeding caste: no */
    make(w, r, AZ_MM_PROMISC, 0); make(w, c, AZ_MM_PROMISC, 1);
    CHECK(score(w, r, c) >= 0);
    offer(w, r)[AZ_O_DISP] &= (u8)~AZ_OD_RECEPTIVE;
    CHECK(score(w, r, c) < 0);
    make(w, r, AZ_MM_PROMISC, 0);
    offer(w, c)[AZ_O_DISP] &= (u8)~AZ_OD_BREEDER;
    CHECK(score(w, r, c) < 0);
    make(w, c, AZ_MM_PROMISC, 1);
    CHECK_EQ(az_mate_score(w, r, c, AZ_ACT_SHARE), -1);                  /* not this module's act */

    /* two hermaphrodites: each ready to bear by its own SEX_BIAS */
    static const u8 bias[4][2] = { { 255, 0 }, { 0, 255 }, { 100, 100 }, { 0, 0 } };
    u32 bore[4];
    for (u32 i = 0; i < 4u; i++) {
        bore[i] = 0;
        make(w, r, AZ_MM_HERMA, 0); make(w, c, AZ_MM_HERMA, 1);
        traits(w, r)[AZ_T_SEX_BIAS] = bias[i][0]; traits(w, c)[AZ_T_SEX_BIAS] = bias[i][1];
        for (u32 k = 0; k < 2000u; k++) {
            const u32 who = mate(w, r, c);
            CHECK(who != AZ_NONE);
            bore[i] += who == r;
            restock(w, r); restock(w, c);
            w->tick++;
        }
    }
    CHECK(bore[0] >= 1970u);
    CHECK(bore[1] <= 30u);
    CHECK(bore[2] >= 850u && bore[2] <= 1150u);
    CHECK(bore[3] >= 850u && bore[3] <= 1150u);
    drain(&b);
    printf("kinds: of 256 pairs of kind and mating type %u can breed, each with the bearer the rule gives; of 2000 "
           "matings of two hermaphrodites the first bore %u, %u, %u, %u times with SEX_BIAS 255:0, 0:255, 100:100, 0:0\n",
           pairs, bore[0], bore[1], bore[2], bore[3]);
    bench_close(&b);
}

/* ---- 2 and 3. the chooser ---- */
static void test_chooser(void)
{
    Bench b;
    u32 sl[3];
    if (!open_ticked(&b, 5, sl, 3)) { CHECK(!"bench"); return; }
    AzWorld *w = b.w;
    const u32 r = sl[0], c = sl[1], d = sl[2];
    make(w, r, AZ_MM_PROMISC, 0); make(w, c, AZ_MM_PROMISC, 1); make(w, d, AZ_MM_PROMISC, 1);
    u8 *tr = traits(w, r), *tc = traits(w, c), *td = traits(w, d);

    /* no preference gene: everybody is the same to her */
    tc[AZ_T_ORNAMENT] = 200; offer(w, c)[AZ_O_ORNAMENT] = 254;
    CHECK_EQ(score(w, r, c), 0); CHECK_EQ(score(w, r, d), 0);

    /* PREF_SIGNAL: ORNAMENT as far as it is displayed, multiplied by SHEEN, plus emitted signals */
    tr[AZ_T_PREF_SIGNAL] = 255;
    const i32 showy = score(w, r, c);
    CHECK_EQ(showy, (200 * 254) >> 8);
    td[AZ_T_ORNAMENT] = 20; offer(w, d)[AZ_O_ORNAMENT] = 254;
    const i32 plain = score(w, r, d);
    CHECK(showy > plain && plain > 0);
    offer(w, c)[AZ_O_ORNAMENT] = 60;                      /* the same ornament, shown less */
    CHECK(score(w, r, c) < showy && score(w, r, c) > 0);
    offer(w, c)[AZ_O_ORNAMENT] = 0;
    CHECK_EQ(score(w, r, c), 0);
    offer(w, c)[AZ_O_ORNAMENT] = 254;
    tc[AZ_T_SHEEN] = 255;
    const i32 sheen = score(w, r, c);
    CHECK_EQ(sheen, (((200 * (256 + 255)) >> 8) * 254) >> 8);
    w->param[AZ_WP_MATE_SHEEN_GAIN] = 0;
    CHECK_EQ(score(w, r, c), showy);
    w->param[AZ_WP_MATE_SHEEN_GAIN] = 128;
    tc[AZ_T_SHEEN] = 0;
    pub(w, d)->sig_a = 200; pub(w, d)->sig_b = 120;       /* a loud one */
    CHECK_EQ(score(w, r, d), plain + 80);
    w->param[AZ_WP_MATE_SHOW_GAIN] = 0;                   /* a world in which no display carries */
    CHECK_EQ(score(w, r, c), 0); CHECK_EQ(score(w, r, d), 0);
    w->param[AZ_WP_MATE_SHOW_GAIN] = 512;
    CHECK_EQ(score(w, r, c), 2 * showy);
    w->param[AZ_WP_MATE_SHOW_GAIN] = 256;
    pub(w, d)->sig_a = pub(w, d)->sig_b = 0;

    /* PREF_SIZE */
    tr[AZ_T_PREF_SIGNAL] = 0; tr[AZ_T_PREF_SIZE] = 255;
    pub(w, c)->rf = (u8)((15u << 4) | AZ_PF_RECEPT); pub(w, d)->rf = (u8)((2u << 4) | AZ_PF_RECEPT);
    CHECK_EQ(score(w, r, c), 255); CHECK_EQ(score(w, r, d), 34);

    /* PREF_SIMILAR: PATTERN (the low byte of the tag) and SHAPE_M; kin are most alike */
    tr[AZ_T_PREF_SIZE] = 0; tr[AZ_T_PREF_SIMILAR] = 255;
    CHECK_EQ(score(w, r, c), 255);
    pub(w, c)->tag = (u16)(0x4000 | (0x40 + 100));
    CHECK_EQ(score(w, r, c), 255 - 50);
    pub(w, c)->tag = (u16)(0xFF00 | 0x40);                /* another hue is not another pattern */
    CHECK_EQ(score(w, r, c), 255);
    tc[AZ_T_SHAPE_M] = 200;
    CHECK_EQ(score(w, r, c), 255 - 100);
    tr[AZ_T_SHAPE_M] = 200;
    CHECK_EQ(score(w, r, c), 255);
    tr[AZ_T_SHAPE_M] = 0;
    pub_id(w, c)->parent_a = 777; pub_id(w, r)->parent_b = 777;          /* half siblings */
    CHECK_EQ(score(w, r, c), 255);
    pub_id(w, c)->parent_a = 0; pub_id(w, r)->parent_b = pub_id(w, c)->uid;   /* her own parent */
    CHECK_EQ(score(w, r, c), 255);
    pub_id(w, r)->parent_b = 0;
    CHECK_EQ(score(w, r, c), 255 - 100);
    tc[AZ_T_SHAPE_M] = 0;

    /* PREF_RANK */
    tr[AZ_T_PREF_SIMILAR] = 0; tr[AZ_T_PREF_RANK] = 255;
    offer(w, c)[AZ_O_RANK] = 200; offer(w, d)[AZ_O_RANK] = 50;
    CHECK_EQ(score(w, r, c), 200); CHECK_EQ(score(w, r, d), 50);

    /* all four: the mean by her own weights */
    tr[AZ_T_PREF_SIGNAL] = 100; tr[AZ_T_PREF_SIZE] = 50; tr[AZ_T_PREF_SIMILAR] = 30; tr[AZ_T_PREF_RANK] = 20;
    CHECK_EQ(score(w, r, c), (100 * showy + 50 * 255 + 30 * 255 + 20 * 200) / 200);

    /* 3. CHOOSY: below her own threshold she refuses; the unit is a parameter */
    memset(tr, 0, AZ_TRAITS);
    tr[AZ_T_PREF_SIZE] = 255;
    pub(w, c)->rf = (u8)((8u << 4) | AZ_PF_RECEPT); pub(w, d)->rf = (u8)((7u << 4) | AZ_PF_RECEPT);
    CHECK_EQ(score(w, r, c), 136); CHECK_EQ(score(w, r, d), 119);
    tr[AZ_T_CHOOSY] = 255;                                /* asks 127 */
    CHECK_EQ(score(w, r, c), 136); CHECK_EQ(score(w, r, d), -1);
    tr[AZ_T_CHOOSY] = 200;                                /* asks 100 */
    CHECK_EQ(score(w, r, d), 119);
    w->param[AZ_WP_MATE_CHOOSY_SCALE] = 300;
    CHECK_EQ(score(w, r, c), -1);
    w->param[AZ_WP_MATE_CHOOSY_SCALE] = 0;
    tr[AZ_T_CHOOSY] = 255;
    CHECK_EQ(score(w, r, d), 119);
    w->param[AZ_WP_MATE_CHOOSY_SCALE] = 128;
    tr[AZ_T_PREF_SIZE] = 0;                               /* choosy with no preference: nothing to choose by */
    CHECK_EQ(score(w, r, d), 0);
    printf("chooser: a full display of ORNAMENT 200 scores %d (with SHEEN 255: %d), ORNAMENT 20 scores %d; size, "
           "likeness, rank and the weighted mean as the rule gives; CHOOSY 255 takes quality 136 and refuses 119\n",
           showy, sheen, plain);
    bench_close(&b);
}

/* ---- 4. bonds ---- */
static void test_bonds(void)
{
    Bench b;
    u32 sl[3];
    if (!open_ticked(&b, 7, sl, 3)) { CHECK(!"bench"); return; }
    AzWorld *w = b.w;
    const u32 f = sl[0], m = sl[1], x = sl[2];
    /* formed with a chance of BOND / 256, by both in a monogamous kind */
    static const u8 bond[3] = { 0, 128, 255 };
    u32 formed_f[3], formed_m[3];
    for (u32 i = 0; i < 3u; i++) {
        formed_f[i] = formed_m[i] = 0;
        for (u32 k = 0; k < 512u; k++) {
            make(w, f, AZ_MM_MONOG, 0); make(w, m, AZ_MM_MONOG, 1);
            traits(w, f)[AZ_T_BOND] = bond[i]; traits(w, m)[AZ_T_BOND] = bond[i];
            CHECK_EQ(mate(w, m, f), f);
            formed_f[i] += w->bond[f] != 0 && w->partner[f] == w->uid[m];
            formed_m[i] += w->bond[m] != 0 && w->partner[m] == w->uid[f];
            w->tick++;
        }
    }
    CHECK(formed_f[0] == 0 && formed_m[0] == 0);
    CHECK(formed_f[1] >= 200u && formed_f[1] <= 312u && formed_m[1] >= 200u && formed_m[1] <= 312u);
    CHECK(formed_f[2] >= 500u && formed_m[2] >= 500u);
    drain(&b);
    CHECK(b.mates == 3u * 512u && b.mates_bound == formed_f[0] + formed_f[1] + formed_f[2]);
    CHECK_EQ(b.bonds_formed, formed_m[0] + formed_m[1] + formed_m[2]);   /* the siring initiator's own event */
    CHECK(b.last.kind != 0);

    /* which kinds pair up */
    static const struct { u8 mode, bearer_binds, sire_binds; } kind[] = {
        { AZ_MM_HERMA, 1, 1 }, { AZ_MM_PROMISC, 0, 0 }, { AZ_MM_POLYGYN, 1, 0 }, { AZ_MM_MONOG, 1, 1 },
        { AZ_MM_LEK, 0, 0 }, { AZ_MM_EUSOC, 1, 1 } };
    for (u32 i = 0; i < sizeof kind / sizeof kind[0]; i++) {
        u32 nb = 0, ns = 0;
        for (u32 k = 0; k < 64u; k++) {
            make(w, f, kind[i].mode, 0); make(w, m, kind[i].mode, 1);
            traits(w, f)[AZ_T_BOND] = 255; traits(w, m)[AZ_T_BOND] = 255;
            const u32 who = mate(w, m, f);
            CHECK(who != AZ_NONE);
            const u32 other = who == f ? m : f;
            nb += w->bond[who] != 0; ns += w->bond[other] != 0;
            w->tick++;
        }
        CHECK_EQ(nb > 0, kind[i].bearer_binds);
        CHECK_EQ(ns > 0, kind[i].sire_binds);
    }

    /* the partner strengthens the bond, a mating outside wears it */
    make(w, f, AZ_MM_MONOG, 0); make(w, m, AZ_MM_MONOG, 1); make(w, x, AZ_MM_MONOG, 1);
    traits(w, f)[AZ_T_BOND] = 255;
    u32 guard = 0;
    while (!w->bond[f] && guard++ < 64u) { (void)mate(w, m, f); restock(w, f); restock(w, m); w->tick++; }
    CHECK_EQ(w->bond[f], 64);                             /* 1 + 255 * 64 / 256 */
    static const u8 grows[3] = { 128, 192, 255 };
    for (u32 i = 0; i < 3u; i++) {
        (void)mate(w, m, f); restock(w, f); restock(w, m); w->tick++;
        CHECK_EQ(w->bond[f], grows[i]);
    }
    w->param[AZ_WP_MATE_BOND_STEP] = 0;
    w->bond[f] = 10;
    (void)mate(w, m, f); restock(w, f); restock(w, m); w->tick++;
    CHECK_EQ(w->bond[f], 11);
    w->param[AZ_WP_MATE_BOND_STEP] = 64;
    traits(w, f)[AZ_T_BOND] = 0;                          /* she takes no new partner */
    w->bond[f] = 200;
    static const u8 wears[8] = { 100, 50, 25, 12, 6, 3, 1, 0 };
    for (u32 i = 0; i < 8u; i++) {
        (void)mate(w, x, f); restock(w, f); restock(w, x); w->tick++;
        CHECK_EQ(w->bond[f], wears[i]);
        CHECK_EQ(w->partner[f], wears[i] ? w->uid[m] : 0u);
    }

    /* FIDELITY: with her partner beside her she refuses others; a widow does not */
    static const u8 fid[3] = { 0, 128, 255 };
    u32 refused[3], widow = 0;
    for (u32 i = 0; i < 3u; i++) {
        refused[i] = 0;
        make(w, f, AZ_MM_MONOG, 0); make(w, x, AZ_MM_MONOG, 1);
        traits(w, f)[AZ_T_FIDELITY] = fid[i];
        w->bond[f] = 100; w->partner[f] = w->uid[m];
        for (u32 k = 0; k < 2000u; k++) {
            offer(w, f)[AZ_O_DISP] |= AZ_OD_BONDED;
            refused[i] += score(w, f, x) < 0;
            if (i == 2u) { offer(w, f)[AZ_O_DISP] &= (u8)~AZ_OD_BONDED; widow += score(w, f, x) < 0; }
            w->tick++;
        }
    }
    CHECK_EQ(refused[0], 0);
    CHECK(refused[1] >= 850u && refused[1] <= 1150u);
    CHECK(refused[2] >= 1960u);
    CHECK_EQ(widow, 0);

    /* GUARD: the partner is taken without a second look, and its guarding outbids a better rival */
    make(w, f, AZ_MM_MONOG, 0); make(w, m, AZ_MM_MONOG, 1); make(w, x, AZ_MM_MONOG, 1);
    traits(w, f)[AZ_T_PREF_SIZE] = 255; traits(w, f)[AZ_T_CHOOSY] = 255;
    pub(w, m)->rf = (u8)((2u << 4) | AZ_PF_RECEPT); pub(w, x)->rf = (u8)((15u << 4) | AZ_PF_RECEPT);
    CHECK_EQ(score(w, f, m), -1);                         /* a stranger of quality 34: below her threshold */
    w->bond[f] = 50; w->partner[f] = w->uid[m];
    CHECK_EQ(score(w, f, m), 35);                         /* her partner */
    CHECK(score(w, f, x) > score(w, f, m));               /* an unguarded partner loses to the better rival */
    traits(w, m)[AZ_T_GUARD] = 255;
    CHECK_EQ(score(w, f, m), 35 + 255);
    CHECK(score(w, f, m) > score(w, f, x));
    w->param[AZ_WP_MATE_GUARD_GAIN] = 0;
    CHECK_EQ(score(w, f, m), 35);
    w->param[AZ_WP_MATE_GUARD_GAIN] = 16;

    /* the siring side pays for its display as heat; the books stay exact */
    make(w, f, AZ_MM_PROMISC, 0); make(w, m, AZ_MM_PROMISC, 1);
    offer(w, m)[AZ_O_ORNAMENT] = 254;
    books(&b);
    CHECK_EQ(mate(w, m, f), f);
    CHECK_EQ(w->energy[m], 100000 - ((254 * 16) >> 8));
    az_life_commit(w);
    drain(&b);
    CHECK(exact(&b));
    make(w, m, AZ_MM_PROMISC, 1);
    offer(w, m)[AZ_O_ORNAMENT] = 254;
    w->energy[m] = 3;                                     /* it cannot pay more than it has */
    restock(w, f); w->energy[m] = 3;
    books(&b);
    (void)mate(w, m, f);
    CHECK_EQ(w->energy[m], 0);
    az_life_commit(w);
    drain(&b);
    CHECK(exact(&b));
    printf("bonds: of 512 matings %u, %u, %u bound the bearer with BOND 0, 128, 255 (the sire %u, %u, %u); strength "
           "64, 128, 192, 255 with the partner, 100, 50 ... 0 outside; with the partner beside her she refused %u, %u, "
           "%u of 2000 others with FIDELITY 0, 128, 255, a widow none\n",
           formed_f[0], formed_f[1], formed_f[2], formed_m[0], formed_m[1], formed_m[2], refused[0], refused[1],
           refused[2]);
    bench_close(&b);
}

/* ---- 5. intent ---- */
static void clear_pair(AzWorld *w, u32 s)
{
    AzPair *p = &w->pair[s];
    p->claim = 0; p->peer = AZ_NONE; p->act = AZ_ACT_NONE; p->urge = 0;
    memset(offer(w, s), 0, AZ_OFFER);
}

static void test_intent(void)
{
    Bench b;
    u32 sl[40];
    if (!open_ticked(&b, 9, sl, 40)) { CHECK(!"bench"); return; }
    AzWorld *w = b.w;
    /* a responder and an initiator of this tick among the others */
    u32 s = sl[0], resp = AZ_NONE, init = AZ_NONE, resp2 = AZ_NONE;
    for (u32 i = 1; i < 40u; i++) {
        if (az_is_initiator(w, w->uid[sl[i]])) { if (init == AZ_NONE) init = sl[i]; }
        else if (resp == AZ_NONE) resp = sl[i];
        else if (resp2 == AZ_NONE) resp2 = sl[i];
    }
    if (resp == AZ_NONE || init == AZ_NONE || resp2 == AZ_NONE) { CHECK(!"roles"); return; }
    make(w, s, AZ_MM_PROMISC, 1); make(w, resp, AZ_MM_PROMISC, 0); make(w, init, AZ_MM_PROMISC, 0);
    make(w, resp2, AZ_MM_PROMISC, 0);
    AzPercept p;
    memset(&p, 0, sizeof p);
    p.best_mate = resp; p.bond = p.parent = p.child = AZ_NONE;
    i8 *drive = &w->motor[(size_t)s * AZ_MOT + AZ_M_REPRO];

    /* the display and the bits */
    w->flags[s] = AZ_F_ALIVE | AZ_F_RECEPT | AZ_F_INIT;
    *drive = 60;
    clear_pair(w, s);
    az_mate_intent(w, s, &p, 0);
    CHECK_EQ(offer(w, s)[AZ_O_ORNAMENT], 120);
    CHECK_EQ(offer(w, s)[AZ_O_DISP], AZ_OD_RECEPTIVE | AZ_OD_SEX | AZ_OD_BREEDER);
    CHECK(w->pair[s].act == AZ_ACT_MATE && w->pair[s].peer == resp && w->pair[s].urge == 60);
    *drive = -5;
    clear_pair(w, s);
    az_mate_intent(w, s, &p, 0);
    CHECK_EQ(offer(w, s)[AZ_O_ORNAMENT], 0);
    CHECK_EQ(w->pair[s].act, AZ_ACT_NONE);
    *drive = 60;
    static const u8 passive[3] = { AZ_F_EGG, AZ_F_JUV, AZ_F_DORMANT };
    for (u32 i = 0; i < 3u; i++) {
        w->flags[s] = (u8)(AZ_F_ALIVE | AZ_F_RECEPT | AZ_F_INIT | passive[i]);
        clear_pair(w, s);
        az_mate_intent(w, s, &p, 0);
        CHECK(offer(w, s)[AZ_O_ORNAMENT] == 0 && !(offer(w, s)[AZ_O_DISP] & AZ_OD_RECEPTIVE));
        CHECK_EQ(w->pair[s].act, AZ_ACT_NONE);
    }
    /* who does not propose */
    w->flags[s] = AZ_F_ALIVE | AZ_F_RECEPT;                               /* a responder this tick */
    clear_pair(w, s); az_mate_intent(w, s, &p, 0);
    CHECK_EQ(w->pair[s].act, AZ_ACT_NONE);
    w->flags[s] = AZ_F_ALIVE | AZ_F_INIT;                                 /* not ripe */
    clear_pair(w, s); az_mate_intent(w, s, &p, 0);
    CHECK_EQ(w->pair[s].act, AZ_ACT_NONE);
    w->flags[s] = AZ_F_ALIVE | AZ_F_RECEPT | AZ_F_INIT;
    for (u32 mode = AZ_MM_BUD; mode <= AZ_MM_EUSOC; mode++) {             /* the kinds that breed alone */
        w->phen[(size_t)s * AZ_PHEN + AZ_P_MATE] = (u8)(mode | 8u);
        clear_pair(w, s); az_mate_intent(w, s, &p, 0);
        CHECK_EQ(w->pair[s].act == AZ_ACT_MATE, mode >= AZ_MM_HERMA);
    }
    /* the eusocial kind: in a colony only the breeder caste */
    w->colony[s] = 5;
    clear_pair(w, s); az_mate_intent(w, s, &p, 0);
    CHECK(w->pair[s].act == AZ_ACT_NONE && !(offer(w, s)[AZ_O_DISP] & AZ_OD_BREEDER));
    w->role[s] = AZ_ROLE_BREEDER;
    clear_pair(w, s); az_mate_intent(w, s, &p, 0);
    CHECK(w->pair[s].act == AZ_ACT_MATE && (offer(w, s)[AZ_O_DISP] & AZ_OD_BREEDER));
    w->colony[s] = 0; w->role[s] = AZ_ROLE_NONE;
    w->phen[(size_t)s * AZ_PHEN + AZ_P_MATE] = (u8)(AZ_MM_MONOG | 8u);
    /* nobody to court, or the neighbour is an initiator too */
    p.best_mate = AZ_NONE;
    clear_pair(w, s); az_mate_intent(w, s, &p, 0);
    CHECK_EQ(w->pair[s].act, AZ_ACT_NONE);
    p.best_mate = init;
    clear_pair(w, s); az_mate_intent(w, s, &p, 0);
    CHECK_EQ(w->pair[s].act, AZ_ACT_NONE);
    p.best_mate = resp;
    /* another module's stronger proposal stands, a weaker one gives way */
    clear_pair(w, s);
    w->pair[s].act = AZ_ACT_SHARE; w->pair[s].peer = resp2; w->pair[s].urge = 100;
    az_mate_intent(w, s, &p, 0);
    CHECK(w->pair[s].act == AZ_ACT_SHARE && w->pair[s].peer == resp2 && w->pair[s].urge == 100);
    clear_pair(w, s);
    w->pair[s].act = AZ_ACT_SHARE; w->pair[s].peer = resp2; w->pair[s].urge = 10;
    az_mate_intent(w, s, &p, 0);
    CHECK(w->pair[s].act == AZ_ACT_MATE && w->pair[s].peer == resp && w->pair[s].urge == 60);
    /* bonded: the partner beside it, if ripe, before the nearest; with an unripe partner it mostly stays */
    w->bond[s] = 255; w->partner[s] = w->uid[resp2];
    p.bond = resp2;
    clear_pair(w, s); az_mate_intent(w, s, &p, 0);
    CHECK(w->pair[s].act == AZ_ACT_MATE && w->pair[s].peer == resp2);
    CHECK(offer(w, s)[AZ_O_DISP] & AZ_OD_BONDED);
    pub(w, resp2)->rf = (u8)(8u << 4);                                    /* the partner is not ripe */
    u32 strays[2] = { 0, 0 };
    for (u32 k = 0; k < 2u; k++) {
        w->bond[s] = k ? 64 : 255;
        for (u32 i = 0; i < 1000u; i++) {
            clear_pair(w, s); az_mate_intent(w, s, &p, 0);
            if (w->pair[s].act == AZ_ACT_MATE) { strays[k]++; CHECK_EQ(w->pair[s].peer, resp); }
            w->tick++;
            if (az_is_initiator(w, w->uid[resp])) i--;                    /* only ticks in which she can be asked count */
        }
    }
    CHECK(strays[0] <= 15u);
    CHECK(strays[1] >= 650u && strays[1] <= 850u);
    printf("intent: display twice the drive, bits as the rule gives; a bonded suitor whose partner is beside it "
           "but not ripe courts another in %u of 1000 ticks with a bond of 255 and in %u with a bond of 64\n",
           strays[0], strays[1]);
    bench_close(&b);
}

/* ---- 6. worlds ---- */
typedef struct Run { u64 hash, births; u32 live, sexual, bonded; int ok; } Run;

/* a third budders, a third hermaphrodites, a third of two sexes (the children are what their genes say) */
static void three_kinds(AzWorld *w)
{
    static const u8 kind[3] = { AZ_MM_BUD, AZ_MM_HERMA, AZ_MM_PROMISC };
    for (u32 s = 0; s < w->n_slots; s++)
        if (w->flags[s] & AZ_F_ALIVE) {
            u8 *pm = &w->phen[(size_t)s * AZ_PHEN + AZ_P_MATE];
            *pm = (u8)((*pm & ~7u) | kind[s % 3u]);
        }
}

static Run world_run(u32 threads, u8 reorder, u32 lesion)
{
    Run r;
    memset(&r, 0, sizeof r);
    AzConfig cfg = az_test_config(9, 2000);
    cfg.reorder_period = reorder;
    cfg.cap = 40000;
    cfg.lesion = lesion;
    AzTestWorld t;
    if (!az_test_world(&t, &cfg, threads)) { CHECK(!"world"); return r; }
    AzWorld *w = t.w;
    three_kinds(w);
    const u64 m0 = az_total_matter(w);
    const i64 e0 = az_total_energy(w);
    r.ok = 1;
    for (u32 i = 0; i < 150u; i++) {
        az_tick(w);
        if (az_total_matter(w) != m0 || az_total_energy(w) != e0) r.ok = 0;
    }
    for (u32 s = 0; s < w->n_slots; s++) {
        if (!(w->flags[s] & AZ_F_ALIVE)) continue;
        r.sexual += w->parent_b[s] != 0;
        r.bonded += w->bond[s] != 0;
    }
    r.hash = az_state_hash(w); r.births = w->totals.births; r.live = w->n_live;
    az_test_world_free(&t);
    return r;
}

static void test_worlds(void)
{
    const Run a = world_run(0, 1, 0), b = world_run(1, 9, 0), c = world_run(3, 1, 0), d = world_run(8, 9, 0);
    CHECK(a.ok && b.ok && c.ok && d.ok);
    CHECK(a.sexual > 50u);
    CHECK_EQ(a.hash, b.hash); CHECK_EQ(a.hash, c.hash); CHECK_EQ(a.hash, d.hash);
    CHECK(a.sexual == d.sexual && a.births == d.births);
    const Run l = world_run(0, 1, AZ_LESION_MATING), l8 = world_run(8, 9, AZ_LESION_MATING);
    CHECK(l.ok && l8.ok);
    CHECK_EQ(l.sexual, 0);                               /* the outcrossers stop */
    CHECK(l.births > 500u);                              /* budding and selfing go on */
    CHECK_EQ(l.hash, l8.hash);
    CHECK(l.hash != a.hash);
    printf("worlds: 2000 founders, 150 ticks: %" PRIu64 " births, %u of the %u alive have two parents, %u are "
           "bonded, hash %016" PRIx64 " on 0, 1, 3 and 8 workers; with the mating lesion %" PRIu64 " births, "
           "none with two parents\n", a.births, a.sexual, a.live, a.bonded, a.hash, l.births);
}

/* ---- 7. the signal ---- */
typedef struct Sig {
    double kids, adults;         /* historical acceptance metric */
    u32 n_kids;
    double end_mean;
    u32 live;
    double f1_ornament, sire_mean;
    u32 f1_kids;
    u64 excluded_sire_ticks;
} Sig;

/* All founders in this assay are set to two-sex PROMISC before its first tick. */
static bool signal_founder_sire(const AzWorld *w, u32 s)
{
    const u8 stage = AZ_F_ALIVE | AZ_F_RECEPT | AZ_F_EGG | AZ_F_JUV | AZ_F_DORMANT;
    return (w->flags[s] & stage) == (AZ_F_ALIVE | AZ_F_RECEPT) &&
        w->parent_a[s] == 0 && (w->phen[(size_t)s * AZ_PHEN + AZ_P_MATE] & 8u);
}

/* The next unused UID at the end of worldgen separates founders from descendants.
 * generation[] follows the bearing parent alone. */
static bool signal_founder_cross(const AzWorld *w, u32 s, u64 founder_limit)
{
    return w->parent_a[s] != 0 && w->parent_a[s] < founder_limit &&
        w->parent_b[s] != 0 && w->parent_b[s] < founder_limit;
}

static void test_signal_population(void)
{
    AzConfig cfg = az_test_config(7, 3);
    AzTestWorld t;
    if (!az_test_world(&t, &cfg, 0)) { CHECK(!"signal population world"); return; }
    AzWorld *w = t.w;
    const u64 founder_limit = w->uid_next;
    az_grid_build(w);
    make(w, 0, AZ_MM_PROMISC, 0);
    make(w, 1, AZ_MM_PROMISC, 1);
    make(w, 2, AZ_MM_PROMISC, 1);
    w->flags[2] &= (u8)~AZ_F_RECEPT;
    CHECK(!signal_founder_sire(w, 0));  /* bearing founder */
    CHECK(signal_founder_sire(w, 1));
    CHECK(!signal_founder_sire(w, 2));  /* unreceptive founder */

    /* A new slot's recorded parents stay meaningful after the parent's
     * slot is reused. A founder mother alone does not make a two-founder cross. */
    w->generation[2] = 1;
    w->parent_a[2] = w->uid[0];
    w->parent_b[2] = w->uid[1];
    CHECK(signal_founder_cross(w, 2, founder_limit));
    w->parent_b[2] = founder_limit;
    CHECK(!signal_founder_cross(w, 2, founder_limit));
    w->parent_b[2] = 0;
    CHECK(!signal_founder_cross(w, 2, founder_limit));
    az_test_world_free(&t);
}

static Sig signal_run(u64 seed, u32 pref)
{
    Sig r;
    memset(&r, 0, sizeof r);
    AzConfig cfg = az_test_config(seed, 3000);
    cfg.cap = 40000;
    AzTestWorld t;
    if (!az_test_world(&t, &cfg, 8)) { CHECK(!"world"); return r; }
    AzWorld *w = t.w;
    const u64 founder_limit = w->uid_next;
    for (u32 s = 0; s < w->n_slots; s++)                  /* every founder of two sexes; choosing by display or not */
        if (w->flags[s] & AZ_F_ALIVE) {
            u8 *pm = &w->phen[(size_t)s * AZ_PHEN + AZ_P_MATE];
            *pm = (u8)((*pm & ~7u) | AZ_MM_PROMISC);
            u8 *tr = traits(w, s);
            tr[AZ_T_PREF_SIGNAL] = (u8)pref; tr[AZ_T_PREF_SIZE] = 0; tr[AZ_T_PREF_SIMILAR] = 0; tr[AZ_T_PREF_RANK] = 0;
            tr[AZ_T_CHOOSY] = 64;
        }
    for (u32 i = 0; i < 200u; i++) {
        /* Preserve the original all-active-founder comparator as its own gate. */
        u64 sum = 0, sire_sum = 0;
        u32 n = 0, sires = 0;
        for (u32 s = 0; s < w->n_slots; s++)
            if ((w->flags[s] & (AZ_F_ALIVE | AZ_F_EGG | AZ_F_JUV | AZ_F_DORMANT)) == AZ_F_ALIVE && w->parent_a[s] == 0) {
                sum += traits(w, s)[AZ_T_ORNAMENT]; n++;
                if (signal_founder_sire(w, s)) {
                    sire_sum += traits(w, s)[AZ_T_ORNAMENT];
                    sires++;
                }
            }
        const double adults = n ? (double)sum / n : 0;
        r.excluded_sire_ticks += n - sires;
        const u64 now = w->tick;
        az_tick(w);
        for (u32 s = 0; s < w->n_slots; s++)
            if ((w->flags[s] & AZ_F_ALIVE) && w->birth_tick[s] == now && w->parent_b[s] != 0 &&
                w->generation[s] == 1u) {
                r.kids += traits(w, s)[AZ_T_ORNAMENT]; r.adults += adults; r.n_kids++;
                if (signal_founder_cross(w, s, founder_limit)) {
                    CHECK(sires != 0);
                    if (sires != 0) {
                        r.f1_ornament += traits(w, s)[AZ_T_ORNAMENT];
                        r.sire_mean += (double)sire_sum / sires;
                        r.f1_kids++;
                    }
                }
            }
    }
    u64 sum = 0;
    for (u32 s = 0; s < w->n_slots; s++)
        if (w->flags[s] & AZ_F_ALIVE) { sum += traits(w, s)[AZ_T_ORNAMENT]; r.live++; }
    r.end_mean = r.live ? (double)sum / r.live : 0;
    az_test_world_free(&t);
    return r;
}

static void test_signal(void)
{
    u32 seen_up = 0, dark_up = 0, seeds = 0;
    double seen_sum = 0, dark_sum = 0;
    u32 kids_seen = 0, kids_dark = 0;
    u32 f1_up = 0, f1_seeds = 0;
    double f1_seen = 0, f1_dark = 0;
    const char *tune = getenv("AZ_TEST_TUNE");
    const u64 first = tune ? 1u : 201u;
    for (u64 seed = first; seed < first + 8u; seed++) {
        const Sig seen = signal_run(seed, 255), dark = signal_run(seed, 0);
        if (seen.f1_kids >= 30u && dark.f1_kids >= 30u) {
            const double ds = (seen.f1_ornament - seen.sire_mean) / seen.f1_kids;
            const double dd = (dark.f1_ornament - dark.sire_mean) / dark.f1_kids;
            f1_seeds++; f1_up += ds > dd;
            f1_seen += ds; f1_dark += dd;
        }
        if (getenv("AZ_TEST_VERBOSE")) printf("  seed %u: with %u kids %.2f vs adults %.2f; without %u kids %.2f vs %.2f\n", (u32)seed, seen.n_kids, seen.n_kids ? seen.kids / seen.n_kids : 0, seen.n_kids ? seen.adults / seen.n_kids : 0, dark.n_kids, dark.n_kids ? dark.kids / dark.n_kids : 0, dark.n_kids ? dark.adults / dark.n_kids : 0);
        if (seen.n_kids < 30u || dark.n_kids < 30u) continue;
        const double ds = (seen.kids - seen.adults) / seen.n_kids, dd = (dark.kids - dark.adults) / dark.n_kids;
        seeds++;
        seen_up += ds > dd; dark_up += dd > ds;
        seen_sum += ds; dark_sum += dd;
        kids_seen += seen.n_kids; kids_dark += dark.n_kids;
    }
    CHECK(seeds >= 6u);
    CHECK(seen_up > dark_up);
    CHECK(seen_sum > dark_sum);
    printf("signal: historical maternal-generation-one children against all active adult founders, mean "
           "ORNAMENT difference: %+.2f where the founders prefer display (%u children), %+.2f where they have no "
           "preference (%u); larger with the preference on %u of %u seeds (%u of 8 seeds left out: too few children)\n",
           seeds ? seen_sum / seeds : 0.0, kids_seen, seeds ? dark_sum / seeds : 0.0, kids_dark, seen_up, seeds,
           8u - seeds);
    printf("signal: two-founder offspring against receptive sire-capable founders: "
           "preference %+.2f, no preference %+.2f, preference larger in %u of %u eligible worlds\n",
           f1_seeds ? f1_seen / f1_seeds : 0.0, f1_seeds ? f1_dark / f1_seeds : 0.0, f1_up, f1_seeds);
}

int main(void)
{
    test_kinds();
    test_chooser();
    test_bonds();
    test_intent();
    test_worlds();
    test_signal_population();
    test_signal();
    (void)P;
    return az_test_done();
}
