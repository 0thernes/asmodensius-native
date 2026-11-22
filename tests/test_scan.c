/* test_scan.c - the fast neighbour scan (core/az_scan.c) against the
 * reference (core/stubs/az_scan.c), bit for bit.
 *
 * The reference is compiled into this file under renamed symbols
 * (ref_scan_range, ref_scan_list). The module is also compiled a second time
 * with its portable range test (port_scan_range, port_scan_list), so both
 * code paths of the module are compared on the same inputs.
 *
 *   1. signal attenuation: the multiply-by-reciprocal equals the division
 *   2. made-up worlds: sparse, dense (more than 64 neighbours, neighbourhoods
 *      of thousands of records), z floor and ceiling, wrap in x and y, kin
 *      lesion on and off, colonies, bond partners, parents and children
 *      nearby, receptive / passive / juvenile flags, distance ties
 *   3. live worlds: the scan of every tick of a running world, the two
 *      conservation invariants, thread counts and reorder periods
 *   4. the kin lesion changes a measured outcome
 *   5. timing against the reference (noisy, parallel load)
 */
#include "az_test.h"
#include <time.h>

/* ---- the reference, under renamed symbols -------------------------------- */
void ref_scan_range(const AzWorld *w, u32 p0, u32 count, AzPercept *out, u32 worker);
u32  ref_scan_list(const AzWorld *w, u32 slot, i32 (*off)[3], u32 *oslot, u32 max, u32 worker);
#define az_scan_range ref_scan_range
#define az_scan_list  ref_scan_list
#include "../core/stubs/az_scan.c"
#undef az_scan_range
#undef az_scan_list

/* ---- the module once more, with the portable range test ------------------ */
void port_scan_range(const AzWorld *w, u32 p0, u32 count, AzPercept *out, u32 worker);
u32  port_scan_list(const AzWorld *w, u32 slot, i32 (*off)[3], u32 *oslot, u32 max, u32 worker);
#define az_scan_range port_scan_range
#define az_scan_list  port_scan_list
#define AZ_SCAN_PORTABLE 1
#include "../core/az_scan.c"
#undef az_scan_range
#undef az_scan_list

/* ---- test randomness (not simulation randomness) -------------------------- */
static u64 rng_state = 1;
static u64 rnd64(void) { return az_test_rand64(&rng_state); }
static u32 rnd(u32 n) { return (u32)((rnd64() >> 16) % n); }

static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

/* ---- made-up worlds ------------------------------------------------------- */
/* Only what the scan reads is filled in: the grid geometry, cell_start[],
 * order[], pub[], cell[], spos[], phen[] and cfg.lesion. */
enum {
    FAB_CLUSTER = 1,   /* most entities in a few cells (corners, z floor and ceiling) */
    FAB_LATTICE = 2,   /* coarse coordinates: exact distance ties and contacts        */
    FAB_FARSIGHT = 4,  /* every sense range at its maximum                            */
    FAB_SENSE128 = 8,  /* every sense range at half a cell                            */
    FAB_WIDE_UID = 16  /* uids above 2^32 whose low halves repeat every 50 positions  */
};

typedef struct Fab {
    AzWorld *w;
    u32 n, cap;
    void *ws_raw;      /* the allocation behind w->ws */
} Fab;

static void fab_free(Fab *f)
{
    AzWorld *w = f->w;
    if (!w) return;
    free(w->cell_start); free(w->order); free(w->pub); free(w->pub_id); free(w->cell); free(w->spos); free(w->phen);
    free(f->ws_raw);
    az_test_shell_free(w);
    f->w = NULL;
    f->ws_raw = NULL;
}

static bool fab_make(Fab *f, u64 seed, u32 gxb, u32 gyb, u32 gzb, u32 n, u32 mode)
{
    rng_state = seed * 0x100000001B3ull + 77;
    f->w = NULL; f->ws_raw = NULL;
    AzWorld *w = az_test_shell();       /* aligned as AzWorld needs; calloc is not */
    if (!w) return false;
    /* worker 0's scratch block, cache-line aligned: the scan keeps the pieces
     * of a neighbourhood there (az_ws_scan) */
    f->ws_raw = calloc(1, AZ_WS_BYTES + 64);
    if (!f->ws_raw) { az_test_shell_free(w); return false; }
    w->ws = (u8 *)(((uintptr_t)f->ws_raw + 63u) & ~(uintptr_t)63u);
    const u32 cap = n + 64;
    f->w = w; f->n = n; f->cap = cap;
    w->gx_bits = (u8)gxb; w->gy_bits = (u8)gyb; w->gz_bits = (u8)gzb;
    w->gx = 1u << gxb; w->gy = 1u << gyb; w->gz = 1u << gzb;
    w->n_cells = w->gx * w->gy * w->gz;
    w->cap = cap; w->n_slots = cap; w->n_live = n; w->n_sorted = n;
    const u32 nc = w->n_cells;
    w->cell_start = (u32 *)calloc(nc + 2, sizeof(u32));
    w->order = (u32 *)malloc((size_t)(n + 1) * sizeof(u32));
    w->pub = (AzPub *)calloc(n + 1, sizeof(AzPub));
    w->pub_id = (AzPubId *)calloc(n + 1, sizeof(AzPubId));
    w->cell = (u32 *)malloc((size_t)cap * sizeof(u32));
    w->spos = (u32 *)malloc((size_t)cap * sizeof(u32));
    w->phen = (u8 *)calloc(cap, AZ_PHEN);
    u32 *ecell = (u32 *)malloc((size_t)(n + 1) * sizeof(u32));
    u32 *perm = (u32 *)malloc((size_t)cap * sizeof(u32));
    if (!w->cell_start || !w->order || !w->pub || !w->pub_id || !w->cell || !w->spos || !w->phen || !ecell || !perm) {
        free(ecell); free(perm);
        return false;
    }

    /* the hot cells of a clustered world: grid corners, the floor and the
     * ceiling, the wrap columns, and a few random ones */
    u32 hot[12];
    hot[0] = 0;
    hot[1] = nc - 1;
    hot[2] = az_cell_xyz(w, w->gx - 1, 0, 0);
    hot[3] = az_cell_xyz(w, 0, w->gy - 1, w->gz - 1);
    hot[4] = az_cell_xyz(w, 1 % w->gx, 0, 0);
    hot[5] = az_cell_xyz(w, w->gx - 1, w->gy - 1, 0);
    for (u32 i = 6; i < 12; i++) hot[i] = rnd(nc);

    for (u32 i = 0; i < n; i++) {
        u32 c = rnd(nc);
        if ((mode & FAB_CLUSTER) && rnd(10) < 8) c = hot[rnd(12)];
        ecell[i] = c;
        w->cell_start[c + 1]++;
    }
    for (u32 c = 0; c < nc; c++) w->cell_start[c + 1] += w->cell_start[c];

    for (u32 s = 0; s < cap; s++) { perm[s] = s; w->cell[s] = AZ_NONE; w->spos[s] = AZ_NONE; }
    for (u32 s = cap - 1; s > 0; s--) { u32 j = rnd(s + 1); u32 t = perm[s]; perm[s] = perm[j]; perm[j] = t; }

    static const u16 families[6] = { 0x1010, 0x1418, 0x8080, 0x8890, 0xF0F0, 0x0000 };
    static const u8 windows[6] = { 0, 6, 20, 60, 255, 128 };
    for (u32 c = 0, p = 0; c < nc; c++) {
        for (; p < w->cell_start[c + 1]; p++) {
            AzPub *o = &w->pub[p];
            const u32 slot = perm[p];
            w->order[p] = slot;
            w->cell[slot] = c;
            w->spos[slot] = p;
            if (mode & FAB_LATTICE) {
                o->x = (u16)az_min_u32(rnd(9) * 2048u + rnd(2), AZ_SUB_ONE - 1);
                o->y = (u16)az_min_u32(rnd(9) * 2048u + rnd(2), AZ_SUB_ONE - 1);
                o->z = (u16)az_min_u32(rnd(9) * 2048u + rnd(2), AZ_SUB_ONE - 1);
            } else {
                o->x = (u16)rnd(AZ_SUB_ONE); o->y = (u16)rnd(AZ_SUB_ONE); o->z = (u16)rnd(AZ_SUB_ONE);
                if (rnd(16) == 0) o->x = (u16)(rnd(2) ? 0 : AZ_SUB_ONE - 1);
                if (rnd(16) == 0) o->y = (u16)(rnd(2) ? 0 : AZ_SUB_ONE - 1);
                if (rnd(16) == 0) o->z = (u16)(rnd(2) ? 0 : AZ_SUB_ONE - 1);
            }
            o->vx = (i8)(rnd(256) - 128); o->vy = (i8)(rnd(256) - 128); o->vz = (i8)(rnd(256) - 128);
            o->rf = (u8)((rnd(16) << 4) | rnd(16));
            if (rnd(3) == 0) o->rf = (u8)((o->rf & 0xF0) | AZ_PF_RECEPT);   /* more eligible mates */
            u32 fam = families[rnd(6)];
            o->tag = (u16)(rnd(4) == 0 ? rnd(65536)
                         : (((fam >> 8) + rnd(8)) & 255) << 8 | (((fam & 255) + rnd(8)) & 255));
            o->colony = rnd(3) ? 0 : 1 + rnd(3);
            o->sig_a = (u8)rnd(256); o->sig_b = (u8)rnd(256);
            /* increasing with p, so increasing inside a cell; one world in two
             * has uids above 2^32 whose low halves repeat */
            w->pub_id[p].uid = (mode & FAB_WIDE_UID)
                ? (((u64)(1u + p / 50u) << 32) | (10u + 3u * (p % 50u) + rnd(3)))
                : (u64)(10u + 3u * p + rnd(3));
            u8 *ph = &w->phen[(size_t)slot * AZ_PHEN];
            u32 sense = rnd(256);
            if (rnd(8) == 0) sense = rnd(2) ? 255 : 0;
            if (mode & FAB_FARSIGHT) sense = 255;
            if (mode & FAB_SENSE128) sense = 128;
            ph[AZ_P_SENSE] = (u8)sense;
            ph[AZ_P_KIN] = windows[rnd(6)];
        }
    }
    /* relatives and partners are neighbours in the sorted order, so most of
     * them are in the same or an adjacent cell */
    for (u32 p = 0; p < n; p++) {
        AzPubId *o = &w->pub_id[p];
        const u32 span = 24;
        u32 a = p + rnd(2 * span + 1), b = p + rnd(2 * span + 1), d = p + rnd(2 * span + 1);
        a = a < span ? 0 : az_min_u32(a - span, n - 1);
        b = b < span ? 0 : az_min_u32(b - span, n - 1);
        d = d < span ? 0 : az_min_u32(d - span, n - 1);
        o->parent_a = rnd(3) == 0 ? w->pub_id[a].uid : (rnd(4) ? 0 : rnd(4 * n));
        o->parent_b = rnd(4) == 0 ? w->pub_id[b].uid : 0;
        o->partner = rnd(3) == 0 ? w->pub_id[d].uid : 0;
    }
    free(ecell); free(perm);
    return true;
}

/* ---- comparison ----------------------------------------------------------- */

typedef struct Cover {
    u64 used, touch, kin, col, prey, threat, mate, bond, parent, child;
    u32 capped;        /* entities that hit AZ_NEIGH_MAX      */
    u32 max_records;   /* largest 27-cell neighbourhood       */
    u32 floor_ents, ceil_ents, wrap_ents;
} Cover;

static void cover_add(const AzWorld *w, const AzPercept *pc, const u8 *valid, u32 n, Cover *cv)
{
    for (u32 p = 0; p < n; p++) {
        if (valid && !valid[p]) continue;
        const AzPercept *q = &pc[p];
        cv->used += q->n_used; cv->touch += q->n_touch; cv->kin += q->n_kin; cv->col += q->n_col;
        cv->prey += q->n_prey; cv->threat += q->n_threat;
        cv->mate += q->best_mate != AZ_NONE; cv->bond += q->bond != AZ_NONE;
        cv->parent += q->parent != AZ_NONE; cv->child += q->child != AZ_NONE;
        cv->capped += q->n_used == AZ_NEIGH_MAX;
        const u32 c = w->cell[w->order[p]];
        if (q->n_used) {
            cv->floor_ents += az_cell_z(w, c) == 0;
            cv->ceil_ents += az_cell_z(w, c) == w->gz - 1;
            cv->wrap_ents += az_cell_x(w, c) == 0 || az_cell_x(w, c) == w->gx - 1
                          || az_cell_y(w, c) == 0 || az_cell_y(w, c) == w->gy - 1;
        }
    }
    for (u32 c = 0; c < w->n_cells; c++) {
        if (w->cell_start[c] == w->cell_start[c + 1]) continue;
        u32 rec = 0;
        for (i32 dz = -1; dz <= 1; dz++) for (i32 dy = -1; dy <= 1; dy++) for (i32 dx = -1; dx <= 1; dx++) {
            u32 nb = az_cell_nb(w, c, dx, dy, dz);
            if (nb != AZ_NONE) rec += w->cell_start[nb + 1] - w->cell_start[nb];
        }
        if (rec > cv->max_records) cv->max_records = rec;
    }
}

static void report_diff(const char *what, const AzPercept *a, const AzPercept *b, u32 n)
{
    for (u32 p = 0; p < n; p++) {
        if (memcmp(&a[p], &b[p], sizeof(AzPercept)) == 0) continue;
        printf("  %s: first difference at sorted position %u: n_used %u/%u touch %u/%u kin %u/%u col %u/%u "
               "prey %u/%u threat %u/%u sig_a %u/%u d2_kin %u/%u mate %u/%u\n", what, p,
               a[p].n_used, b[p].n_used, a[p].n_touch, b[p].n_touch, a[p].n_kin, b[p].n_kin,
               a[p].n_col, b[p].n_col, a[p].n_prey, b[p].n_prey, a[p].n_threat, b[p].n_threat,
               a[p].sig_a, b[p].sig_a, a[p].d2_kin, b[p].d2_kin, a[p].best_mate, b[p].best_mate);
        return;
    }
}

#define LIST_GUARD 72
static const u32 list_max[6] = { 0, 1, 7, 63, 64, 200 };

/* az_scan_list of one slot against the reference, for several caps */
static void compare_list(const AzWorld *w, u32 slot, u32 expect_n_used)
{
    i32 ra[LIST_GUARD][3], fa[LIST_GUARD][3], pa[LIST_GUARD][3];
    u32 rs[LIST_GUARD], fs[LIST_GUARD], ps[LIST_GUARD];
    for (u32 m = 0; m < 6; m++) {
        memset(ra, 0x11, sizeof ra); memset(fa, 0x22, sizeof fa); memset(pa, 0x33, sizeof pa);
        memset(rs, 0x11, sizeof rs); memset(fs, 0x22, sizeof fs); memset(ps, 0x33, sizeof ps);
        const u32 nr = ref_scan_list(w, slot, ra, rs, list_max[m], 0);
        const u32 nf = az_scan_list(w, slot, fa, fs, list_max[m], 0);
        const u32 np = port_scan_list(w, slot, pa, ps, list_max[m], 0);
        bool ok = nr == nf && nr == np && nr <= AZ_NEIGH_MAX && nr <= list_max[m]
               && memcmp(ra, fa, (size_t)nr * sizeof ra[0]) == 0 && memcmp(rs, fs, (size_t)nr * sizeof rs[0]) == 0
               && memcmp(ra, pa, (size_t)nr * sizeof ra[0]) == 0 && memcmp(rs, ps, (size_t)nr * sizeof rs[0]) == 0;
        /* nothing is written past the returned count */
        for (u32 i = nr; ok && i < LIST_GUARD; i++)
            ok = fa[i][0] == 0x22222222 && fa[i][2] == 0x22222222 && fs[i] == 0x22222222u
              && pa[i][0] == 0x33333333 && ps[i] == 0x33333333u;
        if (list_max[m] >= AZ_NEIGH_MAX && expect_n_used != AZ_NONE) ok = ok && nr == expect_n_used;
        CHECK(ok);
        if (!ok && az_failures <= 20) printf("  list: slot %u max %u: counts %u %u %u\n", slot, list_max[m], nr, nf, np);
    }
}

typedef void (*ScanFn)(const AzWorld *w, u32 p0, u32 count, AzPercept *out, u32 worker);

/* Run fn over the sorted positions that are valid (all of them if valid is
 * NULL), cut into ranges: mode 0 = as long as possible, 1 = the tick's
 * batches of 32, 2 = random lengths that start and end anywhere. */
static void scan_valid(const AzWorld *w, const u8 *valid, ScanFn fn, AzPercept *out, u32 mode)
{
    const u32 n = w->n_sorted;
    for (u32 p = 0; p < n; ) {
        if (valid && !valid[p]) { p++; continue; }
        u32 lim = n;
        if (mode == 1) lim = az_min_u32(n, (p & ~31u) + 32);
        if (mode == 2) lim = az_min_u32(n, p + 1 + rnd(rnd(4) ? 5 : 90));
        u32 e = p + 1;
        while (e < lim && (!valid || valid[e])) e++;
        fn(w, p, e - p, &out[p], 0);
        p = e;
    }
}

/* positions that were not scanned get the reference's (untouched) bytes */
static void copy_invalid(AzPercept *dst, const AzPercept *src, const u8 *valid, u32 n)
{
    if (!valid) return;
    for (u32 p = 0; p < n; p++) if (!valid[p]) dst[p] = src[p];
}

/* Every valid entity of the world: reference, module and portable module,
 * in the longest ranges, in batches of 32 (the tick's batch), and in random
 * ranges. Returns the reference percepts (caller frees). */
static AzPercept *compare_world(const AzWorld *w, const u8 *valid, const char *what, u32 list_step, Cover *cv)
{
    const u32 n = w->n_sorted;
    const size_t bytes = (size_t)(n + 1) * sizeof(AzPercept);
    const size_t used = (size_t)n * sizeof(AzPercept);
    AzPercept *r = (AzPercept *)malloc(bytes), *f = (AzPercept *)malloc(bytes), *pt = (AzPercept *)malloc(bytes);
    if (!r || !f || !pt) { CHECK(!"memory"); free(r); free(f); free(pt); return NULL; }
    memset(r, 0xA5, bytes); memset(f, 0x5A, bytes); memset(pt, 0x3C, bytes);

    scan_valid(w, valid, ref_scan_range, r, 0);
    scan_valid(w, valid, az_scan_range, f, 0);
    scan_valid(w, valid, port_scan_range, pt, 0);
    copy_invalid(f, r, valid, n); copy_invalid(pt, r, valid, n);
    CHECK(memcmp(r, f, used) == 0);
    if (memcmp(r, f, used) != 0) report_diff(what, r, f, n);
    CHECK(memcmp(r, pt, used) == 0);
    if (memcmp(r, pt, used) != 0) report_diff(what, r, pt, n);
    /* the entry after the last one is not written */
    CHECK(((const u8 *)&f[n])[0] == 0x5A && ((const u8 *)&f[n])[sizeof(AzPercept) - 1] == 0x5A);

    memset(f, 0x5A, bytes);
    scan_valid(w, valid, az_scan_range, f, 1);
    copy_invalid(f, r, valid, n);
    CHECK(memcmp(r, f, used) == 0);

    memset(f, 0x5A, bytes); memset(pt, 0x3C, bytes);
    scan_valid(w, valid, az_scan_range, f, 2);
    scan_valid(w, valid, port_scan_range, pt, 2);
    copy_invalid(f, r, valid, n); copy_invalid(pt, r, valid, n);
    CHECK(memcmp(r, f, used) == 0);
    CHECK(memcmp(r, pt, used) == 0);
    az_scan_range(w, 0, 0, f, 0);   /* an empty range is legal and writes nothing */
    CHECK(memcmp(r, f, used) == 0);

    if (list_step) {
        for (u32 p = rnd(list_step); p < n; p += list_step)
            if (!valid || valid[p]) compare_list(w, w->order[p], r[p].n_used);
    }
    if (cv) cover_add(w, r, valid, n, cv);
    free(f); free(pt);
    return r;
}

/* ---- 1. attenuation ------------------------------------------------------- */
static void test_attenuation(void)
{
    u32 bad = 0, tried = 0;
    for (u32 sense = 0; sense < 256; sense++) {
        const u32 range = (sense + 1u) * (AZ_SUB_ONE / 256);
        const u32 r2 = range * range;
        const u64 inv = fs_inv(r2);
        for (u32 i = 0; i < 3000; i++) {
            u32 diff;
            if (i < 520) {   /* around every step of the quotient */
                u64 edge = ((u64)r2 * (i / 2) + 255) / 256;
                diff = (u32)(edge - (i & 1 ? 1 : 0));
                if (edge == 0 && (i & 1)) diff = 0;
            } else if (i < 530) diff = r2 - (i - 520);
            else diff = (u32)(rnd64() % ((u64)r2 + 1));
            if (diff > r2) diff = r2;
            const u32 want = (u32)((((u64)diff) << 8) / r2);
            bad += fs_att(diff, r2, inv) != want;
            tried++;
        }
    }
    CHECK_EQ(bad, 0);
    CHECK(tried == 256u * 3000u);
    /* exhaustive for the smallest and one mid range */
    for (u32 sense = 0; sense < 256; sense += 37) {
        const u32 range = (sense + 1u) * (AZ_SUB_ONE / 256);
        const u32 r2 = range * range;
        const u64 inv = fs_inv(r2);
        u32 wrong = 0;
        const u32 step = r2 > 4000000u ? 97u : 1u;
        for (u32 diff = 0; diff <= r2; diff += step) wrong += fs_att(diff, r2, inv) != (u32)((((u64)diff) << 8) / r2);
        wrong += fs_att(r2, r2, inv) != 256u;
        CHECK_EQ(wrong, 0);
    }
}

/* ---- 2. made-up worlds ---------------------------------------------------- */
static void test_fab(void)
{
    static const struct { u32 gxb, gyb, gzb, n, mode; } cases[] = {
        { 2, 2, 0,    40, 0 },                               /* one layer: floor is ceiling        */
        { 2, 2, 1,   300, 0 },
        { 2, 2, 2,  3000, 0 },                               /* about 47 per cell, everything wraps */
        { 3, 2, 1,  2500, FAB_LATTICE },                     /* ties and contacts                  */
        { 4, 4, 3,  2048, 0 },                               /* about 1 per cell                   */
        { 4, 4, 3,  8192, FAB_SENSE128 },
        { 4, 4, 3,  6000, FAB_CLUSTER },                     /* hundreds per hot cell              */
        { 4, 4, 3,  6000, FAB_CLUSTER | FAB_WIDE_UID },      /* 64-bit identities                  */
        { 3, 2, 1,  2500, FAB_LATTICE | FAB_WIDE_UID },
        { 3, 3, 2,  9000, FAB_CLUSTER | FAB_LATTICE },
        { 4, 3, 2, 12000, FAB_CLUSTER | FAB_FARSIGHT },      /* thousands of records per scan      */
        { 2, 2, 0,  4000, FAB_FARSIGHT },
        { 5, 5, 3, 30000, 0 },
        { 2, 3, 3,     1, 0 },                               /* alone                              */
        { 2, 2, 1,     2, FAB_FARSIGHT }
    };
    Cover cv;
    memset(&cv, 0, sizeof cv);
    Cover dense;
    memset(&dense, 0, sizeof dense);
    for (u32 i = 0; i < AZ_LEN(cases); i++) {
        for (u32 lesion = 0; lesion < 2; lesion++) {
            Fab f;
            if (!fab_make(&f, 1000 + i * 7 + lesion, cases[i].gxb, cases[i].gyb, cases[i].gzb, cases[i].n, cases[i].mode)) {
                CHECK(!"fab_make"); fab_free(&f); continue;
            }
            f.w->cfg.lesion = lesion ? AZ_LESION_KIN : 0;
            char what[64];
            snprintf(what, sizeof what, "fab %u lesion %u", i, lesion);
            const u32 step = cases[i].n > 4000 ? 13 : (cases[i].n > 500 ? 5 : 1);
            Cover one;
            memset(&one, 0, sizeof one);
            AzPercept *r = compare_world(f.w, NULL, what, step, &one);
            if (lesion) CHECK_EQ(one.kin, 0);
            if (cases[i].mode & FAB_CLUSTER) { dense.capped += one.capped; if (one.max_records > dense.max_records) dense.max_records = one.max_records; }
            cv.used += one.used; cv.touch += one.touch; cv.kin += one.kin; cv.col += one.col; cv.prey += one.prey;
            cv.threat += one.threat; cv.mate += one.mate; cv.bond += one.bond; cv.parent += one.parent;
            cv.child += one.child; cv.capped += one.capped;
            cv.floor_ents += one.floor_ents; cv.ceil_ents += one.ceil_ents; cv.wrap_ents += one.wrap_ents;
            if (one.max_records > cv.max_records) cv.max_records = one.max_records;
            /* a dead slot and a slot born this tick have no list */
            if (r && f.n > 1) {
                i32 off[AZ_NEIGH_MAX][3]; u32 os[AZ_NEIGH_MAX];
                u32 dead = AZ_NONE;
                for (u32 s = 0; s < f.cap; s++) if (f.w->cell[s] == AZ_NONE) { dead = s; break; }
                CHECK(dead != AZ_NONE);
                if (dead != AZ_NONE) {
                    CHECK_EQ(az_scan_list(f.w, dead, off, os, AZ_NEIGH_MAX, 0), 0);
                    f.w->cell[dead] = f.w->cell[f.w->order[0]];   /* born: a cell, no sorted position */
                    CHECK_EQ(az_scan_list(f.w, dead, off, os, AZ_NEIGH_MAX, 0), 0);
                    CHECK_EQ(ref_scan_list(f.w, dead, off, os, AZ_NEIGH_MAX, 0), 0);
                    f.w->cell[dead] = AZ_NONE;
                }
            }
            free(r);
            fab_free(&f);
        }
    }
    printf("fab coverage: used %llu touch %llu kin %llu col %llu prey %llu threat %llu mate %llu bond %llu "
           "parent %llu child %llu capped %u max_records %u floor %u ceil %u wrap %u\n",
           (unsigned long long)cv.used, (unsigned long long)cv.touch, (unsigned long long)cv.kin,
           (unsigned long long)cv.col, (unsigned long long)cv.prey, (unsigned long long)cv.threat,
           (unsigned long long)cv.mate, (unsigned long long)cv.bond, (unsigned long long)cv.parent,
           (unsigned long long)cv.child, cv.capped, cv.max_records, cv.floor_ents, cv.ceil_ents, cv.wrap_ents);
    /* the comparisons above mean something only if the cases were reached */
    CHECK(cv.touch > 1000); CHECK(cv.kin > 1000); CHECK(cv.col > 1000); CHECK(cv.prey > 1000); CHECK(cv.threat > 1000);
    CHECK(cv.mate > 1000); CHECK(cv.bond > 100); CHECK(cv.parent > 100); CHECK(cv.child > 100);
    CHECK(dense.capped > 1000);                  /* more than 64 neighbours in range     */
    CHECK(dense.max_records > 4 * FS_BUF);       /* the gather really ran in pieces      */
    CHECK(cv.floor_ents > 100); CHECK(cv.ceil_ents > 100); CHECK(cv.wrap_ents > 100);
}

/* ---- 4. the kin lesion changes a measured outcome ------------------------- */
static void test_lesion(void)
{
    Fab f;
    if (!fab_make(&f, 51, 3, 3, 2, 4000, 0)) { CHECK(!"fab_make"); fab_free(&f); return; }
    const u32 n = f.n;
    AzPercept *a = (AzPercept *)malloc((size_t)n * sizeof(AzPercept));
    AzPercept *b = (AzPercept *)malloc((size_t)n * sizeof(AzPercept));
    if (a && b) {
        f.w->cfg.lesion = 0;
        az_scan_range(f.w, 0, n, a, 0);
        f.w->cfg.lesion = AZ_LESION_KIN;
        az_scan_range(f.w, 0, n, b, 0);
        u64 kin_a = 0, kin_b = 0, col_a = 0, col_b = 0, other_a = 0, other_b = 0, moved = 0;
        for (u32 p = 0; p < n; p++) {
            kin_a += a[p].n_kin; kin_b += b[p].n_kin;
            col_a += a[p].n_col; col_b += b[p].n_col;
            other_a += (u64)a[p].n_prey + a[p].n_threat; other_b += (u64)b[p].n_prey + b[p].n_threat;
            moved += memcmp(&a[p], &b[p], sizeof(AzPercept)) != 0;
            /* the lesion changes nothing else: same neighbours, same contacts, same signals */
            CHECK(a[p].n_used == b[p].n_used && a[p].n_touch == b[p].n_touch && a[p].n_col == b[p].n_col
                  && a[p].sig_a == b[p].sig_a && a[p].sig_b == b[p].sig_b && a[p].best_mate == b[p].best_mate
                  && a[p].bond == b[p].bond && a[p].parent == b[p].parent && a[p].child == b[p].child
                  && memcmp(a[p].push, b[p].push, sizeof a[p].push) == 0);
        }
        printf("kin lesion: n_kin %llu -> %llu, prey+threat %llu -> %llu, percepts changed %llu of %u\n",
               (unsigned long long)kin_a, (unsigned long long)kin_b, (unsigned long long)other_a,
               (unsigned long long)other_b, (unsigned long long)moved, n);
        CHECK(kin_a > 1000);
        CHECK_EQ(kin_b, 0);
        CHECK_EQ(col_a, col_b);
        CHECK_EQ(other_b, other_a + kin_a);   /* former kin are seen as prey or threat */
        CHECK(moved > n / 4);
    } else CHECK(!"memory");
    free(a); free(b);
    fab_free(&f);
}

/* ---- 3. live worlds ------------------------------------------------------- */
static u64 run_live(u64 seed, u32 n, u32 ticks, u32 threads, u8 reorder, u32 lesion, int compare,
                    u64 *kin_seen, u64 *scanned)
{
    AzConfig cfg = az_test_config(seed, n);
    cfg.reorder_period = reorder;
    cfg.lesion = lesion;
    AzTestWorld t;
    if (!az_test_world(&t, &cfg, threads)) { CHECK(!"world creation"); return 0; }
    const u64 m0 = az_total_matter(t.w);
    const i64 e0 = az_total_energy(t.w);
    for (u32 i = 0; i < ticks; i++) {
        az_tick(t.w);
        CHECK_EQ(az_total_matter(t.w), m0);
        CHECK_EQ(az_total_energy(t.w), e0);
        if (compare && (i % 4 == 0 || i + 1 == ticks)) {
            /* After a tick pub[], order[] and cell_start[] still hold that
             * tick's values. Entities that died in it have no cell any more,
             * so only the positions whose entity still has its cell and its
             * sorted position are scanned (the dead stay as neighbours in
             * pub[], for the reference and the module alike). Nothing is
             * written to the world. */
            const AzWorld *w = t.w;
            const u32 ns = w->n_sorted;
            u8 *valid = (u8 *)malloc(ns + 1);
            u32 n_valid = 0;
            if (!valid) { CHECK(!"memory"); break; }
            for (u32 p = 0; p < ns; p++) {
                const u32 slot = w->order[p];
                const u32 c = slot < w->cap ? w->cell[slot] : AZ_NONE;
                valid[p] = c != AZ_NONE && c < w->n_cells && w->spos[slot] == p
                        && w->cell_start[c] <= p && p < w->cell_start[c + 1];
                n_valid += valid[p];
            }
            CHECK(n_valid * 2 >= ns && n_valid > 0);
            Cover cv;
            memset(&cv, 0, sizeof cv);
            AzPercept *r = compare_world(w, valid, "live", 17, &cv);
            if (kin_seen) *kin_seen += cv.kin;
            if (scanned) *scanned += n_valid;
            free(r);
            free(valid);
        }
    }
    CHECK(t.w->n_live > 0);
    CHECK_EQ(t.w->totals.field_underflow, 0);
    const u64 h = az_state_hash(t.w);
    az_test_world_free(&t);
    return h;
}

static void test_live(void)
{
    const u32 N = 3000, T = 16;
    u64 kin = 0, kin_lesioned = 0, scanned = 0, scanned_dense = 0;
    /* tuning seeds 51..53 (the blocks 5001..5024 and 5101..5124 are reserved
     * for the milestone-8 reporting runs) */
    const u64 h0 = run_live(51, N, T, 0, 1, 0, 1, &kin, &scanned);
    const u64 h1 = run_live(51, N, T, 1, 1, 0, 0, NULL, NULL);
    const u64 h3 = run_live(51, N, T, 3, 1, 0, 1, NULL, NULL);
    const u64 h8 = run_live(51, N, T, 8, 1, 0, 0, NULL, NULL);
    const u64 h9 = run_live(51, N, T, 0, 9, 0, 0, NULL, NULL);
    const u64 hk = run_live(51, N, T, 0, 1, AZ_LESION_KIN, 1, &kin_lesioned, NULL);
    const u64 hs = run_live(52, N, T, 0, 1, 0, 1, NULL, NULL);
    /* crowded live world: 20000 entities in 2048 cells */
    (void)run_live(53, 20000, 6, 0, 1, 0, 1, NULL, &scanned_dense);
    printf("live: hash serial %016" PRIx64 " t1 %016" PRIx64 " t3 %016" PRIx64 " t8 %016" PRIx64
           " reorder9 %016" PRIx64 " kin-lesion %016" PRIx64 " seed52 %016" PRIx64 "\n", h0, h1, h3, h8, h9, hk, hs);
    printf("live: %llu + %llu entity scans compared; kin neighbours seen %llu, with the kin lesion %llu; "
           "state hash %s under the kin lesion\n",
           (unsigned long long)scanned, (unsigned long long)scanned_dense,
           (unsigned long long)kin, (unsigned long long)kin_lesioned, h0 != hk ? "differs" : "is the same");
    CHECK(h0 == h1); CHECK(h0 == h3); CHECK(h0 == h8); CHECK(h0 == h9);
    CHECK(h0 != hs);
    CHECK(scanned > 5000); CHECK(scanned_dense > 20000);
    CHECK(kin > 0);
    CHECK_EQ(kin_lesioned, 0);
}

/* ---- 5. timing ------------------------------------------------------------ */
static void bench(u32 occupancy, u32 mode, const char *label)
{
    Fab f;
    const u32 gxb = 5, gyb = 5, gzb = 4;
    const u32 n = occupancy << (gxb + gyb + gzb);
    if (!fab_make(&f, 42 + occupancy, gxb, gyb, gzb, n, mode)) { CHECK(!"fab_make"); fab_free(&f); return; }
    AzPercept ra[32], fa[32];
    double best_ref = 1e30, best_fast = 1e30;
    u64 sum_ref = 0, sum_fast = 0, used = 0;
    for (u32 rep = 0; rep < 3; rep++) {
        double t0 = now_s();
        sum_ref = 0;
        for (u32 p = 0; p < n; p += 32) {
            u32 cnt = az_min_u32(32, n - p);
            ref_scan_range(f.w, p, cnt, ra, 0);
            for (u32 k = 0; k < cnt; k++) sum_ref += ra[k].n_used + ra[k].sig_a + (u32)ra[k].kin[0];
        }
        double t1 = now_s();
        sum_fast = 0; used = 0;
        for (u32 p = 0; p < n; p += 32) {
            u32 cnt = az_min_u32(32, n - p);
            az_scan_range(f.w, p, cnt, fa, 0);
            for (u32 k = 0; k < cnt; k++) { sum_fast += fa[k].n_used + fa[k].sig_a + (u32)fa[k].kin[0]; used += fa[k].n_used; }
        }
        double t2 = now_s();
        if (t1 - t0 < best_ref) best_ref = t1 - t0;
        if (t2 - t1 < best_fast) best_fast = t2 - t1;
    }
    CHECK(sum_ref == sum_fast);
    printf("bench %-22s occupancy %2u: %7u entities, %5.1f neighbours used each; reference %7.1f ns/entity, "
           "az_scan_range %7.1f ns/entity, ratio %.2f  (noisy, parallel load)\n",
           label, occupancy, n, (double)used / n, 1e9 * best_ref / n, 1e9 * best_fast / n, best_ref / best_fast);
    fab_free(&f);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);   /* keep the progress lines if a run is cut short */
    test_attenuation();
    test_fab();
    test_lesion();
#ifndef AZ_TEST_SCAN_NO_LIVE
    test_live();
#endif
    bench(1, 0, "sense uniform 0..255");
    bench(4, 0, "sense uniform 0..255");
    bench(16, 0, "sense uniform 0..255");
    bench(1, FAB_SENSE128, "sense 128 (half cell)");
    bench(4, FAB_SENSE128, "sense 128 (half cell)");
    bench(16, FAB_SENSE128, "sense 128 (half cell)");
    bench(1, FAB_FARSIGHT, "sense 255 (one cell)");
    bench(4, FAB_FARSIGHT, "sense 255 (one cell)");
    bench(16, FAB_FARSIGHT, "sense 255 (one cell)");
    return az_test_done();
}
