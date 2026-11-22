/* Balanced transfers through the normal cell posting and commit entry points. */
#include "az_test.h"
#include <inttypes.h>

static int check_transfers(u32 species, bool withdraw)
{
    AzConfig cfg = az_test_config(7, 0);
    cfg.cap = 8;
    cfg.gx_bits = 2;
    cfg.gy_bits = 2;
    cfg.gz_bits = 1;
    AzArena arena = {0};
    AzWinInfo info = {0};
    if (!az_win_arena(&arena, az_world_bytes(&cfg), AZ_WIN_LOCK, &info) || !info.arena_locked) return 2;
    AzWorld *w = az_world_create(&arena, &cfg, NULL);
    if (!w) { az_win_arena_free(&arena); return 2; }
    const i32 transfers[3] = { 1073741824, 1073741823, withdraw ? 2 : 1 };
    for (u32 i = 0; i < 3; i++) {
        u32 source_cell = withdraw ? 0 : i + 1;
        w->c_sp[(size_t)source_cell * AZ_NSPECIES + species] += (u32)transfers[i];
    }
    u64 matter = az_total_matter(w);
    i64 energy = az_total_energy(w);
    int failed = 0;
    for (u32 i = 0; i < 3; i++) {
        u32 from = withdraw ? 0 : i + 1;
        u32 to = withdraw ? i + 1 : 0;
        az_cell_post(w, 0, from, species, -transfers[i]);
        az_cell_post(w, 0, to, species, transfers[i]);
        if (az_total_matter(w) != matter || az_total_energy(w) != energy) failed++;
    }
    az_field_commit(w, 0, 4, 0);
    const u32 expected = withdraw ? 0 : UINT32_C(2147483648);
    u32 actual = w->c_sp[species];
    u64 underflow = w->workers[0].tally.field_underflow;
    printf("species=%u withdraw=%u locked=%u matter_before=%" PRIu64 " matter_after=%" PRIu64
           " energy_before=%" PRId64 " energy_after=%" PRId64 " target=%u actual=%u tally=%" PRIu64
           " precommit_failed=%d\n", species, (unsigned)withdraw, (unsigned)info.arena_locked, matter,
           az_total_matter(w), energy, az_total_energy(w), expected, actual, underflow, failed);
    if (actual != expected || underflow || az_total_matter(w) != matter || az_total_energy(w) != energy) failed++;
    az_win_arena_free(&arena);
    return failed != 0;
}

/* Test the sparse high limbs independently of committed field state. */
#ifdef AZ_TEST_HAS_SNAPSHOT
static void check_persisted_carry(void)
{
    AzConfig cfg = az_test_config(11, 0);
    cfg.gx_bits = cfg.gy_bits = 2;
    cfg.gz_bits = 1;
    cfg.cap = 8;
    AzWinInfo info = {0};
    AzArena arena = {0}, read_arena = {0};
    u64 bytes = az_world_bytes(&cfg);
    if (!az_win_arena(&arena, bytes, AZ_WIN_LOCK, &info) || !info.arena_locked) {
        CHECK(!"first locked arena unavailable");
        return;
    }
    if (!az_win_arena(&read_arena, bytes, AZ_WIN_LOCK, &info) || !info.arena_locked) {
        CHECK(!"restore locked arena unavailable");
        az_win_arena_free(&arena);
        return;
    }
    AzWorld *w = az_world_create(&arena, &cfg, NULL);
    CHECK(w != NULL);
    if (w) {
        const u32 k = AZ_SP_ORG_Y;
        for (u32 c = 1; c <= 4; c++) w->c_sp[(size_t)c * AZ_NSPECIES + k] = UINT32_C(1) << 30;
        const u64 matter = az_total_matter(w);
        const i64 energy = az_total_energy(w);
        for (u32 c = 1; c <= 4; c++) {
            az_cell_post(w, 0, c, k, -(INT32_C(1) << 30));
            az_cell_post(w, 0, 0, k, INT32_C(1) << 30);
        }
        CHECK_EQ(w->c_acc[0].delta[k], 0);
        CHECK_EQ(w->c_species_carry[k], 1);
        const u64 hash = az_state_hash(w);
        w->c_species_carry[k] = 0;
        CHECK(az_state_hash(w) != hash);
        w->c_species_carry[k] = 1;
        const u64 size = az_snapshot_bytes(w);
        u8 *save = malloc((size_t)size);
        CHECK(save != NULL);
        if (save) {
            CHECK_EQ(az_snapshot_save(w, save, size), size);
            AzWorld *loaded = az_snapshot_load(&read_arena, save, size, NULL);
            CHECK(loaded != NULL);
            if (loaded) {
                CHECK_EQ(az_state_hash(loaded), hash);
                CHECK_EQ(az_total_matter(loaded), matter);
                CHECK_EQ(az_total_energy(loaded), energy);
                az_field_commit(loaded, 0, loaded->n_cells, 0);
                CHECK_EQ(loaded->workers[0].tally.field_underflow, 0);
                CHECK_EQ(loaded->c_sp[k], UINT32_MAX);
                CHECK_EQ(az_pending_species(loaded, 0, k), 1);
                /* Move the surplus to another cell. */
                az_cell_post(loaded, 0, 0, k, -1);
                az_cell_post(loaded, 0, 5, k, 1);
                az_field_commit(loaded, 0, loaded->n_cells, 0);
                CHECK_EQ(loaded->c_sp[k], UINT32_MAX);
                CHECK_EQ(loaded->c_sp[5 * AZ_NSPECIES + k], 1);
                CHECK_EQ(az_pending_species(loaded, 0, k), 0);
                CHECK_EQ(loaded->workers[0].tally.field_underflow, 0);
                CHECK_EQ(az_total_matter(loaded), matter);
                CHECK_EQ(az_total_energy(loaded), energy);
            }
            free(save);
        }
    }
    az_win_arena_free(&read_arena);
    az_win_arena_free(&arena);
}


#endif

/* Both chem implementations turn this pair of toxin units into exactly
 * one waste unit and two units of heat on their first step. */
static void check_reaction_at_full_storage(void)
{
    AzConfig cfg = az_test_config(8, 0);
    cfg.cap = 8;
    cfg.gx_bits = cfg.gy_bits = 2;
    cfg.gz_bits = 1;
    AzArena arena = {0};
    AzWinInfo info = {0};
    if (!az_win_arena(&arena, az_world_bytes(&cfg), AZ_WIN_LOCK, &info) || !info.arena_locked) {
        CHECK(!"locked arena unavailable");
        return;
    }
    AzWorld *w = az_world_create(&arena, &cfg, NULL);
    CHECK(w != NULL);
    if (w) {
        const u32 c = 17, k = AZ_SP_WST_W;
        CHECK(az_param_set(w, AZ_WP_CHEM_TOX_DECAY, 65536));
        CHECK(az_param_set(w, AZ_WP_CHEM_BURIAL, 0));
        memset(w->c_solid, 255, w->n_cells);
        w->c_solid[c] = 0;
        w->c_sp[(size_t)c * AZ_NSPECIES + k] = UINT32_MAX;
        w->c_sp[(size_t)c * AZ_NSPECIES + AZ_SP_TOX_Z] = 2;
        u64 matter = az_total_matter(w);
        i64 energy = az_total_energy(w);
        az_chem_cell(w, c, 0);
        az_field_commit(w, 0, w->n_cells, 0);
        CHECK_EQ(w->c_sp[(size_t)c * AZ_NSPECIES + AZ_SP_TOX_Z], 1);
        CHECK_EQ(w->c_sp[(size_t)c * AZ_NSPECIES + k], UINT32_MAX);
        CHECK_EQ(w->c_heat[c], 2);
        CHECK_EQ(az_pending_species(w, c, k), 1);
        az_field_commit(w, c, c + 1, 0);
        CHECK_EQ(az_pending_species(w, c, k), 1);
        CHECK_EQ(w->workers[0].tally.field_underflow, 0);
        az_cell_post(w, 0, c, k, -2);
        az_cell_post(w, 0, c + 1u, k, 2);
        az_field_commit(w, 0, w->n_cells, 0);
        CHECK_EQ(w->c_sp[(size_t)c * AZ_NSPECIES + k], UINT32_MAX - 1u);
        CHECK_EQ(az_pending_species(w, c, k), 0);
        CHECK_EQ(w->workers[0].tally.field_underflow, 0);
        CHECK_EQ(az_total_matter(w), matter);
        CHECK_EQ(az_total_energy(w), energy);
    }
    az_win_arena_free(&arena);
}

int main(void)
{
    check_reaction_at_full_storage();
    int failures = 0;
    for (u32 species = 0; species < AZ_NSPECIES; species++)
        for (u32 withdraw = 0; withdraw <= 1; withdraw++)
            failures += check_transfers(species, withdraw != 0);
    printf("%d of 16 balanced transfer cases failed\n", failures);
    CHECK_EQ(failures, 0);
#ifdef AZ_TEST_HAS_SNAPSHOT
    check_persisted_carry();
#endif
    return az_test_done();
}

