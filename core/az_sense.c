/* Observe the cell and its six faces, then project signals into the body frame.
 * Birth-cached TAXIS scales directional gain and CURVE blends linear and log response.
 * See docs/agents/ABI-V4-NOTICE.md and build/work/w2/sense/DESIGN.md. */
#include "az_body_axes.h"
#include "az_body_parts.h"

static u32 sense_face(const AzWorld *w, u32 c, u32 neighbor)
{
    return w->c_solid[c] == 255 || w->c_solid[neighbor] == 255 ? c : neighbor;
}

void az_sense_cell(const AzWorld *w, u32 c, AzCellSense *cs)
{
    u32 x = az_cell_x(w,c), y = az_cell_y(w,c), z = az_cell_z(w,c);
    u32 lo[3] = { az_cell_xyz(w,(x-1) & (w->gx-1),y,z),
                  az_cell_xyz(w,x,(y-1) & (w->gy-1),z),
                  z ? c-w->gx*w->gy : c };
    u32 hi[3] = { az_cell_xyz(w,(x+1) & (w->gx-1),y,z),
                  az_cell_xyz(w,x,(y+1) & (w->gy-1),z),
                  z+1 < w->gz ? c+w->gx*w->gy : c };
    cs->light = w->c_light[c]; cs->temp = w->c_temp[c];
    cs->toxin = w->c_sp[(size_t)c*AZ_NSPECIES + AZ_SP_TOX_Z];
    cs->pressure = w->c_pressure[c];
    cs->dens = w->c_med[(size_t)c*2]; cs->visc = w->c_med[(size_t)c*2+1];
    cs->mark_id = w->c_mark_id[c];
    for (u32 k=0; k<AZ_NMARK; k++) cs->mark[k] = w->c_mark[(size_t)c*AZ_NMARK+k];
    for (u32 k=0; k<4; k++) cs->here[k] = az_field_avail(w,c,k);
    for (u32 a=0; a<3; a++) {
        u32 minus = sense_face(w,c,lo[a]), plus = sense_face(w,c,hi[a]);
        for (u32 k=0; k<4; k++)
            cs->food[k][a] = (i64)az_field_avail(w,plus,k) - az_field_avail(w,minus,k);
        cs->temp_grad[a] = (i32)w->c_temp[plus]-w->c_temp[minus];
        cs->light_grad[a] = (i32)w->c_light[plus]-w->c_light[minus];
        cs->mark_grad[a] = (i32)w->c_mark[(size_t)plus*AZ_NMARK]-w->c_mark[(size_t)minus*AZ_NMARK];
        cs->wind[a] = w->c_wind[(size_t)c*3+a];
        cs->grav[a] = w->c_grav[(size_t)c*4+a];
        cs->lvec[a] = w->c_lvec[(size_t)c*4+a];
    }
}

/* Each entry equals (az_log2_q8(level+1)*127+896)/(7*256) for its index. */
static const u8 sense_logarithmic[128] = {
    0, 18, 29, 36, 42, 47, 51, 54, 58, 60, 63, 65, 67, 69, 71, 73,
    74, 76, 77, 78, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91,
    91, 92, 93, 94, 95, 95, 96, 97, 97, 98, 98, 99, 100, 100, 101, 101,
    102, 102, 103, 103, 104, 104, 105, 105, 106, 106, 107, 107, 108, 108, 108, 109,
    109, 110, 110, 110, 111, 111, 112, 112, 112, 113, 113, 113, 114, 114, 114, 115,
    115, 115, 116, 116, 116, 117, 117, 117, 118, 118, 118, 118, 119, 119, 119, 119,
    120, 120, 120, 121, 121, 121, 121, 122, 122, 122, 122, 123, 123, 123, 123, 124,
    124, 124, 124, 124, 125, 125, 125, 125, 126, 126, 126, 126, 126, 127, 127, 127,
};

/* Curve interpolates a linear seven-bit response and a log response with the same
 * endpoints. The divisor 7 is log2(128), the width of the sensory encoding. */
static u8 response(u32 level, u8 curve)
{
    level = az_min_u32(level,127);
    if (!curve || !level || level == 127) return (u8)level;
    u32 logarithmic = sense_logarithmic[level];
    return (u8)((level*(255u-curve)+logarithmic*curve+127u)/255u);
}

static u8 bipolar(i64 value, u8 curve)
{
    u32 magnitude = (u32)az_clamp_i64(value < 0 ? -value : value,0,127);
    i32 scaled = (response(magnitude,curve)+1u)/2u;
    return (u8)az_clamp_i32(64 + (value < 0 ? -scaled : scaled),0,127);
}

/* Exposure units double per octave. Four sensory counts distinguish each octave. */
static u8 log_level(u64 amount)
{
    u32 n = amount >= UINT32_MAX ? UINT32_MAX : (u32)amount+1u;
    return (u8)az_min_u32(127,az_log2_q8(n)>>6);
}

static void tetra(const i32 world[3], const AzBodyAxes *axes, u32 shift, u8 curve, u32 acuity, u8 out[4])
{
    /* A zero vector projects to zero on every body axis for every rotor. */
    if (!(world[0] | world[1] | world[2])) {
        out[0]=out[1]=out[2]=out[3]=0;
        return;
    }
    i32 body[3];
    for(u32 axis=0;axis<3;axis++) body[axis]=az_body_axis(axes,world,axis);
    az_tetra4(body,shift,out);
    for (u32 k=0;k<4;k++) out[k]=response((u32)out[k]*acuity/128u,curve);
}

/* Keep the direction cosine fractional until receptor encoding. The normalized
 * length uses all 32 root bits, while its integer part preserves proximity.
 * Squared distance is u64 for a colony across the world. */
static u8 target_bearing(const i32 offset[3], u64 squared, const AzBodyAxes *axes,
                         u8 curve, u32 acuity, u32 *distance)
{
    if (!squared) {
        *distance=0;
        return 64;
    }
    u32 shift=(u32)__builtin_clzll(squared)/2u;
    u32 length=az_isqrt_u64(squared<<(2u*shift));
    *distance=length>>shift;
    i64 scale=1ll<<shift;
    i64 ahead;
    if (axes->upright) {
        ahead=az_clamp_i64((i64)offset[0]*scale,-(i64)length,length);
    } else {
        /* The physical vector at quarter scale has length below 2^30. Its
         * rotor dot product fits i64 even with the largest stored i16 rotor.
         * Clamp inconsistent externally supplied percepts before narrowing. */
        i32 precise[3];
        for (u32 axis=0;axis<3;axis++)
            precise[axis]=(i32)az_clamp_i64((i64)offset[axis]*scale/4,INT32_MIN,INT32_MAX);
        ahead=4ll*az_body_axis(axes,precise,0);
    }
    return bipolar(ahead*127*acuity/((i64)length*128),curve);
}


static void target(const i32 offset[3], u32 d2, u32 range, const AzBodyAxes *axes, u8 curve, u32 acuity, u8 out[2])
{
    out[0] = 0; out[1] = 64;
    if (d2==AZ_NONE) return;
    u32 distance;
    out[1]=target_bearing(offset,d2,axes,curve,acuity,&distance);
    out[0]=response((range-az_min_u32(range,distance))*127u/range,curve);
}

static i32 neighborhood_delta(u32 goal,u32 pos,u32 shift,bool wraps)
{
    i64 d=wraps ? (i64)(i32)(goal-pos) : (i64)goal-pos;
    return (i32)(d / (1ll << (shift-AZ_SUB_BITS)));
}

/* UP is a direction cosine. Keep fractional precision through both the
 * rotation and the gravity norm. One unit of diagonal gravity otherwise
 * looks vertical after rounding sqrt(2) down to 1. */
static i64 body_up(const AzBodyAxes *axes,const i32 gravity[3],u32 acuity)
{
    u64 squared=0;
    for (u32 a=0;a<3;a++) squared+=(u64)((i64)gravity[a]*gravity[a]);
    if (!squared) {
        const i32 up[3]={0,0,127};
        return (i64)az_body_axis(axes,up,2)*acuity/128;
    }
    i32 precise[3];
    u32 length;
    if (squared<=UINT32_MAX) {
        /* Q14 covers all physical cell vectors and keeps the norm under 2^30. */
        for (u32 a=0;a<3;a++) precise[a]=gravity[a]*(1<<14);
        length=az_isqrt_u64(squared<<28);
    } else {
        /* A full-width percept can exceed the rotor's i32 output range.
         * In this branch length / 4 still carries at least 14 useful bits. */
        for (u32 a=0;a<3;a++) precise[a]=gravity[a]/4;
        length=az_isqrt_u64(squared)/4;
    }
    return -(i64)az_body_axis(axes,precise,2)*127*acuity/((i64)length*128);
}

u8 az_sense_and_fin_noise(const AzWorld *w, u32 slot, const AzPercept *p, const AzCellSense *cs, u8 sens[AZ_SENS], u8 noise)
{
    const AzSenseMemory *history=az_sense_memory(w,slot);
    const u8 *ph=&w->phen[(size_t)slot*AZ_PHEN];
    u8 curve=history ? history->curve : 0, taxis=history ? history->taxis : 0;
    const i16 rotor[4]={w->rot_w[slot],w->rot_a[slot],w->rot_b[slot],w->rot_c[slot]};
    AzBodyAxes axes; az_prepare_axes(&axes,rotor);
    const AzBodyParts parts = az_body_parts(w, slot);
    const u64 half = w->param[AZ_WP_SENSE_SENSOR_HALF_MASS];
    const u64 scale = parts.sensor_matter + half;
    /* Rewrite floor(127*m/(m+half)) without a large numerator. */
    const u32 acuity = 255u - (u32)((127u * half + scale - 1u) / scale);
    const u32 gain=(w->cfg.lesion & AZ_LESION_TAXIS) ? 0u : taxis*acuity/255u;
    const u32 shift=w->param[AZ_WP_SENSE_GRAD_SHIFT];
    const u32 range=(ph[AZ_P_SENSE]+1u)*(AZ_SUB_ONE/256u);

    sens[AZ_S_ENERGY]=log_level(w->energy[slot]>0 ? (u32)w->energy[slot] : 0);
    sens[AZ_S_HEALTH]=(u8)(w->health[slot]>>9);
    sens[AZ_S_PAIN]=(u8)az_min_u32(127,((65535u-w->health[slot])>>9)+log_level(w->toxin[slot]));
    sens[AZ_S_CROWD]=response((u32)p->n_used*127u/AZ_NEIGH_MAX,curve);
    sens[AZ_S_LIGHT]=response(cs->light>>1,curve);
    const i64 diff=(i64)cs->temp-AZ_TEMP_OF_TOPT(ph[AZ_P_TOPT]);
    const i32 tol=AZ_TEMP_TOL(ph[AZ_P_TTOL]);
    sens[AZ_S_HOT]=response((u32)az_clamp_i64((diff-tol)*127/tol,0,127),curve);
    sens[AZ_S_COLD]=response((u32)az_clamp_i64((-diff-tol)*127/tol,0,127),curve);
    sens[AZ_S_MEDIUM]=bipolar(((i64)cs->dens-ph[AZ_P_DENS])/2,curve);
    sens[AZ_S_TOXIN]=response(log_level(cs->toxin),curve);
    sens[AZ_S_PRESS]=response(log_level(cs->pressure),curve);
    const u8 flags=w->flags[slot];
    sens[AZ_S_AGE]=(flags&AZ_F_SENESC) ? 127 : (flags&AZ_F_JUV) ? 32 : (flags&AZ_F_RECEPT) ? 96 : 64;
    u64 carry=0;
    for (u32 k=0;k<AZ_NINV;k++) carry+=w->inv[(size_t)slot*AZ_NINV+k];
    sens[AZ_S_CARRY]=log_level(carry);
    sens[AZ_S_READY]=(flags&AZ_F_RECEPT) ? 127 : 0;
    sens[AZ_S_WASTE]=log_level(w->waste[slot]);
    sens[AZ_S_SIG_A]=response(p->sig_a>>2,curve);
    sens[AZ_S_SIG_B]=response(p->sig_b>>2,curve);

    i32 food[3], kin[3];
    for (u32 a=0;a<3;a++) {
        /* Keep all four face differences exact through diet weighting. A u32
         * stock times four byte affinities and a byte gain fits in i64. */
        i64 sum=0;
        for (u32 d=0;d<4;d++) sum+=(i64)ph[AZ_P_DIET_L+d]*cs->food[d][a];
        food[a]=(i32)az_clamp_i64(sum*gain/(255ll*128ll),INT32_MIN,INT32_MAX);
        kin[a]=(i32)az_clamp_i64((i64)p->kin[a]+p->col[a],INT32_MIN,INT32_MAX);
    }
    tetra(food,&axes,shift,curve,128,&sens[AZ_S_FOOD]);
    tetra(p->threat,&axes,10,curve,acuity,&sens[AZ_S_THREAT]);
    tetra(p->prey,&axes,10,curve,acuity,&sens[AZ_S_PREY]);
    tetra(kin,&axes,10,curve,acuity,&sens[AZ_S_KIN]);
    target(p->mate_off,p->best_mate==AZ_NONE ? AZ_NONE : p->d2_mate,range,&axes,curve,acuity,&sens[AZ_S_MATE_NEAR]);
    target(p->bond_off,p->bond==AZ_NONE ? AZ_NONE : p->d2_bond,range,&axes,curve,acuity,&sens[AZ_S_BOND_NEAR]);
    u32 parent=p->parent==AZ_NONE ? AZ_NONE : p->d2_parent;
    u32 child=p->child==AZ_NONE ? AZ_NONE : p->d2_child;
    target(parent<=child ? p->parent_off : p->child_off,parent<=child ? parent : child,range,&axes,curve,acuity,&sens[AZ_S_FAMILY_NEAR]);

    u32 col=w->colony[slot];
    if (col && col<w->n_colonies && w->colonies[col].alive) {
        const AzColony *h=&w->colonies[col];
        i32 home[3]={neighborhood_delta(h->cx,w->pos_x[slot],w->shift_x,true),
                     neighborhood_delta(h->cy,w->pos_y[slot],w->shift_y,true),
                     neighborhood_delta(h->cz,w->pos_z[slot],w->shift_z,false)};
        u64 d2=(u64)((i64)home[0]*home[0])+(u64)((i64)home[1]*home[1])+(u64)((i64)home[2]*home[2]);
        u32 dist;
        sens[AZ_S_HOME_AHEAD]=target_bearing(home,d2,&axes,curve,acuity,&dist);
        sens[AZ_S_HOME]=response((u32)((u64)127*range/(range+(u64)dist)),curve);
        sens[AZ_S_COLONY]=response(h->signal[0]>>1,curve);
        sens[AZ_S_COLONY_NEED]=response(h->need[w->role[slot]<AZ_ROLE_COUNT ? w->role[slot] : 0]>>1,curve);
    } else {
        sens[AZ_S_HOME]=response(cs->mark[0]>>1,curve);
        i32 ahead=az_body_axis(&axes,cs->mark_grad,0);
        sens[AZ_S_HOME_AHEAD]=bipolar((i64)ahead*gain/128,curve);
        sens[AZ_S_COLONY]=sens[AZ_S_COLONY_NEED]=0;
    }
    sens[AZ_S_TOUCH]=response((u32)p->n_touch*32u,curve);
    i32 along=az_body_axis(&axes,p->vel,0);
    u32 allies=p->n_kin+p->n_col;
    sens[AZ_S_ALIGN]=bipolar(allies ? (i64)along*acuity/((i64)allies*128) : 0,curve);
    i32 wind=az_body_axis(&axes,cs->wind,0);
    sens[AZ_S_FLOW]=bipolar((i64)wind*acuity/128,curve);
    sens[AZ_S_FLOW_AHEAD]=bipolar((i64)wind*gain/128,curve);
    i32 gravity=az_body_axis(&axes,cs->grav,0);
    sens[AZ_S_GRAV_AHEAD]=bipolar((i64)gravity*gain/128,curve);
    sens[AZ_S_UP]=bipolar(body_up(&axes,cs->grav,acuity),curve);
    sens[AZ_S_RANK]=bipolar(w->rank[slot]/256,0);
    sens[AZ_S_DAY]=response((u32)az_clamp_i64(((i64)w->gstate[AZ_G_SUN0_LIGHT]+w->gstate[AZ_G_SUN1_LIGHT])/4,0,127),curve);
    sens[AZ_S_NOISE]=(u8)(noise&127u);
    sens[AZ_S_AROUSAL]=w->arousal[slot]>>1;
    i32 temp=az_body_axis(&axes,cs->temp_grad,0);
    sens[AZ_S_GRAD_T]=bipolar(((i64)temp*gain/128)/(1ll<<shift),curve);
    i32 from_light[3];
    for (u32 a=0;a<3;a++) from_light[a]=(i32)az_clamp_i64((i64)cs->light_grad[a]+(i64)cs->lvec[a]*cs->light/255,INT32_MIN,INT32_MAX);
    i32 light=az_body_axis(&axes,from_light,0);
    sens[AZ_S_LIGHT_AHEAD]=bipolar(((i64)light*gain/128)/(1ll<<shift),curve);
    sens[AZ_S_SIGA_AHEAD]=bipolar((i64)az_body_axis(&axes,p->sigv_a,0)*gain/(128ll*256),curve);
    sens[AZ_S_SIGA_SIDE]=bipolar((i64)az_body_axis(&axes,p->sigv_a,1)*gain/(128ll*256),curve);
    sens[AZ_S_SIGB_AHEAD]=bipolar((i64)az_body_axis(&axes,p->sigv_b,0)*gain/(128ll*256),curve);
    sens[AZ_S_SIGB_SIDE]=bipolar((i64)az_body_axis(&axes,p->sigv_b,1)*gain/(128ll*256),curve);
    sens[AZ_S_MASS]=log_level(w->mass[slot]);
    sens[AZ_S_PARTS]=log_level(parts.count);
    sens[AZ_S_SEASON]=response(((u32)w->gstate[AZ_G_SEASON]&65535u)>>9,curve);
    sens[AZ_S_STORM]=response((u32)az_clamp_i32(w->gstate[AZ_G_STORM]>>1,0,127),curve);
    sens[AZ_S_PAYOFF]=(u8)az_clamp_i32(64+(history ? history->payoff : 0),0,127);
    sens[AZ_S_STAKE]=(u8)az_min_u32(127,p->faced_stake);
    return parts.fin;
}

u8 az_sense_and_fin(const AzWorld *w, u32 slot, const AzPercept *p, const AzCellSense *cs, u8 sens[AZ_SENS])
{
    return az_sense_and_fin_noise(w, slot, p, cs, sens, (u8)(az_rand(w, w->uid[slot], AZ_RS_NOISE, 0) & 127u));
}

void az_sense(const AzWorld *w, u32 slot, const AzPercept *p, const AzCellSense *cs, u8 sens[AZ_SENS])
{
    (void)az_sense_and_fin(w, slot, p, cs, sens);
}
