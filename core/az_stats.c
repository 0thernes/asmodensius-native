/* Between-tick population statistics. The simulation supplies state and resolved
 * actions. Classification is read-only so observing the world cannot change its
 * trajectory. Rank complete, attributable activity counts over their common age
 * denominator. Preserve fractional travel and the lower-ID rule for exact ties.
 * A range of admissible waste origins can leave the winning activity unknown.
 *
 * Distributions use order statistics at floor(p * (n - 1)). Bytewise radix
 * selection shares prefix counts among ranks using fixed scratch, so the core
 * neither allocates a population-sized array nor changes slot order. Colony
 * size counts current living members; the cached Tier 2 count may lag a death.
 * References: docs/eval/DECISIONS.md, DEC-0069 through DEC-0074, and
 * build/work/w2/stats/DESIGN.md, Revised colony quantile population.
 */
#include "az_abi.h"
#include "az_stats_traits.h"

static bool higher_activity(u64 events, u32 id, u64 top_events, u32 top_id, u32 travel_fraction)
{
    if (events != top_events) return events > top_events;
    if (travel_fraction && (id == AZ_ARCH_NOMAD || top_id == AZ_ARCH_NOMAD))
        return id == AZ_ARCH_NOMAD && top_id != AZ_ARCH_NOMAD;
    return id < top_id;
}


u32 az_classify_observed(const AzWorld *w, u32 s, const AzClassObservations *x)
{
    const u32 *v = w->stat + (size_t)s * AZ_NSTAT;
    u64 score[AZ_ARCH_COUNT] = {0};
    /* All rates have the same age denominator. One activity unit is one native
       event, one unit of intake or stolen matter, or one cell of travel. */
    for (u32 i = 0; i < 4; ++i)
        score[AZ_ARCH_PRODUCER + i] = x ? (x->intake_complete ? x->intake_occasions[i] : 0) : v[AZ_ST_EAT_L + i];
    if (x && x->predation_complete) score[AZ_ARCH_PREDATOR] = x->predation_events;
    if (x && x->teaching_complete) score[AZ_ARCH_TEACHER] = x->teaching_events;
    if (x && x->declined_recruit_complete) score[AZ_ARCH_HERMIT] = x->declined_recruit_events;
    if (x && x->commons_complete) score[AZ_ARCH_FREELOADER] = x->withheld_contribution_events;
    if (x && x->builder_complete) score[AZ_ARCH_BUILDER] = x->builder_events;
    score[AZ_ARCH_THIEF] = v[AZ_ST_STOLEN];
    score[AZ_ARCH_ALTRUIST] = v[AZ_ST_SHARES];
    score[AZ_ARCH_TRADER] = v[AZ_ST_TRADES];
    score[AZ_ARCH_WARRIOR] = v[AZ_ST_ATTACKS];
    score[AZ_ARCH_DOMINANT] = v[AZ_ST_WINS];
    score[AZ_ARCH_SUBORDINATE] = v[AZ_ST_LOSSES];
    const u64 measured_travel = (u64)v[AZ_ST_TRAVEL] + w->travel_carry[s];
    const u32 travel_q16 = measured_travel > UINT32_MAX ? UINT32_MAX : (u32)measured_travel;
    score[AZ_ARCH_NOMAD] = travel_q16 >> 16;
    score[AZ_ARCH_SIGNALLER] = v[AZ_ST_SIGNALS];
    /* PARTNERS counts bond formations, including renewals with a former mate.
       It cannot establish the number or overlap of distinct partners. */
    if (x && x->partner_complete) {
        score[AZ_ARCH_MONOGAMIST] = x->exclusive_bonded_matings;
        score[AZ_ARCH_POLYGAMIST] = x->overlapping_bonded_mates;
    }
    score[AZ_ARCH_BROODER] = v[AZ_ST_OFFSPRING];
    u32 best = AZ_ARCH_NONE;
    const u32 fraction = travel_q16 & UINT16_MAX;
    for (u32 i = 1; i < AZ_ARCH_COUNT; ++i)
        if (higher_activity(score[i], i, score[best], best, fraction)) best = i;
    if (x && x->intake_complete && best != AZ_ARCH_DECOMPOSER &&
        x->waste_possible_occasions &&
        higher_activity(x->waste_possible_occasions, AZ_ARCH_DECOMPOSER, score[best], best, fraction))
        return AZ_ARCH_NONE;
    return best;
}

u32 az_classify(const AzWorld *w, u32 s)
{
    return az_classify_observed(w, s, NULL);
}

u32 az_look_class(const AzWorld *w, u32 s)
{
    static const u8 empty[16] = {0};
    const u8 *b = w->clade[s] < w->n_clades ? w->clades[w->clade[s]].body : empty;
    const u8 *t = w->trait + (size_t)s * AZ_TRAITS;
    const u32 hue = (u32)((i32)b[AZ_B_HUE] + ((i32)t[AZ_T_HUE] - 128) * 3 / 8) & 255u;
    const u32 sat = (u32)az_clamp_i32((i32)b[AZ_B_SAT] + ((i32)t[AZ_T_SAT] - 128) / 2, 0, 255);
    const u32 pattern = (u32)az_clamp_i32((i32)b[AZ_B_PATTERN] + t[AZ_T_PATTERN] - 128, 0, 255);
    const u32 sheen = (u32)az_clamp_i32((i32)b[AZ_B_SHEEN] + t[AZ_T_SHEEN] - 128, 0, 255);
    const u32 glow = (u32)az_clamp_i32((i32)b[AZ_B_GLOW] + t[AZ_T_GLOW] - 128, 0, 255);
    /* Expressed body-plan coordinates used by the native silhouette. Multiple
       encodings of a lobe count, symmetry or segment count share one value. */
    const i32 spike = az_clamp_i32(384 * (b[AZ_B_SPIKES] >> 3) + 31 * ((i32)t[AZ_T_SPIKES] - 128), 0, 15872);
    u64 shape = b[AZ_B_SHAPE_M] & 7u;
    for (u32 j = AZ_B_SHAPE_N1; j <= AZ_B_ELONG; ++j) shape = (shape << 8) | b[j];
    const u32 segments = b[AZ_B_SEGMENTS] % 6u;
    shape = (shape << 3) | (segments >= 2 ? segments : 0u);
    shape = (shape << 1) | (b[AZ_B_SYMMETRY] & 1u);
    shape = (shape << 3) | (spike ? (b[AZ_B_SPIKES] & 7u) : 0u);
    shape = (shape << 6) | ((u32)spike >> 8);
    const u64 skin = ((u64)(sat >> 5) << 32) | ((u64)(pattern >> 5) << 28) | ((sheen >> 5) << 24)
        | ((glow >> 5) << 20) | ((u32)b[AZ_B_GLYPH_A] << 8) | b[AZ_B_GLYPH_B];
    return (hue >> 5) * 8u + (u32)(az_mix64(shape ^ az_mix64(skin)) & 7u);
}

/* Tier 2 refreshes its member cache periodically. A census counts the current
   live slots instead. One pass over the slots supplies up to 128 active
   colonies, even when their IDs are far apart. Each group uses 1 KiB. */
static AZ_NOINLINE void colony_bins(const AzWorld *w, u64 lo, u64 prefix, unsigned shift,
                                    u32 bins[256], u64 *minimum, u64 *maximum)
{
    enum { GROUP = 128 };
    for (u32 next = 1; next < w->n_colonies;) {
        u32 ids[GROUP], members[GROUP] = {0};
        u32 n = 0;
        while (next < w->n_colonies && n < GROUP) {
            if (w->colonies[next].alive) ids[n++] = next;
            ++next;
        }
        if (!n) break;
        const u32 first = ids[0], last = ids[n - 1];
        const bool dense = last - first == n - 1;
        for (u32 s = 0; s < w->n_slots; ++s) {
            const u32 id = w->colony[s];
            if (id < first || id > last || !(w->flags[s] & AZ_F_ALIVE)) continue;
            if (dense) {
                ++members[id - first];
            } else {
                u32 left = 0, right = n - 1;
                while (left < right) {
                    const u32 mid = (left + right) / 2;
                    if (ids[mid] < id) left = mid + 1;
                    else right = mid;
                }
                if (ids[left] == id) ++members[left];
            }
        }
        for (u32 j = 0; j < n; ++j) {
            const u64 x = members[j];
            if (minimum) {
                if (x < *minimum) *minimum = x;
                if (x > *maximum) *maximum = x;
            }
            const u64 bucket = (x - lo) >> shift;
            if ((bucket >> 8) == prefix) ++bins[bucket & 255u];
        }
    }
}


static u64 value(const AzWorld *w, u32 dim, u32 i)
{
    if (dim == AZ_STATS_DEPTH) return w->generation[i];
    if (!w->blk[i]) return 0;
    const AzBlk *b = az_blk(w, i);
    switch (dim) {
    case AZ_STATS_GENES: return b->len[AZ_SEG_GEN_A] / sizeof(AzGene) + (u64)(b->len[AZ_SEG_GEN_B] / sizeof(AzGene));
    case AZ_STATS_PARTS: return b->len[AZ_SEG_BODY] / sizeof(AzPart);
    case AZ_STATS_BODY_MATTER: return az_body_matter(w, i);
    case AZ_STATS_MEMORY: return b->len[AZ_SEG_MEMORY] / sizeof(u64);
    default: {
        u32 bytes;
        const AzBrain *br = az_brain(w, i, &bytes);
        if (bytes < sizeof(*br)) return 0;
        return dim == AZ_STATS_NEURONS ? 8u * br->n_hb : (u64)br->n_t1 + br->n_t2;
    }
    }
}

static void distribution(const AzWorld *w, u32 dim, u32 n, u64 q[AZ_STATS_QUANTILES])
{
    static const u8 pct[] = {5, 25, 50, 75, 95};
    if (!n) return;
    u32 counts[256] = {0};
    u64 lo = UINT64_MAX, hi = 0;
    if (dim == AZ_STATS_COLONY_SIZE) {
        colony_bins(w, 0, 0, 0, counts, &lo, &hi);
    } else {
        for (u32 i = 0; i < w->n_slots; ++i) if (w->flags[i] & AZ_F_ALIVE) {
            const u64 x = value(w, dim, i);
            if (x < lo) lo = x;
            if (x > hi) hi = x;
        }
    }
    q[0] = lo;
    q[6] = hi;
    if (lo == hi) {
        for (u32 j = 1; j < 6; ++j) q[j] = lo;
        return;
    }
    /* The initial colony scan already binned every size up to 255. */
    const bool bins_ready = dim == AZ_STATS_COLONY_SIZE && hi <= 255u;
    if (bins_ready) lo = 0;
    u32 rank[5];
    u64 prefix[5] = {0};
    for (u32 j = 0; j < 5; ++j) rank[j] = (u32)((u64)pct[j] * (n - 1u) / 100u);
    unsigned shift = 0;
    while (((hi - lo) >> shift) > 255u) shift += 8;
    for (;;) {
        for (u32 start = 0; start < 5;) {
            u32 end = start + 1;
            while (end < 5 && prefix[end] == prefix[start]) ++end;
            const u64 base = prefix[start];
            if (!bins_ready) {
                az_memset(counts, 0, sizeof counts);
                if (dim == AZ_STATS_COLONY_SIZE) {
                    colony_bins(w, lo, base, shift, counts, NULL, NULL);
                } else {
                    for (u32 i = 0; i < w->n_slots; ++i) if (w->flags[i] & AZ_F_ALIVE) {
                        const u64 bucket = (value(w, dim, i) - lo) >> shift;
                        if ((bucket >> 8) == base) ++counts[bucket & 255u];
                    }
                }
            }
            u32 bin = 0, before = 0;
            for (u32 j = start; j < end; ++j) {
                while (rank[j] - before >= counts[bin]) before += counts[bin++];
                rank[j] -= before;
                prefix[j] = (base << 8) | bin;
            }
            start = end;
        }
        if (!shift) break;
        shift -= 8;
    }
    for (u32 j = 0; j < 5; ++j) q[j + 1] = lo + prefix[j];
}

static void traits(const AzWorld *w, AzStats *out)
{
    for (u32 start = 0; start < AZ_TRAITS; start += 32) {
        u64 sum[32] = {0}, squares[32] = {0};
        const u32 width = az_min_u32(32, AZ_TRAITS - start);
        for (u32 s = 0; s < w->n_slots; ++s) if (w->flags[s] & AZ_F_ALIVE) {
            const u8 *p = w->trait + (size_t)s * AZ_TRAITS + start;
            add_trait_values(sum, squares, p, width);
        }
        for (u32 j = 0; j < width; ++j) trait_moments(out, start + j, out->n_live, sum[j], squares[j]);
    }
}

/* Count distinct sparse founder IDs in occupied 8192-ID windows. This only
   runs for a population actually containing IDs outside species_live[]. */
static u32 beyond_species(const AzWorld *w, u32 first)
{
    u32 total = 0;
    for (;;) {
        const u32 base = first & ~8191u;
        u64 words[128] = {0};
        u32 next = 0;
        bool more = false;
        for (u32 s = 0; s < w->n_slots; ++s) if (w->flags[s] & AZ_F_ALIVE) {
            const u32 id = w->lineage[s];
            if (id < 128 || id < base) continue;
            const u32 off = id - base;
            if (off < 8192) {
                const u64 bit = UINT64_C(1) << (off & 63u);
                if (!(words[off >> 6] & bit)) { words[off >> 6] |= bit; ++total; }
            } else if (!more || id < next) { next = id; more = true; }
        }
        if (!more) return total;
        first = next;
    }
}

static u64 mean_q8(u64 sum, u32 n)
{
    return ((sum / n) << 8) + ((sum % n) << 8) / n;
}

void az_stats_collect(const AzWorld *w, AzStats *out)
{
    az_memset(out, 0, sizeof(*out));
    u64 energy = 0, mass = 0, ages = 0;
    u32 carries = 0, min_beyond = UINT32_MAX;
    bool beyond = false;
    for (u32 s = 0; s < w->n_slots; ++s) {
        const u8 f = w->flags[s];
        if (!(f & AZ_F_ALIVE)) continue;
        ++out->n_live;
        out->n_eggs += !!(f & AZ_F_EGG);
        out->n_juv += !!(f & AZ_F_JUV);
        out->n_senescent += !!(f & AZ_F_SENESC);
        out->n_dormant += !!(f & AZ_F_DORMANT);
        out->n_bonded += w->bond[s] != 0;
        out->n_colonial += w->colony[s] != 0;
        if (w->role[s] < AZ_ROLE_COUNT) ++out->role[w->role[s]];
        ++out->archetype[az_classify(w, s)];
        ++out->look[az_look_class(w, s)];
        ++out->mating_mode[w->phen[(size_t)s * AZ_PHEN + AZ_P_MATE] & 7u];
        const u32 id = w->lineage[s];
        if (id < 128) ++out->species_live[id];
        else { beyond = true; if (id < min_beyond) min_beyond = id; }
        const u32 *eat = w->stat + (size_t)s * AZ_NSTAT + AZ_ST_EAT_L;
        u32 diet = 0;
        for (u32 j = 1; j < 4; ++j) if (eat[j] > eat[diet]) diet = j;
        if (eat[diet]) ++out->diet[diet];
        else ++out->n_diet_unknown;
        energy += (u32)(w->energy[s] > 0 ? w->energy[s] : 0);
        mass += w->mass[s];
        const u64 age = w->tick - w->birth_tick[s];
        ages += age;
        carries += ages < age;
        if (w->generation[s] > out->max_generation) out->max_generation = w->generation[s];
    }
    for (u32 c = 0; c < w->n_clades; ++c) out->n_clades += w->clades[c].alive && w->clades[c].refcount;
    for (u32 c = 1; c < w->n_colonies; ++c) out->n_colonies += w->colonies[c].alive != 0;
    if (!out->n_live && !out->n_colonies) return;
    for (u32 j = 0; j < AZ_STATS_DISTRIBUTIONS; ++j)
        distribution(w, j, j == AZ_STATS_COLONY_SIZE ? out->n_colonies : out->n_live, out->quantile[j]);
    if (!out->n_live) return;
    if (beyond) out->n_species_beyond = beyond_species(w, min_beyond);
    out->n_species_live = out->n_species_beyond;
    for (u32 i = 0; i < 128; ++i) out->n_species_live += out->species_live[i] != 0;
    for (u32 i = 0; i < 64; ++i) out->n_looks += out->look[i] != 0;
    const u32 n = out->n_live;
    out->mean_energy_q8 = mean_q8(energy, n);
    out->mean_mass_q8 = mean_q8(mass, n);
    /* Divide a 96-bit age sum by n in 32-bit limbs. carries is less than n. */
    const u64 upper = ((u64)carries << 32) | (ages >> 32);
    const u64 lower = ((upper % n) << 32) | (u32)ages;
    const u64 whole = ((upper / n) << 32) | (lower / n);
    if (whole > (UINT64_MAX >> 8)) { out->mean_age_q8 = UINT64_MAX; out->age_q8_saturated = 1; }
    else out->mean_age_q8 = (whole << 8) + ((lower % n) << 8) / n;
    traits(w, out);
}
