/* test_weather.c - core/az_weather.c: suns, light, heat, wind, storms and vents.
 *
 *   1. suns     where each sun stands and what it gives, at the points where the sine is exact;
 *               the season; the light vector; a top cell follows the sky through a day and is
 *               dark in the night; under AZ_LESION_WEATHER it keeps the light it had
 *   2. depth    light falls from layer to layer by what is in each cell (checked against the sum
 *               of optical depths worked out by hand), organic matter darkens what is under it,
 *               bedrock lets nothing through, half a cell of rock about half
 *   3. heat     a hot cell warms its open neighbours and not the bedrock one, with the heat total
 *               unchanged; a lifeless world under its suns: the books hold every tick against the
 *               tallied input and output, the mean temperature stays in a band, the top is warmer
 *               than the deep and warmer by day than at the end of the night
 *   4. vents    the reservoir loses exactly what the cells gain, never more than it holds; they
 *               erupt; a vent under bedrock is shut; a vent warms its cell
 *   5. storms   storms start, grow to their peak, move and end; under them the charge rises, the
 *               wind turns and lightning turns gas into mineral, unit for unit, with exact books;
 *               a sky with no storm does none of it
 *   6. chaos    the same seed gives the same sky; one unit of difference in one variable gives
 *               another sky; another seed another; the cloud cover of a day is not that of the
 *               day 62,000 ticks later, when both suns stand exactly where they stood
 *   7. worlds   living worlds: one hash on 0, 1, 3 and 8 workers, books exact every tick, the
 *               lesion gives another world
 *
 * Seeds 71 to 78 (the tuning block).
 */
#include "az_test.h"

enum { WG_X = 7, WG_Y, WG_Z, WG_CLOUD, WG_SKY, WG_STAGE, WG_PEAK, WG_CX, WG_CY, WG_LX, WG_LY, WG_LZ };   /* az_weather.c's own words */
static i32 G(const AzWorld *w, u32 i) { return w->gstate[AZ_G_WEATHER + i]; }
static u32 P(const AzWorld *w, u32 id) { return w->param[id]; }
static u32 *sp(const AzWorld *w, u32 c) { return &w->c_sp[(size_t)c * AZ_NSPECIES]; }

static bool lifeless(AzTestWorld *t, u64 seed, u32 gz_bits, u32 lesion)
{
    AzConfig cfg = az_test_config(seed, 0);
    cfg.gz_bits = (u8)gz_bits;
    cfg.lesion = lesion;
    if (!az_test_world_ex(t, &cfg, 0, AZ_TEST_NO_WORLDGEN)) return false;
    az_test_uniform(t->w);              /* the checks of these parts pin the uniform world (az_test.h) */
    return true;
}
/* the energy audit: what is in the world, less what came in, plus what went out (az_world.c);
 * constant when the books are exact */
static i64 books(const AzWorld *w) { return az_total_energy(w); }
static i64 heat_sum(const AzWorld *w)                    /* in the cells, with what is on its way */
{
    i64 h = 0;
    for (u32 c = 0; c < w->n_cells; c++) h += (i64)w->c_heat[c] + az_pending_heat(&w->c_acc[c]);
    return h;
}
/* no sun, no sky to radiate to, and the light that worldgen left taken out of the cells */
static void dark(AzWorld *w)
{
    w->param[AZ_WP_WEATHER_SUN0_LIGHT] = 0; w->param[AZ_WP_WEATHER_SUN1_LIGHT] = 0; w->param[AZ_WP_WEATHER_COOL] = 0;
    az_weather_global(w);
    for (u32 c = w->n_cells; c-- > 0u;) az_weather_derive(w, c);
}
static u64 sum_species(const AzWorld *w, u32 k)          /* in the cells, with what is on its way */
{
    i64 n = 0;
    for (u32 c = 0; c < w->n_cells; c++) n += (i64)sp(w, c)[k] + (i64)w->c_acc[c].delta[k];
    return (u64)n;
}
static i64 layer_temp(const AzWorld *w, u32 z)           /* mean temperature of one layer */
{
    i64 t = 0;
    for (u32 y = 0; y < w->gy; y++)
        for (u32 x = 0; x < w->gx; x++) t += w->c_temp[az_cell_xyz(w, x, y, z)];
    return t / (i64)(w->gx * w->gy);
}
static void derive_all(AzWorld *w)                       /* from the top layer down */
{
    for (u32 c = w->n_cells; c-- > 0u;) az_weather_derive(w, c);
}

/* ---- 1: the suns -------------------------------------------------------------------- */
static void test_suns(void)
{
    AzTestWorld t;
    if (!lifeless(&t, 71, 3, 0)) { CHECK(!"world"); return; }
    AzWorld *w = t.w;
    CHECK_EQ(w->sun_period[0], 2000u); CHECK_EQ(w->sun_period[1], 3100u);
    CHECK_EQ(G(w, AZ_G_SUN0_PHASE), (i32)P(w, AZ_WP_WEATHER_SUN0_START));     /* worldgen called the module once */
    w->param[AZ_WP_WEATHER_SUN0_START] = 0; w->param[AZ_WP_WEATHER_SUN1_START] = 0; w->param[AZ_WP_WEATHER_CLOUD_DIM] = 0;
    w->tick = 0; az_weather_global(w);                                        /* dawn of both */
    CHECK_EQ(G(w, AZ_G_SUN0_PHASE), 0); CHECK_EQ(G(w, AZ_G_SUN0_LIGHT), 0); CHECK_EQ(G(w, AZ_G_SUN1_LIGHT), 0);
    CHECK_EQ(G(w, WG_SKY), 0); CHECK_EQ(G(w, WG_LX), 0); CHECK_EQ(G(w, WG_LZ), 0);
    w->tick = 250; az_weather_global(w);                                      /* sun 0 half way up */
    CHECK_EQ(G(w, AZ_G_SUN0_PHASE), 8192); CHECK_EQ(G(w, AZ_G_SUN0_LIGHT), 141);
    w->tick = 500; az_weather_global(w);                                      /* noon of sun 0 */
    CHECK_EQ(G(w, AZ_G_SUN0_PHASE), 16384); CHECK_EQ(G(w, AZ_G_SUN0_LIGHT), 200);
    CHECK_EQ(G(w, AZ_G_SUN1_PHASE), 10570); CHECK_EQ(G(w, AZ_G_SUN1_LIGHT), 101);
    CHECK_EQ(G(w, AZ_G_SEASON), 16384 - 10570); CHECK_EQ(G(w, WG_SKY), 255);
    w->tick = 775; az_weather_global(w);                                      /* noon of sun 1 */
    CHECK_EQ(G(w, AZ_G_SUN1_PHASE), 16384); CHECK_EQ(G(w, AZ_G_SUN1_LIGHT), 120);
    w->tick = 1000; az_weather_global(w);                                     /* dusk of sun 0 */
    CHECK_EQ(G(w, AZ_G_SUN0_PHASE), 32768); CHECK_EQ(G(w, AZ_G_SUN0_LIGHT), 0);
    w->tick = 1700; az_weather_global(w);                                     /* night of both */
    CHECK_EQ(G(w, AZ_G_SUN0_LIGHT), 0); CHECK_EQ(G(w, AZ_G_SUN1_LIGHT), 0); CHECK_EQ(G(w, WG_SKY), 0);
    CHECK_EQ(G(w, WG_LX), 0); CHECK_EQ(G(w, WG_LY), 0); CHECK_EQ(G(w, WG_LZ), 0);
    w->tick = 2000; az_weather_global(w);                                     /* the next dawn */
    CHECK_EQ(G(w, AZ_G_SUN0_PHASE), 0);
    /* the light vector: sun 0 alone, half way up along x, then overhead; sun 1 alone, along y */
    w->param[AZ_WP_WEATHER_SUN1_LIGHT] = 0;
    w->tick = 250; az_weather_global(w);
    CHECK_EQ(G(w, WG_LX), 89); CHECK_EQ(G(w, WG_LY), 0); CHECK_EQ(G(w, WG_LZ), 89);
    w->tick = 500; az_weather_global(w);
    CHECK_EQ(G(w, WG_LX), 0); CHECK_EQ(G(w, WG_LZ), 127);
    w->param[AZ_WP_WEATHER_SUN1_LIGHT] = 120; w->param[AZ_WP_WEATHER_SUN0_LIGHT] = 0;
    w->sun_period[1] = 4000;
    w->tick = 1500; az_weather_global(w);                                     /* sun 1 three quarters through its day's light */
    CHECK_EQ(G(w, AZ_G_SUN1_PHASE), 24576);
    CHECK_EQ(G(w, WG_LX), 0); CHECK_EQ(G(w, WG_LY), -89); CHECK_EQ(G(w, WG_LZ), 89);
    w->param[AZ_WP_WEATHER_SUN0_START] = 16384; w->tick = 0; az_weather_global(w);
    CHECK_EQ(G(w, AZ_G_SUN0_PHASE), 16384);                                   /* the start is a parameter */
    az_test_world_free(&t);

    /* a top cell through a day of real ticks, in the intact world and under the lesion */
    u32 lo[2] = { 255, 255 }, hi[2] = { 0, 0 }, night[2] = { 0, 0 }, lit_deep = 0;
    for (u32 v = 0; v < 2u; v++) {
        if (!lifeless(&t, 71, 3, v ? AZ_LESION_WEATHER : 0u)) { CHECK(!"world"); return; }
        w = t.w;
        w->param[AZ_WP_WEATHER_CLOUD_DIM] = 0;
        const u32 top = az_cell_xyz(w, 3, 5, w->gz - 1u), deep = az_cell_xyz(w, 3, 5, 0);
        for (u32 i = 0; i < 2000u; i++) {
            az_tick(w);
            const u32 l = w->c_light[top];
            if (l < lo[v]) lo[v] = l;
            if (l > hi[v]) hi[v] = l;
            if (i >= 1600u && i < 1740u && l > night[v]) night[v] = l;    /* both suns are down from tick 1550 to 1750 */
            if (!v && w->c_light[deep] > lit_deep) lit_deep = w->c_light[deep];
            if (!v) CHECK(w->c_light[deep] <= l + 12u);                     /* the deep is never brighter than the top was a moment ago */
        }
        CHECK_EQ(w->totals.field_underflow, 0u);
        az_test_world_free(&t);
    }
    CHECK_EQ(lo[0], 0u); CHECK_EQ(hi[0], 255u); CHECK_EQ(night[0], 0u); CHECK(lit_deep > 100u);
    CHECK_EQ(lo[1], hi[1]); CHECK(lo[1] > 0u);
    printf("suns: light of a top cell through a day 0 to %u (0 in the night; the deepest of 8 layers up to %u); "
           "under the lesion it stays %u\n", hi[0], lit_deep, lo[1]);
}

/* ---- 2: light with depth ------------------------------------------------------------- */
static void test_depth(void)
{
    AzTestWorld t;
    if (!lifeless(&t, 72, 4, 0)) { CHECK(!"world"); return; }
    AzWorld *w = t.w;
    w->param[AZ_WP_WEATHER_SUN0_START] = 0; w->param[AZ_WP_WEATHER_SUN1_START] = 0; w->param[AZ_WP_WEATHER_CLOUD_DIM] = 0;
    w->tick = 500; az_weather_global(w);
    CHECK_EQ(G(w, WG_SKY), 255);
    const u32 top = w->gz - 1u;
    /* the cell the stub world is made of: 1,000 solvent, 400 organic, 200 oxidiser */
    const u32 *s0 = sp(w, az_cell_xyz(w, 3, 5, top));
    CHECK_EQ(s0[AZ_SP_SOLV], 1000u); CHECK_EQ(s0[AZ_SP_ORG_Y], 400u); CHECK_EQ(s0[AZ_SP_OXI], 200u);
    const u32 depth = (1000u * P(w, AZ_WP_WEATHER_DIM_SOLV) + 400u * P(w, AZ_WP_WEATHER_DIM_ORG) + 200u * P(w, AZ_WP_WEATHER_DIM_OTHER)) >> 16;
    CHECK_EQ(depth, 11u);                                   /* 11/256 of a halving per layer: 3 per cent */
    const u32 pass = az_exp2_q8(4096u - depth);
    sp(w, az_cell_xyz(w, 3, 5, 12))[AZ_SP_ORG_Y] += 20000u; /* a bloom, or what is left of one */
    w->c_solid[az_cell_xyz(w, 4, 5, 10)] = 255;             /* bedrock */
    w->c_solid[az_cell_xyz(w, 5, 5, 10)] = 128;             /* half a cell of rock */
    derive_all(w);
    u32 l = 255, steps_down = 0;
    for (u32 z = top + 1u; z-- > 0u;) {                     /* an open column */
        CHECK_EQ(w->c_light[az_cell_xyz(w, 9, 9, z)], l);
        if (z < top) steps_down += w->c_light[az_cell_xyz(w, 9, 9, z)] < w->c_light[az_cell_xyz(w, 9, 9, z + 1u)];
        l = (u32)(((u64)l * pass) >> 16);
    }
    CHECK_EQ(steps_down, top);
    const u32 bottom = w->c_light[az_cell_xyz(w, 9, 9, 0)];
    const u32 l12 = w->c_light[az_cell_xyz(w, 3, 5, 12)], l11 = w->c_light[az_cell_xyz(w, 3, 5, 11)];
    CHECK_EQ(l12, (u32)w->c_light[az_cell_xyz(w, 9, 9, 12)]);
    CHECK_EQ(l11, (u32)(((u64)l12 * az_exp2_q8(4096u - 480u)) >> 16));   /* (128,000 + 20,400 x 1,536 + 25,600) / 65,536 = 480 */
    CHECK(l11 * 3u < l12);
    CHECK_EQ(w->c_light[az_cell_xyz(w, 4, 5, 11)], (u32)w->c_light[az_cell_xyz(w, 9, 9, 11)]);
    for (u32 z = 0; z <= 10u; z++) CHECK_EQ(w->c_light[az_cell_xyz(w, 4, 5, z)], 0u);
    CHECK_EQ(w->c_lvec[(size_t)az_cell_xyz(w, 4, 5, 9) * 4 + 2], 0);
    CHECK(w->c_lvec[(size_t)az_cell_xyz(w, 9, 9, 9) * 4 + 2] > 100);        /* both suns are high */
    const u32 h10 = w->c_light[az_cell_xyz(w, 5, 5, 10)];
    CHECK_EQ(h10, (u32)w->c_light[az_cell_xyz(w, 9, 9, 10)]);
    CHECK_EQ(w->c_light[az_cell_xyz(w, 5, 5, 9)], (u32)(((u64)h10 * ((u64)pass * 127u / 255u)) >> 16));
    printf("depth: of 255 at the top %u reach the lowest of 16 layers; under a cell with 20,000 more organic "
           "matter %u of %u; under bedrock 0; under half a cell of rock %u of %u\n",
           bottom, l11, l12, w->c_light[az_cell_xyz(w, 5, 5, 9)], h10);
    az_test_world_free(&t);
}

/* ---- 3: heat -------------------------------------------------------------------------- */
static void test_heat(void)
{
    AzTestWorld t;
    if (!lifeless(&t, 73, 3, AZ_LESION_CHEM)) { CHECK(!"world"); return; }     /* weather alone: chemistry frees heat too (test_chem) */
    AzWorld *w = t.w;
    /* (a) no sun and no sky: only conduction */
    dark(w);
    const u32 a = az_cell_xyz(w, 8, 8, 4), rock = az_cell_xyz(w, 9, 8, 4), far = az_cell_xyz(w, 8, 10, 4);
    const u32 base = w->c_heat[far];
    w->c_heat[a] += 10000u;
    w->c_solid[rock] = 255;
    const i64 h0 = heat_sum(w);
    const i64 e0 = az_total_energy(w), in0 = w->totals.energy_in, out0 = w->totals.energy_out;
    u32 wrong = 0;
    for (u32 i = 0; i < 40u; i++) { az_tick(w); wrong += az_total_energy(w) != e0; }
    CHECK_EQ(wrong, 0u);
    CHECK_EQ(w->totals.energy_in, in0); CHECK_EQ(w->totals.energy_out, out0);
    CHECK_EQ(heat_sum(w), h0);
    CHECK(w->c_heat[a] < base + 10000u); CHECK(w->c_heat[a] > base);
    static const i32 side[5][3] = { { -1, 0, 0 }, { 0, -1, 0 }, { 0, 1, 0 }, { 0, 0, -1 }, { 0, 0, 1 } };
    for (u32 f = 0; f < 5u; f++) CHECK(w->c_heat[az_cell_nb(w, a, side[f][0], side[f][1], side[f][2])] > base);
    CHECK_EQ(w->c_heat[rock], base);
    CHECK(w->c_heat[far] > base);                           /* two cells away, after 40 ticks */
    CHECK(w->c_temp[a] > w->c_temp[az_cell_nb(w, a, -1, 0, 0)]);
    CHECK_EQ(w->totals.field_underflow, 0u);
    printf("heat: of 10,000 eu put into one cell %u are left after 40 ticks, the neighbour has %u, the cell "
           "two away %u, the bedrock one 0; the total is unchanged\n",
           w->c_heat[a] - base, w->c_heat[az_cell_nb(w, a, -1, 0, 0)] - base, w->c_heat[far] - base);
    az_test_world_free(&t);

    /* (b) a lifeless world of 32 layers under its suns, for 2,000 ticks */
    if (!lifeless(&t, 73, 5, 0)) { CHECK(!"world"); return; }
    w = t.w;
    w->param[AZ_WP_WEATHER_SUN0_START] = 0;
    const i64 b0 = books(w);
    const u32 top = w->gz - 1u;
    i64 lo = 32767, hi = 0, noon = 0, dawn = 0, deep_lo = 32767, deep_hi = 0;
    wrong = 0;
    for (u32 i = 0; i < 2000u; i++) {
        az_tick(w);
        wrong += books(w) != b0;
        if (i % 50u != 49u) continue;
        i64 mean = 0;
        for (u32 z = 0; z <= top; z++) mean += layer_temp(w, z);
        mean /= (i64)w->gz;
        if (mean < lo) lo = mean;
        if (mean > hi) hi = mean;
        const i64 d = layer_temp(w, 0);
        if (d < deep_lo) deep_lo = d;
        if (d > deep_hi) deep_hi = d;
        if (i == 599u) noon = layer_temp(w, top);
        if (i == 1999u) dawn = layer_temp(w, top);
    }
    CHECK_EQ(wrong, 0u);
    CHECK(w->totals.energy_in > 0); CHECK(w->totals.energy_out > 0);
    CHECK(lo > 7600 && hi < 9300);                          /* the worldgen temperature is 8192 */
    CHECK(noon > dawn + 300);
    CHECK(noon > deep_hi + 300);
    CHECK(deep_hi - deep_lo < noon - dawn);
    CHECK_EQ(w->totals.field_underflow, 0u);
    printf("heat: lifeless world of 32 layers, 2,000 ticks: mean temperature %lld to %lld; the top layer %lld "
           "soon after noon and %lld at the end of the night, the lowest %lld to %lld; %lld eu in, %lld out, "
           "books exact every tick\n", (long long)lo, (long long)hi, (long long)noon, (long long)dawn,
           (long long)deep_lo, (long long)deep_hi, (long long)w->totals.energy_in, (long long)w->totals.energy_out);
    az_test_world_free(&t);
}

/* ---- 4: vents ------------------------------------------------------------------------- */
/* Adjacent accepted settings around the old all-or-nothing cutoff, then
 * the maximum allowed single vent. The thermal input and both stores are
 * checked independently of the books, which also balance if both are lost. */
static void test_vent_heat_range(void)
{
    const u32 amounts[] = {262143, 262144, 1114112};
    for (u32 i = 0; i < AZ_LEN(amounts); i++) {
        const u32 q = amounts[i], base = q > 1048576 ? q - 1048576 : 0;
        AzTestWorld t;
        AzConfig cfg = az_test_config(11, 0);
        if (!az_test_world_ex(&t, &cfg, 0, AZ_TEST_NO_WORLDGEN)) { CHECK(!"world"); return; }
        AzWorld *w = t.w;
        CHECK(az_param_set(w, AZ_WP_WEATHER_VENT_RATE, base));
        CHECK(az_param_set(w, AZ_WP_WEATHER_VENT_PULSE, q - base));
        CHECK(az_param_set(w, AZ_WP_WEATHER_VENT_PULSE_P, 65536));
        CHECK(az_param_set(w, AZ_WP_WEATHER_VENT_HEAT, 4096));
        w->sun_period[0] = 2000; w->sun_period[1] = 3100;
        w->vent_count = 1; w->vent_cell[0] = 1; w->totals.reservoir_mu = q;
        const i64 heat = (i64)q * 4096, energy = az_total_energy(w);
        const u64 matter = az_total_matter(w);
        az_weather_global(w);
        CHECK_EQ(w->workers[0].tally.energy_in, heat + (i64)q * az_eps_species[AZ_SP_NUT_X]);
        CHECK_EQ(az_pending_heat(&w->c_acc[1]), heat);
        CHECK_EQ(az_total_energy(w), energy);
        az_field_commit(w, 0, w->n_cells, 0);
        CHECK_EQ(w->c_heat[1], heat > UINT32_MAX ? UINT32_MAX : (u32)heat);
        CHECK_EQ(az_pending_heat(&w->c_acc[1]), heat - w->c_heat[1]);
        CHECK_EQ(w->workers[0].tally.field_underflow, 0);
        CHECK_EQ(az_total_energy(w), energy);
        CHECK_EQ(az_total_matter(w), matter);
        az_test_world_free(&t);
    }
}

static void test_vents(void)
{
    AzTestWorld t;
    if (!lifeless(&t, 74, 3, AZ_LESION_CHEM)) { CHECK(!"world"); return; }     /* weather alone: chemistry buries, and the vents then bring more up (test_chem) */
    AzWorld *w = t.w;
    dark(w);                                                /* every unit of energy that comes in is the vents' */
    const u32 v1 = az_cell_xyz(w, 2, 2, 0), v2 = az_cell_xyz(w, 10, 3, 1), shut = az_cell_xyz(w, 5, 5, 0);
    w->c_solid[shut] = 255;
    w->vent_cell[0] = v1; w->vent_cell[1] = v2; w->vent_cell[2] = shut; w->vent_count = 3;
    w->totals.reservoir_mu = 20000;
    w->param[AZ_WP_WEATHER_VENT_PULSE_P] = 3277;            /* an eruption in 20 ticks */
    const u64 m0 = az_total_matter(w);
    const i64 b0 = books(w), in0 = w->totals.energy_in, heat0 = heat_sum(w);
    CHECK_EQ(sum_species(w, AZ_SP_NUT_X), 0u);
    u32 wrong = 0, eruptions = 0, ticks = 0, first = 0;
    u64 before = 0;
    while (w->totals.reservoir_mu && ticks < 20000u) {
        az_tick(w); ticks++;
        const u64 now = sum_species(w, AZ_SP_NUT_X);
        wrong += az_total_matter(w) != m0 || books(w) != b0 || now + w->totals.reservoir_mu != 20000u;
        if (ticks == 1u) first = (u32)now;
        eruptions += now - before >= P(w, AZ_WP_WEATHER_VENT_PULSE);
        before = now;
    }
    CHECK_EQ(wrong, 0u);
    CHECK(first % 512u == 8u);                              /* two open vents at 4 a tick, and whole eruptions */
    CHECK(eruptions > 5u);
    CHECK_EQ(w->totals.reservoir_mu, 0u);
    const i32 warmer = w->c_temp[v1] - w->c_temp[az_cell_xyz(w, 12, 12, 0)];
    CHECK(warmer > 300);                                    /* the mineral comes up hot */
    CHECK_EQ(heat_sum(w) - heat0, 20000 * (i64)P(w, AZ_WP_WEATHER_VENT_HEAT));
    CHECK_EQ(w->totals.energy_in - in0, 20000 * ((i64)P(w, AZ_WP_WEATHER_VENT_HEAT) + (i64)az_eps_species[AZ_SP_NUT_X]));
    CHECK_EQ(w->totals.energy_out, 0);
    for (u32 i = 0; i < 50u; i++) az_tick(w);               /* an empty reservoir gives nothing */
    CHECK_EQ(sum_species(w, AZ_SP_NUT_X), 20000u);
    CHECK_EQ(az_total_matter(w), m0); CHECK_EQ(books(w), b0);
    CHECK_EQ(sp(w, shut)[AZ_SP_NUT_X], 0u);
    CHECK_EQ(sp(w, v1)[AZ_SP_NUT_X] + sp(w, v2)[AZ_SP_NUT_X], 20000u);
    printf("vents: 20,000 mu of the reservoir came up in %u ticks through two vents (%u eruptions; %u and %u mu), "
           "none through the one under bedrock; a vent's cell was %d warmer than its layer; books exact every tick\n",
           ticks, eruptions, sp(w, v1)[AZ_SP_NUT_X], sp(w, v2)[AZ_SP_NUT_X], warmer);
    az_test_world_free(&t);
}

/* ---- 5: storms ------------------------------------------------------------------------ */
typedef struct Sky { u32 storms, ended, peaks, moved, charge, wind, wrong; u64 fixed, gas_lost; } Sky;
static void sky_run(u32 rate, Sky *r)
{
    AzTestWorld t;
    memset(r, 0, sizeof *r);
    if (!lifeless(&t, 75, 3, AZ_LESION_CHEM)) { CHECK(!"world"); return; }    /* weather alone: chemistry would settle some of the mineral as structure */
    AzWorld *w = t.w;
    w->param[AZ_WP_WEATHER_STORM_RATE] = rate;
    for (u32 c = 0; c < w->n_cells; c++) if (az_cell_z(w, c) + 4u >= w->gz) sp(w, c)[AZ_SP_GAS] = 4000;
    derive_all(w);
    const u64 m0 = az_total_matter(w), gas0 = sum_species(w, AZ_SP_GAS);
    const i64 b0 = books(w);
    u32 was = 0, started_at = 0;
    for (u32 i = 0; i < 3000u; i++) {
        az_tick(w);
        r->wrong += az_total_matter(w) != m0 || books(w) != b0;
        const u32 s = (u32)G(w, AZ_G_STORM), cell = (u32)G(w, AZ_G_STORM_CELL);
        if (s && !was) { r->storms++; started_at = cell; }
        if (!s && was) r->ended++;
        if (s && cell != started_at) r->moved = 1;
        if (s && G(w, WG_STAGE) == 2 && s == (u32)G(w, WG_PEAK)) r->peaks++;
        was = s;
        CHECK(s <= 255u && cell < w->n_cells && az_cell_z(w, cell) == w->gz - 1u);
        if (w->c_charge[cell] > r->charge) r->charge = w->c_charge[cell];
        for (u32 c = w->n_cells - w->gx * w->gy; c < w->n_cells && i >= 500u; c++) {   /* the top layer, once the added gas is warm */
            const i8 *v = &w->c_wind[(size_t)c * 3];
            const u32 m = (u32)(v[0] < 0 ? -v[0] : v[0]) + (u32)(v[1] < 0 ? -v[1] : v[1]);
            if (m > r->wind) r->wind = m;
        }
    }
    r->fixed = sum_species(w, AZ_SP_NUT_X);
    r->gas_lost = gas0 - sum_species(w, AZ_SP_GAS);
    CHECK_EQ(w->totals.field_underflow, 0u);
    az_test_world_free(&t);
}
static void test_storms(void)
{
    /* the swirl by hand: a storm of strength 200 over (8, 8) in a world of one temperature turns
     * the wind round its centre, strongest part of the way out, not at all beyond its reach of 6
     * cells or under its depth of 4 layers (planted defect w13 passed before this was checked) */
    AzTestWorld t;
    if (!lifeless(&t, 75, 3, 0)) { CHECK(!"world"); return; }
    AzWorld *w = t.w;
    w->gstate[AZ_G_STORM] = 200; w->gstate[AZ_G_WEATHER + WG_CX] = 8 << 8; w->gstate[AZ_G_WEATHER + WG_CY] = 8 << 8;
    static const struct { u32 x, y, down; i32 wx, wy; } at[] = {
        { 11, 8, 0, 0, 28 }, { 8, 11, 0, -28, 0 }, { 5, 8, 0, 0, -28 }, { 8, 5, 0, 28, 0 },   /* 200 x 27 / 36 = 150; 3 x 150 x 16 / 256 = 28 */
        { 10, 10, 0, -19, 19 },                                                                 /* on the diagonal: 200 x 28 / 36 = 155; 2 x 155 x 16 / 256 = 19 */
        { 8, 8, 0, 0, 0 }, { 14, 8, 0, 0, 0 }, { 11, 8, 3, 0, 28 }, { 11, 8, 4, 0, 0 } };
    for (u32 i = 0; i < sizeof at / sizeof at[0]; i++) {
        const u32 c = az_cell_xyz(w, at[i].x, at[i].y, w->gz - 1u - at[i].down);
        az_weather_cell(w, c, 0);
        CHECK_EQ(w->c_wind[(size_t)c * 3], at[i].wx); CHECK_EQ(w->c_wind[(size_t)c * 3 + 1], at[i].wy);
    }
    CHECK(w->c_charge[az_cell_xyz(w, 11, 8, w->gz - 1u)] > w->c_charge[az_cell_xyz(w, 14, 8, w->gz - 1u)]);
    az_test_world_free(&t);

    Sky wild, calm;
    sky_run(262144, &wild);
    sky_run(0, &calm);
    CHECK_EQ(wild.wrong, 0u); CHECK_EQ(calm.wrong, 0u);
    CHECK(wild.storms >= 3u); CHECK(wild.ended >= 2u); CHECK(wild.peaks >= 2u); CHECK_EQ(wild.moved, 1u);
    CHECK(wild.charge > calm.charge + 50u && wild.charge < 192u);   /* a strike takes the charge down again */
    CHECK(wild.fixed > 0u); CHECK_EQ(wild.fixed, wild.gas_lost);
    CHECK(wild.wind >= 20u);
    CHECK_EQ(calm.storms, 0u); CHECK_EQ(calm.fixed, 0u); CHECK_EQ(calm.gas_lost, 0u);
    CHECK(calm.charge < 192u); CHECK(calm.wind < 8u);
    printf("storms: in 3,000 ticks %u storms started, %u reached their peak and %u ended, their centre moving; "
           "charge under the centre up to %u, wind in the top layer up to %u, lightning fixed %llu mu of gas "
           "as mineral; with no storm: charge up to %u, wind up to %u, nothing fixed\n",
           wild.storms, wild.peaks, wild.ended, wild.charge, wild.wind, (unsigned long long)wild.fixed, calm.charge, calm.wind);
}

/* ---- 6: chaos ------------------------------------------------------------------------- */
static void test_chaos(void)
{
    enum { DAY = 2000, BEAT = 62000, N = 40 };              /* 62,000 ticks: 31 days of sun 0 and 20 of sun 1 */
    AzTestWorld t[4];
    static u8 cloud[3][N];
    for (u32 k = 0; k < 4u; k++) if (!lifeless(&t[k], k == 3u ? 77u : 76u, 3, 0)) { CHECK(!"world"); return; }
    CHECK(G(t[0].w, WG_X) == G(t[1].w, WG_X) && G(t[0].w, WG_Z) == G(t[1].w, WG_Z));
    CHECK(G(t[0].w, WG_X) != G(t[3].w, WG_X));              /* another seed: another start */
    t[2].w->gstate[AZ_G_WEATHER + WG_X] += 256;             /* one part in 256 of one unit */
    i64 big = 0;
    u32 lo = 255, hi = 0, storms = 0, was = 0, same_suns = 0;
    i32 ph[2] = { 0, 0 };
    for (u32 i = 0; i < 2u * BEAT + DAY; i++) {
        for (u32 k = 0; k < 4u; k++) { t[k].w->tick = i; az_weather_global(t[k].w); }
        const AzWorld *w = t[0].w;
        for (u32 j = WG_X; j <= WG_Z; j++) { const i64 v = G(w, j) < 0 ? -(i64)G(w, j) : G(w, j); if (v > big) big = v; }
        const u32 c = (u32)G(w, WG_CLOUD), s = (u32)G(w, AZ_G_STORM);
        if (c < lo) lo = c;
        if (c > hi) hi = c;
        storms += s && !was; was = s;
        if (i == 700u) { ph[0] = G(w, AZ_G_SUN0_PHASE); ph[1] = G(w, AZ_G_SUN1_PHASE); }
        if (i == 700u + BEAT) same_suns = ph[0] == G(w, AZ_G_SUN0_PHASE) && ph[1] == G(w, AZ_G_SUN1_PHASE);
        if (i % BEAT < DAY && i % 50u == 0u) cloud[i / BEAT][(i % BEAT) / 50u] = (u8)c;
    }
    CHECK(memcmp(t[0].w->gstate, t[1].w->gstate, sizeof t[0].w->gstate) == 0);          /* the same seed: the same sky */
    const i64 apart = (i64)G(t[0].w, WG_X) - (i64)G(t[2].w, WG_X);
    CHECK(apart > 65536 || apart < -65536);                 /* 1/256 apart became more than a whole */
    CHECK(G(t[0].w, WG_CLOUD) != G(t[3].w, WG_CLOUD) || G(t[0].w, WG_X) != G(t[3].w, WG_X));
    CHECK_EQ(same_suns, 1u);
    u32 differ[2] = { 0, 0 };
    for (u32 j = 0; j < N; j++) { differ[0] += cloud[0][j] != cloud[1][j]; differ[1] += cloud[1][j] != cloud[2][j]; }
    CHECK(differ[0] > N / 4u && differ[1] > N / 4u);        /* the same suns, another sky: no cycle table */
    CHECK(big < ((i64)1 << 23));
    CHECK(lo < 100u && hi > 156u);
    CHECK(storms > 5u);
    printf("chaos: 126,000 ticks: cloud cover %u to %u, %u storms at the default rate; a difference of 256 of 65,536 in one "
           "variable put the skies %lld of 65,536 apart; of 40 samples of a day %u and %u differ from the day 62,000 ticks "
           "later, when both suns stand where they stood\n", lo, hi, storms, (long long)(apart < 0 ? -apart : apart),
           differ[0], differ[1]);
    for (u32 k = 0; k < 4u; k++) az_test_world_free(&t[k]);
}

/* ---- 7: living worlds ----------------------------------------------------------------- */
typedef struct Run { u64 hash, births, deaths; u32 live, wrong, light_lo, light_hi; i32 temp_lo, temp_hi; bool ok; } Run;
static void world_run(u32 threads, u32 lesion, bool audit, Run *r)
{
    AzTestWorld t;
    memset(r, 0, sizeof *r);
    r->light_lo = 255; r->temp_lo = 32767;
    AzConfig cfg = az_test_config(78, 2000);
    cfg.lesion = lesion;
    if (!az_test_world(&t, &cfg, threads)) { CHECK(!"world"); return; }
    AzWorld *w = t.w;
    const u64 m0 = az_total_matter(w);
    const i64 b0 = books(w);
    const u32 top = az_cell_xyz(w, 3, 5, w->gz - 1u);
    for (u32 i = 0; i < 150u; i++) {
        az_tick(w);
        if (audit) r->wrong += az_total_matter(w) != m0 || books(w) != b0;
        if (w->c_light[top] < r->light_lo) r->light_lo = w->c_light[top];
        if (w->c_light[top] > r->light_hi) r->light_hi = w->c_light[top];
    }
    for (u32 c = 0; c < w->n_cells; c++) {
        if (w->c_temp[c] < r->temp_lo) r->temp_lo = w->c_temp[c];
        if (w->c_temp[c] > r->temp_hi) r->temp_hi = w->c_temp[c];
    }
    r->hash = az_state_hash(w); r->births = w->totals.births; r->deaths = w->totals.deaths; r->live = w->n_live;
    r->ok = az_total_matter(w) == m0 && books(w) == b0 && w->totals.field_underflow == 0;
    az_test_world_free(&t);
}
static void test_worlds(void)
{
    Run a, b, c, d, les;
    world_run(0, 0, true, &a);
    world_run(1, 0, false, &b);
    world_run(3, 0, false, &c);
    world_run(8, 0, false, &d);
    world_run(8, AZ_LESION_WEATHER, false, &les);
    CHECK(a.ok && b.ok && c.ok && d.ok && les.ok);
    CHECK_EQ(a.wrong, 0u);
    CHECK(a.hash == b.hash && a.hash == c.hash && a.hash == d.hash);
    CHECK(les.hash != a.hash);
    CHECK(a.light_hi > a.light_lo + 20u);                   /* the morning of sun 0 */
    CHECK_EQ(les.light_lo, les.light_hi);
    CHECK(a.live > 0u && a.births > 0u);
    printf("worlds: seed 78, 2,000 founders, 150 ticks: %llu births, %llu deaths, %u alive; light of a top cell %u "
           "to %u, temperatures %d to %d; one hash on 0, 1, 3 and 8 workers (%016llx); lesion WEATHER: %llu births, "
           "%llu deaths, %u alive, light %u, temperatures %d to %d\n",
           (unsigned long long)a.births, (unsigned long long)a.deaths, a.live, a.light_lo, a.light_hi, a.temp_lo, a.temp_hi,
           (unsigned long long)a.hash, (unsigned long long)les.births, (unsigned long long)les.deaths, les.live,
           les.light_lo, les.temp_lo, les.temp_hi);
}

int main(void)
{
    test_suns();
    test_depth();
    test_heat();
    test_vents();
    test_vent_heat_range();
    test_storms();
    test_chaos();
    test_worlds();
    return az_test_done();
}
