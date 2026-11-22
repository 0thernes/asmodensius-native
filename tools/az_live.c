/* az_live.c - one living world to watch and steer (hosted C, floats allowed).
 *
 *   az_live --dir DIR [--n 20000] [--lab [--seed 42]] [--grid x,y,z] [--cap 0]
 *           [--species 100] [--lesion 0x0] [--param name=value,...] [--load FILE]
 *           [--threads 0] [--kernels 0] [--size 960x540] [--every 1] [--fps 8]
 *           [--keep 8] [--max-ticks 0] [--label TEXT]
 *
 * Makes one world as az_run does (az_run's defaults for every option not
 * listed here; without --grid the grid follows --n, and --cap 0 means
 * n + n/2 + 4096) and ticks it until it is told to stop. Every --every ticks
 * it renders the world with az_view_render, writes DIR/frame-NNNNNNNN.png
 * (the newest --keep frames stay on disk) and prints one JSON line on stdout.
 * It is the engine half of the live demo (demo/live_server.py) and also runs
 * on its own.
 *
 * The world is WILD unless --lab is given: made of fresh entropy from the
 * operating system, 16 more bytes drawn every tick, no seed, never the same
 * twice. --lab is the seeded, repeatable test bench (--seed, 42 by default;
 * --seed without --lab is refused). Matter and energy are compared with their
 * starting totals after every tick, in both modes. The view is an observer
 * (ARCHITECTURE rule D7): each line carries the state hash taken before and
 * after rendering, and the two must be equal.
 *
 * Commands arrive one per line on stdin when stdin is a pipe:
 *   run | pause | step [K] | every K | fps F | camera YAW PITCH DIST |
 *   size WxH | save FILE | quit
 * The end of the pipe (the GUI went away) means quit. A console stdin sends
 * no commands: the world runs until --max-ticks, or until Ctrl+C.
 * The arena is locked in RAM; a failed lock is an error (no pagefile).
 *
 * Exit codes: 0 ok, 1 matter or energy differed from the start after some
 * tick, a frame or snapshot could not be written, or the view changed the
 * state; 2 bad options or setup failed; 3 the arena could not be locked.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "../core/az_abi.h"
#include "../platform/win/az_host_win.h"
#include "../kernels/az_kernels.h"
#ifdef AZ_HAVE_GPU
#include "../gpu/az_gpu.h"
#endif
#include "../view/az_view.h"
#include "az_tools.h"
#include "az_stats_report.h"
#include "az_teaching_observer.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#define LIVE_PATH 1200

static const char *const opt_value[] = {
    "--dir", "--n", "--seed", "--grid", "--cap", "--species", "--lesion", "--param", "--load", "--threads",
    "--kernels", "--gpu", "--prefetch", "--size", "--every", "--fps", "--keep", "--max-ticks", "--label",
    "--sps", "--tps", "--pace", "--observer-ms", NULL
};
static const char *const opt_flag[] = { "--lab", "--stream", "--ecores", "--large-pages", NULL };

static int usage(void)
{
    printf("usage: az_live --dir DIR [--n N] [--lab [--seed S]] [--grid x,y,z] [--cap C] [--species 1..100]\n"
           "               [--lesion 0..0x%x] [--param name=value,...] [--load FILE] [--threads 0..%u] [--ecores]\n"
           "               [--large-pages] [--kernels 0|1] [--gpu 0|1] [--prefetch 0|1] [--size WxH] [--every K]\n"
           "               [--fps F] [--keep K] [--max-ticks T] [--label TEXT]\n"
           "               [--stream [--sps 1..60] [--tps T] [--observer-ms M] [--pace free|picture]]\n"
           "  The world is wild (fresh entropy every tick, no seed, never the same twice) unless --lab is given;\n"
           "  --lab is the seeded, repeatable test bench, and --seed needs it.\n"
           "  stdin commands: run | pause | step [K] | every K | fps F | camera YAW PITCH DIST | size WxH |\n"
           "                  save FILE | quit\n",
           AZ_LESION_ALL, AZ_MAX_WORKERS);
    return 2;
}

/* "WxH", each a whole number from 64 to 4096 */
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
    if (a < 64 || a > 4096 || b < 64 || b > 4096) return false;
    *iw = (u32)a;
    *ih = (u32)b;
    return true;
}

/* the 5x7 view font at scale 2, white on a dark box, clipped to the image (as az_snap draws it, but in
 * the bottom left corner: the view's clade legend is in the top left) */
static void draw_label(u32 *px, u32 w, u32 h, const char *text)
{
    const u32 scale = 2, x0 = 8, y0 = h > 7 * scale + 12 ? h - 7 * scale - 8 : 4, len = (u32)strlen(text);
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

/* a JSON string: quotes, backslashes and control characters escaped */
static void json_str(const char *s)
{
    putchar('"');
    for (; *s; s++) {
        const unsigned char ch = (unsigned char)*s;
        if (ch == '"' || ch == '\\') { putchar('\\'); putchar((int)ch); }
        else if (ch < 0x20) printf("\\u%04x", (unsigned)ch);
        else putchar((int)ch);
    }
    putchar('"');
}

static void event_msg(const char *event, const char *what, const char *detail)
{
    printf("{\"event\":");
    json_str(event);
    printf(",\"what\":");
    json_str(what);
    printf(",\"detail\":");
    json_str(detail ? detail : "");
    printf("}\n");
    fflush(stdout);
}

/* the whole file in a malloc'd buffer; NULL on any error */
static u8 *read_file(const char *path, u64 *n_out)
{
    *n_out = 0;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    u8 *buf = NULL;
    long long len = -1;
    if (_fseeki64(f, 0, SEEK_END) == 0) len = _ftelli64(f);
    if (len > 0 && _fseeki64(f, 0, SEEK_SET) == 0 && (buf = malloc((size_t)len)) != NULL &&
        fread(buf, 1, (size_t)len, f) != (size_t)len) {
        free(buf);
        buf = NULL;
    }
    fclose(f);
    if (buf) *n_out = (u64)len;
    return buf;
}

/* az_snapshot_save into FILE; the number of bytes written, 0 on failure */
static u64 save_world(const AzWorld *w, const char *path)
{
    const u64 need = az_snapshot_bytes(w);
    u64 got = 0;
    u8 *buf = need ? malloc((size_t)need) : NULL;
    if (buf) got = az_snapshot_save(w, buf, need);
    bool ok = false;
    if (got) {
        FILE *f = fopen(path, "wb");
        if (f) {
            ok = fwrite(buf, 1, (size_t)got, f) == (size_t)got;
            ok = (fclose(f) == 0) && ok;
        }
    }
    free(buf);
    return ok ? got : 0;
}

/* ---- commands on stdin ---------------------------------------------------- */

typedef struct LiveCtl {
    bool paused, quit, redraw, resize, want_save;
    u32 every, fps, step;
    float yaw, pitch, dist;
    u32 iw, ih;
    char save_path[LIVE_PATH];
} LiveCtl;

typedef struct LiveIn {
    HANDLE h;
    bool pipe;
    u32 len;
    char line[LIVE_PATH + 64];
} LiveIn;

static bool finite_in(float v, float lo, float hi)
{
    return v == v && v >= lo && v <= hi;       /* v == v is false for NaN */
}

static void command(LiveCtl *c, char *line)
{
    char word[16];
    int used = 0;
    if (sscanf(line, "%15s%n", word, &used) != 1) return;
    const char *rest = line + used;
    unsigned a = 0, b = 0;
    float x = 0, y = 0, z = 0;
    /* run and pause draw one picture at once, so a viewer sees the new state even before the next tick */
    if (!strcmp(word, "run")) { c->paused = false; c->redraw = true; }
    else if (!strcmp(word, "pause")) { c->paused = true; c->redraw = true; }
    else if (!strcmp(word, "step")) {
        if (sscanf(rest, "%u", &a) != 1 || a == 0) a = 1;
        if (a > 100000) a = 100000;
        c->step = c->step + a > 1000000u ? 1000000u : c->step + a;
    } else if (!strcmp(word, "every")) {
        if (sscanf(rest, "%u", &a) == 1 && a >= 1 && a <= 10000) c->every = a;
        else event_msg("error", "every", "needs a whole number from 1 to 10000");
    } else if (!strcmp(word, "fps")) {
        if (sscanf(rest, "%u", &a) == 1 && a >= 1 && a <= 60) c->fps = a;
        else event_msg("error", "fps", "needs a whole number from 1 to 60");
    } else if (!strcmp(word, "camera")) {
        if (sscanf(rest, "%f %f %f", &x, &y, &z) == 3 && finite_in(x, -1000.0f, 1000.0f) &&
            finite_in(y, -1.55f, 1.55f) && finite_in(z, 0.0f, 1.0e6f)) {
            c->yaw = x; c->pitch = y; c->dist = z; c->redraw = true;
        } else event_msg("error", "camera", "needs YAW PITCH DIST (pitch -1.55..1.55, dist 0 = fit the world)");
    } else if (!strcmp(word, "size")) {
        if (sscanf(rest, "%ux%u", &a, &b) == 2 && a >= 64 && a <= 4096 && b >= 64 && b <= 4096) {
            c->iw = a; c->ih = b; c->resize = true; c->redraw = true;
        } else event_msg("error", "size", "needs WxH, each from 64 to 4096");
    } else if (!strcmp(word, "save")) {
        while (*rest == ' ' || *rest == '\t') rest++;
        const size_t n = strlen(rest);
        if (n > 0 && n < sizeof c->save_path) {
            memcpy(c->save_path, rest, n + 1);
            c->want_save = true;
        } else event_msg("error", "save", "needs a file name");
    } else if (!strcmp(word, "quit")) c->quit = true;
    else event_msg("error", "unknown command", word);
}

/* read whatever commands have arrived; false at the end of the pipe */
static bool input_poll(LiveIn *in, LiveCtl *c)
{
    if (!in->pipe) return true;
    for (;;) {
        DWORD avail = 0;
        if (!PeekNamedPipe(in->h, NULL, 0, NULL, &avail, NULL)) return false;
        if (avail == 0) return true;
        char buf[512];
        DWORD want = avail < (DWORD)sizeof buf ? avail : (DWORD)sizeof buf, got = 0;
        if (!ReadFile(in->h, buf, want, &got, NULL) || got == 0) return false;
        for (DWORD i = 0; i < got; i++) {
            const char ch = buf[i];
            if (ch == '\n' || ch == '\r') {
                if (in->len) {
                    in->line[in->len] = 0;
                    command(c, in->line);
                    in->len = 0;
                }
            } else if (in->len + 1 < (u32)sizeof in->line) {
                in->line[in->len++] = ch;
            }
        }
    }
}

/* delete frames older than the newest `keep`; a frame still open elsewhere is tried again later */
static void prune(const char *dir, u32 newest, u32 keep, u32 *oldest)
{
    char path[LIVE_PATH + 32];
    while (*oldest + keep <= newest) {
        snprintf(path, sizeof path, "%s\\frame-%08u.png", dir, *oldest);
        if (remove(path) != 0 && errno != ENOENT) break;
        (*oldest)++;
    }
}

/* how unlike each other the living are (az_run's creatures line): min, max and sum */
typedef struct Spread { u32 lo, hi; u64 sum; } Spread;

static void spread_add(Spread *s, u32 v)
{
    if (v < s->lo) s->lo = v;
    if (v > s->hi) s->hi = v;
    s->sum += v;
}

static void spread_print(const char *name, const Spread *s, u64 n, bool comma)
{
    printf("%s\"%s\":[%u,%u,%.2f]", comma ? "," : "", name, n ? s->lo : 0u, s->hi, n ? (double)s->sum / (double)n : 0.0);
}

#include "az_stream_loop.h"

int main(int argc, char **argv)
{
    const char *dir = NULL, *size = "960x540", *params = NULL, *load = NULL, *label = "asmodensius-native live";
    u32 n = 20000, cap = 0, species = 100, lesion = 0, threads = 0, kernels = 0, gpu_req = 0, prefetch = 1;
    u32 every = 1, fps = 8, keep = 8, max_ticks = 0;
    u64 seed = 42;
    u32 grid[3] = { 6, 6, 5 }, iw = 960, ih = 540;
    bool grid_given = false, lab = false, stream = false, ecores = false, large_pages = false;
    u32 sps = 15, tps = 0, observer_ms = 0;
    const char *pace = "free";

    bool ok = azt_known(argc, argv, 1, opt_value, opt_flag) &&
              azt_str(argc, argv, 1, "--dir", &dir) &&
              azt_u32(argc, argv, 1, "--n", 1, AZ_CAP_MAX, &n) &&
              azt_u64(argc, argv, 1, "--seed", 0, UINT64_MAX, &seed) &&
              azt_grid(argc, argv, 1, "--grid", 12, grid, &grid_given) &&
              azt_u32(argc, argv, 1, "--cap", 0, AZ_CAP_MAX, &cap) &&
              azt_u32(argc, argv, 1, "--species", 1, 100, &species) &&
              azt_u32(argc, argv, 1, "--lesion", 0, AZ_LESION_ALL, &lesion) &&
              azt_str(argc, argv, 1, "--param", &params) &&
              azt_str(argc, argv, 1, "--load", &load) &&
              azt_u32(argc, argv, 1, "--threads", 0, AZ_MAX_WORKERS, &threads) &&
              azt_u32(argc, argv, 1, "--kernels", 0, 1, &kernels) &&
              azt_u32(argc, argv, 1, "--gpu", 0, 1, &gpu_req) &&
              azt_u32(argc, argv, 1, "--prefetch", 0, 1, &prefetch) &&
              azt_str(argc, argv, 1, "--size", &size) &&
              azt_u32(argc, argv, 1, "--every", 1, 10000, &every) &&
              azt_u32(argc, argv, 1, "--fps", 1, 60, &fps) &&
              azt_u32(argc, argv, 1, "--keep", 2, 1000, &keep) &&
              azt_u32(argc, argv, 1, "--max-ticks", 0, 0xFFFFFFFFu, &max_ticks) &&
              azt_str(argc, argv, 1, "--label", &label) &&
              azt_flag(argc, argv, 1, "--lab", &lab) &&
              azt_flag(argc, argv, 1, "--stream", &stream) &&
              azt_flag(argc, argv, 1, "--ecores", &ecores) &&
              azt_flag(argc, argv, 1, "--large-pages", &large_pages) &&
              azt_u32(argc, argv, 1, "--sps", 1, 60, &sps) &&
              azt_u32(argc, argv, 1, "--tps", 0, UINT32_MAX, &tps) &&
              azt_u32(argc, argv, 1, "--observer-ms", 0, UINT32_MAX, &observer_ms) &&
              azt_str(argc, argv, 1, "--pace", &pace);
    if (strcmp(pace, "free") && strcmp(pace, "picture")) ok = false;
    if (!stream && (azt_find(argc, argv, 1, "--sps") >= 0 || azt_find(argc, argv, 1, "--tps") >= 0 ||
        azt_find(argc, argv, 1, "--observer-ms") >= 0 || azt_find(argc, argv, 1, "--pace") >= 0)) ok = false;
    if (ok && !lab && azt_find(argc, argv, 1, "--seed") >= 0) {
        fprintf(stderr, "--seed needs --lab: a wild world has no seed (wild is the default; --lab is the seeded "
                "test bench)\n");
        ok = false;
    }
    if (ok && load) {
        static const char *const world_opts[] = { "--n", "--seed", "--grid", "--cap", "--species", "--lesion",
                                                  "--param", "--lab", NULL };
        for (u32 i = 0; world_opts[i]; i++)
            if (azt_find(argc, argv, 1, world_opts[i]) >= 0) {
                fprintf(stderr, "option %s: --load takes the world from the snapshot\n", world_opts[i]);
                ok = false;
                break;
            }
    }
    if (!ok) return usage();
    if (stream && !dir) dir = ".";
    if (!dir) { fprintf(stderr, "option --dir is required\n"); return usage(); }
    if (strlen(dir) > LIVE_PATH - 40) { fprintf(stderr, "option --dir: the path is too long\n"); return usage(); }
    if (grid_given && (grid[0] < 2 || grid[1] < 2 || grid[0] + grid[1] + grid[2] > 26)) {
        fprintf(stderr, "option --grid: %u,%u,%u needs x and y of at least 2 bits and at most 26 bits in all\n",
                grid[0], grid[1], grid[2]);
        return usage();
    }
    if (!parse_size(size, &iw, &ih)) {
        fprintf(stderr, "option --size: '%s' is not WxH with whole numbers from 64 to 4096\n", size);
        return usage();
    }
    if (!cap) {
        const u64 c = (u64)n + n / 2 + 4096;
        if (c > AZ_CAP_MAX) {
            fprintf(stderr, "option --n: %u makes the default cap %" PRIu64 " too large; give --cap\n", n, c);
            return usage();
        }
        cap = (u32)c;
    }
    if (!grid_given) azt_grid_for(n, grid);

    AzConfig cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.mode = lab ? AZ_MODE_LAB : AZ_MODE_WILD;    /* wild is the living world; lab the test bench */
    cfg.seed = lab ? seed : 0;
    cfg.n_initial = n;
    cfg.cap = cap;
    cfg.n_species = species;
    cfg.gx_bits = (u8)grid[0]; cfg.gy_bits = (u8)grid[1]; cfg.gz_bits = (u8)grid[2];
    cfg.reorder_period = 16;                       /* az_run's defaults */
    cfg.tier1_period = 32;
    cfg.tier2_period = 256;
    cfg.field_stride = 8;
    cfg.lesion = lesion;
    cfg.kernels = kernels;

    u8 *snap = NULL;
    u64 snap_n = 0;
    if (load) {
        snap = read_file(load, &snap_n);
        if (!snap) { fprintf(stderr, "--load: cannot read '%s'\n", load); return 2; }
        AzConfig sc;
        if (!az_snapshot_config(snap, snap_n, &sc)) {
            fprintf(stderr, "--load: '%s' is not a valid az snapshot (header, version or checksum)\n", load);
            free(snap);
            return 2;
        }
        cfg = sc;
    }

    AzWinInfo info;
    memset(&info, 0, sizeof info);
    const u32 hflags = ecores ? AZ_WIN_USE_ECORES : 0u;
    const u32 aflags = AZ_WIN_LOCK | (large_pages ? AZ_WIN_LARGE_PAGES : 0u);
    AzHost *host = az_win_host_create(threads, hflags, &info);
    if (!host) { fprintf(stderr, "host creation failed\n"); free(snap); return 2; }
    const u64 bytes = az_world_bytes(&cfg);
    if (!bytes) {
        fprintf(stderr, "the world config is invalid\n");
        az_win_host_destroy(host);
        free(snap);
        return 2;
    }
    AzArena arena;
    AztTeaching teaching;
    if (!azt_teaching_arena(&arena, bytes, cfg.cap, aflags, &info, !stream, &teaching)) {
        fprintf(stderr, "error: the %.1f MB arena %s (%s)\n", (double)bytes / 1048576.0,
                info.arena_refused ? "could not be locked in RAM; try fewer creatures" : "could not be allocated",
                info.note);
        az_win_host_destroy(host);
        free(snap);
        return info.arena_refused ? 3 : 2;
    }
    AzArena world_arena = arena;
    world_arena.size = bytes;
    AzWorld *w = snap ? az_snapshot_load(&world_arena, snap, snap_n, host) : az_world_create(&world_arena, &cfg, host);
    free(snap);
    if (!w) {
        fprintf(stderr, load ? "--load: the snapshot was refused (damaged, or another ABI)\n"
                             : "world creation failed (bad config or arena too small)\n");
        az_win_host_destroy(host);
        az_win_arena_free(&arena);
        return 2;
    }
    if (w->rock_repaired_on_load) fprintf(stderr, "--load: repaired %u live positions in bedrock\n", w->rock_repaired_on_load);
    w->no_prefetch = prefetch ? 0u : 1u;
    if (w->cfg.kernels) az_kernels_install(w, AZ_K_ALL);
    bool gpu_active = false;
    char gpu_name[128] = "";
#ifdef AZ_HAVE_GPU
    AzGpuTick *gpu_tick = NULL;
    if (gpu_req) {
        AzGpuInfo gi;
        memset(&gi, 0, sizeof gi);
        int gs = az_gpu_tick_create(&gpu_tick, w->cap, &gi);
        if (gs == AZ_GPU_OK) {
            az_gpu_tick_attach(gpu_tick, host);
            gpu_active = true;
            snprintf(gpu_name, sizeof gpu_name, "%s", gi.device_name);
        } else {
            fprintf(stderr, "warning: --gpu 1 requested but unavailable (%s: %s); running on CPU\n",
                    az_gpu_status_name(gs), gi.msg);
        }
    }
#else
    if (gpu_req) fprintf(stderr, "warning: --gpu 1 requested but this binary has no GPU component; running on CPU\n");
#endif
    if (params && !azt_params(w, params)) {
#ifdef AZ_HAVE_GPU
        if (gpu_tick) { az_gpu_tick_detach(gpu_tick, host); az_gpu_tick_destroy(gpu_tick); }
#endif
        az_win_host_destroy(host);
        az_win_arena_free(&arena);
        return 2;
    }
    if (!load) az_worldgen(w);

    const bool wild = w->cfg.mode == AZ_MODE_WILD;
    const u64 tick0 = w->tick, matter0 = az_total_matter(w);
    const i64 energy0 = az_total_energy(w);

    LiveCtl c;
    memset(&c, 0, sizeof c);
    c.every = every; c.fps = fps;
    c.yaw = 0.6f; c.pitch = 0.5f; c.dist = 0.0f;   /* az_view's default camera */
    c.iw = iw; c.ih = ih;
    c.redraw = true;                               /* frame 0: the world as worldgen (or the snapshot) left it */
    LiveIn in;
    memset(&in, 0, sizeof in);
    in.h = GetStdHandle(STD_INPUT_HANDLE);
    in.pipe = in.h && in.h != INVALID_HANDLE_VALUE && GetFileType(in.h) == FILE_TYPE_PIPE;

    if (stream) {
        int rc = stream_run(w, &in, &c, sps, tps, observer_ms, !strcmp(pace, "free"), max_ticks,
                            info.arena_locked, gpu_active, gpu_name, ecores, info.arena_large, prefetch, w->cfg.kernels);
#ifdef AZ_HAVE_GPU
        if (gpu_tick) { az_gpu_tick_detach(gpu_tick, host); az_gpu_tick_destroy(gpu_tick); }
#endif
        az_win_host_destroy(host);
        az_win_arena_free(&arena);
        return rc;
    }

    AzView *v = az_view_create(c.iw, c.ih);
    u32 *px = malloc((size_t)c.iw * c.ih * sizeof *px);
    if (!v || !px) {
        fprintf(stderr, "out of memory for a %ux%u view\n", c.iw, c.ih);
#ifdef AZ_HAVE_GPU
        if (gpu_tick) { az_gpu_tick_detach(gpu_tick, host); az_gpu_tick_destroy(gpu_tick); }
#endif
        az_win_host_destroy(host);
        az_win_arena_free(&arena);
        return 2;
    }

    azt_teaching_attach(&teaching, w, load != NULL);
#ifdef AZ_INTAKE_OBSERVER
    (void)azt_intake_attach(w, !load, teaching.intake_tail, teaching.intake_bytes);
#ifdef AZ_PARTNER_OBSERVER
    (void)azt_partner_attach(w, !load, teaching.partner_tail, teaching.partner_bytes);
#endif
#ifdef AZ_HERMIT_OBSERVER
#ifdef AZ_BUILDER_OBSERVER
    (void)azt_builder_attach(w, teaching.builder_tail, teaching.builder_bytes);
#endif
    (void)azt_hermit_attach(w, teaching.hermit_tail, teaching.hermit_bytes, azt_record_refusal, &teaching);
#endif
#endif
    printf("{\"event\":\"start\",\"abi\":%u,\"mode\":\"%s\",", AZ_ABI_VERSION, wild ? "wild" : "lab");
    if (wild) printf("\"origin\":\"%08x%08x\",", w->key[1], w->key[0]);
    else printf("\"seed\":%" PRIu64 ",", w->cfg.seed);
    printf("\"n\":%u,\"cap\":%u,\"species\":%u,\"grid\":[%u,%u,%u],\"cells\":%u,\"lesion\":%u,\"workers\":%u,"
           "\"arena_mb\":%.1f,\"locked\":%u,\"large_pages\":%u,\"kernels\":%u,\"prefetch\":%u,\"ecores\":%s,"
           "\"gpu\":%s,\"gpu_name\":",
           w->cfg.n_initial, w->cap, w->cfg.n_species, w->gx, w->gy, w->gz, w->n_cells, w->cfg.lesion,
           host->n_workers, (double)arena.size / 1048576.0, info.arena_locked, info.arena_large,
           w->cfg.kernels, prefetch, ecores ? "true" : "false", gpu_active ? "true" : "false");
    json_str(gpu_name);
    printf(",\"size\":[%u,%u],\"every\":%u,\"fps\":%u,\"tick\":%" PRIu64 ","
           "\"loaded\":%s,\"matter0\":%" PRIu64 ",\"energy0\":%" PRId64 ",\"pipe\":%s,\"label\":",
           c.iw, c.ih, c.every, c.fps,
           w->tick, load ? "true" : "false", matter0, energy0, in.pipe ? "true" : "false");
    json_str(label);
    printf("}\n");
    fflush(stdout);

    u32 frame = 0, oldest = 0, unbalanced = 0, write_fail = 0, observer_fail = 0;
    bool done = false, first = true;               /* frame 0 shows the world before its first tick */
    u64 prof_prev[AZ_PROF_COUNT];
    for (u32 k = 0; k < AZ_PROF_COUNT; k++) prof_prev[k] = w->prof_ns[k];
    u64 t_frame = host->now_ns(host->ctx);
    char path[LIVE_PATH + 32], text[256];

    for (;;) {
        if (!input_poll(&in, &c)) c.quit = true;
        if (c.quit) break;
        if (c.want_save) {
            c.want_save = false;
            const u64 got = save_world(w, c.save_path);
            if (got) {
                printf("{\"event\":\"saved\",\"tick\":%" PRIu64 ",\"bytes\":%" PRIu64 ",\"hash\":\"%016" PRIx64
                       "\",\"file\":", w->tick, got, az_state_hash(w));
                json_str(c.save_path);
                printf("}\n");
                fflush(stdout);
            } else {
                write_fail++;
                event_msg("error", "save", c.save_path);
            }
        }
        if (c.resize) {
            c.resize = false;
            AzView *nv = az_view_create(c.iw, c.ih);
            u32 *npx = malloc((size_t)c.iw * c.ih * sizeof *npx);
            if (nv && npx) {
                az_view_destroy(v);
                free(px);
                v = nv;
                px = npx;
            } else {
                if (nv) az_view_destroy(nv);
                free(npx);
                c.iw = az_view_width(v);
                c.ih = az_view_height(v);
                event_msg("error", "size", "out of memory; the old size stays");
            }
        }

        u32 todo = 0;
        if (first) {
            first = false;                         /* c.redraw is set: render without ticking */
        } else if (c.step) {
            todo = c.step > 1000 ? 1000 : c.step;   /* long steps go out in pieces, so commands stay answered */
            c.step -= todo;
        } else if (!c.paused && !done) {
            const u64 now = host->now_ns(host->ctx);
            if (now - t_frame < 1000000000ull / c.fps) { Sleep(2); continue; }
            todo = c.every;
        }
        if (max_ticks) {
            const u64 ran = w->tick - tick0;
            if (ran >= max_ticks) {
                todo = 0;
                if (!done) {
                    done = true;
                    printf("{\"event\":\"max-ticks\",\"tick\":%" PRIu64 "}\n", w->tick);
                    fflush(stdout);
                    if (!in.pipe) break;           /* nobody can send a command: the run is over */
                }
            } else if (todo > max_ticks - ran) {
                todo = (u32)(max_ticks - ran);
            }
        }
        if (!todo && !c.redraw) { Sleep(10); continue; }

        t_frame = host->now_ns(host->ctx);
        for (u32 t = 0; t < todo; t++) {
            az_tick(w);
            if (az_total_matter(w) != matter0 || az_total_energy(w) != energy0) unbalanced++;
        }
        const u64 t_ticked = host->now_ns(host->ctx);

        const u64 h0 = az_state_hash(w);
        az_view_camera(v, c.yaw, c.pitch, c.dist, NULL);
        az_view_render(v, w);
        const u64 h1 = az_state_hash(w);
        if (h0 != h1) observer_fail++;
        const u64 t_drawn = host->now_ns(host->ctx);

        AzViewInfo vi;
        az_view_info(v, &vi);
        const size_t np = (size_t)c.iw * c.ih;
        memcpy(px, az_view_pixels(v), np * sizeof *px);
        snprintf(text, sizeof text, "%s  %s  tick %" PRIu64 "  live %u", label, wild ? "wild" : "lab", w->tick,
                 vi.n_live);
        draw_label(px, c.iw, c.ih, text);
        size_t png_len = 0;
        u8 *png = az_png_encode(px, c.iw, c.ih, &png_len);
        bool wrote = false;
        if (png) {
            snprintf(path, sizeof path, "%s\\frame-%08u.png", dir, frame);
            FILE *f = fopen(path, "wb");
            if (f) {
                wrote = fwrite(png, 1, png_len, f) == png_len;
                wrote = (fclose(f) == 0) && wrote;
            }
            free(png);
        }
        if (!wrote) {
            write_fail++;
            event_msg("error", "frame", path);
        }
        const u64 t_written = host->now_ns(host->ctx);

        AzStats st;
        az_stats_collect(w, &st);
#ifdef AZ_INTAKE_OBSERVER
#ifdef AZ_PARTNER_OBSERVER
        azt_partner_prepare(w);
#endif
        azt_intake_apply(w, &st);
        AztTeachingSummary teaching_summary = azt_teaching_apply(&teaching, w, &st, azt_intake_fill);
#else
        AztTeachingSummary teaching_summary = azt_teaching_apply(&teaching, w, &st, NULL);
#endif
        Spread sg = { ~0u, 0, 0 }, sh = { ~0u, 0, 0 }, sp = { ~0u, 0, 0 }, su = { ~0u, 0, 0 };
        u64 nl = 0;
        u32 tiles_max = 0;
        for (u32 s = 0; s < w->n_slots; s++) {
            if (!(w->flags[s] & AZ_F_ALIVE) || !w->blk[s]) continue;
            u32 ga, gb, bb, nparts;
            (void)az_genes(w, s, 0, &ga);
            (void)az_genes(w, s, 1, &gb);
            const AzBrain *br = az_brain(w, s, &bb);
            (void)az_parts(w, s, &nparts);
            const u32 hid = bb >= sizeof(AzBrain) ? 8u * br->n_hb : 0u;
            const u32 tiles = bb >= sizeof(AzBrain) ? br->n_t1 + br->n_t2 : 0u;
            nl++;
            spread_add(&sg, ga + gb);
            spread_add(&sh, hid);
            spread_add(&sp, nparts);
            spread_add(&su, az_blk(w, s)->units);
            if (tiles > tiles_max) tiles_max = tiles;
        }

        const double ms_tick = todo ? (double)(t_ticked - t_frame) / 1e6 / todo : 0.0;
        printf("{\"frame\":%u,\"file\":\"frame-%08u.png\",\"tick\":%" PRIu64 ",\"ticks\":%u,\"live\":%u,\"eggs\":%u,"
               "\"clades\":%u,\"colonies\":%u,\"max_gen\":%u,\"mean_energy\":%.2f,\"births\":%" PRIu64 ","
               "\"deaths\":%" PRIu64 ",\"cap_denied\":%" PRIu64 ",\"store_denied\":%" PRIu64 ","
               "\"matter\":%" PRIu64 ",\"energy\":%" PRId64 ",\"unbalanced\":%u,\"hash\":\"%016" PRIx64 "\","
               "\"hash_after_view\":\"%016" PRIx64 "\",\"view_hash\":\"%016" PRIx64 "\",\"drawn\":%u,"
               "\"marched\":%u,\"ms_tick\":%.3f,\"ms_render\":%.2f,\"ms_png\":%.2f,\"entropy_missed\":%" PRIu64 ","
               "\"paused\":%s,\"every\":%u,\"fps\":%u,\"size\":[%u,%u],\"camera\":[%.3f,%.3f,%.1f],\"wrote\":%s",
               frame, frame, w->tick, todo, st.n_live, st.n_eggs, st.n_clades, st.n_colonies, st.max_generation,
               (double)st.mean_energy_q8 / 256.0, w->totals.births, w->totals.deaths, w->totals.cap_denied,
               w->totals.store_denied, az_total_matter(w), az_total_energy(w), unbalanced, h0, h1, az_view_hash(v),
               vi.n_drawn, vi.n_marched, ms_tick, (double)(t_drawn - t_ticked) / 1e6,
               (double)(t_written - t_drawn) / 1e6, w->totals.entropy_missed, c.paused ? "true" : "false",
               c.every, c.fps, c.iw, c.ih, (double)c.yaw, (double)c.pitch, (double)c.dist, wrote ? "true" : "false");
        printf(",\"pairs\":{");
        for (u32 a = 1; a < AZ_ACT_COUNT; a++)
            printf("%s\"%s\":%" PRIu64, a > 1 ? "," : "", azt_act_name(a), w->totals.interactions[a]);
        printf("},\"top\":[");
        for (u32 i = 0; i < vi.top_n; i++) printf("%s[%u,%u]", i ? "," : "", vi.top_clade[i], vi.top_count[i]);
        printf("],\"mating\":[");
        for (u32 i = 0; i < 8; i++) printf("%s%u", i ? "," : "", st.mating_mode[i]);
        printf("],\"census\":");
        azt_census_fields(stdout, &st);
        azt_teaching_json(stdout, &teaching_summary);
#ifndef AZ_INTAKE_OBSERVER
        fputs(",\"waste_conversion\":{\"available\":false}",stdout);
#endif
#ifdef AZ_INTAKE_OBSERVER
        azt_intake_json(stdout, w);
#ifdef AZ_COMMONS_OBSERVER
        azt_commons_json(stdout, w);
#endif
#ifdef AZ_PARTNER_OBSERVER
        azt_partner_json(stdout, w);
#endif
#ifdef AZ_HERMIT_OBSERVER
#ifdef AZ_BUILDER_OBSERVER
        azt_builder_json(stdout, w, teaching_summary.builder_covered, teaching_summary.builder_partial,
                         teaching_summary.builder_positive, teaching_summary.builder_exact);
#endif
        azt_hermit_json(stdout, w, teaching_summary.refusal_covered, teaching_summary.refusal_partial,
                        teaching_summary.refusal_positive, teaching_summary.refusal_exact);
#endif
#endif
        fputc('}', stdout);
        printf(",\"body\":{");
        spread_print("genes", &sg, nl, false);
        spread_print("neurons", &sh, nl, true);
        spread_print("parts", &sp, nl, true);
        spread_print("units", &su, nl, true);
        printf(",\"tiles_max\":%u},\"stages\":{", tiles_max);
        for (u32 k = 0; k < AZ_PROF_COUNT; k++) {
            const u64 d = w->prof_ns[k] - prof_prev[k];
            prof_prev[k] = w->prof_ns[k];
            printf("%s\"%s\":%.3f", k ? "," : "", azt_prof_name(k), todo ? (double)d / 1e6 / todo : 0.0);
        }
        printf("}}\n");
        fflush(stdout);

        c.redraw = false;
        if (wrote) prune(dir, frame, keep, &oldest);
        frame++;
    }

    const u64 h_end = az_state_hash(w);
    printf("{\"event\":\"end\",\"tick\":%" PRIu64 ",\"frames\":%u,\"unbalanced\":%u,\"write_fail\":%u,"
           "\"observer_fail\":%u,\"hash\":\"%016" PRIx64 "\"}\n", w->tick, frame, unbalanced, write_fail,
           observer_fail, h_end);
    fflush(stdout);
#ifdef AZ_INTAKE_OBSERVER
#ifdef AZ_HERMIT_OBSERVER
#ifdef AZ_BUILDER_OBSERVER
    azt_builder_detach();
#endif
    azt_hermit_detach();
#endif
#ifdef AZ_PARTNER_OBSERVER
    azt_partner_detach();
#endif
    azt_intake_detach();
#endif
    azt_teaching_detach(&teaching);
#ifdef AZ_HAVE_GPU
    if (gpu_tick) { az_gpu_tick_detach(gpu_tick, host); az_gpu_tick_destroy(gpu_tick); }
#endif
    free(px);
    az_view_destroy(v);
    az_win_host_destroy(host);
    az_win_arena_free(&arena);
    return (unbalanced || write_fail || observer_fail) ? 1 : 0;
}
