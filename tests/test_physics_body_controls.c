/* Independent controls for the locked native physics hooks and public tick. */
#include "az_test.h"
#include "az_kernels.h"

typedef struct {AzWorld *w; AzHost *host; AzArena arena; AzWinInfo info;} Body;
static const u32 floor_z=65536u, ceiling_z=UINT32_MAX-65536u;
static u32 coordinate(u32 cell,u32 fraction,u32 shift)
{
    return (cell << shift) + (fraction << (shift-16));
}
static void put(AzWorld *w,u32 x,u32 y,u32 z)
{
    w->pos_x[0]=x;w->pos_y[0]=y;w->pos_z[0]=z;
    w->cell[0]=az_cell_at(w,x,y,z);
}
static Body *open_body(Body *b,bool fin)
{
    memset(b,0,sizeof(*b));
    AzConfig cfg=az_test_config(71,1);
    cfg.cap=64;cfg.n_species=1;cfg.gx_bits=cfg.gy_bits=cfg.gz_bits=2;
    cfg.max_clades=32;cfg.max_colonies=32;
    b->host=az_win_host_create(1,0,&b->info);
    if(!b->host||!az_win_arena(&b->arena,az_world_bytes(&cfg),AZ_WIN_LOCK,&b->info)||!b->info.arena_locked){fprintf(stderr,"locked host/arena refused\n");exit(2);}
    AzWorld *w=b->w=az_world_create(&b->arena,&cfg,b->host);
    if(!w){fprintf(stderr,"world creation refused\n");exit(2);}
    az_worldgen(w);az_grid_build(w);
    if(w->n_slots!=1||!w->blk[0]){fprintf(stderr,"founders slots=%u live=%u first_handle=%u\n",w->n_slots,w->n_live,w->blk[0]);exit(2);}
    memset(w->c_solid,0,w->n_cells);
    memset(w->c_med,0,2*w->n_cells);
    memset(w->c_grav,0,4*w->n_cells);
    memset(w->c_wind,0,3*w->n_cells);
    memset(w->motor,0,AZ_MOT);
    memset(w->acc,0,3*sizeof(*w->acc));
    memset(w->c_acc,0,(size_t)w->n_cells*sizeof(*w->c_acc));
    w->flags[0]=AZ_F_ALIVE;w->energy[0]=4096;
    w->phen[AZ_P_DENS]=128;w->phen[AZ_P_DRAG]=0;w->phen[AZ_P_THRUST]=255;
    w->vel_x[0]=w->vel_y[0]=w->vel_z[0]=0;
    w->ang_a[0]=w->ang_b[0]=w->ang_c[0]=0;
    w->rot_w[0]=16384;w->rot_a[0]=w->rot_b[0]=w->rot_c[0]=0;
    w->travel_carry[0]=0;w->stat[AZ_ST_TRAVEL]=0;
    u32 parts=0;AzPart *part=az_parts(w,0,&parts);
    if(!parts){fprintf(stderr,"generated founder has no body parts\n");exit(2);}
    for(u32 i=0;i<parts;i++){part[i].matter=0;part[i].kind=AZ_PART_CORE;part[i].health=255;}
    part[0].kind=fin?AZ_PART_FIN:AZ_PART_CORE;part[0].matter=64;w->mass[0]=64;
    put(w,coordinate(1,32768,w->shift_x),coordinate(1,32768,w->shift_y),coordinate(1,32768,w->shift_z));
    return b;
}
static void close_body(Body *b)
{
    az_win_arena_free(&b->arena);az_win_host_destroy(b->host);
}
static void medium(AzWorld *w,u8 density,u8 visc,i8 gx,i8 gy,i8 gz)
{
    for(u32 i=0;i<w->n_cells;i++){
        w->c_med[2*i]=density;w->c_med[2*i+1]=visc;
        w->c_grav[4*i]=gx;w->c_grav[4*i+1]=gy;w->c_grav[4*i+2]=gz;
    }
}
static void phase(AzWorld *w,const AzPercept *p)
{
    w->cell[0]=az_cell_at(w,w->pos_x[0],w->pos_y[0],w->pos_z[0]);
    az_physics_steer(w,0,p,0);
    az_physics_range(w,0,1,0);
}
static void commit(AzWorld *w)
{
    az_field_commit(w,0,w->n_cells,0);az_tally_merge(w);w->tick++;
}
static void calm_and_weight(void)
{
    const AzPercept quiet={0};
    for(u32 mode=0;mode<6;mode++){
        Body b;open_body(&b,false);AzWorld *w=b.w;
        const bool light=(mode&1)!=0,lesion=mode==2||mode==3,neutral=mode==4,quiescent=mode==5;
        if(lesion)w->cfg.lesion|=AZ_LESION_GRAVITY;
        w->phen[AZ_P_DENS]=neutral?128:(light?32:224);w->phen[AZ_P_DRAG]=128;
        medium(w,128,128,0,0,quiescent?0:-48);
        u32 unit=1u<<(w->shift_z-16),start=light?ceiling_z-4096u*unit:floor_z+4096u*unit;
        put(w,w->pos_x[0],w->pos_y[0],start);
        u32 x=w->pos_x[0],y=w->pos_y[0],rest=0,changed=0;
        u64 matter=az_total_matter(w);i64 energy=az_total_energy(w);
        for(u32 k=0;k<200;k++){
            phase(w,&quiet);commit(w);
            rest+=k>=175&&w->vel_z[0]==0;
            changed+=w->pos_z[0]!=start;
            CHECK(w->pos_x[0]==x && w->pos_y[0]==y);
            CHECK(w->pos_z[0]>=floor_z && w->pos_z[0]<=ceiling_z);
            CHECK(az_total_matter(w)==matter && az_total_energy(w)==energy);
        }
        if(lesion||neutral||quiescent){CHECK_EQ(changed,0);CHECK_EQ(w->vel_z[0],0);}
        else {CHECK(changed>0);CHECK_EQ(w->pos_z[0],light?ceiling_z:floor_z);CHECK_EQ(rest,25);}
        CHECK(w->totals.field_underflow==0 && w->totals.dropped_events==0);close_body(&b);
    }
    Body b;open_body(&b,false);AzWorld *w=b.w;
    w->phen[AZ_P_DENS]=192;medium(w,128,0,-11,7,0);
    phase(w,&quiet);CHECK_EQ(w->vel_x[0],-44);CHECK_EQ(w->vel_y[0],28);CHECK_EQ(w->vel_z[0],0);
    close_body(&b);
}
static i32 retain(i32 velocity,u32 drag){return velocity*(i32)(256u-drag)/256;}
static u32 unit_pair(u32 a,u32 b){return (a*b+127u)/255u;}
static void drag_and_wind(void)
{
    AzPercept p={0};
    const u8 solid[]={0,128,254};
    for(u32 k=0;k<3;k++)for(int sign=-1;sign<=1;sign+=2){
        Body b;open_body(&b,false);AzWorld *w=b.w;
        medium(w,150,140,0,0,0);w->phen[AZ_P_DRAG]=190;w->c_solid[w->cell[0]]=solid[k];
        w->vel_x[0]=(i16)(sign*800);w->vel_y[0]=(i16)-sign;
        u32 visc=unit_pair(190,140),drag=255-unit_pair(255-visc,255-solid[k]);
        phase(w,&p);CHECK_EQ(w->vel_x[0],retain(sign*800,drag));CHECK_EQ(w->vel_y[0],retain(-sign,drag));
        close_body(&b);
    }
    for(u32 density=0;density<=2;density++)for(int sign=-1;sign<=1;sign+=2){
        Body b;open_body(&b,false);AzWorld *w=b.w;
        u32 d=density==2?255:density*128;
        medium(w,(u8)d,128,0,0,0);w->phen[AZ_P_DRAG]=255;w->energy[0]=0;
        w->cfg.lesion|=AZ_LESION_THRUST;w->c_wind[3*w->cell[0]+1]=(i8)(sign*8);
        i32 impulse=(sign*8*256*(i32)d*128)/(255*256);
        phase(w,&p);CHECK_EQ(w->vel_y[0],retain(impulse,128));CHECK_EQ(w->energy[0],0);
        close_body(&b);
    }
}
static void spatial(void)
{
    AzPercept p={0};
    for(u32 axis=0;axis<3;axis++)for(int sign=-1;sign<=1;sign+=2){
        Body b;open_body(&b,false);AzWorld *w=b.w;
        u32 q[3];u32 shift=w->shift_x;
        for(u32 k=0;k<3;k++)q[k]=coordinate(1,k==axis?(sign>0?65520:16):32768,shift);
        put(w,q[0],q[1],q[2]);
        u32 next[3]={q[0],q[1],q[2]};next[axis]+=(u32)(sign*64)*(1u<<(shift-16));
        u32 solid=az_cell_at(w,next[0],next[1],next[2]);w->c_solid[solid]=255;
        i16 *vel[]={w->vel_x,w->vel_y,w->vel_z};u32 *pos[]={w->pos_x,w->pos_y,w->pos_z};
        vel[axis][0]=(i16)(sign*64);vel[(axis+1)%3][0]=48;
        phase(w,&p);
        CHECK_EQ(pos[axis][0],q[axis]);CHECK_EQ(vel[axis][0],-sign*64);
        CHECK_EQ(vel[(axis+1)%3][0],48);CHECK(pos[(axis+1)%3][0]!=q[(axis+1)%3]);
        CHECK(w->c_solid[az_cell_at(w,w->pos_x[0],w->pos_y[0],w->pos_z[0])]!=255);
        close_body(&b);
    }
    Body b;open_body(&b,false);AzWorld *w=b.w;
    u32 scale=1u<<(w->shift_x-16);
    put(w,UINT32_MAX-15*scale,14*scale,w->pos_z[0]);u32 x=w->pos_x[0],y=w->pos_y[0];
    w->vel_x[0]=64;w->vel_y[0]=-64;phase(w,&p);
    CHECK_EQ(w->pos_x[0],x+64*scale);CHECK_EQ(w->pos_y[0],y-64*scale);
    close_body(&b);
    open_body(&b,false);w=b.w;
    x=coordinate(1,65520,w->shift_x);y=coordinate(1,65520,w->shift_y);
    put(w,x,y,w->pos_z[0]);w->c_solid[az_cell_at(w,x+64*scale,y+64*scale,w->pos_z[0])]=255;
    w->vel_x[0]=w->vel_y[0]=64;phase(w,&p);
    CHECK_EQ(w->pos_x[0],x+64*scale);CHECK_EQ(w->pos_y[0],y);CHECK_EQ(w->vel_y[0],-64);
    close_body(&b);
}
static u32 quote(const AzWorld *w)
{
    u32 request=0;
    for(u32 a=0;a<3;a++)request+=(u32)abs(w->motor[AZ_M_FWD+a]);
    u32 denom=255u*128u;
    return (request*w->phen[AZ_P_THRUST]*w->param[AZ_WP_PHYS_THRUST_COST]+denom-1u)/denom;
}
static void propulsion(void)
{
    AzPercept p={0};
    const u8 health[]={0,128,255};
    for(u32 k=0;k<3;k++)for(u32 j=0;j<3;j++){
        Body b;open_body(&b,true);AzWorld *w=b.w;u32 count,den=j==2?255:j*128;
        az_parts(w,0,&count)[0].health=health[k];medium(w,(u8)den,0,0,0,0);w->motor[AZ_M_FWD]=127;
        u32 strength=unit_pair(unit_pair(health[k],den),192);
        int before=w->energy[0];w->cell[0]=az_cell_at(w,w->pos_x[0],w->pos_y[0],w->pos_z[0]);
        az_physics_steer(w,0,&p,0);
        CHECK_EQ(w->acc[0],strength);CHECK_EQ(w->phys_quote[0],strength?quote(w):0);
        az_physics_range(w,0,1,0);CHECK_EQ(w->vel_x[0],strength);
        CHECK_EQ(before-w->energy[0],strength?4:0);close_body(&b);
    }
    Body b;open_body(&b,false);AzWorld *w=b.w;medium(w,8,0,0,0,0);w->motor[AZ_M_VERT]=127;
    phase(w,&p);CHECK_EQ(w->vel_z[0],0);CHECK_EQ(w->energy[0],4096);close_body(&b);
    for(u32 face=0;face<6;face++)for(u32 near=0;near<=1;near++){
        open_body(&b,false);w=b.w;
        u32 rad=az_radius_ns(az_radius_code(w->mass[0]));
        u32 dist=near?rad/2:rad+2;
        u32 fraction=(dist*65536u)/AZ_SUB_ONE;
        u32 axis=face/2;bool plus=(face&1)!=0;
        u32 point[3],rock[3];
        for(u32 j=0;j<3;j++){point[j]=coordinate(1,j==axis?(plus?65535u-fraction:fraction):32768u,w->shift_x);rock[j]=point[j];}
        rock[axis]=coordinate(plus?2:0,32768,w->shift_x);
        w->c_solid[az_cell_at(w,rock[0],rock[1],rock[2])]=255;
        put(w,point[0],point[1],point[2]);w->motor[AZ_M_FWD]=127;
        phase(w,&p);
        bool accepted=near && face!=5;
        CHECK((w->vel_x[0]>0)==accepted);CHECK_EQ(w->energy[0],accepted?4092:4096);
        close_body(&b);
    }
    for(u32 ceiling=0;ceiling<2;ceiling++){
        open_body(&b,false);w=b.w;put(w,w->pos_x[0],w->pos_y[0],ceiling?ceiling_z:floor_z);
        w->motor[AZ_M_FWD]=127;phase(w,&p);
        CHECK((w->vel_x[0]>0)==!ceiling);close_body(&b);
    }
}
static void billing(void)
{
    AzPercept p={0};p.push[1]=4096;
    /* A no-fin body under otherwise identical conditions isolates the passive push. */
    Body ctl;open_body(&ctl,false);AzWorld *base=ctl.w;medium(base,255,0,0,0,0);
    base->motor[AZ_M_FWD]=127;
    az_physics_steer(base,0,&p,0);
    const i32 passive_y=base->acc[1];CHECK(passive_y>0);close_body(&ctl);
    for(u32 j=0;j<5;j++){
        const i32 budgets[]={0,1,2,4,20};
        Body b;open_body(&b,true);AzWorld *w=b.w;medium(w,255,0,0,0,0);
        w->motor[AZ_M_FWD]=127;w->energy[0]=budgets[j];
        /* Finish a cell step even when the powered impulse is small. */
        u32 x=coordinate(1,65520,w->shift_x);put(w,x,w->pos_y[0],w->pos_z[0]);
        u32 origin=w->cell[0];u32 prior_heat=w->c_heat[origin];
        u64 m=az_total_matter(w);i64 e=az_total_energy(w);
        az_physics_steer(w,0,&p,0);
        CHECK_EQ(w->energy[0],budgets[j]);CHECK_EQ(w->phys_quote[0],4);
        CHECK_EQ(w->phys_passive[1],passive_y);CHECK_EQ(w->acc[0],192);
        az_physics_range(w,0,1,0);i32 paid=budgets[j]>4?4:budgets[j];
        CHECK_EQ(w->energy[0],budgets[j]-paid);CHECK_EQ(w->vel_x[0],192*paid/4);CHECK_EQ(w->vel_y[0],passive_y);
        CHECK_EQ(az_pending_heat(&w->c_acc[origin]),paid);
        u32 destination=az_cell_at(w,w->pos_x[0],w->pos_y[0],w->pos_z[0]);
        if(paid){CHECK(origin!=destination);CHECK_EQ(az_pending_heat(&w->c_acc[destination]),0);}
        CHECK_EQ(az_total_matter(w),m);CHECK_EQ(az_total_energy(w),e);
        commit(w);CHECK_EQ(w->c_heat[origin],prior_heat+(u32)paid);
        CHECK_EQ(az_total_energy(w),e);CHECK_EQ(w->totals.field_underflow,0);CHECK_EQ(w->totals.dropped_events,0);
        close_body(&b);
    }
    Body b;open_body(&b,true);AzWorld *w=b.w;medium(w,255,0,0,0,0);
    w->param[AZ_WP_PHYS_THRUST_COST]=255;
    w->motor[AZ_M_FWD]=w->motor[AZ_M_LAT]=w->motor[AZ_M_VERT]=-128;
    AzPercept zero={0};u64 m=az_total_matter(w);i64 e=az_total_energy(w);
    phase(w,&zero);CHECK_EQ(w->phys_quote[0],765);CHECK_EQ(w->energy[0],4096-765);
    CHECK_EQ(az_pending_heat(&w->c_acc[w->cell[0]]),765);commit(w);
    CHECK_EQ(az_total_matter(w),m);CHECK_EQ(az_total_energy(w),e);close_body(&b);
    open_body(&b,true);w=b.w;medium(w,255,0,0,0,0);
    w->motor[AZ_M_FWD]=127;u32 origin=w->cell[0];w->c_heat[origin]=UINT32_MAX;
    e=az_total_energy(w);phase(w,&zero);commit(w);
    CHECK_EQ(w->c_heat[origin],UINT32_MAX);CHECK_EQ(az_pending_heat(&w->c_acc[origin]),4);
    CHECK_EQ(w->energy[0],4092);CHECK_EQ(az_total_energy(w),e);CHECK_EQ(w->totals.field_underflow,0);
    close_body(&b);
    open_body(&b,true);w=b.w;medium(w,255,0,0,0,0);
    w->motor[AZ_M_FWD]=127;w->energy[0]=1;
    AzPercept huge={0};huge.push[0]=INT32_MAX;
    az_physics_steer(w,0,&huge,0);
    CHECK_EQ(w->phys_passive[0],INT16_MAX);CHECK_EQ(w->acc[0],INT16_MAX);
    az_physics_range(w,0,1,0);CHECK_EQ(w->vel_x[0],16000);
    CHECK_EQ(w->phys_quote[0],0);CHECK_EQ(w->energy[0],1);close_body(&b);
    open_body(&b,true);w=b.w;medium(w,255,0,0,0,0);
    w->motor[AZ_M_FWD]=127;w->energy[0]=-3;phase(w,&zero);
    CHECK_EQ(w->vel_x[0],0);CHECK_EQ(w->energy[0],-3);CHECK_EQ(az_pending_heat(&w->c_acc[w->cell[0]]),0);close_body(&b);
}
static void turning_and_stage(void)
{
    AzPercept p={0};
    for(u32 lesion=0;lesion<2;lesion++){
        Body b;open_body(&b,true);AzWorld *w=b.w;medium(w,255,0,0,0,0);
        if(lesion)w->cfg.lesion|=AZ_LESION_THRUST;
        w->rot_w[0]=w->rot_c[0]=11585;w->motor[AZ_M_FWD]=127;w->motor[AZ_M_ROLL]=10;
        phase(w,&p);
        CHECK(abs(w->vel_x[0])<=1);CHECK((w->vel_y[0]>185)==!lesion);CHECK_EQ(w->vel_z[0],0);
        CHECK_EQ(w->energy[0],lesion?4096:4092);
        CHECK(abs(w->ang_a[0])<=1);CHECK(w->ang_b[0]>=1278&&w->ang_b[0]<=1281);
        CHECK_EQ(w->ang_c[0],0);
        CHECK(w->rot_a[0]!=0||w->rot_b[0]!=0);
        i64 norm=(i64)w->rot_w[0]*w->rot_w[0]+(i64)w->rot_a[0]*w->rot_a[0]+(i64)w->rot_b[0]*w->rot_b[0]+(i64)w->rot_c[0]*w->rot_c[0];
        CHECK(norm>267900000 && norm<269000000);
        close_body(&b);
    }
    for(u32 k=0;k<3;k++){
        const u32 channel[]={AZ_M_ROLL,AZ_M_PITCH,AZ_M_YAW};
        Body b;open_body(&b,false);AzWorld *w=b.w;
        w->motor[channel[k]]=-2;w->ang_a[0]=-3;phase(w,&p);
        i16 av[]={w->ang_a[0],w->ang_b[0],w->ang_c[0]};
        for(u32 axis=0;axis<3;axis++)CHECK_EQ(av[axis],(axis==k?-256:0)+(axis==0?-2:0));
        close_body(&b);
    }
    for(u32 kind=0;kind<3;kind++){
        Body b;open_body(&b,true);AzWorld *w=b.w;medium(w,128,0,0,0,-16);w->phen[AZ_P_DENS]=160;
        w->motor[AZ_M_FWD]=127;w->motor[AZ_M_YAW]=80;w->ang_b[0]=731;
        if(kind<2)w->flags[0]|=kind==0?AZ_F_EGG:AZ_F_DORMANT;
        else w->cfg.lesion|=AZ_LESION_THRUST;
        phase(w,&p);CHECK_EQ(w->vel_x[0],0);CHECK_EQ(w->vel_z[0],-32);CHECK_EQ(w->energy[0],4096);
        if(kind<2){
            CHECK_EQ(w->ang_b[0],0);CHECK_EQ(w->ang_c[0],0);CHECK_EQ(w->rot_w[0],16384);CHECK_EQ(w->rot_c[0],0);
            w->flags[0]=AZ_F_ALIVE;memset(w->motor,0,AZ_MOT);commit(w);phase(w,&p);
            CHECK_EQ(w->ang_b[0],0);CHECK_EQ(w->rot_w[0],16384);
        } else {CHECK(w->ang_c[0]>0);CHECK(w->rot_c[0]>0);}
        close_body(&b);
    }
}
static void travel_and_clamp(void)
{
    AzPercept p={0};Body b;open_body(&b,false);AzWorld *w=b.w;
    while(!az_phase_due(w,w->uid[0],AZ_RS_PHYS,4))w->tick++;
    w->tick++;u32 begin=w->pos_x[0];
    for(u32 i=0;i<16;i++) {
        w->vel_x[0]=i<8?400:-400;
        phase(w,&p);
        if(i<15){CHECK_EQ(w->travel_carry[0],(i+1)*400);CHECK_EQ(w->stat[AZ_ST_TRAVEL],0);}
        else {CHECK_EQ(w->travel_carry[0],0);CHECK_EQ(w->stat[AZ_ST_TRAVEL],6400);}
        commit(w);
    }
    CHECK_EQ(w->pos_x[0],begin);
    while(!az_phase_due(w,w->uid[0],AZ_RS_PHYS,4))w->tick++;
    w->stat[AZ_ST_TRAVEL]=UINT32_MAX-5;w->vel_x[0]=12;
    phase(w,&p);CHECK_EQ(w->stat[AZ_ST_TRAVEL],UINT32_MAX);CHECK_EQ(w->travel_carry[0],0);
    close_body(&b);
    for(int sign=-1;sign<=1;sign+=2)for(int excess=0;excess<=1;excess++){
        open_body(&b,false);w=b.w;medium(w,128,0,(i8)sign,0,0);w->phen[AZ_P_DENS]=144;
        w->vel_x[0]=(i16)(sign*(15999+excess));u64 before=w->totals.sat[AZ_SAT_VELOCITY];
        phase(w,&p);commit(w);CHECK_EQ(w->vel_x[0],sign*16000);
        CHECK_EQ(w->totals.sat[AZ_SAT_VELOCITY]-before,excess);close_body(&b);
    }
    open_body(&b,false);w=b.w;w->flags[0]=0;w->vel_x[0]=20;w->motor[AZ_M_FWD]=80;
    u32 x=w->pos_x[0];i32 e=w->energy[0];w->ang_c[0]=-73;
    phase(w,&p);CHECK_EQ(w->pos_x[0],x);CHECK_EQ(w->energy[0],e);CHECK_EQ(w->ang_c[0],-73);close_body(&b);
    open_body(&b,false);w=b.w;
    while(az_phase_due(w,w->uid[0],AZ_RS_PHYS,4))w->tick++;
    u32 step=1u<<(w->shift_z-16);
    put(w,UINT32_MAX-20*step,9*step,floor_z+20*step+step/2);
    w->vel_x[0]=30;w->vel_y[0]=-40;w->vel_z[0]=-64;
    phase(w,&p);
    CHECK_EQ(w->pos_z[0],floor_z);CHECK_EQ(w->travel_carry[0],53); /* floor(sqrt(30^2+40^2+20^2)) */
    close_body(&b);
    open_body(&b,true);w=b.w;medium(w,255,0,0,0,0);
    w->motor[AZ_M_FWD]=127;x=coordinate(1,65520,w->shift_x);put(w,x,w->pos_y[0],w->pos_z[0]);
    u32 origin=w->cell[0];u32 destination=az_cell_at(w,coordinate(2,0,w->shift_x),w->pos_y[0],w->pos_z[0]);
    w->c_solid[destination]=255;phase(w,&p);
    CHECK_EQ(w->pos_x[0],x);CHECK_EQ(w->energy[0],4092);CHECK_EQ(az_pending_heat(&w->c_acc[origin]),4);
    CHECK_EQ(w->travel_carry[0]+w->stat[AZ_ST_TRAVEL],0);close_body(&b);
}
static void fixed_motor(const AzBrain *brain,u8 *mind,i8 *motor)
{
    (void)brain;(void)mind;memset(motor,0,AZ_MOT);motor[AZ_M_FWD]=127;
}
/* A saturated mass still has the largest body radius and can touch the rock
 * one eighth of a cell below it. Neither mass should reach the next cell. */
static void saturated_mass_traction(void)
{
    for (u32 arm = 0; arm < 2; ++arm) {
        Body b;
        open_body(&b, false);
        AzWorld *w = b.w;
        const u32 mass = (UINT32_MAX - 1u) + arm;
        u32 parts;
        az_parts(w, 0, &parts)[0].matter = mass;
        w->pool_a[0] = w->pool_m[0] = w->pool_t[0] = w->waste[0] = w->toxin[0] = 0;
        w->mass[0] = mass;
        w->energy[0] = 1000000000;
        put(w, coordinate(1, 32768, w->shift_x), coordinate(1, 32768, w->shift_y),
            coordinate(1, 8192, w->shift_z));
        w->c_solid[az_cell_nb(w, w->cell[0], 0, 0, -1)] = 255;
        w->param[AZ_WP_PHYS_CONTACT_GAIN] = 255;
        w->k.brain_step = fixed_motor;
        w->cfg.lesion |= AZ_LESION_WEATHER | AZ_LESION_CHEM | AZ_LESION_MATING;
        const u32 x = w->pos_x[0];
        const u64 matter = az_total_matter(w);
        const i64 energy = az_total_energy(w);
        az_tick(w);
        CHECK_EQ(w->motor[AZ_M_FWD], 127);
        /* Body radius = 1/2 cell, face distance = 1/8 cell: 3/4 of 255. */
        CHECK_EQ(w->vel_x[0], 191);
        CHECK_EQ(w->pos_x[0] - x, 191u << (w->shift_x - 16));
        CHECK_EQ(w->phys_quote[0], 4);
        CHECK_EQ(az_total_matter(w), matter); CHECK_EQ(az_total_energy(w), energy);
        CHECK_EQ(w->totals.field_underflow, 0); CHECK_EQ(w->totals.dropped_events, 0);
        close_body(&b);
    }
}

static void public_credit(void)
{
    const i32 budget[]={-3,0,2,150,1000};u32 partial=0,full=0,empty=0;
    for(u32 j=0;j<AZ_LEN(budget);j++) {
        Body a,b;open_body(&a,true);open_body(&b,true);
        AzWorld *active=a.w,*control=b.w;
        control->cfg.lesion|=AZ_LESION_THRUST;
        AzWorld *pair[]={active,control};
        for(u32 i=0;i<2;i++){
            AzWorld *w=pair[i];
            w->cfg.lesion|=AZ_LESION_WEATHER|AZ_LESION_CHEM|AZ_LESION_MATING;
            w->k.brain_step=fixed_motor;
            medium(w,255,0,0,0,0);
            w->energy[0]=budget[j];w->param[AZ_WP_PHYS_THRUST_COST]=255;
            u64 m=az_total_matter(w);i64 e=az_total_energy(w);
            az_tick(w);
            CHECK_EQ(az_total_matter(w),m);CHECK_EQ(az_total_energy(w),e);
            CHECK_EQ(w->totals.dropped_events,0);CHECK_EQ(w->totals.field_underflow,0);
        }
        u32 cost=quote(active);i32 available=control->energy[0];
        u32 paid=available<=0?0:((u32)available<cost?(u32)available:cost);
        CHECK_EQ(active->motor[AZ_M_FWD],127);CHECK_EQ(control->motor[AZ_M_FWD],127);
        CHECK_EQ(active->phys_quote[0],cost);CHECK_EQ(control->phys_quote[0],0);
        CHECK_EQ(control->energy[0]-active->energy[0],paid);
        CHECK_EQ(active->vel_x[0],cost?192u*paid/cost:0);CHECK_EQ(control->vel_x[0],0);
        u32 c=control->cell[0];
        i64 ah=(i64)active->c_heat[c]+az_pending_heat(&active->c_acc[c]);
        i64 bh=(i64)control->c_heat[c]+az_pending_heat(&control->c_acc[c]);
        CHECK_EQ(ah-bh,paid);
        empty+=paid==0;partial+=paid>0&&paid<cost;full+=cost>0&&paid==cost;
        printf("public-credit initial=%d control=%d quoted=%u paid=%u vx=%d actual_debit=%d extra_heat=%" PRId64 "\n",
               budget[j],available,active->phys_quote[0],paid,active->vel_x[0],control->energy[0]-active->energy[0],ah-bh);
        close_body(&b);close_body(&a);
    }
    CHECK(empty>0);CHECK(partial>0);CHECK(full>0);
}
static void public_schedule(void)
{
    u64 reference[18]={0};bool first=true;
    const u32 threads[]={1,3,8};const u32 period[]={1,9};
    for(u32 j=0;j<3;j++)for(u32 k=0;k<2;k++)for(u32 assm=0;assm<=1;assm++){
        AzConfig cfg=az_test_config(31,96);cfg.cap=256;cfg.gx_bits=cfg.gy_bits=3;cfg.gz_bits=2;
        cfg.max_clades=128;cfg.max_colonies=128;cfg.kernels=assm;cfg.reorder_period=(u8)period[k];
        AzWinInfo info={0};AzHost *host=az_win_host_create(threads[j],0,&info);AzArena arena={0};
        if(!host||!az_win_arena(&arena,az_world_bytes(&cfg),AZ_WIN_LOCK,&info)||!info.arena_locked)exit(2);
        AzWorld *w=az_world_create(&arena,&cfg,host);if(!w)exit(2);
        az_kernels_install(w,assm?AZ_K_ALL:0);
        az_worldgen(w);u64 m=az_total_matter(w);i64 e=az_total_energy(w);u32 rocks=0;
        for(u32 ix=0;ix<18;ix++){
            az_tick(w);u64 h=az_state_hash(w);
            if(first)reference[ix]=h;
            CHECK_EQ(h,reference[ix]);CHECK_EQ(az_total_matter(w),m);CHECK_EQ(az_total_energy(w),e);
            for(u32 s=0;s<w->n_slots;s++)if(w->flags[s]&AZ_F_ALIVE){
                rocks+=w->c_solid[az_cell_at(w,w->pos_x[s],w->pos_y[s],w->pos_z[s])]==255;
                CHECK(abs(w->vel_x[s])<=16000 && abs(w->vel_y[s])<=16000 && abs(w->vel_z[s])<=16000);
            }
        }
        printf("schedule workers=%u reorder=%u kernel=%u hash=%016" PRIx64 " live=%u rock_hits=%u\n",
               threads[j],period[k],assm,az_state_hash(w),w->n_live,rocks);
        CHECK_EQ(rocks,0);CHECK_EQ(w->totals.field_underflow,0);CHECK_EQ(w->totals.dropped_events,0);
        first=false;az_win_arena_free(&arena);az_win_host_destroy(host);
    }
}
static void rock_rest_and_stopping(void)
{
    AzPercept p={0};
    for(u32 up=0;up<2;up++){
        Body b;open_body(&b,false);AzWorld *w=b.w;medium(w,128,128,0,0,-48);
        w->phen[AZ_P_DRAG]=128;w->phen[AZ_P_DENS]=up?32:224;
        for(u32 c=0;c<w->n_cells;c++)if(c/(w->gx*w->gy)==(up?2u:1u))w->c_solid[c]=255;
        u32 level=2u<<w->shift_z,scale=1u<<(w->shift_z-16);
        put(w,w->pos_x[0],w->pos_y[0],up?level-4096u*scale:level+4096u*scale);
        u32 stable=0,previous=0;u64 matter=az_total_matter(w);i64 energy=az_total_energy(w);
        for(u32 k=0;k<200;k++){
            phase(w,&p);commit(w);
            CHECK(w->c_solid[az_cell_at(w,w->pos_x[0],w->pos_y[0],w->pos_z[0])]!=255);
            CHECK(az_total_matter(w)==matter && az_total_energy(w)==energy);
            if(k>=175 && w->pos_z[0]==previous && w->vel_z[0]==0)stable++;
            previous=w->pos_z[0];
        }
        u32 gap=up?level-w->pos_z[0]:w->pos_z[0]-level;
        CHECK_EQ(stable,25);CHECK(gap<=288u*scale);
        close_body(&b);
    }
    for(int sign=-1;sign<=1;sign+=2){
        u32 stopping[2]={0};
        for(u32 medium_index=0;medium_index<2;medium_index++){
            Body b;open_body(&b,false);AzWorld *w=b.w;
            u32 viscosity=medium_index?128:16;
            w->phen[AZ_P_DRAG]=255;medium(w,127,(u8)viscosity,0,0,0);
            w->vel_y[0]=(i16)(sign*1000);i32 expected=w->vel_y[0];
            for(u32 tick=1;tick<=128;tick++){
                expected=retain(expected,viscosity);phase(w,&p);
                CHECK_EQ(w->vel_y[0],expected);commit(w);
                if(!expected){stopping[medium_index]=tick;break;}
            }
            close_body(&b);
        }
        CHECK(stopping[0]>stopping[1]&&stopping[1]>0);
    }
}
static void compare_motor_cost(void)
{
    AzPercept p={0};
    const i8 motors[][3]={{100,0,0},{50,-50,0},{33,-33,34}};
    i32 charge[3]={0};
    for(u32 k=0;k<3;k++){
        Body b;open_body(&b,true);AzWorld *w=b.w;medium(w,255,0,0,0,0);
        memcpy(w->motor,motors[k],3);u32 q=quote(w);phase(w,&p);
        charge[k]=4096-w->energy[0];CHECK_EQ(charge[k],q);
        CHECK_EQ(w->vel_x[0],motors[k][0]*192/127);CHECK_EQ(w->vel_y[0],motors[k][1]*192/127);
        CHECK_EQ(w->vel_z[0],motors[k][2]*192/127);close_body(&b);
    }
    CHECK(charge[0]==charge[1]&&charge[1]==charge[2]);
}
static void test_group(const char *name,void (*check)(void))
{
    int checks=az_checks,fails=az_failures;check();
    printf("group %s checks=%d failed=%d\n",name,az_checks-checks,az_failures-fails);
}
static void resumed_public_world(void)
{
    AzConfig cfg=az_test_config(31,96);
    cfg.cap=256;cfg.gx_bits=cfg.gy_bits=3;cfg.gz_bits=2;
    cfg.max_colonies=cfg.max_clades=128;cfg.reorder_period=9;
    AzWinInfo info={0};AzArena arena={0},restart={0};
    AzHost *host=az_win_host_create(3,0,&info);
    u64 bytes=az_world_bytes(&cfg);
    if(!host||!az_win_arena(&arena,bytes,AZ_WIN_LOCK,&info)||!info.arena_locked)exit(2);
    if(!az_win_arena(&restart,bytes,AZ_WIN_LOCK,&info)||!info.arena_locked)exit(2);
    AzWorld *original=az_world_create(&arena,&cfg,host);
    if(!original)exit(2);
    az_kernels_install(original,0);
    az_worldgen(original);
    for(u32 i=0;i<5;i++)az_tick(original);
    u32 moving=0;
    for(u32 s=0;s<original->n_slots;s++)
        if((original->flags[s]&AZ_F_ALIVE)&&original->travel_carry[s])moving++;
    CHECK(moving>0);
    u64 size=az_snapshot_bytes(original);u8 *data=malloc((size_t)size);
    if(!data||!size||az_snapshot_save(original,data,size)!=size)exit(2);
    AzWorld *copy=az_snapshot_load(&restart,data,size,host);
    if(!copy)exit(2);
    free(data);
    az_kernels_install(copy,0);
    CHECK_EQ(az_state_hash(original),az_state_hash(copy));
    /* Reorder also performs phase-zero grid rebuild, including this tick's
     * initiator flags. Compare two worlds at that same point in the phase. */
    az_grid_build(original);
    az_grid_reorder(copy);
    u32 different_slots=0;
    for(u32 i=0;i<19;i++){
        CHECK_EQ(az_state_hash(copy),az_state_hash(original));
        CHECK_EQ(az_total_matter(copy),az_total_matter(original));
        CHECK_EQ(az_total_energy(copy),az_total_energy(original));
        for(u32 s=0;s<original->n_slots;s++)if(original->flags[s]&AZ_F_ALIVE){
            u32 t=0;
            while(t<copy->n_slots && (!(copy->flags[t]&AZ_F_ALIVE)||copy->uid[t]!=original->uid[s]))t++;
            CHECK(t<copy->n_slots);
            if(t==copy->n_slots)continue;
            if(i==0&&s!=t)different_slots++;
            CHECK_EQ(original->travel_carry[s],copy->travel_carry[t]);
            CHECK_EQ(original->stat[(size_t)s*AZ_NSTAT+AZ_ST_TRAVEL],
                     copy->stat[(size_t)t*AZ_NSTAT+AZ_ST_TRAVEL]);
        }
        if(i<18){az_tick(original);az_tick(copy);}
    }
    CHECK(different_slots>0);
    printf("resume checkpoint=5 comparisons=19 moving=%u permuted=%u final_hash=%016" PRIx64 "\n",
           moving,different_slots,az_state_hash(original));
    az_win_arena_free(&restart);az_win_arena_free(&arena);az_win_host_destroy(host);
}
static void physics_billing_signed_heat_boundary(void)
{
    Body b;
    open_body(&b, true);
    AzWorld *w = b.w;
    AzPercept quiet = {0};
    medium(w, 255, 0, 0, 0, 0);
    w->motor[AZ_M_FWD] = 127;
    u32 origin = w->cell[0];
    const u32 backing = (u32)INT32_MAX - 1u;
    w->c_heat[origin] = backing;
    const i64 original_energy = az_total_energy(w);
    const u64 original_matter = az_total_matter(w);
    /* Move existing heat to the field journal. The motor pays four eu on this cell. */
    w->c_heat[origin] -= backing;
    az_cell_post_heat(w, 0, origin, (i32)backing);
    CHECK_EQ(az_total_energy(w), original_energy);
    phase(w, &quiet);
    CHECK_EQ(w->energy[0], 4092);
    CHECK_EQ(az_pending_heat(&w->c_acc[origin]), (i64)backing + 4);
    CHECK_EQ(az_total_energy(w), original_energy);
    CHECK_EQ(az_total_matter(w), original_matter);
    commit(w);
    CHECK_EQ(w->c_heat[origin], backing + 4);
    CHECK_EQ(az_pending_heat(&w->c_acc[origin]), 0);
    CHECK_EQ(az_total_energy(w), original_energy);
    CHECK_EQ(az_total_matter(w), original_matter);
    CHECK_EQ(w->totals.field_underflow, 0);
    CHECK_EQ(w->totals.dropped_events, 0);
    close_body(&b);
}

/* Four range jobs bill four physical bodies into the same prefunded journal. */
static void physics_billing_shared_heat(void)
{
    AzConfig cfg = az_test_config(72, 4);
    cfg.cap = 64; cfg.n_species = 1; cfg.gx_bits = cfg.gy_bits = cfg.gz_bits = 2;
    cfg.max_clades = cfg.max_colonies = 32;
    AzWinInfo info = {0};
    AzArena arena = {0};
    AzHost *host = az_win_host_create(4, AZ_WIN_NO_PIN, &info);
    if (!host || !az_win_arena(&arena, az_world_bytes(&cfg), AZ_WIN_LOCK, &info) || !info.arena_locked) exit(2);
    AzWorld *w = az_world_create(&arena, &cfg, host);
    if (!w) exit(2);
    az_worldgen(w);
    if (w->n_slots != 4 || w->n_workers != 4) exit(2);
    memset(w->c_solid, 0, w->n_cells);
    memset(w->motor, 0, 4*AZ_MOT);
    medium(w, 0, 0, 0, 0, 0);
    w->param[AZ_WP_PHYS_CONTACT_GAIN] = 255;
    w->param[AZ_WP_PHYS_THRUST_COST] = 4;
    const u32 c = az_cell_at(w, 0x30000000u, 0x30000000u, floor_z);
    AzPercept quiet = {0};
    for (u32 i=0; i<4; i++) {
        w->flags[i]=AZ_F_ALIVE;
        w->pos_x[i]=w->pos_y[i]=0x30000000u; w->pos_z[i]=floor_z; w->cell[i]=c;
        w->vel_x[i]=w->vel_y[i]=w->vel_z[i]=0;
        w->rot_w[i]=16384; w->rot_a[i]=w->rot_b[i]=w->rot_c[i]=0;
        w->phen[(size_t)i*AZ_PHEN+AZ_P_THRUST]=255;
        w->motor[(size_t)i*AZ_MOT+AZ_M_FWD]=127; w->energy[i]=4096;
        az_physics_steer(w,i,&quiet,0);
        CHECK_EQ(w->phys_quote[i],4);
    }
    const u32 funding = (u32)INT32_MAX - 3u;
    w->c_heat[c]=funding;
    const i64 e = az_total_energy(w);
    const u64 m = az_total_matter(w);
    w->c_heat[c]=0;az_cell_post_heat(w,0,c,(i32)funding);
    CHECK_EQ(az_total_energy(w),e);
    const u32 splits[] = {0,1,2,3,4};
    host->parallel_for(host->ctx,w,az_physics_range,splits,4);
    for (u32 i=0;i<4;i++) CHECK_EQ(w->energy[i],4092);
    CHECK_EQ(az_pending_heat(&w->c_acc[c]),(i64)funding+16);
    CHECK_EQ(az_total_energy(w),e); CHECK_EQ(az_total_matter(w),m);
    commit(w);
    CHECK_EQ(w->c_heat[c],funding+16); CHECK_EQ(az_pending_heat(&w->c_acc[c]),0);
    CHECK_EQ(az_total_energy(w),e); CHECK_EQ(az_total_matter(w),m);
    CHECK_EQ(w->totals.field_underflow,0); CHECK_EQ(w->totals.dropped_events,0);
    az_win_arena_free(&arena); az_win_host_destroy(host);
}

int main(void)
{
    test_group("shared_heat_crossing",physics_billing_shared_heat);
    test_group("heat_crossing",physics_billing_signed_heat_boundary);
    test_group("calm_weight",calm_and_weight);test_group("drag_wind",drag_and_wind);
    test_group("spatial",spatial);test_group("propulsion",propulsion);test_group("billing",billing);
    test_group("turning_stage",turning_and_stage);test_group("travel_clamp",travel_and_clamp);
    test_group("rock_rest_stopping",rock_rest_and_stopping);test_group("equal_cost_requests",compare_motor_cost);
    test_group("saturated_mass_traction",saturated_mass_traction);
    test_group("public_credit",public_credit);test_group("public_schedule",public_schedule);
    test_group("resumed_public_world",resumed_public_world);return az_test_done();
}
