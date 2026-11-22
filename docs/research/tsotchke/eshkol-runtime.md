# Tsotchke audit: eshkol-runtime (native C/C++ kernels inside the Eshkol repo)

Audit date 2026-11-17. Scope: the C/C++ runtime and native kernels in
`T:\Eshkol\eshkol_repo` (not the compiler, not the `.esk` library). Everything
marked "ran" below was run in this session from `S:\tsotchke\eshkol-runtime\`
with gcc 16.1.0 (WinLibs, UCRT). All timings are noisy, parallel load.

## Repo facts

| Item | Value |
|---|---|
| Path | `T:\Eshkol\eshkol_repo` (read-only; nothing built or changed there) |
| HEAD | `eb224260_2026-11-02` (from `git -C T:/Eshkol/eshkol_repo log -1 --format=%h_%cs`) |
| License | MIT License, "Copyright (c) 2025 tsotchke" (`LICENSE`) |
| Main language | Eshkol (Scheme dialect): 2503 tracked `.esk` files. Native side: 269 `.cpp`, 163 `.h`, plus C files; `lib/` holds about 343k lines of C/C++/headers |
| Size | 314 MB on disk, 4860 tracked files |
| README version | v1.3.6-evolve (prepared for publication, per README) |

The native code is mostly an LLVM code generator (`lib/backend/llvm_codegen.cpp`,
47.5k lines), a bytecode VM (`lib/backend/vm_*.c`), a hosted runtime tied to
tagged values and a growing arena, and an "agent" FFI layer (HTTP, SQLite,
subprocess, terminal). Standalone numeric kernels callable from C are few.

## Candidates

| # | Kernel | File and lines | Signatures (main) | Dependencies | Pure / stateful | Float / integer | Thread safety | Engine slot |
|---|---|---|---|---|---|---|---|---|
| 1 | Exact fixed-point and integer dot | `lib/math/fixed_point/eshkol_fixed_point.h` 1-238; `.c` 1-613 (dot kernels 521-612, fixed mul 288-300, SplitMix64 127-132) | `esk_i128 esk_idot_i8(const int8_t*, const int8_t*, size_t)`; `esk_idot_i16`, `esk_idot_i32`; `bool esk_imatmul_i32(const int32_t*, const int32_t*, size_t rows, size_t inner, size_t cols, esk_i128_abi* out)`; `esk_fixed esk_fixed_mul(esk_fixed, esk_fixed, esk_round_mode, esk_overflow_mode, esk_rng*, bool*)`; `esk_fixed_add/sub/div/convert`; `void esk_accum128_merge(esk_accum128*, const esk_accum128*)`; `uint64_t esk_rng_next_u64(esk_rng*)` | C standard library only; needs `__int128`; `<math.h>` only for the double conversions | Pure (the RNG takes its state by pointer) | Integer core; doubles only in `from_double`, `to_double` and the `scale_a*scale_b` step of `esk_dot_exact_*` | Re-entrant, no globals | (b) and (g) |
| 2 | DNC content-addressed memory | `lib/core/dnc_core.h` 33-199 (header-only, `static inline`) | `void dnc_content_weights(const double* mem, int N, int W, const double* key, double beta, double* w)`; `void dnc_read_mem(const double* mem, int N, int W, const double* w, double* out)`; `void dnc_write_mem(double* mem, double* usage, int N, int W, const double* w, const double* erase, const double* add)`; `void dnc_alloc_weights(const double* usage, int N, double beta, double* w)`; `void dnc_loc_weights(double addr, double beta, int N, double* w)`; `double dnc_loss_and_grads(...)` | `<math.h>` (`exp`, `sqrt`) | Pure functions over caller-owned buffers | Float (double) | Re-entrant | (f) |
| 3 | Factor-graph belief propagation, free energy, expected free energy (the natives the repo calls its "consciousness engine") | `lib/core/inference.cpp` 218-775; header `inc/eshkol/core/inference.h` 1-140 | `eshkol_factor_graph_t* eshkol_make_factor_graph(arena_t*, uint32_t num_vars, const uint32_t* var_dims)`; `eshkol_factor_t* eshkol_make_factor(arena_t*, const uint32_t* var_indices, uint32_t num_vars, const double* cpt_data, const uint32_t* dims)`; `void eshkol_fg_add_factor(arena_t*, eshkol_factor_graph_t*, eshkol_factor_t*)`; `bool eshkol_fg_infer(arena_t*, eshkol_factor_graph_t*, uint32_t max_iterations, double tolerance)`; `double eshkol_free_energy(const eshkol_factor_graph_t*, const double* observations, uint32_t num_obs)`; `double eshkol_expected_free_energy(arena_t*, const eshkol_factor_graph_t*, uint32_t action_var, uint32_t action_state)` | C++ translation unit; Eshkol headers; 5 external symbols (`arena_allocate_aligned`, `arena_allocate_zeroed`, `arena_allocate_with_header`, `get_global_arena`, `eshkol_printf`); `exp`, `log`; `alloca` inside loops | Stateful (graph, messages, beliefs mutate in place; pointer-chasing layout) | Float (double, log space) | Not thread safe on one graph; separate graphs are independent | (l) decision module |
| 4 | FNV-1a 64 hash | `lib/core/merkle.c` 24-35 | `int64_t eshkol_fnv1a_64(const char* data, int64_t len)` | none | Pure | Integer | Re-entrant | (j) receipts (non-cryptographic) |
| 5 | Constant-curvature geometry (sphere and hyperbolic exp/log maps, distance, compensated dot) | `inc/eshkol/backend/riemannian_core.h` 1-2691 (e.g. `eshkol_rm_sphere_exp_map` 1819, `eshkol_rm_sphere_log_map` 1840, `eshkol_rm_dot` 158) | `static const char* eshkol_rm_sphere_exp_map(const double* x, const double* v, ...)` and similar; returns an error string or NULL | `<math.h>`, `<float.h>`; C++ branch pulls `<vector>` | Pure | Float (double, double-double helpers, `fma`) | Re-entrant | none directly (see "not usable") |

Not compiled in this audit: candidate 5. It was read, not built.

## What was built and run

### Candidate 1: fixed-point (ran)

The repo ships its own standalone tests (`tests/fixed_point/`). Copied
`eshkol_fixed_point.c/.h`, `test_i128.c`, `test_fixed.c`, `test_dot_exact.c`,
`test_harness.h` to `S:\tsotchke\eshkol-runtime\fixed_point\`.

```
cd /s/tsotchke/eshkol-runtime/fixed_point
gcc -std=c2x -O2 -Wall -Wextra -I. test_i128.c      eshkol_fixed_point.c -lm -o test_i128.exe      && ./test_i128.exe
gcc -std=c2x -O2 -Wall -Wextra -I. test_fixed.c     eshkol_fixed_point.c -lm -o test_fixed.exe     && ./test_fixed.exe
gcc -std=c2x -O2 -Wall -Wextra -I. test_dot_exact.c eshkol_fixed_point.c -lm -o test_dot_exact.exe && ./test_dot_exact.exe
```

Output tails (no compiler warnings):

```
test_i128: 33/33 checks passed
test_fixed: 28/28 checks passed
PASS: accum128 partial-merge == single-stream
PASS: accum128 merge sums counts
test_dot_exact: 23/23 checks passed
```

Own probe `engine_probe.c` (int8 GEMV 64 inputs by 32 outputs, column
permutation, Q16.16 multiply against a hand-written int64 reference):

```
gcc -std=c2x -O2 -Wall -Wextra -I. engine_probe.c eshkol_fixed_point.c -lm -o engine_probe.exe && ./engine_probe.exe
```

```
gemv 64x32 int8: mismatch_vs_int32=0 mismatch_permuted=0 hash=9fdee8eaceb6eb42
q16.16 mul vs int64 reference over 200000 pairs: truncate_mismatch=0 nearest_even_mismatch=0
64-elem int8 dot: esk_idot_i8 (i128 acc) 27.29 ns/call, int32 acc 1.77 ns/call (noisy, parallel load)
sizeof(esk_fixed)=32
exit=0
```

Reading of the result: the kernel is correct and order-independent, but the
i128 accumulator is about 15 times slower than a plain int32 accumulator at a
creature-sized dot (64 int8 terms cannot exceed 64*128*128 = 2^20, so int32 is
already exact). `esk_fixed` is a 32-byte struct carrying W and F per value,
which does not fit structure-of-arrays state.

### Candidate 2: DNC memory (ran)

Copied `dnc_core.h` to `S:\tsotchke\eshkol-runtime\dnc\`; probe `dnc_probe.c`
stores 16 random patterns (width 8) in a 32-row bank, recalls each from a
corrupted cue, checks least-used allocation and compares the analytic gradient
with central finite differences.

```
cd /s/tsotchke/eshkol-runtime/dnc
gcc -std=c2x -O2 -Wall -Wextra -I. dnc_probe.c -lm -o dnc_probe.exe && ./dnc_probe.exe
```

```
recall from corrupted cue: 16/16 rows correct, worst L1 read error 1.5688
least-used allocation picks row 17 (rows 0..15 are written): free row
loss 2.060280; max |analytic - finite difference|: key 2.849e-10, memory 3.188e-10
content read N=32 W=8: 267.1 ns per read (double, exp, sqrt; noisy, parallel load)
exit=0
```

Reading: the argmax row is right for every cue, and the hand-derived gradient
matches finite differences. The soft (blended) read is poor at beta = 30 with
random width-8 patterns (worst L1 error 1.57), so a hard winner-row read is the
form worth porting. 267 ns per read is far too slow for every tick.

### Candidate 3: belief propagation (ran, with a finding)

Copied `inference.cpp` to `S:\tsotchke\eshkol-runtime\inference\`. It compiles
alone; the only undefined non-libc symbols are the five listed in the table.
Driver `bp_probe.c` is plain C with a 1 MB static bump arena providing those
five stubs.

```
cd /s/tsotchke/eshkol-runtime/inference
g++ -std=c++20 -O2 -c -I T:/Eshkol/eshkol_repo/inc -I T:/Eshkol/eshkol_repo/lib/core inference.cpp -o inference.o
gcc -std=c2x -O2 -Wall -Wextra -c -I T:/Eshkol/eshkol_repo/inc bp_probe.c -o bp_probe.o
g++ bp_probe.o inference.o -o bp_probe.exe && ./bp_probe.exe
```

```
BP converged=1  p(S=1|O=1)=0.500000  exact Bayes=0.850000  abs err=3.50e-01
free energy F=0.336672 (with observation pair O=1)
expected free energy G(a=0)=0.510826  G(a=1)=0.510826  -> picks a=0 (lower is preferred)
arena bytes used=656
[evidence-factor variant] converged=1  p(S=1|O=1)=0.850000  abs err vs 0.85=7.00e-13
[evidence-factor variant] G(a=0)=1.550292  G(a=1)=0.012235  -> picks a=1
clamp variant matches Bayes: NO; evidence-factor variant matches Bayes: yes
exit=0
```

Finding (observed in this probe, C entry points of `inference.cpp` only; the VM
path and the Eshkol-level builtin were not run): clamping an observed variable
the way the runtime's observe routine does (`inference.cpp` 1243-1255: belief
set to 0 / -1e30 and `observed[v] = true`) pins that variable's own belief, but
`compute_var_to_factor_message` (452-486) never reads the clamp, so the
evidence does not reach neighbours. A two-variable model with an 0.85-accurate
sensor returned posterior 0.5 instead of 0.85. Feeding the same observation as
a unary evidence factor gives 0.85 to 7e-13, and the expected-free-energy call
then separates the two actions. The repo test
`tests/vm/kb_factor_graph_extensions_regression.esk` checks only that the
clamped variable itself is clamped. Other costs: `alloca` inside the message
loops (stack grows until the function returns), `exp`/`log` per message entry,
pointer-per-message layout.

### Small extras (ran)

Copied `merkle.c`, `quantum_rng.c`, `quantum_rng.h` to
`S:\tsotchke\eshkol-runtime\misc\`; probe `misc_probe.c`.

```
cd /s/tsotchke/eshkol-runtime/misc
gcc -std=c2x -O2 -Wall -Wextra -I. misc_probe.c merkle.c quantum_rng.c -lm -o misc_probe.exe && ./misc_probe.exe
```

```
fnv1a64("a")=af63dc4c8601ec8c expect af63dc4c8601ec8c OK
fnv1a64("foobar")=85944171f73967e8 expect 85944171f73967e8 OK
quantum_rng same-seed streams: 0/64 outputs equal (first feb497d0f5e83042 vs 3f27126b0219576b)
exit=0
```

(One warning: unused variable `aux` in `quantum_rng.c:139`.) Two contexts
created from the same explicit 16-byte seed produced different streams, and a
second run produced different numbers again. `quantum_rng.c` mixes wall clock,
process id, `clock()`, a stack address and RDTSC, and its mixer calls `sin`,
`cos`, `sqrt`. It is a classical mixer that is not reproducible.

## Proposed integration

Names below are proposals for our engine; none exists yet.

1. **Exact integer GEMV / dot, as a reference oracle and for wide reductions.**
   Tooling plus Tier 2 and world fields. Keep the Eshkol code unchanged in the
   test tree as the oracle; in the engine use a trimmed form:
   ```c
   /* Tier 0 hot loop: int32 accumulator, exact while n * 2^14 < 2^31 */
   void an_gemv_i8(int32_t *restrict y, const int8_t *restrict W,
                   const int8_t *restrict x, uint32_t rows, uint32_t cols);
   /* Tier 2 / world-field reductions over many creatures: order-independent */
   typedef struct { int64_t lo; int64_t hi; } an_acc128;
   void an_acc128_add_i64(an_acc128 *a, int64_t v);
   void an_acc128_merge(an_acc128 *dst, const an_acc128 *src);
   /* fixed-point multiply with the rounding mode as an argument, never global */
   int32_t an_q16_mul(int32_t a, int32_t b, int round_mode);
   ```
   What we take from Eshkol: the merge-able exact accumulator (per-thread
   partial sums combine to the same bits for any thread count, which is the
   determinism law for colony energy and matter totals), rounding mode as a
   per-operation argument, and the contract tests (shuffle invariance) as our
   own test pattern. Stochastic rounding must draw from Philox, not SplitMix64.
2. **Content-addressed associative memory.** Tier 1 (phased, every 32 to 64
   ticks), per creature or per colony. Port to integers:
   ```c
   /* bank: rows x width int8, row norms cached as int32; returns winner row */
   uint32_t an_cam_recall_i8(const int8_t *bank, const int32_t *row_norm2,
                             uint32_t rows, uint32_t width, const int8_t *key);
   /* least-used allocation: smallest usage counter wins, ties to lowest index */
   uint32_t an_cam_alloc(const uint16_t *usage, uint32_t rows);
   void an_cam_write_i8(int8_t *bank, int32_t *row_norm2, uint16_t *usage,
                        uint32_t row, const int8_t *value, uint32_t width);
   ```
   Cosine ranking without division: compare `dot_i^2 * norm_j` with
   `dot_j^2 * norm_i` in int64 (sign handled separately). No `exp`, no `sqrt`.
   Causal role: a lesion (zeroing the bank) must change recall-dependent
   behaviour; that is the test to write.
3. **Discrete action selection by expected free energy.** Tier 1, per
   creature, on a tiny fixed graph (for example 2 to 4 hidden states, 2 to 4
   actions). Rewrite, do not port: integer log-domain tables (Q8.8 negative
   log-probabilities), min-sum messages, evidence entered as a unary factor.
   ```c
   /* returns the action index with the lowest integer expected free energy */
   uint32_t an_efe_pick(const int16_t *nll_lik, const int16_t *nll_pref,
                        const int16_t *nll_prior, uint32_t n_state,
                        uint32_t n_obs, uint32_t n_act, uint32_t obs);
   ```
   This is a mechanistic indicator and a decision rule, nothing more.
4. **FNV-1a 64.** Tooling only (cheap snapshot checksums). It is not
   collision-resistant; evidence receipts that must resist tampering need a
   real hash, which this repo only provides through OS crypto libraries.

## Not usable, and why

- `lib/quantum/quantum_rng.c`: not reproducible from a seed (ran, above), uses
  libm in the mixer. Breaks law 6 and must not be called quantum randomness.
- `lib/core/prng.cpp`: a drand48-style 48-bit LCG, sequential state, allocates
  through the Eshkol arena. Not counter-based; Philox stays.
- (a) counter-based RNG: none in native code (no Philox or Threefry; grep).
- (c) state-vector simulation: none here. `lib/agent/c/agent_quantum.c` is a
  shim over the separate Moonlab library and returns "unavailable" without it.
- (d) spherical harmonics, Wigner-D, Clebsch-Gordan: none (grep found no
  native or `.esk` file). `riemannian_core.h` has sphere exp/log maps only, in
  double with libm; ideas only.
- (e) quantum geometric tensor: one mention, in the Moonlab shim. None native.
- (f) Ising / Hopfield: none (grep). The DNC memory is the nearest thing.
- (h) Metropolis / path-integral samplers: none in native code (grep).
- (i) PDE solvers: only `lib/core/pde/ns-residual.esk`, which is Eshkol source;
  excluded by the native-only law. No heat or diffusion kernel in C.
- (k) rendering / shaders: none in the runtime (`gpu_cuda_kernels.cu` and the
  Metal files are tensor back ends).
- `lib/backend/blas_backend.cpp`: f64 AVX GEMM micro-kernels with back-end
  dispatch. Float, C++, summation order depends on the path taken; our GEMV is
  int8 and tiny.
- `lib/core/runtime_tensor_quant.cpp`: GGUF q8_0 / q4_0 weight dequantisation
  for a pretrained language model. Excluded by the no-LLM law.
- `lib/backend/weight_matrices.c`, `lib/core/sdnc_core.h`: a bytecode VM
  compiled into transformer weights (float). A VM inside the simulation is
  forbidden.
- `lib/core/workspace.cpp`: the global-workspace step calls module functions
  that are Eshkol closures (tagged values). Only the idea is reusable: salience
  scores, one winner broadcast per cycle.
- `lib/core/runtime_autodiff.cpp`, `runtime_taylor.c`, `tensor_backward.cpp`:
  bound to tagged values, object headers and the arena. Possibly useful
  off-line for checking a hand-derived gradient; not inside the simulation.
- `lib/core/arena_memory.h`: growing malloc-backed block arena with object
  headers, mutex variants and scope push/pop. Opposite of a pre-allocated,
  page-locked arena; scope push/pop is the only idea worth keeping.
- `lib/agent/c/agent_compression.c`: thin zlib wrapper (the codec itself
  is zlib, which the repo fetches at build time; not built in this audit). `lib/core/crypto_primitives.c`: SHA-256 via BCrypt /
  OpenSSL / CommonCrypto, no portable implementation to lift.
- SIMD: the only intrinsics in the runtime are in `blas_backend.cpp`.

## Verdict

**adapt.** One piece is usable nearly as is for tooling (the fixed-point
library with its tests); the DNC memory and the belief-propagation code need
integer ports or rewrites before anything enters a hot loop. Nothing here
supplies RNG, state-vector simulation, SO(3) representation theory, spin
networks, samplers, PDE solvers, codecs or shaders.

## Files

- `S:\tsotchke\eshkol-runtime\fixed_point\engine_probe.c` (own probe) plus the
  copied library and repo tests
- `S:\tsotchke\eshkol-runtime\dnc\dnc_probe.c`
- `S:\tsotchke\eshkol-runtime\inference\bp_probe.c`
- `S:\tsotchke\eshkol-runtime\misc\misc_probe.c`
