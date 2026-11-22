/* az_field.c (reference) - field plumbing: grant and commit.
 * The rule that keeps the world exact: entities and field dynamics only post
 * integer changes (az_cell_post, az_cell_post_heat, az_cell_demand,
 * az_cell_mark); this file is the only place that applies them. */
#include "../az_abi.h"

u32 az_field_avail(const AzWorld *w, u32 c, u32 diet)
{
    const u32 *sp = &w->c_sp[(size_t)c * AZ_NSPECIES];
    switch (diet) {
    case AZ_D_LIGHT: return (u32)w->c_light[c] * AZ_LIGHT_EU;
    case AZ_D_NUT_X: return sp[AZ_SP_NUT_X];
    case AZ_D_ORG_Y: return sp[AZ_SP_ORG_Y];
    case AZ_D_WST_W: return sp[AZ_SP_WST_W];
    case AZ_D_OXI:   return sp[AZ_SP_OXI];
    case AZ_D_STR_S: return sp[AZ_SP_STR_S];
    case AZ_D_TOX_Z: return sp[AZ_SP_TOX_Z];
    case AZ_D_SOLV:  return sp[AZ_SP_SOLV];
    default:         return 0;
    }
}

/* Pressure: weight of the gas and solvent in this cell and in every cell
 * above it, scaled down. Simple and monotone in depth; the module owner may
 * refine it, but every module must use this one function. */
u32 az_cell_pressure(const AzWorld *w, u32 c)
{
    u32 x = az_cell_x(w, c), y = az_cell_y(w, c), z = az_cell_z(w, c);
    u64 sum = 0;
    for (u32 zz = z; zz < w->gz; zz++) {
        const u32 *sp = &w->c_sp[(size_t)az_cell_xyz(w, x, y, zz) * AZ_NSPECIES];
        sum += (u64)sp[AZ_SP_GAS] + 4ull * sp[AZ_SP_SOLV];
    }
    sum >>= 6;
    return sum > 0xFFFFFFFFull ? 0xFFFFFFFFu : (u32)sum;
}

void az_publish_pressure(AzWorld *w)
{
    for (u32 c=0;c<w->n_cells;c++) w->c_pressure[c]=az_cell_pressure(w,c);
}

/* grant = fraction of each demand that can be met; an entity takes
 * floor(demand * grant / 65535) (az_cell_granted), so the sum never exceeds
 * what is there. The demand is cleared here, so there is no per-tick reset
 * pass; a grant is only meaningful for a channel that had demand this tick. */
void az_field_grant(AzWorld *w, u32 begin, u32 end, u32 worker)
{
    (void)worker;
    for (u32 c = begin; c < end; c++) {
        AzCellNeed *nd = &w->c_need[c];
        for (u32 d = 0; d < AZ_NDIET; d++) {
            u64 dem = nd->dem[d];
            if (!dem) continue;
            u32 avail = az_field_avail(w, c, d);
            w->c_grant[(size_t)c * AZ_NDIET + d] = dem <= avail ? 65535u : (u16)(((u64)avail * 65535u) / dem);
            nd->dem[d] = 0;
        }
    }
}

void az_field_commit(AzWorld *w, u32 begin, u32 end, u32 worker)
{
    AzTally *ty = az_tally(w, worker);
    for (u32 c = begin; c < end; c++) {
        AzCellAcc *a = &w->c_acc[c];
        for (u32 k = 0; k < AZ_NSPECIES; k++) az_commit_species(w, c, k, ty);
        az_commit_heat(w, c, ty);
        u8 *mk = &w->c_mark[(size_t)c * AZ_NMARK];
        if (a->dmark_id && (a->dmark_id >> 32) >= mk[0]) w->c_mark_id[c] = (u32)a->dmark_id;
        a->dmark_id = 0;
        for (u32 k = 0; k < AZ_NMARK; k++) {
            if (!a->dmark[k]) continue;
            u64 v = (u64)mk[k] + a->dmark[k];
            if (v > 255) { v = 255; ty->sat[AZ_SAT_MARK]++; }
            mk[k] = (u8)v;
            a->dmark[k] = 0;
        }
    }
}
