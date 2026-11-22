#include "intake_report.h"
#include "observed_rows.h"
#include "waste_provenance.h"
#ifdef AZ_PARTNER_OBSERVER
#include "partner_report.h"
#endif
#include "../core/az_intake_hooks.h"
#include <inttypes.h>
#include <string.h>

enum { LANES = 5 };
typedef struct {
    u64 uid, occasions[LANES], units[LANES];
    AztWasteHistory waste;
    u32 high[LANES];
#ifdef AZ_BUILDER_OBSERVER
    u32 builder_events;
#endif
#ifdef AZ_PREDATION_OBSERVER
    u64 predation_events, predation_mu;
    u32 predation_high;
#endif
#ifdef AZ_COMMONS_OBSERVER
    u64 pool_founder, pool_birth, pending_receipts, receipts, contributions, withheld;
#endif
} IntakeRow;

/* One main-thread-owned reporting world in each command-line program. Only a
 * slot's owning worker changes its row; reorder and reporting are serial. */
static const AzWorld *observed_world;
static IntakeRow *rows;
static u8 *move_flags;
static u64 first_tick, first_complete_uid;

u64 azt_intake_bytes(u32 capacity)
{
    const u64 cap = ((u64)capacity + 63u) & ~UINT64_C(63);
    return (cap * (sizeof(IntakeRow) + 1u) + 63u) & ~UINT64_C(63);
}

bool azt_intake_attach(const AzWorld *w, bool from_worldgen, void *tail, u64 bytes)
{
    if (observed_world || !w || !tail || (uintptr_t)tail % 64u || bytes < azt_intake_bytes(w->cap)) return false;
    /* Reattaching with reused storage begins a new observation interval. */
    memset(tail, 0, (size_t)azt_intake_bytes(w->cap));
    rows = tail;
    move_flags = (u8 *)(rows + w->cap);
    first_tick = w->tick;
    first_complete_uid = from_worldgen ? 0 : w->uid_next;
    /* Restored living UIDs can carry matter acquired before attachment.
     * Its original source is unknown; future births begin with empty history. */
    if (!from_worldgen) for (u32 s = 0; s < w->n_slots; ++s) {
        if (!(w->flags[s] & AZ_F_ALIVE) || !w->waste[s]) continue;
        rows[s].uid = w->uid[s];
        rows[s].waste.pool_max = rows[s].waste.retained_max_path = w->waste[s];
    }
    observed_world = w;
    return true;
}

void azt_intake_detach(void)
{
    observed_world = NULL;
    rows = NULL;
    move_flags = NULL;
}

static IntakeRow *intake_row(const AzWorld *w, u32 slot)
{
    IntakeRow *r = rows + slot;
    if (r->uid != w->uid[slot]) {
        memset(r, 0, sizeof(*r));
        r->uid = w->uid[slot];
    }
    return r;
}

#ifdef AZ_PREDATION_OBSERVER
void azt_predation_settled(const AzWorld *w, u32 slot, u32 credited_mu)
{
    if (w != observed_world || !credited_mu) return;
    IntakeRow *r = intake_row(w, slot);
    ++r->predation_events;
    r->predation_mu += credited_mu;
    r->predation_high += r->predation_mu < credited_mu;
}
#endif

#ifdef AZ_COMMONS_OBSERVER
void azt_commons_pulse(const AzWorld *w, u32 slot, const AzColony *colony,
                       bool received, bool contributed, bool eligible)
{
    if (w != observed_world) return;
    IntakeRow *row = intake_row(w, slot);
    if (row->pool_founder != colony->founder_uid || row->pool_birth != colony->born_tick) {
        row->pool_founder = colony->founder_uid;
        row->pool_birth = colony->born_tick;
        row->pending_receipts = 0;
    }
    if (received) {
        ++row->receipts;
        ++row->pending_receipts;
    } else if (contributed) {
        ++row->contributions;
        row->pending_receipts = 0;
    } else if (eligible && row->pending_receipts) {
        --row->pending_receipts;
        ++row->withheld;
    }
}
#endif

#ifdef AZ_BUILDER_OBSERVER
bool azt_intake_is_attached(const AzWorld *w)
{
    return observed_world && observed_world == w;
}

bool azt_intake_record_builder(const AzWorld *w, u32 slot)
{
    if (w != observed_world) return false;
    IntakeRow *r = intake_row(w, slot);
    if (r->builder_events == UINT32_MAX) return false;
    ++r->builder_events;
    return true;
}

u32 azt_intake_builder_events(const AzWorld *w, u32 slot)
{
    return w == observed_world && rows[slot].uid == w->uid[slot] ? rows[slot].builder_events : 0;
}
#endif

void azt_waste_removed(const AzWorld *w, u32 slot, u32 total, u32 amount, bool converted)
{
    if (w != observed_world || !amount) return;
    azt_waste_drain(&intake_row(w,slot)->waste,total,amount,converted);
}

/* Each settled lane can contribute at most once per UID per tick. Amounts
 * span 96 bits. The high limb lives off the common path until the low limb wraps. */
static inline void record_intake_lane(IntakeRow *r, u32 lane, u32 amount)
{
    if (!amount) return;
    ++r->occasions[lane];
    r->units[lane] += amount;
    if (r->units[lane] < amount) ++r->high[lane];
}

void azt_intake_settled(const AzWorld *w, u32 slot, u32 light, u32 nut_x, u32 org_y,
                        u32 wst_fixed, u32 wst_carried)
{
    if (w != observed_world) return;
    IntakeRow *r = intake_row(w, slot);
    if (wst_carried) azt_waste_gain(&r->waste,wst_carried);
    record_intake_lane(r, 0, light);
    record_intake_lane(r, 1, nut_x);
    record_intake_lane(r, 2, org_y);
    record_intake_lane(r, 3, wst_fixed);
    record_intake_lane(r, 4, wst_carried);
}

void azt_intake_reorder(const AzWorld *w)
{
    if (w != observed_world) return;
    IntakeRow saved;
    azt_reorder_rows(w,rows,sizeof *rows,&saved,move_flags);
}

static bool intake_range(const AzWorld *w, u32 slot, u64 occasions[4], u64 *possible)
{
    *possible = 0;
    for (u32 j = 0; j < 4; ++j) occasions[j] = 0;
    if (w != observed_world || w->uid[slot] < first_complete_uid) return false;
    if (rows[slot].uid == w->uid[slot]) {
        for (u32 j = 0; j < 3; ++j) occasions[j] = rows[slot].occasions[j];
        azt_waste_votes(&rows[slot].waste,&occasions[3],possible);
    }
    return true;
}

bool azt_intake_class(const AzWorld *w, u32 slot, u64 occasions[4])
{
    u64 upper;
    return intake_range(w,slot,occasions,&upper) && occasions[3] == upper;
}

void azt_intake_fill(const AzWorld *w, u32 slot, AzClassObservations *observation)
{
    observation->intake_complete = intake_range(w,slot,observation->intake_occasions,
                                                &observation->waste_possible_occasions);
#ifdef AZ_PREDATION_OBSERVER
    observation->predation_complete = observation->intake_complete;
    observation->predation_events = observation->predation_complete && rows[slot].uid == w->uid[slot]
        ? rows[slot].predation_events : 0;
#else
    observation->predation_complete = false;
    observation->predation_events = 0;
#endif
#ifdef AZ_COMMONS_OBSERVER
    observation->commons_complete = observation->intake_complete;
    observation->withheld_contribution_events = 0;
    if (observation->commons_complete) {
        const IntakeRow *row = rows + slot;
        if (row->uid == w->uid[slot] && !row->contributions)
            observation->withheld_contribution_events = row->withheld;
    }
#endif
#ifdef AZ_PARTNER_OBSERVER
    azt_partner_fill(w,slot,observation);
#endif
}

void azt_intake_apply(const AzWorld *w, AzStats *st)
{
    /* Funded occasions are comparable across channels. Only complete history
     * can determine a lifetime category. Equal positive counts use lower lane ID. */
    memset(st->diet, 0, sizeof(st->diet));
    st->n_diet_unknown = 0;
    for (u32 s = 0; s < w->n_slots; ++s) if (w->flags[s] & AZ_F_ALIVE) {
        u64 votes[4], upper;
        if (!intake_range(w,s,votes,&upper)) { ++st->n_diet_unknown; continue; }
        u32 best = 0;
        for (u32 j = 1; j < 3; ++j) if (votes[j] > votes[best]) best = j;
        if (votes[3] > votes[best]) ++st->diet[3];
        else if (upper <= votes[best] && votes[best]) ++st->diet[best];
        else ++st->n_diet_unknown;
    }
}

/* Living population <= u32 slots, each amount <= u32 per tick for <= u64
 * ticks. The cohort total fits 128 bits. Emit decimal strings losslessly. */
static void decimal(FILE *f, __uint128_t x)
{
    char buf[41]; char *p = buf + sizeof(buf);
    *--p = 0;
    do { *--p = (char)('0' + (unsigned)(x % 10)); x /= 10; } while (x);
    fprintf(f, "\"%s\"", p);
}

typedef struct {
    u32 covered, partial, converting, uncertain_units, uncertain_occasions, uncertain_diet, partial_with_conversion;
    __uint128_t conversions, biomass, ext_min, ext_max, event_min, event_max;
} AztWasteSummary;

static void waste_collect(AztWasteSummary *summary, const IntakeRow *row, bool complete, bool same_uid)
{
    if (complete) ++summary->covered;
    else ++summary->partial;
    if (!same_uid || !row->waste.conversions) return;
    const AztWasteHistory *h = &row->waste;
    const __uint128_t lo = azt_waste_value(h->external_min), hi = azt_waste_value(h->external_max);
    u64 certain, possible;
    azt_waste_votes(h,&certain,&possible);
    ++summary->converting;
    summary->partial_with_conversion += !complete;
    summary->conversions += h->conversions;
    summary->biomass += azt_waste_value(h->converted);
    summary->ext_min += lo; summary->ext_max += hi;
    summary->event_min += certain; summary->event_max += possible;
    summary->uncertain_units += lo != hi;
    summary->uncertain_occasions += certain != possible;
    u64 rival = 0;
    for (u32 j = 0; j < 3; ++j) if (row->occasions[j] > rival) rival = row->occasions[j];
    summary->uncertain_diet += complete && certain <= rival && rival < possible;
}

static void waste_json(FILE *f, const AzWorld *w, const AztWasteSummary *summary)
{
    fprintf(f,",\"waste_conversion\":{\"available\":true,\"from_tick\":\"%" PRIu64 "\",\"to_tick_exclusive\":\"%" PRIu64
        "\",\"amount_scope\":\"observed_interval_living_uids\",\"external_source\":\"field_waste_taken_into_carried_pool\","
        "\"lifetime_covered_live\":%u,\"partial_live\":%u,\"converting_live\":%u,\"partial_converting_live\":%u,"
        "\"source_units_ambiguous_live\":%u,\"source_occasions_ambiguous_live\":%u,\"diet_winner_ambiguous_live\":%u,"
        "\"occasions\":",first_tick,w->tick,summary->covered,summary->partial,summary->converting,summary->partial_with_conversion,summary->uncertain_units,summary->uncertain_occasions,summary->uncertain_diet);
    decimal(f,summary->conversions); fputs(",\"biomass_formed_mu\":",f); decimal(f,summary->biomass);
    fputs(",\"external_converted_mu_lower\":",f); decimal(f,summary->ext_min);
    fputs(",\"external_converted_mu_upper\":",f); decimal(f,summary->ext_max);
    fputs(",\"external_occasions_lower\":",f); decimal(f,summary->event_min);
    fputs(",\"external_occasions_upper\":",f); decimal(f,summary->event_max); fputc('}',f);
}

void azt_intake_json(FILE *f, const AzWorld *w)
{
    AztWasteSummary waste = {0};
    if (w != observed_world) { fputs(",\"waste_conversion\":{\"available\":false},\"intake\":{\"available\":false},\"predation\":{\"available\":false}", f); return; }
    const char *const names[LANES] = {"light_eu","nut_x_mu","org_y_mu","wst_fixed_mu","wst_carried_mu"};
    u32 coverage[2][2] = {{0}}, users[LANES] = {0};
#ifdef AZ_PREDATION_OBSERVER
    u32 predation_coverage[2][2] = {{0}};
    __uint128_t predation_events = 0, predation_mu = 0;
#endif
    __uint128_t units[LANES] = {0}, events[LANES] = {0};
    for (u32 s = 0; s < w->n_slots; ++s) if (w->flags[s] & AZ_F_ALIVE) {
        const IntakeRow *r = &rows[s];
        const bool complete = w->uid[s] >= first_complete_uid, same_uid = r->uid == w->uid[s];
#ifdef AZ_PREDATION_OBSERVER
        const bool fed = same_uid && r->predation_events != 0;
        ++predation_coverage[complete][fed];
        if (fed) {
            predation_events += r->predation_events;
            predation_mu += ((__uint128_t)r->predation_high << 64) | r->predation_mu;
        }
#endif
        waste_collect(&waste,r,complete,same_uid);
        const bool positive = same_uid && (r->occasions[0] | r->occasions[1] | r->occasions[2] |
                                           r->occasions[3] | r->occasions[4]);
        ++coverage[complete][positive];
        if (!positive) continue;
        for (u32 j = 0; j < LANES; ++j) {
            users[j] += r->occasions[j] != 0;
            events[j] += r->occasions[j];
            units[j] += ((__uint128_t)r->high[j] << 64) | r->units[j];
        }
    }
    waste_json(f,w,&waste);
    fprintf(f, ",\"intake\":{\"available\":true,\"from_tick\":\"%" PRIu64 "\",\"to_tick_exclusive\":\"%" PRIu64
        "\",\"locked_observer_bytes\":\"%" PRIu64 "\",\"complete_positive\":%u,\"complete_zero\":%u,\"partial_positive\":%u,\"partial_zero\":%u,"
        "\"amount_scope\":\"observed_interval_living_uids\",\"diet_basis\":\"positive_funding_occasions_lower_id_tie\",\"channels\":{",
        first_tick, w->tick, azt_intake_bytes(w->cap), coverage[1][1], coverage[1][0], coverage[0][1], coverage[0][0]);
    for (u32 j = 0; j < LANES; ++j) {
        fprintf(f,"%s\"%s\":{\"uids\":%u,\"occasions\":",j?",":"",names[j],users[j]);
        decimal(f, events[j]); fputs(",\"amount\":",f); decimal(f, units[j]); fputc('}',f);
    }
    fputs("}}", f);
#ifdef AZ_PREDATION_OBSERVER
    fprintf(f, ",\"predation\":{\"available\":true,\"from_tick\":\"%" PRIu64 "\",\"to_tick_exclusive\":\"%" PRIu64
            "\",\"complete_positive\":%u,\"complete_zero\":%u,\"partial_positive\":%u,\"partial_zero\":%u,"
            "\"amount_scope\":\"observed_interval_living_uids\",\"occasions\":",
            first_tick, w->tick, predation_coverage[1][1], predation_coverage[1][0],
            predation_coverage[0][1], predation_coverage[0][0]);
    decimal(f, predation_events);
    fputs(",\"assimilated_mu\":", f); decimal(f, predation_mu); fputc('}', f);
#else
    fputs(",\"predation\":{\"available\":false}", f);
#endif
}

#ifdef AZ_COMMONS_OBSERVER
void azt_commons_json(FILE *out, const AzWorld *w)
{
    if (w != observed_world) { fputs(",\"commons\":{\"available\":false}", out); return; }
    u32 covered = 0, partial = 0, qualifying = 0;
    __uint128_t received = 0, donated = 0, held = 0;
    for (u32 s = 0; s < w->n_slots; ++s) if (w->flags[s] & AZ_F_ALIVE) {
        const bool complete = w->uid[s] >= first_complete_uid;
        if (complete) ++covered; else ++partial;
        const IntakeRow *row = &rows[s];
        if (row->uid != w->uid[s]) continue;
        received += row->receipts;
        donated += row->contributions;
        held += row->withheld;
        if (complete && !row->contributions && row->withheld) ++qualifying;
    }
    fprintf(out,",\"commons\":{\"available\":true,\"from_tick\":\"%" PRIu64 "\",\"to_tick_exclusive\":\"%" PRIu64
            "\",\"lifetime_covered_live\":%u,\"partial_live\":%u,\"qualifying_live\":%u,\"receipt_pulses\":",
            first_tick,w->tick,covered,partial,qualifying);
    decimal(out,received); fputs(",\"contribution_pulses\":",out); decimal(out,donated);
    fputs(",\"matched_withholdings\":",out); decimal(out,held); fputc('}',out);
}
#endif
