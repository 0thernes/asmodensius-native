/* Public az_birth_request with all provisions debited; only one child can fit. */
#include "az_test.h"

static void birth(u32 a, u32 m, u32 n, i32 credit)
{
    AzConfig cfg=az_test_config(7,63);cfg.cap=64;cfg.n_species=2;cfg.max_colonies=16;cfg.max_clades=32;
    cfg.gx_bits=cfg.gy_bits=cfg.gz_bits=2;
    AzArena arena={0};AzWinInfo info={0};
    CHECK(az_win_arena(&arena,az_world_bytes(&cfg),AZ_WIN_LOCK,&info) && info.arena_locked);
    if (!arena.base) return;
    AzWorld *w=az_world_create(&arena,&cfg,NULL);CHECK(w!=NULL);
    if(!w){az_win_arena_free(&arena);return;}
    az_test_uniform(w);az_grid_build(w);CHECK_EQ(w->n_slots,63);
    u32 mother=0,c=w->cell[mother],donor1=(c+1)%w->n_cells,donor2=(c+2)%w->n_cells;
    u32 *pa=&w->c_sp[(size_t)donor1*AZ_NSPECIES+AZ_SP_ORG_Y], *pm=&w->c_sp[(size_t)donor2*AZ_NSPECIES+AZ_SP_ORG_Y];
    *pa=a;*pm=m;w->c_heat[donor1]=(u32)credit;
    w->pool_a[mother]=w->pool_m[mother]=w->pool_t[mother]=0;w->energy[mother]=0;
    const u64 before_m=az_total_matter(w);const i64 before_e=az_total_energy(w);
    w->pool_a[mother]=*pa;*pa=0;w->pool_m[mother]=*pm;*pm=0;
    w->energy[mother]=(i32)w->c_heat[donor1];w->c_heat[donor1]=0;
    const u64 mass=(u64)a+m;
    w->mass[mother]=mass>UINT32_MAX ? UINT32_MAX : (u32)mass;
    CHECK_EQ(az_total_matter(w),before_m);CHECK_EQ(az_total_energy(w),before_e);
    CHECK(az_birth_request(w,mother,AZ_NONE,n,credit,a,m,0));
    w->energy[mother]-=credit;w->pool_a[mother]-=a;w->pool_m[mother]-=m;w->mass[mother]=0;
    CHECK_EQ(az_total_matter(w),before_m);CHECK_EQ(az_total_energy(w),before_e);
    const u64 uid=w->uid_next, births=w->totals.births;
    az_life_commit(w);
    CHECK_EQ(w->uid_next,uid+1);CHECK_EQ(w->n_slots,64);CHECK_EQ(w->totals.births,births+1);
    u32 child=63; CHECK_EQ(w->uid[child],uid);CHECK_EQ(w->parent_a[child],w->uid[mother]);
    CHECK(w->flags[child]&AZ_F_ALIVE);CHECK(w->c_solid[w->cell[child]]!=255);
    const u32 share_a=a/n, share_m=m/n, share_e=(u32)credit/n;
    const u64 cm=(u64)share_a+share_m;
    CHECK_EQ(w->pool_a[child],share_a);CHECK_EQ(w->pool_m[child],share_m);
    CHECK_EQ(w->pool_t[child],0);CHECK_EQ(w->mass[child],cm>UINT32_MAX ? UINT32_MAX:(u32)cm);
    CHECK_EQ(w->energy[child],(i32)share_e);
    const i64 refund=az_pending_species(w,c,AZ_SP_ORG_Y);
    const i64 heat=az_pending_heat(&w->c_acc[c]);
    CHECK_EQ(refund,(i64)(mass-cm));CHECK_EQ(heat,(i64)credit-share_e);
    const i64 dm=(i64)az_total_matter(w)-(i64)before_m,de=az_total_energy(w)-before_e;
    CHECK_EQ(dm,0);CHECK_EQ(de,0);az_field_commit(w,0,w->n_cells,0);az_tally_merge(w);
    CHECK_EQ(az_total_matter(w),before_m);CHECK_EQ(az_total_energy(w),before_e);
    CHECK_EQ(w->totals.field_underflow,0);CHECK_EQ(w->totals.dropped_events,0);
    printf("funded a=%u m=%u n=%u credit=%d child_a=%u child_m=%u child_mass=%u child_energy=%d pending_mu=%" PRId64
           " pending_heat=%" PRId64 " delta_m=%" PRId64 " delta_e=%" PRId64 " child_uid=%" PRIu64 " births=%" PRIu64 "\n",
           a,m,n,credit,w->pool_a[child],w->pool_m[child],w->mass[child],w->energy[child],refund,heat,dm,de,uid,w->totals.births);
    az_win_arena_free(&arena);
}
int main(void)
{
    birth(42,12,1,108);
    birth(0x80000000u,0x7fffffffu,1,108);
    birth(0x80000000u,0x80000000u,1,108);
    birth(0x80000001u,0x80000000u,1,108);
    birth(0x80000001u,0,0x80000001u,INT32_MAX);
    return az_test_done();
}
