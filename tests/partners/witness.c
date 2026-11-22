/* Native resolved pairs on a generated locked world. Intent, funding, and
   confirmed bond direction are exercised independently. */
#include "az_test.h"
#include "../../tools/az_teaching_observer.h"
#include "../../core/az_partner_hooks.h"
#include <windows.h>
#include <psapi.h>

static u8 *trait(AzWorld *w, u32 s) { return w->trait + (size_t)s*AZ_TRAITS; }
static u8 *offer(AzWorld *w, u32 s) { return w->offer + (size_t)s*AZ_OFFER; }
static void adult(AzWorld *w, u32 s, u32 mode, bool sire)
{
    w->flags[s] = AZ_F_ALIVE | AZ_F_RECEPT;
    w->phen[(size_t)s*AZ_PHEN+AZ_P_MATE] = (u8)(mode | (sire ? 8u:0u));
    memset(trait(w,s),0,AZ_TRAITS);
    trait(w,s)[AZ_T_BOND]=255;
    trait(w,s)[AZ_T_PROVISION]=255;
    offer(w,s)[AZ_O_DISP]=AZ_OD_RECEPTIVE|AZ_OD_BREEDER;
    memset(w->stat+(size_t)s*AZ_NSTAT,0,AZ_NSTAT*sizeof *w->stat);
    w->travel_carry[s]=0;
}
static AzClassObservations seen(AzWorld *w,u32 s)
{
    AzClassObservations e={0};
    const u64 before=az_state_hash(w);
    azt_partner_prepare(w);
    azt_partner_fill(w,s,&e);
    CHECK_EQ(az_state_hash(w),before);
    return e;
}
static void pair(AzWorld *w,u32 sire,u32 dam)
{
    w->flags[dam]|=AZ_F_RECEPT;
    az_grid_build(w);
    CHECK(az_mate_score(w,dam,sire,AZ_ACT_MATE)>=0);
    az_mate_resolve(w,sire,dam,AZ_ACT_MATE,true,0);
    az_mate_resolve(w,dam,sire,AZ_ACT_MATE,false,0);
    CHECK_EQ(w->birth[dam].father,sire);
    CHECK(w->birth[dam].n>0);
    azt_partner_tick(w);
    az_life_commit(w);
    az_tally_merge(w);
    az_ledger_flush(w);
    ++w->tick;
}
int main(void)
{
    AzConfig cfg=az_test_config(19,24);
    AzWinInfo info={0};
    AzHost *host=az_win_host_create(1,0,&info);
    AzArena arena;
    AztTeaching observer;
    const u64 bytes=az_world_bytes(&cfg);
    if (!host || !azt_teaching_arena(&arena,bytes,cfg.cap,AZ_WIN_LOCK,&info,true,&observer) || !info.arena_locked) return 80;
    AzArena prefix=arena;
    prefix.size=bytes;
    AzWorld *w=az_world_create(&prefix,&cfg,host);
    if (!w) return 81;
    az_worldgen(w);
    CHECK_EQ(observer.partner_bytes,azt_partner_bytes(w->cap));
    CHECK(!azt_partner_attach(w,true,(u8 *)observer.partner_tail+1,observer.partner_bytes));
    CHECK(!azt_partner_attach(w,true,observer.partner_tail,observer.partner_bytes-1));
    CHECK(!azt_partner_attach(w,true,NULL,0));
    CHECK(!seen(w,0).partner_complete);
    CHECK(azt_partner_attach(w,true,observer.partner_tail,observer.partner_bytes));
    CHECK(!azt_partner_attach(w,true,observer.partner_tail,observer.partner_bytes));
    const uintptr_t lo=(uintptr_t)observer.partner_tail,hi=lo+observer.partner_bytes;
    CHECK(lo>=(uintptr_t)arena.base+bytes && hi<=(uintptr_t)arena.base+arena.size);
    u32 pages=0;
    for (uintptr_t p=lo&~(uintptr_t)4095;p<hi;p+=4096) {
        PSAPI_WORKING_SET_EX_INFORMATION v={0};v.VirtualAddress=(void*)p;
        CHECK(QueryWorkingSetEx(GetCurrentProcess(),&v,sizeof v) && v.VirtualAttributes.Valid && v.VirtualAttributes.Locked);
        ++pages;
    }
    const u32 center=0,a=1,b=2,solo=3,other=4,third=5;
    for (u32 s=0;s<6;++s) adult(w,s,s<3 ? AZ_MM_POLYGYN:AZ_MM_MONOG,s==center || s==solo || s==third);
    const u64 matter=az_total_matter(w);
    const i64 energy=az_total_energy(w);
    AzClassObservations e=seen(w,center);
    CHECK(e.partner_complete); CHECK_EQ(az_classify_observed(w,center,&e),AZ_ARCH_NONE);
    /* A fertile chooser can refuse a low ranked offer repeatedly, then accept
       the same candidate at a higher rank. Zero partners does not identify abstention. */
    trait(w,a)[AZ_T_PREF_RANK]=255; trait(w,a)[AZ_T_CHOOSY]=100;
    offer(w,center)[AZ_O_RANK]=0;
    az_grid_build(w);
    CHECK(az_mate_score(w,a,center,AZ_ACT_MATE)<0);
    CHECK(az_mate_score(w,a,center,AZ_ACT_MATE)<0);
    offer(w,center)[AZ_O_RANK]=255;
    CHECK(az_mate_score(w,a,center,AZ_ACT_MATE)>=0);
    trait(w,a)[AZ_T_CHOOSY]=0;
    pair(w,center,a);
    CHECK(!w->bond[center] && w->bond[a] && w->partner[a]==w->uid[center]);
    e=seen(w,center);CHECK(!e.overlapping_bonded_mates && !e.exclusive_bonded_matings);
    pair(w,center,a);
    e=seen(w,center);CHECK(!e.overlapping_bonded_mates && !e.exclusive_bonded_matings);
    pair(w,center,b);
    CHECK(w->bond[a] && w->bond[b] && w->partner[a]==w->uid[center] && w->partner[b]==w->uid[center]);
    e=seen(w,center);CHECK_EQ(e.overlapping_bonded_mates,2);
    CHECK_EQ(az_classify_observed(w,center,&e),AZ_ARCH_POLYGAMIST);
    printf("concurrent ");azt_partner_json(stdout,w);putchar('\n');
    pair(w,third,a); /* first source physically switches to a different sire */
    CHECK_EQ(w->partner[a],w->uid[third]);
    e=seen(w,center);CHECK_EQ(e.overlapping_bonded_mates,0);
    CHECK_EQ(az_classify_observed(w,center,&e),AZ_ARCH_NONE);
    pair(w,solo,other);
    e=seen(w,solo);CHECK_EQ(e.exclusive_bonded_matings,0);
    pair(w,solo,other);
    e=seen(w,solo);CHECK_EQ(e.exclusive_bonded_matings,1);
    CHECK_EQ(az_classify_observed(w,solo,&e),AZ_ARCH_MONOGAMIST);
    printf("exclusive ");azt_partner_json(stdout,w);putchar('\n');
    pair(w,solo,b);
    e=seen(w,solo);CHECK_EQ(e.exclusive_bonded_matings,0);
    CHECK_EQ(e.overlapping_bonded_mates,2); /* both former and new mate still bond to this UID */
    /* A stale directed pointer from a dead mate no longer establishes overlap. */
    az_life_kill(w,b,0);e=seen(w,solo);CHECK_EQ(e.overlapping_bonded_mates,0);
    CHECK_EQ(az_total_matter(w),matter);CHECK_EQ(az_total_energy(w),energy);
    azt_partner_detach();
    CHECK(azt_partner_attach(w,false,observer.partner_tail,observer.partner_bytes));
    pair(w,solo,other);pair(w,solo,other);
    e=seen(w,solo);CHECK(!e.partner_complete);CHECK_EQ(e.exclusive_bonded_matings,0);
    CHECK_EQ(az_classify_observed(w,solo,&e),AZ_ARCH_NONE);
    /* A material-poor bearer can still resolve a two-sided mating. Both
       native mate counters advance even though zero brood is funded. */
    const u32 dam=6, sire=7;
    adult(w,dam,AZ_MM_MONOG,false);adult(w,sire,AZ_MM_MONOG,true);
    const u32 mass_a=w->pool_a[dam], mass_t=w->pool_t[dam];
    w->pool_a[dam]=0;w->pool_t[dam]=0;
    az_grid_build(w);
    CHECK(az_mate_score(w,dam,sire,AZ_ACT_MATE)>=0);
    az_mate_resolve(w,dam,sire,AZ_ACT_MATE,false,0);
    az_mate_resolve(w,sire,dam,AZ_ACT_MATE,true,0);
    CHECK_EQ(w->birth[dam].n,0);
    CHECK_EQ(w->stat[(size_t)dam*AZ_NSTAT+AZ_ST_MATES],1);
    CHECK_EQ(w->stat[(size_t)sire*AZ_NSTAT+AZ_ST_MATES],1);
    w->pool_a[dam]=mass_a;w->pool_t[dam]=mass_t;
    azt_partner_tick(w);++w->tick;
    azt_partner_prepare(w);printf("unfunded ");azt_partner_json(stdout,w);putchar('\n');
    /* A deliberately skipped observer boundary invalidates the timeline. */
    w->tick++;azt_partner_tick(w);w->tick++;CHECK(!seen(w,solo).partner_complete);
    printf("native relationship controls, %u locked tail pages\n",pages);
    azt_partner_detach();az_win_arena_free(&arena);az_win_host_destroy(host);
    return az_test_done();
}
