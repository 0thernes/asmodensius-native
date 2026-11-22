/* Physics state follows the UID through slot reuse, reorder, and save/restore. */
#include "az_test.h"

static void lock_arena(AzArena *arena, const AzConfig *cfg)
{
    AzWinInfo info = {0};
    if (!az_win_arena(arena, az_world_bytes(cfg), AZ_WIN_LOCK, &info) || !info.arena_locked) {
        fputs("physics state test requires a locked world arena\n", stderr);
        exit(2);
    }
    printf("physics state arena locked=%u bytes=%" PRIu64 "\n", (unsigned)info.arena_locked, arena->size);
}

static void descriptors(void)
{
    const u32 ids[] = {AZ_WP_PHYS_BUOY_SHIFT, AZ_WP_PHYS_FIN_GAIN, AZ_WP_PHYS_CONTACT_GAIN,
                       AZ_WP_PHYS_THRUST_COST, AZ_WP_PHYS_ANG_RETAIN};
    const u32 defaults[] = {4, 192, 128, 4, 192};
    const u32 lows[] = {0, 0, 0, 1, 0}, highs[] = {15, 255, 255, 255, 255};
    CHECK_EQ(AZ_WP_COUNT, 240);
    CHECK_EQ(AZ_LESION_THRUST, 1u << 22);
    for (u32 k = 0; k < AZ_LEN(ids); k++) {
        const AzParamDesc *desc = az_param_desc(ids[k]);
        CHECK_EQ(ids[k], 235 + k);
        CHECK(desc && desc->def == defaults[k] && desc->lo == lows[k] && desc->hi == highs[k]);
    }
}

static void reuse(void)
{
    AzConfig cfg = az_test_config(11, 0);
    cfg.cap = 64;
    AzArena world_arena = {0};
    lock_arena(&world_arena, &cfg);
    AzWorld *w = az_world_create(&world_arena, &cfg, NULL);
    CHECK(w != NULL);
    if (!w) { az_win_arena_free(&world_arena); return; }
    u32 slot = az_spawn(w);
    CHECK(slot != AZ_NONE);
    if (slot != AZ_NONE) {
        u64 original_uid = w->uid[slot];
        w->travel_carry[slot] = 301;
        w->phys_passive[3u*slot] = -123;
        w->phys_quote[slot] = 765;
        w->flags[slot] = 0;
        az_grid_build(w);
        CHECK_EQ(az_spawn(w), slot);
        CHECK(w->uid[slot] != original_uid);
        CHECK_EQ(w->travel_carry[slot], 0);
        CHECK_EQ(w->phys_passive[3u*slot], 0);
        CHECK_EQ(w->phys_quote[slot], 0);
    }
    az_win_arena_free(&world_arena);
}

#ifdef AZ_TEST_HAS_SNAPSHOT
static void restart(void)
{
    AzConfig cfg = az_test_config(47, 48);
    cfg.cap = 128;
    AzArena world_arena = {0};
    lock_arena(&world_arena, &cfg);
    AzWorld *w = az_world_create(&world_arena, &cfg, NULL);
    CHECK(w != NULL);
    if (!w) { az_win_arena_free(&world_arena); return; }
    az_worldgen(w);
    CHECK(w->n_slots > 0);
    if (!w->n_slots) { az_win_arena_free(&world_arena); return; }
    /* Normalize the grid's persistent initiator flags before isolating the permutation. */
    az_grid_build(w);
    for (u32 s = 0; s < w->n_slots; s++)
        w->travel_carry[s] = 113u * (u32)w->uid[s];
    const u64 state = az_state_hash(w);
    w->phys_quote[0] = 765;
    w->phys_passive[0] = -231; w->phys_passive[1] = 127; w->phys_passive[2] = 1007;
    CHECK_EQ(az_state_hash(w), state);
    w->travel_carry[0] ^= 1;
    CHECK(az_state_hash(w) != state);
    w->travel_carry[0] ^= 1;
    const u64 size = az_snapshot_bytes(w);
    u8 *buf = malloc((size_t)size);
    CHECK(buf && size && az_snapshot_save(w, buf, size) == size);
    if (!buf || !size) { free(buf); az_win_arena_free(&world_arena); return; }
    CHECK_EQ(buf[12], 9);
    AzArena arena = {0};
    lock_arena(&arena, &w->cfg);
    AzWorld *copy = az_snapshot_load(&arena, buf, size, NULL);
    CHECK(copy != NULL);
    if (copy) {
        CHECK_EQ(az_state_hash(copy), state);
        CHECK_EQ(copy->phys_quote[0], 0);
        CHECK_EQ(copy->phys_passive[0], 0);
        CHECK_EQ(copy->phys_passive[1], 0);
        CHECK_EQ(copy->phys_passive[2], 0);
        const u32 slots = copy->n_slots;
        u64 *previous_uids = malloc((size_t)slots * sizeof(*previous_uids));
        CHECK(previous_uids != NULL);
        if (previous_uids) {
            memcpy(previous_uids, copy->uid, (size_t)slots * sizeof(*previous_uids));
            az_grid_reorder(copy);
            bool moved = false;
            for (u32 s = 0; s < copy->n_slots; s++) {
                moved |= copy->uid[s] != previous_uids[s];
                CHECK_EQ(copy->travel_carry[s], 113u * (u32)copy->uid[s]);
            }
            CHECK(moved);
            free(previous_uids);
        }
        CHECK_EQ(az_state_hash(copy), state);
        for (u32 tick = 0; tick < 18; tick++) {
            az_tick(w);
            az_tick(copy);
            CHECK_EQ(az_state_hash(copy), az_state_hash(w));
            CHECK_EQ(az_total_matter(copy), az_total_matter(w));
            CHECK_EQ(az_total_energy(copy), az_total_energy(w));
        }
    }
    az_win_arena_free(&arena); free(buf); az_win_arena_free(&world_arena);
}
#endif

int main(void)
{
    descriptors();
    reuse();
#ifdef AZ_TEST_HAS_SNAPSHOT
    restart();
#endif
    return az_test_done();
}
