#include "hermit_report.h"
#include "../core/az_hermit_hooks.h"
#include <inttypes.h>

/* cap < 2^31 (az_world.c config_ok). Use the high bit for the outcome and
   keep the two 32-bit links in a single preallocated row per potential sender. */
#define REFUSED (UINT32_C(1) << 31)
typedef struct { u32 target, next; } Proposal;
typedef struct { u32 first; u8 pad[60]; } WorkerList;
static _Alignas(64) WorkerList lists[AZ_MAX_WORKERS];
static Proposal *proposals;
static u8 *offers;
static const AzWorld *owner;
static AztRecordRefusal persist;
static void *context;
static u64 start, next, total, example_uid, example_tick, committed_bytes;
static bool complete;

u64 azt_hermit_bytes(u32 capacity)
{
    const u64 rounded = ((u64)capacity + 63u) & ~UINT64_C(63);
    return rounded * (sizeof(Proposal) + 1u);
}

bool azt_hermit_attach(const AzWorld *w, void *tail, u64 bytes, AztRecordRefusal record, void *ctx)
{
    if (owner || !w || !tail || ((uintptr_t)tail & 63u) || bytes < azt_hermit_bytes(w->cap) || !record) return false;
    owner = w;
    proposals = tail;
    offers = (u8 *)(proposals + w->cap);
    for (u32 i = 0; i < AZ_MAX_WORKERS; ++i) lists[i].first = AZ_NONE;
    persist = record;
    context = ctx;
    complete = true;
    start = next = w->tick;
    total = example_uid = example_tick = 0;
    committed_bytes = bytes;
    return true;
}

void azt_hermit_detach(void)
{
    owner = NULL;
    proposals = NULL;
    offers = NULL;
    persist = NULL;
    context = NULL;
    complete = false;
}

/* Each sender offers at most once. Only its executing worker changes that
   sender's row and that worker's list. All readers wait for the claims job. */
void azt_hermit_proposal(const AzWorld *w, u32 sender, u32 r, u32 worker, u32 act, i32 score)
{
    if (w != owner || !complete || w->tick != next) return;
    u32 marked = r;
    if (act == AZ_ACT_RECRUIT && score == AZ_RECRUIT_UNWILLING && !w->cfg.lesion) {
        const u32 c = w->pub[w->spos[sender]].colony;
        if (!((w->flags[sender] | w->flags[r]) & (AZ_F_EGG | AZ_F_DORMANT)) &&
            !w->colony[r] && !w->bond[r] && !w->partner[r] && c && c < w->n_colonies && w->colonies[c].alive)
            marked |= REFUSED;
    }
    proposals[sender] = (Proposal){marked, lists[worker].first};
    lists[worker].first = sender;
}

/* Run after all workers finished scoring, while the validated recipients
   still own those physical slots and before any retirement or slot reuse. */
void azt_hermit_barrier(const AzWorld *w)
{
    if (w != owner || !complete) return;
    if (w->tick != next || next == UINT64_MAX) { complete = false; return; }
    const u32 workers = w->host ? w->host->n_workers : 1;
    for (u32 k = 0; k < workers; ++k)
        for (u32 s = lists[k].first; s != AZ_NONE; s = proposals[s].next) {
            u8 *n = &offers[proposals[s].target & ~REFUSED];
            if (*n < 2) ++*n;
        }
    for (u32 k = 0; k < workers; ++k)
        for (u32 s = lists[k].first; s != AZ_NONE; s = proposals[s].next) {
            const u32 marked = proposals[s].target, r = marked & ~REFUSED;
            if (!(marked & REFUSED) || offers[r] != 1) continue;
            const u64 uid = w->uid[r];
            if (total == UINT64_MAX || !persist(context, uid)) complete = false;
            else ++total;
            if (w->tick != example_tick || uid > example_uid) { example_tick = w->tick; example_uid = uid; }
        }
    for (u32 k = 0; k < workers; ++k) {
        for (u32 s = lists[k].first; s != AZ_NONE; s = proposals[s].next)
            offers[proposals[s].target & ~REFUSED] = 0;
        lists[k].first = AZ_NONE;
    }
    ++next;
}

bool azt_hermit_window(const AzWorld *w)
{
    return owner && w == owner && complete && next == w->tick;
}

void azt_hermit_json(FILE *out, const AzWorld *w, u32 covered, u32 partial, u32 positive, bool exact)
{
    if (w != owner || !owner) {
        fprintf(out,",\"hermit\":{\"available\":false,\"window_complete\":false,\"lifetime_covered_live\":0,"
                    "\"partial_live\":%u,\"eligible_refusals\":null}",partial);
        return;
    }
    fprintf(out,",\"hermit\":{\"available\":true,\"basis\":\"sole_eligible_recruitment_refusal\","
            "\"from_tick\":\"%" PRIu64 "\",\"to_tick_exclusive\":\"%" PRIu64 "\",\"locked_observer_bytes\":\"%" PRIu64
            "\",\"window_complete\":%s,\"lifetime_covered_live\":%u,\"partial_live\":%u,"
            "\"living_observed_refusers\":%u,\"eligible_refusals\":",start,next,committed_bytes,
            exact ? "true" : "false",covered,partial,positive);
    if (exact) fprintf(out,"\"%" PRIu64 "\"",total);
    else fputs("null",out);
    fputs(",\"example\":",out);
    if (example_uid) fprintf(out,"{\"tick\":\"%" PRIu64 "\",\"recipient_uid\":\"%" PRIu64 "\"}",example_tick,example_uid);
    else fputs("null",out);
    fputc('}',out);
}
