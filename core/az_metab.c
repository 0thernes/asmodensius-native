/* az_metab.c - feeding, respiration, growth, stress and starvation.
 *
 * Every transfer is an integer and balances exactly in matter and in energy.
 * The reactions are the redox cycle of the brief (chem shares it):
 *   photosynthesis  2 WST_W + 12 eu light -> 1 biomass + 1 OXI (to the cell)
 *   chemotrophy     1 NUT_X -> 1 biomass + 8 eu
 *   heterotrophy    1 ORG_Y -> 1 biomass
 *   scavenging      1 WST_W + 14 eu -> 1 biomass
 *   aerobic         1 biomass + 1 OXI -> 2 waste + 12 eu
 *   fermentation    2 biomass -> 1 waste + 1 ORG_Y (to the cell) + 14 eu
 *   detoxification  1 toxin -> 1 waste + 2 eu heat
 *   secretion       1 waste + 2 eu -> 1 TOX_Z (to the cell)
 *   repair          1 wall -> 1 waste + 14 eu heat, and health
 *
 * Size is mass^(3/4) (Kleiber 1932: metabolic rate scales with the 3/4 power
 * of mass), pace is METAB_PACE_BASE + the METAB trait. Uptake, the cost of
 * living and the rate of respiration all scale with size x pace, so a fast
 * life eats, burns, builds and pays more. The stores follow the dynamic
 * energy budget picture (Kooijman 2010): a free reserve (pool_a), structure
 * (the body's parts and the wall pool_m) and a germ store (pool_t).
 *
 * EVERY TICK, every active creature (HOT state, the cell, its own block):
 *   az_metab_demand  what it asks of its cell, by its DIET_* affinities, its
 *     size, its pace and its brain's EAT drive: light (12 eu per unit of
 *     biomass it could fix) with 2 WST_W per unit, NUT_X, ORG_Y, WST_W. On
 *     pulse ticks also OXI for the aerobic share of what it may burn.
 *   az_metab_range   takes what the cell granted, as far as the body has
 *     room (PART_HOLD per part): light as energy, booked as energy_in, and
 *     no more than its energy store has room for; biomass from NUT_X and
 *     ORG_Y; photosynthesis as far as this tick's light and the granted WST_W
 *     go; other WST_W is carried as waste until a pulse upgrades it. The
 *     cell's toxin comes with the food, at the concentration it has among
 *     the food of the cell (METAB_TOXIN_INTAKE of 256 of it). Then
 *     the cost of living (size x pace, the sense range, and outside its
 *     thermal band an extra cost and damage), paid as heat; what cannot be
 *     paid is damage. Toxin carried damages by 256 - TOXRES. Energy above
 *     what the body holds leaves as heat. mass is rewritten; death is asked
 *     for with the cause that did most this tick.
 *
 * THE PULSE (one tick in four per creature, behind az_phase_due; the trait
 * row is read only here):
 *   upkeep       of parts, brain bytes, long-term memory words, the ORNAMENT
 *                and the four tolerances: what a creature carries costs
 *   respiration  burns free biomass to refill the energy store set by
 *                STORE_CAP and to pay for the next four ticks and for
 *                building; never more than its rate, which falls as EFFIC
 *                rises (rate against yield: Pfeiffer, Schuster and Bonhoeffer
 *                2001). The share OXI_USE is aerobic and needs granted OXI;
 *                the rest ferments. The body keeps (128 + EFFIC / 2) / 256 of
 *                the energy; the rest is heat. A starving body draws the germ
 *                store, then the wall, then its parts back.
 *   scavenging   surplus energy upgrades carried waste, by DIET_W
 *   building     energy above the store builds free biomass into the wall
 *                (share GROWTH, up to METAB_WALL_SHARE) and the germ store
 *   repair       health from the wall
 *   parts        the part whose turn it is takes a share by ALLOMETRY; a full
 *                body asks for a part when the brain drives GROW, an empty
 *                last part is given up when it drives SHED
 *   detox, secretion (TOX_SECRETE), excretion (WASTE_RATE, or the brain's
 *                EXCRETE drive); waste still carried harms a little
 *   pressure     above (PRESS_TOL + 1) x METAB_PRESS_UNIT damages
 *   radiation    cell charge above RAD_TOL damages
 *
 * AZ_LESION_TOXIN: no uptake of, damage from or secretion of toxin.
 * Eggs and cysts are outside metabolism: they ask nothing and pay nothing.
 */
#include "az_abi.h"
#ifdef AZ_INTAKE_OBSERVER
#include "az_intake_hooks.h"
#endif

AZ_INLINE u32 wp(const AzWorld *w, u32 id) { return w->param[id]; }
AZ_INLINE u32 cap32(u64 v, u32 hi) { return v > hi ? hi : (u32)v; }

/* about mass^(3/4): exact at the powers of 16, on the secant of each octave
 * between (within 3 per cent of the true value; test_metab measures it) */
static u32 size34(u32 m)
{
    static const u16 root[4] = { 256, 304, 362, 431 };       /* 2^(r/4) in 1/256 */
    if (m < 2u) return m;
    const u32 b = 31u - (u32)__builtin_clz(m);               /* 2^b <= m */
    const u32 e = 3u * b;                                    /* 2^(3b/4), the exponent in quarters */
    u64 v = (u64)root[e & 3u] << (e >> 2);                   /* 256 * (2^b)^(3/4) */
    const u32 frac = (u32)(((u64)(m - (1u << b)) << 8) >> b);   /* 0..255: the way to the next power of two */
    v = (v * (256u + ((frac * 175u) >> 8))) >> 16;           /* 2^(3/4) = 1 + 175/256 at the end of the octave */
    return (u32)v;
}

/* what the pools of this body can hold: PART_HOLD for every part it has */
static u32 hold_mu(const AzWorld *w, u32 np)
{
    const u64 h = (u64)wp(w, AZ_WP_PART_HOLD) * np;
    return h > wp(w, AZ_WP_CELL_HOLD) ? wp(w, AZ_WP_CELL_HOLD) : (u32)h;
}

/* most biomass one pulse respires: four ticks' worth; a thrifty burner is a slow one */
static u32 pulse_burn(const AzWorld *w, u32 size, u32 pace, u32 effic)
{
    const u64 per_tick = 1u + (((u64)size * pace * wp(w, AZ_WP_METAB_BURN_SCALE)) >> 16);
    return cap32((per_tick * 4u * (384u - effic)) >> 8, 0x3FFFFFFFu);
}

void az_metab_demand(AzWorld *w, u32 slot, u32 worker)
{
    u32 *eat = &w->eat[(size_t)slot * AZ_NDIET];
    for (u32 d = 0; d < AZ_NDIET; d++) eat[d] = 0;
    const u8 fl = w->flags[slot];
    if (!(fl & AZ_F_ALIVE) || (fl & (AZ_F_EGG | AZ_F_DORMANT))) return;
    const u8 *ph = &w->phen[(size_t)slot * AZ_PHEN];
    const u32 c = w->cell[slot];
    const u32 size = size34(w->mass[slot]);
    const u32 pace = wp(w, AZ_WP_METAB_PACE_BASE) + ph[AZ_P_METAB];
    const u32 appetite = (u32)(128 + w->motor[(size_t)slot * AZ_MOT + AZ_M_EAT]);   /* 0..255 */
    /* per unit of affinity, in 1/2^24 matter units */
    const u64 unit = (((u64)size * pace * appetite) >> 8) * wp(w, AZ_WP_METAB_UPTAKE_SCALE);
    const u32 photo = cap32((unit * ph[AZ_P_DIET_L]) >> 24, 1u << 26);   /* biomass it could fix by light */
    const u32 nx = cap32((unit * ph[AZ_P_DIET_X]) >> 24, 1u << 26);
    const u32 ny = cap32((unit * ph[AZ_P_DIET_Y]) >> 24, 1u << 26);
    const u32 nw = cap32((unit * ph[AZ_P_DIET_W]) >> 24, 1u << 26) + 2u * photo;
    if (photo) { eat[AZ_D_LIGHT] = photo * 12u; az_cell_demand(w, worker, c, AZ_D_LIGHT, photo * 12u); }
    if (nx) { eat[AZ_D_NUT_X] = nx; az_cell_demand(w, worker, c, AZ_D_NUT_X, nx); }
    if (ny) { eat[AZ_D_ORG_Y] = ny; az_cell_demand(w, worker, c, AZ_D_ORG_Y, ny); }
    if (nw) { eat[AZ_D_WST_W] = nw; az_cell_demand(w, worker, c, AZ_D_WST_W, nw); }
    if (az_phase_due(w, w->uid[slot], AZ_RS_METAB, 2)) {      /* a pulse tick: the trait row may be read */
        const u8 *tr = &w->trait[(size_t)slot * AZ_TRAITS];
        const u64 reserve = (u64)w->pool_a[slot] + w->pool_t[slot] + w->pool_m[slot];
        const u32 burn = cap32(reserve, pulse_burn(w, size, pace, tr[AZ_T_EFFIC]));
        const u32 ox = (u32)(((u64)burn * tr[AZ_T_OXI_USE]) >> 8);
        if (ox) { eat[AZ_D_OXI] = ox; az_cell_demand(w, worker, c, AZ_D_OXI, ox); }
    }
}

/* one body while it is worked on; written back once */
typedef struct Body {
    i64 e, heat;                    /* energy store; heat given off this tick */
    u32 pa, pm, pt, ws, tx;         /* the pools */
    u32 health;
    u64 dmg[5];                     /* by cause: starved, poison, cold or heat, pressure, other */
} Body;
enum { DMG_STARVE = 0, DMG_POISON, DMG_THERMAL, DMG_PRESSURE, DMG_OTHER };

/* the pulse, part one: upkeep is added to the cost, then respiration.
 * Returns the energy store this creature's STORE_CAP stands for. */
static AZ_NOINLINE i64 pulse_respire(AzWorld *w, u32 s, u32 worker, Body *b, u64 *cost, u32 np, i64 e_hold)
{
    const u8 *tr = &w->trait[(size_t)s * AZ_TRAITS];
    const u8 *ph = &w->phen[(size_t)s * AZ_PHEN];
    const u32 c = w->cell[s];
    u32 bytes, words;
    (void)az_brain(w, s, &bytes);
    (void)az_memory(w, s, &words);
    const u32 bear = (u32)tr[AZ_T_TTOL] + tr[AZ_T_TOXRES] + tr[AZ_T_PRESS_TOL] + tr[AZ_T_RAD_TOL];
    *cost += (u64)np * wp(w, AZ_WP_METAB_PART_UPKEEP)
           + (((u64)bytes * wp(w, AZ_WP_METAB_BRAIN_UPKEEP)) >> 8)
           + (((u64)words * wp(w, AZ_WP_TIER1_WORD_EU) * wp(w, AZ_WP_METAB_WORD_UPKEEP)) >> 8)
           + (((u64)tr[AZ_T_ORNAMENT] * wp(w, AZ_WP_METAB_ORNAMENT_UPKEEP)) >> 8)
           + (((u64)bear * wp(w, AZ_WP_METAB_TOLERANCE_UPKEEP)) >> 8);

    i64 store = (i64)(tr[AZ_T_STORE_CAP] + 1u) * wp(w, AZ_WP_METAB_STORE_EU);
    if (store > e_hold) store = e_hold;
    const u32 keep = 128u + (tr[AZ_T_EFFIC] >> 1);                       /* of 256 */
    const u64 build = 1u + (((u64)b->pa * wp(w, AZ_WP_METAB_BUILD_SHARE)) >> 8);
    const i64 want = store + 4 * (i64)cap32(*cost, 1u << 28) + (i64)cap32(build * wp(w, AZ_WP_METAB_BUILD_EU), 1u << 28) - b->e;
    if (want > 0) {
        const u32 yield = (12u * keep) >> 8;                             /* eu kept per unit burned with OXI: 6..11 */
        const u32 size = size34(w->mass[s]);
        const u32 pace = wp(w, AZ_WP_METAB_PACE_BASE) + ph[AZ_P_METAB];
        u32 burn = cap32(((u64)want + yield - 1u) / yield, pulse_burn(w, size, pace, tr[AZ_T_EFFIC]));
        if (b->pa < burn) {                                              /* starvation draws the stores back */
            u32 d = az_min_u32(burn - b->pa, b->pt);
            b->pt -= d; b->pa += d;
            d = az_min_u32(burn - b->pa, b->pm);
            b->pm -= d; b->pa += d;
            if (burn > b->pa) burn = b->pa;
        }
        const u32 air_want = (u32)(((u64)burn * tr[AZ_T_OXI_USE]) >> 8);
        const u32 eat_ox = w->eat[(size_t)s * AZ_NDIET + AZ_D_OXI];
        const u32 air = eat_ox ? az_min_u32(air_want, az_cell_granted(w, c, AZ_D_OXI, eat_ox)) : 0u;
        const u32 pairs = (burn - air_want) >> 1;                        /* fermentation takes two at a time */
        const u64 released = 12ull * air + 14ull * pairs;
        const u64 kept = (released * keep) >> 8;
        b->e += (i64)kept;
        b->heat += (i64)(released - kept);
        b->pa -= air + 2u * pairs;
        b->ws += 2u * air + pairs;
        if (air)   az_cell_post(w, worker, c, AZ_SP_OXI, -(i32)air);
        if (pairs) az_cell_post(w, worker, c, AZ_SP_ORG_Y, (i32)pairs);
    }
    return store;
}

/* the pulse, part two: what the body does with what it has */
static AZ_NOINLINE void pulse_body(AzWorld *w, u32 s, u32 worker, Body *b, i64 store, AzPart *part, u32 np, u32 hold)
{
    const u8 *tr = &w->trait[(size_t)s * AZ_TRAITS];
    const u8 *ph = &w->phen[(size_t)s * AZ_PHEN];
    const i8 *mo = &w->motor[(size_t)s * AZ_MOT];
    const u32 c = w->cell[s];
    const bool toxin_on = !(w->cfg.lesion & AZ_LESION_TOXIN);

    /* scavenging: surplus energy upgrades waste it carries, by its DIET_W */
    if (b->ws && b->e > store) {
        const u32 n = cap32((u64)(b->e - store) / 14u, (u32)(((u64)b->ws * ph[AZ_P_DIET_W]) >> 8));
#ifdef AZ_INTAKE_OBSERVER
        if (n) azt_waste_removed(w, s, b->ws, n, true);
#endif
        b->ws -= n; b->pa += n; b->e -= 14 * (i64)n;
    }
    /* building: energy above the store turns free biomass into wall and germ store */
    if (b->pa && b->e > store) {
        u64 n = 1u + (((u64)b->pa * wp(w, AZ_WP_METAB_BUILD_SHARE)) >> 8);
        if (n > b->pa) n = b->pa;
        const u32 each = wp(w, AZ_WP_METAB_BUILD_EU);
        if (each && n > (u64)(b->e - store) / each) n = (u64)(b->e - store) / each;
        const u32 wall = (u32)(((u64)hold * wp(w, AZ_WP_METAB_WALL_SHARE)) >> 8);
        u32 to_m = (u32)((n * tr[AZ_T_GROWTH]) >> 8);
        if (to_m > (wall > b->pm ? wall - b->pm : 0u)) to_m = wall > b->pm ? wall - b->pm : 0u;
        b->pa -= (u32)n; b->pm += to_m; b->pt += (u32)n - to_m;
        b->e -= (i64)n * each; b->heat += (i64)n * each;
    }
    /* repair: health from the wall */
    const u32 heal = wp(w, AZ_WP_METAB_HEAL);
    if (heal && b->pm && b->health < 65535u) {
        u32 r = (65535u - b->health + heal - 1u) / heal;
        r = az_min_u32(r, az_min_u32(b->pm, wp(w, AZ_WP_METAB_HEAL_RATE)));
        b->health = az_min_u32(65535u, b->health + r * heal);
        b->pm -= r; b->ws += r;
        b->heat += (i64)r * (AZ_EPS_BIO - AZ_EPS_WASTE);
    }
    /* parts: they take turns; a share of the free pool (its own ALLOMETRY) is built into this
     * pulse's part, and a body that has run out of pool takes matter back from it */
    if (np) {
        const u32 part_hold = wp(w, AZ_WP_PART_HOLD);
        AzPart *p = &part[(u32)(((w->tick >> 2) + w->uid[s]) % np)];
        const u64 demand = ((u64)b->pa * (8u + tr[AZ_T_ALLOMETRY]) * wp(w, AZ_WP_METAB_PART_SHARE)) >> 15;
        const u32 share = cap32(demand, b->pa);
        if (share && p->matter < part_hold) {
            const u32 g = az_min_u32(share, part_hold - p->matter);
            b->pa -= g; p->matter += g;
        } else if (b->pa == 0 && p->matter) {
            const u32 size = size34(w->mass[s]);
            const u32 g = az_min_u32(p->matter, pulse_burn(w, size, wp(w, AZ_WP_METAB_PACE_BASE) + ph[AZ_P_METAB], tr[AZ_T_EFFIC]));
            p->matter -= g; b->pa += g;
        }
        bool full = true;
        for (u32 i = 0; i < np; i++) full = full && part[i].matter >= part_hold;
        if (full && mo[AZ_M_GROW] > 0) {
            az_part_request(w, s, 1u + (u32)((tr[AZ_T_SEGMENT] + np) % (AZ_PART_KINDS - 1u)));
        } else if (np > 1u && part[np - 1u].matter == 0 && mo[AZ_M_SHED] > 96) {
            az_part_shed_request(w, s, np - 1u);
        }
    }
    /* detoxification; secretion as a defence */
    if (b->tx) {
        const u32 d = az_min_u32(b->tx, 1u + (u32)(((u64)b->tx * wp(w, AZ_WP_METAB_DETOX)) >> 8));
        b->tx -= d; b->ws += d;
        b->heat += (i64)d * (AZ_EPS_TOXIN - AZ_EPS_WASTE);
    }
    if (toxin_on && tr[AZ_T_TOX_SECRETE] && b->ws && b->e > 0) {
        const u32 n = cap32((u64)b->e / (AZ_EPS_TOXIN - AZ_EPS_WASTE),
                            (u32)(((u64)b->ws * tr[AZ_T_TOX_SECRETE] * wp(w, AZ_WP_METAB_SECRETE_SHARE)) >> 16));
        if (n) {
#ifdef AZ_INTAKE_OBSERVER
            azt_waste_removed(w, s, b->ws, n, false);
#endif
            b->ws -= n; b->e -= (i64)n * (AZ_EPS_TOXIN - AZ_EPS_WASTE);
            az_cell_post(w, worker, c, AZ_SP_TOX_Z, (i32)n);
        }
    }
    /* excretion; what is still carried harms a little */
    const u32 level = ((255u - tr[AZ_T_WASTE_RATE]) * wp(w, AZ_WP_METAB_WASTE_HOLD)) >> 4;
    if (b->ws && (b->ws > level || mo[AZ_M_EXCRETE] > 0)) {
        az_cell_post(w, worker, c, AZ_SP_WST_W, (i32)b->ws);
#ifdef AZ_INTAKE_OBSERVER
        azt_waste_removed(w, s, b->ws, b->ws, false);
#endif
        b->ws = 0;
    }
    b->dmg[DMG_OTHER] += ((u64)b->ws * wp(w, AZ_WP_METAB_WASTE_DAMAGE)) >> 4;
    /* pressure and radiation */
    const u64 press = az_cell_pressure(w, c);
    const u64 bears = (u64)(tr[AZ_T_PRESS_TOL] + 1u) * wp(w, AZ_WP_METAB_PRESS_UNIT);
    if (press > bears) b->dmg[DMG_PRESSURE] += ((press - bears) * wp(w, AZ_WP_METAB_PRESS_DAMAGE)) / wp(w, AZ_WP_METAB_PRESS_UNIT);
    const u32 charge = w->c_charge[c];
    if (charge > tr[AZ_T_RAD_TOL]) b->dmg[DMG_OTHER] += (u64)(charge - tr[AZ_T_RAD_TOL]) * wp(w, AZ_WP_METAB_RAD_DAMAGE);
}

void az_metab_range(AzWorld *w, u32 begin, u32 end, u32 worker)
{
    static const u8 cause_of[5] = { AZ_DIE_STARVED, AZ_DIE_POISON, AZ_DIE_COLD_HEAT, AZ_DIE_PRESSURE, AZ_DIE_WOUNDS };
    const bool toxin_on = !(w->cfg.lesion & AZ_LESION_TOXIN);
    for (u32 s = begin; s < end; s++) {
        if (s + 4u < end) az_blk_prefetch(w, s + 4u);    /* the body parts of a creature soon to come */
        const u8 fl = w->flags[s];
        if (!(fl & AZ_F_ALIVE) || (fl & (AZ_F_EGG | AZ_F_DORMANT))) continue;
        const u32 c = w->cell[s];
        const u8 *ph = &w->phen[(size_t)s * AZ_PHEN];
        const u32 *eat = &w->eat[(size_t)s * AZ_NDIET];
        u32 np;
        AzPart *part = az_parts(w, s, &np);
        const u32 hold = hold_mu(w, np);
        const i64 e_hold = (i64)hold * AZ_EPS_BIO > 0x3FFFFFFFll ? 0x3FFFFFFFll : (i64)hold * AZ_EPS_BIO;
        Body b = { .e = w->energy[s], .heat = 0, .pa = w->pool_a[s], .pm = w->pool_m[s], .pt = w->pool_t[s],
                   .ws = w->waste[s], .tx = w->toxin[s], .health = w->health[s], .dmg = { 0, 0, 0, 0, 0 } };

        /* ---- uptake: my share of what the cell granted; what does not fit stays in the cell ---- */
        const u64 carried = (u64)b.pa + b.pm + b.pt + b.ws + b.tx;
        u32 room = carried < hold ? (u32)(hold - carried) : 0u;
        u32 light = 0, eaten = 0;                        /* eaten: units of food taken out of the cell */
#ifdef AZ_INTAKE_OBSERVER
        u32 paid_x = 0, paid_y = 0, fixed = 0, carried_w = 0;
#endif
        if (eat[AZ_D_LIGHT]) {                           /* light is energy only, and only what the store has room for */
            light = az_cell_granted(w, c, AZ_D_LIGHT, eat[AZ_D_LIGHT]);
            const i64 e_room = b.e < e_hold ? e_hold - b.e : 0;
            if ((i64)light > e_room) light = (u32)e_room;
            if (light) { b.e += light; az_tally(w, worker)->energy_in += light; }
        }
        if (eat[AZ_D_NUT_X]) {                           /* chemotrophy: 1 NUT_X (24) -> 1 biomass (16) + 8 eu */
            const u32 x = az_min_u32(az_cell_granted(w, c, AZ_D_NUT_X, eat[AZ_D_NUT_X]), room);
#ifdef AZ_INTAKE_OBSERVER
            paid_x = x;
#endif
            if (x) { b.pa += x; room -= x; eaten += x; b.e += 8 * (i64)x; az_cell_post(w, worker, c, AZ_SP_NUT_X, -(i32)x); }
        }
        if (eat[AZ_D_ORG_Y]) {                           /* heterotrophy: 1 ORG_Y (16) -> 1 biomass (16) */
            const u32 y = az_min_u32(az_cell_granted(w, c, AZ_D_ORG_Y, eat[AZ_D_ORG_Y]), room);
#ifdef AZ_INTAKE_OBSERVER
            paid_y = y;
#endif
            if (y) { b.pa += y; room -= y; eaten += y; az_cell_post(w, worker, c, AZ_SP_ORG_Y, -(i32)y); }
        }
        if (eat[AZ_D_WST_W]) {
            const u32 got = az_cell_granted(w, c, AZ_D_WST_W, eat[AZ_D_WST_W]);
            const u32 for_light = az_cell_granted(w, c, AZ_D_WST_W, eat[AZ_D_LIGHT] / 6u);   /* 2 per 12 eu asked */
            /* photosynthesis: 2 WST_W (4) + 12 eu of this tick's light -> 1 biomass (16) + 1 OXI */
            const u32 p = az_min_u32(az_min_u32(light / 12u, for_light >> 1), room);
#ifdef AZ_INTAKE_OBSERVER
            fixed = 2u * p;
#endif
            if (p) {
                b.pa += p; room -= p; eaten += 2u * p; b.e -= 12 * (i64)p;
                az_cell_post(w, worker, c, AZ_SP_WST_W, -(i32)(2u * p));
                az_cell_post(w, worker, c, AZ_SP_OXI, (i32)p);
            }
            /* the waste it asked for by DIET_W is carried until a pulse can upgrade it */
            const u32 v = az_min_u32(got > for_light ? got - for_light : 0u, room);
#ifdef AZ_INTAKE_OBSERVER
            carried_w = v;
#endif
            if (v) { b.ws += v; room -= v; eaten += v; az_cell_post(w, worker, c, AZ_SP_WST_W, -(i32)v); }
        }
#ifdef AZ_INTAKE_OBSERVER
        if (light || eaten) azt_intake_settled(w, s, light, paid_x, paid_y, fixed, carried_w);
#endif
        if (toxin_on && eaten) {
            /* The toxin comes with the food, as concentrated as it is among the food of the cell.
             * All who eat here take at most the food there is, so together they take less toxin
             * than there is: no grant is needed. */
            const u32 *here = &w->c_sp[(size_t)c * AZ_NSPECIES];
            const u64 tox = here[AZ_SP_TOX_Z];
            if (tox) {
                const u64 food = (u64)here[AZ_SP_NUT_X] + here[AZ_SP_ORG_Y] + here[AZ_SP_WST_W];
                const u32 t = az_min_u32((u32)(((u64)eaten * tox * wp(w, AZ_WP_METAB_TOXIN_INTAKE)) / ((food + tox) << 8)), room);
                if (t) { b.tx += t; az_cell_post(w, worker, c, AZ_SP_TOX_Z, -(i32)t); }
            }
        }

        /* ---- the cost of living: size x pace, the senses, the cold and the heat ---- */
        const u32 size = size34(w->mass[s]);
        const u32 pace = wp(w, AZ_WP_METAB_PACE_BASE) + ph[AZ_P_METAB];
        u64 cost = wp(w, AZ_WP_METAB_COST_MIN) + (((u64)size * pace * wp(w, AZ_WP_METAB_COST_SCALE)) >> 16)
                 + (((u64)ph[AZ_P_SENSE] * wp(w, AZ_WP_METAB_SENSE_COST)) >> 8);
        i32 off = (i32)w->c_temp[c] - AZ_TEMP_OF_TOPT(ph[AZ_P_TOPT]);
        if (off < 0) off = -off;
        off -= AZ_TEMP_TOL(ph[AZ_P_TTOL]);
        if (off > 0) {
            cost += ((u64)(u32)off * wp(w, AZ_WP_METAB_THERMAL_COST)) >> 10;
            b.dmg[DMG_THERMAL] = ((u64)(u32)off * wp(w, AZ_WP_METAB_THERMAL_DAMAGE)) >> 10;
        }
        const bool pulse = az_phase_due(w, w->uid[s], AZ_RS_METAB, 2);
        i64 store = 0;
        if (pulse) store = pulse_respire(w, s, worker, &b, &cost, np, e_hold);
        const u64 pay = b.e > 0 ? (cost < (u64)b.e ? cost : (u64)b.e) : 0u;
        b.e -= (i64)pay; b.heat += (i64)pay;
        b.dmg[DMG_STARVE] = (cost - pay) * wp(w, AZ_WP_METAB_STARVE_DAMAGE);
        if (toxin_on && b.tx)
            b.dmg[DMG_POISON] = ((u64)b.tx * (256u - ph[AZ_P_TOXRES]) * wp(w, AZ_WP_METAB_TOXIN_DAMAGE)) >> 8;
        if (pulse) pulse_body(w, s, worker, &b, store, part, np, hold);

        /* ---- damage; a body holds the energy its matter capacity is worth, the rest is heat ---- */
        u64 hurt = 0;
        u32 worst = DMG_OTHER;
        for (u32 k = 0; k < 5u; k++) {
            hurt += b.dmg[k] > 65535u ? 65535u : b.dmg[k];
            if (b.dmg[k] > b.dmg[worst]) worst = k;
        }
        b.health = hurt >= b.health ? 0u : b.health - (u32)hurt;
        if (b.e > e_hold) {
            b.heat += b.e - e_hold;
            b.e = e_hold;
            az_tally(w, worker)->sat[AZ_SAT_ENERGY]++;
        }
        while (b.heat > 0x40000000ll) { az_cell_post_heat(w, worker, c, 0x40000000); b.heat -= 0x40000000ll; }
        if (b.heat) az_cell_post_heat(w, worker, c, (i32)b.heat);

        u64 built = 0;
        for (u32 i = 0; i < np; i++) built += part[i].matter;
        w->energy[s] = (i32)b.e;
        w->pool_a[s] = b.pa; w->pool_m[s] = b.pm; w->pool_t[s] = b.pt;
        w->waste[s] = b.ws; w->toxin[s] = b.tx;
        w->health[s] = (u16)b.health;
        w->mass[s] = cap32((u64)b.pa + b.pm + b.pt + b.ws + b.tx + built, 0xFFFFFFFFu);
        if (b.health == 0) az_request_death(w, s, cause_of[worst]);
        else if (b.pa == 0 && b.pm == 0 && b.pt == 0 && built == 0 && b.e <= 0) az_request_death(w, s, AZ_DIE_STARVED);
    }
}
