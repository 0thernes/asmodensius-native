/* az_sink_win.h - the ledger sink: a ring between the simulation and a file.
 *
 * az_ledger_flush (core) hands one sorted batch of events per tick to
 * AzHost.sink. This sink copies the batch into a single-producer
 * single-consumer ring and returns at once; a writer thread appends the
 * events to a binary file. The simulation thread never waits for the disk:
 * when the ring is full the events that do not fit are counted as dropped.
 *
 * File format (little-endian):
 *   AzLedgerHeader (header_bytes), then records of sizeof(AzEvent) = 40
 *   bytes each, in the order they were emitted. The header is written at
 *   open with closed = 0 and rewritten at close with the final counters. A
 *   file whose writer died has closed = 0; its record count is then
 *   (file size - header_bytes) / 40.
 *
 *   version 3 (written and read now): 128-byte header, 40-byte records
 *     (64-bit tick, uids and value: nothing in a record can wrap). The
 *     header holds start_chain and start_count, the ledger chain and count
 *     of the world when the first record was written (both 0 for a fresh
 *     world; w->ledger_chain and w->ledger_count after az_snapshot_load for
 *     a resumed one, see az_sink_set_start). chain is the fold of the
 *     records starting from start_chain, and every tick summary checkpoints
 *     that same chain (finding s2).
 *   versions 1 and 2 (20-byte records with 32-bit ticks and uids) are
 *     refused: az_ledger_header_decode returns -1 for them.
 *
 * Hosted C. Used by tools/az_run-style runners, tools/az_ledgerdump.c and
 * tests/test_ledger.c.
 */
#ifndef AZ_SINK_WIN_H
#define AZ_SINK_WIN_H

#include "../../core/az_abi.h"

#define AZ_LEDGER_MAGIC   0x524547444C5A41ull   /* "AZLDGER\0" as bytes on disk */
#define AZ_LEDGER_VERSION 3u
#define AZ_LEDGER_MIN_HEADER_BYTES 64u   /* enough of a header to tell what the file is */

typedef struct AzLedgerHeader {
    u64 magic;          /* AZ_LEDGER_MAGIC                                       */
    u32 version;        /* AZ_LEDGER_VERSION                                     */
    u32 abi;            /* AZ_ABI_VERSION of the writer                          */
    u64 seed;           /* world seed                                            */
    u32 header_bytes;   /* 128                                                   */
    u32 record_bytes;   /* sizeof(AzEvent)                                       */
    u64 n_records;      /* records in the file (valid when closed)               */
    u64 n_dropped;      /* events the ring refused (valid when closed)           */
    u64 chain;          /* az_ledger_fold over the records, from start_chain
                           (valid when closed)                                   */
    u32 closed;         /* 1 = closed cleanly, counters valid                    */
    u32 write_errors;   /* failed writes (the events of a failed write are lost) */
    u64 start_chain;    /* world ledger_chain before the first record; 0 = fresh */
    u64 start_count;    /* world ledger_count before the first record            */
    u64 reserved[6];    /* 0                                                     */
} AzLedgerHeader;

_Static_assert(sizeof(AzLedgerHeader) == 128, "ledger header is 128 bytes");
_Static_assert(sizeof(AzEvent) == 40, "ledger record is 40 bytes");

/* The chain step is az_ledger_fold in core/az_abi.h, the one copy the core,
 * this sink, the tools and the tests share (finding v55). */

typedef struct AzSink AzSink;

typedef struct AzSinkStats {
    u64 offered;        /* events handed to az_sink_push                         */
    u64 accepted;       /* events that entered the ring                          */
    u64 dropped;        /* events refused because the ring was full              */
    u64 written;        /* events appended to the file so far                    */
    u64 chain;          /* az_ledger_fold over the written events, from the
                           start chain (0 unless az_sink_set_start set it)       */
    u32 write_errors;
    u32 capacity;       /* ring capacity in events                               */
} AzSinkStats;

/* Open `path` for writing (an existing file is replaced), write the header
 * and start the writer thread. ring_events is rounded up to a power of two;
 * 0 means 1 << 20 (20 MB). NULL if the file or the memory is unavailable.
 * The start chain and count are 0 (a fresh world). */
AzSink *az_sink_open(const char *path, u64 seed, u32 ring_events);

/* A world resumed from a snapshot (finding s2): az_snapshot_load restores
 * w->ledger_chain and w->ledger_count, and the core keeps folding from
 * there. Call this once after az_snapshot_load and before the first az_tick,
 * on the producer thread, with
 *     az_sink_set_start(sink, w->ledger_chain, w->ledger_count);
 * The sink then folds its chain from `chain`, and the header records both
 * values (it is rewritten at once, so even a file whose writer dies keeps
 * them). Returns false, and changes nothing, when s is NULL, any event has
 * already been pushed, or the header cannot be rewritten. Without this call
 * a resumed world's file fails every tick-summary check and its chain
 * differs from w->ledger_chain. */
bool az_sink_set_start(AzSink *s, u64 chain, u64 count);

/* Test aid (finding v18): the writer's next chunk writes only
 * bytes_before_failure bytes and then fails, as a full volume can. The sink
 * cuts the file back to the last whole record and counts the error. With
 * cut_fails the cut-back fails too (a pulled drive): the sink then appends
 * nothing more and counts every later chunk as a failed write, so the file
 * keeps its whole records followed by the fragment. */
void az_sink_fail_next_write(AzSink *s, u32 bytes_before_failure, bool cut_fails);

/* Drain the ring, finish the header, stop the writer, free everything. The
 * final counters go to *final when it is not NULL. Returns true when every
 * accepted event reached the file and no write failed. */
bool az_sink_close(AzSink *s, AzSinkStats *final);

/* Producer side (one thread only). Copies up to n events into the ring,
 * counts the rest as dropped, never blocks. `sink` is the AzSink. */
void az_sink_push(void *sink, const AzEvent *ev, u32 n);

/* AzHost.sink receives AzHost.ctx, and that context belongs to the worker
 * pool. So the sink is plugged in by wrapping: the returned host forwards
 * parallel_for and now_ns to `inner` and routes events to the ring. With
 * inner == NULL it is a one-worker host. Pass the returned host to
 * az_world_create (or az_snapshot_load); it lives until az_sink_close.
 * Destroy `inner` yourself (after the last tick), not the wrapper. */
AzHost *az_sink_host(AzSink *s, AzHost *inner);

void az_sink_stats(const AzSink *s, AzSinkStats *out);

/* Events in the ring that the writer has not taken yet. */
u32 az_sink_pending(const AzSink *s);

/* Hold or release the writer thread (file rotation, tests of the full-ring
 * path). While held, pushes still succeed until the ring is full. */
void az_sink_pause(AzSink *s, bool paused);

/* ---- reading a ledger file (az_ledgerdump and the tests share this) ---- */

/* Decode the header from the first `n` bytes of a file (pass
 * sizeof(AzLedgerHeader) bytes). Returns the offset of the first record
 * (128), 0 when the bytes are not a ledger header (shorter than 64 bytes or
 * wrong magic), or -1 for a ledger of an unsupported version (1 and 2
 * included), header size or record size, or one cut inside its header (the
 * first 64 bytes of *out are filled as read). */
int az_ledger_header_decode(const void *bytes, u64 n, AzLedgerHeader *out);

/* The structure checks of a ledger, fed record by record from the start of
 * the file. Begin with the header's start_chain. */
typedef struct AzLedgerCheck {
    u64 n;              /* records seen                                          */
    u64 chain;          /* fold of the records, from the start chain             */
    u64 blocks;         /* tick summaries seen                                   */
    u64 block_start;    /* index of the first record of the open block           */
    u64 unsorted;       /* events not in az_event_less order inside their tick   */
    u64 bad_summary;    /* summaries whose count or chain checkpoint is wrong    */
    u64 first_bad_tick; /* tick of the first bad summary (when bad_summary)      */
    u64 unknown_kind;   /* events of kind 0 or kind >= AZ_EV_COUNT               */
    u64 max_drop_flag;  /* largest summary aux (the core's dropped flag)         */
    u64 counts[AZ_EV_COUNT];   /* events per kind; [0] counts kind 0             */
    bool have_prev;
    AzEvent prev;
} AzLedgerCheck;

void az_ledger_check_begin(AzLedgerCheck *c, u64 start_chain);
void az_ledger_check_step(AzLedgerCheck *c, const AzEvent *ev, u64 n);
/* true when no event is out of order or of unknown kind, every summary
 * matches its block, and the last record is a summary (or there is none) */
bool az_ledger_check_clean(const AzLedgerCheck *c);

#endif
