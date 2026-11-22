# Tsotchke audit: `tensorcore`

Audit date 2026-11-17. Source `T:\mirrors\tensorcore` (read-only). Scratch `S:\tsotchke\tensorcore\`.
Everything marked **ran** was run in this session; commands and output tails are in section 4.
All timings: **noisy, parallel load** (other agents share the machine).

## 1. Repo facts

| Item | Value |
|---|---|
| HEAD | `553823c_2026-08-30` (`git -C T:/mirrors/tensorcore log -1 --format=%h_%cs`) |
| License | MIT License, "Copyright (c) 2026 tsotchke" (`LICENSE`) |
| Version stated in README | 0.1.23 |
| Size | 575 tracked files, 38 MB on disk including `.git` |
| Languages by file count | 195 `.py` (scripts, tooling), 69 `.c`, 51 `.cpp`, 46 `.h`, 16 `.metal`, 14 `.mm`, 12 `.esk`, 2 `.cu` |
| Library language | C++17 behind an `extern "C"` ABI (`include/tensorcore/*.h`, 37 headers); Metal shaders + Objective-C++ for the flagship backend |
| What it is | A tensor runtime for LLM training and inference: GEMM, flash attention, RMSnorm/RoPE/SwiGLU/AdamW, Q4_0/Q8_0 quantized GEMV, GGUF reader, distributed allreduce / DiLoCo. Metal is the tuned backend; the CPU backend is a portable reference. A second, smaller family of "math substrate" kernels (quantum gates, density matrix, Lie groups, Riemannian manifolds) lives in `lib/ops/*_cpu.cpp`. |

Built here: **partly**. Four translation units were copied and compiled standalone with g++ 16.1.0
(`quantum_gates_cpu.cpp`, `lie_groups_cpu.cpp`, `holonomic_cpu.cpp`, `gemm_cpu_avx2.cpp`). The full
library (CMake, `tc_context`, buffers) was not built. Nothing in `lib/cuda` was built: there is no
nvcc/ptxas on this machine, `lib/cuda/gemm.cpp` needs cuBLAS, `training.cu` needs `cuda_runtime.h`,
`cuda_fp16.h`, `mma.h`. Metal, AMX (Apple), HIP: not applicable to this machine.

## 2. Candidate kernels

| # | Kernel | File and lines | Signatures | Dependencies | Pure / stateful | Number type | Thread safety |
|---|---|---|---|---|---|---|---|
| 1 | State-vector gate apply (2 to 6 qubits fits in 128 B to 512 B) | `lib/ops/quantum_gates_cpu.cpp` 54-161 (1q matrices), 163-301 (2q matrices), 303-331 (zero, 1q apply), 333-380 (2q apply), 406-424 (dispatch), 426-449 (prob, norm), 489-503 (inner), 516-610 (Pauli-string Trotter), 638-688 (3q apply). Header `include/tensorcore/quantum_gates.h` | `void tc_qstate_zero(float* state, int n_qubits)`; `void tc_qstate_apply_1q_unitary(float* state, int n_qubits, int qubit, const float* U)`; `void tc_qstate_apply_2q_unitary(float* state, int n_qubits, int q_ctrl, int q_targ, const float* U)`; `void tc_qstate_apply_gate(float* state, int n_qubits, tc_gate_type_t type, const int* qubits, const float* params)`; `float tc_qstate_prob_one(const float*, int, int)`; `float tc_qstate_norm_sq(const float*, int)`; `void tc_qstate_trotter_step(float*, int, const tc_pauli_term_t*, int, float t, int n_trotter_steps)` | libm (`sinf`/`cosf`), `<cstring>`; no `tc_context`, no heap in the apply path | Pure functions on caller memory, in place | fp32 interleaved complex | Reentrant (no globals); safe if each thread owns its state |
| 2 | Quantum geometric tensor (Fubini-Study metric + Berry curvature) | `lib/ops/quantum_gates_cpu.cpp` 612-636 | `void tc_quantum_geometric_tensor(const float* psi, const float* const* dpsi, int n_qubits, int n_params, float* out_G)` | `tc_qstate_inner` (double accumulator); two `std::vector<float>(n_params)` heap allocations per call | Pure, but allocates | fp32 in and out, double inside | Reentrant |
| 3 | SU(2)/SO(3) closed-form exp, log, product, bridge; SU(2) loop composer | `lib/ops/lie_groups_cpu.cpp` 30-55, 57-81, 83-99, 101-134, 136-161, 163-173, 175-197; `lib/ops/holonomic_cpu.cpp` 23-46, 48-73 | `void tc_su2_exp(float a, float b, float c, float* U)`; `void tc_su2_log(const float* U, float*, float*, float*)`; `void tc_su2_mul(const float*, const float*, float*)`; `void tc_so3_exp(float wx, float wy, float wz, float* R)`; `void tc_so3_log(const float* R, float*, float*, float*)`; `void tc_so3_mul(...)`; `void tc_su2_to_so3(const float* U, float* R)`; `int tc_holonomic_compose_su2(const float* generators, int32_t n_segments, float* out_U)` | libm only | Pure | fp32 | Reentrant |
| 4 | AVX2+FMA fp32 GEMM, BLIS-style 6x16 micro-kernel | `lib/ops/gemm_cpu_avx2.cpp` 85-123 (packing), 125-172 (micro-kernel), 229-341 (macro-kernel), 348-359 (entry) | `int tc_avx2_gemm_f32(int M, int N, int K, float alpha, const float* A, int lda, const float* B, int ldb, float beta, float* C, int ldc)` | `<immintrin.h>`, `std::mutex`, `_aligned_malloc`; optional OpenMP; includes `tensorcore/tensorcore.h` but uses nothing from it | **Stateful**: function-static pack buffers grown with the heap, guarded by a function-static mutex | fp32 | Serialised by the internal mutex (one caller at a time); optional OpenMP fan-out inside |
| 5 | int8 GEMM reference and Q4_0/Q8_0 quantized GEMV | `lib/ops/gemm_cpu.cpp` 503-520 (int8 -> int32 triple loop); `lib/ops/quantized_cpu.cpp` 102-162 (quantize), 164-230 (GEMV) | `tc_status_t tc_gemm(tc_context*, const tc_gemm_desc*, ...)` with `TC_DTYPE_I8` in, `TC_DTYPE_I32` out; `tc_status_t tc_gemv_quantized(tc_context*, const tc_buffer* X, const tc_buffer* W_quant, tc_buffer* Y, tc_quant_t fmt, int M, int N, int K)` | `tc_context`, `tc_buffer`, fp16 conversion helpers | Needs the runtime context | int8 weights, but **float** alpha/beta and fp16 activations with an fp32 accumulator | Per-context |
| ref | CUDA kernels (reference for a PTX path) | `lib/cuda/training.cu` 68-381 (rmsnorm, adamw, swiglu, layernorm, rope, softmax), 748-1460 (flash attention, scalar and `wmma` variants); `lib/cuda/riemannian_adam.cu` 142-398; `lib/cuda/gemm.cpp` (cuBLAS wrappers only) | `__global__` kernels on `__half` data | nvcc, `cuda_fp16.h`, `mma.h`, cuBLAS | n/a | fp16 / fp32 | n/a |

Mapping to the engine's wish list: #1 -> (c); #2 -> (e); #3 -> (d), partially; #4 and #5 -> (g);
CUDA -> (g) reference. Nothing in this repo maps to (a) RNG/hash mixers, (b) fixed-point math,
(f) Ising/Hopfield, (h) Monte Carlo, (i) PDE solvers, (k) rendering. For (j) there is a top-k sparse
codec (`lib/distributed/sparse_compress.cpp`) and an HMAC-SHA-256 (`lib/distributed/transport_auth.cpp`
line 172; a `Sha256` class in `lib/distributed/diloco.cpp` line 271); both were read, not run.

## 3. What the tests showed

1. **State-vector kernel (#1): works as written.** Bell state exact to 1e-6. Run-to-run bit identical
   for the same binary. Norm drift after 100 000 random fp32 gates: 1.2e-5 (4 qubits), 1.4e-5 (6 qubits).
   Repeating one RY gate 2 million times drifted the norm to 1.051, so a long-lived state needs
   renormalisation. Rough cost at 4 qubits: about 15 ns per 1-qubit gate, about 68 ns per generic
   2-qubit gate (the 2-qubit path multiplies a full 4x4 complex matrix even for CNOT, which is a pure
   permutation).
2. **QGT (#2): matches the closed form.** For a Bloch state at theta = 1.1, phi = 0.7 the kernel returns
   G_theta,theta = 0.250000, G_phi,phi = 0.198563 (= sin^2(theta)/4), Im G_theta,phi = 0.222802
   (= sin(theta)/4). The caller must supply the derivative states; the kernel is only the overlaps.
3. **Lie groups (#3): exp/log are fine, the bridge has a convention defect.**
   - `tc_so3_exp` output is orthogonal to 5e-7; `log(exp(w))` round trip 7.7e-6 for |w| < 2.95.
   - `tc_so3_log` breaks near pi: input angle 3.14 returns 3.238, input 3.1415 returns -3329.7.
   - `tc_su2_to_so3` is an **anti-homomorphism**: R(UV) = R(V)R(U) to 6.6e-7, while R(UV) vs R(U)R(V)
     differs by up to 2.0. `tc_su2_mul` itself is a correct matrix product (1.3e-7). The header calls the
     bridge the adjoint representation, which would be a homomorphism. Anyone composing rotations through
     this bridge gets the wrong order unless they transpose R.
4. **AVX2 GEMM (#4): correct, but not a win at brain sizes.** Matches a double-precision reference to
   1e-5 or better on seven shapes including edge tiles and alpha/beta. Against a plain loop compiled
   with the same flags (`-O3 -mavx2 -mfma`): slower at 8x8 (116 ns vs 78 ns) and at 16x16 (348 ns vs
   299 ns), faster at 32x32 (1770 vs 2894 ns) and 64x64 (11.4 vs 17.5 us). The 256x256 timing flipped
   between two runs, so it is not reported as a result.
5. **int8 path (#5): not exact.** The CPU int8 GEMM converts the int64 sum to `float` before the
   int32 store (`gemm_cpu.cpp` line 519). Using the same arithmetic, a dot product of 2049 pairs of
   127 gives 33 048 320 where the exact value is 33 048 321. It is exact only while |sum| < 2^24.
   The README claim "bit-exact i32 results in the tested matrix suite" is about the Metal/MPS path
   and its own test sizes; I did not run that suite.

## 4. Build and run commands with output tails (ran, 2026-11-17)

Copy (from Git Bash):

```
mkdir -p /s/tsotchke/tensorcore/src/tensorcore /s/tsotchke/tensorcore/tests
cd T:/mirrors/tensorcore
cp -r include/tensorcore/. /s/tsotchke/tensorcore/src/tensorcore/
cp lib/ops/quantum_gates_cpu.cpp lib/ops/lie_groups_cpu.cpp lib/ops/holonomic_cpu.cpp lib/ops/gemm_cpu_avx2.cpp /s/tsotchke/tensorcore/src/
```

Tests written by this agent: `S:\tsotchke\tensorcore\tests\test_qsv.c`, `test_lie.c`, `test_gemm.c`.

Build (in `/s/tsotchke/tensorcore`):

```
g++ -std=c++17 -O2 -Wall -Wextra -Isrc -c src/quantum_gates_cpu.cpp -o qg.o
g++ -std=c++17 -O2 -Wall -Wextra -Isrc -c src/lie_groups_cpu.cpp -o lie.o
g++ -std=c++17 -O2 -Wall -Wextra -Isrc -c src/holonomic_cpu.cpp -o hol.o
g++ -std=c++17 -O2 -mavx2 -mfma -Wall -Wextra -Isrc -c src/gemm_cpu_avx2.cpp -o gemm.o
gcc -std=c2x -O2 -Wall -Wextra -Isrc -c tests/test_qsv.c -o t_qsv.o
gcc -std=c2x -O2 -Wall -Wextra -Isrc -c tests/test_lie.c -o t_lie.o
gcc -std=c2x -O2 -Wall -Wextra -Isrc -c tests/test_gemm.c -o t_gemm.o
g++ -o test_qsv.exe t_qsv.o qg.o -static
g++ -o test_lie.exe t_lie.o lie.o hol.o -static
g++ -o test_gemm.exe t_gemm.o gemm.o -static
gcc -std=c2x -O3 -mavx2 -mfma -Wall -Wextra -Isrc -c tests/test_gemm.c -o t_gemm_avx2.o
g++ -o test_gemm_fair.exe t_gemm_avx2.o gemm.o -static
```

The four repo sources compiled with no warnings under `-Wall -Wextra`.

`./test_qsv.exe` (exit 0):

```
A bell amps: |00>=0.707107 |01>=0.000000 |10>=0.000000 |11>=0.707107  P(q0=1)=0.500000 P(q1=1)=0.500000
B n=4 gates=  1000 norm_sq=1.0000007 drift=7.15e-07 same_bits=1 hash=6c369fb527a42049
B n=4 gates= 10000 norm_sq=1.0000015 drift=1.55e-06 same_bits=1 hash=58175245c3418721
B n=4 gates=100000 norm_sq=1.0000124 drift=1.24e-05 same_bits=1 hash=c50c47b5342568b6
B n=6 gates=  1000 norm_sq=1.0000010 drift=9.54e-07 same_bits=1 hash=dbde37a4f566728f
B n=6 gates= 10000 norm_sq=0.9999996 drift=3.58e-07 same_bits=1 hash=3614d6c7bee422b0
B n=6 gates=100000 norm_sq=1.0000138 drift=1.38e-05 same_bits=1 hash=f0ed46aa8694bc71
C QGT G_thth=0.250000+0.000000i (exp 0.25)  G_phph=0.198563+0.000000i (exp 0.198563)
C QGT G_thph=0.000000+0.222802i (exp 0+0.222802i)  G_phth=0.000000-0.222802i
D trotter X a=0.3: (0.955336+0.000000i, 0.000000-0.295520i)  expect (0.955337, -0.295520i)
E n=2: 1q 4.7 ns/gate, 2q 18.4 ns/gate (noisy, parallel load) norm_sq=1.051418
E n=4: 1q 14.7 ns/gate, 2q 68.3 ns/gate (noisy, parallel load) norm_sq=1.051228
E n=6: 1q 54.0 ns/gate, 2q 273.6 ns/gate (noisy, parallel load) norm_sq=1.051408
RESULT: PASS (0 failures)
```

`./test_lie.exe` (exit 0):

```
su2_mul vs hand matrix product (entry 00): max err = 1.341e-07
R(UV) vs R(U)R(V): max err = 1.996e+00 ; R(UV) vs R(V)R(U): max err = 6.557e-07
so3: max |R^T R - I| = 5.364e-07, max |det-1| = 5.960e-07, max |log(exp w) - w| = 7.749e-06
su2: max |log(exp v) - v| = 1.788e-07, homomorphism max err = 1.996e+00
double cover: max |R(exp v) - so3_exp(+2v)| = 5.960e-07, vs so3_exp(-2v) = 2.000e+00
near-pi: exp(0,0,3.0000) -> log = (0.00000, 0.00000, 3.00002)
near-pi: exp(0,0,3.1000) -> log = (0.00000, 0.00000, 3.09998)
near-pi: exp(0,0,3.1400) -> log = (0.00000, 0.00000, 3.23847)
near-pi: exp(0,0,3.1415) -> log = (-0.00000, -0.00000, -3329.70239)
holonomy of square loop a=0.3: U00=0.984746+0.159411i  U01=-0.049311+0.049311i  angle=0.174886 (2a^2=0.180000)
RESULT: PASS (exp/log/orthogonality/cover); bridge is an ANTI-homomorphism
```

`./test_gemm.exe` (test file built with plain `-O2`, exit 0):

```
tc_avx2_gemm_f32_available = 1
gemm   6x 16x  8 alpha=1.0 beta=0.0 rc=0/0 max_abs_err=2.133e-07 repeat_bits_same=1
gemm   7x  5x  3 alpha=1.0 beta=0.0 rc=0/0 max_abs_err=4.191e-08 repeat_bits_same=1
gemm  16x 16x 16 alpha=1.0 beta=0.0 rc=0/0 max_abs_err=4.480e-07 repeat_bits_same=1
gemm  37x 53x 29 alpha=1.0 beta=0.0 rc=0/0 max_abs_err=1.392e-06 repeat_bits_same=1
gemm  37x 53x 29 alpha=0.5 beta=2.0 rc=0/0 max_abs_err=6.962e-07 repeat_bits_same=1
gemm  64x 64x 64 alpha=1.0 beta=1.0 rc=0/0 max_abs_err=2.262e-06 repeat_bits_same=1
gemm 128x 96x300 alpha=1.0 beta=0.0 rc=0/0 max_abs_err=1.141e-05 repeat_bits_same=1
bench n=  8: tc_avx2 119.8 ns/call (8.54 GFLOPS), naive -O2 171.5 ns/call (5.97 GFLOPS)  [noisy, parallel load]
bench n= 16: tc_avx2 341.4 ns/call (24.00 GFLOPS), naive -O2 1183.9 ns/call (6.92 GFLOPS)  [noisy, parallel load]
bench n= 32: tc_avx2 1648.8 ns/call (39.75 GFLOPS), naive -O2 7928.1 ns/call (8.27 GFLOPS)  [noisy, parallel load]
bench n= 64: tc_avx2 9520.5 ns/call (55.07 GFLOPS), naive -O2 72365.4 ns/call (7.25 GFLOPS)  [noisy, parallel load]
bench n=256: tc_avx2 526133.5 ns/call (63.78 GFLOPS), naive -O2 3623955.0 ns/call (9.26 GFLOPS)  [noisy, parallel load]
int8 dot K=1024 all-127: exact=16516096 repo_formula=16516096 diff=0
int8 dot K=2049 all-127: exact=33048321 repo_formula=33048320 diff=-1
RESULT: PASS (0 failures)
```

`./test_gemm_fair.exe | grep -E "bench|RESULT"` (test file built with `-O3 -mavx2 -mfma`; the
printed label still says "naive -O2", the naive loop here is really `-O3 -mavx2 -mfma`):

```
bench n=  8: tc_avx2 115.8 ns/call (8.85 GFLOPS), naive -O2 78.4 ns/call (13.05 GFLOPS)  [noisy, parallel load]
bench n= 16: tc_avx2 347.8 ns/call (23.55 GFLOPS), naive -O2 298.5 ns/call (27.44 GFLOPS)  [noisy, parallel load]
bench n= 32: tc_avx2 1769.9 ns/call (37.03 GFLOPS), naive -O2 2893.5 ns/call (22.65 GFLOPS)  [noisy, parallel load]
bench n= 64: tc_avx2 11432.4 ns/call (45.86 GFLOPS), naive -O2 17528.6 ns/call (29.91 GFLOPS)  [noisy, parallel load]
bench n=256: tc_avx2 1158430.5 ns/call (28.97 GFLOPS), naive -O2 929153.0 ns/call (36.11 GFLOPS)  [noisy, parallel load]
RESULT: PASS (0 failures)
```

## 5. Proposed integration

None of this can enter a hot loop as is: every kernel here is fp32 or fp16, and the engine's state
must be integer or fixed point (law 6). The proposals below are ports, sized from the measurements.

**P1. Per-creature small state-vector module, classical simulation (c). Tier 1, phased every 32 to 64 ticks.**
Port the index sweep of `tc_qstate_apply_1q_unitary` (lines 309-331) to Q1.15 amplitudes; drop the
generic 4x4 path for CNOT/CZ/SWAP and implement them as index permutations and sign flips, which are
exact in integers. 4 qubits = 16 complex Q1.15 = 64 bytes, one cache line per creature.

```c
/* st: 2 << n int16 values, interleaved re/im, Q1.15. U: 8 int16, Q1.15, from a genome-indexed table. */
void an_qsv_apply_1q_q15(int16_t *restrict st, uint32_t n_qubits, uint32_t qubit, const int16_t U[8]);
void an_qsv_cnot(int16_t *restrict st, uint32_t n_qubits, uint32_t ctrl, uint32_t targ);   /* exact */
void an_qsv_renorm_q15(int16_t *restrict st, uint32_t n_qubits);    /* integer rescale, fixed rounding */
/* Born-rule readout: integer cumulative |amp|^2 compared against one Philox draw. */
uint32_t an_qsv_sample(const int16_t *st, uint32_t n_qubits, uint32_t philox_u32);
```

Causal role: sensor values set rotation angles (table lookup), the sampled bit string selects an action
or gates a memory write. Lesion test: replace `an_qsv_sample` output with the Philox draw alone and
compare behaviour statistics. Wording rule: this is a classical simulation driven by the counter RNG;
no quantum randomness, no speed-up. Budget from the fp32 measurement: about 15 ns per 1-qubit gate at
4 qubits, so 8 gates for 1/32 of 1 M creatures is about 4 ms of one core per tick. The Q1.15 port's
rounding makes gates slightly non-unitary, so `an_qsv_renorm_q15` is required (the fp32 version already
drifts 5 % after 2 M gates). Whether such a module beats an integer recurrent cell of the same size
is unknown and has to be measured in the engine.

**P2. Fubini-Study metric as a plasticity preconditioner (e). Tier 2 (colony level) or tooling.**
Keep the formula of lines 612-636, replace `std::vector` with caller scratch, compute derivative states
with a parameter shift over the P1 circuit.

```c
/* G: n_params * n_params complex pairs. scratch: 2 * n_params floats. No heap. */
void an_qgt(const float *psi, const float *const *dpsi, uint32_t n_qubits, uint32_t n_params,
            float *restrict G, float *restrict scratch);
```

Use: natural-gradient step on a handful (<= 8) of circuit parameters, delta = -eta * inverse(Re G) * grad.
Cost per call is n_params^2 inner products over 2^n amplitudes, fine at colony rate, too heavy per
creature per tick. It is float; either run it as an offline indicator, or port to Q-format with a fixed
operation order before it may write simulation state.

**P3. Rotation kernels (d, partial). World/body update or tooling.**
`tc_so3_exp` (Rodrigues, lines 101-134) is a usable reference for integrating angular velocity into a
body frame. The engine version should be a Q1.15 quaternion update with renormalisation instead.
Do not take `tc_su2_to_so3` or `tc_so3_log` without the fixes noted in section 3.

```c
void an_quat_integrate_q15(int16_t q[4], const int16_t omega_q15[3], uint32_t dt_shift);
```

**P4. Small integer GEMV/GEMM (g). Tier 0.**
Take the structure of the 6x16 micro-kernel (12 accumulators, broadcast A, two loads of B, lines
125-172) and re-express it with AVX-VNNI `vpdpbusd` on int8 weights and uint8 activations with int32
accumulators, no packing step and no heap, because at 8x8 to 16x16 the packing overhead already loses
to a plain loop (measured). This is a rewrite guided by the layout, not a copy.

```c
/* y[r] = sum_c W[r*cols + c] * x[c]; exact int32; cols a multiple of 32; no allocation. */
void an_gemv_i8u8_vnni(const int8_t *restrict W, const uint8_t *restrict x,
                       int32_t *restrict y, uint32_t rows, uint32_t cols);
```

`tc_avx2_gemm_f32` itself can be used unchanged in **tooling only** (offline analysis such as
covariance or PCA over genomes at sizes >= 32), compiled with `-mavx2 -mfma`.

**P5. PTX path (reference only).** `training.cu` shows two reusable patterns: a per-row block reduce
(`block_reduce_sum_f32`, warp reduce then shared memory, lines 41-66) and 16x16x16 `wmma` fragment tiling for fp16 (lines
940-1230). There is no int8 CUDA kernel source in the repo; int8 goes through `cublasGemmEx`
(`lib/cuda/gemm.cpp` 365-440). Nothing was compiled; whether clang's nvptx64 target can build an
integer kernel for this GPU without the CUDA headers is unknown from this audit.

## 6. NOT usable, and why

- **The whole LLM surface: excluded by law 3.** GGUF reader (`lib/io/gguf.c`), Q4_0/Q8_0 block format
  (ggml weight layout), flash attention, RoPE, SwiGLU, AdamW, llama config helpers, DiLoCo, PyTorch
  bindings. Reusable numeric idea only: per-block scale for int8 weights (use a power-of-two shift so
  it stays integer).
- **Network code: excluded by law.** `lib/distributed/*` (Gloo TCP, mesh, remote tensors, transport
  auth). The HMAC-SHA-256 inside it is tied to C++ `Keyring`/`Io` types; a receipt hash for evidence is
  better written fresh in C than extracted. Not run here.
- **Python, Rust, Swift, WASM, Eshkol bindings and 195 Python scripts:** tooling for the LLM runtime,
  interpreted or foreign-language, no simulation value.
- **Metal shaders, Apple AMX (`gemm_cpu_amx.cpp`), NEON, HIP:** wrong hardware.
- **CUDA code as code:** needs nvcc, cuBLAS, cuSPARSELt; none present. GEMM is a cuBLAS call, so
  there is no kernel to port.
- **`tc_avx2_gemm_f32` inside the simulation:** fp32 state, heap allocation on first call,
  function-static buffers behind a mutex (all callers serialise), and slower than a plain loop at
  16x16 and below.
- **CPU int8 GEMM:** a scalar reference loop with float alpha/beta; inexact above 2^24 (measured).
- **`tc_gemv_quantized`:** int8/4-bit weights but fp16 activations and an fp32 accumulator; not an
  integer kernel.
- **Density matrix + Lindblad (`density_matrix_cpu.cpp`):** 4^n floats and heap temporaries per call;
  too heavy per creature, and open-system noise has no causal job that P1 plus the counter RNG does not
  already cover. Not run.
- **Riemannian manifolds, geodesics, Christoffel by finite differences, Riemannian Adam
  (`poincare`, `lorentz`, `sphere`, `torus`, `product_manifold`, `metric`, `geodesic`,
  `riemannian_adam`):** fp32, `std::vector` per call, built for hyperbolic embedding training. Not run.
  No clear causal role in the simulation.
- **2:4 sparse GEMM:** the CPU version is a dense fallback; the real path is cuSPARSELt.
- **Absent altogether (searched `lib`, `include`, `kernels` by keyword):** counter-based RNG or hash
  mixers, fixed-point math, real spherical harmonics, Wigner-D, Clebsch-Gordan, Ising/Hopfield
  networks, Metropolis or path-integral samplers, PDE or stencil solvers.

## 7. Verdict

**reference_only**, with two small exceptions worth a port (adapt): the state-vector index sweep (P1)
and the QGT formula (P2). The repo's centre of mass is an LLM tensor runtime, which the owner's laws
exclude; its CPU kernels are float references rather than tuned integer code; and the pieces that
compile and pass here are short enough that a fixed-point rewrite is less work than adapting them.

Defects found in the source while testing (useful if the Tsotchke code is ever taken as is):
`tc_su2_to_so3` reverses composition order; `tc_so3_log` is correct at angle 3.10 but wrong at 3.14
and beyond (the exact threshold between was not located); the CPU int8 GEMM rounds through `float`.
`tc_qstate_inner` accumulates in double (lines 489-503), so the QGT result depends on double
arithmetic as well as fp32.
