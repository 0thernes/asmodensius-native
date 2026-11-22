# rosette-wire: audit for engine reuse

Agent key: `rosette-wire`. Date: 2026-11-17. Scratch: `S:\tsotchke\rosette-wire\`.
Focus given: the verification-receipt contract and the C probe. Could our engine
emit receipts in this format for its own determinism and conservation checks?

**Short answer: yes.** A 250-line integer-only C emitter (written here, ported
from the Lisp contract) produces `receipt:1` documents. It reproduces the repo's
canonical example file byte for byte, and a replay verifier built on it refuses
two planted bugs (a thread-dependent RNG key and a one-unit energy leak). What
is NOT shown: acceptance by the repo's own Lisp verifier, because SBCL is not
installed on this machine.

## Repo facts

| Item | Value |
|---|---|
| Path | `T:\mirrors\rosette-wire` (read-only) |
| HEAD | `bb34bbe_2026-09-05` (`git -C T:/mirrors/rosette-wire log -1 --format=%h_%cs`) |
| Release | `0.1.0-rc.3` (`release/VERSION`), pre-1.0 |
| License | Apache License 2.0 (`LICENSE`); `NOTICE`: Copyright 2026 Richard Hoekstra |
| Author | Richard Hoekstra. Not a Tsotchke-authored repo; it adapts to Eshkol and Moonlab |
| Main language | Common Lisp on SBCL: 438 `.lisp` files, 75,725 lines, 80 ASDF systems |
| C in the repo | 2 `.c` + 1 `.h`, 395 lines total (one 367-line probe, one 14-line seL4 client, one 14-line header) |
| Size | 737 tracked files, 5,084,735 bytes |
| Build | `Makefile` drives `sbcl --script tools/...`; needs SBCL, GNU Make, Bash |
| Built here | Lisp: NO (no `sbcl` on PATH, no GNU make). C probe: yes, with a shim (below) |

What it is: a component protocol. A **Component** declares typed ports, effects,
capabilities and verifiers. A **Composition** wires components and names the
`requiredEvidence`. Running it yields a **receipt**; registered verifiers re-run
their checks and yield a verification receipt. Identity everywhere is
`sha256:` + hex of the SHA-256 of a canonical JSON projection.

## The receipt contract (read from source)

`src/compiler/wire/wire-graph/src/graph.lisp:269-281` and `decode.lisp:329-352`.
A receipt is a JSON object with **exactly eight keys** (decode refuses extras
and omissions):

| Key | Content |
|---|---|
| `schema` | `"urn:rosette-wire:schema:receipt:1"` |
| `kind` | `validation`, `execution`, `verification` or `certification` |
| `verdict` | `pass` or `fail` |
| `graph` | `{"digest":"sha256:<64 hex>"}`, the canonical id of the composition |
| `violations` | array of `{code, detail, path, stage}` |
| `stepOrder` | array of step ids, in executed order |
| `outputs` | object of named outputs (`[]` when none) |
| `evidence` | execution: an object; verification: array of `{name, result, value}` |

Canonical JSON (`canonical-json.lisp:113-167`): the RFC 8785 subset with objects,
arrays, strings and integers only. Keys sorted by UTF-16 code unit, duplicates
refused, integers outside +/-(2^53 - 1) refused, **floats refused** unless wrapped
in an explicit `$float` envelope `{sign, significand, exponent, format}`
(`canonical-json.lisp:169-189`). `canonical-id` = `"sha256:" + hex(sha256(bytes))`
(`:191-193`). Verification never trusts a prior verdict: `verify-wire-receipt`
(`runtime.lisp:254-288`) checks the graph digest, then re-runs every required
verifier and records `evidence-failed`, `verifier-error` or
`verifier-unavailable`. A missing verifier is a refusal, never a pass.

This fits our laws well: integer-only, float-refusing, deterministic bytes.
One consequence to respect: a `u64` (seed, 64-bit hash, large energy total)
does not fit a JSON number here and must be emitted as a string.

## Candidates

| # | Kernel | File and lines | Signatures | Dependencies | Pure / stateful | Float / integer | Thread safety | Use (a)-(l) |
|---|---|---|---|---|---|---|---|---|
| 1 | Receipt projection + canonical id | `wire-graph/src/graph.lisp:231-281`, `canonical-json.lisp:113-167,191-193` | `(wire-receipt->value receipt)`, `(canonical-json value)`, `(canonical-id value)`, `(wire-receipt-id receipt)` | Lisp `json`, `symmetric-crypto` (sha256) | pure | integer and string only; floats refused | pure functions, safe | (j) evidence |
| 2 | Verify-by-rerun rule | `wire-graph/src/runtime.lisp:254-288` | `(verify-wire-receipt graph receipt runner)` -> verification receipt | registered verifier closures | pure given verifiers | n/a | n/a | (j) evidence design |
| 3 | ABI probe (C) | `wire-diagnostics/tools/moonlab-abi-probe.c:1-367` (`main` 294-367, `run_surface_probe` 168-292, `bind_symbol` 78-83) | `int main(int argc, char **argv)`; request on stdin, records on stdout | POSIX `dlfcn.h`, libm, C99 `complex.h`; a Moonlab shared library at run time | stateless process | double (prints `%.17e`, round-trip exact) | single-threaded process | (l) tooling pattern |
| 4 | RSC1 Cell (content-addressed container) | `wire-protocol/src/wire-protocol.lisp:75-150` | `(make-cell :tag :media-type :payload :id)`, `(encode-cell cell)`, `(decode-cell bytes :max-cell-bytes)` | sha256 | pure | integer (big-endian u16/u16/u64 header) | pure | (j) snapshot chunks |
| 5 | Typed float envelope | `wire-graph/src/canonical-json.lisp:169-189` | `(%typed-protocol-value value)` | none | pure | float -> exact integers | pure | (j) only if a float ever leaves the engine |

Candidates 1, 2, 4 and 5 exist only as Lisp. What is reusable is the wire
format and the rule, not the code. Candidate 3 is real C but is a test harness
around someone else's library, with no numeric kernel of its own.

Mapping to the list: (j) for 1, 2, 4, 5; (l) for 3; **none** for (a) to (i) and (k).

## What was built and run here

Files written in `S:\tsotchke\rosette-wire\` (all mine except `upstream\`):
`rw_receipt.h`, `rw_receipt.c` (SHA-256 from FIPS 180-4, canonical JSON writer,
`receipt:1` emitter, RSC1 cell encoder; no heap, no floats), `receipt_test.c`
(known answers, a toy integer world, engine + verifier + two mutants),
`shim\dlfcn.h` (Win32 shim), `mock_statevec.c` (a mock library, not Moonlab),
`bell.req`, `bad.req`. Copied unmodified into `upstream\`:
`moonlab-abi-probe.c`, `canonical-json.lisp`, `graph.lisp`, `runtime.lisp`,
`wire-protocol.lisp`, `minimal-composition.json`, `LICENSE`, `NOTICE`.

The toy world in the test is a stand-in written for this audit: 4096 cells of
`u32` energy, pairwise transfers drawn from Philox4x32-10 keyed on
`(pair, tick)`, red/black pairing so threads never share a cell.

### Candidate 1 + 2 + 4: C receipt emitter (ran)

```
cd /s/tsotchke/rosette-wire
gcc -std=c2x -O2 -Wall -Wextra -o receipt_test.exe receipt_test.c rw_receipt.c -lpthread
./receipt_test.exe upstream/minimal-composition.json
```

No compiler warnings. Output (hashes shortened here with `...` only in the long
receipt lines; the full lines are in `S:\tsotchke\rosette-wire\receipts.jsonl`):

```
KAT sha256 abc=ok empty=ok 56byte=ok
KAT philox4x32-10 zero : 6627e8d5 e169c58d bc57ac4c 9b00dbd8 ok
KAT philox4x32-10 ones : 408f276d 41c83b0e a20bc7c6 6d5451fd ok
upstream minimal-composition.json: 215 bytes, writer output identical: yes
  id sha256:50276f5c23fc1c7c68617a9e31b0de143413f469faa3e53a4a804c3f67646a67
refusals: unsorted key err=2 (want 2), int>2^53-1 err=3 (want 3)
composition (267 bytes): {"capabilityGrants":[],"inputs":[],"limits":{"maxSteps":1},"name":"local.native-engine/selfcheck","nodes":[],"outputs":[],"requiredEvidence":["conservation.energy","determinism.replay"],"schema":"urn:rosette-wire:schema:composition:1","serviceBindings":[],"steps":[]}
  graph digest sha256:60117f0e690007e02997acc7fc444e0183ddf070934aee6c65e44f5c1360b47d
state hash T=1 sha256:272485d11f91992a6dd3154b61e6c4a078211bf2f89b0fa63ee7504460a28328
state hash T=8 sha256:272485d11f91992a6dd3154b61e6c4a078211bf2f89b0fa63ee7504460a28328  equal=yes total=6202461
EXECUTION RECEIPT (473 bytes, id sha256:944eba7098ceba140af9d105bdd9ac60a126c57660db68653277f34858b58b2d):
{"evidence":{"engine":{"hash":"sha256(le-u32-energy)","rng":"philox4x32-10"}},"graph":{"digest":"sha256:60117f0e...b47d"},"kind":"execution","outputs":{"cells":4096,"energyTotal":6202461,"seed":"0xa5a5f00dcafe1234","stateHash":"sha256:272485d1...8328","threads":8,"ticks":200},"schema":"urn:rosette-wire:schema:receipt:1","stepOrder":["run"],"verdict":"pass","violations":[]}
RSC1 cell: 512 bytes, header 52534331 tag_len=7 media_len=16 payload_len=473, id sha256:d81ab753613fbaca9842e7649c586117f9af3c1753ef63f6ad3d520838018830
VERIFICATION RECEIPT honest (verdict pass):
{"evidence":[{"name":"conservation.energy","result":"pass","value":{"claimedTotal":6202461,"initialTotal":6202461,"replayedTotal":6202461}},{"name":"determinism.replay","result":"pass","value":{"claimedStateHash":"sha256:272485d1...8328","replayThreads":3,"replayedStateHash":"sha256:272485d1...8328"}}],"graph":{...},"kind":"verification","outputs":[],"schema":"urn:rosette-wire:schema:receipt:1","stepOrder":[],"verdict":"pass","violations":[]}
VERIFICATION RECEIPT mutant thread-keyed RNG (verdict fail):
{"evidence":[{"name":"conservation.energy","result":"pass",...},{"name":"determinism.replay","result":"fail","value":{"claimedStateHash":"sha256:257ac6bc...8015","replayThreads":3,"replayedStateHash":"sha256:272485d1...8328"}}],...,"verdict":"fail","violations":[{"code":"evidence-failed","detail":"verifier returned failure","path":"$.evidence.determinism.replay","stage":"verification"}]}
VERIFICATION RECEIPT mutant energy leak (verdict fail):
{"evidence":[{"name":"conservation.energy","result":"fail","value":{"claimedTotal":6202460,"initialTotal":6202461,"replayedTotal":6202461}},{"name":"determinism.replay","result":"fail",...}],...,"verdict":"fail","violations":[{"code":"evidence-failed",...,"path":"$.evidence.conservation.energy",...},{"code":"evidence-failed",...,"path":"$.evidence.determinism.replay",...}]}
emit+sha256 one execution receipt: 1797 ns (noisy, parallel load; sink 54500104)
RESULT: PASS (0 failures)
exit=0
```

Independent cross-checks (ran):

```
head -c 215 upstream/minimal-composition.json | sha256sum
50276f5c23fc1c7c68617a9e31b0de143413f469faa3e53a4a804c3f67646a67 *-
```

matches the id my C code computed, and a Python check that each of the four
receipt lines parses and equals `json.dumps(v, sort_keys=True, separators=(',',':'))`:

```
execution pass keys True python-canonical-roundtrip-identical True sha256:944eba7098ceba14
verification pass keys True python-canonical-roundtrip-identical True sha256:8dae42e475206a03
verification fail keys True python-canonical-roundtrip-identical True sha256:e2d0fbaa819962f2
verification fail keys True python-canonical-roundtrip-identical True sha256:5c4eed7b3f4af212
ALL True
```

What this evidence does and does not show:

- Shown: the C writer reproduces the one canonical document the repo ships
  (`examples/minimal-composition.json`, 215 bytes once the checkout's CRLF is
  stripped); the receipts have the eight keys in canonical order; the replay
  verifier separates an honest run from two planted bugs, and the two bugs
  produce different violation sets.
- Not shown: that `bin/rosette verify` accepts these receipts. SBCL is missing,
  so the Lisp side never ran. The repo contains no pinned receipt digest to
  compare against (the only `sha256:` literals are a distribution id and a
  dummy `...0001`).
- The demo composition has `nodes: []` yet names two required evidences.
  Reading `graph.lisp:535-541`, the Lisp validator would refuse it with
  `missing-verifier` until a component node declares those verifiers. A fully
  valid composition needs a component descriptor, which I did not write
  because I could not validate it.
- The Philox known answers are the Random123 vectors as I remember them; they
  matched, but they were not fetched from a reference file today.
- The 1797 ns figure is for the toy receipt on a loaded machine.

### Candidate 3: the C probe (ran, against a mock)

Unmodified, it does not compile on this toolchain:

```
gcc -std=c2x -O2 -Wall -Wextra -o probe_asis.exe upstream/moonlab-abi-probe.c -lm
upstream/moonlab-abi-probe.c:4:10: fatal error: dlfcn.h: No such file or directory
```

With an 11-line `dlfcn.h` shim over `LoadLibraryA`/`GetProcAddress`, the same
unmodified source compiles with no warnings and runs. No Moonlab build exists
here, so I wrote a mock library exporting the 16 base symbols:

```
gcc -std=c2x -O2 -Wall -Wextra -shared -o mock_statevec.dll mock_statevec.c -lm
gcc -std=c2x -O2 -Wall -Wextra -Ishim -o probe.exe upstream/moonlab-abi-probe.c -lm
./probe.exe ./mock_statevec.dll < bell.req
ROSETTE-MOONLAB/1
ABI 0 0 0
OWNERSHIP 1
ERROR-PATH 1
STATE 4
AMP 7.07106781186547573e-01 0.00000000000000000e+00
AMP 0.00000000000000000e+00 0.00000000000000000e+00
AMP 0.00000000000000000e+00 0.00000000000000000e+00
AMP 7.07106781186547573e-01 0.00000000000000000e+00
END
exit=0
./probe.exe ./mock_statevec.dll < bad.req            -> exit=67   (unknown gate)
./probe.exe ./nope.dll < bell.req                    -> "Moonlab library unavailable", exit=69
SURFACES/2 request against the mock                  -> "missing public ABI symbol: measurement_probability_one", exit=66
./probe.exe                                          -> exit=64
```

This exercises the probe's protocol and its refusal paths. It says nothing
about Moonlab: the amplitudes above come from my mock. The `SURFACES/2` path
(measurement, noise channels, QGT/Chern, VQE gradient, GPU state) was not run
because it needs the real library.

What the probe is good for is its shape, which is worth copying:

1. A separate process that loads the library by path and binds **public symbols
   only**; a missing symbol is exit 66, never a silent skip.
2. A versioned first line (`ROSETTE-MOONLAB/1`), bounded inputs (qubits 1 to 20,
   at most 256 gates), an explicit `END`.
3. It deliberately calls the API wrongly first (`x(state, n)`) and requires an
   error, so "the error path works" is part of the record.
4. Line records with `%.17e`, which round-trips a double exactly.
5. sysexits-style codes: 64 usage, 65 bad request, 66 missing symbol, 67 gate
   failure, 69 library unavailable, 70/71 internal.

## Proposed integration

All of this is **tooling / evidence path only**. Nothing here runs in Tier 0,
Tier 1, Tier 2 or the world-field loops, and nothing belongs in a hot loop.
A receipt has no causal role in the simulation; its role is law 8 (honest
evidence) and law 6 (determinism) made checkable.

```c
/* engine side, called by the driver at a checkpoint, off the tick path */
typedef struct {
    uint64_t seed, tick;            /* emitted as "0x..." strings (above 2^53) */
    uint32_t threads;
    uint8_t  state_hash[32];        /* SHA-256 over the canonical SoA arena dump */
    uint64_t matter_total, energy_total;   /* exact integers */
} asmo_checkpoint;

size_t asmo_receipt_execution(const asmo_checkpoint *cp, const char *graph_digest,
                              char *out, size_t cap);      /* returns bytes, 0 on error */

/* verifier side: a separate executable that loads the engine DLL by public
   symbols (the probe pattern), replays from (seed, tick) at another thread
   count, and writes a verification receipt. Exit 0 = pass, 1 = fail. */
int asmo_verify_main(int argc, char **argv);
```

- Evidence names: `determinism.replay` (same seed, different thread count, same
  state hash) and `conservation.matter` / `conservation.energy` (initial total
  = claimed total = replayed total).
- Emit one execution receipt per checkpoint (for example every 4096 ticks) and
  one per test run; wrap it in an RSC1 cell beside the snapshot so the snapshot
  and its receipt share a content address.
- Keep the tick-level state hash a cheap integer hash; use SHA-256 only at
  checkpoints. SHA-256 over a multi-megabyte arena is a checkpoint cost, not a
  per-tick cost (not measured here).
- The CI lesion test should include at least one planted determinism bug and
  one planted leak, and require a `fail` verdict, as the repo's `mutate`
  example does for its factorial program.
- The Lisp verifier (`bin/rosette`) stays optional external tooling. If SBCL is
  ever installed, the first thing to do is feed it these receipts and a valid
  composition and record the result.

Effort: small. The emitter exists in scratch (about 250 lines of C); wiring it
to the real arena hash and adding the verifier executable is the remaining work.

## NOT usable, and why

- **All 438 Lisp files inside the simulation**: excluded by law 1 (Lisp is
  tooling only). This covers every numeric system in the repo: `quantum-vm`
  (state-vector tape VM with AD and QGT callbacks), `hyper-dual`,
  `probability-core`, `exact-linear-subquotient`, `smith-normal-form`,
  `reaction-network` / `reaction-balancer` (exact stoichiometry),
  `symmetric-crypto` (AES, ChaCha20, SHA-256, HKDF), `compression-codec`
  (Huffman, LZ77, DEFLATE encoder, range coder), `net-frame` (CRC32),
  `gpu-kernel-dsl` (s-expression to CUDA-C source generator).
- **Reusable ideas from those, as ideas only**: exact integer stoichiometry
  balancing as a conservation check for creature chemistry; an independent
  second implementation as the oracle for any small state-vector module; a
  range coder for snapshot compression. None was ported or measured here.
- **`gpu-kernel-dsl`**: generates CUDA-C text; this machine has no `nvcc`, so
  the output could not be compiled even as tooling.
- **`moonlab-abi-probe.c` as a kernel**: it contains no simulation code. Its
  only math is a 2x2 QWZ Hamiltonian callback and a finite-difference check,
  both in double precision, both test scaffolding.
- **`rawcap_client.c` / `rosette_sel4.h`** (28 lines): seL4 Microkit calls; no
  relevance to a Windows user-mode engine.
- **The full Component / Composition machinery** (typed ports, capabilities,
  service bindings, work scheduler, isolated worker, e-graph, HoTT/cubical,
  proof witnesses): far more protocol than a single-binary engine needs. Only
  the receipt and the canonical-id rule earn their keep.
- **Wire Frames** (60-byte header, `wire-protocol.lisp:152` onward): a stream
  transport; not read in detail, not needed for file snapshots.
- **(a)-(i), (k)**: nothing. No counter-based RNG in native code, no
  fixed-point kernel, no SO(3) code, no Ising/Hopfield, no GEMV, no Monte
  Carlo, no PDE solver, no shaders.

## Verdict

**adapt**, for the evidence path only. The receipt format and the
verify-by-rerun rule port cleanly to a small integer-only C emitter (done in
scratch, 0 test failures). The repo's own code is Lisp and stays outside the
simulation; acceptance by its Lisp verifier is unverified.
