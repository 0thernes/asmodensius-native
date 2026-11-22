/* A funded parallel transfer across each 32-bit pantry boundary. */
#include "az_test.h"

enum { DONORS = 64 };
#define UNITS (1u << 27)

static void deposit(AzWorld *w, u32 begin, u32 end, u32 worker)
{
    (void)worker;
    for (u32 s = begin; s < end; s++)
        for (u32 k = 0; k < AZ_NINV; k++) {
            w->inv[(size_t)s * AZ_NINV + k] -= UNITS;
            az_atomic_add_u64(&w->colony_eph[1].dep[k], UNITS);
            az_atomic_add_u64(&w->colony_eph[1].dem[k], UNITS);
        }
}

static void withdraw(AzWorld *w, u32 begin, u32 end, u32 worker)
{
    (void)worker;
    for (u32 s = begin; s < end; s++)
        for (u32 k = 0; k < AZ_NINV; k++) {
            w->inv[(size_t)s * AZ_NINV + k] += UNITS;
            az_atomic_add_u64(&w->colony_eph[1].taken[k], UNITS);
        }
}

#ifdef AZ_TEST_REAL_PANTRY
/* The pantry producer and consumer use the developed body capacity and the
 * actual phase-1/phase-3 protocol. Each loan is funded by the colony store. */
#ifdef AZ_TEST_REAL_COLONY_FOUNDING
/* Founding runs after the ordinary pool barrier. Its gift must respect the
 * same cap immediately, using the founder's real, generated energy. */
static void funded_founder_cap(void)
{
    AzConfig cfg = az_test_config(42, 128);
    cfg.cap = 512;
    cfg.gx_bits = cfg.gy_bits = 3;
    cfg.gz_bits = 2;
    cfg.max_clades = cfg.max_colonies = 256;
    AzArena arena = {0};
    AzWinInfo info = {0};
    if (!az_win_arena(&arena, az_world_bytes(&cfg), AZ_WIN_LOCK, &info) || !info.arena_locked) {
        CHECK(!"locked arena"); return;
    }
    AzWorld *w = az_world_create(&arena, &cfg, NULL);
    CHECK(w != NULL);
    if (!w) { az_win_arena_free(&arena); return; }
    CHECK(az_param_set(w, AZ_WP_CELL_HOLD, 65536));
    CHECK(az_param_set(w, AZ_WP_PART_HOLD, 65536));
    CHECK(az_param_set(w, AZ_WP_WORLDGEN_FOUNDER_ENERGY, 1048576));
    CHECK(az_param_set(w, AZ_WP_COLONY_ENDOW, 262144));
    CHECK(az_param_set(w, AZ_WP_COLONY_FOUND, 256));
    az_worldgen(w);
    az_grid_build(w);
    az_ledger_flush(w);
    az_tally_merge(w);
    const u64 m = az_total_matter(w);
    const i64 e = az_total_energy(w);
    const u64 sat = w->totals.sat[AZ_SAT_ENERGY];
    u64 h0 = 0, h1 = 0;
    for (u32 i = 0; i < w->n_cells; i++) h0 += w->c_heat[i] + az_pending_heat(&w->c_acc[i]);
    az_colony_tier2(w);
    const u32 founded = w->n_colonies - 1;
    CHECK(founded > 0);
    for (u32 i = 1; i < w->n_colonies; i++) CHECK_EQ(w->colonies[i].energy, 65536);
    for (u32 i = 0; i < w->n_cells; i++) h1 += w->c_heat[i] + az_pending_heat(&w->c_acc[i]);
    CHECK_EQ(h1 - h0, (u64)founded * (262144 - 65536));
    CHECK_EQ(w->totals.sat[AZ_SAT_ENERGY] - sat, founded);
    CHECK_EQ(az_total_matter(w), m);
    CHECK_EQ(az_total_energy(w), e);
    printf("native founding colonies=%u pool=%" PRId64 " refunded=%" PRIu64 " refusals=%" PRIu64 "\n",
           founded, w->colonies[1].energy, h1 - h0, w->totals.sat[AZ_SAT_ENERGY] - sat);
    az_win_arena_free(&arena);
}

#endif

static void demand_from_members(AzWorld *w)
{
    w->colonies[1].store[1] = w->param[AZ_WP_CELL_HOLD];
    AzPercept peer = {0};
    u64 sum = 0, expected = 0;
    for (u32 s = 0; s < DONORS; s++) {
        if (!az_phase_due(w, w->uid[s], AZ_RS_ECON, 2) ||
            (w->flags[s] & (AZ_F_ALIVE | AZ_F_EGG | AZ_F_DORMANT)) != AZ_F_ALIVE) continue;
        u32 parts = 0;
        az_parts(w, s, &parts);
        if (!parts) continue;
        memset(&w->inv[(size_t)s * AZ_NINV], 0, AZ_NINV * sizeof(u32));
        w->colony[s] = 1;
        w->trait[(size_t)s * AZ_TRAITS + AZ_T_CARRY_CAP] = 255;
        w->motor[(size_t)s * AZ_MOT + AZ_M_GIVE] = 0;
        w->pool_a[s] = 0;
        w->mass[s] = 1u << 30;
        w->pool_m[s] = w->mass[s] - w->pool_t[s];
        az_econ_intent(w, s, &peer, 0);
        sum += w->gather[(size_t)s * AZ_NINV + 1];
    }
    CHECK(sum > UINT32_MAX);
    CHECK_EQ(w->colony_eph[1].dem[1], sum);
    if (!sum) return;
    const u64 stock = w->colonies[1].store[1];
    const u64 fraction = stock * 65535u / sum;
    az_colony_grant(w);
    CHECK_EQ(w->colony_eph[1].grant[1], fraction);
    const u64 m = az_total_matter(w);
    const i64 energy = az_total_energy(w);
    for (u32 s = 0; s < DONORS; s++) {
        if (w->colony[s] != 1) continue;
        const u64 due = (u64)w->gather[(size_t)s * AZ_NINV + 1] * fraction / 65535u;
        az_econ_range(w, s, s + 1, 0);
        CHECK_EQ(w->inv[(size_t)s * AZ_NINV + 1], due);
        expected += due;
    }
    CHECK(expected > 0 && expected <= stock);
    CHECK_EQ(w->colony_eph[1].taken[1], expected);
    az_colony_commit(w);
    CHECK_EQ(w->colonies[1].store[1], stock - expected);
    CHECK_EQ(az_total_matter(w), m);
    CHECK_EQ(az_total_energy(w), energy);
    printf("physical pantry demand=%" PRIu64 " grant=%" PRIu64 " withdrawn=%" PRIu64 "\n",
           sum, fraction, expected);
}
#endif

static void run(u32 threads)
{
    AzConfig cfg = az_test_config(42, DONORS);
    cfg.cap = 256;
    cfg.max_colonies = cfg.max_clades = 16;
    cfg.lesion |= AZ_LESION_TRADE;
    cfg.gx_bits = cfg.gy_bits = 3;
    cfg.gz_bits = 2;
    AzWinInfo info = {0};
    AzArena arena = {0}, restore = {0};
    AzHost *host = az_win_host_create(threads, 0, &info);
    CHECK(host != NULL);
    if (!host) return;
    const u64 bytes = az_world_bytes(&cfg);
    if (!az_win_arena(&arena, bytes, AZ_WIN_LOCK, &info) || !info.arena_locked) {
        CHECK(!"locked arenas");
        goto done;
    }
    AzWorld *w = az_world_create(&arena, &cfg, host);
    CHECK(w != NULL);
    if (!w) goto done;
#ifdef AZ_TEST_REAL_PANTRY
    CHECK(az_param_set(w, AZ_WP_PART_HOLD, 1u << 30));
    CHECK(az_param_set(w, AZ_WP_ECON_CARRY, 256));
    CHECK(az_param_set(w, AZ_WP_ECON_KEEP, 256));
    CHECK(az_param_set(w, AZ_WP_ECON_OPEN_SHARE, 0));
#endif
    az_worldgen(w);
    CHECK(w->n_slots >= DONORS);
    if (w->n_slots < DONORS) goto done;
    az_grid_build(w);
    az_ledger_flush(w);
    az_tally_merge(w);
    w->n_colonies = 2;
    for (u32 s = 0; s < DONORS; s++)
        for (u32 k = 0; k < AZ_NINV; k++) w->inv[(size_t)s * AZ_NINV + k] = UNITS;
    for (u32 k = 0; k < AZ_NINV; k++) w->colonies[1].store[k] = 13u + k;
    u64 field[AZ_NINV] = {0};
    for (u32 c = 0; c < w->n_cells; c++)
        for (u32 k = 0; k < AZ_NINV; k++) {
            w->c_sp[(size_t)c * AZ_NSPECIES + az_inv_species[k]] = UINT32_MAX;
            field[k] += UINT32_MAX;
        }
    const u64 m = az_total_matter(w);
    const i64 energy = az_total_energy(w);
    u32 splits[DONORS + 1];
    for (u32 i = 0; i <= DONORS; i++) splits[i] = i;
    host->parallel_for(host->ctx, w, deposit, splits, DONORS);
    const u64 amount = (u64)DONORS * UNITS;
    const u32 limit = w->param[AZ_WP_CELL_HOLD];
    CHECK_EQ(az_total_matter(w), m);
    CHECK_EQ(az_total_energy(w), energy);
    CHECK_EQ(az_snapshot_bytes(w), 0);
    const u64 h = az_state_hash(w);
    w->colony_eph[1].dep[0] ^= 1ull << 32;
    CHECK(az_state_hash(w) != h);
    w->colony_eph[1].dep[0] ^= 1ull << 32;
    az_colony_commit(w);
    az_colony_grant(w);
    az_field_commit(w, 0, w->n_cells, 0);
    az_tally_merge(w);
    for (u32 k = 0; k < AZ_NINV; k++) {
        u64 observed = 0;
        for (u32 c = 0; c < w->n_cells; c++) {
            const u32 sp = az_inv_species[k];
            observed += w->c_sp[(size_t)c * AZ_NSPECIES + sp] + az_pending_species(w, c, sp);
        }
        CHECK_EQ(observed - field[k], amount + 13u + k - limit);
        CHECK_EQ(w->colonies[1].store[k], limit);
        CHECK_EQ(w->colony_eph[1].grant[k], (u64)limit * 65535u / amount);
    }
    CHECK_EQ(w->totals.sat[AZ_SAT_INVENTORY], 4);
    CHECK_EQ(w->totals.field_underflow, 0);
    CHECK_EQ(az_total_matter(w), m);
    CHECK_EQ(az_total_energy(w), energy);
    AzWorld *copy = w;
#ifdef AZ_TEST_HAS_SNAPSHOT
    if (!az_win_arena(&restore, bytes, AZ_WIN_LOCK, &info) || !info.arena_locked) {
        CHECK(!"locked restore arena");
        goto done;
    }
    const u64 n = az_snapshot_bytes(w);
    u8 *buf = malloc((size_t)n);
    if (!n || !buf || az_snapshot_save(w, buf, n) != n) {
        CHECK(!"save refunded field balance");
        free(buf);
        goto done;
    }
    CHECK_EQ(buf[12], 9);
    copy = az_snapshot_load(&restore, buf, n, host);
    free(buf);
    CHECK(copy != NULL);
    if (!copy) goto done;
    CHECK_EQ(az_state_hash(copy), az_state_hash(w));
#endif
    /* Refunded field stock cannot collateralize a later pantry withdrawal. */
    host->parallel_for(host->ctx, copy, withdraw, splits, DONORS);
    CHECK_EQ(az_total_matter(copy), m);
    CHECK_EQ(az_total_energy(copy), energy);
    az_colony_commit(copy);
    CHECK_EQ(copy->totals.field_underflow, AZ_NINV);
    CHECK_EQ(az_snapshot_bytes(copy), 0);
    for (u32 k = 0; k < AZ_NINV; k++) {
        CHECK_EQ(copy->colony_eph[1].taken[k], amount);
        CHECK_EQ(copy->colonies[1].store[k], limit);
    }
    host->parallel_for(host->ctx, copy, deposit, splits, DONORS);
    az_colony_commit(copy);
    CHECK_EQ(copy->totals.sat[AZ_SAT_INVENTORY], AZ_NINV);
    for (u32 k = 0; k < AZ_NINV; k++) {
        CHECK_EQ(copy->colony_eph[1].taken[k], 0);
        CHECK_EQ(copy->colony_eph[1].dep[k], 0);
        CHECK_EQ(copy->colonies[1].store[k], limit);
    }
    CHECK_EQ(az_total_matter(copy), m);
    CHECK_EQ(az_total_energy(copy), energy);
#ifdef AZ_TEST_REAL_PANTRY
    memset(copy->colony_eph[1].dem, 0, sizeof copy->colony_eph[1].dem);
    demand_from_members(copy);
#endif
done:
    if (restore.base) az_win_arena_free(&restore);
    if (arena.base) az_win_arena_free(&arena);
    az_win_host_destroy(host);
}

int main(void)
{
#ifdef AZ_TEST_REAL_COLONY_FOUNDING
    funded_founder_cap();
#endif
    run(1);
    run(4);
    return az_test_done();
}
