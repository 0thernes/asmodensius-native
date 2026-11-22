# Tsotchke audit: quantum_rng and Quantum-RNG-API

Agent key: `quantum-rng`. Date: 2026-11-17. Scratch: `S:\tsotchke\quantum-rng\`.
Everything marked "ran" below was run in this session; commands and output
tails are given. Timings are "noisy, parallel load" (other agents share the box).

## 1. Short answer

- `quantum_rng` is a classical library. Nothing in it is quantum randomness
  (law 7). The usable part for this engine is small: three pure 64-bit integer
  hash functions and one byte-string digest (about 40 lines), plus a clean
  FIPS 180-4 SHA-256 header in the examples. Verdict: **adapt** (trimmed port
  written and verified bit-identical to upstream: `S:\tsotchke\quantum-rng\tq_mix.h`).
- `Quantum-RNG-API` is an Express (Node.js) HTTP server around a native addon
  that wraps the same C. It adds no numeric code. Verdict: **excluded_by_law**
  (network API + JavaScript; laws 1 and 3).
- None of this replaces Philox4x32-10 (law 6). The mixers only fit as seed/key
  plumbing and tooling digests. Honest value: low to moderate.

## 2. Repo facts

| | quantum_rng | Quantum-RNG-API |
|---|---|---|
| Path | `T:\mirrors\quantum_rng` | `T:\mirrors\Quantum-RNG-API` |
| HEAD (`git log -1 --format=%h_%cs`) | `1a77e77_2026-07-11` | `b02c703_2026-07-14` |
| `git describe --tags` | `v3.0.1-5-g1a77e77` | not checked |
| Licence (LICENSE file) | MIT, "Copyright (c) 2024 Tsotchke" | MIT, "Copyright (c) 2024 Tsotchke Corporation" |
| Main language | C (91 `.c`, 45 `.h`; 45,405 C/H lines, 16,460 under `src/`) | JavaScript + C++ glue (1,281 own lines) over vendored C |
| Size (work tree, no `.git`) | 9.8 MB, 174 tracked files | 2.9 MB, 59 tracked files |
| Upstream build flags | `-Ofast -flto -ffast-math -funroll-loops` (Makefile:17) | node-gyp |

`quantum_rng.c` sha256 = `86dcf1c4836462f8fdfcdf63c39df0e0d52570786c471290d1ea97f250471bce`
(same value the prior audit recorded; recomputed here with `sha256sum` and with
upstream's own `sha256.h`).

Three generators live in the tree (confirmed by reading, as the prior audit said):
1. legacy "quantum-inspired" core `src/quantum_rng/quantum_rng.c` (607 lines):
   integer hashes mixed with a `sin`/`cos` chaotic map (`quantum_noise`, lines 39-58);
2. v3: an 8-qubit `double _Complex` state-vector simulator whose output is XORed
   with OS/CPU entropy (`quantum_rng_v3.c`, `quantum_state.c`, `quantum_gates.c`);
3. `secure_rng`: hardware entropy that seeds generator 1.

## 3. Check of the prior audit (`W:\Glossodrom Language Lab\docs\research\qrng.md`)

| Prior claim | Result here |
|---|---|
| Integer KATs for `splitmix64` / `hadamard_mix` (8 inputs each) and `absorb_seed` | **Reproduced**, 19/19 values checked in code against the unmodified upstream source (ran `kat.exe`); the remaining absorb values match by eye in the KAT table below |
| "0 is a fixed point of both mixers" | **Reproduced** (also true of the `qrng_uint64` output mix) |
| "`splitmix64()` is not standard SplitMix64" | **Reproduced and sharpened**: it is exactly the standard SplitMix64 finalizer (Stafford mix13) followed by `x *= 0xC13FA9A902A6328F; x ^= x >> 29`, with no additive gamma (checked on 2^22 inputs; `mix13(0x9E3779B97F4A7C15) = 0xE220A8397B1DCDAF`, which I recall as the first SplitMix64 output for seed 0 - recalled, not looked up) |
| HEAD `1a77e77`, `v3.0.1` + 5 commits | **Reproduced** (`git rev-list --count v3.0.1..HEAD` = 5; the API's `UPSTREAM.md` says "one commit newer", which is wrong) |
| API vendored `src` identical to the mirror | **Reproduced** (`diff -rq`: only `qrng_cli_v2.c`, `quantum_rng_cli.c`, `secure_rng/` are missing from the vendored copy) |
| Seeded legacy stream is libm/FMA dependent, 7 different streams | **Not re-run** (out of scope: only integer mixers are of interest). Consistent with the source: `quantum_noise` calls `sin`/`cos` on huge arguments |
| Pure-integer parts are bit-portable | **Reproduced** on gcc -O0/-O2/-O3 -march=native -ffast-math, clang 21 (mingw target), and wasm32 (32-bit) |
| NEW, not in the prior audit | `hadamard_mix` and the `qrng_uint64` output mix are **not injective**; concrete collisions found (section 6). The prior audit suggested them as a "counter-mode mixer"; that needs this caveat |

## 4. Candidate kernels

| # | Kernel | File and lines (upstream) | Signature | Deps | Pure / stateful | Float / int | Thread safe | Engine use |
|---|---|---|---|---|---|---|---|---|
| 1 | `splitmix64` (port: `tq_sm`) | `src/quantum_rng/quantum_rng.c:61-70` | `static inline uint64_t splitmix64(uint64_t x)` | none | pure | integer | yes | (a) bijective 64-bit finalizer |
| 2 | `hadamard_mix` (port: `tq_hm`) | `quantum_rng.c:73-83` | `static inline uint64_t hadamard_mix(uint64_t x)` | #1 | pure | integer | yes | (a) hash only; not injective |
| 3 | `absorb_seed` (port: `tq_absorb`) | `quantum_rng.c:113-120` | `static uint64_t absorb_seed(const uint8_t *seed, size_t seed_len)` | #1, #2 | pure | integer | yes | (a) byte string -> 64-bit digest, tooling |
| 4 | `qrng_uint64` output mix (port: `tq_fin`) | `quantum_rng.c:422-427` (inside `qrng_uint64`, 414-430) | inline code, no function upstream | #1 | pure | integer | yes | (a) hash only; not injective |
| 5 | SHA-256 | `examples/crypto/sha256.h:1-159` | `sha256_init(sha256_ctx_t*)`, `sha256_update(sha256_ctx_t*, const void*, size_t)`, `sha256_final(sha256_ctx_t*, uint8_t[32])`, `sha256(const void*, size_t, uint8_t[32])` | `<string.h>` | stateful ctx (112 bytes on x86-64), no globals | integer | yes per ctx | (j) verification receipts, tooling |
| 6 | unbiased bounded draw | `quantum_rng.c:437-463` (`qrng_range32`), 465+ (`qrng_range64`) | `int32_t qrng_range32(qrng_ctx*, int32_t min, int32_t max)` | a generator | pattern is pure | integer (one `%` per draw) | per ctx | (a) idea only: threshold `(0u - range) % range` rejection |
| 7 | state-vector simulator | `quantum_state.c:19-550`, `quantum_gates.c:39-1021` (measure-all at 879-920) | e.g. `qs_error_t gate_hadamard(quantum_state_t*, int qubit)`, `gate_ry(quantum_state_t*, int, double)`, `gate_cnot(quantum_state_t*, int, int)`, `uint64_t quantum_measure_all_fast(quantum_state_t*, quantum_entropy_ctx_t*)` | libm, C99 `_Complex`, `simd_ops.c` (needs `-msse3`), `matrix_math.c`, `accelerate_ops.c`, heap | stateful (heap amplitudes + 1024-entry history) | `double _Complex` | no (per state only) | (c) reference only |
| 8 | SP 800-90B RCT / APT health tests | `src/health/health_tests.c:154-250` | `health_error_t health_test_rct(health_test_ctx_t*, uint8_t)`, `health_test_apt(...)` | heap window buffer, callback pointer | stateful | integer counters (cut-offs computed with `double`) | no | none (see section 9) |

Top two for the build-and-run requirement: #1-#4 (the mixers) and #5 (SHA-256).
#7 was also built once to measure float stability.

## 5. Test vectors (8 per mixer) - ran

Arithmetic: `uint64` mod 2^64, logical shifts. These lines are the output of the
**unmodified upstream source** (unity-included) and are byte-identical to the
output of the trimmed port on every build tried. sha256 of the 32-line file
(LF endings) `S:\tsotchke\quantum-rng\kat_upstream.txt`:
`9e68fb63b17a58e3eaa718e3542092a4d06a4e06c8a1b426f35ba0250b154686`.

```
KAT sm  x=0000000000000000 y=0000000000000000
KAT sm  x=0000000000000001 y=6b6a95bfe299a90e
KAT sm  x=0000000000000002 y=a8f4dd0d0a298554
KAT sm  x=ffffffffffffffff y=91f1c086b11cd9a4
KAT sm  x=0123456789abcdef y=61c901838c4f2390
KAT sm  x=9e3779b97f4a7c15 y=b9db92989b90862b
KAT sm  x=8000000000000000 y=b5c8bfadc54c5555
KAT sm  x=deadbeefcafebabe y=8d1f67dbebc0a568
KAT hm  x=0000000000000000 y=0000000000000000
KAT hm  x=0000000000000001 y=7590e4342b418e25
KAT hm  x=0000000000000002 y=a714723d6cb5500e
KAT hm  x=ffffffffffffffff y=bb6bfbb9d8393374
KAT hm  x=0123456789abcdef y=641058138d83f599
KAT hm  x=9e3779b97f4a7c15 y=3b070a0cab32d2eb
KAT hm  x=8000000000000000 y=8c93bcb506787c11
KAT hm  x=deadbeefcafebabe y=d44c2a8f54e0d3f4
KAT fin x=0000000000000000 y=0000000000000000
KAT fin x=0000000000000001 y=5b8f7e26a5c49390
KAT fin x=0000000000000002 y=ac3efc0d908fbb94
KAT fin x=ffffffffffffffff y=d2efaa796545315e
KAT fin x=0123456789abcdef y=b88ffa9517fb5ee4
KAT fin x=9e3779b97f4a7c15 y=c31c697f221ccb19
KAT fin x=8000000000000000 y=1f078aa4a1180ae3
KAT fin x=deadbeefcafebabe y=47b0db28a67b2648
KAT abs seed=empty      y=d3d274ff682c4f23
KAT abs seed='a'        y=8a42c1740a6980dc
KAT abs seed='abc'      y=1e55e6c3a884fc7e
KAT abs seed=00         y=68925e22a93f3b41
KAT abs seed=00..07     y=b69a51f7106e8c3d
KAT abs seed='fuzztest' y=530c56ceb0e133e1
KAT abs seed=00..1f     y=157b7f5810cd3efa
KAT abs seed=fox(43)    y=d571552d2dbff396
```

`sm` = upstream `splitmix64`; `hm` = `hadamard_mix`; `fin` = the `qrng_uint64`
output mix with `runtime_entropy = 0` (seeded mode); `abs` = `absorb_seed`.
Seeds: empty string; ASCII `a`; ASCII `abc`; one byte 0x00; bytes 00..07;
ASCII `fuzztest`; bytes 00..1f; ASCII "The quick brown fox jumps over the lazy dog".
The `fin` vectors are the port's; the port was checked against the real
`qrng_uint64` on 100 words (0 mismatches), not by a stand-alone upstream function
(upstream has none).

Definitions (from `tq_mix.h`, same operations as upstream):

```
sm(x):  x^=x>>30; x*=0xbf58476d1ce4e5b9; x^=x>>27; x*=0x94d049bb133111eb;
        x^=x>>31; x*=0xC13FA9A902A6328F; x^=x>>29
hm(x):  x=sm(x); x^=PX*(x>>12); x*=0x7297352743776A1B; x^=PY*(x>>25);
        x*=0x6955927086495225; x^=PZ*(x>>27); x*=0x91E10DA5C79E7B1D; x^=x>>13
fin(x): x=sm(x); x^=PX*(x>>27); x*=0xC13FA9A902A6328F; x^=PY*(x>>31);
        x*=0x91E10DA5C79E7B1D; x^=PZ*(x>>29)
abs(p,n): h = 0x6A09E667F3BCC908 ^ sm(n + G);
          for each byte b: h ^= b + G + (h<<6) + (h>>2); h = hm(h);
          return h ^ sm(n)
PX=0x4C957F2D8A1E6B3C  PY=0xD3E99E3B6C1A4F78  PZ=0x8F142FC07892A5B6  G=0x9E3779B97F4A7C15
```

## 6. Build and run record

All commands were run in `S:\tsotchke\quantum-rng\` (Git Bash). `up\` holds byte
copies of the upstream files; nothing was built inside `T:`.
Tools: gcc 16.1.0 (WinLibs, UCRT), clang 21.1.8, zig 0.17.0, node v25.8.2.

### 6.1 Port equals upstream (ran)

`kat.c` `#include`s the unmodified `up/src/quantum_rng/quantum_rng.c` so its
`static` functions can be called directly.

```
gcc -std=gnu2x -O2 -w -o kat.exe kat.c -lm && ./kat.exe | tail -9
CHECK prior-audit values reproduced: 19/19
CHECK port vs upstream sm/hm: 8388608 comparisons, 0 mismatches
CHECK port vs upstream absorb: 2408 comparisons, 0 mismatches
CHECK tq_fin vs qrng_uint64 (100 words): 0 mismatches
CHECK mix13(GOLDEN) == e220a8397b1dcdaf (1st SplitMix64 output, seed 0): yes
CHECK sm == mix13 + (mul HEISENBERG, xorshift 29), and sm_inv round trips (3 x 2^22): 0 failures
CHECK fixed point: sm(0)=0000000000000000 hm(0)=0000000000000000 fin(0)=0000000000000000
CHECK upstream hadamard_mix collision: hm(cbfb0aa09eee783d)=415993b11b797cd7 hm(437c47cd698729e3)=415993b11b797cd7 -> COLLIDE (not injective)
RESULT: PASS
```

(`-w` only silences warnings that come from the upstream file.)

### 6.2 Bit-portability of the port (ran)

`kat_port.c` uses only `tq_mix.h` (no upstream code, no libm; `nm -u kat_port.o`
lists only `__main` and `printf`).

```
gcc -std=c2x -O2 -Wall -Wextra -o kp_O2.exe kat_port.c          (no warnings)
gcc -std=c2x -O0 -Wall -Wextra -o kp_O0.exe kat_port.c
gcc -std=c2x -O3 -march=native -ffast-math -Wall -Wextra -o kp_O3n.exe kat_port.c
Z:/Tools/LLVM-21/bin/clang.exe -std=c2x -O2 --target=x86_64-w64-mingw32 -o kp_clang.exe kat_port.c
zig cc -target wasm32-wasi -O2 -std=c2x -o kp_wasm32.wasm kat_port.c
node --no-warnings run_wasi.mjs kp_wasm32.wasm        (dev tooling only: runs the 32-bit build)

kp_O2.txt == kat_upstream.txt
kp_O0.txt == kat_upstream.txt
kp_O3n.txt == kat_upstream.txt
kp_clang.txt == kat_upstream.txt
kp_wasm32.txt == kat_upstream.txt
```

Not run: `-m32` (this gcc has no 32-bit libs: `skipping incompatible ... libkernel32.a`),
UBSan (`cannot find -lubsan`), any big-endian or ARM target (no emulator). The
code uses only unsigned arithmetic and byte-wise input, so no endian or
signed-overflow dependence is expected, but that is reasoning, not a run.

### 6.3 Properties of the mixers (ran)

```
gcc -std=gnu2x -O2 -fopenmp -Wall -Wextra -o props.exe props.c && ./props.exe      (no warnings)
FIN step x^=PAULI_X*(x>>27): N=2^21 of 2^37 h values, g-collisions found=12 (random-function expectation 16.0), 0.22 s
  fin collision: u1=bfce418c957133f5 u2=a97dd05900650a0c fin(u1)=2d2d9922889f2ab2 fin(u2)=2d2d9922889f2ab2 CONFIRMED
  fin collision: u1=0292363f799fcd7a u2=aeee8321908afd11 fin(u1)=57dd50b7fb566958 fin(u2)=57dd50b7fb566958 CONFIRMED
HM step x^=PAULI_X*(x>>12): N=2^28 of 2^52 h values, g-collisions found=1 (random-function expectation 8.0), 6.14 s
  hm collision: u1=cbfb0aa09eee783d u2=437c47cd698729e3 hm(u1)=415993b11b797cd7 hm(u2)=415993b11b797cd7 CONFIRMED
AVAL mix13  random  mean flipped bits=32.0004 worst |p-0.5|=0.00546 (in bit 63 -> out bit 50)
AVAL sm     random  mean flipped bits=32.0015 worst |p-0.5|=0.00485 (in bit 60 -> out bit 62)
AVAL hm     random  mean flipped bits=32.0010 worst |p-0.5|=0.00509 (in bit 17 -> out bit 9)
AVAL fin    random  mean flipped bits=32.0002 worst |p-0.5|=0.00466 (in bit 19 -> out bit 11)
AVAL mix13  counter mean flipped bits=31.9986 worst |p-0.5|=0.00696 (in bit 16 -> out bit 34)
AVAL sm     counter mean flipped bits=32.0005 worst |p-0.5|=0.00648 (in bit 9 -> out bit 13)
AVAL hm     counter mean flipped bits=31.9977 worst |p-0.5|=0.00664 (in bit 8 -> out bit 39)
AVAL fin    counter mean flipped bits=32.0009 worst |p-0.5|=0.00676 (in bit 6 -> out bit 38)
STREAM mix13  key=0000000000000000 16 MiB: ones fraction=0.499990 byte chi2(255 dof)=262.91
STREAM sm     key=0000000000000000 16 MiB: ones fraction=0.500017 byte chi2(255 dof)=257.87
STREAM hm     key=0000000000000000 16 MiB: ones fraction=0.499964 byte chi2(255 dof)=247.84
STREAM fin    key=0000000000000000 16 MiB: ones fraction=0.499996 byte chi2(255 dof)=242.72
STREAM sm     key=0123456789abcdef 16 MiB: ones fraction=0.499965 byte chi2(255 dof)=268.41
STREAM hm     key=0123456789abcdef 16 MiB: ones fraction=0.499997 byte chi2(255 dof)=271.75
TIME mix13  chained 2.68 ns/call, independent 0.66 ns/call (...) [noisy, parallel load]
TIME sm     chained 3.67 ns/call, independent 0.85 ns/call (...) [noisy, parallel load]
TIME hm     chained 9.23 ns/call, independent 2.27 ns/call (...) [noisy, parallel load]
TIME fin    chained 8.53 ns/call, independent 2.37 ns/call (...) [noisy, parallel load]
TIME absorb 16 MiB: 77.4 MiB/s digest=7ba23e0a6ba18eaa [noisy, parallel load]
```

What these lines mean:

- **`sm` is a permutation of 2^64.** Every step is an xorshift or a multiply by
  an odd constant; `tq_sm_inv` inverts it exactly (2 x 2^22 round trips, 0 failures).
- **`hm` and `fin` are not injective.** Each contains steps of the form
  `x ^= C * (x >> k)` with an even `C`, which is not invertible. Writing
  `x = (h << k) | l`, the high bits of the step output are
  `g(h) = h ^ ((C*h) >> k)`; a birthday search over `g` finds `h1 != h2` with
  equal `g`, and pulling both back through `sm_inv` gives two different inputs
  with the same output. Three such pairs are printed above and the `hm` pair was
  re-checked with upstream's own `hadamard_mix` (section 6.1).
  Consequence: never use `hm`/`fin` where distinct inputs must give distinct
  outputs (IDs, permutations, a counter-mode generator that must not repeat).
  The `hm` search found 1 collision where a random function would give about 8;
  the sample (`h = 0..2^28-1`) is structured, so I make no estimate of how many
  output values `hm` can reach. Unknown.
- **Avalanche:** with 131,072 samples per cell the sampling floor for the worst
  of 4,096 cells is about 0.005; all four functions sit at that floor. At this
  sensitivity `sm`, `hm` and `fin` are indistinguishable from standard mix13.
  This is a weak test. No PractRand / TestU01 exists on this machine and none was run.
- **Stream check:** 16 MiB of `f(key + i*G)`; byte chi-square 243 to 272 against
  255 degrees of freedom. Sanity only.
- **Cost (noisy, parallel load):** `sm` is about 1.3x the cost of mix13, `hm`
  and `fin` about 3.5x. `absorb` runs one `hm` per input byte: about 77 MiB/s.

### 6.4 SHA-256 header (ran)

```
gcc -std=c2x -O2 -Wall -Wextra -o sha_kat.exe sha_kat.c && ./sha_kat.exe up/src/quantum_rng/quantum_rng.c   (no warnings)
SHA256 len= 0 e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 ok
SHA256 len= 3 ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad ok
SHA256 len=56 248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1 ok
SHA256 1e6 x 'a' cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0 ok
SHA256 file 22159 bytes 86dcf1c4836462f8fdfcdf63c39df0e0d52570786c471290d1ea97f250471bce
SHA256 64 MiB in 0.172 s = 372 MiB/s [noisy, parallel load]
RESULT: PASS
```

The expected digests are the FIPS 180-4 example values as I recall them; the
file digest equals coreutils `sha256sum` on the same file, which is an
independent check. Scalar code, no SHA-NI.

### 6.5 State-vector simulator: float stability (ran)

`sim_probe.c` runs upstream's unmodified `quantum_state.c` / `quantum_gates.c`
on 4 qubits (16 rounds of RY, RZ, H, CNOT, RX, then measure-all), with the
sampling source replaced by a deterministic counter (`tq_sm`), and prints a
digest of the raw amplitude bytes.

```
SRC="sim_probe.c up/src/quantum_rng/quantum_state.c up/src/quantum_rng/quantum_gates.c up/src/quantum_rng/matrix_math.c up/src/quantum_rng/simd_ops.c up/src/quantum_rng/accelerate_ops.c"
gcc -std=gnu11 -O2 -msse3 -w -I. -Iup/src/quantum_rng -o sim_O2.exe $SRC -lm
gcc -std=gnu11 -O2 -march=native -w -I. -Iup/src/quantum_rng -o sim_native.exe $SRC -lm
gcc -std=gnu11 -Ofast -ffast-math -funroll-loops -msse3 -w -I. -Iup/src/quantum_rng -o sim_ofast.exe $SRC -lm
gcc -std=gnu11 -O0 -msse3 -w -I. -Iup/src/quantum_rng -o sim_O0.exe $SRC -lm

== sim_O0     [round0 amp digest ca1d13a3aaacf342 norm 1.0000000000000004] ... digest over 16 rounds: baf6783e20b3dd73
== sim_O2     [round0 amp digest ca1d13a3aaacf342 norm 1.0000000000000004] ... digest over 16 rounds: baf6783e20b3dd73
== sim_native [round0 amp digest fcc3816d9a82ba3c norm 1.0000000000000004] ... digest over 16 rounds: 957e11e76e3fdf1e
== sim_ofast  [round0 amp digest ca1d13a3aaacf342 norm 1.0000000000000002] ... digest over 16 rounds: baf6783e20b3dd73
measured (all four builds): 15 0 13 0 0 12 9 1 15 13 13 12 3 9 14 15
sizeof(quantum_state_t)=88, amplitudes=256 bytes on heap
```

- Without `-msse3` the file `simd_ops.c` does not compile (`_mm_addsub_pd`).
- `-march=native` (FMA contraction on this CPU) changes the amplitude bits after
  the first round. The 16 sampled outcomes happened to agree here, but the state
  bytes do not, so a state hash over these amplitudes would depend on build
  flags. That breaks law 6 as the code stands.
- The gate code also calls libm `sin`/`cos` for every rotation (`gate_ry`,
  `quantum_gates.c:279-280`), which the prior audit showed is libm dependent.

## 7. Proposed integration

All names below are proposals. Nothing was added to `N:\` except this note.
Rename on import: the upstream names ("hadamard", "Pauli", "Planck") describe
nothing physical here and would read as theater (laws 4 and 7).

| Proposed C signature (engine side) | From | Where it runs | Causal role |
|---|---|---|---|
| `static inline uint64_t asn_mix64(uint64_t x);` (= `tq_sm`) | kernel 1 | Setup and Tier 2 (colony spawn, snapshot load). Not Tier 0. | Derives the Philox4x32 key for an entity or stream from `(world_seed, entity_id, stream_id)`, e.g. `k = asn_mix64(asn_mix64(world_seed + G) ^ (entity_id * G + stream_id))`; a different key changes every draw of that entity. Always add an odd constant first because `asn_mix64(0) == 0`. |
| `static inline uint64_t asn_unmix64(uint64_t y);` (= `tq_sm_inv`) | added here | Tooling only | Lets a debugger map a scrambled ID back to its counter. |
| `uint64_t asn_seed_digest(const uint8_t *p, size_t n);` (= `tq_absorb`) | kernel 3 | Tooling only (CLI: seed string or config blob -> 64-bit world seed) | Sets the world seed. About 77 MiB/s and strictly serial, so not for state hashing. |
| `void asn_sha256(const void *p, size_t n, uint8_t out[32]);` (upstream `sha256.h`, used as is) | kernel 5 | Tooling only (snapshot files, evidence receipts) | None inside the simulation; it makes "same seed gives same state" claims checkable from outside. |

Not proposed: `hm` and `fin` as stand-alone engine functions. They cost 3.5x
mix13, show no measured statistical gain over it, and are not injective.

Honest comparison: `asn_mix64` is standard mix13 plus one more round. I measured
no difference in avalanche between the two, and mix13 has far more public
analysis. If the engine already has a 64-bit finalizer for key derivation,
this adds nothing; if it has none, either works and both are 5 to 7 lines.
I did not search for published analysis of this exact 3-multiply variant.

Licence duty if any of it is imported: keep "Copyright (c) 2024 Tsotchke" and
the MIT permission notice with the code (header comment or a licence file).

## 8. What the REST API repo adds

`T:\mirrors\Quantum-RNG-API` = `server.js` (510 lines, Express 5 + swagger) +
`src/binding.cc` (341 lines, Node-API C++ addon) + `src/qrng_v3_bridge.c/.h`
(233 lines of C glue) + a vendored copy of `quantum_rng/src` + tests, Dockerfile,
`railway.toml`.

- Routes (server.js:173-441): `GET /`, `/v1/openapi.json`, `/v1/health`,
  `/v1/qrng/stats`, `/v1/qrng/verification`, `/v1/qrng/bytes/:count`,
  `/v1/qrng/number`, `/v1/qrng/range`, `/v1/qrng/range64`, `/v1/qrng/boolean`,
  `POST /v1/qrng/choice`.
- The addon builds only the v3 path (`binding.gyp` lists `quantum_rng_v3.c`,
  state, gates, Bell test, Grover, entropy pool, hardware entropy, health tests,
  profiling). It does not compile the legacy `quantum_rng.c`, so **none of the
  integer mixers audited above are reachable through the API**.
- Output is non-reproducible by design (hardware entropy XOR an 8-bit basis
  index), which also conflicts with law 6.
- It needs `npm ci` + node-gyp to run; not attempted (no installs allowed).
- New numeric content: none. The range/choice endpoints call the same C range
  functions. Reusable idea: none beyond what the library already has.

Verdict: **excluded_by_law** - it is a network API (law 3) written in
JavaScript (law 1). Nothing from it belongs inside the simulation or its tooling.

## 9. What is NOT usable, and why

1. **Legacy seeded stream (`qrng_init` / `qrng_bytes` / `qrng_uint64`)** - by the prior audit's count (not recounted) 1,008
   libm `sin`/`cos` calls per 128 output bytes on arguments up to ~8e18; the
   prior audit measured 7 different streams from one source. Violates law 6.
   About 5 MiB/s by the prior audit's measurement (not re-measured).
2. **`quantum_noise` (quantum_rng.c:39-58)** - the libm part itself. Excluded by the task focus.
3. **v3 generator (`quantum_rng_v3.c`)** - output = hardware entropy XOR a
   basis index; never reproducible. Violates law 6; calling it quantum
   randomness would violate law 7.
4. **`secure_rng`, `entropy/`, `hardware_entropy.c`** - RDSEED/RDRAND/OS entropy
   collection with a background thread. The simulation must not consume
   non-deterministic entropy.
5. **State-vector simulator as written (kernel 7)** - heap-allocated
   `double _Complex` amplitudes plus a 1,024-entry heap history per state,
   libm trig per rotation, built upstream with `-ffast-math`, and amplitude
   bits that change with `-march=native` (section 6.5). For use (c), a
   per-creature 2-6 qubit module would have to be rewritten as a fixed-size,
   fixed-point structure-of-arrays kernel with table-driven rotations. The gate
   loops are a few lines each (e.g. Hadamard scalar loop: `quantum_gates.c:139-150`), so
   this repo gives the idea and the stride-based indexing, not the code. Other
   Tsotchke repos (Moonlab) are the likelier source; not examined here.
6. **Health tests (kernel 8)** - they test an entropy *source*; a deterministic
   counter-based generator has nothing for them to monitor. No causal role.
7. **`hm` / `fin` as ID scramblers or as a counter-mode generator core** - not
   injective (section 6.3).
8. **`qrng_range32` as code** - one hardware divide per draw plus a rejection
   loop; fine for tooling, too slow for Tier 0 (a multiply-shift bounded draw
   avoids the divide). Kept as an idea only.
9. **Bell/CHSH test, Grover search, Metal/CUDA bridges, the `examples/` tree**
   (finance, games, "ml", network, crypto demos) - all are consumers of the
   generator or float demos on top of the simulator; `examples/ml/quantum_transformer.c`
   and `quantum_gan.c` were not opened (names alone put them outside law 3's
   spirit and outside this task's focus). `examples/games/terrain_generation.c`
   and `quantum_evolution.c` are float code driven by `qrng_double`; nothing integer to take.
10. **Physics-named constants** - the 64-bit constants are arbitrary odd/even
    numbers with physics labels; `QRNG_RYDBERG` and `QRNG_GOLDEN_RATIO` are the
    same value (quantum_rng.c:14,16). No physical meaning should be claimed.

## 10. Not verified

- Statistical quality beyond the weak avalanche and chi-square checks above.
- Period or cycle structure of `sm` iterated on itself (not needed for counter use).
- How many distinct outputs `hm` / `fin` can produce.
- Behaviour on big-endian or non-x86 hardware (only reasoning; x86-64 and wasm32 were run).
- The upstream test suites and the Node addon were not built or run.
- The libm-dependence results of the prior audit were not repeated.
- The FIPS and SplitMix64 reference values were recalled from memory, with one
  independent cross-check each (`sha256sum`; the SplitMix64 structure check).
- No prior-art search was done for the non-injectivity observation; it is
  reported as a measured property of this code, not as a new result.

## 11. Files in scratch (`S:\tsotchke\quantum-rng\`)

`tq_mix.h` (trimmed port), `kat.c`, `kat_body.h`, `kat_port.c`, `props.c`,
`sha_kat.c`, `sim_probe.c`, `run_wasi.mjs`, `kat_upstream.txt` (32 KAT lines),
`up\` (byte copies of the upstream sources used, plus `LICENSE`).
