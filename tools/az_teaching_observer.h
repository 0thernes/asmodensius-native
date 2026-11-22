/* Between-tick summaries of native resolved TEACH events. The host's ordinary
   ledger reader still receives every complete original batch. Format 9 stores
   none of this optional history, so a loaded survivor starts without a past. */
#ifndef AZ_TEACHING_OBSERVER_H
#define AZ_TEACHING_OBSERVER_H
#include "../core/az_abi.h"
#include "../platform/win/az_host_win.h"
#ifdef AZ_INTAKE_OBSERVER
#include "intake_report.h"
#ifdef AZ_HERMIT_OBSERVER
#include "hermit_report.h"
#ifdef AZ_PARTNER_OBSERVER
#include "partner_report.h"
#endif
#ifdef AZ_BUILDER_OBSERVER
#include "builder_report.h"
#endif
#endif
#endif
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct { u64 uid; u32 acts, refusals; } AztTeacher;
_Static_assert(sizeof(AztTeacher) == 16, "teaching and refusal counts share the original 16-byte entry");
typedef struct {
    AztTeacher *rows;
    size_t capacity, used;
    u64 start, next, first_covered_uid, events, example_tick, actor, learner;
    u32 initial_drops;
    bool complete;
    AzHost *host;
#ifdef AZ_INTAKE_OBSERVER
    void *intake_tail;
    u64 intake_bytes;
#ifdef AZ_HERMIT_OBSERVER
#ifdef AZ_BUILDER_OBSERVER
    void *builder_tail;
    u64 builder_bytes;
#endif
#ifdef AZ_PARTNER_OBSERVER
    void *partner_tail;
    u64 partner_bytes;
#endif
    void *hermit_tail;
    u64 hermit_bytes;
#endif
#endif
    void (*forward)(void *, const AzEvent *, u32);
    void *forward_ctx;
} AztTeaching;

typedef struct {
    const AztTeaching *history;
    u32 covered, partial, positive;
    bool exact;
#ifdef AZ_HERMIT_OBSERVER
    u32 refusal_covered, refusal_partial, refusal_positive;
    bool refusal_exact;
#ifdef AZ_BUILDER_OBSERVER
    u32 builder_covered, builder_partial, builder_positive;
    bool builder_exact;
#endif
#endif
} AztTeachingSummary;

static inline size_t azt_teacher_index(const AztTeaching *o, u64 uid)
{
    const size_t mask = o->capacity - 1;
    size_t i = (size_t)az_mix64(uid) & mask;
    while (o->rows[i].uid && o->rows[i].uid != uid) i = (i + 1) & mask;
    return i;
}

/* One OS allocation. The world sees only its original prefix. The tail is
   retired with the allocation and never serialized or handed to core allocators. */
static inline size_t azt_teaching_entries(u32 cap)
{
    const u64 rounded = ((u64)cap + 63u) & ~UINT64_C(63);
    size_t entries = 1;
    while (entries < rounded * 2u) entries *= 2u;
    return entries;
}

static inline bool azt_teaching_arena(AzArena *arena, u64 bytes, u32 cap, u32 flags,
                                     AzWinInfo *info, bool enabled, AztTeaching *o)
{
    *o = (AztTeaching){0};
    const u64 offset = (bytes + 63u) & ~UINT64_C(63);
    if (enabled && (flags & AZ_WIN_LOCK)) {
        const size_t count = azt_teaching_entries(cap);
        const u64 teaching_bytes = (u64)count * sizeof *o->rows;
        u64 needed = teaching_bytes;
#ifdef AZ_INTAKE_OBSERVER
        needed += azt_intake_bytes(cap);
#endif
        bool allocated = false;
#ifdef AZ_HERMIT_OBSERVER
        const u64 hermit_bytes = azt_hermit_bytes(cap);
        if (offset >= bytes && needed <= UINT64_MAX - offset && hermit_bytes <= UINT64_MAX - offset - needed) {
            const u64 original = offset + needed + hermit_bytes;
            /* The builder and relationship probes can be dropped independently.
               A fully observed run lays out builder first, then relationships.
               If both do not fit, try the larger relationship sector alone,
               then the smaller builder sector, then the original prefix. */
            u64 builder_bytes = 0, partner_bytes = 0;
#ifdef AZ_BUILDER_OBSERVER
            builder_bytes = azt_builder_bytes(cap);
#endif
#ifdef AZ_PARTNER_OBSERVER
            partner_bytes = azt_partner_bytes(cap);
#endif
            const u32 locked_flags = flags & ~(u32)AZ_WIN_ALLOW_PAGEABLE;
            u64 take_builder = 0, take_partner = 0;
            if (builder_bytes && partner_bytes && builder_bytes <= UINT64_MAX - original &&
                partner_bytes <= UINT64_MAX - original - builder_bytes &&
                az_win_arena(arena, original + builder_bytes + partner_bytes, locked_flags, info)) {
                take_builder = builder_bytes;
                take_partner = partner_bytes;
            } else if (partner_bytes && partner_bytes <= UINT64_MAX - original &&
                       az_win_arena(arena, original + partner_bytes, locked_flags, info)) {
                take_partner = partner_bytes;
            } else if (builder_bytes && builder_bytes <= UINT64_MAX - original &&
                       az_win_arena(arena, original + builder_bytes, locked_flags, info)) {
                take_builder = builder_bytes;
            }
            allocated = take_builder || take_partner;
#ifdef AZ_BUILDER_OBSERVER
            if (take_builder) {
                o->builder_tail = arena->base + original;
                o->builder_bytes = take_builder;
            }
#endif
#ifdef AZ_PARTNER_OBSERVER
            if (take_partner) {
                o->partner_tail = arena->base + original + take_builder;
                o->partner_bytes = take_partner;
            }
#endif
            if (!allocated)
                allocated = az_win_arena(arena, original, flags & ~(u32)AZ_WIN_ALLOW_PAGEABLE, info);
        }
        if (allocated) {
            o->hermit_tail = arena->base + offset + needed;
            o->hermit_bytes = hermit_bytes;
        }
#endif
        if (allocated || (offset >= bytes && needed <= UINT64_MAX - offset &&
            az_win_arena(arena, offset + needed, flags & ~(u32)AZ_WIN_ALLOW_PAGEABLE, info))) {
            o->rows = (AztTeacher *)(arena->base + offset);
            o->capacity = count;
#ifdef AZ_INTAKE_OBSERVER
            o->intake_bytes = needed - teaching_bytes;
            o->intake_tail = arena->base + offset + teaching_bytes;
#endif
            return true;
        }
    }
    return az_win_arena(arena, bytes, flags, info);
}

static inline AztTeacher *azt_behavior_row(AztTeaching *o, u64 uid)
{
    if (!uid || !o->capacity) return NULL;
    const size_t i = azt_teacher_index(o, uid);
    if (!o->rows[i].uid) {
        if (o->used >= o->capacity / 2) return NULL;
        o->rows[i].uid = uid;
        ++o->used;
    }
    return &o->rows[i];
}

static inline bool azt_teacher_record(AztTeaching *o, u64 uid)
{
    AztTeacher *r = azt_behavior_row(o, uid);
    if (!r) return false;
    if (r->acts < UINT32_MAX) ++r->acts;
    return true;
}

#ifdef AZ_HERMIT_OBSERVER
static inline bool azt_record_refusal(void *ctx, u64 uid)
{
    AztTeacher *r = azt_behavior_row(ctx, uid);
    if (!r || r->refusals == UINT32_MAX) return false;
    ++r->refusals;
    return true;
}
#endif

static inline void azt_teacher_forget(AztTeaching *o, u64 uid)
{
    if (!o->capacity || !uid) return;
    size_t i = azt_teacher_index(o, uid);
    if (!o->rows[i].uid) return;
    o->rows[i] = (AztTeacher){0};
    --o->used;
    /* Reinsert only the following collision cluster to keep lookups valid. */
    for (i = (i + 1) & (o->capacity - 1); o->rows[i].uid; i = (i + 1) & (o->capacity - 1)) {
        const AztTeacher entry = o->rows[i];
        o->rows[i] = (AztTeacher){0};
        o->rows[azt_teacher_index(o, entry.uid)] = entry;
    }
}

/* The real ledger sorts by kind and then actor UID, ending with a summary. */
static inline u32 azt_teaching_first(const AzEvent *events, u32 size, u32 kind)
{
    u32 lo = 0, hi = size;
    while (lo < hi) {
        const u32 mid = lo + (hi - lo) / 2;
        if (events[mid].kind < kind) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

static inline void azt_teaching_receive(void *ctx, const AzEvent *ev, u32 n)
{
    AztTeaching *o = ctx;
    if (o->forward) o->forward(o->forward_ctx, ev, n);
    if (!n || ev[n - 1].kind != AZ_EV_TICK) { o->complete = false; return; }
    const AzEvent *summary = ev + n - 1;
    if (summary->tick != o->next || summary->aux != o->initial_drops ||
        summary->a != n - 1u || summary->tick == UINT64_MAX) o->complete = false;
    o->next = summary->tick + 1u;
    for (u32 i = azt_teaching_first(ev, n - 1, AZ_EV_TEACH); i < n - 1 && ev[i].kind == AZ_EV_TEACH; ++i) {
        if (o->events < UINT64_MAX) ++o->events; else o->complete = false;
        o->example_tick = ev[i].tick;
        o->actor = ev[i].a;
        o->learner = ev[i].b;
        if (!azt_teacher_record(o, ev[i].a)) o->complete = false;
    }
    /* An actor may teach then die in the same tick, regardless of ledger sort order. */
    if (o->used)
        for (u32 i = azt_teaching_first(ev, n - 1, AZ_EV_DEATH); i < n - 1 && ev[i].kind == AZ_EV_DEATH; ++i)
            azt_teacher_forget(o, ev[i].a);
}

static inline void azt_teaching_attach(AztTeaching *o, const AzWorld *w, bool loaded)
{
    o->start = o->next = w->tick;
    o->first_covered_uid = loaded ? w->uid_next : 1;
    o->complete = o->capacity && w->host && w->totals.dropped_events < UINT32_MAX;
    o->initial_drops = (u32)w->totals.dropped_events;
    o->host = w->host;
    if (o->host) {
        o->forward = o->host->sink;
        o->forward_ctx = o->host->sink_ctx;
        o->host->sink = azt_teaching_receive;
        o->host->sink_ctx = o;
    }
}

static inline void azt_teaching_detach(AztTeaching *o)
{
    if (o->host) {
        o->host->sink = o->forward;
        o->host->sink_ctx = o->forward_ctx;
    }
    *o = (AztTeaching){0};
}

static inline bool azt_teaching_class(const AztTeaching *o, const AzWorld *w, u32 slot, u64 *acts)
{
    const u64 uid = w->uid[slot];
    *acts = o->used ? o->rows[azt_teacher_index(o, uid)].acts : 0;
    return o->complete && o->next == w->tick && uid >= o->first_covered_uid;
}

/* Only full-lifetime observations can compete with the other lifetime scores. */
typedef void (*AztClassFill)(const AzWorld *, u32, AzClassObservations *);
static inline AztTeachingSummary azt_teaching_apply(const AztTeaching *o, const AzWorld *w,
                                                   AzStats *st, AztClassFill fill)
{
    AztTeachingSummary result = {.history=o, .exact=o->complete && o->next == w->tick};
#ifdef AZ_HERMIT_OBSERVER
    result.refusal_exact = result.exact && azt_hermit_window(w);
#ifdef AZ_BUILDER_OBSERVER
    result.builder_exact = azt_builder_window(w);
#endif
#endif
    for (u32 s = 0; s < w->n_slots; ++s) if (w->flags[s] & AZ_F_ALIVE) {
        AzClassObservations evidence = {0};
        const u64 uid = w->uid[s];
        const AztTeacher *row = o->used ? &o->rows[azt_teacher_index(o, uid)] : NULL;
        evidence.teaching_events = row ? row->acts : 0;
        evidence.teaching_complete = result.exact && uid >= o->first_covered_uid;
        result.positive += evidence.teaching_events != 0;
        if (evidence.teaching_complete) ++result.covered;
        else ++result.partial;
#ifdef AZ_HERMIT_OBSERVER
        evidence.declined_recruit_complete = result.refusal_exact && uid >= o->first_covered_uid;
        evidence.declined_recruit_events = row ? row->refusals : 0;
        result.refusal_positive += evidence.declined_recruit_events != 0;
        if (evidence.declined_recruit_complete) ++result.refusal_covered;
        else ++result.refusal_partial;
#endif
#ifdef AZ_BUILDER_OBSERVER
        evidence.builder_complete = result.builder_exact && uid >= o->first_covered_uid;
        evidence.builder_events = azt_intake_builder_events(w, s);
        result.builder_positive += evidence.builder_events != 0;
        if (evidence.builder_complete) ++result.builder_covered;
        else ++result.builder_partial;
#endif
        /* Merge intake, predation, commons and verified partnerships into the
           same per-UID evidence as teaching, building and recruit refusals. */
        if (fill) fill(w, s, &evidence);
        const u32 *legacy_intake = w->stat + (size_t)s * AZ_NSTAT + AZ_ST_EAT_L;
        if (fill || evidence.builder_events || evidence.teaching_events || evidence.declined_recruit_events || legacy_intake[0] || legacy_intake[1] || legacy_intake[2] || legacy_intake[3]) {
            const u32 legacy = az_classify(w, s);
            const u32 observed = az_classify_observed(w, s, &evidence);
            --st->archetype[legacy];
            ++st->archetype[observed];
        }
    }
    return result;
}

static inline void azt_teaching_json(FILE *f, const AztTeachingSummary *x)
{
    const AztTeaching *o = x->history;
    fprintf(f, ",\"teaching\":{\"from_tick\":\"%" PRIu64 "\",\"to_tick_exclusive\":\"%" PRIu64
            "\",\"locked_observer_bytes\":\"%" PRIu64 "\",\"window_complete\":%s,\"resolved_events\":\"%" PRIu64 "\","
            "\"lifetime_covered_live\":%u,\"partial_live\":%u,\"living_observed_actors\":",
            o->start, o->next, (u64)o->capacity * sizeof *o->rows, x->exact ? "true" : "false", o->events, x->covered, x->partial);
    if (o->capacity) fprintf(f, "%u", x->positive);
    else fputs("null", f);
    fputs(",\"example\":", f);
    if (o->actor)
        fprintf(f, "{\"tick\":\"%" PRIu64 "\",\"teacher_uid\":\"%" PRIu64 "\",\"learner_uid\":\"%" PRIu64 "\"}",
                o->example_tick, o->actor, o->learner);
    else fputs("null", f);
    fputc('}', f);
}
#endif
