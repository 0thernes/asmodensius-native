#ifndef AZ_COLONY_STORE_H
#define AZ_COLONY_STORE_H

/* the cell under a point; above it where that cell is bedrock */
AZ_INLINE u32 az_colony_ground(const AzWorld *w, u32 x, u32 y, u32 z)
{
    u32 c = az_cell_at(w, x, y, z);
    while (w->c_solid[c] == 255u) {
        const u32 up = az_cell_nb(w, c, 0, 0, 1);
        if (up == AZ_NONE) break;
        c = up;
    }
    return c;
}

/* everything goes back, to the last unit; more than one post carries moves on to the next cell */
AZ_INLINE void az_colony_spill(AzWorld *w, u32 cell, u32 species, bool heat, u64 amount)
{
    while (amount) {
        const u32 part = amount > 0x40000000u ? 0x40000000u : (u32)amount;
        if (heat) az_cell_post_heat(w, 0, cell, (i32)part);
        else az_cell_post(w, 0, cell, species, (i32)part);
        amount -= part;
        cell = az_cell_nb(w, cell, 1, 0, 0);
    }
}

/* A net withdrawal must be funded before either posted sum is cleared. */
AZ_INLINE bool az_pantry_balance_after(u64 stock, u64 dep, u64 taken, u64 *balance)
{
    if (taken > dep) {
        const u64 draw = taken - dep;
        if (draw > stock) return false;
        *balance = stock - draw;
    } else {
        const u64 gift = dep - taken;
        if (gift > UINT64_MAX - stock) return false;
        *balance = stock + gift;
    }
    return true;
}

/* At the serial barrier each overfull destination records one refusal and
 * immediately posts the excess to its centroid field, using the dissolution
 * path. Field posts are durable even when their own destination is full. */
AZ_INLINE u32 az_pantry_accept(AzWorld *w, const AzColony *c, u64 amount, u32 species, bool heat)
{
    const u32 limit = w->param[AZ_WP_CELL_HOLD];
    if (amount <= limit) return (u32)amount;
    const u32 cell = az_colony_ground(w, c->cx, c->cy, c->cz);
    az_colony_spill(w, cell, species, heat, amount - limit);
    w->totals.sat[heat ? AZ_SAT_ENERGY : AZ_SAT_INVENTORY]++;
    return limit;
}

AZ_INLINE void az_grant_colony_stores(AzWorld *w)
{
    for (u32 i = 1; i < w->n_colonies; i++) {
        AzColonyEph *e = &w->colony_eph[i];
        for (u32 k = 0; k < AZ_NINV; k++) {
            const u64 dem = e->dem[k];
            const u32 have = az_min_u32(w->colonies[i].store[k], w->param[AZ_WP_CELL_HOLD]);
            e->grant[k] = !dem ? 0 : (dem <= have ? 65535u : (u16)((u64)have * 65535u / dem));
        }
    }
}

AZ_INLINE void az_commit_colony_stores(AzWorld *w)
{
    for (u32 i = 1; i < w->n_colonies; i++) {
        AzColony *c = &w->colonies[i];
        AzColonyEph *e = &w->colony_eph[i];
        for (u32 k = 0; k < AZ_NINV; k++) {
            if (!e->dep[k] && !e->taken[k] && c->store[k] <= w->param[AZ_WP_CELL_HOLD]) continue;
            u64 balance;
            if (!az_pantry_balance_after(c->store[k], e->dep[k], e->taken[k], &balance)) {
                w->totals.field_underflow++;
                continue;
            }
            c->store[k] = az_pantry_accept(w, c, balance, az_inv_species[k], false);
            e->dep[k] = 0;
            e->taken[k] = 0;
        }
    }
}

AZ_INLINE void az_commit_colony_energy(AzWorld *w, AzColony *c, AzColonyEph *e)
{
    u64 dep = e->dep_e, taken = e->taken_e;
    if (!dep && !taken && c->energy <= (i64)w->param[AZ_WP_CELL_HOLD]) return;
    u64 stock = c->energy > 0 ? (u64)c->energy : 0;
    if (c->energy < 0) {
        /* Historical signed debt remains visible. A gift repays it before
         * adding to the pool. A further unfunded draw stays pending. */
        const u64 debt = 0u - (u64)c->energy;
        if (taken > dep) { w->totals.field_underflow++; return; }
        dep -= taken;
        taken = 0;
        if (dep < debt) {
            c->energy = (i64)((u64)c->energy + dep);
            e->dep_e = e->taken_e = 0;
            return;
        }
        dep -= debt;
    }
    u64 balance;
    if (!az_pantry_balance_after(stock, dep, taken, &balance)) {
        w->totals.field_underflow++;
        return;
    }
    c->energy = az_pantry_accept(w, c, balance, 0, true);
    e->dep_e = e->taken_e = 0;
}

#endif
