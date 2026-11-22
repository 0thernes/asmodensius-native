/* az_snapshot.c - exact save and restore, generated from the column registry.
 *
 * A snapshot is taken between ticks and holds everything the next tick reads:
 *
 *   header    magic, ABI version, format version, total size, the sizes of
 *             the structs stored raw, counts, the AzConfig, and one 16-byte
 *             record per saved column: hash of its name, element size,
 *             elements per entity or cell, and how it is stored. A layout
 *             change in az_columns.def or az_cells.def fails the load.
 *   scalars   tick, uid_next, key, role_key, ledger chain and count,
 *             sun_period, gravity, vents, gstate, totals, the genetic code,
 *             the world parameters
 *   entities  every HOT and WARM column for slots 0 .. n_slots-1 (dead slots
 *             included: they are part of the matter audit until a reorder)
 *   fields    every FIELD cell column
 *   tables    clades, colonies
 *   store     the creature store: for every slot the size of its block in
 *             units (0 = none), then the blocks themselves in slot order.
 *             The handle column is not saved: a load packs the blocks in
 *             slot order and rebuilds the handles.
 *   pending   the cell accumulator `acc`: a module that posts after the last
 *             field commit of a tick (life commit, Tier 1, Tier 2) leaves
 *             matter, heat or marks there that the next tick applies and that
 *             both audits count. Stored sparsely as (cell, record), so a
 *             world with nothing pending pays 8 bytes.
 *   checksum  64 bits over everything before it
 *
 * Entity EPH columns are never saved: a reorder does not move them, so no
 * module may carry them across a tick. The free list, which lives in the EPH
 * column spos, is rebuilt on load exactly as az_grid_build defines it. The
 * cell demand and grant scratch is rewritten by every tick before it is read and is
 * not saved either.
 *
 * Every section is padded to 8 bytes. Integers are little-endian; structs
 * and columns are copied as they lie in memory (az_abi.h asserts the byte
 * order and that saved structs have no implicit padding).
 */
#include "az_abi.h"

#define SNAP_MAGIC      0x343050414E535A41ull   /* the bytes "AZSNAP04" */
#define SNAP_FORMAT     9u /* 9 adds the persistent travel carry and five physics parameters */
#define SNAP_V5_V8_PARAMS 235u /* all four historical writers used these descriptor IDs */
_Static_assert(AZ_WP_PHYS_BUOY_SHIFT == SNAP_V5_V8_PARAMS && AZ_WP_PHYS_ANG_RETAIN == 239,
               "snapshot physics parameters must be appended after the historical schema");
#define SNAP_HEAD_FIXED 80u                     /* bytes before the config */
#define SNAP_COL_REC    16u
#define SNAP_NVENT      64u
#define SNAP_MAX_COLS   (2u * AZ_MAX_COLS)
#define SNAP_SCALARS    64u                     /* fixed scalars before the vent table */

_Static_assert(sizeof(((AzWorld *)0)->vent_cell) == SNAP_NVENT * 4u, "vent table size is part of the format");

enum { SNAP_K_ENTITY = 1, SNAP_K_FIELD = 2, SNAP_K_PENDING = 3 };

typedef struct SnapCol {
    u8  *data;
    u64  items;     /* entities or cells held for this world */
    u64  width;     /* bytes per item = elem * per */
    u64  hash;      /* of the column name */
    u32  elem;      /* bytes per element */
    u32  per;       /* elements per entity or cell */
    u32  kind;      /* SNAP_K_* */
} SnapCol;

/* The plan names its columns by registry index, two bytes each; snap_col
 * reads one description back when it is needed. A table of full SnapCol
 * records was 9 KB of stack in save, load and bytes (finding v39). */
typedef struct SnapPlan {
    u8  idx[SNAP_MAX_COLS];     /* index in the entity or the cell registry      */
    u8  kind[SNAP_MAX_COLS];    /* SNAP_K_*                                      */
    u32 n;
    u32 n_slots;                /* entities held: the world's, or the file's     */
} SnapPlan;

_Static_assert(AZ_MAX_COLS <= 256u, "a registry index must fit one byte of the plan");

/* ---------------------------------------------------------------------- */
/* little-endian put and get                                               */
/* ---------------------------------------------------------------------- */
static u8 *put8(u8 *p, u32 v)  { p[0] = (u8)v; return p + 1; }
static u8 *put16(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); return p + 2; }
static u8 *put32(u8 *p, u32 v)
{
    p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24);
    return p + 4;
}
static u8 *put64(u8 *p, u64 v) { return put32(put32(p, (u32)v), (u32)(v >> 32)); }

static u32 get16(const u8 *p) { return (u32)p[0] | ((u32)p[1] << 8); }
static u32 get32(const u8 *p)
{
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}
static u64 get64(const u8 *p) { return (u64)get32(p) | ((u64)get32(p + 4) << 32); }

static u64 pad8(u64 n) { return (n + 7u) & ~(u64)7u; }

/* copy n bytes and zero the padding up to the next multiple of 8 */
static u8 *put_raw(u8 *p, const void *src, u64 n)
{
    az_memcpy(p, src, (size_t)n);
    for (u64 i = n; i < pad8(n); i++) p[i] = 0;
    return p + pad8(n);
}

static bool snap_all_zero(const u8 *p, u64 n)
{
    u64 i = 0;
    for (; i + 8 <= n; i += 8) {
        u64 t;
        __builtin_memcpy(&t, p + i, 8);
        if (t) return false;
    }
    for (; i < n; i++) if (p[i]) return false;
    return true;
}

static bool snap_equal(const u8 *a, const u8 *b, u64 n)
{
    for (u64 i = 0; i < n; i++) if (a[i] != b[i]) return false;
    return true;
}

/* ---------------------------------------------------------------------- */
/* checksum                                                                */
/* ---------------------------------------------------------------------- */
/* Four interleaved chains of the 64-bit mixer over 8-byte words. Every step
 * is a bijection of the chain value, so a change confined to one word always
 * changes the result: a single corrupted byte can never pass. */
static u64 snap_sum(const u8 *p, u64 n)
{
    u64 lane[4] = { 0x243F6A8885A308D3ull ^ n, 0x13198A2E03707344ull, 0xA4093822299F31D0ull, 0x082EFA98EC4E6C89ull };
    const u64 words = n / 8;
    for (u64 i = 0; i < words; i++) {
        u64 t;
        __builtin_memcpy(&t, p + i * 8, 8);
        lane[i & 3] = az_mix64(lane[i & 3] ^ t) + 0x9E3779B97F4A7C15ull;
    }
    u64 acc = az_mix64(n);
    for (u32 k = 0; k < 4; k++) acc = az_mix64(acc ^ lane[k]) + 0x9E3779B97F4A7C15ull;
    return acc;
}

/* ---------------------------------------------------------------------- */
/* the plan: which columns are stored, in file order                       */
/* ---------------------------------------------------------------------- */
static u64 snap_name_hash(const char *s)
{
    u64 h = 0xCBF29CE484222325ull;                    /* FNV-1a, then mixed */
    for (u32 i = 0; i < 64 && s[i]; i++) h = (h ^ (u8)s[i]) * 0x100000001B3ull;
    return az_mix64(h);
}

static void snap_add(SnapPlan *pl, u32 k, u32 kind)
{
    if (pl->n >= SNAP_MAX_COLS) return;
    pl->idx[pl->n] = (u8)k;
    pl->kind[pl->n] = (u8)kind;
    pl->n++;
}

/* entry k of the plan, described from the registry */
static void snap_col(const AzWorld *w, const SnapPlan *pl, u32 k, SnapCol *c)
{
    AzColDesc d;
    const u32 kind = pl->kind[k];
    if (kind == SNAP_K_ENTITY) az_column(w, pl->idx[k], &d);
    else az_cell_column(w, pl->idx[k], &d);
    c->data = (u8 *)d.data;
    c->items = kind == SNAP_K_ENTITY ? pl->n_slots : w->n_cells;
    c->width = (u64)d.elem * d.per;
    c->hash = snap_name_hash(d.name);
    c->elem = d.elem;
    c->per = d.per;
    c->kind = kind;
}

static void snap_plan(const AzWorld *w, u32 n_slots, u32 format, SnapPlan *pl)
{
    AzColDesc d;
    pl->n = 0;
    pl->n_slots = n_slots;
    u32 n = az_n_columns();
    for (u32 k = 0; k < n; k++) {
        az_column(w, k, &d);
        if (format < 9u && d.data == (const void *)w->travel_carry) continue;
        if (d.tier == AZ_TIER_HOT || d.tier == AZ_TIER_WARM) snap_add(pl, k, SNAP_K_ENTITY);
    }
    n = az_n_cell_columns();
    for (u32 k = 0; k < n; k++) {
        az_cell_column(w, k, &d);
        if (d.tier == AZ_TIER_FIELD) snap_add(pl, k, SNAP_K_FIELD);
    }
    /* Every cell accumulator except demand, grants and the derived pressure cache may hold a value at a tick
     * boundary. One added to az_cells.def later is carried by default:
     * never lose state. */
    for (u32 k = 0; k < n; k++) {
        az_cell_column(w, k, &d);
        if (d.tier != AZ_TIER_EPH || az_cell_scratch(w, d.data)) continue;
        if (format < 8u && d.data == (const void *)w->c_species_carry) continue;
        snap_add(pl, k, SNAP_K_PENDING);
    }
}

/* index of the first item at or after `from` that is not all zero, or c->items */
static u64 snap_next(const SnapCol *c, u64 from)
{
    for (u64 i = from; i < c->items; i++)
        if (!snap_all_zero(c->data + i * c->width, c->width)) return i;
    return c->items;
}

static u64 snap_scalar_bytes(void)
{
    return SNAP_SCALARS + SNAP_NVENT * 4 + pad8(sizeof(((AzWorld *)0)->gstate)) +
           pad8(sizeof(AzTotals)) + pad8(sizeof(AzGeneticCode)) + pad8(sizeof(((AzWorld *)0)->param));
}

static u64 snap_head_bytes(u32 n_cols)
{
    return SNAP_HEAD_FIXED + pad8(sizeof(AzConfig)) + (u64)n_cols * SNAP_COL_REC;
}

/* A worker tally that is not merged yet means the world is inside a tick. */
static bool snap_quiescent(const AzWorld *w)
{
    for (u32 k = 0; k < AZ_MAX_WORKERS; k++)
        if (!snap_all_zero((const u8 *)&w->workers[k].tally, sizeof(AzTally))) return false;
    /* colony deposits and withdrawals that no commit applied are not saved:
     * refuse the world rather than lose them (finding v40) */
    for (u32 i = 0; i < w->n_colonies; i++) {
        const AzColonyEph *e = &w->colony_eph[i];
        if (!snap_all_zero((const u8 *)e->dep, sizeof e->dep) ||
            !snap_all_zero((const u8 *)e->taken, sizeof e->taken) || e->dep_e || e->taken_e) return false;
    }
    return true;
}

static u64 snap_total(const AzWorld *w, const SnapPlan *pl)
{
    u64 n = snap_head_bytes(pl->n) + snap_scalar_bytes();
    for (u32 k = 0; k < pl->n; k++) {
        SnapCol col;
        const SnapCol *c = &col;
        snap_col(w, pl, k, &col);
        if (c->kind != SNAP_K_PENDING) { n += pad8(c->items * c->width); continue; }
        u64 found = 0;
        for (u64 i = snap_next(c, 0); i < c->items; i = snap_next(c, i + 1)) found++;
        n += 8 + found * (8 + pad8(c->width));
    }
    n += pad8((u64)w->n_clades * sizeof(AzClade)) + pad8((u64)w->n_colonies * sizeof(AzColony));
    n += pad8((u64)w->n_slots * 4u);
    for (u32 s = 0; s < w->n_slots; s++)
        if (w->blk[s]) n += (u64)az_blk(w, s)->units * AZ_UNIT;
    return n + 8;
}

/* ---------------------------------------------------------------------- */
/* save                                                                    */
/* ---------------------------------------------------------------------- */
u64 az_snapshot_bytes(const AzWorld *w)
{
    SnapPlan pl;
    if (!w || !snap_quiescent(w)) return 0;
    snap_plan(w, w->n_slots, SNAP_FORMAT, &pl);
    return snap_total(w, &pl);
}

u64 az_snapshot_save(const AzWorld *w, u8 *buf, u64 cap)
{
    SnapPlan pl;
    if (!w || !buf || !snap_quiescent(w)) return 0;
    snap_plan(w, w->n_slots, SNAP_FORMAT, &pl);
    const u64 total = snap_total(w, &pl);
    if (cap < total) return 0;

    u8 *p = buf;
    p = put64(p, SNAP_MAGIC);                          /* offset  0 */
    p = put32(p, AZ_ABI_VERSION);
    p = put32(p, SNAP_FORMAT);
    p = put64(p, total);                               /* offset 16 */
    p = put32(p, (u32)sizeof(AzConfig));               /* offset 24 */
    p = put32(p, (u32)sizeof(AzClade));
    p = put32(p, (u32)sizeof(AzColony));
    p = put32(p, (u32)sizeof(AzTotals));
    p = put32(p, (u32)sizeof(AzGeneticCode));
    p = put32(p, AZ_NGLOBAL);
    p = put32(p, SNAP_NVENT);
    p = put32(p, pl.n);                                /* offset 52 */
    p = put32(p, w->n_slots);                          /* offset 56 */
    p = put32(p, w->n_cells);
    p = put32(p, w->n_clades);
    p = put32(p, w->n_colonies);
    p = put32(p, w->n_live);
    p = put32(p, AZ_WP_COUNT);                         /* offset 76 */
    p = put_raw(p, &w->cfg, sizeof(AzConfig));         /* offset 80 */
    for (u32 k = 0; k < pl.n; k++) {
        SnapCol col;
        snap_col(w, &pl, k, &col);
        p = put64(p, col.hash);
        p = put32(p, col.elem);
        p = put16(p, col.per);
        p = put8(p, col.kind);
        p = put8(p, 0);
    }

    p = put64(p, w->tick);
    p = put64(p, w->uid_next);
    p = put32(p, w->key[0]);
    p = put32(p, w->key[1]);
    p = put64(p, w->role_key);
    p = put64(p, w->ledger_chain);
    p = put64(p, w->ledger_count);
    p = put32(p, w->sun_period[0]);
    p = put32(p, w->sun_period[1]);
    p = put16(p, w->gravity);
    p = put16(p, w->vent_count);
    p = put32(p, 0);
    p = put_raw(p, w->vent_cell, sizeof(w->vent_cell));
    p = put_raw(p, w->gstate, sizeof(w->gstate));
    p = put_raw(p, &w->totals, sizeof(AzTotals));
    p = put_raw(p, &w->code, sizeof(AzGeneticCode));
    p = put_raw(p, w->param, sizeof(w->param));

    for (u32 k = 0; k < pl.n; k++) {
        SnapCol col;
        snap_col(w, &pl, k, &col);
        if (col.kind != SNAP_K_PENDING) p = put_raw(p, col.data, col.items * col.width);
    }
    p = put_raw(p, w->clades, (u64)w->n_clades * sizeof(AzClade));
    p = put_raw(p, w->colonies, (u64)w->n_colonies * sizeof(AzColony));

    for (u32 s = 0; s < w->n_slots; s++) p = put32(p, w->blk[s] ? az_blk(w, s)->units : 0u);
    if (w->n_slots & 1u) p = put32(p, 0);
    for (u32 s = 0; s < w->n_slots; s++)
        if (w->blk[s]) p = put_raw(p, az_blk(w, s), (u64)az_blk(w, s)->units * AZ_UNIT);

    for (u32 k = 0; k < pl.n; k++) {
        SnapCol col;
        const SnapCol *c = &col;
        snap_col(w, &pl, k, &col);
        if (c->kind != SNAP_K_PENDING) continue;
        u8 *at_count = p;
        u64 found = 0;
        p += 8;
        for (u64 i = snap_next(c, 0); i < c->items; i = snap_next(c, i + 1)) {
            p = put64(p, i);
            p = put_raw(p, c->data + i * c->width, c->width);
            found++;
        }
        put64(at_count, found);
    }

    if ((u64)(p - buf) + 8 != total) return 0;         /* the two passes must agree */
    put64(p, snap_sum(buf, total - 8));
    return total;
}

/* ---------------------------------------------------------------------- */
/* load                                                                    */
/* ---------------------------------------------------------------------- */

/* Check the pending sections, which start at `at` and must end exactly at
 * `end`. Entries are canonical (ascending cell, never an all-zero record),
 * so a world has exactly one encoding. */
static bool snap_pending_ok(const AzWorld *w, const SnapPlan *pl, const u8 *buf, u64 at, u64 end)
{
    for (u32 k = 0; k < pl->n; k++) {
        SnapCol col;
        const SnapCol *c = &col;
        snap_col(w, pl, k, &col);
        if (c->kind != SNAP_K_PENDING) continue;
        const u64 rec = 8 + pad8(c->width);
        if (at + 8 > end) return false;
        u64 found = get64(buf + at);
        at += 8;
        if (found > c->items || found > (end - at) / rec) return false;
        u64 prev = 0;
        for (u64 i = 0; i < found; i++, at += rec) {
            u64 idx = get64(buf + at);
            if (idx >= c->items || (i && idx <= prev)) return false;
            if (snap_all_zero(buf + at + 8, c->width)) return false;
            /* Versions 5 and 6 defined these four bytes as zero padding;
             * a distinct nonzero high limb is real state only from v7. */
            if (get32(buf + 12) < 7u && c->data == (const u8 *)w->c_acc &&
                get32(buf + at + 8 + offsetof(AzCellAcc, heat_carry)) != 0) return false;
            prev = idx;
        }
    }
    return at == end;
}

/* Everything the bulk copy relies on, checked against the new world. Returns
 * the offset of the pending sections, or 0 if the snapshot does not fit. */
static u64 snap_fits(const AzWorld *w, const SnapPlan *pl, const u8 *buf, u64 n)
{
    const u32 n_slots = get32(buf + 56), n_clades = get32(buf + 64), n_colonies = get32(buf + 68);
    /* the file holds a config that az_world_create has already normalised */
    if (!snap_equal((const u8 *)&w->cfg, buf + SNAP_HEAD_FIXED, sizeof(AzConfig))) return 0;
    if (n_slots > w->cap || get32(buf + 60) != w->n_cells) return 0;
    if (n_clades > w->cfg.max_clades || n_colonies < 1 || n_colonies > w->cfg.max_colonies) return 0;
    if (get32(buf + 72) > n_slots || get32(buf + 52) != pl->n) return 0;

    u64 at = snap_head_bytes(pl->n) + snap_scalar_bytes();
    const u8 *r = buf + SNAP_HEAD_FIXED + pad8(sizeof(AzConfig));
    for (u32 k = 0; k < pl->n; k++, r += SNAP_COL_REC) {
        SnapCol col;
        const SnapCol *c = &col;
        snap_col(w, pl, k, &col);
        if (get64(r) != c->hash || get32(r + 8) != c->elem || get16(r + 12) != c->per ||
            r[14] != c->kind || r[15] != 0) return 0;
        if (c->kind != SNAP_K_PENDING) at += pad8(c->items * c->width);
    }
    at += pad8((u64)n_clades * sizeof(AzClade)) + pad8((u64)n_colonies * sizeof(AzColony));
    /* the creature store: the size table, then that many units */
    if (at + pad8((u64)n_slots * 4u) + 8 > n) return 0;
    if ((n_slots & 1u) && get32(buf + at + (u64)n_slots * 4u)) return 0;
    u64 units = 0;
    for (u32 s = 0; s < n_slots; s++) units += get32(buf + at + (u64)s * 4u);
    if (units + 1u > w->store_units) return 0;
    at += pad8((u64)n_slots * 4u);
    if (units * AZ_UNIT > n - 8 - at) return 0;
    at += units * AZ_UNIT;
    if (at + 8 > n || !snap_pending_ok(w, pl, buf, at, n - 8)) return 0;
    return at;
}

/* Indices the tick uses without a range check must point inside their tables. */
static bool snap_indices_ok(const AzWorld *w)
{
    if (w->vent_count > SNAP_NVENT) return false;
    for (u32 i = 0; i < w->vent_count; i++) if (w->vent_cell[i] >= w->n_cells) return false;
    for (u32 s = 0; s < w->n_slots; s++)
        if (w->clade[s] >= w->cfg.max_clades || w->colony[s] >= w->cfg.max_colonies) return false;
    /* a brain the kernel will walk must be well formed */
    for (u32 s = 0; s < w->n_slots; s++) {
        u32 bb, mb;
        const AzBrain *b = az_brain(w, s, &bb);
        (void)az_seg(w, s, AZ_SEG_MIND, &mb);
        if (bb && !az_brain_ok(b, bb, mb)) return false;
    }
    for (u32 i = 0; i < AZ_LEN(w->param); i++) {
        const AzParamDesc *d = az_param_desc(i);
        if (d ? (w->param[i] < d->lo || w->param[i] > d->hi) : (w->param[i] != 0)) return false;
    }
    return true;
}

/* Per-tick scratch of a world between ticks: an alive slot has the cell of
 * its position and no sorted position (order[] and pub[] are empty until the
 * next az_grid_build); the dead slots form the free list exactly as
 * az_grid_build chains it, from the highest slot down. */
static void snap_scratch(AzWorld *w)
{
    u32 head = AZ_NONE;
    for (u32 s = 0; s < w->n_slots; s++) {
        w->pair[s].peer = AZ_NONE;
        if (w->flags[s] & AZ_F_ALIVE) {
            w->cell[s] = az_cell_at(w, w->pos_x[s], w->pos_y[s], w->pos_z[s]);
            w->spos[s] = AZ_NONE;
        } else {
            w->cell[s] = AZ_NONE;
            w->spos[s] = head;
            head = s;
        }
    }
    w->free_head = head;
    w->n_sorted = 0;
}

/* False when a block of the creature store is not in canonical form. */
static bool snap_restore(AzWorld *w, const SnapPlan *pl, const u8 *buf, u64 pending_at)
{
    bool ok = true;
    const u8 *p = buf + snap_head_bytes(pl->n);
    w->n_slots = get32(buf + 56);
    w->n_clades = get32(buf + 64);
    w->n_colonies = get32(buf + 68);
    w->n_live = get32(buf + 72);
    if (get32(p + 60) != 0) return false; /* reserved scalar word in all supported formats */
    w->tick = get64(p);
    w->uid_next = get64(p + 8);
    w->key[0] = get32(p + 16);
    w->key[1] = get32(p + 20);
    w->role_key = get64(p + 24);
    w->ledger_chain = get64(p + 32);
    w->ledger_count = get64(p + 40);
    w->sun_period[0] = get32(p + 48);
    w->sun_period[1] = get32(p + 52);
    w->gravity = (u16)get16(p + 56);
    w->vent_count = (u16)get16(p + 58);
    az_tick_keys(w);
    p += SNAP_SCALARS;
    az_memcpy(w->vent_cell, p, sizeof(w->vent_cell));  p += SNAP_NVENT * 4;
    az_memcpy(w->gstate, p, sizeof(w->gstate));        p += pad8(sizeof(w->gstate));
    az_memcpy(&w->totals, p, sizeof(AzTotals));        p += pad8(sizeof(AzTotals));
    az_memcpy(&w->code, p, sizeof(AzGeneticCode));     p += pad8(sizeof(AzGeneticCode));
    /* Each writer serialized all 256 words. Validate its own reserved tail
     * before importing only its authored prefix. az_world_create supplied
     * current defaults for any later descriptors and zeroed new columns. */
    const size_t param_bytes = (size_t)get32(buf + 76) * sizeof(w->param[0]);
    if (!snap_all_zero(p + param_bytes, sizeof(w->param) - param_bytes)) return false;
    az_memcpy(w->param, p, param_bytes);
    p += pad8(sizeof(w->param));

    for (u32 k = 0; k < pl->n; k++) {
        SnapCol col;
        const SnapCol *c = &col;
        snap_col(w, pl, k, &col);
        if (c->kind == SNAP_K_PENDING) continue;
        az_memcpy(c->data, p, (size_t)(c->items * c->width));
        p += pad8(c->items * c->width);
    }
    az_memcpy(w->clades, p, (size_t)w->n_clades * sizeof(AzClade));
    p += pad8((u64)w->n_clades * sizeof(AzClade));
    az_memcpy(w->colonies, p, (size_t)w->n_colonies * sizeof(AzColony));
    p += pad8((u64)w->n_colonies * sizeof(AzColony));

    {   /* blocks in slot order from unit 1 of the first half; snap_fits has
         * checked that they fit the file and the store */
        const u8 *tab = p;
        u32 top = 1;
        p += pad8((u64)w->n_slots * 4u);
        for (u32 s = 0; s < w->n_slots; s++) {
            const u32 units = get32(tab + (u64)s * 4u);
            w->blk[s] = 0;
            if (!units) continue;
            AzBlk *b = (AzBlk *)(void *)(w->store + ((size_t)top << 6));
            az_memcpy(b, p, (size_t)units << 6);
            if (!az_blk_ok(b, units)) return false;
            if (get32(buf+12)==5u && b->len[AZ_SEG_BRAIN]) {
                /* Format 5 ended MIND with a four-byte record followed by four
                 * checked zero alignment bytes. Adopt exactly one of them. */
                u32 m=b->len[AZ_SEG_MIND];
                if (m<AZ_SENS+4u || (m-AZ_SENS-4u)%16u || b->off[AZ_SEG_BRAIN]-b->off[AZ_SEG_MIND]-m<1u)
                    return false;
                *((u8 *)(void *)b+b->off[AZ_SEG_MIND]+m)=w->trait[(size_t)s*AZ_TRAITS+AZ_T_STAKE];
                b->len[AZ_SEG_MIND]=m+1u;
            }
            w->blk[s] = top;
            top += units;
            p += (u64)units << 6;
        }
        w->store_base = 0;
        w->store_top = top;
        w->store_live = top - 1u;
    }

    p = buf + pending_at;
    for (u32 k = 0; k < pl->n; k++) {
        SnapCol col;
        const SnapCol *c = &col;
        snap_col(w, pl, k, &col);
        if (c->kind != SNAP_K_PENDING) continue;
        u64 found = get64(p);
        p += 8;
        for (u64 i = 0; i < found; i++, p += 8 + pad8(c->width))
            az_memcpy(c->data + get64(p) * c->width, p + 8, (size_t)c->width);
    }
    snap_scratch(w);
    return ok;
}

/* The checks every reader of a snapshot header makes first. */
static bool snap_header_ok(const u8 *buf, u64 n)
{
    if (!buf || n < SNAP_HEAD_FIXED + 8 || (n & 7)) return false;
    if (get64(buf) != SNAP_MAGIC || get32(buf + 8) != AZ_ABI_VERSION || (get32(buf + 12) < 5u || get32(buf + 12) > SNAP_FORMAT)) return false;
    u32 format = get32(buf + 12);
    u32 descriptors = format < 9u ? SNAP_V5_V8_PARAMS : AZ_WP_COUNT;
    if (get32(buf + 76) != descriptors) return false;
    if (get64(buf + 16) != n) return false;            /* truncated or extended */
    if (get64(buf + n - 8) != snap_sum(buf, n - 8)) return false;
    return get32(buf + 24) == sizeof(AzConfig) && n >= SNAP_HEAD_FIXED + sizeof(AzConfig) + 8;
}

bool az_snapshot_config(const u8 *buf, u64 n, AzConfig *out)
{
    if (!out || !snap_header_ok(buf, n)) return false;
    az_memcpy(out, buf + SNAP_HEAD_FIXED, sizeof(AzConfig));
    return true;
}

/* Restore terrain validity before a caller can publish or sense these slots. */
static bool snap_terrain(AzWorld *w, u32 format)
{
    u32 open = 0;
    while (open < w->n_cells && w->c_solid[open] == 255u) open++;
    if (open == w->n_cells) return false;
#if defined(AZ_ROCK_AWARE_PHYSICS)
    (void)format;
#else
    /* Reference physics can generate covered format-9 positions. Its own
     * current images must round-trip exactly, including those positions. */
    if (format >= 9u) return true;
#endif
    for (u32 s = 0; s < w->n_slots; s++) {
        if (!(w->flags[s] & AZ_F_ALIVE)) continue;
        if (w->c_solid[w->cell[s]] != 255u) continue;
        u32 cell = az_rock_place(w, &w->pos_x[s], &w->pos_y[s], &w->pos_z[s]);
        if (cell == AZ_NONE) return false;
        w->cell[s] = cell;
        w->vel_x[s] = 0; w->vel_y[s] = 0; w->vel_z[s] = 0;
        w->rock_repaired_on_load++;
    }
    if (w->rock_repaired_on_load) az_grid_build(w);
    return true;
}

AzWorld *az_snapshot_load(AzArena *a, const u8 *buf, u64 n, AzHost *host)
{
    if (!a || !snap_header_ok(buf, n)) return NULL;
    if (get32(buf + 24) != sizeof(AzConfig) || get32(buf + 28) != sizeof(AzClade) ||
        get32(buf + 32) != sizeof(AzColony) || get32(buf + 36) != sizeof(AzTotals) ||
        get32(buf + 40) != sizeof(AzGeneticCode) || get32(buf + 44) != AZ_NGLOBAL ||
        get32(buf + 48) != SNAP_NVENT) return NULL;
    const u32 n_cols = get32(buf + 52);
    if (n_cols > SNAP_MAX_COLS || snap_head_bytes(n_cols) + snap_scalar_bytes() + 8 > n) return NULL;

    AzConfig cf;
    SnapPlan pl;
    az_memcpy(&cf, buf + SNAP_HEAD_FIXED, sizeof(AzConfig));
    if (get32(buf + 12) < 9u && (cf.lesion & AZ_LESION_THRUST)) return NULL;
    const u64 used0 = a->used;
    AzWorld *w = az_world_create(a, &cf, host);
    if (w) {
        snap_plan(w, get32(buf + 56), get32(buf + 12), &pl);
        u64 pending_at = snap_fits(w, &pl, buf, n);
        if (pending_at) {
            if (snap_restore(w, &pl, buf, pending_at) && snap_indices_ok(w) && snap_terrain(w, get32(buf + 12))) {
                az_publish_pressure(w);
                return w;
            }
        }
    }
    a->used = used0;                                   /* a failed load takes nothing from the arena */
    return NULL;
}
