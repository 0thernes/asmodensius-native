/* az_colony.c - the colony as an organism. Parameters: params/az_colony.def.
 *
 * A colony is a record one level above the creature (AzColony): pools of
 * matter and energy of its own inside the exact balance, a centre and a
 * velocity, members counted by role, a call for every role, an alarm, a mood,
 * a culture and a memory. No field of a creature limits how many members it
 * has, and no number here says how many of them do what.
 *
 * THE STORES (az_colony_grant, az_colony_commit: every tick, serial). Demand
 * becomes a granted fraction, deposits and withdrawals are applied; a store
 * never gives what it does not hold. The energy pool and the four voice
 * channels are the engine's books (az_tick.c).
 *
 * THE PASS (az_colony_tier2: serial, every cfg.tier2_period ticks; not under
 * AZ_LESION_COLONY). Everything it computes is a sum, a count or a draw keyed
 * by a uid, so the result is the same for every slot order and worker count.
 *  1. Each creature for itself. A membership of a colony that no longer
 *     exists is cleared. An adult member leaves with a chance of
 *     (255 - COLONIALITY) x (255 - the colony's mood) / 256 x COLONY_LEAVE in
 *     65,536: staying is its own trait against how the colony fares. A
 *     solitary adult that holds twice COLONY_ENDOW of energy founds a colony
 *     with a chance of COLONIALITY x COLONY_FOUND in 65,536.
 *  2. Founding, in uid order (az_colony_found). The founder is the first
 *     member and gives COLONY_ENDOW of its energy to the pool; the colony
 *     starts at the founder's place, with its tag and with its context as the
 *     first culture. The lowest free record of the table is taken, a record
 *     dissolved in this pass not before the next. A full table is an event:
 *     totals.colony_denied, and AZ_EV_REFUSED the first time.
 *  3. Members are grouped by colony (a counting sort into w->tmp_u32).
 *  4. A colony without a living member dissolves: its stores go to the cell
 *     under its centre as the species they are, its energy pool as heat, to
 *     the last unit (AZ_EV_COLONY with v = 0), and the record is free again.
 *  5. A living colony is measured from its members:
 *     - centre and velocity: the mean place (offsets from the last centre, so
 *       the wrap of x and y does not tear it) and the mean velocity;
 *     - alarm: the mean of what its members stand in (the alarm mark of their
 *       cell, laid by losers of contests) or carry (their wounds), smoothed;
 *     - mood: growth or loss of members since the last pass, less half the
 *       alarm, smoothed. Members read it when they decide to stay (step 1);
 *     - culture: moves toward the mean context of the members (the Tier 1
 *       ctx vector) and toward the nearest culture the colony remembers.
 *       az_tier1.c pulls a member's context to it by its CONFORMITY;
 *     - memory: four remembered cultures; the present one is written over the
 *       oldest whenever the mood has risen. A colony keeps what it was like
 *       when things went well, and drifts back to it (tradition);
 *     - need[role], the call for a role: the stimulus of the response-
 *       threshold model. It grows with the role's demand and is worked down
 *       by the share of members holding the role:
 *           need += demand x COLONY_NEED_GAIN / 256
 *                   - (holders / members) x COLONY_NEED_WORK
 *       The demands (0..255) are measured, not set: FORAGER by how far the
 *       provisions per member (energy pool plus stores at their chemical
 *       energy) are under COLONY_EASE_EU; SOLDIER by the alarm; NURSE by the
 *       share of eggs and young; BUILDER by what the nest mark of the centre
 *       cell lacks; SCOUT by how closely the members keep to the centre
 *       (against COLONY_SPREAD cells); BREEDER by the breeders missing of one
 *       per COLONY_BREEDER_SPAN adults; TRADER by how lopsided the two
 *       usable stores are (doublings, as az_econ.c counts them).
 *       need[AZ_ROLE_NONE] is the loudest call: what an idle member hears.
 *  6. Roles (Bonabeau, Theraulaz and Deneubourg 1996, Proc. R. Soc. B 263,
 *     the fixed response-threshold model; Theraulaz, Bonabeau and Deneubourg
 *     1998 for thresholds that differ between individuals). A member with a
 *     role gives it up with a chance of COLONY_QUIT in 256. An idle member
 *     meets the roles in an order that starts at a draw and runs either way
 *     round the ring (so no role is met before its neighbour more often than
 *     after it) and takes the first one that passes its own test, with
 *     probability
 *           T = s^2 / (s^2 + theta^2)
 *     where s is the call for the role and theta the member's threshold for
 *     it: COLONY_THETA_MIN for the role its CASTE trait points at on the ring
 *     of roles, up to COLONY_THETA_SPAN more for the role farthest from it.
 *     Thresholds are heritable (CASTE) and graded, calls are measured and
 *     graded, so who does what is an outcome: no table assigns it. Eggs and
 *     the dormant keep what they have; the young do not take BREEDER.
 *     caste[] counts the members by role (caste[0]: without one).
 *
 * Who reads what this writes: az_mate.c (under the eusocial mating kind only
 * a BREEDER breeds inside a colony), az_tier1.c (culture), the senses
 * (AZ_S_COLONY_NEED: the call for the member's own role), the public record
 * (role, colony), az_social.c (recruiting into the colony, territory).
 *
 * Decisions, each with its reason:
 *  - Founding costs the founder energy and gives the colony a pool from its
 *    first tick: a colony is an organism with its own energy, and a rule that
 *    cost nothing would found one for every creature with the trait.
 *  - One trait byte gives seven thresholds through a ring: the roles are an
 *    alphabet of this world (az_abi.h), and a lean on a ring makes the two
 *    neighbours of a role the next most likely ones, without a table.
 *  - The centre is a mean of offsets from the last centre and not of
 *    positions, because x and y wrap. Distances for the SCOUT demand are
 *    taken from the last centre as well (one walk over the members).
 *  - A remembered culture of eight zero bytes means "nothing remembered".
 *  - Tier 2 is slow by design (cfg.tier2_period, 256 ticks by default): a
 *    child or a recruit has no role until the next pass.
 *
 * Lesion COLONY: the tick does not call the pass, so no colony is founded,
 * left, measured or dissolved, and no role changes; az_social.c stops
 * recruiting. The stores keep their books.
 */
#include "az_abi.h"

#define ROLES   (AZ_ROLE_COUNT - 1u)            /* the roles a member can hold; AZ_ROLE_NONE is none of them */
#define RING    (ROLES * 256u)                  /* the ring the CASTE trait points into */

AZ_INLINE u32 wp(const AzWorld *w, u32 id) { return w->param[id]; }
AZ_INLINE const u8 *trow(const AzWorld *w, u32 s) { return &w->trait[(size_t)s * AZ_TRAITS]; }
AZ_INLINE u32 absd(i64 v) { return (u32)(v < 0 ? -v : v); }

/* ---- the stores (every tick) --------------------------------------------- */
#include "az_colony_store.h"

void az_colony_grant(AzWorld *w)
{
    az_grant_colony_stores(w);
}

void az_colony_commit(AzWorld *w)
{
    az_commit_colony_stores(w);
}

/* ---- founding ------------------------------------------------------------- */
/* the lowest free record at or after *cursor; the caller's cursor moves on */
static u32 found_from(AzWorld *w, u32 slot, u32 *cursor)
{
    if (slot >= w->n_slots || !(w->flags[slot] & AZ_F_ALIVE) || w->colony[slot]) return 0;
    u32 id = *cursor;
    while (id < w->n_colonies && w->colonies[id].alive) id++;
    *cursor = id;
    if (id >= w->n_colonies) {
        if (w->n_colonies >= w->cfg.max_colonies) {                /* the table is memory: an event, not a crash */
            if (w->totals.colony_denied++ == 0) az_emit(w, 0, AZ_EV_REFUSED, AZ_REF_COLONY, w->uid[slot], 0, 0);
            return 0;
        }
        id = w->n_colonies++;
        az_memset(&w->colonies[id], 0, sizeof(AzColony));
    }
    *cursor = id + 1u;
    AzColony *k = &w->colonies[id];
    /* a free record holds nothing but, at most, what a commit could not apply
     * when it dissolved: that stays in the books of whoever founds here */
    const i64 left_e = k->energy;
    u32 left[AZ_NINV];
    for (u32 i = 0; i < AZ_NINV; i++) left[i] = k->store[i];
    az_memset(k, 0, sizeof *k);
    for (u32 i = 0; i < AZ_NINV; i++) k->store[i] = left[i];
    k->founder_uid = w->uid[slot];
    k->born_tick = w->tick;
    k->cx = w->pos_x[slot]; k->cy = w->pos_y[slot]; k->cz = w->pos_z[slot];
    k->tag = w->tag[slot];
    k->mood = 128;
    for (u32 j = 0; j < AZ_ROLE_COUNT; j++) k->need[j] = (u8)wp(w, AZ_WP_COLONY_NEED_START);
    const i8 *ctx = &w->ctx[(size_t)slot * 8u];
    for (u32 l = 0; l < 8u; l++) k->culture[l] = (u8)(128 + ctx[l]);
    /* the founder's gift: the first energy of the colony's own pool */
    const i64 have = w->energy[slot] > 0 ? w->energy[slot] / 2 : 0;
    const i64 gift = az_clamp_i64(wp(w, AZ_WP_COLONY_ENDOW), 0, have);
    w->energy[slot] -= (i32)gift;
    /* A founder may carry 16 eu per unit of body capacity. Its colony still
     * holds at most CELL_HOLD eu, including at the end of this Tier 2 pass. */
    k->energy = left_e < -gift ? left_e + gift :
                (i64)az_pantry_accept(w, k, (u64)left_e + (u64)gift, 0, true);
    k->alive = 1;
    w->colony[slot] = id;
    w->role[slot] = AZ_ROLE_NONE;
    az_emit(w, 0, AZ_EV_COLONY, id, k->founder_uid, 0, 1);
    return id;
}

u32 az_colony_found(AzWorld *w, u32 founder_slot)
{
    u32 cursor = 1;
    return found_from(w, founder_slot, &cursor);
}

/* ---- dissolution ----------------------------------------------------------- */
static void dissolve(AzWorld *w, u32 id)
{
    AzColony *k = &w->colonies[id];
    const u32 cell = az_colony_ground(w, k->cx, k->cy, k->cz);
    for (u32 i = 0; i < AZ_NINV; i++) az_colony_spill(w, cell, az_inv_species[i], false, k->store[i]);
    const i64 debt = k->energy < 0 ? k->energy : 0;     /* a pool never goes under 0; if it ever did, the books keep it */
    if (k->energy > 0) az_colony_spill(w, cell, 0, true, (u64)k->energy);
    az_emit(w, 0, AZ_EV_COLONY, id, k->founder_uid, 0, 0);
    az_memset(k, 0, sizeof *k);
    k->energy = debt;
}

/* ---- one living colony ----------------------------------------------------- */
static void measure(AzWorld *w, u32 id, const u32 *list, u32 n, u32 before)
{
    AzColony *k = &w->colonies[id];
    i64 sx = 0, sy = 0, sz = 0, svx = 0, svy = 0, svz = 0, sctx[8] = { 0 };
    u64 salarm = 0, sdist = 0;
    u32 cnt[AZ_ROLE_COUNT] = { 0 }, young = 0, adults = 0;

    for (u32 i = 0; i < n; i++) {
        const u32 s = list[i];
        const i32 dx = (i32)(w->pos_x[s] - k->cx), dy = (i32)(w->pos_y[s] - k->cy);
        const i64 dz = (i64)w->pos_z[s] - (i64)k->cz;
        sx += dx; sy += dy; sz += dz;
        sdist += (((u64)absd(dx) << 8) >> w->shift_x) + (((u64)absd(dy) << 8) >> w->shift_y)
               + (((u64)absd(dz) << 8) >> w->shift_z);
        svx += w->vel_x[s]; svy += w->vel_y[s]; svz += w->vel_z[s];
        cnt[w->role[s] < AZ_ROLE_COUNT ? w->role[s] : 0u]++;
        if (w->flags[s] & (AZ_F_EGG | AZ_F_JUV)) young++; else adults++;
        const u32 mark = w->c_mark[(size_t)w->cell[s] * AZ_NMARK + AZ_MK_ALARM];
        const u32 hurt = (65535u - w->health[s]) >> 8;
        salarm += mark > hurt ? mark : hurt;
        const i8 *ctx = &w->ctx[(size_t)s * 8u];
        for (u32 l = 0; l < 8u; l++) sctx[l] += ctx[l];
    }
    const i64 nn = (i64)n;

    /* where it is and how it moves */
    k->cx += (u32)(i32)(sx / nn);
    k->cy += (u32)(i32)(sy / nn);
    k->cz = (u32)az_clamp_i64((i64)k->cz + sz / nn, 1 << 16, 0xFFFFFFFFll - (1 << 16));
    k->vx = (i16)(svx / nn); k->vy = (i16)(svy / nn); k->vz = (i16)(svz / nn);
    k->members = n;

    /* alarm and mood */
    const i32 alarm_now = (i32)(salarm / n);
    k->alarm = (u8)((i32)k->alarm + ((alarm_now - (i32)k->alarm) * (i32)wp(w, AZ_WP_COLONY_ALARM_RATE)) / 256);
    i32 mood_now = 128;
    if (before) mood_now += (i32)az_clamp_i64(((nn - (i64)before) * 128) / (i64)before, -128, 127);
    mood_now = az_clamp_i32(mood_now - (i32)k->alarm / 2, 0, 255);
    const u8 mood_before = k->mood;
    k->mood = (u8)((i32)k->mood + ((mood_now - (i32)k->mood) * (i32)wp(w, AZ_WP_COLONY_MOOD_RATE)) / 256);

    /* culture, tradition and memory */
    u32 best = 4, best_d = 0xFFFFFFFFu;
    i32 now[8];
    for (u32 l = 0; l < 8u; l++) now[l] = 128 + (i32)(sctx[l] / nn);
    for (u32 p = 0; p < 4u; p++) {
        const u8 *m = &k->memory[p * 8u];
        u32 any = 0, d = 0;
        for (u32 l = 0; l < 8u; l++) { any |= m[l]; d += absd((i64)m[l] - now[l]); }
        if (any && d < best_d) { best_d = d; best = p; }
    }
    for (u32 l = 0; l < 8u; l++) {
        i32 v = k->culture[l];
        v += ((now[l] - v) * (i32)wp(w, AZ_WP_COLONY_CULTURE_RATE)) / 256;
        if (best < 4u) v += (((i32)k->memory[best * 8u + l] - v) * (i32)wp(w, AZ_WP_COLONY_TRADITION)) / 256;
        k->culture[l] = (u8)az_clamp_i32(v, 0, 255);
    }
    if (k->mood > mood_before) {
        const u32 turn = (u32)(((w->tick - k->born_tick) / w->cfg.tier2_period) & 3u);
        for (u32 l = 0; l < 8u; l++) k->memory[turn * 8u + l] = k->culture[l];
    }

    /* what each role is wanted for, from the colony's own state */
    u32 dem[AZ_ROLE_COUNT] = { 0 };
    u64 prov = k->energy > 0 ? (u64)k->energy : 0u;
    for (u32 i = 0; i < AZ_NINV; i++) prov += (u64)k->store[i] * (u64)az_eps_species[az_inv_species[i]];
    const u64 ease = (u64)wp(w, AZ_WP_COLONY_EASE_EU) * n;
    dem[AZ_ROLE_FORAGER] = prov >= ease ? 0u : 255u - (u32)((prov * 255u) / ease);
    dem[AZ_ROLE_SOLDIER] = k->alarm;
    dem[AZ_ROLE_NURSE] = (u32)(((u64)young * 255u) / n);
    dem[AZ_ROLE_BUILDER] = 255u - w->c_mark[(size_t)az_cell_at(w, k->cx, k->cy, k->cz) * AZ_NMARK + AZ_MK_NEST];
    {
        const u64 mean = sdist / n, span = (u64)wp(w, AZ_WP_COLONY_SPREAD) << 8;
        dem[AZ_ROLE_SCOUT] = mean >= span ? 0u : 255u - (u32)((mean * 255u) / span);
        const u64 want = adults / wp(w, AZ_WP_COLONY_BREEDER_SPAN) + 1u, have = cnt[AZ_ROLE_BREEDER];
        dem[AZ_ROLE_BREEDER] = have >= want ? 0u : 255u - (u32)((have * 255u) / want);
        const u32 a = az_log2_q8(1u + k->store[0]), b = az_log2_q8(1u + k->store[1]);
        dem[AZ_ROLE_TRADER] = az_min_u32((a > b ? a - b : b - a) >> 3, 255u);
    }
    u32 top = 0;
    for (u32 j = 1; j < AZ_ROLE_COUNT; j++) {
        const i32 up = (i32)((dem[j] * wp(w, AZ_WP_COLONY_NEED_GAIN)) >> 8);
        const i32 down = (i32)(((((u64)cnt[j] << 8) / n) * wp(w, AZ_WP_COLONY_NEED_WORK)) >> 8);
        k->need[j] = (u8)az_clamp_i32((i32)k->need[j] + up - down, 0, 255);
        if (k->need[j] > top) top = k->need[j];
    }
    k->need[AZ_ROLE_NONE] = (u8)top;

    /* roles: each member against the calls, by its own thresholds */
    const u32 quit = wp(w, AZ_WP_COLONY_QUIT) << 8;
    const u64 th_min = wp(w, AZ_WP_COLONY_THETA_MIN), th_span = wp(w, AZ_WP_COLONY_THETA_SPAN);
    u32 fin[AZ_ROLE_COUNT] = { 0 };
    for (u32 i = 0; i < n; i++) {
        const u32 s = list[i];
        const u8 fl = w->flags[s];
        u32 role = w->role[s] < AZ_ROLE_COUNT ? w->role[s] : 0u;
        if (!(fl & (AZ_F_EGG | AZ_F_DORMANT))) {
            u32 o[4];
            az_rand4(w, w->uid[s], AZ_RS_TIER2, 0, o);
            if (role != AZ_ROLE_NONE) {
                if ((o[3] >> 16) < quit) role = AZ_ROLE_NONE;
            } else {
                const u32 lean = trow(w, s)[AZ_T_CASTE] * ROLES + ROLES / 2u;
                const u32 first = (o[3] >> 16) % ROLES;
                const bool back = (o[3] >> 31) != 0u;
                for (u32 t = 0; t < ROLES; t++) {
                    const u32 j = 1u + (back ? first + ROLES - t : first + t) % ROLES;
                    const u64 call = k->need[j];
                    if (!call || (j == AZ_ROLE_BREEDER && (fl & AZ_F_JUV))) continue;
                    const u32 at = (j - 1u) * 256u + 128u;
                    u32 d = lean > at ? lean - at : at - lean;
                    if (d > RING / 2u) d = RING - d;
                    const u64 theta = th_min + ((u64)d * th_span) / (RING / 2u);
                    const u32 pass = (u32)(((call * call) << 16) / (call * call + theta * theta));
                    const u32 draw = ((t & 1u) ? o[t >> 1] >> 16 : o[t >> 1]) & 0xFFFFu;
                    if (draw < pass) { role = j; break; }
                }
            }
            w->role[s] = (u8)role;
        }
        fin[role]++;
    }
    for (u32 j = 0; j < AZ_ROLE_COUNT; j++) k->caste[j] = fin[j];
}

/* ---- the pass --------------------------------------------------------------- */
static void sift(const AzWorld *w, u32 *a, u32 i, u32 n)
{
    for (;;) {
        u32 l = 2u * i + 1u;
        if (l >= n) return;
        if (l + 1u < n && w->uid[a[l + 1u]] > w->uid[a[l]]) l++;
        if (w->uid[a[i]] >= w->uid[a[l]]) return;
        const u32 t = a[i]; a[i] = a[l]; a[l] = t;
        i = l;
    }
}

/* slots in the order of their uids (a heap sort: no memory of its own) */
static void sort_by_uid(const AzWorld *w, u32 *a, u32 n)
{
    for (u32 i = n / 2u; i-- > 0u;) sift(w, a, i, n);
    for (u32 m = n; m-- > 1u;) {
        const u32 t = a[0]; a[0] = a[m]; a[m] = t;
        sift(w, a, 0, m);
    }
}

void az_colony_tier2(AzWorld *w)
{
    u32 *list = w->tmp_u32;                    /* serial stage: at least one entry per slot */
    const u32 found_k = wp(w, AZ_WP_COLONY_FOUND), leave_k = wp(w, AZ_WP_COLONY_LEAVE);
    const i64 rich = 2 * (i64)wp(w, AZ_WP_COLONY_ENDOW);
    u32 n_found = 0;

    /* 1. each creature for itself */
    for (u32 s = 0; s < w->n_slots; s++) {
        const u8 fl = w->flags[s];
        if (!(fl & AZ_F_ALIVE)) continue;
        u32 c = w->colony[s];
        if (c && (c >= w->n_colonies || !w->colonies[c].alive)) { w->colony[s] = 0; w->role[s] = AZ_ROLE_NONE; c = 0; }
        if (fl & (AZ_F_EGG | AZ_F_JUV | AZ_F_DORMANT)) continue;
        const u32 col = trow(w, s)[AZ_T_COLONIALITY];
        const u32 r = az_rand(w, w->uid[s], AZ_RS_COLONY, 0) & 0xFFFFu;
        if (c) {
            if (r < (((255u - col) * (255u - w->colonies[c].mood)) >> 8) * leave_k) { w->colony[s] = 0; w->role[s] = AZ_ROLE_NONE; }
        } else if (r < col * found_k && (i64)w->energy[s] >= rich) {
            list[n_found++] = s;
        }
    }

    /* 2. founding, in uid order; a record dissolved below is free from the next pass on */
    sort_by_uid(w, list, n_found);
    u32 cursor = 1;
    for (u32 i = 0; i < n_found; i++) (void)found_from(w, list[i], &cursor);

    /* 3. members grouped by colony: counts, then ends, then starts (caste[0] keeps the last count) */
    for (u32 c = 1; c < w->n_colonies; c++) { w->colonies[c].caste[0] = w->colonies[c].members; w->colonies[c].members = 0; }
    for (u32 s = 0; s < w->n_slots; s++)
        if ((w->flags[s] & AZ_F_ALIVE) && w->colony[s]) w->colonies[w->colony[s]].members++;
    u32 total = 0;
    for (u32 c = 1; c < w->n_colonies; c++) { total += w->colonies[c].members; w->colonies[c].members = total; }
    for (u32 s = 0; s < w->n_slots; s++)
        if ((w->flags[s] & AZ_F_ALIVE) && w->colony[s]) list[--w->colonies[w->colony[s]].members] = s;

    /* 4. to 6. each colony */
    for (u32 c = 1; c < w->n_colonies; c++) {
        AzColony *k = &w->colonies[c];
        const u32 start = k->members, end = c + 1u < w->n_colonies ? w->colonies[c + 1u].members : total;
        const u32 before = k->caste[0];
        if (!k->alive) { k->members = 0; k->caste[0] = 0; continue; }
        if (end == start) dissolve(w, c);
        else measure(w, c, list + start, end - start, before);
    }
}
