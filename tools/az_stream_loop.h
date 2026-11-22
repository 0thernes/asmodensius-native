/* Stream controller. The world thread owns reads, clock decisions and credit. */
#include "az_stream_capture.h"
#include "az_stream_writer.h"
#include "az_stream_hash.h"
#include <ctype.h>

typedef struct {
    StreamCapture capture;
    StreamWriter writer;
    StreamHasher hasher;
    AzWorld *world;
    u8 *shape_keys, *shape_seen;
    bool paused, quit, inflight, pipe, free_pace, refresh, had_snapshot, had_fields, have_target;
    u32 sps, tps, observer_ms, step, every, fps, width, height;
    u32 next_seq, outstanding, passed, failed, skipped, unbalanced;
    u64 last_checked_ns, checked_tick, prev_snap_tick, tick_ns, last_fields_ns, last_snap_ns;
    u64 tick_end_unix_ms;
    float yaw, pitch, dist, target[3];
    u64 matter0, matter;
    i64 energy0, energy;
} StreamState;

static u64 stream_unix_ms(void)
{
    FILETIME ft;
    GetSystemTimePreciseAsFileTime(&ft);
    u64 timestamp = ((u64)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return (timestamp - UINT64_C(116444736000000000)) / UINT64_C(10000);
}

/* Pay first-touch faults while opening the viewer, including for scratch
 * used only after the simulation first reorders creature slots. */
static void stream_touch(void *buffer, size_t bytes, size_t page)
{
    volatile u8 *p = buffer;
    for (size_t i = 0; i < bytes; i += page) p[i] = 0;
}

static u64 stream_now(const StreamState *s)
{
    return s->world->host->now_ns(s->world->host->ctx);
}

static void stream_check(StreamState *s, u64 before, u64 after)
{
    if (before != after) s->failed++;
    else s->passed++;
    s->checked_tick = s->world->tick;
    s->last_checked_ns = stream_now(s);
}

static bool stream_shapes(StreamState *s)
{
    const AzWorld *w = s->world;
    u32 needed = 0;
    for (u32 i = 0; i < w->n_clades; i++) {
        if (w->clades[i].alive && (!s->shape_seen[i] ||
            memcmp(s->shape_keys + (size_t)i * 16, w->clades[i].body, 16))) needed++;
    }
    if (!needed && s->had_snapshot) return true;
    ShapeKey *keys = malloc((needed ? needed : 1u) * sizeof *keys);
    if (!keys) return false;
    u32 j = 0;
    for (u32 i = 0; i < w->n_clades; i++) {
        const u8 *body = w->clades[i].body;
        u8 *prev = s->shape_keys + (size_t)i * 16;
        if (!w->clades[i].alive || (s->shape_seen[i] && !memcmp(prev, body, 16))) continue;
        keys[j].id = i; memcpy(keys[j++].body, body, 16);
        s->shape_seen[i] = 1; memcpy(prev, body, 16);
    }
    return stream_enqueue(&s->writer, AZST_SHAPES, keys, needed);
}

static bool stream_snapshot(StreamState *s, bool last)
{
    const AzWorld *w = s->world;
    StreamWriter *q = &s->writer;
    if (s->inflight || stream_queue_free(q) < 6 || InterlockedCompareExchange(&q->snap_busy, 1, 0)) {
        s->skipped++;
        return true;
    }
    u64 now = stream_now(s);
    bool check = !s->had_snapshot || s->paused || last || !s->observer_ms ||
                 now - s->last_checked_ns >= (u64)s->observer_ms * 1000000u;
    u64 before = check ? stream_state_hash(&s->hasher, w) : 0;
    u64 prehash_ns = stream_now(s) - now;
    u32 size = 0;
    if (!s->had_snapshot) {
        u8 *geometry = stream_static(w, &size);
        if (!stream_enqueue(q, AZST_STATIC, geometry, size)) return false;
    }
    if (!stream_shapes(s)) return false;
    u64 copy_start = stream_now(s);
    u32 n = stream_creatures(w, &s->capture);
    double ms_copy = (double)(stream_now(s) - copy_start) / 1e6;
    AzStreamHeader *h = (AzStreamHeader *)s->capture.snapshot;
    u8 *fields = NULL;
    u32 fields_size = 0;
    if ((!s->had_fields || now - s->last_fields_ns >= 1000000000u) &&
        !(w->tick % w->cfg.field_stride) && !InterlockedCompareExchange(&q->fields_busy, 1, 0)) {
        fields = stream_fields(w, &fields_size);
        if (!fields) return false;
    }
    u64 posthash_start = stream_now(s);
    u64 after = check ? stream_state_hash(&s->hasher, w) : 0;
    u64 captured = stream_now(s);
    double ms_hash = check ? (double)(prehash_ns + captured - posthash_start) / 1e6 : 0.0;
    double ms_read = (double)(captured - now) / 1e6;
    if (check) stream_check(s, before, after);
    u64 delta = w->tick - s->prev_snap_tick;
    h->delta = (u32)(delta > UINT32_MAX ? UINT32_MAX : delta);
    u64 us = delta ? s->tick_ns / delta / 1000u : 0;
    h->tick_us = (u32)(us > UINT32_MAX ? UINT32_MAX : us);
    h->before = before; h->after = after; h->failures = s->failed;
    char stat[1500];
    snprintf(stat, sizeof stat, "{\"event\":\"stats\",\"tick\":%" PRIu64 ",\"live\":%u,"
             "\"mode\":\"%s\",\"ticks\":%u,\"ms_tick\":%.3f,\"ms_copy\":%.4f,\"tick_end_unix_ms\":%" PRIu64 ","
             "\"ms_hash\":%.4f,\"ms_read\":%.4f,\"ticks_per_s\":%.4f,\"hash\":\"%016" PRIx64 "\",\"hash_after_view\":\"%016" PRIx64 "\","
             "\"observer_pass\":%u,\"observer_fail\":%u,\"observer_tick\":%" PRIu64 ","
             "\"unbalanced\":%u,\"skipped\":%u,\"paused\":%s,\"matter\":%" PRIu64 ","
             "\"energy\":%" PRId64 ",\"births\":%" PRIu64 ",\"deaths\":%" PRIu64 ","
             "\"sun_phase\":[%d,%d],\"sun_light\":[%d,%d],\"field_underflow\":%" PRIu64 ",\"dropped_events\":%" PRIu64 "}",
             w->tick, n, w->cfg.mode == AZ_MODE_WILD ? "wild" : "lab", h->delta, (double)us / 1000, ms_copy, s->tick_end_unix_ms,
             ms_hash, ms_read, s->had_snapshot && now > s->last_snap_ns ? (double)delta * 1e9 / (double)(now - s->last_snap_ns) : 0.0,
             before, after, s->passed, s->failed, s->checked_tick, s->unbalanced, s->skipped,
             s->paused ? "true" : "false", s->matter, s->energy, w->totals.births,
             w->totals.deaths, w->gstate[AZ_G_SUN0_PHASE], w->gstate[AZ_G_SUN1_PHASE],
             w->gstate[AZ_G_SUN0_LIGHT], w->gstate[AZ_G_SUN1_LIGHT], w->totals.field_underflow, w->totals.dropped_events);
    if (!stream_enqueue(q, AZST_CREATURES, s->capture.snapshot, (u32)(sizeof *h + (size_t)n * AZST_RECORD_BYTES))) {
        free(fields); return false;
    }
    if (fields && !stream_enqueue(q, AZST_FIELDS, fields, fields_size)) return false;
    if (fields) { s->last_fields_ns = now; s->had_fields = true; }
    if (!stream_json(q, stat)) return false;
    s->outstanding = s->next_seq++; s->inflight = s->pipe; s->had_snapshot = true;
    s->prev_snap_tick = w->tick; s->last_snap_ns = now; s->tick_ns = 0; s->refresh = false;
    return true;
}

static bool stream_picture(StreamState *s)
{
    StreamWriter *q = &s->writer;
    if (InterlockedCompareExchange(&q->proof_busy, 1, 0))
        return stream_json(q, "{\"event\":\"error\",\"what\":\"picture\",\"detail\":\"previous proof is still being drawn\"}");
    u64 before = stream_state_hash(&s->hasher, s->world);
    ProofScene *p = calloc(1, sizeof *p);
    if (!p) return false;
    p->scene = az_view_capture(s->world);
    if (!p->scene) { free(p); return false; }
    p->width = s->width; p->height = s->height;
    p->yaw = s->yaw; p->pitch = s->pitch; p->dist = s->dist;
    p->have_target = s->have_target; memcpy(p->target, s->target, sizeof p->target);
    u64 after = stream_state_hash(&s->hasher, s->world); stream_check(s, before, after);
    char line[500];
    snprintf(line, sizeof line, "{\"event\":\"picture\",\"tick\":%" PRIu64 ",\"hash\":\"%016" PRIx64
             "\",\"hash_after_view\":\"%016" PRIx64 "\",\"size\":[%u,%u],\"camera\":[%.8g,%.8g,%.8g],"
             "\"target\":[%.8g,%.8g,%.8g]}", s->world->tick, before, after, p->width, p->height,
             (double)p->yaw, (double)p->pitch, (double)p->dist,
             s->have_target ? (double)s->target[0] : s->world->gx * 0.5,
             s->have_target ? (double)s->target[1] : s->world->gy * 0.5,
             s->have_target ? (double)s->target[2] : s->world->gz * 0.5);
    if (!stream_json(q, line)) { stream_dispose(q, (StreamJob){AZST_PICTURE, 0, p}); return false; }
    return stream_enqueue(q, AZST_PICTURE, p, 0);
}

static bool stream_cmd(StreamState *s, char *line)
{
    char *arg = strpbrk(line, " \t");
    if (arg) { *arg++ = 0; while (*arg == ' ' || *arg == '\t') arg++; }
    else arg = line + strlen(line);
    u64 n = 0;
    bool number = azt_parse_u64(arg, &n) && n <= UINT32_MAX;
    if (!strcmp(line, "ack") && number) {
        if (s->inflight && s->outstanding == n) s->inflight = false;
    } else if (!strcmp(line, "pause") || !strcmp(line, "run")) {
        s->paused = !strcmp(line, "pause"); s->refresh = true;
    } else if (!strcmp(line, "step") && ((!*arg) || (number && n >= 1 && n <= 100000))) {
        s->paused = true; s->step += (u32)(*arg ? n : 1);
    } else if (!strcmp(line, "quit")) s->quit = true;
    else if (!strcmp(line, "sps") && number && n >= 1 && n <= 60) s->sps = (u32)n;
    else if (!strcmp(line, "tps") && number) s->tps = (u32)n;
    else if (!strcmp(line, "fps") && number && n >= 1 && n <= 60) s->fps = (u32)n;
    else if (!strcmp(line, "every") && number && n >= 1 && n <= 10000) s->every = (u32)n;
    else if (!strcmp(line, "observer-ms") && number) s->observer_ms = (u32)n;
    else if (!strcmp(line, "pace") && (!strcmp(arg, "free") || !strcmp(arg, "picture"))) s->free_pace = !strcmp(arg, "free");
    else if (!strcmp(line, "size") && parse_size(arg, &s->width, &s->height)) { /* proof dimensions */ }
    else if (!strcmp(line, "camera")) {
        float yaw, pitch, dist, tx, ty, tz;
        char extra;
        int got = sscanf(arg, "%f %f %f %f %f %f %c", &yaw, &pitch, &dist, &tx, &ty, &tz, &extra);
        if ((got == 3 || got == 6) && finite_in(yaw,-1000,1000) && finite_in(pitch,-1.55f,1.55f) &&
            finite_in(dist,0,1e6f) && (got == 3 || (isfinite(tx) && isfinite(ty) && isfinite(tz)))) {
            s->yaw = yaw; s->pitch = pitch; s->dist = dist; s->have_target = got == 6;
            if (s->have_target) {s->target[0] = tx; s->target[1] = ty; s->target[2] = tz;}
        } else goto invalid;
    } else if (!strcmp(line, "inspect") && strlen(arg) == 16) {
        for (u32 i = 0; i < 16; i++) if (!isxdigit((unsigned char)arg[i])) goto invalid;
        char response[1500];
        u64 before = stream_state_hash(&s->hasher, s->world);
        stream_inspect(s->world, strtoull(arg, NULL, 16), response, sizeof response);
        stream_check(s, before, stream_state_hash(&s->hasher, s->world));
        return stream_json(&s->writer, response);
    } else if (!strcmp(line, "picture")) return stream_picture(s);
    else if (!strcmp(line, "save") && *arg) {
        u64 got = save_world(s->world, arg);
        char response[128];
        snprintf(response, sizeof response, "{\"event\":\"%s\",\"what\":\"save\",\"bytes\":%" PRIu64 "}",
                 got ? "saved" : "error", got);
        return stream_json(&s->writer, response);
    } else {
invalid:
        return stream_json(&s->writer, "{\"event\":\"error\",\"what\":\"command\",\"detail\":\"unknown command or invalid argument\"}");
    }
    return true;
}

/* Keep one command response per input line even when a pipe delivers them in one write. */
static bool stream_poll(StreamState *s, LiveIn *in)
{
    if (!in->pipe) return true;
    for (u32 batch = 0; batch < 4096 && stream_queue_free(&s->writer) >= 6; batch++) {
        DWORD n = 0;
        if (!PeekNamedPipe(in->h, NULL, 0, NULL, &n, NULL)) { s->quit = true; break; }
        if (!n) break;
        char ch;
        if (!ReadFile(in->h, &ch, 1, &n, NULL) || !n) { s->quit = true; break; }
        if (ch == '\r' || ch == '\n') {
            if (in->len && in->len < sizeof in->line) {
                in->line[in->len] = 0;
                if (!stream_cmd(s, in->line)) return false;
            } else if (in->len && !stream_json(&s->writer, "{\"event\":\"error\",\"what\":\"command too long\"}")) return false;
            in->len = 0;
        } else if (in->len < sizeof in->line - 1) in->line[in->len++] = ch;
        else in->len = sizeof in->line;
    }
    return true;
}

static int stream_run(AzWorld *w, LiveIn *in, const LiveCtl *c, u32 sps, u32 tps, u32 observer_ms, bool free_pace, u32 max_ticks, u32 arena_locked)
{
    StreamState s = {0};
    s.tick_end_unix_ms = stream_unix_ms();
    s.world = w; s.sps = sps; s.tps = tps; s.observer_ms = observer_ms;
    s.width = c->iw; s.height = c->ih; s.yaw = c->yaw; s.pitch = c->pitch;
    s.fps = c->fps; s.every = c->every; s.free_pace = free_pace; s.pipe = in->pipe;
    s.prev_snap_tick = w->tick; s.matter = s.matter0 = az_total_matter(w); s.energy = s.energy0 = az_total_energy(w);
    const u64 tick0 = w->tick;
    if ((u64)w->cap * AZST_RECORD_BYTES + AZST_HEADER_BYTES > AZST_MAX_BYTES) {
        fprintf(stderr, "world capacity exceeds the AZST v1 frame limit\n"); return 2;
    }
    s.capture.snapshot = malloc(AZST_HEADER_BYTES + (size_t)w->cap * AZST_RECORD_BYTES);
    s.capture.scratch = malloc((size_t)w->cap * AZST_RECORD_BYTES);
    s.capture.buckets = malloc(65536 * sizeof(u32));
    s.capture.index_count = (size_t)w->cap * 2;
    s.capture.uid_index = malloc(s.capture.index_count * sizeof *s.capture.uid_index);
    s.shape_keys = calloc(w->cfg.max_clades, 16);
    s.shape_seen = calloc(w->cfg.max_clades, 1);
    bool started = stream_writer_start(&s.writer);
    stream_hasher_open(&s.hasher, w->host->n_workers);
    bool ok = started && s.capture.snapshot && s.capture.scratch && s.capture.buckets &&
              s.capture.uid_index && s.shape_keys && s.shape_seen;
    if (ok) {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        stream_touch(s.capture.snapshot, AZST_HEADER_BYTES + (size_t)w->cap * AZST_RECORD_BYTES, si.dwPageSize);
        stream_touch(s.capture.scratch, (size_t)w->cap * AZST_RECORD_BYTES, si.dwPageSize);
        stream_touch(s.capture.uid_index, s.capture.index_count * sizeof *s.capture.uid_index, si.dwPageSize);
    }
    char json[800];
    snprintf(json, sizeof json, "{\"event\":\"start\",\"abi\":%u,\"mode\":\"%s\",\"tick\":%" PRIu64
             ",\"seed\":\"%016" PRIx64 "\",\"origin\":\"%08x%08x\",\"grid\":[%u,%u,%u],\"cap\":%u,"
             "\"locked\":%u,\"sps\":%u,\"tps\":%u,\"observer_ms\":%u,\"size\":[%u,%u],\"workers\":%u,\"observer_workers\":%u}",
             AZ_ABI_VERSION, w->cfg.mode == AZ_MODE_WILD ? "wild" : "lab", w->tick,
             w->cfg.seed, w->key[1], w->key[0], w->gx, w->gy, w->gz, w->cap,
             arena_locked, sps, tps, observer_ms, c->iw, c->ih, w->host->n_workers, s.hasher.workers);
    if (ok) ok = stream_json(&s.writer, json) && stream_snapshot(&s, false);
    u64 next_tick = 0;
    while (ok && !s.writer.failed) {
        if (!stream_poll(&s, in)) { ok = false; break; }
        bool last = max_ticks && w->tick - tick0 >= max_ticks;
        if (s.quit || last || s.unbalanced || s.failed) {
            if (last && s.prev_snap_tick != w->tick && !s.inflight) ok = stream_snapshot(&s, true);
            break;
        }
        u64 now = stream_now(&s);
        bool stepping = s.step > 0;
        if (stepping || (!s.paused && now >= next_tick)) {
            u32 rate = s.free_pace ? s.tps : s.fps * s.every;
            next_tick = rate ? now + 1000000000u / rate : now;
            az_tick(w);
            s.tick_end_unix_ms = stream_unix_ms();
            s.tick_ns += stream_now(&s) - now;
            s.matter = az_total_matter(w);
            s.energy = az_total_energy(w);
            if (s.matter != s.matter0 || s.energy != s.energy0) s.unbalanced++;
            if (stepping && !--s.step) s.refresh = true;
        } else Sleep(1);
        now = stream_now(&s);
        if (!s.step && (s.refresh || (w->tick != s.prev_snap_tick && now - s.last_snap_ns >= 1000000000u / s.sps))) {
            ok = stream_snapshot(&s, max_ticks && w->tick - tick0 >= max_ticks);
        }
    }
    u64 before = stream_state_hash(&s.hasher, w);
    const u64 matter = az_total_matter(w);
    const i64 energy = az_total_energy(w);
    const u64 after = stream_state_hash(&s.hasher, w);
    stream_check(&s, before, after);
    snprintf(json, sizeof json, "{\"event\":\"end\",\"tick\":%" PRIu64 ",\"frames\":%u,\"unbalanced\":%u,"
             "\"write_fail\":%u,\"observer_fail\":%u,\"observer_pass\":%u,\"matter\":%" PRIu64 ",\"energy\":%" PRId64
             ",\"hash\":\"%016" PRIx64 "\",\"hash_after_view\":\"%016" PRIx64 "\"}",
             w->tick,s.next_seq,s.unbalanced,ok ? 0u : 1u,s.failed,s.passed,matter,energy,before,after);
    if (started && !s.writer.failed && !stream_json(&s.writer, json)) ok = false;
    if (!stream_writer_close(&s.writer)) ok = false;
    stream_hasher_close(&s.hasher);
    free(s.shape_keys); free(s.shape_seen);
    free(s.capture.snapshot); free(s.capture.scratch); free(s.capture.buckets);
    free(s.capture.uid_index);
    if (!ok) fprintf(stderr, "stream allocation, capture, or write failed\n");
    return ok && !s.failed && !s.unbalanced ? 0 : 1;
}
