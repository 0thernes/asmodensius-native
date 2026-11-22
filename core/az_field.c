/* az_field.c - field plumbing: grant and commit (ABI 3).
 *
 * The rule that keeps the world exact: entities and field dynamics only post
 * integer changes (az_cell_post, az_cell_post_heat, az_cell_demand,
 * az_cell_mark); this file is the only place that applies them to the FIELD
 * columns.
 *
 * Speed: both range functions run over every cell every tick, and most cells
 * have nothing pending. A cell's demand (AzCellNeed) and its pending changes
 * (AzCellAcc) are one cache line each, so the range is walked eight cells at
 * a time and the lines of the group are ORed into one word: a group with
 * nothing pending costs a few 32-byte loads and one branch. Only groups with
 * something pending take the per-cell path, and there the eight species of a
 * cell are applied as one vector when none of them leaves the u32 range. The
 * result is the same as doing every cell one by one, for any [begin, end).
 */
#include "az_abi.h"

/* 32 bytes of a column seen as lanes. The views are declared unaligned and
 * may alias the column structs. */
typedef u64 AzVecQ __attribute__((vector_size(32), aligned(1), may_alias));
typedef u32 AzVecU __attribute__((vector_size(32), aligned(1), may_alias));
typedef i32 AzVecI __attribute__((vector_size(32), aligned(1), may_alias));

_Static_assert(AZ_NSPECIES * sizeof(u32) == 32, "the species of one cell must be one 32-byte group");
_Static_assert(AZ_NDIET * sizeof(u64) == 2 * sizeof(AzVecQ), "scan both vector halves of each demand line");
_Static_assert(offsetof(AzCellNeed, dem) == 0 && sizeof(AzCellNeed) == 64, "AzCellNeed layout");
_Static_assert(offsetof(AzCellAcc, delta) == 0 && sizeof(AzCellAcc) == 64, "AzCellAcc layout");

#define AZ_FIELD_GROUP 8u   /* cells (cache lines) tested together */

/* OR of n 32-byte groups, `stride` bytes apart, folded into one word: zero
 * iff all of them are zero */
AZ_INLINE u64 az_or32(const void *p, u32 n, size_t stride)
{
    const u8 *b = (const u8 *)p;
    AzVecQ a = *(const AzVecQ *)b;
    for (u32 i = 1; i < n; i++) a |= *(const AzVecQ *)(b + (size_t)i * stride);
    return a[0] | a[1] | a[2] | a[3];
}

/* ---------------------------------------------------------------------- */
/* what a cell offers                                                      */
/* ---------------------------------------------------------------------- */
/* uptake channel -> field species (AZ_D_LIGHT is not a species) */
static const u8 az_diet_species[AZ_NDIET] = {
    [AZ_D_LIGHT] = 0,           [AZ_D_NUT_X] = AZ_SP_NUT_X, [AZ_D_ORG_Y] = AZ_SP_ORG_Y,
    [AZ_D_WST_W] = AZ_SP_WST_W, [AZ_D_OXI] = AZ_SP_OXI,     [AZ_D_STR_S] = AZ_SP_STR_S,
    [AZ_D_TOX_Z] = AZ_SP_TOX_Z, [AZ_D_SOLV] = AZ_SP_SOLV
};

AZ_INLINE u32 avail_of(const AzWorld *w, u32 c, u32 diet)   /* diet < AZ_NDIET */
{
    if (diet == AZ_D_LIGHT) return (u32)w->c_light[c] * AZ_LIGHT_EU;
    return w->c_sp[(size_t)c * AZ_NSPECIES + az_diet_species[diet]];
}

u32 az_field_avail(const AzWorld *w, u32 c, u32 diet)
{
    return diet < AZ_NDIET ? avail_of(w, c, diet) : 0u;
}

/* Pressure: weight of the gas and solvent in this cell and in every cell
 * above it, scaled down. Every term is non-negative, and the shift and the
 * clamp are monotone, so pressure never decreases with depth. Every module
 * must use this one function. */
static u64 layer_weight(const u32 *species)
{
    return (u64)species[AZ_SP_GAS]+4ull*species[AZ_SP_SOLV];
}

static u32 hydrostatic_readout(u64 sum)
{
    sum >>= 6;
    return sum>UINT32_MAX ? UINT32_MAX : (u32)sum;
}

void az_publish_pressure(AzWorld *w)
{
    const u32 plane=w->gx*w->gy;
    /* Each disjoint vertical column is integrated once, top to bottom. The
     * 64-column block reads contiguous cell rows and keeps its sums on stack. */
    for(u32 start=0; start<plane; start+=64u) {
        u64 sums[64]={0}; const u32 n=az_min_u32(64u,plane-start);
        for(u32 z=w->gz; z-- > 0;) {
            const u32 base=z*plane+start;
            for(u32 x=0; x<n; x++) {
                sums[x]+=layer_weight(&w->c_sp[(size_t)(base+x)*AZ_NSPECIES]);
                w->c_pressure[base+x]=hydrostatic_readout(sums[x]);
            }
        }
    }
}

u32 az_cell_pressure(const AzWorld *w, u32 c)
{
    const u64 plane = (u64)w->gx * w->gy;   /* one step up in z */
    u64 sum = 0;
    for (u64 cc = c; cc < w->n_cells; cc += plane) {
        const u32 *sp = &w->c_sp[(size_t)cc * AZ_NSPECIES];
        sum += layer_weight(sp);
    }
    return hydrostatic_readout(sum);
}

/* ---------------------------------------------------------------------- */
/* grant: the fraction of each demand that can be met                      */
/* ---------------------------------------------------------------------- */
/* An entity takes floor(demand * grant / 65535) (az_cell_granted). With
 * grant = floor(avail * 65535 / dem) the takes sum to at most avail; with
 * grant = 65535 (dem <= avail) every entity takes exactly its demand. The
 * demand is cleared here, so there is no per-tick reset pass; a grant is
 * only meaningful for a channel that had demand this tick, and the grant of
 * a channel without demand is left as it was. */
AZ_INLINE void grant_cell(AzWorld *w, u32 c)
{
    AzCellNeed *nd = &w->c_need[c];
    for (u32 d = 0; d < AZ_NDIET; d++) {
        u64 dem = nd->dem[d];
        if (!dem) continue;
        u32 avail = avail_of(w, c, d);
        /* avail < dem on the division branch, so the quotient is below 65535 */
        w->c_grant[(size_t)c * AZ_NDIET + d] = dem <= avail ? (u16)65535u : (u16)(((u64)avail * 65535u) / dem);
        nd->dem[d] = 0;
    }
}

void az_field_grant(AzWorld *w, u32 begin, u32 end, u32 worker)
{
    (void)worker;
    u32 c = begin;
    for (; c < end && end - c >= AZ_FIELD_GROUP; c += AZ_FIELD_GROUP) {
        if (!az_or32(w->c_need[c].dem, 2u * AZ_FIELD_GROUP, sizeof(AzVecQ))) continue;
        for (u32 k = 0; k < AZ_FIELD_GROUP; k++)
            if (az_or32(w->c_need[c + k].dem, 2, sizeof(AzVecQ))) grant_cell(w, c + k);
    }
    for (; c < end; c++) grant_cell(w, c);
}

/* ---------------------------------------------------------------------- */
/* commit: apply and clear the pending changes                             */
/* ---------------------------------------------------------------------- */
static void commit_cell(AzWorld *w, u32 c, AzTally *ty)
{
    AzCellAcc *a = &w->c_acc[c];
    u32 *sp = &w->c_sp[(size_t)c * AZ_NSPECIES];
    const u32 *carry = &w->c_species_carry[(size_t)c * AZ_NSPECIES];
    if (AZ_UNLIKELY(az_or32(carry, 1, 0))) {
        for (u32 k = 0; k < AZ_NSPECIES; k++) az_commit_species(w, c, k, ty);
    } else if (az_or32(a->delta, 1, 0)) {
        const AzVecI d = *(const AzVecI *)a->delta;
        const AzVecU s = *(const AzVecU *)sp;
        const AzVecU n = s + (AzVecU)d;
        const AzVecI neg = d < 0;
        const AzVecI bad = ((AzVecI)(n > s) & neg) | ((AzVecI)(n < s) & ~neg);
        if (AZ_LIKELY(!az_or32(&bad, 1, 0))) {
            *(AzVecU *)sp = n;
            *(AzVecI *)a->delta = (AzVecI){ 0, 0, 0, 0, 0, 0, 0, 0 };
        } else {
            for (u32 k = 0; k < AZ_NSPECIES; k++) az_commit_species(w, c, k, ty);
        }
    }

    az_commit_heat(w, c, ty);

    /* dmark_id is the atomic max over posts of (strength << 32 | colony id):
     * the strongest post, ties to the higher id. It takes the cell when it is
     * at least as strong as the standing mark[0], that is the mark before
     * this commit's deposits are added; otherwise the owner stays. */
    u8 *mk = &w->c_mark[(size_t)c * AZ_NMARK];
    u64 post = a->dmark_id;
    if (post) {
        if ((post >> 32) >= mk[0]) w->c_mark_id[c] = (u32)post;
        a->dmark_id = 0;
    }
    if (a->dmark[0] | a->dmark[1] | a->dmark[2] | a->dmark[3]) {
        for (u32 k = 0; k < AZ_NMARK; k++) {
            u32 add = a->dmark[k];
            if (!add) continue;
            u32 room = 255u - mk[k];   /* compared, not summed: add may be anything up to 2^32 - 1 */
            if (add > room) { mk[k] = 255; ty->sat[AZ_SAT_MARK]++; }
            else mk[k] = (u8)(mk[k] + add);
            a->dmark[k] = 0;
        }
    }
}

void az_field_commit(AzWorld *w, u32 begin, u32 end, u32 worker)
{
    AzTally *ty = az_tally(w, worker);
    u32 c = begin;
    for (; c < end && end - c >= AZ_FIELD_GROUP; c += AZ_FIELD_GROUP) {
        /* both halves of each of the eight lines */
        const u32 *carry = &w->c_species_carry[(size_t)c * AZ_NSPECIES];
        if (!az_or32(&w->c_acc[c], 2 * AZ_FIELD_GROUP, 32) && !az_or32(carry, AZ_FIELD_GROUP, 32)) continue;
        for (u32 k = 0; k < AZ_FIELD_GROUP; k++)
            if (az_or32(&w->c_acc[c + k], 2, 32) || az_or32(carry + k * AZ_NSPECIES, 1, 0))
                commit_cell(w, c + k, ty);
    }
    for (; c < end; c++) commit_cell(w, c, ty);
}
