# Tsotchke audit: pq-consensus-bandwidth (key `pq-consensus`)

Audited 2026-11-17. Scratch: `S:\tsotchke\pq-consensus\`. Source read-only at `T:\mirrors\pq-consensus-bandwidth`.

**Verdict: reference_only.** Nothing in this repo has a causal role inside the
simulation. The only reusable content is three lines of sizing arithmetic and one
varint-length helper, usable as *tooling* for sizing an evidence ledger, plus two
evidence-method ideas. It maps to category (j) (receipts for evidence), tooling
tier only; categories (a) to (i) and (k): none.

## Repo facts

| Item | Value |
|---|---|
| HEAD (`git -C T:/mirrors/pq-consensus-bandwidth log -1 --format=%h_%cs`) | `245c3be_2026-10-04` |
| License (LICENSE file) | Apache License, Version 2.0 |
| Main language | Go (`go.mod`: go 1.25.0), single `main` package, 10 `.go` files, 2,405 lines including a 506-line test file |
| Size | 2.5 MB on disk including `.git`; 23 tracked files |
| Only code dependency | `github.com/cometbft/cometbft v0.40.0` (protobuf encoders, ed25519 and mldsa65 packages) |
| What it is | A measurement tool: marshaled sizes of CometBFT `Vote` / `Commit` / handshake messages for post-quantum signature schemes, and link-budget models built on those sizes. It is not a signature implementation, not a hash chain, not a ledger. |

Law check: Go is compiled, so it is not "interpreted-only" code, but the subject
matter is a network consensus protocol. The engine has no network, no validators
and no signatures, so the models have nothing to act on in the simulation.

## Candidates

| # | Kernel | File and lines | Signature | Dependencies | Pure / stateful | Float / integer | Thread safety |
|---|---|---|---|---|---|---|---|
| 1 | Capacity ceiling | `ceiling.go:43-75` | `func DeriveCeiling(voteBytesPerValidator int, b Budget) Ceiling` | none (file imports only `fmt`) | pure | mixed: `float64` budget and utilization, `int64` division | safe (no shared state) |
| 2 | Storage projection | `breakdown.go:95-104` | `func ProjectStorage(commitBytes, validators int, blockSeconds float64) StorageProjection` | function: none; its file imports CometBFT | pure | mixed: `float64` period, `int64` product | safe |
| 3 | Ceiling inverted | `solve.go:41-48` | `func Require(t Target, b Budget) Requirement` | function: none; its file calls `Measure` (CometBFT) | pure | mixed, as 1 | safe |
| 4 | Varint length | `handshake.go:137-148` | `func varintLen(n int) int`, `func varintGrowth(from, to int) int` | function: none; file imports CometBFT | pure | integer | safe |
| 5 | Scheme size table | `schemes.go:15-76` | `type Scheme struct{Name; SigBytes, PubKeyBytes int; Variable bool; SigBytesMax int; Note}`, `var schemes []Scheme` | none | read-only data | integer constants | safe (never written) |

Also read, idea only (not a kernel): `breakdown.go:28-82` `Breakdown(scheme Scheme) ([]Field, int, error)`
attributes message bytes to fields by marshaling the message again with one field
emptied; and `migration.go:95-110` `EstimateGossip(...)`, which is `n * bytes * peers`
scaled by an unmeasured suppression fraction.

## What was built and run (ran now, 2026-11-17)

### A. Whole repo: does NOT build here

CometBFT v0.40.0 is not in the local Go module cache and downloads are forbidden.

```
cd S:/tsotchke/pq-consensus/full        # copy of the repo's *.go, go.mod, go.sum
GOPROXY=off GOTOOLCHAIN=local GOFLAGS=-mod=mod go build .
```
```
handshake.go:8:2: module lookup disabled by GOPROXY=off
cputime.go:9:2: module lookup disabled by GOPROXY=off
migration.go:7:2: module lookup disabled by GOPROXY=off
handshake.go:9:2: module lookup disabled by GOPROXY=off
breakdown.go:8:2: module lookup disabled by GOPROXY=off
breakdown.go:9:2: module lookup disabled by GOPROXY=off
```

So `Measure`, `Breakdown`, the handshake profiles, the CPU timing and the repo's
15 tests were **not run**. The published byte sizes below are taken from the
repo's `docs/RESULTS_20260906.md`, not re-measured.

### B. Dependency-free Go subset (candidates 1 to 5): built and ran

`S:\tsotchke\pq-consensus\go\`: `schemes.go` and `ceiling.go` are byte-identical
copies (`cmp` clean); `extract.go` holds candidates 2, 3, 4 and `EstimateGossip`
transcribed, each checked against the source line range with `diff` (clean);
`main.go` is my driver.

```
cd S:/tsotchke/pq-consensus/go
GOPROXY=off GOTOOLCHAIN=local go vet .
GOPROXY=off GOTOOLCHAIN=local go build -o pqsubset.exe .
./pqsubset.exe > ../go_out.txt        # exit 0
```
Output head and last line:
```
ceiling ed25519 690 12489000 12500000
ceiling ml-dsa-65 36 12337200 12500000
ceiling composite-ed25519-ml-dsa-65 35 12246500 12500000
ceiling composite-ml-dsa-65-slh-dsa-128s 11 12420100 12500000
storage ed25519 5256000 55597968000
storage ml-dsa-65 5256000 1762221168000
...
require 100 2500
require 10000 25
...
# ml-dsa-65 commit storage/yr: 1.6 TiB; ed25519: 51.8 GiB
```
The ceilings 690 / 36 / 35 / 11 (100 Mbit/s, 1 s round, 50 peers) and the
1.6 TiB and 51.8 GiB yearly figures equal the README's published values, given
the published per-vote and per-commit byte sizes as inputs. That confirms the
arithmetic, not the byte sizes.

### C. Integer C23 port: built, ran, diffed against Go

`S:\tsotchke\pq-consensus\c\an_ledger_sizing.{h,c}` plus `test_ledger_sizing.c`.

```
cd S:/tsotchke/pq-consensus/c
gcc -std=c2x -O2 -Wall -Wextra -Wconversion -o test_ledger_sizing.exe an_ledger_sizing.c test_ledger_sizing.c
./test_ledger_sizing.exe > ../c_out.txt     # exit 0, no warnings
cd .. && diff <(grep -v '^#' go_out.txt | tr -d '\r') <(grep -v '^#' c_out.txt | tr -d '\r')
```
```
DIFF: identical (38 lines)
# plain chain: 1562500 records, 112500000 B over 100000000 ticks
# 1 GiB holds 14913080 plain records = 954437120 ticks
# per-record ML-DSA-65: 3383 B/record, 5285937500 B total
# checkpoint-signed (1 per 4096): 381 sigs, 113761491 B total
# all checks passed
```
The 38 diffed lines cover ceiling, storage, inverse, gossip upper bound, varint
length at the 1/2/3/4-byte boundaries, and the scheme table. The test also
checks that a zero divisor and u64 overflow return `false` with outputs
untouched, and a round-trip property over 715 record sizes.

The four `#` lines are an **illustration with assumed inputs** (72-byte record =
32 B previous digest + 32 B state hash + 8 B tick; one record per 64 ticks; a
100,000,000-tick span; a 1 GiB budget). They are not engine measurements. What
they show: signing every record with ML-DSA-65 would make the ledger about 47
times larger (3,383 B against 72 B per record), while signing one checkpoint per
4,096 records adds about 1.1%.

## Proposed integration

Tier: **tooling only.** Never called from Tier 0, Tier 1, Tier 2 or world-field
code. It would be called once at start-up by the evidence writer to reject a run
whose ledger cannot fit its disk budget, and by offline report scripts.

```c
/* all: pure, u64 only, false on overflow or zero divisor, outputs untouched */
bool an_ledger_budget_bytes(uint64_t bits_per_sec, uint64_t window_ms, uint64_t *budget_bytes);
bool an_ledger_max_records(uint64_t budget_bytes, uint64_t record_bytes, uint64_t fanout,
                           uint64_t *n, uint64_t *bytes_used);            /* from DeriveCeiling */
bool an_ledger_max_record_bytes(uint64_t budget_bytes, uint64_t n_records, uint64_t fanout,
                                uint64_t *record_bytes);                  /* from Require */
bool an_ledger_span_bytes(uint64_t span, uint64_t units_per_record, uint64_t record_bytes,
                          uint64_t *records, uint64_t *bytes);            /* from ProjectStorage */
uint32_t an_varint_len(uint64_t n);                                       /* from varintLen */
```

Changes from the Go originals: the `float64` budget (`LinkBitsPerSec/8.0*RoundSeconds`)
and `float64` period count (`31536000/blockSeconds`) are replaced by integer
division; multiplications are overflow-checked with `<stdckdint.h>`; the
`searchMax = 100_000` clamp is dropped (it guards a table, not arithmetic).

Honest size of this: each function is one multiply and one divide. Writing them
fresh costs minutes; the repo saves no real engineering effort. If adopted, keep
the Apache-2.0 attribution line in the header as written.

Two method ideas worth more than the code:

1. **Size from the real encoder, not from a formula.** The repo marshals the
   actual message and pins every published number in a test, so a format change
   breaks the test. The engine equivalent: a test that serializes one real
   ledger record and one real snapshot header and asserts their byte counts,
   and feeds those counts (not hand-computed ones) to the functions above.
2. **Differential field attribution** (`Breakdown`): serialize with one field
   emptied and attribute the difference. Useful once for reporting where
   snapshot bytes go, including length prefixes that a hand count misses.

A sizing fact to carry over if receipts are ever signed for third parties:
ML-DSA-65 is 3,309 B per signature and 1,952 B per public key, SLH-DSA-128s is
7,856 B and 32 B (constants from `schemes.go`, which cites FIPS 204 and 205;
I did not re-check them against the standards). Sign checkpoints, not records.

## NOT usable, and why

- **No hash function, no hash chain, no Merkle code.** `measure.go:46-49` `digest(label)`
  calls the Go standard library's `crypto/sha256` on a fixed label to get
  realistically sized 32-byte placeholder block hashes; nothing is chained. A provenance
  ledger gets its chain primitive from somewhere else; this repo gives sizes only.
- **No signature or KEM implementation.** Schemes are two integers each. Sign and
  verify timing calls CometBFT's packages, which are absent here.
- **`Measure`, `Breakdown`, `MeasureClientUpdate`, the three handshake profiles,
  `Mitigations`, `Assess`**: all need CometBFT protobuf types. Not built, not run,
  and they describe a network protocol the engine does not have.
- **`MeasureCPU`, `AssessRound` (`cputime.go`)**: wall-clock timing of signatures
  from one Apple M2 Ultra; non-deterministic by design and irrelevant to a
  deterministic simulation.
- **`EstimateGossip` suppression fraction**: a free parameter the repo itself
  says is unmeasured. Not evidence of anything.
- **Categories (a) RNG / hash mixers, (b) fixed-point kernels, (c) state-vector
  simulation, (d) SO(3)/SU(2), (e) quantum geometric tensor, (f) spin / Hopfield,
  (g) GEMV / GEMM, (h) Monte Carlo, (i) PDE solvers, (k) rendering**: nothing in
  the repo. I listed every `func`, `type`, `const` and `var` in the non-test
  files (`grep -n "^func \|^type \|^const \|^var " *.go`) and none falls in these.
- **Float use**: candidates 1 to 3 use `float64` for the budget; harmless in a
  report tool, but they could not be used as-is under the integer-state law,
  hence the integer port.
- **"Post-quantum" has no meaning inside the simulation.** Using these names for
  any creature or world mechanism would be cosmetic theater (law 4) and risks the
  wording law 7 forbids.

## Unknowns

- Whether the repo's own 15 tests pass on this machine: unknown (cannot build).
- Whether the published byte sizes (181, 3,427, 3,499, 11,291, 10,578, 335,278 ...)
  are right: not re-measured; taken from the repo's results document.
- Whether the engine will have an evidence ledger with a disk budget at all:
  that is a design decision outside this audit. Without one, the verdict is
  effectively not_applicable.
