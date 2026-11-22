/* az_life.c (reference stub) - death with full matter/energy return, asexual
 * budding, and the serial lifecycle commit. The module owner adds stages,
 * eggs, senescence, clutch sizes, sexual births and clade macro-mutation. */
#include "../az_abi.h"

/* Return everything the entity holds to its cell, then mark it dead.
 * Safe in phase 3: touches only this slot, the cell accumulators and atomics. */
void az_life_kill(AzWorld *w, u32 slot, u32 worker)
{
    u32 c = w->cell[slot];
    az_cell_deposit(w, worker, c, AZ_SP_ORG_Y, w->pool_a[slot]);
    az_cell_deposit(w, worker, c, AZ_SP_ORG_Y, w->pool_m[slot]);
    az_cell_deposit(w, worker, c, AZ_SP_ORG_Y, w->pool_t[slot]);
    {   /* what was built into the body goes back with the rest */
        u32 np;
        AzPart *part = az_parts(w, slot, &np);
        for (u32 i = 0; i < np; i++) {
            az_cell_deposit(w, worker, c, AZ_SP_ORG_Y, part[i].matter);
            part[i].matter = 0;
        }
    }
    az_cell_deposit(w, worker, c, AZ_SP_WST_W, w->waste[slot]);
    az_cell_deposit(w, worker, c, AZ_SP_TOX_Z, w->toxin[slot]);
    for (u32 k = 0; k < AZ_NINV; k++) {
        u32 q = w->inv[(size_t)slot * AZ_NINV + k];
        az_cell_deposit(w, worker, c, az_inv_species[k], q);
        w->inv[(size_t)slot * AZ_NINV + k] = 0;
    }
    if (w->energy[slot] > 0) az_cell_post_heat(w, worker, c, w->energy[slot]);
    w->pool_a[slot] = w->pool_m[slot] = w->pool_t[slot] = 0;
    w->waste[slot] = w->toxin[slot] = 0;
    w->energy[slot] = 0;
    w->mass[slot] = 0;
    w->flags[slot] = 0;
    az_atomic_add_u32(&w->clades[w->clade[slot]].refcount, 0xFFFFFFFFu);   /* -1 */
    az_tally(w, worker)->deaths++;
    az_emit(w, worker, AZ_EV_DEATH, (u32)(w->pair[slot].req >> 4), w->uid[slot], 0, 0);
}

/* one child with half of the mother's stores, when she holds 400 of each */
u32 az_life_brood(AzWorld *w, u32 mother, u32 father, u32 worker)
{
    (void)worker;
    if (w->pool_a[mother] < 400 || w->energy[mother] < 400) return 0;
    u32 prov_a = w->pool_a[mother] / 2u;
    i32 prov_e = w->energy[mother] / 2;
    if (!az_birth_request(w, mother, father, 1, prov_e, prov_a, 0, 0)) return 0;
    w->pool_a[mother] -= prov_a;
    w->energy[mother] -= prov_e;
    return 1;
}

void az_life_range(AzWorld *w, u32 begin, u32 end, u32 worker)
{
    for (u32 s = begin; s < end; s++) {
        if (!(w->flags[s] & AZ_F_ALIVE)) continue;
        if (w->pair[s].req & AZ_RQ_DIE) { az_life_kill(w, s, worker); continue; }
        (void)az_life_brood(w, s, AZ_NONE, worker);
    }
}

typedef struct BirthKey { u64 mother_uid; u64 mother_slot; } BirthKey;

/* stable LSD radix sort by mother uid; eight byte passes leave the result in a */
static void sort_births(BirthKey *a, BirthKey *b, u32 n)
{
    u32 cnt[256];
    for (u32 pass = 0; pass < 8; pass++) {
        const u32 shift = pass * 8;
        for (u32 i = 0; i < 256; i++) cnt[i] = 0;
        for (u32 i = 0; i < n; i++) cnt[(u32)(a[i].mother_uid >> shift) & 255u]++;
        u32 sum = 0;
        for (u32 i = 0; i < 256; i++) { u32 c = cnt[i]; cnt[i] = sum; sum += c; }
        for (u32 i = 0; i < n; i++) b[cnt[(u32)(a[i].mother_uid >> shift) & 255u]++] = a[i];
        BirthKey *t = a; a = b; b = t;
    }
}

void az_life_commit(AzWorld *w)
{
    /* collect the mothers, then put them in uid order (the canonical order);
     * tmp holds cap * 128 bytes: room for two arrays of one key per slot */
    BirthKey *all = (BirthKey *)(void *)w->tmp;
    u32 n = 0;
    for (u32 s = 0; s < w->n_slots; s++)
        if (w->birth[s].n) { all[n].mother_uid = w->uid[s]; all[n].mother_slot = s; n++; }
    sort_births(all, all + n, n);
    bool all_rock = false;  /* terrain stays fixed through this serial birth pass */

    for (u32 i = 0; i < n; i++) {
        const u32 m = (u32)all[i].mother_slot;
        AzBirth b = w->birth[m];
        w->birth[m].n = 0;                     /* the request is consumed either way */
        const u32 kids = b.n;
        for (u32 j = 0; j < kids && !all_rock; j++) {
            /* child j gets an equal share; the last child also gets the remainders */
            u32 left = kids - j;
            i32 e_share = (i32)((i64)b.energy / left);
            u32 a_share = b.prov[0] / left, m_share = b.prov[1] / left, t_share = b.prov[2] / left;
            const u64 uid = w->uid_next;
            u32 rr[4];
            az_rand4(w, uid, AZ_RS_BIRTH, 0, rr);
            const u32 jx = 1u << (w->shift_x - 3), jy = 1u << (w->shift_y - 3), jz = 1u << (w->shift_z - 3);
            u32 px = w->pos_x[m] + (rr[0] % jx) - jx / 2;
            u32 py = w->pos_y[m] + (rr[1] % jy) - jy / 2;
            i64 z = (i64)w->pos_z[m] + (i64)(rr[2] % jz) - (i64)(jz / 2);
            u32 pz = (u32)az_clamp_i64(z, 1 << 16, 0xFFFFFFFFll - (1 << 16));
            const u32 cell = az_rock_birth_place(w, m, &px, &py, &pz);
            if (cell == AZ_NONE) {
                all_rock = true;
                az_emit(w, 0, AZ_EV_REFUSED, AZ_REF_TERRAIN, w->uid[m], 0, 0);
                break;                         /* unused brood reserves are refunded below */
            }
            u32 ch = az_spawn(w);
            if (ch == AZ_NONE) break;          /* no slot: the rest is refunded below */
            /* the child's block (its genome) comes first: without room in the
             * creature store there is no child, and the slot stays dead */
            if (!az_conceive(w, ch, m, b.father)) break;
            b.energy -= e_share;
            b.prov[0] -= a_share;
            b.prov[1] -= m_share;
            b.prov[2] -= t_share;

            w->pos_x[ch] = px; w->pos_y[ch] = py; w->pos_z[ch] = pz;
            w->cell[ch] = cell;
            w->energy[ch] = e_share;
            w->pool_a[ch] = a_share; w->pool_m[ch] = m_share; w->pool_t[ch] = t_share;
            const u64 body_matter = (u64)a_share + m_share + t_share;
            w->mass[ch] = body_matter > UINT32_MAX ? UINT32_MAX : (u32)body_matter;
            w->health[ch] = 40000;
            w->flags[ch] = AZ_F_ALIVE | AZ_F_JUV;
            az_clade_assign(w, ch, m);       /* the mother's lineage, or a new one */
            w->colony[ch] = w->colony[m];
            w->parent_a[ch] = w->uid[m];
            w->parent_b[ch] = (b.father != AZ_NONE && b.father != m) ? w->uid[b.father] : 0;
            w->birth_tick[ch] = w->tick;
            w->generation[ch] = w->generation[m] + 1u;
            w->lineage[ch] = w->lineage[m];
            w->totals.births++;
            az_emit(w, 0, AZ_EV_BIRTH, 0, uid, w->uid[m], 0);
        }
        /* whatever was not handed to a child goes back to the mother's cell as
         * organic matter and heat (the mother may have died this tick) */
        for (u32 pool = 0; pool < 3; ++pool)
            az_cell_deposit(w, 0, w->cell[m], AZ_SP_ORG_Y, b.prov[pool]);
        if (b.energy > 0) az_cell_post_heat(w, 0, w->cell[m], b.energy);
    }
}
