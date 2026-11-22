/* az_ledger.c - the cold ledger (serial, end of tick).
 *
 * Workers append events to private buffers during the parallel stages, so
 * the raw order depends on the thread count and on the schedule. The flush
 * removes that dependence: it gathers every worker's events into w->tmp,
 * sorts them into one canonical order, folds them into a running digest
 * (w->ledger_chain), appends one AZ_EV_TICK summary and hands the sorted
 * array to the host sink.
 *
 * Canonical order: (kind, a, b, aux, v) ascending, v as a signed number,
 * then tick as the last tie-break. Every field takes part, so the order of
 * a given multiset of events is unique and the digest cannot depend on which
 * worker emitted what.
 *
 * The chain is evidence, not simulation state: nothing in the world reads it.
 *
 * Three regimes, by how much of w->tmp (cap * 128 bytes) the tick needs:
 *   1. two copies fit  : stable LSD radix sort, skipping key bytes that are
 *                        the same in every event (the usual case);
 *   2. one copy fits   : in-place heapsort (same result, the order is total);
 *   3. not even one    : the events that do not fit are counted in
 *                        totals.dropped_events. That case is lossy and then
 *                        the chain does depend on the thread count; the
 *                        summary event carries the drop counter so a reader
 *                        can see it.
 */
#include "az_abi.h"

enum { LEDGER_FIELDS = 10 };

/* One step of the chain: az_ledger_fold in az_abi.h, shared with the sink and
 * the tools (finding v55). */

/* The sort key as ten 32-bit fields, least significant first (the 64-bit
 * members as two halves). v is biased so that unsigned order of the field
 * equals signed order of v. */
AZ_INLINE u32 key_field(const AzEvent *e, u32 f)
{
    switch (f) {
    case 0:  return (u32)e->tick;
    case 1:  return (u32)(e->tick >> 32);
    case 2:  return (u32)(u64)e->v;
    case 3:  return (u32)((u64)e->v >> 32) ^ 0x80000000u;
    case 4:  return e->aux;
    case 5:  return (u32)e->b;
    case 6:  return (u32)(e->b >> 32);
    case 7:  return (u32)e->a;
    case 8:  return (u32)(e->a >> 32);
    default: return e->kind;
    }
}

static bool ev_less(const AzEvent *x, const AzEvent *y)
{
    for (u32 f = LEDGER_FIELDS; f-- > 0; ) {
        u32 kx = key_field(x, f), ky = key_field(y, f);
        if (kx != ky) return kx < ky;
    }
    return false;
}

bool az_event_less(const AzEvent *x, const AzEvent *y) { return ev_less(x, y); }

/* Stable counting sort of n events on one key byte, src -> dst. */
static void radix_pass(const AzEvent *src, AzEvent *dst, u32 n, u32 f, u32 shift)
{
    u32 start[256];
    for (u32 i = 0; i < 256; i++) start[i] = 0;
    for (u32 i = 0; i < n; i++) start[(key_field(&src[i], f) >> shift) & 255u]++;
    u32 at = 0;
    for (u32 i = 0; i < 256; i++) { u32 c = start[i]; start[i] = at; at += c; }
    for (u32 i = 0; i < n; i++) dst[start[(key_field(&src[i], f) >> shift) & 255u]++] = src[i];
}

/* LSD radix sort between two buffers; returns the buffer holding the result.
 * vary[f] has a bit set wherever field f differs between two events, so a
 * byte that is constant over the whole tick costs nothing. */
static AzEvent *radix_sort(AzEvent *a, AzEvent *b, u32 n, const u32 vary[LEDGER_FIELDS])
{
    for (u32 f = 0; f < LEDGER_FIELDS; f++) {
        for (u32 shift = 0; shift < 32; shift += 8) {
            if (((vary[f] >> shift) & 255u) == 0) continue;
            radix_pass(a, b, n, f, shift);
            AzEvent *t = a; a = b; b = t;
        }
    }
    return a;
}

static void sift_down(AzEvent *e, u32 root, u32 n)
{
    AzEvent x = e[root];
    for (;;) {
        u32 child = 2 * root + 1;
        if (child >= n) break;
        if (child + 1 < n && ev_less(&e[child], &e[child + 1])) child++;
        if (!ev_less(&x, &e[child])) break;
        e[root] = e[child];
        root = child;
    }
    e[root] = x;
}

static void heap_sort(AzEvent *e, u32 n)
{
    if (n < 2) return;
    for (u32 i = n / 2; i-- > 0; ) sift_down(e, i, n);
    for (u32 end = n - 1; end > 0; end--) {
        AzEvent t = e[0]; e[0] = e[end]; e[end] = t;
        sift_down(e, 0, end);
    }
}

void az_ledger_flush(AzWorld *w)
{
    AzEvent *ev = (AzEvent *)w->tmp;
    u64 room64 = ((u64)w->cap * 128u) / sizeof(AzEvent);
    u32 room = room64 > 0xFFFFFFFFull ? 0xFFFFFFFFu : (u32)room64;
    u32 keep = room ? room - 1u : 0u;     /* one slot is reserved for the summary */

    /* gather the used blocks of the event pool, in block order (which worker
     * filled which block depends on the schedule; the sort removes that) */
    u32 n = 0;
    u64 lost = 0, pending_drops = 0;
    u32 vary[LEDGER_FIELDS] = { 0 };
    const u32 blocks = az_events_close(w);
    for (u32 bk = 0; bk < blocks; bk++) {
        const AzEvent *src = w->ev_pool + (size_t)bk * AZ_EV_BLOCK;
        u32 take = w->ev_fill[bk];
        if (take > keep - n) { lost += take - (keep - n); take = keep - n; }
        for (u32 i = 0; i < take; i++) {
            const AzEvent *s = &src[i];
            ev[n + i] = *s;
            for (u32 f = 0; f < LEDGER_FIELDS; f++) vary[f] |= key_field(s, f) ^ key_field(&ev[0], f);
        }
        n += take;
    }
    az_events_reset(w);
    for (u32 k = 0; k < AZ_MAX_WORKERS; k++)
        pending_drops += w->workers[k].dropped_events;   /* az_tally_merge moves these to the totals */
    w->totals.dropped_events += lost;

    AzEvent *sorted = ev;
    if (n > 1) {
        if ((u64)2 * ((u64)n + 1u) <= room64) sorted = radix_sort(ev, ev + n + 1u, n, vary);
        else heap_sort(ev, n);
    }

    u64 c = w->ledger_chain;
    for (u32 i = 0; i < n; i++) c = az_ledger_fold(c, &sorted[i]);

    /* The summary closes the tick: how many events, how many alive at the end
     * of the tick (counted now, after the tick's deaths and births; w->n_live
     * is the count of the tick's grid build, finding v11), a checkpoint of
     * the chain, and whether anything was ever lost. */
    if (room) {
        u32 alive = 0;
        for (u32 s = 0; s < w->n_slots; s++) alive += (w->flags[s] & AZ_F_ALIVE) ? 1u : 0u;
        u64 drops = w->totals.dropped_events + pending_drops;
        AzEvent *s = &sorted[n];
        s->tick = w->tick;
        s->kind = AZ_EV_TICK;
        s->pad  = 0;
        s->aux  = drops > 0xFFFFFFFFull ? 0xFFFFFFFFu : (u32)drops;
        s->a    = n;
        s->b    = alive;
        s->v    = (i64)c;
        c = az_ledger_fold(c, s);
        n++;
    }
    w->ledger_chain = c;
    w->ledger_count += n;

    if (n && w->host && w->host->sink) w->host->sink(w->host->sink_ctx, sorted, n);
}
