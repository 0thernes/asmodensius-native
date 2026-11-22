/* Real field-gradient and sensor tests. The baseline stub must fail these fixtures. */
#include "az_test.h"
#include "../core/az_body_axes.h"

/* Use the scheduler's actual dispatcher on a settled pair. The test gets access
 * to the private function without changing the production C interface. */
/* In the instrumented test-only tick, one worker owns each tally.
 * Native builds compile away the observer. */
typedef struct SenseWrites {
    u64 frames;
    u8 lo[AZ_SENS],hi[AZ_SENS];
} SenseWrites;
static SenseWrites writes[AZ_MAX_WORKERS];
static bool observing_writes;
static void reset_writes(void)
{
    memset(writes,0,sizeof writes);
    for(u32 k=0;k<AZ_MAX_WORKERS;k++) memset(writes[k].lo,127,AZ_SENS);
}
static void capture_write(u32 worker,const u8 *frame)
{
    if(!observing_writes) return;
    SenseWrites *sample=&writes[worker];
    sample->frames++;
    for(u32 j=0;j<AZ_SENS;j++) {
        if(frame[j]<sample->lo[j]) sample->lo[j]=frame[j];
        if(frame[j]>sample->hi[j]) sample->hi[j]=frame[j];
    }
}
#define AZ_OBSERVE_MIND_WRITE(worker,frame) capture_write(worker,frame)
void az_test_private_tick(AzWorld *w);
#define az_tick az_test_private_tick
#include "../core/az_tick.c"
#undef az_tick
#undef AZ_OBSERVE_MIND_WRITE

static void sense_world_free(AzTestWorld *t)
{
    if (t->host) az_win_host_destroy(t->host);
    if (t->arena.base) az_win_arena_free(&t->arena);
    memset(t,0,sizeof *t);
}

static bool sense_world(AzTestWorld *t,const AzConfig *cfg,u32 threads)
{
    memset(t,0,sizeof *t);
    if (threads) {
        t->host=az_win_host_create(threads,AZ_WIN_USE_ECORES,&t->info);
        if (!t->host) return false;
    }
    u64 bytes=az_world_bytes(cfg);
    bool ok=az_win_arena(&t->arena,bytes,AZ_WIN_LOCK,&t->info);
    printf("sense_arena seed=%llu bytes=%llu locked=%u refused=%u win_error=%u\n",
           (unsigned long long)cfg->seed,(unsigned long long)bytes,t->info.arena_locked,
           t->info.arena_refused,t->info.last_error);
    if (!ok || !t->info.arena_locked) {sense_world_free(t);return false;}
    t->w=az_world_create(&t->arena,cfg,t->host);
    if (!t->w) {sense_world_free(t);return false;}
    az_worldgen(t->w);
    return true;
}

static void check_committed_pair(void)
{
    AzConfig cfg=az_test_config(7,2); AzTestWorld world;
    if (!sense_world(&world,&cfg,0)) { CHECK(!"world"); return; }
    AzWorld *w=world.w;
    for(u32 i=0;i<2;i++) {
        w->flags[i]=AZ_F_ALIVE;
        w->pos_x[i]=0x80000000u+i*(1u<<(w->shift_x-AZ_SUB_BITS+6));
        w->pos_y[i]=w->pos_z[i]=0x80000000u;
        w->rot_w[i]=16384; w->rot_a[i]=w->rot_b[i]=w->rot_c[i]=0;
        w->phen[i*AZ_PHEN+AZ_P_SENSE]=255;
        w->energy[i]=0;
        az_place(w,i);
    }
    w->trait[AZ_T_STAKE]=255; w->trait[AZ_T_ALTRUISM]=255; w->pool_a[0]=256; w->pool_a[1]=0;
    w->rot_w[1]=0; w->rot_c[1]=16384;
    az_grid_build(w);
    AzPercept source, recipient;
    az_scan_range(w,w->spos[0],1,&source,0); az_scan_range(w,w->spos[1],1,&recipient,0);
    CHECK_EQ(recipient.faced_stake,0);
    AzSenseMemory *cache=az_sense_memory(w,0);
    cache->innate_stake=255;
    w->trait[AZ_T_STAKE]=0; /* poison WARM storage after development, before common offer production */
    az_social_intent(w,0,&source,0);
    u8 committed=cache->stake;
    w->trait[AZ_T_STAKE]=255;
    az_social_intent(w,0,&source,0);
    CHECK_EQ(cache->stake,committed);
    CHECK(committed>0);
    AzSenseMemory *hs=az_sense_memory(w,0), *hr=az_sense_memory(w,1);
    CHECK(hs->stake>0); CHECK_EQ(w->pub[w->spos[0]].stake,0);
    az_grid_build(w);
    az_scan_range(w,w->spos[1],1,&recipient,0);
    CHECK_EQ(recipient.faced_stake,hs->stake); CHECK(recipient.faced_stake>0);
    w->rot_w[1]=16384; w->rot_c[1]=0; az_grid_build(w);
    az_scan_range(w,w->spos[1],1,&recipient,0);
    CHECK_EQ(recipient.faced_stake,0);
    u8 *offer=&w->offer[0];
    az_offer_set_u32(offer,AZ_O_ENERGY,0); az_offer_set_u32(offer,AZ_O_MATTER,3);
    const u64 m=az_total_matter(w); const i64 e=az_total_energy(w);
    pair_resolve(w,0,1,AZ_ACT_SHARE,true,0);
    pair_resolve(w,1,0,AZ_ACT_SHARE,false,0);
    CHECK_EQ(w->energy[0],0); CHECK_EQ(w->energy[1],0);
    CHECK_EQ(w->pool_a[0],253); CHECK_EQ(w->pool_a[1],3);
    CHECK_EQ(az_total_matter(w),m); CHECK_EQ(az_total_energy(w),e);
    CHECK_EQ(hs->payoff,-3*AZ_EPS_BIO); CHECK_EQ(hr->payoff,3*AZ_EPS_BIO);
    AzCellSense cs={0}; u8 out[AZ_SENS]; az_sense(w,1,&recipient,&cs,out);
    CHECK_EQ(out[AZ_S_PAYOFF],64+3*AZ_EPS_BIO);
    sense_world_free(&world);
}

static u64 changed_channels(const u8 before[AZ_SENS],const u8 after[AZ_SENS])
{
    u64 bits=0;
    for(u32 j=0;j<AZ_SENS;j++) if(before[j]!=after[j]) bits|=UINT64_C(1)<<j;
    return bits;
}

static void check_causes(void)
{
    AzTestWorld t; AzConfig cfg=az_test_config(7,16);
    if (!sense_world(&t,&cfg,0)) { CHECK(!"world"); return; }
    AzWorld *w=t.w; const u32 s=0;
    AzSenseMemory *h=az_sense_memory(w,s);
    CHECK_EQ(h->innate_stake,w->trait[(size_t)s*AZ_TRAITS+AZ_T_STAKE]);
    h->curve=w->trait[AZ_T_CURVE]=0; h->taxis=w->trait[AZ_T_TAXIS]=255;
    w->rot_w[s]=16384; w->rot_a[s]=w->rot_b[s]=w->rot_c[s]=0; w->colony[s]=0;
    w->phen[AZ_P_SENSE]=255; w->phen[AZ_P_DENS]=0;
    w->gstate[AZ_G_SUN0_LIGHT]=w->gstate[AZ_G_SUN1_LIGHT]=0;
    w->gstate[AZ_G_STORM]=w->gstate[AZ_G_SEASON]=0;
    AzCellSense cell={0}; AzPercept empty={0};
    cell.temp=AZ_TEMP_OF_TOPT(w->phen[AZ_P_TOPT]);
    empty.best_mate=empty.bond=empty.parent=empty.child=AZ_NONE;
    empty.d2_mate=empty.d2_bond=empty.d2_parent=empty.d2_child=AZ_NONE;
    u8 base[AZ_SENS], out[AZ_SENS];
    az_sense(w,s,&empty,&cell,base);
#define BIT(channel) (UINT64_C(1)<<AZ_S_##channel)
#define MEASURE(mask,action) do { AzCellSense cs=cell; AzPercept p=empty; action; \
    az_sense(w,s,&p,&cs,out); CHECK_EQ(changed_channels(base,out),(mask)); } while(0)
    for(u32 d=0;d<4;d++) {
        for(u32 j=0;j<4;j++) w->phen[AZ_P_DIET_L+j]=(d==j ? 255 : 0);
        MEASURE(BIT(FOOD)|BIT(FOOD_1),cs.food[d][0]=256);
        MEASURE(0,cs.food[(d+1)%4][0]=256);
    }
    MEASURE(BIT(HOT),cs.temp+=32767);
    MEASURE(BIT(COLD),cs.temp-=32767);
    MEASURE(BIT(GRAD_T),cs.temp_grad[0]=256);
    MEASURE(BIT(LIGHT),cs.light=200);
    MEASURE(BIT(LIGHT_AHEAD),cs.light_grad[0]=120);
    MEASURE(BIT(UP)|BIT(GRAV_AHEAD),cs.grav[0]=64);
    MEASURE(BIT(MEDIUM),cs.dens=180);
    MEASURE(BIT(FLOW)|BIT(FLOW_AHEAD),cs.wind[0]=96);
    MEASURE(BIT(TOXIN),cs.toxin=512);
    MEASURE(BIT(PRESS),cs.pressure=512);
    MEASURE(BIT(HOME),cs.mark[0]=128);
    MEASURE(BIT(HOME_AHEAD),cs.mark_grad[0]=64);
    MEASURE(BIT(SIG_A),p.sig_a=256);
    MEASURE(BIT(SIG_B),p.sig_b=256);
    MEASURE(BIT(SIGA_AHEAD)|BIT(SIGA_SIDE),p.sigv_a[0]=p.sigv_a[1]=8192);
    MEASURE(BIT(SIGB_AHEAD)|BIT(SIGB_SIDE),p.sigv_b[0]=p.sigv_b[1]=8192);
    MEASURE(BIT(THREAT)|BIT(THREAT_1),p.threat[0]=32768);
    MEASURE(BIT(PREY)|BIT(PREY_1),p.prey[0]=32768);
    MEASURE(BIT(KIN)|BIT(KIN_1),p.kin[0]=32768);
    MEASURE(BIT(MATE_NEAR)|BIT(MATE_AHEAD),p.best_mate=1;p.d2_mate=4096;p.mate_off[0]=64);
    MEASURE(BIT(BOND_NEAR)|BIT(BOND_AHEAD),p.bond=1;p.d2_bond=4096;p.bond_off[0]=-64);
    MEASURE(BIT(FAMILY_NEAR)|BIT(FAMILY_AHEAD),p.parent=1;p.d2_parent=4096;p.parent_off[0]=64);
    MEASURE(BIT(FAMILY_NEAR)|BIT(FAMILY_AHEAD),p.child=1;p.d2_child=4096;p.child_off[0]=-64);
    MEASURE(BIT(ALIGN),p.n_col=1;p.vel[0]=63);
    MEASURE(BIT(CROWD),p.n_used=32);
    MEASURE(BIT(TOUCH),p.n_touch=2);
    MEASURE(BIT(STAKE),p.faced_stake=39);
    MEASURE(BIT(PAYOFF),h->payoff=-18); h->payoff=0;
    MEASURE(BIT(SEASON),w->gstate[AZ_G_SEASON]=32768); w->gstate[AZ_G_SEASON]=0;
    MEASURE(BIT(STORM),w->gstate[AZ_G_STORM]=100); w->gstate[AZ_G_STORM]=0;
    MEASURE(BIT(DAY),w->gstate[AZ_G_SUN0_LIGHT]=240); w->gstate[AZ_G_SUN0_LIGHT]=0;
    CHECK(w->cfg.max_colonies>=2);
    w->n_colonies=2; w->colony[0]=1;
    AzColony *col=&w->colonies[1]; memset(col,0,sizeof *col); col->alive=1;
    col->cx=w->pos_x[0]; col->cy=w->pos_y[0]; col->cz=w->pos_z[0];
    w->role[0]=0;
    MEASURE(BIT(HOME), (void)0);
    az_sense(w,s,&empty,&cell,base);
    MEASURE(0,col->members=8); /* member count alone is not the shared signal */
    MEASURE(BIT(COLONY),col->signal[0]=128); col->signal[0]=0;
    MEASURE(BIT(COLONY_NEED),col->need[0]=96); col->need[0]=0;
    MEASURE(BIT(HOME)|BIT(HOME_AHEAD),col->cx+=1u<<w->shift_x);
    w->colony[0]=0;
    az_sense(w,s,&empty,&cell,base);
    u32 np; (void)az_parts(w,s,&np);
    CHECK(az_part_add(w,s,AZ_PART_CORE));
    MEASURE(BIT(PARTS),(void)0);
    CHECK(az_part_drop(w,s,np));
    const u32 mass=w->mass[0];
    MEASURE(BIT(MASS),w->mass[0]=UINT32_MAX); w->mass[0]=mass;
    for (u32 i=0;i<AZ_SENS;i++) out[i]=254;
    az_sense(w,s,&empty,&cell,out); CHECK_EQ(changed_channels(base,out),0);
#undef MEASURE
#undef BIT
    sense_world_free(&t);
}

static void check_tick_lesions(void)
{
    static const u8 preserved[AZ_SENS]={
        [AZ_S_ENERGY]=1,[AZ_S_HEALTH]=1,[AZ_S_PAIN]=1,[AZ_S_AGE]=1,[AZ_S_CARRY]=1,
        [AZ_S_READY]=1,[AZ_S_WASTE]=1,[AZ_S_RANK]=1,[AZ_S_NOISE]=1,[AZ_S_AROUSAL]=1,
        [AZ_S_MASS]=1,[AZ_S_PARTS]=1,[AZ_S_PAYOFF]=1
    };
    const u32 modes[4]={0,AZ_LESION_NOISE,AZ_LESION_SENSES,AZ_LESION_TAXIS};
    u64 ids[16]={0};
    u8 baseline[16][AZ_SENS]={{0}};
    u32 compared[4]={0};
    for(u32 m=0;m<4;m++) {
        AzConfig cfg=az_test_config(7,16); cfg.lesion=modes[m];
        AzTestWorld t;
        if (!sense_world(&t,&cfg,0)) {CHECK(!"world");return;}
        AzWorld *w=t.w;
        u64 founder_ids[16]; u8 solitary[16],active[16];
        for(u32 k=0;k<16;k++) { founder_ids[k]=w->uid[k]; active[k]=(w->flags[k]&(AZ_F_EGG|AZ_F_DORMANT))==0; u32 c=w->colony[k]; solitary[k]=!c || c>=w->n_colonies || !w->colonies[c].alive; }
        u64 mass=az_total_matter(w); i64 energy=az_total_energy(w);
        az_tick(w);
        CHECK_EQ(az_total_matter(w),mass); CHECK_EQ(az_total_energy(w),energy);
        for(u32 s=0;s<w->n_slots;s++) {
            if (!w->blk[s] || w->spos[s]==AZ_NONE || !(w->flags[s]&AZ_F_ALIVE)) continue;
            u32 bytes; const u8 *frame=az_seg(w,s,AZ_SEG_MIND,&bytes);
            if(bytes<AZ_SENS) continue;
            if(!m) {
                if(compared[0]>=16) {CHECK(!"unexpected birth in retained frames");break;}
                u32 k=compared[0]++;
                ids[k]=w->uid[s]; memcpy(baseline[k],frame,AZ_SENS);
            } else {
                u32 k=0; while(k<compared[0] && ids[k]!=w->uid[s]) k++;
                if(k==compared[0]) continue;
                compared[m]++;
                u32 f=0; while(f<16 && founder_ids[f]!=ids[k]) f++;
                CHECK(f<16); if(f==16) continue;
                for(u32 j=0;j<AZ_SENS;j++) {
                    u8 expected=baseline[k][j];
                    if((m==1 && j==AZ_S_NOISE) || (m==2 && !preserved[j])) expected=0;
                    if(m==3) {
                        if(j>=AZ_S_FOOD && j<=AZ_S_FOOD_3) expected=0;
                        if((j>=AZ_S_GRAD_T && j<=AZ_S_SIGB_SIDE) || (solitary[f] && j==AZ_S_HOME_AHEAD)) expected=64;
                        /* An egg/persistently dormant body skipped the sense call altogether. */
                        if(!active[f]) expected=baseline[k][j];
                    }
                    CHECK_EQ(frame[j],expected);
                }
            }
        }
        sense_world_free(&t);
    }
    CHECK(compared[0]>0); CHECK_EQ(compared[1],compared[0]); CHECK_EQ(compared[2],compared[0]); CHECK_EQ(compared[3],compared[0]);
}

/* Opposed, perfectly legal u32 inventories must be combined before any
 * signed narrowing. A neutral/saturated receptor can otherwise reverse the
 * sign or invent a food gradient where both diet-weighted sides are equal. */
static void check_full_stock_food(void)
{
    AzTestWorld t;
    AzConfig cfg=az_test_config(7,16);
    if(!sense_world(&t,&cfg,0)){ CHECK(!"food fixture world"); return; }
    AzWorld *w=t.w;
    const u32 cell=az_cell_xyz(w,3,3,2);
    const u32 plus=az_cell_xyz(w,4,3,2), minus=az_cell_xyz(w,2,3,2);
    for(u32 c=0;c<w->n_cells;c++) {
        w->c_solid[c]=0;
        w->c_sp[(size_t)c*AZ_NSPECIES+AZ_SP_NUT_X]=0;
        w->c_sp[(size_t)c*AZ_NSPECIES+AZ_SP_ORG_Y]=0;
    }
    u8 *ph=&w->phen[0];
    ph[AZ_P_DIET_L]=ph[AZ_P_DIET_W]=0;
    ph[AZ_P_DIET_X]=190; ph[AZ_P_DIET_Y]=200;
    az_sense_memory(w,0)->curve=0;
    az_sense_memory(w,0)->taxis=255;
    w->rot_w[0]=16384; w->rot_a[0]=w->rot_b[0]=w->rot_c[0]=0;
    u32 samples[][2]={{30000u,27000u},{3000000000u,2100000000u},
                      {3000000000u,2700000000u},{4000000000u,3800000000u},
                      {UINT32_MAX,0},{0,UINT32_MAX}};
    AzPercept pc={0};
    for(u32 j=0;j<AZ_LEN(samples);j++) for(u32 rev=0;rev<2;rev++) {
        const u32 up=rev?minus:plus,down=rev?plus:minus;
        u32 *xa=&w->c_sp[(size_t)up*AZ_NSPECIES];
        u32 *ya=&w->c_sp[(size_t)down*AZ_NSPECIES];
        xa[AZ_SP_NUT_X]=samples[j][0]; xa[AZ_SP_ORG_Y]=0;
        ya[AZ_SP_ORG_Y]=samples[j][1]; ya[AZ_SP_NUT_X]=0;
        const i64 true_delta=((i64)samples[j][0]*190-(i64)samples[j][1]*200)*(rev?-1:1);
        AzCellSense cs; u8 senses[AZ_SENS];
        az_sense_cell(w,cell,&cs);
        CHECK(cs.food[AZ_D_NUT_X][0]==(i64)samples[j][0]*(rev?-1:1));
        CHECK(cs.food[AZ_D_ORG_Y][0]==(i64)samples[j][1]*(rev?1:-1));
        az_sense(w,0,&pc,&cs,senses);
        for(u32 k=0;k<4;k++)
            CHECK_EQ(senses[AZ_S_FOOD+k],(k<2 ? true_delta>0 : true_delta<0) ? 127 : 0);
    }
    sense_world_free(&t);
}

static void check_cell(void)
{
    AzTestWorld t;
    AzConfig cfg = az_test_config(7, 16);
    if (!sense_world(&t, &cfg, 0)) { CHECK(!"world"); return; }
    AzWorld *w = t.w;
    const u32 c = 0, hi = 1, lo = w->gx - 1, above = w->gx * w->gy;
    w->c_solid[c] = w->c_solid[hi] = w->c_solid[lo] = w->c_solid[above] = 0;
    w->c_light[c] = 20; w->c_light[hi] = 80; w->c_light[lo] = 10; w->c_light[above] = 50;
    w->c_temp[c] = 90; w->c_temp[hi] = 120; w->c_temp[lo] = 70;
    w->c_mark[(size_t)hi * AZ_NMARK] = 73;
    w->c_mark[(size_t)lo * AZ_NMARK] = 9;
    w->c_grav[c*4+1] = -17; w->c_lvec[c*4+2] = 100; w->c_wind[c*3] = 11;
    w->c_med[c*2] = 22; w->c_med[c*2+1] = 37;
    AzCellSense a;
    const u64 before = az_state_hash(w);
    az_sense_cell(w,c,&a);
    CHECK_EQ(a.light_grad[0],70); CHECK_EQ(a.light_grad[2],30);
    CHECK_EQ(a.temp_grad[0],50); CHECK_EQ(a.mark_grad[0],64);
    CHECK_EQ(a.food[AZ_D_LIGHT][0], (i32)az_field_avail(w,hi,AZ_D_LIGHT)-(i32)az_field_avail(w,lo,AZ_D_LIGHT));
    CHECK_EQ(a.here[AZ_D_LIGHT], az_field_avail(w,c,AZ_D_LIGHT));
    CHECK_EQ(a.grav[1],-17); CHECK_EQ(a.lvec[2],100); CHECK_EQ(a.wind[0],11);
    CHECK_EQ(a.dens,22); CHECK_EQ(a.visc,37); CHECK_EQ(a.pressure,az_cell_pressure(w,c));
    CHECK_EQ(az_state_hash(w),before);
    AzCellSense distant=a;
    u32 far=az_cell_xyz(w,0,0,2);
    w->c_sp[(size_t)far*AZ_NSPECIES+AZ_SP_NUT_X]+=65536;
    az_sense_cell(w,c,&distant);
    CHECK(memcmp(a.food,distant.food,sizeof a.food)==0);
    CHECK_EQ(a.pressure,distant.pressure);
    w->c_sp[(size_t)far*AZ_NSPECIES+AZ_SP_GAS]+=65536;
    az_sense_cell(w,c,&distant);
    CHECK(memcmp(a.food,distant.food,sizeof a.food)==0);
    CHECK_EQ(distant.pressure,a.pressure); /* unpublished remote input cannot enter a private receptor */
    CHECK_EQ(az_cell_pressure(w,c),a.pressure+1024);
    az_publish_pressure(w);
    az_sense_cell(w,c,&distant);
    CHECK_EQ(distant.pressure,a.pressure+1024);
    for (u32 other=0;other<w->n_cells;other++) CHECK_EQ(w->c_pressure[other],az_cell_pressure(w,other));
    w->c_solid[hi]=255; az_sense_cell(w,c,&a);
    CHECK_EQ(a.light_grad[0],10);
    w->c_solid[c]=255; az_sense_cell(w,c,&a);
    for(u32 axis=0; axis<3; axis++) CHECK_EQ(a.light_grad[axis],0);
    sense_world_free(&t);
}

static void check_body(void)
{
    AzTestWorld world;
    AzConfig cfg = az_test_config(7,16);
    if (!sense_world(&world,&cfg,0)) { CHECK(!"world"); return; }
    AzWorld *w = world.w;
    const u32 s = 0;
    AzPercept p = {0};
    AzCellSense cs = {0};
    p.best_mate=p.bond=p.parent=p.child=p.near_prey=p.near_threat=p.near_kin=AZ_NONE;
    p.d2_mate=p.d2_bond=p.d2_parent=p.d2_child=p.d2_prey=p.d2_threat=p.d2_kin=AZ_NONE;
    p.best_mate=1; p.d2_mate=4096; p.mate_off[0]=64;
    p.threat[0]=32768; p.sigv_a[1]=32768; p.n_threat=1; p.n_used=1;
    cs.food[AZ_D_LIGHT][0]=128; cs.temp_grad[0]=512; cs.light_grad[0]=120;
    cs.wind[0]=96; cs.grav[2]=-32; cs.lvec[0]=32;
    w->phen[s*AZ_PHEN+AZ_P_DIET_L]=255;
    w->trait[s*AZ_TRAITS+AZ_T_TAXIS]=255;
    w->trait[s*AZ_TRAITS+AZ_T_CURVE]=0;
    AzSenseMemory *h=az_sense_memory(w,s); CHECK(h!=NULL); if (!h) {sense_world_free(&world); return;}
    h->taxis=255; h->curve=0;
    w->rot_w[s]=16384; w->rot_a[s]=w->rot_b[s]=w->rot_c[s]=0;
    u8 forward[AZ_SENS], reverse[AZ_SENS], other[AZ_SENS];
    az_sense(w,s,&p,&cs,forward);
    CHECK(forward[AZ_S_FOOD] > 0 && forward[AZ_S_FOOD_2] == 0);
    CHECK(forward[AZ_S_THREAT] > 0 && forward[AZ_S_THREAT_2] == 0);
    CHECK(forward[AZ_S_GRAD_T]>64 && forward[AZ_S_LIGHT_AHEAD]>64 && forward[AZ_S_FLOW_AHEAD]>64);
    CHECK(forward[AZ_S_SIGA_SIDE]>64 && forward[AZ_S_UP]>64);
    /* A pure +z threat reaches the two upward tetrahedral directions. */
    AzPercept vertical=p;
    vertical.threat[0]=0; vertical.threat[2]=p.threat[0];
    az_sense(w,s,&vertical,&cs,other);
    CHECK_EQ(other[AZ_S_THREAT],forward[AZ_S_THREAT]);
    CHECK_EQ(other[AZ_S_THREAT_1],0);
    CHECK_EQ(other[AZ_S_THREAT_2],0);
    CHECK_EQ(other[AZ_S_THREAT_3],forward[AZ_S_THREAT]);
    w->rot_w[s]=0; w->rot_c[s]=16384;
    az_sense(w,s,&p,&cs,reverse);
    CHECK(forward[AZ_S_MATE_AHEAD]>64 && reverse[AZ_S_MATE_AHEAD]<64);
    CHECK_EQ(forward[AZ_S_FOOD],reverse[AZ_S_FOOD_2]);
    CHECK_EQ(forward[AZ_S_THREAT],reverse[AZ_S_THREAT_2]);
    CHECK(reverse[AZ_S_GRAD_T]<64 && reverse[AZ_S_LIGHT_AHEAD]<64 && reverse[AZ_S_FLOW_AHEAD]<64);
    CHECK(reverse[AZ_S_SIGA_SIDE]<64 && reverse[AZ_S_UP]==forward[AZ_S_UP]);
    CHECK_EQ(reverse[AZ_S_NOISE],forward[AZ_S_NOISE]);
    w->phen[s*AZ_PHEN+AZ_P_DIET_L]=0;
    az_sense(w,s,&p,&cs,other);
    CHECK_EQ(other[AZ_S_FOOD_2],0);
    CHECK_EQ(other[AZ_S_THREAT_2],reverse[AZ_S_THREAT_2]);
    w->rot_c[s]=0; w->rot_w[s]=16384; w->health[s]=65535; w->toxin[s]=0;
    cs.temp=AZ_TEMP_OF_TOPT(w->phen[s*AZ_PHEN+AZ_P_TOPT]);
    az_sense(w,s,&p,&cs,other);
    CHECK_EQ(other[AZ_S_HEALTH],127); CHECK_EQ(other[AZ_S_PAIN],0); CHECK_EQ(other[AZ_S_HOT],0); CHECK_EQ(other[AZ_S_COLD],0);
    cs.temp+=32768; w->health[s]=0;
    az_sense(w,s,&p,&cs,other);
    CHECK(other[AZ_S_HOT]>0 && other[AZ_S_COLD]==0 && other[AZ_S_PAIN]>0);
    for(u32 i=0;i<AZ_SENS;i++) CHECK(other[i]<=127);
    sense_world_free(&world);
}

/* UP measures direction, so changing only gravity magnitude preserves its
 * output. The analytic seven-bit cosine is 109 at 45 degrees with neutral
 * acuity and a linear response. Signed fallbacks retain the body's frame. */
/* Independent geometric expectations. A yaw rotor proportional to (2,0,0,1)
 * has forward row (3,4,0)/5. The four-equal-component rotor looks along +y.
 * For an exact row n/k and offset v the uncurved rank is the largest integer
 * b with (b*k)^2 * dot(v,v) <= (127*dot(n,v))^2. */
static void check_target_bearings(void)
{
    const struct { i32 v[3]; u32 floor_length; u8 bearing[4]; } cases[]={
        {{0,0,0},0,{64,64,64,64}},
        {{1,0,0},1,{127,102,64,0}},
        {{-1,0,0},1,{0,26,64,127}},
        {{0,1,0},1,{64,115,127,64}},
        {{0,0,1},1,{64,64,64,64}},
        {{1,1,0},1,{109,127,109,19}},
        {{-1,-1,0},1,{19,1,19,109}},
        {{64,64,0},90,{109,127,109,19}},
        {{3,4,0},5,{102,127,115,26}},
        {{-6,8,0},10,{26,82,115,102}},
        {{1,-1,1},1,{101,57,27,27}},
        {{21,42,-42},63,{85,111,106,43}},
        {{4095,8190,-8190},12285,{85,111,106,43}},
        {{16384,0,0},16384,{127,102,64,0}},
        {{11585,11585,0},16383,{109,127,109,19}},
        {{65535,65535,0},92680,{109,127,109,19}},
        {{-65536,-65536,0},92681,{19,1,19,109}},
        {{0,0,98304},98304,{64,64,64,64}},
    };
    const struct { i16 q[4]; u8 column; } rotations[]={
        {{16384,0,0,0},0},{{0,0,0,0},0},{{2,0,0,1},1},{{30000,0,0,15000},1},
        {{-32768,-32768,-32768,-32768},2},{{0,0,0,-32768},3}
    };
    AzTestWorld world;
    AzConfig cfg=az_test_config(7,2);
    cfg.gx_bits=cfg.gy_bits=cfg.gz_bits=3;
    if (!sense_world(&world,&cfg,0)) { CHECK(!"world"); return; }
    AzWorld *w=world.w;
    AzSenseMemory *memory=az_sense_memory(w,0);
    memory->curve=0;
    u32 count;
    AzPart *parts=az_parts(w,0,&count);
    for (u32 i=0;i<count;i++) if (parts[i].kind==AZ_PART_SENSOR) parts[i].health=0;
    w->phen[AZ_P_SENSE]=255;
    w->colony[0]=1; w->n_colonies=2;
    AzColony *home=&w->colonies[1]; home->alive=1;
    w->pos_x[0]=UINT32_MAX; w->pos_y[0]=0; w->pos_z[0]=0;
    for (u32 r=0;r<AZ_LEN(rotations);r++) {
        w->rot_w[0]=rotations[r].q[0]; w->rot_a[0]=rotations[r].q[1];
        w->rot_b[0]=rotations[r].q[2]; w->rot_c[0]=rotations[r].q[3];
        for (u32 i=0;i<AZ_LEN(cases);i++) {
            const i32 *v=cases[i].v;
            bool peer=cases[i].floor_length<=AZ_SUB_ONE;
            AzPercept p={0}; AzCellSense cell={0}; u8 senses[AZ_SENS];
            p.best_mate=p.bond=peer ? 1 : AZ_NONE;
            p.parent=peer && !(i&1) ? 1 : AZ_NONE;
            p.child=peer && (i&1) ? 1 : AZ_NONE;
            u32 d2=0;
            for (u32 k=0;k<3;k++) {
                if (peer) d2+=(u32)(v[k]*v[k]);
                p.mate_off[k]=p.bond_off[k]=p.parent_off[k]=p.child_off[k]=v[k];
            }
            p.d2_mate=p.d2_bond=p.d2_parent=p.d2_child=d2;
            /* Twice cross a wrapped axis; z has a hard wall, so choose a
             * legal base for the signed displacement at each end. */
            w->pos_z[0]=v[2]<0 ? UINT32_MAX : 0;
            home->cx=w->pos_x[0]+(u32)v[0]*(1u<<(w->shift_x-AZ_SUB_BITS));
            home->cy=w->pos_y[0]+(u32)v[1]*(1u<<(w->shift_y-AZ_SUB_BITS));
            home->cz=w->pos_z[0]+(u32)v[2]*(1u<<(w->shift_z-AZ_SUB_BITS));
            az_sense(w,0,&p,&cell,senses);
            const u32 want=cases[i].bearing[rotations[r].column];
            const u32 near=peer ? (AZ_SUB_ONE-cases[i].floor_length)*127u/AZ_SUB_ONE : 0;
            CHECK_EQ(senses[AZ_S_MATE_NEAR],near); CHECK_EQ(senses[AZ_S_BOND_NEAR],near);
            CHECK_EQ(senses[AZ_S_FAMILY_NEAR],near);
            CHECK_EQ(senses[AZ_S_HOME],127u*AZ_SUB_ONE/(AZ_SUB_ONE+cases[i].floor_length));
            CHECK_EQ(senses[AZ_S_MATE_AHEAD],peer ? want : 64);
            CHECK_EQ(senses[AZ_S_BOND_AHEAD],peer ? want : 64);
            CHECK_EQ(senses[AZ_S_FAMILY_AHEAD],peer ? want : 64);
            CHECK_EQ(senses[AZ_S_HOME_AHEAD],want);
        }
    }
    sense_world_free(&world);
}


/* Largest supported z dimension. Centroid vectors almost span the entire
 * bounded coordinate axis, including a squared distance approaching 2^52. */
static void check_home_at_world_ends(void)
{
    AzConfig cfg=az_test_config(7,2);
    cfg.gx_bits=cfg.gy_bits=2; cfg.gz_bits=12;
    AzTestWorld world;
    if (!sense_world(&world,&cfg,0)) { CHECK(!"world"); return; }
    AzWorld *w=world.w;
    AzSenseMemory *memory=az_sense_memory(w,0); memory->curve=0;
    u32 count;
    AzPart *parts=az_parts(w,0,&count);
    for (u32 i=0;i<count;i++) if (parts[i].kind==AZ_PART_SENSOR) parts[i].health=0;
    w->colony[0]=1; w->n_colonies=2; w->colonies[1].alive=1;
    w->colonies[1].cx=w->pos_x[0]; w->colonies[1].cy=w->pos_y[0];
    w->rot_w[0]=30000; w->rot_a[0]=0; w->rot_b[0]=-15000; w->rot_c[0]=0;
    AzPercept p={0}; AzCellSense cell={0}; u8 senses[AZ_SENS];
    p.best_mate=p.bond=p.parent=p.child=AZ_NONE;
    for (u32 side=0;side<2;side++) {
        w->pos_z[0]=side ? UINT32_MAX : 0;
        w->colonies[1].cz=side ? 0 : UINT32_MAX;
        az_sense(w,0,&p,&cell,senses);
        /* Forward row (3/5,0,4/5) and a purely vertical offset. */
        CHECK_EQ(senses[AZ_S_HOME_AHEAD],side ? 13 : 115);
        CHECK_EQ(senses[AZ_S_HOME],0);
        CHECK_EQ(senses[AZ_S_MATE_AHEAD],64);
    }
    sense_world_free(&world);
}


static void check_up_direction(void)
{
    AzTestWorld t;
    AzConfig cfg=az_test_config(7,1); cfg.cap=64;
    if (!sense_world(&t,&cfg,0)) {CHECK(!"world");return;}
    AzWorld *w=t.w; AzSenseMemory *h=az_sense_memory(w,0);
    if(!h) {CHECK(!"mind");sense_world_free(&t);return;}
    h->curve=0;
    u32 n; AzPart *parts=az_parts(w,0,&n);
    for(u32 i=0;i<n;i++) parts[i].kind=AZ_PART_CORE;
    const i32 scales[]={1,3,31,127,32767,INT32_MAX};
    const i16 rotations[][4]={{16384,0,0,0},{0,0,16384,0},{11585,0,11585,0}};
    const u8 expected[]={109,19,19};
    AzPercept p={0}; p.best_mate=p.bond=p.parent=p.child=AZ_NONE;
    AzCellSense cs={0}; u8 out[AZ_SENS];
    for(u32 r=0;r<AZ_LEN(rotations);r++) {
        w->rot_w[0]=rotations[r][0]; w->rot_a[0]=rotations[r][1];
        w->rot_b[0]=rotations[r][2]; w->rot_c[0]=rotations[r][3];
        for(u32 j=0;j<AZ_LEN(scales);j++) {
            cs.grav[0]=scales[j]; cs.grav[2]=-scales[j];
            az_sense(w,0,&p,&cs,out); CHECK_EQ(out[AZ_S_UP],expected[r]);
        }
    }
    w->rot_w[0]=16384; w->rot_b[0]=0;
    cs.grav[0]=1; cs.grav[1]=1; cs.grav[2]=-1;
    az_sense(w,0,&p,&cs,out); CHECK_EQ(out[AZ_S_UP],101);
    cs.grav[0]=4; cs.grav[1]=0; cs.grav[2]=-16;
    az_sense(w,0,&p,&cs,out); CHECK_EQ(out[AZ_S_UP],126);
    cs.grav[0]=cs.grav[2]=0;
    az_sense(w,0,&p,&cs,out); CHECK_EQ(out[AZ_S_UP],127);
    sense_world_free(&t);
}

static void check_traits(void)
{
    AzTestWorld world;
    AzConfig cfg=az_test_config(7,16);
    if (!sense_world(&world,&cfg,0)) { CHECK(!"world"); return; }
    AzWorld *w=world.w;
    AzCellSense cs={0}; AzPercept p={0};
    p.best_mate=p.bond=p.parent=p.child=AZ_NONE;
    cs.food[AZ_D_NUT_X][0]=256; cs.temp_grad[0]=128;
    p.sigv_a[0]=16384; p.n_col=1; p.vel[0]=40; cs.wind[0]=32; cs.grav[1]=48; cs.grav[2]=-16;
    w->rot_w[0]=16384; w->rot_a[0]=w->rot_b[0]=w->rot_c[0]=0;
    u8 *ph=w->phen; for(u32 d=0;d<4;d++) ph[AZ_P_DIET_L+d]=d==AZ_D_NUT_X ? 255 : 0;
    AzSenseMemory *h=az_sense_memory(w,0);
    CHECK(h && h->taxis==w->trait[AZ_T_TAXIS] && h->curve==w->trait[AZ_T_CURVE]);
    if (!h) { sense_world_free(&world); return; }
    u32 count; AzPart *part=az_parts(w,0,&count);
    CHECK(count>0);
    if (!count) {sense_world_free(&world); return;}
    for (u32 i=0;i<count;i++) part[i].kind=AZ_PART_CORE;
    w->trait[AZ_T_CURVE]=h->curve=0; w->trait[AZ_T_TAXIS]=h->taxis=255;
    p.threat[0]=p.prey[0]=p.kin[0]=16384;
    p.best_mate=p.bond=p.parent=1;
    p.d2_mate=p.d2_bond=p.d2_parent=10000;
    p.mate_off[0]=p.bond_off[0]=p.parent_off[0]=60;
    p.mate_off[1]=p.bond_off[1]=p.parent_off[1]=80;
    u8 std[AZ_SENS], alt[AZ_SENS];
    az_sense(w,0,&p,&cs,std);
    /* Direct row-only edits can never affect active sensing after development. */
    w->trait[AZ_T_TAXIS]=0; w->trait[AZ_T_CURVE]=255;
    az_sense(w,0,&p,&cs,alt); CHECK(memcmp(std,alt,AZ_SENS)==0);
    w->trait[AZ_T_TAXIS]=255; w->trait[AZ_T_CURVE]=0;
    part[0].kind=AZ_PART_SENSOR; part[0].health=255;
    part[0].matter=w->param[AZ_WP_SENSE_SENSOR_HALF_MASS];
    az_sense(w,0,&p,&cs,alt);
    CHECK(alt[AZ_S_FOOD]>std[AZ_S_FOOD]);
    CHECK(alt[AZ_S_THREAT]>std[AZ_S_THREAT]); CHECK(alt[AZ_S_PREY]>std[AZ_S_PREY]);
    CHECK(alt[AZ_S_KIN]>std[AZ_S_KIN]); CHECK(alt[AZ_S_MATE_AHEAD]>std[AZ_S_MATE_AHEAD]);
    CHECK(alt[AZ_S_BOND_AHEAD]>std[AZ_S_BOND_AHEAD]); CHECK(alt[AZ_S_FAMILY_AHEAD]>std[AZ_S_FAMILY_AHEAD]);
    CHECK_EQ(alt[AZ_S_MATE_NEAR],std[AZ_S_MATE_NEAR]);
    CHECK(alt[AZ_S_FLOW]>std[AZ_S_FLOW]); CHECK(alt[AZ_S_UP]>std[AZ_S_UP]); CHECK(alt[AZ_S_ALIGN]>std[AZ_S_ALIGN]);
    u8 straight[AZ_SENS]; memcpy(straight,alt,sizeof straight);
    w->trait[AZ_T_CURVE]=h->curve=255;
    az_sense(w,0,&p,&cs,alt);
    CHECK(alt[AZ_S_FOOD]>straight[AZ_S_FOOD]); CHECK(alt[AZ_S_FLOW]>straight[AZ_S_FLOW]);
    w->cfg.lesion|=AZ_LESION_TAXIS;
    az_sense(w,0,&p,&cs,alt);
    CHECK_EQ(alt[AZ_S_FOOD],0); CHECK_EQ(alt[AZ_S_GRAD_T],64);
    CHECK_EQ(alt[AZ_S_FLOW_AHEAD],64); CHECK_EQ(alt[AZ_S_SIGA_AHEAD],64);
    CHECK(alt[AZ_S_FLOW]>64); CHECK_EQ(alt[AZ_S_ENERGY],std[AZ_S_ENERGY]);
    CHECK_EQ(alt[AZ_S_NOISE],az_rand(w,w->uid[0],AZ_RS_NOISE,0)&127u);
    w->cfg.lesion=0; w->trait[AZ_T_TAXIS]=h->taxis=0;
    u8 off[AZ_SENS]; az_sense(w,0,&p,&cs,off); CHECK(memcmp(alt,off,AZ_SENS)==0);
    h->payoff=-13; p.faced_stake=89; az_sense(w,0,&p,&cs,off);
    CHECK_EQ(off[AZ_S_PAYOFF],51); CHECK_EQ(off[AZ_S_STAKE],89);
    h->payoff=-65; az_sense(w,0,&p,&cs,off); CHECK_EQ(off[AZ_S_PAYOFF],0);
    h->payoff=64; az_sense(w,0,&p,&cs,off); CHECK_EQ(off[AZ_S_PAYOFF],127);
    CHECK_EQ(world.info.arena_locked,1);
    sense_world_free(&world);
}

/* Sample a maximally nutrient-specific receptor, identity rotor, native scale at
 * the vent and its six faces. The sampled sites include each non-rock cell once
 * per vent. Aim below saturation at the 95th percentile of peak tetra signals. */
static int compare_u32(const void *lp,const void *rp)
{
    u32 l=*(const u32*)lp,r=*(const u32*)rp;
    return (l>r)-(l<r);
}

static int calibrate(void)
{
    static u32 observed[49u*64u*7u];
    u32 count=0,vents=0;
    const i8 faces[7][3]={{0,0,0},{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    for(u32 seed=1;seed<=49;seed++) {
        AzConfig cfg=az_test_config(seed,0);
        cfg.gx_bits=cfg.gy_bits=6; cfg.gz_bits=5;
        AzTestWorld world;
        if (!sense_world(&world,&cfg,0)) return 1;
        AzWorld *w=world.w;
        vents+=w->vent_count;
        for(u32 i=0;i<w->vent_count;i++) for(u32 j=0;j<7;j++) {
            u32 c=az_cell_nb(w,w->vent_cell[i],faces[j][0],faces[j][1],faces[j][2]);
            if (c==AZ_NONE || w->c_solid[c]==255) continue;
            AzCellSense cs; az_sense_cell(w,c,&cs);
            const i64 *v=cs.food[AZ_D_NUT_X];
            const i64 dots[4]={(i64)v[0]+v[1]+v[2],(i64)v[0]-v[1]-v[2],
                              -(i64)v[0]+v[1]-v[2],-(i64)v[0]-v[1]+v[2]};
            i64 peak=0; for(u32 k=0;k<4;k++) if(dots[k]>peak) peak=dots[k];
            observed[count++]=(u32)az_clamp_i64(peak,0,UINT32_MAX);
        }
        sense_world_free(&world);
    }
    if(!count) return 1;
    qsort(observed,count,sizeof observed[0],compare_u32);
    u32 p95=observed[(95u*count+99u)/100u-1u],shift=0;
    while((p95>>shift)>=127u) shift++;
    printf("seeds=1..49 grid=64x64x32 sites=%u vents=%u p50_raw=%u p95_raw=%u max_raw=%u chosen_shift=%u p95_scaled=%u\n",
           count,vents,observed[(count-1)/2],p95,observed[count-1],shift,p95>>shift);
    return 0;
}

static int range_probe(void)
{
    AzTestWorld world;
    AzConfig cfg=az_test_config(7,16);
    if (!sense_world(&world,&cfg,0)) return 1;
    AzWorld *w=world.w;
    u64 state=7;
    unsigned violations=0;
    for(u32 i=0;i<1000000u;i++) {
        AzPercept p; AzCellSense cs;
        u8 *raw=(u8*)(void*)&p;
        for(u32 j=0;j<sizeof p;j+=4) { u32 x=(u32)az_test_rand64(&state); memcpy(raw+j,&x,az_min_u32(4,(u32)sizeof p-j)); }
        raw=(u8*)(void*)&cs;
        for(u32 j=0;j<sizeof cs;j+=4) { u32 x=(u32)az_test_rand64(&state); memcpy(raw+j,&x,az_min_u32(4,(u32)sizeof cs-j)); }
        /* Food entries encode differences of two unsigned field inventories.
         * Generate the entire input domain, including either extreme. */
        for(u32 d=0;d<4;d++) for(u32 a=0;a<3;a++) {
            const u64 faces=az_test_rand64(&state);
            cs.food[d][a]=(i64)(u32)faces-(u32)(faces>>32);
        }
        cs.food[0][0]=(i&1u) ? (i64)UINT32_MAX : -(i64)UINT32_MAX;
        u64 rotor=az_test_rand64(&state);
        w->rot_w[0]=(i16)rotor; w->rot_a[0]=(i16)(rotor>>16);
        w->rot_b[0]=(i16)(rotor>>32); w->rot_c[0]=(i16)(rotor>>48);
        AzSenseMemory *h=az_sense_memory(w,0);
        h->payoff=(i8)(state>>56);
        h->taxis=w->trait[AZ_T_TAXIS]=(u8)state; h->curve=w->trait[AZ_T_CURVE]=(u8)(state>>8);
        w->health[0]=(u16)(state>>16); w->energy[0]=(i32)(state>>32);
        w->toxin[0]=(u32)rotor; w->flags[0]=(u8)(rotor>>48);
        u8 out[AZ_SENS]; az_sense(w,0,&p,&cs,out);
        for(u32 j=0;j<AZ_SENS;j++) if(out[j]>127) violations++;
    }
    printf("random_cases=1000000 channel_checks=64000000 violations=%u\n",violations);
    sense_world_free(&world);
    return violations!=0;
}

/* Read only completed MIND frames. Newborns lack sorted positions; the initial
 * all-zero MIND is excluded. The observation runs serially between ticks. */
static void check_projection(void)
{
    const i16 rotors[][4]={{0,0,0,0},{16384,0,0,0},{-32768,0,0,0},{0,-32768,0,0},
      {0,0,32767,0},{0,0,0,-32768},{8192,8192,-8192,8192},{-32768,-32768,-32768,-32768},
      {32767,-32768,32767,-32768},{-1,0,0,0},{1,1,1,1}};
    const i32 values[]={INT32_MIN,INT32_MIN+1,-1,0,1,INT32_MAX};
    u32 errors=0; u64 cases=0;
    for(u32 k=0;k<AZ_LEN(rotors);k++) {
        AzBodyAxes axes; az_prepare_axes(&axes,rotors[k]);
        for(u32 x=0;x<AZ_LEN(values);x++) for(u32 y=0;y<AZ_LEN(values);y++) for(u32 z=0;z<AZ_LEN(values);z++) {
            i32 v[3]={values[x],values[y],values[z]},expect[3]; az_rotor_apply_inv(expect,rotors[k],v);
            for(u32 axis=0;axis<3;axis++) errors+=expect[axis]!=az_body_axis(&axes,v,axis);
            cases++;
        }
    }
    u64 state=7;
    for(u32 k=0;k<1000000u;k++) {
        u64 bits=az_test_rand64(&state); i16 r[4]={(i16)bits,(i16)(bits>>16),(i16)(bits>>32),(i16)(bits>>48)};
        if(k%4==0) r[1]=r[2]=r[3]=0;
        i32 v[3];for(u32 j=0;j<3;j++) v[j]=(i32)az_test_rand64(&state);
        AzBodyAxes axes;az_prepare_axes(&axes,r);
        i32 expect[3];az_rotor_apply_inv(expect,r,v);
        for(u32 axis=0;axis<3;axis++) errors+=expect[axis]!=az_body_axis(&axes,v,axis);
        cases++;
    }
    printf("projection_cases=%llu component_comparisons=%llu mismatches=%u\n",(unsigned long long)cases,(unsigned long long)cases*3u,errors);
    CHECK_EQ(errors,0);
}

static void check_saved_history(void)
{
    AzTestWorld world; AzConfig cfg=az_test_config(7,2);
    if(!sense_world(&world,&cfg,0)) {CHECK(!"world");return;}
    AzWorld *w=world.w;
    az_sense_memory(w,0)->payoff=-65; az_sense_memory(w,1)->payoff=64;
    const u64 before=az_state_hash(w);
    w->c_pressure[0]^=65535u;
    CHECK_EQ(before,az_state_hash(w)); /* derived scratch does not change saved identity */
    const u64 bytes=az_snapshot_bytes(w);
    AzArena raw={0},restore={0}; AzWinInfo info={0};
    bool have_raw=az_win_arena(&raw,bytes,AZ_WIN_LOCK,&info);
    CHECK(have_raw && info.arena_locked);
    bool have_dest=az_win_arena(&restore,az_world_bytes(&cfg),AZ_WIN_LOCK,&info);
    CHECK(have_dest && info.arena_locked);
    if (have_raw && have_dest) {
        CHECK_EQ(az_snapshot_save(w,raw.base,bytes),bytes);
        AzWorld *copy=az_snapshot_load(&restore,raw.base,bytes,NULL);
        CHECK(copy!=NULL);
        if (copy) {
            AzPercept p={0}; AzCellSense cs={0}; u8 out[AZ_SENS];
            CHECK_EQ(az_state_hash(w),az_state_hash(copy));
            CHECK_EQ(copy->c_pressure[0],az_cell_pressure(copy,0));
            CHECK(copy->c_pressure[0]!=w->c_pressure[0]);
            az_sense(copy,0,&p,&cs,out); CHECK_EQ(out[AZ_S_PAYOFF],0);
            az_sense(copy,1,&p,&cs,out); CHECK_EQ(out[AZ_S_PAYOFF],127);
        }
    }
    if(have_dest) az_win_arena_free(&restore);
    if(have_raw) az_win_arena_free(&raw);
    sense_world_free(&world);
}

static void check_fresh_writes(void)
{
    AzTestWorld t; AzConfig cfg=az_test_config(7,2);
    if(!sense_world(&t,&cfg,0)) {CHECK(!"world");return;}
    AzWorld *w=t.w;
    const u64 dormant=w->uid[1];
    w->flags[0]=AZ_F_ALIVE; w->flags[1]=AZ_F_ALIVE|AZ_F_DORMANT;
    u8 *prior=az_seg(w,1,AZ_SEG_MIND,NULL); memset(prior,79,AZ_SENS);
    reset_writes(); observing_writes=true;
    az_test_private_tick(w);
    observing_writes=false;
    CHECK_EQ(writes[0].frames,1);
    u32 slot=0; while(slot<w->n_slots && w->uid[slot]!=dormant) slot++;
    CHECK(slot<w->n_slots);
    if(slot<w->n_slots) { const u8 *kept=az_seg(w,slot,AZ_SEG_MIND,NULL); CHECK_EQ(kept[AZ_S_FOOD],79); }
    sense_world_free(&t);
}

static int coverage_probe(void)
{
    AzConfig cfg=az_test_config(42,100000);
    cfg.cap=400000; cfg.gx_bits=cfg.gy_bits=6; cfg.gz_bits=5;
    cfg.n_species=100; cfg.reorder_period=16; cfg.tier1_period=32;
    cfg.tier2_period=256; cfg.field_stride=8;
    AzTestWorld world;
    if (!sense_world(&world,&cfg,8)) return 1;
    AzWorld *w=world.w;
    const u64 baseline_mass=az_total_matter(w); const i64 baseline_energy=az_total_energy(w);
    u8 lo[AZ_SENS],hi[AZ_SENS]={0}; memset(lo,127,sizeof lo);
    u64 frames=0; u32 failed=0;
    reset_writes(); observing_writes=true;
    for (u32 tick=0;tick<60;tick++) {
        az_test_private_tick(w);
        failed+=(az_total_matter(w)!=baseline_mass || az_total_energy(w)!=baseline_energy);
        for (u32 slot=0;slot<w->n_slots;slot++) {
            if (!(w->flags[slot]&AZ_F_ALIVE) || w->spos[slot]==AZ_NONE) continue;
            u32 bytes; const u8 *m=az_seg(w,slot,AZ_SEG_MIND,&bytes);
            if (bytes<AZ_SENS) continue;
            u8 occupied=0; for(u32 j=0;j<AZ_SENS;j++) occupied|=m[j];
            if (!occupied) continue;
            frames++;
            for(u32 j=0;j<AZ_SENS;j++) { if(m[j]<lo[j]) lo[j]=m[j]; if(m[j]>hi[j]) hi[j]=m[j]; }
        }
    }
    observing_writes=false;
    unsigned varied=0,fresh_varied=0;
    u64 fresh=0; for(u32 k=0;k<AZ_MAX_WORKERS;k++) fresh+=writes[k].frames;
    for(u32 j=0;j<AZ_SENS;j++) {
        u8 fresh_lo=127,fresh_hi=0;
        for(u32 k=0;k<AZ_MAX_WORKERS;k++) {
            if(writes[k].lo[j]<fresh_lo) fresh_lo=writes[k].lo[j];
            if(writes[k].hi[j]>fresh_hi) fresh_hi=writes[k].hi[j];
        }
        printf("fresh_channel=%u min=%u max=%u\n",j,fresh_lo,fresh_hi);
        fresh_varied+=fresh_hi>fresh_lo;
        printf("channel=%u min=%u max=%u\n",j,lo[j],hi[j]);
        varied+=hi[j]>lo[j];
    }
    printf("fresh_write_events=%llu fresh_varied=%u/64\n",(unsigned long long)fresh,fresh_varied);
    printf("seed=42 ticks=60 founders=100000 workers=8 retained_nonzero_frames=%llu varied=%u/64 audit_failures=%u hash=%016llx\n",
           (unsigned long long)frames,varied,failed,(unsigned long long)az_state_hash(w));
    sense_world_free(&world);
    return failed!=0;
}

int main(int argc,char **argv)
{
    if(argc==2 && strcmp(argv[1],"--coverage")==0) return coverage_probe();
    if (argc==2 && strcmp(argv[1],"--calibrate")==0) return calibrate();
    if (argc==2 && strcmp(argv[1],"--range")==0) return range_probe();
    check_projection();
    check_saved_history();
    check_fresh_writes();
    check_tick_lesions();
    check_causes();
    check_committed_pair();
    check_traits();
    check_cell();
    check_full_stock_food();
    check_body();
    check_target_bearings();
    check_home_at_world_ends();
    check_up_direction();
    return az_test_done();
}
