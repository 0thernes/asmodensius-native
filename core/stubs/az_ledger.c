/* az_ledger.c (reference stub) - hand the tick's event blocks to the host
 * sink, then empty the pool. The module owner adds canonical sorting, the
 * hash chain and SPSC rings. */
#include "../az_abi.h"

/* the canonical order the real module defines (az_abi.h) */
bool az_event_less(const AzEvent *x, const AzEvent *y)
{
    if (x->kind != y->kind) return x->kind < y->kind;
    if (x->a != y->a) return x->a < y->a;
    if (x->b != y->b) return x->b < y->b;
    if (x->aux != y->aux) return x->aux < y->aux;
    if (x->v != y->v) return x->v < y->v;
    return x->tick < y->tick;
}

void az_ledger_flush(AzWorld *w)
{
    const u32 blocks = az_events_close(w);
    for (u32 b = 0; b < blocks; b++)
        if (w->ev_fill[b] && w->host && w->host->sink)
            w->host->sink(w->host->sink_ctx, w->ev_pool + (size_t)b * AZ_EV_BLOCK, w->ev_fill[b]);
    az_events_reset(w);
}
