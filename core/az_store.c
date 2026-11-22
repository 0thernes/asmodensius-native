/* az_store.c - the creature store: variable-size blocks behind a handle.
 * Owner: project lead. Freestanding, integers only.
 *
 * Everything about a creature that has no fixed size (its genes, its neural
 * program, what its brain keeps, its body parts) lives in one block of
 * AZ_UNIT units inside the locked arena; the fixed record keeps a handle
 * (column `blk`). The store has two halves. Blocks are handed out by a bump
 * pointer in the active half; a compaction copies every block that a slot
 * still holds into the other half, in slot order, and makes that half the
 * active one. So after a compaction the blocks lie in creature order, the
 * order in which the tick walks them, and nothing is left between them.
 *
 * Determinism (lab mode): blocks are allocated, released and moved only by
 * serial code or by a parallel region whose result does not depend on its
 * cuts; no OS allocator is involved. Whether a block is granted depends only
 * on store_live, the units all blocks hold, never on where they lie; so the
 * compaction schedule, the thread count and the reorder period cannot change
 * what happens. Handles and addresses are neither hashed nor saved; the
 * blocks' bytes are (az_state_hash, az_snapshot_save).
 *
 * A block is always in canonical form: units == az_blk_units(len), segments
 * back to back in index order on 8-byte boundaries, all other bytes zero.
 */
#include "az_abi.h"

#define STORE_PARTS 64u            /* most parts of a compaction                */
#define STORE_SUMS  512u           /* w->splits[STORE_SUMS + k]: units of part k */
_Static_assert(STORE_SUMS + STORE_PARTS + 1u <= AZ_MAX_PARTS + 1u, "the sums share w->splits with the cuts");

static u32 seg_pad(u32 n) { return (n + 7u) & ~7u; }

AZ_INLINE AzBlk *blk_at(const AzWorld *w, u32 base, u32 h)
{
    return (AzBlk *)(void *)(w->store + ((size_t)(base + h) << 6));
}

u32 az_blk_units(const u32 len[AZ_SEG_COUNT])
{
    u64 bytes = AZ_UNIT;
    for (u32 k = 0; k < AZ_SEG_COUNT; k++) bytes += ((u64)len[k] + 7u) & ~(u64)7u;
    const u64 units = (bytes + AZ_UNIT - 1u) / AZ_UNIT;
    return units > 0x7FFFFFFFull ? 0x7FFFFFFFu : (u32)units;
}

bool az_blk_ok(const AzBlk *b, u32 units)
{
    if (units == 0 || b->units != units || b->pad0 || b->pad1[0] || b->pad1[1]) return false;
    if (az_blk_units(b->len) != units) return false;
    const u8 *p = (const u8 *)(const void *)b;
    u64 at = AZ_UNIT;
    for (u32 k = 0; k < AZ_SEG_COUNT; k++) {
        if (b->off[k] != at) return false;
        const u64 end = at + b->len[k], next = (end + 7u) & ~(u64)7u;
        if (next > (u64)units * AZ_UNIT) return false;
        for (u64 i = end; i < next; i++)
            if (p[i]) return false;
        at = next;
    }
    for (u64 i = at; i < (u64)units * AZ_UNIT; i++)
        if (p[i]) return false;
    if ((b->len[AZ_SEG_GEN_A] | b->len[AZ_SEG_GEN_B]) & 3u) return false;
    return true;
}

/* ---------------------------------------------------------------------- */
/* compaction                                                              */
/* ---------------------------------------------------------------------- */
static u32 part_of(const AzWorld *w, u32 b, u32 e)
{
    u32 k = 0;
    while (k < STORE_PARTS && !(w->splits[k] == b && w->splits[k + 1] == e)) k++;
    return k;
}

static void job_sum(AzWorld *w, u32 b, u32 e, u32 worker)
{
    (void)worker;
    if (b == e) return;
    const u32 *blk = w->blk;
    u32 units = 0;
    for (u32 s = b; s < e; s++)
        if (blk[s]) units += blk_at(w, w->store_base, blk[s])->units;
    w->splits[STORE_SUMS + part_of(w, b, e)] = units;
}

static void job_copy(AzWorld *w, u32 b, u32 e, u32 worker)
{
    (void)worker;
    if (b == e) return;
    u32 *blk = w->blk;
    const u32 from = w->store_base, to = w->store_units - w->store_base;
    u32 at = w->splits[STORE_SUMS + part_of(w, b, e)];
    for (u32 s = b; s < e; s++) {
        if (!blk[s]) continue;
        const AzBlk *src = blk_at(w, from, blk[s]);
        const u32 units = src->units;
        az_memcpy(blk_at(w, to, at), src, (size_t)units << 6);
        blk[s] = at;
        at += units;
    }
}

void az_store_compact(AzWorld *w)
{
    const u32 n = w->n_slots;
    u32 parts = 1;
    if (az_host_parallel(w) && n >= 8192u) {
        parts = w->n_workers;
        if (w->host->max_parts && parts > w->host->max_parts) parts = w->host->max_parts;
        if (parts > STORE_PARTS) parts = STORE_PARTS;
    }
    u32 *cut = w->splits, *sum = w->splits + STORE_SUMS;
    for (u32 k = 0; k <= parts; k++) cut[k] = (u32)(((u64)n * k) / parts);
    /* an empty part never runs; a part that did not run sums to nothing */
    for (u32 k = 0; k <= STORE_PARTS; k++) sum[k] = 0;
    if (parts > 1) w->host->parallel_for(w->host->ctx, w, job_sum, cut, parts);
    else job_sum(w, 0, n, 0);
    u32 at = 1;                                  /* unit 0 of a half stays zero */
    for (u32 k = 0; k < parts; k++) { const u32 t = sum[k]; sum[k] = at; at += t; }
    if (parts > 1) w->host->parallel_for(w->host->ctx, w, job_copy, cut, parts);
    else job_copy(w, 0, n, 0);
    w->store_base = w->store_units - w->store_base;
    w->store_top = at;
    w->store_live = at - 1u;
}

/* ---------------------------------------------------------------------- */
/* blocks                                                                  */
/* ---------------------------------------------------------------------- */
static void refuse(AzWorld *w, u32 slot, u32 units)
{
    if (w->totals.store_denied++ == 0) az_emit(w, 0, AZ_EV_REFUSED, AZ_REF_STORE, w->uid[slot], units, 0);
}

/* `units` units at the top of the active half, or 0. May compact. */
static u32 store_take(AzWorld *w, u32 units)
{
    if ((u64)w->store_live + units + 1u > w->store_units) return 0;
    if ((u64)w->store_top + units > w->store_units) az_store_compact(w);
    const u32 h = w->store_top;
    w->store_top += units;
    w->store_live += units;
    return h;
}

/* a block shrinks from `old` to `now` units; the last block gives its tail back */
static void store_shrink(AzWorld *w, u32 h, u32 old, u32 now)
{
    w->store_live -= old - now;
    if (h + old == w->store_top) w->store_top -= old - now;
}

static void blk_layout(AzBlk *b, u32 units, const u32 len[AZ_SEG_COUNT])
{
    u32 at = AZ_UNIT;
    b->units = units;
    for (u32 k = 0; k < AZ_SEG_COUNT; k++) {
        b->off[k] = at;
        b->len[k] = len[k];
        at += seg_pad(len[k]);
    }
}

bool az_blk_new(AzWorld *w, u32 slot, const u32 len[AZ_SEG_COUNT])
{
    const u32 units = az_blk_units(len);
    const u32 h = store_take(w, units);
    if (!h) { refuse(w, slot, units); return false; }
    AzBlk *b = blk_at(w, w->store_base, h);
    az_memset(b, 0, (size_t)units << 6);
    blk_layout(b, units, len);
    w->blk[slot] = h;
    return true;
}

void az_blk_release(AzWorld *w, u32 slot)
{
    const u32 h = w->blk[slot];
    if (!h) return;
    const u32 units = blk_at(w, w->store_base, h)->units;
    w->blk[slot] = 0;
    store_shrink(w, h, units, 0);
}

void az_blk_fit(AzWorld *w, u32 slot, const u32 len[AZ_SEG_COUNT])
{
    const u32 h = w->blk[slot];
    if (!h) return;
    AzBlk *b = blk_at(w, w->store_base, h);
    u8 *base = (u8 *)(void *)b;
    const u32 old_units = b->units;
    u32 at = AZ_UNIT;
    for (u32 k = 0; k < AZ_SEG_COUNT; k++) {
        const u32 n = len[k] < b->len[k] ? len[k] : b->len[k];
        /* a move toward lower addresses: the forward copy is safe on overlap */
        if (b->off[k] != at && n) az_memcpy(base + at, base + b->off[k], n);
        /* the gap up to the next 8-byte boundary lies below the next segment's
         * old place, so clearing it destroys nothing that still has to move */
        az_memset(base + at + n, 0, seg_pad(n) - n);
        b->off[k] = at;
        b->len[k] = n;
        at += seg_pad(n);
    }
    const u32 units = (at + AZ_UNIT - 1u) / AZ_UNIT;
    az_memset(base + at, 0, ((size_t)old_units << 6) - at);
    b->units = units;
    store_shrink(w, h, old_units, units);
}

/* copy n bytes toward higher addresses (dst > src, the ranges may overlap) */
static void move_up(u8 *dst, const u8 *src, size_t n)
{
    while (n >= 8) {
        u64 t;
        n -= 8;
        __builtin_memcpy(&t, src + n, 8);
        __builtin_memcpy(dst + n, &t, 8);
    }
    while (n) { n--; dst[n] = src[n]; }
}

bool az_blk_resize(AzWorld *w, u32 slot, u32 seg, u32 bytes)
{
    u32 len[AZ_SEG_COUNT];
    if (!w->blk[slot]) {
        for (u32 k = 0; k < AZ_SEG_COUNT; k++) len[k] = 0;
        len[seg] = bytes;
        return az_blk_new(w, slot, len);
    }
    AzBlk *b = az_blk(w, slot);
    for (u32 k = 0; k < AZ_SEG_COUNT; k++) len[k] = b->len[k];
    if (bytes <= len[seg]) {
        len[seg] = bytes;
        az_blk_fit(w, slot, len);
        return true;
    }
    const u32 old_len = len[seg], old_units = b->units;
    len[seg] = bytes;
    const u32 units = az_blk_units(len);
    if (units == old_units) {
        /* room inside the block: shift the later segments up, the last first */
        u8 *base = (u8 *)(void *)b;
        const u32 shift = seg_pad(bytes) - seg_pad(old_len);
        for (u32 k = AZ_SEG_COUNT - 1u; k > seg; k--) {
            if (shift && b->len[k]) move_up(base + b->off[k] + shift, base + b->off[k], b->len[k]);
            b->off[k] += shift;
        }
        az_memset(base + b->off[seg] + old_len, 0, (size_t)seg_pad(bytes) - old_len);
        b->len[seg] = bytes;
        /* the pads of the moved segments: clear what the shift left behind */
        for (u32 k = seg + 1u; k < AZ_SEG_COUNT; k++)
            az_memset(base + b->off[k] + b->len[k], 0, seg_pad(b->len[k]) - b->len[k]);
        return true;
    }
    const u32 nh = store_take(w, units);
    if (!nh) { refuse(w, slot, units); return false; }
    b = az_blk(w, slot);                         /* store_take may have moved it */
    AzBlk *nb = blk_at(w, w->store_base, nh);
    az_memset(nb, 0, (size_t)units << 6);
    blk_layout(nb, units, len);
    for (u32 k = 0; k < AZ_SEG_COUNT; k++)
        az_memcpy((u8 *)(void *)nb + nb->off[k], (const u8 *)(const void *)b + b->off[k], b->len[k]);
    const u32 oh = w->blk[slot];
    w->blk[slot] = nh;
    store_shrink(w, oh, old_units, 0);
    return true;
}

/* ---- body parts -------------------------------------------------------- */

bool az_part_add(AzWorld *w, u32 slot, u32 kind)
{
    u32 bytes;
    if (!w->blk[slot]) return false;
    (void)az_seg(w, slot, AZ_SEG_BODY, &bytes);
    if (bytes > 0x7FFFFFF0u - (u32)sizeof(AzPart)) return false;
    if (!az_blk_resize(w, slot, AZ_SEG_BODY, bytes + (u32)sizeof(AzPart))) return false;
    AzPart *p = (AzPart *)az_seg(w, slot, AZ_SEG_BODY, &bytes);
    AzPart *np = &p[bytes / (u32)sizeof(AzPart) - 1u];
    np->matter = 0;
    np->kind = (u8)(kind % AZ_PART_KINDS);
    np->health = 255;
    np->aux = 0;
    return true;
}

bool az_part_drop(AzWorld *w, u32 slot, u32 index)
{
    u32 n;
    AzPart *p = az_parts(w, slot, &n);
    if (index >= n || p[index].matter) return false;
    for (u32 i = index; i + 1u < n; i++) p[i] = p[i + 1u];
    return az_blk_resize(w, slot, AZ_SEG_BODY, (n - 1u) * (u32)sizeof(AzPart));
}

typedef struct PartKey { u64 uid; u64 slot; } PartKey;

/* stable LSD radix sort by uid; eight byte passes leave the result in a */
static void sort_parts(PartKey *a, PartKey *b, u32 n)
{
    u32 cnt[256];
    for (u32 pass = 0; pass < 8; pass++) {
        const u32 shift = pass * 8;
        for (u32 i = 0; i < 256; i++) cnt[i] = 0;
        for (u32 i = 0; i < n; i++) cnt[(u32)(a[i].uid >> shift) & 255u]++;
        u32 sum = 0;
        for (u32 i = 0; i < 256; i++) { u32 c = cnt[i]; cnt[i] = sum; sum += c; }
        for (u32 i = 0; i < n; i++) b[cnt[(u32)(a[i].uid >> shift) & 255u]++] = a[i];
        PartKey *t = a; a = b; b = t;
    }
}

void az_store_commit(AzWorld *w)
{
    /* tmp holds cap * 128 bytes: room for two arrays of one key per slot */
    PartKey *all = (PartKey *)(void *)w->tmp;
    u32 n = 0;
    for (u32 s = 0; s < w->n_slots; s++) {
        if (!(w->pair[s].req & (AZ_RQ_GROW | AZ_RQ_SHED)) && !w->blk_req[(size_t)s * 3 + 2]) continue;
        if (!(w->flags[s] & AZ_F_ALIVE)) continue;
        all[n].uid = w->uid[s]; all[n].slot = s; n++;
    }
    if (!n) return;
    sort_parts(all, all + n, n);
    for (u32 i = 0; i < n; i++) {
        const u32 s = (u32)all[i].slot;
        const u8 req = w->pair[s].req;
        u32 *rq = &w->blk_req[(size_t)s * 3];
        if (req & AZ_RQ_SHED) (void)az_part_drop(w, s, rq[1]);
        if (req & AZ_RQ_GROW) (void)az_part_add(w, s, rq[0]);
        if (rq[2]) {
            /* more words than a segment can count is more than any store holds: the resize refuses it */
            const u32 words = rq[2] == 0xFFFFFFFFu ? rq[2] : rq[2] - 1u;
            (void)az_blk_resize(w, s, AZ_SEG_MEMORY, words > 0x1FFFFFFFu ? 0xFFFFFFF8u : words * 8u);
            rq[2] = 0;
        }
        w->pair[s].req = (u8)(req & ~(AZ_RQ_GROW | AZ_RQ_SHED));
    }
}
