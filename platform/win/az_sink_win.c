/* az_sink_win.c - ledger sink: SPSC ring + writer thread. See az_sink_win.h.
 *
 * Ring discipline: `tail` is written only by the producer (the simulation
 * thread, inside az_ledger_flush), `head` only by the writer thread. Both
 * are free-running 64-bit counters; the slot of position p is p & mask. The
 * producer publishes with one store of `tail` after the copy, so the writer
 * never sees a half-written event.
 */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "az_sink_win.h"

enum {
    SINK_DEFAULT_EVENTS = 1 << 20,
    SINK_MIN_EVENTS     = 16,
    SINK_MAX_EVENTS     = 1 << 27,   /* 2.5 GB of ring; more is a caller bug   */
    SINK_WRITE_CHUNK    = 1 << 16    /* events per WriteFile, so head advances  */
};

struct AzSink {
    AzHost   host;          /* the wrapper handed to az_world_create           */
    AzHost  *inner;         /* the worker pool it forwards to, or NULL         */
    AzEvent *ring;
    u32      cap, mask;
    HANDLE   file, thread, wake;
    LARGE_INTEGER qpf;
    AzLedgerHeader hdr;

    /* producer line */
    u64 tail AZ_ALIGNED(64);
    u64 head_seen;          /* producer's last view of head                    */
    u64 offered, dropped;

    /* writer line */
    u64 head AZ_ALIGNED(64);
    u64 written, chain;
    u32 write_errors;
    u32 broken;             /* a cut-back failed: the writer appends nothing more */
    u32 fail_armed;         /* test aid: the next write writes fail_bytes, then fails */
    u32 fail_bytes;
    u32 fail_cut;           /* test aid: the cut-back after that failure fails too */

    /* control line */
    u32 quit AZ_ALIGNED(64);
    u32 paused;             /* request: writer must not take events            */
    u32 parked;             /* answer: writer is holding                        */
    u32 sleeping;           /* writer waits on `wake` because the ring is empty */
};

static inline u64 ld64(const u64 *p) { return __atomic_load_n(p, __ATOMIC_SEQ_CST); }
static inline void st64(u64 *p, u64 v) { __atomic_store_n(p, v, __ATOMIC_SEQ_CST); }
static inline u32 ld32(const u32 *p) { return __atomic_load_n(p, __ATOMIC_SEQ_CST); }
static inline void st32(u32 *p, u32 v) { __atomic_store_n(p, v, __ATOMIC_SEQ_CST); }

static bool write_all(HANDLE f, const void *p, u64 bytes)
{
    const u8 *b = (const u8 *)p;
    while (bytes) {
        DWORD want = bytes > (1u << 30) ? (1u << 30) : (DWORD)bytes, got = 0;
        if (!WriteFile(f, b, want, &got, NULL) || got == 0) return false;
        b += got; bytes -= got;
    }
    return true;
}

/* One chunk of event records. The test aid az_sink_fail_next_write makes the
 * next call write only part of the chunk and then fail, as a full volume or a
 * pulled drive can. */
static bool write_records(AzSink *s, const void *p, u64 bytes)
{
    if (s->broken) return false;
    if (__atomic_exchange_n(&s->fail_armed, 0u, __ATOMIC_SEQ_CST)) {
        DWORD part = s->fail_bytes < bytes ? s->fail_bytes : (DWORD)bytes, got = 0;
        if (part) (void)WriteFile(s->file, p, part, &got, NULL);
        return false;
    }
    return write_all(s->file, p, bytes);
}

/* After a failed chunk: cut the file back to the end of the last whole record.
 * The file holds the header and then exactly `written` records (open and
 * az_sink_set_start leave the pointer after the header; only whole chunks are
 * counted), so that end is known without asking the file. */
static bool cut_to_last_record(AzSink *s)
{
    if (__atomic_exchange_n(&s->fail_cut, 0u, __ATOMIC_SEQ_CST)) return false;   /* test aid */
    LARGE_INTEGER end;
    end.QuadPart = (LONGLONG)(sizeof(AzLedgerHeader) + s->written * sizeof(AzEvent));
    return SetFilePointerEx(s->file, end, NULL, FILE_BEGIN) && SetEndOfFile(s->file);
}

void az_sink_fail_next_write(AzSink *s, u32 bytes_before_failure, bool cut_fails)
{
    if (!s) return;
    s->fail_bytes = bytes_before_failure;
    __atomic_store_n(&s->fail_cut, cut_fails ? 1u : 0u, __ATOMIC_SEQ_CST);
    __atomic_store_n(&s->fail_armed, 1u, __ATOMIC_SEQ_CST);
}

static DWORD WINAPI writer_main(LPVOID arg)
{
    AzSink *s = (AzSink *)arg;
    u64 head = 0;
    for (;;) {
        /* pause handshake: `parked` is cleared before `paused` is read
         * again, and az_sink_pause sets `paused` before it reads `parked`,
         * so one of the two always notices the other. */
        if (ld32(&s->paused)) {
            st32(&s->parked, 1);
            WaitForSingleObject(s->wake, 2);
            continue;
        }
        st32(&s->parked, 0);
        if (ld32(&s->paused)) continue;

        u32 quit = ld32(&s->quit);            /* read before tail: nothing is */
        u64 tail = ld64(&s->tail);            /* pushed after quit is set     */
        u64 avail = tail - head;
        if (avail == 0) {
            if (quit) break;
            st32(&s->sleeping, 1);
            if (ld64(&s->tail) == head && !ld32(&s->quit) && !ld32(&s->paused))
                WaitForSingleObject(s->wake, 10);
            st32(&s->sleeping, 0);
            continue;
        }
        u32 at = (u32)head & s->mask;
        u32 n = s->cap - at;                  /* contiguous run               */
        if (n > avail) n = (u32)avail;
        if (n > SINK_WRITE_CHUNK) n = SINK_WRITE_CHUNK;
        const AzEvent *e = &s->ring[at];
        if (write_records(s, e, (u64)n * sizeof(AzEvent))) {
            u64 c = s->chain;
            for (u32 i = 0; i < n; i++) c = az_ledger_fold(c, &e[i]);
            st64(&s->chain, c);
            st64(&s->written, s->written + n);
        } else {
            /* A failed write may leave part of the chunk in the file. Cut it
             * back, so every later record stays aligned (finding v18); if even
             * that fails, append nothing more, so the file stays its header plus
             * whole records and a trailing fragment. The chunk's events are
             * lost and counted. */
            if (!s->broken && !cut_to_last_record(s)) s->broken = 1;
            st32(&s->write_errors, s->write_errors + 1);
        }
        head += n;
        st64(&s->head, head);                 /* frees the slots              */
    }
    return 0;
}

void az_sink_push(void *sink, const AzEvent *ev, u32 n)
{
    AzSink *s = (AzSink *)sink;
    u64 tail = s->tail;
    u64 room = s->cap - (tail - s->head_seen);
    if (room < n) {
        s->head_seen = ld64(&s->head);
        room = s->cap - (tail - s->head_seen);
    }
    u32 take = n < room ? n : (u32)room;
    if (take) {
        u32 at = (u32)tail & s->mask;
        u32 first = s->cap - at;
        if (first > take) first = take;
        memcpy(&s->ring[at], ev, (size_t)first * sizeof(AzEvent));
        memcpy(&s->ring[0], ev + first, (size_t)(take - first) * sizeof(AzEvent));
        st64(&s->tail, tail + take);
    }
    st64(&s->offered, s->offered + n);
    if (take != n) st64(&s->dropped, s->dropped + (n - take));
    if (take && ld32(&s->sleeping)) SetEvent(s->wake);
}

/* ---- the wrapping host --------------------------------------------------- */
static void wrap_parallel_for(void *ctx, struct AzWorld *w, AzJobFn fn, const u32 *splits, u32 n_parts)
{
    AzSink *s = (AzSink *)ctx;
    if (s->inner && s->inner->parallel_for) {
        s->inner->parallel_for(s->inner->ctx, w, fn, splits, n_parts);
    } else {
        for (u32 k = 0; k < n_parts; k++) fn(w, splits[k], splits[k + 1], 0);   /* one worker: token 0 */
    }
}

static u64 wrap_now_ns(void *ctx)
{
    AzSink *s = (AzSink *)ctx;
    if (s->inner && s->inner->now_ns) return s->inner->now_ns(s->inner->ctx);
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (u64)((unsigned __int128)t.QuadPart * 1000000000ull / (u64)s->qpf.QuadPart);
}

static void wrap_sink(void *ctx, const AzEvent *ev, u32 n)
{
    AzSink *s = (AzSink *)ctx;
    az_sink_push(s, ev, n);
    if (s->inner && s->inner->sink) s->inner->sink(s->inner->sink_ctx, ev, n);   /* tee */
}

static bool wrap_entropy(void *ctx, void *buf, u32 bytes)
{
    AzSink *s = (AzSink *)ctx;
    return s->inner && s->inner->entropy && s->inner->entropy(s->inner->ctx, buf, bytes);
}

static void wrap_entropy_log(void *ctx, u64 tick, const u8 bytes[16])
{
    AzSink *s = (AzSink *)ctx;
    if (s->inner && s->inner->entropy_log) s->inner->entropy_log(s->inner->ctx, tick, bytes);
}

AzHost *az_sink_host(AzSink *s, AzHost *inner)
{
    if (!s) return inner;
    s->inner = inner;
    s->host.ctx = s;
    s->host.n_workers = (inner && inner->n_workers) ? inner->n_workers : 1;
    s->host.max_parts = inner ? inner->max_parts : 0;
    s->host.parallel_for = wrap_parallel_for;
    s->host.now_ns = wrap_now_ns;
    s->host.sink = wrap_sink;
    s->host.sink_ctx = s;
    s->host.entropy = (inner && inner->entropy) ? wrap_entropy : NULL;
    s->host.entropy_log = wrap_entropy_log;
    return &s->host;
}

/* ---- open / close -------------------------------------------------------- */
AzSink *az_sink_open(const char *path, u64 seed, u32 ring_events)
{
    if (!path) return NULL;
    u32 cap = SINK_MIN_EVENTS;
    if (ring_events == 0) ring_events = SINK_DEFAULT_EVENTS;
    if (ring_events > SINK_MAX_EVENTS) ring_events = SINK_MAX_EVENTS;
    while (cap < ring_events) cap <<= 1;

    AzSink *s = (AzSink *)_aligned_malloc(sizeof(AzSink), 64);
    if (!s) return NULL;
    memset(s, 0, sizeof(*s));
    s->file = INVALID_HANDLE_VALUE;
    s->cap = cap; s->mask = cap - 1;
    QueryPerformanceFrequency(&s->qpf);
    s->ring = (AzEvent *)_aligned_malloc((size_t)cap * sizeof(AzEvent), 64);
    s->wake = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (s->ring && s->wake)
        s->file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);

    s->hdr.magic = AZ_LEDGER_MAGIC;
    s->hdr.version = AZ_LEDGER_VERSION;
    s->hdr.abi = AZ_ABI_VERSION;
    s->hdr.seed = seed;
    s->hdr.header_bytes = (u32)sizeof(AzLedgerHeader);
    s->hdr.record_bytes = (u32)sizeof(AzEvent);

    bool ok = s->file != INVALID_HANDLE_VALUE && write_all(s->file, &s->hdr, sizeof(s->hdr));
    if (ok) {
        az_sink_host(s, NULL);
        s->thread = CreateThread(NULL, 1 << 16, writer_main, s, 0, NULL);
        ok = s->thread != NULL;
    }
    if (!ok) {
        if (s->file != INVALID_HANDLE_VALUE) CloseHandle(s->file);
        if (s->wake) CloseHandle(s->wake);
        if (s->ring) _aligned_free(s->ring);
        _aligned_free(s);
        return NULL;
    }
    return s;
}

bool az_sink_set_start(AzSink *s, u64 chain, u64 count)
{
    /* Only before the first push: the writer touches the file and the chain
     * only when the ring holds events, and the producer (this thread) has
     * pushed none, so the writer is idle and the file pointer is at the end
     * of the header. */
    if (!s || ld64(&s->offered) != 0 || ld64(&s->tail) != 0) return false;
    AzLedgerHeader h = s->hdr;
    h.start_chain = chain;
    h.start_count = count;
    LARGE_INTEGER zero, end;
    zero.QuadPart = 0;
    end.QuadPart = (LONGLONG)sizeof(AzLedgerHeader);
    bool ok = SetFilePointerEx(s->file, zero, NULL, FILE_BEGIN) &&
              write_all(s->file, &h, sizeof h);
    ok = SetFilePointerEx(s->file, end, NULL, FILE_BEGIN) && ok;
    if (!ok) {
        /* put the old header back so the file does not claim a start it
         * will not be folded from */
        if (SetFilePointerEx(s->file, zero, NULL, FILE_BEGIN))
            write_all(s->file, &s->hdr, sizeof s->hdr);
        SetFilePointerEx(s->file, end, NULL, FILE_BEGIN);
        return false;
    }
    s->hdr = h;
    st64(&s->chain, chain);    /* published before any tail store, so the writer folds from it */
    return true;
}

void az_sink_stats(const AzSink *s, AzSinkStats *out)
{
    memset(out, 0, sizeof(*out));
    if (!s) return;
    out->offered = ld64(&s->offered);
    out->dropped = ld64(&s->dropped);
    out->accepted = out->offered - out->dropped;
    out->written = ld64(&s->written);
    out->chain = ld64(&s->chain);
    out->write_errors = ld32(&s->write_errors);
    out->capacity = s->cap;
}

u32 az_sink_pending(const AzSink *s)
{
    return s ? (u32)(ld64(&s->tail) - ld64(&s->head)) : 0;
}

void az_sink_pause(AzSink *s, bool paused)
{
    if (!s) return;
    if (!paused) {
        st32(&s->paused, 0);
        SetEvent(s->wake);
        return;
    }
    st32(&s->paused, 1);
    SetEvent(s->wake);
    /* wait until the writer has finished the write it was in and holds */
    while (!ld32(&s->parked)) {
        if (WaitForSingleObject(s->thread, 0) != WAIT_TIMEOUT) break;   /* writer is gone */
        SwitchToThread();
    }
}

bool az_sink_close(AzSink *s, AzSinkStats *final)
{
    if (final) memset(final, 0, sizeof(*final));
    if (!s) return false;
    st32(&s->paused, 0);
    st32(&s->quit, 1);
    SetEvent(s->wake);
    WaitForSingleObject(s->thread, INFINITE);   /* it drains the ring first */
    CloseHandle(s->thread);

    AzSinkStats st;
    az_sink_stats(s, &st);
    s->hdr.n_records = st.written;
    s->hdr.n_dropped = st.dropped;
    s->hdr.chain = st.chain;
    s->hdr.write_errors = st.write_errors;
    s->hdr.closed = 1;
    LARGE_INTEGER zero;
    zero.QuadPart = 0;
    bool ok = SetFilePointerEx(s->file, zero, NULL, FILE_BEGIN) &&
              write_all(s->file, &s->hdr, sizeof(s->hdr));
    ok = CloseHandle(s->file) && ok;
    ok = ok && st.write_errors == 0 && st.written == st.accepted;
    if (final) *final = st;

    CloseHandle(s->wake);
    _aligned_free(s->ring);
    _aligned_free(s);
    return ok;
}

/* ---- reading ------------------------------------------------------------- */
int az_ledger_header_decode(const void *bytes, u64 n, AzLedgerHeader *out)
{
    memset(out, 0, sizeof(*out));
    if (!bytes || n < AZ_LEDGER_MIN_HEADER_BYTES) return 0;
    memcpy(out, bytes, AZ_LEDGER_MIN_HEADER_BYTES);
    if (out->magic != AZ_LEDGER_MAGIC) return 0;
    if (out->record_bytes != sizeof(AzEvent)) return -1;
    if (out->version == AZ_LEDGER_VERSION && out->header_bytes == sizeof(AzLedgerHeader)) {
        if (n < sizeof(AzLedgerHeader)) return -1;       /* cut inside the header */
        memcpy(out, bytes, sizeof(AzLedgerHeader));
        return (int)sizeof(AzLedgerHeader);
    }
    return -1;
}

void az_ledger_check_begin(AzLedgerCheck *c, u64 start_chain)
{
    memset(c, 0, sizeof(*c));
    c->chain = start_chain;
}

void az_ledger_check_step(AzLedgerCheck *c, const AzEvent *ev, u64 n)
{
    for (u64 i = 0; i < n; i++, c->n++) {
        const AzEvent *e = &ev[i];
        if (e->kind == 0 || e->kind >= AZ_EV_COUNT) c->unknown_kind++;
        if (e->kind < AZ_EV_COUNT) c->counts[e->kind]++;
        if (e->kind == AZ_EV_TICK) {
            /* the summary counts its block and checkpoints the chain as it
             * stood before the summary itself was folded in */
            bool bad = e->a != c->n - c->block_start || (u64)e->v != c->chain;
            if (bad && c->bad_summary++ == 0) c->first_bad_tick = e->tick;
            if (e->aux > c->max_drop_flag) c->max_drop_flag = e->aux;
            c->block_start = c->n + 1;
            c->blocks++;
            c->have_prev = false;
        } else {
            if (c->have_prev && az_event_less(e, &c->prev)) c->unsorted++;
            c->prev = *e;
            c->have_prev = true;
        }
        c->chain = az_ledger_fold(c->chain, e);
    }
}

bool az_ledger_check_clean(const AzLedgerCheck *c)
{
    return c->unsorted == 0 && c->bad_summary == 0 && c->unknown_kind == 0 && c->block_start == c->n;
}
