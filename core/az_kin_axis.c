/* Definition shared by the portable and modular native builds. */
#include "az_abi.h"

void az_kin_axis_c(u32 *pos, i16 *vel, const i16 *dv, const u8 *drag, u32 n, u32 shift)
{
    for (u32 i = 0; i < n; i++) {
        i32 v = az_clamp_i32((i32)vel[i] + dv[i], -AZ_VEL_MAX, AZ_VEL_MAX);
        v = v * (256 - (i32)drag[i]) / 256;
        vel[i] = (i16)v;
        pos[i] = az_pos_step(pos[i], v, shift);
    }
}
