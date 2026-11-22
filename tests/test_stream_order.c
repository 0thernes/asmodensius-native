/* Exercise UID ranges and vertical terrain that a short live run rarely reaches. */
#include "az_test.h"
#include "../tools/az_stream_capture.h"

static void check_sorted(AzWorld *world, StreamCapture *buffers, u64 *uids,
                         const u32 *source_slots, u32 count)
{
    world->uid = uids;
    CHECK(stream_creatures(world, buffers) == count);
    const AzStreamRecord *records = (const AzStreamRecord *)(buffers->snapshot + AZST_HEADER_BYTES);
    for (u32 i = 0; i < count; i++) {
        u32 slot = source_slots[i];
        CHECK(records[i].uid == uids[slot]);
        CHECK(records[i].x == world->pos_x[slot] >> 16);
        CHECK(records[i].hue == world->tag[slot] >> 8);
        CHECK(records[i].flags == world->flags[slot]);
    }
}

int main(void)
{
    AzWorld *world = az_test_shell();
    if (!world) return 1;
    CHECK((uintptr_t)world % _Alignof(AzWorld) == 0);
    u32 xyz[] = {1u<<16, 2u<<16, 3u<<16, 4u<<16, 5u<<16};
    u16 tags[] = {0x0102, 0x0304, 0x0506, 0x0708, 0x090a};
    u8 alive[] = {1, 0, 1, 1, 1};
    i16 rotation[5] = {0};
    i8 light[4] = {0};
    world->gx = world->gy = world->gz = world->n_cells = 1;
    world->n_slots = 5; world->pos_x = world->pos_y = world->pos_z = xyz;
    world->mass = world->clade = xyz; world->flags = alive; world->tag = tags;
    world->rot_b = world->rot_c = rotation; world->c_lvec = light;
    StreamCapture b = {0};
    b.snapshot = malloc(AZST_HEADER_BYTES + 5 * sizeof(AzStreamRecord));
    b.scratch = malloc(5 * sizeof(AzStreamRecord));
    b.buckets = malloc(65536 * sizeof(u32));
    b.index_count = 10;
    b.uid_index = malloc(b.index_count * sizeof *b.uid_index);
    if (!b.snapshot || !b.scratch || !b.buckets || !b.uid_index) return 1;
    u64 dense[] = {UINT64_MAX, 0, UINT64_MAX - 2, UINT64_MAX - 5, UINT64_MAX - 1};
    const u32 dense_order[] = {3, 2, 4, 0};
    check_sorted(world, &b, dense, dense_order, 4);
    u64 sparse[] = {UINT64_C(0xffff000000000012), 0, 1, UINT64_C(0x100000001), UINT64_C(0x100000000)};
    const u32 sparse_order[] = {2, 4, 3, 0};
    check_sorted(world, &b, sparse, sparse_order, 4);
    memset(alive, 0, sizeof alive);
    check_sorted(world, &b, sparse, NULL, 0);
    char inspect[1500];
    stream_inspect(world, 1, inspect, sizeof inspect);
    CHECK(strstr(inspect, "\"found\":false") != NULL);

    enum { HEIGHT = 512 };
    u8 rock[HEIGHT] = {0};
    rock[HEIGHT-1] = 255;
    world->gz = world->n_cells = HEIGHT;
    world->c_solid = rock;
    u32 bytes, h = 0;
    u8 *terrain = stream_static(world, &bytes);
    CHECK(terrain != NULL);
    if (terrain) {
        CHECK(bytes == 32 + HEIGHT + 4);
        memcpy(&h, terrain + 32 + HEIGHT, sizeof h);
        CHECK(h == 256 * HEIGHT);
        free(terrain);
    }
    world->c_phase = calloc(HEIGHT, sizeof *world->c_phase);
    world->c_light = calloc(HEIGHT, sizeof *world->c_light);
    world->c_temp = calloc(HEIGHT, sizeof *world->c_temp);
    world->c_sp = calloc(HEIGHT * AZ_NSPECIES, sizeof *world->c_sp);
    if (!world->c_phase || !world->c_light || !world->c_temp || !world->c_sp) return 1;
    world->c_phase[HEIGHT-1] = AZ_PH_LIQUID;
    world->c_sp[(HEIGHT-1) * AZ_NSPECIES + AZ_SP_SOLV] = 1;
    u8 *fields = stream_fields(world, &bytes);
    CHECK(fields != NULL);
    if (fields) {
        CHECK(bytes == 32 + 4 + HEIGHT * 4);
        memcpy(&h, fields + 32, sizeof h);
        CHECK(h == HEIGHT * 256);
        free(fields);
    }
    free(world->c_phase); free(world->c_light); free(world->c_temp); free(world->c_sp);
    free(b.uid_index); free(b.scratch); free(b.buckets); free(b.snapshot);
    az_test_shell_free(world);
    return az_test_done();
}
