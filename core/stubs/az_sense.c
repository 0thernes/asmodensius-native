/* az_sense.c (reference stub) - assemble the sensory channels.
 * Minimal: a few interoceptive channels, crowding, noise, and the four
 * neighbour directions in the WORLD frame. The module owner implements the
 * body frame, the field gradients and every remaining channel. */
#include "../az_abi.h"
#include "../az_body_parts.h"

void az_sense_cell(const AzWorld *w, u32 c, AzCellSense *cs)
{
    az_memset(cs, 0, sizeof(*cs));
    cs->light = w->c_light[c];
    cs->temp = w->c_temp[c];
    cs->toxin = w->c_sp[(size_t)c * AZ_NSPECIES + AZ_SP_TOX_Z];
}

void az_sense(const AzWorld *w, u32 slot, const AzPercept *p, const AzCellSense *cs, u8 sens[AZ_SENS])
{
    for (u32 i = 0; i < AZ_SENS; i++) sens[i] = 0;
    sens[AZ_S_ENERGY]  = (u8)az_clamp_i32(w->energy[slot] >> 6, 0, 127);
    sens[AZ_S_HEALTH]  = (u8)(w->health[slot] >> 9);
    sens[AZ_S_CROWD]   = (u8)az_clamp_i32((i32)p->n_used * 4, 0, 127);
    sens[AZ_S_LIGHT]   = (u8)az_clamp_i32((i32)cs->light >> 1, 0, 127);
    sens[AZ_S_NOISE]   = (u8)(az_rand(w, w->uid[slot], AZ_RS_NOISE, 0) & 127u);
    sens[AZ_S_TOUCH]   = (u8)az_clamp_i32((i32)p->n_touch * 32, 0, 127);
    sens[AZ_S_AROUSAL] = (u8)(w->arousal[slot] >> 1);
    sens[AZ_S_SEASON]  = (u8)(((u32)w->gstate[AZ_G_SEASON] & 0xFFFFu) >> 9);     /* how far the two suns are apart (az_weather.c) */
    sens[AZ_S_STORM]   = (u8)az_clamp_i32(w->gstate[AZ_G_STORM] >> 1, 0, 127);   /* the storm's strength, felt everywhere */
    sens[AZ_S_SIG_A]   = (u8)az_clamp_i32((i32)(p->sig_a >> 2), 0, 127);
    sens[AZ_S_SIG_B]   = (u8)az_clamp_i32((i32)(p->sig_b >> 2), 0, 127);
    i32 kin[3] = { p->kin[0] + p->col[0], p->kin[1] + p->col[1], p->kin[2] + p->col[2] };
    az_tetra4(p->threat, 10, &sens[AZ_S_THREAT]);
    az_tetra4(p->prey, 10, &sens[AZ_S_PREY]);
    az_tetra4(kin, 10, &sens[AZ_S_KIN]);
    /* the directional channels read 64 where nothing pulls either way */
    for (u32 i = AZ_S_GRAD_T; i <= AZ_S_SIGB_SIDE; i++) sens[i] = 64;
    sens[AZ_S_PAYOFF] = 64;
    {   /* the body and the colony: the levels below and above the creature */
        u32 np, m = w->mass[slot], lg = 0;
        (void)az_parts(w, slot, &np);
        while (m > 1u) { m >>= 1; lg++; }
        sens[AZ_S_MASS] = (u8)(lg * 4u);
        sens[AZ_S_PARTS] = (u8)az_min_u32(np * 8u, 127u);
        const u32 col = w->colony[slot];
        if (col && col < w->n_colonies) {
            sens[AZ_S_COLONY] = (u8)(w->colonies[col].signal[0] >> 1);
            /* the call for its own role; for one without a role, the loudest call (az_colony.c) */
            sens[AZ_S_COLONY_NEED] = (u8)(w->colonies[col].need[w->role[slot] < AZ_ROLE_COUNT ? w->role[slot] : 0u] >> 1);
        }
    }
}

u8 az_sense_and_fin(const AzWorld *w, u32 slot, const AzPercept *p, const AzCellSense *cs, u8 sens[AZ_SENS])
{
    az_sense(w, slot, p, cs, sens);
    return az_body_parts(w, slot).fin;
}
