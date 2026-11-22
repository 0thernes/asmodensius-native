/* Strict Windows allocation for older fixtures that keep an opaque block. */
#ifndef AZ_TEST_LOCKED_BLOCK_H
#define AZ_TEST_LOCKED_BLOCK_H
#include "az_test.h"

static inline void *az_test_locked_block(u64 bytes)
{
    AzArena arena = {0};
    AzWinInfo info = {0};
    if (!az_win_arena(&arena, bytes, AZ_WIN_LOCK, &info) || !info.arena_locked) {
        fprintf(stderr, "world test requires locked arena: %s\n", info.note);
        exit(2);
    }
    return arena.base;
}

static inline void az_test_locked_block_free(void *block)
{
    AzArena arena = {(u8 *)block, 0, 0};
    az_win_arena_free(&arena);
}
#endif
