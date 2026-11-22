/* az_physics.c (reference stub) - thrust in the world frame, linear drag,
 * wrap in x and y, reflect in z. The module owner adds the body frame,
 * gravity, buoyancy, the medium, terrain and the energy cost of thrust. */
#include "../az_abi.h"

void az_physics_steer(AzWorld *w, u32 slot, const AzPercept *p, u32 worker)
{
    (void)worker;
    i16 *acc = &w->acc[(size_t)slot * 3];
    const i8 *m = &w->motor[(size_t)slot * AZ_MOT];
    u32 gain = 1u + (w->phen[(size_t)slot * AZ_PHEN + AZ_P_THRUST] >> 5);
    for (u32 a = 0; a < 3; a++) {
        i32 v = (i32)m[AZ_M_FWD + a] * (i32)gain + (p->push[a] >> 4);
        acc[a] = az_sat_i16(v);
    }
}

void az_physics_range(AzWorld *w, u32 begin, u32 end, u32 worker)
{
    (void)worker;
    for (u32 s = begin; s < end; s++) {
        u8 fl = w->flags[s];
        if (!(fl & AZ_F_ALIVE) || (fl & (AZ_F_EGG | AZ_F_DORMANT))) continue;
        const i16 *acc = &w->acc[(size_t)s * 3];
        i32 vx = w->vel_x[s] + acc[0], vy = w->vel_y[s] + acc[1], vz = w->vel_z[s] + acc[2];
        vx -= vx >> 3; vy -= vy >> 3; vz -= vz >> 3;
        vx = az_clamp_i32(vx, -16000, 16000);
        vy = az_clamp_i32(vy, -16000, 16000);
        vz = az_clamp_i32(vz, -16000, 16000);

        w->pos_x[s] = az_pos_step(w->pos_x[s], vx, w->shift_x);
        w->pos_y[s] = az_pos_step(w->pos_y[s], vy, w->shift_y);

        /* z is bounded: reflect at the floor and the ceiling */
        i64 z = (i64)w->pos_z[s] + (i64)vz * (i64)(1u << (w->shift_z - 16));
        const i64 lo = 1 << 16, hi = 0xFFFFFFFFll - (1 << 16);
        if (z < lo) { z = lo; vz = -vz; }
        if (z > hi) { z = hi; vz = -vz; }
        w->pos_z[s] = (u32)z;

        w->vel_x[s] = (i16)vx; w->vel_y[s] = (i16)vy; w->vel_z[s] = (i16)vz;
    }
}

void az_physics_steer_fin(AzWorld *w, u32 slot, const AzPercept *p, u8 fin, u32 worker)
{
    (void)fin;
    az_physics_steer(w, slot, p, worker);
}
