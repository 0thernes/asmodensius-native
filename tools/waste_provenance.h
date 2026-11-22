#ifndef AZT_WASTE_PROVENANCE_H
#define AZT_WASTE_PROVENANCE_H
#include "../core/az_abi.h"

/* u32 matter per tick for up to u64 ticks. Keep per-UID physical sums in 96 bits. */
typedef struct { u64 low; u32 high; } AztWasteAmount;
typedef struct {
    AztWasteAmount converted, external_min, external_max;
    u64 conversions, certain_events, possible_events;
    u32 pool_min, pool_max, retained_min_path, retained_max_path;
} AztWasteHistory;

static inline void azt_waste_sum(AztWasteAmount *sum, u32 value)
{
    sum->low += value;
    sum->high += sum->low < value;
}

static inline __uint128_t azt_waste_value(AztWasteAmount x)
{
    return ((__uint128_t)x.high << 64) | x.low;
}

static inline void azt_waste_gain(AztWasteHistory *h, u32 external)
{
    h->pool_min += external;
    h->pool_max += external;
    h->retained_min_path += external;
    h->retained_max_path += external;
}

static inline u32 azt_waste_must(u32 take, u32 total, u32 external)
{
    const u32 other = total - external;
    return take > other ? take - other : 0;
}

static inline void azt_waste_drain(AztWasteHistory *h, u32 total, u32 take, bool converted)
{
    /* Minimize cumulative external conversion by preferring internal for conversion
     * and external for other drains. The maximum follows the opposite routing. */
    const u32 min_take = converted ? azt_waste_must(take,total,h->retained_min_path)
                                   : az_min_u32(take,h->retained_min_path);
    const u32 max_take = converted ? az_min_u32(take,h->retained_max_path)
                                   : azt_waste_must(take,total,h->retained_max_path);
    if (converted) {
        ++h->conversions;
        h->certain_events += azt_waste_must(take,total,h->pool_min) != 0;
        h->possible_events += az_min_u32(take,h->pool_max) != 0;
        azt_waste_sum(&h->converted,take);
        azt_waste_sum(&h->external_min,min_take);
        azt_waste_sum(&h->external_max,max_take);
    }
    h->retained_min_path -= min_take;
    h->retained_max_path -= max_take;
    /* Independent extrema of the remaining stock, irrespective of earlier routing. */
    h->pool_min -= az_min_u32(take,h->pool_min);
    h->pool_max = az_min_u32(h->pool_max,total-take);
}

static inline void azt_waste_votes(const AztWasteHistory *h, u64 *lower, u64 *upper)
{
    *lower = h->certain_events;
    if (!*lower && (h->external_min.low || h->external_min.high)) *lower = 1;
    *upper = h->possible_events;
    if (!h->external_max.high && *upper > h->external_max.low) *upper = h->external_max.low;
}
#endif
