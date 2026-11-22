/* az_ledgerdump.c - inspect a ledger file written by the ledger sink.
 *
 *   az_ledgerdump <file> [--chain 0x<hex>] [--first N] [--kind <name|number>]
 *
 * Prints the header, the number of events of each kind, the recomputed hash
 * chain, and the first N events as text (default 20; --kind limits the
 * listing to one kind). Checks the structure the core guarantees: every
 * tick block is in canonical order (az_event_less) and ends with a summary
 * that counts the block and checkpoints the chain.
 *
 * The recomputation starts from the header's start chain: 0 for a fresh
 * world, the restored w->ledger_chain for a world resumed from a snapshot
 * (format version 2, finding s2). Version-1 files have no start field and
 * read as starting from 0.
 *
 * --chain: the value the run reported (AzWorld.ledger_chain at its end), in
 * hexadecimal with or without 0x. The tool compares it with the chain it
 * recomputes from the records.
 *
 * Options are strict (tools/az_tools.h): an unknown option, a bad value or a
 * repeated option is refused.
 *
 * Exit code: 0 = the file is well formed and every comparison made matched;
 *            1 = a chain or structure mismatch; 2 = usage or unreadable file.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "../core/az_abi.h"
#include "../platform/win/az_sink_win.h"
#include "az_tools.h"

static void print_event(u64 index, const AzEvent *e)
{
    char num[16];
    const char *name = azt_ev_name(e->kind);
    if (!name) {
        snprintf(num, sizeof num, "kind%u", (unsigned)e->kind);
        name = num;
    }
    if (e->kind == AZ_EV_TICK)
        printf("%10" PRIu64 "  tick %-8" PRIu64 " %-8s events=%" PRIu64 " alive=%" PRIu64 " chain=%016" PRIx64 " dropped=%u\n",
               index, e->tick, name, e->a, e->b, (u64)e->v, (unsigned)e->aux);
    else
        printf("%10" PRIu64 "  tick %-8" PRIu64 " %-8s a=%" PRIu64 " b=%" PRIu64 " aux=%u v=%" PRId64 "\n",
               index, e->tick, name, e->a, e->b, (unsigned)e->aux, e->v);
}

/* --chain is hexadecimal; the 0x is optional (as before) */
static bool parse_chain(const char *s, u64 *out)
{
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) return azt_parse_u64(s, out);
    char buf[40];
    if (strlen(s) + 3 > sizeof buf) return false;
    snprintf(buf, sizeof buf, "0x%s", s);
    return azt_parse_u64(buf, out);
}

static int usage(void)
{
    printf("usage: az_ledgerdump <file> [--chain 0x<hex>] [--first N] [--kind <name|number>]\n");
    return 2;
}

int main(int argc, char **argv)
{
    if (argc < 2 || !strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) return usage();
    if (argv[1][0] == '-' && argv[1][1] == '-') {
        fprintf(stderr, "the first argument is the ledger file, not an option ('%s')\n", argv[1]);
        return usage();
    }
    static const char *const with_value[] = { "--chain", "--first", "--kind", NULL };
    const int first_opt = 2;                       /* after the file argument */
    const char *path = argv[1];
    const char *chain_arg = NULL, *kind_arg = NULL;
    u64 first = 20;
    if (!azt_known(argc, argv, first_opt, with_value, NULL) ||
        !azt_str(argc, argv, first_opt, "--chain", &chain_arg) ||
        !azt_str(argc, argv, first_opt, "--kind", &kind_arg) ||
        !azt_u64(argc, argv, first_opt, "--first", 0, ~0ull, &first))
        return 2;
    int only_kind = 0;
    /* usage and input errors go to stderr (exit 2); the report goes to stdout */
    if (kind_arg && (only_kind = azt_parse_kind(kind_arg)) < 0) {
        fprintf(stderr, "option --kind: unknown kind '%s'\n", kind_arg);
        return 2;
    }
    u64 want_chain = 0;
    if (chain_arg && !parse_chain(chain_arg, &want_chain)) {
        fprintf(stderr, "option --chain: '%s' is not a hexadecimal number\n", chain_arg);
        return 2;
    }

    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return 2; }
    u8 raw[sizeof(AzLedgerHeader)];
    size_t got_hdr = fread(raw, 1, sizeof raw, f);
    AzLedgerHeader h;
    int body = az_ledger_header_decode(raw, got_hdr, &h);
    if (body == 0) {
        fprintf(stderr, "%s: not a ledger file\n", path);
        fclose(f);
        return 2;
    }
    if (body < 0) {
        fprintf(stderr, "%s: unsupported ledger version %u (header %u, record %u bytes)\n",
                path, h.version, h.header_bytes, h.record_bytes);
        fclose(f);
        return 2;
    }
    if (fseek(f, body, SEEK_SET) != 0) {
        fprintf(stderr, "%s: cannot seek past the header\n", path);
        fclose(f);
        return 2;
    }
    printf("file      %s\n", path);
    printf("seed      %" PRIu64 "\n", h.seed);
    printf("format    version %u, abi %u, record %u bytes\n", h.version, h.abi, h.record_bytes);
    if (h.version >= 2)
        printf("start     chain %016" PRIx64 ", ledger count %" PRIu64 "  (%s)\n", h.start_chain,
               h.start_count, (h.start_chain || h.start_count) ? "resumed from a snapshot" : "a fresh world");
    if (h.closed)
        printf("header    closed: records %" PRIu64 ", dropped by the ring %" PRIu64 ", write errors %u, chain %016" PRIx64 "\n",
               h.n_records, h.n_dropped, h.write_errors, h.chain);
    else
        printf("header    NOT closed (the writer did not finish); counting records from the file size\n");

    enum { CHUNK = 65536 };
    AzEvent *buf = (AzEvent *)malloc(CHUNK * sizeof(AzEvent));
    if (!buf) { fclose(f); return 2; }
    AzLedgerCheck ck;
    az_ledger_check_begin(&ck, h.start_chain);
    u64 shown = 0;
    bool tail_bytes = false;
    if (first) printf("events\n");
    for (;;) {
        size_t got_bytes = fread(buf, 1, CHUNK * sizeof(AzEvent), f);
        size_t got = got_bytes / sizeof(AzEvent);
        if (got_bytes % sizeof(AzEvent)) tail_bytes = true;
        for (size_t i = 0; i < got && shown < first; i++) {
            const AzEvent *e = &buf[i];
            if (!only_kind || e->kind == only_kind) { print_event(ck.n + i, e); shown++; }
        }
        az_ledger_check_step(&ck, buf, got);
        if (got_bytes < CHUNK * sizeof(AzEvent)) break;
    }
    fclose(f);
    free(buf);

    const u64 n = ck.n, chain = ck.chain;
    printf("records   %" PRIu64 " in %" PRIu64 " ticks\n", n, ck.blocks);
    for (u32 k = 1; k < AZ_EV_COUNT; k++) {
        if (!ck.counts[k]) continue;
        printf("  %-9s %" PRIu64 "\n", azt_ev_name(k), ck.counts[k]);
    }
    const u64 unknown = ck.unknown_kind - ck.counts[0];
    if (unknown) printf("  %-9s %" PRIu64 "\n", "unknown", unknown);
    if (ck.counts[0]) printf("  kind 0    %" PRIu64 "  (invalid)\n", ck.counts[0]);
    printf("chain     %016" PRIx64 "  (recomputed from the records)\n", chain);

    int rc = 0;
    if (tail_bytes) { printf("FAIL      the file ends inside a record\n"); rc = 1; }
    if (ck.unsorted) { printf("FAIL      %" PRIu64 " events out of canonical order\n", ck.unsorted); rc = 1; }
    if (ck.bad_summary) {
        printf("FAIL      %" PRIu64 " tick summaries disagree with their block; first at tick %" PRIu64 "\n",
               ck.bad_summary, ck.first_bad_tick);
        rc = 1;
    }
    if (ck.block_start != n) {
        printf("FAIL      the last tick has no summary (%" PRIu64 " trailing events)\n", n - ck.block_start);
        rc = 1;
    }
    if (ck.unknown_kind) { printf("FAIL      events of unknown kind\n"); rc = 1; }
    if (ck.max_drop_flag)
        printf("note      the core reported dropped events (summary flag up to %" PRIu64 "): the ledger is incomplete\n",
               ck.max_drop_flag);
    if (h.closed) {
        bool same = h.chain == chain && h.n_records == n;
        printf("header    %s\n", same ? "chain and record count MATCH the records" : "MISMATCH with the records");
        if (!same) rc = 1;
        if (h.n_dropped) printf("note      the ring dropped %" PRIu64 " events: the ledger is incomplete\n", h.n_dropped);
    }
    if (chain_arg) {
        bool same = want_chain == chain;
        printf("expected  %016" PRIx64 "  %s\n", want_chain, same ? "MATCH" : "MISMATCH");
        if (!same) rc = 1;
    }
    if (rc == 0 && !tail_bytes) printf("ok\n");
    return rc;
}
