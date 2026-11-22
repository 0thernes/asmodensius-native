# Tsotchke audit: classical_rng

Audit key `classical-rng`. Source read-only at `T:\mirrors\classical_rng`; working copy and
experiments at `S:\tsotchke\classical-rng\`. Everything marked "ran" was run on 2026-11-17 on the
project laptop while other agents were also running, so every timing is **noisy, parallel load**.

## 1. Repo facts

| Item | Value |
|---|---|
| HEAD | `b9c2a10_2026-07-14` (`git -C T:/mirrors/classical_rng log -1 --format=%h_%cs`) |
| History | 4 commits; first commit 2024-12-16, v2 rewrite 2026-07-14 |
| License | MIT License, "Copyright (c) 2024 Tsotchke" (`LICENSE`, 21 lines) |
| Main language | C11 (library, tests, examples). A React/Vite histogram viewer under `tests/visualization` is JavaScript. |
| Size | 66 tracked files, 2.5 MB on disk, git pack 114.91 KiB. Library core is 3 files, 615 lines: `src/game_rng/game_rng.c` (300), `src/crypto_rng/crypto_rng.c` (295), `src/common/status.c` (20). |
| Build files | `Makefile` (GNU make, not available here) and `CMakeLists.txt`; I compiled directly with gcc using the Makefile's own flags. |
| Dependencies | libc only; `-lbcrypt` on Windows for the OS entropy call. No libm use. |

Answers to the task's three focus questions:

1. **Which generators are implemented.** Exactly two.
   (a) `game_rng`: a xoshiro256** core seeded by SplitMix64, whose every output is additionally
   XOR-mixed with the remainders of dividing 200-digit fixed-point pi and e by state-derived 32-bit
   divisors, then passed through the SplitMix64 finalizer.
   (b) `crypto_rng`: a thin wrapper over the operating system generator (`BCryptGenRandom` on
   Windows), plus a deterministic Miller-Rabin primality test for `uint64_t`.
   There is **no Philox, no Threefry, no counter-based generator of any kind**.
2. **Deterministic and integer-only?** `game_rng` is deterministic and integer-only on its whole
   output path (the only floating-point line is the convenience `crng_game_rng_next_double`,
   `game_rng.c:298-300`). Ran: identical digest at `-O0`, `-O2` and `-O3 -march=native` (section 4).
   `crypto_rng`'s byte source is non-deterministic by design; its primality test is deterministic
   and integer-only.
3. **Statistical test code reusable on our Philox stream?** Only as counters. The repo's single
   statistical routine (`tests/test_utils/statistical_tests.c`, 56 lines) builds a 16-bucket
   low-nibble histogram and 64 per-bit one-counts; its only verdict is "no bucket is empty". Ran: it
   accepts the plain counter 0, 1, 2, ... as passing. The repo's own `docs/guarantees.md` says smoke
   histograms do not validate randomness. There is no chi-square threshold, no serial, runs, gap,
   birthday or avalanche test, and no PractRand / TestU01 / NIST harness.

## 2. Candidate kernels

Engine-use letters refer to the task list: (a) counter-based RNG or integer hash mixers,
(b) deterministic fixed-point or integer math kernels, (l) other.

| # | Kernel | File and lines | Signature | Deps | Pure / stateful | Float / integer | Thread safety | Engine use |
|---|---|---|---|---|---|---|---|---|
| 1 | SplitMix64 step and its finalizer ("avalanche") | `src/game_rng/game_rng.c:87-105`, constants `src/common/constants.h:7-10` | `static uint64_t crng_splitmix64(uint64_t *state)`; `static uint64_t crng_avalanche64(uint64_t value)` | none | finalizer pure; step mutates one `uint64_t` | integer | finalizer reentrant | (a) |
| 2 | Unbiased bounded draw by threshold rejection, and the overflow-free signed range on top of it | `game_rng.c:247-269`, `271-296` (same code over OS bytes at `crypto_rng.c:116-164`) | `crng_status crng_game_rng_uniform_u64(crng_game_rng *rng, uint64_t upper_bound, uint64_t *out)`; `crng_status crng_game_rng_range_i32(crng_game_rng *rng, int32_t minimum, int32_t maximum, int32_t *out)` | a 64-bit word source | stateful through the generator; the mapping itself is pure | integer | per-object | (a)/(b) |
| 3 | Deterministic Miller-Rabin for all `uint64_t` (7 fixed witnesses) with overflow-free modular add / multiply / power | `src/crypto_rng/crypto_rng.c:166-258` | `bool crng_crypto_is_prime_u64(uint64_t value)`; static helpers `crng_add_mod_u64`, `crng_mul_mod_u64`, `crng_pow_mod_u64` | none | pure | integer | reentrant | (b), tooling |
| 4 | Histogram and bit-count accumulator | `tests/test_utils/statistical_tests.c:7-39`, struct in `statistical_tests.h:9-13` | `void crng_test_analyze_distribution(crng_test_distribution *report, const uint64_t *values, size_t count)`; `int crng_test_distribution_has_coverage(const crng_test_distribution *report)` | none | pure on caller buffers | integer | reentrant | (l) tooling |
| 5 | Fixed-point big decimal divided by a 32-bit divisor with exact remainder (base 10^9 limbs), and the pi/e residue step built on it | `game_rng.c:19-85`, `113-143`; generator step `204-220` | `static uint32_t crng_fixed_decimal_div_small(const crng_high_precision_decimal *value, uint32_t divisor, crng_high_precision_decimal *quotient)`; `uint64_t crng_game_rng_next_u64(crng_game_rng *rng)` | none | stateful, 296-byte state object | integer | per-object, no globals | (b), reference only |

Notes on what each one really is:

- Kernel 1 is the standard public SplitMix64 / Stafford "Mix13" finalizer. The repo names the two
  multipliers `CRNG_CONSTANT_E` and `CRNG_CONSTANT_PI`, but `0xbf58476d1ce4e5b9` and
  `0x94d049bb133111eb` are the ordinary SplitMix64 constants, not digits of e or pi. Ran: seed 0
  gives the reference first output `e220a8397b1dcdaf`.
- Kernel 5 is the repo's distinguishing idea. In our terms it is decoration: it adds two 23-limb
  division loops per draw and the repo itself claims no period, equidistribution or stream
  independence for the result.

## 3. Exact build and run commands with output tails

Copy step (read-only source to scratch), then the repo's own tests with the Makefile's flags:

```
mkdir -p /s/tsotchke/classical-rng/repo   # then cp of include/, src/common, src/game_rng/game_rng.c,
                                          # src/crypto_rng/crypto_rng.c, tests/*.c|h, tests/test_utils/*, LICENSE
cd /s/tsotchke/classical-rng/repo
gcc -std=c11 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Iinclude \
    src/game_rng/game_rng.c src/crypto_rng/crypto_rng.c src/common/status.c \
    tests/test_classical_rng.c -lbcrypt -o ../build/crng_tests.exe && ../build/crng_tests.exe
```

Output (ran, no warnings):

```
classical_rng: 330090 checks passed
exit=0
```

```
gcc -std=c11 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Iinclude -Itests \
    src/game_rng/game_rng.c src/crypto_rng/crypto_rng.c src/common/status.c \
    tests/test_game_rng.c tests/test_utils/statistical_tests.c -lbcrypt \
    -o ../build/test_game_rng.exe && ../build/test_game_rng.exe
```

Output head (ran, exit 0):

```
{"generator":"game_rng","samples":32768,"buckets":[2003,2045,2102,2065,2043,2051,2082,2063,2033,2014,1978,2107,2079,2022,2019,2062],"one_bits":[16429,16478,...
```

Audit experiment `S:\tsotchke\classical-rng\exp_kernels.c` (written for this audit). It links the
repo's unmodified sources and contains a reference Philox4x32-10 so the kernels can be exercised on
the kind of stream the engine uses:

```
cd /s/tsotchke/classical-rng
gcc -std=c2x -O2 -Wall -Wextra -Irepo/include -Irepo/tests exp_kernels.c \
    repo/src/game_rng/game_rng.c repo/src/crypto_rng/crypto_rng.c repo/src/common/status.c \
    repo/tests/test_utils/statistical_tests.c -lbcrypt -o build/exp_O2.exe
./build/exp_O2.exe > build/out_O2.txt
```

Full output (ran, exit 0, no compiler warnings):

```
A philox4x32-10 KAT (3 vectors): match
B splitmix64(seed 0) = e220a8397b1dcdaf: match; bijection round-trip errors (2e6): 0
B avalanche64 SAC: mean flipped output bits per input bit x1000 = 31995 (ideal 32000), worst cell z^2 x100 = 1404 over 4096 cells, M=20000
B avalanche64(0) = 0000000000000000 (zero is a fixed point)
C philox SAC: mean flipped output bits per input bit x1000 = 64000 (ideal 64000), worst cell z^2 x100 = 1525 over 24576 cells (fail above 2500), M=4096
D philox  N=1048576: repo coverage=1 | chi2(15dof) x100=3110 pass=1 | worst monobit z^2 x100=487 pass=1
D game_rng N=1048576: repo coverage=1 | chi2(15dof) x100=1480 pass=1 | worst monobit z^2 x100=613 pass=1
D counter N=1048576: repo coverage=1 | chi2(15dof) x100=0 pass=1 | worst monobit z^2 x100=104857600 pass=0
D2 philox low-nibble chi2 over 64 keys, N=262144 each: mean x100=1507 (expected 1500), min x100=426, max x100=3148, above 25.00: 2 (expected about 3.2), above 37.70: 0
E bound=7 S=210000: threshold chi2(6dof) x100=334, lemire chi2 x100=419 (fail above 2246); max attempts seen (incl. bound 2^63+1)=20
E timing incl. one philox block (noisy, parallel load): threshold 8.0 ns, lemire 6.5 ns per draw
F primes below 2000000 by sieve: 148933; mismatches repo+fast vs sieve: 0; repo vs fast on 200000 random odd u64: 0
F timing on 2000 primes just above 2^63 (noisy, parallel load): repo shift-add 73573 ns, 128-bit product 4972 ns per call
G ns per call (noisy, parallel load): game_rng next_u64 135.5 | splitmix64 1.35 | philox4x32-10 block (128 bits) 6.43
G sizeof(crng_game_rng) = 296 bytes
H DET_HASH=30c4aee0884d2953
ALL CHECKS PASSED (0 failures)
```

Determinism across optimisation levels (same sources, `-O0` and `-O3 -march=native`), then
`grep -H "DET_HASH\|ALL CHECKS\|^G ns" build/out_O*.txt` (ran):

```
build/out_O0.txt:G ns per call (noisy, parallel load): game_rng next_u64 297.2 | splitmix64 1.63 | philox4x32-10 block (128 bits) 27.81
build/out_O0.txt:H DET_HASH=30c4aee0884d2953
build/out_O0.txt:ALL CHECKS PASSED (0 failures)
build/out_O2.txt:G ns per call (noisy, parallel load): game_rng next_u64 135.5 | splitmix64 1.35 | philox4x32-10 block (128 bits) 6.43
build/out_O2.txt:H DET_HASH=30c4aee0884d2953
build/out_O2.txt:ALL CHECKS PASSED (0 failures)
build/out_O3n.txt:G ns per call (noisy, parallel load): game_rng next_u64 137.4 | splitmix64 1.26 | philox4x32-10 block (128 bits) 4.69
build/out_O3n.txt:H DET_HASH=30c4aee0884d2953
build/out_O3n.txt:ALL CHECKS PASSED (0 failures)
```

What each line establishes:

- **A**: the reference Philox4x32-10 in the experiment reproduces three known-answer vectors
  (all-zero, all-ones, pi-digit inputs). The vectors were typed from memory of the Random123
  `kat_vectors` file, which is not on this machine; three independent 128-bit matches make a wrong
  recollection implausible, but the engine's own Philox should still be checked against the file.
- **B**: kernel 1 is a bijection (an inverse written for the test round-trips 2,000,000 values with
  0 errors), flips on average 31.995 of 64 output bits per flipped input bit, and has no
  input-bit/output-bit cell beyond 3.75 sigma in 4096 cells. It maps 0 to 0.
- **C**: the same avalanche test applied to Philox over all 192 input bits (128 counter + 64 key)
  and 128 output bits: mean 64.000, worst cell 3.9 sigma in 24576 cells. This test is mine, not
  the repo's.
- **D**: the repo's accumulator works on a Philox stream, but its verdict (`coverage=1`) is the
  same for Philox, for `game_rng`, and for a plain counter. The two integer thresholds I added
  (chi-square on the 16 buckets, 5-sigma monobit per bit) separate them: the counter fails
  monobit with z^2 = 1048576. The counter's chi-square of exactly 0 also shows the verdict needs a
  lower tail ("too uniform") check.
- **D2**: the single Philox chi-square of 31.10 in line D (p about 0.008) is an ordinary
  fluctuation: over 64 keys the mean is 15.07 against an expected 15, two keys exceed the 5 percent
  point (3.2 expected) and none exceeds the 0.1 percent point.
- **E**: kernel 2's threshold rejection, re-expressed over a counter-based source (attempt number
  is a private counter lane), is uniform at bound 7 and correct at the worst-case bound 2^63+1
  (at most 20 attempts seen in 100,000 draws). A Lemire multiply-shift variant measures slightly
  faster; the difference is inside the noise of a shared machine.
- **F**: kernel 3 agrees with a sieve on every integer up to 2,000,000, rejects the strong
  pseudoprimes 3215031751 and 3825123056546413051, and agrees with a 128-bit-product variant on
  200,000 random odd 64-bit values. As written it costs roughly 74 to 83 microseconds per 64-bit
  prime (two runs) because modular multiply is a 64-step shift-and-add loop; replacing that with
  one `unsigned __int128` product gives roughly 5 microseconds.
- **G**: `game_rng` costs roughly 135 to 147 ns per 64-bit draw at `-O2` (two runs), against
  roughly 6 to 9 ns for a whole 128-bit Philox block. Per output bit that is about 30 times slower.
- **H**: the digest covers 100,000 `game_rng` outputs plus Philox, mixer, bounded-draw and
  primality results; it is identical at all three optimisation levels.

## 4. Proposed integration

Nothing from this repo belongs in the engine as a generator. Three small pieces are worth having,
and all three are a few lines each, so "adopt" means "write our own copy of a public algorithm, with
this repo as a cross-check", not "vendor the library".

```c
/* core/rng/mix64.h  -  Tier 0 (every tick), pure, inline, integer.
 * Bijective 64-bit finalizer. Uses: derive a per-entity Philox key from (world seed, entity id),
 * hash a lattice cell for the spatial grid, fold words into the state hash that law 6 compares
 * across thread counts. Must not be fed 0 unkeyed: mix64(0) == 0 (ran), so always add an odd
 * constant or the world seed first. */
static inline uint64_t asm_mix64(uint64_t x);

/* core/rng/below.h  -  Tier 0 / Tier 1, pure, inline, integer.
 * Unbiased integer in [0, bound) from the Philox block addressed by (key, entity, tick, purpose).
 * 32-bit multiply-shift with rejection; a rejected word is replaced by the next word of the same
 * block, then by the next block on a private lane, so the number of attempts never shifts any
 * other draw. Uses: mutation site choice, neighbour choice, tie-breaks, colony lottery (Tier 2). */
static inline uint32_t asm_rng_below_u32(uint64_t key, uint64_t entity, uint32_t tick,
                                         uint32_t purpose, uint32_t bound);

/* tools/rngcheck/  -  tooling only, never linked into the simulation. Integer verdicts. */
typedef struct { uint64_t nibble[16]; uint64_t ones[64]; uint64_t n; } asm_rngtest_freq;
void asm_rngtest_freq_add(asm_rngtest_freq *r, const uint64_t *v, size_t n);
int  asm_rngtest_freq_verdict(const asm_rngtest_freq *r,      /* two-sided chi2 + 5-sigma monobit */
                              uint32_t *chi2_x100, uint32_t *worst_z2_x100);
int  asm_rngtest_sac_philox(uint32_t samples, uint32_t *worst_z2_x100); /* 192 x 128 avalanche */

/* tools/ or world setup (runs once, before tick 0)  -  integer, pure.
 * Uses: pick a prime table size or a prime stride for a full-cycle visiting order. Port with the
 * 128-bit product; the repo's shift-add multiply is only needed on compilers without __int128. */
bool asm_is_prime_u64(uint64_t n);
```

Where the repo's unbiased-range point matters to us: the repo documents that bounded rejection
"can consume multiple source words", which changes the call schedule of a sequential generator. With
a counter-based source that hazard disappears only if the retry index is private to the draw, which
is why `asm_rng_below_u32` takes a `purpose` lane and never advances shared state.

The `range_i32` trick (`game_rng.c:288`: compute the span in `int64_t`, add 1 in `uint64_t`) is the
right way to cover the full signed range without overflow and should be copied into our helper.

Suggested test additions for our Philox, in order of value per line of code:
known-answer vectors, the 192 x 128 avalanche matrix (line C), per-key chi-square repeated over
many keys with the count of exceedances compared to its expectation (line D2), and a
thread-count-invariance digest. None of these is a substitute for a real battery; PractRand and
TestU01 are not installed here and the repo does not contain them.

## 5. What is NOT usable, and why

- **`game_rng` as the engine generator.** It is a sequential, stateful generator: output i depends
  on i-1 calls having happened, so it cannot give the same state hash for any thread count without
  one 296-byte state per stream (296 MB for a million creatures). It has no jump or split function,
  and the repo states it makes no claim about period, equidistribution or independence of nearby
  seeds. Measured about 30 times slower per bit than Philox.
- **The pi/e residue engine (kernel 5).** Integer-exact and deterministic, but it has no causal role
  we could defend: the residues come from public constants, the repo says they are not entropy, and
  nothing shows the output is statistically better than the xoshiro core alone. I did not run a
  lesion (core with the residue step removed) so "no benefit" is unmeasured, not disproven.
- **`crng_secure_*` (OS entropy).** Non-deterministic by design; forbidden inside the simulation by
  law 6. For tooling, picking a fresh seed that is then logged is one `BCryptGenRandom` call and
  does not need this wrapper.
- **`crng_game_rng_next_double`.** Floating point; not for simulation state.
- **The statistical test as a test.** The verdict accepts a plain counter (ran). Only the counters
  are reusable, and they are 15 lines.
- **`tests/visualization` (React/Vite).** JavaScript development tooling that plots the JSON
  histogram; irrelevant to the engine and not examined beyond its file list.
- **`crng_crypto_random_prime_u64`.** Draws candidates from OS entropy, so it is non-deterministic;
  only the primality predicate under it is reusable.
- **Miller-Rabin inside a tick.** Even the faster variant is microseconds per call; setup or tooling
  only.
- **Not examined:** `examples/game/particle_system.c`, `examples/game/terrain_generation.c`,
  `examples/crypto/*`, the two CLI programs, the v1 compatibility headers, `CMakeLists.txt`, the CI
  workflow and most of `docs/` (only `README.md` lines 1-140 and `docs/guarantees.md` were read).
  `examples/shuffle.c` was read: it is a 10-element Fisher-Yates over kernel 2, nothing more.

## 6. Verdict

**reference_only.** The repo contains no counter-based generator and no statistical battery. Its
reusable content is three public textbook algorithms (SplitMix64 finalizer, threshold rejection,
64-bit Miller-Rabin) that we would rewrite in a few lines each, and it served as an independent
cross-check for those rewrites. MIT licence, so copying is permitted with the copyright notice if
we ever do take code verbatim.

Engine-use mapping: kernel 1 to (a); kernel 2 to (a)/(b); kernel 3 to (b), tooling; kernel 4 to
(l), tooling; kernel 5 to none.

No novelty is claimed for anything in this note; no prior-art search was run because nothing here
is new.
