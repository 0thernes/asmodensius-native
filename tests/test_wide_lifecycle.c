/* Page-locked, funded endpoint tests. Exit 1 means a release invariant failed. */
#include "az_test.h"

static void check_books(const AzWorld *w, u64 matter, i64 energy)
{
    CHECK_EQ(az_total_matter(w),matter);
    CHECK_EQ(az_total_energy(w),energy);
}

static void event_count(const AzWorld *w, u32 kind, u32 aux, u64 who, u32 expected)
{
    u32 count=0;
    const AzWorker *wk=&w->workers[0];
    for (u32 i=0; i<wk->n_events; i++) {
        const AzEvent *e=&wk->events[i];
        count += e->kind==kind && e->aux==aux && e->a==who;
    }
    CHECK_EQ(count,expected);
}

static void fixture(const char *mode, u32 units, u32 carrier)
{
    const bool death=!strcmp(mode,"death"), terrain=!strcmp(mode,"terrain");
    const bool request=!strcmp(mode,"wide-request");
    AzConfig cfg=az_test_config(7,terrain ? 2 : 64);
    cfg.cap=64; cfg.n_species=2; cfg.max_colonies=16; cfg.max_clades=32;
    cfg.gx_bits=cfg.gy_bits=cfg.gz_bits=2;
    AzArena arena={0}; AzWinInfo info={0};
    CHECK(az_win_arena(&arena,az_world_bytes(&cfg),AZ_WIN_LOCK,&info) && info.arena_locked);
    if (!arena.base) return;
    AzWorld *w=az_world_create(&arena,&cfg,NULL);CHECK(w!=NULL);
    if (!w) { az_win_arena_free(&arena); return; }
    az_test_uniform(w);az_grid_build(w);
    CHECK_EQ(w->n_slots,cfg.n_initial);CHECK_EQ(w->n_live,cfg.n_initial);
    const u32 s=0, c=w->cell[s], d1=(c+1)%w->n_cells, d2=(c+2)%w->n_cells;
    const u64 uid=w->uid[s], next=w->uid_next, start_births=w->totals.births;
    w->pool_a[s]=w->pool_t[s]=w->pool_m[s]=w->waste[s]=w->toxin[s]=0;
    w->energy[s]=216;
    u32 np=0;AzPart *part=az_parts(w,s,&np);CHECK(np!=0);if(!np){az_win_arena_free(&arena);return;}
    for(u32 i=0;i<np;i++)part[i].matter=0;
    for(u32 i=0;i<AZ_NINV;i++)w->inv[(size_t)s*AZ_NINV+i]=0;
    u32 *hold=&w->pool_a[s], sp=AZ_SP_ORG_Y;
    if(death){
        if(carrier==1)hold=&w->pool_m[s];
        if(carrier==2)hold=&w->pool_t[s];
        if(carrier==3){hold=&w->waste[s];sp=AZ_SP_WST_W;}
        if(carrier==4){hold=&w->toxin[s];sp=AZ_SP_TOX_Z;}
        if(carrier==5)hold=&part[0].matter;
        if(carrier>=6){u32 inv=carrier-6;hold=&w->inv[(size_t)s*AZ_NINV+inv];sp=az_inv_species[inv];}
    }
    u32 *donor1=&w->c_sp[(size_t)d1*AZ_NSPECIES+sp];
    u32 *donor2=&w->c_sp[(size_t)d2*AZ_NSPECIES+sp];
    *donor1=units;
    if (!death) *donor2=units;
    u64 m0=az_total_matter(w);i64 e0=az_total_energy(w);
    *hold=*donor1;*donor1=0;
    if(!death){w->pool_t[s]=*donor2;*donor2=0;}
    const u64 body=(u64)units*(death?1u:2u);
    w->mass[s]=body>UINT32_MAX ? UINT32_MAX:(u32)body;
    check_books(w,m0,e0);
    const u32 stored=w->c_sp[(size_t)c*AZ_NSPECIES+sp];
    const u32 heat=w->c_heat[c]; u64 deposit=units; i64 heat_in=0;
    if(death){
        az_request_death(w,s,AZ_DIE_AGE);az_life_range(w,s,s+1,0);heat_in=216;
        event_count(w,AZ_EV_DEATH,AZ_DIE_AGE,uid,1);
        CHECK(!(w->flags[s]&AZ_F_ALIVE));
    }else{
        w->flags[s]=AZ_F_ALIVE|AZ_F_RECEPT;w->trait[(size_t)s*AZ_TRAITS+AZ_T_PROVISION]=255;
        w->trait[(size_t)s*AZ_TRAITS+AZ_T_CLUTCH]=0;
        if(terrain)memset(w->c_solid,255,w->n_cells);
        if(request){
            CHECK(az_birth_request(w,s,AZ_NONE,1,108,units,0,units));
            w->pool_a[s]-=units;w->pool_t[s]-=units;w->energy[s]-=108;deposit=2ull*units;
        }else{
#ifdef AZ_TEST_REAL_BROOD
            CHECK_EQ(az_life_brood(w,s,AZ_NONE,0),1);
#else
            /* The reference brood provisions half of pool A and requires
             * 400 energy. Submit the same funded refusal at its public entry. */
            CHECK(az_birth_request(w,s,AZ_NONE,1,108,units,0,0));
            w->pool_a[s] -= units;
            w->energy[s] -= 108;
#endif
        }
        heat_in=108;CHECK_EQ(w->birth[s].energy,heat_in);
        CHECK_EQ((u64)w->birth[s].prov[0]+w->birth[s].prov[1]+w->birth[s].prov[2],deposit);
        check_books(w,m0,e0);
        az_life_commit(w);CHECK_EQ(w->birth[s].n,0);
        event_count(w,AZ_EV_REFUSED,terrain?AZ_REF_TERRAIN:AZ_REF_SLOT,terrain?uid:next,1);
        CHECK_EQ(w->uid_next,next);CHECK_EQ(w->n_slots,cfg.n_initial);CHECK_EQ(w->totals.births,start_births);
    }
    const i64 pending=az_pending_species(w,c,sp);
    const i64 before_commit_m=(i64)az_total_matter(w)-(i64)m0, before_commit_e=az_total_energy(w)-e0;
    CHECK_EQ(pending,(i64)deposit);CHECK_EQ(az_pending_heat(&w->c_acc[c]),heat_in);
    check_books(w,m0,e0);
    az_field_commit(w,0,w->n_cells,0);az_tally_merge(w);
    const i64 settled=(i64)w->c_sp[(size_t)c*AZ_NSPECIES+sp]+az_pending_species(w,c,sp);
    CHECK_EQ(settled,(i64)((u64)stored+deposit));
    CHECK_EQ((i64)w->c_heat[c]+az_pending_heat(&w->c_acc[c]),(i64)heat+heat_in);
    check_books(w,m0,e0);
    CHECK_EQ(w->totals.field_underflow,0);CHECK_EQ(w->totals.dropped_events,0);
    printf("case=%s carrier=%u sp=%u units=%u expected=%" PRIu64 " pending=%" PRId64 " delta_m=%" PRId64
           " delta_e=%" PRId64 " field_underflow=%" PRIu64 " drops=%" PRIu64 " heat=%" PRId64 "\n",
           mode,carrier,sp,units,deposit,pending,before_commit_m,before_commit_e,
           w->totals.field_underflow,w->totals.dropped_events,heat_in);
    az_win_arena_free(&arena);
}
#ifdef AZ_TEST_REAL_BROOD
/* Empty terrain isolates the dispersal draw from collision placement. */
static void check_dispersal(u32 zbits, u32 scale, u8 trait, const u32 retry_case[4])
{
    AzConfig cfg = az_test_config(19, 1);
    cfg.cap = 256; cfg.n_species = 1;
    cfg.gx_bits = 2; cfg.gy_bits = 3; cfg.gz_bits = zbits;
    cfg.max_colonies = 32; cfg.max_clades = 64;
    AzArena arena = {0};
    AzWinInfo info = {0};
    CHECK(az_win_arena(&arena, az_world_bytes(&cfg), AZ_WIN_LOCK, &info) && info.arena_locked);
    if (!info.arena_locked) return;
    AzWorld *w = az_world_create(&arena, &cfg, NULL);
    CHECK(w != NULL);
    if (!w) { az_win_arena_free(&arena); return; }
    az_test_uniform(w);
    memset(w->c_solid, 0, w->n_cells);
    const u32 middle = 1u << 31;
    w->pos_x[0] = w->pos_y[0] = w->pos_z[0] = middle;
    az_grid_build(w);
    w->param[AZ_WP_LIFE_DISPERSAL_SCALE] = scale;
    w->trait[AZ_T_DISPERSAL] = trait;
    const u32 shifts[] = {w->shift_x, w->shift_y, w->shift_z};
    u64 widths[3];
    const u64 reach = ((u64)trait + 1) * scale;
    for (u32 a = 0; a < 3; a++) widths[a] = ((1ull << shifts[a]) * reach / 128u) | 1u;
    if (retry_case) w->uid_next = retry_case[0];
    const u32 count = 48;
    const u64 matter = az_total_matter(w);
    const i64 energy = az_total_energy(w);
    CHECK(az_birth_request(w, 0, AZ_NONE, count, 480, 96, 0, 0));
    w->energy[0] -= 480; w->pool_a[0] -= 96; w->mass[0] -= 96;
    az_life_commit(w);
    CHECK_EQ(w->n_slots, count + 1);
    u32 above[3] = {0}, below[3] = {0}, legacy_equal[3] = {0};
    for (u32 s = 1; s < w->n_slots; s++) {
        const u32 pos[] = {w->pos_x[s], w->pos_y[s], w->pos_z[s]};
        if (s == 1 && retry_case) {
            CHECK_EQ(w->uid[s], retry_case[0]);
            for (u32 a = 0; a < 3; a++) CHECK_EQ(pos[a], retry_case[a + 1]);
        }
        u32 old[4];
        az_rand4(w, w->uid[s], AZ_RS_BIRTH, 0, old);
        for (u32 a = 0; a < 3; a++) {
            const i64 proposal = (i64)middle + (i64)(old[a] % widths[a]) - (i64)(widths[a] / 2);
            const u32 legacy = a == 2 ? (u32)az_clamp_i64(proposal, 65536, UINT32_MAX - 65536) : (u32)proposal;
            legacy_equal[a] += pos[a] == legacy;
            above[a] += pos[a] > middle;
            below[a] += pos[a] < middle;
            if (widths[a] <= UINT32_MAX) CHECK_EQ(pos[a], legacy);
        }
        CHECK(w->c_solid[w->cell[s]] != 255);
    }
    for (u32 a = 0; a < 3; a++) {
        if (widths[a] == 1) { CHECK_EQ(above[a] + below[a], 0); }
        else { CHECK(above[a] > 0 && below[a] > 0); }
        if (widths[a] > UINT32_MAX) CHECK(legacy_equal[a] < count);
        printf("spread layers=%u gain=%u trait=%u axis=%u width=%" PRIu64 " below=%u above=%u legacy_equal=%u/%u\n",
               w->gz, scale, trait, a, widths[a], below[a], above[a], legacy_equal[a], count);
    }
    az_field_commit(w, 0, w->n_cells, 0);
    az_tally_merge(w);
    check_books(w, matter, energy);
    CHECK_EQ(w->totals.field_underflow, 0); CHECK_EQ(w->totals.dropped_events, 0);
    az_win_arena_free(&arena);
}
#endif

int main(void)
{
#ifdef AZ_TEST_REAL_BROOD
    check_dispersal(1, 0, 255, NULL);
    check_dispersal(2, 4, 0, NULL);
    check_dispersal(2, 4, 127, NULL); /* wide x and z alongside a historical y range */
    check_dispersal(2, 4, 255, NULL);
    check_dispersal(1, 4, 255, NULL);
    check_dispersal(2, 4096, 255, NULL);
    /* Each first UID rejects the first 64-bit draw on one axis (x,y,z).
     * Integers from Philox, reduced with independent arbitrary-precision
     * arithmetic and then wrapped/clipped, fix all three final coordinates. */
    const u32 retry_cases[][4] = {
        {3161989u, 2951683518u, 1826261085u, 4294901759u},
        {2371759u, 3926575393u, 1204718619u, 4294901759u},
        {911337u, 1405043337u, 2603915255u, 65536u},
    };
    for (u32 i = 0; i < AZ_LEN(retry_cases); i++) check_dispersal(1, 4096, 255, retry_cases[i]);
#endif
    const u32 boundary=(u32)INT32_MAX+1u;
    fixture("capacity",INT32_MAX,0);fixture("capacity",boundary,0);
    fixture("terrain",INT32_MAX,0);fixture("terrain",boundary,0);
    fixture("wide-request",boundary,0);
    for(u32 k=0;k<6u+AZ_NINV;k++){fixture("death",INT32_MAX,k);fixture("death",boundary,k);}
    fixture("terrain",UINT32_MAX,0);
    fixture("wide-request",UINT32_MAX,0);
    for (u32 k=0;k<6u+AZ_NINV;k++) fixture("death",UINT32_MAX,k);
    return az_test_done();
}
