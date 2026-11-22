/* az_brain.c - the per-creature int8 recurrent network, and the lineages.
 *
 * Every creature carries its own brain in its block (segment AZ_SEG_BRAIN;
 * the layout is described at AzBrain in az_abi.h): any number of hidden
 * blocks of 8 neurons, any set of tiles of connections. A child inherits its
 * mother's brain; neural mutation shifts weights and biases, adds or removes
 * a tile, adds or removes a block of neurons. Nothing fixes the size of a
 * brain: what limits it is what its bearer can pay for it (az_metab) and the
 * room in the creature store.
 *
 * az_brain_step_c defines the exact integer semantics of one step; the
 * assembly twins (kernels/az_brain_vnni.asm) are bit-identical to it. All
 * sums wrap modulo 2^32, so the step is defined for every weight and bias.
 * Contract for the twins: sens and hid are 0..127 (az_sense and the clamp
 * guarantee it); the AVX2 twin relies on that, the VNNI twin and the
 * reference do not.
 */
#include "az_abi.h"

#define BR_SG (AZ_SENS / 4u)          /* sensor input groups                 */
#define BR_MB (AZ_MOT / 8u)           /* motor blocks                        */
#define BR_HB_MAX 0x7FE0u             /* hidden blocks an u16 group index can address */

static u64 pad8(u64 n) { return (n + 7u) & ~(u64)7u; }

#define BR_AT(type, b, off) ((type *)(void *)((u8 *)(void *)(b) + (b)->off))
#define BR_AT_C(type, b, off) ((const type *)(const void *)((const u8 *)(const void *)(b) + (b)->off))

u64 az_brain_bytes(u32 n_hb, u32 n_t1, u32 n_t2)
{
    return sizeof(AzBrain) + 32ull * n_hb + 4ull * AZ_MOT + pad8(2ull * n_hb) + pad8(2ull * BR_MB) +
           pad8(2ull * n_t1) + pad8(2ull * n_t2) + 32ull * n_t1 + 32ull * n_t2;
}

u32 az_mind_bytes(u32 n_hb) { return AZ_SENS + 16u * n_hb + (u32)sizeof(AzSenseMemory); }

static void brain_layout(AzBrain *b, u32 n_hb, u32 n_t1, u32 n_t2)
{
    u32 at = (u32)sizeof(AzBrain);
    b->n_hb = (u16)n_hb; b->n_mb = (u16)BR_MB; b->n_t1 = n_t1; b->n_t2 = n_t2;
    b->n_sg = (u8)BR_SG; b->pad = 0;
    b->off_b1 = at;  at += 32u * n_hb;
    b->off_b2 = at;  at += 4u * AZ_MOT;
    b->off_c1 = at;  at += (u32)pad8(2ull * n_hb);
    b->off_c2 = at;  at += (u32)pad8(2ull * BR_MB);
    b->off_in1 = at; at += (u32)pad8(2ull * n_t1);
    b->off_in2 = at; at += (u32)pad8(2ull * n_t2);
    b->off_w1 = at;  at += 32u * n_t1;
    b->off_w2 = at;
}

/* ---------------------------------------------------------------------- */
/* one step                                                                */
/* ---------------------------------------------------------------------- */
AZ_INLINE void tile_add(u32 acc[8], const i8 *wt, const u8 *xi)
{
    const i32 x0 = xi[0], x1 = xi[1], x2 = xi[2], x3 = xi[3];
    for (u32 k = 0; k < 8u; k++)
        acc[k] += (u32)((i32)wt[k * 4u] * x0 + (i32)wt[k * 4u + 1u] * x1 +
                        (i32)wt[k * 4u + 2u] * x2 + (i32)wt[k * 4u + 3u] * x3);
}

void az_brain_step_mod_c(const AzBrain *b, const u8 gain[8], const i8 bias[8], u8 *mind, i8 *motor)
{
    const i32 *b1 = BR_AT_C(i32, b, off_b1), *b2 = BR_AT_C(i32, b, off_b2);
    const u16 *c1 = BR_AT_C(u16, b, off_c1), *c2 = BR_AT_C(u16, b, off_c2);
    const u16 *it = BR_AT_C(u16, b, off_in1);
    const i8 *wt = BR_AT_C(i8, b, off_w1);
    const u32 n_hb = b->n_hb, n_mb = b->n_mb;
    const u32 s1 = b->shift1 & 31u, s2 = b->shift2 & 31u;
    u8 *h2 = mind + AZ_SENS + 8u * n_hb;

    for (u32 hb = 0; hb < n_hb; hb++) {
        u32 acc[8];
        for (u32 k = 0; k < 8u; k++) acc[k] = (u32)b1[hb * 8u + k];
        for (u32 t = c1[hb]; t; t--, it++, wt += 32) tile_add(acc, wt, mind + 4u * (u32)*it);
        for (u32 k = 0; k < 8u; k++) {
            i32 a = (i32)acc[k] >> s1;
            a = (i32)((u32)a * (u32)gain[k]) >> 6;
            a = (i32)((u32)a + (u32)(2 * (i32)bias[k]));
            h2[hb * 8u + k] = (u8)az_clamp_i32(a, 0, 127);
        }
    }
    it = BR_AT_C(u16, b, off_in2);
    wt = BR_AT_C(i8, b, off_w2);
    for (u32 mb = 0; mb < n_mb; mb++) {
        u32 acc[8];
        for (u32 k = 0; k < 8u; k++) acc[k] = (u32)b2[mb * 8u + k];
        for (u32 t = c2[mb]; t; t--, it++, wt += 32) tile_add(acc, wt, h2 + 4u * (u32)*it);
        for (u32 k = 0; k < 8u; k++)
            motor[mb * 8u + k] = (i8)az_clamp_i32((i32)acc[k] >> s2, -127, 127);
    }
}

void az_brain_step_c(const AzBrain *b, u8 *mind, i8 *motor)
{
    az_brain_step_mod_c(b, b->gain, b->bias, mind, motor);
}

bool az_brain_ok(const AzBrain *b, u32 bytes, u32 mind_bytes)
{
    if (bytes < sizeof(AzBrain)) return false;
    if (b->n_mb != BR_MB || b->n_sg != BR_SG || b->n_hb > BR_HB_MAX || b->shift1 > 15 || b->shift2 > 15 || b->pad)
        return false;
    if (az_brain_bytes(b->n_hb, b->n_t1, b->n_t2) != bytes) return false;
    AzBrain want;
    brain_layout(&want, b->n_hb, b->n_t1, b->n_t2);
    if (b->off_b1 != want.off_b1 || b->off_b2 != want.off_b2 || b->off_c1 != want.off_c1 ||
        b->off_c2 != want.off_c2 || b->off_in1 != want.off_in1 || b->off_in2 != want.off_in2 ||
        b->off_w1 != want.off_w1 || b->off_w2 != want.off_w2) return false;
    if (mind_bytes != az_mind_bytes(b->n_hb)) return false;
    /* the tiles of every block: counted, ascending groups, inside the inputs */
    const u16 *c1 = BR_AT_C(u16, b, off_c1), *c2 = BR_AT_C(u16, b, off_c2);
    const u16 *in1 = BR_AT_C(u16, b, off_in1), *in2 = BR_AT_C(u16, b, off_in2);
    u64 at = 0;
    for (u32 hb = 0; hb < b->n_hb; hb++) {
        if (at + c1[hb] > b->n_t1) return false;
        for (u32 t = 0; t < c1[hb]; t++) {
            const u32 g = in1[at + t];
            if (g >= BR_SG + 2u * b->n_hb || (t && g <= in1[at + t - 1u])) return false;
        }
        at += c1[hb];
    }
    if (at != b->n_t1) return false;
    at = 0;
    for (u32 mb = 0; mb < BR_MB; mb++) {
        if (at + c2[mb] > b->n_t2) return false;
        for (u32 t = 0; t < c2[mb]; t++) {
            const u32 g = in2[at + t];
            if (g >= 2u * b->n_hb || (t && g <= in2[at + t - 1u])) return false;
        }
        at += c2[mb];
    }
    return at == b->n_t2;
}

/* ---------------------------------------------------------------------- */
/* founders                                                                */
/* ---------------------------------------------------------------------- */
/* A species' founder brain is developed from a small random program: a type
 * 0..7 for every neuron, a wiring rule (one weight per pair of types) and a
 * bias per type. A synapse's weight is the rule's entry for its (post type,
 * pre type) pair plus a per-synapse variation that is a pure function of
 * (salt, layer, post, pre). Neuron index space of the program: sensors
 * 0..AZ_SENS-1, then the hidden units, then the motors.
 *
 * Founder ranges. These are initial conditions only (how wide the random
 * program is drawn), chosen so that a founder network is neither silent nor
 * saturated; nothing here says what a creature should do. All inputs are
 * non-negative and a wiring rule has only 8 x 8 entries, so the summed drive
 * of a hidden unit has a mean several times its fluctuation; a narrow rule
 * and these shifts keep the mean drive inside the 0..127 window and the
 * motors off the rails. The per-synapse variation stays smaller than the
 * rule. Which tiles a founder has at all is the world parameter
 * BRAIN_DENSITY; how many hidden blocks, BRAIN_FOUND_HID. */
#define FOUND_THETA_SPAN 32     /* theta uniform in -32..32                  */
#define FOUND_VAR_MIN    16     /* var uniform in 16..47                     */
#define FOUND_TBIAS_MIN  64     /* tbias uniform in 64..127                  */
#define FOUND_SHIFT1     9
#define FOUND_SHIFT2     7

typedef struct Prog { i8 theta[64]; i8 tbias[8]; u16 salt; u8 var; u8 pad; } Prog;

/* Philox draw with the tick word fixed at 0: species constants must not
 * depend on when they are made. */
static void rand4_t0(const AzWorld *w, u32 id, u32 stream, u32 idx, u32 out[4])
{
    u32 ctr[4] = { id, 0u, stream, idx };
    az_philox4x32(out, ctr, w->key);
}

static void prog_draw(const AzWorld *w, u32 seed_id, Prog *p)
{
    u32 r[4];
    for (u32 i = 0; i < 64u; i++) {
        if ((i & 3u) == 0) rand4_t0(w, seed_id, AZ_RS_CLADE, 64u + (i >> 2), r);
        p->theta[i] = (i8)((i32)(r[i & 3u] % (2u * FOUND_THETA_SPAN + 1u)) - FOUND_THETA_SPAN);
    }
    rand4_t0(w, seed_id, AZ_RS_CLADE, 128u, r);
    for (u32 i = 0; i < 8u; i++)
        p->tbias[i] = (i8)((i32)((r[i & 3u] >> (8u * (i >> 2))) & 63u) + FOUND_TBIAS_MIN);
    rand4_t0(w, seed_id, AZ_RS_CLADE, 129u, r);
    p->salt = (u16)r[0];
    p->var = (u8)(FOUND_VAR_MIN + (r[1] & 31u));
    p->pad = 0;
}

/* the types of the 4 neurons first, first + 1, .. (first is a multiple of 4) */
static void prog_types(const AzWorld *w, u32 seed_id, u32 first, u8 t[4])
{
    u32 r[4];
    const u32 blk = first >> 2;
    rand4_t0(w, seed_id, AZ_RS_CLADE, blk < 64u ? blk : 4096u + blk, r);
    for (u32 i = 0; i < 4u; i++) t[i] = (u8)(r[i] & 7u);
}

static i32 synapse_noise(const Prog *p, u32 layer, u32 post, u32 pre)
{
    u64 h = az_mix64(((u64)p->salt << 32) ^ ((u64)layer << 24) ^ ((u64)post << 12) ^ pre);
    i32 n = (i32)(h & 0xFF) - 128;                 /* -128..127 */
    return (n * (i32)p->var) >> 8;
}

/* does the founder brain of this species have the tile (layer, out block, in group)? */
static bool tile_on(const AzWorld *w, u32 seed_id, u32 layer, u32 ob, u32 g)
{
    const u32 dens = w->param[AZ_WP_BRAIN_DENSITY];
    if (dens >= 256u) return true;
    const u64 h = az_mix64(az_mix64(((u64)seed_id << 32) ^ w->key[0] ^ ((u64)w->key[1] << 20)) +
                           (((u64)layer << 40) ^ ((u64)ob << 20) ^ g));
    return (u32)(h & 255u) < dens;
}

static u32 found_hb(const AzWorld *w)
{
    const u32 n = w->param[AZ_WP_BRAIN_FOUND_HID];
    return n > BR_HB_MAX ? BR_HB_MAX : n;
}

static void found_count(const AzWorld *w, u32 seed_id, u32 n_hb, u32 *n_t1, u32 *n_t2)
{
    u32 t1 = 0, t2 = 0;
    for (u32 ob = 0; ob < n_hb; ob++)
        for (u32 g = 0; g < BR_SG + 2u * n_hb; g++) t1 += tile_on(w, seed_id, 1, ob, g) ? 1u : 0u;
    for (u32 ob = 0; ob < BR_MB; ob++)
        for (u32 g = 0; g < 2u * n_hb; g++) t2 += tile_on(w, seed_id, 2, ob, g) ? 1u : 0u;
    *n_t1 = t1; *n_t2 = t2;
}

void az_brain_found_size(const AzWorld *w, u32 species_seed, u32 *brain_bytes, u32 *mind_bytes)
{
    const u32 n_hb = found_hb(w);
    u32 t1, t2;
    found_count(w, species_seed, n_hb, &t1, &t2);
    const u64 bytes = az_brain_bytes(n_hb, t1, t2);
    *brain_bytes = bytes > 0xFFFFFFF0ull ? 0xFFFFFFF0u : (u32)bytes;
    *mind_bytes = az_mind_bytes(n_hb);
}

void az_brain_found(AzWorld *w, u32 slot, u32 species_seed)
{
    u32 bytes;
    AzBrain *b = az_brain(w, slot, &bytes);
    if (bytes < sizeof(AzBrain)) return;
    const u32 n_hb = found_hb(w);
    u32 t1, t2;
    found_count(w, species_seed, n_hb, &t1, &t2);
    brain_layout(b, n_hb, t1, t2);
    b->shift1 = FOUND_SHIFT1;
    b->shift2 = FOUND_SHIFT2;
    for (u32 k = 0; k < AZ_MODG; k++) { b->gain[k] = 64; b->bias[k] = 0; }
    Prog pg;
    prog_draw(w, species_seed, &pg);

    i32 *b1 = BR_AT(i32, b, off_b1), *b2 = BR_AT(i32, b, off_b2);
    u16 *c1 = BR_AT(u16, b, off_c1), *c2 = BR_AT(u16, b, off_c2);
    u16 *it = BR_AT(u16, b, off_in1);
    i8 *wt = BR_AT(i8, b, off_w1);
    for (u32 ob = 0; ob < n_hb; ob++) {
        u8 post[8];
        prog_types(w, species_seed, AZ_SENS + ob * 8u, post);
        prog_types(w, species_seed, AZ_SENS + ob * 8u + 4u, post + 4);
        for (u32 k = 0; k < 8u; k++) b1[ob * 8u + k] = (i32)pg.tbias[post[k]] * 64;
        u32 n = 0;
        for (u32 g = 0; g < BR_SG + 2u * n_hb; g++) {
            if (!tile_on(w, species_seed, 1, ob, g)) continue;
            u8 pre[4];
            prog_types(w, species_seed, g * 4u, pre);
            for (u32 k = 0; k < 8u; k++)
                for (u32 j = 0; j < 4u; j++) {
                    const i32 wv = (i32)pg.theta[post[k] * 8u + pre[j]] + synapse_noise(&pg, 1, ob * 8u + k, g * 4u + j);
                    wt[k * 4u + j] = (i8)az_clamp_i32(wv, -127, 127);
                }
            *it++ = (u16)g;
            wt += 32;
            n++;
        }
        c1[ob] = (u16)n;
    }
    it = BR_AT(u16, b, off_in2);
    wt = BR_AT(i8, b, off_w2);
    for (u32 ob = 0; ob < BR_MB; ob++) {
        u8 post[8];
        prog_types(w, species_seed, AZ_SENS + n_hb * 8u + ob * 8u, post);
        prog_types(w, species_seed, AZ_SENS + n_hb * 8u + ob * 8u + 4u, post + 4);
        for (u32 k = 0; k < 8u; k++) b2[ob * 8u + k] = (i32)pg.tbias[post[k]] * 16;
        u32 n = 0;
        for (u32 g = 0; g < 2u * n_hb; g++) {
            if (!tile_on(w, species_seed, 2, ob, g)) continue;
            u8 pre[4];
            prog_types(w, species_seed, AZ_SENS + g * 4u, pre);
            for (u32 k = 0; k < 8u; k++)
                for (u32 j = 0; j < 4u; j++) {
                    const i32 wv = (i32)pg.theta[post[k] * 8u + pre[j]] + synapse_noise(&pg, 2, ob * 8u + k, g * 4u + j);
                    wt[k * 4u + j] = (i8)az_clamp_i32(wv, -127, 127);
                }
            *it++ = (u16)g;
            wt += 32;
            n++;
        }
        c2[ob] = (u16)n;
    }
}

/* ---------------------------------------------------------------------- */
/* inheritance and neural mutation                                         */
/* ---------------------------------------------------------------------- */
enum { ED_NONE = 0, ED_ADD_TILE, ED_DEL_TILE, ED_ADD_BLOCK, ED_DEL_BLOCK };

/* At most one change of structure per birth. */
typedef struct Edit {
    u32 kind;
    u32 layer;      /* ADD_TILE, DEL_TILE: 1 or 2; ADD_BLOCK: the layer its output tile is in */
    u32 ob;         /* ADD_TILE: out block; ADD_BLOCK: the block that reads the new neurons    */
    u32 g;          /* ADD_TILE: in group; DEL_TILE: tile index in its layer;                  */
                    /* ADD_BLOCK: the in group of the new block's own tile                     */
    u32 hb;         /* DEL_BLOCK: the hidden block; ADD_BLOCK: 0 or 1, which half of the new   */
                    /* block the reading tile takes                                            */
} Edit;

/* how strongly the mother's line mutates brains: the world scale times her
 * own heritable rate (which: 0 point changes, 1 growth, 2 pruning). A
 * neutral trait (128) gives the world scale times 136. */
static u32 neuro_scale(const AzWorld *w, u32 mother, u32 which)
{
    static const u8 rate_trait[3] = { AZ_T_NEURO_MUT, AZ_T_NEURO_GROW, AZ_T_NEURO_PRUNE };
    if (w->cfg.lesion & AZ_LESION_MUTATION) return 0;
    return w->param[AZ_WP_NEURO_RATE] * (8u + w->trait[(size_t)mother * AZ_TRAITS + rate_trait[which]]);
}

static bool has_tile(const AzBrain *b, u32 layer, u32 ob, u32 g)
{
    const u16 *cnt = layer == 1u ? BR_AT_C(u16, b, off_c1) : BR_AT_C(u16, b, off_c2);
    const u16 *in = layer == 1u ? BR_AT_C(u16, b, off_in1) : BR_AT_C(u16, b, off_in2);
    u32 at = 0;
    for (u32 k = 0; k < ob; k++) at += cnt[k];
    for (u32 t = 0; t < cnt[ob]; t++)
        if (in[at + t] == g) return true;
    return false;
}

static void edit_draw(const AzWorld *w, u32 mother, u64 child_uid, const AzBrain *m, Edit *e)
{
    e->kind = ED_NONE; e->layer = 0; e->ob = 0; e->g = 0; e->hb = 0;
    const u32 grow = neuro_scale(w, mother, 1), prune = neuro_scale(w, mother, 2);
    if (!grow && !prune) return;
    u32 r[4];
    az_rand4(w, child_uid, AZ_RS_CLADE, 200u, r);
    const u32 u = r[0] & 0x3FFFFu;                   /* chances are out of 2^18 per birth */
    const u32 n_hb = m->n_hb;
    u32 lim = grow;
    if (u < lim) {                                   /* a new tile of connections */
        if (n_hb == 0) return;
        e->layer = (r[1] & 1u) ? 2u : 1u;
        e->ob = e->layer == 1u ? r[2] % n_hb : r[2] % BR_MB;
        e->g = e->layer == 1u ? r[3] % (BR_SG + 2u * n_hb) : r[3] % (2u * n_hb);
        if (!has_tile(m, e->layer, e->ob, e->g)) e->kind = ED_ADD_TILE;
        return;
    }
    lim += prune;
    if (u < lim) {                                   /* a tile is lost */
        const u64 total = (u64)m->n_t1 + m->n_t2;
        if (!total) return;
        const u64 idx = (((u64)r[2] << 32) | r[3]) % total;
        e->layer = idx < m->n_t1 ? 1u : 2u;
        e->g = (u32)(idx < m->n_t1 ? idx : idx - m->n_t1);
        e->kind = ED_DEL_TILE;
        return;
    }
    lim += grow / 8u;
    if (u < lim) {                                   /* a new block of 8 neurons, wired in and out */
        if (n_hb >= BR_HB_MAX) return;
        e->g = r[2] % (BR_SG + 2u * n_hb);
        e->layer = (n_hb == 0 || (r[1] & 1u)) ? 2u : 1u;
        e->ob = e->layer == 1u ? r[3] % n_hb : r[3] % BR_MB;
        e->hb = r[3] >> 31;
        e->kind = ED_ADD_BLOCK;
        return;
    }
    lim += prune / 8u;
    if (u < lim && n_hb) {                           /* a block of neurons is lost with its tiles */
        e->hb = r[2] % n_hb;
        e->kind = ED_DEL_BLOCK;
    }
}

/* Where the rebuild writes; with dst == NULL it only counts. */
typedef struct Out {
    AzBrain *dst;
    u16 *in;
    i8  *wt;
    u32  n;           /* tiles written in the current layer */
    u32  blk_n;       /* tiles written in the current block */
} Out;

static void out_tile(Out *o, u32 g, const i8 *wsrc)
{
    if (o->dst) {
        o->in[o->n] = (u16)g;
        if (wsrc) az_memcpy(o->wt + (size_t)o->n * 32u, wsrc, 32);
    }
    o->n++;
    o->blk_n++;
}

/* One layer of the child from the mother's, with the edit applied. `layer` 1
 * maps input groups (sensor groups, then hidden groups), layer 2 hidden
 * groups only. Returns the tiles of the layer. */
static u32 rebuild_layer(const AzBrain *m, const Edit *e, AzBrain *dst, u32 layer)
{
    const u16 *cnt = layer == 1u ? BR_AT_C(u16, m, off_c1) : BR_AT_C(u16, m, off_c2);
    const u16 *in = layer == 1u ? BR_AT_C(u16, m, off_in1) : BR_AT_C(u16, m, off_in2);
    const i8 *wt = layer == 1u ? BR_AT_C(i8, m, off_w1) : BR_AT_C(i8, m, off_w2);
    const u32 n_out = layer == 1u ? m->n_hb : BR_MB;
    const u32 hid0 = layer == 1u ? BR_SG : 0u;       /* group index of hidden group 0 */
    Out o = { dst, NULL, NULL, 0, 0 };
    u16 *dcnt = NULL;
    if (dst) {
        o.in = layer == 1u ? BR_AT(u16, dst, off_in1) : BR_AT(u16, dst, off_in2);
        o.wt = layer == 1u ? BR_AT(i8, dst, off_w1) : BR_AT(i8, dst, off_w2);
        dcnt = layer == 1u ? BR_AT(u16, dst, off_c1) : BR_AT(u16, dst, off_c2);
    }
    u32 at = 0, ob_new = 0;
    for (u32 ob = 0; ob < n_out; ob++) {
        const u32 n = cnt[ob];
        if (layer == 1u && e->kind == ED_DEL_BLOCK && ob == e->hb) { at += n; continue; }
        o.blk_n = 0;
        bool add = (e->kind == ED_ADD_TILE && e->layer == layer && e->ob == ob);
        for (u32 t = 0; t < n; t++) {
            u32 g = in[at + t];
            if (e->kind == ED_DEL_TILE && e->layer == layer && e->g == at + t) continue;
            if (e->kind == ED_DEL_BLOCK && g >= hid0) {
                const u32 q = g - hid0;              /* a hidden group */
                if (q / 2u == e->hb) continue;       /* it read the lost block */
                if (q / 2u > e->hb) g -= 2u;
            }
            if (add && e->g < g) { out_tile(&o, e->g, NULL); add = false; }
            out_tile(&o, g, wt + (size_t)(at + t) * 32u);
        }
        if (add) out_tile(&o, e->g, NULL);
        /* the tile that reads the new block: its group is above every other */
        if (e->kind == ED_ADD_BLOCK && e->layer == layer && e->ob == ob)
            out_tile(&o, hid0 + 2u * m->n_hb + e->hb, NULL);
        if (dcnt) dcnt[ob_new] = (u16)o.blk_n;
        ob_new++;
        at += n;
    }
    if (layer == 1u && e->kind == ED_ADD_BLOCK) {    /* the new block's own tile */
        o.blk_n = 0;
        out_tile(&o, e->g, NULL);
        if (dcnt) dcnt[ob_new] = (u16)o.blk_n;
    }
    return o.n;
}

void az_brain_child_size(const AzWorld *w, u32 mother, u32 *brain_bytes, u32 *mind_bytes)
{
    u32 bytes;
    const AzBrain *m = az_brain(w, mother, &bytes);
    if (bytes < sizeof(AzBrain)) { *brain_bytes = 0; *mind_bytes = 0; return; }
    /* one edit adds at most one block and two tiles */
    const u64 need = az_brain_bytes(m->n_hb + 1u, m->n_t1 + 2u, m->n_t2 + 2u);
    *brain_bytes = need > 0xFFFFFFF0ull ? 0xFFFFFFF0u : (u32)need;
    *mind_bytes = az_mind_bytes(m->n_hb + 1u);
}

/* fresh weights of a tile nobody had before: small, -16..16 */
static void tile_fresh(const AzWorld *w, u64 child_uid, u32 which, i8 *wt)
{
    u32 r[4];
    for (u32 i = 0; i < 32u; i++) {
        if ((i & 3u) == 0) az_rand4(w, child_uid, AZ_RS_CLADE, 208u + which * 8u + (i >> 2), r);
        wt[i] = (i8)((i32)(r[i & 3u] % 33u) - 16);
    }
}

void az_brain_inherit(AzWorld *w, u32 child, u32 mother, u32 *brain_bytes, u32 *mind_bytes)
{
    u32 mbytes, cbytes;
    const AzBrain *m = az_brain(w, mother, &mbytes);
    AzBrain *c = az_brain(w, child, &cbytes);
    if (mbytes < sizeof(AzBrain) || cbytes < sizeof(AzBrain)) { *brain_bytes = 0; *mind_bytes = 0; return; }
    const u64 uid = w->uid[child];
    Edit e;
    edit_draw(w, mother, uid, m, &e);

    const u32 n_hb = m->n_hb + (e.kind == ED_ADD_BLOCK ? 1u : 0u) - (e.kind == ED_DEL_BLOCK ? 1u : 0u);
    const u32 t1 = rebuild_layer(m, &e, NULL, 1), t2 = rebuild_layer(m, &e, NULL, 2);
    brain_layout(c, n_hb, t1, t2);
    c->shift1 = m->shift1;
    c->shift2 = m->shift2;
    for (u32 k = 0; k < AZ_MODG; k++) { c->gain[k] = m->gain[k]; c->bias[k] = m->bias[k]; }
    {   /* biases: the mother's, without a lost block; a new block starts at 0 */
        const i32 *mb1 = BR_AT_C(i32, m, off_b1);
        i32 *cb1 = BR_AT(i32, c, off_b1);
        u32 to = 0;
        for (u32 hb = 0; hb < m->n_hb; hb++) {
            if (e.kind == ED_DEL_BLOCK && hb == e.hb) continue;
            az_memcpy(cb1 + (size_t)to * 8u, mb1 + (size_t)hb * 8u, 32);
            to++;
        }
        az_memcpy(BR_AT(i32, c, off_b2), BR_AT_C(i32, m, off_b2), 4u * AZ_MOT);
    }
    (void)rebuild_layer(m, &e, c, 1);
    (void)rebuild_layer(m, &e, c, 2);
    /* the tiles the edit made: find them again by their place and fill them */
    if (e.kind == ED_ADD_TILE || e.kind == ED_ADD_BLOCK) {
        u16 *c1 = BR_AT(u16, c, off_c1), *c2 = BR_AT(u16, c, off_c2);
        u16 *in1 = BR_AT(u16, c, off_in1), *in2 = BR_AT(u16, c, off_in2);
        i8 *w1 = BR_AT(i8, c, off_w1), *w2 = BR_AT(i8, c, off_w2);
        const u32 g_read = (e.layer == 1u ? BR_SG : 0u) + 2u * m->n_hb + e.hb;
        const u32 want_g = e.kind == ED_ADD_TILE ? e.g : g_read;
        const u16 *cnt = e.layer == 1u ? c1 : c2;
        const u16 *in = e.layer == 1u ? in1 : in2;
        i8 *wt = e.layer == 1u ? w1 : w2;
        u32 at = 0;
        for (u32 k = 0; k < e.ob; k++) at += cnt[k];
        for (u32 t = 0; t < cnt[e.ob]; t++)
            if (in[at + t] == want_g) tile_fresh(w, uid, 0, wt + (size_t)(at + t) * 32u);
        if (e.kind == ED_ADD_BLOCK) tile_fresh(w, uid, 1, w1 + (size_t)(t1 - 1u) * 32u);
    }

    /* point changes: 8 trials; a weight moves by -16..16, a bias by 16 times that */
    const u32 rate = neuro_scale(w, mother, 0);      /* chance per trial out of 2^15 */
    const u64 n_w = ((u64)t1 + t2) * 32u, n_b = 8ull * n_hb + AZ_MOT;
    u32 r[4];
    for (u32 t = 0; t < 8u && rate; t++) {
        az_rand4(w, uid, AZ_RS_CLADE, 224u + t, r);
        if ((r[0] & 0x7FFFu) >= rate) continue;
        const u64 idx = (((u64)r[1] << 32) | r[2]) % (n_w + n_b);
        i32 d = (i32)(r[3] % 33u) - 16;
        if (d == 0) d = (r[3] >> 31) ? 1 : -1;
        if (idx < n_w) {
            i8 *wt = idx < (u64)t1 * 32u ? BR_AT(i8, c, off_w1) + idx : BR_AT(i8, c, off_w2) + (idx - (u64)t1 * 32u);
            *wt = (i8)az_clamp_i32((i32)*wt + d, -127, 127);
        } else {
            const u64 bi = idx - n_w;
            i32 *bp = bi < 8ull * n_hb ? BR_AT(i32, c, off_b1) + bi : BR_AT(i32, c, off_b2) + (bi - 8ull * n_hb);
            *bp = az_clamp_i32(*bp + d * 16, -(1 << 20), 1 << 20);
        }
    }
    if (rate) {                                      /* rarely, a layer's scale shifts by one bit */
        az_rand4(w, uid, AZ_RS_CLADE, 232u, r);
        if ((r[0] & 0x3FFFFFu) < rate) {
            u8 *sh = (r[1] & 1u) ? &c->shift2 : &c->shift1;
            const i32 v = (i32)*sh + ((r[1] & 2u) ? 1 : -1);
            *sh = (u8)az_clamp_i32(v, 1, 15);
        }
    }
    *brain_bytes = (u32)az_brain_bytes(n_hb, t1, t2);
    *mind_bytes = az_mind_bytes(n_hb);
}

/* ---------------------------------------------------------------------- */
/* the lineage table                                                       */
/* ---------------------------------------------------------------------- */
/* Records are reused: a released one (alive == 0) is all zero bytes, so the
 * state hash over the table never sees stale data. Allocation takes the
 * lowest free record, which is deterministic because births are committed in
 * canonical order. cfg.max_clades is the capacity of the table (memory, set
 * at run time). */
static u32 clade_slot(AzWorld *w)
{
    for (u32 i = 0; i < w->n_clades; i++)
        if (!w->clades[i].alive) return i;
    if (w->n_clades >= w->cfg.max_clades) return AZ_NONE;
    return w->n_clades++;
}

/* Returns 0 when the table is full: index 0 is always a valid lineage then,
 * so a caller that ignores the limit stays memory-safe. */
u32 az_clade_found(AzWorld *w, u32 seed_id, u32 species)
{
    u32 idx = clade_slot(w);
    if (idx == AZ_NONE) return 0;
    AzClade *c = &w->clades[idx];
    az_memset(c, 0, sizeof(*c));
    u32 r[4];
    rand4_t0(w, seed_id, AZ_RS_CLADE, 130u, r);
    for (u32 i = 0; i < 16u; i++) c->body[i] = (u8)(r[i >> 2] >> (8u * (i & 3u)));
    c->body[AZ_B_RESERVED] = 0;
    c->alive = 1;
    c->founder_uid = seed_id;
    c->parent_uid = 0;
    c->born_tick = w->tick;
    c->species = species;
    return idx;
}

void az_clade_assign(AzWorld *w, u32 child, u32 mother)
{
    u32 cl = w->clade[mother];
    u32 cb, mb;
    const AzBrain *c = az_brain(w, child, &cb), *m = az_brain(w, mother, &mb);
    if (cb >= sizeof(AzBrain) && mb >= sizeof(AzBrain) &&
        (c->n_hb != m->n_hb || c->n_t1 != m->n_t1 || c->n_t2 != m->n_t2)) {
        const u32 idx = clade_slot(w);
        if (idx != AZ_NONE) {
            const AzClade *p = &w->clades[cl];
            AzClade *n = &w->clades[idx];
            az_memset(n, 0, sizeof(*n));
            az_memcpy(n->body, p->body, sizeof n->body);
            /* the new line looks a little different: its hue and its glyph drift */
            const u64 h = az_mix64(w->uid[child] ^ 0x636C616465ull);
            n->body[AZ_B_HUE] = (u8)(n->body[AZ_B_HUE] + (u8)(h & 31u) - 16u);
            n->body[AZ_B_GLYPH_B] = (u8)(n->body[AZ_B_GLYPH_B] ^ (u8)(1u << ((h >> 8) & 7u)));
            n->alive = 1;
            n->species = p->species;
            n->founder_uid = w->uid[child];
            n->parent_uid = p->founder_uid;
            n->born_tick = w->tick;
            az_emit(w, 0, AZ_EV_CLADE, 0, w->uid[child], p->founder_uid, 0);
            cl = idx;
        } else if (w->totals.clade_denied++ == 0) {
            az_emit(w, 0, AZ_EV_REFUSED, AZ_REF_CLADE, w->uid[child], 0, 0);
        }
    }
    w->clade[child] = cl;
    w->clades[cl].refcount++;
}

void az_clade_release(AzWorld *w, u32 clade)
{
    if (clade >= w->n_clades) return;
    AzClade *c = &w->clades[clade];
    if (!c->alive || c->refcount != 0) return;
    az_memset(c, 0, sizeof(*c));
}
