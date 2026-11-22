/* az_tick.c - the tick pipeline. Owner: project lead.
 *
 * One tick, in order (docs/ARCHITECTURE.md section 3):
 *   0  reorder (periodic) + grid build         serial entry, parallel inside the grid module
 *      scratch reset                            parallel over slots
 *   1  perceive  : scan, sense, brain, intents, demand, steer     parallel over sorted positions
 *   2a claims    : initiators post claims on their targets        parallel over slots
 *   2b settle    : winners take their target, losers stand down   parallel over slots
 *      grant     : cells and colony stores turn demand into granted fractions
 *   3  act       : resolve pairs, social, economy, metabolism, physics, life
 *                  parallel over slots, one cache block (AZ_CHUNK slots) at a time
 *      colony commit                                               serial
 *   4  fields    : commit entity changes, global weather, strided chem + weather, commit fluxes
 *   5  life commit (births)                                        serial
 *   6  tier 1 (phased subset) ; tier 2 (every K2 ticks) ; ledger flush
 */
#include "az_abi.h"
#ifdef AZ_BUILDER_OBSERVER
#include "az_builder_hooks.h"
#endif
#ifdef AZ_PARTNER_OBSERVER
#include "az_partner_hooks.h"
#endif
#ifdef AZ_HERMIT_OBSERVER
#include "az_hermit_hooks.h"
#endif
#include "az_colony_store.h"

/* Entities perceived per az_scan_range call. The percepts of one batch live
 * in the worker's scratch block (az_ws_tick), not on the stack: 32 of them
 * were a 6.7 KB frame (finding v39). A percept does not depend on how the
 * sorted positions are batched. */
#define AZ_SCAN_BATCH 32u

_Static_assert(AZ_SCAN_BATCH * sizeof(AzPercept) <= AZ_WS_TICK_BYTES, "a batch of percepts must fit the tick part of the worker scratch");

/* ---------------------------------------------------------------------- */
/* work splitting                                                          */
/* ---------------------------------------------------------------------- */
static void run_parts(AzWorld *w, AzJobFn fn, u32 n_parts)
{
    if (n_parts > 1 && az_host_parallel(w)) {
        w->host->parallel_for(w->host->ctx, w, fn, w->splits, n_parts);
    } else {
        for (u32 k = 0; k < n_parts; k++) fn(w, w->splits[k], w->splits[k + 1], 0);
    }
}

/* how many parts a region may be cut into */
static u32 part_budget(const AzWorld *w)
{
    u32 parts = w->n_workers;
    if (az_host_parallel(w) && w->host->max_parts > parts) {   /* the host hands parts out dynamically */
        parts = w->n_workers * 8u;
        if (parts > w->host->max_parts) parts = w->host->max_parts;
    } else if (w->cfg.debug_split_seed && !az_host_parallel(w)) {
        parts = 7;                                      /* serial test runs still see odd splits */
    }
    return parts > AZ_MAX_PARTS ? AZ_MAX_PARTS : parts;
}

/* Test aid: replace the cut points of [0, units) by random ones. The results
 * of a tick must not depend on where the work is cut. */
static void scramble_cuts(AzWorld *w, u32 parts, u32 units, u32 *cut /* parts + 1 */)
{
    u64 s = ((u64)w->cfg.debug_split_seed << 32) ^ ((u64)w->tick << 8) ^ (u64)(w->split_counter++);
    cut[0] = 0;
    for (u32 k = 1; k < parts; k++) {
        s = az_mix64(s + 0x9E3779B97F4A7C15ull);
        cut[k] = units ? (u32)(s % (units + 1u)) : 0;
    }
    cut[parts] = units;
    for (u32 i = 2; i < parts; i++) {                   /* insertion sort of the inner cuts */
        u32 key = cut[i], j = i;
        while (j > 1 && cut[j - 1] > key) { cut[j] = cut[j - 1]; j--; }
        cut[j] = key;
    }
}

/* [0, n) into parts whose boundaries are multiples of quantum */
static u32 split_even(AzWorld *w, u32 n, u32 quantum)
{
    u32 parts = part_budget(w);
    u32 units = (n + quantum - 1) / quantum;
    if (units == 0) { w->splits[0] = 0; w->splits[1] = 0; return 1; }
    if (parts > units) parts = units;
    if (w->cfg.debug_split_seed) {
        scramble_cuts(w, parts, units, w->splits);
        for (u32 k = 0; k <= parts; k++) {
            u64 at = (u64)w->splits[k] * quantum;
            w->splits[k] = at > n ? n : (u32)at;
        }
    } else {
        for (u32 k = 0; k <= parts; k++) {
            u64 at = ((u64)units * k / parts) * quantum;
            w->splits[k] = at > n ? n : (u32)at;
        }
    }
    w->splits[parts] = n;
    return parts;
}

/* ranges of sorted positions that start on cell boundaries and hold about the
 * same number of entities each; a cell is never split between two parts */
static u32 split_sorted_by_cells(AzWorld *w)
{
    u32 parts = part_budget(w);
    u32 n = w->n_sorted;
    if (n == 0) { w->splits[0] = 0; w->splits[1] = 0; return 1; }
    if (n < 2048 && !w->cfg.debug_split_seed) parts = 1;
    if (w->cfg.debug_split_seed) scramble_cuts(w, parts, n, w->splits);
    else for (u32 k = 0; k <= parts; k++) w->splits[k] = (u32)((u64)n * k / parts);
    /* move every inner cut forward to the first position of the next cell */
    for (u32 k = 1; k < parts; k++) {
        u32 p = w->splits[k];
        if (p < w->splits[k - 1]) p = w->splits[k - 1];
        if (p > 0 && p < n) {
            u32 c = w->cell[w->order[p - 1]];           /* cell of the entity before the cut */
            p = w->cell_start[c + 1];                    /* first position after that cell    */
        }
        w->splits[k] = p > n ? n : p;
    }
    w->splits[0] = 0;
    w->splits[parts] = n;
    return parts;
}

static u64 now(const AzWorld *w)
{
    return (w->host && w->host->now_ns) ? w->host->now_ns(w->host->ctx) : 0;
}

/* ---------------------------------------------------------------------- */
/* dispatch of pair interactions to their owning module                    */
/* ---------------------------------------------------------------------- */
static i32 pair_score(const AzWorld *w, u32 r, u32 c, u32 act)
{
    switch (act) {
    case AZ_ACT_TRADE: return az_econ_score(w, r, c, act);
    case AZ_ACT_MATE:  return az_mate_score(w, r, c, act);
    default:           return az_social_score(w, r, c, act);
    }
}

/* The social wealth unit values one retained matter unit at AZ_EPS_BIO eu.
 * Pair actions can change pools, waste, toxin and inventory. Body parts stay
 * constant across this dispatcher. Metabolic costs occur later in the tick. */
static i64 pair_holdings(const AzWorld *w, u32 slot)
{
    i64 matter=(i64)w->pool_a[slot]+w->pool_m[slot]+w->pool_t[slot]+w->waste[slot]+w->toxin[slot];
    for (u32 k=0;k<AZ_NINV;k++) matter+=w->inv[(size_t)slot*AZ_NINV+k];
    return (i64)w->energy[slot]+matter*AZ_EPS_BIO;
}

static void pair_resolve(AzWorld *w, u32 slot, u32 partner, u32 act, bool initiator, u32 worker)
{
    const i64 before = pair_holdings(w,slot);
    switch (act) {
    case AZ_ACT_TRADE: az_econ_resolve(w, slot, partner, act, initiator, worker); break;
    case AZ_ACT_MATE:  az_mate_resolve(w, slot, partner, act, initiator, worker); break;
    default:           az_social_resolve(w, slot, partner, act, initiator, worker); break;
    }
    AzSenseMemory *history = az_sense_memory(w,slot);
    if (history) {
        i64 change = pair_holdings(w,slot) - before;
        history->payoff = (i8)az_clamp_i64(change,-64,63);
    }
}

/* channels that stay live under AZ_LESION_SENSES (the body's own state) */
static const u8 az_interoceptive[AZ_SENS] = {
    [AZ_S_ENERGY] = 1, [AZ_S_HEALTH] = 1, [AZ_S_PAIN] = 1, [AZ_S_AGE] = 1, [AZ_S_CARRY] = 1,
    [AZ_S_READY] = 1, [AZ_S_WASTE] = 1, [AZ_S_RANK] = 1, [AZ_S_NOISE] = 1, [AZ_S_AROUSAL] = 1,
    [AZ_S_MASS] = 1, [AZ_S_PARTS] = 1, [AZ_S_PAYOFF] = 1
};

/* ---------------------------------------------------------------------- */
/* jobs                                                                    */
/* ---------------------------------------------------------------------- */
/* The colony's energy pool and its shared signal channels are the engine's
 * own books (the pools of matter are the colony module's): demand -> granted
 * fraction, never more than the pool holds. */
static void colony_pool_grant(AzWorld *w)
{
    for (u32 i = 1; i < w->n_colonies; i++) {
        AzColonyEph *e = &w->colony_eph[i];
        u64 dem = e->dem_e;
        const i64 pool = w->colonies[i].energy;
        u64 have = (u64)az_clamp_i64(pool, 0, w->param[AZ_WP_CELL_HOLD]);
        if (!dem) { e->grant_e = 0; continue; }
        if (dem <= have) { e->grant_e = 65535u; continue; }
        e->grant_e = (u32)((have * 65535u) / dem);
    }
}

/* apply what the members gave and took, and turn their voices into the
 * signal the colony carries into the next tick */
static void colony_pool_commit(AzWorld *w)
{
    for (u32 i = 1; i < w->n_colonies; i++) {
        AzColonyEph *e = &w->colony_eph[i];
        AzColony *c = &w->colonies[i];
        az_commit_colony_energy(w, c, e);
        for (u32 k = 0; k < 4u; k++) {
            if (e->sig_n[k]) {
                const u64 mean = e->sig_sum[k] / e->sig_n[k];
                c->signal[k] = (u8)(mean > 255u ? 255u : mean);
            } else {
                c->signal[k] = (u8)(c->signal[k] - ((c->signal[k] + 7u) >> 3));
            }
        }
    }
}

static void job_scratch_reset(AzWorld *w, u32 b, u32 e, u32 worker)
{
    (void)worker;
    for (u32 s = b; s < e; s++) {
        AzPair *p = &w->pair[s];
        p->claim = 0; p->peer = AZ_NONE; p->act = AZ_ACT_NONE; p->urge = 0; p->req = 0; p->pad = 0;
        w->blk_req[(size_t)s * 3] = 0; w->blk_req[(size_t)s * 3 + 1] = 0; w->blk_req[(size_t)s * 3 + 2] = 0;
        w->birth[s].n = 0;
    }
}

static void job_field_grant(AzWorld *w, u32 b, u32 e, u32 worker) { az_field_grant(w, b, e, worker); }
static void job_field_commit(AzWorld *w, u32 b, u32 e, u32 worker) { az_field_commit(w, b, e, worker); }

/* Every creature has a brain of its own, so the next brain is not in cache
 * the way a shared one was: ask for the front of its block (header, brain
 * header, biases, the first tiles) while the current creature is worked on.
 * A prefetch reads nothing and cannot fault. */
AZ_INLINE void blk_prefetch(const AzWorld *w, u32 slot)
{
    const u8 *b = (const u8 *)(const void *)az_blk(w, slot);
    for (u32 q = 0; q < 1024u; q += 64u) __builtin_prefetch(b + q, 0, 3);
}

/* b, e are sorted positions; a part always starts and ends on a cell boundary */
static void job_perceive(AzWorld *w, u32 b, u32 e, u32 worker)
{
    AzPercept *pc = (AzPercept *)az_ws_tick(w, worker);
    AzCellSense cs;
    u8 sens[AZ_SENS] AZ_ALIGNED(32);
    const u32 lesion = w->cfg.lesion;
    /* lesions are masks on one code path: the same instructions run either way */
    const i8 motor_mask = (lesion & AZ_LESION_BRAIN) ? 0 : -1;
    const u8 hid_mask = (lesion & AZ_LESION_RECURRENCE) ? 0 : 0xFF;
    const u8 noise_mask = (lesion & AZ_LESION_NOISE) ? 0 : 0xFF;
    const u8 extero_mask = (lesion & AZ_LESION_SENSES) ? 0 : 0xFF;
    u32 cur_cell = AZ_NONE;

    for (u32 p0 = b; p0 < e; p0 += AZ_SCAN_BATCH) {
        u32 count = e - p0 < AZ_SCAN_BATCH ? e - p0 : AZ_SCAN_BATCH;
        az_scan_range(w, p0, count, pc, worker);
        for (u32 k = 0; k < count; k++) {
            u32 slot = w->order[p0 + k];
            if (k + 2u < count) blk_prefetch(w, w->order[p0 + k + 2u]);
            i8 *motor = &w->motor[(size_t)slot * AZ_MOT];
            u8 fin = 0;
            u8 fl = w->flags[slot];
            if (fl & (AZ_F_EGG | AZ_F_DORMANT)) {
                for (u32 m = 0; m < AZ_MOT; m++) motor[m] = 0;
            } else {
                u32 c = w->cell[slot];
                if (c != cur_cell) { az_sense_cell(w, c, &cs); cur_cell = c; }
                fin = az_sense_and_fin(w, slot, &pc[k], &cs, sens);
                sens[AZ_S_NOISE] &= noise_mask;
                for (u32 i = 0; i < AZ_SENS; i++)
                    sens[i] &= (u8)(extero_mask | (az_interoceptive[i] ? 0xFF : 0));
                /* the creature's own brain: its senses go into its mind, the
                 * step writes the next hidden state beside the old one */
                u32 bb, mb;
                const AzBrain *br = az_brain(w, slot, &bb);
                u8 *mind = (u8 *)az_seg(w, slot, AZ_SEG_MIND, &mb);
                if (bb >= sizeof(AzBrain) && mb >= AZ_SENS + 16u * br->n_hb) {
                    const u32 nh = 8u * br->n_hb;
                    u8 *hid = mind + AZ_SENS;
                    az_memcpy(mind, sens, AZ_SENS);
#ifdef AZ_OBSERVE_MIND_WRITE
                    AZ_OBSERVE_MIND_WRITE(worker, sens);
#endif
                    w->k.brain_step(br, mind, motor);
                    for (u32 h = 0; h < nh; h++) hid[h] = hid[nh + h] & hid_mask;
                    for (u32 m = 0; m < AZ_MOT; m++) motor[m] &= motor_mask;
                } else {
                    for (u32 m = 0; m < AZ_MOT; m++) motor[m] = 0;   /* no brain: no action */
                }
            }

            /* intents: social first (it zeroes the offer), then economy, then
             * mating. A lesioned module still writes its offer bytes; only its
             * proposal is discarded. */
            AzPair *pr = &w->pair[slot];
            az_social_intent(w, slot, &pc[k], worker);
            u8 a0 = pr->act, u0 = pr->urge; u32 t0 = pr->peer;
            az_econ_intent(w, slot, &pc[k], worker);
            if ((lesion & AZ_LESION_TRADE) && pr->act == AZ_ACT_TRADE) { pr->act = a0; pr->urge = u0; pr->peer = t0; }
            a0 = pr->act; u0 = pr->urge; t0 = pr->peer;
            az_mate_intent(w, slot, &pc[k], worker);
            if ((lesion & AZ_LESION_MATING) && pr->act == AZ_ACT_MATE) { pr->act = a0; pr->urge = u0; pr->peer = t0; }
            if (lesion & AZ_LESION_SOCIAL) { pr->act = AZ_ACT_NONE; pr->peer = AZ_NONE; pr->urge = 0; }
            /* only an initiator holds a proposal; a responder's peer is set in phase 2 */
            if (!(fl & AZ_F_INIT)) { pr->act = AZ_ACT_NONE; pr->peer = AZ_NONE; pr->urge = 0; }

            az_metab_demand(w, slot, worker);
            az_physics_steer_fin(w, slot, &pc[k], fin, worker);
        }
    }
}

/* Initiators post (score, ~sorted position) on their target; the maximum wins,
 * so of equal scores the claimant that comes first in the canonical order
 * (cell, then uid) does. The sorted position is unique in a tick and does not
 * depend on threads or on the reorder period; a 64-bit uid would not fit the
 * key. */
static void job_claims(AzWorld *w, u32 b, u32 e, u32 worker)
{
    (void)worker;
    for (u32 s = b; s < e; s++) {
        u8 fl = w->flags[s];
        AzPair *pr = &w->pair[s];
        if (!(fl & AZ_F_ALIVE) || !(fl & AZ_F_INIT)) continue;
        u32 act = pr->act;
        u32 r = pr->peer;
        if (act == AZ_ACT_NONE || act >= AZ_ACT_COUNT || r == AZ_NONE || r >= w->n_slots || r == s ||
            !(w->flags[r] & AZ_F_ALIVE) || (w->flags[r] & AZ_F_INIT)) {
            pr->act = AZ_ACT_NONE; pr->peer = AZ_NONE;
            continue;
        }
        i32 score = pair_score(w, r, s, act);
#ifdef AZ_HERMIT_OBSERVER
        azt_hermit_proposal(w, s, r, worker, act, score);
#endif
        if (score < 0) { pr->act = AZ_ACT_NONE; pr->peer = AZ_NONE; continue; }
        u64 key = ((u64)(u32)score << 32) | (u64)(u32)~w->spos[s];
        u64 *cl = &w->pair[r].claim;
        u64 cur = __atomic_load_n(cl, __ATOMIC_RELAXED);
        while (cur < key &&
               !__atomic_compare_exchange_n(cl, &cur, key, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) { }
    }
}

/* the winning initiator records itself on the target; losers stand down */
static void job_settle(AzWorld *w, u32 b, u32 e, u32 worker)
{
    (void)worker;
    for (u32 s = b; s < e; s++) {
        AzPair *pr = &w->pair[s];
        if (!(w->flags[s] & AZ_F_INIT) || pr->act == AZ_ACT_NONE) continue;
        u32 r = pr->peer;
        if ((u32)w->pair[r].claim == (u32)~w->spos[s]) w->pair[r].peer = s;  /* the single winner writes */
        else { pr->act = AZ_ACT_NONE; pr->peer = AZ_NONE; }
    }
}

static void job_act(AzWorld *w, u32 b, u32 e, u32 worker)
{
    AzTally *ty = az_tally(w, worker);
    for (u32 cb = b; cb < e; cb += AZ_CHUNK) {
        u32 ce = cb + AZ_CHUNK < e ? cb + AZ_CHUNK : e;
        for (u32 s = cb; s < ce; s++) {
            u8 fl = w->flags[s];
            if (!(fl & AZ_F_ALIVE)) continue;
            const AzPair *pr = &w->pair[s];
            if (fl & AZ_F_INIT) {
                u32 act = pr->act;
                if (act != AZ_ACT_NONE) {
                    ty->interactions[act]++;
                    pair_resolve(w, s, pr->peer, act, true, worker);
                }
            } else if (pr->peer != AZ_NONE) {
                u32 c = pr->peer;
                pair_resolve(w, s, c, w->pair[c].act, false, worker);
            }
        }
        az_social_range(w, cb, ce, worker);
        az_econ_range(w, cb, ce, worker);
        az_metab_range(w, cb, ce, worker);
        az_physics_range(w, cb, ce, worker);
        az_life_range(w, cb, ce, worker);
    }
}

static void job_field_step(AzWorld *w, u32 cb, u32 ce, u32 worker)
{
    const u32 mask = (u32)w->cfg.field_stride - 1u;     /* stride is a power of two */
    const u32 lesion = w->cfg.lesion;
    for (u32 c = cb; c < ce; c++) {
        u32 x = az_cell_x(w, c), y = az_cell_y(w, c), z = az_cell_z(w, c);
        if ((x + y + z + w->tick) & mask) continue;
        if (!(lesion & AZ_LESION_CHEM))    az_chem_cell(w, c, worker);
        if (!(lesion & AZ_LESION_WEATHER)) az_weather_cell(w, c, worker);
    }
}

static void job_tier1(AzWorld *w, u32 b, u32 e, u32 worker) { az_tier1_range(w, b, e, worker); }

/* ---------------------------------------------------------------------- */
/* the tick                                                                */
/* ---------------------------------------------------------------------- */
/* A wild world draws fresh entropy before anything else happens in the tick.
 * If the host has none this once, the tick runs on the last draw stirred
 * with the tick number, and the world says so (totals.entropy_missed, and a
 * ledger event the first time). */
static void wild_draw(AzWorld *w)
{
    u64 e[2];
    if (w->host && w->host->entropy && w->host->entropy(w->host->ctx, e, 16u)) {
        w->wild[0] = e[0];
        w->wild[1] = e[1];
    } else {
        w->wild[0] = az_mix64(w->wild[0] ^ w->tick);
        w->wild[1] = az_mix64(w->wild[1] + w->tick + 0x9E3779B97F4A7C15ull);
        if (w->totals.entropy_missed++ == 0) az_emit(w, 0, AZ_EV_REFUSED, AZ_REF_ENTROPY, 0, w->tick, 0);
    }
    if (w->host && w->host->entropy_log) w->host->entropy_log(w->host->ctx, w->tick, (const u8 *)(const void *)w->wild);
    az_tick_keys(w);
}

void az_tick(AzWorld *w)
{
    u64 t0, t1;
    u32 parts;
    w->split_counter = 0;
    if (w->cfg.mode == AZ_MODE_WILD) wild_draw(w);

    t0 = now(w);
    if (w->tick % w->cfg.reorder_period == 0) az_grid_reorder(w);
    t1 = now(w); w->prof_ns[AZ_PROF_REORDER] += t1 - t0; t0 = t1;

    az_grid_build(w);
    /* pack the creature store in slot order (the dead have just given their
     * blocks back). Where blocks lie is not state: any period gives the same
     * world. */
    if (w->tick % w->cfg.store_period == 0) az_store_compact(w);
    parts = split_even(w, w->n_slots, AZ_CHUNK);
    run_parts(w, job_scratch_reset, parts);
    /* colony demand and grants are per tick; deposits and withdrawals stay
     * pending until az_colony_commit applies them (finding v40: wiping them
     * here lost what a commit had refused and every deposit to colony 0) */
    for (u32 i = 0; i < w->n_colonies; i++) {
        az_memset(w->colony_eph[i].dem, 0, sizeof w->colony_eph[i].dem);
        az_memset(w->colony_eph[i].grant, 0, sizeof w->colony_eph[i].grant);
        w->colony_eph[i].dem_e = 0;
        w->colony_eph[i].grant_e = 0;
        az_memset(w->colony_eph[i].sig_sum, 0, sizeof w->colony_eph[i].sig_sum);
        az_memset(w->colony_eph[i].sig_n, 0, sizeof w->colony_eph[i].sig_n);
    }
    t1 = now(w); w->prof_ns[AZ_PROF_GRID] += t1 - t0; t0 = t1;

    parts = split_sorted_by_cells(w);
    run_parts(w, job_perceive, parts);
    t1 = now(w); w->prof_ns[AZ_PROF_PERCEIVE] += t1 - t0; t0 = t1;

    parts = split_even(w, w->n_slots, AZ_CHUNK);
    run_parts(w, job_claims, parts);
#ifdef AZ_HERMIT_OBSERVER
    azt_hermit_barrier(w);
#endif
    run_parts(w, job_settle, parts);
    parts = split_even(w, w->n_cells, 64);
    run_parts(w, job_field_grant, parts);
    az_colony_grant(w);
    colony_pool_grant(w);
    t1 = now(w); w->prof_ns[AZ_PROF_MATCH] += t1 - t0; t0 = t1;

    parts = split_even(w, w->n_slots, AZ_CHUNK);
    run_parts(w, job_act, parts);
#ifdef AZ_PARTNER_OBSERVER
    azt_partner_tick(w);
#endif
    az_tally_merge(w);
    az_colony_commit(w);
    colony_pool_commit(w);
    if (!(w->cfg.lesion & AZ_LESION_TRADE)) az_econ_global(w);
    t1 = now(w); w->prof_ns[AZ_PROF_ACT] += t1 - t0; t0 = t1;

    parts = split_even(w, w->n_cells, 64);
#ifdef AZ_BUILDER_OBSERVER
    azt_builder_before_commit(w);
#endif
    run_parts(w, job_field_commit, parts);
#ifdef AZ_BUILDER_OBSERVER
    azt_builder_after_commit(w);
#endif
    if (!(w->cfg.lesion & AZ_LESION_WEATHER)) az_weather_global(w);
    run_parts(w, job_field_step, parts);
    run_parts(w, job_field_commit, parts);
    az_tally_merge(w);
    t1 = now(w); w->prof_ns[AZ_PROF_FIELD] += t1 - t0; t0 = t1;

    az_life_commit(w);
    az_tally_merge(w);
    t1 = now(w); w->prof_ns[AZ_PROF_LIFE] += t1 - t0; t0 = t1;

    parts = split_even(w, w->n_slots, AZ_CHUNK);
    run_parts(w, job_tier1, parts);
    /* what the living asked of their own blocks this tick, in phase 3 or in
     * Tier 1: parts shed and grown, memory made longer or shorter */
    az_store_commit(w);
    az_tally_merge(w);
    t1 = now(w); w->prof_ns[AZ_PROF_TIER1] += t1 - t0; t0 = t1;

    if (!(w->cfg.lesion & AZ_LESION_COLONY) && (w->tick % w->cfg.tier2_period) == 0)
        az_colony_tier2(w);
    az_tally_merge(w);
    t1 = now(w); w->prof_ns[AZ_PROF_TIER2] += t1 - t0; t0 = t1;

    az_ledger_flush(w);
    t1 = now(w); w->prof_ns[AZ_PROF_LEDGER] += t1 - t0;

    az_publish_pressure(w);
    w->tick++;
    az_tick_keys(w);
}
