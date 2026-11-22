/* test_genome.c - heredity with genomes of any length (owner: genome).
 *   1. genetic code: structure for many seeds, a pure function of the seed
 *   2. development: an independent restatement of the published rule, checked
 *      on random genomes of any length (copies, missing families, families
 *      outside the core); "no gene, no organ"; dosage; phenotype, tag, sex,
 *      the brain's innate gain and bias
 *   3. founders: one gene per core family, standing variation, sex ratio, the
 *      body a creature is born with, FOUNDER_GENE_LOSS
 *   4. meiosis: Mendelian transmission, linkage, AZ_T_RECOMB, unequal exchange
 *   5. mutation: point mutations (exact count per child, mean, AZ_T_MUT_RATE,
 *      radiotoxin, lesions), and the events that change the size of a genome
 *      (gene and block copies and losses, new genes, family change,
 *      expression drift) with their world rates and heritable rate traits
 *   6. az_genome_fit; independence of call order; a world that runs
 * Tolerances are at least five standard errors of the quantity they bound.
 * The default run uses seeds from 1..99 only (tuning seeds); `test_genome
 * <seed>` runs another.
 */
#include "az_test.h"
#include <math.h>

static u8 *T(AzWorld *w, u32 s) { return &w->trait[(size_t)s * AZ_TRAITS]; }

static AzGene *genes(AzWorld *w, u32 s, u32 hap, u32 *n)
{
    u32 bytes;
    AzGene *g = (AzGene *)az_seg(w, s, AZ_SEG_GEN_A + hap, &bytes);
    *n = bytes / (u32)sizeof(AzGene);
    return g;
}

static u32 founder(AzWorld *w, u32 species_seed)
{
    u32 s = az_spawn(w);
    if (s == AZ_NONE) return AZ_NONE;
    if (!az_found(w, s, species_seed)) return AZ_NONE;
    return s;
}

/* the child slot is used again and again: a new uid, a new conception */
static bool reborn(AzWorld *w, u32 ch, u64 uid, u32 mother, u32 father)
{
    if (w->blk[ch]) az_blk_release(w, ch);
    w->uid[ch] = uid;
    return az_conceive(w, ch, mother, father);
}

static bool near(double got, double want, double tol) { return fabs(got - want) <= tol; }

/* the module's published expression rule, restated independently */
static u32 expr_ref(u32 a, u32 b, u32 mode)
{
    u32 hi = a > b ? a : b, lo = a > b ? b : a;
    if (mode == 0) return (a + b + 1) / 2;
    if (mode == 1) return hi;
    if (mode == 2) return lo;
    u32 v = hi + (hi - lo) / 2;
    return v > 255 ? 255 : v;
}
static int clamp255(long long v) { return v < 0 ? 0 : (v > 255 ? 255 : (int)v); }

/* ---- 1. genetic code ---- */
static void test_code_structure(AzWorld *w, u32 seed_lo, u32 n_seeds)
{
    u32 key0 = w->key[0];
    for (u32 sd = seed_lo; sd < seed_lo + n_seeds; sd++) {
        w->key[0] = sd; az_tick_keys(w);
        memset(&w->code, 0xEE, sizeof w->code);
        az_genetic_code_init(w);
        const AzGeneticCode *c = &w->code;
        u32 reads[AZ_CORE_FAMS] = { 0 };
        int distinct = 1, wok = 1, first_pos = 1, base_ok = 1, dom_ok = 1;
        for (u32 k = 0; k < AZ_TRAITS; k++) {
            const u8 *ix = c->dev_idx[k];
            if (ix[0] == ix[1] || ix[0] == ix[2] || ix[1] == ix[2]) distinct = 0;
            for (u32 j = 0; j < 3; j++) {
                if (ix[j] >= AZ_CORE_FAMS) { distinct = 0; continue; }
                reads[ix[j]]++;
                int m = abs((int)c->dev_w[k][j]);
                if (m < 8 || m > 24) wok = 0;
            }
            if (c->dev_w[k][0] <= 0) first_pos = 0;
            if (c->dev_base[k] != 128) base_ok = 0;
        }
        /* the marker families (sex and kinship) feed no trait; every other
         * family is read 4 or 5 times */
        u8 is_marker[AZ_CORE_FAMS] = { 0 };
        for (u32 i = 0; i < 4; i++) if (c->marker_locus[i] < AZ_CORE_FAMS) is_marker[c->marker_locus[i]] = 1;
        u32 min_reads = 99, max_reads = 0, marker_reads = 0, dom_n[4] = { 0, 0, 0, 0 };
        for (u32 l = 0; l < AZ_CORE_FAMS; l++) {
            if (is_marker[l]) marker_reads += reads[l];
            else {
                if (reads[l] < min_reads) min_reads = reads[l];
                if (reads[l] > max_reads) max_reads = reads[l];
            }
            if (c->dom[l] > 3) dom_ok = 0; else dom_n[c->dom[l]]++;
        }
        CHECK(distinct); CHECK(wok); CHECK(first_pos); CHECK(base_ok); CHECK(dom_ok);
        CHECK_EQ(marker_reads, 0);
        CHECK(min_reads >= 4 && max_reads <= 5);
        /* dominance by quota: half additive, then high, low, overdominant */
        CHECK_EQ(dom_n[0], AZ_CORE_FAMS / 2);
        CHECK_EQ(dom_n[1], (AZ_CORE_FAMS * 45) / 64 - AZ_CORE_FAMS / 2);
        CHECK_EQ(dom_n[2], (AZ_CORE_FAMS * 58) / 64 - (AZ_CORE_FAMS * 45) / 64);
        CHECK(dom_n[3] >= 1);
        /* 16 modulation families and 4 markers: in range and distinct */
        u32 seen[AZ_CORE_FAMS] = { 0 };
        int mm_ok = 1;
        for (u32 i = 0; i < AZ_MODG * 2; i++) { if (c->mod_locus[i] >= AZ_CORE_FAMS || seen[c->mod_locus[i]]++) mm_ok = 0; }
        for (u32 i = 0; i < 4; i++) { if (c->marker_locus[i] >= AZ_CORE_FAMS || seen[c->marker_locus[i]]++) mm_ok = 0; }
        CHECK(mm_ok);
    }
    printf("code: %u seeds from %u\n", n_seeds, seed_lo);

    /* a pure function of the seed: same seed same code, also at a later tick;
     * another seed gives another code and another latent map */
    AzGeneticCode a, b, d;
    w->key[0] = 777; az_tick_keys(w); az_genetic_code_init(w); a = w->code;
    w->tick = 12345; az_tick_keys(w); az_genetic_code_init(w); b = w->code; w->tick = 0;
    w->key[0] = 778; az_tick_keys(w); az_genetic_code_init(w); d = w->code;
    CHECK(memcmp(&a, &b, sizeof a) == 0);
    CHECK(memcmp(&a, &d, sizeof a) != 0);
    CHECK(a.latent_key != d.latent_key);
    w->key[0] = key0; az_tick_keys(w);
    az_genetic_code_init(w);
}

/* ---- 2. development ---- */
typedef struct RefOut { u8 trait[AZ_TRAITS]; u8 e[AZ_CORE_FAMS]; u8 have[AZ_CORE_FAMS]; u8 sex; } RefOut;

/* the contract of az_genome.c, written out again from its text */
static void ref_develop(AzWorld *w, u32 s, RefOut *o)
{
    const AzGeneticCode *c = &w->code;
    static long long sum[2][AZ_CORE_FAMS], lat[AZ_TRAITS];
    static int cnt[2][AZ_CORE_FAMS];
    memset(sum, 0, sizeof sum); memset(lat, 0, sizeof lat); memset(cnt, 0, sizeof cnt);
    for (u32 h = 0; h < 2; h++) {
        u32 n;
        const AzGene *g = genes(w, s, h, &n);
        for (u32 i = 0; i < n; i++) {
            const int d = (int)g[i].allele - 128;
            if (g[i].fam < AZ_CORE_FAMS) { sum[h][g[i].fam] += d * (int)g[i].expr; cnt[h][g[i].fam]++; continue; }
            const u64 hh = az_mix64(c->latent_key ^ ((u64)g[i].fam * 0x9E3779B97F4A7C15ull));
            const u32 k = (u32)(hh % AZ_TRAITS);
            const int mag = 8 + (int)((hh >> 32) % 17u);
            const int x = mag * ((d * (int)g[i].expr) / 256);
            lat[k] += (hh >> 63) ? -x : x;
        }
    }
    int v[2][AZ_CORE_FAMS];
    for (u32 f = 0; f < AZ_CORE_FAMS; f++) {
        for (u32 h = 0; h < 2; h++) v[h][f] = clamp255(128 + sum[h][f] / 128);
        const bool pa = cnt[0][f] > 0, pb = cnt[1][f] > 0;
        o->have[f] = (u8)(pa || pb);
        o->e[f] = (u8)(pa && pb ? expr_ref((u32)v[0][f], (u32)v[1][f], c->dom[f])
                               : pa ? (u32)v[0][f] : pb ? (u32)v[1][f] : 128u);
    }
    for (u32 k = 0; k < AZ_TRAITS; k++) {
        if (!o->have[c->dev_idx[k][0]]) { o->trait[k] = 0; continue; }
        long long t = lat[k];
        for (u32 j = 0; j < 3; j++) t += (long long)c->dev_w[k][j] * ((int)o->e[c->dev_idx[k][j]] - 128);
        o->trait[k] = (u8)clamp255((long long)c->dev_base[k] + t / 16);
    }
    const u32 sl = c->marker_locus[0];
    const u32 va = cnt[0][sl] ? (u32)v[0][sl] : 0u, vb = cnt[1][sl] ? (u32)v[1][sl] : 0u;
    o->sex = (u8)(__builtin_popcount(va ^ vb) & 1);
}

/* give haplotype `hap` of the slot n random genes: copies, gaps and families
 * outside the core included */
static void random_hap(AzWorld *w, u32 s, u32 hap, u32 n, u64 *rs)
{
    CHECK(az_blk_resize(w, s, AZ_SEG_GEN_A + hap, n * (u32)sizeof(AzGene)));
    u32 got;
    AzGene *g = genes(w, s, hap, &got);
    CHECK_EQ(got, n);
    for (u32 i = 0; i < n && i < got; i++) {
        const u64 r = az_test_rand64(rs);
        const u32 kind = (u32)(r & 15u);
        g[i].fam = (u16)(kind < 12 ? (r >> 8) % AZ_CORE_FAMS : (kind < 15 ? AZ_CORE_FAMS + (r >> 8) % 4000u : (r >> 8) % 12u));
        g[i].allele = (u8)(r >> 32);
        g[i].expr = (u8)(kind == 7 ? 0 : (kind == 8 ? 255 : (r >> 40)));
    }
}

static void check_developed(AzWorld *w, u32 s, const char *what)
{
    RefOut o;
    ref_develop(w, s, &o);
    const u8 *t = T(w, s);
    u32 bad = 0;
    for (u32 k = 0; k < AZ_TRAITS; k++) if (t[k] != o.trait[k]) bad++;
    if (bad) printf("  %s: %u traits differ from the restated rule\n", what, bad);
    CHECK_EQ(bad, 0);
    CHECK_EQ(w->sex[s], o.sex);
    const u8 *p = &w->phen[(size_t)s * AZ_PHEN];
    CHECK_EQ(p[AZ_P_METAB], t[AZ_T_METAB]);
    CHECK_EQ(p[AZ_P_THRUST], t[AZ_T_THRUST]);
    CHECK_EQ(p[AZ_P_DRAG], t[AZ_T_DRAG]);
    CHECK_EQ(p[AZ_P_DENS], t[AZ_T_DENSITY]);
    CHECK_EQ(p[AZ_P_TOXRES], t[AZ_T_TOXRES]);
    CHECK_EQ(p[AZ_P_SENSE], t[AZ_T_SENSE_RANGE]);
    CHECK_EQ(p[AZ_P_AGGR], t[AZ_T_AGGR]);
    CHECK_EQ(p[AZ_P_KIN], t[AZ_T_KIN_BIAS]);
    CHECK_EQ(p[AZ_P_MATE], (u8)((t[AZ_T_MATING_MODE] >> 5) | (o.sex << 3) | ((t[AZ_T_CHOOSY] >> 4) << 4)));
    CHECK_EQ(w->tag[s], (u16)(((u16)t[AZ_T_HUE] << 8) | t[AZ_T_PATTERN]));
    u32 bb;
    const AzBrain *br = az_brain(w, s, &bb);
    CHECK(bb >= sizeof(AzBrain));
    if (bb >= sizeof(AzBrain)) {
        u32 mod_bad = 0;
        for (u32 k = 0; k < AZ_MODG; k++) {
            if (br->gain[k] != (u8)(32u + (o.e[w->code.mod_locus[k]] >> 2))) mod_bad++;
            if (br->bias[k] != (i8)((int)(o.e[w->code.mod_locus[AZ_MODG + k]] >> 3) - 16)) mod_bad++;
        }
        CHECK_EQ(mod_bad, 0);
    }
}

static void test_develop(AzWorld *w)
{
    u64 rs = 0xD0E5ull;
    const u32 s = founder(w, 0x20001u);
    CHECK(s != AZ_NONE);
    if (s == AZ_NONE) return;
    check_developed(w, s, "founder");

    /* random genomes of every length, 0 genes included */
    for (u32 round = 0; round < 600; round++) {
        const u32 na = (u32)(az_test_rand64(&rs) % (round < 40 ? 4u : 220u));
        const u32 nb = (u32)(az_test_rand64(&rs) % (round < 40 ? 4u : 220u));
        random_hap(w, s, 0, na, &rs);
        random_hap(w, s, 1, nb, &rs);
        az_develop(w, s);
        check_developed(w, s, "random genome");
        u32 units = az_blk(w, s)->units;
        CHECK(az_blk_ok(az_blk(w, s), units));
    }

    /* dosage, by hand: family f in one haplotype only */
    const AzGeneticCode *c = &w->code;
    u32 k = 0;
    while (k < AZ_TRAITS && c->dev_w[k][0] <= 0) k++;
    const u32 f = c->dev_idx[k][0];
    random_hap(w, s, 1, 0, &rs);
    CHECK(az_blk_resize(w, s, AZ_SEG_GEN_A, 3 * (u32)sizeof(AzGene)));
    u32 n;
    AzGene *g = genes(w, s, 0, &n);
    CHECK_EQ(n, 3);
    RefOut o;
    /* one copy at full expression gives its allele */
    g[0] = (AzGene){ (u16)f, 160, 128 }; g[1] = (AzGene){ 4001, 128, 0 }; g[2] = (AzGene){ 4002, 128, 0 };
    az_develop(w, s); ref_develop(w, s, &o);
    CHECK_EQ(o.e[f], 160);
    const u8 one = T(w, s)[k];
    /* a second copy pushes further from the neutral 128; half expression pulls back */
    g = genes(w, s, 0, &n);
    g[1] = (AzGene){ (u16)f, 160, 128 };
    az_develop(w, s); ref_develop(w, s, &o);
    CHECK_EQ(o.e[f], 192);
    CHECK(T(w, s)[k] > one);
    g = genes(w, s, 0, &n);
    g[1] = (AzGene){ 4001, 128, 0 }; g[0].expr = 64;
    az_develop(w, s); ref_develop(w, s, &o);
    CHECK_EQ(o.e[f], 144);
    CHECK(T(w, s)[k] < one);
    check_developed(w, s, "dosage");
    /* a silent gene (expression 0) still counts as present: the organ exists, at the neutral value */
    g = genes(w, s, 0, &n);
    g[0].expr = 0;
    az_develop(w, s); ref_develop(w, s, &o);
    CHECK_EQ(o.e[f], 128);
    CHECK(o.have[f]);
    /* no gene, no organ: without the family the trait is 0 whatever the rest says */
    g = genes(w, s, 0, &n);
    g[0].fam = 4003;
    az_develop(w, s);
    CHECK_EQ(T(w, s)[k], 0);
    check_developed(w, s, "missing family");
    /* an empty genome: every trait 0, sex 0 */
    random_hap(w, s, 0, 0, &rs);
    az_develop(w, s);
    u32 nz = 0;
    for (u32 i = 0; i < AZ_TRAITS; i++) nz += T(w, s)[i] != 0;
    CHECK_EQ(nz, 0);
    CHECK_EQ(w->sex[s], 0);
    printf("development: 600 random genomes equal the restated rule; dosage, silent gene, missing family, empty genome\n");
}

/* ---- 3. founders ---- */
static u32 base_dist1(u8 a, u8 b)
{
    u32 x = (u32)(a ^ b), d = 0;
    for (u32 i = 0; i < 4; i++) d += ((x >> (2 * i)) & 3u) != 0;
    return d;
}

static void test_founders(u64 seed)
{
    AzConfig cfg = az_test_config(seed, 64);
    AzTestWorld t;
    if (!az_test_world(&t, &cfg, 0)) { CHECK(!"world creation"); return; }
    AzWorld *w = t.w;
    enum { NF = 400 };
    static u32 fs[NF];
    u32 sex1 = 0, shape_bad = 0, body_bad = 0, blk_bad = 0, parts_seen = 0;
    for (u32 i = 0; i < NF; i++) {
        fs[i] = founder(w, 0x30001u);
        CHECK(fs[i] != AZ_NONE);
        if (fs[i] == AZ_NONE) { az_test_world_free(&t); return; }
    }
    static u32 hist[AZ_CORE_FAMS][256];
    memset(hist, 0, sizeof hist);
    for (u32 i = 0; i < NF; i++) {
        const u32 s = fs[i];
        for (u32 h = 0; h < 2; h++) {
            u32 n;
            const AzGene *g = genes(w, s, h, &n);
            if (n != AZ_CORE_FAMS) { shape_bad++; continue; }
            for (u32 l = 0; l < n; l++) {
                if (g[l].fam != l || g[l].expr != 128) shape_bad++;
                hist[l][g[l].allele]++;
            }
        }
        sex1 += w->sex[s];
        u32 bb, mb, np;
        const AzBrain *br = az_brain(w, s, &bb);
        (void)az_seg(w, s, AZ_SEG_MIND, &mb);
        if (!az_blk_ok(az_blk(w, s), az_blk(w, s)->units) || !az_brain_ok(br, bb, mb)) blk_bad++;
        const AzPart *p = az_parts(w, s, &np);
        if (np != 1u + (T(w, s)[AZ_T_SEGMENT] >> 5) || np == 0 || p[0].kind != AZ_PART_CORE) body_bad++;
        for (u32 j = 0; j < np; j++)
            if (p[j].matter != 0 || p[j].kind >= AZ_PART_KINDS || (j > 0 && p[j].kind == AZ_PART_CORE)) body_bad++;
        parts_seen |= 1u << (np > 31 ? 31 : np);
        RefOut o;
        ref_develop(w, s, &o);
        if (memcmp(o.trait, T(w, s), AZ_TRAITS) != 0) shape_bad++;     /* a founder comes developed */
    }
    CHECK_EQ(shape_bad, 0);
    CHECK_EQ(blk_bad, 0);
    CHECK_EQ(body_bad, 0);
    /* half the founders are of each sex: 200 +- 5 sigma (sigma = 10) */
    CHECK(sex1 >= 150 && sex1 <= 250);
    /* standing variation: per family one base allele and one alternative, a
     * single-base variant, carried by about 1/16 of the haplotype genes */
    u32 alt_genes = 0, alt_bad = 0, three = 0;
    for (u32 l = 0; l < AZ_CORE_FAMS; l++) {
        u32 best = 0, second = 256, kinds = 0;
        for (u32 a = 0; a < 256; a++) if (hist[l][a] > hist[l][best]) best = a;
        for (u32 a = 0; a < 256; a++) {
            if (!hist[l][a]) continue;
            kinds++;
            if (a != best) second = a;
        }
        if (kinds > 2) three++;
        if (kinds == 2) {
            if (base_dist1((u8)best, (u8)second) != 1) alt_bad++;
            if (l != w->code.marker_locus[0]) alt_genes += hist[l][second];
        }
    }
    CHECK_EQ(three, 0);
    CHECK_EQ(alt_bad, 0);
    /* 79 families x 800 haplotype genes / 16 = 3950; sigma = 61 */
    CHECK(alt_genes >= 3950 - 320 && alt_genes <= 3950 + 320);
    /* another species has other alleles */
    const u32 other = founder(w, 0x30002u);
    CHECK(other != AZ_NONE);
    if (other != AZ_NONE) {
        u32 n, na, same = 0;
        const AzGene *g = genes(w, other, 0, &n), *ga = genes(w, fs[0], 0, &na);
        for (u32 l = 0; l < n && l < na; l++) same += g[l].allele == ga[l].allele;
        CHECK(same < AZ_CORE_FAMS / 4);
    }
    az_test_world_free(&t);

    /* FOUNDER_GENE_LOSS: a species can lack families altogether (never a
     * marker); the traits those families come first in are then 0 */
    AzTestWorld u;
    if (!az_test_world_ex(&u, &cfg, 0, AZ_TEST_NO_WORLDGEN)) { CHECK(!"empty world"); return; }
    w = u.w;
    CHECK(az_param_set(w, AZ_WP_FOUNDER_GENE_LOSS, 64));
    az_genetic_code_init(w);
    u32 lost_total = 0, organ_bad = 0, same_bad = 0, marker_lost = 0, first_n = 0;
    static u16 first_fams[AZ_CORE_FAMS];
    for (u32 i = 0; i < 40; i++) {
        const u32 s = founder(w, 0x30007u);
        CHECK(s != AZ_NONE);
        if (s == AZ_NONE) break;
        u32 na, nb;
        const AzGene *ga = genes(w, s, 0, &na), *gb = genes(w, s, 1, &nb);
        u8 present[AZ_CORE_FAMS] = { 0 };
        if (na != nb) same_bad++;
        for (u32 l = 0; l < na && l < nb; l++) {
            if (ga[l].fam != gb[l].fam || ga[l].fam >= AZ_CORE_FAMS) { same_bad++; continue; }
            present[ga[l].fam] = 1;
        }
        if (i == 0) { first_n = na; for (u32 l = 0; l < na; l++) first_fams[l] = ga[l].fam; }
        else if (na != first_n) same_bad++;                              /* the loss is the species', not the individual's */
        else for (u32 l = 0; l < na; l++) if (ga[l].fam != first_fams[l]) same_bad++;
        for (u32 m = 0; m < 4; m++) if (!present[w->code.marker_locus[m]]) marker_lost++;
        for (u32 k = 0; k < AZ_TRAITS; k++)
            if (!present[w->code.dev_idx[k][0]] && T(w, s)[k] != 0) organ_bad++;
        if (i == 0) lost_total = AZ_CORE_FAMS - na;
        RefOut o;
        ref_develop(w, s, &o);
        if (memcmp(o.trait, T(w, s), AZ_TRAITS) != 0) organ_bad++;
    }
    CHECK_EQ(same_bad, 0);
    CHECK_EQ(marker_lost, 0);
    CHECK_EQ(organ_bad, 0);
    /* 76 families at risk x 1/4: 19 expected, sigma 3.8 */
    CHECK(lost_total >= 2 && lost_total <= 40);
    printf("founders seed %" PRIu64 ": sex1 %u of %u, %u alternative genes (3950 expected), body sizes seen mask 0x%x; "
           "with FOUNDER_GENE_LOSS 64 the species lacks %u families\n", seed, sex1, (u32)NF, alt_genes, parts_seen, lost_total);
    az_test_world_free(&u);
}

/* ---- 4. meiosis ---- */
/* tag a parent: the allele of a gene is its place, the expression says which
 * haplotype it lies on (meiosis moves genes, it does not read them) */
static void tag_parent(AzWorld *w, u32 s, u8 tag_a, u8 tag_b)
{
    for (u32 h = 0; h < 2; h++) {
        u32 n;
        AzGene *g = genes(w, s, h, &n);
        for (u32 i = 0; i < n; i++) { g[i].allele = (u8)i; g[i].expr = h ? tag_b : tag_a; }
    }
}

typedef struct Link { u32 n, from_a, r01, r040, r079, bad; } Link;

static void cross_many(AzWorld *w, u32 ch, u32 m, u32 f, u64 uid0, u32 n, Link *mo, Link *fa)
{
    memset(mo, 0, sizeof *mo); memset(fa, 0, sizeof *fa);
    for (u32 i = 0; i < n; i++) {
        CHECK(reborn(w, ch, uid0 + i, m, f));
        for (u32 h = 0; h < 2; h++) {
            Link *l = h ? fa : mo;
            const u8 ta = h ? 3 : 1, tb = h ? 4 : 2;
            u32 cn;
            const AzGene *g = genes(w, ch, h, &cn);
            l->n++;
            if (cn != AZ_CORE_FAMS) { l->bad++; continue; }
            for (u32 j = 0; j < cn; j++)
                if (g[j].allele != j || (g[j].expr != ta && g[j].expr != tb) || g[j].fam != j) l->bad++;
            l->from_a += g[0].expr == ta;
            l->r01 += g[0].expr != g[1].expr;
            l->r040 += g[0].expr != g[40].expr;
            l->r079 += g[0].expr != g[79].expr;
        }
    }
}

static void test_meiosis(u64 seed)
{
    AzConfig cfg = az_test_config(seed, 64);
    cfg.lesion = AZ_LESION_MUTATION;            /* children are pure recombinants */
    AzTestWorld t;
    if (!az_test_world(&t, &cfg, 0)) { CHECK(!"world creation"); return; }
    AzWorld *w = t.w;
    const u32 m = founder(w, 0x40001u), f = founder(w, 0x40001u), ch = az_spawn(w);
    CHECK(m != AZ_NONE && f != AZ_NONE && ch != AZ_NONE);
    if (m == AZ_NONE || f == AZ_NONE || ch == AZ_NONE) { az_test_world_free(&t); return; }

    /* the sex ratio of a sex-1 x sex-0 cross is 1:1 (untouched founders) */
    {
        u32 a = AZ_NONE, b = AZ_NONE;
        for (u32 i = 0; i < 64 && (a == AZ_NONE || b == AZ_NONE); i++) {
            const u32 s = founder(w, 0x40001u);
            if (s == AZ_NONE) break;
            if (w->sex[s] == 1 && a == AZ_NONE) a = s;
            if (w->sex[s] == 0 && b == AZ_NONE) b = s;
        }
        CHECK(a != AZ_NONE && b != AZ_NONE);
        if (a != AZ_NONE && b != AZ_NONE) {
            u32 s1 = 0, s1r = 0;
            const u32 n = 4000;
            for (u32 i = 0; i < n; i++) { CHECK(reborn(w, ch, 500000u + i, a, b)); s1 += w->sex[ch]; }
            for (u32 i = 0; i < n; i++) { CHECK(reborn(w, ch, 600000u + i, b, a)); s1r += w->sex[ch]; }
            CHECK(near((double)s1 / n, 0.5, 0.04));          /* sigma 0.0079 */
            CHECK(near((double)s1r / n, 0.5, 0.04));
            printf("meiosis seed %" PRIu64 ": sex-1 x sex-0 gives %.3f sex-1 children, the reciprocal cross %.3f\n",
                   seed, (double)s1 / n, (double)s1r / n);
        }
    }

    tag_parent(w, m, 1, 2);
    tag_parent(w, f, 3, 4);
    const u32 n = 6000;
    Link mo, fa;
    /* one chiasma (RECOMB 0): the ends recombine in half the gametes, the
     * middle in 32/63 of that, neighbours in 1/63 of that */
    T(w, m)[AZ_T_RECOMB] = 0; T(w, f)[AZ_T_RECOMB] = 0;
    cross_many(w, ch, m, f, 1000000u, n, &mo, &fa);
    CHECK_EQ(mo.bad, 0); CHECK_EQ(fa.bad, 0);
    for (u32 h = 0; h < 2; h++) {
        const Link *l = h ? &fa : &mo;
        CHECK(near((double)l->from_a / n, 0.5, 0.035));                /* Mendel's first law; sigma 0.0065 */
        CHECK(near((double)l->r079 / n, 0.5, 0.035));
        CHECK(near((double)l->r040 / n, 0.5 * 32.0 / 63.0, 0.03));     /* sigma 0.0056 */
        CHECK(near((double)l->r01 / n, 0.5 / 63.0, 0.006));            /* sigma 0.0011 */
    }
    printf("meiosis, 1 chiasma: r(0,1) %.4f r(0,40) %.4f r(0,79) %.4f (expected %.4f %.4f 0.5)\n",
           (double)mo.r01 / n, (double)mo.r040 / n, (double)mo.r079 / n, 0.5 / 63.0, 16.0 / 63.0);
    /* four chiasmata (RECOMB 255): more exchange, never more than one half */
    T(w, m)[AZ_T_RECOMB] = 255; T(w, f)[AZ_T_RECOMB] = 255;
    Link mo4, fa4;
    cross_many(w, ch, m, f, 2000000u, n, &mo4, &fa4);
    CHECK_EQ(mo4.bad, 0); CHECK_EQ(fa4.bad, 0);
    {
        const double q = 16.0 / 63.0, odd = (1.0 - pow(1.0 - 2.0 * q, 4.0)) / 2.0;
        const double q1 = 0.5 / 63.0, odd1 = (1.0 - pow(1.0 - 2.0 * q1, 4.0)) / 2.0;
        CHECK(near((double)mo4.r040 / n, odd, 0.035));
        CHECK(near((double)mo4.r079 / n, 0.5, 0.035));
        CHECK(near((double)mo4.r01 / n, odd1, 0.012));
        CHECK(mo4.r01 > mo.r01 && mo4.r040 > mo.r040);
        printf("meiosis, 4 chiasmata: r(0,1) %.4f r(0,40) %.4f r(0,79) %.4f (expected %.4f %.4f 0.5)\n",
               (double)mo4.r01 / n, (double)mo4.r040 / n, (double)mo4.r079 / n, odd1, odd);
    }

    /* budding copies both haplotypes */
    {
        CHECK(reborn(w, ch, 3000000u, m, AZ_NONE));
        u32 same = 0;
        for (u32 h = 0; h < 2; h++) {
            u32 cn, pn;
            const AzGene *g = genes(w, ch, h, &cn), *pg = genes(w, m, h, &pn);
            same += cn == pn && memcmp(g, pg, (size_t)cn * sizeof(AzGene)) == 0;
        }
        CHECK_EQ(same, 2);
    }

    /* unequal exchange: the father's second haplotype has 40 genes. A gamete
     * with an exchange then has a length of its own, between 40 and 80, and
     * is made of whole runs of the father's genes in their order. */
    {
        CHECK(az_blk_resize(w, f, AZ_SEG_GEN_B, 40 * (u32)sizeof(AzGene)));
        tag_parent(w, f, 3, 4);
        T(w, f)[AZ_T_RECOMB] = 0;
        u32 lens[128] = { 0 }, run_bad = 0, lmin = 999, lmax = 0, kinds = 0;
        for (u32 i = 0; i < 3000; i++) {
            CHECK(reborn(w, ch, 4000000u + i, m, f));
            u32 cn;
            const AzGene *g = genes(w, ch, 1, &cn);
            if (cn < lmin) lmin = cn;
            if (cn > lmax) lmax = cn;
            if (cn < 128) lens[cn]++;
            for (u32 j = 0; j < cn; j++) {
                if (g[j].expr != 3 && g[j].expr != 4) run_bad++;
                if (g[j].allele >= (g[j].expr == 3 ? 80u : 40u)) run_bad++;
                if (j && g[j].expr == g[j - 1].expr && g[j].allele != g[j - 1].allele + 1) run_bad++;
            }
        }
        for (u32 i = 0; i < 128; i++) kinds += lens[i] != 0;
        CHECK_EQ(run_bad, 0);
        CHECK(lmin >= 40 && lmax <= 80);
        CHECK(lens[40] > 0 && lens[80] > 0);
        CHECK(kinds >= 20);
        printf("meiosis, unequal exchange: gamete lengths %u..%u, %u different lengths\n", lmin, lmax, kinds);
    }
    az_test_world_free(&t);
}

/* ---- 5. mutation ---- */
typedef struct MutCond { u32 dup, del, neo, expr; u8 t_mut, t_dup, t_del, t_new; } MutCond;

typedef struct MutStat {
    u32 n, grew1, grew_more, shrank1, shrank_more, same_len;
    u32 fam_changed, expr_changed, expr_far, shape_bad, new_bad, clean, exact_bad;
    u64 points;
} MutStat;

/* what the mother's rate must be for the child, restated: (8 + MUT_RATE) * (64 + tox) */
static u32 lam_of(u32 mut_rate, u32 tox) { return (8u + mut_rate) * (64u + (tox > 512u ? 512u : tox)); }

static u32 base_dist(const AzGene *a, const AzGene *b, u32 n)
{
    u32 d = 0;
    for (u32 i = 0; i < n; i++) d += base_dist1(a[i].allele, b[i].allele);
    return d;
}

/* is b the list a with one run of `len` genes copied behind itself (families only)? */
static bool is_copy(const AzGene *a, u32 na, const AzGene *b, u32 nb)
{
    if (nb <= na) return false;
    const u32 len = nb - na;
    for (u32 at = 0; at + len <= na; at++) {
        bool ok = true;
        for (u32 i = 0; i < at + len && ok; i++) ok = b[i].fam == a[i].fam;
        for (u32 i = 0; i < len && ok; i++) ok = b[at + len + i].fam == a[at + i].fam;
        for (u32 i = at + len; i < na && ok; i++) ok = b[i + len].fam == a[i].fam;
        if (ok) return true;
    }
    return false;
}

/* is b the list a with one run of genes taken out (families only)? */
static bool is_loss(const AzGene *a, u32 na, const AzGene *b, u32 nb)
{
    if (nb >= na) return false;
    const u32 len = na - nb;
    for (u32 at = 0; at + len <= na; at++) {
        bool ok = true;
        for (u32 i = 0; i < at && ok; i++) ok = b[i].fam == a[i].fam;
        for (u32 i = at + len; i < na && ok; i++) ok = b[i - len].fam == a[i].fam;
        if (ok) return true;
    }
    return false;
}

static void bud_many(AzWorld *w, u32 ch, u32 m, u64 uid0, u32 n, const MutCond *c, u32 tox, MutStat *st)
{
    memset(st, 0, sizeof *st);
    w->param[AZ_WP_GENE_DUP_RATE] = c->dup;
    w->param[AZ_WP_GENE_DEL_RATE] = c->del;
    w->param[AZ_WP_GENE_NEW_RATE] = c->neo;
    w->param[AZ_WP_EXPR_MUT_RATE] = c->expr;
    T(w, m)[AZ_T_MUT_RATE] = c->t_mut; T(w, m)[AZ_T_GENE_DUP] = c->t_dup;
    T(w, m)[AZ_T_GENE_DEL] = c->t_del; T(w, m)[AZ_T_GENE_NEW] = c->t_new;
    const u32 lam = (w->cfg.lesion & AZ_LESION_MUTATION) ? 0u : lam_of(c->t_mut, (w->cfg.lesion & AZ_LESION_TOXIN) ? 0u : tox);
    static AzGene mg[2][512];
    u32 mn[2];
    for (u32 h = 0; h < 2; h++) {
        const AzGene *g = genes(w, m, h, &mn[h]);
        memcpy(mg[h], g, (size_t)mn[h] * sizeof(AzGene));
    }
    for (u32 i = 0; i < n; i++) {
        const u64 uid = uid0 + i;
        CHECK(reborn(w, ch, uid, m, AZ_NONE));
        st->n++;
        /* which draws fired for this child (the layout of the module's draws) */
        u32 r[4];
        az_rand4(w, uid, AZ_RS_MUTATE, 0, r);
        const bool rearranged = (r[0] & 0x1FFFFFu) < lam || (r[2] & 0x1FFFFFu) < lam;
        u32 want_points = 0;
        for (u32 tr = 0; tr < 16; tr++) {
            if ((tr & 3u) == 0) az_rand4(w, uid, AZ_RS_MUTATE, 1u + (tr >> 2), r);
            want_points += (r[tr & 3u] & 0x3FFFFu) < lam;
        }
        u32 cn[2];
        const AzGene *cg[2];
        cg[0] = genes(w, ch, 0, &cn[0]);
        cg[1] = genes(w, ch, 1, &cn[1]);
        const i32 delta = (i32)(cn[0] + cn[1]) - (i32)(mn[0] + mn[1]);
        if (delta == 1) st->grew1++;
        else if (delta > 1) st->grew_more++;
        else if (delta == -1) st->shrank1++;
        else if (delta < -1) st->shrank_more++;
        else st->same_len++;
        if (delta > 8 || delta < -8) st->shape_bad++;
        if (rearranged) continue;                    /* a conversion or an inversion: not judged gene by gene */
        st->clean++;
        if (delta == 0) {
            if (cn[0] != mn[0] || cn[1] != mn[1]) { st->shape_bad++; continue; }
            u32 fam_diff = 0, pts = 0;
            for (u32 h = 0; h < 2; h++) {
                pts += base_dist(mg[h], cg[h], mn[h]);
                for (u32 j = 0; j < mn[h]; j++) {
                    fam_diff += cg[h][j].fam != mg[h][j].fam;
                    if (cg[h][j].expr != mg[h][j].expr) {
                        st->expr_changed++;
                        if (abs((int)cg[h][j].expr - (int)mg[h][j].expr) > 32) st->expr_far++;
                    }
                }
            }
            st->points += pts;
            if (pts != want_points) st->exact_bad++;         /* every successful trial changes one new base */
            if (fam_diff > 1) st->shape_bad++;
            st->fam_changed += fam_diff;
        } else {
            /* exactly one haplotype changed its length, by a copied or a lost run */
            const u32 h = cn[0] != mn[0] ? 0u : 1u;
            if (cn[h ^ 1u] != mn[h ^ 1u]) { st->shape_bad++; continue; }
            if (delta > 0 && c->neo == 0 && !is_copy(mg[h], mn[h], cg[h], cn[h])) st->shape_bad++;
            if (delta < 0 && !is_loss(mg[h], mn[h], cg[h], cn[h])) st->shape_bad++;
            if (delta == 1 && c->neo != 0 && c->dup == 0) {
                /* a new gene: the list with it taken out is the mother's; it is weakly expressed */
                bool ok = false;
                for (u32 pos = 0; pos <= mn[h] && !ok; pos++) {
                    bool same = true;
                    for (u32 j = 0; j < pos && same; j++) same = cg[h][j].fam == mg[h][j].fam;
                    for (u32 j = pos; j < mn[h] && same; j++) same = cg[h][j + 1].fam == mg[h][j].fam;
                    if (same && cg[h][pos].expr >= 32 && cg[h][pos].expr <= 95 &&
                        cg[h][pos].fam < w->param[AZ_WP_FAMILY_SPACE]) ok = true;
                }
                if (!ok) st->new_bad++;
            }
        }
    }
}

static void test_mutation(u64 seed)
{
    AzConfig cfg = az_test_config(seed, 64);
    AzTestWorld t;
    if (!az_test_world(&t, &cfg, 0)) { CHECK(!"world creation"); return; }
    AzWorld *w = t.w;
    /* the mother is a founder that worldgen placed: she has a cell */
    u32 m = AZ_NONE;
    for (u32 s = 0; s < w->n_slots && m == AZ_NONE; s++)
        if ((w->flags[s] & AZ_F_ALIVE) && w->cell[s] < w->n_cells) m = s;
    const u32 ch = az_spawn(w);
    CHECK(m != AZ_NONE && ch != AZ_NONE);
    if (m == AZ_NONE || ch == AZ_NONE) { az_test_world_free(&t); return; }
    u32 *tox = &w->c_sp[(size_t)w->cell[m] * AZ_NSPECIES + AZ_SP_TOX_Z];
    const u32 n = 20000;
    MutStat st;
    *tox = 0;

    /* point mutations alone: every child's count is exact, the mean is lambda */
    const MutCond points_hi = { 0, 0, 0, 0, 255, 128, 128, 128 };
    bud_many(w, ch, m, 10000000u, n, &points_hi, 0, &st);
    double lam = lam_of(255, 0) / 16384.0, mean = (double)st.points / st.clean;
    CHECK_EQ(st.exact_bad, 0); CHECK_EQ(st.shape_bad, 0); CHECK_EQ(st.same_len, n);
    CHECK_EQ(st.fam_changed, 0); CHECK_EQ(st.expr_changed, 0);
    CHECK(near(mean, lam, 0.04));                                   /* sigma 0.0072 */
    /* a conversion or an inversion: lambda / 128 each */
    CHECK(near((double)(n - st.clean) / n, 2.0 * lam / 128.0, 0.006));       /* sigma 0.0009 */
    printf("mutation seed %" PRIu64 ": MUT_RATE 255 mean %.4f (lambda %.4f), rearranged %.4f (expected %.4f)\n",
           seed, mean, lam, (double)(n - st.clean) / n, 2.0 * lam / 128.0);
    const MutCond points_lo = { 0, 0, 0, 0, 0, 128, 128, 128 };
    bud_many(w, ch, m, 11000000u, n, &points_lo, 0, &st);
    lam = lam_of(0, 0) / 16384.0; mean = (double)st.points / st.clean;
    CHECK_EQ(st.exact_bad, 0);
    CHECK(near(mean, lam, 0.007));                                  /* sigma 0.00125 */
    /* radiotoxin in the mother's cell multiplies the rate; the toxin lesion removes the term */
    const MutCond points_mid = { 0, 0, 0, 0, 128, 128, 128, 128 };
    *tox = 512;
    bud_many(w, ch, m, 12000000u, n, &points_mid, 512, &st);
    lam = lam_of(128, 512) / 16384.0; mean = (double)st.points / st.clean;
    CHECK_EQ(st.exact_bad, 0);
    CHECK(near(mean, lam, 0.08));                                   /* sigma 0.013 */
    printf("mutation: with radiotoxin 512 mean %.3f (lambda %.3f)\n", mean, lam);
    *tox = 100000;                                                  /* the term saturates at 512 */
    bud_many(w, ch, m, 12500000u, 4000, &points_mid, 100000, &st);
    CHECK_EQ(st.exact_bad, 0);
    w->cfg.lesion |= AZ_LESION_TOXIN;
    bud_many(w, ch, m, 13000000u, n, &points_mid, 512, &st);
    lam = lam_of(128, 0) / 16384.0; mean = (double)st.points / st.clean;
    CHECK_EQ(st.exact_bad, 0);
    CHECK(near(mean, lam, 0.03));                                   /* sigma 0.005 */
    w->cfg.lesion &= ~AZ_LESION_TOXIN;
    *tox = 0;
    /* the mutation lesion: every child is its mother */
    w->cfg.lesion |= AZ_LESION_MUTATION;
    const MutCond all_on = { 1024, 1024, 1024, 64, 255, 255, 255, 255 };
    bud_many(w, ch, m, 14000000u, 2000, &all_on, 0, &st);
    CHECK_EQ(st.points, 0); CHECK_EQ(st.same_len, 2000); CHECK_EQ(st.clean, 2000);
    CHECK_EQ(st.fam_changed, 0); CHECK_EQ(st.expr_changed, 0);
    w->cfg.lesion &= ~AZ_LESION_MUTATION;

    /* the genome changes size. Chances out of 2^21: lambda_q14 * world rate * (8 + trait) >> 13 */
    const double l14 = (double)lam_of(255, 0);
    const double p_hi = floor(l14 * 1024.0 * 263.0 / 8192.0) / 2097152.0;     /* 0.2639 */
    const double p_blk = floor(floor(l14 * 1024.0 * 263.0 / 8192.0) / 4.0) / 2097152.0;
    /* copies: one gene, or a run of 2..8 */
    const MutCond dup_on = { 1024, 0, 0, 0, 255, 255, 128, 128 };
    bud_many(w, ch, m, 20000000u, n, &dup_on, 0, &st);
    CHECK_EQ(st.shape_bad, 0); CHECK_EQ(st.shrank1 + st.shrank_more, 0);
    CHECK(near((double)st.grew1 / n, p_hi + p_blk / 80.0, 0.017));  /* sigma 0.0031 */
    CHECK(near((double)st.grew_more / n, p_blk * 79.0 / 80.0, 0.010));       /* sigma 0.0018 */
    printf("mutation: gene copies %.4f (expected %.4f), block copies %.4f (expected %.4f)\n",
           (double)st.grew1 / n, p_hi + p_blk / 80.0, (double)st.grew_more / n, p_blk * 79.0 / 80.0);
    /* the mother's own rate trait scales it: GENE_DUP 0 leaves 8/263 of it */
    const MutCond dup_low = { 1024, 0, 0, 0, 255, 0, 128, 128 };
    bud_many(w, ch, m, 21000000u, n, &dup_low, 0, &st);
    {
        const double p = floor(l14 * 1024.0 * 8.0 / 8192.0) / 2097152.0;
        CHECK(near((double)(st.grew1 + st.grew_more) / n, p * 1.25, 0.004));  /* sigma 0.0007 */
        printf("mutation: with GENE_DUP 0 copies fall to %.4f (expected %.4f)\n",
               (double)(st.grew1 + st.grew_more) / n, p * 1.25);
    }
    /* losses */
    const MutCond del_on = { 0, 1024, 0, 0, 255, 128, 255, 128 };
    bud_many(w, ch, m, 22000000u, n, &del_on, 0, &st);
    CHECK_EQ(st.shape_bad, 0); CHECK_EQ(st.grew1 + st.grew_more, 0);
    CHECK(near((double)st.shrank1 / n, p_hi + p_blk / 80.0, 0.017));
    CHECK(near((double)st.shrank_more / n, p_blk * 79.0 / 80.0, 0.010));
    printf("mutation: gene losses %.4f, block losses %.4f\n", (double)st.shrank1 / n, (double)st.shrank_more / n);
    /* new genes of any family, and genes that change family */
    const MutCond new_on = { 0, 0, 1024, 0, 255, 128, 128, 255 };
    bud_many(w, ch, m, 23000000u, n, &new_on, 0, &st);
    CHECK_EQ(st.shape_bad, 0); CHECK_EQ(st.new_bad, 0);
    CHECK_EQ(st.grew_more + st.shrank1 + st.shrank_more, 0);
    CHECK(near((double)st.grew1 / n, p_hi, 0.017));
    CHECK(near((double)st.fam_changed / st.clean, p_hi, 0.017));
    printf("mutation: new genes %.4f, family changes %.4f (expected %.4f each)\n",
           (double)st.grew1 / n, (double)st.fam_changed / st.clean, p_hi);
    /* expression drifts: 4 trials of lambda_q14 * rate / 64 out of 2^18 */
    const MutCond expr_on = { 0, 0, 0, 64, 255, 128, 128, 128 };
    bud_many(w, ch, m, 24000000u, n, &expr_on, 0, &st);
    {
        const double p = 4.0 * l14 / 262144.0;
        CHECK_EQ(st.shape_bad, 0); CHECK_EQ(st.expr_far, 0); CHECK_EQ(st.fam_changed, 0);
        CHECK(near((double)st.expr_changed / st.clean, p, 0.02));    /* sigma 0.0035 */
        printf("mutation: expression changes per child %.4f (expected %.4f)\n", (double)st.expr_changed / st.clean, p);
    }
    az_test_world_free(&t);
}

/* ---- 6. az_genome_fit, call order, a world ---- */
static void test_fit(u64 seed)
{
    AzConfig cfg = az_test_config(seed, 64);
    AzTestWorld t;
    if (!az_test_world(&t, &cfg, 0)) { CHECK(!"world creation"); return; }
    AzWorld *w = t.w;
    const u32 a = founder(w, 0x50001u), b = az_spawn(w);
    CHECK(a != AZ_NONE && b != AZ_NONE);
    if (a == AZ_NONE || b == AZ_NONE) { az_test_world_free(&t); return; }
    w->uid[b] = 777;
    CHECK(az_conceive(w, b, a, AZ_NONE));                 /* a copy with a few mutations: nearly the same genome */
    u8 target[AZ_TRAITS], weight[AZ_TRAITS];
    memset(weight, 0, sizeof weight);
    for (u32 k = 0; k < AZ_TRAITS; k++) target[k] = T(w, a)[k];
    static const u8 pick[6] = { AZ_T_METAB, AZ_T_THRUST, AZ_T_TOPT, AZ_T_SENSE_RANGE, AZ_T_AGGR, AZ_T_SEGMENT };
    u32 before = 0;
    for (u32 i = 0; i < 6; i++) {
        target[pick[i]] = (u8)(i & 1 ? 40 : 215);
        weight[pick[i]] = 4;
        before += 4u * (u32)abs((int)T(w, a)[pick[i]] - (int)target[pick[i]]);
    }
    const u8 sex0 = w->sex[a];
    const u32 after = az_genome_fit(w, a, target, weight, 800);
    u32 recount = 0;
    for (u32 i = 0; i < 6; i++) recount += 4u * (u32)abs((int)T(w, a)[pick[i]] - (int)target[pick[i]]);
    CHECK_EQ(after, recount);                             /* it returns what it reached, and leaves the slot developed */
    CHECK(after * 4 <= before);
    CHECK_EQ(w->sex[a], sex0);                            /* the sex family only takes shifts that keep the sex */
    RefOut o;
    ref_develop(w, a, &o);
    CHECK(memcmp(o.trait, T(w, a), AZ_TRAITS) == 0);
    /* no random numbers: the same call on the same genome gives the same genome */
    u32 n1, n2;
    const u32 c = founder(w, 0x50001u), d = az_spawn(w);
    CHECK(c != AZ_NONE && d != AZ_NONE);
    if (c != AZ_NONE && d != AZ_NONE) {
        CHECK(az_blk_resize(w, d, AZ_SEG_GEN_A, AZ_CORE_FAMS * 4) && az_blk_resize(w, d, AZ_SEG_GEN_B, AZ_CORE_FAMS * 4));
        for (u32 h = 0; h < 2; h++) {
            const AzGene *src = genes(w, c, h, &n1);
            AzGene *dst = genes(w, d, h, &n2);
            CHECK_EQ(n1, n2);
            memcpy(dst, src, (size_t)n1 * sizeof(AzGene));
        }
        const u32 r1 = az_genome_fit(w, c, target, weight, 800), r2 = az_genome_fit(w, d, target, weight, 800);
        CHECK_EQ(r1, r2);
        for (u32 h = 0; h < 2; h++) {
            const AzGene *x = genes(w, c, h, &n1), *y = genes(w, d, h, &n2);
            CHECK(n1 == n2 && memcmp(x, y, (size_t)n1 * sizeof(AzGene)) == 0);
        }
    }
    printf("fit seed %" PRIu64 ": weighted error %u -> %u\n", seed, before, after);
    az_test_world_free(&t);
}

enum { ORDER_KIDS = 60, ORDER_GENES = 2 * (AZ_CORE_FAMS + 16) };
typedef struct OrderOut { u32 n[2]; AzGene g[ORDER_GENES]; u8 trait[AZ_TRAITS]; u32 brain_bytes; u64 brain_sum; } OrderOut;

static void order_run(u64 seed, int reverse, OrderOut *out)
{
    AzConfig cfg = az_test_config(seed, 64);
    AzTestWorld t;
    memset(out, 0, sizeof(OrderOut) * ORDER_KIDS);
    if (!az_test_world(&t, &cfg, 0)) { CHECK(!"world creation"); return; }
    AzWorld *w = t.w;
    const u32 m = founder(w, 0x60001u), f = founder(w, 0x60001u), ch = az_spawn(w);
    CHECK(m != AZ_NONE && f != AZ_NONE && ch != AZ_NONE);
    for (u32 q = 0; q < ORDER_KIDS && ch != AZ_NONE; q++) {
        const u32 i = reverse ? ORDER_KIDS - 1 - q : q;
        CHECK(reborn(w, ch, 9000000u + i, m, (i & 3) == 3 ? AZ_NONE : f));
        OrderOut *o = &out[i];
        u32 at = 0;
        for (u32 h = 0; h < 2; h++) {
            const AzGene *g = genes(w, ch, h, &o->n[h]);
            for (u32 j = 0; j < o->n[h] && at < ORDER_GENES; j++) o->g[at++] = g[j];
        }
        memcpy(o->trait, T(w, ch), AZ_TRAITS);
        const u8 *br = (const u8 *)az_brain(w, ch, &o->brain_bytes);
        for (u32 j = 0; j < o->brain_bytes; j++) o->brain_sum = az_mix64(o->brain_sum + br[j]);
    }
    az_test_world_free(&t);
}

static void test_order(u64 seed)
{
    static OrderOut a[ORDER_KIDS], b[ORDER_KIDS];
    order_run(seed, 0, a);
    order_run(seed, 1, b);
    /* a child is a function of its uid and its parents, not of what was conceived before it */
    CHECK(memcmp(a, b, sizeof a) == 0);
    u32 distinct = 0;
    for (u32 i = 1; i < ORDER_KIDS; i++) distinct += memcmp(&a[i], &a[0], sizeof a[0]) != 0;
    CHECK(distinct >= ORDER_KIDS - 2);
}

typedef struct WorldOut { u64 hash; u64 matter; i64 energy; u64 births; u32 gmin, gmax, hmin, hmax, pmax, bad, live; int ok; } WorldOut;

static WorldOut world_run(u64 seed, u32 threads, u8 reorder, u16 store_period, u32 lesion)
{
    WorldOut o;
    memset(&o, 0, sizeof o);
    AzConfig cfg = az_test_config(seed, 1500);
    cfg.reorder_period = reorder;
    cfg.store_period = store_period;
    cfg.lesion = lesion;
    AzTestWorld t;
    if (!az_test_world(&t, &cfg, threads)) { CHECK(!"world creation"); return o; }
    AzWorld *w = t.w;
    az_test_founders_bud(w);
    /* a fast-changing genome, so that 150 ticks show it */
    w->param[AZ_WP_GENE_DUP_RATE] = 2048; w->param[AZ_WP_GENE_DEL_RATE] = 2048; w->param[AZ_WP_GENE_NEW_RATE] = 1024;
    w->param[AZ_WP_NEURO_RATE] = 1024;
    const u64 m0 = az_total_matter(w);
    const i64 e0 = az_total_energy(w);
    o.ok = 1;
    for (u32 i = 0; i < 150; i++) {
        az_tick(w);
        if (az_total_matter(w) != m0 || az_total_energy(w) != e0) o.ok = 0;
    }
    o.hash = az_state_hash(w); o.matter = az_total_matter(w); o.energy = az_total_energy(w);
    o.births = w->totals.births; o.live = w->n_live;
    o.gmin = o.hmin = ~0u;
    for (u32 s = 0; s < w->n_slots; s++) {
        if (!(w->flags[s] & AZ_F_ALIVE)) continue;
        u32 na, nb, bb, mb, np;
        (void)az_genes(w, s, 0, &na); (void)az_genes(w, s, 1, &nb);
        const AzBrain *br = az_brain(w, s, &bb);
        (void)az_seg(w, s, AZ_SEG_MIND, &mb);
        (void)az_parts(w, s, &np);
        if (!w->blk[s] || !az_blk_ok(az_blk(w, s), az_blk(w, s)->units) || !az_brain_ok(br, bb, mb)) { o.bad++; continue; }
        const u32 g = na + nb, h = 8u * br->n_hb;
        if (g < o.gmin) o.gmin = g;
        if (g > o.gmax) o.gmax = g;
        if (h < o.hmin) o.hmin = h;
        if (h > o.hmax) o.hmax = h;
        if (np > o.pmax) o.pmax = np;
    }
    az_test_world_free(&t);
    return o;
}

static void test_world(u64 seed)
{
    const WorldOut a = world_run(seed, 0, 1, 0, 0);
    const WorldOut b = world_run(seed, 8, 7, 3, 0);
    const WorldOut c = world_run(seed, 3, 31, 64, 0);
    const WorldOut l = world_run(seed, 8, 7, 3, AZ_LESION_MUTATION);
    CHECK(a.ok && b.ok && c.ok && l.ok);                  /* matter and energy exact at every tick */
    CHECK_EQ(a.bad + b.bad + c.bad + l.bad, 0);           /* every block and brain well formed */
    CHECK(a.births > 100);
    /* threads, slot order and where the blocks lie are not state */
    CHECK_EQ(a.hash, b.hash);
    CHECK_EQ(a.hash, c.hash);
    /* the living have come to differ in the size of genome and brain */
    CHECK(a.gmax > a.gmin);
    CHECK(a.hmax > a.hmin);
    /* without mutation nothing changes size, and the world is another world */
    CHECK_EQ(l.gmin, 2 * AZ_CORE_FAMS); CHECK_EQ(l.gmax, 2 * AZ_CORE_FAMS);
    CHECK_EQ(l.hmin, l.hmax);
    CHECK(l.hash != a.hash);
    printf("world seed %" PRIu64 ": %" PRIu64 " births, live %u, genes %u..%u, neurons %u..%u, most parts %u, hash %016" PRIx64
           " on 1, 8 and 3 threads; mutation lesion: genes %u..%u\n",
           seed, a.births, a.live, a.gmin, a.gmax, a.hmin, a.hmax, a.pmax, a.hash, l.gmin, l.gmax);
}

int main(int argc, char **argv)
{
    /* default: two tuning seeds; `test_genome <seed>` runs another */
    u64 seeds[2] = { 41, 71 };
    u32 n_seeds = 2;
    if (argc > 1) { seeds[0] = strtoull(argv[1], NULL, 10); n_seeds = 1; }

    AzConfig cfg = az_test_config(seeds[0], 64);
    AzTestWorld t;
    if (!az_test_world(&t, &cfg, 0)) { CHECK(!"world creation"); return az_test_done(); }
    test_code_structure(t.w, 1, 24);    /* tuning seeds 1..24 and 51..74 */
    test_code_structure(t.w, 51, 24);
    test_code_structure(t.w, 900000, 400);
    test_develop(t.w);
    az_test_world_free(&t);

    for (u32 i = 0; i < n_seeds; i++) {
        test_founders(seeds[i]);
        test_meiosis(seeds[i]);
        test_mutation(seeds[i]);
        test_fit(seeds[i]);
        test_order(seeds[i]);
        test_world(seeds[i]);
    }
    return az_test_done();
}
