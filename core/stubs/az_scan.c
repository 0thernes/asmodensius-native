/* az_scan.c (reference) - neighbour scan over the 27 surrounding cells.
 *
 * This file defines the exact integer semantics every faster twin (optimised
 * C, assembly, GPU) must reproduce bit for bit.
 *
 * Canonical order: dz, then dy, then dx increasing from -1 to +1; inside a
 * cell increasing uid (the order[] sequence). A neighbour counts if its
 * squared distance is <= range^2. Only the first AZ_NEIGH_MAX such
 * neighbours are used. "Nearest" ties keep the earlier one in that order.
 */
#include "../az_abi.h"

static void percept_clear(AzPercept *p)
{
    p->n_used = p->n_touch = p->n_kin = p->n_col = p->n_prey = p->n_threat = 0;
    for (u32 a = 0; a < 3; a++) {
        p->kin[a] = p->col[a] = p->prey[a] = p->threat[a] = p->vel[a] = p->push[a] = 0;
        p->mate_off[a] = p->bond_off[a] = p->parent_off[a] = p->child_off[a] = 0;
    }
    p->sig_a = p->sig_b = 0;
    for (u32 a = 0; a < 3; a++) p->sigv_a[a] = p->sigv_b[a] = 0;
    p->near_prey = p->near_threat = p->near_kin = AZ_NONE;
    p->best_mate = p->bond = p->parent = p->child = AZ_NONE;
    p->faced_d2 = AZ_NONE; p->faced_stake = 0;
    p->d2_prey = p->d2_threat = p->d2_kin = 0xFFFFFFFFu;
    p->d2_mate = p->d2_bond = p->d2_parent = p->d2_child = 0xFFFFFFFFu;
}

static void scan_one(const AzWorld *w, u32 c, u32 me_p, AzPercept *p)
{
    const AzPub *me = &w->pub[me_p];
    const u32 slot = w->order[me_p];
    const bool no_kin = (w->cfg.lesion & AZ_LESION_KIN) != 0;

    percept_clear(p);

    const u32 range = ((u32)w->phen[(size_t)slot * AZ_PHEN + AZ_P_SENSE] + 1u) * (AZ_SUB_ONE / 256);
    const u32 r2 = range * range;
    const u32 my_size = AZ_PUB_SIZE(me->rf);
    const u32 my_r = az_radius_ns(my_size);
    const u32 kin_window = w->phen[(size_t)slot * AZ_PHEN + AZ_P_KIN];
    u32 used = 0;

    for (i32 dz = -1; dz <= 1 && used < AZ_NEIGH_MAX; dz++)
    for (i32 dy = -1; dy <= 1 && used < AZ_NEIGH_MAX; dy++)
    for (i32 dx = -1; dx <= 1 && used < AZ_NEIGH_MAX; dx++) {
        u32 nb = az_cell_nb(w, c, dx, dy, dz);
        if (nb == AZ_NONE) continue;
        u32 q0 = w->cell_start[nb], q1 = w->cell_start[nb + 1];
        for (u32 q = q0; q < q1 && used < AZ_NEIGH_MAX; q++) {
            if (q == me_p) continue;
            const AzPub *o = &w->pub[q];
            i32 off[3];
            off[0] = (i32)o->x - (i32)me->x + dx * AZ_SUB_ONE;
            off[1] = (i32)o->y - (i32)me->y + dy * AZ_SUB_ONE;
            off[2] = (i32)o->z - (i32)me->z + dz * AZ_SUB_ONE;
            u32 d2 = (u32)(off[0] * off[0]) + (u32)(off[1] * off[1]) + (u32)(off[2] * off[2]);
            if (d2 > r2) continue;
            used++;

            u32 oslot = w->order[q];
            u32 touch = my_r + az_radius_ns(AZ_PUB_SIZE(o->rf));
            if (d2 < touch * touch) {
                p->n_touch++;
                for (u32 a = 0; a < 3; a++) p->push[a] -= off[a] >> 2;
            }
            /* signal intensity falls off linearly in squared distance: 256 at 0, 0 at the range */
            u32 att = (u32)((((u64)(r2 - d2)) << 8) / r2);
            const i32 got_a = (i32)(((u32)o->sig_a * att) >> 8), got_b = (i32)(((u32)o->sig_b * att) >> 8);
            p->sig_a += (u32)got_a;
            p->sig_b += (u32)got_b;
            for (u32 a = 0; a < 3; a++) {
                p->sigv_a[a] += (off[a] >> 6) * got_a;
                p->sigv_b[a] += (off[a] >> 6) * got_b;
            }

            bool passive = (o->rf & AZ_PF_PASSIVE) != 0;
            bool same_col = me->colony != 0 && o->colony == me->colony;
            bool kin = !no_kin && az_tag_dist(me->tag, o->tag) <= kin_window;
            if (same_col) {
                p->n_col++;
                for (u32 a = 0; a < 3; a++) { p->col[a] += off[a]; }
                p->vel[0] += o->vx; p->vel[1] += o->vy; p->vel[2] += o->vz;
                if (d2 < p->d2_kin) { p->d2_kin = d2; p->near_kin = oslot; }
            } else if (kin) {
                p->n_kin++;
                for (u32 a = 0; a < 3; a++) { p->kin[a] += off[a]; }
                p->vel[0] += o->vx; p->vel[1] += o->vy; p->vel[2] += o->vz;
                if (d2 < p->d2_kin) { p->d2_kin = d2; p->near_kin = oslot; }
            } else if (!passive && AZ_PUB_SIZE(o->rf) > my_size) {
                p->n_threat++;
                for (u32 a = 0; a < 3; a++) { p->threat[a] += off[a]; }
                if (d2 < p->d2_threat) { p->d2_threat = d2; p->near_threat = oslot; }
            } else {
                p->n_prey++;
                for (u32 a = 0; a < 3; a++) { p->prey[a] += off[a]; }
                if (d2 < p->d2_prey) { p->d2_prey = d2; p->near_prey = oslot; }
            }

            const i32 ahead = me->face[0]*off[0] + me->face[1]*off[1] + me->face[2]*off[2];
            if (ahead > 0 && d2 < p->faced_d2) { p->faced_d2 = d2; p->faced_stake = o->stake; }
            if ((o->rf & AZ_PF_RECEPT) && !(o->rf & (AZ_PF_PASSIVE | AZ_PF_JUV)) && d2 < p->d2_mate) {
                p->d2_mate = d2; p->best_mate = oslot;
                for (u32 a = 0; a < 3; a++) p->mate_off[a] = off[a];
            }
            const AzPubId *oid = &w->pub_id[q], *mid = &w->pub_id[me_p];
            if (mid->partner != 0 && oid->uid == mid->partner && d2 < p->d2_bond) {
                p->d2_bond = d2; p->bond = oslot;
                for (u32 a = 0; a < 3; a++) p->bond_off[a] = off[a];
            }
            if ((mid->parent_a != 0 && oid->uid == mid->parent_a) || (mid->parent_b != 0 && oid->uid == mid->parent_b)) {
                if (d2 < p->d2_parent) {
                    p->d2_parent = d2; p->parent = oslot;
                    for (u32 a = 0; a < 3; a++) p->parent_off[a] = off[a];
                }
            }
            if ((oid->parent_a == mid->uid || oid->parent_b == mid->uid) && d2 < p->d2_child) {
                p->d2_child = d2; p->child = oslot;
                for (u32 a = 0; a < 3; a++) p->child_off[a] = off[a];
            }
        }
    }
    p->n_used = (u16)used;
}

void az_scan_range(const AzWorld *w, u32 p0, u32 count, AzPercept *out, u32 worker)
{
    (void)worker;   /* the reference needs no working buffer */
    for (u32 k = 0; k < count; k++) {
        u32 p = p0 + k;
        scan_one(w, w->cell[w->order[p]], p, &out[k]);
    }
}

u32 az_scan_list(const AzWorld *w, u32 slot, i32 (*off)[3], u32 *oslot, u32 max, u32 worker)
{
    (void)worker;
    const u32 c = w->cell[slot];
    const u32 me_p = w->spos[slot];
    if (c == AZ_NONE || me_p == AZ_NONE) return 0;
    const AzPub *me = &w->pub[me_p];
    const u32 range = ((u32)w->phen[(size_t)slot * AZ_PHEN + AZ_P_SENSE] + 1u) * (AZ_SUB_ONE / 256);
    const u32 r2 = range * range;
    if (max > AZ_NEIGH_MAX) max = AZ_NEIGH_MAX;
    u32 n = 0;
    for (i32 dz = -1; dz <= 1 && n < max; dz++)
    for (i32 dy = -1; dy <= 1 && n < max; dy++)
    for (i32 dx = -1; dx <= 1 && n < max; dx++) {
        u32 nb = az_cell_nb(w, c, dx, dy, dz);
        if (nb == AZ_NONE) continue;
        u32 q0 = w->cell_start[nb], q1 = w->cell_start[nb + 1];
        for (u32 q = q0; q < q1 && n < max; q++) {
            if (q == me_p) continue;
            const AzPub *o = &w->pub[q];
            i32 ox = (i32)o->x - (i32)me->x + dx * AZ_SUB_ONE;
            i32 oy = (i32)o->y - (i32)me->y + dy * AZ_SUB_ONE;
            i32 oz = (i32)o->z - (i32)me->z + dz * AZ_SUB_ONE;
            u32 d2 = (u32)(ox * ox) + (u32)(oy * oy) + (u32)(oz * oz);
            if (d2 > r2) continue;
            off[n][0] = ox; off[n][1] = oy; off[n][2] = oz;
            oslot[n] = w->order[q];
            n++;
        }
    }
    return n;
}
