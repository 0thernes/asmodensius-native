/* test_tier1.c - slow cognition (core/az_tier1.c, parameters core/params/az_tier1.def).
 *
 *   1. feeling    the expected reward follows the reward; arousal rises with
 *                 surprise by AROUSAL_GAIN, decays by AROUSAL_DECAY, wanders
 *                 by MOODINESS
 *   2. trace      the eligibility of a lane and its persistence, PLASTICITY
 *   3. learning   a two-armed world: the lane that pays gains bias over the
 *                 lane that does not, sooner with a higher LEARN_RATE, never
 *                 under the plasticity lesion; the metric changes the step
 *                 (lesions METRIC and TIER1_Q give the plain one)
 *   4. noise      EXPLORE, and IMPROV with the brain's own drive
 *   5. state      the state vector: Q_DEPTH, Q_COUPLE, lesion TIER1_Q; and
 *                 the coupling of the context to the bias, CTX_GAIN
 *   6. memory     storing by MEMORY, recall by HOPFIELD, long-term words
 *                 asked for, paid for as heat, recognised, given up; lesion
 *                 MEMORY
 *   7. others     ATTENTION and the neighbourhood (lesion TIER1_SH),
 *                 CONFORMITY and the colony's culture
 *   8. edges      a brain with no hidden block, an egg, a cyst
 *   9. worlds     books exact, equal hashes for 0, 1, 3 and 8 workers and
 *                 reorder 1 and 9; each of the five lesions gives another
 *                 world
 *
 * Parts 1 to 8 call az_tier1_range for one hand-set creature at its own due
 * ticks, followed by az_store_commit as in the tick.
 */
#include "az_test.h"

typedef struct Bench {
    AzTestWorld t;
    AzWorld *w;
    u64 m0;
    i64 e0;
} Bench;

static bool bench_open(Bench *b, u64 seed, u32 n, u32 lesion, bool dense)
{
    AzConfig cfg = az_test_config(seed, n);
    cfg.lesion = lesion;
    if (dense) { cfg.gx_bits = 2; cfg.gy_bits = 2; cfg.gz_bits = 1; }
    if (!az_test_world(&b->t, &cfg, 0)) return false;
    b->w = b->t.w;
    az_tick(b->w);                                        /* every founder gets its place in the grid */
    return true;
}

/* the bench on the uniform world (az_test.h): for a part whose checks assume its founders
 * (the first creature sees a crowd: its sense range is the drawn genome's) */
static bool bench_open_uniform(Bench *b, u64 seed, u32 n, u32 lesion, bool dense)
{
    AzConfig cfg = az_test_config(seed, n);
    cfg.lesion = lesion;
    if (dense) { cfg.gx_bits = 2; cfg.gy_bits = 2; cfg.gz_bits = 1; }
    if (!az_test_world_ex(&b->t, &cfg, 0, AZ_TEST_NO_WORLDGEN)) return false;
    az_test_uniform(b->t.w);
    b->w = b->t.w;
    az_tick(b->w);                                        /* every founder gets its place in the grid */
    return true;
}

static void bench_close(Bench *b) { az_test_world_free(&b->t); }
static void books(Bench *b) { b->m0 = az_total_matter(b->w); b->e0 = az_total_energy(b->w); }
static bool exact(const Bench *b) { return az_total_matter(b->w) == b->m0 && az_total_energy(b->w) == b->e0; }

static u8 *traits(const AzWorld *w, u32 s) { return &w->trait[(size_t)s * AZ_TRAITS]; }
static i8 *elig(const AzWorld *w, u32 s) { return &w->elig[(size_t)s * 8]; }
static i8 *ctx(const AzWorld *w, u32 s) { return &w->ctx[(size_t)s * 8]; }
static AzBrain *brain(const AzWorld *w, u32 s) { u32 n; return az_brain(w, s, &n); }
static u8 *mind(const AzWorld *w, u32 s) { u32 n; return (u8 *)az_seg(w, s, AZ_SEG_MIND, &n); }

static u32 visits = 0, inexact = 0;

/* the creature's next visit; the world's matter and energy must be the same after it as before */
static void visit(Bench *b, u32 s)
{
    AzWorld *w = b->w;
    const u32 per = w->cfg.tier1_period, m = (u32)((w->uid[s] + w->tick) % per);
    if (m) w->tick += per - m;
    const u64 m0 = az_total_matter(w);
    const i64 e0 = az_total_energy(w);
    az_tier1_range(w, s, s + 1u, 0);
    az_store_commit(w);
    visits++;
    inexact += az_total_matter(w) != m0 || az_total_energy(w) != e0;
    w->tick++;
}

/* every hidden unit of lane `lane` at `on`, all the others at `off` */
static void set_hid(const AzWorld *w, u32 s, u32 lane, u8 on, u8 off)
{
    u8 *hid = mind(w, s) + AZ_SENS;
    const u32 n_hb = brain(w, s)->n_hb;
    for (u32 i = 0; i < n_hb * 8u; i++) hid[i] = (i & 7u) == lane ? on : off;
}

/* an adult with no trait at all, a neutral inner state and a still mind */
static void blank(const AzWorld *w, u32 s)
{
    memset(traits(w, s), 0, AZ_TRAITS);
    w->flags[s] = AZ_F_ALIVE;
    w->colony[s] = 0;
    w->arousal[s] = 0; w->rbase[s] = 0; w->rprev[s] = w->energy[s];
    memset(elig(w, s), 0, 8); memset(ctx(w, s), 0, 8);
    memset(&w->qstate[(size_t)s * 16], 0, 32);
    w->memory[s] = 0; w->hop_n[s] = 0;
    memset(&w->engram[(size_t)s * AZ_HOP_PATTERNS], 0, 8 * AZ_HOP_PATTERNS);
    memset(&w->motor[(size_t)s * AZ_MOT], 0, AZ_MOT);
    AzBrain *br = brain(w, s);
    for (u32 l = 0; l < 8u; l++) { br->gain[l] = 64; br->bias[l] = 0; }
    memset(mind(w, s), 0, AZ_SENS);
    set_hid(w, s, 0, 40, 40);
}

static u32 pick(const AzWorld *w)
{
    for (u32 s = 0; s < w->n_slots; s++)
        if ((w->flags[s] & AZ_F_ALIVE) && w->spos[s] != AZ_NONE && w->blk[s] && brain(w, s)->n_hb > 0) return s;
    return AZ_NONE;
}

/* ---- 1. feeling ---- */
static void test_feeling(void)
{
    Bench b;
    if (!bench_open(&b, 3, 64, 0, false)) { CHECK(!"bench"); return; }
    AzWorld *w = b.w;
    const u32 s = pick(w);
    if (s == AZ_NONE) { CHECK(!"creature"); return; }
    u8 *t = traits(w, s);
    books(&b);

    /* nothing in it: nothing happens */
    blank(w, s);
    for (u32 i = 0; i < 20u; i++) visit(&b, s);
    const AzBrain *br = brain(w, s);
    u32 moved = w->arousal[s] != 0 || w->rbase[s] != 0;
    for (u32 l = 0; l < 8u; l++) moved += br->bias[l] != 0 || br->gain[l] != 64 || ctx(w, s)[l] != 0 || elig(w, s)[l] != 0;
    CHECK_EQ(moved, 0);

    /* the expected reward follows the reward */
    blank(w, s);
    for (u32 i = 0; i < 60u; i++) { w->energy[s] += 40; visit(&b, s); }
    const i32 base = w->rbase[s];
    CHECK(base >= 33 && base <= 40);
    w->param[AZ_WP_TIER1_BASE_SHIFT] = 0;
    blank(w, s);
    w->energy[s] += 40; visit(&b, s);
    CHECK_EQ(w->rbase[s], 40);
    w->param[AZ_WP_TIER1_BASE_SHIFT] = 3;

    /* arousal rises with surprise by AROUSAL_GAIN; a full surprise is TIER1_SURPRISE_EU */
    static const u8 gain[3] = { 0, 128, 255 };
    u32 rose[3];
    for (u32 i = 0; i < 3u; i++) {
        blank(w, s);
        t[AZ_T_AROUSAL_GAIN] = gain[i];
        w->energy[s] += 64; visit(&b, s);
        rose[i] = w->arousal[s];
    }
    CHECK_EQ(rose[0], 0); CHECK_EQ(rose[1], 127); CHECK_EQ(rose[2], 254);
    blank(w, s);
    t[AZ_T_AROUSAL_GAIN] = 255;
    w->energy[s] -= 16; visit(&b, s);                     /* a loss surprises as a gain does */
    CHECK_EQ(w->arousal[s], (63 * 255) >> 8);
    w->param[AZ_WP_TIER1_SURPRISE_EU] = 640;
    blank(w, s);
    t[AZ_T_AROUSAL_GAIN] = 255;
    w->energy[s] += 64; visit(&b, s);
    CHECK_EQ(w->arousal[s], (25 * 255) >> 8);
    w->param[AZ_WP_TIER1_SURPRISE_EU] = 64;

    /* and decays by AROUSAL_DECAY */
    static const u8 decay[3] = { 0, 127, 255 };
    u32 left[3];
    for (u32 i = 0; i < 3u; i++) {
        blank(w, s);
        t[AZ_T_AROUSAL_DECAY] = decay[i];
        w->arousal[s] = 200;
        visit(&b, s);
        left[i] = w->arousal[s];
    }
    CHECK_EQ(left[0], 200); CHECK_EQ(left[1], 100); CHECK_EQ(left[2], 0);

    /* MOODINESS: it wanders by itself */
    u32 wander[2];
    for (u32 i = 0; i < 2u; i++) {
        blank(w, s);
        t[AZ_T_MOODINESS] = i ? 255 : 0;
        w->arousal[s] = 100;
        u32 lo = 255, hi = 0;
        for (u32 k = 0; k < 60u; k++) {
            visit(&b, s);
            if (w->arousal[s] < lo) lo = w->arousal[s];
            if (w->arousal[s] > hi) hi = w->arousal[s];
        }
        wander[i] = hi - lo;
    }
    CHECK_EQ(wander[0], 0);
    CHECK(wander[1] >= 20u);
    printf("feeling: the expected reward reached %d of a steady 40; a full surprise raised arousal to %u, %u, %u with "
           "AROUSAL_GAIN 0, 128, 255; of 200 a visit left %u, %u, %u with AROUSAL_DECAY 0, 127, 255; with MOODINESS 255 "
           "it wandered over %u\n", base, rose[0], rose[1], rose[2], left[0], left[1], left[2], wander[1]);
    bench_close(&b);
}

/* ---- 2 to 5. trace, learning, noise, state ---- */
/* visits until lane 0, which pays, has `margin` more bias than lane 1, which does not; 0 = never */
static u32 two_arms(Bench *b, u32 s, u32 learn_rate, u32 limit, i32 margin)
{
    AzWorld *w = b->w;
    blank(w, s);
    traits(w, s)[AZ_T_LEARN_RATE] = (u8)learn_rate;
    u64 rs = 0x7A0A5ull;
    const AzBrain *br = brain(w, s);
    for (u32 i = 1; i <= limit; i++) {
        /* its brain pulls the arm whose lane has the larger bias, with some chance of the other */
        const i32 lean = (i32)br->bias[0] - (i32)br->bias[1] + (i32)(az_test_rand64(&rs) % 41u) - 20;
        const u32 arm = lean >= 0 ? 0u : 1u;
        set_hid(w, s, arm, 110, 20);
        if (arm == 0) w->energy[s] += 48;                 /* arm 0 pays */
        visit(b, s);
        if ((i32)br->bias[0] - (i32)br->bias[1] >= margin) return i;
    }
    return 0;
}

static void test_learning(void)
{
    Bench b;
    if (!bench_open(&b, 5, 64, 0, false)) { CHECK(!"bench"); return; }
    AzWorld *w = b.w;
    const u32 s = pick(w);
    if (s == AZ_NONE) { CHECK(!"creature"); return; }
    u8 *t = traits(w, s);
    const AzBrain *br = brain(w, s);
    books(&b);

    /* 2. the trace: a lane above the mean, and how much of it outlasts a visit */
    static const u8 keep[3] = { 0, 128, 255 };
    i32 after[3];
    for (u32 i = 0; i < 3u; i++) {
        blank(w, s);
        t[AZ_T_PLASTICITY] = keep[i];
        set_hid(w, s, 2, 100, 20);                        /* mean 30 */
        visit(&b, s);
        CHECK_EQ(elig(w, s)[2], 70);
        CHECK_EQ(elig(w, s)[5], -10);
        set_hid(w, s, 0, 40, 40);
        visit(&b, s);
        after[i] = elig(w, s)[2];
    }
    CHECK_EQ(after[0], 0); CHECK_EQ(after[1], 35); CHECK_EQ(after[2], 69);

    /* 3. the two-armed world */
    const u32 slow = two_arms(&b, s, 40, 4000, 40), fast = two_arms(&b, s, 200, 4000, 40);
    CHECK(fast > 0 && slow > 0);
    CHECK(fast < slow);
    CHECK_EQ(two_arms(&b, s, 0, 300, 1), 0);              /* no LEARN_RATE gene: no learning */

    /* one step, with and without the metric: the same surprise and trace, another step */
    i32 step_metric[8], step_plain[8];
    for (u32 k = 0; k < 2u; k++) {
        blank(w, s);
        t[AZ_T_LEARN_RATE] = 255; t[AZ_T_Q_DEPTH] = 255;
        for (u32 l = 0; l < 8u; l++) ctx(w, s)[l] = (i8)(10 * (i32)l - 35);
        w->param[AZ_WP_TIER1_CTX_LEAK] = 256;
        w->param[AZ_WP_TIER1_METRIC_FLOOR] = k ? 65536u : 1024u;        /* a floor of 1.0 flattens the metric */
        set_hid(w, s, 1, 120, 10);
        w->energy[s] += 64;
        visit(&b, s);
        for (u32 l = 0; l < 8u; l++) (k ? step_plain : step_metric)[l] = br->bias[l];
    }
    w->param[AZ_WP_TIER1_CTX_LEAK] = 16; w->param[AZ_WP_TIER1_METRIC_FLOOR] = 1024;
    u32 differ = 0;
    for (u32 l = 0; l < 8u; l++) differ += step_metric[l] != step_plain[l];
    CHECK(differ > 0);
    CHECK(step_plain[1] > 0 && step_metric[1] > 0);       /* the lane that was active when the reward came */

    /* 4. noise of its own: EXPLORE, and IMPROV with the brain's drive */
    static const struct { u8 explore, improv; i8 drive; u32 moves; } nz[] = {
        { 0, 0, 0, 0 }, { 255, 0, 0, 1 }, { 0, 255, 0, 0 }, { 0, 255, -100, 0 }, { 0, 255, 127, 1 } };
    for (u32 i = 0; i < sizeof nz / sizeof nz[0]; i++) {
        blank(w, s);
        t[AZ_T_EXPLORE] = nz[i].explore; t[AZ_T_IMPROV] = nz[i].improv;
        w->motor[(size_t)s * AZ_MOT + AZ_M_IMPROV] = nz[i].drive;
        u32 moved = 0;
        for (u32 k = 0; k < 30u; k++) { visit(&b, s); for (u32 l = 0; l < 8u; l++) moved += br->bias[l] != 0; }
        CHECK_EQ(moved > 0, nz[i].moves);
    }

    /* 5. the state vector */
    i16 q0[16], q1[16];
    blank(w, s);
    for (u32 l = 0; l < 8u; l++) ctx(w, s)[l] = (i8)(9 * (i32)l - 30);
    visit(&b, s);
    memcpy(q0, &w->qstate[(size_t)s * 16], 32);
    u32 norm = 0;
    for (u32 i = 0; i < 16u; i++) norm += (u32)((i32)q0[i] * q0[i]);
    CHECK(norm > (1u << 28) - (1u << 22) && norm < (1u << 28) + (1u << 22));     /* a unit state */
    blank(w, s);
    for (u32 l = 0; l < 8u; l++) ctx(w, s)[l] = (i8)(9 * (i32)l - 30);
    t[AZ_T_Q_DEPTH] = 255;
    visit(&b, s);
    memcpy(q1, &w->qstate[(size_t)s * 16], 32);
    CHECK(memcmp(q0, q1, 32) != 0);                       /* a longer program, another state */
    /* its sample names a lane; Q_COUPLE gives that lane context and eligibility */
    u32 lanes = 0;
    for (u32 k = 0; k < 200u; k++) {
        blank(w, s);
        t[AZ_T_Q_COUPLE] = 255;
        visit(&b, s);
        u32 hit = 8, n = 0;
        for (u32 l = 0; l < 8u; l++) if (ctx(w, s)[l]) { hit = l; n++; }
        CHECK_EQ(n, 1);
        if (hit < 8u) { CHECK_EQ(ctx(w, s)[hit], 15); CHECK_EQ(elig(w, s)[hit], 31); lanes |= 1u << hit; }
    }
    CHECK_EQ(lanes, 255);                                 /* an even state: every lane comes up */
    blank(w, s);
    visit(&b, s);
    u32 any = 0;
    for (u32 l = 0; l < 8u; l++) any += ctx(w, s)[l] != 0;
    CHECK_EQ(any, 0);                                     /* no Q_COUPLE gene: the sample moves nothing */

    /* CTX_GAIN: the bias follows the context */
    for (u32 k = 0; k < 2u; k++) {
        blank(w, s);
        t[AZ_T_Q_COUPLE] = 255; t[AZ_T_CTX_GAIN] = k ? 255 : 0;
        for (u32 i = 0; i < 40u; i++) visit(&b, s);
        u32 bad = 0, some = 0;
        for (u32 l = 0; l < 8u; l++) {
            bad += br->bias[l] != (k ? ((i32)ctx(w, s)[l] * 255) / 512 : 0);
            some += ctx(w, s)[l] != 0;
        }
        CHECK_EQ(bad, 0);
        CHECK(some > 0);
    }
    bench_close(&b);

    /* the lesions on the same path */
    static const u32 les[3] = { AZ_LESION_PLASTICITY, AZ_LESION_METRIC, AZ_LESION_TIER1_Q };
    u32 never = 1, plain_same = 0, q_kept = 0;
    for (u32 k = 0; k < 3u; k++) {
        if (!bench_open(&b, 5, 64, les[k], false)) { CHECK(!"bench"); return; }
        w = b.w;
        const u32 z = pick(w);
        if (k == 0) {
            never = two_arms(&b, z, 200, 300, 1);
            blank(w, z);
            traits(w, z)[AZ_T_EXPLORE] = 255;
            u32 moved = 0;
            for (u32 i = 0; i < 30u; i++) { visit(&b, z); for (u32 l = 0; l < 8u; l++) moved += brain(w, z)->bias[l] != 0; }
            CHECK_EQ(moved, 0);
        } else {
            blank(w, z);
            traits(w, z)[AZ_T_LEARN_RATE] = 255; traits(w, z)[AZ_T_Q_DEPTH] = 255;
            for (u32 l = 0; l < 8u; l++) ctx(w, z)[l] = (i8)(10 * (i32)l - 35);
            w->param[AZ_WP_TIER1_CTX_LEAK] = 256;
            memset(&w->qstate[(size_t)z * 16], 0x11, 32);
            set_hid(w, z, 1, 120, 10);
            w->energy[z] += 64;
            visit(&b, z);
            u32 same = 0;
            for (u32 l = 0; l < 8u; l++) same += brain(w, z)->bias[l] == step_plain[l];
            plain_same += same == 8u;
            if (k == 2) { q_kept = 1; for (u32 i = 0; i < 16u; i++) q_kept &= w->qstate[(size_t)z * 16 + i] == 0x1111; }
        }
        bench_close(&b);
    }
    CHECK_EQ(never, 0);
    CHECK_EQ(plain_same, 2);                              /* both give the step of the flat metric */
    CHECK_EQ(q_kept, 1);                                  /* and without the module the state is not touched */
    printf("learning: the trace of 70 left %d, %d, %d with PLASTICITY 0, 128, 255; the paying lane led by 40 after "
           "%u visits with LEARN_RATE 200 and %u with 40, never under the plasticity lesion; the metric changed the "
           "step on %u of 8 lanes\n", after[0], after[1], after[2], fast, slow, differ);
}

/* ---- 6. memory ---- */
static void sense_pattern(const AzWorld *w, u32 s, u64 pattern)
{
    u8 *sens = mind(w, s);
    for (u32 i = 0; i < 64u; i++) sens[i] = (pattern >> i) & 1u ? 100 : 20;
}

static void test_memory(void)
{
    Bench b;
    if (!bench_open(&b, 7, 64, 0, false)) { CHECK(!"bench"); return; }
    AzWorld *w = b.w;
    const u32 s = pick(w);
    if (s == AZ_NONE) { CHECK(!"creature"); return; }
    u8 *t = traits(w, s);
    u32 words;
    u64 rs = 0x3E30ull;
    books(&b);

    /* MEMORY: a surprising state is stored in the working set; without the gene never */
    for (u32 k = 0; k < 2u; k++) {
        blank(w, s);
        t[AZ_T_MEMORY] = k ? 255 : 0;
        for (u32 i = 0; i < 12u; i++) {
            sense_pattern(w, s, az_test_rand64(&rs));
            w->energy[s] += (i & 1u) ? 300 : -300;       /* a surprise every time */
            visit(&b, s);
        }
        CHECK_EQ(w->hop_n[s], k ? AZ_HOP_PATTERNS : 0);
        (void)az_memory(w, s, &words);
        CHECK_EQ(words, 0);                               /* no HOPFIELD gene: nothing goes to the long term */
    }

    /* long-term words: asked for, granted empty, written for TIER1_WORD_EU as heat */
    blank(w, s);
    t[AZ_T_MEMORY] = 255; t[AZ_T_HOPFIELD] = 255;
    w->energy[s] = 100000; w->rprev[s] = 100000;
    books(&b);
    u32 written = 0, most = 0, empty_seen = 0;
    i64 spent = 0;
    for (u32 i = 0; i < 80u; i++) {
        sense_pattern(w, s, az_test_rand64(&rs));
        const i32 swing = (i & 1u) ? 300 : -300;
        w->energy[s] += swing;
        books(&b);
        const i32 before = w->energy[s];
        u32 w0;
        const u64 *m0 = az_memory(w, s, &w0);
        const bool open = w0 && m0[w0 - 1u] == 0;
        visit(&b, s);
        const u64 *m1 = az_memory(w, s, &words);
        if (open && words == w0) { CHECK(m1[w0 - 1u] != 0); written++; spent += before - w->energy[s]; }
        if (words > w0) { CHECK_EQ(m1[words - 1u], 0); empty_seen++; }
        if (words > most) most = words;
        CHECK(exact(&b));
    }
    CHECK(most >= 8u && written >= 8u && empty_seen >= 8u);
    CHECK_EQ(spent, (i64)written * 4);
    const u32 kept = words;

    /* a long-term word that is the present is recognised: it calms, and it is in the working set again */
    u32 mwords;
    const u64 *mem = az_memory(w, s, &mwords);
    const u64 old = mem[0];
    i32 ar[2];
    for (u32 k = 0; k < 2u; k++) {
        w->arousal[s] = 200; w->hop_n[s] = 0;
        memset(&w->engram[(size_t)s * AZ_HOP_PATTERNS], 0, 8 * AZ_HOP_PATTERNS);
        t[AZ_T_MEMORY] = 0;                               /* nothing new is stored */
        w->rbase[s] = 0; w->rprev[s] = w->energy[s];
        sense_pattern(w, s, k ? old : ~old);
        if (mem[mwords - 1u] == 0) ((u64 *)az_memory(w, s, &mwords))[mwords - 1u] = 0x5555;   /* no word open */
        visit(&b, s);
        ar[k] = w->arousal[s];
        if (k) { CHECK_EQ(w->hop_n[s], 1); CHECK_EQ(w->engram[(size_t)s * AZ_HOP_PATTERNS], old); }
        else CHECK_EQ(w->hop_n[s], 0);
    }
    CHECK_EQ(ar[0] - ar[1], 31);

    /* it cannot pay: the open word is given up */
    t[AZ_T_MEMORY] = 255;
    (void)az_memory(w, s, &words);
    az_memory_request(w, s, words + 1u);
    az_store_commit(w);
    (void)az_memory(w, s, &words);
    CHECK_EQ(words, kept + 1u);
    w->energy[s] = 2; w->rprev[s] = 2;
    sense_pattern(w, s, 0xF0F0F0F0F0F0F0F0ull);
    visit(&b, s);
    (void)az_memory(w, s, &words);
    CHECK_EQ(words, kept);
    CHECK_EQ(w->energy[s], 2);

    /* HOPFIELD: the recalled word moves the context, one byte per lane */
    i32 pull[2][2];
    for (u32 k = 0; k < 2u; k++) {
        blank(w, s);
        t[AZ_T_HOPFIELD] = k ? 255 : 0;
        w->energy[s] = 100000; w->rprev[s] = 100000;
        w->engram[(size_t)s * AZ_HOP_PATTERNS] = 0x00000000FF000000ull;   /* lane 3 */
        w->hop_n[s] = 1;
        w->param[AZ_WP_TIER1_HOP_TEMP] = 0;               /* the cold rule, whatever the trait */
        sense_pattern(w, s, 0x00000000FF000000ull);
        visit(&b, s);
        pull[k][0] = ctx(w, s)[3]; pull[k][1] = ctx(w, s)[0];
        if (k) CHECK_EQ(w->memory[s], 0x00000000FF000000ull);
    }
    w->param[AZ_WP_TIER1_HOP_TEMP] = 64;
    CHECK(pull[0][0] == 0 && pull[0][1] == 0);
    CHECK_EQ(pull[1][0], 15); CHECK_EQ(pull[1][1], -15);
    /* recall is noisy at a high temperature and exact at none */
    u32 flips[2] = { 0, 0 };
    for (u32 k = 0; k < 2u; k++) {
        for (u32 i = 0; i < 40u; i++) {
            blank(w, s);
            t[AZ_T_HOPFIELD] = k ? 255 : 0;
            w->param[AZ_WP_TIER1_HOP_TEMP] = 2000;
            w->engram[(size_t)s * AZ_HOP_PATTERNS] = 0x0123456789ABCDEFull;
            w->hop_n[s] = 1;
            sense_pattern(w, s, 0x0123456789ABCDEFull);
            visit(&b, s);
            flips[k] += w->memory[s] != 0x0123456789ABCDEFull;
        }
    }
    w->param[AZ_WP_TIER1_HOP_TEMP] = 64;
    CHECK(flips[0] >= 30u);
    CHECK_EQ(flips[1], 0);
    bench_close(&b);

    /* the lesion: nothing is stored, recalled or asked for */
    if (!bench_open(&b, 7, 64, AZ_LESION_MEMORY, false)) { CHECK(!"bench"); return; }
    w = b.w;
    const u32 z = pick(w);
    blank(w, z);
    traits(w, z)[AZ_T_MEMORY] = 255; traits(w, z)[AZ_T_HOPFIELD] = 255;
    w->memory[z] = 0xABCDull;
    for (u32 i = 0; i < 40u; i++) {
        sense_pattern(w, z, az_test_rand64(&rs));
        w->energy[z] += (i & 1u) ? 300 : -300;
        visit(&b, z);
    }
    (void)az_memory(w, z, &words);
    CHECK(w->hop_n[z] == 0 && words == 0 && w->memory[z] == 0xABCDull);
    u32 any = 0;
    for (u32 l = 0; l < 8u; l++) any += ctx(w, z)[l] != 0;
    CHECK_EQ(any, 0);
    bench_close(&b);
    printf("memory: the working set filled to %u patterns; %u long-term words were asked for and %u written at 4 eu "
           "each, paid as heat, books exact at every visit; a recognised state lowered arousal by %d; the recalled "
           "word moved its lane by %+d and the others by %+d; recall at a high temperature differed in %u of 40 "
           "visits without the HOPFIELD gene and in none with it\n",
           (u32)AZ_HOP_PATTERNS, empty_seen, written, ar[0] - ar[1], pull[1][0], pull[1][1], flips[0]);
}

/* ---- 7 and 8. the neighbourhood, the culture, the edges ---- */
static void test_others(void)
{
    /* ATTENTION: the gains follow the shape of the crowd around the creature */
    u32 gap[3] = { 0, 0, 0 };
    static const struct { u8 attention; u32 lesion; } at[3] = { { 255, 0 }, { 0, 0 }, { 255, AZ_LESION_TIER1_SH } };
    for (u32 k = 0; k < 3u; k++) {
        Bench b;
        if (!bench_open_uniform(&b, 11, 400, at[k].lesion, true)) { CHECK(!"bench"); return; }
        AzWorld *w = b.w;
        const u32 s = pick(w);
        blank(w, s);
        traits(w, s)[AZ_T_ATTENTION] = at[k].attention;
        books(&b);
        for (u32 i = 0; i < 12u; i++) visit(&b, s);
        for (u32 l = 0; l < 8u; l++) gap[k] += (u32)abs((int)brain(w, s)->gain[l] - 64);
        CHECK(exact(&b));
        bench_close(&b);
    }
    CHECK(gap[0] > 0);
    CHECK_EQ(gap[1], 0);
    CHECK_EQ(gap[2], 0);

    Bench b;
    if (!bench_open(&b, 13, 64, 0, false)) { CHECK(!"bench"); return; }
    AzWorld *w = b.w;
    const u32 s = pick(w);
    u8 *t = traits(w, s);
    books(&b);
    /* CONFORMITY: the colony's shared prototype pulls the context */
    if (w->n_colonies < 2u) w->n_colonies = 2;
    memset(w->colonies[1].culture, 228, 8);               /* +100 */
    i32 got[3];
    static const struct { u8 conf; u32 colony; } cf[3] = { { 255, 1 }, { 0, 1 }, { 255, 0 } };
    for (u32 k = 0; k < 3u; k++) {
        blank(w, s);
        t[AZ_T_CONFORMITY] = cf[k].conf;
        w->colony[s] = cf[k].colony;
        for (u32 i = 0; i < 60u; i++) visit(&b, s);
        got[k] = ctx(w, s)[4];
    }
    w->colony[s] = 0;
    CHECK(got[0] >= 60); CHECK_EQ(got[1], 0); CHECK_EQ(got[2], 0);

    /* 8. edges. A brain with no hidden block: no lane activity, traces decay, the rest runs */
    blank(w, s);
    memset(t, 255, AZ_TRAITS);
    AzBrain *br = brain(w, s);
    const u16 n_hb = br->n_hb;
    br->n_hb = 0;
    memset(elig(w, s), 50, 8);
    t[AZ_T_PLASTICITY] = 0; t[AZ_T_Q_COUPLE] = 0; t[AZ_T_EXPLORE] = 0; t[AZ_T_IMPROV] = 0;
    for (u32 i = 0; i < 20u; i++) { w->energy[s] += (i & 1u) ? 90 : -90; visit(&b, s); }
    u32 left = 0;
    for (u32 l = 0; l < 8u; l++) left += elig(w, s)[l] != 0;
    CHECK_EQ(left, 0);
    br->n_hb = n_hb;
    /* an egg and a cyst have no working brain: only the energy mark moves */
    static const u8 off[2] = { AZ_F_EGG, AZ_F_DORMANT };
    for (u32 k = 0; k < 2u; k++) {
        blank(w, s);
        memset(t, 255, AZ_TRAITS);
        w->flags[s] = (u8)(AZ_F_ALIVE | off[k]);
        w->arousal[s] = 77;
        w->energy[s] += 500;
        visit(&b, s);
        u32 moved = w->arousal[s] != 77 || w->rbase[s] != 0 || w->hop_n[s] != 0;
        for (u32 l = 0; l < 8u; l++) moved += brain(w, s)->bias[l] != 0 || ctx(w, s)[l] != 0 || elig(w, s)[l] != 0;
        CHECK_EQ(moved, 0);
        CHECK_EQ(w->rprev[s], w->energy[s]);
    }
    printf("others: ATTENTION 255 moved the gains by %u in all (none without the gene, none under the lesion); "
           "CONFORMITY 255 drew the context to %d of the colony's 100 (0 without the gene, 0 for a solitary one)\n",
           gap[0], got[0]);
    bench_close(&b);
}

/* ---- 9. worlds ---- */
typedef struct Run { u64 hash, births; u32 live, words, learned; int ok; } Run;

static Run world_run(u32 threads, u8 reorder, u32 lesion)
{
    Run r;
    memset(&r, 0, sizeof r);
    AzConfig cfg = az_test_config(15, 2000);
    cfg.reorder_period = reorder;
    cfg.cap = 40000;
    cfg.lesion = lesion;
    AzTestWorld t;
    if (!az_test_world(&t, &cfg, threads)) { CHECK(!"world"); return r; }
    AzWorld *w = t.w;
    az_test_founders_bud(w);
    const u64 m0 = az_total_matter(w);
    const i64 e0 = az_total_energy(w);
    r.ok = 1;
    for (u32 i = 0; i < 120u; i++) {
        az_tick(w);
        if (az_total_matter(w) != m0 || az_total_energy(w) != e0) r.ok = 0;
    }
    for (u32 s = 0; s < w->n_slots; s++) {
        if (!(w->flags[s] & AZ_F_ALIVE) || !w->blk[s]) continue;
        u32 words;
        (void)az_memory(w, s, &words);
        r.words += words;
        r.learned += w->hop_n[s] != 0;
    }
    r.hash = az_state_hash(w); r.births = w->totals.births; r.live = w->n_live;
    az_test_world_free(&t);
    return r;
}

static void test_worlds(void)
{
    const Run a = world_run(0, 1, 0), b = world_run(1, 9, 0), c = world_run(3, 1, 0), d = world_run(8, 9, 0);
    CHECK(a.ok && b.ok && c.ok && d.ok);
    CHECK_EQ(a.hash, b.hash); CHECK_EQ(a.hash, c.hash); CHECK_EQ(a.hash, d.hash);
    CHECK(a.learned > 0);
    static const u32 les[5] = { AZ_LESION_PLASTICITY, AZ_LESION_METRIC, AZ_LESION_TIER1_Q, AZ_LESION_TIER1_SH,
                                AZ_LESION_MEMORY };
    u32 other = 0;
    for (u32 i = 0; i < 5u; i++) {
        const Run l = world_run(0, 1, les[i]), l8 = world_run(8, 9, les[i]);
        CHECK(l.ok && l8.ok);
        CHECK_EQ(l.hash, l8.hash);
        other += l.hash != a.hash;
        if (les[i] == AZ_LESION_MEMORY) CHECK(l.learned == 0 && l.words == 0);
    }
    CHECK_EQ(other, 5);
    printf("worlds: 2000 founders, 120 ticks: %" PRIu64 " births, %u alive, %u of them hold a pattern in the working "
           "set, %u long-term words in all, hash %016" PRIx64 " on 0, 1, 3 and 8 workers; each of the five lesions "
           "gives another world, the same on 0 and 8 workers\n", a.births, a.live, a.learned, a.words, a.hash);
}

int main(void)
{
    test_feeling();
    test_learning();
    test_memory();
    test_others();
    test_worlds();
    CHECK_EQ(inexact, 0);
    printf("books: matter and energy the same after each of %u visits as before it\n", visits);
    return az_test_done();
}
