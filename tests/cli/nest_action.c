/* Same-seed, remove-one experiments using the native pantry and field commit.
   A held posture, stocked treasury or saturated mark buys no credit by itself. */
#include "az_test.h"
#ifdef AZ_BUILDER_OBSERVER
#include "az_teaching_observer.h"
#include "az_builder_hooks.h"
#endif

typedef struct {
    const char *name;
    u32 standing, background;
    u8 second, provision, status;
    u8 expected_without_a, expected_after, credit_a, credit_b;
} Case;
enum { ACTIVE, EGG, DORMANT, DEAD, WRONG_ROLE, NO_COLONY, OFF_PHASE, NO_STRENGTH };

typedef struct {
    u32 physical, counts[2], grant;
    u64 uid[2], state, matter;
    i64 energy;
} Result;

static Result run_case(const Case *c, u32 suppress, bool observe, bool loaded)
{
    Result r = {0};
    AzConfig cfg = az_test_config(103, 32);
    AzTestWorld t;
    if (!az_test_world(&t, &cfg, 0)) { CHECK(false); return r; }
    AzWorld *w = t.w;
    u32 slots[2], n = 0;
    for (u32 s = 0; s < w->n_slots && n < 2; ++s)
        if ((w->flags[s] & AZ_F_ALIVE) && az_phase_due(w, w->uid[s], AZ_RS_ECON, 2)) slots[n++] = s;
    CHECK_EQ(n, 2);
    if (n < 2) { az_test_world_free(&t); return r; }
    w->param[AZ_WP_COLONY_ENDOW] = 0;
    const u32 id = az_colony_found(w, slots[0]); CHECK(id != 0);
    const u32 cell = w->cell[slots[0]];
    u8 *mark = &w->c_mark[(size_t)cell * AZ_NMARK + AZ_MK_NEST];
    *mark = (u8)c->standing;
    if (c->background) az_cell_mark(w, 0, cell, AZ_MK_NEST, c->background, id);
    w->param[AZ_WP_ECON_NEST] = 20; /* neutral motor requests ten */
    for (u32 j = 0; j < 2; ++j) {
        const u32 s = slots[j]; r.uid[j] = w->uid[s];
        w->cell[s] = cell; w->colony[s] = id; w->role[s] = AZ_ROLE_BUILDER;
        w->flags[s] = AZ_F_ALIVE;
        w->energy[s] = 0; w->pool_a[s] = 0;
        memset(&w->inv[(size_t)s * AZ_NINV], 0, AZ_NINV * sizeof(u32));
        memset(&w->trait[(size_t)s * AZ_TRAITS], 0, AZ_TRAITS);
        memset(&w->stat[(size_t)s * AZ_NSTAT], 0, AZ_NSTAT * sizeof(u32));
        w->travel_carry[s] = 0;
        w->motor[(size_t)s * AZ_MOT + AZ_M_BUILD] = j == suppress || (j && !c->second) ? -128 : 0;
    }
    const u32 a = slots[0];
    if (c->status == EGG) w->flags[a] |= AZ_F_EGG;
    if (c->status == DORMANT) w->flags[a] |= AZ_F_DORMANT;
    if (c->status == DEAD) w->flags[a] &= (u8)~AZ_F_ALIVE;
    if (c->status == WRONG_ROLE) w->role[a] = AZ_ROLE_FORAGER;
    if (c->status == NO_COLONY) w->colony[a] = 0;
    if (c->status == OFF_PHASE) do { ++w->tick; } while (az_phase_due(w, w->uid[a], AZ_RS_ECON, 2));
    if (c->status == NO_STRENGTH) w->param[AZ_WP_ECON_NEST] = 0;
    /* This is an actual 7-mu ORG_Y grant from the colony, made available
       to the first actor. The same ask has zero fulfillment in the empty arm. */
    w->colonies[id].store[1] = c->provision ? 7 : 0;
    w->colony_eph[id].dem[1] = 7;
    w->gather[(size_t)a * AZ_NINV + 1] = 7;
    w->gather[(size_t)a * AZ_NINV + 2] = id;
    az_colony_grant(w);
#ifdef AZ_BUILDER_OBSERVER
    AzArena obs = {0}; AzWinInfo info = {0};
    AztTeaching life = {.first_covered_uid=loaded ? w->uid_next : 1};
    if (observe) {
        const u64 ib = azt_intake_bytes(w->cap), bb = azt_builder_bytes(w->cap);
        CHECK(az_win_arena(&obs, ib + bb, AZ_WIN_LOCK, &info) && info.arena_locked);
        CHECK(azt_intake_attach(w, !loaded, obs.base, ib));
        CHECK(azt_builder_attach(w, obs.base + ib, bb));
    }
#else
    (void)observe; (void)loaded;
#endif
    const u64 matter = az_total_matter(w); const i64 energy = az_total_energy(w);
    for (u32 j = 0; j < 2; ++j) az_econ_range(w, slots[j], slots[j] + 1, 0);
    r.grant = (u32)w->colony_eph[id].taken[1];
    az_colony_commit(w);
#ifdef AZ_BUILDER_OBSERVER
    azt_builder_before_commit(w);
#endif
    az_field_commit(w, cell, cell + 1, 0);
#ifdef AZ_BUILDER_OBSERVER
    azt_builder_after_commit(w);
#endif
    ++w->tick;
    r.physical = *mark;
    r.state = az_state_hash(w); r.matter = az_total_matter(w); r.energy = az_total_energy(w);
    CHECK_EQ(r.matter, matter); CHECK_EQ(r.energy, energy);
#ifdef AZ_BUILDER_OBSERVER
    if (observe) {
        CHECK(azt_builder_window(w));
        AzStats st; az_stats_collect(w, &st);
        const AztTeachingSummary co = azt_teaching_apply(&life, w, &st, azt_intake_fill);
        CHECK(co.builder_exact);
        for (u32 j = 0; j < 2; ++j) {
            r.counts[j] = azt_intake_builder_events(w, slots[j]);
            AzClassObservations x = {.builder_complete=!loaded, .builder_events=r.counts[j]};
            CHECK_EQ(az_classify_observed(w, slots[j], &x), r.counts[j] && !loaded ? AZ_ARCH_BUILDER : AZ_ARCH_NONE);
        }
        CHECK_EQ(st.archetype[AZ_ARCH_BUILDER], loaded ? 0 : (r.counts[0] != 0) + (r.counts[1] != 0));
        if (r.counts[0] && !loaded) {
            w->stat[(size_t)a * AZ_NSTAT + AZ_ST_TRADES] = 2;
            AzClassObservations x = {.builder_complete=true, .builder_events=r.counts[0]};
            CHECK_EQ(az_classify_observed(w, a, &x), AZ_ARCH_TRADER);
        }
        printf("REPORT %s suppress=%u loaded=%d ", c->name, suppress, (int)loaded);
        azt_builder_json(stdout, w, co.builder_covered, co.builder_partial, co.builder_positive, co.builder_exact);
        puts("");
        azt_builder_detach(); azt_intake_detach(); az_win_arena_free(&obs);
    }
#endif
    az_test_world_free(&t);
    return r;
}

int main(void)
{
    static const Case cases[] = {
        {"solo_empty", 0, 0, 0, 0, ACTIVE, 0, 10, 10, 0},
        {"solo_funded", 0, 0, 0, 1, ACTIVE, 0, 10, 10, 0},
        {"clipped", 252, 0, 0, 0, ACTIVE, 252, 255, 3, 0},
        {"full", 255, 0, 0, 0, ACTIVE, 255, 255, 0, 0},
        {"siblings", 243, 0, 1, 0, ACTIVE, 253, 255, 2, 2},
        {"redundant", 250, 0, 1, 0, ACTIVE, 255, 255, 0, 0},
        {"pending", 240, 8, 0, 0, ACTIVE, 248, 255, 7, 0},
        {"pending_u32_max", 0, UINT32_MAX, 1, 0, ACTIVE, 255, 255, 0, 0},
        {"egg", 0, 0, 0, 0, EGG, 0, 0, 0, 0},
        {"dormant", 0, 0, 0, 0, DORMANT, 0, 0, 0, 0},
        {"dead", 0, 0, 0, 0, DEAD, 0, 0, 0, 0},
        {"wrong_role", 0, 0, 0, 0, WRONG_ROLE, 0, 0, 0, 0},
        {"no_colony", 0, 0, 0, 0, NO_COLONY, 0, 0, 0, 0},
        {"off_phase", 0, 0, 0, 0, OFF_PHASE, 0, 0, 0, 0},
        {"zero_strength", 0, 0, 0, 0, NO_STRENGTH, 0, 0, 0, 0}
    };
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        const Case *c = &cases[i];
        const Result on = run_case(c, 2, true, false), off = run_case(c, 2, false, false);
        const Result no_a = run_case(c, 0, true, false), no_b = run_case(c, 1, true, false);
        CHECK_EQ(on.state, off.state); CHECK_EQ(on.matter, off.matter); CHECK_EQ(on.energy, off.energy);
        CHECK_EQ(on.physical, c->expected_after); CHECK_EQ(no_a.physical, c->expected_without_a);
        CHECK_EQ(on.physical - no_a.physical, c->credit_a); CHECK_EQ(on.physical - no_b.physical, c->credit_b);
        CHECK_EQ(on.counts[0], c->credit_a != 0); CHECK_EQ(on.counts[1], c->credit_b != 0);
        CHECK_EQ(on.grant, c->provision ? 7 : 0); CHECK_EQ(no_a.grant, on.grant);
        printf("CASE %s uids=%" PRIu64 ",%" PRIu64 " standing=%u after=%u without=%u,%u marginals=%u,%u occasions=%u,%u grant_mu=%u books=%" PRIu64 ",%" PRId64 " observer_hash=%016" PRIx64 " detached_hash=%016" PRIx64 "\n",
               c->name,on.uid[0],on.uid[1],c->standing,on.physical,no_a.physical,no_b.physical,
               on.physical-no_a.physical,on.physical-no_b.physical,on.counts[0],on.counts[1],on.grant,on.matter,on.energy,on.state,off.state);
    }
    (void)run_case(&cases[0], 2, true, true);
    return az_test_done();
}
