/* test_chem.c - core/az_chem.c: reactions, burial, movement between cells, the fading of marks,
 * phases, the medium.
 *
 *   1. reactions  each alone on a hand-made cell: amounts and heat change by exactly the stated
 *                 numbers and both audits hold; oxidation doubles with CHEM_DOUBLE of temperature
 *                 and halves under it, toxin decay does not; mineral settles out only in a hot
 *                 cell or beside one; only the lowest open cell of a column buries; a pool too
 *                 small for a whole unit reacts one unit at a time, at the stated chance
 *   2. marks      fade by a share and one unit, to 0
 *   3. medium     the phase of every species against its melting and boiling point, plasma at
 *                 the charge; density and viscosity of an ocean cell, a gas cell, rock
 *   4. worlds     a lifeless world for 2,000 ticks: both audits every tick, no underflow, what
 *                 oxidation and burial move adds up, one hash on 1, 3 and 8 workers; vents and
 *                 burial turn the reservoir over; a living world: one hash on 0 and 8 workers,
 *                 audits every tick, the lesion gives another world in which marks do not fade
 *   5. movement   diffusion to the sides that hold less, by the phase of the species (gas 500,
 *                 liquid 125, grains 16 of the same difference), exact; heat goes with the matter
 *                 and the giver keeps its temperature; nothing to a side that holds more or into
 *                 bedrock; never more than a quarter of the cell; half a unit moves one unit at a
 *                 time; weight: what is solid sinks with the pull, what is gas rises against it,
 *                 what is liquid stays, down or sideways, and one species does all three by the
 *                 temperature; under AZ_LESION_GRAVITY nothing has weight and diffusion goes on;
 *                 a lifeless world with gravity for 500 ticks: organic matter sinks, audits exact
 *                 every tick, one hash on 1 and 8 workers, no fall without gravity or with the
 *                 lesion
 *
 * Parts 1 to 3 switch movement off (still), so that they state the reactions alone.
 * Seeds 81 to 88 (the tuning block).
 */
#include "az_test.h"

static u32 *sp(const AzWorld *w, u32 c) { return &w->c_sp[(size_t)c * AZ_NSPECIES]; }
static i32 dl(const AzWorld *w, u32 c, u32 k) { return w->c_acc[c].delta[k]; }

static bool lifeless(AzTestWorld *t, u64 seed, u32 lesion, u32 threads)
{
    AzConfig cfg = az_test_config(seed, 0);
    cfg.lesion = lesion;
    if (!az_test_world_ex(t, &cfg, threads, AZ_TEST_NO_WORLDGEN)) return false;
    az_test_uniform(t->w);              /* the checks of these parts pin the uniform world (az_test.h) */
    return true;
}
/* a hand-made cell: these amounts at this temperature, nothing on its way */
static void fill(AzWorld *w, u32 c, u32 solv, u32 gas, u32 oxi, u32 nut, u32 org, u32 tox, u32 wst, u32 str, i32 temp)
{
    const u32 a[AZ_NSPECIES] = { solv, gas, oxi, nut, org, tox, wst, str };
    u64 m = 0;
    for (u32 k = 0; k < AZ_NSPECIES; k++) { sp(w, c)[k] = a[k]; m += a[k]; }
    w->c_heat[c] = (u32)((((u64)(u32)temp) * (m + 16u) + 1023u) >> 10);
    w->c_temp[c] = (i16)az_temp_of(w->c_heat[c], m);
    CHECK_EQ(w->c_temp[c], temp);
}
static void rates(AzWorld *w, u32 oxidation, u32 tox, u32 precip, u32 burial)
{
    w->param[AZ_WP_CHEM_OXIDATION] = oxidation; w->param[AZ_WP_CHEM_TOX_DECAY] = tox;
    w->param[AZ_WP_CHEM_PRECIP] = precip; w->param[AZ_WP_CHEM_BURIAL] = burial;
}
/* nothing moves between cells */
static void still(AzWorld *w)
{
    w->param[AZ_WP_CHEM_DIFFUSE_SOLID] = 0; w->param[AZ_WP_CHEM_DIFFUSE_LIQUID] = 0;
    w->param[AZ_WP_CHEM_DIFFUSE_GAS] = 0; w->param[AZ_WP_CHEM_SETTLE] = 0; w->param[AZ_WP_CHEM_RISE] = 0;
}
/* the six face neighbours of c: -x, +x, -y, +y, -z, +z */
static void sides(const AzWorld *w, u32 c, u32 nb[6])
{
    for (u32 f = 0; f < 6u; f++) {
        const i32 d = (f & 1u) ? 1 : -1;
        nb[f] = az_cell_nb(w, c, f < 2u ? d : 0, (f >> 1) == 1u ? d : 0, f >= 4u ? d : 0);
    }
}
static u64 sum_species(const AzWorld *w, u32 k)
{
    i64 n = 0;
    for (u32 c = 0; c < w->n_cells; c++) n += (i64)sp(w, c)[k] + (i64)w->c_acc[c].delta[k];
    return (u64)n;
}

/* ---- 1: the reactions ----------------------------------------------------------------- */
static void test_reactions(void)
{
    AzTestWorld t;
    if (!lifeless(&t, 81, 0, 0)) { CHECK(!"world"); return; }
    AzWorld *w = t.w;
    still(w);
    const u32 ox[3] = { az_cell_xyz(w, 3, 3, 4), az_cell_xyz(w, 6, 3, 4), az_cell_xyz(w, 9, 3, 4) };
    const u32 full = az_cell_xyz(w, 12, 3, 4), tx[2] = { az_cell_xyz(w, 3, 6, 4), az_cell_xyz(w, 6, 6, 4) };
    const u32 hot = az_cell_xyz(w, 3, 9, 4), cool = az_cell_xyz(w, 6, 9, 4), beside = az_cell_xyz(w, 10, 9, 4);
    const u32 low = az_cell_xyz(w, 3, 12, 0), above = az_cell_xyz(w, 6, 12, 1), shelf = az_cell_xyz(w, 9, 12, 3);
    const u32 small = az_cell_xyz(w, 12, 12, 4);
    fill(w, ox[0], 1000, 0, 50000, 0, 60000, 0, 0, 0, 8192);
    fill(w, ox[1], 1000, 0, 50000, 0, 60000, 0, 0, 0, 8192 + 1024);
    fill(w, ox[2], 1000, 0, 50000, 0, 60000, 0, 0, 0, 8192 - 1024);
    fill(w, full, 1000, 0, 50000, 0, 60000, 0, 0, 0, 8192);
    fill(w, tx[0], 1000, 0, 0, 0, 0, 1600, 0, 0, 8192);
    fill(w, tx[1], 1000, 0, 0, 0, 0, 1600, 0, 0, 8192 + 1024);
    fill(w, hot, 1000, 0, 0, 4096, 0, 0, 0, 0, 11000);
    fill(w, cool, 1000, 0, 0, 4096, 0, 0, 0, 0, 8192);
    fill(w, beside, 1000, 0, 0, 4096, 0, 0, 0, 0, 8192);
    w->c_temp[az_cell_nb(w, beside, 0, 1, 0)] = 12000;      /* the cell beside it is hot */
    fill(w, low, 1000, 0, 0, 0, 8192, 0, 4096, 4096, 8192);
    fill(w, above, 1000, 0, 0, 0, 8192, 0, 4096, 4096, 8192);
    fill(w, shelf, 1000, 0, 0, 0, 8192, 0, 4096, 4096, 8192);
    w->c_solid[az_cell_nb(w, shelf, 0, 0, -1)] = 255;       /* a shelf of bedrock under it */
    fill(w, small, 1000, 0, 10, 0, 10, 0, 0, 0, 8192);
    const u64 m0 = az_total_matter(w);
    const i64 e0 = az_total_energy(w);

    /* slow oxidation, 1 per cent per step: 500.5 units at the worldgen temperature */
    rates(w, 656, 0, 0, 0);
    i32 n[3];
    for (u32 i = 0; i < 3u; i++) {
        az_chem_cell(w, ox[i], 0);
        n[i] = -dl(w, ox[i], AZ_SP_ORG_Y);
        CHECK_EQ(dl(w, ox[i], AZ_SP_OXI), -n[i]); CHECK_EQ(dl(w, ox[i], AZ_SP_WST_W), 2 * n[i]);
        CHECK_EQ(w->c_acc[ox[i]].dheat, 12 * n[i]);
        CHECK_EQ(dl(w, ox[i], AZ_SP_SOLV), 0); CHECK_EQ(dl(w, ox[i], AZ_SP_STR_S), 0);
    }
    CHECK(n[0] == 500 || n[0] == 501); CHECK(n[1] == 1000 || n[1] == 1001); CHECK(n[2] == 250 || n[2] == 251);
    rates(w, 65536, 0, 0, 0);                               /* never more than half the scarcer in a step */
    az_chem_cell(w, full, 0);
    CHECK_EQ(dl(w, full, AZ_SP_ORG_Y), -25000); CHECK_EQ(dl(w, full, AZ_SP_WST_W), 50000);
    /* toxin decay, 1/16 per step, whatever the temperature */
    rates(w, 0, 4096, 0, 0);
    for (u32 i = 0; i < 2u; i++) {
        az_chem_cell(w, tx[i], 0);
        CHECK_EQ(dl(w, tx[i], AZ_SP_TOX_Z), -100); CHECK_EQ(dl(w, tx[i], AZ_SP_WST_W), 100); CHECK_EQ(w->c_acc[tx[i]].dheat, 200);
    }
    /* precipitation, 1/32 per step, in a hot cell or beside one */
    rates(w, 0, 0, 2048, 0);
    az_chem_cell(w, hot, 0); az_chem_cell(w, cool, 0); az_chem_cell(w, beside, 0);
    CHECK_EQ(dl(w, hot, AZ_SP_NUT_X), -128); CHECK_EQ(dl(w, hot, AZ_SP_STR_S), 128); CHECK_EQ(w->c_acc[hot].dheat, 24 * 128);
    CHECK_EQ(dl(w, cool, AZ_SP_NUT_X), 0); CHECK_EQ(w->c_acc[cool].dheat, 0);
    CHECK_EQ(dl(w, beside, AZ_SP_NUT_X), -128); CHECK_EQ(dl(w, beside, AZ_SP_STR_S), 128);
    /* burial, 1/16 per step, from the lowest open cell of a column only */
    rates(w, 0, 0, 0, 4096);
    const i64 r0 = w->workers[0].tally.reservoir;
    az_chem_cell(w, low, 0); az_chem_cell(w, above, 0);
    CHECK_EQ(dl(w, low, AZ_SP_WST_W), -256); CHECK_EQ(dl(w, low, AZ_SP_ORG_Y), -512); CHECK_EQ(dl(w, low, AZ_SP_STR_S), -256);
    CHECK_EQ(w->workers[0].tally.reservoir - r0, 1024);
    CHECK_EQ(w->c_acc[low].dheat, 2 * 256 + 16 * 512);      /* what was buried leaves its chemical energy as heat */
    CHECK_EQ(dl(w, above, AZ_SP_ORG_Y), 0); CHECK_EQ(w->c_acc[above].dheat, 0);
    az_chem_cell(w, shelf, 0);
    CHECK_EQ(dl(w, shelf, AZ_SP_ORG_Y), -512); CHECK_EQ(w->workers[0].tally.reservoir - r0, 2048);
    CHECK_EQ(az_total_matter(w), m0); CHECK_EQ(az_total_energy(w), e0);

    /* a pool of 10 at 1 per cent: a tenth of a unit, so one unit in about one step of ten */
    rates(w, 656, 0, 0, 0);
    u32 units = 0, big = 0;
    for (u32 i = 0; i < 400u; i++) {
        w->tick++;
        memset(&w->c_acc[small], 0, sizeof w->c_acc[small]);
        az_chem_cell(w, small, 0);
        const u32 k = (u32)-dl(w, small, AZ_SP_ORG_Y);
        units += k; big += k > 1u;
    }
    CHECK_EQ(big, 0u);
    CHECK(units > 16u && units < 64u);                      /* 40 expected, 6 standard deviation */
    printf("reactions: 1 per cent of 50,000: %d units oxidised at the worldgen temperature, %d one doubling up, %d "
           "one down; 100 of 1,600 toxin decayed at both temperatures; 128 of 4,096 mineral settled in a hot cell "
           "and beside one, none in a cool one; the lowest cell buried 1,024, the one above it nothing; a pool of 10 "
           "gave %u single units in 400 steps\n", n[0], n[1], n[2], units);
    az_test_world_free(&t);
}

/* ---- 2: marks -------------------------------------------------------------------------- */
static void test_marks(void)
{
    AzTestWorld t;
    if (!lifeless(&t, 82, 0, 0)) { CHECK(!"world"); return; }
    AzWorld *w = t.w;
    rates(w, 0, 0, 0, 0);
    const u32 c = az_cell_xyz(w, 5, 5, 5);
    u8 *m = &w->c_mark[(size_t)c * AZ_NMARK];
    m[0] = 200; m[1] = 100; m[2] = 3; m[3] = 0;
    az_chem_cell(w, c, 0);
    CHECK_EQ(m[0], 193); CHECK_EQ(m[1], 96); CHECK_EQ(m[2], 2); CHECK_EQ(m[3], 0);      /* 200 - 6 - 1, 100 - 3 - 1, 3 - 0 - 1 */
    u32 steps = 1;
    while ((m[0] || m[1] || m[2]) && steps < 1000u) { az_chem_cell(w, c, 0); steps++; }
    CHECK(steps < 120u);
    CHECK_EQ(m[0], 0);
    printf("marks: 200, 100, 3 became 193, 96, 2 in a step and 0 after %u steps\n", steps);
    az_test_world_free(&t);
}

/* ---- 3: phases and the medium ---------------------------------------------------------- */
static void test_medium(void)
{
    AzTestWorld t;
    if (!lifeless(&t, 83, 0, 0)) { CHECK(!"world"); return; }
    AzWorld *w = t.w;
    const u32 c = az_cell_xyz(w, 5, 5, 5);
    const u8 *med = &w->c_med[(size_t)c * 2];
    enum { S = AZ_PH_SOLID, L = AZ_PH_LIQUID, V = AZ_PH_GAS, X = AZ_PH_PLASMA };
    /* the cell the stub world is made of, as worldgen left it */
    static const u8 at_8192[AZ_NSPECIES] = { L, V, V, S, S, L, L, S };      /* SOLV GAS OXI NUT_X ORG_Y TOX_Z WST_W STR_S */
    for (u32 k = 0; k < AZ_NSPECIES; k++) CHECK_EQ(AZ_PHASE_OF(w->c_phase[c], k), at_8192[k]);
    CHECK_EQ(med[0], 112); CHECK_EQ(med[1], 162);           /* (1000 x 4096 + 400 x 8192 + 200 x 128) / 65536; (.. 4096, 16384, 64) */
    static const struct { i32 temp; u8 charge; u8 ph[AZ_NSPECIES]; } v[] = {
        { 12500, 0,   { V, V, V, S, L, L, V, S } },         /* the solvent and the waste boil, organic matter melts */
        { 5000,  0,   { S, V, V, S, S, L, S, S } },         /* the solvent and the waste freeze */
        { 8192,  200, { L, X, X, S, S, L, L, S } },         /* charged: what is gas is plasma */
        { 300,   200, { S, L, S, S, S, S, S, S } } };       /* near nothing: only the carrier gas still flows */
    for (u32 i = 0; i < sizeof v / sizeof v[0]; i++) {
        fill(w, c, 1000, 0, 200, 0, 400, 0, 0, 0, v[i].temp);
        w->c_charge[c] = v[i].charge;
        az_chem_derive(w, c);
        for (u32 k = 0; k < AZ_NSPECIES; k++) CHECK_EQ(AZ_PHASE_OF(w->c_phase[c], k), v[i].ph[k]);
    }
    w->c_charge[c] = 0;
    fill(w, c, 0, 2000, 0, 0, 0, 0, 0, 0, 8192);            /* a cell of the atmosphere */
    az_chem_derive(w, c);
    CHECK_EQ(med[0], 3); CHECK_EQ(med[1], 1);
    fill(w, c, 1000, 0, 200, 0, 400, 0, 0, 5000, 8192);     /* a reef: structure thickens it */
    az_chem_derive(w, c);
    CHECK_EQ(med[0], 255); CHECK_EQ(med[1], 255);
    fill(w, c, 1000, 0, 200, 0, 400, 0, 0, 0, 8192);
    w->c_solid[c] = 100;                                    /* partly rock */
    az_chem_derive(w, c);
    CHECK_EQ(med[0], 212);
    w->c_solid[c] = 255;
    az_chem_derive(w, c);
    CHECK_EQ(med[0], 255); CHECK_EQ(med[1], 255); CHECK_EQ(w->c_phase[c], 0);
    printf("medium: an ocean cell has density 112 and viscosity 162, a cell of gas 3 and 1, a reef and bedrock 255\n");
    az_test_world_free(&t);
}

/* ---- 4: worlds -------------------------------------------------------------------------- */
static u64 lifeless_run(u32 threads, bool audit)
{
    AzTestWorld t;
    if (!lifeless(&t, 84, 0, threads)) { CHECK(!"world"); return 0; }
    AzWorld *w = t.w;
    const u64 m0 = az_total_matter(w);
    const i64 e0 = az_total_energy(w);
    const u64 oxi0 = sum_species(w, AZ_SP_OXI);
    const u64 redox0 = sum_species(w, AZ_SP_ORG_Y) + oxi0 + sum_species(w, AZ_SP_WST_W) + w->totals.reservoir_mu;
    u32 wrong = 0;
    for (u32 i = 0; i < 2000u; i++) {
        az_tick(w);
        if (audit) wrong += az_total_matter(w) != m0 || az_total_energy(w) != e0;
    }
    CHECK_EQ(wrong, 0u);
    CHECK_EQ(az_total_matter(w), m0); CHECK_EQ(az_total_energy(w), e0);
    CHECK_EQ(w->totals.field_underflow, 0u);
    const u64 oxi1 = sum_species(w, AZ_SP_OXI);
    CHECK(oxi1 < oxi0); CHECK(w->totals.reservoir_mu > 0u);
    CHECK_EQ(sum_species(w, AZ_SP_ORG_Y) + oxi1 + sum_species(w, AZ_SP_WST_W) + w->totals.reservoir_mu, redox0);
    if (audit)
        printf("worlds: lifeless, 2,000 ticks: %llu of %llu oxidiser used, %llu mu buried, audits exact every tick\n",
               (unsigned long long)(oxi0 - oxi1), (unsigned long long)oxi0, (unsigned long long)w->totals.reservoir_mu);
    const u64 h = az_state_hash(w);
    az_test_world_free(&t);
    return h;
}
typedef struct Run { u64 hash, births, deaths, marks; u32 live, wrong; bool ok; } Run;
static void living_run(u32 threads, u32 lesion, bool audit, Run *r)
{
    AzTestWorld t;
    memset(r, 0, sizeof *r);
    AzConfig cfg = az_test_config(86, 2000);
    cfg.lesion = lesion;
    if (!az_test_world(&t, &cfg, threads)) { CHECK(!"world"); return; }
    AzWorld *w = t.w;
    const u64 m0 = az_total_matter(w);
    const i64 e0 = az_total_energy(w);
    for (u32 i = 0; i < 150u; i++) {
        az_tick(w);
        if (audit) r->wrong += az_total_matter(w) != m0 || az_total_energy(w) != e0;
    }
    for (size_t i = 0; i < (size_t)w->n_cells * AZ_NMARK; i++) r->marks += w->c_mark[i];
    r->hash = az_state_hash(w); r->births = w->totals.births; r->deaths = w->totals.deaths; r->live = w->n_live;
    r->ok = az_total_matter(w) == m0 && az_total_energy(w) == e0 && w->totals.field_underflow == 0;
    az_test_world_free(&t);
}
static void test_worlds(void)
{
    const u64 h1 = lifeless_run(1, true), h3 = lifeless_run(3, false), h8 = lifeless_run(8, false);
    CHECK(h1 == h3 && h1 == h8);

    /* the cycle: what is buried comes up again through the vents */
    AzTestWorld t;
    if (!lifeless(&t, 85, 0, 0)) { CHECK(!"world"); return; }
    AzWorld *w = t.w;
    w->vent_cell[0] = az_cell_xyz(w, 2, 2, 0); w->vent_cell[1] = az_cell_xyz(w, 10, 3, 0); w->vent_count = 2;
    w->totals.reservoir_mu = 5000;
    w->param[AZ_WP_CHEM_BURIAL] = 1024;
    const u64 m0 = az_total_matter(w);
    const i64 e0 = az_total_energy(w);
    u32 wrong = 0;
    for (u32 i = 0; i < 3000u; i++) { az_tick(w); wrong += az_total_matter(w) != m0 || az_total_energy(w) != e0; }
    CHECK_EQ(wrong, 0u);
    const u64 up = sum_species(w, AZ_SP_NUT_X) + sum_species(w, AZ_SP_STR_S);
    CHECK(up > 5000u);                                      /* more came up than the reservoir held at the start */
    CHECK_EQ(w->totals.field_underflow, 0u);
    printf("worlds: two vents and a reservoir of 5,000 mu: %llu mu of mineral came up in 3,000 ticks, %llu of it "
           "settled as structure; the reservoir holds %llu\n", (unsigned long long)up,
           (unsigned long long)sum_species(w, AZ_SP_STR_S), (unsigned long long)w->totals.reservoir_mu);
    az_test_world_free(&t);

    Run a, b, les;
    living_run(0, 0, true, &a);
    living_run(8, 0, false, &b);
    living_run(8, AZ_LESION_CHEM, false, &les);
    CHECK(a.ok && b.ok && les.ok);
    CHECK_EQ(a.wrong, 0u);
    CHECK(a.hash == b.hash);
    CHECK(les.hash != a.hash);
    CHECK(les.marks > a.marks);                             /* with the lesion marks only grow */
    CHECK(a.live > 0u && a.births > 0u);
    printf("worlds: seed 86, 2,000 founders, 150 ticks: %llu births, %llu deaths, %u alive, marks %llu, one hash on 0 "
           "and 8 workers (%016llx); lesion CHEM: %llu births, %llu deaths, %u alive, marks %llu\n",
           (unsigned long long)a.births, (unsigned long long)a.deaths, a.live, (unsigned long long)a.marks,
           (unsigned long long)a.hash, (unsigned long long)les.births, (unsigned long long)les.deaths, les.live,
           (unsigned long long)les.marks);
}

/* ---- 5: movement between cells --------------------------------------------------------- */
/* a cell and its six sides, all with these amounts at this temperature */
static void block(AzWorld *w, u32 c, u32 nb[6], u32 solv, u32 gas, u32 nut, u32 wst, i32 temp)
{
    sides(w, c, nb);
    fill(w, c, solv, gas, 0, nut, 0, 0, wst, 0, temp);
    for (u32 f = 0; f < 6u; f++) {
        CHECK(nb[f] != AZ_NONE);
        if (nb[f] != AZ_NONE) fill(w, nb[f], solv, gas, 0, nut, 0, 0, wst, 0, temp);
    }
}
typedef struct Fall { u64 hash; i64 z1024; u32 wrong; bool ok; } Fall;
/* a lifeless world with this gravity along z in every cell, 500 ticks; where the organic matter is */
static void fall_run(u32 threads, i32 gz, u32 lesion, Fall *r)
{
    AzTestWorld t;
    memset(r, 0, sizeof *r);
    if (!lifeless(&t, 88, lesion, threads)) { CHECK(!"world"); return; }
    AzWorld *w = t.w;
    for (u32 c = 0; c < w->n_cells; c++) w->c_grav[(size_t)c * 4 + 2] = (i8)gz;
    const u64 m0 = az_total_matter(w);
    const i64 e0 = az_total_energy(w);
    for (u32 i = 0; i < 500u; i++) { az_tick(w); r->wrong += az_total_matter(w) != m0 || az_total_energy(w) != e0; }
    u64 sum = 0, zsum = 0;
    for (u32 c = 0; c < w->n_cells; c++) { const u64 a = sp(w, c)[AZ_SP_ORG_Y]; sum += a; zsum += a * az_cell_z(w, c); }
    r->z1024 = sum ? (i64)((zsum << 10) / sum) : -1;
    r->hash = az_state_hash(w);
    r->ok = w->totals.field_underflow == 0 && az_total_matter(w) == m0 && az_total_energy(w) == e0;
    az_test_world_free(&t);
}
static void test_movement(void)
{
    AzTestWorld t;
    if (!lifeless(&t, 87, 0, 0)) { CHECK(!"world"); return; }
    AzWorld *w = t.w;
    rates(w, 0, 0, 0, 0);                                   /* no reaction, no burial: movement alone */
    u32 nb[6];

    /* by phase: at 8192 the carrier gas is gas, the solvent liquid, the mineral solid. Every side
     * holds 4,000 less gas, 4,000 less solvent and 4,096 less mineral */
    const u32 a = az_cell_xyz(w, 4, 4, 3);
    block(w, a, nb, 36000, 36000, 36000, 0, 8192);
    fill(w, a, 40000, 40000, 0, 40096, 0, 0, 0, 0, 8192);
    const u32 heat_a = w->c_heat[a];
    az_chem_cell(w, a, 0);
    const i32 h_side = (i32)(((u64)(500 + 125 + 16) * heat_a) / (120096u + 16u));
    for (u32 f = 0; f < 6u; f++) {
        CHECK_EQ(dl(w, nb[f], AZ_SP_GAS), 500);             /* 4000 x 8192 / 65536 */
        CHECK_EQ(dl(w, nb[f], AZ_SP_SOLV), 125);            /* 4000 x 2048 / 65536 */
        CHECK_EQ(dl(w, nb[f], AZ_SP_NUT_X), 16);            /* 4096 x  256 / 65536 */
        CHECK_EQ(w->c_acc[nb[f]].dheat, h_side);            /* heat goes with the matter */
    }
    CHECK_EQ(dl(w, a, AZ_SP_GAS), -3000); CHECK_EQ(dl(w, a, AZ_SP_SOLV), -750); CHECK_EQ(dl(w, a, AZ_SP_NUT_X), -96);
    CHECK(h_side > 0); CHECK_EQ(w->c_acc[a].dheat, -6 * h_side);
    const i32 t_after = az_temp_of((u32)((i64)heat_a + w->c_acc[a].dheat), 120096u - 6u * 641u);
    CHECK(t_after >= 8191 && t_after <= 8193);              /* the cell that gave keeps its temperature */

    /* push only, and nothing into bedrock */
    const u32 b = az_cell_xyz(w, 9, 4, 3);
    block(w, b, nb, 0, 36000, 0, 0, 8192);
    fill(w, b, 0, 40000, 0, 0, 0, 0, 0, 0, 8192);
    fill(w, nb[0], 0, 50000, 0, 0, 0, 0, 0, 0, 8192);       /* this side holds more */
    w->c_solid[nb[3]] = 255;                                /* this side is rock */
    az_chem_cell(w, b, 0);
    CHECK_EQ(dl(w, nb[0], AZ_SP_GAS), 0); CHECK_EQ(w->c_acc[nb[0]].dheat, 0);
    CHECK_EQ(dl(w, nb[3], AZ_SP_GAS), 0); CHECK_EQ(w->c_acc[nb[3]].dheat, 0);
    CHECK_EQ(dl(w, nb[1], AZ_SP_GAS), 500); CHECK_EQ(dl(w, nb[4], AZ_SP_GAS), 500);
    CHECK_EQ(dl(w, b, AZ_SP_GAS), -2000);

    /* never more than a quarter of what the cell holds, and every side its share of that */
    const u32 q = az_cell_xyz(w, 4, 9, 3);
    block(w, q, nb, 0, 0, 0, 0, 8192);
    fill(w, q, 0, 40000, 0, 0, 0, 0, 0, 0, 8192);
    az_chem_cell(w, q, 0);
    i32 out = 0;
    for (u32 f = 0; f < 6u; f++) { const i32 n = dl(w, nb[f], AZ_SP_GAS); CHECK(n == 1666 || n == 1667); out += n; }
    CHECK(out >= 9996 && out <= 10000); CHECK_EQ(dl(w, q, AZ_SP_GAS), -out);

    /* a difference too small for a whole unit: 16 units of solvent beside empty cells are half a
     * unit a side, moved one unit at a time, four at most (a quarter of 16); 100 steps */
    const u32 s = az_cell_xyz(w, 9, 9, 3);
    block(w, s, nb, 0, 0, 0, 0, 8192);
    fill(w, s, 16, 0, 0, 0, 0, 0, 0, 0, 8192);
    const u64 tick0 = w->tick;
    u32 units = 0, most = 0, partial = 0;
    for (u32 i = 0; i < 100u; i++) {
        w->tick = tick0 + i;
        az_chem_cell(w, s, 0);
        const u32 n = (u32)-dl(w, s, AZ_SP_SOLV);
        u32 got = 0;
        for (u32 f = 0; f < 6u; f++) { got += (u32)dl(w, nb[f], AZ_SP_SOLV); memset(&w->c_acc[nb[f]], 0, sizeof w->c_acc[0]); }
        memset(&w->c_acc[s], 0, sizeof w->c_acc[0]);
        CHECK_EQ(got, n); CHECK(n <= 4u);
        units += n;
        if (n > most) most = n;
        if (n > 0u && n < 4u) partial++;
    }
    w->tick = tick0;
    CHECK(units > 240u && units < 335u);                    /* 287.5 expected, standard deviation 10.4 */
    CHECK_EQ(most, 4u);
    CHECK(partial > 40u);                                   /* each side has its own draw: 64 expected */
    fill(w, s, 3, 0, 0, 0, 0, 0, 0, 0, 8192);               /* a quarter of 3 is no unit: it stays */
    az_chem_cell(w, s, 0);
    CHECK_EQ(dl(w, s, AZ_SP_SOLV), 0);
    printf("movement: gas 500, liquid 125, grains 16 a side of one difference; 16 units beside empty cells gave "
           "%u units in 100 steps\n", units);

    /* weight. Gravity of 8 down (-z): of 65,536 units of mineral (solid) 512 sink, of 65,536 of
     * carrier gas 128 rise, the solvent (liquid) stays; no side holds less, so nothing diffuses */
    const u32 down = az_cell_xyz(w, 12, 4, 3);
    block(w, down, nb, 65536, 65536, 65536, 0, 8192);
    w->c_grav[(size_t)down * 4 + 2] = -8;
    az_chem_cell(w, down, 0);
    CHECK_EQ(dl(w, nb[4], AZ_SP_NUT_X), 512); CHECK_EQ(dl(w, nb[5], AZ_SP_GAS), 128);
    CHECK_EQ(dl(w, down, AZ_SP_NUT_X), -512); CHECK_EQ(dl(w, down, AZ_SP_GAS), -128);
    CHECK_EQ(dl(w, down, AZ_SP_SOLV), 0);
    for (u32 f = 0; f < 4u; f++) { CHECK_EQ(dl(w, nb[f], AZ_SP_NUT_X), 0); CHECK_EQ(dl(w, nb[f], AZ_SP_GAS), 0); }
    CHECK_EQ(dl(w, nb[5], AZ_SP_NUT_X), 0); CHECK_EQ(dl(w, nb[4], AZ_SP_GAS), 0);
    /* "down" can bend: gravity along +x sends the mineral to the +x side and the gas to the -x side */
    const u32 bent = az_cell_xyz(w, 12, 9, 3);
    block(w, bent, nb, 65536, 65536, 65536, 0, 8192);
    w->c_grav[(size_t)bent * 4 + 0] = 8;
    az_chem_cell(w, bent, 0);
    CHECK_EQ(dl(w, nb[1], AZ_SP_NUT_X), 512); CHECK_EQ(dl(w, nb[0], AZ_SP_GAS), 128);
    CHECK_EQ(dl(w, nb[0], AZ_SP_NUT_X), 0); CHECK_EQ(dl(w, nb[1], AZ_SP_GAS), 0);
    CHECK_EQ(dl(w, nb[4], AZ_SP_NUT_X), 0); CHECK_EQ(dl(w, nb[5], AZ_SP_GAS), 0);
    /* one species, three temperatures: waste is solid under 5120 and sinks, liquid at 8192 and
     * stays, gas from 11264 and rises */
    const u32 cold = az_cell_xyz(w, 4, 13, 3), mild = az_cell_xyz(w, 9, 13, 3), warm = az_cell_xyz(w, 12, 13, 3);
    block(w, cold, nb, 0, 0, 0, 65536, 4096);
    w->c_grav[(size_t)cold * 4 + 2] = -8;
    az_chem_cell(w, cold, 0);
    CHECK_EQ(dl(w, nb[4], AZ_SP_WST_W), 512); CHECK_EQ(dl(w, nb[5], AZ_SP_WST_W), 0);
    block(w, mild, nb, 0, 0, 0, 65536, 8192);
    w->c_grav[(size_t)mild * 4 + 2] = -8;
    az_chem_cell(w, mild, 0);
    CHECK_EQ(dl(w, nb[4], AZ_SP_WST_W), 0); CHECK_EQ(dl(w, nb[5], AZ_SP_WST_W), 0); CHECK_EQ(dl(w, mild, AZ_SP_WST_W), 0);
    block(w, warm, nb, 0, 0, 0, 65536, 12032);
    w->c_grav[(size_t)warm * 4 + 2] = -8;
    az_chem_cell(w, warm, 0);
    CHECK_EQ(dl(w, nb[4], AZ_SP_WST_W), 0); CHECK_EQ(dl(w, nb[5], AZ_SP_WST_W), 128);
    CHECK_EQ(az_total_matter(w) > 0u, true);
    az_test_world_free(&t);

    /* the lesion: nothing has weight, diffusion goes on */
    if (!lifeless(&t, 87, AZ_LESION_GRAVITY, 0)) { CHECK(!"world"); return; }
    w = t.w;
    rates(w, 0, 0, 0, 0);
    const u32 les = az_cell_xyz(w, 12, 4, 3);
    block(w, les, nb, 65536, 65536, 65536, 0, 8192);
    fill(w, nb[1], 65536, 61536, 0, 65536, 0, 0, 0, 0, 8192);
    w->c_grav[(size_t)les * 4 + 2] = -8;
    az_chem_cell(w, les, 0);
    CHECK_EQ(dl(w, nb[4], AZ_SP_NUT_X), 0); CHECK_EQ(dl(w, nb[5], AZ_SP_GAS), 0); CHECK_EQ(dl(w, les, AZ_SP_NUT_X), 0);
    CHECK_EQ(dl(w, nb[1], AZ_SP_GAS), 500); CHECK_EQ(dl(w, les, AZ_SP_GAS), -500);
    az_test_world_free(&t);

    /* a world: organic matter sinks where there is gravity, and only there */
    Fall none, pull, pull8, lesion;
    fall_run(1, 0, 0, &none);
    fall_run(1, -16, 0, &pull);
    fall_run(8, -16, 0, &pull8);
    fall_run(8, -16, AZ_LESION_GRAVITY, &lesion);
    CHECK(none.ok && pull.ok && pull8.ok && lesion.ok);
    CHECK_EQ(none.wrong + pull.wrong + pull8.wrong + lesion.wrong, 0u);
    CHECK(pull.hash == pull8.hash);
    CHECK(pull.z1024 < none.z1024 - 256);                   /* more than a quarter of a cell lower */
    CHECK_EQ(lesion.z1024, none.z1024);
    CHECK(lesion.hash != pull.hash);
    printf("movement: lifeless, 500 ticks, mean height of the organic matter (cells x 1024): %lld without gravity, "
           "%lld with gravity 16 down (one hash on 1 and 8 workers, %016llx), %lld under the lesion\n",
           (long long)none.z1024, (long long)pull.z1024, (unsigned long long)pull.hash, (long long)lesion.z1024);
}

/* Long division of moved * heat by divisor. Process one input heat bit at a
 * time; the partial remainder stays below 3*divisor, including at full stock. */
static u64 transport_oracle(u64 moved, u32 heat, u64 divisor)
{
    u64 quotient = 0, remainder = 0;
    for (u32 bit = 32; bit-- > 0;) {
        remainder = 2u * remainder + (((heat >> bit) & 1u) ? moved : 0u);
        quotient = 2u * quotient + remainder / divisor;
        remainder %= divisor;
    }
    return quotient;
}

static void test_large_face_heat(void)
{
    /* The first column is the sum of species quarter-budgets. At one open
     * face, gravity 32 and settle 512 move exactly that amount if the stocks
     * are multiples of four. Include an exact quotient and the adjacent
     * heat values, a full u32 inventory of every species, and two faces. */
    const u64 limit = UINT32_MAX, maximum = AZ_NSPECIES * (limit / 4u);
    const struct { u64 budget; u32 heat; bool full, two_faces; } cases[] = {
        {0, UINT32_MAX, false, false}, {28, 32, false, false},
        {limit-1u, UINT32_MAX, false, false}, {limit, UINT32_MAX, false, false},
        {limit+1u, 1073741824u, false, false}, {limit+1u, 1073741825u, false, false},
        {limit+1u, 1073741826u, false, false}, {limit+16u, UINT32_MAX, false, false},
        {4921838393ull, 3169321091u, false, false}, /* both quotient adjustments apply */
        {maximum-1u, UINT32_MAX, false, false}, {maximum, UINT32_MAX, false, false},
        {maximum, 0, true, false}, {maximum, UINT32_MAX, true, false},
        {maximum, UINT32_MAX, true, true}
    };
    for (u32 j = 0; j < AZ_LEN(cases); j++) {
        AzTestWorld t;
        AzConfig cfg = az_test_config(87, 0);
        cfg.cap = 8; cfg.gx_bits = cfg.gy_bits = 2; cfg.gz_bits = 1;
        cfg.field_stride = 2; cfg.lesion = AZ_LESION_WEATHER;
        if (!az_test_world_ex(&t, &cfg, 0, AZ_TEST_NO_WORLDGEN)) { CHECK(!"world"); return; }
        AzWorld *w = t.w;
        memset(w->c_solid, 255, w->n_cells);
        const u32 x = az_cell_nb(w, 0, 1, 0, 0), y = az_cell_nb(w, 0, 0, 1, 0);
        w->c_solid[0] = w->c_solid[x] = 0;
        if (cases[j].two_faces) w->c_solid[y] = 0;
        w->c_grav[0] = cases[j].two_faces ? 24 : 32;
        w->c_grav[1] = cases[j].two_faces ? 8 : 0;
        rates(w, 0, 0, 0, 0); still(w);
        CHECK(az_param_set(w, AZ_WP_CHEM_SETTLE, 512));

        u64 left = cases[j].budget, matter = 0;
        u32 initial[AZ_NSPECIES];
        for (u32 k = 0; k < AZ_NSPECIES; k++) {
            CHECK(az_param_set(w, AZ_WP_CHEM_MELT_SOLV + k, 32767));
            const u64 part = left < limit/4u ? left : limit/4u;
            left -= part;
            initial[k] = cases[j].full ? UINT32_MAX : (u32)(4u * part);
            matter += initial[k];
        }
        /* Construct legal committed inventories using i32-sized public posts. */
        for (u32 shift = 0; shift < 3u; shift++) {
            const u64 offset = (u64)shift * INT32_MAX;
            for (u32 k = 0; k < AZ_NSPECIES; k++) {
                if (initial[k] <= offset) continue;
                const u64 amount = initial[k] - offset;
                az_cell_post(w, 0, 0, k, (i32)(amount < INT32_MAX ? amount : INT32_MAX));
            }
            if (cases[j].heat > offset) {
                const u64 amount = cases[j].heat - offset;
                az_cell_post_heat(w, 0, 0, (i32)(amount < INT32_MAX ? amount : INT32_MAX));
            }
            az_field_commit(w, 0, 1, 0);
        }
        const i64 energy = az_total_energy(w);
        CHECK_EQ(az_total_matter(w), matter);
        CHECK_EQ(w->c_heat[0], cases[j].heat);
        az_tick(w);
        u64 moved_x = 0, moved_y = 0;
        for (u32 k = 0; k < AZ_NSPECIES; k++) {
            moved_x += sp(w, x)[k]; moved_y += sp(w, y)[k];
            CHECK_EQ((u64)sp(w, 0)[k] + sp(w, x)[k] + sp(w, y)[k], initial[k]);
        }
        if (!cases[j].full) CHECK_EQ(moved_x, cases[j].budget);
        if (cases[j].full) CHECK(moved_x > UINT32_MAX);
        if (cases[j].two_faces) CHECK(moved_y > 0);
        else CHECK_EQ(moved_y, 0);
        const u64 hx = transport_oracle(moved_x, cases[j].heat, matter+16u);
        const u64 hy = transport_oracle(moved_y, cases[j].heat, matter+16u);
        CHECK_EQ(w->c_heat[x], hx); CHECK_EQ(w->c_heat[y], hy);
        CHECK_EQ(w->c_heat[0], cases[j].heat - hx - hy);
        CHECK_EQ(az_total_energy(w), energy);
        CHECK_EQ(az_total_matter(w), matter);
        CHECK_EQ(w->totals.field_underflow, 0);
        az_test_world_free(&t);
    }
}

int main(void)
{
    test_reactions();
    test_marks();
    test_medium();
    test_worlds();
    test_movement();
    test_large_face_heat();
    return az_test_done();
}
