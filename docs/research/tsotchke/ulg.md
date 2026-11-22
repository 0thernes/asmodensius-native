# Tsotchke audit: `ulg` (ULG Triad Demo)

Audit date 2026-11-17. Key `ulg`. Mirror `T:\mirrors\ulg` (read-only). Scratch `S:\tsotchke\ulg\`.

Verdict: **excluded_by_law** for the code (the whole runtime is JavaScript and WGSL-in-JavaScript,
run by a browser). Four of its compute designs were re-implemented from scratch in integer C23 and
ran here; those designs are worth carrying into the engine.

## 1. Repo facts

| Item | Value |
|---|---|
| HEAD | `2bcb16d_2026-09-22` (`git -C T:/mirrors/ulg log -1 --format=%h_%cs`), 1075 commits |
| License | **No LICENSE or COPYING file is tracked.** `package.json` has `"private": true` and no `license` field. Treat as all rights reserved. |
| Main language | JavaScript ES modules (323 `.mjs` + 287 `.js` of 851 tracked files). WGSL exists only as template strings inside `.js` files; there is no `.wgsl` file. |
| Other | 172 `.md` plan and handoff files, 34 JSON, 2 prebuilt `.wasm`, 1 C file (58 lines) |
| Size | 6.6 MB of tracked files, 273 MB on disk with `.git` |
| Build | `package.json` + Vite (`npm run dev`, `npm test`, `npm run build`). Not built here: it needs `npm install` (forbidden) and a WebGPU browser (forbidden). |
| What it is | A browser demo that wires PeerCompute (worker supervision), Eshkol (closure artifacts) and MoonLab (WASM quantum probe) around a WebGPU SPH / MLS-MPM particle solver. Its own README calls the physics toy or reference scoped and not scientifically validated. |

The largest sources are `src/runtime/peercomputeBrowserResidentHost.js` (10526 lines),
`ulg-gpu-abi/src/wgsl.js` (20066 lines of WGSL strings), `src/runtime/sphPhaseDemo.js` (6503) and
`src/runtime/webgpuRadixScanUnique.js` (4703).

## 2. Candidate kernels

All five are designs to re-implement. None is source that can be linked.

| # | Kernel | Where in the mirror | Source signatures | Source dependencies | Pure or stateful | Float or integer | Thread safety in source | Engine use |
|---|---|---|---|---|---|---|---|---|
| K1 | Cell directory: spatial key, stable radix sort, unique, CSR table | `src/runtime/webgpuRadixScanUnique.js` 587-709 (`webGpuStableRadixWgsl`: `initialize_indices`, `histogram`, `scatter`), 714-739 (`scan_histogram_serial`), 741-835 (`webGpuSortedUniqueWgsl`: `mark_heads`, `scatter_unique`, `finalize_unique`); `ulg-gpu-abi/src/schroederSpatialEpochWgsl.js` 85-87 (`signed_order_key`), 144-322 (`emit_spatial_keys`), 486-558 (`assemble_directory`). CPU reference: `src/runtime/spatialHash.js` 72-127 `buildSpatialHash(state, options)`, 129-191 `queryNeighborPairs(hash, {radius, includeSelf, symmetric, maxPairs})` | WebGPU device, bind groups, indirect dispatch plumbing (most of the 4703 lines) | Pure function of the key array | Keys and indices `u32`; positions `f32`, divided by a float spacing | GPU: 4-bit digits, per-workgroup histograms stored digit-major, one scan, disjoint scatter ranges. CPU reference: single threaded, `Map` with string keys | (l) neighbour search for perception, contact and feeding |
| K2 | Table-interpolation closure | `src/runtime/closureHandle.js` 88-101 `interpolate(left, right, x)`, `interpolateDerivative(left, right, x)`; 103-158 `createClosureHandle(closureArtifact)` returning `sample(coords)` | none | Pure after construction | `f64` in JS, `f32x4` rows on GPU | Read-only table | (b) fixed-point response curves |
| K3 | Antisymmetric edge messages | `src/runtime/edgeMessages.js` 78-150 `evaluateEdgeMessages({neighborGraph, pairs, particles, closureHandle, fields, tolerance})`, 152-196 `summarizeEdgeMessages(...)` | K1 pairs, K2 handle | Pure | `f64`; residual checked against `1e-12` | Single threaded | (b) exactly conserving pair exchange (law 5) |
| K4 | Cross-level reflux ledger | `ulg-gpu-abi/src/schroederCrossLevelRefluxLedger.js` 1-31 (status bits and phases), 33-190 (136-word header, 18-word rows), 222 `createSchroederCrossLevelRefluxLedgerLayout`, 252 `...LedgerHeader`, 341 `deriveSchroederCrossLevelRefluxEnergyClosure` | WebGPU buffers | Stateful: a phase machine with a terminal receipt | Residuals stored as `f32` bits and compared with tolerances | One GPU lane owns the ledger | (i) coupling world fields that run at different rates; (j) receipts |
| K5 | Quantized particle-to-grid deposit | `ulg-gpu-abi/src/wgsl.js` 9441 onward (`mlsMpmP2gGridProjectionWgsl`): 9519 `p2g_quantize(value: f32) -> i32`, 9523 `quadratic_weights(fx: f32) -> vec3<f32>`, 9774 `p2g_atomic_add(node_index: u32, mass: f32, momentum: vec3<f32>)` | WebGPU atomics | Stateful accumulation into a grid | Float products rounded to Q16.16, then `atomicAdd` on `i32` | Order independent by construction (integer atomics) | (i) creatures writing scent, heat or matter into world fields |

What each source design does, in one line:

- **K1.** Each particle gets an integer cell key; signed coordinates become order-preserving unsigned
  words with `x ^ 0x80000000`; rejected or inactive rows get the all-ones key so a stable sort puts
  them last; a head-flag pass plus an exclusive scan turns the sorted list into a compressed cell
  table (cell keys, offsets, members, particle-to-cell). The histogram is laid out as
  `digit * workgroup_count + workgroup`, so one serial scan gives every (digit, workgroup) its own
  output range. That layout is the reason the result cannot depend on how the work is split.
- **K2.** A sampled law (value and derivative on one axis) is interpolated linearly; a query outside
  the table throws instead of extrapolating, and the caller counts it as an out-of-range edge.
- **K3.** One closure sample per unordered pair; the force goes to the source with one sign and to
  the target with the other; the summary checks the per-pair residual and the net force.
- **K4.** Fine-level substeps accumulate what actually crossed a coarse/fine boundary; the ledger is
  sealed, applied once to the coarse side, and consumed. Each phase transition is checked and every
  misuse is counted and refused ("fail closed").
- **K5.** Quadratic B-spline weights over 27 nodes; contributions are converted to integers before
  they are added, so the sum does not depend on the order of the GPU invocations.

## 3. What was built and run here

Nothing from the mirror was compiled (it has no native kernel). The four C files below were written
for this audit from the designs above. The mirror's CPU reference `spatialHash.js` was copied
unmodified to `S:\tsotchke\ulg\ref\` and run under node as a tooling-only cross-check.
Compiler: `gcc.exe (MinGW-W64 x86_64-ucrt-posix-seh) 16.1.0`. Node `v25.8.2`.
Every compile printed no warning.

### 3.1 K1 cell directory: `S:\tsotchke\ulg\celldir.c`

```
cd /s/tsotchke/ulg && gcc -std=c2x -O2 -Wall -Wextra -o celldir.exe celldir.c && ./celldir.exe 20000 9 10 1 | tail -22
```

```
radix bits=4 chunks=1  sorted_hash=4c0c5254c8a84c45 == qsort reference
radix bits=4 chunks=2  sorted_hash=4c0c5254c8a84c45 == qsort reference
radix bits=4 chunks=3  sorted_hash=4c0c5254c8a84c45 == qsort reference
radix bits=4 chunks=8  sorted_hash=4c0c5254c8a84c45 == qsort reference
radix bits=4 chunks=24 sorted_hash=4c0c5254c8a84c45 == qsort reference
radix bits=4 chunks=61 sorted_hash=4c0c5254c8a84c45 == qsort reference
radix bits=8 chunks=1  sorted_hash=4c0c5254c8a84c45 == qsort reference
radix bits=8 chunks=2  sorted_hash=4c0c5254c8a84c45 == qsort reference
radix bits=8 chunks=3  sorted_hash=4c0c5254c8a84c45 == qsort reference
radix bits=8 chunks=8  sorted_hash=4c0c5254c8a84c45 == qsort reference
radix bits=8 chunks=24 sorted_hash=4c0c5254c8a84c45 == qsort reference
radix bits=8 chunks=61 sorted_hash=4c0c5254c8a84c45 == qsort reference
n=20000 world=512^3 cell=16 radius=16 dead_pct=10
signed_order_key order failures=0
live=17983 (expected 17983) invalid_or_dead=2017 cells=13822
directory invariant failures=0 sort mismatches=0
directory_hash=d90d039e5a9bd840
pairs=19883 checksum=4182093920
brute pairs=19883 checksum=4182093920 -> match
timing (noisy, parallel load): sort 0.18 ms, assemble 0.09 ms, 27-cell query 27.99 ms
RESULT PASS
```

What this shows: the sorted order is bit-identical for 1, 2, 3, 8, 24 and 61 work chunks and for
4-bit and 8-bit digits, and equals a `qsort` on (key, index). Ten percent of rows were marked dead
at random positions with the all-ones key and ended up outside the directory. The radius query
through the directory matches an O(n^2) scan. "chunks" is a loop in one thread that stands in for
worker threads; no real multi-threaded run was made.

Cross-check against the mirror's own JavaScript reference on identical integer positions:

```
cd /s/tsotchke/ulg && ./celldir.exe 2000 8 0 1 | tail -8 && node xcheck.mjs 2000 8
```

```
signed_order_key order failures=0
live=2000 (expected 2000) invalid_or_dead=0 cells=1599
directory invariant failures=0 sort mismatches=0
directory_hash=1a89e2ab431d5720
pairs=1875 checksum=1256386610
brute pairs=1875 checksum=1256386610 -> match
timing (noisy, parallel load): sort 0.02 ms, assemble 0.01 ms, 27-cell query 1.89 ms
RESULT PASS
js-reference n=2000 world=256^3 cells=1599 pairs=1875 checksum=1256386610
```

Same occupied cell count (1599), same pair count (1875), same pair checksum.

One million rows, no brute-force check:

```
cd /s/tsotchke/ulg && ./celldir.exe 1000000 12 10 0 | tail -8
```

```
n=1000000 world=4096^3 cell=16 radius=16 dead_pct=10
signed_order_key order failures=0
live=899940 (expected 899940) invalid_or_dead=100060 cells=876365
directory invariant failures=0 sort mismatches=0
directory_hash=95e5f102899eb731
pairs=99863 checksum=1540958184
timing (noisy, parallel load): sort 17.14 ms, assemble 8.21 ms, 27-cell query 1812.99 ms
RESULT PASS
```

Timings are noisy, parallel load, single thread. Sort plus assemble for one million rows took about
25 ms. The query took 1.8 s because the test finds each of the 27 neighbour cells with a binary
search over 876365 cell keys; that lookup is the test's own shortcut, not part of the design, and
the engine must not use it (see section 4).

### 3.2 K2 + K3 fixed-point closure and edge messages: `S:\tsotchke\ulg\closure_edge.c`

```
cd /s/tsotchke/ulg && gcc -std=c2x -O2 -Wall -Wextra -o closure_edge.exe closure_edge.c -lm && ./closure_edge.exe 4096 | tail -12
```

```
closure table: 65 samples, step 0.25, domain [0,16]
fixed vs double interpolation: max|dE|=0 max|d(dE/dr)|=0 (Q16.16 lsb=1.53e-05)
fixed vs analytic harmonic law: max|dE|=0.00781 (table bound h^2/8=0.00781)
out-of-domain samples refused: 64 of 64 probed
n=4096 pairs_in_domain=60502 degenerate=0 total_sampled_energy_q16=49310508768
integer: per-pair antisymmetric residual failures=0, net force=(0,0,0)
integer: accumulator hash forward=bf38429d4c7e3707 reversed=bf38429d4c7e3707 (order independent)
float32 contrast: net force=(0.000184,3.97e-05,-0.000102), components that change with pair order=9518 of 12288
RESULT PASS
```

What this shows: a Q16.16 table with a power-of-two step reproduces the double transcription of the
JavaScript interpolation, and refuses every query outside its domain. Over 60502 pairs the integer
force seen from the source and from the target cancel exactly for every pair, the net force is
exactly zero, and the per-particle accumulators have the same hash when the pairs are visited in
reverse. The same arithmetic in `float` leaves a net force of order 1e-4 and 9518 of 12288
accumulator components change with pair order. Caveat: the zero interpolation error is specific to
this toy harmonic table, whose increments are multiples of the step; for a general table expect up
to one unit in the last place.

### 3.3 K4 reflux ledger: `S:\tsotchke\ulg\reflux.c`

```
cd /s/tsotchke/ulg && gcc -std=c2x -O2 -Wall -Wextra -o reflux.exe reflux.c && ./reflux.exe | tail -8
```

```
coarse cells=32 fine cells=64 substeps=2 initial total=117799
with reflux ledger   : worst |total drift| over 5000 steps = 0, min cell = 240, rejects = 0, state hash = a8de3ddd42f44a28
lesion (no reflux)   : worst |total drift| over 5000 steps = 7924, min cell = 240, state hash = cf21bd0cdc16dd20
phase machine: misuse accepted=0 (want 0), rejects counted=6 (want 6), sealed=1 applied=1 consumed=1, cell=90 (want 90)
RESULT PASS
```

What this shows: a 1D integer diffusion field with a coarse half (one step) and a fine half (two
substeps) keeps its total exactly over 5000 steps when the ledger correction is applied, and drifts
by up to 7924 units out of 117799 when the correction is removed. That is a lesion with a measured
effect. The six misuse cases (apply before seal, seal with a missing substep, extra substep, wrong
generation, double apply, double consume) are all refused and counted. This is a 1D miniature; the
mirror's ledger also tracks momentum, angular momentum and energy, which were not reproduced.

### 3.4 K5 integer deposit: `S:\tsotchke\ulg\p2g.c`

```
cd /s/tsotchke/ulg && gcc -std=c2x -O2 -Wall -Wextra -o p2g.exe p2g.c && ./p2g.exe 200000 | tail -8
```

```
particles=200000 grid=64^3 deposited total=10006558099
weight partition-of-unity failures=0 of 256
integer: grid total=10006558099 (residual 0), min node=0
integer: grid hash forward=864af7231628a66a reversed=864af7231628a66a (order independent)
float32 contrast: grid total residual=-5.59128, nodes that change with particle order=163712 of 262144
RESULT PASS
```

What this shows: with Q16 B-spline weights that sum to exactly 65536 and the rounding remainder
given to the centre node on each axis, 200000 deposits land on a 64^3 grid with a residual of
exactly zero and the same grid hash in forward and reverse particle order. The float version loses
5.6 units and 163712 of 262144 nodes change with order. The mirror's WGSL is order independent but
not exactly conservative, because it rounds float products; the exact split is this audit's change.

## 4. Proposed integration

Names are suggestions. All state is integer and lives in the pre-allocated arena.

**K1, cell directory. World fields stage, once per tick (or once per perception phase).**

```c
typedef struct {
    uint32_t  n_live, n_cells;
    uint32_t *members;          /* [n_live]    row indices grouped by cell, ascending inside a cell */
    uint32_t *cell_offsets;     /* [n_cells+1] CSR boundaries into members                         */
    uint32_t *cell_keys;        /* [n_cells]   ascending cell ordinals                             */
    uint32_t *particle_to_cell; /* [capacity]  cell slot, 0xffffffff when not live                 */
} asm_celldir;

static inline uint32_t asm_signed_order_key(int32_t v);           /* v ^ 0x80000000            */
void asm_cellkey_emit(const int32_t *x, const int32_t *y, const int32_t *z,
                      const uint8_t *alive, uint32_t capacity,
                      const asm_cell_atlas *atlas, uint32_t *keys);   /* dead or out of atlas -> all ones */
void asm_radix_sort_indices(const uint32_t *keys, uint32_t n, uint32_t n_workers,
                            uint32_t *sorted, uint32_t *scratch, uint32_t *hist);
void asm_celldir_assemble(const uint32_t *keys, const uint32_t *sorted,
                          uint32_t capacity, asm_celldir *out);
```

Notes for the port: use 8-bit digits (four passes; the source's 4-bit digits fit a WebGPU workgroup
limit, not a CPU); keep the digit-major histogram so the output is the same for any worker count;
give the colony or species id the high key bits so a colony's members are contiguous. For the
neighbour walk do not search the key list per cell: either keep a dense cell-to-slot table when the
atlas is bounded, or walk the sorted cells and merge the three adjacent rows. Births and deaths
need no separate compaction pass, because the sentinel key already moves dead rows to the end; the
sorted order can also be used to reorder the structure-of-arrays blocks so neighbours share cache
lines.

**K2, table closure. Tier 0, every tick, inside the pair loop.**

```c
typedef struct { int32_t value[65]; int32_t deriv[65]; uint32_t r_max; uint8_t shift; } asm_closure;
/* returns 0 when sampled, 1 when r is outside the table (caller counts it, never extrapolates) */
static inline int asm_closure_sample(const asm_closure *t, uint32_t r, int32_t *value, int32_t *deriv);
```

65 Q16.16 pairs are 520 bytes, so several tables per species stay inside L1. Tables are filled from
genome integers during development. Uses: attraction and repulsion laws, sensor response against
distance, metabolic rate curves.

**K3, antisymmetric pair exchange. Tier 0, every tick.**

```c
/* one evaluation per unordered pair; the same integer is added to one row and subtracted from the other */
void asm_pair_exchange(const asm_celldir *dir, const asm_closure *law,
                       const int32_t *x, const int32_t *y, const int32_t *z,
                       int64_t *acc_x, int64_t *acc_y, int64_t *acc_z);
```

This is the rule for every transfer between two creatures (momentum, energy, matter): compute once,
apply with opposite signs, in integers. For threads, colour the cells or give each worker a private
accumulator and add them afterwards; integer addition makes either choice give the same result.

**K4, reflux ledger. World fields at mixed rates, and Tier 2 to Tier 0 coupling.**

```c
typedef enum { ASM_RL_ALLOCATED, ASM_RL_ACCUMULATING, ASM_RL_SEALED, ASM_RL_APPLIED, ASM_RL_CONSUMED } asm_rl_phase;
typedef struct { asm_rl_phase phase; uint32_t generation, substeps, expected_substeps, rejects;
                 int64_t coarse_flux, fine_flux; } asm_reflux_ledger;
int asm_rl_begin(asm_reflux_ledger *l, uint32_t generation, uint32_t expected_substeps);
int asm_rl_record_coarse(asm_reflux_ledger *l, int64_t flux);
int asm_rl_accumulate_fine(asm_reflux_ledger *l, int64_t flux);
int asm_rl_seal(asm_reflux_ledger *l);
int asm_rl_apply_coarse(asm_reflux_ledger *l, uint32_t generation, int64_t *coarse_cell);
int asm_rl_consume(asm_reflux_ledger *l);
```

Use it wherever a slow, coarse quantity (a field updated every 32 to 64 ticks, or a colony-level
store) exchanges matter or energy with something updated every tick. The fast side records what it
really took; the slow side is corrected once per slow step; the sum never drifts. The reject
counter doubles as evidence for the conservation audit.

**K5, integer deposit. World fields, every tick.**

```c
static inline void asm_bspline_q16(uint32_t frac_q8, uint32_t w[3]);   /* weights sum to 65536 */
void asm_field_deposit(int64_t *grid, uint32_t nx, uint32_t ny, uint32_t nz,
                       const uint32_t pos_q8[3], int64_t amount);      /* exact: grid gains amount */
```

Creatures emit scent, heat or waste into a field with a smooth footprint, with an exact total and
no dependence on thread order. The matching read (grid to creature) uses the same weights.

**Scheduling and peer compute (tooling only).** `src/runtime/WorkerSupervisor.js` (218 lines) and
`src/runtime/ChildWorkerLeaseManager.js` (64 lines) give each root task a tree of workers with
counted, expiring, revocable leases and a `cancelTree(rootTaskId)`. Leases expire on `Date.now()`,
so this cannot sit inside a deterministic tick. It is a reasonable pattern for the experiment
runner that launches batches of seeds, nothing more.

## 5. Not usable, and why

- **All source code in the repo.** JavaScript and WGSL run by a browser: forbidden inside the
  simulation by law 1. There is also no license file, so even a line-by-line port of the text would
  need the author's permission; the C in section 3 was written from the algorithms, not transcribed.
- **The WGSL shaders as shaders.** No Vulkan SDK, no `glslc`, no WGSL compiler on this machine, and
  the engine's GPU path is CUDA/PTX. Only the algorithms carry over.
- **Float state.** Positions, spacings and every conservation residual in the repo are `f32`, with
  tolerances. The engine needs integers with a residual of zero; every port above changes that.
- **`src/runtime/spatialHash.js` as written.** String cell keys in a `Map`, one object per pair, a
  sort per query. Useful only as the reference it was used as here.
- **SPH / MLS-MPM / reaction / thermal solver** (`ulg-gpu-abi/src/wgsl.js`, 20066 lines, and the
  `schroeder*` files beside it). Too large to audit in this pass; the README states it is not scientifically
  validated. Only the deposit scheme (K5) was looked at. No fluid solver from here is proposed.
- **MoonLab WASM Bell-state probe, Ising calibration, Eshkol closure bundle.** Prebuilt WebAssembly
  and JSON contracts for a browser; the numerics belong to the MoonLab and Eshkol repos and are
  covered by their own audits. Nothing here is a quantum-circuit kernel that can be lifted.
- **`tools/moonlab-microphysics/h2_h2o_microphysics.c`** (the only C file). A dense Pauli-matrix
  build plus shifted power iteration for a ground-state energy; it needs MoonLab's
  `src/algorithms/vqe.h`, uses `double _Complex`, and is chemistry tooling with no role in a
  creature. Not built.
- **Material-property data** (`data/material-properties/`). Datasets: excluded by law 3.
- **Rendering** (marching cubes, surface draw, three.js worker tree). Display code for the demo; it
  does not feed back into simulation state, so it fails law 4 as a mechanism. Category (k): none.
- **Categories with nothing to offer:** (a) RNG or hash mixers, (c) quantum circuits, (d) SO(3)
  representation theory, (e) quantum geometric tensor, (f) spin or Hopfield networks, (g) GEMV or
  GEMM kernels, (h) Monte Carlo samplers. The Ising and Bell items are calls into MoonLab.

## 6. Limits of this audit

- Read in full: `README.md`, `spatialHash.js`, `closureHandle.js`, `particleState.js`, the parts of
  `edgeMessages.js`, `webgpuRadixScanUnique.js`, `schroederSpatialEpochWgsl.js` and
  `schroederCrossLevelRefluxLedger.js` cited above. Everything else was listed or searched, not read.
- The repo's own test suite (`npm test`, 223 files under `tests/`) was not run: it needs installed
  packages. No WGSL was executed.
- No multi-threaded run and no CUDA run were made. Thread-count independence is shown by a chunked
  single-thread loop that follows the same data layout.
- No prior-art search was done and nothing here is claimed as new: a radix-sorted cell list, flux
  refluxing between grid levels and B-spline particle-to-grid transfer are all standard techniques.
- One `git status --porcelain` was run inside the mirror by mistake (it printed nothing: the tree is
  clean). It may have refreshed git's index stat cache; no file content changed.
