/* Body motion and energy transfer. Decisions DQ-PHY-1 through DQ-PHY-13 in
 * build/work/w2/physics/DESIGN.md. Steering uses the orientation observed by
 * the brain; payment uses remaining energy after metabolism in the same tick. */
#include "az_abi.h"
#include "az_body_parts.h"

enum { PHYS_FLOOR = 65536u };
#define PHYS_CEILING (UINT32_MAX - PHYS_FLOOR)

static u32 byte_product(u32 a, u32 b) { return (a * b + 127u) / 255u; }

static u32 face_traction(const AzWorld *w, u32 neighbor, u32 distance, u32 radius)
{
    if (neighbor == AZ_NONE || distance >= radius) return 0;
    return (u32)w->c_solid[neighbor] * (radius - distance) / radius;
}

static u32 support(const AzWorld *w, u32 slot, u32 cell)
{
    const u32 radius = az_radius_ns(az_radius_code(w->mass[slot]));
    const u32 x = az_sub(w->pos_x[slot], w->shift_x);
    const u32 y = az_sub(w->pos_y[slot], w->shift_y);
    const u32 z = az_sub(w->pos_z[slot], w->shift_z);
    /* The outer floor can support a body at contact even in an empty grid. */
    u32 contact = w->pos_z[slot] <= PHYS_FLOOR ? 255u : 0u;
    if (z < radius)
        contact = az_max_u32(contact, face_traction(w, az_cell_nb(w, cell, 0, 0, -1), z, radius));
    if (x < radius)
        contact = az_max_u32(contact, face_traction(w, az_cell_nb(w, cell, -1, 0, 0), x, radius));
    else if (AZ_SUB_ONE - x < radius)
        contact = az_max_u32(contact, face_traction(w, az_cell_nb(w, cell, 1, 0, 0), AZ_SUB_ONE - x, radius));
    if (y < radius)
        contact = az_max_u32(contact, face_traction(w, az_cell_nb(w, cell, 0, -1, 0), y, radius));
    else if (AZ_SUB_ONE - y < radius)
        contact = az_max_u32(contact, face_traction(w, az_cell_nb(w, cell, 0, 1, 0), AZ_SUB_ONE - y, radius));
    return contact;
}

void az_physics_steer_fin(AzWorld *w, u32 slot, const AzPercept *p, u8 fin, u32 worker)
{
    (void)worker;
    const size_t base = (size_t)slot * 3u;
    const u8 flags = w->flags[slot];
    if (!(flags & AZ_F_ALIVE)) return;
    const u8 *ph = w->phen + (size_t)slot * AZ_PHEN;
    const i8 *motor = w->motor + (size_t)slot * AZ_MOT;
    i32 thrust[3] = {0};
    u32 quote = 0;
    if ((flags & (AZ_F_ALIVE | AZ_F_EGG | AZ_F_DORMANT)) == AZ_F_ALIVE &&
        !(w->cfg.lesion & AZ_LESION_THRUST) && ph[AZ_P_THRUST] &&
        (motor[AZ_M_FWD] || motor[AZ_M_LAT] || motor[AZ_M_VERT])) {
        const u32 c = w->cell[slot];
        const u32 swim = byte_product(byte_product(fin, w->c_med[(size_t)c * 2]), w->param[AZ_WP_PHYS_FIN_GAIN]);
        const u32 crawl = byte_product(support(w, slot, c), w->param[AZ_WP_PHYS_CONTACT_GAIN]);
        const i32 gain = (i32)(az_min_u32(255u, swim + crawl) * ph[AZ_P_THRUST]);
        i32 frame[3];
        u32 request = 0;
        for (u32 axis = 0; axis < 3; ++axis) {
            const i32 intent = motor[AZ_M_FWD + axis];
            request += (u32)az_abs_i32(intent);
            frame[axis] = intent * gain / (127 * 255);
        }
        const i16 rotor[4] = {w->rot_w[slot], w->rot_a[slot], w->rot_b[slot], w->rot_c[slot]};
        az_rotor_apply(thrust, rotor, frame);
        const u32 units = request * ph[AZ_P_THRUST] * w->param[AZ_WP_PHYS_THRUST_COST];
        quote = (units + 255u * 128u - 1u) / (255u * 128u);
    }
    bool powered = false;
    for (u32 axis = 0; axis < 3; ++axis) {
        const i16 passive = az_sat_i16(p->push[axis] / 16);
        w->phys_passive[base + axis] = passive;
        w->acc[base + axis] = az_sat_i16(passive + thrust[axis]);
        powered |= w->acc[base + axis] != passive;
    }
    w->phys_quote[slot] = powered ? (u16)quote : 0;
}

void az_physics_steer(AzWorld *w, u32 slot, const AzPercept *p, u32 worker)
{
    const u8 state = w->flags[slot] & (AZ_F_ALIVE | AZ_F_EGG | AZ_F_DORMANT);
    const u8 fin = state == AZ_F_ALIVE ? az_body_parts(w, slot).fin : 0;
    az_physics_steer_fin(w, slot, p, fin, worker);
}

typedef struct AzMotionBlock {
    u32 slot[AZ_CHUNK], x[AZ_CHUNK], y[AZ_CHUNK];
    i16 vx[AZ_CHUNK], vy[AZ_CHUNK], vz[AZ_CHUNK], dx[AZ_CHUNK], dy[AZ_CHUNK];
    i32 force[AZ_CHUNK][3];
    u8 drag[AZ_CHUNK];
} AzMotionBlock;
_Static_assert(sizeof(AzMotionBlock) <= AZ_WS_MODULE_BYTES, "motion block must fit worker scratch");

static void turn(AzWorld *w, u32 s)
{
    if (w->flags[s] & (AZ_F_EGG | AZ_F_DORMANT)) {
        w->ang_a[s] = w->ang_b[s] = w->ang_c[s] = 0;
        return;
    }
    const i8 *motor = w->motor + (size_t)s * AZ_MOT;
    if (!(motor[AZ_M_ROLL] || motor[AZ_M_PITCH] || motor[AZ_M_YAW] ||
          w->ang_a[s] || w->ang_b[s] || w->ang_c[s])) return;
    i16 rotor[4] = {w->rot_w[s], w->rot_a[s], w->rot_b[s], w->rot_c[s]};
    const i32 local[3] = {motor[AZ_M_ROLL] * 128, motor[AZ_M_PITCH] * 128, motor[AZ_M_YAW] * 128};
    i32 world[3];
    az_rotor_apply(world, rotor, local);
    const i32 keep = (i32)w->param[AZ_WP_PHYS_ANG_RETAIN];
    const i16 angular[3] = {
        az_sat_i16(w->ang_a[s] * keep / 256 + world[0]),
        az_sat_i16(w->ang_b[s] * keep / 256 + world[1]),
        az_sat_i16(w->ang_c[s] * keep / 256 + world[2])
    };
    w->ang_a[s] = angular[0]; w->ang_b[s] = angular[1]; w->ang_c[s] = angular[2];
    if (angular[0] || angular[1] || angular[2]) {
        az_rotor_step(rotor, angular);
        w->rot_w[s] = rotor[0]; w->rot_a[s] = rotor[1]; w->rot_b[s] = rotor[2]; w->rot_c[s] = rotor[3];
    }
}

static void gather(AzWorld *w, AzMotionBlock *b, u32 lane, u32 s, u32 worker)
{
    const u32 c = w->cell[s];
    const size_t base = (size_t)s * 3u;
    const u8 *ph = w->phen + (size_t)s * AZ_PHEN;
    const i32 density = w->c_med[(size_t)c * 2u];
    const i32 viscous = (i32)byte_product(ph[AZ_P_DRAG], w->c_med[(size_t)c * 2u + 1u]);
    const i32 contrast = w->cfg.lesion & AZ_LESION_GRAVITY ? 0 : (i32)ph[AZ_P_DENS] - density;
    const i32 divisor = (i32)(1u << w->param[AZ_WP_PHYS_BUOY_SHIFT]);
    b->drag[lane] = (u8)(255u - byte_product(255u - (u32)viscous, 255u - w->c_solid[c]));
    const u32 quote = w->flags[s] & (AZ_F_EGG | AZ_F_DORMANT) ? 0 : w->phys_quote[s];
    const u32 paid = az_min_u32(quote, (u32)az_max_i32(0, w->energy[s]));
    if (paid) {
        w->energy[s] -= (i32)paid;
        az_cell_post_heat(w, worker, c, (i32)paid);
    }
    b->slot[lane] = s; b->x[lane] = w->pos_x[s]; b->y[lane] = w->pos_y[s];
    const i16 start[3] = {w->vel_x[s], w->vel_y[s], w->vel_z[s]};
    AzTally *tally = az_tally(w, worker);
    for (u32 a = 0; a < 3; ++a) {
        const i32 passive = w->phys_passive[base + a];
        const i32 request = (i32)w->acc[base + a] - passive;
        const i32 active = !paid ? 0 : paid == quote ? request : request * (i32)paid / (i32)quote;
        const i32 weight = (i32)w->c_grav[(size_t)c * 4u + a] * contrast / divisor;
        const i32 wind = (i32)w->c_wind[(size_t)c * 3u + a] * density * viscous / 255;
        const i32 force = passive + active + weight + wind;
        b->force[lane][a] = force;
        const i32 raw = start[a] + force;
        const i32 limited = az_clamp_i32(raw, -AZ_VEL_MAX, AZ_VEL_MAX);
        tally->sat[AZ_SAT_VELOCITY] += raw != limited;
        if (a == 2) {
            b->vz[lane] = (i16)(limited * (256 - b->drag[lane]) / 256);
        } else {
            /* dv is an i16 kernel operand. A large force is equivalent to the
             * already clamped velocity plus zero increment. Count the original
             * full-width clamp above, including an old save with a wide speed. */
            const bool fits = force >= INT16_MIN && force <= INT16_MAX;
            i16 v = fits ? start[a] : (i16)limited;
            i16 dv = fits ? (i16)force : 0;
            if (a == 0) { b->vx[lane] = v; b->dx[lane] = dv; }
            else        { b->vy[lane] = v; b->dy[lane] = dv; }
        }
    }
}

static i16 impact(i16 proposed, i16 prior, i32 force)
{
    const bool pressed = (proposed > 0 && force > 0) || (proposed < 0 && force < 0);
    return pressed && az_abs_i32(prior) <= az_abs_i32(force) ? 0 : (i16)-proposed;
}

static i32 wrap_distance(u32 after, u32 before, u32 shift)
{
    const u32 forward = after - before;
    const i64 signed_delta = forward <= INT32_MAX ? (i64)forward : (i64)forward - (1ll << 32);
    return (i32)(signed_delta / (1ll << (shift - 16u)));
}

static void commit_motion(AzWorld *w, AzMotionBlock *b, u32 i)
{
    const u32 s = b->slot[i];
    const u32 old_x = w->pos_x[s], old_y = w->pos_y[s], old_z = w->pos_z[s];
    const i16 vx = b->vx[i], vy = b->vy[i], vz = b->vz[i];
    const i64 step_z = 1ll << (w->shift_z - 16u);
    const i64 requested_z = (i64)old_z + (i64)vz * step_z;
    u32 x = b->x[i], y = b->y[i];
    u32 z = (u32)az_clamp_i64(requested_z, PHYS_FLOOR, PHYS_CEILING);
    const bool hit_x = ((old_x ^ x) >> w->shift_x) && w->c_solid[az_cell_at(w, x, old_y, old_z)] == 255;
    if (hit_x) x = old_x;
    const bool hit_y = ((old_y ^ y) >> w->shift_y) && w->c_solid[az_cell_at(w, x, y, old_z)] == 255;
    if (hit_y) y = old_y;
    const bool rock_z = ((old_z ^ z) >> w->shift_z) && w->c_solid[az_cell_at(w, x, y, z)] == 255;
    if (rock_z) z = old_z;
    w->vel_x[s] = hit_x ? impact(vx, w->vel_x[s], b->force[i][0]) : vx;
    w->vel_y[s] = hit_y ? impact(vy, w->vel_y[s], b->force[i][1]) : vy;
    w->vel_z[s] = rock_z || (i64)z != requested_z ? impact(vz, w->vel_z[s], b->force[i][2]) : vz;
    w->pos_x[s] = x; w->pos_y[s] = y; w->pos_z[s] = z;
    const i64 dx = wrap_distance(x, old_x, w->shift_x);
    const i64 dy = wrap_distance(y, old_y, w->shift_y);
    const i64 dz = ((i64)z - (i64)old_z) / step_z;
    const u32 travel = az_isqrt_u64((u64)(dx * dx + dy * dy + dz * dz));
    const u64 carried = (u64)w->travel_carry[s] + travel;
    w->travel_carry[s] = carried > UINT32_MAX ? UINT32_MAX : (u32)carried;
    if (az_phase_due(w, w->uid[s], AZ_RS_PHYS, 4)) {
        az_stat_bump(w, s, AZ_ST_TRAVEL, w->travel_carry[s]);
        w->travel_carry[s] = 0;
    }
    turn(w, s);
}

void az_physics_range(AzWorld *w, u32 begin, u32 end, u32 worker)
{
    AzMotionBlock *b = (AzMotionBlock *)az_scratch(w, worker);
    while (begin < end) {
        u32 count = 0;
        const u32 limit = begin + az_min_u32(AZ_CHUNK, end - begin);
        for (; begin < limit; ++begin)
            if (w->flags[begin] & AZ_F_ALIVE) gather(w, b, count++, begin, worker);
        if (!count) continue;
        w->k.kin_axis(b->x, b->vx, b->dx, b->drag, count, w->shift_x);
        w->k.kin_axis(b->y, b->vy, b->dy, b->drag, count, w->shift_y);
        for (u32 i = 0; i < count; ++i) commit_motion(w, b, i);
    }
}

