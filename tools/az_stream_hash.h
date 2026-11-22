/* Hosted read-only observer. Each callback reads a disjoint range of the
 * immutable tick, then contributes its integer sum once. The caller joins
 * all readers before capturing a frame or admitting the next tick. */
#ifndef AZ_STREAM_HASH_H
#define AZ_STREAM_HASH_H
#include "../core/az_hash_parts.h"
#include <windows.h>

typedef struct {
    PTP_WORK work;
    const AzWorld *world;
    u32 workers, parts;
    LONG next_part;
    u64 sum;
} StreamHasher;

static void stream_hash_ranges(StreamHasher *h)
{
    const AzWorld *w = h->world;
    u64 sum = 0;
    for (;;) {
        u32 part = (u32)InterlockedIncrement(&h->next_part) - 1;
        if (part >= h->parts) break;
        u32 eb = (u32)((u64)w->n_slots * part / h->parts);
        u32 ee = (u32)((u64)w->n_slots * (part + 1) / h->parts);
        u32 cb = (u32)((u64)w->n_cells * part / h->parts);
        u32 ce = (u32)((u64)w->n_cells * (part + 1) / h->parts);
        sum += az_hash_entities_batched(w, eb, ee) + az_hash_cells(w, cb, ce);
    }
    __atomic_fetch_add(&h->sum, sum, __ATOMIC_RELAXED);
}

static void CALLBACK stream_hash_worker(PTP_CALLBACK_INSTANCE instance, void *context, PTP_WORK work)
{
    (void)instance;
    (void)work;
    stream_hash_ranges(context);
}

static void stream_hasher_open(StreamHasher *h, u32 workers)
{
    h->workers = workers ? workers : 1;
    h->work = h->workers > 1 ? CreateThreadpoolWork(stream_hash_worker, h, NULL) : NULL;
    if (!h->work) h->workers = 1;
}

static void stream_hasher_close(StreamHasher *h)
{
    if (h->work) CloseThreadpoolWork(h->work);
}

static u64 stream_state_hash(StreamHasher *h, const AzWorld *w)
{
    if (h->workers == 1) return az_state_hash(w);
    h->world = w;
    h->parts = h->workers * 8;
    h->next_part = 0;
    h->sum = 0;
    for (u32 i = 1; i < h->workers; i++) SubmitThreadpoolWork(h->work);
    u64 tail = az_hash_tail(w);
    stream_hash_ranges(h);
    WaitForThreadpoolWorkCallbacks(h->work, FALSE);
    return tail + __atomic_load_n(&h->sum, __ATOMIC_RELAXED);
}
#endif
