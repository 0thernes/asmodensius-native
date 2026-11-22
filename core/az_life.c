/* az_life.c - life history: stages, broods, births and deaths.
 *
 * A creature's stage is its flags plus birth_tick (az_abi.h, "Life stages").
 * This module is the only writer of ALIVE, EGG, JUV, SENESC, DORMANT and
 * RECEPT, the only caller of az_birth_request, and the one place where a
 * creature dies and where its matter and energy go back to the world.
 *
 * Nothing here is a fixed size or a fixed number of the biology. Every
 * duration, level and rate is the creature's own trait counted in a unit
 * that is a world parameter (core/params/az_life.def); a creature that lacks
 * the gene behind a trait reads 0 for it and still lives by the same rules
 * (no egg time: born live; no clutch gene: one child; no lifespan gene: old
 * at once).
 *
 * Every tick, HOT state only (flags, the request byte, motors, phen):
 *   - a requested death is carried out (az_life_kill);
 *   - a requested waking (AZ_RQ_HATCH) hatches an egg or opens a cyst;
 *   - a ripe adult of a budding kind whose brain drives AZ_M_REPRO buds.
 * Once in 2^LIFE_CHECK_LOG2 ticks, at a phase of its own (az_phase_due; the
 * trait row may be read there), one step of its life history:
 *   age     ticks since birth; a founder adds an age of its own, a fixed
 *           function of its uid and the world's key, of up to
 *           LIFE_FOUNDER_SPREAD / 256 of its time to senescence
 *   hazard  LIFE_HAZARD in 2^20 per check, at every stage (cause HAZARD)
 *   egg     hatches when age >= EGG_TIME * LIFE_EGG_SCALE / 16
 *   cyst    wakes when its cell holds LIFE_WAKE_FOOD of organic matter, or
 *           by a draw of LIFE_WAKE_CHANCE * (384 - DORMANCY) / 256 in 65536
 *   young   becomes adult when age >= MATURITY * LIFE_MATURITY_SCALE / 16
 *           and its body matter >= SIZE * LIFE_SIZE_MU / 16
 *   adult   is ripe (RECEPT) while energy >= REPRO_THRESH * LIFE_REPRO_EU /
 *           16 and its free biomass (pool_a) >= the same size; becomes
 *           senescent when age >= LIFESPAN * LIFE_SPAN_SCALE / 16
 *   old     dies of age with chance LIFE_SENESC_HAZARD * 2^(t / LIFE_SENESC_
 *           DOUBLING) in 2^20 per check, t = ticks since senescence began
 *           (Gompertz 1825: the fraction of a doubling is linear)
 *   rest    a creature whose brain drives AZ_M_REST and whose energy is
 *           below DORMANCY * LIFE_DORMANT_EU / 16 encysts with chance
 *           DORMANCY * drive * 2 in 65536 (drive 1..127)
 *   self    a ripe hermaphrodite whose brain drives AZ_M_REPRO selfs: in
 *           mode SELF always, in mode HERMA with chance SELFING / 256
 * At most one stage changes per check, with one event (HATCH or STAGE).
 *
 * Broods (az_life_brood; Smith and Fretwell 1974: more young means less
 * each): 1 + CLUTCH * LIFE_CLUTCH_SCALE / 256 young share (PROVISION + 1) /
 * 512 of the mother's free stores: pool_t first, then pool_a, and the same
 * share of her energy. No child without at least 1 mu: a poor mother has
 * fewer. Nothing caps a brood but her matter, the slots and the store.
 *
 * Births (az_life_commit, serial, mothers in uid order): each child gets its
 * block first (az_conceive: genome, brain, body; refused when the store is
 * full, then the rest of the provision goes back to the mother's cell). It is
 * placed within (DISPERSAL + 1) * LIFE_DISPERSAL_SCALE / 256 cells of her,
 * never in bedrock; its own EGG_TIME makes it an egg or a live young; its
 * own DEV_PLAST lets the place of birth bend its body: each part but the
 * first changes kind with chance DEV_PLAST / 256 to a kind drawn by what the
 * cell holds (more gut where food is plenty, more store where it is scarce,
 * more sensor in the dark, more armour in poison, more fin in a moving
 * medium).
 */
#include "az_abi.h"

#define STAGE_BITS (AZ_F_EGG | AZ_F_JUV | AZ_F_SENESC | AZ_F_DORMANT)

AZ_INLINE u32 wp(const AzWorld *w, u32 id) { return w->param[id]; }

/* a trait byte counted in a world unit per 16 */
static u32 scaled(u32 trait, u32 scale)
{
    const u64 v = ((u64)trait * scale) >> 4;
    return v > 0xFFFFFFFFull ? 0xFFFFFFFFu : (u32)v;
}

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

u32 az_life_brood(AzWorld *w, u32 mother, u32 father, u32 worker)
{
    (void)worker;
    const u8 fl = w->flags[mother];
    if (!(fl & AZ_F_ALIVE) || !(fl & AZ_F_RECEPT) || (fl & (AZ_F_EGG | AZ_F_JUV | AZ_F_DORMANT))) return 0;
    if ((w->pair[mother].req & AZ_RQ_DIE) || w->birth[mother].n) return 0;
    const u8 *tr = &w->trait[(size_t)mother * AZ_TRAITS];

    u64 n = 1u + (((u64)tr[AZ_T_CLUTCH] * wp(w, AZ_WP_LIFE_CLUTCH_SCALE)) >> 8);
    const u64 share = (u64)tr[AZ_T_PROVISION] + 1u;                  /* of 512: she keeps at least half */
    const u64 give = (((u64)w->pool_t[mother] + w->pool_a[mother]) * share) >> 9;
    const i32 e = w->energy[mother] > 0 ? (i32)(((u64)(u32)w->energy[mother] * share) >> 9) : 0;
    if (give < n) n = give;                                          /* no child without matter of its own */
    if (n == 0) return 0;
    const u32 from_t = (u32)(give < w->pool_t[mother] ? give : w->pool_t[mother]);
    const u32 from_a = (u32)(give - from_t);
    if (!az_birth_request(w, mother, father, (u32)n, e, from_a, 0, from_t)) return 0;
    w->pool_t[mother] -= from_t;
    w->pool_a[mother] -= from_a;
    w->energy[mother] -= e;
    w->flags[mother] = (u8)(fl & ~AZ_F_RECEPT);
    az_stat_bump(w, mother, AZ_ST_OFFSPRING, (u32)n);
    return (u32)n;
}

/* chance in 2^20 per check of dying of age, `past` ticks into senescence */
static u32 age_hazard(const AzWorld *w, u64 past)
{
    const u32 dbl = wp(w, AZ_WP_LIFE_SENESC_DOUBLING);
    const u64 k = past / dbl, f = past % dbl;
    u64 h = wp(w, AZ_WP_LIFE_SENESC_HAZARD);
    if (k >= 21u) return h ? 1u << 20 : 0u;
    h <<= k;
    h += (h * f) / dbl;
    return h > (1u << 20) ? 1u << 20 : (u32)h;
}

static void stage_event(AzWorld *w, u32 s, u32 worker, u32 kind, u8 fl)
{
    w->flags[s] = fl;
    az_emit(w, worker, kind, kind == AZ_EV_STAGE ? (u32)(fl & STAGE_BITS) : 0u, w->uid[s], 0, 0);
}

/* an egg hatches into a juvenile, a cyst opens; true if something woke */
static bool wake(AzWorld *w, u32 s, u32 worker)
{
    const u8 fl = w->flags[s];
    if (fl & AZ_F_EGG) { stage_event(w, s, worker, AZ_EV_HATCH, (u8)((fl & ~AZ_F_EGG) | AZ_F_JUV)); return true; }
    if (fl & AZ_F_DORMANT) { stage_event(w, s, worker, AZ_EV_STAGE, (u8)(fl & ~AZ_F_DORMANT)); return true; }
    return false;
}

/* one step of the life history (phased; the trait row is read here) */
static void life_check(AzWorld *w, u32 s, u32 worker)
{
    const u8 *tr = &w->trait[(size_t)s * AZ_TRAITS];
    const u64 uid = w->uid[s];
    u8 fl = w->flags[s];
    const u32 onset = scaled(tr[AZ_T_LIFESPAN], wp(w, AZ_WP_LIFE_SPAN_SCALE));
    u64 age = w->tick - w->birth_tick[s];
    if (w->parent_a[s] == 0) {
        /* a founder has an age of its own from the start: founders are not one cohort */
        const u64 spread = ((u64)onset * wp(w, AZ_WP_LIFE_FOUNDER_SPREAD)) >> 8;
        if (spread) age += az_mix64(uid ^ ((u64)w->key[1] << 32 | w->key[0])) % spread;
    }
    const u32 r0 = az_rand(w, uid, AZ_RS_LIFE, 0);
    const u32 r1 = az_rand(w, uid, AZ_RS_LIFE, 1);

    if ((r0 & 0xFFFFFu) < wp(w, AZ_WP_LIFE_HAZARD)) { az_request_death(w, s, AZ_DIE_HAZARD); return; }

    if (fl & AZ_F_EGG) {
        if (age >= scaled(tr[AZ_T_EGG_TIME], wp(w, AZ_WP_LIFE_EGG_SCALE))) (void)wake(w, s, worker);
        return;
    }
    if (fl & AZ_F_DORMANT) {
        const u32 food = w->c_sp[(size_t)w->cell[s] * AZ_NSPECIES + AZ_SP_ORG_Y];
        const u64 chance = ((u64)wp(w, AZ_WP_LIFE_WAKE_CHANCE) * (384u - tr[AZ_T_DORMANCY])) >> 8;
        if (food >= wp(w, AZ_WP_LIFE_WAKE_FOOD) || (r0 >> 16) < chance) (void)wake(w, s, worker);
        return;
    }

    const u32 size_need = scaled(tr[AZ_T_SIZE], wp(w, AZ_WP_LIFE_SIZE_MU));
    const i8 *mo = &w->motor[(size_t)s * AZ_MOT];
    bool changed = false;

    if (fl & AZ_F_JUV) {
        if (age >= scaled(tr[AZ_T_MATURITY], wp(w, AZ_WP_LIFE_MATURITY_SCALE)) && w->mass[s] >= size_need) {
            fl = (u8)(fl & ~AZ_F_JUV);
            stage_event(w, s, worker, AZ_EV_STAGE, fl);
            changed = true;
        }
    } else {
        if (!(fl & AZ_F_SENESC) && age >= onset) {
            fl |= AZ_F_SENESC;
            stage_event(w, s, worker, AZ_EV_STAGE, fl);
            changed = true;
        }
        if (fl & AZ_F_SENESC) {
            const u64 past = age >= onset ? age - onset : 0u;
            if ((r1 & 0xFFFFFu) < age_hazard(w, past)) { az_request_death(w, s, AZ_DIE_AGE); return; }
        }
        /* ripe or not: her own threshold against her own stores */
        const bool ripe = w->energy[s] >= 0 &&
                          (u32)w->energy[s] >= scaled(tr[AZ_T_REPRO_THRESH], wp(w, AZ_WP_LIFE_REPRO_EU)) &&
                          w->pool_a[s] >= size_need;
        fl = ripe ? (u8)(fl | AZ_F_RECEPT) : (u8)(fl & ~AZ_F_RECEPT);
        w->flags[s] = fl;
    }

    /* rest: a creature low on energy whose brain asks for it becomes a cyst */
    if (!changed && mo[AZ_M_REST] > 0 &&
        (w->energy[s] < 0 || (u32)w->energy[s] < scaled(tr[AZ_T_DORMANCY], wp(w, AZ_WP_LIFE_DORMANT_EU)))) {
        if (((r0 >> 4) & 0xFFFFu) < (u32)tr[AZ_T_DORMANCY] * (u32)mo[AZ_M_REST] * 2u) {
            stage_event(w, s, worker, AZ_EV_STAGE, (u8)((fl | AZ_F_DORMANT) & ~AZ_F_RECEPT));
            return;
        }
    }

    /* a hermaphrodite alone: selfing (az_life_brood clears RECEPT) */
    if ((fl & AZ_F_RECEPT) && mo[AZ_M_REPRO] > 0) {
        const u32 mode = w->phen[(size_t)s * AZ_PHEN + AZ_P_MATE] & 7u;
        if (mode == AZ_MM_SELF || (mode == AZ_MM_HERMA && (r1 >> 24) < tr[AZ_T_SELFING]))
            (void)az_life_brood(w, s, s, worker);
    }
}

void az_life_range(AzWorld *w, u32 begin, u32 end, u32 worker)
{
    const u32 log2p = wp(w, AZ_WP_LIFE_CHECK_LOG2);
    for (u32 s = begin; s < end; s++) {
        u8 fl = w->flags[s];
        if (!(fl & AZ_F_ALIVE)) continue;
        const u8 req = w->pair[s].req;
        if (req & AZ_RQ_DIE) { az_life_kill(w, s, worker); continue; }
        bool woke = false;
        if (req & AZ_RQ_HATCH) { woke = wake(w, s, worker); fl = w->flags[s]; }
        if ((fl & AZ_F_RECEPT) && !(fl & (AZ_F_EGG | AZ_F_DORMANT)) &&
            w->motor[(size_t)s * AZ_MOT + AZ_M_REPRO] > 0 &&
            (w->phen[(size_t)s * AZ_PHEN + AZ_P_MATE] & 7u) == AZ_MM_BUD)
            (void)az_life_brood(w, s, AZ_NONE, worker);
        if (!woke && az_phase_due(w, w->uid[s], AZ_RS_LIFE, log2p)) life_check(w, s, worker);
        if (w->pair[s].req & AZ_RQ_DIE) az_life_kill(w, s, worker);
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

static u32 log2_u32(u32 v)
{
    u32 l = 0;
    while (v > 1u) { v >>= 1; l++; }
    return l;
}

/* the place of birth bends the body: see the header */
static void bend_body(AzWorld *w, u32 ch)
{
    const u32 plast = w->trait[(size_t)ch * AZ_TRAITS + AZ_T_DEV_PLAST];
    u32 np;
    AzPart *part = az_parts(w, ch, &np);
    if (!plast || np < 2u) return;
    const u32 c = w->cell[ch];
    const u32 food = log2_u32(w->c_sp[(size_t)c * AZ_NSPECIES + AZ_SP_ORG_Y]);     /* 0..31 */
    const u32 tox = log2_u32(w->c_sp[(size_t)c * AZ_NSPECIES + AZ_SP_TOX_Z]);
    const i8 *wind = &w->c_wind[(size_t)c * 3];
    u32 flow = 0;
    for (u32 k = 0; k < 3u; k++) flow += (u32)(wind[k] < 0 ? -wind[k] : wind[k]);
    u32 wgt[AZ_PART_KINDS];
    for (u32 k = 0; k < AZ_PART_KINDS; k++) wgt[k] = 1u;
    wgt[AZ_PART_CORE] = 0;
    wgt[AZ_PART_GUT] += food;
    wgt[AZ_PART_STORE] += 31u - food;
    wgt[AZ_PART_SENSOR] += (255u - w->c_light[c]) >> 3;
    wgt[AZ_PART_ARMOR] += tox;
    wgt[AZ_PART_FIN] += flow >> 3;
    u32 total = 0;
    for (u32 k = 0; k < AZ_PART_KINDS; k++) total += wgt[k];
    for (u32 i = 1; i < np; i++) {
        const u32 r = az_rand(w, w->uid[ch], AZ_RS_BIRTH, 8u + i);
        if ((r & 255u) >= plast) continue;
        u32 pick = (r >> 8) % total, k = 0;
        while (pick >= wgt[k]) { pick -= wgt[k]; k++; }
        part[i].kind = (u8)k;
    }
}

/* Draw a signed displacement about the mother. One-word intervals use the
 * historical lane for that axis. Wider intervals use a separate Philox
 * counter per axis. Reject the incomplete residue cycle of the 64-bit range
 * so equal-sized regions on either side of the mother have equal probability. */
static i64 birth_offset(const AzWorld *w, u64 uid, u32 axis, u32 low, u64 width)
{
    u64 value = low;
    if (width > UINT32_MAX) {
        const u64 first_accepted = (0ull - width) % width;
        u64 counter = 1u + axis;
        u32 extra[4];
        az_rand4(w, uid, AZ_RS_BIRTH, counter, extra);
        value |= (u64)extra[0] << 32;
        while (value < first_accepted) {
            /* Birth counters 1..3 in subsequent high words are reserved for
             * this axis. Other axes and per-part development keep their draws. */
            counter += 1ull << 32;
            az_rand4(w, uid, AZ_RS_BIRTH, counter, extra);
            value = ((u64)extra[0] << 32) | extra[1];
        }
    }
    return (i64)(value % width) - (i64)(width / 2u);
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
        /* how far her young are placed: in 1/256 of a cell */
        const u64 reach = ((u64)w->trait[(size_t)m * AZ_TRAITS + AZ_T_DISPERSAL] + 1u) *
                          wp(w, AZ_WP_LIFE_DISPERSAL_SCALE);
        for (u32 j = 0; j < kids && !all_rock; j++) {
            /* child j gets an equal share; the last child also gets the remainders */
            u32 left = kids - j;
            i32 e_share = (i32)((i64)b.energy / left);
            u32 a_share = b.prov[0] / left, m_share = b.prov[1] / left, t_share = b.prov[2] / left;
            if (!a_share && !m_share && !t_share) {
                /* three small pools can each round down to nothing: the child still
                 * gets its 1 mu (the brood holds at least 1 mu per child) */
                if (b.prov[0]) a_share = 1; else if (b.prov[2]) t_share = 1; else if (b.prov[1]) m_share = 1;
            }
            const u64 uid = w->uid_next;
            u32 rr[4];
            az_rand4(w, uid, AZ_RS_BIRTH, 0, rr);
            /* a box of side 2 * reach / 256 cells around her, at least one position unit */
            const u64 jx = (((u64)1 << w->shift_x) * reach >> 7) | 1u;
            const u64 jy = (((u64)1 << w->shift_y) * reach >> 7) | 1u;
            const u64 jz = (((u64)1 << w->shift_z) * reach >> 7) | 1u;
            u32 px = w->pos_x[m] + (u32)birth_offset(w, uid, 0, rr[0], jx);
            u32 py = w->pos_y[m] + (u32)birth_offset(w, uid, 1, rr[1], jy);
            i64 z = (i64)w->pos_z[m] + birth_offset(w, uid, 2, rr[2], jz);
            u32 pz = (u32)az_clamp_i64(z, 1 << 16, 0xFFFFFFFFll - (1 << 16));
            const u32 cell = az_rock_birth_place(w, m, &px, &py, &pz);
            if (cell == AZ_NONE) {
                all_rock = true;
                az_emit(w, 0, AZ_EV_REFUSED, AZ_REF_TERRAIN, w->uid[m], 0, 0);
                break;                         /* unused brood reserves are refunded below */
            }
            u32 ch = az_spawn(w);
            if (ch == AZ_NONE) break;          /* no slot: the rest is refunded below */
            /* the child's block (genome, brain, body) comes first: without room in
             * the creature store there is no child, and the slot stays dead */
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
            w->health[ch] = (u16)wp(w, AZ_WP_LIFE_BIRTH_HEALTH);
            const bool egg = scaled(w->trait[(size_t)ch * AZ_TRAITS + AZ_T_EGG_TIME], wp(w, AZ_WP_LIFE_EGG_SCALE)) > 0;
            w->flags[ch] = (u8)(AZ_F_ALIVE | (egg ? AZ_F_EGG : AZ_F_JUV));
            az_clade_assign(w, ch, m);       /* the mother's lineage, or a new one */
            w->colony[ch] = w->colony[m];
            w->parent_a[ch] = w->uid[m];
            w->parent_b[ch] = (b.father != AZ_NONE && b.father != m) ? w->uid[b.father] : 0;
            w->birth_tick[ch] = w->tick;
            w->generation[ch] = w->generation[m] + 1u;
            w->lineage[ch] = w->lineage[m];
            bend_body(w, ch);
            w->totals.births++;
            az_emit(w, 0, AZ_EV_BIRTH, egg ? 1u : 0u, uid, w->uid[m], 0);
        }
        /* whatever was not handed to a child goes back to the mother's cell as
         * organic matter and heat (the mother may have died this tick) */
        for (u32 pool = 0; pool < 3; ++pool)
            az_cell_deposit(w, 0, w->cell[m], AZ_SP_ORG_Y, b.prov[pool]);
        if (b.energy > 0) az_cell_post_heat(w, 0, w->cell[m], b.energy);
    }
}
