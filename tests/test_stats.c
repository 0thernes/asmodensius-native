#include "az_test.h"
#include "../core/az_stats_traits.h"

static int order(const void *a, const void *b)
{
    const u64 x = *(const u64 *)a, y = *(const u64 *)b;
    return (x > y) - (x < y);
}

static void exact_ranks(const AzWorld *w, const AzStats *st)
{
    static const u32 pct[] = {0,5,25,50,75,95,100};
    u64 ref[AZ_STATS_DISTRIBUTIONS][256] = {{0}};
    u32 n = 0, colonies = 0;
    for (u32 s = 0; s < w->n_slots; ++s) if (w->flags[s] & AZ_F_ALIVE) {
        CHECK(n < 256);
        if (n >= 256) return;
        if (w->blk[s]) {
            u32 ga, gb, bytes, parts, words;
            az_genes(w,s,0,&ga); az_genes(w,s,1,&gb);
            AzBrain *brain = az_brain(w,s,&bytes);
            const AzPart *p = az_parts(w,s,&parts);
            az_memory(w,s,&words);
            ref[AZ_STATS_GENES][n] = (u64)ga + gb;
            ref[AZ_STATS_NEURONS][n] = bytes >= sizeof(*brain) ? (u64)brain->n_hb * 8 : 0;
            ref[AZ_STATS_TILES][n] = bytes >= sizeof(*brain) ? (u64)brain->n_t1 + brain->n_t2 : 0;
            ref[AZ_STATS_PARTS][n] = parts;
            for (u32 j = 0; j < parts; ++j) ref[AZ_STATS_BODY_MATTER][n] += p[j].matter;
            ref[AZ_STATS_MEMORY][n] = words;
        }
        ref[AZ_STATS_DEPTH][n++] = w->generation[s];
    }
    for (u32 i = 1; i < w->n_colonies; ++i) if (w->colonies[i].alive) {
        CHECK(colonies < 256);
        if (colonies >= 256) return;
        u32 members = 0;
        for (u32 s = 0; s < w->n_slots; ++s)
            members += (w->flags[s] & AZ_F_ALIVE) && w->colony[s] == i;
        ref[AZ_STATS_COLONY_SIZE][colonies++] = members;
    }
    CHECK_EQ(n,st->n_live); CHECK_EQ(colonies,st->n_colonies);
    for (u32 k = 0; k < AZ_STATS_DISTRIBUTIONS; ++k) {
        const u32 count = k == AZ_STATS_COLONY_SIZE ? colonies : n;
        qsort(ref[k],count,sizeof(u64),order);
        for (u32 j = 0; j < 7; ++j)
            CHECK_EQ(st->quantile[k][j], count ? ref[k][pct[j]*(count-1)/100] : 0);
    }
}

static void checked_collect(AzTestWorld *t, AzStats *st)
{
    const size_t bytes = (size_t)t->arena.used;
    void *copy = malloc(bytes);
    CHECK(copy != NULL);
    if (!copy) return;
    const u64 hash = az_state_hash(t->w);
    memcpy(copy,t->arena.base,bytes);
    az_stats_collect(t->w,st);
    CHECK(memcmp(copy,t->arena.base,bytes) == 0);
    CHECK_EQ(az_state_hash(t->w),hash);
    free(copy);
    exact_ranks(t->w,st);
}

static void traits_wide(void)
{
    AzStats st = {0};
    u64 sum = 0, squares = 0;
    volatile u8 low = 250;
    for (u32 i = 0; i < 20000000; ++i) {
        u8 x = (u8)(low + (i & 1u) * 5u);
        add_trait_values(&sum,&squares,&x,1);
    }
    CHECK_EQ(sum,UINT64_C(5050000000));
    trait_moments(&st,0,20000000,sum,squares);
    CHECK_EQ(st.trait_mean[0],252); CHECK_EQ(st.trait_var[0],6);
}

static void observed_categories(AzWorld *w)
{
    u8 *body=w->clades[w->clade[0]].body, *trait=w->trait;
    u8 original_body[16], original_traits[AZ_TRAITS];
    memcpy(original_body,body,16); memcpy(original_traits,trait,AZ_TRAITS);
    memset(body,0,16); memset(trait,128,AZ_TRAITS);
    const u32 original=az_look_class(w,0);
    const u8 keys[]={AZ_B_SHAPE_M,AZ_B_SEGMENTS,AZ_B_SEGMENTS,AZ_B_SYMMETRY};
    const u8 inputs[]={8,6,1,2};
    for(u32 i=0;i<4;++i) {
        body[keys[i]]=inputs[i]; CHECK_EQ(az_look_class(w,0),original); body[keys[i]]=0;
    }
    const u8 unused[]={AZ_T_SHAPE_M,AZ_T_SHAPE_N,AZ_T_SPIKES};
    for(u32 i=0;i<3;++i) {
        trait[unused[i]]=0; CHECK_EQ(az_look_class(w,0),original); trait[unused[i]]=128;
    }
    body[AZ_B_HUE]=64; CHECK(az_look_class(w,0)!=original);
    memcpy(body,original_body,16); memcpy(trait,original_traits,AZ_TRAITS);
    u32 *v=w->stat; memset(v,0,AZ_NSTAT*sizeof(u32));
    AzClassObservations evidence = {.predation_events = 1};
    v[AZ_ST_ATTACKS] = v[AZ_ST_WINS] = 1;
    CHECK_EQ(az_classify_observed(w,0,&evidence),AZ_ARCH_WARRIOR);
    evidence.predation_complete = true;
    CHECK_EQ(az_classify_observed(w,0,&evidence),AZ_ARCH_PREDATOR);
    v[AZ_ST_ATTACKS] = 2;
    CHECK_EQ(az_classify_observed(w,0,&evidence),AZ_ARCH_WARRIOR);
    v[AZ_ST_ATTACKS] = v[AZ_ST_WINS] = 0;
    v[AZ_ST_PARTNERS]=2; v[AZ_ST_MATES]=7; CHECK_EQ(az_classify(w,0),AZ_ARCH_NONE);
    v[AZ_ST_PARTNERS]=1; CHECK_EQ(az_classify(w,0),AZ_ARCH_NONE);
    v[AZ_ST_TRADES]=1; v[AZ_ST_TRAVEL]=65535; w->travel_carry[0]=2;
    CHECK_EQ(az_classify(w,0),AZ_ARCH_NOMAD);
    v[AZ_ST_TRAVEL]+=2;w->travel_carry[0]=0;CHECK_EQ(az_classify(w,0),AZ_ARCH_NOMAD);
    memset(v,0,AZ_NSTAT*sizeof(u32));
}

static void extreme_world(void)
{
    AzConfig cfg = az_test_config(5731,96);
    AzTestWorld t;
    CHECK(az_test_world(&t,&cfg,1));
    if (!t.w) return;
    AzWorld *w = t.w;
    AzStats st;
    CHECK(t.info.arena_locked);
    CHECK_EQ(w->n_slots,96);
    if (w->n_slots != 96) { az_test_world_free(&t); return; }
    observed_categories(w);
    w->tick = 100;
    u64 rng=9111, ages=0;
    for (u32 i=0;i<96;++i) {
        w->flags[i] = AZ_F_ALIVE;
        w->archetype[i] = AZ_ARCH_SWITCHER;
        w->generation[i] = (u32)az_test_rand64(&rng);
        w->birth_tick[i] = i; ages += 100-i;
        w->bond[i] = i & 1u;
        w->lineage[i] = (u32[]){0,127,128,129,4000000000u,UINT32_MAX}[i%6];
        w->trait[(size_t)i*AZ_TRAITS] = (u8)((i & 1u) ? 250 : 255);
        memset(w->stat+(size_t)i*AZ_NSTAT,0,sizeof(u32)*AZ_NSTAT);
    }
    w->flags[0] |= AZ_F_EGG; w->flags[1] |= AZ_F_JUV; w->flags[2] |= AZ_F_SENESC; w->flags[3] |= AZ_F_DORMANT;
    w->generation[0]=0; w->generation[1]=UINT32_MAX;
    w->n_colonies=4;
    for (u32 i=1;i<4;++i) { w->colonies[i].alive=1; w->colonies[i].members=i*1000; }
    CHECK_EQ(az_classify(w,1),AZ_ARCH_NONE);
    w->stat[AZ_NSTAT+AZ_ST_TRADES]=2;
    w->stat[AZ_NSTAT+AZ_ST_SIGNALS]=2;
    CHECK_EQ(az_classify(w,1),AZ_ARCH_TRADER);
    w->stat[AZ_NSTAT+AZ_ST_SIGNALS]=3;
    CHECK_EQ(az_classify(w,1),AZ_ARCH_SIGNALLER);
    for(u32 j=0;j<2;++j) {
        const u32 dest=33+j;
        memcpy(w->trait+(size_t)dest*AZ_TRAITS,w->trait+AZ_TRAITS,AZ_TRAITS);
        memcpy(w->stat+(size_t)dest*AZ_NSTAT,w->stat+AZ_NSTAT,AZ_NSTAT*sizeof(u32));
        w->clade[dest]=w->clade[1];
        w->lineage[dest]=j ? 128 : UINT32_MAX;
        CHECK_EQ(az_look_class(w,dest),az_look_class(w,1));
        CHECK_EQ(az_classify(w,dest),AZ_ARCH_SIGNALLER);
    }
    w->trait[34*AZ_TRAITS] = 255; /* restore the moment fixture */
    memset(w->stat,0,AZ_NSTAT*sizeof(u32)); w->stat[AZ_ST_EAT_X]=9;
    for(u32 i=0;i<96;++i) {
        u32 parts; AzPart *p=az_parts(w,i,&parts);
        for(u32 j=0;j<parts;++j) p[j].matter=(u32)az_test_rand64(&rng);
    }
    checked_collect(&t,&st);
    CHECK_EQ(st.n_eggs,1); CHECK_EQ(st.n_juv,1); CHECK_EQ(st.n_senescent,1); CHECK_EQ(st.n_dormant,1);
    CHECK_EQ(st.n_bonded,48); CHECK_EQ(st.n_species_live,6); CHECK_EQ(st.n_species_beyond,4);
    CHECK(st.n_looks>=1); CHECK_EQ(st.n_looks_beyond,0); CHECK_EQ(st.n_diet_unknown,95); CHECK_EQ(st.diet[1],1);
    CHECK_EQ(st.trait_mean[0],252); CHECK_EQ(st.trait_var[0],6); CHECK_EQ(st.archetype[AZ_ARCH_SWITCHER],0);
    CHECK_EQ(st.mean_age_q8,ages*256/96); CHECK_EQ(st.max_generation,UINT32_MAX);
    CHECK(st.quantile[AZ_STATS_BODY_MATTER][6]>UINT32_MAX);
    /* Flooring E[(X-floor(mean))^2] overstates the exact variance here. */
    for (u32 i = 0; i < 96; ++i) w->flags[i] = i < 3 ? AZ_F_ALIVE : 0;
    w->trait[0] = 0;
    w->trait[AZ_TRAITS] = 2;
    w->trait[2 * AZ_TRAITS] = 3;
    az_stats_collect(w, &st);
    CHECK_EQ(st.trait_mean[0], 1);
    CHECK_EQ(st.trait_var[0], 1); /* exact population variance = 14/9 */
    for (u32 i = 0; i < 96; ++i) w->flags[i] = AZ_F_ALIVE;
    /* Singleton without a block, empty world, and an age sum exceeding 64 bits. */
    for(u32 i=0;i<96;++i) w->birth_tick[i]=0;
    w->tick=UINT64_MAX;
    checked_collect(&t,&st);
    CHECK_EQ(st.mean_age_q8,UINT64_MAX);CHECK_EQ(st.age_q8_saturated,1);
    for(u32 i=1;i<96;++i) w->flags[i]=0;
    CHECK(az_blk_resize(w,0,AZ_SEG_GEN_A,0));
    CHECK(az_blk_resize(w,0,AZ_SEG_GEN_B,(u32)sizeof(AzGene)));
    CHECK(az_blk_resize(w,0,AZ_SEG_BODY,(u32)sizeof(AzPart)));
    CHECK(az_blk_resize(w,0,AZ_SEG_MEMORY,0));
    CHECK(az_blk_resize(w,0,AZ_SEG_BRAIN,az_brain_bytes(0,0,0)));
    u32 bb; AzBrain *br=az_brain(w,0,&bb);
    memset(br,0,bb);
    checked_collect(&t,&st);
    CHECK_EQ(st.quantile[AZ_STATS_GENES][3],1);
    CHECK_EQ(st.quantile[AZ_STATS_NEURONS][3],0);
    CHECK_EQ(st.quantile[AZ_STATS_TILES][3],0);
    CHECK_EQ(st.quantile[AZ_STATS_PARTS][3],1);
    CHECK_EQ(st.quantile[AZ_STATS_MEMORY][3],0);
    CHECK(az_blk_resize(w,0,AZ_SEG_MEMORY,24));
    checked_collect(&t,&st); CHECK_EQ(st.quantile[AZ_STATS_MEMORY][3],3);
    az_blk_release(w,0); w->flags[0]=AZ_F_ALIVE|AZ_F_EGG;
    checked_collect(&t,&st);
    CHECK_EQ(st.n_live,1); CHECK_EQ(st.quantile[AZ_STATS_PARTS][6],0); CHECK_EQ(st.n_eggs,1);
    w->flags[0]=0; w->n_colonies=1;
    checked_collect(&t,&st);
    CHECK_EQ(st.n_live,0); CHECK_EQ(st.n_looks,0);
    az_test_world_free(&t);
}

static void colony_retirement(void)
{
    AzConfig cfg = az_test_config(21, 96);
    cfg.tier1_period = 1;
    cfg.tier2_period = 16;
    AzTestWorld t;
    CHECK(az_test_world(&t, &cfg, 1));
    if (!t.w) return;
    AzWorld *w = t.w;
    w->param[AZ_WP_COLONY_FOUND] = 0;
    w->param[AZ_WP_COLONY_LEAVE] = 0;
    CHECK_EQ(az_colony_found(w, 0), 1);
    const u64 matter = az_total_matter(w);
    const i64 energy = az_total_energy(w);
    az_tick(w); /* Tier 2 recorded its one founder. */
    AzStats st;
    az_stats_collect(w, &st);
    CHECK_EQ(st.n_colonial, 1);
    CHECK_EQ(st.quantile[AZ_STATS_COLONY_SIZE][6], 1);
    w->param[AZ_WP_LIFE_HAZARD] = 1u << 20;
    for (u32 i = 0; i < 15; ++i) az_tick(w);
    CHECK_EQ(w->tick, 16);
    CHECK_EQ(w->colonies[1].members, 1);
    CHECK(w->totals.deaths > 0);
    az_stats_collect(w, &st);
    CHECK_EQ(st.n_colonies, 1);
    CHECK_EQ(st.n_colonial, 0);
    for (u32 j = 0; j < AZ_STATS_QUANTILES; ++j)
        CHECK_EQ(st.quantile[AZ_STATS_COLONY_SIZE][j], 0);
    CHECK_EQ(az_total_matter(w), matter);
    CHECK_EQ(az_total_energy(w), energy);
    az_test_world_free(&t);
}


static void colony_rosters(void)
{
    enum { COLONIES = 260, WIDE_ID = 65539 };
    AzConfig cfg = az_test_config(21, 1025);
    cfg.max_colonies = WIDE_ID + 1;
    AzTestWorld t;
    CHECK(az_test_world(&t, &cfg, 3));
    if (!t.w) return;
    CHECK(t.info.arena_locked);
    AzWorld *w = t.w;
    w->n_colonies = cfg.max_colonies;
    u32 ids[COLONIES];
    for (u32 i = 0; i < COLONIES; ++i) {
        ids[i] = i == COLONIES - 1 ? WIDE_ID : 1 + i * 2;
        w->colonies[ids[i]].alive = 1;
        w->colonies[ids[i]].members = 777; /* previous Tier 2 roster */
    }
    for (u32 scenario = 0; scenario < 2; ++scenario) {
        const u32 n = scenario ? 3 : COLONIES;
        if (scenario) {
            for (u32 i = 2; i + 1 < COLONIES; ++i) w->colonies[ids[i]].alive = 0;
            ids[2] = WIDE_ID;
        }
        u64 expected[COLONIES] = {0};
        for (u32 s = 0; s < w->n_slots; ++s) {
            const u32 i = scenario ? (s < 350 ? 1u : 2u) : (s * 13u) % (n - 1u);
            w->colony[s] = ids[i];
            if (s == 0) w->flags[s] &= (u8)~AZ_F_ALIVE;
            if (s == 1) w->colony[s] = 2; /* inactive gap in the sparse group */
            if ((w->flags[s] & AZ_F_ALIVE) && w->colony[s] == ids[i]) ++expected[i];
        }
        qsort(expected, n, sizeof(*expected), order);
        for (u32 permuted = 0; permuted < 2; ++permuted) {
            AzStats st;
            az_stats_collect(w, &st);
            CHECK_EQ(st.n_colonies, n);
            static const u32 pct[] = {0,5,25,50,75,95,100};
            for (u32 j = 0; j < AZ_STATS_QUANTILES; ++j)
                CHECK_EQ(st.quantile[AZ_STATS_COLONY_SIZE][j], expected[pct[j]*(n-1)/100]);
            if (!permuted) az_grid_reorder(w);
        }
    }
    /* 513 living slots cross 2^64 in their sum while the mean fits Q8.
       257 ages equal tick; 256 are one tick younger. */
    w->tick = (UINT64_C(1) << 55) + 17;
    for (u32 s = 0; s < w->n_slots; ++s) {
        w->flags[s] = s < 513 ? AZ_F_ALIVE : 0;
        w->birth_tick[s] = s & 1u;
    }
    AzStats age_sample;
    az_stats_collect(w, &age_sample);
    CHECK_EQ(age_sample.n_live, 513);
    CHECK_EQ(age_sample.age_q8_saturated, 0);
    CHECK_EQ(age_sample.mean_age_q8, (w->tick << 8) - 128);
    az_test_world_free(&t);
}


static void schedule(u32 workers, u32 reorder, AzStats *st)
{
    AzConfig cfg=az_test_config(7711,49); cfg.reorder_period=reorder;
    AzTestWorld t;
    CHECK(az_test_world(&t,&cfg,workers));
    if (!t.w) return;
    CHECK(t.info.arena_locked);
    for(u32 i=0;i<6;++i) az_tick(t.w);
    checked_collect(&t,st);
    az_test_world_free(&t);
}

static void activity_width(void)
{
    u32 counters[AZ_NSTAT] = {0};
    u32 residual_travel = 0;
    AzWorld w = {0};
    w.stat = counters;
    w.travel_carry = &residual_travel;
    AzClassObservations x = {0};
    x.intake_complete = true;
    x.predation_complete = true;
    x.commons_complete = true;
    x.intake_occasions[1] = UINT32_MAX;
    x.predation_events = (u64)UINT32_MAX + 1;
    CHECK_EQ(az_classify_observed(&w, 0, &x), AZ_ARCH_PREDATOR);
    x.intake_occasions[1] = UINT64_MAX - 1;
    x.predation_events = UINT64_MAX;
    CHECK_EQ(az_classify_observed(&w, 0, &x), AZ_ARCH_PREDATOR);
    x.intake_occasions[1] = UINT64_MAX;
    CHECK_EQ(az_classify_observed(&w, 0, &x), AZ_ARCH_CHEMOTROPH);
    x.intake_occasions[1] = 0;
    x.predation_events = (UINT64_C(1) << 48);
    x.withheld_contribution_events = x.predation_events + 1;
    CHECK_EQ(az_classify_observed(&w, 0, &x), AZ_ARCH_FREELOADER);
    x.commons_complete = false;
    CHECK_EQ(az_classify_observed(&w, 0, &x), AZ_ARCH_PREDATOR);
    x.predation_complete = false;
    CHECK_EQ(az_classify_observed(&w, 0, &x), AZ_ARCH_NONE);
    x.intake_occasions[0] = 2;
    counters[AZ_ST_TRAVEL] = 2u << 16;
    CHECK_EQ(az_classify_observed(&w, 0, &x), AZ_ARCH_PRODUCER);
    residual_travel = 1;
    CHECK_EQ(az_classify_observed(&w, 0, &x), AZ_ARCH_NOMAD);
    x.intake_occasions[0] = 0;
    counters[AZ_ST_OFFSPRING] = 2;
    CHECK_EQ(az_classify_observed(&w, 0, &x), AZ_ARCH_NOMAD);
    residual_travel = 0;
    CHECK_EQ(az_classify_observed(&w, 0, &x), AZ_ARCH_NOMAD);
    counters[AZ_ST_TRAVEL]--;
    CHECK_EQ(az_classify_observed(&w, 0, &x), AZ_ARCH_BROODER);
    /* A possible waste vote must strictly beat the known winner under the
       same lower-id and fractional-travel rules as a confirmed vote. */
    memset(counters, 0, sizeof counters);
    memset(&x, 0, sizeof x);
    residual_travel = 1;
    counters[AZ_ST_TRAVEL] = 2u << 16;
    x.intake_complete = true;
    x.waste_possible_occasions = 2;
    CHECK_EQ(az_classify_observed(&w, 0, &x), AZ_ARCH_NOMAD);
    residual_travel = 0;
    CHECK_EQ(az_classify_observed(&w, 0, &x), AZ_ARCH_NONE);
    x.intake_occasions[3] = 2;
    CHECK_EQ(az_classify_observed(&w, 0, &x), AZ_ARCH_DECOMPOSER);
    counters[AZ_ST_TRAVEL] = 0;
    x.intake_occasions[3] = 0;
    x.intake_occasions[0] = UINT64_MAX - 1;
    x.waste_possible_occasions = UINT64_MAX;
    CHECK_EQ(az_classify_observed(&w, 0, &x), AZ_ARCH_NONE);
    x.intake_occasions[0] = UINT64_MAX;
    CHECK_EQ(az_classify_observed(&w, 0, &x), AZ_ARCH_PRODUCER);
    x.intake_complete = false;
    counters[AZ_ST_OFFSPRING] = 3;
    CHECK_EQ(az_classify_observed(&w, 0, &x), AZ_ARCH_BROODER);

}



int main(void)
{
    activity_width();
    colony_retirement();
    colony_rosters();
    traits_wide();
    extreme_world();
    AzStats reference, other;
    schedule(1,1,&reference);
    schedule(3,4,&other); CHECK(memcmp(&reference,&other,sizeof(other))==0);
    schedule(8,0,&other); CHECK(memcmp(&reference,&other,sizeof(other))==0);
    return az_test_done();
}
