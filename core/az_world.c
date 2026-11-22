/* az_world.c - arena, world creation, column registry, hashing, audits.
 * Owner: project lead. Freestanding, integers only.
 */
#include "az_abi.h"
#include "az_hash_parts.h"

/* ---------------------------------------------------------------------- */
/* compile-time budget of the HOT tier (the store handle counts)           */
/* ---------------------------------------------------------------------- */
enum {
#define AZ_TIER_BYTES_HOT(type, per)  + (sizeof(type) * (per))
#define AZ_TIER_BYTES_HND(type, per)  + (sizeof(type) * (per))
#define AZ_TIER_BYTES_WARM(type, per)
#define AZ_TIER_BYTES_EPH(type, per)
#define AZ_COL(name, type, per, tier) AZ_TIER_BYTES_##tier(type, per)
    AZ_HOT_BYTES = 0
#include "az_columns.def"
#undef AZ_COL
#undef AZ_TIER_BYTES_HOT
#undef AZ_TIER_BYTES_HND
#undef AZ_TIER_BYTES_WARM
#undef AZ_TIER_BYTES_EPH
};
_Static_assert(AZ_HOT_BYTES == 128, "HOT tier must be exactly 128 bytes per entity");

/* ---------------------------------------------------------------------- */
/* az_owners.def against the enums: one owner line per trait (an unknown   */
/* or repeated name does not compile), and an owner for every lesion bit   */
/* ---------------------------------------------------------------------- */
enum {
#define AZ_TRAIT(name, owner, via) AZ_OWNED_##name = AZ_T_##name,
#define AZ_LESION_OWNER(name, owner) AZ_HONOURED_##name##_BY_##owner = AZ_LESION_##name,
#include "az_owners.def"
#undef AZ_TRAIT
#undef AZ_LESION_OWNER
    AZ_OWNER_LINES = 0
#define AZ_TRAIT(name, owner, via) + 1
#define AZ_LESION_OWNER(name, owner)
#include "az_owners.def"
#undef AZ_TRAIT
#undef AZ_LESION_OWNER
};
_Static_assert(AZ_OWNER_LINES == AZ_TRAITS, "az_owners.def needs exactly one AZ_TRAIT line per trait");
_Static_assert((0u
#define AZ_TRAIT(name, owner, via)
#define AZ_LESION_OWNER(name, owner) | AZ_LESION_##name
#include "az_owners.def"
#undef AZ_TRAIT
#undef AZ_LESION_OWNER
    ) == AZ_LESION_ALL, "az_owners.def needs an AZ_LESION_OWNER line for every lesion bit");

/* ---------------------------------------------------------------------- */
/* column registry: one static table per .def file                         */
/* ---------------------------------------------------------------------- */
typedef struct ColInfo {
    u32 off;            /* offset of the column pointer inside AzWorld */
    u16 elem;           /* bytes per element                            */
    u16 per;            /* elements per entity / cell                   */
    u32 tier;
    const char *name;
} ColInfo;

static const ColInfo az_ent_cols[] = {
#define AZ_COL(name_, type, per_, tier_) \
    { (u32)__builtin_offsetof(AzWorld, name_), (u16)sizeof(type), (u16)(per_), AZ_TIER_##tier_, #name_ },
#include "az_columns.def"
#undef AZ_COL
};
static const ColInfo az_cell_cols[] = {
#define AZ_CELL(name_, type, per_, tier_) \
    { (u32)__builtin_offsetof(AzWorld, c_##name_), (u16)sizeof(type), (u16)(per_), AZ_TIER_##tier_, #name_ },
#include "az_cells.def"
#undef AZ_CELL
};
#define N_ENT_COLS  AZ_LEN(az_ent_cols)
#define N_CELL_COLS AZ_LEN(az_cell_cols)
_Static_assert(AZ_LEN(az_ent_cols) <= AZ_MAX_COLS && AZ_LEN(az_cell_cols) <= AZ_MAX_COLS, "too many columns");

static void *col_ptr(const AzWorld *w, const ColInfo *ci)
{
    return *(void *const *)(const void *)((const u8 *)w + ci->off);
}
static void col_set(AzWorld *w, const ColInfo *ci, void *p)
{
    *(void **)(void *)((u8 *)w + ci->off) = p;
}

u32 az_n_columns(void) { return N_ENT_COLS; }
u32 az_n_cell_columns(void) { return N_CELL_COLS; }

void az_column(const AzWorld *w, u32 k, AzColDesc *out)
{
    const ColInfo *ci = &az_ent_cols[k];
    out->data = col_ptr(w, ci); out->elem = ci->elem; out->per = ci->per;
    out->tier = ci->tier; out->name = ci->name;
}
void az_cell_column(const AzWorld *w, u32 k, AzColDesc *out)
{
    const ColInfo *ci = &az_cell_cols[k];
    out->data = col_ptr(w, ci); out->elem = ci->elem; out->per = ci->per;
    out->tier = ci->tier; out->name = ci->name;
}

/* ---------------------------------------------------------------------- */
/* world parameters                                                        */
/* ---------------------------------------------------------------------- */
static const AzParamDesc az_params[] = {
#define AZ_PARAM(name_, def_, lo_, hi_, what_) { #name_, def_, lo_, hi_, what_ },
#include "az_params.def"
#undef AZ_PARAM
};
_Static_assert(AZ_LEN(az_params) == AZ_WP_COUNT && AZ_WP_COUNT <= AZ_LEN(((AzWorld *)0)->param),
               "every parameter of az_params.def has a place in AzWorld.param");
#define AZ_PARAM(name_, def_, lo_, hi_, what_) \
    _Static_assert((lo_) <= (def_) && (def_) <= (hi_), "the default of " #name_ " is inside its range");
#include "az_params.def"
#undef AZ_PARAM

const AzParamDesc *az_param_desc(u32 id)
{
    return id < AZ_WP_COUNT ? &az_params[id] : NULL;
}

bool az_param_set(AzWorld *w, u32 id, u32 value)
{
    if (id >= AZ_WP_COUNT || w->tick != 0 || w->n_slots != 0 || w->uid_next != 1) return false;
    if (value < az_params[id].lo || value > az_params[id].hi) return false;
    w->param[id] = value;
    return true;
}

/* ---------------------------------------------------------------------- */
/* freestanding memory primitives                                          */
/* ---------------------------------------------------------------------- */
void az_memset(void *dst, int v, size_t n)
{
    u8 *d = (u8 *)dst;
    const u8 b = (u8)v;
    const u64 w8 = 0x0101010101010101ull * b;
    while (n >= 8) { __builtin_memcpy(d, &w8, 8); d += 8; n -= 8; }
    while (n) { *d++ = b; n--; }
}

void az_memcpy(void *dst, const void *src, size_t n)
{
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;
    while (n >= 8) {
        u64 t;
        __builtin_memcpy(&t, s, 8);
        __builtin_memcpy(d, &t, 8);
        d += 8; s += 8; n -= 8;
    }
    while (n) { *d++ = *s++; n--; }
}

void *az_arena_alloc(AzArena *a, u64 bytes, u64 align)
{
    u64 at = (a->used + (align - 1)) & ~(align - 1);
    if (at + bytes > a->size) return NULL;
    a->used = at + bytes;
    return a->base + at;
}

/* ---------------------------------------------------------------------- */
/* sizes                                                                   */
/* ---------------------------------------------------------------------- */
static u32 round_cap(u32 cap) { return (cap + 63u) & ~63u; }
static u64 pad64(u64 n) { return (n + 63u) & ~(u64)63u; }

/* A column is a 4096-aligned block whose data starts a few cache lines in.
 * The stagger differs per column so that the columns of one entity do not
 * all land in the same cache set (they would with a power-of-two capacity). */
#define COL_SLACK 8192u
static u64 col_stagger(u32 k) { return (u64)((k * 11u) & 63u) * 64u; }

static void *alloc_col(AzArena *a, u64 bytes, u32 k)
{
    u8 *p = (u8 *)az_arena_alloc(a, pad64(bytes) + 4096u, 4096u);
    if (!p) return NULL;
    p += col_stagger(k);
    az_memset(p, 0, (size_t)pad64(bytes));
    return p;
}

static bool config_ok(const AzConfig *cfg)
{
    /* x and y need at least 4 cells so the neighbour offsets -1, 0, +1 reach
     * three different cells across the wrap */
    if (cfg->gx_bits < 2 || cfg->gy_bits < 2 || cfg->gz_bits < 1) return false;
    if (cfg->gx_bits > 12 || cfg->gy_bits > 12 || cfg->gz_bits > 12) return false;
    if (cfg->gx_bits + cfg->gy_bits + cfg->gz_bits > 26) return false;
    u32 s = cfg->field_stride;
    if (s != 0 && (s < 2 || s > 128 || (s & (s - 1)) != 0)) return false;
    if (cfg->cap == 0 || cfg->cap > 0x7FFFFFC0u) return false;
    /* a switch without a name switches nothing off: refuse it (ABI 4) */
    if ((cfg->lesion & ~AZ_LESION_ALL) || cfg->lesion_ext[0] || cfg->lesion_ext[1]) return false;
    if (cfg->mode > 1 || cfg->pad) return false;
    if (cfg->max_clades > (1u << 24) || cfg->max_colonies > (1u << 24)) return false;
    if (cfg->store_units > 0x40000000u || (cfg->store_units && cfg->store_units < 2u)) return false;
    return true;
}

/* capacities of the lineage and the colony table: memory, chosen at run time */
static u32 clades_for(const AzConfig *cfg) { return cfg->max_clades ? cfg->max_clades : 4096u; }
static u32 colonies_for(const AzConfig *cfg) { return cfg->max_colonies ? cfg->max_colonies : 4096u; }

/* units in each half of the creature store */
static u32 store_units_for(const AzConfig *cfg, u32 cap)
{
    if (cfg->store_units) return cfg->store_units;
    /* default: 64 units (4 KiB) per slot of capacity in each half, about one
     * and a half founders; a world of bigger creatures asks for more */
    const u64 units = (u64)cap * 64u + 64u;
    return units > 0x40000000ull ? 0x40000000u : (u32)units;
}

/* Ledger events live in one pool per world, sized by the slot capacity
 * (finding v04: a fixed buffer per worker overflowed when one tick had more
 * births than it held, since the serial life commit emits every birth on
 * worker 0; finding v60: buffers were reserved for AZ_MAX_WORKERS workers
 * whatever the host had). Births and deaths together cannot exceed the slot
 * count in one tick (a slot that dies is reused only after the next grid
 * build, and a newborn cannot die in the tick it is born), so 2 events per
 * slot is twice that bound; AZ_EV_EXTRA covers world events, and one open,
 * partly filled block per worker comes on top. Event kinds that wave 2 adds
 * must fit this budget or raise it (ABI v4). What finds no room is counted
 * in totals.dropped_events. */
#define AZ_EV_PER_SLOT 3u
#define AZ_EV_EXTRA    4096u

static u32 ev_blocks_for(u32 cap)
{
    const u64 events = (u64)AZ_EV_PER_SLOT * cap + AZ_EV_EXTRA;
    return (u32)((events + AZ_EV_BLOCK - 1u) / AZ_EV_BLOCK) + AZ_MAX_WORKERS;
}

u64 az_world_bytes(const AzConfig *cfg)
{
    if (!config_ok(cfg)) return 0;
    u32 cap = round_cap(cfg->cap);
    u32 n_cells = 1u << (cfg->gx_bits + cfg->gy_bits + cfg->gz_bits);
    u32 tmpn = (cap > n_cells ? cap : n_cells) + 1u;
    u64 total = 8192;
    total += pad64(sizeof(AzWorld)) + 4096;
    for (u32 k = 0; k < N_ENT_COLS; k++)
        total += pad64((u64)cap * az_ent_cols[k].per * az_ent_cols[k].elem) + COL_SLACK;
    for (u32 k = 0; k < N_CELL_COLS; k++)
        total += pad64((u64)n_cells * az_cell_cols[k].per * az_cell_cols[k].elem) + COL_SLACK;
    total += pad64((u64)(n_cells + 1u) * 4) + COL_SLACK;                 /* cell_start */
    total += pad64((u64)cap * 4) + COL_SLACK;                            /* order      */
    total += pad64((u64)cap * sizeof(AzPub)) + COL_SLACK;                /* pub        */
    total += pad64((u64)cap * sizeof(AzPubId)) + COL_SLACK;              /* pub_id     */
    total += pad64((u64)store_units_for(cfg, cap) * 2u * AZ_UNIT) + COL_SLACK;   /* store */
    total += pad64((u64)clades_for(cfg) * sizeof(AzClade)) + COL_SLACK;
    total += pad64((u64)colonies_for(cfg) * sizeof(AzColony)) + COL_SLACK;
    total += pad64((u64)colonies_for(cfg) * sizeof(AzColonyEph)) + COL_SLACK;
    total += pad64((u64)cap * 128) + COL_SLACK;                          /* tmp        */
    total += pad64((u64)tmpn * 4) + COL_SLACK;                           /* tmp_u32    */
    total += pad64((u64)(AZ_MAX_PARTS + 1) * 4) + COL_SLACK;             /* splits     */
    total += pad64((u64)AZ_MAX_WORKERS * AZ_WS_BYTES) + COL_SLACK;       /* ws         */
    const u64 eb = ev_blocks_for(cap);
    total += pad64(eb * AZ_EV_BLOCK * sizeof(AzEvent)) + COL_SLACK;      /* ev_pool    */
    total += pad64(eb * 4) + COL_SLACK;                                  /* ev_fill    */
    return total;
}

/* ---------------------------------------------------------------------- */
/* creation                                                                */
/* ---------------------------------------------------------------------- */
AzWorld *az_world_create(AzArena *arena, const AzConfig *cfg, AzHost *host)
{
    if (!config_ok(cfg)) return NULL;
    if (((uintptr_t)arena->base & 4095u) != 0) return NULL;      /* host obligation */
    /* a wild world is made of fresh entropy: no host that gives it, no world */
    u64 fresh[2] = { 0, 0 };
    if (cfg->mode == AZ_MODE_WILD && !(host && host->entropy && host->entropy(host->ctx, fresh, 16u))) return NULL;
    /* P4-CORE-02: stage arena cursor and restore on any failed creation path so a smaller retry succeeds */
    const u64 saved_used = arena->used;

    AzWorld *w = (AzWorld *)az_arena_alloc(arena, sizeof(AzWorld), 4096);
    if (!w) { arena->used = saved_used; return NULL; }
    az_memset(w, 0, sizeof(*w));

    w->cfg = *cfg;
    if (w->cfg.reorder_period < 1) w->cfg.reorder_period = 1;
    if (w->cfg.tier1_period < 1)   w->cfg.tier1_period = 32;
    if (w->cfg.tier2_period < 1)   w->cfg.tier2_period = 256;
    if (w->cfg.field_stride == 0)  w->cfg.field_stride = 8;
    w->cfg.max_clades = clades_for(cfg);
    w->cfg.max_colonies = colonies_for(cfg);
    if (w->cfg.store_period < 1)   w->cfg.store_period = w->cfg.reorder_period;
    {
        const u64 origin = cfg->mode == AZ_MODE_WILD ? fresh[0] : cfg->seed;
        w->key[0] = (u32)origin;
        w->key[1] = (u32)(origin >> 32);
        w->role_key = az_mix64((cfg->mode == AZ_MODE_WILD ? fresh[1] : cfg->seed) ^ 0x9E3779B97F4A7C15ull);
    }
    w->tick = 0;
    w->uid_next = 1;
    az_tick_keys(w);
    for (u32 i = 0; i < AZ_WP_COUNT; i++) w->param[i] = az_params[i].def;
    w->cap = round_cap(cfg->cap);
    w->free_head = AZ_NONE;
    w->gx_bits = cfg->gx_bits; w->gy_bits = cfg->gy_bits; w->gz_bits = cfg->gz_bits;
    w->gx = 1u << cfg->gx_bits; w->gy = 1u << cfg->gy_bits; w->gz = 1u << cfg->gz_bits;
    w->n_cells = w->gx * w->gy * w->gz;
    w->shift_x = (u8)(32 - cfg->gx_bits);
    w->shift_y = (u8)(32 - cfg->gy_bits);
    w->shift_z = (u8)(32 - cfg->gz_bits);
    w->arena = arena;
    w->host = host;
    /* a worker token indexes workers[], so a host with more workers than
     * AZ_MAX_WORKERS would make az_tally and az_emit write past the array
     * (finding v44): refuse it instead of clamping only our own count */
    if (host && host->n_workers > AZ_MAX_WORKERS) { arena->used = saved_used; return NULL; }
    w->n_workers = (host && host->n_workers) ? host->n_workers : 1;

    const u32 cap = w->cap;
    const u32 nc = w->n_cells;
    u32 k_all = 0;

    for (u32 k = 0; k < N_ENT_COLS; k++, k_all++) {
        void *p = alloc_col(arena, (u64)cap * az_ent_cols[k].per * az_ent_cols[k].elem, k_all);
        if (!p) { arena->used = saved_used; return NULL; }
        col_set(w, &az_ent_cols[k], p);
    }
    for (u32 k = 0; k < N_CELL_COLS; k++, k_all++) {
        void *p = alloc_col(arena, (u64)nc * az_cell_cols[k].per * az_cell_cols[k].elem, k_all);
        if (!p) { arena->used = saved_used; return NULL; }
        col_set(w, &az_cell_cols[k], p);
    }

    u32 tmpn = (cap > nc ? cap : nc) + 1u;
    w->cell_start = (u32 *)alloc_col(arena, (u64)(nc + 1u) * 4, k_all++);
    w->order      = (u32 *)alloc_col(arena, (u64)cap * 4, k_all++);
    w->pub        = (AzPub *)alloc_col(arena, (u64)cap * sizeof(AzPub), k_all++);
    w->pub_id     = (AzPubId *)alloc_col(arena, (u64)cap * sizeof(AzPubId), k_all++);
    w->store_units = store_units_for(cfg, cap);
    w->cfg.store_units = w->store_units;
    {   /* only unit 0 of each half has to be zero: a block is cleared when it is made */
        const u64 bytes = pad64((u64)w->store_units * 2u * AZ_UNIT);
        w->store = (u8 *)az_arena_alloc(arena, bytes, 4096u);
        if (!w->store) { arena->used = saved_used; return NULL; }
        az_memset(w->store, 0, AZ_UNIT);
        az_memset(w->store + (size_t)w->store_units * AZ_UNIT, 0, AZ_UNIT);
    }
    w->store_base = 0;
    w->store_top = 1;
    w->store_live = 0;
    w->clades     = (AzClade *)alloc_col(arena, (u64)w->cfg.max_clades * sizeof(AzClade), 0);
    w->colonies   = (AzColony *)alloc_col(arena, (u64)w->cfg.max_colonies * sizeof(AzColony), 0);
    w->colony_eph = (AzColonyEph *)alloc_col(arena, (u64)w->cfg.max_colonies * sizeof(AzColonyEph), 0);
    w->tmp        = (u8 *)alloc_col(arena, (u64)cap * 128, 0);
    w->tmp_u32    = (u32 *)alloc_col(arena, (u64)tmpn * 4, 0);
    w->splits     = (u32 *)alloc_col(arena, (u64)(AZ_MAX_PARTS + 1) * 4, 0);
    /* one scratch block per possible worker token (2 MB in all): a world can
     * be given to a host with any worker count up to AZ_MAX_WORKERS */
    w->ws         = (u8 *)alloc_col(arena, (u64)AZ_MAX_WORKERS * AZ_WS_BYTES, 0);
    if (!w->cell_start || !w->order || !w->pub || !w->pub_id || !w->clades || !w->colonies || !w->colony_eph ||
        !w->tmp || !w->tmp_u32 || !w->splits || !w->ws) { arena->used = saved_used; return NULL; }
    w->n_clades = 0;
    w->n_colonies = 1;   /* colony 0 = "no colony" */
    w->k.brain_step = az_brain_step_c;   /* C reference kernels; twins are installed by the host */
    w->k.philox_n = az_philox4x32_n;
    w->k.kin_axis = az_kin_axis_c;
    w->k.asm_mask = 0;

    w->ev_blocks = ev_blocks_for(cap);
    w->ev_pool = (AzEvent *)alloc_col(arena, (u64)w->ev_blocks * AZ_EV_BLOCK * sizeof(AzEvent), 0);
    w->ev_fill = (u32 *)alloc_col(arena, (u64)w->ev_blocks * 4, 0);
    if (!w->ev_pool || !w->ev_fill) { arena->used = saved_used; return NULL; }
    w->ev_next = 0;                       /* every worker starts with no open block */
    return w;
}

/* Shift every address stored in the world by delta: used when the arena is
 * copied to another base (a device buffer, a snapshot mapped elsewhere). The
 * host-only members are cleared and the kernel table goes back to the C
 * reference, which every caller can run (finding v45: a NULL brain_step made
 * the next az_tick fault); the caller installs its own host and twins. */
void az_world_rebase(AzWorld *w, i64 delta)
{
#define REB(p) do { if (p) (p) = (void *)((u8 *)(p) + delta); } while (0)
    for (u32 k = 0; k < N_ENT_COLS; k++) {
        void *p = col_ptr(w, &az_ent_cols[k]);
        if (p) col_set(w, &az_ent_cols[k], (u8 *)p + delta);
    }
    for (u32 k = 0; k < N_CELL_COLS; k++) {
        void *p = col_ptr(w, &az_cell_cols[k]);
        if (p) col_set(w, &az_cell_cols[k], (u8 *)p + delta);
    }
    REB(w->cell_start); REB(w->order); REB(w->pub); REB(w->pub_id); REB(w->store);
    REB(w->clades); REB(w->colonies); REB(w->colony_eph);
    REB(w->tmp); REB(w->tmp_u32); REB(w->splits); REB(w->ws);
    REB(w->ev_pool); REB(w->ev_fill);
    for (u32 k = 0; k < AZ_MAX_WORKERS; k++) REB(w->workers[k].events);   /* open blocks */
#undef REB
    w->host = NULL;
    w->arena = NULL;
    w->k.brain_step = az_brain_step_c;
    w->k.philox_n = az_philox4x32_n;
    w->k.kin_axis = az_kin_axis_c;
    w->k.asm_mask = 0;
}

/* ---------------------------------------------------------------------- */
/* slots                                                                   */
/* ---------------------------------------------------------------------- */
/* The keys of the current tick. While the tick is below 2^32 they are the
 * world keys themselves; beyond, the high half of the tick is folded in, so
 * no (id, tick) pair ever repeats a counter block. */
void az_tick_keys(AzWorld *w)
{
    const u64 hi = w->tick >> 32;
    u64 m = hi ? az_mix64(hi) : 0;
    u64 r = hi ? az_mix64(m ^ 0x5DEECE66Dull) : 0;
    /* a wild world: what this tick drew goes into every draw of the tick */
    if (w->cfg.mode == AZ_MODE_WILD) { m ^= w->wild[0]; r ^= w->wild[1]; }
    w->tkey[0] = w->key[0] ^ (u32)m;
    w->tkey[1] = w->key[1] ^ (u32)(m >> 32);
    w->role_tkey = w->role_key ^ r;
}

u32 az_spawn(AzWorld *w)
{
    /* uid 0 means "none"; uids are 64-bit and never run out */
    u32 s;
    if (w->free_head != AZ_NONE) {            /* reuse a slot that was dead at tick start */
        s = w->free_head;
        w->free_head = w->spos[s];
        /* P4-CORE-03: release any restored store block held by this dead slot before clearing columns */
        if (w->blk[s]) az_blk_release(w, s);
    } else if (w->n_slots < w->cap) {
        s = w->n_slots++;
    } else {
        /* no slot: every slot was alive at tick start or is already reused; a
         * slot that died this tick joins the free list at the next grid build
         * (v42). Independent of the reorder period. Recorded: the counter, and
         * a ledger event the first time. */
        if (w->totals.cap_denied++ == 0) az_emit(w, 0, AZ_EV_REFUSED, AZ_REF_SLOT, w->uid_next, 0, 0);
        return AZ_NONE;
    }
    for (u32 k = 0; k < N_ENT_COLS; k++) {
        size_t bytes = (size_t)az_ent_cols[k].elem * az_ent_cols[k].per;
        az_memset((u8 *)col_ptr(w, &az_ent_cols[k]) + (size_t)s * bytes, 0, bytes);
    }
    w->uid[s] = w->uid_next++;
    w->cell[s] = AZ_NONE;    /* the caller sets a position and calls az_place */
    w->spos[s] = AZ_NONE;    /* no sorted position until the next az_grid_build */
    w->pair[s].peer = AZ_NONE;
    w->rot_w[s] = 1 << 14;   /* identity rotor */
    return s;
}

/* Close the worker's open block (record how full it is) and take the next
 * free block of the pool; false when the pool is used up. The block a worker
 * gets depends on the schedule, which is harmless: the ledger sorts the
 * events into one canonical order. */
static bool ev_take_block(AzWorld *w, AzWorker *wk)
{
    if (wk->events) w->ev_fill[(u32)(wk->events - w->ev_pool) / AZ_EV_BLOCK] = wk->n_events;
    wk->events = NULL;
    wk->n_events = wk->cap_events = 0;
    /* the load keeps ev_next from running far past the end on repeated failures */
    u32 blk = w->ev_blocks;
    if (__atomic_load_n(&w->ev_next, __ATOMIC_RELAXED) < w->ev_blocks)
        blk = __atomic_fetch_add(&w->ev_next, 1u, __ATOMIC_RELAXED);
    if (blk >= w->ev_blocks) return false;
    wk->events = w->ev_pool + (size_t)blk * AZ_EV_BLOCK;
    wk->cap_events = AZ_EV_BLOCK;
    return true;
}

void az_emit(AzWorld *w, u32 worker, u32 kind, u32 aux, u64 a, u64 b, i64 v)
{
    AzWorker *wk = &w->workers[worker];
    if (wk->n_events >= wk->cap_events && !ev_take_block(w, wk)) { wk->dropped_events++; return; }
    AzEvent *e = &wk->events[wk->n_events++];
    e->tick = w->tick; e->a = a; e->b = b; e->v = v; e->aux = aux; e->kind = (u16)kind; e->pad = 0;
}

u32 az_events_close(AzWorld *w)
{
    for (u32 k = 0; k < AZ_MAX_WORKERS; k++) {
        AzWorker *wk = &w->workers[k];
        if (wk->events) w->ev_fill[(u32)(wk->events - w->ev_pool) / AZ_EV_BLOCK] = wk->n_events;
        wk->events = NULL;
        wk->n_events = wk->cap_events = 0;
    }
    return w->ev_next < w->ev_blocks ? w->ev_next : w->ev_blocks;
}

void az_events_reset(AzWorld *w)
{
    w->ev_next = 0;
}

/* Fold every worker's private tally into the totals. Serial; called by the
 * tick after each parallel stage. Integer sums: independent of thread count. */
void az_tally_merge(AzWorld *w)
{
    AzTotals *t = &w->totals;
    for (u32 k = 0; k < AZ_MAX_WORKERS; k++) {
        AzWorker *wk = &w->workers[k];
        AzTally *y = &wk->tally;
        t->energy_in += y->energy_in;
        t->energy_out += y->energy_out;
        t->reservoir_mu = (u64)((i64)t->reservoir_mu + y->reservoir);
        t->deaths += y->deaths;
        for (u32 i = 0; i < AZ_ACT_COUNT; i++) t->interactions[i] += y->interactions[i];
        for (u32 i = 0; i < AZ_NINV; i++)
            for (u32 j = 0; j < AZ_NINV; j++) t->trade_units[i][j] += y->trade_units[i][j];
        for (u32 i = 0; i < 8; i++) t->sat[i] += y->sat[i];
        t->field_underflow += y->field_underflow;
        t->dropped_events += wk->dropped_events;
        wk->dropped_events = 0;
        az_memset(y, 0, sizeof(*y));
    }
}

/* ---------------------------------------------------------------------- */
/* state hash: independent of slot order, thread count and reorder period  */
/* ---------------------------------------------------------------------- */
AZ_INLINE u64 hash_bytes(u64 acc, const u8 *p, size_t n)
{
    while (n >= 8) {
        u64 t;
        __builtin_memcpy(&t, p, 8);
        acc = az_mix64(acc ^ t) + 0x9E3779B97F4A7C15ull;
        p += 8; n -= 8;
    }
    if (n) {
        u64 t = 0;
        for (size_t i = 0; i < n; i++) t |= (u64)p[i] << (8 * i);
        acc = az_mix64(acc ^ t ^ ((u64)n << 56)) + 0x9E3779B97F4A7C15ull;
    }
    return acc;
}

static bool all_zero(const u8 *p, size_t n)
{
    while (n >= sizeof(u64)) {
        u64 word;
        __builtin_memcpy(&word, p, sizeof word);
        if (word) return false;
        p += sizeof word;
        n -= sizeof word;
    }
    for (size_t i = 0; i < n; i++)
        if (p[i]) return false;
    return true;
}

static u64 hash_entity_columns(const AzWorld *w, u32 slot)
{
    u64 acc = az_mix64(0xA5A5A5A5ull + w->uid[slot]);
    for (u32 k = 0; k < N_ENT_COLS; k++) {
        const ColInfo *ci = &az_ent_cols[k];
        if (ci->tier == AZ_TIER_EPH || ci->tier == AZ_TIER_HND) continue;
        size_t bytes = (size_t)ci->elem * ci->per;
        acc = hash_bytes(acc ^ (u64)(k + 1) * 0x100000001B3ull,
                         (const u8 *)col_ptr(w, ci) + (size_t)slot * bytes, bytes);
    }
    return acc;
}

u64 az_hash_entities(const AzWorld *w, u32 begin, u32 end)
{
    u64 sum = 0;

    /* entities: commutative sum of per-entity hashes keyed by uid */
    for (u32 s = begin; s < end; s++) {
        if (!(w->flags[s] & AZ_F_ALIVE)) continue;
        u64 acc = hash_entity_columns(w, s);
        /* the creature's block: its bytes, not where it lies */
        if (w->blk[s]) {
            const AzBlk *b = az_blk(w, s);
            acc = hash_bytes(acc ^ 0xB10CB10CB10CB10Cull, (const u8 *)(const void *)b, (size_t)b->units << 6);
        }
        sum += az_mix64(acc);
    }

    return sum;
}

/* Each block has a serial mixing dependency. Interleave several completely
 * independent creatures so later lanes can issue loads and multiplies while
 * earlier ones wait. The order of bytes and mixing within a lane is the
 * canonical scalar order, including for unequal or absent blocks. */
enum { HASH_LANES = 4 };
static u64 hash_four_entities(const AzWorld *w, const u32 slot[HASH_LANES])
{
    const u8 *data[HASH_LANES];
    size_t bytes[HASH_LANES], common = (size_t)-1;
    u64 state[HASH_LANES];
    for (u32 j = 0; j < HASH_LANES; j++) {
        state[j] = hash_entity_columns(w, slot[j]);
        const AzBlk *block = w->blk[slot[j]] ? az_blk(w, slot[j]) : NULL;
        data[j] = (const u8 *)(const void *)block;
        bytes[j] = block ? (size_t)block->units << 6 : 0;
        if (block) state[j] ^= 0xB10CB10CB10CB10Cull;
        if (bytes[j] < common) common = bytes[j];
    }
    u64 a = state[0], b = state[1], c = state[2], d = state[3];
    for (size_t offset = 0; offset < common; offset += 8) {
        a = hash_bytes(a, data[0] + offset, sizeof(u64));
        b = hash_bytes(b, data[1] + offset, sizeof(u64));
        c = hash_bytes(c, data[2] + offset, sizeof(u64));
        d = hash_bytes(d, data[3] + offset, sizeof(u64));
    }
    state[0] = a; state[1] = b; state[2] = c; state[3] = d;
    u64 sum = 0;
    for (u32 j = 0; j < HASH_LANES; j++) {
        if (bytes[j] > common)
            state[j] = hash_bytes(state[j], data[j] + common, bytes[j] - common);
        sum += az_mix64(state[j]);
    }
    return sum;
}

u64 az_hash_entities_batched(const AzWorld *w, u32 begin, u32 end)
{
    u32 slots[HASH_LANES], n = 0;
    u64 sum = 0;
    for (u32 i = begin; i < end; i++) {
        if (!(w->flags[i] & AZ_F_ALIVE)) continue;
        slots[n++] = i;
        if (n == HASH_LANES) {
            sum += hash_four_entities(w, slots);
            n = 0;
        }
    }
    if (n) sum += az_hash_entities(w, slots[0], end);
    return sum;
}

/* Apply one registered column to the independent cell states in this tile.
 * The .def expansion supplies a fixed width so the compiler can specialize
 * the existing byte reader, including its partial-word terminator. */
AZ_INLINE void hash_cell_column(u64 *state, u32 count, const u8 *base, size_t width,
                                u64 salt, bool skip_zero)
{
    for (u32 j = 0; j < count; j++) {
        const u8 *p = base + (size_t)j * width;
        if (!skip_zero || !all_zero(p, width))
            state[j] = hash_bytes(state[j] ^ salt, p, width);
    }
}

/* The reduction is commutative across cells. Visit each registered column
 * across a small tile, preserving salt, byte order and pending-post rules
 * within each cell. A short final tile handles every partition boundary. */
u64 az_hash_cells(const AzWorld *w, u32 begin, u32 end)
{
    enum { CELL_BATCH = 8 };
    u64 sum = 0;
    while (begin < end) {
        u32 count = az_min_u32(end - begin, CELL_BATCH);
        u64 state[CELL_BATCH];
        for (u32 j = 0; j < count; j++)
            state[j] = az_mix64(0xC3C3C3C3ull + begin + j);
        /* Persistent fields always contribute. Pending posts, including high
         * limbs, contribute when nonzero. Demand, grants and pressure are
         * scratch. Retain the registry index even for an omitted column. */
        u32 index = 0;
#define AZ_CELL(name, type, per, tier) do {                                                \
            u64 salt = (u64)++index * 0x100000001B3ull;                                     \
            if (AZ_TIER_##tier == AZ_TIER_FIELD || !az_cell_scratch(w, w->c_##name))        \
                hash_cell_column(state, count, (const u8 *)(w->c_##name + (size_t)begin * (per)), \
                                 sizeof(type) * (per), salt, AZ_TIER_##tier != AZ_TIER_FIELD); \
        } while (0);
#include "az_cells.def"
#undef AZ_CELL
        for (u32 j = 0; j < count; j++) sum += az_mix64(state[j]);
        begin += count;
    }
    return sum;
}

u64 az_hash_tail(const AzWorld *w)
{
    /* clades and colonies (their index order is canonical: made in serial code) */
    u64 acc = az_mix64(0x5151ull + w->n_clades);
    for (u32 i = 0; i < w->n_clades; i++)
        acc = hash_bytes(acc + i, (const u8 *)&w->clades[i], sizeof(AzClade));
    acc = az_mix64(acc + w->n_colonies);
    for (u32 i = 0; i < w->n_colonies; i++)
        acc = hash_bytes(acc + i, (const u8 *)&w->colonies[i], sizeof(AzColony));
    /* colony deposits and withdrawals that no commit applied (finding v40);
     * normally none, and then nothing is added */
    for (u32 i = 0; i < w->n_colonies; i++) {
        const AzColonyEph *e = &w->colony_eph[i];
        if (all_zero((const u8 *)e->dep, sizeof e->dep) && all_zero((const u8 *)e->taken, sizeof e->taken) &&
            !e->dep_e && !e->taken_e)
            continue;
        /* Preserve the original encoding for every previously representable pending post. */
        u32 dep[AZ_NINV], taken[AZ_NINV], high[2 * AZ_NINV];
        for (u32 k = 0; k < AZ_NINV; k++) {
            dep[k] = (u32)e->dep[k];
            taken[k] = (u32)e->taken[k];
            high[k] = (u32)(e->dep[k] >> 32);
            high[AZ_NINV + k] = (u32)(e->taken[k] >> 32);
        }
        acc = hash_bytes(acc ^ (0xC0100000ull + i), (const u8 *)dep, sizeof dep);
        acc = hash_bytes(acc, (const u8 *)taken, sizeof taken);
        if (!all_zero((const u8 *)high, sizeof high))
            acc = hash_bytes(acc ^ 0xC010400000ull, (const u8 *)high, sizeof high);
        acc = az_mix64(acc ^ e->dep_e) + az_mix64(e->taken_e + 0x9E37ull);
    }
    u64 sum = az_mix64(acc);

    /* globals: counters, world-level module state, world constants, genetic code */
    u64 g = az_mix64(az_mix64(w->tick) ^ w->uid_next);
    g = az_mix64(g ^ w->totals.reservoir_mu);
    g = az_mix64(g ^ (u64)w->totals.energy_in);
    g = az_mix64(g ^ (u64)w->totals.energy_out);
    g = az_mix64(g ^ w->totals.births);
    g = az_mix64(g ^ w->totals.deaths);
    g = hash_bytes(g, (const u8 *)w->gstate, sizeof(w->gstate));
    g = hash_bytes(g, (const u8 *)w->sun_period, sizeof(w->sun_period));
    g = az_mix64(g ^ ((u64)w->gravity << 16) ^ w->vent_count);
    g = hash_bytes(g, (const u8 *)w->vent_cell, sizeof(w->vent_cell));
    g = hash_bytes(g, (const u8 *)&w->code, sizeof(w->code));
    g = hash_bytes(g, (const u8 *)w->param, sizeof(w->param));
    g = az_mix64(g ^ ((u64)w->key[0] << 32) ^ w->key[1]);
    return sum + g;
}

u64 az_state_hash(const AzWorld *w)
{
    return az_hash_entities_batched(w, 0, w->n_slots) + az_hash_cells(w, 0, w->n_cells) + az_hash_tail(w);
}

/* ---------------------------------------------------------------------- */
/* conservation audits                                                     */
/* ---------------------------------------------------------------------- */
/* Opposing transfers cancel in u64 before a pending debt is interpreted as
 * signed. The public audits keep their original widths and energy weights. */
static u64 pantry_balance(const AzWorld *w, u32 i, u32 k)
{
    return w->colonies[i].store[k] + w->colony_eph[i].dep[k] - w->colony_eph[i].taken[k];
}

u64 az_total_matter(const AzWorld *w)
{
    i64 m = (i64)w->totals.reservoir_mu;
    for (u32 c = 0; c < w->n_cells; c++)
        for (u32 k = 0; k < AZ_NSPECIES; k++)
            m += (i64)w->c_sp[(size_t)c * AZ_NSPECIES + k] + az_pending_species(w, c, k);
    for (u32 s = 0; s < w->n_slots; s++) {
        m += (i64)w->pool_a[s] + w->pool_m[s] + w->pool_t[s] + w->waste[s] + w->toxin[s];
        if (w->blk[s]) m += (i64)az_body_matter(w, s);       /* what is built into the body's parts */
        for (u32 k = 0; k < AZ_NINV; k++) m += w->inv[(size_t)s * AZ_NINV + k];
        /* provisions already taken from a mother, waiting for the life commit */
        const AzBirth *b = &w->birth[s];
        if (b->n) m += (i64)b->prov[0] + b->prov[1] + b->prov[2];
    }
    for (u32 i = 0; i < w->n_colonies; i++)
        for (u32 k = 0; k < AZ_NINV; k++)
            m = (i64)((u64)m + pantry_balance(w, i, k));
    for (u32 k = 0; k < AZ_MAX_WORKERS; k++) m += w->workers[k].tally.reservoir;
    return (u64)m;
}

i64 az_total_energy(const AzWorld *w)
{
    i64 e = 0;
    for (u32 c = 0; c < w->n_cells; c++) {
        e += (i64)w->c_heat[c] + az_pending_heat(&w->c_acc[c]);
        for (u32 k = 0; k < AZ_NSPECIES; k++)
            e += (i64)az_eps_species[k] * ((i64)w->c_sp[(size_t)c * AZ_NSPECIES + k] + az_pending_species(w, c, k));
    }
    for (u32 s = 0; s < w->n_slots; s++) {
        e += (i64)w->energy[s];
        e += (i64)AZ_EPS_BIO * ((i64)w->pool_a[s] + w->pool_m[s] + w->pool_t[s]);
        if (w->blk[s]) e += (i64)AZ_EPS_BIO * (i64)az_body_matter(w, s);
        e += (i64)AZ_EPS_WASTE * w->waste[s];
        e += (i64)AZ_EPS_TOXIN * w->toxin[s];
        for (u32 k = 0; k < AZ_NINV; k++)
            e += (i64)az_eps_species[az_inv_species[k]] * w->inv[(size_t)s * AZ_NINV + k];
        const AzBirth *b = &w->birth[s];
        if (b->n) e += (i64)b->energy + (i64)AZ_EPS_BIO * ((i64)b->prov[0] + b->prov[1] + b->prov[2]);
    }
    for (u32 i = 0; i < w->n_colonies; i++) {
        /* the colony's own energy pool, with what its members gave and took this tick */
        e = (i64)((u64)e + (u64)w->colonies[i].energy + w->colony_eph[i].dep_e - w->colony_eph[i].taken_e);
        for (u32 k = 0; k < AZ_NINV; k++)
            e = (i64)((u64)e + (u64)az_eps_species[az_inv_species[k]] * pantry_balance(w, i, k));
    }
    /* external flows: subtract what came in, add back what left (merged totals
     * plus whatever is still in the workers' private tallies) */
    i64 in = w->totals.energy_in, out = w->totals.energy_out;
    for (u32 k = 0; k < AZ_MAX_WORKERS; k++) {
        in += w->workers[k].tally.energy_in;
        out += w->workers[k].tally.energy_out;
    }
    return e - in + out;
}
