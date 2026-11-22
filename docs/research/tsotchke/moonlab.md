# Tsotchke audit: moonlab

Audit date 2026-11-17. Everything marked "ran" was run in this session; scratch is `S:\tsotchke\moonlab\`.
All timings are **noisy, parallel load** (other agents share the machine).

## 1. Repo facts

| Item | Value |
|---|---|
| Path | `T:\mirrors\moonlab` (read-only, not modified) |
| HEAD | `cd3b234_2026-09-11` (`git -C T:/mirrors/moonlab log -1 --format=%h_%cs`) |
| Version | 1.2.1 (`VERSION.txt`) |
| License | MIT License, "Copyright (c) 2024-2026 tsotchke" (`LICENSE`) |
| Main language | C (C11, `double _Complex`); 394 `.c` + 120 `.h` of 1,421 tracked files; also Python (135), TypeScript (99), Rust (72) bindings |
| Size | 178 MB on disk (includes build folders and prebuilt WASM) |
| What it is | `libquantumsim`: dense state-vector simulator (up to 32 qubits), tensor networks, Clifford tableau, QEC, VQE/QAOA, crypto, entropy stack |

Verdict: **adapt**. The dense kernel is correct and compiles trimmed, but as written it breaks laws 2 and 6
(heap per state, global config, libm, FMA-sensitive doubles). A 208-line fixed-point port that follows its
conventions was written and verified against it here.

## 2. Prior audit check (`W:\Glossodrom Language Lab\docs\research\moonlab.md`)

| Prior claim | Status now |
|---|---|
| HEAD cd3b234 (2026-09-11), v1.2.1, MIT | confirmed (ran git log, read files) |
| Little-endian: qubit q is bit q of the index | confirmed (ran: `X(q2)|000> -> P[4] = 1.0`) |
| RX/RY/RZ/H/CNOT/CZ conventions | confirmed: an independently written port using those matrices agrees with upstream to 9e-8 over 65,536 gates |
| Dense measure rule: outcome 1 iff r < P(1) | confirmed by reading `measurement.c:181-198`; not exercised |
| CMake compiles `simd_ops.c` with `-msse3` | confirmed (`CMakeLists.txt:1536`); without it the file does not compile (see 4.1) |
| WASM, npm, JS-binding, Node claims | not re-verified and not relevant: a WASM or JS engine is a VM, excluded by law 1 |

New facts the prior audit did not have: (a) a static trimmed link with MinGW GCC 16.1 fails on two
`__attribute__((weak))` definitions and needs a 15-line shim; (b) the double kernel's state hash changes
with `-march=native` because of FMA contraction; (c) every upstream state carries two heap blocks, one of
them an 8,192-byte measurement history.

## 3. Candidate kernels

| # | Candidate | File and lines | Signatures | Dependencies | Pure / stateful | Float / integer | Thread safety | Engine use |
|---|---|---|---|---|---|---|---|---|
| 1 | Dense 1q/2q gate kernel (upstream) | `src/quantum/gates.c` X 123-154, Y 156-192, Z 194-224, H 226-272, RX 385-427, RY 429-479, RZ 481-518, CNOT 571-639, CZ 641-668; `state.c` init 39-81; `measurement.c` probs 140-179, P(1) 73, measure 181, `<Z>` 428, `<ZZ>` 509 | `qs_error_t quantum_state_init(quantum_state_t*, size_t)`; `qs_error_t gate_hadamard/pauli_x/pauli_y/pauli_z(quantum_state_t*, int)`; `gate_rx/ry/rz(quantum_state_t*, int, double)`; `gate_cnot/cz(quantum_state_t*, int, int)`; `void measurement_probability_distribution(const quantum_state_t*, double*)` | 8 `.c` files, 5,791 lines: state, gates, measurement, gpu_route_cpu_unavailable, simd_ops, matrix_math, config, moonlab_status; libm (`sin`, `cos`, `cexp`), pthread (`pthread_once`), calloc | stateful: heap amplitudes + 8 KB history per state, global config behind a mutex, one-time SIMD dispatch table | double complex | per-state safe after first call; first call initialises globals | (c), tooling oracle only |
| 2 | Fixed-point 2..6 qubit kernel (port written here, not upstream code) | `S:\tsotchke\moonlab\qsv_impl.h` 1-208 | `qsv_init`, `qsv_x/y/z(s, n, q)`, `qsv_h(s, n, q, inv_sqrt2)`, `qsv_rx/ry/rz(s, n, q, cs_table, k)`, `qsv_cnot/cz(s, n, a, b)`, `uint64_t qsv_probs(s, n, p)`, `qsv_renorm`, `int qsv_sample(s, n, u32)`, `qsv_collapse(s, n, q, bit)` | none (stdint only); cos/sin table built by integer-only Q60 Taylor series | pure functions on a caller-owned 256 B (Q1.14) or 512 B (Q1.30) block; no heap, no globals | integer (int16 or int32) | fully reentrant | (c) and (b), Tier 1 |
| 3 | Clifford stabilizer tableau | `src/backends/clifford/clifford.c` 1-690 (gates 132-260, measure ~290-420, splitmix64 584) | `clifford_tableau_t* clifford_tableau_create(size_t)`; `clifford_h/s/x/y/z(t, q)`; `clifford_cnot/cz/swap(t, a, b)`; `clifford_measure(t, q, uint64_t* rng, int* outcome, int* kind)`; `clifford_sample_all(t, uint64_t* rng, uint64_t* result)` | none beyond libc; 4 callocs per tableau, more inside measure | stateful heap object; RNG state is caller-owned | integer, bit-packed, exact | reentrant per tableau | (c)/(b) idea: exact integer module for Clifford-only programs |
| 4 | SHA3-256 / SHAKE | `src/crypto/sha3/sha3.c` 1-235 | `void sha3_256(const uint8_t* in, size_t inlen, uint8_t out[32])`; streaming `sha3_256_init/sha3_update/sha3_final` | none | pure | integer | reentrant | (j) evidence receipts, tooling |
| 5 | Fubini-Study metric / quantum natural gradient | `src/algorithms/vqe_qng.c` 166-193 (`vqe_compute_qgt`), 195 (`vqe_compute_berry_curvature`), 270 (`vqe_natural_gradient_direction`) | `int vqe_compute_qgt(vqe_solver_t*, const double* params, double* qgt_out)` | whole VQE solver (`vqe.c`, 2,669 lines), malloc, central differences | stateful (solver object) | double | not assessed | (e) idea only |

Not selected as candidates but looked at: `algorithms/quantum_annealing.c` (521 lines, closed-system
transverse-field annealing on a double state vector, then exhaustive enumeration; idea for (f) only),
`integration/libirrep_bridge.c` (returns NOT_BUILT without an external library; nothing for (d)),
`utils/matrix_math.c` (small Hermitian eigensolver in double; (g) at best, no need).

## 4. Builds and runs (ran now)

Copied files live under `S:\tsotchke\moonlab\up\src\...` with their upstream relative paths, plus `LICENSE`.
All commands were run from `S:\tsotchke\moonlab` in Git Bash. `UP` stands for:

```
UP="up/src/quantum/state.c up/src/quantum/gates.c up/src/quantum/measurement.c
    up/src/quantum/gpu_route_cpu_unavailable.c up/src/optimization/simd_ops.c
    up/src/utils/matrix_math.c up/src/utils/config.c up/src/utils/moonlab_status.c shim_sync.c"
```

### 4.1 Does the upstream kernel compile trimmed? Yes, with two fixes

1. `gcc -std=gnu11 -O2 -c optimization/simd_ops.c` fails:
   `simd_ops.c:479:23: error: implicit declaration of function '_mm_addsub_pd'`. Fix: add `-msse3` (upstream CMake does this per file).
2. Linking the 8 files fails with `undefined reference to 'quantum_state_sync_to_host'` and `..._from_host`
   (defined `__attribute__((weak))` in `state.c:671-687`; MinGW PE/COFF does not resolve them). Fix:
   `shim_sync.c`, 15 lines with strong CPU-only definitions.

Headers needed besides the 8 `.c` files: `state.h gates.h measurement.h matrix_math.h secure_memory.h validation.h
config.h moonlab_weak.h quantum_entropy.h constants.h moonlab_status.h simd_ops.h accelerate_ops.h moonlab_api.h`.

### 4.2 Candidate 1: upstream kernel test

```
gcc -std=gnu11 -O2 -msse3 -o test_up_A.exe test_up.c $UP -lm
./test_up_A.exe
```

Output tail (build A):

```
bell probs: 0.50000000000000011 0 0 0.50000000000000011
X(q2)|000> -> P[4] = 1.0
RY(pi/2): P1(q0) = 0.50000000000000011
n=2 depth=256 amp_hash=2735ee3392e3fea2  sumP-1=+4.441e-15  P[0]=0.77138188418118858
n=3 depth=256 amp_hash=499238e34a8f286b  sumP-1=+3.997e-15  P[0]=0.043769792048390307
n=4 depth=256 amp_hash=817b3118552c8ff1  sumP-1=+3.331e-15  P[0]=0.12389663434039344
n=5 depth=256 amp_hash=9a259763a7736142  sumP-1=+1.554e-15  P[0]=0.020033633442428169
n=6 depth=256 amp_hash=a72470c725135216  sumP-1=+2.887e-15  P[0]=0.037913650589523076
n=3 ns/gate: H=4.8 X=6.9 Y=20.2 Z=6.7 RX=14.5 RY=9.9 RZ=63.4 CNOT=7.6 CZ=4.3
n=4 ns/gate: H=7.0 X=12.5 Y=32.3 Z=12.2 RX=18.0 RY=10.4 RZ=91.0 CNOT=9.7 CZ=7.1
n=5 ns/gate: H=11.2 X=19.9 Y=59.9 Z=19.0 RX=25.1 RY=18.3 RZ=148.7 CNOT=15.2 CZ=13.2
n=6 ns/gate: H=20.0 X=33.4 Y=93.0 Z=30.1 RX=38.6 RY=23.5 RZ=236.8 CNOT=30.1 CZ=25.6
sizeof(quantum_state_t)=72 sizeof(complex_t)=16
RESULT: PASS (0 fails)
```

Memory per upstream state: 72 B struct + 16 * 2^n B heap amplitudes (1,024 B at n=6) + 8,192 B heap
measurement history (`DEFAULT_MAX_MEASUREMENTS 1024`, `state.c:16,71`). Executable text 75,740 B.

### 4.3 Float determinism of the upstream kernel (ran four builds of the same test)

| Build | Flags | n=6 amp_hash | Same as A? |
|---|---|---|---|
| A | `-std=gnu11 -O2 -msse3` | `a72470c725135216` | - |
| B | `-std=gnu11 -O3 -march=native` | `83abd46db3832583` | **no (all five n differ)** |
| C | `-std=gnu11 -O3 -march=native -ffp-contract=off` | `a72470c725135216` | yes |
| D | `-std=gnu11 -O0 -msse3` | `a72470c725135216` | yes |

Reading: the double kernel is bit-reproducible on this machine only when FMA contraction is disabled.
GCC's default in gnu mode contracts `a*b+c` into FMA under `-march=native`, and the last bits change
(`P[0]` at n=2: `...118858` vs `...118903`). RX/RY/RZ also call libm `sin`, `cos`, `cexp` (UCRT here), so
equality across operating systems or C libraries is not guaranteed and was not tested. X, Y, Z, CNOT, CZ are
pure permutations and sign flips and are exact in any build. There is no threading below 2^18 amplitudes
(`gates.c:121`), so thread count is irrelevant at 2 to 6 qubits.

### 4.4 Candidate 2: fixed-point port, differential against upstream

```
gcc -std=gnu11 -O2 -msse3 -Wall -Wextra -o test_qsv_A.exe test_qsv.c $UP -lm     (no warnings from test_qsv.c / qsv_impl.h)
gcc -std=gnu11 -O3 -march=native -o test_qsv_B.exe test_qsv.c $UP -lm
./test_qsv_A.exe
```

The test replays one seeded gate stream (9 gate kinds, angles k * 2*pi/1024) through the upstream double
kernel, the Q1.30 port, the Q1.14 port, and the Q1.14 port with an integer renormalisation every 32 gates.
Output (build A; columns: max |dP| against upstream, and sum(P)-1 of the raw fixed-point state):

```
A trig table (integer-only Q60 Taylor): worst |err| vs libm = 0.498 LSB (Q30), 0.500 LSB (Q14)
A table hashes: cs30=063e66417c0ad00a cs14=ae4b1b5e17f2c27c (8192 B and 4096 B)
B columns: n depth | maxdP q30 (sumP-1) | maxdP q14 raw (sumP-1) | maxdP q14 renorm/32 (sumP-1) | hash q30 | hash q14r
B n=3 d=   32 | 8.65e-10 (-3.0e-09) | 7.01e-05 (+3.7e-05) | 7.01e-05 (+3.7e-05) | 957f1dc2ebf534f3 | 78ed981163890a4a
B n=3 d=  256 | 4.05e-09 (+3.3e-09) | 1.87e-04 (-1.4e-03) | 1.26e-04 (+8.6e-05) | d64f48f52cae9ba1 | 0dd8a77340c9aa17
B n=3 d= 4096 | 1.38e-08 (+5.7e-08) | 1.05e-03 (-8.6e-03) | 1.20e-03 (-8.2e-05) | bc815c107ad520e6 | 172eecffd5912034
B n=3 d=65536 | 4.93e-08 (+4.1e-07) | 4.23e-03 (-1.8e-01) | 4.08e-03 (+6.7e-05) | 00423ee27059fc5c | 80a5e65ed390ca65
B n=6 d=   32 | 1.15e-09 (-2.2e-09) | 4.30e-05 (-4.4e-04) | 4.95e-05 (+1.6e-05) | ce3ca96432eccf69 | 9328a7f4615466fd
B n=6 d=  256 | 2.51e-09 (+1.2e-09) | 1.37e-04 (-9.7e-04) | 1.17e-04 (-1.8e-05) | 1881c0514aad25f7 | 087d0876d8ab5764
B n=6 d= 4096 | 5.00e-09 (+4.3e-08) | 5.60e-04 (-1.5e-02) | 7.41e-04 (+8.6e-05) | 454426aad7b20837 | ea444d1427891b8e
B n=6 d=65536 | 4.15e-08 (+7.9e-07) | 2.01e-03 (-2.0e-01) | 2.51e-03 (-1.4e-04) | fa9058ef086ee69e | 3df1ba90bf872c5a
C bell q14 amps re: 11585 0 0 11585 ; samples: 50064 0 0 49936
C after collapse(q0=1): re = 0 0 0 16384 (expect 0 0 0 16384)
D n=3 q14 ns/gate: H=9.2 X=4.7 Y=5.0 Z=5.1 RX=11.5 RY=11.0 RZ=11.3 CNOT=6.2 CZ=4.6
D n=4 q14 ns/gate: H=14.1 X=7.7 Y=7.8 Z=6.7 RX=17.6 RY=17.4 RZ=17.3 CNOT=10.8 CZ=8.4
D n=5 q14 ns/gate: H=24.0 X=15.0 Y=14.5 Z=9.0 RX=29.4 RY=29.1 RZ=37.5 CNOT=20.6 CZ=18.6
D n=6 q14 ns/gate: H=42.2 X=27.4 Y=27.2 Z=14.0 RX=48.9 RY=48.6 RZ=50.7 CNOT=40.3 CZ=30.6
E n=3 q14 init+16 gates+sample: 107.6 ns per creature-circuit
E n=4 q14 init+16 gates+sample: 176.0 ns per creature-circuit
E n=5 q14 init+16 gates+sample: 300.1 ns per creature-circuit
E n=6 q14 init+16 gates+sample: 500.1 ns per creature-circuit
sizeof(qsv_t_q30)=512 sizeof(qsv_t_q14)=256
RESULT: PASS (0 fails)
```

(All 20 rows n=2..6 were printed and passed; n=2, 4, 5 are omitted above for length. Worst over all rows:
Q30 max |dP| 9.0e-8; Q14 raw 4.6e-3; Q14 renorm/32 4.4e-3, all at depth 65,536. Q30 timings were close to
Q14 at n=3..5 and slower on rotations at n=6, for example RX 78 ns against 49 ns.)

Determinism of the port (ran):

```
./test_qsv_A.exe | grep '^B n' | awk '{print $(NF-2), $NF}' > hashA.txt
./test_qsv_B.exe | grep '^B n' | awk '{print $(NF-2), $NF}' > hashB.txt
cmp hashA.txt hashB.txt && echo IDENTICAL
-> FIXED-POINT STATE HASHES IDENTICAL across -O2 -msse3 and -O3 -march=native (20 rows x 2 kernels)
```

Answers to the focus questions:

- **Could amplitudes be fixed-point? Yes.** X, Y, Z, CNOT, CZ are exact. H and the rotations round once per
  component. Q1.30 (int32, 8 B per amplitude) stays within 9e-8 of the double kernel after 65,536 gates with
  no renormalisation. Q1.14 (int16, 4 B per amplitude) is within 9e-5 at 32 gates, 3.5e-4 at 256 gates and
  about 1e-3 at 4,096 gates; its raw norm decays (sum P - 1 reaches -1.5e-2 at 4,096 gates and -0.2 at
  65,536), so it needs the integer renormalisation, which held |sum P - 1| at about 1.4e-4 or less in every row.
- **Cost per gate at 3 to 6 qubits (noisy, parallel load, one thread, scalar code, runtime-variable qubit):**
  port Q1.14: 5 to 12 ns at n=3, 7 to 18 ns at n=4, 9 to 38 ns at n=5, 14 to 51 ns at n=6. Upstream double:
  4 to 63 ns at n=3 and 20 to 237 ns at n=6; its RZ is the outlier because each call runs two `cexp`.
  A whole per-creature circuit (reset, 16 gates, one sample) in Q1.14: 108 / 176 / 300 / 500 ns at n = 3 / 4 / 5 / 6.
- **Trig without libm:** the cos/sin(theta/2) table is generated by an integer-only Q60 Taylor series
  (`test_qsv.c`, `taylor_q60`, `sincos_q60`), matches libm to 0.5 LSB, and has a fixed hash, so it can be
  frozen into the engine as a constant with a checksum.

### 4.5 Candidates 3 and 4: Clifford tableau and SHA3 (ran)

```
gcc -std=gnu11 -O2 -Wall -Wextra -o test_cliff_sha3.exe test_cliff_sha3.c up/src/backends/clifford/clifford.c up/src/crypto/sha3/sha3.c
./test_cliff_sha3.exe
clifford GHZ6 x20000: 000000=9902 111111=10098 other=0  final rng=ae05bbf89b1e18d9  1150 ns per create+6 gates+sample+free
clifford n=6: 1.7 ns per gate (H and CNOT alternating)
sha3_256("")    = a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a ok
sha3_256("abc") = 3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532 ok
sha3_256 throughput: 111 MiB/s (1 MiB x 20)
RESULT: PASS (0 fails)
```

Both files compile with no other Moonlab source. The Clifford gates are exact integer bit operations at
1.7 ns per gate, but the object is heap-allocated (create + free dominates the 1,150 ns) and the measure
path allocates again.

## 5. Proposed integration

Use: **(c) small state-vector module per creature, built on (b) fixed-point math.** Tier 1 (phased, every
32 to 64 ticks). Never Tier 0.

```c
/* n = 4 qubits: 16 amplitudes, Q1.14, structure of arrays = 64 bytes = one cache line per creature. */
typedef struct { int16_t re[16]; int16_t im[16]; } an_qmod4_t;

/* Frozen table: cos/sin(k*pi/256) in Q1.14, k = 0..255 (1,024 bytes), checked against a constant hash at start-up. */
extern const int16_t an_qmod_cs14[2 * 256];

void     an_qmod4_reset (an_qmod4_t *s);
/* prog word: kind:4 | q:2 | q2:2 | angle source:8 (a genome constant or an index into sensor[]). */
void     an_qmod4_run   (an_qmod4_t *s, const uint16_t *prog, uint32_t n_gates, const uint8_t *sensor);
void     an_qmod4_renorm(an_qmod4_t *s);                      /* integer, call once per run */
uint32_t an_qmod4_sample(const an_qmod4_t *s, uint32_t philox_word);   /* basis index 0..15 */
void     an_qmod4_expect_z(const an_qmod4_t *s, int32_t z_q14[4]);     /* integer <Z_q> per qubit */
```

- Causal role: the genome encodes the gate program; sensor bytes set rotation angles; the sampled index
  (or the integer `<Z>` vector) selects the action; the state is carried between Tier-1 phases as memory.
  Lesion tests: zero the program, or delete the CNOT/CZ genes, and compare outcomes.
- Randomness: the only random input is one Philox4x32-10 word keyed by (creature id, tick), so the result is
  independent of thread count. This is a classical simulation of a small norm-preserving linear map plus a
  sampling step. It must be labelled a state-vector module; it is not quantum randomness and gives no speed-up.
- Budget estimate from the measured 176 ns per 16-gate n=4 circuit (noisy): 1,000,000 creatures phased over
  32 ticks is 31,250 circuits per tick, about 5.5 ms on one thread before any SIMD or threading.
- Untested idea: process 16 creatures that share one program in AVX2 int16 lanes (`vpmulhrsw` is a rounding
  Q15 multiply). The rounding rule of the scalar reference would have to be redefined to match it exactly.
- Tooling only: keep the trimmed upstream build (section 4.2) as the differential oracle for the port, built
  with `-ffp-contract=off`. SHA3-256 (`sha3.c`, 235 lines) can hash evidence receipts in tooling.
- Lower priority: a fixed-size Clifford tableau (for n=6: 12 rows of 13 bits) as an exact integer module.
  Its outcomes are only deterministic or 50/50, so it is less expressive than the rotation kernel.
- Idea for (e), tooling or Tier 2 only: the Fubini-Study metric g_ij = Re(<d_i psi|d_j psi> - <d_i psi|psi><psi|d_j psi>)
  of a creature's P-angle program costs 2P+1 circuit runs by central differences; not built or tested here.

License: the port is new code that follows Moonlab's conventions; credit the repo. If any upstream file is
shipped (even in tooling), keep the MIT notice.

## 6. Not usable, and why

- **Upstream dense kernel inside the simulation:** two heap blocks per state (9.2 KB at n=6), a global config
  behind a mutex, `pthread_once` dispatch, libm trig, doubles whose bits change with FMA contraction. Breaks
  laws 2 and 6. Oracle use only.
- **WASM build, JS/TS, Python and Rust bindings:** VM or interpreted; excluded by law 1.
- **QRNG / entropy stack (`applications/qrng.c`, `hardware_entropy.c`, `algorithms/quantum_rng.c`):** OS entropy
  or time-seeded xorshift; non-deterministic and mislabelled as quantum in places. Breaks laws 6 and 7.
- **splitmix64 / xorshift64 helpers:** fine code, but the engine mandates Philox4x32-10; nothing to take for (a).
- **Tensor networks (MPS, DMRG, TDVP), VQE chemistry, QAOA, QPE, Shor estimator, QEC decoders, MBL:** large
  double-precision solvers with no per-creature causal role.
- **Quantum annealing (`quantum_annealing.c`):** double state-vector evolution followed by exhaustive
  enumeration; a plain integer Ising or Hopfield update does not need it. Idea only for (f).
- **libirrep bridge:** stub that reports NOT_BUILT; gives nothing for (d).
- **GPU backends:** CUDA `.cu` needs nvcc (absent), Metal is Apple-only, WebGPU kernels are float32.
- **`vqe_qng.c` as code:** needs the whole 2,669-line VQE solver and malloc. The formula is reusable; the file is not.
- **ML-KEM, AES, DRBG:** no role in the simulation.
- **Clifford tableau as written:** heap object and allocations in the measure path; only the bit-packed
  update rules are worth porting.

## 7. Not verified

- Cross-OS or cross-libm equality of the upstream double kernel (only one machine, one C library).
- The port with hand-written SIMD, fixed-n specialisation, or NASM; all timings are scalar GCC output.
- Any Q1.14 behaviour beyond the random 9-gate streams above (for example long runs of one rotation gate).
- The upstream measurement, `<Z>`, `<ZZ>` and entanglement functions were read, not run.
- Clifford measure path internals beyond the GHZ sampling test.
- Files in scratch: `qsv_impl.h`, `shim_sync.c`, `test_up.c`, `test_qsv.c`, `test_cliff_sha3.c`, `hashA.txt`, `hashB.txt`, `up\`.
