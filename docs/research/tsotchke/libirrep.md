# Tsotchke audit: libirrep

Audit date 2026-11-17. Scratch: `S:\tsotchke\libirrep\`. Everything marked "ran" was run in this session;
the commands and output tails are in section 4. Timings are noisy (parallel load).

## 1. Repo facts

| Item | Value |
|---|---|
| Path | `T:\mirrors\libirrep` (read-only; nothing was built or changed there) |
| HEAD | `6c6acab_2026-05-19` (`git -C T:/mirrors/libirrep log -1 --format=%h_%cs`) |
| Version | `1.5.0` (file `VERSION`), README calls it "pre-release" |
| License | MIT License, "Copyright (c) 2026 tsotchke" (file `LICENSE`) |
| Language | C11 (276 `.c`, 69 `.h` tracked; 1 Python script, tooling only: `scripts/bench_vs_e3nn.py`) |
| Size | 54 MB on disk (3.2 MB `.git`, 4.5 MB `src`, 3.7 MB `include`, 13 MB `tests`); 408 tracked files; `src/*.c` = 27,347 lines |
| Runtime deps | libc + libm only. Optional AVX2 / NEON translation units behind a runtime dispatch table |
| Scope | SO(3)/SU(2)/O(3) representation theory (the part audited here) plus lattices, point groups, exact diagonalisation, a QEC stabilizer-code substrate and a NequIP layer (not audited beyond header listing) |

Build system: `Makefile` and `CMakeLists.txt`. I did not use either (GNU make is not on this machine and
building inside `T:` is forbidden); I compiled the copied translation units directly with gcc 16.1.0.

Porting note for Windows static builds: `include/irrep/export.h` maps `IRREP_API` to
`__declspec(dllimport)` unless `IRREP_BUILDING_DLL` is defined, so a static compile needs
`-DIRREP_BUILDING_DLL`. `src/spherical_harmonics.c` calls `irrep_dispatch_get()` (defined in
`src/simd_runtime.c`, which drags in the radial kernels); I replaced it with an 18-line scalar shim
(`S:\tsotchke\libirrep\shim_dispatch.c`, my file, not libirrep's).

## 2. Candidate kernels

All candidates map to engine use **(d)** (SO(3)/SU(2) representation theory for rotation-equivariant
3D perception). None maps to (a), (c), (e), (f), (h), (i), (j) or (k). Candidate 5 touches (b) and (g).

| # | Kernel | File and lines | Signatures | Dependencies | Pure / stateful | Float / int | Thread safety |
|---|---|---|---|---|---|---|---|
| 1 | Real spherical harmonics, closed polynomial form, l <= 4 | `src/solid_harmonics.c` 100-129 (l <= 3 polynomials), 68-94 (constants), 337-386 (entry points) | `void irrep_solid_harm_cart(int l, double *out, const double r[3]);` `void irrep_solid_harm_cart_all(int l_max, double *out, const double r[3]);` | libm `sqrt` (constants only), `<stdatomic.h>` | Pure after a lazy one-time constant init guarded by an `atomic_bool` | double | Safe for l <= 4. **Not safe for l >= 5**: lines 364 and 549-550 use function-`static` scratch buffers shared by all threads |
| 2 | Real spherical harmonics, Legendre recurrence (any l <= 16) | `src/spherical_harmonics.c` 38-93 (`irrep_legendre_assoc`), 139-196 (`_cart`, `_cart_all`), 242-262 (complex-to-real matrix U) | `void irrep_sph_harm_cart(int l, double *out, const double r_hat[3]);` `void irrep_sph_harm_cart_all(int l_max, double *out, const double r_hat[3]);` `void irrep_sph_harm_complex_to_real(int l, double _Complex *out);` | libm `sqrt`; `internal/dispatch.h` only for the batch entry | Pure | double (f32 wrappers just cast) | Safe (no shared state on the scalar path) |
| 3 | Clebsch-Gordan / Wigner 3j | `src/clebsch_gordan.c` 86-168 (Racah single sum, j <= 15), 326-348 (`irrep_cg_2j`, `irrep_cg`), 438-545 (heap table), 547-561 (`irrep_cg_block`) | `double irrep_cg(int j1,int m1,int j2,int m2,int J,int M);` `double irrep_cg_2j(int two_j1,int two_m1,int two_j2,int two_m2,int two_J,int two_M);` `void irrep_cg_block(int j1,int j2,int J,double *out);` `cg_table_t *irrep_cg_table_build(int j1_max,int j2_max);` | libm `sqrt`; `stdlib` (`calloc`) only for the heap table | Pure, except a lazy factorial table behind a plain `static int` flag (lines 86-97) | double | Benign race on first call (every thread writes identical values, but it is a data race by the letter of C11); call once before threading |
| 4 | Wigner small-d and D | `src/wigner_d.c` 61-84 (Jacobi recurrence), 141-200 (`irrep_wigner_d_small_2j`), 206-230 (complex D), 275-314 (`irrep_wigner_d_matrix`) | `double irrep_wigner_d_small_2j(int two_j,int two_mp,int two_m,double beta);` `void irrep_wigner_d_matrix(int j,double *out,double beta);` `void irrep_wigner_D_matrix(int j,double _Complex *out,double alpha,double beta,double gamma);` | libm `sin`, `cos`, `sqrt` (`lgamma`, `exp` only for j > 15); `<complex.h>` | Pure, same lazy factorial-table pattern as CG (lines 129-139) | double / double complex | Same benign first-call race as CG |
| 5 | SO(3)/SU(2) utilities and sphere quadrature | `src/so3.c` 33-345 (quaternion, matrix, Euler ZYZ, exp/log, apply), 358-380 (Shoemake random quaternion on an LCG), `src/su2.c` 28-166, `src/quadrature.c` 26-80 (Gauss-Legendre), 172-262 (Lebedev orders 3/5/7 only), 270-325 (product rule) | `irrep_rot_matrix_t irrep_rot_from_quat(irrep_quaternion_t);` `irrep_euler_zyz_t irrep_euler_zyz_from_rot(irrep_rot_matrix_t);` `void irrep_rot_apply(irrep_rot_matrix_t,const double v[3],double out[3]);` `bool irrep_lebedev_fill(int order,double *xyz_weights);` `bool irrep_quadrature_sphere_fill(int exactness_deg,double *xyz_weights);` | libm trig, `sqrt`, `atan2`; `malloc` in the quadrature product rule and Lebedev registry | Pure, except the Lebedev registry (global array, unsynchronised) and caller-owned RNG state | double | Pure functions safe; the Lebedev registry is not |

Secondary (not compiled here): `src/tensor_product.c` 228-300 builds the real-basis coupling tensor
from the complex CG block plus the `i^(la+lb-lc)` phase. It is heap-allocating descriptor code; I
re-implemented those 30 lines in the tests rather than port the 918-line file.

## 3. What the numerical checks show

1. **Orthonormality (the requested identity).** The 16 real Y_lm with l <= 3 are orthonormal on the
   26-point Lebedev rule and on the 28-point Gauss-Legendre x phi rule to 1.3e-15. A 6-point rule
   (negative control) fails with error 1.5, as it must.
2. **Three independent SH code paths agree** (recurrence, closed polynomial, polar form) to 3e-14.
3. **Wigner small-d** is orthogonal to 1.1e-15 for 2j = 0..6 including half-integer spin; four
   closed-form values match to 1.1e-16; complex D is unitary to 4.4e-16.
4. **Convention that makes the pieces fit** (not written down in the headers; found by test):
   `Y_real(R r) = [U conj(D(alpha,beta,gamma)) U^dagger] Y_real(r)` with (alpha,beta,gamma) from
   `irrep_euler_zyz_from_rot(R)` and U from `irrep_sph_harm_complex_to_real`. Error 2.0e-14; the three
   other candidate forms are wrong by order 1.
5. **Trig-free rotation of features.** The real l = 1 block is the 3x3 rotation matrix with axes ordered
   (y, z, x) (error 3.5e-15). D^2 = W112^T (D^1 x D^1) W112 and D^3 = W123^T (D^1 x D^2) W123 reproduce
   the Wigner-D route to 1.0e-15 and 1.9e-15. So the engine never needs Wigner-d, Euler angles or libm
   at run time: a creature's 3x3 orientation matrix plus two small constant tables is enough.
6. **Clebsch-Gordan sum rules.** Row orthonormality and completeness hold to 5.6e-16 over all
   j1, j2 in {0, 1/2, ..., 2} (3025 sums each); seven textbook values match to 1.1e-16.

### Table sizes for l <= 2 (ran)

| Object | Size |
|---|---|
| Real SH values per direction | 9 (l <= 2), 16 (l <= 3) |
| Coupling paths (la, lb, lc all <= 2, triangle-valid) | 15 |
| Dense coupling entries over the 15 paths | 615 (1,230 bytes as int16; 4,920 bytes as double) |
| Nonzero entries, complex basis | 113 |
| Nonzero entries, real basis | 137 (685 bytes as 5-byte COO `{a,b,c,q15}`) |
| Largest single path (2,2,2) | 125 dense, 25 nonzero real-basis |
| Small-d or real D matrices for l <= 2 | 1 + 9 + 25 = 35 reals per rotation |

**Can a fixed-point table be generated at build time? Yes (ran).** `test_cg_tables.exe <path>` writes a
compilable C header with the 137 nonzero real-basis entries in Q15 plus a 15-row path directory.
Quantisation error is 3.05e-5 (half an LSB, plus saturation of the value 1.0 to 32767) and the
quantised table is orthonormal to 6.1e-5. The CG path for j <= 15 uses only `sqrt`, multiply, add and
divide on doubles, so with `-ffp-contract=off` the generated integers do not depend on the libm. The
safer policy is still to check the generated header in and compare its hash in CI (sha256 of this
session's file: `2433ca18...c0f11f`).

Wigner D cannot be tabulated the same way for arbitrary rotations (it depends on continuous angles);
point 5 above removes the need. For yaw-only rotation the real-basis D is 2x2 blocks of
(cos m*phi, sin m*phi), which needs only a sine table.

### Fixed-point prototype (my code, derived from candidate 1; ran)

`an_sh3_q15` in `S:\tsotchke\libirrep\test_fixed_sh.c` evaluates all 16 values with int64
multiply-add and nine Q14 constants, no division, no libm.

- Error against the same polynomial in double: 1.99 LSB of Q15 (dominated by the Q14 constants; wider
  constants would cut it). Error against libirrep on the unquantised direction: 4.26 LSB.
- The integer power spectrum p_l = sum_m c_lm^2 of 12 weighted neighbours changes by at most 1.9e-4 of
  p_0 under 2000 random rotations; a single component (negative control) changes by 1.19.
- The int16 output hash is identical between `-std=c2x -O2` and `-std=gnu2x -O3 -march=native`
  (`4b6f66fc46aa94b1`). The hash of the double outputs differs between the same two builds
  (`30296a5f...` vs `2a73d1f5...`). The difference may come from the direction generator as well as
  from the SH code; either way the double path is not bit-stable across compiler flags.
- Cost per 16-value evaluation (noisy, parallel load): libirrep recurrence 49 ns, libirrep polynomial
  8.3 ns, Q15 prototype 7.2 ns, single thread, scalar.

## 4. Exact build and run commands with output tails

All commands were run from `S:\tsotchke\libirrep` in Git Bash. Sources copied from the mirror:
`include/irrep/{types,export,spherical_harmonics,solid_harmonics,wigner_d,clebsch_gordan,quadrature,so3,su2,simd,multiset}.h`,
`src/{spherical_harmonics,solid_harmonics,wigner_d,clebsch_gordan,quadrature,so3,su2,error}.c`,
`src/internal/dispatch.h`, and `LICENSE` (as `LICENSE.libirrep`).

### Test 1: SH orthonormality, Wigner, equivariance

```
gcc -std=c2x -O2 -Wall -Wextra -DIRREP_BUILDING_DLL -Iinclude -Isrc -I. test_sh_wigner.c shim_dispatch.c src/spherical_harmonics.c src/solid_harmonics.c src/wigner_d.c src/clebsch_gordan.c src/quadrature.c src/so3.c src/su2.c src/error.c -o build/test_sh_wigner.exe -lm
./build/test_sh_wigner.exe
```

Compiler output: two warnings only (`#pragma STDC FP_CONTRACT` ignored by gcc at
`src/spherical_harmonics.c:26`; unused parameter `j_max` at `src/clebsch_gordan.c:178`).

```
A1 Lebedev order 7: 26 points, weight sum 1.000000000000000, max|G-I| (16x16, l<=3) = 1.332e-15
A2 Gauss-Legendre x phi (exactness 6): 28 points, weight sum 1.000000000000000, max|G-I| = 1.110e-15
A3 negative control, Lebedev order 3 (6 points): max|G-I| = 1.528e+00 (must be large)
B  2000 random directions, l<=3: |recurrence - polynomial| max = 3.154e-14, |cart - polar| max = 8.882e-16
C1 small-d orthogonality, 2j = 0..6 (incl. half-integer), 16 betas: max|d d^T - I| = 1.110e-15
C2 closed-form small-d values (4 cases): max err = 1.110e-16
C3 complex D unitarity l=0..3 at (0.3,0.9,1.5): max|D D^+ - I| = 4.441e-16
D  variant U D U^+       : max|Dreal Y(r) - Y(Rr)| = 1.146e+00, max imag part = 0.000e+00
D  variant U conj(D) U^+ : max|Dreal Y(r) - Y(Rr)| = 1.954e-14, max imag part = 0.000e+00
D  variant U D^T U^+     : max|Dreal Y(r) - Y(Rr)| = 1.144e+00, max imag part = 0.000e+00
D  variant U D^+ U^+     : max|Dreal Y(r) - Y(Rr)| = 1.060e+00, max imag part = 0.000e+00
D  best variant index = 1, error 1.954e-14
E1 D^1(real, order y,z,x) equals permuted 3x3 rotation matrix: max err = 3.497e-15
E2 D^2 = W112^T (D1 x D1) W112 vs Wigner-D route: max err = 9.992e-16
E3 D^3 = W123^T (D1 x D2) W123 vs Wigner-D route: max err = 1.887e-15
RESULT test_sh_wigner: PASS (0 failing checks)
```

### Test 2: Clebsch-Gordan sum rules, table sizes, Q15 table generator

```
gcc -std=c2x -O2 -Wall -Wextra -Wno-unknown-pragmas -Wno-unused-parameter -DIRREP_BUILDING_DLL -Iinclude -Isrc -I. test_cg_tables.c shim_dispatch.c src/spherical_harmonics.c src/solid_harmonics.c src/clebsch_gordan.c src/error.c -o build/test_cg_tables.exe -lm
./build/test_cg_tables.exe build/an_cg_l2_q15.h
gcc -std=c2x -Wall -Wextra -fsyntax-only -x c build/an_cg_l2_q15.h
sha256sum build/an_cg_l2_q15.h
```

```
1  CG row orthonormality, all j1,j2 in {0,1/2,...,2}: 3025 sums, max err = 5.551e-16
2  CG completeness over (J,M): 3025 sums, max err = 4.441e-16
3  textbook CG / 3j values (7 cases): max err = 1.110e-16
3b heap table irrep_cg_table_build(2,2) lookup vs direct: max diff = 0.000e+00
4  path (la,lb,lc): dense entries | nonzero complex CG | nonzero real-basis W
   (0,0,0):   1 |  1 |  1
   (0,1,1):   9 |  3 |  3
   (0,2,2):  25 |  5 |  5
   (1,0,1):   9 |  3 |  3
   (1,1,0):   9 |  3 |  3
   (1,1,1):  27 |  6 |  6
   (1,1,2):  45 |  9 | 11
   (1,2,1):  45 |  9 | 11
   (1,2,2):  75 | 12 | 16
   (2,0,2):  25 |  5 |  5
   (2,1,1):  45 |  9 | 11
   (2,1,2):  75 | 12 | 16
   (2,2,0):  25 |  5 |  5
   (2,2,1):  75 | 12 | 16
   (2,2,2): 125 | 19 | 25
   TOTAL l<=2: 15 paths, dense 615 entries (1230 bytes as int16, 4920 bytes as double), nonzero complex 113, nonzero real-basis 137 (685 bytes as 5-byte COO {a,b,c,q15})
   Q15 quantisation: max |q/32768 - W| = 3.052e-05 ; quantised-table orthonormality error = 6.103e-05
RESULT test_cg_tables: PASS (0 failing checks)
163 build/an_cg_l2_q15.h
2433ca1830a5d4c8c6f222f2fe103169dd928b368bd09cd5bed7e6a648c0f11f *build/an_cg_l2_q15.h
```

The syntax-only compile of the generated header printed nothing (no warnings, no errors).

### Test 3: integer SH prototype, invariance, determinism, timing

```
gcc -std=c2x -O2 -Wall -Wextra -Wno-unknown-pragmas -DIRREP_BUILDING_DLL -Iinclude -Isrc -I. test_fixed_sh.c shim_dispatch.c src/spherical_harmonics.c src/solid_harmonics.c src/so3.c src/error.c -o build/test_fixed_sh.exe -lm
./build/test_fixed_sh.exe
gcc -std=gnu2x -O3 -march=native -Wno-unknown-pragmas -DIRREP_BUILDING_DLL -Iinclude -Isrc -I. test_fixed_sh.c shim_dispatch.c src/spherical_harmonics.c src/solid_harmonics.c src/so3.c src/error.c -o build/test_fixed_sh_native.exe -lm
./build/test_fixed_sh_native.exe | grep -E "hash|RESULT|ns per"
```

```
1  Q15 integer SH l<=3, 100000 directions: max err vs same polynomial in double = 6.065e-05 (1.99 LSB),
   max err vs libirrep on the unquantised unit direction = 1.301e-04 (4.26 LSB)
   hash(int16 outputs) = 4b6f66fc46aa94b1   hash(libirrep double outputs) = 30296a5fbeb3f69d
3  integer power spectrum p_l = sum_m c_lm^2 under 2000 random rotations of 12 weighted neighbours:
   max |p_l(rotated) - p_l| / p_0 : l=0 0.00e+00, l=1 1.06e-04, l=2 1.76e-04, l=3 1.86e-04
   negative control (single component c_20^2, not an invariant): 1.19e+00
4  ns per 16-value evaluation (noisy, parallel load): libirrep recurrence 48.8, libirrep polynomial 8.3, Q15 integer prototype 7.2
RESULT test_fixed_sh: PASS (0 failing checks)
---- variant: -std=gnu2x -O3 -march=native (FMA contraction allowed)
   hash(int16 outputs) = 4b6f66fc46aa94b1   hash(libirrep double outputs) = 2a73d1f567a0efd2
4  ns per 16-value evaluation (noisy, parallel load): libirrep recurrence 38.4, libirrep polynomial 8.1, Q15 integer prototype 7.0
RESULT test_fixed_sh: PASS (0 failing checks)
```

(The first build printed two `-Wunused-but-set-variable` warnings for the timing sinks in my test file.)

## 5. Proposed integration

Principle: libirrep's double-precision code runs **only in tooling** (table generation and test
oracles). The simulation gets small integer kernels whose constants come from libirrep at build time.
This keeps laws 1, 2 and 6 (native, cache-resident, integer state) without giving up the mathematics.

| Engine piece | C signature (proposal) | Where it runs | Source |
|---|---|---|---|
| Table generator | `tools/gen_irrep_tables.c` -> `an_irrep_tables.h` (nine SH constants, coupling tables for the chosen paths, path directory); header checked in, hash checked in CI | Tooling only | libirrep candidates 1, 2, 3 linked into the generator |
| Direction encoding | `void an_sh3_q15(const int16_t v[3], int16_t out[16]);` and an AVX2 structure-of-arrays form `void an_sh3_q15_x16(const int16_t *x, const int16_t *y, const int16_t *z, int16_t *out /* 16 planes */, size_t n);` | Tier 1 (phased, every 32 to 64 ticks) per creature over its neighbour list; too costly for Tier 0 at millions of creatures | Port of candidate 1 polynomials (prototype measured above) |
| Neighbourhood moments | `void an_percept_moments(const int16_t *dx, const int16_t *dy, const int16_t *dz, const int16_t *w, int n, int32_t c[16]);` using un-normalised solid harmonics so no square root or division is needed | Tier 1 | Candidate 1 |
| Rotation-invariant descriptor | `void an_power_spectrum(const int32_t c[16], int64_t p[4]);` plus optional bispectrum terms through the coupling table | Tier 1; Tier 2 for colony-level shape descriptors | Candidate 3 table |
| Equivariant steering output | l = 1 block of the moments is a vector in (y, z, x) order; coupling path (1,1,1) is the cross product, (1,2,1) and (2,1,1) bend a vector by a quadrupole | Tier 1 | Candidate 3 table |
| Body-frame rotation of features | `void an_rot_features_l2(const int16_t R[9], const int32_t in[9], int32_t out[9]);` built from D^1 = permuted R and D^2 = W112^T (R x R) W112 | Tier 1 (only when a creature needs body-frame features) | Result 5 in section 3 |
| Test oracle | libirrep doubles compared against the integer kernels with a stated LSB budget | Tooling only (unit tests) | Candidates 1 to 5 |

Causal role and lesion test: the descriptor feeds the creature's controller inputs. Lesions to run:
(a) zero the l >= 2 moments, (b) replace the invariant p_l by raw components (breaks rotation
invariance, the negative control above shows the size of the effect). If foraging, flocking or
predator-evasion outcomes do not change under (a), the l >= 2 channels are decoration and should be
removed.

Open design points, not settled by this audit:
- Q15 constants give about 2 LSB error; whether that matters depends on the controller's input
  precision. Wider constants (Q20 in int32) are free with int64 intermediates.
- `>>` on negative signed integers is implementation-defined in C; the kernel must use an explicit
  arithmetic-shift idiom or NASM `sar` so that every compiler produces the same bits.
- Invariance in fixed point is approximate (1.9e-4 of p_0 here), not exact. Any hash-relevant state
  must therefore depend only on the integer outputs, never on an assumption of exact invariance.

## 6. What is NOT usable, and why

- **The library as a linked dependency inside the simulation.** Every kernel is double or double
  complex and several call libm trig; law 6 requires integer or fixed-point state and the double
  outputs were not bit-stable across compiler flags here.
- **`irrep_sph_harm_cart` (recurrence path) in a hot loop.** It recomputes the Legendre recurrence and
  a `sqrt` per (l, m); it measured about 6 times slower than the closed polynomial for l <= 3.
- **Wigner small-d / D at run time.** Needs Euler angles (`atan2`, `acos`) and `sin`/`cos`; the
  coupling-table route replaces it. Keep it as a test oracle only.
- **`cg_table_t` heap tables and `tp_descriptor_t` tensor-product descriptors.** They `calloc`
  (16 allocation sites in `tensor_product.c`); law 2 wants a pre-allocated arena and constant tables.
- **`irrep_solid_harm_cart` for l >= 5 and the Lebedev registry.** Shared static buffers and an
  unsynchronised global registry make them unsafe with 24 threads. Not needed for l <= 3.
- **Lebedev rules above order 7.** Not shipped; the header says they must be registered by the caller
  (a download script exists, `scripts/fetch_lebedev_tables.sh`; not run, downloads are forbidden).
- **`irrep_quat_random`.** Uses its own 64-bit LCG and libm; the engine's RNG is Philox4x32-10.
- **NequIP layer, equivariant linear layers, e3nn-style weighted tensor products.** They are learned
  float layers with backward passes; the engine's intelligence is genome plus plasticity, and no
  pretrained weights are allowed. The path-enumeration idea (which (la, lb, lc) triples exist) is the
  only part worth keeping, and section 3 already lists the 15 paths.
- **QEC codes, lattice exact diagonalisation, magnon and skyrmion modules, point and space groups.**
  Out of scope for this audit's focus and, as far as the headers show, no causal role in a creature
  simulation. Not compiled, not verified; their README maturity claims are the author's, untested here.
- **AVX2 / NEON batch kernels and the runtime dispatcher.** Not compiled here. Runtime dispatch on CPU
  features is also a determinism hazard unless every variant is bit-identical.
- **README performance claims** (for example "2488x faster than e3nn") were not reproduced.

## 7. Verdict

**adapt.** The mathematics is correct to machine precision on everything tested and the license is
MIT, but nothing can be used as-is inside the simulation because it is double precision. Use libirrep
as the build-time generator and test oracle; port the l <= 3 polynomials and the sparse coupling
tables to integer kernels (prototype in scratch already passes).

Files left in scratch: `S:\tsotchke\libirrep\{shim_dispatch.c, test_sh_wigner.c, test_cg_tables.c,
test_fixed_sh.c}`, the copied sources under `src\` and `include\`, and `build\an_cg_l2_q15.h`.
