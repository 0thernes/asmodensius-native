# Tsotchke audit: qgtl (quantum_geometric_tensor)

Audit date 2026-11-17. Everything marked **ran** was executed in this session with the
command shown. Scratch folder: `S:\tsotchke\qgtl\`. The mirror at `T:` was only read.
The prior audit (`W:\Glossodrom Language Lab\docs\research\qgtl.md`) was used as a map and
re-checked; where this note says "confirmed" the check was re-run here, not copied.

## Verdict

**adapt.** About 130 lines out of 201,530 are worth carrying: the state+derivative
quantum-geometric-tensor kernels (Fubini-Study metric, Berry curvature, Berry phase, ray
distance). They compile standalone and match closed forms to 4e-15. They are double
precision and carry an engine handle, so they need the trimmed integer port that is already
written and tested in scratch (`an_qgt.c`). One further kernel turned out to be real and was
not in the prior audit: real spherical harmonics up to degree 4. Everything else that was
executed is wrong, does not compile, or is scaffold.

## Repo facts

| Item | Value |
|---|---|
| Path | `T:\mirrors\quantum_geometric_tensor` |
| HEAD | `770994a_2026-01-09` (**ran** `git -C T:/mirrors/quantum_geometric_tensor log -1 --format=%h_%cs`) |
| License | MIT License, "Copyright (c) 2024 Tsotchke" (`LICENSE`, lines 1-3) |
| Language | C (CMake project, C11); no interpreted-language core |
| Size | 51 MB on disk with `.git`, 47 MB working tree; 770 tracked files; `src/` 234 `.c` files = 201,530 lines; `include/` 309 headers; 89 `tests/test_*.c` (**ran** `find`/`wc`) |
| Builds here as a whole | No. Not attempted in this session; the prior audit's reasons were spot-checked (no BLAS/LAPACK discovery on Windows, POSIX-only headers). Only single translation units and extracted fragments were built. |
| Compiler used | `gcc.exe (MinGW-W64 x86_64-ucrt-posix-seh, built by Brecht Sanders, r2) 16.1.0` |

## Candidates

| # | Kernel | File and lines (under `src/quantum_geometric/`) | Signature | Deps | Pure / stateful | Number type | Thread safety | Executed result |
|---|---|---|---|---|---|---|---|---|
| 1 | Fubini-Study metric + Berry curvature from a state and its parameter derivatives | `distributed/differential_geometry.c:2819-2906` | `bool diffgeo_compute_fubini_study(diffgeo_engine_t*, const ComplexDouble* state, size_t dim, const ComplexDouble* param_derivatives, size_t num_params, double* metric_out)`; `bool diffgeo_compute_berry_curvature(...same..., double* curvature_out)` | libc only (file includes `<cblas.h>` but calls nothing from it) | math is pure; increments `engine->stats` counters | double | not safe on a shared engine (plain counter writes); safe with one engine per thread | **correct**, max abs error 3.6e-15 against closed forms |
| 2 | Berry phase of a closed loop of states; Fubini-Study ray distance | same file `:2908-2945`, `:377-413` | `ComplexDouble diffgeo_compute_berry_phase(diffgeo_engine_t*, const ComplexDouble** states, size_t num_states, size_t dim)`; `double diffgeo_distance(diffgeo_engine_t*, diffgeo_point_t* p, diffgeo_point_t* q)` | libc (`sqrt`, `acos`) | as above; distance needs heap-allocated `diffgeo_point_t` | double | as above | **correct**, phase equals the exact discrete value to 1e-12, distances pi/4 and pi/2 to 4e-15 |
| 3 | Real spherical harmonics, degree 0..4 (25 functions) | `core/molecular_geometric_operations.c:22-124` | `static void compute_spherical_harmonics(const double* r, double* Y_lm, size_t l_max)` with `Y_lm[l*l + l + m]` | libc (`sqrt`, `atan2`, `cos`, `sin`) | pure | double | safe (no shared state) | **correct**: orthonormal to 6.7e-15, addition theorem to 2.8e-16; carries the Condon-Shortley sign |
| 4 | Wigner small-d and "Wigner-D element" | same file `:226-312` | `static double wigner_small_d(size_t l, int m_prime, int m, double beta)`; `static double wigner_D_element(size_t l, int m_prime, int m, const double* R)` | libc | pure | double | safe | **wrong** for l >= 2 (16 of 25 elements at l=2), not orthogonal; rotation of harmonics fails even at l=1 |
| 5 | Single-spin-flip Metropolis annealer for Ising problems | `hardware/quantum_dwave_backend.c:450-591` | `static DWaveJobResult* simulated_annealing(const DWaveProblem*, const DWaveSamplingParams*)` | `rand_r`, `getpid`, `time`, libm, heap | stateful, seeded from wall clock | double | not safe (file-level result cache) | **does not compile** as shipped; after a one-line patch it finds the ground state, but its coupling layout disagrees with the public setter |
| 6 | State-vector single-qubit gate kernel | `hardware/quantum_simulator_cpu.c:107-151` | `static void apply_single_qubit_gate_cpu(double complex* state, const double complex* gate_matrix, size_t target_qubit, size_t n_qubits)` | C99 complex | pure | double complex | safe | **wrong** for every target qubit except qubit 0 (15 of 21 cases tested) |

Not executed, read only: regularized natural gradient
`bool compute_regularized_natural_gradient(const ComplexFloat* gradient, const ComplexFloat* metric, ComplexFloat* natural_gradient, size_t dimension, const natural_gradient_config_t* config)`
at `core/quantum_geometric_gradient.c:2887-2964`. It needs `lapack_svd` and `matrix_inverse`
(LAPACK), is single precision, and for the 2 to 6 parameters we would use it is replaced by a
small integer Cholesky solve. Idea only.

## What was run

All commands were run from `S:\tsotchke\qgtl` in Git Bash. Files copied from the mirror:
`src/differential_geometry.c`, `include/quantum_geometric/distributed/differential_geometry.h`,
`LICENSE.upstream`, and four verbatim line ranges cut with `sed -n A,Bp` into
`upstream_sim1q.inc`, `upstream_sa.inc`, `upstream_addq.inc`, `upstream_sh.inc`,
`upstream_wigner.inc`. `stub/cblas.h` is an empty header.

### Run 1: upstream QGT kernels against closed forms, plus the trimmed port (candidates 1 and 2)

```
gcc -std=gnu11 -O2 -Wall -Wextra -I include -I stub -I . src/differential_geometry.c an_qgt.c t_qgt.c -o t_qgt.exe -lm
./t_qgt.exe
```

Compiler output: warnings only (two unused variables at `differential_geometry.c:3022-3023`
and three unused static functions). Program output, complete:

```
T1/T2 Bloch + gauge: worst abs err so far 6.66e-16
T3 GHZ N=1..6: worst abs err so far 3.55e-15
T4 berry phase M=   8 arg=1.507836086 discrete-exact=1.507836086 continuum=1.570796327
T4 berry phase M= 100 arg=1.570408653 discrete-exact=1.570408653 continuum=1.570796327
T4 berry phase M=2000 arg=1.570795358 discrete-exact=1.570795358 continuum=1.570796327
T5 distance: worst abs err so far 3.55e-15
T6 port vs upstream (30 shapes): f64 worst 0, Q30 worst 5.5e-09
T7 Q30 bloch t=1: g_tt=268435456 (want 268435456) g_pp=190072011 (want 190072011) F_tp=-451761295 (want -451761295)
T7 fidelity Q30: <0|0>=1073741824 <0|+>=536870912 <0|1>=0 (1<<30=1073741824)
T8 timing dim=16 np=4 (noisy, parallel load): upstream g+F 780 ns, port f64 165 ns, port Q30 225 ns  [sink 2.14e+05 114826719682981]
RESULT: PASS, fails=0, worst closed-form abs err 7.85e-11
exit=0
```

What each test is:

- T1: two-level system, psi = (cos(t/2), e^{ip} sin(t/2)), t in {0.3, pi/3, pi/2, 2.0}, p = 0.7.
  Closed forms g = diag(1/4, sin^2(t)/4) and F_tp = -sin(t)/2 (this file's convention is
  F = -2 Im Q). All eight entries of g and F checked at each angle.
- T2: the same state multiplied by a parameter-dependent phase e^{i(0.3t + 0.9p)}. g and F
  are unchanged, so the kernels are gauge invariant as they must be.
- T3: N-qubit GHZ phase state for N = 1..6 (dimension 2 to 64), one parameter: g = N^2/4, F = 0.
- T4: Berry phase of a latitude loop at t = pi/3 sampled at M points. The result equals the
  exact discrete expression and tends to +pi(1 - cos t). Sign note: this is the opposite
  sign to the textbook gamma = i * loop integral of <psi|d psi>.
- T5: ray distance arccos|<p|q>|: (|0>,|+>) = pi/4, (|0>,|1>) = pi/2, (|0>,|0>) = 0.
- T6: the trimmed port against upstream on 30 pseudo-random shapes (dimension 4 to 64,
  1 to 6 parameters). The double port is bit-identical. The integer Q30 port differs by at
  most 5.5e-9, which is input quantization (inputs rounded to 2^-30).
- T7: the integer port on the two-level system reproduces the closed forms to the last
  integer digit shown; squared overlap in Q30 gives exactly 1, 1/2 and 0.
- T8: timing only, **noisy, parallel load**. The final "worst 7.85e-11" in the RESULT line is
  the Q30 check of T7 (tolerance 4e-9), not an upstream error; the upstream worst is 3.55e-15.

Re-verification of the prior audit on this candidate: confirmed (HEAD unchanged, single
translation unit compiles with an empty `cblas.h`, closed forms match, both Berry sign
conventions exist in the repo). One small correction: with `-Wall -Wextra` there are more
than "2 unused-variable warnings"; three unused static functions are also reported.

### Run 2: upstream real spherical harmonics and Wigner functions (candidates 3 and 4)

```
gcc -std=gnu11 -O2 -Wall -I . t_sh.c -o t_sh.exe -lm
./t_sh.exe
```

```
S1 orthonormality of 25 real SH (l<=4): max |<Ya|Yb> - delta| = 6.661e-15
S2 Y10=0.390882009522 want 0.390882009522 | Y20=0.290160240032 want 0.290160240032 | Y11=-0.175896904285 (CS phase: -sqrt(3/4pi)x=-0.175896904285, no-CS: +0.175896904285)
S3 addition theorem sum_m Y_lm^2 = (2l+1)/4pi, l=0..4: max err 2.776e-16
W1 l=1: 0 of 9 elements differ from textbook d (max err 0.000e+00 at m'=0 m=0); |d d^T - 1| max 2.776e-17
W1 l=2: 16 of 25 elements differ from textbook d (max err 1.705e+00 at m'=-1 m=-2); |d d^T - 1| max 1.010e+00
W1 l=3: 36 of 49 elements differ from textbook d (max err 2.119e+00 at m'=-2 m=-3); |d d^T - 1| max 2.286e+00
W1 l=4: 64 of 81 elements differ from textbook d (max err 2.397e+00 at m'=3 m=4); |d d^T - 1| max 5.279e+00
W2 l=1 equivariance |Y(Rr) - D Y(r)| max: 1.907e-01 (D), 4.355e-01 (D^T)   [|Y| ~ 0.3]
W2 l=2 equivariance |Y(Rr) - D Y(r)| max: 5.690e-01 (D), 4.289e-01 (D^T)   [|Y| ~ 0.3]
```

Reading: the harmonics are right (S1 integrates all 625 pairs with a 32 x 64 Gauss-Legendre
grid; S3 checks that the per-degree power is the same at every direction). The Wigner
small-d general formula is missing the square-root factorial prefactor, so it is only right
where l = 1 is hard-coded. `wigner_D_element` returns `d * cos(phase)`, which is not the
rotation matrix of real harmonics; it fails at l = 1 as well. So upstream offers correct
rotation-*invariant* features (per-degree power) but no working rotation-*equivariant* map.

### Run 3: upstream state-vector single-qubit kernel (candidate 6)

```
gcc -std=gnu11 -O2 -Wall -I . t_sim1q.c -o t_sim1q.exe -lm
./t_sim1q.exe
```

Tail:

```
nq=5 target=4  max|upstream-ref|=2.639e-01  upstream norm^2=1.105716  WRONG
nq=6 target=0  max|upstream-ref|=0.000e+00  upstream norm^2=1.000000  ok
nq=6 target=1  max|upstream-ref|=1.807e-01  upstream norm^2=0.972212  WRONG
nq=6 target=2  max|upstream-ref|=1.970e-01  upstream norm^2=1.041779  WRONG
nq=6 target=3  max|upstream-ref|=2.037e-01  upstream norm^2=0.959699  WRONG
nq=6 target=4  max|upstream-ref|=2.379e-01  upstream norm^2=0.983218  WRONG
nq=6 target=5  max|upstream-ref|=2.048e-01  upstream norm^2=0.998330  WRONG
RESULT: upstream single-qubit kernel wrong in 15 of 21 (nq,target) cases
```

A Hadamard on any qubit other than qubit 0 gives a wrong state that is not even normalized.
The prior audit predicted this by reading the code; it is now confirmed by execution. Built
without OpenMP (the pragmas are ignored), which is the single-thread behaviour. The 10-line
reference kernel `ref_1q` in `t_sim1q.c` is the correct form.

### Run 4: upstream Ising annealer (candidate 5)

```
gcc -std=gnu11 -O2 -Wall -I . t_sa.c -o t_sa.exe -lm
```

```
upstream_sa.inc:125:22: error: 'T_low' undeclared (first use in this function)
  125 |     double T_final = T_low;
```

`T_low` is declared inside the per-read loop (`quantum_dwave_backend.c:525`) and used after
it (`:574`). This is the branch compiled when libcurl and json-c are absent (`#else` at
`:442`), so that file cannot compile in the default configuration. With the one line patched
in a scratch copy (`upstream_sa_patched.inc`, `T_final = 0.01`) and a `rand_r` shim:

```
gcc -std=gnu11 -O2 -Wall -DUSE_PATCHED -I . t_sa.c -o t_sa.exe -lm
./t_sa.exe
```

```
A: chain N=8, 50 of 50 reads reached ground energy -7; min=-7.0 max=-7.0; tracked-vs-recomputed worst 0
B: add_quadratic_term(0,1)=1 stored at slot 1; add_quadratic_term(1,2)=0
B: spins (+1,+1,-1): intended energy with J01=+1 only is +1; upstream compute_ising_energy gives -1.0
```

A: the Metropolis loop itself works on an 8-spin ferromagnetic chain when couplings are laid
out in the annealer's own order. B: the public setter `add_quadratic_term` (`:732-741`)
writes slot `var1 * n + var2`, the annealer reads slots as a running upper-triangle pair
counter, so a coupling meant for spins (0,1) is applied to (0,2) and the coupling (1,2) is
rejected. Each flip also rescans all n(n-1)/2 pairs.

## Proposed integration

Engine names are placeholders; the code for the first two exists in `S:\tsotchke\qgtl\an_qgt.{h,c}`
and passed Run 1.

| Engine kernel | C signature | Maps to | Where it runs | Causal role and lesion test |
|---|---|---|---|---|
| QGT metric and curvature, integer | `void an_qgt_q30(const int32_t *psi, const int32_t *dpsi, uint32_t dim, uint32_t np, int64_t *g_out, int64_t *f_out);` | (e) | Tier 1, phased every 32 to 64 ticks, per creature that owns a small parametrized state module (dim 4 to 64, np <= 8) | Preconditions the plasticity step: delta = -eta * (g + lambda*I)^-1 * grad. Lesion: replace g by the identity and compare learning curves and fitness. |
| Ray overlap, integer | `int64_t an_fidelity_q30(const int32_t *p, const int32_t *q, uint32_t dim);` | (e), also usable for (c) readout | Tier 0 or Tier 1 | Novelty or kin gate between two internal states (1<<30 identical, 0 orthogonal); no libm. Lesion: constant gate. |
| Small symmetric solve for the step | `int an_spd_solve_q30(const int64_t *g, const int64_t *rhs, uint32_t np, int64_t lambda_q30, int64_t *x_out);` (to write: integer Cholesky, np <= 8) | (e) | Tier 1 | Needed by the row above; upstream's LAPACK routine is only the idea (Tikhonov term, default lambda 1e-4). |
| Loop phase (Bargmann product) | `void an_berry_phase_q30(const int32_t *states, uint32_t num_states, uint32_t dim, int32_t *re_out, int32_t *im_out);` (to write from `:2908-2945`) | (e) / (l) | Tooling only for now | It is an indicator of path dependence. It gets a tier only if some mechanism reads it and a lesion changes outcomes. |
| Real harmonics of a direction, degree <= 4 | `void an_rsh4_q15(int32_t x, int32_t y, int32_t z, int16_t out[25]);` (to write as fixed polynomials in x, y, z; no trig) | (d) | Tier 0, per perceived neighbour or field sample | Direction encoding for 3D perception; per-degree power sum_m Y_lm^2 is rotation invariant (checked, S3). Lesion: zero the l >= 1 channels. |
| Ising Metropolis sweep | none taken from upstream | (f), (h) | Tier 2 if built | Idea only: integer delta-E from a neighbour list, acceptance by Philox against a precomputed integer threshold table. |

Cost estimate (derived from the noisy T8 figure, not a benchmark): 225 ns per integer QGT at
dim 16, np 4 gives about 3.5 ms of single-core time per tick for one million creatures when
phased every 64 ticks. The 128-bit accumulation is the simple exact form; for AVX2 the
products can be shifted to Q30 before summing at the cost of dim * 2^-31 extra error.

What the port still needs from us, because upstream has nothing that works: the derivative
vectors d_j psi. For gates of the form exp(-i theta P / 2) these are exact and cheap
(apply the generator: d psi = -i/2 * P * psi at that gate, then finish the circuit). The
upstream circuit-level derivative code was not re-run here; the prior audit's static finding
that it fails stands unverified by this session.

License duty for `an_qgt.c`: it is a translation of MIT code, so the upstream copyright line
and permission notice must ship with it (`LICENSE.upstream` is in scratch).

## What is NOT usable, and why

- **The library as a whole.** Not built here. Prior audit's reasons spot-checked and
  consistent with what was seen (LAPACK and POSIX dependencies, plus the compile error found
  in Run 4). Treat it as a set of files, not a dependency.
- **State-vector simulator** (`hardware/quantum_simulator_cpu.c`): single-qubit kernel wrong
  for 15 of 21 cases (Run 3); the two-qubit kernel allocates a tensor network and calls
  `malloc` per amplitude group inside the loop. Category (c) must be written by us.
- **Wigner-D / rotation of harmonics** (`core/molecular_geometric_operations.c:226-312`):
  wrong for l >= 2 and not equivariant at l = 1 (Run 2). No Clebsch-Gordan code exists in the
  repo (**ran** grep for `clebsch`: 0 files). Category (d) beyond the harmonics themselves
  must be written by us.
- **Ising annealer**: does not compile as shipped, wall-clock seeded, O(n^2) per flip,
  layout mismatch with its own setter (Run 4). No Hopfield code exists (**ran** grep: 0 files).
- **Vendored RNG** (`core/quantum_rng.c`, read lines 1-140, not run): seeded from
  `gettimeofday`, `getpid`, `clock`, a stack address and `rdtsc`, mixes in more wall-clock
  time per step and routes values through `sin`/`cos` of doubles. Not deterministic, not
  counter based, libm dependent. It must not be called "quantum" randomness in our notes.
  Category (a): none.
- **Regularized natural gradient** as code: single precision, LAPACK. Idea kept (see table).
- **Core circuit QGT, connection, curvature, Chern number, Bures metric**: not re-run in this
  session. The prior audit reports them as failing or placeholder; that is carried here as
  "reported, not re-verified", not as a finding of this session.
- **Diffgeo Christoffel / Riemann / geodesic chain** (same file as candidate 1): compiled as
  part of the translation unit but not exercised here; finite differences with h = 1e-7 and
  heap allocation per call rule it out of a hot loop regardless of accuracy.
- **MPS / DMRG / tree tensor networks**: need LAPACK `cgesvd_`; not run. No use at 2 to 6 qubits.
- **`physics/quantum_diffusion.c`**: a small neural network (PINN) over a quantum state with
  Xavier init and a Gaussian sampler; it is not a heat or diffusion field solver. Read the
  function list only. Category (i): none.
- **`ai/` and `learning/` modules** (attention, transformer, data loaders): outside the laws
  of this project (pretrained-model style machinery); not examined beyond file names.
- Categories with nothing found: (b) fixed-point kernels (upstream is float throughout; the
  Q30 code is ours), (g) nothing better than a plain loop, (j) codecs or receipts, (k) rendering.

## Limits of this audit

- Only the six fragments above were executed. "Not usable" for unexecuted code is a reading
  judgement and is labelled as such.
- Closed-form checks cover a two-level system, GHZ states up to 6 qubits and pseudo-random
  port-equivalence inputs. No statistical test of the integer port beyond those 30 shapes.
- Timings are **noisy, parallel load** (other agents were running).
- No novelty is claimed for anything here; these are textbook formulas.
