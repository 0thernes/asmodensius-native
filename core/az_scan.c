/* az_scan.c - neighbour scan over the 27 surrounding cells (fast version).
 *
 * Bit-identical, entity by entity, to core/stubs/az_scan.c, which stays the
 * definition of the semantics: canonical order (dz, dy, dx from -1 to +1;
 * inside a cell increasing uid), a neighbour counts if its squared distance
 * is <= range^2, only the first AZ_NEIGH_MAX such neighbours are used, and
 * "nearest" ties keep the earlier one. tests/test_scan.c compares the two.
 *
 * How it is faster:
 *   1. A range of sorted positions is cut into runs of one cell. The
 *      neighbourhood of that cell is gathered once for the whole run into a
 *      small structure-of-arrays buffer (position relative to the centre
 *      cell's origin, sorted position), in canonical order and in bounded
 *      pieces of FS_BUF records, so the memory use is fixed however crowded
 *      the cells are. The buffer is the calling worker's scratch block
 *      (az_ws_scan), not the stack: it is 8 KB, and no core frame may exceed
 *      2 KB (finding v39).
 *   2. Neighbour cells that no entity of the batch can reach are not read at
 *      all (a record in the cell at dx = -1 is at least x + 1 away).
 *   3. The range test runs eight records at a time and yields a bit mask;
 *      only the hits are looked at again.
 *   4. The per-hit work is branch-light: the class is an index, the contact
 *      and velocity terms are masked, and the one division per hit is
 *      replaced by an exact multiply with a per-entity reciprocal.
 *
 * Precondition (the AzPub contract): sub-cell coordinates are in
 * 0 .. AZ_SUB_ONE-1. Point 2 relies on it; az_grid_build guarantees it.
 */
#include "az_abi.h"

#define FS_BUF   512u   /* records per piece: 4 arrays * 4 bytes * 512 = 8 KB of worker scratch */
#define FS_CELLS 27u

_Static_assert(FS_BUF % 8u == 0, "pieces are tested eight records at a time");
_Static_assert(FS_BUF <= 65536u, "hit indices are stored in 16 bits");

typedef struct FsBuf {
    i32 rx[FS_BUF] AZ_ALIGNED(32);   /* position relative to the centre cell's origin */
    i32 ry[FS_BUF] AZ_ALIGNED(32);
    i32 rz[FS_BUF] AZ_ALIGNED(32);
    u32 q[FS_BUF];                   /* sorted position of the record                 */
} FsBuf;

_Static_assert(sizeof(FsBuf) <= AZ_WS_SCAN_BYTES, "a piece must fit the scan part of the worker scratch");

/* one neighbour cell that has to be read: its records and its origin */
typedef struct FsSrc {
    u32 q0, q1;
    i32 bx, by, bz;
} FsSrc;

/* cursor over the sources of one neighbourhood */
typedef struct FsIter {
    FsSrc src[FS_CELLS];
    u32 n_src;
    u32 i;      /* current source         */
    u32 q;      /* next record to gather  */
} FsIter;

AZ_INLINE u32 fs_range(const AzWorld *w, u32 slot)
{
    return ((u32)w->phen[(size_t)slot * AZ_PHEN + AZ_P_SENSE] + 1u) * (AZ_SUB_ONE / 256);
}

/* Which of the neighbour cell layers an entity can reach: bit 0 / 1 = x-1 /
 * x+1, bit 2 / 3 = y, bit 4 / 5 = z. A record in the layer below is at least
 * coordinate + 1 away along that axis, one in the layer above at least
 * AZ_SUB_ONE - coordinate. */
AZ_INLINE u32 fs_need(const AzPub *me, u32 range)
{
    u32 need = 0;
    need |= (u32)((u32)me->x < range) << 0;
    need |= (u32)((u32)AZ_SUB_ONE - (u32)me->x <= range) << 1;
    need |= (u32)((u32)me->y < range) << 2;
    need |= (u32)((u32)AZ_SUB_ONE - (u32)me->y <= range) << 3;
    need |= (u32)((u32)me->z < range) << 4;
    need |= (u32)((u32)AZ_SUB_ONE - (u32)me->z <= range) << 5;
    return need;
}

/* layer d (-1, 0, +1) of an axis whose two need bits are `bits` */
AZ_INLINE u32 fs_layer_ok(u32 bits, i32 d)
{
    return (u32)(d == 0) | ((bits >> (u32)(d > 0)) & (u32)(d != 0));
}

/* the non-empty neighbour cells that are needed, in canonical order */
static void fs_sources(const AzWorld *w, u32 c, u32 need, FsIter *it)
{
    /* the cells az_cell_nb(w, c, dx, dy, dz) names: x and y wrap, z does not */
    const u32 cx = az_cell_x(w, c), cy = az_cell_y(w, c), cz = az_cell_z(w, c);
    u32 n = 0;
    for (i32 dz = -1; dz <= 1; dz++) {
        const u32 z = cz + (u32)dz;
        if (z >= w->gz || !fs_layer_ok(need >> 4, dz)) continue;   /* z = -1 wraps to a huge value */
        for (i32 dy = -1; dy <= 1; dy++) {
            if (!fs_layer_ok(need >> 2, dy)) continue;
            const u32 row = az_cell_xyz(w, 0, (cy + (u32)dy) & (w->gy - 1), z);
            for (i32 dx = -1; dx <= 1; dx++) {
                if (!fs_layer_ok(need, dx)) continue;
                const u32 nb = row | ((cx + (u32)dx) & (w->gx - 1));
                u32 q0 = w->cell_start[nb], q1 = w->cell_start[nb + 1];
                if (q0 >= q1) continue;
                FsSrc *s = &it->src[n++];
                s->q0 = q0; s->q1 = q1;
                s->bx = dx * AZ_SUB_ONE; s->by = dy * AZ_SUB_ONE; s->bz = dz * AZ_SUB_ONE;
            }
        }
    }
    it->n_src = n;
    it->i = 0;
    it->q = n ? it->src[0].q0 : 0;
}

/* Gather the next piece (up to FS_BUF records). Returns the record count;
 * the buffer is zero-padded to a multiple of eight. */
static u32 fs_fill(const AzWorld *w, FsIter *it, FsBuf *b)
{
    u32 n = 0;
    u32 i = it->i, q = it->q;
    while (i < it->n_src && n < FS_BUF) {
        const FsSrc *s = &it->src[i];
        u32 take = s->q1 - q;
        if (take > FS_BUF - n) take = FS_BUF - n;
        const AzPub *o = &w->pub[q];
        const i32 bx = s->bx, by = s->by, bz = s->bz;
        for (u32 t = 0; t < take; t++) {
            b->rx[n + t] = (i32)o[t].x + bx;
            b->ry[n + t] = (i32)o[t].y + by;
            b->rz[n + t] = (i32)o[t].z + bz;
            b->q[n + t] = q + t;
        }
        n += take;
        q += take;
        if (q == s->q1) {
            i++;
            if (i < it->n_src) q = it->src[i].q0;
        }
    }
    it->i = i;
    it->q = q;
    for (u32 t = n; t < ((n + 7u) & ~7u); t++) { b->rx[t] = 0; b->ry[t] = 0; b->rz[t] = 0; b->q[t] = 0; }
    return n;
}

/* ---- the range test, eight records at a time --------------------------- */
/* Result: bit 4*i is set when record i is within range (d2 <= r2). The
 * arithmetic is the stub's: 32-bit, wrapping. */

#if defined(__AVX2__) && !defined(AZ_SCAN_PORTABLE)

typedef u32  FsV8  __attribute__((vector_size(32), aligned(4), may_alias));
typedef char FsV32 __attribute__((vector_size(32), may_alias));

typedef struct FsKey { FsV8 x, y, z, r2; } FsKey;

AZ_INLINE FsV8 fs_splat(u32 v) { return (FsV8){ v, v, v, v, v, v, v, v }; }

AZ_INLINE FsKey fs_key(u32 mx, u32 my, u32 mz, u32 r2)
{
    FsKey k = { fs_splat(mx), fs_splat(my), fs_splat(mz), fs_splat(r2) };
    return k;
}

AZ_INLINE u32 fs_mask8(const FsBuf *b, u32 j, const FsKey *k)
{
    FsV8 ox = *(const FsV8 *)&b->rx[j] - k->x;
    FsV8 oy = *(const FsV8 *)&b->ry[j] - k->y;
    FsV8 oz = *(const FsV8 *)&b->rz[j] - k->z;
    FsV8 d2 = ox * ox + oy * oy + oz * oz;
    return (u32)__builtin_ia32_pmovmskb256((FsV32)(d2 <= k->r2)) & 0x11111111u;
}

#else

typedef struct FsKey { u32 x, y, z, r2; } FsKey;

AZ_INLINE FsKey fs_key(u32 mx, u32 my, u32 mz, u32 r2)
{
    FsKey k = { mx, my, mz, r2 };
    return k;
}

AZ_INLINE u32 fs_mask8(const FsBuf *b, u32 j, const FsKey *k)
{
    u32 m = 0;
    for (u32 i = 0; i < 8; i++) {
        u32 ox = (u32)b->rx[j + i] - k->x;
        u32 oy = (u32)b->ry[j + i] - k->y;
        u32 oz = (u32)b->rz[j + i] - k->z;
        u32 d2 = ox * ox + oy * oy + oz * oz;
        m |= (u32)(d2 <= k->r2) << (4u * i);
    }
    return m;
}

#endif

/* Indices (into the piece) of the records within range of one entity, in
 * canonical order, the entity itself left out, at most `room` of them.
 * `hits` must hold room + 8 entries. */
static u32 fs_hits(const FsBuf *b, u32 n, const AzPub *me, u32 r2, u32 me_p, u32 room, u16 *hits)
{
    const FsKey key = fs_key(me->x, me->y, me->z, r2);
    u32 nh = 0;
    for (u32 j = 0; j < n && nh < room; j += 8) {
        u32 m = fs_mask8(b, j, &key);
        if (n - j < 8u) m &= (1u << (4u * (n - j))) - 1u;   /* the zero padding is not a record */
        while (m) {
            u32 t = j + ((u32)__builtin_ctz(m) >> 2);
            m &= m - 1u;
            hits[nh] = (u16)t;
            nh += (u32)(b->q[t] != me_p);
        }
    }
    return nh < room ? nh : room;
}

/* ---- signal attenuation -------------------------------------------------- */
/* floor((diff << 8) / r2) for diff <= r2 and 2^12 <= r2 <= 2^28 (every sense
 * range), without a division per neighbour: with inv = floor(2^39 / r2) the
 * product (diff << 8) * inv fits 64 bits and its top part is the quotient or
 * one less; one compare of the remainder settles which. */
#define FS_INV_BITS 39

AZ_INLINE u64 fs_inv(u32 r2) { return ((u64)1 << FS_INV_BITS) / r2; }

AZ_INLINE u32 fs_att(u32 diff, u32 r2, u64 inv)
{
    u64 num = (u64)diff << 8;
    u64 qe = (num * inv) >> FS_INV_BITS;
    u64 rem = num - qe * r2;
    return (u32)qe + (u32)(rem >= r2);
}

/* ---- the percept --------------------------------------------------------- */

static void fs_clear(AzPercept *p)
{
    p->n_used = p->n_touch = p->n_kin = p->n_col = p->n_prey = p->n_threat = 0;
    for (u32 a = 0; a < 3; a++) {
        p->kin[a] = p->col[a] = p->prey[a] = p->threat[a] = p->vel[a] = p->push[a] = 0;
        p->mate_off[a] = p->bond_off[a] = p->parent_off[a] = p->child_off[a] = 0;
    }
    p->sig_a = p->sig_b = 0;
    for (u32 a = 0; a < 3; a++) p->sigv_a[a] = p->sigv_b[a] = 0;
    p->near_prey = p->near_threat = p->near_kin = AZ_NONE;
    p->best_mate = p->bond = p->parent = p->child = AZ_NONE;
    p->faced_d2 = AZ_NONE; p->faced_stake = 0;
    p->d2_prey = p->d2_threat = p->d2_kin = 0xFFFFFFFFu;
    p->d2_mate = p->d2_bond = p->d2_parent = p->d2_child = 0xFFFFFFFFu;
}

/* classes as indices, in the order the percept stores them */
enum { FS_KIN = 0, FS_COL = 1, FS_PREY = 2, FS_THREAT = 3 };

/* four 32-bit lanes (generic vector code: any target) */
typedef u32 FsV4 __attribute__((vector_size(16)));

AZ_INLINE FsV4 fs_splat4(u32 v) { return (FsV4){ v, v, v, v }; }

/* Fold the hits of one piece into the percept of the entity at sorted
 * position me_p. Sums are kept unsigned so that wrap-around is defined
 * without relying on a compiler flag; the stored bits are the same. Kept out
 * of line: the loop needs every register it can get. */
AZ_NOINLINE static void fs_apply(const AzWorld *w, const FsBuf *b, const u16 *hits, u32 nh,
                       u32 me_p, u32 r2, AzPercept *p)
{
    const AzPub *me = &w->pub[me_p];
    const u32 slot = w->order[me_p];
    const u32 mx = me->x, my = me->y, mz = me->z;
    const u32 my_size = AZ_PUB_SIZE(me->rf);
    const u32 my_r = az_radius_ns(my_size);
    const u32 my_col = me->colony;
    const AzPubId *mid = &w->pub_id[me_p];
    const u64 my_uid = mid->uid, my_pa = mid->parent_a, my_pb = mid->parent_b, my_partner = mid->partner;
    const u16 my_tag = me->tag;
    /* the kin lesion is data: a window nothing can be inside */
    const i32 kin_window = (w->cfg.lesion & AZ_LESION_KIN) ? -1
                         : (i32)w->phen[(size_t)slot * AZ_PHEN + AZ_P_KIN];
    const u64 inv = fs_inv(r2);

    /* per class: the offset sum in lanes 0..2 and the count in lane 3 */
    FsV4 acc[4];
    acc[FS_KIN]    = (FsV4){ (u32)p->kin[0],    (u32)p->kin[1],    (u32)p->kin[2],    p->n_kin };
    acc[FS_COL]    = (FsV4){ (u32)p->col[0],    (u32)p->col[1],    (u32)p->col[2],    p->n_col };
    acc[FS_PREY]   = (FsV4){ (u32)p->prey[0],   (u32)p->prey[1],   (u32)p->prey[2],   p->n_prey };
    acc[FS_THREAT] = (FsV4){ (u32)p->threat[0], (u32)p->threat[1], (u32)p->threat[2], p->n_threat };
    /* contact: the push in lanes 0..2 and the touch count in lane 3 */
    FsV4 push = { (u32)p->push[0], (u32)p->push[1], (u32)p->push[2], p->n_touch };
    FsV4 vel = { (u32)p->vel[0], (u32)p->vel[1], (u32)p->vel[2], 0 };
    /* nearest per kind: kin and colony-mates share one, then prey, then threat */
    u32 nd2[3] = { p->d2_kin, p->d2_prey, p->d2_threat };
    u32 nsl[3] = { p->near_kin, p->near_prey, p->near_threat };
    u32 sig_a = p->sig_a, sig_b = p->sig_b;

    for (u32 h = 0; h < nh; h++) {
        const u32 j = hits[h];
        const u32 q = b->q[j];
        const AzPub *o = &w->pub[q];
        const u32 oslot = w->order[q];
        const u32 ox = (u32)b->rx[j] - mx, oy = (u32)b->ry[j] - my, oz = (u32)b->rz[j] - mz;
        const u32 d2 = ox * ox + oy * oy + oz * oz;
        const i32 ahead = me->face[0]*(i32)ox + me->face[1]*(i32)oy + me->face[2]*(i32)oz;
        if (ahead > 0 && d2 < p->faced_d2) { p->faced_d2 = d2; p->faced_stake = o->stake; }
        const u32 rf = o->rf;
        const u32 osz = AZ_PUB_SIZE(rf);

        /* a contact subtracts off >> 2 from the push and -1 from the count lane */
        const u32 touch = my_r + az_radius_ns(osz);
        const u32 tm = 0u - (u32)(d2 < touch * touch);
        const FsV4 pv = { (u32)((i32)ox >> 2), (u32)((i32)oy >> 2), (u32)((i32)oz >> 2), 0xFFFFFFFFu };
        push -= pv & fs_splat4(tm);

        /* signal intensity falls off linearly in squared distance: 256 at 0, 0 at the range */
        const u32 att = fs_att(r2 - d2, r2, inv);
        const i32 got_a = (i32)(((u32)o->sig_a * att) >> 8), got_b = (i32)(((u32)o->sig_b * att) >> 8);
        sig_a += (u32)got_a;
        sig_b += (u32)got_b;
        p->sigv_a[0] += ((i32)ox >> 6) * got_a; p->sigv_a[1] += ((i32)oy >> 6) * got_a; p->sigv_a[2] += ((i32)oz >> 6) * got_a;
        p->sigv_b[0] += ((i32)ox >> 6) * got_b; p->sigv_b[1] += ((i32)oy >> 6) * got_b; p->sigv_b[2] += ((i32)oz >> 6) * got_b;

        const u32 same_col = (u32)(my_col != 0) & (u32)(o->colony == my_col);
        const u32 kin = (u32)((i32)az_tag_dist(my_tag, o->tag) <= kin_window);
        const u32 threat = (u32)((rf & AZ_PF_PASSIVE) == 0) & (u32)(osz > my_size);
        /* col beats kin beats threat; everything else is prey */
        const u32 cls = same_col | (((same_col | kin) ^ 1u) * (2u + threat));
        const FsV4 offv = { ox, oy, oz, 1u };
        acc[cls] += offv;
        const FsV4 vv = { (u32)(i32)o->vx, (u32)(i32)o->vy, (u32)(i32)o->vz, 0 };
        vel += vv & fs_splat4(0u - (u32)(cls < 2u));
        const u32 ni = (cls >> 1) * (cls - 1u);   /* kin, col -> 0; prey -> 1; threat -> 2 */
        const u32 closer = (u32)(d2 < nd2[ni]);
        nd2[ni] = closer ? d2 : nd2[ni];
        nsl[ni] = closer ? oslot : nsl[ni];

        if ((rf & (AZ_PF_RECEPT | AZ_PF_PASSIVE | AZ_PF_JUV)) == AZ_PF_RECEPT && d2 < p->d2_mate) {
            p->d2_mate = d2; p->best_mate = oslot;
            p->mate_off[0] = (i32)ox; p->mate_off[1] = (i32)oy; p->mate_off[2] = (i32)oz;
        }
        /* bond partner, parent, child: rare, so one well-predicted test guards them */
        const AzPubId *oid = &w->pub_id[q];
        const u64 ouid = oid->uid;
        if (AZ_UNLIKELY((ouid == my_partner) | (ouid == my_pa) | (ouid == my_pb)
                        | (oid->parent_a == my_uid) | (oid->parent_b == my_uid))) {
            if (my_partner != 0 && ouid == my_partner && d2 < p->d2_bond) {
                p->d2_bond = d2; p->bond = oslot;
                p->bond_off[0] = (i32)ox; p->bond_off[1] = (i32)oy; p->bond_off[2] = (i32)oz;
            }
            if ((my_pa != 0 && ouid == my_pa) || (my_pb != 0 && ouid == my_pb)) {
                if (d2 < p->d2_parent) {
                    p->d2_parent = d2; p->parent = oslot;
                    p->parent_off[0] = (i32)ox; p->parent_off[1] = (i32)oy; p->parent_off[2] = (i32)oz;
                }
            }
            if ((oid->parent_a == my_uid || oid->parent_b == my_uid) && d2 < p->d2_child) {
                p->d2_child = d2; p->child = oslot;
                p->child_off[0] = (i32)ox; p->child_off[1] = (i32)oy; p->child_off[2] = (i32)oz;
            }
        }
    }

    for (u32 a = 0; a < 3; a++) {
        p->kin[a] = (i32)acc[FS_KIN][a];   p->col[a] = (i32)acc[FS_COL][a];
        p->prey[a] = (i32)acc[FS_PREY][a]; p->threat[a] = (i32)acc[FS_THREAT][a];
        p->vel[a] = (i32)vel[a];
        p->push[a] = (i32)push[a];
    }
    p->n_touch = (u16)push[3];
    p->n_kin = (u16)acc[FS_KIN][3];   p->n_col = (u16)acc[FS_COL][3];
    p->n_prey = (u16)acc[FS_PREY][3]; p->n_threat = (u16)acc[FS_THREAT][3];
    p->d2_kin = nd2[0];   p->d2_prey = nd2[1];   p->d2_threat = nd2[2];
    p->near_kin = nsl[0]; p->near_prey = nsl[1]; p->near_threat = nsl[2];
    p->sig_a = sig_a; p->sig_b = sig_b;
}

/* The entities at sorted positions base .. base+count-1, all of cell c. */
static void fs_run(const AzWorld *w, u32 c, u32 base, u32 count, AzPercept *out, FsBuf *buf)
{
    u32 need = 0;
    for (u32 k = 0; k < count; k++) {
        fs_clear(&out[k]);
        need |= fs_need(&w->pub[base + k], fs_range(w, w->order[base + k]));
    }

    FsIter it;
    u16 hits[AZ_NEIGH_MAX + 8];
    fs_sources(w, c, need, &it);
    while (it.i < it.n_src) {
        const u32 n = fs_fill(w, &it, buf);
        u32 open = 0;   /* entities that can still take neighbours */
        for (u32 k = 0; k < count; k++) {
            AzPercept *p = &out[k];
            const u32 used = p->n_used;
            if (used >= AZ_NEIGH_MAX) continue;
            const u32 me_p = base + k;
            const u32 range = fs_range(w, w->order[me_p]);
            const u32 r2 = range * range;
            const u32 nh = fs_hits(buf, n, &w->pub[me_p], r2, me_p, AZ_NEIGH_MAX - used, hits);
            if (nh) {
                fs_apply(w, buf, hits, nh, me_p, r2, p);
                p->n_used = (u16)(used + nh);
            }
            open += (u32)(used + nh < AZ_NEIGH_MAX);
        }
        if (!open) break;
    }
}

void az_scan_range(const AzWorld *w, u32 p0, u32 count, AzPercept *out, u32 worker)
{
    FsBuf *buf = (FsBuf *)az_ws_scan(w, worker);
    u32 k = 0;
    while (k < count) {
        /* one run = the part of the range that lies in one cell */
        const u32 p = p0 + k;
        const u32 c = w->cell[w->order[p]];
        u32 run = w->cell_start[c + 1] - p;
        if (run > count - k) run = count - k;
        fs_run(w, c, p, run, &out[k], buf);
        k += run;
    }
}

u32 az_scan_list(const AzWorld *w, u32 slot, i32 (*off)[3], u32 *oslot, u32 max, u32 worker)
{
    const u32 c = w->cell[slot];
    const u32 me_p = w->spos[slot];
    if (c == AZ_NONE || me_p == AZ_NONE) return 0;
    const AzPub *me = &w->pub[me_p];
    const u32 range = fs_range(w, slot);
    const u32 r2 = range * range;
    if (max > AZ_NEIGH_MAX) max = AZ_NEIGH_MAX;

    FsIter it;
    FsBuf *buf = (FsBuf *)az_ws_scan(w, worker);
    u16 hits[AZ_NEIGH_MAX + 8];
    u32 n = 0;
    fs_sources(w, c, fs_need(me, range), &it);
    while (it.i < it.n_src && n < max) {
        const u32 len = fs_fill(w, &it, buf);
        const u32 nh = fs_hits(buf, len, me, r2, me_p, max - n, hits);
        for (u32 h = 0; h < nh; h++) {
            const u32 j = hits[h];
            off[n][0] = (i32)((u32)buf->rx[j] - (u32)me->x);
            off[n][1] = (i32)((u32)buf->ry[j] - (u32)me->y);
            off[n][2] = (i32)((u32)buf->rz[j] - (u32)me->z);
            oslot[n] = w->order[buf->q[j]];
            n++;
        }
    }
    return n;
}
