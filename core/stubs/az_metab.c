/* az_metab.c (reference stub) - one-channel metabolism with exact books.
 * Eats organic matter, burns it for energy, pays a basal cost as heat,
 * excretes waste. Every transfer is an integer and balances exactly. */
#include "../az_abi.h"

/* what the pools of this body can hold: PART_HOLD for every part it has */
static u32 hold_mu(const AzWorld *w, u32 np)
{
    const u64 h = (u64)w->param[AZ_WP_PART_HOLD] * np;
    return h > w->param[AZ_WP_CELL_HOLD] ? w->param[AZ_WP_CELL_HOLD] : (u32)h;
}

void az_metab_demand(AzWorld *w, u32 slot, u32 worker)
{
    u32 *eat = &w->eat[(size_t)slot * AZ_NDIET];
    for (u32 d = 0; d < AZ_NDIET; d++) eat[d] = 0;
    u8 fl = w->flags[slot];
    if (!(fl & AZ_F_ALIVE) || (fl & (AZ_F_EGG | AZ_F_DORMANT))) return;
    i32 drive = w->motor[(size_t)slot * AZ_MOT + AZ_M_EAT];
    u32 want = 8u + (drive > 0 ? (u32)drive >> 3 : 0u);
    eat[AZ_D_ORG_Y] = want;
    az_cell_demand(w, worker, w->cell[slot], AZ_D_ORG_Y, want);
}

void az_metab_range(AzWorld *w, u32 begin, u32 end, u32 worker)
{
    for (u32 s = begin; s < end; s++) {
        if (s + 4u < end) az_blk_prefetch(w, s + 4u);    /* the body parts of a creature soon to come */
        u8 fl = w->flags[s];
        if (!(fl & AZ_F_ALIVE) || (fl & (AZ_F_EGG | AZ_F_DORMANT))) continue;
        u32 c = w->cell[s];
        const u8 *ph = &w->phen[(size_t)s * AZ_PHEN];

        /* uptake: my share of what the cell granted; what does not fit stays in the cell */
        u32 want = w->eat[(size_t)s * AZ_NDIET + AZ_D_ORG_Y];
        u32 taken = az_cell_granted(w, c, AZ_D_ORG_Y, want);
        u32 np;
        AzPart *part = az_parts(w, s, &np);
        const u32 hold = hold_mu(w, np);
        u32 room = hold > w->pool_a[s] ? hold - w->pool_a[s] : 0u;
        if (taken > room) taken = room;
        if (taken) {
            w->pool_a[s] += taken;                           /* ORG_Y (16) -> biomass (16) */
            az_cell_post(w, worker, c, AZ_SP_ORG_Y, -(i32)taken);
        }

        /* respiration: biomass (16) -> waste (2) releases 14 eu per mu */
        u32 burn = 1u + (ph[AZ_P_METAB] >> 6);
        if (burn > w->pool_a[s]) burn = w->pool_a[s];
        w->pool_a[s] -= burn;
        w->waste[s] += burn;
        w->energy[s] += (i32)(burn * (AZ_EPS_BIO - AZ_EPS_WASTE));

        /* basal cost, paid as heat; unpaid cost becomes damage */
        i32 cost = 4 + (ph[AZ_P_METAB] >> 4);
        i32 pay = az_min_i32(cost, w->energy[s]);
        w->energy[s] -= pay;
        az_cell_post_heat(w, worker, c, pay);
        if (pay < cost) {
            i32 dmg = (cost - pay) * 256;
            w->health[s] = (u16)az_max_i32(0, (i32)w->health[s] - dmg);
        }
        /* a body holds the energy its matter capacity is worth; the rest is heat */
        const i64 e_hold = (i64)hold * AZ_EPS_BIO > 0x3FFFFFFFll ? 0x3FFFFFFFll : (i64)hold * AZ_EPS_BIO;
        if (w->energy[s] > e_hold) {
            az_cell_post_heat(w, worker, c, w->energy[s] - (i32)e_hold);
            w->energy[s] = (i32)e_hold;
            az_tally(w, worker)->sat[AZ_SAT_ENERGY]++;
        }

        /* growth: the parts take turns; a share of the pool (the creature's
         * own ALLOMETRY) is built into this tick's part, and a body that has
         * run out of pool takes matter back from it. A new part is asked for
         * when the brain wants one and every part is full; an empty last
         * part is given up when the brain wants that. */
        u32 built = 0;
        if (np) {
            const u32 part_hold = w->param[AZ_WP_PART_HOLD];
            AzPart *p = &part[(u32)((w->tick + w->uid[s]) % np)];
            const u32 share = (u32)(((u64)w->pool_a[s] * (8u + w->trait[(size_t)s * AZ_TRAITS + AZ_T_ALLOMETRY])) >> 15);
            if (share && p->matter < part_hold) {
                const u32 g = az_min_u32(share, part_hold - p->matter);
                w->pool_a[s] -= g; p->matter += g;
            } else if (w->pool_a[s] == 0 && p->matter) {
                const u32 g = az_min_u32(p->matter, 1u + (burn << 2));
                p->matter -= g; w->pool_a[s] += g;
            }
            bool full = true;
            for (u32 i = 0; i < np; i++) { built += part[i].matter; full = full && part[i].matter >= part_hold; }
            const i8 *mo = &w->motor[(size_t)s * AZ_MOT];
            if (full && mo[AZ_M_GROW] > 0) {
                az_part_request(w, s, 1u + (u32)((w->trait[(size_t)s * AZ_TRAITS + AZ_T_SEGMENT] + np) % (AZ_PART_KINDS - 1u)));
            } else if (np > 1u && part[np - 1u].matter == 0 && mo[AZ_M_SHED] > 96) {
                az_part_shed_request(w, s, np - 1u);
            }
        }

        /* excretion */
        if (w->waste[s] > 16) {
            az_cell_post(w, worker, c, AZ_SP_WST_W, (i32)w->waste[s]);
            w->waste[s] = 0;
        }

        w->mass[s] = w->pool_a[s] + w->pool_m[s] + w->pool_t[s] + w->waste[s] + w->toxin[s] + built;
        if (w->health[s] == 0 || (w->pool_a[s] == 0 && built == 0 && w->energy[s] == 0)) w->pair[s].req |= AZ_RQ_DIE;
    }
}
