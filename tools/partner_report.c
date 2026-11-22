#include "partner_report.h"
#include "observed_rows.h"
#include "../core/az_partner_hooks.h"
#include <inttypes.h>

typedef struct {
    u64 uid, first, second, confirmed_bond, acts, mutual;
    u32 incoming;
} PartnerRow;

static const AzWorld *owner;
static PartnerRow *rows;
static u8 *marks;
static u32 *index_slots;
static size_t index_size;
static u64 first_uid, start, next, locked;
static bool continuous, prepared;
typedef struct {
    u32 covered, partial, none, once, repeat, multiple, mono, overlap;
    u32 partial_positive;
    u64 mono_uid, mono_peer, poly_uid, poly_a, poly_b;
} Counts;
static Counts counts;

static size_t index_capacity(u32 cap)
{
    size_t size = 1;
    while (size < (size_t)cap * 2u) size *= 2u;
    return size;
}

u64 azt_partner_bytes(u32 cap)
{
    const u64 rounded = ((u64)cap + 63u) & ~UINT64_C(63);
    return rounded * (sizeof(PartnerRow) + 1u) + index_capacity((u32)rounded) * sizeof(u32);
}

bool azt_partner_attach(const AzWorld *w, bool genesis, void *tail, u64 bytes)
{
    if (!w || owner || !tail || (uintptr_t)tail % 64u || bytes < azt_partner_bytes(w->cap)) return false;
    owner = w;
    rows = tail;
    marks = (u8 *)(rows + w->cap);
    index_slots = (u32 *)(void *)(marks + w->cap);
    index_size = index_capacity(w->cap);
    memset(rows, 0, (size_t)w->cap * sizeof *rows);
    first_uid = genesis ? 1 : w->uid_next;
    start = next = w->tick;
    locked = azt_partner_bytes(w->cap);
    continuous = true;
    prepared = false;
    return true;
}

void azt_partner_detach(void)
{
    owner = NULL;
    rows = NULL;
    marks = NULL;
    index_slots = NULL;
    continuous = prepared = false;
}

/* Each worker writes only its own slot. The published identities are immutable
   throughout parallel resolution, unlike the other participant's live bond. */
void azt_partner_resolved(const AzWorld *w, u32 s, u32 p)
{
    if (w != owner || !continuous || w->tick != next) return;
    PartnerRow *r = rows + s;
    if (r->uid != w->uid[s]) *r = (PartnerRow){.uid=w->uid[s]};
    const u64 peer = w->uid[p];
    if (!r->first) r->first = peer;
    else if (peer != r->first && !r->second) r->second = peer;
    ++r->acts; /* one native match per UID per tick */
    const bool bonded = w->bond[s] && w->partner[s] == peer;
    const AzPubId *own = &w->pub_id[w->spos[s]], *other = &w->pub_id[w->spos[p]];
    if (bonded && own->partner == peer && other->partner == r->uid) ++r->mutual;
    if (bonded) r->confirmed_bond = peer;
    if (!w->bond[s] || w->partner[s] != r->confirmed_bond) r->confirmed_bond = 0;
}

void azt_partner_tick(const AzWorld *w)
{
    if (w != owner) return;
    if (w->tick != next || next == UINT64_MAX) continuous = false;
    else ++next;
    prepared = false;
}

void azt_partner_reorder(const AzWorld *w)
{
    if (w != owner) return;
    PartnerRow saved;
    azt_reorder_rows(w,rows,sizeof *rows,&saved,marks);
    prepared = false;
}

/* The scratch index holds only living participants who actually resolved a
   mating. Its load is at most one half and slot zero is encoded as one. */
static size_t location(const AzWorld *w, u64 uid)
{
    size_t i = (size_t)az_mix64(uid) & (index_size - 1u);
    while (index_slots[i] && w->uid[index_slots[i]-1u] != uid)
        i = (i + 1u) & (index_size - 1u);
    return i;
}

void azt_partner_prepare(const AzWorld *w)
{
    if (w != owner) return;
    counts = (Counts){0};
    memset(index_slots,0,index_size*sizeof *index_slots);
    for (u32 s = 0; s < w->n_slots; ++s) if (w->flags[s] & AZ_F_ALIVE) {
        PartnerRow *r = rows+s;
        if (r->uid != w->uid[s]) continue;
        r->incoming = 0;
        index_slots[location(w,r->uid)] = s+1u;
    }
    for (u32 s = 0; s < w->n_slots; ++s) if (w->flags[s] & AZ_F_ALIVE) {
        const PartnerRow *r = rows+s;
        if (r->uid != w->uid[s] || !w->bond[s] || !r->confirmed_bond || w->partner[s] != r->confirmed_bond) continue;
        const u32 pos = index_slots[location(w,r->confirmed_bond)];
        if (pos) ++rows[pos-1u].incoming;
    }
    for (u32 s = 0; s < w->n_slots; ++s) if (w->flags[s] & AZ_F_ALIVE) {
        const PartnerRow *r = rows+s;
        const u64 uid = w->uid[s];
        const bool has = r->uid == uid && r->acts;
        if (!continuous || next != w->tick || uid < first_uid) {
            ++counts.partial;
            counts.partial_positive += has;
            continue;
        }
        ++counts.covered;
        if (!has) ++counts.none;
        else if (r->second) ++counts.multiple;
        else if (r->acts == 1) ++counts.once;
        else ++counts.repeat;
        if (has && !r->second && r->acts > 1 && r->mutual) {
            ++counts.mono;
            if (!counts.mono_uid || uid < counts.mono_uid) { counts.mono_uid = uid; counts.mono_peer = r->first; }
        }
        if (has && r->second && r->incoming > 1) {
            ++counts.overlap;
            if (!counts.poly_uid || uid < counts.poly_uid) counts.poly_uid = uid;
        }
    }
    if (counts.poly_uid) {
        for (u32 s = 0; s < w->n_slots; ++s) if (w->flags[s] & AZ_F_ALIVE) {
            const PartnerRow *r = rows+s;
            if (r->uid != w->uid[s] || !w->bond[s] || r->confirmed_bond != counts.poly_uid || w->partner[s] != counts.poly_uid) continue;
            const u64 uid = r->uid;
            if (!counts.poly_a || uid < counts.poly_a) { counts.poly_b = counts.poly_a; counts.poly_a = uid; }
            else if (!counts.poly_b || uid < counts.poly_b) counts.poly_b = uid;
        }
    }
    prepared = true;
}

void azt_partner_fill(const AzWorld *w, u32 s, AzClassObservations *e)
{
    if (w != owner || !prepared || !continuous || next != w->tick || w->uid[s] < first_uid) return;
    e->partner_complete = true;
    const PartnerRow *r = rows+s;
    if (r->uid != w->uid[s]) return;
    if (!r->second && r->acts > 1) e->exclusive_bonded_matings = r->mutual;
    if (r->second && r->incoming > 1) e->overlapping_bonded_mates = r->incoming;
}

void azt_partner_json(FILE *f, const AzWorld *w)
{
    if (w != owner || !prepared) {
        fputs(",\"partners\":{\"available\":false,\"window_complete\":false,\"observed\":null}",f);
        return;
    }
    fprintf(f,",\"partners\":{\"available\":true,\"window_complete\":%s,\"from_tick\":\"%" PRIu64
        "\",\"to_tick_exclusive\":\"%" PRIu64 "\",\"locked_observer_bytes\":\"%" PRIu64
        "\",\"lifetime_covered_live\":%u,\"partial_live\":%u,\"partial_observed_mating_live\":%u,"
        "\"zero\":%u,\"one_mating\":%u,\"repeated_single_partner\":%u,\"multiple_distinct_partners\":%u,"
        "\"repeated_exclusive_reciprocal\":%u,\"simultaneous_incoming_bonds\":%u,\"celibacy\":null,\"overlap_example\":",
        continuous && next==w->tick ? "true":"false", start,next,locked,counts.covered,counts.partial,
        counts.partial_positive,counts.none,counts.once,counts.repeat,counts.multiple,counts.mono,counts.overlap);
    if (counts.poly_uid)
        fprintf(f,"{\"uid\":\"%" PRIu64 "\",\"partners\":[\"%" PRIu64 "\",\"%" PRIu64 "\"]}",counts.poly_uid,counts.poly_a,counts.poly_b);
    else fputs("null",f);
    fputs(",\"exclusive_example\":",f);
    if (counts.mono_uid)
        fprintf(f,"{\"uid\":\"%" PRIu64 "\",\"partner_uid\":\"%" PRIu64 "\"}",counts.mono_uid,counts.mono_peer);
    else fputs("null",f);
    fputc('}',f);
}
