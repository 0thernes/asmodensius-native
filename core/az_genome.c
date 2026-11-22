/* az_genome.c - diploid heredity with genomes of any length (ARCHITECTURE
 * section 8).
 *
 * A genome is two haplotypes, each a list of genes of any length, kept in
 * the creature's block (segments AZ_SEG_GEN_A: from the bearing parent, and
 * AZ_SEG_GEN_B: from the other parent). A gene is 4 bytes: its family, its
 * allele (4 bases of 2 bits) and its expression (128 = 1.0). Nothing fixes
 * how many genes a haplotype has or which families are among them: mutation
 * and unequal exchange add, remove and copy genes, and the store is the only
 * bound (az_store.c).
 *
 * Contract of this module (the tests in tests/test_genome.c hold it):
 *   value of a family in one haplotype:
 *     clamp(128 + sum over its copies of (allele - 128) * expr / 128, 0, 255)
 *     (one sum, then one C division); a haplotype without a copy has none.
 *   expression of a core family from its two haplotype values a, b
 *   (hi = max, lo = min); with one value only, that value; with none, the
 *   family is absent and reads as the neutral 128:
 *     dom 0 additive       (a + b + 1) >> 1
 *     dom 1 high dominant  hi
 *     dom 2 low dominant   lo
 *     dom 3 overdominant   min(255, hi + (hi - lo) / 2)
 *   trait k = clamp(base + (sum_j w[k][j] * (expr(idx[k][j]) - 128) + latent[k]) / 16, 0, 255)
 *   (C division: truncation toward zero), and trait k = 0 when the genome
 *   carries no gene of the trait's first family idx[k][0]: no gene, no organ.
 *   latent[k]: every gene of a family outside the core adds
 *   weight * ((allele - 128) * expr / 256) to one trait; the trait and the
 *   weight (+-8..24) are a hash of the world's code key and the family.
 *   sex = parity of (a ^ b) of the two haplotype values of marker family 0
 *   (an absent value counts as 0): a heterozygote in parity is sex 1, so a
 *   sex-1 x sex-0 cross gives 1:1 by Mendel's rule.
 *   Meiosis: 1 + (RECOMB >> 6) chiasmata at independent uniform gaps; a
 *   gamete carries each chiasma with probability 1/2 (it is one of four
 *   chromatids), so the recombination fraction never exceeds 1/2. An
 *   exchange falls at the same fraction of the length of both haplotypes:
 *   between haplotypes of different length it is unequal and changes the
 *   number of genes.
 *   Mutation: lambda = (8 + MUT_RATE) * (64 + min(tox, 512)) / 16384 point
 *   mutations expected per birth (tox = radiotoxin in the mother's cell),
 *   16 Bernoulli trials, distinct bases, a mutated base always changes.
 *   AZ_LESION_MUTATION sets lambda to 0; AZ_LESION_TOXIN removes the
 *   radiotoxin term.
 *   The genome itself changes size. Per birth, at most one of: a gene is
 *   copied next to itself, a gene is lost, a block of 2..8 genes is copied,
 *   a block is lost, a new gene of any family appears (families outside the
 *   core act through the latent map above), a gene changes family. Their
 *   chances are lambda times the world rates GENE_DUP_RATE, GENE_DEL_RATE,
 *   GENE_NEW_RATE times the mother's own heritable rates (traits GENE_DUP,
 *   GENE_DEL, GENE_NEW). A gene's expression drifts too (EXPR_MUT_RATE): at
 *   0 the gene is silent.
 *   The body: a creature is born with 1 + (SEGMENT >> 5) parts (segment
 *   AZ_SEG_BODY), their kinds in proportion to its own traits. Parts are
 *   born empty; they fill by eating, and more are grown in life (az_metab,
 *   AZ_RQ_GROW).
 */
#include "az_abi.h"

#define GEN_POOL (AZ_TRAITS * 3)          /* family reads of the development map */
#define GEN_TRAIT_LOCI (AZ_CORE_FAMS - 4) /* every core family but the 4 markers */
#define GEN_MAP 64u                       /* points of a chromosome's length where an exchange can fall */
_Static_assert(GEN_POOL >= GEN_TRAIT_LOCI * 4 && GEN_POOL - GEN_TRAIT_LOCI * 4 <= GEN_TRAIT_LOCI,
               "each trait locus must be read 4 or 5 times");
_Static_assert(AZ_MODG * 2 <= GEN_TRAIT_LOCI, "the modulation loci are taken from the trait loci");
#define GEN_TRIALS 16u                    /* Bernoulli trials for point mutations */
#define GEN_TOX_CAP 512u

/* Philox draw with the tick word fixed at 0: az_rand4 as it is at tick 0.
 * World and species constants must not depend on when they are made. */
static void rand4_t0(const AzWorld *w, u32 id, u32 stream, u32 idx, u32 out[4])
{
    u32 ctr[4] = { id, 0u, stream, idx };
    az_philox4x32(out, ctr, w->key);
}

/* a sequential reader over the blocks (0, AZ_RS_CODE, 0), (0, AZ_RS_CODE, 1), ... */
typedef struct CodeStream { u32 buf[4]; u32 at; u32 block; } CodeStream;

static u32 code_next(const AzWorld *w, CodeStream *s)
{
    if (s->at >= 4u) { rand4_t0(w, 0u, AZ_RS_CODE, s->block++, s->buf); s->at = 0u; }
    return s->buf[s->at++];
}
static u32 code_below(const AzWorld *w, CodeStream *s, u32 n)
{
    return (u32)(((u64)code_next(w, s) * (u64)n) >> 32);
}
static void code_shuffle(const AzWorld *w, CodeStream *s, u8 *a, u32 n)
{
    for (u32 i = n - 1u; i > 0u; i--) {
        u32 j = code_below(w, s, i + 1u);
        u8 t = a[i]; a[i] = a[j]; a[j] = t;
    }
}

/* does pool[p] equal another member of its triple (other than position skip)? */
static bool triple_has(const u8 *pool, u32 triple, u32 skip, u8 v)
{
    for (u32 j = 0; j < 3u; j++) {
        u32 q = triple * 3u + j;
        if (q != skip && pool[q] == v) return true;
    }
    return false;
}

void az_genetic_code_init(AzWorld *w)
{
    AzGeneticCode *c = &w->code;
    CodeStream s = { { 0, 0, 0, 0 }, 4u, 0u };
    u8 pool[GEN_POOL];
    u8 perm[AZ_CORE_FAMS];

    /* the 4 marker loci (sex determination and kinship) come first and feed
     * no trait (finding v09: the sex locus fed about 4.5 traits, so founder
     * sex built a dimorphism into them that no module chose) */
    for (u32 l = 0; l < AZ_CORE_FAMS; l++) perm[l] = (u8)l;
    code_shuffle(w, &s, perm, AZ_CORE_FAMS);
    for (u32 i = 0; i < 4u; i++) c->marker_locus[i] = perm[i];
    u8 trait_loci[GEN_TRAIT_LOCI];
    for (u32 i = 0; i < GEN_TRAIT_LOCI; i++) trait_loci[i] = perm[4u + i];

    /* GEN_POOL reads over the other families: every one 4 times, the rest of
     * the pool a fifth time */
    for (u32 i = 0; i < GEN_TRAIT_LOCI; i++)
        for (u32 r = 0; r < 4u; r++) pool[i * 4u + r] = trait_loci[i];
    code_shuffle(w, &s, trait_loci, GEN_TRAIT_LOCI);
    for (u32 i = 0; i < GEN_POOL - GEN_TRAIT_LOCI * 4u; i++) pool[GEN_TRAIT_LOCI * 4u + i] = trait_loci[i];
    code_shuffle(w, &s, pool, GEN_POOL);

    /* make the three loci of every trait distinct: swap a repeated entry with
     * an entry of another triple that fits both. A fit always exists (a locus
     * occurs at most 5 times), so the bounded scan cannot run dry. */
    for (u32 k = 0; k < AZ_TRAITS; k++) {
        for (u32 j = 1; j < 3u; j++) {
            u32 p = k * 3u + j;
            for (u32 step = 1; step < GEN_POOL && triple_has(pool, k, p, pool[p]); step++) {
                u32 q = (p + step) % GEN_POOL;
                u32 t = q / 3u;
                if (t == k) continue;
                if (triple_has(pool, k, p, pool[q]) || triple_has(pool, t, q, pool[p])) continue;
                u8 v = pool[p]; pool[p] = pool[q]; pool[q] = v;
            }
        }
    }

    for (u32 k = 0; k < AZ_TRAITS; k++) {
        for (u32 j = 0; j < 3u; j++) {
            u32 r = code_next(w, &s);
            i32 mag = 8 + (i32)(((u64)(r >> 1) * 17u) >> 31);      /* 8..24 */
            bool neg = (j > 0u) && (r & 1u);                       /* first weight positive */
            c->dev_idx[k][j] = pool[k * 3u + j];
            c->dev_w[k][j] = (i8)(neg ? -mag : mag);
        }
        c->dev_base[k] = 128;
    }

    /* dominance by quota: 32 additive, 13 high, 13 low, 6 overdominant */
    code_shuffle(w, &s, perm, AZ_CORE_FAMS);
    for (u32 i = 0; i < AZ_CORE_FAMS; i++)
        c->dom[perm[i]] = (u8)(i < AZ_CORE_FAMS / 2u ? 0u : (i < (AZ_CORE_FAMS * 45u) / 64u ? 1u :
                               (i < (AZ_CORE_FAMS * 58u) / 64u ? 2u : 3u)));

    /* 16 modulation loci, taken from the trait loci: all 20 distinct from
     * the 4 markers */
    code_shuffle(w, &s, trait_loci, GEN_TRAIT_LOCI);
    for (u32 i = 0; i < AZ_MODG * 2u; i++) c->mod_locus[i] = trait_loci[i];

    /* what the families outside the core do */
    c->latent_key = ((u64)code_next(w, &s) << 32) | code_next(w, &s);
}

static u8 express(u32 a, u32 b, u32 mode)
{
    u32 hi = a > b ? a : b, lo = a > b ? b : a;
    switch (mode & 3u) {
    case 0u:  return (u8)((a + b + 1u) >> 1);
    case 1u:  return (u8)hi;
    case 2u:  return (u8)lo;
    default:  return az_sat_u8((i32)(hi + ((hi - lo) >> 1)));
    }
}

/* ---- reading a genome --------------------------------------------------- */

/* What the two haplotypes hold of each core family: the sum over its copies
 * of (allele - 128) * expr, and the number of copies. */
typedef struct FamAcc {
    i32 sum[2][AZ_CORE_FAMS];
    u8  cnt[2][AZ_CORE_FAMS];      /* saturates at 255 */
} FamAcc;

#define GEN_SUM_MAX (1 << 30)

static void fam_scan(const AzGene *g, u32 n, i32 *sum, u8 *cnt)
{
    for (u32 f = 0; f < AZ_CORE_FAMS; f++) { sum[f] = 0; cnt[f] = 0; }
    for (u32 i = 0; i < n; i++) {
        const u32 f = g[i].fam;
        if (f >= AZ_CORE_FAMS) continue;
        const i64 t = (i64)sum[f] + ((i32)g[i].allele - 128) * (i32)g[i].expr;
        sum[f] = (i32)az_clamp_i64(t, -GEN_SUM_MAX, GEN_SUM_MAX);
        if (cnt[f] < 255u) cnt[f]++;
    }
}

static void fam_scan_slot(const AzWorld *w, u32 slot, FamAcc *a)
{
    for (u32 h = 0; h < 2u; h++) {
        u32 n;
        const AzGene *g = az_genes(w, slot, h, &n);
        fam_scan(g, n, a->sum[h], a->cnt[h]);
    }
}

/* the value a haplotype gives a family: one copy at full expression gives its
 * allele; more copies, or a stronger one, push further from the neutral 128 */
static u32 hap_value(i32 sum) { return az_sat_u8(128 + sum / 128); }

/* the expressed value of a core family; *have = some haplotype carries it */
static u8 fam_express(const AzGeneticCode *c, const FamAcc *a, u32 f, u8 *have)
{
    const bool pa = a->cnt[0][f] != 0, pb = a->cnt[1][f] != 0;
    if (have) *have = (pa || pb) ? 1u : 0u;
    if (pa && pb) return express(hap_value(a->sum[0][f]), hap_value(a->sum[1][f]), c->dom[f]);
    if (pa) return (u8)hap_value(a->sum[0][f]);
    if (pb) return (u8)hap_value(a->sum[1][f]);
    return 128;
}

/* A family outside the core acts on one trait, chosen with its weight by a
 * hash of the world's code key: every copy adds, in the units of the trait
 * sum, weight * (allele - 128) * expr / 256 (half a dose per haplotype). */
static void latent_scan(const AzGeneticCode *c, const AzGene *g, u32 n, i32 *lat)
{
    for (u32 i = 0; i < n; i++) {
        const u32 f = g[i].fam;
        if (f < AZ_CORE_FAMS) continue;
        const u64 h = az_mix64(c->latent_key ^ ((u64)f * 0x9E3779B97F4A7C15ull));
        const u32 k = (u32)(h % AZ_TRAITS);
        const i32 mag = 8 + (i32)((h >> 32) % 17u);
        const i32 x = mag * ((((i32)g[i].allele - 128) * (i32)g[i].expr) / 256);
        const i64 t = (i64)lat[k] + ((h >> 63) ? -x : x);
        lat[k] = (i32)az_clamp_i64(t, -GEN_SUM_MAX, GEN_SUM_MAX);
    }
}

void az_develop(AzWorld *w, u32 slot)
{
    const AzGeneticCode *c = &w->code;
    u8 *t = &w->trait[(size_t)slot * AZ_TRAITS];
    u8 *p = &w->phen[(size_t)slot * AZ_PHEN];
    FamAcc acc;
    u8 e[AZ_CORE_FAMS], have[AZ_CORE_FAMS];
    i32 lat[AZ_TRAITS];
    fam_scan_slot(w, slot, &acc);
    for (u32 f = 0; f < AZ_CORE_FAMS; f++) e[f] = fam_express(c, &acc, f, &have[f]);
    for (u32 k = 0; k < AZ_TRAITS; k++) lat[k] = 0;
    for (u32 h = 0; h < 2u; h++) {
        u32 n;
        const AzGene *g = az_genes(w, slot, h, &n);
        latent_scan(c, g, n, lat);
    }
    for (u32 k = 0; k < AZ_TRAITS; k++) {
        /* a trait exists only where its first family does: without the gene
         * there is no organ, whatever the other families say */
        if (!have[c->dev_idx[k][0]]) { t[k] = 0; continue; }
        i32 s = lat[k];
        for (u32 j = 0; j < 3u; j++) s += (i32)c->dev_w[k][j] * ((i32)e[c->dev_idx[k][j]] - 128);
        t[k] = az_sat_u8((i32)c->dev_base[k] + s / 16);
    }

    AzSenseMemory *sense_memory = az_sense_memory(w,slot);
    if (sense_memory) { sense_memory->taxis = t[AZ_T_TAXIS]; sense_memory->curve = t[AZ_T_CURVE]; sense_memory->innate_stake = t[AZ_T_STAKE]; }
    const u32 sl = c->marker_locus[0];
    const u32 va = acc.cnt[0][sl] ? hap_value(acc.sum[0][sl]) : 0u;
    const u32 vb = acc.cnt[1][sl] ? hap_value(acc.sum[1][sl]) : 0u;
    u8 sex = (u8)(__builtin_popcount(va ^ vb) & 1);
    w->sex[slot] = sex;
    p[AZ_P_METAB]  = t[AZ_T_METAB];
    p[AZ_P_THRUST] = t[AZ_T_THRUST];
    p[AZ_P_DRAG]   = t[AZ_T_DRAG];
    p[AZ_P_DENS]   = t[AZ_T_DENSITY];
    p[AZ_P_DIET_L] = t[AZ_T_DIET_LIGHT];
    p[AZ_P_DIET_X] = t[AZ_T_DIET_X];
    p[AZ_P_DIET_Y] = t[AZ_T_DIET_Y];
    p[AZ_P_DIET_W] = t[AZ_T_DIET_W];
    p[AZ_P_TOXRES] = t[AZ_T_TOXRES];
    p[AZ_P_TOPT]   = t[AZ_T_TOPT];
    p[AZ_P_TTOL]   = t[AZ_T_TTOL];
    p[AZ_P_SENSE]  = t[AZ_T_SENSE_RANGE];
    p[AZ_P_AGGR]   = t[AZ_T_AGGR];
    p[AZ_P_SOCIAL] = t[AZ_T_SOCIAL];
    p[AZ_P_MATE]   = (u8)((t[AZ_T_MATING_MODE] >> 5) | (sex << 3) | ((t[AZ_T_CHOOSY] >> 4) << 4));
    p[AZ_P_KIN]    = t[AZ_T_KIN_BIAS];
    w->tag[slot] = (u16)(((u16)t[AZ_T_HUE] << 8) | t[AZ_T_PATTERN]);
    u32 brain_bytes;
    AzBrain *br = az_brain(w, slot, &brain_bytes);
    for (u32 k = 0; k < AZ_MODG && brain_bytes >= sizeof(AzBrain); k++) {
        /* innate modulation: a neutral family (128) gives gain 64 = 1.0, bias 0 */
        br->gain[k] = (u8)(32u + (e[c->mod_locus[k]] >> 2));
        br->bias[k] = (i8)((i32)(e[c->mod_locus[AZ_MODG + k]] >> 3) - 16);
    }
}

/* ---- the body ----------------------------------------------------------- */

#define GEN_BODY_BIRTH 8u   /* most parts a body is born with: 1 + (255 >> 5) */

/* The body a creature is born with: 1 + (SEGMENT >> 5) parts, all empty (the
 * matter comes with eating). The first is the core; the kinds of the others
 * follow the creature's own traits in proportion (largest remainder, ties to
 * the lower kind). The BODY segment has room for GEN_BODY_BIRTH parts when
 * this is called. Returns the number of parts made. */
static u32 body_build(AzWorld *w, u32 slot)
{
    static const u8 affinity[AZ_PART_KINDS] = {
        [AZ_PART_GUT] = AZ_T_METAB, [AZ_PART_FIN] = AZ_T_THRUST, [AZ_PART_ARMOR] = AZ_T_ARMOR,
        [AZ_PART_WEAPON] = AZ_T_WEAPON, [AZ_PART_STORE] = AZ_T_STORE_CAP,
        [AZ_PART_SENSOR] = AZ_T_SENSE_ACUITY, [AZ_PART_GONAD] = AZ_T_CLUTCH
    };
    const u8 *t = &w->trait[(size_t)slot * AZ_TRAITS];
    u32 room;
    AzPart *p = az_parts(w, slot, &room);
    u32 n = 1u + ((u32)t[AZ_T_SEGMENT] >> 5);
    if (n > room) n = room;
    if (n == 0u) return 0;
    u32 a[AZ_PART_KINDS], placed[AZ_PART_KINDS], total = 0;
    for (u32 k = 1; k < AZ_PART_KINDS; k++) { a[k] = 1u + t[affinity[k]]; placed[k] = 0; total += a[k]; }
    p[0].matter = 0; p[0].kind = AZ_PART_CORE; p[0].health = 255; p[0].aux = 0;
    for (u32 i = 1; i < n; i++) {
        u32 best = 1;
        i64 best_v = -(1ll << 62);
        for (u32 k = 1; k < AZ_PART_KINDS; k++) {
            const i64 v = (i64)a[k] * (i64)(n - 1u) - (i64)placed[k] * (i64)total;
            if (v > best_v) { best_v = v; best = k; }
        }
        placed[best]++;
        p[i].matter = 0; p[i].kind = (u8)best; p[i].health = 255; p[i].aux = 0;
    }
    return n;
}

/* the genome, brain and mind of the slot are in place and trimmed: develop
 * it and give it its body */
static void finish_creature(AzWorld *w, u32 slot)
{
    az_develop(w, slot);
    const u32 n = body_build(w, slot);
    (void)az_blk_resize(w, slot, AZ_SEG_BODY, n * (u32)sizeof(AzPart));    /* a shrink never fails */
}

/* ---- founders ---------------------------------------------------------- */

/* A founder carries one gene of every core family in each haplotype, in
 * family order, at full expression. A species is a base allele per family
 * plus one alternative allele that is a single-base variant of it; an
 * individual carries the alternative at each haplotype gene with probability
 * 1/16 (standing variation that segregates). Base alleles are the mean of
 * four uniform bytes, so founder traits spread around the neutral value
 * instead of piling up at 0 and 255. The sex family is special: its
 * alternative differs in parity and sits on haplotype 1 of every second
 * founder, so founders are half sex 0 and half sex 1.
 * World parameter FOUNDER_GENE_LOSS (of 256): the chance that a species
 * lacks a family altogether (never a marker family). Such a species has no
 * organ for the traits that family comes first in. */
bool az_found(AzWorld *w, u32 slot, u32 species_seed)
{
    u32 len[AZ_SEG_COUNT] = { 0 };
    len[AZ_SEG_GEN_A] = len[AZ_SEG_GEN_B] = AZ_CORE_FAMS * (u32)sizeof(AzGene);
    len[AZ_SEG_BODY] = GEN_BODY_BIRTH * (u32)sizeof(AzPart);
    az_brain_found_size(w, species_seed, &len[AZ_SEG_BRAIN], &len[AZ_SEG_MIND]);
    if (!az_blk_new(w, slot, len)) return false;
    AzGene *ga = (AzGene *)az_seg(w, slot, AZ_SEG_GEN_A, NULL);
    AzGene *gb = (AzGene *)az_seg(w, slot, AZ_SEG_GEN_B, NULL);
    const u64 uid = w->uid[slot];
    const u32 sex_locus = w->code.marker_locus[0];
    const u32 loss = w->param[AZ_WP_FOUNDER_GENE_LOSS];
    u32 rb[4] = { 0, 0, 0, 0 }, ra[4] = { 0, 0, 0, 0 }, ri[4] = { 0, 0, 0, 0 }, rl[4] = { 0, 0, 0, 0 };
    u32 n = 0;
    for (u32 l = 0; l < AZ_CORE_FAMS; l++) {
        if ((l & 3u) == 0u) {
            rand4_t0(w, species_seed, AZ_RS_INIT, 1000u + (l >> 2), rb);   /* base alleles    */
            rand4_t0(w, species_seed, AZ_RS_INIT, 2000u + (l >> 2), ra);   /* alternatives    */
            az_rand4(w, uid, AZ_RS_INIT, 3000u + (l >> 2), ri);            /* who carries one */
            if (loss) rand4_t0(w, species_seed, AZ_RS_INIT, 4000u + (l >> 2), rl);   /* families the species lacks */
        }
        u32 x = rb[l & 3u], y = ra[l & 3u], z = ri[l & 3u];
        if (loss && (rl[l & 3u] & 255u) < loss) {
            bool marker = false;
            for (u32 i = 0; i < 4u; i++) marker = marker || (w->code.marker_locus[i] == l);
            if (!marker) continue;
        }
        u8 base = (u8)(((x & 255u) + ((x >> 8) & 255u) + ((x >> 16) & 255u) + (x >> 24) + 2u) >> 2);
        u32 d = 1u + (((y >> 8) * 3u) >> 24);          /* 1..3: the changed base's new offset */
        u32 pos = y & 3u;                              /* which of the 4 bases                */
        if (l == sex_locus) d = 1u + (d & 1u);         /* 1 or 2: flips the allele parity     */
        u8 alt = (u8)(base ^ (d << (2u * pos)));
        ga[n].fam = gb[n].fam = (u16)l;
        ga[n].expr = gb[n].expr = 128;
        if (l == sex_locus) {
            ga[n].allele = base;
            gb[n].allele = (z & 1u) ? alt : base;
        } else {
            ga[n].allele = ((z & 15u) == 0u) ? alt : base;
            gb[n].allele = (((z >> 4) & 15u) == 0u) ? alt : base;
        }
        n++;
    }
    az_brain_found(w, slot, species_seed);
    if (n < AZ_CORE_FAMS) {
        len[AZ_SEG_GEN_A] = len[AZ_SEG_GEN_B] = n * (u32)sizeof(AzGene);
        az_blk_fit(w, slot, len);
    }
    finish_creature(w, slot);
    return true;
}

/* ---- mutation ---------------------------------------------------------- */

static u8 rev_bases(u8 v)
{
    return (u8)(((v & 3u) << 6) | ((v & 12u) << 2) | ((v & 48u) >> 2) | ((v & 192u) >> 6));
}

/* expected point mutations per birth in Q14. The lesion is a zero rate on
 * the same code path. */
static u32 mutation_rate_q14(const AzWorld *w, u32 mother)
{
    if (w->cfg.lesion & AZ_LESION_MUTATION) return 0u;
    u32 tox = 0u;
    u32 cell = w->cell[mother];
    if (cell < w->n_cells && !(w->cfg.lesion & AZ_LESION_TOXIN))
        tox = az_min_u32(w->c_sp[(size_t)cell * AZ_NSPECIES + AZ_SP_TOX_Z], GEN_TOX_CAP);
    return (8u + w->trait[(size_t)mother * AZ_TRAITS + AZ_T_MUT_RATE]) * (64u + tox);
}

/* put cnt genes in at position `at` of a haplotype of *n genes with room for
 * cap; what does not fit is left out. Returns how many went in. */
static u32 genes_open(AzGene *g, u32 *n, u32 cap, u32 at, const AzGene *src, u32 cnt)
{
    if (*n >= cap) return 0;
    if (cnt > cap - *n) cnt = cap - *n;
    for (u32 i = *n; i > at; i--) g[i - 1u + cnt] = g[i - 1u];
    for (u32 i = 0; i < cnt; i++) g[at + i] = src[i];
    *n += cnt;
    return cnt;
}

static void genes_close(AzGene *g, u32 *n, u32 at, u32 cnt)
{
    for (u32 i = at; i + cnt < *n; i++) g[i] = g[i + cnt];
    *n -= cnt;
}

#define GEN_GROW 8u         /* most genes one birth adds to a haplotype (a copied block) */

/* The genome changes size: at most one of six events per birth. The chances
 * (out of 2^21) are lambda * world rate * (8 + the mother's own rate trait)
 * / 8192: with every rate neutral, a gene copy and a gene loss each happen
 * about as often as an inversion. */
static void mutate_size(AzWorld *w, u32 child, u32 mother, u32 lam, AzGene *const g[2], u32 n[2], const u32 cap[2])
{
    const u8 *mt = &w->trait[(size_t)mother * AZ_TRAITS];
    const u64 dup = ((u64)lam * w->param[AZ_WP_GENE_DUP_RATE] * (8u + mt[AZ_T_GENE_DUP])) >> 13;
    const u64 del = ((u64)lam * w->param[AZ_WP_GENE_DEL_RATE] * (8u + mt[AZ_T_GENE_DEL])) >> 13;
    const u64 neo = ((u64)lam * w->param[AZ_WP_GENE_NEW_RATE] * (8u + mt[AZ_T_GENE_NEW])) >> 13;
    if (!(dup | del | neo)) return;
    u32 r[4];
    az_rand4(w, w->uid[child], AZ_RS_MUTATE, 40u, r);
    const u64 u = r[0] & 0x1FFFFFu;
    const u32 hap = r[1] & 1u, m = n[hap];
    AzGene *gh = g[hap];
    const u32 at = m ? (r[1] >> 1) % m : 0u;
    const u32 blk = 2u + (((r[2] & 0xFFFFu) * 7u) >> 16);           /* 2..8 */
    u64 lim = dup;
    if (u < lim) {                                      /* a gene is copied next to itself */
        if (m) { const AzGene c = gh[at]; (void)genes_open(gh, &n[hap], cap[hap], at + 1u, &c, 1u); }
        return;
    }
    lim += del;
    if (u < lim) {                                      /* a gene is lost */
        if (m) genes_close(gh, &n[hap], at, 1u);
        return;
    }
    lim += dup >> 2;
    if (u < lim) {                                      /* a block of genes is copied behind itself */
        if (m) {
            AzGene c[GEN_GROW];
            const u32 len = az_min_u32(blk, m - at);
            for (u32 i = 0; i < len; i++) c[i] = gh[at + i];
            (void)genes_open(gh, &n[hap], cap[hap], at + len, c, len);
        }
        return;
    }
    lim += del >> 2;
    if (u < lim) {                                      /* a block of genes is lost */
        if (m) genes_close(gh, &n[hap], at, az_min_u32(blk, m - at));
        return;
    }
    lim += neo;
    if (u < lim) {                                      /* a new gene, of any family, weakly expressed */
        AzGene c;
        c.fam = (u16)(r[3] % w->param[AZ_WP_FAMILY_SPACE]);
        c.allele = (u8)(r[2] >> 16);
        c.expr = (u8)(32u + ((r[2] >> 24) & 63u));
        (void)genes_open(gh, &n[hap], cap[hap], m ? at + 1u : 0u, &c, 1u);
        return;
    }
    lim += neo;
    if (u < lim && m)                                   /* a gene takes up another family's work */
        gh[at].fam = (u16)(r[3] % w->param[AZ_WP_FAMILY_SPACE]);
}

/* The child's two haplotypes are being built in place: g[h] has n[h] genes
 * and room for cap[h]. */
static void mutate(AzWorld *w, u32 child, u32 mother, AzGene *const g[2], u32 n[2], const u32 cap[2])
{
    const u64 uid = w->uid[child];
    const u32 lam = mutation_rate_q14(w, mother);      /* <= 263 * 576 < 2^18 */
    u32 r[4];

    /* at most one structural event, each kind with probability lambda / 128 */
    az_rand4(w, uid, AZ_RS_MUTATE, 0u, r);
    if ((r[0] & 0x1FFFFFu) < lam) {                    /* conversion: a gene's sequence over a neighbour's */
        const u32 hap = r[1] & 1u, m = n[hap];
        if (m >= 2u) {
            const u32 src = (((r[1] >> 1) & 63u) | ((r[1] >> 8) << 6)) % m;
            u32 dst = ((r[1] >> 7) & 1u) ? src + 1u : src - 1u;      /* src - 1 wraps for src == 0 */
            if (dst >= m) dst = (src == 0u) ? 1u : m - 2u;
            g[hap][dst].allele = g[hap][src].allele;
        }
    } else if ((r[2] & 0x1FFFFFu) < lam) {             /* inversion of 2..8 genes */
        const u32 hap = r[3] & 1u, m = n[hap];
        u32 len = 2u + ((((r[3] >> 1) & 0xFFFFu) * 7u) >> 16);
        if (len > m) len = m;
        if (len >= 2u) {
            const u32 first = (u32)(((u64)(r[3] >> 17) * (u64)(m - len + 1u)) >> 15);
            AzGene *seg = &g[hap][first];
            for (u32 i = 0, j = len - 1u; i <= j; i++, j--) {
                AzGene a = seg[i], b = seg[j];
                a.allele = rev_bases(a.allele); b.allele = rev_bases(b.allele);
                seg[i] = b; seg[j] = a;
                if (i == j) break;
            }
        }
    }

    mutate_size(w, child, mother, lam, g, n, cap);

    /* expression: 4 trials; a gene's expression moves by 1..16 either way */
    {
        const u32 genes = n[0] + n[1];
        const u64 chance = ((u64)lam * w->param[AZ_WP_EXPR_MUT_RATE]) >> 6;     /* out of 2^18 */
        if (genes && chance) az_rand4(w, uid, AZ_RS_MUTATE, 41u, r);
        for (u32 t = 0; t < 4u && genes && chance; t++) {
            if ((r[t] & 0x3FFFFu) >= chance) continue;
            const u32 q = az_rand(w, uid, AZ_RS_MUTATE, 48u + t);
            const u32 gi = q % genes;
            AzGene *gene = gi < n[0] ? &g[0][gi] : &g[1][gi - n[0]];
            const i32 d = 1 + (i32)((q >> 24) & 15u);
            gene->expr = az_sat_u8((i32)gene->expr + ((q >> 31) ? -d : d));
        }
    }

    /* point mutations: 16 trials of probability lambda / 16, distinct bases */
    const u32 total = (n[0] + n[1]) * 4u;              /* bases of the two haplotypes */
    if (total == 0u) return;
    u32 picked[GEN_TRIALS], np = 0;
    for (u32 t = 0; t < GEN_TRIALS; t++) {
        if ((t & 3u) == 0u) az_rand4(w, uid, AZ_RS_MUTATE, 1u + (t >> 2), r);
        if ((r[t & 3u] & 0x3FFFFu) >= lam) continue;
        u32 q = az_rand(w, uid, AZ_RS_MUTATE, 16u + t);
        if (np >= total) continue;                     /* every base has changed already */
        u32 at = q % total;
        for (u32 i = 0; i < np; i++)
            if (picked[i] == at) { at = (at + 1u) % total; i = (u32)-1; }
        picked[np++] = at;
        u32 d = 1u + (((q >> 9) * 3u) >> 23);          /* 1..3: the base always changes */
        const u32 gi = at >> 2;
        AzGene *gene = gi < n[0] ? &g[0][gi] : &g[1][gi - n[0]];
        gene->allele ^= (u8)(d << (2u * (at & 3u)));
    }
}

/* ---- meiosis ----------------------------------------------------------- */

/* One gamete of `parent` (slot) into dst; returns its number of genes, never
 * more than the parent's two haplotypes hold together. The bivalent forms
 * 1 + (RECOMB >> 6) chiasmata; this chromatid takes part in each with
 * probability 1/2. An exchange falls on one of GEN_MAP - 1 points of the
 * chromosome's length, the same fraction of the way along both haplotypes;
 * where they differ in length the exchange is unequal, and the gamete's gene
 * count differs from both. `which` separates the two gametes of one child. */
static u32 gamete(const AzWorld *w, AzGene *dst, u32 parent, u64 child_uid, u32 which)
{
    const AzGene *src[2];
    u32 n[2];
    src[0] = az_genes(w, parent, 0, &n[0]);
    src[1] = az_genes(w, parent, 1, &n[1]);
    u32 r[4];
    az_rand4(w, child_uid, AZ_RS_MEIOSIS, which, r);
    const u32 nch = 1u + ((u32)w->trait[(size_t)parent * AZ_TRAITS + AZ_T_RECOMB] >> 6);
    u64 flip = 0;                                      /* bit g: an exchange just before map point g */
    for (u32 i = 0; i < nch; i++) {
        u32 x16 = (r[1 + (i >> 1)] >> (16u * (i & 1u))) & 0xFFFFu;
        u32 gap = 1u + ((x16 * (GEN_MAP - 1u)) >> 16);                   /* 1..63 */
        if ((r[0] >> (1u + i)) & 1u) flip ^= 1ull << gap;
    }
    u32 hap = r[0] & 1u, out = 0, from = 0;
    for (u32 g = 1; g <= GEN_MAP; g++) {
        if (g < GEN_MAP && !((flip >> g) & 1u)) continue;
        const u32 b = (u32)(((u64)from * n[hap]) / GEN_MAP), e = (u32)(((u64)g * n[hap]) / GEN_MAP);
        for (u32 i = b; i < e; i++) dst[out++] = src[hap][i];
        hap ^= 1u;
        from = g;
    }
    return out;
}

bool az_conceive(AzWorld *w, u32 child, u32 mother, u32 father)
{
    const bool bud = (father == AZ_NONE);
    u32 cap[2], n[2], len[AZ_SEG_COUNT] = { 0 };
    {
        u32 ma, mb, fa = 0, fb = 0;
        (void)az_genes(w, mother, 0, &ma);
        (void)az_genes(w, mother, 1, &mb);
        if (!bud) { (void)az_genes(w, father, 0, &fa); (void)az_genes(w, father, 1, &fb); }
        cap[0] = (bud ? ma : ma + mb) + GEN_GROW;      /* room for the longest gamete and */
        cap[1] = (bud ? mb : fa + fb) + GEN_GROW;      /* for what one birth can add      */
    }
    len[AZ_SEG_GEN_A] = cap[0] * (u32)sizeof(AzGene);
    len[AZ_SEG_GEN_B] = cap[1] * (u32)sizeof(AzGene);
    len[AZ_SEG_BODY] = GEN_BODY_BIRTH * (u32)sizeof(AzPart);
    az_brain_child_size(w, mother, &len[AZ_SEG_BRAIN], &len[AZ_SEG_MIND]);
    if (!az_blk_new(w, child, len)) return false;
    /* the parents' blocks may have moved: every pointer is fetched from here on */
    AzGene *const g[2] = { (AzGene *)az_seg(w, child, AZ_SEG_GEN_A, NULL),
                           (AzGene *)az_seg(w, child, AZ_SEG_GEN_B, NULL) };
    if (bud) {
        for (u32 h = 0; h < 2u; h++) {
            const AzGene *src = az_genes(w, mother, h, &n[h]);
            az_memcpy(g[h], src, (size_t)n[h] * sizeof(AzGene));
        }
    } else {
        n[0] = gamete(w, g[0], mother, w->uid[child], 0u);
        n[1] = gamete(w, g[1], father, w->uid[child], 1u);
    }
    mutate(w, child, mother, g, n, cap);
    len[AZ_SEG_GEN_A] = n[0] * (u32)sizeof(AzGene);
    len[AZ_SEG_GEN_B] = n[1] * (u32)sizeof(AzGene);
    /* the brain: the mother's, with neural mutation (az_brain.c) */
    az_brain_inherit(w, child, mother, &len[AZ_SEG_BRAIN], &len[AZ_SEG_MIND]);
    az_blk_fit(w, child, len);
    finish_creature(w, child);
    return true;
}

/* ---- founder niches ---------------------------------------------------- */

#define GEN_FIT_READS 8u   /* weighted traits considered per family (the code has at most 5) */

/* the first gene of family f in a haplotype, or n */
static u32 gene_find(const AzGene *g, u32 n, u32 f)
{
    if (f < n && g[f].fam == f) return f;              /* a founder keeps family order */
    for (u32 i = 0; i < n; i++)
        if (g[i].fam == f) return i;
    return n;
}

/* Coordinate descent. One step takes one core family (a fixed cycle over all
 * of them) and shifts the allele of its first gene in both haplotypes by the
 * same amount, choosing the shift that minimises sum weight * (trait -
 * target)^2 over the traits that read the family; ties keep the smallest
 * shift. Shifting both alleles keeps their difference, so standing variation
 * survives the fit, and the sex family only takes shifts that keep the sex.
 * A family the genome does not carry is left alone. No random numbers are
 * drawn (the uid is not used): members of one species, which start from
 * nearly the same genome, end at nearly the same solution. Stops early after
 * a full cycle without a change. Leaves the slot developed.
 * Returns sum weight[k] * |trait[k] - target[k]|. */
u32 az_genome_fit(AzWorld *w, u32 slot, const u8 *target, const u8 *weight, u32 iters)
{
    const AzGeneticCode *c = &w->code;
    AzGene *g[2];
    u32 gn[2];
    for (u32 h = 0; h < 2u; h++) {
        u32 bytes;
        g[h] = (AzGene *)az_seg(w, slot, AZ_SEG_GEN_A + h, &bytes);
        gn[h] = bytes / (u32)sizeof(AzGene);
    }
    FamAcc acc;
    u8 e[AZ_CORE_FAMS];
    fam_scan_slot(w, slot, &acc);
    for (u32 f = 0; f < AZ_CORE_FAMS; f++) e[f] = fam_express(c, &acc, f, NULL);
    u32 idle = 0;
    for (u32 it = 0, l = 0; it < iters && idle < AZ_CORE_FAMS; it++, l = (l + 37u) % AZ_CORE_FAMS) {
        idle++;
        /* the weighted traits that read family l, and their sums without it */
        i32 rest[GEN_FIT_READS], wl[GEN_FIT_READS];
        u32 tk[GEN_FIT_READS], n = 0;
        for (u32 k = 0; k < AZ_TRAITS && n < GEN_FIT_READS; k++) {
            if (!weight[k]) continue;
            for (u32 j = 0; j < 3u; j++) {
                if (c->dev_idx[k][j] != l) continue;
                i32 s = 0;
                for (u32 q = 0; q < 3u; q++)
                    if (q != j) s += (i32)c->dev_w[k][q] * ((i32)e[c->dev_idx[k][q]] - 128);
                rest[n] = s; wl[n] = c->dev_w[k][j]; tk[n] = k; n++;
                break;
            }
        }
        if (n == 0u) continue;
        const u32 ia = gene_find(g[0], gn[0], l), ib = gene_find(g[1], gn[1], l);
        const bool pa = ia < gn[0], pb = ib < gn[1];
        if (!pa && !pb) continue;
        const i32 a0 = pa ? g[0][ia].allele : 0, b0 = pb ? g[1][ib].allele : 0;
        const i32 xa = pa ? g[0][ia].expr : 0, xb = pb ? g[1][ib].expr : 0;
        /* what the other copies of the family give each haplotype */
        const i32 ra = acc.sum[0][l] - (a0 - 128) * xa, rb = acc.sum[1][l] - (b0 - 128) * xb;
        const u32 va0 = pa ? hap_value(acc.sum[0][l]) : 0u, vb0 = pb ? hap_value(acc.sum[1][l]) : 0u;
        const bool sex_locus = (l == c->marker_locus[0]);
        u64 best_err = ~0ull;
        i32 best_d = 0;
        for (i32 mag = 0; mag <= 255; mag++) {
            for (i32 sgn = 1; sgn >= -1; sgn -= 2) {
                i32 d = sgn * mag;
                const i32 a1 = az_sat_u8(a0 + d), b1 = az_sat_u8(b0 + d);
                const u32 va = pa ? hap_value(ra + (a1 - 128) * xa) : 0u;
                const u32 vb = pb ? hap_value(rb + (b1 - 128) * xb) : 0u;
                /* the sex family may move only where the parity relation of the two values stays */
                bool keeps_sex = ((__builtin_popcount(va ^ vb) ^ __builtin_popcount(va0 ^ vb0)) & 1) == 0;
                if (!sex_locus || keeps_sex) {
                    u8 ev = (pa && pb) ? express(va, vb, c->dom[l]) : (u8)(pa ? va : vb);
                    u64 err = 0;
                    for (u32 i = 0; i < n; i++) {
                        /* the unclamped value: a trait pinned at 0 or 255 must still feel the pull */
                        i32 raw = (i32)c->dev_base[tk[i]] + (rest[i] + wl[i] * ((i32)ev - 128)) / 16;
                        i64 diff = (i64)raw - (i64)target[tk[i]];
                        err += (u64)weight[tk[i]] * (u64)(diff * diff);
                    }
                    if (err < best_err) { best_err = err; best_d = d; }   /* mag ascends: ties keep the smallest shift */
                }
                if (mag == 0) break;
            }
        }
        if (best_d != 0) {
            if (pa) { g[0][ia].allele = az_sat_u8(a0 + best_d); acc.sum[0][l] = ra + ((i32)g[0][ia].allele - 128) * xa; }
            if (pb) { g[1][ib].allele = az_sat_u8(b0 + best_d); acc.sum[1][l] = rb + ((i32)g[1][ib].allele - 128) * xb; }
            e[l] = fam_express(c, &acc, l, NULL);
            idle = 0;
        }
    }
    az_develop(w, slot);
    const u8 *t = &w->trait[(size_t)slot * AZ_TRAITS];
    u64 total = 0;
    for (u32 k = 0; k < AZ_TRAITS; k++)
        total += (u64)weight[k] * (u64)az_abs_i32((i32)t[k] - (i32)target[k]);
    return total > 0xFFFFFFFFull ? 0xFFFFFFFFu : (u32)total;
}
