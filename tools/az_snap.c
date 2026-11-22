/* az_snap.c - one picture of a world (hosted C, floats allowed).
 *
 *   az_snap --out FILE.png [--label TEXT] [--n 100000] [--ticks 60]
 *           [--lab [--seed 42]] [--grid 6,6,5] [--cap 400000] [--species 100]
 *           [--threads 0] [--kernels 0] [--size 1280x720]
 *
 * Creates and ticks a world as az_run does (az_run's defaults for every option
 * not listed here; the arena is locked in RAM and a failed lock is an error),
 * renders it with az_view_render between ticks, draws the label and the state
 * hash in the top left corner and writes a PNG. It prints the state hash before
 * and after rendering: the view is an observer (ARCHITECTURE rule D7), so the
 * two must be equal.
 * The world is WILD unless --lab is given, as in az_run: made of fresh entropy
 * from the operating system, 16 more bytes drawn every tick, no seed, never
 * the same picture twice. --lab is the seeded, repeatable test bench (--seed,
 * 42 by default; --seed without --lab is refused); with --lab the hash before
 * rendering must also equal az_run --lab's final hash for the same options.
 * Matter and energy are compared with their starting totals after every tick,
 * in both modes: the balance is physics, not determinism.
 * Exit codes: 0 ok, 1 the hash changed, the balance broke or the PNG was not
 * written, 2 bad options or setup failed, 3 the arena could not be locked.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "../core/az_abi.h"
#include "../platform/win/az_host_win.h"
#include "../kernels/az_kernels.h"
#include "../view/az_view.h"
#include "az_tools.h"

#define CAP_MAX 0x7FFFFFC0u                       /* config_ok in core/az_world.c */

static const char *const opt_value[] = {
    "--out", "--label", "--n", "--ticks", "--seed", "--grid", "--cap", "--species", "--threads", "--kernels",
    "--size", NULL
};
static const char *const opt_flag[] = { "--lab", NULL };

static int usage(void)
{
    printf("usage: az_snap --out FILE.png [--label TEXT] [--n N] [--ticks T] [--lab [--seed S]] [--grid x,y,z]\n"
           "               [--cap C] [--species 1..65535] [--threads 0..%u] [--kernels 0|1] [--size WxH]\n"
           "  The world is wild (fresh entropy every tick, no seed, never the same twice) unless --lab is given;\n"
           "  --lab is the seeded, repeatable test bench, and --seed needs it.\n",
           AZ_MAX_WORKERS);
    return 2;
}

/* "WxH", each a whole number from 1 to 16384 (az_view_create's limit) */
static bool parse_size(const char *s, u32 *iw, u32 *ih)
{
    const char *x = strchr(s, 'x');
    char part[16];
    u64 a = 0, b = 0;
    size_t len = x ? (size_t)(x - s) : 0;
    if (!x || len == 0 || len >= sizeof part) return false;
    memcpy(part, s, len);
    part[len] = 0;
    if (!azt_parse_u64(part, &a) || !azt_parse_u64(x + 1, &b)) return false;
    if (a < 1 || a > 16384 || b < 1 || b > 16384) return false;
    *iw = (u32)a;
    *ih = (u32)b;
    return true;
}

/* the 5x7 view font at scale 2, white on a dark box, clipped to the image */
static void draw_label(u32 *px, u32 w, u32 h, const char *text)
{
    const u32 scale = 2, x0 = 8, y0 = 8, len = (u32)strlen(text);
    for (u32 y = y0 - 4; y < y0 + 7 * scale + 4 && y < h; y++)
        for (u32 x = x0 - 4; x < x0 + len * 6 * scale + 4 && x < w; x++) px[(size_t)y * w + x] = 0xFF101018u;
    for (u32 i = 0; i < len; i++)
        for (u32 r = 0; r < 7; r++) {
            u8 bits = az_view_font_row((u32)(unsigned char)text[i], r);
            for (u32 c = 0; c < 5; c++) {
                if (!(bits & (0x10u >> c))) continue;
                for (u32 dy = 0; dy < scale; dy++)
                    for (u32 dx = 0; dx < scale; dx++) {
                        u32 x = x0 + (i * 6 + c) * scale + dx, y = y0 + r * scale + dy;
                        if (x < w && y < h) px[(size_t)y * w + x] = 0xFFFFFFFFu;
                    }
            }
        }
}

int main(int argc, char **argv)
{
    /* defaults: the 100k reference config (az_run --n 100000 --ticks 60 --cap 400000 --grid 6,6,5) */
    const char *out = NULL, *label = "asmodensius-native", *size = "1280x720";
    u32 n = 100000, ticks = 60, cap = 400000, species = 100, threads = 0, kernels = 0;
    u64 seed = 42;
    u32 grid[3] = { 6, 6, 5 }, iw = 1280, ih = 720;
    bool grid_given = false, lab = false;
    bool parsed = azt_known(argc, argv, 1, opt_value, opt_flag) &&
              azt_str(argc, argv, 1, "--out", &out) &&
              azt_str(argc, argv, 1, "--label", &label) &&
              azt_u32(argc, argv, 1, "--n", 1, CAP_MAX, &n) &&
              azt_u32(argc, argv, 1, "--ticks", 0, 0xFFFFFFFFu, &ticks) &&
              azt_u64(argc, argv, 1, "--seed", 0, UINT64_MAX, &seed) &&
              azt_grid(argc, argv, 1, "--grid", 12, grid, &grid_given) &&
              azt_u32(argc, argv, 1, "--cap", 1, CAP_MAX, &cap) &&
              azt_u32(argc, argv, 1, "--species", 1, 65535, &species) &&
              azt_u32(argc, argv, 1, "--threads", 0, AZ_MAX_WORKERS, &threads) &&
              azt_u32(argc, argv, 1, "--kernels", 0, 1, &kernels) &&
              azt_str(argc, argv, 1, "--size", &size) &&
              azt_flag(argc, argv, 1, "--lab", &lab);
    if (parsed && !lab && azt_find(argc, argv, 1, "--seed") >= 0) {
        fprintf(stderr, "--seed needs --lab: a wild world has no seed (wild is the default; --lab is the seeded "
                "test bench)\n");
        parsed = false;
    }
    if (!parsed) return usage();
    if (!out) { fprintf(stderr, "option --out is required\n"); return usage(); }
    if (grid[0] < 2 || grid[1] < 2 || grid[0] + grid[1] + grid[2] > 26) {
        fprintf(stderr, "option --grid: %u,%u,%u needs x and y of at least 2 bits and at most 26 bits in all\n",
                grid[0], grid[1], grid[2]);
        return usage();
    }
    if (!parse_size(size, &iw, &ih)) {
        fprintf(stderr, "option --size: '%s' is not WxH with whole numbers from 1 to 16384\n", size);
        return usage();
    }
    AzConfig cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.mode = lab ? AZ_MODE_LAB : AZ_MODE_WILD;    /* wild is the living world; lab the test bench */
    cfg.seed = lab ? seed : 0;
    cfg.n_initial = n;
    cfg.cap = cap;
    cfg.n_species = (u16)species;
    cfg.gx_bits = (u8)grid[0]; cfg.gy_bits = (u8)grid[1]; cfg.gz_bits = (u8)grid[2];
    cfg.reorder_period = 16;                 /* az_run's defaults */
    cfg.tier1_period = 32;
    cfg.tier2_period = 256;
    cfg.field_stride = 8;
    cfg.kernels = kernels;

    AzWinInfo info;
    memset(&info, 0, sizeof info);
    AzHost *host = az_win_host_create(threads, 0, &info);
    if (!host) { fprintf(stderr, "host creation failed\n"); return 2; }
    AzArena arena;
    if (!az_win_arena(&arena, az_world_bytes(&cfg), AZ_WIN_LOCK, &info)) {
        fprintf(stderr, "error: arena %s (%s)\n",
                info.arena_refused ? "could not be locked in RAM" : "allocation failed", info.note);
        az_win_host_destroy(host);
        return info.arena_refused ? 3 : 2;
    }
    AzWorld *w = az_world_create(&arena, &cfg, host);
    if (!w) { fprintf(stderr, "world creation failed\n"); return 2; }
    if (cfg.kernels) az_kernels_install(w, AZ_K_ALL);
    az_worldgen(w);
    const u64 matter0 = az_total_matter(w);
    const i64 energy0 = az_total_energy(w);
    u32 unbalanced = 0;                      /* ticks after which a total differed from its start */
    for (u32 t = 0; t < ticks; t++) {
        az_tick(w);
        if (az_total_matter(w) != matter0 || az_total_energy(w) != energy0) unbalanced++;
    }

    u64 h0 = az_state_hash(w);
    AzView *v = az_view_create(iw, ih);
    if (!v) { fprintf(stderr, "bad --size %ux%u\n", iw, ih); return 2; }
    az_view_render(v, w);
    u64 h1 = az_state_hash(w);
    AzViewInfo vi;
    az_view_info(v, &vi);

    size_t np = (size_t)iw * ih;
    u32 *px = malloc(np * sizeof *px);
    if (!px) { fprintf(stderr, "out of memory\n"); return 2; }
    memcpy(px, az_view_pixels(v), np * sizeof *px);
    char text[256];
    snprintf(text, sizeof text, "%s  %s  tick %" PRIu64 "  live %u  hash %016" PRIx64,
             label, lab ? "lab" : "wild", w->tick, vi.n_live, h0);
    draw_label(px, iw, ih, text);
    bool ok = az_png_write(out, px, iw, ih);

    printf("arena: %.1f MB, locked=%u\n", (double)arena.size / 1048576.0, info.arena_locked);
    if (w->cfg.mode == AZ_MODE_WILD)
        printf("mode: WILD - made of fresh entropy from the operating system, 16 more bytes drawn every tick; no "
               "seed, never the same twice (origin %08x%08x; %" PRIu64 " ticks had no fresh entropy)\n",
               w->key[1], w->key[0], w->totals.entropy_missed);
    else
        printf("mode: LAB - the seeded, repeatable test bench (seed %" PRIu64 ")\n", w->cfg.seed);
    printf("life: births %" PRIu64 " deaths %" PRIu64 "; matter %" PRIu64 " and energy %" PRId64
           " %s after every one of %u ticks\n", w->totals.births, w->totals.deaths, matter0, energy0,
           unbalanced ? "NOT EQUAL to the start" : "equal to the start", ticks);
    printf("world: tick %" PRIu64 " live %u clades %u drawn %u (full silhouette %u); view hash %016" PRIx64 "\n",
           w->tick, vi.n_live, vi.n_clades, vi.n_drawn, vi.n_marched, az_view_hash(v));
    printf("state hash before render %016" PRIx64 " after %016" PRIx64 " (%s)\n", h0, h1,
           h0 == h1 ? "equal" : "CHANGED");
    printf("png: %s %s\n", out, ok ? "written" : "NOT WRITTEN");
    free(px);
    az_view_destroy(v);
    az_win_host_destroy(host);
    az_win_arena_free(&arena);
    return (ok && h0 == h1 && !unbalanced) ? 0 : 1;
}
