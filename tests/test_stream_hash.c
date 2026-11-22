/* The observer's reduction must equal the canonical serial state hash before
 * and after changes to every additive component, for each worker count. */
#include "az_test.h"
#include "../tools/az_stream_hash.h"

static u64 equivalent(AzWorld *w, StreamHasher *h)
{
    u64 reference = az_state_hash(w);
    CHECK(stream_state_hash(h, w) == reference);
    return reference;
}

int main(void)
{
    AzWinInfo info = {0};
    AzConfig cfg = az_test_config(7, 2000);
    AzHost *host = az_win_host_create(2, 0, &info);
    AzArena arena = {0};
    if (!host || !az_win_arena(&arena, az_world_bytes(&cfg), AZ_WIN_LOCK, &info)) return 1;
    AzWorld *w = az_world_create(&arena, &cfg, host);
    if (!w) return 1;
    az_worldgen(w);
    const u32 counts[] = {1, 2, 8};
    for (size_t j = 0; j < AZ_LEN(counts); j++) {
        StreamHasher h = {0};
        stream_hasher_open(&h, counts[j]);
        CHECK(h.workers == counts[j]);
        for (u32 tick = 0; tick < 4; tick++) {
            az_tick(w);
            u64 original = equivalent(w, &h);
            u32 s = 0;
            while (s < w->n_slots && !(w->flags[s] & AZ_F_ALIVE)) s++;
            CHECK(s < w->n_slots);
            if (s == w->n_slots) return 1;
            w->pos_x[s] ^= 1;
            CHECK(equivalent(w, &h) != original);
            w->pos_x[s] ^= 1;
            AzBlk *block = az_blk(w, s);
            u8 *last_byte = (u8 *)block + ((size_t)block->units << 6) - 1;
            *last_byte ^= 1;
            CHECK(equivalent(w, &h) != original);
            *last_byte ^= 1;
            w->c_light[w->n_cells-1] ^= 1;
            CHECK(equivalent(w, &h) != original);
            w->c_light[w->n_cells-1] ^= 1;
            w->c_acc[0].delta[0] ^= 1;
            CHECK(equivalent(w, &h) != original);
            w->c_acc[0].delta[0] ^= 1;
            w->clades[0].body[0] ^= 1;
            CHECK(equivalent(w, &h) != original);
            w->clades[0].body[0] ^= 1;
            w->param[0] ^= 1;
            CHECK(equivalent(w, &h) != original);
            w->param[0] ^= 1;
            CHECK(equivalent(w, &h) == original);
        }
        stream_hasher_close(&h);
    }
    az_win_arena_free(&arena);
    az_win_host_destroy(host);
    return az_test_done();
}
