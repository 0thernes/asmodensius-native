/* az_test.h - minimal test support. One executable per tests/test_*.c file.
 * A test file includes this header, writes checks with CHECK(...), and ends
 * main with `return az_test_done();`. Exit code 0 = all checks passed. */
#ifndef AZ_TEST_H
#define AZ_TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "../core/az_abi.h"
#include "../platform/win/az_host_win.h"

static int az_checks = 0, az_failures = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        az_checks++;                                                             \
        if (!(cond)) {                                                           \
            az_failures++;                                                       \
            if (az_failures <= 20) printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                        \
    } while (0)

#define CHECK_EQ(a, b)                                                           \
    do {                                                                         \
        long long az_a = (long long)(a), az_b = (long long)(b);                  \
        az_checks++;                                                             \
        if (az_a != az_b) {                                                      \
            az_failures++;                                                       \
            if (az_failures <= 20)                                               \
                printf("FAIL %s:%d: %s == %s  (%lld vs %lld)\n", __FILE__, __LINE__, #a, #b, az_a, az_b); \
        }                                                                        \
    } while (0)

static inline int az_test_done(void)
{
    printf("%s: %d checks, %d failures\n", az_failures ? "FAILED" : "ok", az_checks, az_failures);
    return az_failures ? 1 : 0;
}

/* splitmix64 for tests (finding v58): add the golden gamma to the state, then
 * apply az_mix64, the splitmix64 finaliser. Every test that wrote its own copy
 * used exactly this sequence, so switching to it changes no pinned value. */
static inline u64 az_test_rand64(u64 *state)
{
    *state += 0x9E3779B97F4A7C15ull;
    return az_mix64(*state);
}

/* A test world owns its locked arena and optional Windows worker pool. */
typedef struct AzTestWorld {
    AzArena arena;
    AzWorld *w;
    AzHost *host;
    AzWinInfo info;
} AzTestWorld;

static inline AzConfig az_test_config(u64 seed, u32 n)
{
    AzConfig c;
    memset(&c, 0, sizeof c);
    c.mode = AZ_MODE_LAB;       /* a test needs a world it can repeat */
    c.seed = seed;
    c.n_initial = n;
    c.cap = n * 4 + 1024;
    c.n_species = 8;
    c.gx_bits = 4; c.gy_bits = 4; c.gz_bits = 3;
    c.reorder_period = 1;
    c.tier1_period = 8;
    c.tier2_period = 16;
    c.field_stride = 4;
    return c;
}

/* Options of az_test_world_ex (finding v52: four tests carried their own
 * copy of the helper only to get these). */
enum {
    AZ_TEST_UNPINNED    = 1u,   /* the pool leaves placement to the OS (AZ_WIN_NO_PIN)   */
    AZ_TEST_NO_WORLDGEN = 2u    /* an empty world: the test places its own entities      */
};

static inline void az_test_world_free(AzTestWorld *t)
{
    if (t->host) az_win_host_destroy(t->host);
    az_win_arena_free(&t->arena);
    memset(t, 0, sizeof *t);
}

/* A new world and a snapshot destination require the same residency. */
static inline bool az_test_world_arena(AzTestWorld *t, const AzConfig *cfg)
{
    if (az_win_arena(&t->arena, az_world_bytes(cfg), AZ_WIN_LOCK, &t->info)) return true;
    fprintf(stderr, "test world arena: %s\n", t->info.note);
    return false;
}

/* Zero threads means serial. AZ_TEST_UNPINNED controls only worker placement. */
static inline bool az_test_world_ex(AzTestWorld *t, const AzConfig *cfg, u32 threads, u32 flags)
{
    memset(t, 0, sizeof *t);
    if (!az_test_world_arena(t, cfg)) return false;
    if (threads) {
        u32 hflags = AZ_WIN_USE_ECORES | ((flags & AZ_TEST_UNPINNED) ? AZ_WIN_NO_PIN : 0u);
        t->host = az_win_host_create(threads, hflags, &t->info);
        if (!t->host) { az_test_world_free(t); return false; }
    }
    t->w = az_world_create(&t->arena, cfg, t->host);
    if (!t->w) { az_test_world_free(t); return false; }
    if (!(flags & AZ_TEST_NO_WORLDGEN)) az_worldgen(t->w);
    return true;
}

static inline bool az_test_world(AzTestWorld *t, const AzConfig *cfg, u32 threads)
{
    return az_test_world_ex(t, cfg, threads, 0);
}

/* The uniform world of the reference worldgen stub (core/stubs/az_worldgen.c,
 * verbatim): sun periods 2000 and 3100, every cell solvent 1000, oxidiser 200,
 * organic matter 400 at temperature 8192 with no rock, no vent and no gravity,
 * and n_initial founders spread uniformly at any height. A test whose checks
 * pin that world's own geometry or amounts runs on it: az_test_world_ex(...,
 * AZ_TEST_NO_WORLDGEN), then az_test_uniform(w). Living-world tests use the
 * real az_worldgen. */
static inline void az_test_uniform(AzWorld *w)
{
    w->sun_period[0] = 2000; w->sun_period[1] = 3100;
    az_weather_global(w);                        /* the suns stand somewhere before the cells are derived */
    for (u32 c = w->n_cells; c-- > 0u;) {        /* from the top layer down: a cell's light comes from the cell above */
        u32 *sp = &w->c_sp[(size_t)c * AZ_NSPECIES];
        sp[AZ_SP_SOLV] = 1000;
        sp[AZ_SP_OXI] = 200;
        sp[AZ_SP_ORG_Y] = 400;
        w->c_heat[c] = 8u * (1600u + 16u);       /* 8 eu per matter unit: temperature 8192, the optimum of TOPT 128 */
        w->c_light[c] = 128;
        w->c_lvec[(size_t)c * 4 + 2] = 127;      /* the light comes from above */
        az_chem_derive(w, c);                    /* the derived columns are defined from tick 0 */
        az_weather_derive(w, c);
    }
    w->gravity = 0;
    az_genetic_code_init(w);

    u32 nsp = w->cfg.n_species ? w->cfg.n_species : 1;
    u32 first_clade = w->n_clades;
    for (u32 k = 0; k < nsp; k++) (void)az_clade_found(w, 0x10000u + k, k);

    for (u32 i = 0; i < w->cfg.n_initial; i++) {
        u32 s = az_spawn(w);
        if (s == AZ_NONE) break;
        u32 r[4];
        az_rand4(w, w->uid[s], AZ_RS_WORLDGEN, 0, r);
        w->pos_x[s] = r[0];
        w->pos_y[s] = r[1];
        w->pos_z[s] = (1u << 16) + (u32)(((u64)r[2] * (0xFFFFFFFFull - (2u << 16))) >> 32);
        u32 species = r[3] % nsp;
        u32 cl = first_clade + species;
        if (cl >= w->n_clades) cl = 0;
        w->clade[s] = cl;
        w->clades[cl].refcount++;
        w->energy[s] = 2000;
        w->pool_a[s] = 200;
        w->mass[s] = 200;
        w->health[s] = 40000;
        w->flags[s] = AZ_F_ALIVE;
        w->lineage[s] = species;
        w->birth_tick[s] = 0;
        az_place(w, s);
        if (!az_found(w, s, 0x10000u + species)) {     /* no room in the creature store: no founder */
            w->clades[cl].refcount--;
            w->flags[s] = 0;
            w->energy[s] = 0; w->pool_a[s] = 0; w->mass[s] = 0;
            break;
        }
    }
}

/* Test bench: every founder buds. With az_life.c real and the mating module
 * a stub, which species can reproduce alone depends on the seed (the mating
 * mode is a trait); a test that needs births whatever the seed calls this
 * after worldgen. Children develop their own mode from their own genome. */
static inline void az_test_founders_bud(AzWorld *w)
{
    for (u32 s = 0; s < w->n_slots; s++)
        if (w->flags[s] & AZ_F_ALIVE) w->phen[(size_t)s * AZ_PHEN + AZ_P_MATE] &= (u8)~7u;   /* AZ_MM_BUD is 0 */
}

/* A zeroed AzWorld for a test that fills a few fields by hand instead of
 * calling az_world_create. AzWorld is over-aligned (it holds cache-line
 * aligned members), so calloc, which promises 16 bytes, must not be used for
 * it: the UBSan trap build stops on the first member access through such a
 * pointer (found in test_brain, plan T6). Free with az_test_shell_free. */
static inline AzWorld *az_test_shell(void)
{
    AzWorld *w = (AzWorld *)_aligned_malloc(sizeof(AzWorld), _Alignof(AzWorld));
    if (w) memset(w, 0, sizeof *w);
    return w;
}

static inline void az_test_shell_free(AzWorld *w) { _aligned_free(w); }

#endif
