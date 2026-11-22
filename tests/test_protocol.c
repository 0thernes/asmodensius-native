/* test_protocol.c - the tick's interaction protocol under the determinism matrix.
 *
 * Linked against az_core_proto: the core with the TEST-ONLY interacting
 * module set of tests/protocol/ in place of the social, econ, mate and life
 * modules. With the reference stubs no pair ever forms and nobody dies, so
 * the earlier determinism tests proved equality on a nearly idle world
 * (findings s1, v31, v38, v71). Here every run has
 *   - pair interactions of all three owners (social: attack and share;
 *     econ: trade; mate), with claims that are refused or lost to a rival;
 *   - deaths, so dead slots, the free list, slot reuse by az_spawn and clades
 *     losing members;
 *   - both kinds of birth (budding and sexual);
 * and the test requires each of them to be non-zero before it compares
 * anything. Then, against one serial reference run, tick by tick:
 *   - worker counts 1, 2, 5, 8, 12, 16 and 24 (ideal processors, and unpinned);
 *   - reorder periods 7, 31 and 255 (how often dead slots are compacted);
 *   - randomised work splits, serial and threaded;
 *   - a binding slot capacity (births refused while deaths free slots);
 *   - a snapshot saved mid-run and resumed under another worker count;
 *   - a larger world that takes the grid's private-histogram path;
 *   - a world whose tick and uids start just below 2^32 and run across it
 *     (the old 32-bit limits), under the same matrix;
 *   - colonies as organisms: members pay into and draw on their colony's
 *     energy pool and speak on its channels, under the same matrix; the
 *     pools never go below zero and are inside the exact energy balance;
 *   - generations counted past 65,535 (the old 16-bit limit).
 * Equal means: the state hash after every tick, the ledger chain and count,
 * and every total. Matter and energy are checked with == after every tick.
 *
 * Seeds are tuning seeds (1..99); nothing here is a reported result.
 */
#include "az_test.h"

#define T_MAX 160u

typedef struct Opt {
    u64 seed;
    u32 n, cap, ticks, threads, reorder, split, flags, lesion;
    u32 resume_at, resume_threads;   /* save before tick resume_at, resume with resume_threads */
    int audit;                       /* check both invariants after every tick */
    u64 start;                       /* non-zero: the tick and the next uid the run starts from */
    u32 colonies;                    /* non-zero: this many colonies, two creatures in three are members */
    u32 generation0;                 /* the generation the founders are given */
} Opt;

typedef struct Out {
    bool ok;
    u64 hash, chain, count;
    u64 trace[T_MAX];
    AzTotals tot;
    u32 live;
    u64 reused, appended;            /* newborns in a freed slot / in a new slot */
    u64 sexual, budded;
    u64 pairs, proposals;
    u64 max_uid, end_tick, max_generation;
    u64 grants_full, grants_part, grants_none;   /* colony pool: (colony, tick) cases by what was granted */
    i64 pool_end, pool_peak;                     /* energy in the pools at the end, and the most one held */
    u32 members_end, voices_end;                 /* members alive at the end; channels carrying a signal  */
} Out;

/* the newborns of the tick that just ran */
static void census(const AzWorld *w, u32 slots_before, u64 uid_before, bool reordered, Out *o)
{
    for (u32 s = 0; s < w->n_slots; s++) {
        if (!(w->flags[s] & AZ_F_ALIVE) || w->uid[s] < uid_before) continue;
        if (w->parent_b[s]) o->sexual++; else o->budded++;
        /* a tick that starts with a reorder has no dead slot to reuse */
        if (!reordered && s < slots_before) o->reused++; else o->appended++;
    }
}

static Out run(Opt q)
{
    Out o;
    memset(&o, 0, sizeof o);
    AzConfig cfg = az_test_config(q.seed, q.n);
    if (q.cap) cfg.cap = q.cap;
    cfg.reorder_period = (u8)q.reorder;
    cfg.debug_split_seed = q.split;
    cfg.lesion = q.lesion;
    AzTestWorld t;
    if (q.ticks > T_MAX || !az_test_world_ex(&t, &cfg, q.threads, q.flags)) {
        CHECK(!"world creation");
        return o;
    }
    AzWorld *w = t.w;
    if (q.start) {
        /* move the world in time: its clock and its uid counter start at
         * q.start, and the founders were born at that tick, not at tick 0 */
        w->tick = q.start;
        w->uid_next = q.start;
        az_tick_keys(w);
        for (u32 s = 0; s < w->n_slots; s++) w->birth_tick[s] += q.start;
    }
    if (q.colonies) {
        /* found the colonies by hand (the reference colony module founds
         * none): two creatures in three are members, the pools start empty,
         * and children are born into their mother's colony */
        CHECK(q.colonies < w->cfg.max_colonies);
        w->n_colonies = 1u + q.colonies;
        memset(w->colonies, 0, (size_t)w->n_colonies * sizeof(AzColony));
        memset(w->colony_eph, 0, (size_t)w->n_colonies * sizeof(AzColonyEph));
        for (u32 i = 1; i < w->n_colonies; i++) { w->colonies[i].alive = 1; w->colonies[i].founder_uid = i; }
        for (u32 s = 0; s < w->n_slots; s++)
            if ((w->flags[s] & AZ_F_ALIVE) && w->uid[s] % 3u) w->colony[s] = 1u + (u32)(w->uid[s] % q.colonies);
    }
    if (q.generation0)
        for (u32 s = 0; s < w->n_slots; s++) w->generation[s] = q.generation0;
    const u64 matter0 = az_total_matter(w);
    const i64 energy0 = az_total_energy(w);

    for (u32 k = 0; k < q.ticks; k++) {
        if (q.resume_at && k == q.resume_at) {
            /* save, drop the world and its pool, load into a fresh arena */
            const u64 nb = az_snapshot_bytes(w);
            u8 *snap = (u8 *)malloc((size_t)nb + 8);
            const bool saved = snap && nb && az_snapshot_save(w, snap, nb) == nb;
            CHECK(saved);
            az_test_world_free(&t);
            w = NULL;
            AzConfig sc;
            if (saved && az_snapshot_config(snap, nb, &sc) && az_test_world_arena(&t, &sc)) {
                if (q.resume_threads) t.host = az_win_host_create(q.resume_threads, AZ_WIN_USE_ECORES, &t.info);
                if (t.host || !q.resume_threads) w = t.w = az_snapshot_load(&t.arena, snap, nb, t.host);
            }
            free(snap);
            CHECK(w != NULL);
            if (!w) { az_test_world_free(&t); return o; }
        }
        const u32 slots_before = w->n_slots;
        const u64 uid_before = w->uid_next;
        const bool reordered = (w->tick % w->cfg.reorder_period) == 0;
        az_tick(w);
        census(w, slots_before, uid_before, reordered, &o);
        o.trace[k] = az_state_hash(w);
        for (u32 i = 1; i < w->n_colonies; i++) {
            /* what the tick granted is still in the scratch record */
            const AzColonyEph *e = &w->colony_eph[i];
            if (e->dem_e) { if (e->grant_e == 65535u) o.grants_full++; else if (e->grant_e) o.grants_part++; else o.grants_none++; }
            CHECK(w->colonies[i].energy >= 0);       /* a pool never gives what it does not hold */
            CHECK(e->dep_e == 0 && e->taken_e == 0); /* and the tick applied every gift and every draw */
            if (w->colonies[i].energy > o.pool_peak) o.pool_peak = w->colonies[i].energy;
        }
        if (q.audit) {
            CHECK_EQ(az_total_matter(w), matter0);
            CHECK_EQ(az_total_energy(w), energy0);
            CHECK_EQ(w->totals.field_underflow, 0);
            CHECK_EQ(w->totals.dropped_events, 0);
        }
    }
    /* every run ends with both invariants exact, audited or not */
    CHECK_EQ(az_total_matter(w), matter0);
    CHECK_EQ(az_total_energy(w), energy0);
    CHECK_EQ(w->totals.field_underflow, 0);
    CHECK_EQ(w->totals.dropped_events, 0);

    o.hash = az_state_hash(w);
    o.chain = w->ledger_chain;
    o.count = w->ledger_count;
    o.tot = w->totals;
    u64 members = 0;
    for (u32 s = 0; s < w->n_slots; s++) {
        if (!(w->flags[s] & AZ_F_ALIVE)) continue;
        o.live++;
        if (w->uid[s] > o.max_uid) o.max_uid = w->uid[s];
        if (w->generation[s] > o.max_generation) o.max_generation = w->generation[s];
        CHECK(w->birth_tick[s] <= w->tick);
        CHECK(w->tick - w->birth_tick[s] < 4096);    /* an age, not a wrapped difference */
    }
    o.end_tick = w->tick;
    for (u32 i = 1; i < w->n_colonies; i++) {
        o.pool_end += w->colonies[i].energy;
        for (u32 c = 0; c < 4u; c++) o.voices_end += w->colonies[i].signal[c] != 0;
    }
    for (u32 s = 0; s < w->n_slots; s++) o.members_end += (w->flags[s] & AZ_F_ALIVE) && w->colony[s] != 0;
    for (u32 i = 0; i < w->n_clades; i++) members += w->clades[i].refcount;
    CHECK_EQ(members, o.live);                       /* deaths released their clade membership */
    for (u32 a = 0; a < AZ_ACT_COUNT; a++) o.pairs += o.tot.interactions[a];
    o.proposals = o.tot.sat[AZ_SAT_OTHER];
    o.ok = true;
    az_test_world_free(&t);
    return o;
}

/* v must equal ref tick by tick */
static void same(const Out *ref, const Out *v, u32 ticks, const char *what)
{
    u32 first = ticks;
    for (u32 k = 0; k < ticks; k++) if (ref->trace[k] != v->trace[k]) { first = k; break; }
    const bool eq = v->ok && first == ticks && ref->hash == v->hash && ref->chain == v->chain &&
                    ref->count == v->count && memcmp(&ref->tot, &v->tot, sizeof(AzTotals)) == 0;
    CHECK(v->ok);
    CHECK(first == ticks);
    CHECK(ref->hash == v->hash);
    CHECK(ref->chain == v->chain && ref->count == v->count);
    CHECK(memcmp(&ref->tot, &v->tot, sizeof(AzTotals)) == 0);
    if (!eq && first != ticks) printf("  first differing tick: %u of %u\n", first + 1, ticks);
    printf("  %-44s %016" PRIx64 "  %s\n", what, v->hash, eq ? "equal" : "DIFFERENT");
}

static void describe(const char *what, const Out *o)
{
    printf("%s: hash %016" PRIx64 ", live %u\n", what, o->hash, o->live);
    printf("  births %" PRIu64 " (sexual %" PRIu64 ", budded %" PRIu64 "; %" PRIu64 " in a freed slot, %" PRIu64
           " in a new slot), deaths %" PRIu64 ", refused for want of a slot %" PRIu64 "\n",
           o->tot.births, o->sexual, o->budded, o->reused, o->appended, o->tot.deaths, o->tot.cap_denied);
    printf("  pairs resolved %" PRIu64 " of %" PRIu64 " proposals: attack %" PRIu64 ", share %" PRIu64 ", trade %" PRIu64
           ", mate %" PRIu64 "; trade units %" PRIu64 "; ledger events %" PRIu64 "\n",
           o->pairs, o->proposals, o->tot.interactions[AZ_ACT_ATTACK], o->tot.interactions[AZ_ACT_SHARE],
           o->tot.interactions[AZ_ACT_TRADE], o->tot.interactions[AZ_ACT_MATE], o->tot.trade_units[1][1], o->count);
}

/* the world really interacts: without this, equality proves little */
static void must_interact(const Out *o, u32 n)
{
    CHECK(o->ok);
    CHECK(o->tot.deaths > n / 10u);
    CHECK(o->live > 0);
    CHECK(o->sexual > 0);
    CHECK(o->budded > 0);
    CHECK_EQ(o->sexual + o->budded, o->tot.births);
    CHECK(o->tot.interactions[AZ_ACT_ATTACK] > 0);
    CHECK(o->tot.interactions[AZ_ACT_SHARE] > 0);
    CHECK(o->tot.interactions[AZ_ACT_TRADE] > 0);
    CHECK(o->tot.interactions[AZ_ACT_MATE] > 0);
    CHECK(o->tot.trade_units[1][1] > 0);
    CHECK(o->proposals > o->pairs);                  /* some claims were refused or lost */
    CHECK(o->pairs > 0);
}

int main(void)
{
    const u32 N = 4000, T = 100;
    Opt base = { .seed = 7, .n = N, .ticks = T, .reorder = 1, .audit = 1 };

    /* ---- the reference: serial, compacted every tick ---- */
    const Out ref = run(base);
    describe("reference (serial, reorder 1)", &ref);
    must_interact(&ref, N);
    CHECK_EQ(ref.reused, 0);                         /* compacting every tick leaves nothing to reuse */
    CHECK_EQ(ref.tot.cap_denied, 0);

    /* ---- worker counts ---- */
    printf("worker counts:\n");
    static const u32 threads[7] = { 1, 2, 5, 8, 12, 16, 24 };
    for (u32 i = 0; i < 7; i++) {
        char what[64];
        Opt q = base; q.audit = 0; q.threads = threads[i];
        Out v = run(q);
        snprintf(what, sizeof what, "%u workers", threads[i]);
        same(&ref, &v, T, what);
    }
    {
        Opt q = base; q.audit = 0; q.threads = 8; q.flags = AZ_TEST_UNPINNED;
        Out v = run(q);
        same(&ref, &v, T, "8 workers, unpinned");
        q.threads = 24;
        v = run(q);
        same(&ref, &v, T, "24 workers, unpinned");
    }

    /* ---- reorder periods: dead slots stay, and births reuse them ---- */
    printf("reorder periods:\n");
    static const u32 periods[3] = { 7, 31, 255 };
    for (u32 i = 0; i < 3; i++) {
        char what[64];
        Opt q = base; q.reorder = periods[i];
        Out v = run(q);
        snprintf(what, sizeof what, "reorder %u, serial (%" PRIu64 " reused)", periods[i], v.reused);
        same(&ref, &v, T, what);
        CHECK(v.reused > 0);                          /* az_spawn took slots from the free list */
        q.audit = 0; q.threads = 5 + 3 * i;
        v = run(q);
        snprintf(what, sizeof what, "reorder %u, %u workers", periods[i], q.threads);
        same(&ref, &v, T, what);
        CHECK(v.reused > 0);
    }

    /* ---- randomised work splits ---- */
    printf("work splits:\n");
    {
        Opt q = base; q.audit = 0; q.split = 12345;
        Out v = run(q);
        same(&ref, &v, T, "split seed 12345, serial");
        q.split = 777; q.reorder = 9;
        v = run(q);
        same(&ref, &v, T, "split seed 777, reorder 9, serial");
        q.split = 4242; q.reorder = 5; q.threads = 6;
        v = run(q);
        same(&ref, &v, T, "split seed 4242, reorder 5, 6 workers");
        q.split = 99; q.reorder = 13; q.threads = 24;
        v = run(q);
        same(&ref, &v, T, "split seed 99, reorder 13, 24 workers");
    }

    /* ---- a snapshot saved mid-run and resumed under another worker count ---- */
    printf("snapshot resume:\n");
    {
        Opt q = base; q.audit = 0; q.threads = 8; q.reorder = 7; q.resume_at = T / 2; q.resume_threads = 3;
        Out v = run(q);
        same(&ref, &v, T, "8 workers, saved at tick 50, resumed on 3");
        q.threads = 0; q.reorder = 255; q.resume_at = 37; q.resume_threads = 12;
        v = run(q);
        same(&ref, &v, T, "serial, saved at tick 37, resumed on 12");
        q.threads = 5; q.reorder = 1; q.resume_at = 71; q.resume_threads = 0;
        v = run(q);
        same(&ref, &v, T, "5 workers, saved at tick 71, resumed serial");
    }

    /* ---- binding capacity: births are refused while deaths free slots ---- */
    printf("binding capacity:\n");
    {
        Opt q = base; q.cap = N + 192;
        const Out b = run(q);
        describe("binding capacity (serial, reorder 1)", &b);
        must_interact(&b, N);
        CHECK(b.tot.cap_denied > 0);
        CHECK(b.hash != ref.hash);
        static const u32 bp[4] = { 7, 31, 255, 3 };
        static const u32 bt[4] = { 0, 0, 0, 5 };
        for (u32 i = 0; i < 4; i++) {
            char what[64];
            Opt v = q; v.reorder = bp[i]; v.threads = bt[i]; v.audit = bt[i] == 0;
            Out o = run(v);
            snprintf(what, sizeof what, "reorder %u, %u workers (%" PRIu64 " reused)", bp[i], bt[i], o.reused);
            same(&b, &o, T, what);
            CHECK(o.reused > 0);
        }
        Opt v = q; v.audit = 0; v.reorder = 13; v.threads = 4; v.split = 99;
        Out o = run(v);
        same(&b, &o, T, "reorder 13, 4 workers, split seed 99");
        v.threads = 24; v.split = 0; v.reorder = 255; v.resume_at = 60; v.resume_threads = 2;
        o = run(v);
        same(&b, &o, T, "24 workers, reorder 255, resumed on 2");
    }

    /* ---- colonies as organisms: a shared energy pool and voices ---- */
    printf("colony pools:\n");
    {
        Opt q = base; q.colonies = 5;
        const Out c = run(q);
        describe("colonies (serial, reorder 1)", &c);
        must_interact(&c, N);
        printf("  5 colonies, %u members alive at the end; grants in full %" PRIu64 ", in part %" PRIu64 ", none %" PRIu64
               "; pools hold %" PRId64 " eu at the end, the fullest held %" PRId64 "; %u of 20 channels carry a voice\n",
               c.members_end, c.grants_full, c.grants_part, c.grants_none, c.pool_end, c.pool_peak, c.voices_end);
        CHECK(c.members_end > N / 10u);
        CHECK(c.grants_full > 0 && c.grants_part > 0);   /* pools both filled and ran dry */
        CHECK(c.pool_peak > 0);
        CHECK(c.voices_end > 0);
        CHECK(c.hash != ref.hash);
        Opt v = q; v.audit = 0; v.threads = 8; v.reorder = 7;
        Out o = run(v);
        same(&c, &o, T, "8 workers, reorder 7");
        v.threads = 24; v.reorder = 31; v.split = 99;
        o = run(v);
        same(&c, &o, T, "24 workers, reorder 31, split seed 99");
        v.threads = 3; v.reorder = 1; v.split = 0; v.resume_at = 50; v.resume_threads = 12;
        o = run(v);
        same(&c, &o, T, "3 workers, saved at tick 50, resumed on 12");
    }

    /* ---- generations past the old 16-bit limit ---- */
    printf("generations:\n");
    {
        Opt q = base; q.generation0 = 65534u;
        const Out g = run(q);
        printf("  founders at generation 65534: the deepest lineage alive after %u ticks is at generation %" PRIu64 "\n",
               T, g.max_generation);
        CHECK(g.ok && g.max_generation > 65536u);     /* counted on, not wrapped to a small number */
        Opt v = q; v.audit = 0; v.threads = 8; v.reorder = 7;
        Out o = run(v);
        same(&g, &o, T, "8 workers, reorder 7");
    }

    /* ---- a larger world: the grid counts with private histograms ---- */
    printf("larger world:\n");
    {
        const u32 N2 = 24000, T2 = 40;
        Opt q = { .seed = 11, .n = N2, .ticks = T2, .reorder = 1, .audit = 1 };
        const Out big = run(q);
        describe("larger world (serial, reorder 1)", &big);
        CHECK(big.ok && big.tot.deaths > 0 && big.pairs > 0 && big.sexual > 0);
        static const u32 gt[3] = { 8, 16, 24 };
        static const u32 gp[3] = { 1, 7, 255 };
        for (u32 i = 0; i < 3; i++) {
            char what[64];
            Opt v = q; v.audit = 0; v.threads = gt[i]; v.reorder = gp[i];
            Out o = run(v);
            snprintf(what, sizeof what, "%u workers, reorder %u", gt[i], gp[i]);
            same(&big, &o, T2, what);
        }
    }

    /* ---- across the old limits: the tick and the uids start 40 below 2^32,
     * so both cross it in the run (owner instruction of 2026-11-18: no counter
     * may wrap; the test runs across the old limits under the thread matrix) ---- */
    printf("across 2^32 (tick and uid):\n");
    {
        const u64 START = 0x100000000ull - 40u;
        Opt q = base; q.start = START;
        const Out wide = run(q);
        describe("across 2^32 (serial, reorder 1)", &wide);
        must_interact(&wide, N);
        CHECK_EQ(wide.end_tick, START + T);
        CHECK(wide.end_tick > 0x100000000ull);
        CHECK(wide.max_uid > 0x100000000ull);            /* uids went on past the old limit */
        CHECK_EQ(wide.tot.cap_denied, 0);
        CHECK(wide.max_generation >= 2);
        CHECK(wide.hash != ref.hash);
        static const u32 wt[7] = { 1, 2, 5, 8, 12, 16, 24 };
        for (u32 i = 0; i < 7; i++) {
            char what[64];
            Opt v = q; v.audit = 0; v.threads = wt[i];
            Out o = run(v);
            snprintf(what, sizeof what, "%u workers", wt[i]);
            same(&wide, &o, T, what);
        }
        static const u32 wp[3] = { 7, 31, 255 };
        for (u32 i = 0; i < 3; i++) {
            char what[64];
            Opt v = q; v.reorder = wp[i]; v.threads = i == 1 ? 0 : 6 + 9 * i; v.audit = i == 1;
            Out o = run(v);
            snprintf(what, sizeof what, "reorder %u, %u workers (%" PRIu64 " reused)", wp[i], v.threads, o.reused);
            same(&wide, &o, T, what);
            CHECK(o.reused > 0);
        }
        Opt v = q; v.audit = 0; v.split = 4242; v.reorder = 5; v.threads = 6;
        Out o = run(v);
        same(&wide, &o, T, "split seed 4242, reorder 5, 6 workers");
        /* saved 10 ticks before the clock crosses 2^32, resumed, run across */
        v = q; v.audit = 0; v.threads = 8; v.reorder = 7; v.resume_at = 30; v.resume_threads = 3;
        o = run(v);
        same(&wide, &o, T, "8 workers, saved at tick 30, resumed on 3");
        /* saved after the crossing */
        v.threads = 0; v.reorder = 255; v.resume_at = 55; v.resume_threads = 12;
        o = run(v);
        same(&wide, &o, T, "serial, saved at tick 55, resumed on 12");
    }

    /* ---- the hash sees the seed, and a lesion of all pairs changes the world ---- */
    {
        Opt q = base; q.audit = 0; q.seed = 8;
        const Out other = run(q);
        CHECK(other.ok && other.hash != ref.hash);
        q = base; q.lesion = AZ_LESION_SOCIAL;
        const Out les = run(q);
        CHECK(les.ok && les.pairs == 0);              /* no pair forms under the lesion */
        CHECK(les.hash != ref.hash);
        printf("lesion of all pairs: %" PRIu64 " pairs, hash %016" PRIx64 " (intact %016" PRIx64 ")\n",
               les.pairs, les.hash, ref.hash);
    }
    return az_test_done();
}
