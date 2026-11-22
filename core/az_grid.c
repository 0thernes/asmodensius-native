/* az_grid.c - spatial binning, canonical cell lists, the public snapshot,
 * the free list and the periodic physical reorder (ABI version 3).
 *
 * Output contract (az_abi.h, "az_grid.c : spatial binning"): for every slot
 * below n_slots an alive slot gets cell[], spos[], its AZ_F_INIT bit and its
 * public record; a dead slot gets cell = AZ_NONE and joins the free list,
 * chained through spos[] from the highest dead slot down. cell_start[],
 * order[] (inside a cell by increasing uid), pub[], n_live, n_sorted and
 * free_head are rebuilt. The pair and birth scratch are not touched.
 * tests/test_grid.c compares every output byte with core/stubs/az_grid.c.
 *
 * az_grid_build is a deterministic parallel counting sort:
 *   1. slot ranges, one per part: cell[], the role bit, the free-list links
 *      inside the range, and a private histogram (part k, cell c);
 *   2. cell ranges: prefix sums in (cell, part) order, so part k owns a fixed
 *      run of positions inside every cell, below part k+1 (no atomics);
 *   3. slot ranges again: every part scatters its slots into its own runs;
 *   4. cell ranges balanced by entity count: sort each cell by uid, then
 *      write pub[] and spos[].
 * Before step 4 a cell lists its slots in increasing order, exactly like a
 * serial scatter, and the uid sort is stable, so the result depends neither
 * on the number of parts nor on which worker ran which part, even if two
 * entities had the same uid. A grid with few entities per cell, where
 * private histograms would cost more than they save, keeps step 1 parallel
 * (without counting) and does the counting and the scatter serially.
 *
 * A job only receives (begin, end, worker) and `worker` says who runs it, not
 * which part it is. What else a job needs travels in the module's scratch,
 * w->tmp_u32: the serial caller writes it before the parallel call, jobs read
 * it and write only the entries of their own part. No file-scope state.
 *
 * Scratch use (all of it dead once the call returns):
 *   w->tmp_u32  one histogram / cursor per cell (serial counting), or the
 *               GRID_CTX_* context of a parallel step 1 (part count, slot
 *               boundaries, lowest and highest dead slot of every part).
 *               reorder: [0], [1] = column batch to put back, to gather.
 *   w->tmp      build: histograms (parts * n_cells u32), then radix keys
 *               (two 16-byte records per sorted position). reorder: gathered
 *               columns.
 */
#include "az_abi.h"
#ifdef AZ_PARTNER_OBSERVER
#include "az_partner_hooks.h"
#endif
#ifdef AZ_INTAKE_OBSERVER
#include "az_intake_hooks.h"
#endif

#define GRID_PAR_MIN    4096u   /* below this many items a region is not cut    */
#define GRID_PART_MIN   2048u   /* fewest slots or sorted entries per part      */
#define GRID_CELL_MIN   1024u   /* fewest cells per part of the prefix sums     */
#define GRID_HIST_MIN   16384u  /* fewest slots for private histograms          */
#define GRID_PREFIX_SER 65536u  /* histogram entries summed without a region    */
#define GRID_SMALL      32u     /* cells up to this size: insertion sort        */
#define GRID_MAX_PARTS  256u    /* most parts this module cuts a region into    */
#define GRID_TMP_BYTES  128u    /* w->tmp holds this many bytes per slot        */
#define GRID_NO_BATCH   0xFFu

/* layout of w->tmp_u32 during a parallel step 1 with `parts` parts:
 * [0] = parts, [1 .. parts + 1] = slot boundaries, then two entries per part:
 * its lowest and highest dead slot */
#define GRID_CTX_SPLITS       1u
#define GRID_CTX_DEAD(parts)  ((parts) + 2u)
#define GRID_CTX_LEN(parts)   (3u * (parts) + 2u)

_Static_assert(GRID_MAX_PARTS <= AZ_MAX_PARTS, "the host accepts at most AZ_MAX_PARTS parts");
_Static_assert(AZ_MAX_COLS < GRID_NO_BATCH, "a batch index must fit a byte");

/* one element of any column must fit the reorder scratch */
#define AZ_COL(name, type, per, tier) \
    _Static_assert(sizeof(type) * (per) <= GRID_TMP_BYTES, "column " #name " is wider than the reorder scratch");
#include "az_columns.def"
#undef AZ_COL

/* ---------------------------------------------------------------------- */
/* work splitting                                                          */
/* ---------------------------------------------------------------------- */
static void grid_run(AzWorld *w, AzJobFn fn, const u32 *sp, u32 parts)
{
    if (parts > 1 && az_host_parallel(w)) {
        w->host->parallel_for(w->host->ctx, w, fn, sp, parts);
    } else {
        for (u32 k = 0; k < parts; k++) fn(w, sp[k], sp[k + 1], 0);
    }
}

/* How many parts a region of n items may be cut into, each holding at least
 * `quantum` items. Same budget as the tick: one part per worker; eight per
 * worker when the host hands parts out dynamically (so a slow core does not
 * hold the region up) unless the stage is `pinned` to one private buffer per
 * part; and with cfg.debug_split_seed a serial run is cut into seven parts
 * whatever its size, so tests see the multi-part code without threads. */
static u32 grid_parts(const AzWorld *w, u32 n, u32 quantum, bool pinned)
{
    const AzHost *h = w->host;
    const bool par = az_host_parallel(w);
    const bool dbg = w->cfg.debug_split_seed != 0;
    u32 p = par ? w->n_workers : 1u;
    if (par && !pinned && h->max_parts > p) p = az_min_u32(p * 8u, h->max_parts);
    if (!par && dbg) p = 7;
    if (p > GRID_MAX_PARTS) p = GRID_MAX_PARTS;
    if (dbg) quantum = 2;
    else if (n < GRID_PAR_MIN) p = 1;
    p = az_min_u32(p, n / quantum);
    return p < 1 ? 1 : p;
}

/* [0, n) into `parts` non-empty ranges (needs n >= 2 * parts). With
 * cfg.debug_split_seed the inner boundaries move by a pseudo-random amount
 * below half a part: results must not depend on where a region is cut. */
static void grid_split_even(const AzWorld *w, u32 *sp, u32 n, u32 parts)
{
    const u32 seed = w->cfg.debug_split_seed;
    const u32 half = (n / parts) / 2u;
    for (u32 k = 0; k <= parts; k++) sp[k] = (u32)((u64)n * k / parts);
    if (seed == 0 || half == 0) return;
    for (u32 k = 1; k < parts; k++) {
        u64 h = az_mix64(((u64)seed << 32) ^ ((u64)w->tick << 20) ^ ((u64)n << 8) ^ k);
        sp[k] += (u32)(h % half);
    }
}

/* cell ranges holding about the same number of sorted entries each (a range
 * may be empty) */
static void grid_split_load(const AzWorld *w, u32 *sp, u32 parts)
{
    const u32 *cs = w->cell_start;
    const u32 nc = w->n_cells, n = cs[nc];
    sp[0] = 0;
    for (u32 k = 1; k < parts; k++) {
        u32 target = (u32)((u64)n * k / parts);
        u32 lo = sp[k - 1], hi = nc;
        while (lo < hi) {                   /* first cell with cell_start >= target */
            u32 mid = lo + (hi - lo) / 2;
            if (cs[mid] < target) lo = mid + 1; else hi = mid;
        }
        sp[k] = lo;
    }
    sp[parts] = nc;
}

/* most step-1 parts whose context fits in w->tmp_u32 (max(cap, n_cells) + 1) */
static u32 grid_ctx_parts(const AzWorld *w)
{
    return (az_max_u32(w->cap, w->n_cells) + 1u - 2u) / 3u;
}

/* Private histograms, one per part, or 1 for "count serially". A histogram
 * costs n_cells entries to clear and to sum, so a grid with fewer than two
 * slots per cell, or a small world, counts serially. Also bounded by the
 * room in w->tmp. */
static u32 grid_hist_parts(const AzWorld *w)
{
    const u32 ns = w->n_slots, nc = w->n_cells;
    u32 p = grid_parts(w, ns, GRID_PART_MIN, true);
    const u64 room = ((u64)w->cap * (GRID_TMP_BYTES / 4u)) / nc;
    if (!w->cfg.debug_split_seed && (ns < GRID_HIST_MIN || ns / 2u < nc)) p = 1;
    if (room < p) p = (u32)room;
    p = az_min_u32(p, grid_ctx_parts(w));
    return p < 1 ? 1 : p;
}

/* ---------------------------------------------------------------------- */
/* step 1: cell, role bit, free-list links, histogram                       */
/* ---------------------------------------------------------------------- */
/* dead[0] = lowest dead slot of the range (its link is patched by the
 * caller), dead[1] = highest; both AZ_NONE if the range has no dead slot.
 * hist may be NULL: then nothing is counted. */
AZ_INLINE void grid_prepare(AzWorld *w, u32 b, u32 e, u32 *hist, u32 *dead)
{
    const u32 *px = w->pos_x, *py = w->pos_y, *pz = w->pos_z;
    const u64 *uid = w->uid;
    u8 *flags = w->flags;
    u32 *cell = w->cell, *spos = w->spos;
    u32 *blk = w->blk;
    u32 first = AZ_NONE, last = AZ_NONE;
    u32 freed = 0;                    /* store units of the blocks the dead give back */
    for (u32 s = b; s < e; s++) {
        u8 fl = flags[s];
        if (!(fl & AZ_F_ALIVE)) {
            if (blk[s]) { freed += az_blk(w, s)->units; blk[s] = 0; }
            cell[s] = AZ_NONE;
            spos[s] = last;
            if (last == AZ_NONE) first = s;
            last = s;
            continue;
        }
        u32 c = az_cell_at(w, px[s], py[s], pz[s]);
        cell[s] = c;
        if (hist) hist[c]++;
        fl = (u8)(fl & ~AZ_F_INIT);
        if (az_is_initiator(w, uid[s])) fl |= AZ_F_INIT;
        flags[s] = fl;
    }
    if (freed) __atomic_fetch_sub(&w->store_live, freed, __ATOMIC_RELAXED);
    dead[0] = first;
    dead[1] = last;
}

AZ_INLINE u32 *grid_hist(const AzWorld *w, u32 part)
{
    return (u32 *)(void *)w->tmp + (size_t)part * w->n_cells;
}

/* which part of the slot boundary table starts at `begin` (parts are never
 * empty, so the answer is unique) */
AZ_INLINE u32 grid_part_of(const AzWorld *w, u32 begin)
{
    const u32 *tab = w->tmp_u32 + GRID_CTX_SPLITS;
    u32 lo = 0, hi = w->tmp_u32[0];
    while (hi - lo > 1) {
        u32 mid = lo + (hi - lo) / 2;
        if (tab[mid] <= begin) lo = mid; else hi = mid;
    }
    return lo;
}

static void job_count(AzWorld *w, u32 b, u32 e, u32 worker)
{
    const u32 part = grid_part_of(w, b);
    u32 *hist = grid_hist(w, part);
    az_memset(hist, 0, (size_t)w->n_cells * sizeof(u32));
    grid_prepare(w, b, e, hist, w->tmp_u32 + GRID_CTX_DEAD(w->tmp_u32[0]) + 2u * part);
}

static void job_prepare(AzWorld *w, u32 b, u32 e, u32 worker)
{
    const u32 part = grid_part_of(w, b);
    grid_prepare(w, b, e, NULL, w->tmp_u32 + GRID_CTX_DEAD(w->tmp_u32[0]) + 2u * part);
}

/* run step 1 in `parts` >= 2 slot ranges and join their free lists */
static void grid_step1(AzWorld *w, AzJobFn fn, u32 *sp, u32 parts)
{
    u32 *ctx = w->tmp_u32;
    grid_split_even(w, sp, w->n_slots, parts);
    ctx[0] = parts;
    for (u32 k = 0; k <= parts; k++) ctx[GRID_CTX_SPLITS + k] = sp[k];
    grid_run(w, fn, sp, parts);
    /* the lowest dead slot of a part links to the highest dead slot below it */
    u32 head = AZ_NONE;
    for (u32 k = 0; k < parts; k++) {
        const u32 first = ctx[GRID_CTX_DEAD(parts) + 2u * k];
        const u32 last = ctx[GRID_CTX_DEAD(parts) + 2u * k + 1u];
        if (first == AZ_NONE) continue;
        w->spos[first] = head;
        head = last;
    }
    w->free_head = head;
}

/* ---------------------------------------------------------------------- */
/* step 2: prefix sums in (cell, part) order                                */
/* ---------------------------------------------------------------------- */
/* After this job hist(k)[c] is where part k's run begins inside cell c and
 * cell_start[c] is the start of cell c relative to the first cell of the
 * range - except cell_start[cb], whose relative start is always 0 and which
 * carries the number of entities in the range to the serial caller. */
static void job_prefix(AzWorld *w, u32 cb, u32 ce, u32 worker)
{
    const u32 parts = w->tmp_u32[0];
    const size_t nc = w->n_cells;
    u32 *h0 = grid_hist(w, 0);
    u32 *cs = w->cell_start;
    u32 run = 0;
    for (u32 c = cb; c < ce; c++) {
        u32 off = 0;
        for (u32 k = 0; k < parts; k++) {
            u32 *h = h0 + (size_t)k * nc + c;
            u32 t = *h;
            *h = off;
            off += t;
        }
        cs[c] = run;
        run += off;
    }
    cs[cb] = run;
}

/* ---------------------------------------------------------------------- */
/* step 3: ordered scatter                                                  */
/* ---------------------------------------------------------------------- */
static void job_scatter(AzWorld *w, u32 b, u32 e, u32 worker)
{
    u32 *hist = grid_hist(w, grid_part_of(w, b));
    const u32 *cell = w->cell, *cs = w->cell_start;
    u32 *order = w->order;
    for (u32 s = b; s < e; s++) {
        u32 c = cell[s];
        if (c == AZ_NONE) continue;
        order[cs[c] + hist[c]++] = s;
    }
}

/* ---------------------------------------------------------------------- */
/* step 4: uid order inside each cell, then pub[] and spos[]                */
/* ---------------------------------------------------------------------- */
AZ_INLINE void grid_sort_small(const u64 *uid, u32 *ord, u32 n)
{
    for (u32 i = 1; i < n; i++) {
        u32 s = ord[i];
        u64 key = uid[s];
        u32 j = i;
        while (j > 0 && uid[ord[j - 1]] > key) { ord[j] = ord[j - 1]; j--; }
        ord[j] = s;
    }
}

/* Stable LSD radix sort of order[a .. a+n) by the 64-bit uid, on (uid, slot)
 * records in w->tmp. Sorted input (the usual case one tick after a reorder)
 * returns after one pass; bytes that are equal in every uid are skipped. */
typedef struct GridKey { u64 uid; u64 slot; } GridKey;
_Static_assert(4 * sizeof(GridKey) <= GRID_TMP_BYTES, "two key arrays must fit w->tmp");
static void grid_sort_big(AzWorld *w, u32 a, u32 n)
{
    u32 *ord = w->order + a;
    const u64 *uid = w->uid;
    GridKey *src = (GridKey *)(void *)w->tmp + a;
    GridKey *dst = src + w->cap;
    u64 all_or = 0, all_and = ~0ull, prev = 0;
    u32 unsorted = 0;
    for (u32 i = 0; i < n; i++) {
        u32 s = ord[i];
        u64 u = uid[s];
        src[i].uid = u; src[i].slot = s;
        all_or |= u;
        all_and &= u;
        unsorted |= (u32)(u < prev);
        prev = u;
    }
    if (!unsorted) return;
    const u64 diff = all_or ^ all_and;
    for (u32 pass = 0; pass < 8; pass++) {
        const u32 shift = 8u * pass;
        u32 cnt[256];
        if (((diff >> shift) & 255u) == 0) continue;
        az_memset(cnt, 0, sizeof cnt);
        for (u32 i = 0; i < n; i++) cnt[(u32)(src[i].uid >> shift) & 255u]++;
        u32 run = 0;
        for (u32 d = 0; d < 256; d++) { u32 t = cnt[d]; cnt[d] = run; run += t; }
        for (u32 i = 0; i < n; i++) {
            GridKey key = src[i];
            dst[cnt[(u32)(key.uid >> shift) & 255u]++] = key;
        }
        GridKey *swap = src; src = dst; dst = swap;
    }
    for (u32 i = 0; i < n; i++) ord[i] = (u32)src[i].slot;
}

AZ_INLINE void grid_fill_pub(AzWorld *w, u32 pb, u32 pe)
{
    const u32 *order = w->order;
    for (u32 p = pb; p < pe; p++) {
        /* Each public stake lives at the tail of a variable-length private MIND.
         * Fetch the block header ahead of its descriptor, then its small payload
         * while publishing the intervening slots. Both hints stay within this job. */
        if (pe - p > 48u) {
            u32 future = order[p + 48u];
            __builtin_prefetch(az_blk(w, future), 0, 1);
        }
        if (pe - p > 16u) {
            const AzSenseMemory *future = az_sense_memory(w, order[p + 16u]);
            if (future) __builtin_prefetch(future, 0, 1);
        }
        u32 s = order[p];
        w->spos[s] = p;
        AzPub *o = &w->pub[p];
        o->x = (u16)az_sub(w->pos_x[s], w->shift_x);
        o->y = (u16)az_sub(w->pos_y[s], w->shift_y);
        o->z = (u16)az_sub(w->pos_z[s], w->shift_z);
        o->vx = (i8)(w->vel_x[s] >> 8);
        o->vy = (i8)(w->vel_y[s] >> 8);
        o->vz = (i8)(w->vel_z[s] >> 8);
        u8 fl = w->flags[s];
        u8 pf = 0;
        if (fl & AZ_F_RECEPT) pf |= AZ_PF_RECEPT;
        if (fl & AZ_F_COOP)   pf |= AZ_PF_COOP;
        if (fl & (AZ_F_EGG | AZ_F_DORMANT)) pf |= AZ_PF_PASSIVE;
        if (fl & AZ_F_JUV)    pf |= AZ_PF_JUV;
        o->rf = (u8)((az_radius_code(w->mass[s]) << 4) | pf);
        o->tag = w->tag[s];
        o->sig_a = w->sig[(size_t)s * 2];
        o->sig_b = w->sig[(size_t)s * 2 + 1];
        {   /* the forward axis of the body in the world frame, length 127 */
            const i16 rot[4] = { w->rot_w[s], w->rot_a[s], w->rot_b[s], w->rot_c[s] };
            const i32 fwd[3] = { 127, 0, 0 };
            i32 f[3];
            az_rotor_apply(f, rot, fwd);
            o->face[0] = (i8)az_clamp_i32(f[0], -127, 127);
            o->face[1] = (i8)az_clamp_i32(f[1], -127, 127);
            o->face[2] = (i8)az_clamp_i32(f[2], -127, 127);
        }
        o->role = w->role[s];
        const AzSenseMemory *history = az_sense_memory(w,s);
        o->stake = history ? history->stake : 0;
        o->pad = 0;
        o->colony = w->colony[s];
        o->mass = w->mass[s];
        o->clade = w->clade[s];
        AzPubId *id = &w->pub_id[p];
        id->uid = w->uid[s];
        id->parent_a = w->parent_a[s];
        id->parent_b = w->parent_b[s];
        id->partner = w->partner[s];
    }
}

static void job_finish(AzWorld *w, u32 cb, u32 ce, u32 worker)
{
    const u32 *cs = w->cell_start;
    for (u32 c = cb; c < ce; c++) {
        u32 a = cs[c], n = cs[c + 1] - a;
        if (n < 2) continue;
        if (n <= GRID_SMALL) grid_sort_small(w->uid, w->order + a, n);
        else grid_sort_big(w, a, n);
    }
    grid_fill_pub(w, cs[cb], cs[ce]);
}

/* ---------------------------------------------------------------------- */
/* az_grid_build                                                           */
/* ---------------------------------------------------------------------- */
void az_grid_build(AzWorld *w)
{
    const u32 nc = w->n_cells, ns = w->n_slots;
    u32 *cs = w->cell_start;
    u32 sp[GRID_MAX_PARTS + 1];
    u32 live = 0;

    const u32 hparts = grid_hist_parts(w);
    if (hparts >= 2) {
        /* steps 1 to 3 with private histograms. The cell ranges of step 2
         * borrow sp: one boundary table keeps this frame under the 2 KB
         * stack rule (finding v39). Step 3 takes its slot ranges from the
         * copy step 1 left in w->tmp_u32, the table grid_part_of searches,
         * so job_scatter sees exactly the boundaries job_count saw. */
        u32 *cp = sp;
        u32 cparts = grid_parts(w, nc, GRID_CELL_MIN, true);
        if (!w->cfg.debug_split_seed && (u64)hparts * nc <= GRID_PREFIX_SER) cparts = 1;
        grid_step1(w, job_count, sp, hparts);
        grid_split_even(w, cp, nc, cparts);
        grid_run(w, job_prefix, cp, cparts);
        /* cell_start[first cell of a range] holds the range total: make the
         * starts absolute */
        for (u32 k = 0; k < cparts; k++) {
            const u32 total = cs[cp[k]];
            cs[cp[k]] = live;
            if (live != 0) for (u32 c = cp[k] + 1; c < cp[k + 1]; c++) cs[c] += live;
            live += total;
        }
        cs[nc] = live;
        grid_run(w, job_scatter, w->tmp_u32 + GRID_CTX_SPLITS, hparts);
    } else {
        /* one histogram, counted and scattered serially; step 1 still runs in
         * parallel (without counting) when the world is big enough */
        u32 *cur = w->tmp_u32;
        u32 dead[2];
        const u32 *cell = w->cell;
        u32 *order = w->order;
        const u32 pparts = az_min_u32(grid_parts(w, ns, GRID_PART_MIN, false), grid_ctx_parts(w));
        if (pparts >= 2) {
            grid_step1(w, job_prepare, sp, pparts);
            az_memset(cur, 0, (size_t)nc * sizeof(u32));
            for (u32 s = 0; s < ns; s++) {
                u32 c = cell[s];
                if (c != AZ_NONE) cur[c]++;
            }
        } else {
            az_memset(cur, 0, (size_t)nc * sizeof(u32));
            grid_prepare(w, 0, ns, cur, dead);
            w->free_head = dead[1];
        }
        for (u32 c = 0; c < nc; c++) { u32 t = cur[c]; cs[c] = live; cur[c] = live; live += t; }
        cs[nc] = live;
        for (u32 s = 0; s < ns; s++) {
            u32 c = cell[s];
            if (c == AZ_NONE) continue;
            order[cur[c]++] = s;
        }
    }
    w->n_live = live;
    w->n_sorted = live;

    const u32 fparts = grid_parts(w, live, GRID_PART_MIN, false);
    if (fparts < 2) {
        job_finish(w, 0, nc, 0);
    } else {
        grid_split_load(w, sp, fparts);
        grid_run(w, job_finish, sp, fparts);
    }
}

/* ---------------------------------------------------------------------- */
/* az_grid_reorder                                                         */
/* ---------------------------------------------------------------------- */
/* Pack the HOT and WARM columns into batches of at most GRID_TMP_BYTES per
 * entity (first fit, in column order). batch_of[k] = batch of column k, or
 * GRID_NO_BATCH for an EPH column. Returns the number of batches. */
static u32 grid_pack(const AzWorld *w, u8 *batch_of /* AZ_MAX_COLS */)
{
    u8 fill[AZ_MAX_COLS];
    const u32 ncol = az_n_columns();
    u32 nb = 0;
    for (u32 k = 0; k < ncol; k++) {
        AzColDesc col;
        az_column(w, k, &col);
        batch_of[k] = GRID_NO_BATCH;
        if (col.tier == AZ_TIER_EPH) continue;
        const u32 bytes = col.elem * col.per;
        u32 b = 0;
        while (b < nb && fill[b] + bytes > GRID_TMP_BYTES) b++;
        if (b == nb) fill[nb++] = 0;
        fill[b] = (u8)(fill[b] + bytes);
        batch_of[k] = (u8)b;
    }
    return nb;
}

AZ_INLINE void grid_gather(u8 *AZ_RESTRICT dst, const u8 *AZ_RESTRICT src, const u32 *ord,
                           u32 n, size_t bytes)
{
    for (u32 i = 0; i < n; i++)
        __builtin_memcpy(dst + (size_t)i * bytes, src + (size_t)ord[i] * bytes, bytes);
}

/* One step of the reorder for the sorted positions [pb, pe), which are also
 * the destination slots: first put back the batch tmp_u32[0] gathered by the
 * previous step, then gather the batch tmp_u32[1] (either may be
 * GRID_NO_BATCH). The part owns w->tmp[pb * 128 .. pe * 128) and lays the
 * columns of a batch out one after the other inside it, so parts never share
 * scratch, and the columns it overwrites belong to a batch every part has
 * finished gathering. That makes one parallel region per batch enough. */
static void job_move(AzWorld *w, u32 pb, u32 pe, u32 worker)
{
    u8 batch_of[AZ_MAX_COLS];
    (void)grid_pack(w, batch_of);
    const u32 put = w->tmp_u32[0], get = w->tmp_u32[1];
    const u32 ncol = az_n_columns();
    const size_t cnt = (size_t)(pe - pb);
    const u32 *ord = w->order + pb;
    u8 *region = w->tmp + (size_t)pb * GRID_TMP_BYTES;
    size_t off = 0;
    for (u32 k = 0; put != GRID_NO_BATCH && k < ncol; k++) {
        AzColDesc col;
        if (batch_of[k] != put) continue;
        az_column(w, k, &col);
        const size_t bytes = (size_t)col.elem * col.per;
        az_memcpy((u8 *)col.data + (size_t)pb * bytes, region + off * cnt, cnt * bytes);
        off += bytes;
    }
    off = 0;
    for (u32 k = 0; get != GRID_NO_BATCH && k < ncol; k++) {
        AzColDesc col;
        if (batch_of[k] != get) continue;
        az_column(w, k, &col);
        const size_t bytes = (size_t)col.elem * col.per;
        const u8 *src = (const u8 *)col.data;
        u8 *dst = region + off * cnt;
        switch (bytes) {
        case 1:   grid_gather(dst, src, ord, (u32)cnt, 1);   break;
        case 2:   grid_gather(dst, src, ord, (u32)cnt, 2);   break;
        case 4:   grid_gather(dst, src, ord, (u32)cnt, 4);   break;
        case 8:   grid_gather(dst, src, ord, (u32)cnt, 8);   break;
        case 16:  grid_gather(dst, src, ord, (u32)cnt, 16);  break;
        case 32:  grid_gather(dst, src, ord, (u32)cnt, 32);  break;
        case 64:  grid_gather(dst, src, ord, (u32)cnt, 64);  break;
        case 96:  grid_gather(dst, src, ord, (u32)cnt, 96);  break;
        case 128: grid_gather(dst, src, ord, (u32)cnt, 128); break;
        default:
            for (size_t i = 0; i < cnt; i++)
                az_memcpy(dst + i * bytes, src + (size_t)ord[i] * bytes, bytes);
            break;
        }
        off += bytes;
    }
}

/* Move every alive entity to slot = its sorted position; drop dead slots. */
void az_grid_reorder(AzWorld *w)
{
    az_grid_build(w);
#ifdef AZ_INTAKE_OBSERVER
    azt_intake_reorder(w);
#endif
#ifdef AZ_PARTNER_OBSERVER
    azt_partner_reorder(w);
#endif
    const u32 n = w->n_live;
    if (n > 0) {
        u8 batch_of[AZ_MAX_COLS];
        u32 sp[GRID_MAX_PARTS + 1];
        const u32 nb = grid_pack(w, batch_of);
        const u32 parts = grid_parts(w, n, GRID_PART_MIN, false);
        grid_split_even(w, sp, n, parts);
        for (u32 b = 0; b <= nb; b++) {
            w->tmp_u32[0] = b > 0 ? b - 1u : GRID_NO_BATCH;
            w->tmp_u32[1] = b < nb ? b : GRID_NO_BATCH;
            grid_run(w, job_move, sp, parts);
        }
    }
    w->n_slots = n;
    w->free_head = AZ_NONE;
}
