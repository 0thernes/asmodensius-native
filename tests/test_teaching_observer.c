/* Production TEACH resolutions and canonical event flush, with paired learner
   effect, action-only control, exact books, single-allocation residency and
   joined-scorer tests on real per-UID columns. */
#include "az_test.h"
#include "../tools/az_teaching_observer.h"
#include <windows.h>
#include <psapi.h>

static u32 forwarded, receipts;
static u64 actor, learner;
static void capture(void *ctx, const AzEvent *ev, u32 n)
{
    (void)ctx;
    for (u32 i = 0; i < n; ++i) if (ev[i].kind == AZ_EV_TEACH) {
        ++receipts;
        actor = ev[i].a;
        learner = ev[i].b;
    }
    ++forwarded;
}

static void teach(AzWorld *w)
{
    az_social_resolve(w, 0, 1, AZ_ACT_TEACH, true, 0);
    az_social_resolve(w, 1, 0, AZ_ACT_TEACH, false, 0);
    az_ledger_flush(w);
    ++w->tick;
}

int main(void)
{
    AzConfig cfg = az_test_config(112,8);
    AzWinInfo info = {0};
    AzHost *host = az_win_host_create(1,0,&info);
    if (!host) return 80;
    host->sink = capture;
    AzArena physical;
    AztTeaching obs;
    const u64 bytes = az_world_bytes(&cfg);
    if (!azt_teaching_arena(&physical,bytes,cfg.cap,AZ_WIN_LOCK,&info,true,&obs) || !obs.capacity || !info.arena_locked) return 81;
    AzArena world = physical;
    world.size = bytes;
    AzWorld *w = az_world_create(&world,&cfg,host);
    if (!w) return 82;
    az_worldgen(w);
    const uintptr_t start = (uintptr_t)obs.rows, end = start + obs.capacity * sizeof *obs.rows;
    CHECK(start >= (uintptr_t)world.base + world.size);
    CHECK(end <= (uintptr_t)physical.base + physical.size);
    u32 checked = 0;
    for (uintptr_t pg = start & ~(uintptr_t)4095; pg < end; pg += 4096) {
        PSAPI_WORKING_SET_EX_INFORMATION row = {0};
        row.VirtualAddress = (void *)pg;
        CHECK(QueryWorkingSetEx(GetCurrentProcess(), &row, sizeof row) && row.VirtualAttributes.Valid && row.VirtualAttributes.Locked);
        ++checked;
    }
    azt_teaching_attach(&obs,w,false);
    u32 n;
    AzBrain *src = az_brain(w,0,&n), *dst = az_brain(w,1,&n);
    memset(w->stat,0,2 * AZ_NSTAT * sizeof(u32));
    w->travel_carry[0] = w->travel_carry[1] = 0;
    u8 *imitate = w->trait + AZ_TRAITS + AZ_T_IMITATE;
    src->gain[0] = 164; dst->gain[0] = 64; *imitate = 255;
    u64 matter = az_total_matter(w); i64 energy = az_total_energy(w);
    teach(w);
    CHECK_EQ(dst->gain[0],163); CHECK_EQ(src->gain[0],164);
    CHECK_EQ(actor,w->uid[0]); CHECK_EQ(learner,w->uid[1]);
    CHECK_EQ(az_total_matter(w),matter);CHECK_EQ(az_total_energy(w),energy);
    AzStats st;
    az_stats_collect(w,&st);
    AztTeachingSummary first = azt_teaching_apply(&obs,w,&st,NULL);
    CHECK(first.exact); CHECK_EQ(first.covered, st.n_live); CHECK_EQ(first.partial,0);
    CHECK_EQ(first.positive,1); CHECK_EQ(st.archetype[AZ_ARCH_TEACHER],1);
    u64 count;
    CHECK(azt_teaching_class(&obs,w,0,&count)); CHECK_EQ(count,1);
    CHECK(azt_teaching_class(&obs,w,1,&count)); CHECK_EQ(count,0);
    *imitate=0; dst->gain[0]=64;
    teach(w);
    CHECK_EQ(dst->gain[0],64);CHECK_EQ(receipts,2);CHECK_EQ(forwarded,2);
    CHECK_EQ(obs.events,2);
    AzClassObservations x = {.teaching_complete=true,.teaching_events=2,.intake_complete=true,.declined_recruit_complete=true};
    CHECK_EQ(az_classify_observed(w,0,&x), AZ_ARCH_TEACHER);
    x.intake_occasions[2]=2;
    CHECK_EQ(az_classify_observed(w,0,&x), AZ_ARCH_GRAZER);
    x.declined_recruit_events=3;
    CHECK_EQ(az_classify_observed(w,0,&x), AZ_ARCH_HERMIT);
    x.declined_recruit_complete=false; x.intake_complete=false;
    CHECK_EQ(az_classify_observed(w,0,&x), AZ_ARCH_TEACHER);
    w->stat[AZ_ST_EAT_Y]=UINT32_MAX;
    CHECK_EQ(az_classify(w,0), AZ_ARCH_GRAZER);
    CHECK_EQ(az_classify_observed(w,0,&x), AZ_ARCH_TEACHER);
    x.teaching_events=UINT64_MAX;
    CHECK_EQ(az_classify_observed(w,0,&x), AZ_ARCH_TEACHER);
    AzEvent same_tick[3] = {{.tick=w->tick,.kind=AZ_EV_DEATH,.a=w->uid[0]},
        {.tick=w->tick,.kind=AZ_EV_TEACH,.a=w->uid[0],.b=w->uid[1]},
        {.tick=w->tick,.kind=AZ_EV_TICK,.a=2}};
    azt_teaching_receive(&obs,same_tick,3);
    CHECK_EQ(obs.events,3); CHECK_EQ(obs.used,0);
    ++w->tick;
    /* Exercise deletion in a collision cluster and ignored/lost history. */
    for (u64 uid=1;uid<=600;++uid) CHECK(azt_teacher_record(&obs,uid));
    for (u64 uid=1;uid<=600;uid+=2) azt_teacher_forget(&obs,uid);
    for (u64 uid=1;uid<=600;++uid) CHECK_EQ(obs.rows[azt_teacher_index(&obs,uid)].acts,(uid%2)==0);
    AzEvent dropped={.tick=w->tick,.kind=AZ_EV_TICK,.aux=1};
    azt_teaching_receive(&obs,&dropped,1);++w->tick;
    CHECK(!azt_teaching_class(&obs,w,1,&count));
    azt_teaching_detach(&obs);
    CHECK(host->sink==capture);
    AztTeaching unavailable={0};
    azt_teaching_attach(&unavailable,w,true);
    az_stats_collect(w,&st);
    AztTeachingSummary missing=azt_teaching_apply(&unavailable,w,&st,NULL);
    CHECK(!missing.exact); CHECK_EQ(missing.partial,st.n_live);
    azt_teaching_detach(&unavailable);
    printf("changed_gain=64->163 unchanged_control=64->64 teacher_winner=1 learner_winner=0 resolved_events=2 exact_books=1 checked_locked_tail_pages=%u\n",checked);
    az_win_host_destroy(host); az_win_arena_free(&physical);
    return az_test_done();
}
