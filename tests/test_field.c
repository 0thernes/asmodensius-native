/* test_field.c - field plumbing (ABI 3): grant, commit, avail, pressure.
 *
 *   A. grant property: the sum of floor(demand * grant / 65535) never exceeds
 *      what the cell offers, equals the demand when it fits, and is not
 *      needlessly small; Philox-driven demands plus extreme values.
 *   B. grant and commit against a cell-by-cell reference on random sparse and
 *      dense states and arbitrary [begin, end).
 *   C. random changes posted from 8 threads through a host pool give the same
 *      cells as a serial sum.
 *   D. an excessive withdrawal raises field_underflow and leaves the totals
 *      unchanged.
 *   E. pressure is monotone in depth and matches its definition.
 *   F. marks saturate at 255 and count AZ_SAT_MARK; the ownership rule.
 *   G. a small world keeps both invariants every tick; the state hash is the
 *      same for 1, 3 and 8 threads and reorder periods 1 and 9.
 *   H. timings over 131,072 cells (printed; noisy).
 */
#include "az_test.h"

/* ---- test randomness: Philox4x32-10, counter-based ---- */
static const u32 t_key[2] = { 0x0F1E1D00u, 0x5EEDF00Du };
static u32 t_ctr = 0;
static u32 rnd(void)
{
    static u32 buf[4];
    static u32 have = 0;
    if (!have) {
        u32 c[4] = { t_ctr++, 0xF1E1Du, 0, 0 };
        az_philox4x32(buf, c, t_key);
        have = 4;
    }
    return buf[--have];
}
static u32 rnd_below(u32 n) { return (u32)(((u64)rnd() * n) >> 32); }

static const u8 t_diet_sp[AZ_NDIET] = { 0, AZ_SP_NUT_X, AZ_SP_ORG_Y, AZ_SP_WST_W, AZ_SP_OXI,
                                        AZ_SP_STR_S, AZ_SP_TOX_Z, AZ_SP_SOLV };

/* ---------------------------------------------------------------------- */
/* A. grant property                                                       */
/* ---------------------------------------------------------------------- */
/* n entities demand dem[i] of channel d in cell c that offers `avail`; the
 * grant pass runs over [b, e), which contains c and no other demand */
static void grant_case(AzWorld *w, u32 c, u32 d, const u32 *dem, u32 n, u32 avail, u32 b, u32 e)
{
    AzCellNeed *nd = &w->c_need[c];
    u16 before[AZ_NDIET];
    for (u32 k = 0; k < AZ_NDIET; k++) before[k] = w->c_grant[(size_t)c * AZ_NDIET + k];
    if (d == AZ_D_LIGHT) {
        w->c_light[c] = (u8)(avail / AZ_LIGHT_EU);
        avail = (u32)w->c_light[c] * AZ_LIGHT_EU;
    } else {
        w->c_sp[(size_t)c * AZ_NSPECIES + t_diet_sp[d]] = avail;
    }
    CHECK_EQ(az_field_avail(w, c, d), avail);

    u64 total = 0;
    for (u32 i = 0; i < n; i++) {
        az_cell_demand(w, 0, c, d, dem[i]);
        total += dem[i];
    }
    CHECK_EQ(nd->dem[d], total);
    az_field_grant(w, b, e, 0);
    u64 taken = 0;
    for (u32 i = 0; i < n; i++) taken += az_cell_granted(w, c, d, dem[i]);

    CHECK(taken <= avail);                       /* never more than is there   */
    if (total <= avail) CHECK_EQ(taken, total);  /* everything when it fits    */
    /* and not needlessly little: the two floors lose less than this */
    else CHECK(taken + total / 65535u + n + 1 > avail);
    CHECK_EQ(nd->dem[d], 0);                     /* the demand is consumed     */
    for (u32 k = 0; k < AZ_NDIET; k++)           /* no other channel is touched */
        if (k != d) CHECK_EQ(w->c_grant[(size_t)c * AZ_NDIET + k], before[k]);
}

static void test_grant_property(AzWorld *w)
{
    static u32 dem[65536];
    const u32 nc = w->n_cells;
    for (u32 trial = 0; trial < 20000; trial++) {
        u32 c = rnd_below(nc), d = rnd_below(AZ_NDIET);
        u32 n = 1 + rnd_below(64);
        u32 regime = rnd_below(6);
        u64 total = 0;
        for (u32 i = 0; i < n; i++) {
            u32 x = rnd();
            switch (regime) {
            case 0: x &= 15u; break;                       /* tiny, many zeros */
            case 1: x &= 0xFFFFu; break;
            case 2: x &= 0x3FFFFFFu; break;
            case 3: x >>= (rnd() & 31u); break;             /* every magnitude  */
            case 4: x = i ? (x & 7u) : (x | 0x80000000u); break;   /* one giant */
            default: x = (d == AZ_D_LIGHT) ? (x & 255u) : x; break;
            }
            dem[i] = x;
            total += x;
        }
        u32 avail;
        switch (rnd_below(8)) {
        case 0: avail = 0; break;
        case 1: avail = 1; break;
        case 2: avail = total > UINT32_MAX ? UINT32_MAX : (u32)total; break;
        case 3: avail = total > UINT32_MAX ? UINT32_MAX : total ? (u32)total - 1u : 0u; break;
        case 4: avail = total < 0xFFFFFFFFull ? (u32)total + 1u : 0xFFFFFFFFu; break;
        case 5: avail = total > UINT32_MAX ? rnd() : total ? rnd_below((u32)total) : 0u; break;
        case 6: avail = 0xFFFFFFFFu; break;
        default: avail = rnd() >> (rnd() & 31u); break;
        }
        /* the cell alone, the whole world, or an odd range around the cell */
        u32 b = c, e = c + 1;
        if (trial % 3 == 1) { b = 0; e = nc; }
        if (trial % 3 == 2) { b = c - rnd_below(c < 13 ? c + 1 : 13); e = c + 1 + rnd_below(nc - c < 13 ? nc - c : 13); }
        grant_case(w, c, d, dem, n, avail, b, e);
    }

    /* extremes */
    for (u32 d = 1; d < AZ_NDIET; d++) {
        u32 one = 0xFFFFFFFFu;
        grant_case(w, 5, d, &one, 1, 0xFFFFFFFFu, 0, nc);
        grant_case(w, 5, d, &one, 1, 0xFFFFFFFEu, 5, 6);
        grant_case(w, 5, d, &one, 1, 1u, 0, nc);
        grant_case(w, 5, d, &one, 1, 0u, 5, 6);
        for (u32 i = 0; i < 65535; i++) dem[i] = 65537u;   /* sums to 2^32 - 1 */
        grant_case(w, 6, d, dem, 65535, 0xFFFFFFFFu, 0, nc);
        grant_case(w, 6, d, dem, 65535, 0xFFFFFFFEu, 6, 7);
        grant_case(w, 6, d, dem, 65535, 65534u, 0, nc);
        for (u32 i = 0; i < 65535; i++) dem[i] = 1u;       /* nobody gets a unit unless all do */
        grant_case(w, 7, d, dem, 65535, 65534u, 0, nc);
        grant_case(w, 7, d, dem, 65535, 65535u, 7, 8);
    }
    /* On the complete eight-cell SIMD group and the scalar tail, a lane
     * whose low word is zero is still a request. Seed a previous full grant
     * to make either a skipped group or a skipped cell observable. */
    const u32 small = 1;
    const u32 crossing[4] = { 1u << 30, 1u << 30, 1u << 30, 1u << 30 };
    for (u32 d = 1; d < AZ_NDIET; d++) {
        for (u32 c = 0; c < 9u; c++) {
            grant_case(w, c, d, &small, 1, 900000000u, 0, 9);
            grant_case(w, c, d, crossing, 4, 900000000u, 0, 9);
            CHECK_EQ(w->c_grant[(size_t)c * AZ_NDIET + d], 13732);
        }
    }
    /* One more maximum request at the world's largest supported capacity. */
    w->c_need[0].dem[AZ_D_SOLV] = (u64)(0x7fffffc0u - 1u) * UINT32_MAX;
    az_cell_demand(w, 0, 0, AZ_D_SOLV, UINT32_MAX);
    CHECK_EQ(w->c_need[0].dem[AZ_D_SOLV], (u64)0x7fffffc0u * UINT32_MAX);
    az_field_grant(w, 0, 8, 0);
    CHECK_EQ(w->c_need[0].dem[AZ_D_SOLV], 0);
    CHECK_EQ(w->c_grant[AZ_D_SOLV], 0);

    /* light: what a cell offers is light * AZ_LIGHT_EU */
    u32 want[3] = { 600, 300, 121 };
    grant_case(w, 9, AZ_D_LIGHT, want, 3, 255u * AZ_LIGHT_EU, 0, nc);
    grant_case(w, 9, AZ_D_LIGHT, want, 3, 1021u, 9, 10);
    CHECK_EQ(az_field_avail(w, 9, AZ_NDIET), 0);            /* no such channel */
    CHECK_EQ(az_field_avail(w, 9, 0xFFFFFFFFu), 0);
}

/* ---------------------------------------------------------------------- */
/* D. underflow                                                            */
/* ---------------------------------------------------------------------- */
static void test_underflow(AzWorld *w)
{
    const u32 c = 77, k = AZ_SP_ORG_Y;
    u32 *sp = &w->c_sp[(size_t)c * AZ_NSPECIES];
    i32 *dl = w->c_acc[c].delta;
    az_tally_merge(w);
    u64 uf0 = w->totals.field_underflow;

    sp[k] = 5; sp[AZ_SP_WST_W] = 9;
    const u64 m0 = az_total_matter(w);
    az_cell_post(w, 0, c, k, -6);                 /* one more than is there      */
    az_cell_post(w, 0, c, AZ_SP_WST_W, -9);       /* a legal withdrawal beside it */
    CHECK_EQ(az_total_matter(w), m0 - 15);
    az_field_commit(w, 0, w->n_cells, 0);
    az_tally_merge(w);
    CHECK_EQ(w->totals.field_underflow, uf0 + 1);
    CHECK_EQ(sp[k], 5);                           /* not applied                 */
    CHECK_EQ(dl[k], -6);                          /* still pending               */
    CHECK_EQ(sp[AZ_SP_WST_W], 0);                 /* the legal one went through  */
    CHECK_EQ(dl[AZ_SP_WST_W], 0);
    CHECK_EQ(az_total_matter(w), m0 - 15);        /* commit moved nothing in or out */

    az_cell_post(w, 0, c, k, +1);                 /* the debt is covered later   */
    az_field_commit(w, 0, w->n_cells, 0);
    az_tally_merge(w);
    CHECK_EQ(w->totals.field_underflow, uf0 + 1);
    CHECK_EQ(sp[k], 0);
    CHECK_EQ(dl[k], 0);
    CHECK_EQ(az_total_matter(w), m0 - 14);

    /* Surplus at the top of the range stays booked and covers a later withdrawal. */
    sp[k] = 0xFFFFFFFFu;
    const u64 m1 = az_total_matter(w);
    az_cell_post(w, 0, c, k, +1);
    az_field_commit(w, c, c + 1, 0);
    az_tally_merge(w);
    CHECK_EQ(w->totals.field_underflow, uf0 + 1);
    CHECK_EQ(sp[k], UINT32_MAX);
    CHECK_EQ(az_pending_species(w, c, k), 1);
    CHECK_EQ(az_total_matter(w), m1 + 1);
    az_cell_post(w, 0, c, k, -3);
    az_field_commit(w, c, c + 1, 0);
    CHECK_EQ(sp[k], UINT32_MAX - 2u);
    CHECK_EQ(az_pending_species(w, c, k), 0);
    CHECK_EQ(az_total_matter(w), m1 - 2);
    sp[k] = 400;

    /* heat */
    w->c_heat[c] = 10;
    const i64 e1 = az_total_energy(w);
    az_cell_post_heat(w, 0, c, -11);
    az_field_commit(w, c, c + 1, 0);
    az_tally_merge(w);
    CHECK_EQ(w->totals.field_underflow, uf0 + 2);
    CHECK_EQ(w->c_heat[c], 10);
    CHECK_EQ(w->c_acc[c].dheat, -11);
    CHECK_EQ(az_total_energy(w), e1 - 11);
    az_cell_post_heat(w, 0, c, +11);
    az_field_commit(w, c, c + 1, 0);
    CHECK_EQ(w->c_heat[c], 10);
    CHECK_EQ(w->c_acc[c].dheat, 0);
    CHECK_EQ(az_total_energy(w), e1);
}

/* ---------------------------------------------------------------------- */
/* E. pressure                                                             */
/* ---------------------------------------------------------------------- */
static u32 ref_pressure(const AzWorld *w, u32 c)
{
    u32 x = az_cell_x(w, c), y = az_cell_y(w, c), z = az_cell_z(w, c);
    u64 sum = 0;
    for (u32 zz = z; zz < w->gz; zz++) {
        const u32 *sp = &w->c_sp[(size_t)az_cell_xyz(w, x, y, zz) * AZ_NSPECIES];
        sum += (u64)sp[AZ_SP_GAS] + 4ull * sp[AZ_SP_SOLV];
    }
    sum >>= 6;
    return sum > 0xFFFFFFFFull ? 0xFFFFFFFFu : (u32)sum;
}

static void check_pressure(const AzWorld *w)
{
    u32 bad_ref = 0, bad_mono = 0;
    for (u32 c = 0; c < w->n_cells; c++) {
        u32 p = az_cell_pressure(w, c);
        if (p != ref_pressure(w, c)) bad_ref++;
        u32 up = az_cell_nb(w, c, 0, 0, 1);
        if (up != AZ_NONE && az_cell_pressure(w, up) > p) bad_mono++;   /* deeper = never less */
    }
    CHECK_EQ(bad_ref, 0);
    CHECK_EQ(bad_mono, 0);
}

static void test_pressure(AzWorld *w)
{
    check_pressure(w);                                   /* the worldgen state */
    for (u32 round = 0; round < 12; round++) {
        for (u32 c = 0; c < w->n_cells; c++) {
            u32 *sp = &w->c_sp[(size_t)c * AZ_NSPECIES];
            u32 r = rnd();
            sp[AZ_SP_GAS]  = (r & 3u) == 0 ? 0u : (r & 3u) == 1 ? 0xFFFFFFFFu : rnd() >> (r >> 27);
            sp[AZ_SP_SOLV] = (r & 12u) == 0 ? 0u : (r & 12u) == 4 ? 0xFFFFFFFFu : rnd() >> ((r >> 22) & 31u);
        }
        check_pressure(w);
    }
    /* 64 units of gas per layer: exactly one pressure unit per layer of depth */
    for (u32 c = 0; c < w->n_cells; c++) {
        u32 *sp = &w->c_sp[(size_t)c * AZ_NSPECIES];
        sp[AZ_SP_GAS] = 64; sp[AZ_SP_SOLV] = 0;
    }
    for (u32 z = 0; z < w->gz; z++) CHECK_EQ(az_cell_pressure(w, az_cell_xyz(w, 3, 2, z)), w->gz - z);
    /* solvent weighs four times as much, and only the column above counts */
    w->c_sp[(size_t)az_cell_xyz(w, 3, 2, w->gz - 1) * AZ_NSPECIES + AZ_SP_SOLV] = 64;
    CHECK_EQ(az_cell_pressure(w, az_cell_xyz(w, 3, 2, 0)), w->gz + 4);
    CHECK_EQ(az_cell_pressure(w, az_cell_xyz(w, 4, 2, 0)), w->gz);
    for (u32 c = 0; c < w->n_cells; c++) {               /* back to a plain medium */
        u32 *sp = &w->c_sp[(size_t)c * AZ_NSPECIES];
        sp[AZ_SP_GAS] = 0; sp[AZ_SP_SOLV] = 1000;
    }
}

/* ---------------------------------------------------------------------- */
/* F. marks                                                                */
/* ---------------------------------------------------------------------- */
static void test_marks(AzWorld *w)
{
    const u32 c = 123;
    u8 *mk = &w->c_mark[(size_t)c * AZ_NMARK];
    u32 *dm = w->c_acc[c].dmark;
    az_tally_merge(w);
    u64 sat0 = w->totals.sat[AZ_SAT_MARK];

    /* reaching 255 exactly is not a clamp; passing it is, however far */
    mk[0] = 250; mk[1] = 250; mk[2] = 255; mk[3] = 0;
    dm[0] = 5; dm[1] = 6; dm[2] = 0xFFFFFFFFu; dm[3] = 255;
    az_field_commit(w, c, c + 1, 0);
    az_tally_merge(w);
    CHECK_EQ(mk[0], 255); CHECK_EQ(mk[1], 255); CHECK_EQ(mk[2], 255); CHECK_EQ(mk[3], 255);
    CHECK_EQ(dm[0] | dm[1] | dm[2] | dm[3], 0);
    CHECK_EQ(w->totals.sat[AZ_SAT_MARK], sat0 + 2);
    az_cell_mark(w, 0, c, 3, 1, 0);
    az_field_commit(w, 0, w->n_cells, 0);
    az_tally_merge(w);
    CHECK_EQ(mk[3], 255);
    CHECK_EQ(w->totals.sat[AZ_SAT_MARK], sat0 + 3);

    /* Several valid public posts can cross 2^32 in the same cell. Their
     * combined cue saturates once per channel and the strongest claim wins. */
    u64 before_many = w->totals.sat[AZ_SAT_MARK];
    for (u32 k = 0; k < AZ_NMARK; k++) {
        mk[k] = 0;
        az_cell_mark(w, 0, c, k, k & 1u ? 1u : UINT32_MAX, 7);
        az_cell_mark(w, 0, c, k, k & 1u ? UINT32_MAX : 1u, 11);
    }
    az_field_commit(w, c, c + 1, 0);
    az_tally_merge(w);
    for (u32 k = 0; k < AZ_NMARK; k++) {
        CHECK_EQ(mk[k], 255);
        CHECK_EQ(dm[k], 0);
    }
    CHECK_EQ(w->c_mark_id[c], 7);
    CHECK_EQ(w->totals.sat[AZ_SAT_MARK], before_many + AZ_NMARK);

    /* ownership: a post takes the cell when its strength is at least the
     * standing mark[0]; the standing mark is the one before this commit */
    mk[0] = 100; w->c_mark_id[c] = 11;
    az_cell_mark(w, 0, c, 0, 99, 7);
    az_field_commit(w, c, c + 1, 0);
    CHECK_EQ(w->c_mark_id[c], 11);            /* too weak: the owner stays    */
    CHECK_EQ(w->c_acc[c].dmark_id, 0);        /* and the post is consumed     */
    CHECK_EQ(mk[0], 199);

    mk[0] = 100;
    az_cell_mark(w, 0, c, 0, 100, 7);
    az_field_commit(w, 0, w->n_cells, 0);
    CHECK_EQ(w->c_mark_id[c], 7);             /* equal strength is enough     */
    CHECK_EQ(w->c_acc[c].dmark_id, 0);
    CHECK_EQ(mk[0], 200);

    /* the strongest post wins whatever the order; equal strength: higher id */
    mk[0] = 60; w->c_mark_id[c] = 11;
    az_cell_mark(w, 0, c, 0, 50, 9);
    az_cell_mark(w, 0, c, 0, 61, 3);
    az_cell_mark(w, 0, c, 0, 61, 2);
    az_cell_mark(w, 0, c, 0, 55, 4000);
    az_field_commit(w, c, c + 1, 0);
    CHECK_EQ(w->c_mark_id[c], 3);
    CHECK_EQ(mk[0], 255);                     /* 60 + 227 saturates           */

    /* several weak posts do not add up to a claim */
    mk[0] = 60; w->c_mark_id[c] = 11;
    az_cell_mark(w, 0, c, 0, 40, 9);
    az_cell_mark(w, 0, c, 0, 40, 8);
    az_field_commit(w, c, c + 1, 0);
    CHECK_EQ(w->c_mark_id[c], 11);
    CHECK_EQ(mk[0], 140);

    /* a deposit on another channel is not a claim, however strong */
    mk[0] = 10; mk[1] = 0; w->c_mark_id[c] = 11;
    az_cell_mark(w, 0, c, 1, 200, 5);
    az_field_commit(w, c, c + 1, 0);
    CHECK_EQ(w->c_mark_id[c], 11);
    CHECK_EQ(mk[1], 200);

    /* a full standing mark yields only to a full-strength post */
    mk[0] = 255; w->c_mark_id[c] = 11;
    az_cell_mark(w, 0, c, 0, 254, 5);
    az_field_commit(w, c, c + 1, 0);
    CHECK_EQ(w->c_mark_id[c], 11);
    az_cell_mark(w, 0, c, 0, 255, 5);
    az_field_commit(w, c, c + 1, 0);
    CHECK_EQ(w->c_mark_id[c], 5);

    /* an unmarked cell goes to whoever posts, however weakly */
    mk[0] = 0; w->c_mark_id[c] = 0;
    az_cell_mark(w, 0, c, 0, 0, 42);
    az_field_commit(w, c, c + 1, 0);
    CHECK_EQ(w->c_mark_id[c], 42);
    CHECK_EQ(mk[0], 0);
    mk[0] = mk[1] = mk[2] = mk[3] = 0; w->c_mark_id[c] = 0;
}

/* ---------------------------------------------------------------------- */
/* B. the range functions against a cell-by-cell reference                 */
/* ---------------------------------------------------------------------- */
static void ref_grant(AzWorld *w, u32 b, u32 e)
{
    for (u32 c = b; c < e; c++) {
        AzCellNeed *nd = &w->c_need[c];
        for (u32 d = 0; d < AZ_NDIET; d++) {
            u64 dem = nd->dem[d];
            if (!dem) continue;
            u32 avail = d == AZ_D_LIGHT ? (u32)w->c_light[c] * AZ_LIGHT_EU
                                        : w->c_sp[(size_t)c * AZ_NSPECIES + t_diet_sp[d]];
            w->c_grant[(size_t)c * AZ_NDIET + d] = dem <= avail ? 65535u : (u16)(((u64)avail * 65535u) / dem);
            nd->dem[d] = 0;
        }
    }
}

static void ref_commit(AzWorld *w, u32 b, u32 e, u64 *underflow, u64 *sat)
{
    for (u32 c = b; c < e; c++) {
        AzCellAcc *a = &w->c_acc[c];
        u32 *sp = &w->c_sp[(size_t)c * AZ_NSPECIES];
        for (u32 k = 0; k < AZ_NSPECIES; k++) {
            u32 *carry = &w->c_species_carry[(size_t)c * AZ_NSPECIES + k];
            if (!a->delta[k] && !*carry) continue;
            const __int128 unit = (__int128)1 << 32;
            __int128 pending = (__int128)*carry * unit + a->delta[k];
            if (pending > INT64_MAX) pending -= unit * unit;
            __int128 physical = sp[k] + pending;
            if (physical < 0) { (*underflow)++; continue; }
            sp[k] = physical > UINT32_MAX ? UINT32_MAX : (u32)physical;
            __int128 left = physical - sp[k];
            /* Closest high limb with a signed low, independently of commit. */
            *carry = (u32)((left + unit / 2) / unit);
            a->delta[k] = (i32)(u32)left;
        }
        if (a->dheat || a->heat_carry) {
            __int128 h = (__int128)w->c_heat[c] + az_pending_heat(a);
            if (h < 0) (*underflow)++;
            else {
                w->c_heat[c] = h > UINT32_MAX ? UINT32_MAX : (u32)h;
                az_set_pending_heat(a, (i64)(h - w->c_heat[c]));
            }
        }
        u8 *mk = &w->c_mark[(size_t)c * AZ_NMARK];
        if (a->dmark_id && (a->dmark_id >> 32) >= mk[0]) w->c_mark_id[c] = (u32)a->dmark_id;
        a->dmark_id = 0;
        for (u32 k = 0; k < AZ_NMARK; k++) {
            if (!a->dmark[k]) continue;
            u64 v = (u64)mk[k] + a->dmark[k];
            if (v > 255) { v = 255; (*sat)++; }
            mk[k] = (u8)v;
            a->dmark[k] = 0;
        }
    }
}

static void cells_copy(AzWorld *dst, const AzWorld *src)
{
    for (u32 i = 0; i < az_n_cell_columns(); i++) {
        AzColDesc a, b;
        az_cell_column(src, i, &a);
        az_cell_column(dst, i, &b);
        memcpy(b.data, a.data, (size_t)src->n_cells * a.per * a.elem);
    }
}

/* number of cell columns (FIELD and EPH) that differ between two worlds */
static u32 cells_diff(const AzWorld *x, const AzWorld *y, const char *what)
{
    u32 bad = 0;
    for (u32 i = 0; i < az_n_cell_columns(); i++) {
        AzColDesc a, b;
        az_cell_column(x, i, &a);
        az_cell_column(y, i, &b);
        if (memcmp(a.data, b.data, (size_t)x->n_cells * a.per * a.elem) == 0) continue;
        if (!bad) printf("  %s: cell column '%s' differs\n", what, a.name);
        bad++;
    }
    return bad;
}

static u32 rnd_amount(void)   /* zero, the top of the range, small, or any magnitude */
{
    u32 r = rnd();
    switch (r & 7u) {
    case 0: return 0u;
    case 1: return 0xFFFFFFFFu;
    case 2: case 3: return (r >> 3) & 1023u;
    default: return rnd() >> ((r >> 3) & 31u);
    }
}

/* random field state; a cell has pending work with probability 2^-shift */
static void scramble(AzWorld *w, u32 shift)
{
    for (u32 c = 0; c < w->n_cells; c++) {
        u32 *sp = &w->c_sp[(size_t)c * AZ_NSPECIES];
        u8 *mk = &w->c_mark[(size_t)c * AZ_NMARK];
        AzCellNeed *nd = &w->c_need[c];
        AzCellAcc *a = &w->c_acc[c];
        memset(a, 0, sizeof *a);
        memset(&w->c_species_carry[(size_t)c * AZ_NSPECIES], 0, AZ_NSPECIES * sizeof(u32));
        memset(nd, 0, sizeof *nd);
        memset(&w->c_grant[(size_t)c * AZ_NDIET], 0, AZ_NDIET * sizeof(u16));
        for (u32 k = 0; k < AZ_NSPECIES; k++) sp[k] = rnd_amount();
        w->c_heat[c] = rnd_amount();
        u32 r = rnd();
        for (u32 k = 0; k < AZ_NMARK; k++) mk[k] = (u8)(r >> (8 * k));
        r = rnd();
        w->c_mark_id[c] = (r & 1u) ? r : (r & 0xFFFFu);   /* colony ids are 32-bit */
        w->c_light[c] = (u8)(r >> 16);
        /* grants left over from an earlier tick */
        if ((r >> 24) & 1u) for (u32 d = 0; d < AZ_NDIET; d++) w->c_grant[(size_t)c * AZ_NDIET + d] = (u16)rnd();

        if (shift && (shift >= 32 || rnd_below(1u << shift))) continue;
        u32 m = rnd();   /* which accumulators of this cell are pending */
        for (u32 k = 0; k < AZ_NSPECIES; k++) {
            if (!((m >> k) & 1u)) continue;
            u32 q = rnd();
            a->delta[k] = (q & 15u) == 0 ? (i32)q : (i32)((q >> 4) & 2047u) - 1024;
            if ((q & 31u) == 0)
                w->c_species_carry[(size_t)c * AZ_NSPECIES + k] = (q & 32u) ? 1u : UINT32_MAX;
        }
        if ((m >> 8) & 1u) {
            u32 q = rnd();
            a->dheat = (q & 15u) == 0 ? (i32)q : (i32)((q >> 4) & 2047u) - 1024;
        }
        for (u32 k = 0; k < AZ_NMARK; k++) {
            if (!((m >> (9 + k)) & 1u)) continue;
            u32 q = rnd();
            a->dmark[k] = (q & 3u) == 0 ? rnd() : (q >> 2) & 255u;
        }
        if ((m >> 13) & 1u) {
            u32 q = rnd();
            u32 strength = (q & 1u) ? (q >> 1) & 511u : q >> 16;
            a->dmark_id = ((u64)strength << 32) | ((q & 2u) ? rnd() : (rnd() & 0xFFFFu));
        }
        for (u32 d = 0; d < AZ_NDIET; d++)
            if ((m >> (14 + d)) & 1u) nd->dem[d] = (u64)rnd_amount() + ((u64)(rnd() & 3u) << 32);
    }
}

static void test_reference(AzWorld *w, AzWorld *r)
{
    static const u32 shifts[5] = { 0, 1, 4, 8, 32 };
    static const char *const names[2] = { "grant", "commit" };
    const u32 n = w->n_cells;
    u64 seen_underflow = 0, seen_sat = 0;
    for (u32 round = 0; round < 300; round++) {
        u32 b, e;
        switch (round % 6) {
        case 0: b = 0; e = n; break;
        case 1: b = rnd_below(n); e = b + rnd_below(n - b + 1); break;
        case 2: b = 64 * rnd_below(n / 64); e = b + 64 * rnd_below((n - b) / 64 + 1); break;
        case 3: b = rnd_below(n - 20); e = b + rnd_below(20); break;
        case 4: b = e = rnd_below(n); break;
        default: b = rnd_below(8); e = n - rnd_below(8); break;
        }
        for (u32 op = 0; op < 2; op++) {
            scramble(w, shifts[round % 5]);
            cells_copy(r, w);
            memset(az_tally(w, 0), 0, sizeof(AzTally));
            u64 uf = 0, sat = 0;
            if (op == 0) { az_field_grant(w, b, e, 0);  ref_grant(r, b, e); }
            else         { az_field_commit(w, b, e, 0); ref_commit(r, b, e, &uf, &sat); }
            CHECK_EQ(cells_diff(w, r, names[op]), 0);
            CHECK_EQ(az_tally(w, 0)->field_underflow, uf);
            CHECK_EQ(az_tally(w, 0)->sat[AZ_SAT_MARK], sat);
            seen_underflow += uf;
            seen_sat += sat;
        }
    }
    /* the random states did reach the guarded paths */
    CHECK(seen_underflow > 1000);
    CHECK(seen_sat > 1000);
    memset(az_tally(w, 0), 0, sizeof(AzTally));
}

/* ---------------------------------------------------------------------- */
/* C. posts from 8 threads against a serial sum                            */
/* ---------------------------------------------------------------------- */
#define C_POSTS 400000u
#define C_PARTS 8u

typedef struct CPost { u32 cell, species, channel, strength, colony; i32 d_mu, d_eu; } CPost;

static CPost c_post(const AzWorld *w, u32 i)
{
    u32 ctr[4] = { i, 0xC0FFEEu, 1, 0 }, o[4];
    az_philox4x32(o, ctr, t_key);
    CPost p;
    p.cell = o[0] % w->n_cells;
    p.species = o[1] & 7u;
    p.d_mu = (i32)((o[1] >> 3) % 201u) - 100;
    p.d_eu = (i32)(o[2] & 0xFFFu) - 2048;
    p.colony = o[3] & 0xFFFu;
    if (((o[2] >> 16) & 63u) == 0) {       /* about one post in 64 claims the cell */
        p.channel = 0;
        p.strength = (o[3] >> 16) & 255u;
    } else {                               /* the rest scent channels 1..3         */
        p.channel = 1u + ((o[2] >> 12) & 3u) % 3u;
        p.strength = (o[2] >> 14) & 3u;
    }
    /* Every partition sends its first claim to one shared cell. The total
     * crosses the pending u32 limit even if the workers arrive in reverse. */
    if (i % (C_POSTS / C_PARTS) == 0) {
        p.cell = 0;
        p.channel = 0;
        p.strength = UINT32_MAX / 2u;
    }
    return p;
}

static void job_post(AzWorld *w, u32 b, u32 e, u32 worker)
{
    for (u32 i = b; i < e; i++) {
        CPost p = c_post(w, i);
        az_cell_demand(w, worker, 0, AZ_D_SOLV, UINT32_MAX);
        az_cell_post(w, worker, p.cell, p.species, p.d_mu);
        az_cell_post_heat(w, worker, p.cell, p.d_eu);
        az_cell_mark(w, worker, p.cell, p.channel, p.strength, p.colony);
    }
}

static void job_commit(AzWorld *w, u32 b, u32 e, u32 worker) { az_field_commit(w, b, e, worker); }
static void job_grant(AzWorld *w, u32 b, u32 e, u32 worker) { az_field_grant(w, b, e, worker); }

static void c_base_state(AzWorld *w)
{
    for (u32 c = 0; c < w->n_cells; c++) {
        u32 ctr[4] = { c, 0xBA5Eu, 2, 0 }, o[4];
        az_philox4x32(o, ctr, t_key);
        for (u32 k = 0; k < AZ_NSPECIES; k++) w->c_sp[(size_t)c * AZ_NSPECIES + k] = 1000000u + ((o[0] >> k) & 255u);
        w->c_heat[c] = 1000000u + (o[1] & 0xFFFFu);
        for (u32 k = 0; k < AZ_NMARK; k++) w->c_mark[(size_t)c * AZ_NMARK + k] = (u8)(o[2] >> (8 * k));
        w->c_mark_id[c] = 0x80008000u | (o[3] & 0xFFFu);
    }
}

static void test_threads(void)
{
    AzConfig cfg = az_test_config(12, 64);
    AzTestWorld tp, ts;
    if (!az_test_world(&tp, &cfg, C_PARTS) || !az_test_world(&ts, &cfg, 0)) { CHECK(!"world creation"); return; }
    AzWorld *w = tp.w, *s = ts.w;
    const u32 n = w->n_cells;
    CHECK_EQ(tp.host->n_workers, C_PARTS);
    c_base_state(w);
    c_base_state(s);
    CHECK_EQ(cells_diff(w, s, "threads: base"), 0);

    /* the serial sum, in plain arrays */
    i64 *x_sp = calloc((size_t)n * AZ_NSPECIES, sizeof(i64));
    i64 *x_heat = calloc(n, sizeof(i64));
    u64 *x_mark = calloc((size_t)n * AZ_NMARK, sizeof(u64));
    u64 *x_id = calloc(n, sizeof(u64));
    i64 net_mu = 0, net_eu = 0;
    for (u32 i = 0; i < C_POSTS; i++) {
        CPost p = c_post(w, i);
        x_sp[(size_t)p.cell * AZ_NSPECIES + p.species] += p.d_mu;
        x_heat[p.cell] += p.d_eu;
        x_mark[(size_t)p.cell * AZ_NMARK + p.channel] += p.strength;
        if (p.channel == 0) {
            u64 id = ((u64)p.strength << 32) | p.colony;
            if (id > x_id[p.cell]) x_id[p.cell] = id;
        }
        net_mu += p.d_mu;
        net_eu += p.d_eu + (i64)az_eps_species[p.species] * p.d_mu;
    }
    u64 x_sat = 0;
    u32 bad_acc = 0, bad_sp = 0, bad_heat = 0, bad_mark = 0, bad_id = 0, n_taken = 0, n_kept = 0;

    const u64 m0 = az_total_matter(w);
    const i64 e0 = az_total_energy(w);
    u32 splits[C_PARTS + 1];
    for (u32 k = 0; k <= C_PARTS; k++) splits[k] = (u32)((u64)C_POSTS * k / C_PARTS);
    tp.host->parallel_for(tp.host->ctx, w, job_post, splits, C_PARTS);
    CHECK_EQ(az_total_matter(w), (i64)m0 + net_mu);    /* pending changes are already on the books */
    CHECK_EQ(az_total_energy(w), e0 + net_eu);
    for (u32 c = 0; c < n; c++) {                      /* the accumulators hold the serial sums */
        const AzCellAcc *a = &w->c_acc[c];
        for (u32 k = 0; k < AZ_NSPECIES; k++) bad_acc += a->delta[k] != x_sp[(size_t)c * AZ_NSPECIES + k];
        bad_acc += a->dheat != x_heat[c];
        for (u32 k = 0; k < AZ_NMARK; k++) {
            u64 total = x_mark[(size_t)c * AZ_NMARK + k];
            u32 expected = total > UINT32_MAX ? UINT32_MAX : (u32)total;
            bad_acc += a->dmark[k] != expected;
        }
        bad_acc += a->dmark_id != x_id[c];
    }
    CHECK(x_mark[0] > UINT32_MAX);
    CHECK_EQ(bad_acc, 0);
    CHECK_EQ(w->c_need[0].dem[AZ_D_SOLV], (u64)C_POSTS * UINT32_MAX);

    /* expected cells from the state before the commit */
    for (u32 c = 0; c < n; c++) {
        for (u32 k = 0; k < AZ_NSPECIES; k++) x_sp[(size_t)c * AZ_NSPECIES + k] += w->c_sp[(size_t)c * AZ_NSPECIES + k];
        x_heat[c] += w->c_heat[c];
        u32 standing = w->c_mark[(size_t)c * AZ_NMARK];
        for (u32 k = 0; k < AZ_NMARK; k++) {
            u64 v = x_mark[(size_t)c * AZ_NMARK + k] + w->c_mark[(size_t)c * AZ_NMARK + k];
            if (v > 255) { v = 255; x_sat++; }
            x_mark[(size_t)c * AZ_NMARK + k] = v;
        }
        if (x_id[c] && (x_id[c] >> 32) >= standing) { x_id[c] &= 0xFFFFFFFFull; n_taken++; }
        else { if (x_id[c]) n_kept++; x_id[c] = w->c_mark_id[c]; }
    }

    /* commit on 8 workers, with boundaries that are not multiples of 8 */
    for (u32 k = 1; k < C_PARTS; k++) splits[k] = (u32)((u64)n * k / C_PARTS) + 3u * k;
    splits[0] = 0; splits[C_PARTS] = n;
    tp.host->parallel_for(tp.host->ctx, w, job_commit, splits, C_PARTS);
    az_tally_merge(w);

    for (u32 c = 0; c < n; c++) {
        const AzCellAcc *a = &w->c_acc[c];
        for (u32 k = 0; k < AZ_NSPECIES; k++)
            if ((i64)w->c_sp[(size_t)c * AZ_NSPECIES + k] != x_sp[(size_t)c * AZ_NSPECIES + k] || a->delta[k]) bad_sp++;
        if ((i64)w->c_heat[c] != x_heat[c] || a->dheat) bad_heat++;
        for (u32 k = 0; k < AZ_NMARK; k++)
            if (w->c_mark[(size_t)c * AZ_NMARK + k] != x_mark[(size_t)c * AZ_NMARK + k] || a->dmark[k]) bad_mark++;
        if (w->c_mark_id[c] != x_id[c] || a->dmark_id) bad_id++;
    }
    CHECK_EQ(bad_sp, 0);
    CHECK_EQ(bad_heat, 0);
    CHECK_EQ(bad_mark, 0);
    CHECK_EQ(bad_id, 0);
    CHECK_EQ(w->totals.sat[AZ_SAT_MARK], x_sat);
    CHECK_EQ(w->totals.field_underflow, 0);
    CHECK_EQ(az_total_matter(w), (i64)m0 + net_mu);
    CHECK_EQ(az_total_energy(w), e0 + net_eu);
    CHECK(x_sat > 100);        /* the run saturated marks ...            */
    CHECK(n_taken > 100);      /* ... changed owners ...                 */
    CHECK(n_kept > 100);       /* ... and refused weaker claims          */

    /* the same posts and one commit on a world without a host */
    job_post(s, 0, C_POSTS, 0);
    az_field_commit(s, 0, n, 0);
    az_tally_merge(s);
    CHECK_EQ(cells_diff(w, s, "threads: 8 workers vs serial"), 0);
    CHECK_EQ(s->totals.sat[AZ_SAT_MARK], x_sat);
    CHECK_EQ(az_total_matter(s), az_total_matter(w));
    CHECK_EQ(az_total_energy(s), az_total_energy(w));

    free(x_sp); free(x_heat); free(x_mark); free(x_id);
    az_test_world_free(&tp);
    az_test_world_free(&ts);
}

/* ---------------------------------------------------------------------- */
/* G. a small world, every tick                                            */
/* ---------------------------------------------------------------------- */
static u64 run_world(u64 seed, u32 threads, u8 reorder)
{
    AzConfig cfg = az_test_config(seed, 2000);
    cfg.reorder_period = reorder;
    AzTestWorld t;
    if (!az_test_world(&t, &cfg, threads)) { CHECK(!"world creation"); return 0; }
    AzWorld *w = t.w;
    size_t nsp = (size_t)w->n_cells * AZ_NSPECIES;
    u32 *sp0 = malloc(nsp * sizeof(u32));
    memcpy(sp0, w->c_sp, nsp * sizeof(u32));
    const u64 m0 = az_total_matter(w);
    const i64 e0 = az_total_energy(w);
    for (u32 i = 0; i < 40; i++) {
        az_tick(w);
        CHECK_EQ(az_total_matter(w), m0);
        CHECK_EQ(az_total_energy(w), e0);
        CHECK_EQ(w->totals.field_underflow, 0);
    }
    u32 changed = 0;
    for (size_t i = 0; i < nsp; i++) changed += w->c_sp[i] != sp0[i];
    CHECK(changed > 0);        /* the commits did move matter in the fields */
    u64 h = az_state_hash(w);
    free(sp0);
    az_test_world_free(&t);
    return h;
}

static void test_world(void)
{
    static const u64 seeds[2] = { 3, 51 };   /* tuning seeds; 5001..5024 and 5101..5124 are reserved */
    for (u32 k = 0; k < 2; k++) {
        u64 h0 = run_world(seeds[k], 0, 1);
        u64 h1 = run_world(seeds[k], 1, 1);
        u64 h3 = run_world(seeds[k], 3, 1);
        u64 h8 = run_world(seeds[k], 8, 1);
        u64 h9 = run_world(seeds[k], 0, 9);
        u64 h89 = run_world(seeds[k], 8, 9);
        printf("  seed %-4" PRIu64 " hash %016" PRIx64 " (serial; checked against 1, 3, 8 threads and reorder 9)\n", seeds[k], h0);
        CHECK(h0 == h1); CHECK(h0 == h3); CHECK(h0 == h8);
        CHECK(h0 == h9); CHECK(h0 == h89);
    }
    CHECK(run_world(3, 0, 1) != run_world(4, 0, 1));
}

/* ---------------------------------------------------------------------- */
/* H. timings over 131,072 cells                                           */
/* ---------------------------------------------------------------------- */
enum { K_GRANT = 0, K_COMMIT };
#define BENCH_REPS 41
#define BENCH_PARTS 8u
static u32 bench_splits[BENCH_PARTS + 1];

typedef void (*BenchFn)(AzWorld *w);
static void b_par(AzWorld *w, AzJobFn fn) { w->host->parallel_for(w->host->ctx, w, fn, bench_splits, BENCH_PARTS); }
static void b_grant(AzWorld *w)      { az_field_grant(w, 0, w->n_cells, 0); }
static void b_grant_ref(AzWorld *w)  { ref_grant(w, 0, w->n_cells); }
static void b_grant_par(AzWorld *w)  { b_par(w, job_grant); }
static void b_commit(AzWorld *w)     { az_field_commit(w, 0, w->n_cells, 0); }
static void b_commit_ref(AzWorld *w) { u64 a = 0, b = 0; ref_commit(w, 0, w->n_cells, &a, &b); }
static void b_commit_par(AzWorld *w) { b_par(w, job_commit); }

/* mode 0: nothing pending; 1: one cell in 64; 2: every cell */
static void bench_fill(AzWorld *w, u32 mode)
{
    if (!mode) return;
    for (u32 c = 0; c < w->n_cells; c += mode == 1 ? 64u : 1u) {
        az_cell_post(w, 0, c, AZ_SP_ORG_Y, -1);
        az_cell_post(w, 0, c, AZ_SP_WST_W, +1);
        az_cell_post_heat(w, 0, c, 5);
        az_cell_demand(w, 0, c, AZ_D_ORG_Y, 0x7FFFFFFFu);   /* more than is there: the division */
        az_cell_demand(w, 0, c, AZ_D_SOLV, 1);              /* fits                             */
    }
}

static int cmp_u64(const void *a, const void *b)
{
    u64 x = *(const u64 *)a, y = *(const u64 *)b;
    return x < y ? -1 : x > y;
}

/* median and minimum of BENCH_REPS timed calls, in milliseconds */
static void bench(AzWorld *w, u32 mode, int kind, BenchFn fn, double *med_ms, double *min_ms)
{
    u64 ns[BENCH_REPS];
    for (u32 r = 0; r < BENCH_REPS; r++) {
        bench_fill(w, mode);
        if (kind == K_COMMIT) az_field_grant(w, 0, w->n_cells, 0);    /* untimed: clear the demand */
        u64 t0 = w->host->now_ns(w->host->ctx);
        fn(w);
        u64 t1 = w->host->now_ns(w->host->ctx);
        ns[r] = t1 - t0;
        if (kind != K_COMMIT) az_field_commit(w, 0, w->n_cells, 0);   /* untimed: clear the changes */
    }
    qsort(ns, BENCH_REPS, sizeof ns[0], cmp_u64);
    *med_ms = (double)ns[BENCH_REPS / 2] / 1e6;
    *min_ms = (double)ns[0] / 1e6;
}

static void test_bench(void)
{
    AzConfig cfg = az_test_config(13, 64);
    cfg.gx_bits = 6; cfg.gy_bits = 6; cfg.gz_bits = 5;
    AzTestWorld t;
    if (!az_test_world(&t, &cfg, BENCH_PARTS)) { CHECK(!"world creation"); return; }
    AzWorld *w = t.w;
    CHECK_EQ(w->n_cells, 131072);
    CHECK_EQ(t.host->n_workers, BENCH_PARTS);
    for (u32 k = 0; k <= BENCH_PARTS; k++) bench_splits[k] = (u32)((u64)w->n_cells * k / BENCH_PARTS);

    /* a tall column of the heaviest medium: the clamp holds and stays monotone */
    u32 keep[64][2];
    for (u32 z = 0; z < w->gz; z++) {
        u32 *sp = &w->c_sp[(size_t)az_cell_xyz(w, 1, 1, z) * AZ_NSPECIES];
        keep[z][0] = sp[AZ_SP_GAS]; keep[z][1] = sp[AZ_SP_SOLV];
        sp[AZ_SP_GAS] = 0xFFFFFFFFu; sp[AZ_SP_SOLV] = 0xFFFFFFFFu;
    }
    CHECK_EQ(az_cell_pressure(w, az_cell_xyz(w, 1, 1, w->gz - 1)), (5ull * 0xFFFFFFFFull) >> 6);
    CHECK_EQ(az_cell_pressure(w, az_cell_xyz(w, 1, 1, 0)), 0xFFFFFFFFu);
    for (u32 z = 0; z + 1 < w->gz; z++)
        CHECK(az_cell_pressure(w, az_cell_xyz(w, 1, 1, z)) >= az_cell_pressure(w, az_cell_xyz(w, 1, 1, z + 1)));
    for (u32 z = 0; z < w->gz; z++) {
        u32 *sp = &w->c_sp[(size_t)az_cell_xyz(w, 1, 1, z) * AZ_NSPECIES];
        sp[AZ_SP_GAS] = keep[z][0]; sp[AZ_SP_SOLV] = keep[z][1];
    }
    /* room for the benchmark's withdrawals whatever worldgen put there */
    for (u32 c = 0; c < w->n_cells; c++) w->c_sp[(size_t)c * AZ_NSPECIES + AZ_SP_ORG_Y] = 100000u;

    static const char *const kinds[2] = { "grant", "commit" };
    static const char *const modes[3] = { "nothing pending", "1 cell in 64", "every cell" };
    static const BenchFn fns[2][3] = { { b_grant, b_grant_ref, b_grant_par },
                                       { b_commit, b_commit_ref, b_commit_par } };
    printf("  ms per call over %u cells, median (min) of %d; noisy, parallel load\n", w->n_cells, BENCH_REPS);
    printf("  %-7s %-16s %-17s %-17s %-17s\n", "", "", "module 1 thread", "cell-by-cell ref", "module 8 threads");
    for (int kind = 0; kind < 2; kind++)
        for (u32 mode = 0; mode < 3; mode++) {
            double med[3], mn[3];
            for (u32 v = 0; v < 3; v++) bench(w, mode, kind, fns[kind][v], &med[v], &mn[v]);
            printf("  %-7s %-16s %6.3f (%6.3f)   %6.3f (%6.3f)   %6.3f (%6.3f)\n", kinds[kind], modes[mode],
                   med[0], mn[0], med[1], mn[1], med[2], mn[2]);
        }
    az_tally_merge(w);
    CHECK_EQ(w->totals.field_underflow, 0);
    az_test_world_free(&t);
}

#ifdef AZ_TEST_GENERATED_DEMAND
/* Real founders on the smallest permitted grid. The tick's input positions
 * identify the cell that each creature asked, even if it subsequently moved. */
static void test_generated_demand(void)
{
    enum { FOUNDERS = 1024 };
    AzConfig cfg = az_test_config(87, FOUNDERS);
    cfg.cap = 2 * FOUNDERS; cfg.gx_bits = cfg.gy_bits = 2; cfg.gz_bits = 1;
    cfg.field_stride = 2;
    AzTestWorld t;
    if (!az_test_world_ex(&t, &cfg, 4, AZ_TEST_NO_WORLDGEN)) { CHECK(!"world"); return; }
    AzWorld *w = t.w;
    const u32 params[][2] = {
        {AZ_WP_METAB_UPTAKE_SCALE,4096}, {AZ_WP_METAB_PACE_BASE,65536},
        {AZ_WP_PART_HOLD,1048576}, {AZ_WP_WORLDGEN_FOUNDER_POOL,1048576},
        {AZ_WP_WORLDGEN_PATCHES,1}, {AZ_WP_WORLDGEN_OCEAN_WST,65536},
        {AZ_WP_WORLDGEN_VENT_NUT,65536}, {AZ_WP_WORLDGEN_FLOOR_ORG,65536},
        {AZ_WP_WORLDGEN_OCEAN_OXI,65536}, {AZ_WP_WORLDGEN_AIR_OXI,65536}
    };
    for (u32 j = 0; j < AZ_LEN(params); j++) CHECK(az_param_set(w, params[j][0], params[j][1]));
    az_worldgen(w);
    u32 healthy = 0;
    bool from_cell1[FOUNDERS + 1] = {false};
    for (u32 s = 0; s < w->n_slots; s++) {
        healthy += (w->flags[s] & AZ_F_ALIVE) && w->health[s] > 0;
        if (w->uid[s] <= FOUNDERS)
            from_cell1[w->uid[s]] = az_cell_at(w, w->pos_x[s], w->pos_y[s], w->pos_z[s]) == 1;
    }
    CHECK_EQ(healthy, FOUNDERS);
    const u32 available = az_field_avail(w, 1, AZ_D_WST_W);
    const u64 matter = az_total_matter(w);
    const i64 energy = az_total_energy(w);
    az_tick(w);
    u64 total = 0;
    for (u32 s = 0; s < w->n_slots; s++) {
        if (w->uid[s] <= FOUNDERS && from_cell1[w->uid[s]])
            total += w->eat[(size_t)s * AZ_NDIET + AZ_D_WST_W];
    }
    CHECK(total > UINT32_MAX);
    if (total > UINT32_MAX) {
        const u32 expected = (u32)((u64)available * 65535u / total);
        CHECK_EQ(az_cell_granted(w, 1, AZ_D_WST_W, 65535), expected);
    }
    CHECK_EQ(az_total_energy(w), energy);
    CHECK_EQ(az_total_matter(w), matter);
    CHECK_EQ(w->totals.field_underflow, 0);
    az_test_world_free(&t);
}
#endif


int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);   /* progress lines survive a kill */
    AzConfig cfg = az_test_config(11, 64);
    AzTestWorld t, r;
    if (!az_test_world(&t, &cfg, 0) || !az_test_world(&r, &cfg, 0)) { CHECK(!"world creation"); return az_test_done(); }
    printf("ABI version %u\n", AZ_ABI_VERSION);
    test_grant_property(t.w);
    printf("A grant property      %d checks so far, %d failures\n", az_checks, az_failures);
    test_underflow(t.w);
    printf("D underflow           %d checks so far, %d failures\n", az_checks, az_failures);
    test_pressure(t.w);
    printf("E pressure            %d checks so far, %d failures\n", az_checks, az_failures);
    test_marks(t.w);
    printf("F marks               %d checks so far, %d failures\n", az_checks, az_failures);
    test_reference(t.w, r.w);
    printf("B reference           %d checks so far, %d failures\n", az_checks, az_failures);
    az_test_world_free(&t);
    az_test_world_free(&r);
    test_threads();
#ifdef AZ_TEST_GENERATED_DEMAND
    test_generated_demand();
#endif
    printf("C 8 threads vs serial %d checks so far, %d failures\n", az_checks, az_failures);
    test_world();
    printf("G small world         %d checks so far, %d failures\n", az_checks, az_failures);
    test_bench();
    printf("H timings             %d checks so far, %d failures\n", az_checks, az_failures);
    return az_test_done();
}
