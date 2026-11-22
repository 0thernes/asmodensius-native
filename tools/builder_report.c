#include "builder_report.h"
#include "intake_report.h"
#include "../core/az_builder_hooks.h"
#include <inttypes.h>

typedef struct { u32 next, cell; u8 strength, without, expected; } NestPost;
_Static_assert(sizeof(NestPost) == 12, "one sparse mark-post row per potential sender");
typedef struct { u32 first; u8 pad[60]; } WorkerHead;
static _Alignas(64) WorkerHead heads[AZ_MAX_WORKERS];
static NestPost *posts;
static const AzWorld *owner;
static u64 first_tick, next_tick, events, units, reserved;
static u64 example_tick, example_uid;
static u32 example_cell, example_without, example_after;
static bool complete, prepared;

u64 azt_builder_bytes(u32 cap)
{
    return (((u64)cap + 63u) & ~UINT64_C(63)) * sizeof(NestPost);
}

bool azt_builder_attach(const AzWorld *w, void *tail, u64 bytes)
{
    if (owner || !w || !azt_intake_is_attached(w) || !tail || ((uintptr_t)tail & 63u) || bytes < azt_builder_bytes(w->cap)) return false;
    owner = w;
    posts = tail;
    for (u32 k = 0; k < AZ_MAX_WORKERS; ++k) heads[k].first = AZ_NONE;
    first_tick = next_tick = w->tick;
    events = units = example_uid = example_tick = 0;
    reserved = bytes;
    complete = true;
    prepared = false;
    return true;
}

void azt_builder_detach(void)
{
    owner = NULL;
    posts = NULL;
    complete = prepared = false;
}

/* The native pantry pulses at most once per actor per tick. Each worker
   owns its list and each slot has one writer until the join. */
void azt_builder_post(const AzWorld *w, u32 slot, u32 worker, u32 cell, u32 strength)
{
    if (w != owner || !complete || w->tick != next_tick) return;
    posts[slot] = (NestPost){.next=heads[worker].first, .cell=cell, .strength=(u8)strength};
    heads[worker].first = slot;
}

static u8 standing_after(u32 before, u32 pending)
{
    return pending > 255u - before ? 255 : (u8)(before + pending);
}

void azt_builder_before_commit(const AzWorld *w)
{
    if (w != owner || !complete) return;
    if (prepared || w->tick != next_tick || next_tick == UINT64_MAX) { complete = false; return; }
    const u32 workers = w->n_workers;
    for (u32 k = 0; k < workers; ++k)
        for (u32 s = heads[k].first; s != AZ_NONE; s = posts[s].next) {
            NestPost *p = &posts[s];
            const u32 before = w->c_mark[(size_t)p->cell * AZ_NMARK + AZ_MK_NEST];
            const u32 pending = w->c_acc[p->cell].dmark[AZ_MK_NEST];
            if (pending < p->strength) { complete = false; return; }
            /* Even when the u32 sum clipped, removing a <=254-unit request
               leaves far more than the 255 needed for the standing byte. */
            p->without = standing_after(before, pending - p->strength);
            p->expected = standing_after(before, pending);
        }
    prepared = true;
}

void azt_builder_after_commit(const AzWorld *w)
{
    if (w != owner || !complete) return;
    if (!prepared || w->tick != next_tick) { complete = false; return; }
    const u32 workers = w->n_workers;
    for (u32 k = 0; k < workers; ++k) {
        for (u32 s = heads[k].first; s != AZ_NONE; s = posts[s].next) {
            const NestPost *p = &posts[s];
            const u32 after = w->c_mark[(size_t)p->cell * AZ_NMARK + AZ_MK_NEST];
            if (after != p->expected) { complete = false; return; }
            if (after <= p->without) continue;
            const u32 credited = after - p->without;
            if (events == UINT64_MAX || units > UINT64_MAX - credited || !azt_intake_record_builder(w, s)) {
                complete = false;
                return;
            }
            ++events;
            units += credited;
            if (!example_uid || w->tick != example_tick || w->uid[s] > example_uid) {
                example_uid = w->uid[s];
                example_tick = w->tick;
                example_cell = p->cell;
                example_without = p->without;
                example_after = after;
            }
        }
        heads[k].first = AZ_NONE;
    }
    ++next_tick;
    prepared = false;
}

bool azt_builder_window(const AzWorld *w)
{
    return owner && w == owner && complete && !prepared && next_tick == w->tick;
}

void azt_builder_json(FILE *out, const AzWorld *w, u32 covered, u32 partial, u32 positive, bool exact)
{
    if (!owner || owner != w) {
        fprintf(out, ",\"builder\":{\"available\":false,\"window_complete\":false,\"lifetime_covered_live\":0,"
                "\"partial_live\":%u,\"credited_occasions\":null,\"marginal_mark_units\":null}", partial);
        return;
    }
    fprintf(out, ",\"builder\":{\"available\":true,\"basis\":\"credited_nest_mark\",\"from_tick\":\"%" PRIu64
            "\",\"to_tick_exclusive\":\"%" PRIu64 "\",\"locked_observer_bytes\":\"%" PRIu64 "\",\"window_complete\":%s,"
            "\"lifetime_covered_live\":%u,\"partial_live\":%u,\"living_observed_builders\":%u,",
            first_tick, next_tick, reserved, exact ? "true" : "false", covered, partial, positive);
    if (exact) fprintf(out, "\"credited_occasions\":\"%" PRIu64 "\",\"marginal_mark_units\":\"%" PRIu64 "\"", events, units);
    else fputs("\"credited_occasions\":null,\"marginal_mark_units\":null", out);
    fputs(",\"example\":", out);
    if (example_uid)
        fprintf(out,"{\"tick\":\"%" PRIu64 "\",\"actor_uid\":\"%" PRIu64 "\",\"cell\":%u,\"without\":%u,\"after\":%u}",
                example_tick,example_uid,example_cell,example_without,example_after);
    else fputs("null", out);
    fputc('}', out);
}
