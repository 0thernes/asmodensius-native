/* Test conserved heat transfers with wide pending balances and a u32 field. */
#include "az_test.h"

enum { SOURCE_COUNT = 8 };

static void transfers(AzWorld *w, u32 begin, u32 end, u32 worker)
{
    for (u32 i = begin; i < end; i++) {
        u32 src = i + 1;
        az_cell_post_heat(w, worker, src, -(1 << 30));
        az_cell_post_heat(w, worker, 0, 1 << 30);
    }
}

static void test_transfers(u32 workers)
{
    AzConfig cfg = az_test_config(17, 0);
    cfg.cap = 64;
    cfg.gx_bits = cfg.gy_bits = 2;
    cfg.gz_bits = 1;
    AzTestWorld t;
    CHECK(az_test_world_ex(&t, &cfg, workers, AZ_TEST_NO_WORLDGEN | AZ_TEST_UNPINNED));
    if (!t.w) return;
    AzWorld *w = t.w;
    for (u32 i = 1; i <= SOURCE_COUNT; i++) w->c_heat[i] = UINT32_MAX;
    w->c_heat[0] = 37;
    i64 energy = az_total_energy(w);
    u64 matter = az_total_matter(w);
    u32 splits[SOURCE_COUNT + 1];
    for (u32 i = 0; i <= SOURCE_COUNT; i++) splits[i] = i;
    if (workers) t.host->parallel_for(t.host->ctx, w, transfers, splits, SOURCE_COUNT);
    else transfers(w, 0, SOURCE_COUNT, 0);
    CHECK_EQ(az_pending_heat(&w->c_acc[0]), INT64_C(8589934592));
    CHECK_EQ(az_total_energy(w), energy);
    az_field_commit(w, 0, w->n_cells, 0);
    CHECK_EQ(w->c_heat[0], UINT32_MAX);
    CHECK_EQ(az_pending_heat(&w->c_acc[0]), INT64_C(4294967334));
    CHECK_EQ(az_total_energy(w), energy);
    /* Return the full eight funded transfers. Each commit sees at most one
     * quarter-field withdrawal, then consumes any remaining positive reserve. */
    for (u32 i = 1; i <= SOURCE_COUNT; i++) {
        az_cell_post_heat(w, 0, 0, -(1 << 30));
        az_cell_post_heat(w, 0, i, 1 << 30);
        CHECK_EQ(az_total_energy(w), energy);
        az_field_commit(w, 0, w->n_cells, 0);
        CHECK_EQ(az_total_energy(w), energy);
        CHECK_EQ(w->c_heat[i], UINT32_MAX);
    }
    CHECK_EQ(w->c_heat[0], 37);
    CHECK_EQ(az_pending_heat(&w->c_acc[0]), 0);
    /* More than 2^31 is removed from a funded u32 field in one barrier. */
    az_cell_post_heat(w, 0, 1, INT32_MIN);
    az_cell_post_heat(w, 0, 1, -1);
    az_cell_post_heat(w, 0, 13, INT32_MAX);
    az_cell_post_heat(w, 0, 13, 2);
    CHECK_EQ(az_pending_heat(&w->c_acc[1]), -INT64_C(2147483649));
    CHECK_EQ(az_pending_heat(&w->c_acc[13]), INT64_C(2147483649));
    CHECK_EQ(az_total_energy(w), energy);
    az_field_commit(w, 0, w->n_cells, 0);
    CHECK_EQ(w->c_heat[1], UINT32_C(2147483646));
    CHECK_EQ(w->c_heat[13], UINT32_C(2147483649));
    CHECK_EQ(az_total_energy(w), energy);
    /* A real overdraw: leave the debt and report the defect. */
    az_cell_post_heat(w, 0, 0, -38);
    az_cell_post_heat(w, 0, 12, 38);
    CHECK_EQ(az_total_energy(w), energy);
    az_field_commit(w, 0, w->n_cells, 0);
    CHECK_EQ(w->c_heat[0], 37);
    CHECK_EQ(az_pending_heat(&w->c_acc[0]), -38);
    CHECK_EQ(w->workers[0].tally.field_underflow, 1);
    az_cell_post_heat(w, 0, 0, 1);
    az_cell_post_heat(w, 0, 12, -1);
    az_field_commit(w, 0, w->n_cells, 0);
    CHECK_EQ(w->c_heat[0], 0);
    CHECK_EQ(az_pending_heat(&w->c_acc[0]), 0);
    CHECK_EQ(az_total_energy(w), energy);
    CHECK_EQ(az_total_matter(w), matter);
    CHECK_EQ(w->workers[0].tally.field_underflow, 1);
    az_test_world_free(&t);
}

static void test_limbs(void)
{
    static const i64 values[] = {0, 1, -1, INT32_MIN, INT32_MAX, INT64_C(2147483648),
        -INT64_C(2147483649), INT64_C(4294967296), -INT64_C(4294967296), INT64_MAX, INT64_MIN};
    AzCellAcc acc = {0};
    for (u32 i = 0; i < AZ_LEN(values); i++) {
        az_set_pending_heat(&acc, values[i]);
        CHECK_EQ(az_pending_heat(&acc), values[i]);
        if (values[i] >= INT32_MIN && values[i] <= INT32_MAX) CHECK_EQ(acc.heat_carry, 0);
    }
}

int main(void)
{
    test_limbs();
    test_transfers(0);
    test_transfers(3);
    test_transfers(8);
    return az_test_done();
}
