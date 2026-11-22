/* Public-post regressions for chemistry at the upper end of a u32 cell stock.
 * The same test entry point exercises the selected real or reference chemistry. */
#include "az_test.h"

static AzWorld *empty_world(AzArena *arena)
{
    AzConfig cfg = az_test_config(9, 0);
    cfg.cap = 8;
    cfg.gx_bits = cfg.gy_bits = 2;
    cfg.gz_bits = 1;
    cfg.max_clades = cfg.max_colonies = 8;
    AzWinInfo info = {0};
    if (!az_win_arena(arena, az_world_bytes(&cfg), AZ_WIN_LOCK, &info)) {
        CHECK(!"locked arena allocation failed");
        return NULL;
    }
    CHECK(info.arena_locked);
    AzWorld *w = az_world_create(arena, &cfg, NULL);
    CHECK(w != NULL);
    if (w) memset(w->c_solid, 255, w->n_cells);
    return w;
}

static void stock_cell(AzWorld *w, u32 c, u32 species, u32 stock)
{
    for (u32 left = stock; left;) {
        const u32 add = az_min_u32(left, 1u << 30);
        az_cell_post(w, 0, c, species, (i32)add);
        az_field_commit(w, 0, w->n_cells, 0);
        left -= add;
    }
    CHECK_EQ(w->c_sp[(size_t)c * AZ_NSPECIES + species], stock);
    CHECK_EQ(az_pending_species(w, c, species), 0);
}

static void check_books(const AzWorld *w, u64 matter, i64 energy)
{
    CHECK_EQ(az_total_matter(w), matter);
    CHECK_EQ(az_total_energy(w), energy);
    CHECK_EQ(w->workers[0].tally.field_underflow, 0);
    CHECK_EQ(w->workers[0].dropped_events, 0);
    CHECK_EQ(w->totals.field_underflow, 0);
    CHECK_EQ(w->totals.dropped_events, 0);
}

static void check_committed_heat(const AzWorld *w, u32 c, i64 heat)
{
    const u32 stored = heat > UINT32_MAX ? UINT32_MAX : (u32)heat;
    CHECK_EQ(w->c_heat[c], stored);
    CHECK_EQ(az_pending_heat(&w->c_acc[c]), heat - stored);
}

static void check_decay(u32 stock)
{
    AzArena arena = {0};
    AzWorld *w = empty_world(&arena);
    if (!w) { az_win_arena_free(&arena); return; }
    const u32 c = az_cell_xyz(w, 1, 1, 1);
    w->c_solid[c] = 0;
    stock_cell(w, c, AZ_SP_TOX_Z, stock);
#ifdef AZ_TEST_STUB_CHEM
    const i64 reacted = ((u64)stock + 15u) / 16u;
#else
    CHECK(az_param_set(w, AZ_WP_CHEM_TOX_DECAY, 65536));
    const i64 reacted = ((u64)stock + 1u) / 2u;
#endif
    const u64 matter = az_total_matter(w);
    const i64 energy = az_total_energy(w);
    az_chem_cell(w, c, 0);
    CHECK_EQ(az_pending_species(w, c, AZ_SP_TOX_Z), -reacted);
    CHECK_EQ(az_pending_species(w, c, AZ_SP_WST_W), reacted);
    CHECK_EQ(az_pending_heat(&w->c_acc[c]), 2 * reacted);
    check_books(w, matter, energy);

    az_field_commit(w, 0, w->n_cells, 0);
    CHECK_EQ(w->c_sp[(size_t)c * AZ_NSPECIES + AZ_SP_TOX_Z], stock - reacted);
    CHECK_EQ(w->c_sp[(size_t)c * AZ_NSPECIES + AZ_SP_WST_W], reacted);
    CHECK_EQ(az_pending_species(w, c, AZ_SP_TOX_Z), 0);
    CHECK_EQ(az_pending_species(w, c, AZ_SP_WST_W), 0);
    check_committed_heat(w, c, 2 * reacted);
    check_books(w, matter, energy);
    az_win_arena_free(&arena);
}

#ifndef AZ_TEST_STUB_CHEM
/* Half the organic stock oxidizes, a quarter is buried and an eighth diffuses
 * to the only open neighbour. Both the waste post and net organic withdrawal
 * exceed a single signed post, yet every final species stock is representable. */
static void check_combined_fluxes(void)
{
    AzArena arena = {0};
    AzWorld *w = empty_world(&arena);
    if (!w) { az_win_arena_free(&arena); return; }
    const u32 c = az_cell_xyz(w, 1, 1, 1), nb = az_cell_xyz(w, 2, 1, 1);
    const u32 stock = UINT32_MAX - 7u;
    const i64 ox = stock / 2u, buried = stock / 4u, moved = stock / 8u;
    const i64 heat = 12 * ox + 16 * buried;
    w->c_solid[c] = w->c_solid[nb] = 0;
    stock_cell(w, c, AZ_SP_ORG_Y, stock);
    stock_cell(w, c, AZ_SP_OXI, stock);
    CHECK(az_param_set(w, AZ_WP_CHEM_OXIDATION, 65536));
    CHECK(az_param_set(w, AZ_WP_CHEM_DOUBLE, 32767));
    CHECK(az_param_set(w, AZ_WP_CHEM_BURIAL, 65536));
    CHECK(az_param_set(w, AZ_WP_CHEM_DIFFUSE_SOLID, 8192));
    CHECK(az_param_set(w, AZ_WP_CHEM_DIFFUSE_LIQUID, 8192));
    CHECK(az_param_set(w, AZ_WP_CHEM_DIFFUSE_GAS, 8192));
    const u64 matter = az_total_matter(w);
    const i64 energy = az_total_energy(w);
    az_chem_cell(w, c, 0);
    CHECK_EQ(az_pending_species(w, c, AZ_SP_ORG_Y), -ox - buried - moved);
    CHECK_EQ(az_pending_species(w, c, AZ_SP_OXI), -ox - moved);
    CHECK_EQ(az_pending_species(w, c, AZ_SP_WST_W), 2 * ox);
    CHECK_EQ(az_pending_species(w, nb, AZ_SP_ORG_Y), moved);
    CHECK_EQ(az_pending_species(w, nb, AZ_SP_OXI), moved);
    CHECK_EQ(w->workers[0].tally.reservoir, buried);
    CHECK_EQ(az_pending_heat(&w->c_acc[c]), heat);
    check_books(w, matter, energy);
    az_field_commit(w, 0, w->n_cells, 0);
    const u32 *here = &w->c_sp[(size_t)c * AZ_NSPECIES];
    const u32 *there = &w->c_sp[(size_t)nb * AZ_NSPECIES];
    CHECK_EQ(here[AZ_SP_ORG_Y], stock - ox - buried - moved);
    CHECK_EQ(here[AZ_SP_OXI], stock - ox - moved);
    CHECK_EQ(here[AZ_SP_WST_W], 2 * ox);
    CHECK_EQ(there[AZ_SP_ORG_Y], moved);
    CHECK_EQ(there[AZ_SP_OXI], moved);
    check_committed_heat(w, c, heat);
    check_committed_heat(w, nb, 0);
    az_tally_merge(w);
    CHECK_EQ(w->totals.reservoir_mu, buried);
    check_books(w, matter, energy);
    az_win_arena_free(&arena);
}
#endif

int main(void)
{
    const u32 stocks[] = {0, 1, 15, 16, 17, UINT32_MAX - 15u, UINT32_MAX - 14u,
                          UINT32_MAX - 1u, UINT32_MAX};
    for (size_t i = 0; i < sizeof stocks / sizeof stocks[0]; i++) check_decay(stocks[i]);
#ifndef AZ_TEST_STUB_CHEM
    check_combined_fluxes();
#endif
    return az_test_done();
}
