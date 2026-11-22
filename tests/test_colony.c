/* test_colony.c - the colony as an organism (core/az_colony.c, parameters core/params/az_colony.def).
 *
 *   1. found     the record a founder makes, its gift to the pool, who cannot found, the full table
 *                (an event, counted), dissolution with stores and pool returned to the last unit,
 *                the free record taken again
 *   2. measure   members, centre (also across the wrap of x), velocity, alarm from wounds and from
 *                alarm marks, mood from growth and loss, the call for SOLDIER and NURSE
 *   3. roles     the response-threshold rule against its formula: the share that takes a role at a
 *                given call and threshold, division of labour between two leanings (CASTE), giving
 *                up a role, the young and BREEDER, eggs and the dormant
 *   4. stay      leaving by COLONIALITY and the colony's mood; founding by COLONIALITY, in uid order,
 *                only by those that can pay
 *   5. culture   the mean context of the members, the memory written when the mood rises, tradition
 *   6. worlds    books exact every tick, equal hashes on 0, 1, 3 and 8 workers and reorder 1 and 9,
 *                a snapshot with living colonies resumed on another worker count, dissolutions in a
 *                world, lesion COLONY, and both traits moving an outcome
 *
 *   7. pools     the members' traffic with the colony's pools (core/az_econ.c): asking and taking
 *                energy and matter in want, by the granted fraction when the pool is short; the
 *                GIVE motor; giving by STORE_SHARE and COMMONS; the FORAGER's share; the BUILDER's
 *                nest mark; the voice; nothing taken that was not asked of this colony in this tick
 *
 * Parts 1 to 5 call the pass for hand-made creatures in a world whose own founding is switched off;
 * part 7 calls the economy's two hooks around the grant and the commit, as the tick does.
 * Seeds 61 to 68 (the tuning block).
 */
#include "az_test.h"

static u32 P(const AzWorld *w, u32 id) { return w->param[id]; }
static u8 *tr(const AzWorld *w, u32 s) { return &w->trait[(size_t)s * AZ_TRAITS]; }
static i8 *cx(const AzWorld *w, u32 s) { return &w->ctx[(size_t)s * 8u]; }

/* a world of n founders in which nobody founds or leaves on its own; `want` living slots */
static bool bench(AzTestWorld *t, u64 seed, u32 n, u32 max_colonies, u32 *slot, u32 want)
{
    AzConfig cfg = az_test_config(seed, n);
    cfg.max_colonies = max_colonies;
    if (!az_test_world(t, &cfg, 0)) return false;
    AzWorld *w = t->w;
    w->param[AZ_WP_COLONY_FOUND] = 0; w->param[AZ_WP_COLONY_LEAVE] = 0;
    az_tick(w);
    u32 k = 0;
    for (u32 s = 0; s < w->n_slots && k < want; s++)
        if ((w->flags[s] & AZ_F_ALIVE) && w->spos[s] != AZ_NONE && w->blk[s]) slot[k++] = s;
    return k == want;
}
/* an adult with full health, 10,000 eu, no colony, no role, an empty context and the two traits given */
static void make(AzWorld *w, u32 s, u32 coloniality, u32 caste)
{
    w->flags[s] = (u8)(AZ_F_ALIVE | (w->flags[s] & AZ_F_INIT));
    w->energy[s] = 10000; w->health[s] = 65535; w->colony[s] = 0; w->role[s] = AZ_ROLE_NONE;
    memset(cx(w, s), 0, 8);
    tr(w, s)[AZ_T_COLONIALITY] = (u8)coloniality; tr(w, s)[AZ_T_CASTE] = (u8)caste;
    w->c_mark[(size_t)w->cell[s] * AZ_NMARK + AZ_MK_ALARM] = 0;
}
static void pass(AzWorld *w) { w->tick++; az_colony_tier2(w); }
static u32 count_role(const AzWorld *w, const u32 *slot, u32 n, u32 role)
{
    u32 k = 0;
    for (u32 i = 0; i < n; i++) k += w->role[slot[i]] == role;
    return k;
}

/* ---- 1: founding and dissolution ------------------------------------------------- */
static void test_found(void)
{
    AzTestWorld t;
    u32 a[6];
    if (!bench(&t, 61, 600, 4, a, 6)) { CHECK(!"bench"); return; }
    AzWorld *w = t.w;
    for (u32 i = 0; i < 6u; i++) make(w, a[i], 0, 0);
    static const i8 mind[8] = { 10, -20, 30, -40, 50, -60, 70, -128 };
    memcpy(cx(w, a[0]), mind, 8);
    w->energy[a[1]] = 100;                                /* a poor founder */
    u64 m0 = az_total_matter(w);
    i64 e0 = az_total_energy(w);

    CHECK_EQ(az_colony_found(w, a[0]), 1u);
    const AzColony *k = &w->colonies[1];
    CHECK(k->alive == 1 && k->founder_uid == w->uid[a[0]] && k->born_tick == w->tick);
    CHECK(k->cx == w->pos_x[a[0]] && k->cy == w->pos_y[a[0]] && k->cz == w->pos_z[a[0]] && k->tag == w->tag[a[0]]);
    CHECK_EQ(k->mood, 128);
    for (u32 j = 0; j < AZ_ROLE_COUNT; j++) CHECK_EQ(k->need[j], P(w, AZ_WP_COLONY_NEED_START));
    for (u32 l = 0; l < 8u; l++) CHECK_EQ(k->culture[l], (u8)(128 + mind[l]));
    CHECK_EQ(k->energy, (i64)P(w, AZ_WP_COLONY_ENDOW));
    CHECK_EQ(w->energy[a[0]], 10000 - (i32)P(w, AZ_WP_COLONY_ENDOW));
    CHECK_EQ(w->colony[a[0]], 1u);
    CHECK_EQ(az_colony_found(w, a[0]), 0u);               /* a member founds nothing */
    CHECK_EQ(az_colony_found(w, a[1]), 2u);               /* the poor founder gives half of what it has */
    CHECK_EQ(w->colonies[2].energy, 50); CHECK_EQ(w->energy[a[1]], 50);
    CHECK_EQ(az_colony_found(w, a[2]), 3u);
    CHECK_EQ(w->totals.colony_denied, 0u);
    CHECK_EQ(az_colony_found(w, a[3]), 0u);               /* the table holds three: an event, not a crash */
    CHECK_EQ(az_colony_found(w, a[4]), 0u);
    CHECK_EQ(w->totals.colony_denied, 2u);
    CHECK(w->colony[a[3]] == 0 && w->energy[a[3]] == 10000);
    w->flags[a[5]] = 0;
    CHECK_EQ(az_colony_found(w, a[5]), 0u);               /* nor does a dead slot, and that is not a refusal */
    CHECK_EQ(w->totals.colony_denied, 2u);
    w->flags[a[5]] = AZ_F_ALIVE;
    CHECK_EQ(az_total_matter(w), m0); CHECK_EQ(az_total_energy(w), e0);

    /* colony 2 loses its only member while it holds stores and a pool */
    w->colonies[2].store[0] = 70; w->colonies[2].store[1] = 5; w->colonies[2].store[3] = 9;
    w->colonies[2].energy = 1234;
    w->colony[a[1]] = 0;
    m0 = az_total_matter(w); e0 = az_total_energy(w);
    const u32 cell = az_cell_at(w, w->colonies[2].cx, w->colonies[2].cy, w->colonies[2].cz);
    CHECK(w->c_solid[cell] != 255u);
    const i32 d0 = w->c_acc[cell].delta[az_inv_species[0]], d1 = w->c_acc[cell].delta[az_inv_species[1]],
              d3 = w->c_acc[cell].delta[az_inv_species[3]], h0 = w->c_acc[cell].dheat;
    pass(w);
    AzColony zero;
    memset(&zero, 0, sizeof zero);
    CHECK(memcmp(&w->colonies[2], &zero, sizeof zero) == 0);
    CHECK_EQ(w->c_acc[cell].delta[az_inv_species[0]] - d0, 70);
    CHECK_EQ(w->c_acc[cell].delta[az_inv_species[1]] - d1, 5);
    CHECK_EQ(w->c_acc[cell].delta[az_inv_species[3]] - d3, 9);
    CHECK_EQ(w->c_acc[cell].dheat - h0, 1234);
    CHECK_EQ(az_total_matter(w), m0); CHECK_EQ(az_total_energy(w), e0);
    CHECK(w->colonies[1].alive && w->colonies[3].alive);
    CHECK_EQ(w->colonies[1].members, 1u); CHECK_EQ(w->colonies[3].members, 1u);
    CHECK_EQ(az_colony_found(w, a[3]), 2u);               /* the free record, lowest first */
    CHECK_EQ(w->n_colonies, 4u);
    /* a membership of a colony that is not there is cleared */
    w->colony[a[4]] = 3000; w->role[a[4]] = AZ_ROLE_SCOUT;
    pass(w);
    CHECK(w->colony[a[4]] == 0 && w->role[a[4]] == AZ_ROLE_NONE);
    e0 = az_total_energy(w);
    az_tick(w);                                           /* the field takes the posts: still exact */
    CHECK_EQ(az_total_matter(w), m0); CHECK_EQ(az_total_energy(w), e0);
    CHECK_EQ(w->totals.field_underflow, 0u);
    printf("found: a founder gives %u eu to the pool; a table of 3 refused 2; a dissolved colony returned 70 + 5 + 9 mu "
           "and 1234 eu to its cell exactly; its record was founded again\n", P(w, AZ_WP_COLONY_ENDOW));
    az_test_world_free(&t);
}

/* ---- 2: what a colony is measured to be ------------------------------------------ */
static void test_measure(void)
{
    enum { N = 40 };
    AzTestWorld t;
    u32 a[N];
    if (!bench(&t, 62, 600, 0, a, N)) { CHECK(!"bench"); return; }
    AzWorld *w = t.w;
    w->param[AZ_WP_COLONY_QUIT] = 0;
    const u32 zmid = 0x80000000u;
    i64 sx = 0, sy = 0, sz = 0, sv = 0;
    for (u32 i = 0; i < N; i++) {
        make(w, a[i], 255, 0);
        w->pos_x[a[i]] = (u32)(-1000 + 50 * (i32)i);      /* either side of the wrap of x */
        w->pos_y[a[i]] = 0x40000000u + i * 1000u;
        w->pos_z[a[i]] = zmid + i * 64u;
        w->vel_x[a[i]] = (i16)((i32)i - 20); w->vel_y[a[i]] = 7; w->vel_z[a[i]] = (i16)(-3 * (i32)i);
        az_place(w, a[i]);
        w->c_mark[(size_t)w->cell[a[i]] * AZ_NMARK + AZ_MK_ALARM] = 0;
        w->health[a[i]] = (u16)(i < N / 2 ? 65535 - 25600 : 65535);   /* half of them wounded: 100 of 255 */
    }
    const u32 id = az_colony_found(w, a[0]);
    CHECK(id != 0);
    for (u32 i = 1; i < N; i++) w->colony[a[i]] = id;
    for (u32 i = 0; i < N; i++) {                          /* the test's own centre: mean offset from the founder */
        sx += (i32)(w->pos_x[a[i]] - w->pos_x[a[0]]); sy += (i32)(w->pos_y[a[i]] - w->pos_y[a[0]]);
        sz += (i64)w->pos_z[a[i]] - (i64)w->pos_z[a[0]]; sv += w->vel_x[a[i]];
    }
    const u64 m0 = az_total_matter(w);
    const i64 e0 = az_total_energy(w);
    pass(w);
    const AzColony *k = &w->colonies[id];
    CHECK_EQ(k->members, (u32)N);
    CHECK_EQ(k->cx, w->pos_x[a[0]] + (u32)(i32)(sx / N));
    CHECK_EQ(k->cy, w->pos_y[a[0]] + (u32)(i32)(sy / N));
    CHECK_EQ(k->cz, (u32)((i64)w->pos_z[a[0]] + sz / N));
    CHECK((i32)k->cx > -1000 && (i32)k->cx < 1000);        /* at the wrap, not half a world away */
    CHECK_EQ(k->vx, (i16)(sv / N)); CHECK_EQ(k->vy, 7); CHECK_EQ(k->vz, (i16)(-3 * (0 + 39) / 2));
    u32 sum = 0;
    for (u32 j = 0; j < AZ_ROLE_COUNT; j++) { sum += k->caste[j]; CHECK_EQ(k->caste[j], count_role(w, a, N, j)); }
    CHECK_EQ(sum, (u32)N);
    CHECK_EQ(k->alarm, 25);                                /* half the way (128 of 256) to a mean wound of 50 */
    CHECK_EQ(k->mood, 124);                                /* 96 of 256 of the way from 128 to 128 - 25 / 2 */
    CHECK_EQ(k->need[AZ_ROLE_SOLDIER], 64 + ((25 * 64) >> 8));
    CHECK_EQ(k->need[AZ_ROLE_NURSE], 64);                  /* no young: no demand; nobody held the role yet */
    const u32 soldiers = k->caste[AZ_ROLE_SOLDIER];
    /* alarm marks where they stand, wounds healed; ten of them young */
    for (u32 i = 0; i < N; i++) {
        w->health[a[i]] = 65535;
        w->c_mark[(size_t)w->cell[a[i]] * AZ_NMARK + AZ_MK_ALARM] = 200;
        if (i >= 30) w->flags[a[i]] |= AZ_F_JUV;
    }
    const u32 nurses = k->caste[AZ_ROLE_NURSE];
    pass(w);
    CHECK_EQ(k->alarm, 25 + ((200 - 25) * 128) / 256);
    CHECK_EQ(k->need[AZ_ROLE_SOLDIER], (u32)az_clamp_i32(70 + ((112 * 64) >> 8) - (i32)((((soldiers << 8) / N) * 128u) >> 8), 0, 255));
    CHECK_EQ(k->need[AZ_ROLE_NURSE], (u32)az_clamp_i32(64 + ((((10 * 255) / N) * 64) >> 8) - (i32)((((nurses << 8) / N) * 128u) >> 8), 0, 255));
    const u8 mood1 = k->mood;
    for (u32 i = N / 2; i < N; i++) w->colony[a[i]] = 0;  /* half of them gone: the mood falls */
    pass(w);
    CHECK_EQ(k->members, (u32)(N / 2));
    CHECK(k->mood < mood1);
    const u8 mood2 = k->mood;
    for (u32 i = N / 2; i < N; i++) { w->colony[a[i]] = id; w->c_mark[(size_t)w->cell[a[i]] * AZ_NMARK + AZ_MK_ALARM] = 0; }
    for (u32 i = 0; i < N; i++) w->c_mark[(size_t)w->cell[a[i]] * AZ_NMARK + AZ_MK_ALARM] = 0;
    pass(w); pass(w);
    CHECK(k->mood > mood2);                                /* and rises when they are back and the alarm fades */
    CHECK_EQ(az_total_matter(w), m0); CHECK_EQ(az_total_energy(w), e0);
    printf("measure: 40 members across the wrap of x have their centre at x = %d; alarm 25 from wounds, %u with alarm "
           "marks; mood 124, %u after half left, %u when they were back\n", (i32)k->cx, 25 + 87, mood2, k->mood);
    az_test_world_free(&t);
}

/* ---- 3: roles by response thresholds --------------------------------------------- */
static void calls(AzWorld *w, u32 id, u32 role_a, u32 call_a, u32 role_b, u32 call_b)
{
    memset(w->colonies[id].need, 0, AZ_ROLE_COUNT);
    if (role_a) w->colonies[id].need[role_a] = (u8)call_a;
    if (role_b) w->colonies[id].need[role_b] = (u8)call_b;
}
static void test_roles(void)
{
    enum { N = 2000 };
    AzTestWorld t;
    static u32 a[N];
    if (!bench(&t, 63, 2600, 0, a, N)) { CHECK(!"bench"); return; }
    AzWorld *w = t.w;
    w->param[AZ_WP_COLONY_NEED_GAIN] = 0; w->param[AZ_WP_COLONY_NEED_WORK] = 0;   /* the calls stay as the case sets them */
    w->param[AZ_WP_COLONY_QUIT] = 0;
    for (u32 i = 0; i < N; i++) make(w, a[i], 255, 18);    /* CASTE 18 points at FORAGER: threshold 16 */
    const u32 id = az_colony_found(w, a[0]);
    CHECK(id != 0);
    for (u32 i = 1; i < N; i++) w->colony[a[i]] = id;
    const AzColony *k = &w->colonies[id];

    /* a call of 16 against a threshold of 16: one in two takes the role */
    calls(w, id, AZ_ROLE_FORAGER, 16, 0, 0);
    pass(w);
    const u32 half = count_role(w, a, N, AZ_ROLE_FORAGER);
    CHECK(half > 910u && half < 1090u);                    /* 1000 +- 4 standard deviations */
    CHECK_EQ(k->caste[AZ_ROLE_FORAGER], half); CHECK_EQ(k->caste[AZ_ROLE_NONE], N - half);
    CHECK_EQ(k->need[AZ_ROLE_NONE], 16);                   /* what the idle hear: the loudest call */
    /* the same call for those whose CASTE points to the far side of the ring: threshold 239, 1 in 224 */
    for (u32 i = 0; i < N; i++) { w->role[a[i]] = AZ_ROLE_NONE; tr(w, a[i])[AZ_T_CASTE] = 146; }
    pass(w);
    const u32 far = count_role(w, a, N, AZ_ROLE_FORAGER);
    CHECK(far < 30u);
    /* two leanings, two equal calls: each mostly does what it leans to (0.74 against 0.08) */
    for (u32 i = 0; i < N; i++) { w->role[a[i]] = AZ_ROLE_NONE; tr(w, a[i])[AZ_T_CASTE] = (u8)(i < N / 2 ? 18 : 54); }
    calls(w, id, AZ_ROLE_FORAGER, 32, AZ_ROLE_SOLDIER, 32);
    pass(w);
    const u32 ff = count_role(w, a, N / 2, AZ_ROLE_FORAGER), fs = count_role(w, a, N / 2, AZ_ROLE_SOLDIER),
              sf = count_role(w, a + N / 2, N / 2, AZ_ROLE_FORAGER), ss = count_role(w, a + N / 2, N / 2, AZ_ROLE_SOLDIER);
    CHECK(ff > 688u && ff < 800u && ss > 688u && ss < 800u);
    CHECK(fs > 49u && fs < 121u && sf > 49u && sf < 121u);
    /* giving up: one in five (51 of 256) */
    w->param[AZ_WP_COLONY_QUIT] = 51;
    for (u32 i = 0; i < N; i++) w->role[a[i]] = AZ_ROLE_TRADER;
    calls(w, id, 0, 0, 0, 0);
    pass(w);
    const u32 quit = count_role(w, a, N, AZ_ROLE_NONE);
    CHECK(quit > 326u && quit < 470u);
    CHECK_EQ(count_role(w, a, N, AZ_ROLE_TRADER), N - quit);
    /* the loudest call, BREEDER: adults that lean to it take it, the young never; eggs and the dormant keep what they have */
    w->param[AZ_WP_COLONY_QUIT] = 256;
    for (u32 i = 0; i < N; i++) {
        w->role[a[i]] = AZ_ROLE_NONE; tr(w, a[i])[AZ_T_CASTE] = 201;
        if (i >= 1000u) w->flags[a[i]] |= AZ_F_JUV;
        if (i >= 1900u) { w->flags[a[i]] = (u8)(AZ_F_ALIVE | (i & 1u ? AZ_F_EGG : AZ_F_DORMANT)); w->role[a[i]] = AZ_ROLE_SCOUT; }
    }
    calls(w, id, AZ_ROLE_BREEDER, 255, 0, 0);
    pass(w);
    const u32 queens = count_role(w, a, 1000, AZ_ROLE_BREEDER);
    CHECK(queens > 980u);
    CHECK_EQ(count_role(w, a + 1000, 1000, AZ_ROLE_BREEDER), 0u);
    CHECK_EQ(count_role(w, a + 1900, 100, AZ_ROLE_SCOUT), 100u);
    printf("roles: call 16 against threshold 16: %u of 2000 took the role (1000 by the formula); threshold 239: %u (9); "
           "two leanings under two equal calls: %u and %u did their own, %u and %u the other's (744, 85); %u of 2000 "
           "gave up (398); %u of 1000 adults became breeders, no young one\n", half, far, ff, ss, fs, sf, quit, queens);
    az_test_world_free(&t);
}

/* ---- 4: staying and founding (COLONIALITY) ---------------------------------------- */
static void test_stay(void)
{
    enum { N = 2000 };
    AzTestWorld t;
    static u32 a[N];
    if (!bench(&t, 64, 2600, 0, a, N)) { CHECK(!"bench"); return; }
    AzWorld *w = t.w;
    for (u32 i = 0; i < N; i++) make(w, a[i], 0, 0);
    const u32 id = az_colony_found(w, a[0]);
    CHECK(id != 0);
    w->param[AZ_WP_COLONY_LEAVE] = 32;
    u32 left[3];
    for (u32 c = 0; c < 3u; c++) {                         /* no trait in a gloomy colony; the trait; a happy colony */
        for (u32 i = 0; i < N; i++) { w->colony[a[i]] = id; tr(w, a[i])[AZ_T_COLONIALITY] = (u8)(c == 1 ? 255 : 0); }
        for (u32 i = 0; i < 100u; i++) w->flags[a[i]] |= AZ_F_JUV;
        w->colonies[id].mood = (u8)(c == 2 ? 255 : 0);
        pass(w);
        left[c] = 0;
        for (u32 i = 0; i < N; i++) left[c] += w->colony[a[i]] == 0;
        for (u32 i = 0; i < 100u; i++) { CHECK_EQ(w->colony[a[i]], id); w->flags[a[i]] &= (u8)~AZ_F_JUV; }   /* the young stay */
    }
    CHECK(left[0] > 175u && left[0] < 296u);               /* 1900 x 8128 / 65536 = 236 +- 4 sd */
    CHECK_EQ(left[1], 0u); CHECK_EQ(left[2], 0u);

    /* founding: only those with the trait, only those that can pay, in uid order */
    for (u32 i = 0; i < N; i++) { make(w, a[i], i < 1000u ? 255 : 0, 0); if (i < 100u) w->energy[a[i]] = 100; }
    w->colonies[id].alive = 0;                             /* the old record: free */
    memset(&w->colonies[id], 0, sizeof(AzColony));
    w->param[AZ_WP_COLONY_LEAVE] = 0; w->param[AZ_WP_COLONY_FOUND] = 64;
    const i64 e0 = az_total_energy(w);
    pass(w);
    u32 founded = 0, poor = 0, plain = 0;                  /* founded: by the 900 of the list that can (the world's other creatures found too) */
    u64 last_uid = 0;
    for (u32 c = 1; c < w->n_colonies; c++) {
        const AzColony *k = &w->colonies[c];
        if (!k->alive) continue;
        CHECK(k->founder_uid > last_uid);                  /* ids follow the founders' uids */
        last_uid = k->founder_uid;
        CHECK_EQ(k->members, 1u);
        CHECK_EQ(k->energy, (i64)P(w, AZ_WP_COLONY_ENDOW));
    }
    for (u32 i = 0; i < N; i++) {
        if (i < 100u) poor += w->colony[a[i]] != 0;
        else if (i < 1000u) founded += w->colony[a[i]] != 0;
        else plain += w->colony[a[i]] != 0;
    }
    CHECK(founded > 172u && founded < 276u);               /* 900 x 16320 / 65536 = 224 +- 4 sd */
    CHECK_EQ(poor, 0u); CHECK_EQ(plain, 0u);
    CHECK_EQ(az_total_energy(w), e0);
    printf("stay: of 1900 adults without the trait in a colony at its lowest %u left (236 by the rule), none with the "
           "trait, none from a colony at its best, no young one; %u of 900 that could pay founded a colony (224), "
           "none of 100 that could not, none of 1000 without the trait\n", left[0], founded);
    az_test_world_free(&t);
}

/* ---- 5: culture, memory, tradition ------------------------------------------------ */
static void test_culture(void)
{
    enum { N = 20 };
    AzTestWorld t;
    u32 a[N];
    if (!bench(&t, 65, 600, 0, a, N)) { CHECK(!"bench"); return; }
    AzWorld *w = t.w;
    for (u32 i = 0; i < N; i++) make(w, a[i], 255, 0);
    const u32 id = az_colony_found(w, a[0]);
    CHECK(id != 0);
    AzColony *k = &w->colonies[id];
    for (u32 i = 0; i < N; i++) { memset(cx(w, a[i]), 40, 8); if (i < N / 2) w->colony[a[i]] = id; }
    pass(w);                                               /* ten members that think +40: 64 of 256 of the way */
    for (u32 l = 0; l < 8u; l++) CHECK_EQ(k->culture[l], 128 + (40 * 64) / 256);
    for (u32 l = 0; l < 32u; l++) CHECK_EQ(k->memory[l], 0);   /* the mood did not rise: nothing to remember */
    for (u32 i = N / 2; i < N; i++) w->colony[a[i]] = id; /* it doubles: the mood rises, the culture is remembered */
    const u8 mood0 = k->mood;
    pass(w);
    CHECK(k->mood > mood0);
    const u32 turn = (u32)(((w->tick - k->born_tick) / w->cfg.tier2_period) & 3u);
    for (u32 l = 0; l < 8u; l++) { CHECK_EQ(k->culture[l], 145); CHECK_EQ(k->memory[turn * 8u + l], 145); }
    /* the members change their minds; the colony follows, held back by what it remembers */
    for (u32 i = 0; i < N; i++) memset(cx(w, a[i]), -100, 8);
    AzColony keep = *k;
    pass(w);
    const u8 with = k->culture[0];
    CHECK_EQ(with, 116 + ((145 - 116) * 32) / 256);
    *k = keep;
    memset(k->memory, 0, 32);
    w->tick--;
    pass(w);
    CHECK_EQ(k->culture[0], 116);
    printf("culture: ten members at +40 drew the culture from 128 to 138; at the doubling it was 145 and remembered; "
           "with the members at -100 it went to %u with that memory and to 116 without\n", with);
    az_test_world_free(&t);
}

/* ---- 6: worlds ---------------------------------------------------------------------- */
typedef struct Run {
    u64 hash, births, deaths, recruits, founded, dissolved, stored, pool, voices;
    u32 live, colonies, members, roles, caste[AZ_ROLE_COUNT], ok;
} Run;
typedef struct Opt { u32 threads, reorder, lesion, resume_at, resume_threads; i32 leave, found, coloniality, caste; } Opt;

/* (the options go by pointer: a struct of this size passed by value made GCC for MinGW write its copy
 * with an aligned 256-bit move to a 16-byte aligned stack, a fault in every second run; tools/check_avx_stack.py) */
static Run world_run(u64 seed, u32 ticks, const Opt *opt)
{
    const Opt q = *opt;
    Run r;
    memset(&r, 0, sizeof r);
    AzConfig cfg = az_test_config(seed, 2000);
    cfg.reorder_period = (u8)q.reorder;
    cfg.cap = 40000;
    cfg.lesion = q.lesion;
    AzTestWorld t;
    if (!az_test_world(&t, &cfg, q.threads)) { CHECK(!"world"); return r; }
    AzWorld *w = t.w;
    if (q.leave >= 0) w->param[AZ_WP_COLONY_LEAVE] = (u32)q.leave;
    if (q.found >= 0) w->param[AZ_WP_COLONY_FOUND] = (u32)q.found;
    for (u32 s = 0; s < w->n_slots; s++) {
        if (q.coloniality >= 0) tr(w, s)[AZ_T_COLONIALITY] = (u8)q.coloniality;
        if (q.caste >= 0) tr(w, s)[AZ_T_CASTE] = (u8)q.caste;
    }
    const u64 m0 = az_total_matter(w);
    const i64 e0 = az_total_energy(w);
    u8 *was = (u8 *)calloc(w->cfg.max_colonies, 1);
    r.ok = was != NULL;
    for (u32 i = 0; i < ticks && r.ok; i++) {
        if (q.resume_at && i == q.resume_at) {             /* save, drop the world, load it on another pool */
            const u64 nb = az_snapshot_bytes(w);
            u8 *snap = (u8 *)malloc((size_t)nb + 8);
            const bool saved = snap && nb && az_snapshot_save(w, snap, nb) == nb;
            AzConfig sc;
            az_test_world_free(&t);
            w = NULL;
            if (saved && az_snapshot_config(snap, nb, &sc) && az_test_world_arena(&t, &sc)) {
                t.host = az_win_host_create(q.resume_threads, AZ_WIN_USE_ECORES, &t.info);
                if (t.host) w = t.w = az_snapshot_load(&t.arena, snap, nb, t.host);
            }
            free(snap);
            if (!w) { r.ok = 0; break; }
        }
        az_tick(w);
        if (az_total_matter(w) != m0 || az_total_energy(w) != e0) r.ok = 0;
        for (u32 c = 1; c < w->n_colonies; c++) {
            const u8 now = w->colonies[c].alive;
            r.founded += now && !was[c]; r.dissolved += !now && was[c];
            was[c] = now;
        }
    }
    free(was);
    if (w) {
        for (u32 c = 1; c < w->n_colonies; c++) {
            if (!w->colonies[c].alive) continue;
            r.colonies++;
            if (w->colonies[c].energy < 0) r.ok = 0;
            r.pool += (u64)w->colonies[c].energy; r.voices += w->colonies[c].signal[0] != 0;
            for (u32 j = 0; j < AZ_NINV; j++) r.stored += w->colonies[c].store[j];
            for (u32 j = 0; j < AZ_ROLE_COUNT; j++) r.caste[j] += w->colonies[c].caste[j];
        }
        for (u32 s = 0; s < w->n_slots; s++) {
            if (!(w->flags[s] & AZ_F_ALIVE)) continue;
            r.members += w->colony[s] != 0; r.roles += w->role[s] != AZ_ROLE_NONE;
            if (w->role[s] != AZ_ROLE_NONE && !w->colony[s]) r.ok = 0;      /* a role only inside a colony */
        }
        r.hash = az_state_hash(w); r.births = w->totals.births; r.deaths = w->totals.deaths; r.live = w->n_live;
        r.recruits = w->totals.interactions[AZ_ACT_RECRUIT];
        if (w->totals.field_underflow || w->totals.dropped_events) r.ok = 0;
    }
    az_test_world_free(&t);
    return r;
}
static void show(const char *what, const Run *r)
{
    printf("  %-22s %4" PRIu64 " founded, %3" PRIu64 " dissolved, %3u alive with %4u members (%4u with a role: F %u S %u N %u "
           "B %u Sc %u Br %u T %u), %" PRIu64 " recruited, %" PRIu64 " births, %" PRIu64 " deaths, %u alive\n", what, r->founded,
           r->dissolved, r->colonies, r->members, r->roles, r->caste[1], r->caste[2], r->caste[3], r->caste[4], r->caste[5],
           r->caste[6], r->caste[7], r->recruits, r->births, r->deaths, r->live);
    printf("  %-22s their stores hold %" PRIu64 " mu, their pools %" PRIu64 " eu (%" PRIu64 " was given at founding), %" PRIu64
           " have a voice\n", "", r->stored, r->pool, r->founded * 256u, r->voices);
}

static void test_worlds(void)
{
    const u32 T = 150;
    const Opt base = { 0, 1, 0, 0, 0, -1, 16, -1, -1 };    /* founding at 16 so that 2000 founders show colonies in 150 ticks */
    Opt q = base;
    const Run a = world_run(67, T, &q);
    q.threads = 1; q.reorder = 9; const Run b = world_run(67, T, &q);
    q.threads = 3; q.reorder = 1; const Run c = world_run(67, T, &q);
    q.threads = 8; q.reorder = 9; const Run d = world_run(67, T, &q);
    CHECK(a.ok && b.ok && c.ok && d.ok);
    CHECK_EQ(a.hash, b.hash); CHECK_EQ(a.hash, c.hash); CHECK_EQ(a.hash, d.hash);
    CHECK(a.founded > 0 && a.colonies > 0 && a.members > 0 && a.roles > 0);
    q.resume_at = 75; q.resume_threads = 3;
    const Run e = world_run(67, T, &q);                     /* saved with living colonies, resumed on 3 workers */
    CHECK(e.ok); CHECK_EQ(e.hash, a.hash);
    show("intact", &a);
    /* lesion: no colony is founded, nobody has a role, and the world is another one */
    q = base; q.lesion = AZ_LESION_COLONY;
    const Run l0 = world_run(67, T, &q);
    q.threads = 8; q.reorder = 9;
    const Run l8 = world_run(67, T, &q);
    CHECK(l0.ok && l8.ok); CHECK_EQ(l0.hash, l8.hash); CHECK(l0.hash != a.hash);
    CHECK_EQ(l0.founded, 0u); CHECK_EQ(l0.colonies, 0u); CHECK_EQ(l0.members, 0u); CHECK_EQ(l0.roles, 0u);
    show("lesion COLONY", &l0);
    /* members that all leave: colonies dissolve in a living world and the books stay exact */
    q = base; q.threads = 8; q.reorder = 9; q.leave = 256; q.coloniality = 128;
    const Run x = world_run(67, T, &q);
    CHECK(x.ok); CHECK(x.dissolved > 0);
    show("leaving at 256", &x);
    /* COLONIALITY: founders with little of it against founders with much */
    q = base; q.threads = 8; q.reorder = 9; q.coloniality = 16;
    const Run c16 = world_run(67, T, &q);
    q.coloniality = 240;
    const Run c240 = world_run(67, T, &q);
    CHECK(c16.ok && c240.ok); CHECK(c240.founded > 2 * c16.founded); CHECK(c16.hash != c240.hash);
    show("COLONIALITY 16", &c16); show("COLONIALITY 240", &c240);
    /* CASTE: founders that lean to FORAGER against founders that lean to BREEDER */
    q = base; q.threads = 8; q.reorder = 9; q.caste = 18;
    const Run k18 = world_run(67, T, &q);
    q.caste = 201;
    const Run k201 = world_run(67, T, &q);
    CHECK(k18.ok && k201.ok); CHECK(k18.hash != k201.hash);
    CHECK((u64)k18.caste[AZ_ROLE_FORAGER] * (k201.roles + 1u) > (u64)k201.caste[AZ_ROLE_FORAGER] * (k18.roles + 1u));
    CHECK((u64)k201.caste[AZ_ROLE_BREEDER] * (k18.roles + 1u) > (u64)k18.caste[AZ_ROLE_BREEDER] * (k201.roles + 1u));
    show("CASTE to FORAGER", &k18); show("CASTE to BREEDER", &k201);
    /* a second seed, run once and printed as measured, at the default founding rate */
    q = base; q.threads = 8; q.reorder = 9; q.found = -1;
    const Run s2 = world_run(68, T, &q);
    CHECK(s2.ok);
    show("seed 68, defaults", &s2);
    printf("worlds: 2000 founders, 150 ticks, a colony pass every 16: hash %016" PRIx64 " on 0, 1, 3 and 8 workers and across "
           "a snapshot; books exact every tick\n", a.hash);
}

/* ---- 7: the colony's pools ---------------------------------------------------------- */
static u32 *inv(const AzWorld *w, u32 s) { return &w->inv[(size_t)s * AZ_NINV]; }
static i8 *mot(const AzWorld *w, u32 s) { return &w->motor[(size_t)s * AZ_MOT]; }
/* an adult member of `col` with no trait but a full pantry size and a store of 100 units of energy;
 * a body of 2,000 with 2,000 in its free pool, nothing carried, its brain silent */
static void member(AzWorld *w, u32 s, u32 col)
{
    memset(tr(w, s), 0, AZ_TRAITS);
    tr(w, s)[AZ_T_CARRY_CAP] = 255; tr(w, s)[AZ_T_STORE_CAP] = 99;
    memset(mot(w, s), 0, AZ_MOT);
    w->flags[s] = (u8)(AZ_F_ALIVE | (w->flags[s] & AZ_F_INIT));
    w->energy[s] = (i32)(100u * P(w, AZ_WP_METAB_STORE_EU));
    w->pool_a[s] = 2000; w->pool_m[s] = 0; w->pool_t[s] = 0; w->mass[s] = 2000;
    w->waste[s] = 0; w->toxin[s] = 0; w->health[s] = 65535;
    w->colony[s] = col; w->role[s] = AZ_ROLE_NONE;
    for (u32 k = 0; k < AZ_NINV; k++) inv(w, s)[k] = 0;
    u32 np;
    AzPart *part = az_parts(w, s, &np);
    for (u32 i = 0; i < np; i++) part[i].matter = 0;
}
static u32 pool_steps = 0, pool_inexact = 0;
/* one pantry tick of the given members, as the tick runs it: scratch cleared, phase 1, the grants
 * (the energy pool's by the engine's rule, written out here), phase 3, the commits */
static void pulse(AzWorld *w, const u32 *m, u32 n)
{
    while (!az_phase_due(w, w->uid[m[0]], AZ_RS_ECON, 2)) w->tick++;
    AzPercept nobody;
    memset(&nobody, 0, sizeof nobody);
    nobody.near_prey = nobody.near_threat = nobody.near_kin = nobody.best_mate = nobody.bond = nobody.parent = nobody.child = AZ_NONE;
    const u64 m0 = az_total_matter(w);
    const i64 e0 = az_total_energy(w);
    for (u32 c = 1; c < w->n_colonies; c++) {
        AzColonyEph *e = &w->colony_eph[c];
        memset(e->dem, 0, sizeof e->dem); memset(e->grant, 0, sizeof e->grant);
        e->dem_e = 0; e->grant_e = 0;
        memset(e->sig_sum, 0, sizeof e->sig_sum); memset(e->sig_n, 0, sizeof e->sig_n);
    }
    for (u32 i = 0; i < n; i++) {
        CHECK(az_phase_due(w, w->uid[m[i]], AZ_RS_ECON, 2));
        az_econ_intent(w, m[i], &nobody, 0);
    }
    az_colony_grant(w);
    for (u32 c = 1; c < w->n_colonies; c++) {
        AzColonyEph *e = &w->colony_eph[c];
        const u64 have = w->colonies[c].energy > 0 ? (u64)w->colonies[c].energy : 0u;
        e->grant_e = !e->dem_e ? 0u : e->dem_e <= have ? 65535u : (u32)((have * 65535u) / e->dem_e);
    }
    for (u32 i = 0; i < n; i++) az_econ_range(w, m[i], m[i] + 1u, 0);
    az_colony_commit(w);
    for (u32 c = 1; c < w->n_colonies; c++) {
        AzColonyEph *e = &w->colony_eph[c];
        w->colonies[c].energy += (i64)e->dep_e - (i64)e->taken_e;
        e->dep_e = 0; e->taken_e = 0;
    }
    pool_inexact += az_total_matter(w) != m0 || az_total_energy(w) != e0;
    pool_steps++;
    w->tick++;
}

static void test_pools(void)
{
    AzTestWorld t;
    u32 a[64];
    if (!bench(&t, 66, 600, 0, a, 64)) { CHECK(!"bench"); return; }
    AzWorld *w = t.w;
    make(w, a[0], 255, 0);
    const u32 id = az_colony_found(w, a[0]);
    CHECK(id != 0);
    AzColony *k = &w->colonies[id];
    const u32 s = a[0];
    u32 s2 = 0;                                            /* a second member with the same pantry tick */
    for (u32 i = 1; i < 64u && !s2; i++) if (((w->uid[a[i]] ^ w->uid[s]) & 3u) == 0) s2 = a[i];
    CHECK(s2 != 0);
    const i32 store = (i32)(100u * P(w, AZ_WP_METAB_STORE_EU)), low = (i32)(((i64)store * P(w, AZ_WP_ECON_OPEN_SHARE)) >> 8);

    /* in want of energy: it asks for 127 / 128 of what it lacks and gets it; from a short pool, its share */
    member(w, s, id); w->energy[s] = low - 400; k->energy = 1000;
    pulse(w, &s, 1);
    CHECK_EQ(w->energy[s], low - 400 + 396); CHECK_EQ(k->energy, 1000 - 396);
    member(w, s, id); w->energy[s] = low - 400; k->energy = 100;
    pulse(w, &s, 1);
    CHECK_EQ(w->energy[s], low - 400 + 99); CHECK_EQ(k->energy, 1);
    /* the brain: GIVE pushed asks for nothing, GIVE pulled back asks for twice the lack */
    member(w, s, id); w->energy[s] = low - 400; k->energy = 5000; mot(w, s)[AZ_M_GIVE] = 127;
    pulse(w, &s, 1);
    CHECK_EQ(w->energy[s], low - 400); CHECK_EQ(k->energy, 5000);
    member(w, s, id); w->energy[s] = low - 400; mot(w, s)[AZ_M_GIVE] = -127;
    pulse(w, &s, 1);
    CHECK_EQ(w->energy[s], low - 400 + ((400 * 254) >> 7)); CHECK_EQ(k->energy, 5000 - ((400 * 254) >> 7));
    /* in want of matter: half its ease is 125 of free pool; it asks for 124, ORG_Y first */
    member(w, s, id); w->pool_a[s] = 0; k->store[0] = 500; k->store[1] = 100; k->energy = 0;
    pulse(w, &s, 1);
    CHECK_EQ(inv(w, s)[1], 100u); CHECK_EQ(inv(w, s)[0], 24u);
    CHECK_EQ(k->store[1], 0u); CHECK_EQ(k->store[0], 476u);
    /* two in want of a store that holds less than both ask: each gets the granted fraction, never more than is there */
    member(w, s, id); member(w, s2, id); w->pool_a[s] = 0; w->pool_a[s2] = 0; k->store[0] = 0; k->store[1] = 100;
    const u32 two[2] = { s, s2 };
    pulse(w, two, 2);
    CHECK_EQ(inv(w, s)[1], 49u); CHECK_EQ(inv(w, s2)[1], 49u); CHECK_EQ(k->store[1], 2u);
    CHECK_EQ(w->totals.field_underflow, 0u);
    w->colony[s2] = 0;
    /* giving: STORE_SHARE of the pantry, COMMONS of the energy above its store, half the top rate with the motor at rest */
    member(w, s, id); k->store[0] = 0; k->store[1] = 0; k->energy = 0;
    tr(w, s)[AZ_T_STORE_SHARE] = 255; tr(w, s)[AZ_T_COMMONS] = 255;
    inv(w, s)[0] = 1000; inv(w, s)[1] = 400; w->energy[s] = store + 1000;
    pulse(w, &s, 1);
    CHECK_EQ(k->store[0], 124u); CHECK_EQ(k->store[1], 49u); CHECK_EQ(k->energy, 124);
    CHECK_EQ(inv(w, s)[0], 876u); CHECK_EQ(w->energy[s], store + 876);
    /* a FORAGER gives twice that; without the two traits nothing is given; a brain that pulls GIVE back gives nothing */
    member(w, s, id); k->store[0] = 0; k->store[1] = 0; k->energy = 0; w->role[s] = AZ_ROLE_FORAGER;
    tr(w, s)[AZ_T_STORE_SHARE] = 255; tr(w, s)[AZ_T_COMMONS] = 255;
    inv(w, s)[0] = 1000; w->energy[s] = store + 1000;
    pulse(w, &s, 1);
    CHECK_EQ(k->store[0], 249u); CHECK_EQ(k->energy, 249);
    member(w, s, id); k->store[0] = 0; k->energy = 0;
    inv(w, s)[0] = 1000; w->energy[s] = store + 1000;
    pulse(w, &s, 1);
    CHECK_EQ(k->store[0], 0u); CHECK_EQ(k->energy, 0);
    tr(w, s)[AZ_T_STORE_SHARE] = 255; tr(w, s)[AZ_T_COMMONS] = 255; mot(w, s)[AZ_M_GIVE] = -127;
    pulse(w, &s, 1);
    CHECK_EQ(k->store[0], 0u); CHECK_EQ(k->energy, 0);
    /* a BUILDER marks the nest; the voice goes to channel 0 on every tick */
    member(w, s, id); w->role[s] = AZ_ROLE_BUILDER; mot(w, s)[AZ_M_COLONY_SIG] = 50;
    const u32 nest0 = w->c_acc[w->cell[s]].dmark[AZ_MK_NEST];
    pulse(w, &s, 1);
    CHECK_EQ(w->c_acc[w->cell[s]].dmark[AZ_MK_NEST] - nest0, (P(w, AZ_WP_ECON_NEST) * 128u) >> 8);
    CHECK_EQ(w->colony_eph[id].sig_sum[0], 178u); CHECK_EQ(w->colony_eph[id].sig_n[0], 1u);
    w->role[s] = AZ_ROLE_SCOUT;
    const u32 nest1 = w->c_acc[w->cell[s]].dmark[AZ_MK_NEST];
    pulse(w, &s, 1);
    CHECK_EQ(w->c_acc[w->cell[s]].dmark[AZ_MK_NEST], nest1);
    /* asks that were made of another colony, or in another tick, are not taken here */
    member(w, s, id); k->store[1] = 300; k->energy = 300;
    while (!az_phase_due(w, w->uid[s], AZ_RS_ECON, 2)) w->tick++;
    u32 *ask = &w->gather[(size_t)s * AZ_NINV];
    ask[0] = 0; ask[1] = 77; ask[2] = id + 1u; ask[3] = 55;
    w->colony_eph[id].grant[1] = 65535; w->colony_eph[id].grant_e = 65535;
    az_econ_range(w, s, s + 1u, 0);
    CHECK_EQ(w->colony_eph[id].taken[1], 0u); CHECK_EQ(w->colony_eph[id].taken_e, 0u); CHECK_EQ(inv(w, s)[1], 0u);
    CHECK_EQ(pool_inexact, 0u);
    printf("pools: a member 400 eu short took 396 from a pool of 1000 and 99 from a pool of 100; 793 with GIVE pulled "
           "back, nothing with GIVE pushed; short of matter it took 100 ORG_Y and 24 NUT_X; two asking 100 each of a "
           "store of 100 got 49 each; with STORE_SHARE and COMMONS 255 it gave 124 of 1000 NUT_X and 124 of 1000 eu, a "
           "FORAGER 249; a BUILDER marked its cell %u; books exact after each of %u pulses\n",
           (P(w, AZ_WP_ECON_NEST) * 128u) >> 8, pool_steps);
    az_test_world_free(&t);
}

/* RF-10. Both pools refuse excess in the normal tick, and a later withdrawal
 * cannot spend the field refund. An older signed energy debt remains payable. */
static void capped_colony_commit(void)
{
    AzConfig cfg = az_test_config(87, 16);
    cfg.lesion = AZ_LESION_COLONY;
    AzTestWorld t;
    if (!az_test_world(&t, &cfg, 0)) { CHECK(!"world creation"); return; }
    AzWorld *w = t.w;
    w->n_colonies = 2;
    const u32 cap = w->param[AZ_WP_CELL_HOLD];
    AzColony *c = &w->colonies[1];
    AzColonyEph *p = &w->colony_eph[1];
    az_atomic_add_u64(&p->dep[0], (u64)cap + 17);
    az_colony_give_energy(w, 1, cap + 35);
    const u64 m = az_total_matter(w);
    const i64 e = az_total_energy(w);
    const u64 sat_m = w->totals.sat[AZ_SAT_INVENTORY], sat_e = w->totals.sat[AZ_SAT_ENERGY];
    az_tick(w);
    CHECK_EQ(c->store[0], cap);
    CHECK_EQ(c->energy, cap);
    CHECK_EQ(w->totals.sat[AZ_SAT_INVENTORY], sat_m + 1);
    CHECK_EQ(w->totals.sat[AZ_SAT_ENERGY], sat_e + 1);
    CHECK_EQ(az_total_matter(w), m);
    CHECK_EQ(az_total_energy(w), e);
    const u64 under = w->totals.field_underflow;
    p->taken[0] = p->taken_e = (u64)cap + 1;
    az_tick(w);
    CHECK_EQ(w->totals.field_underflow, under + 2);
    CHECK_EQ(p->taken[0], (u64)cap + 1);
    CHECK_EQ(p->taken_e, (u64)cap + 1);
    CHECK_EQ(az_snapshot_bytes(w), 0);
    p->dep[0] = p->dep_e = (u64)cap + 1;
    az_tick(w);
    CHECK_EQ(p->taken[0], 0);
    CHECK_EQ(p->taken_e, 0);
    CHECK_EQ(w->totals.field_underflow, under + 2);
    c->energy = -10;
    az_colony_give_energy(w, 1, 6);
    az_tick(w);
    CHECK_EQ(c->energy, -4);
    az_colony_give_energy(w, 1, 14);
    az_tick(w);
    CHECK_EQ(c->energy, 10);
    CHECK_EQ(w->totals.field_underflow, under + 2);
    az_test_world_free(&t);
}

int main(void)
{
    capped_colony_commit();
    test_pools();
    test_found();
    test_measure();
    test_roles();
    test_stay();
    test_culture();
    test_worlds();
    return az_test_done();
}
