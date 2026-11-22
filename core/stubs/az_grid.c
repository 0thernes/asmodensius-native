/* az_grid.c (reference) - spatial binning, canonical cell lists, the public
 * snapshot, the free list of dead slots, and the periodic physical reorder.
 * Serial and simple: this file defines the exact outputs. The module owner
 * replaces it with a parallel version that produces the same cell[], spos[],
 * cell_start[], order[], pub[], flags, n_live, n_sorted and free_head. */
#include "../az_abi.h"

void az_grid_build(AzWorld *w)
{
    const u32 nc = w->n_cells;
    u32 *cs = w->cell_start;
    u32 *cur = w->tmp_u32;

    for (u32 c = 0; c <= nc; c++) cs[c] = 0;

    /* cells, roles, and the free list of dead slots (highest slot first) */
    u32 live = 0;
    u32 free_head = AZ_NONE;
    for (u32 s = 0; s < w->n_slots; s++) {
        u8 fl = w->flags[s];
        if (!(fl & AZ_F_ALIVE)) {
            az_blk_release(w, s);        /* the dead give their block back */
            w->cell[s] = AZ_NONE;
            w->spos[s] = free_head;      /* link to the next lower dead slot */
            free_head = s;
            continue;
        }
        u32 c = az_cell_at(w, w->pos_x[s], w->pos_y[s], w->pos_z[s]);
        w->cell[s] = c;
        cs[c + 1]++;
        fl = (u8)(fl & ~AZ_F_INIT);
        if (az_is_initiator(w, w->uid[s])) fl |= AZ_F_INIT;
        w->flags[s] = fl;
        live++;
    }
    w->free_head = free_head;

    for (u32 c = 0; c < nc; c++) cs[c + 1] += cs[c];
    for (u32 c = 0; c < nc; c++) cur[c] = cs[c];

    for (u32 s = 0; s < w->n_slots; s++) {
        if (w->cell[s] == AZ_NONE) continue;
        w->order[cur[w->cell[s]]++] = s;
    }

    /* canonical order inside each cell: increasing uid (determinism rule D3) */
    for (u32 c = 0; c < nc; c++) {
        u32 a = cs[c], b = cs[c + 1];
        for (u32 i = a + 1; i < b; i++) {
            u32 s = w->order[i];
            u64 key = w->uid[s];
            u32 j = i;
            while (j > a && w->uid[w->order[j - 1]] > key) { w->order[j] = w->order[j - 1]; j--; }
            w->order[j] = s;
        }
    }

    for (u32 p = 0; p < live; p++) {
        u32 s = w->order[p];
        w->spos[s] = p;
        AzPub *o = &w->pub[p];
        o->x = (u16)az_sub(w->pos_x[s], w->shift_x);
        o->y = (u16)az_sub(w->pos_y[s], w->shift_y);
        o->z = (u16)az_sub(w->pos_z[s], w->shift_z);
        o->vx = (i8)(w->vel_x[s] >> 8);
        o->vy = (i8)(w->vel_y[s] >> 8);
        o->vz = (i8)(w->vel_z[s] >> 8);
        u8 fl = w->flags[s];
        u8 pf = 0;
        if (fl & AZ_F_RECEPT) pf |= AZ_PF_RECEPT;
        if (fl & AZ_F_COOP)   pf |= AZ_PF_COOP;
        if (fl & (AZ_F_EGG | AZ_F_DORMANT)) pf |= AZ_PF_PASSIVE;
        if (fl & AZ_F_JUV)    pf |= AZ_PF_JUV;
        o->rf = (u8)((az_radius_code(w->mass[s]) << 4) | pf);
        o->tag = w->tag[s];
        o->sig_a = w->sig[(size_t)s * 2];
        o->sig_b = w->sig[(size_t)s * 2 + 1];
        {   /* the forward axis of the body in the world frame, length 127 */
            const i16 rot[4] = { w->rot_w[s], w->rot_a[s], w->rot_b[s], w->rot_c[s] };
            const i32 fwd[3] = { 127, 0, 0 };
            i32 f[3];
            az_rotor_apply(f, rot, fwd);
            o->face[0] = (i8)az_clamp_i32(f[0], -127, 127);
            o->face[1] = (i8)az_clamp_i32(f[1], -127, 127);
            o->face[2] = (i8)az_clamp_i32(f[2], -127, 127);
        }
        o->role = w->role[s];
        const AzSenseMemory *history = az_sense_memory(w, s);
        o->stake = history ? history->stake : 0;
        o->pad = 0;
        o->colony = w->colony[s];
        o->mass = w->mass[s];
        o->clade = w->clade[s];
        AzPubId *id = &w->pub_id[p];
        id->uid = w->uid[s];
        id->parent_a = w->parent_a[s];
        id->parent_b = w->parent_b[s];
        id->partner = w->partner[s];
    }
    w->n_live = live;
    w->n_sorted = live;
}

/* Move every alive entity to slot = its sorted position; drop dead slots. */
void az_grid_reorder(AzWorld *w)
{
    az_grid_build(w);
    const u32 n = w->n_live;
    const u32 ncol = az_n_columns();
    for (u32 k = 0; k < ncol; k++) {
        AzColDesc col;
        az_column(w, k, &col);
        if (col.tier == AZ_TIER_EPH) continue;
        size_t bytes = (size_t)col.elem * col.per;
        u8 *data = (u8 *)col.data;
        u8 *tmp = w->tmp;                      /* cap * 128 bytes; bytes <= 128 */
        for (u32 p = 0; p < n; p++)
            az_memcpy(tmp + (size_t)p * bytes, data + (size_t)w->order[p] * bytes, bytes);
        az_memcpy(data, tmp, (size_t)n * bytes);
    }
    w->n_slots = n;
    w->free_head = AZ_NONE;
}
