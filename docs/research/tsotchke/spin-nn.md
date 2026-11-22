# Tsotchke audit: spin_based_neural_network (key: spin-nn)

Audit date 2026-11-17. Everything marked "ran" was run in this session in
`S:\tsotchke\spin-nn\`; nothing was built or changed inside `T:\`.
All timings: noisy, parallel load.

## 1. Repo facts

| Item | Value |
|---|---|
| Path | `T:\mirrors\spin_based_neural_network` |
| HEAD | `9e8f2d0_2026-05-01` (`git -C T:/mirrors/spin_based_neural_network log -1 --format=%h_%cs`) |
| License | MIT License, "Copyright (c) 2024-2026 tsotchke" (from `LICENSE`) |
| Main language | C11 (160 `.c`, 58 `.h`; also 77 `.json` result files, 38 `.md`, 7 `.sh`, 2 `.py` analysis scripts) |
| Size | 350 tracked files, about 24 MB tracked (27 MB with `.git`); `src` + `include` = 25,295 lines |
| Release | README says v0.5.0 (2026-05-01) |
| Build | GNU `Makefile` (GNU make is not on this machine; sources were compiled directly with gcc) |
| Vendored | `vendor/libirrep` is an unfetched git submodule (empty here) |

What the repo is: a double-precision research framework for quantum spin
systems (neural quantum states, DMRG/TEBD, topological invariants, toric
code, LLG micromagnetics). The classical-spin pieces relevant to us are
small side modules: `thermodynamic/hopfield.c`, `neuromorphic/pbit.c`,
`thermodynamic/rbm_cd.c`, and the 3D Ising lattice in `ising_model.c`.

## 2. Candidate kernels

| # | Kernel | File and lines | Signatures | Deps | Pure / stateful | Float / int | Thread safety | Engine use |
|---|---|---|---|---|---|---|---|---|
| 1 | Hopfield associative memory | `src/thermodynamic/hopfield.c` 21-134 (store 39-56, sync update 66-80, Metropolis sweep 82-105, recall 115-121, energy 123-134); header `include/thermodynamic/hopfield.h` 27-67 | `hopfield_t *hopfield_create(int num_spins)`; `int hopfield_store_patterns(hopfield_t*, const int *patterns, int num_patterns)`; `int hopfield_sync_update(hopfield_t*, int *spins)`; `void hopfield_metropolis_sweep(const hopfield_t*, int *spins, double beta, unsigned long long *rng_state)`; `int hopfield_recall(hopfield_t*, int *spins, int num_sweeps)`; `double hopfield_overlap(...)`; `double hopfield_energy(...)` | libc + libm (`exp`) | State in caller-owned struct; no globals. `sync_update` and the Metropolis sweep `malloc`/`free` a scratch array on every call | `double` N x N weights, `int` (32-bit) spins. Spins are integer-valued, weights are k/N | Re-entrant per network (no globals); not safe to share one `spins` array | (f) |
| 2 | p-bit Ising machine (heat-bath sampler + annealer) | `src/neuromorphic/pbit.c` 27-120 (sweep 47-70, energy 72-85, MaxCut couplings 87-106, anneal 108-120); header `include/neuromorphic/pbit.h` 30-69 | `void pbit_net_init(pbit_net_t*, int n, const double *J, const double *b, double beta, unsigned long long seed)`; `int pbit_sweep(pbit_net_t*, int *spins, int num_sweeps)`; `double pbit_ising_energy(const pbit_net_t*, const int *spins)`; `int pbit_maxcut_couplings(int n, int num_edges, const int *edges_u, const int *edges_v, const double *weights, double *out_J)`; `int pbit_anneal(pbit_net_t*, int *spins, double beta_start, double beta_end, int num_sweeps, double *out_final_energy)` | libc + libm (`exp`, `log`) | Stateful: RNG state lives in the net struct (sequential xorshift64); `malloc` per `pbit_sweep` call | `double` J, bias, beta; `int` spins | Re-entrant per net; the sequential RNG makes results depend on call order, so not thread-count independent | (f), (h) |
| 3 | RBM with contrastive divergence | `src/thermodynamic/rbm_cd.c` 39-198 (train step 106-149); header `include/thermodynamic/rbm_cd.h` 28-86 | `rbm_cd_t *rbm_cd_create(int num_visible, int num_hidden, double weight_scale, unsigned long long seed)`; `int rbm_cd_train_step(rbm_cd_t*, const int *v_data, int k_gibbs, double learning_rate)`; `void rbm_cd_sample_h_given_v(...)`; `void rbm_cd_sample_v_given_h(...)`; `double rbm_cd_free_energy(const rbm_cd_t*, const int *v)` | libc + libm | Stateful (weights and RNG in struct); four `malloc`s per train step | `double` throughout; binary {0,1} units | Re-entrant per RBM | (f) as a learning rule, reference only |
| 4 | 3D Ising lattice: Metropolis flip and Swendsen-Wang cluster step | `src/ising_model.c` 11-74 (init), 87-101 (energy), 138-155 (Metropolis flip), 171-245 (union-find + Swendsen-Wang) | `IsingLattice* initialize_ising_lattice(int, int, int, const char*)`; `double compute_ising_energy(IsingLattice*)`; `void flip_random_spin_ising(IsingLattice*)`; `int ising_swendsen_wang_step(IsingLattice*, double beta)` | libc + libm; **libc `rand()`** | Stateful; global `rand()` state | `int` spins in an `int***` pointer-of-pointer lattice; `double` probabilities | **Not** thread safe and not reproducible across C libraries (`rand()`) | (h), world fields; reference only |
| 5 | LLG spin dynamics (RK4 / Heun, unit-vector renormalise) | `src/llg/llg.c` 14-108; header `include/llg/llg.h` 23-55 | `int llg_rk4_step(const llg_config_t*, double *m, long num_sites)`; `int llg_heun_step(const llg_config_t*, double *m, long num_sites)`; `void llg_renormalize(double *m, long num_sites)`; field callback `void (*)(const double *m, double *b_eff, long num_sites, void *user)` | libc + libm (`sqrt`) | Pure given the callback; 4-6 `calloc`s per step | `double`, AoS xyz | Re-entrant | (i)/(l); reference only |

### How integer-friendly are they?

- **Hopfield (1): fully integer-izable, exactly.** The stored state is
  integer in disguise: `J_ij = (1/N) * sum_mu xi_i xi_j`, spins are +-1.
  Multiplying the local field by N gives an integer:
  `N f_i = sum_mu xi_i^mu * o_mu - K s_i`, with `o_mu = sum_j xi_j^mu s_j`.
  For N = 64 a pattern is one `uint64_t` and `o_mu = 64 - 2*popcount(xi^mu XOR s)`,
  so **no weight matrix is needed at all** (24 bytes for K = 3 instead of
  the repo's 32,768 bytes of doubles). I verified bit-for-bit equality with
  the repo kernel (section 3). This identity holds only for the pure Hebb
  rule with zero diagonal and zero bias; any per-weight plasticity (decay,
  Storkey rule) needs the matrix (int8 64 x 64 = 4 KB per creature).
- **p-bit (2): integer-izable with a threshold table.** `P(+1) = sigma(beta*h)`
  becomes `rand32 < thr[H]` where H is the integer field and `thr` is a
  391-entry `uint32` table (for N = 64, K = 3). The table must be a
  build-time constant (not computed with a runtime libm) to keep the state
  hash libm-independent. The repo's sequential xorshift64 must be replaced
  by Philox keyed on (creature, tick, site block).
- **RBM-CD (3): not integer-friendly as written.** Real-valued weights,
  mean-field sigmoid activations in the gradient, learning rate in double.
  A fixed-point port is possible (int16 weights, table sigmoid) but is a
  redesign, not a port.
- **Ising / Swendsen-Wang (4):** spins are integers but the data layout
  (`int***`), `rand()` and the serial union-find make it unusable as is.
  The algorithm is integer-friendly (bond probability as a `uint32`
  threshold; cluster labels are integers).
- **LLG (5):** inherently continuous (unit vectors, RK4). Fixed-point
  versions exist in principle; nothing here helps with that.

## 3. Build and run evidence

All commands were run from `S:\tsotchke\spin-nn` (Git Bash, gcc 16.1.0).
Copied sources: `src/hopfield.c`, `src/pbit.c`, `src/rbm_cd.c`,
`src/ising_model.c`, `src/llg.c` (read only, not built), the matching
headers under `include/`, and the repo's own `tests/harness.h` and four
test files.

### 3.1 The repo's own tests (ran)

```
for t in hopfield:hopfield pbit:pbit rbm_cd:rbm_cd ising_sw:ising_model; do n=${t%%:*}; s=${t##*:};
  gcc -std=c11 -D_GNU_SOURCE -O2 -Wall -Wextra -Iinclude -Isrc src/test_$n.c src/$s.c -o bin/repo_test_$n.exe -lm
  timeout 90 ./bin/repo_test_$n.exe | tail -6; done
```

Output tails (no compiler warnings were printed):

```
### test_hopfield
# Hopfield MC: <|m|> beta=4.0 -> 1.000, beta=0.01 -> 0.300
ok 4 - test_metropolis_samples_near_attractor
# Hopfield energies: stored=-15.500  neighbour=-13.562  random=-15.500
ok 5 - test_energy_minimum_at_stored_pattern
1..5
# passed: 5 / 5
### test_pbit
# MaxCut K3 annealed: spins=(1,1,-1)  E=-1.00 (min=-1, cut=2)
ok 3 - test_maxcut_triangle
# MaxCut Petersen annealed: cut = 11 / 15 edges  (optimum = 12)
ok 4 - test_maxcut_petersen
1..4
# passed: 4 / 4
### test_rbm_cd
# RBM trained hit rate: 0.996  (chance = 0.125)
ok 3 - test_samples_concentrate_on_training_patterns
ok 4 - test_null_args_return_error
1..4
# passed: 4 / 4
### test_ising_sw
# SW T=inf: <|m|> = 0.055 (expected near 0)
ok 2 - test_sw_demagnetises_at_high_temperature
# SW near T_c: E/N = -0.704 (between -3 and -0.5 expected)
ok 3 - test_sw_near_critical_temperature
1..3
# passed: 3 / 3
```

(The Petersen-graph anneal in the repo's own test reaches cut 11 of an
optimum 12 and still passes; the test tolerance is loose.)

### 3.2 Associative recall: 3 patterns in 64 spins, corrupted cue (ran)

Harness: `S:\tsotchke\spin-nn\assoc_recall.c` (mine). It links the repo's
`hopfield.c` and `pbit.c` unmodified and adds an integer port for comparison.

```
gcc -std=c2x -O2 -Wall -Wextra -Iinclude assoc_recall.c src/hopfield.c src/pbit.c -o bin/assoc_recall.exe -lm
timeout 90 ./bin/assoc_recall.exe
```

Output (complete, `gcc exit=0`, `run exit=0`):

```
== A. repo hopfield.c (double), N=64, K=3, cue = pattern 1 with 12 bits flipped ==
pattern 0 = 09f1fd9d03f0a9b4
pattern 1 = 553274161bbf8475
pattern 2 = 5d5bca4696b343b3
cross overlaps N*m: p0.p1=6 p0.p2=-2 p1.p2=8
cue      = d7317452599ba477  overlap with p1 = +0.6250  E = -11.0312
recalled = 553274161bbf8475  overlap with p1 = +1.0000  E = -31.2812  converged=1  exact=yes
== B. integer port (3 x uint64 patterns, popcount, no weight matrix) ==
recalled = 553274161bbf8475  N*overlap = +64  converged=1  exact=yes  same_as_repo=yes
cross-check vs repo hopfield_sync_update: 45700 sync steps, mismatches: per-site form 0, word-parallel form 0
== C. exact-recall rate vs flipped bits (2000 cues per level, 20 sync sweeps) ==
flips  repo_double  integer   mean_N*overlap(integer)
    0      1.0000    1.0000       64.00
    4      1.0000    1.0000       64.00
    8      1.0000    1.0000       64.00
   12      1.0000    1.0000       64.00
   16      0.9995    0.9995       63.98
   20      0.9785    0.9785       63.31
   24      0.8105    0.8105       56.98
   28      0.3510    0.3510       32.53
   32      0.0000    0.0000        0.08
integer-path state hash (FNV-1a over all recalled words) = a17a1d333e2277c0
== D. timing, one sync update of 64 spins (noisy, parallel load) ==
repo double (4096 doubles of J, malloc per call):   1146.0 ns/update
integer word-parallel (24 bytes of patterns)    :     37.8 ns/update (includes ~1 ns test RNG)
integer per-site reference                      :    101.5 ns/update
(checksum 16047107468141148676)
== E1. Philox4x32-10 known-answer test ==
ctr=0 key=0     -> 6627e8d5 e169c58d bc57ac4c 9b00dbd8 (expect 6627e8d5 e169c58d bc57ac4c 9b00dbd8)
ctr=ff.. key=ff -> 408f276d 41c83b0e a20bc7c6 6d5451fd (expect 408f276d 41c83b0e a20bc7c6 6d5451fd)
ctr=pi  key=pi  -> d16cfe09 94fdcceb 5001e420 24126ea1 (expect d16cfe09 94fdcceb 5001e420 24126ea1)
KAT PASS (expected values recalled from the Random123 kat_vectors file, not re-fetched)
== E2. repo pbit.c two-spin temperature check (J12=J21=1, beta=1, 200000 sweeps) ==
aligned/anti = 2.7284 ; header's stated law exp(beta*(1/2 sJs)) predicts e^2 = 7.3891 ; sigma(beta*h) heat bath predicts e^1 = 2.7183
== E3. repo pbit_anneal MaxCut, random graph n=16, 40 edges, vs brute force ==
brute-force optimum cut = 31 ; p-bit anneal (200 sweeps, 50 seeds): best 31, mean 30.90, optimum hit 46/50
== E4. finite-temperature recall from the 12-flip cue: repo pbit_sweep (double, xorshift64) vs integer heat bath (Philox, table) ==
beta   repo_mean_N*overlap  integer_mean_N*overlap   (400 runs each, 20 sweeps)
  1.0          1.83                  0.10
  2.0          1.72                  0.05
  4.0         58.78                 59.13
  8.0         63.88                 63.92
 16.0         64.00                 64.00
integer heat-bath state hash = d1aed1986c4ae065
== F1. lesion: same 12-flip cue, pattern 1 removed from the store (K=2) ==
intact  : recalled N*overlap with p1 = +64
lesioned: recalled = f60e0262fc0f564b  N*overlap with p1 = -6, p0 = -64, p2 = +2  converged=1
== F2. capacity of 64 spins: K random patterns, cue = 8 flipped bits, 20 sync sweeps, 40 pattern sets x 50 cues ==
  K  exact_recall  mean_N*overlap  stored_pattern_is_fixed_point
  1      1.0000         64.00      1.0000
  2      1.0000         64.00      1.0000
  3      1.0000         64.00      1.0000
  4      0.9995         63.99      1.0000
  5      0.9960         63.97      1.0000
  6      0.9775         63.81      1.0000
  7      0.9135         63.17      0.9571
  8      0.8360         61.99      0.9219
```

What this shows:

- **A.** The repo kernel, unmodified, stores 3 patterns in 64 spins and
  restores pattern 1 exactly from a cue with 12 of 64 bits flipped
  (overlap 0.625 -> 1.000) in a converged zero-temperature recall.
- **B.** The integer port gives the identical recalled word, and matches
  `hopfield_sync_update` on 45,700 synchronous steps from random and
  corrupted states with 0 mismatches (both the per-site form and the
  word-parallel form). For N = 64 the repo's double arithmetic is exact
  (1/64 is a power of two), so equality is expected, not luck.
- **C.** With 3 random patterns, exact recall is 100% up to 12 flipped
  bits, 97.9% at 20, 81% at 24, 35% at 28, 0% at 32 (as it must be: the
  cue is then equidistant from the pattern and its inverse).
- **D.** One update of all 64 spins: about 1.1 microseconds with the repo
  kernel versus about 38 ns (13 ns at `-O3 -march=native`, second run
  below) for the word-parallel integer form. Noisy, parallel load.
- **E2.** A documentation discrepancy in the repo: `pbit.h` line 13 states
  the sampler converges to `exp(beta*(1/2 s^T J s + b^T s))`, but the
  update `P(+1) = sigma(beta*h)` samples that law at **half** the stated
  beta (measured aligned/anti ratio 2.73 = e^1, not e^2 = 7.39). It is a
  convention slip, not a crash bug, but any temperature copied from the
  header would be off by a factor of 2.
- **E3.** The repo annealer finds the brute-force optimum MaxCut (31) on a
  random 16-node, 40-edge graph in 46 of 50 seeds at 200 sweeps.
- **E4.** The integer heat-bath port (Philox + threshold table, site-order
  sweeps) gives the same recall behaviour as the repo's double p-bit
  sweep in the ordered regime (beta 4 to 16). At beta 1 and 2 both are
  disordered; the 1.83 versus 0.10 gap at 400 runs is about 3 naive
  standard errors, and the recheck in 3.4 puts the repo value at
  -0.015 +- 0.18 over 4000 runs, so I treat it as sampling noise.
- **F1 (lesion).** Removing pattern 1 from the store changes the outcome:
  the same cue no longer returns pattern 1 (overlap -6) and instead falls
  into the **inverse of pattern 0** (overlap -64). The memory has a causal
  role, and inverse attractors are real and must be handled.
- **F2 (capacity).** Random patterns, 8 flipped bits: exact recall is
  >= 99.6% for K <= 5, 97.8% at K = 6, 91% at K = 7, 84% at K = 8. From
  K = 7 some stored patterns are no longer fixed points.

### 3.3 Determinism across optimisation levels (ran)

```
gcc -std=c2x -O0 -Iinclude assoc_recall.c src/hopfield.c src/pbit.c -o bin/assoc_recall_O0.exe -lm && ./bin/assoc_recall_O0.exe > out_O0.txt
gcc -std=c2x -O3 -march=native -Iinclude assoc_recall.c src/hopfield.c src/pbit.c -o bin/assoc_recall_O3.exe -lm && ./bin/assoc_recall_O3.exe > out_O3.txt
grep -H "hash\|cross-check" out_O0.txt out_O2.txt out_O3.txt; grep -h "ns/update" out_O3.txt
(strip the timing lines) cmp a.txt c.txt && cmp b.txt c.txt && echo "non-timing output identical ..."
```

```
out_O0.txt:integer-path state hash (FNV-1a over all recalled words) = a17a1d333e2277c0
out_O0.txt:integer heat-bath state hash = d1aed1986c4ae065
out_O2.txt:integer-path state hash (FNV-1a over all recalled words) = a17a1d333e2277c0
out_O2.txt:integer heat-bath state hash = d1aed1986c4ae065
out_O3.txt:integer-path state hash (FNV-1a over all recalled words) = a17a1d333e2277c0
out_O3.txt:integer heat-bath state hash = d1aed1986c4ae065
repo double (4096 doubles of J, malloc per call):    984.7 ns/update
integer word-parallel (24 bytes of patterns)    :     12.8 ns/update (includes ~1 ns test RNG)
integer per-site reference                      :     49.1 ns/update
non-timing output identical at -O0, -O2, -O3 -march=native
```

This is one compiler on one machine. It is not a cross-compiler or
cross-thread-count proof; the integer kernel has no floating point and no
shared state, so there is no mechanism for it to vary, but that was not
tested with clang or MSVC here.

### 3.4 Seed recheck for the E4 gap (ran)

`S:\tsotchke\spin-nn\seed_check.c` (mine): repo `pbit_sweep` on the
Hopfield J at beta = 1, 4000 runs per row.

```
gcc -std=c2x -O2 -Wall -Wextra -Iinclude seed_check.c src/hopfield.c src/pbit.c -o bin/seed_check.exe -lm
timeout 90 ./bin/seed_check.exe
```

```
cue flips = 12
consecutive (1000+run) seeds, 20 sweeps: mean N*overlap = -0.015  std = 11.15  naive SE = 0.176
consecutive (1000+run) seeds, 80 sweeps: mean N*overlap = -0.029  std = 10.85  naive SE = 0.172
splitmix-mixed         seeds, 20 sweeps: mean N*overlap = +0.100  std = 11.06  naive SE = 0.175
splitmix-mixed         seeds, 80 sweeps: mean N*overlap = +0.013  std = 11.01  naive SE = 0.174
```

No bias in the repo sampler and no visible damage from consecutive seeds
in this observable.

## 4. Proposed integration

Names are placeholders. The engine code would be new C23 (and later NASM),
written from the formulas below; the repo file is the reference the port
was checked against. MIT license: keep the copyright notice if any repo
text is carried over (the port in `assoc_recall.c` carries none).

### 4.1 Creature-level associative memory (candidate 1), use (f)

```c
/* K = 3 patterns of 64 spins per creature, SoA: three uint64 columns.
   One synchronous sign update of all 64 spins. Pure, branch-light, no
   memory beyond the arguments. Bit 1 = spin +1; field tie resolves to +1. */
uint64_t an_hop64_step3(uint64_t xi0, uint64_t xi1, uint64_t xi2, uint64_t s);

/* General K (1..8), per-site form; cost grows as 64*K. */
uint64_t an_hop64_step(const uint64_t *xi, int k, uint64_t s);

/* Up to max_sweeps sync updates; returns 1 if a fixed point was reached. */
int      an_hop64_recall(const uint64_t *xi, int k, uint64_t *s, int max_sweeps);
```

- **Where it runs:** Tier 1, phased every 32 to 64 ticks: the creature's
  64-bit sensory/context word is the cue, two or three sweeps are run, and
  the recalled word (and the integer overlap `64 - 2*popcount(recalled ^ cue)`
  as a confidence) feeds the decision stage. At about 13 to 38 ns per update
  (noisy) it would also fit Tier 0 for K = 3, but recall every tick has no
  obvious benefit.
- **Storage is the plasticity rule:** writing an experienced word into one
  of the K slots is exactly Hebbian storage (proved by the bit-for-bit
  match). Which slot is overwritten, and when a word is worth storing, are
  genome-controlled; that policy is ours to design and is not in the repo.
- **Memory:** 24 bytes per creature at K = 3 (24 MB per million creatures,
  streamed in L1-sized blocks). The repo layout (N x N doubles) would be
  32 KB per creature and is not usable.
- **Conservation / determinism:** pure integer function of its inputs; no
  RNG; identical for any thread count.
- **Lesion test available:** zero a slot and the recalled word changes
  (section 3.2, F1).

### 4.2 Stochastic recall and colony-level annealing (candidate 2), uses (f), (h)

```c
/* One asynchronous heat-bath sweep over 64 spins in site order.
   thr[H + 195] = floor(2^32 * sigma(beta*H/64)) is a build-time constant
   table per temperature level; randomness is Philox4x32-10(counter, key). */
uint64_t an_hop64_heatbath(const uint64_t *xi, int k, uint64_t s,
                           const uint32_t thr[static 391],
                           uint32_t key0, uint32_t key1, uint32_t sweep_ctr);

/* Colony-level p-bit annealer over n <= 64 binary decisions with int8
   couplings (a QUBO / MaxCut-style partition, e.g. task or territory split). */
uint64_t an_pbit64_anneal(const int8_t *J /* n*n */, const int16_t *bias, int n,
                          uint64_t s0, const uint32_t *thr_schedule, int sweeps,
                          uint32_t key0, uint32_t key1);
```

- **Where it runs:** `an_hop64_heatbath` in Tier 1 as an optional noisy
  recall (temperature as a genome or arousal parameter; the ordered regime
  starts between table beta 2 and 4 for K = 3, section 3.2 E4).
  `an_pbit64_anneal` at Tier 2 (colony level), a few hundred sweeps per
  colony decision; 16 Philox calls per 64-spin sweep.
- **Why not copy `pbit.c`:** sequential xorshift64 state (order dependent),
  `exp` at run time, `malloc` per call, Fisher-Yates visit order that costs
  extra random draws. The algorithm transfers; the code does not.
- **Open question (not tested):** whether colony decisions can be phrased
  as a small QUBO with a measurable benefit over a greedy rule. Without
  that, the annealer would be decoration and must not be added.

### 4.3 What stays out of the engine

Candidates 3, 4 and 5 are reference only (section 5). No part of this
repo is needed as tooling.

## 5. Not usable, and why

- **`hopfield.c` as written:** double weights (32 KB per 64-spin net),
  `malloc` in the update loop, 32-bit ints per spin, `exp` in the
  Metropolis sweep, sequential xorshift64. Roughly 30 to 90 times slower
  than the integer form here (noisy). Use the algorithm, not the file.
- **Hopfield limits that survive the port:** the inverse of every stored
  pattern is also an attractor (F1 landed on one); synchronous updates can
  enter 2-cycles (the recall returns "not converged"); capacity for 64
  spins is about 5 to 6 random patterns for reliable recall (F2), and real
  sensory words are correlated, so the usable capacity in the engine is
  unknown until measured there.
- **`pbit.h` temperature convention:** documented law is off by a factor
  of 2 in beta relative to the code (E2). Do not copy temperatures from
  the header.
- **RNG helpers (`xs_uniform`, `xorshift_unit`):** sequential xorshift64
  converted to double; fails law 6 (counter-based, thread-count
  independent). Nothing for use (a).
- **`rbm_cd.c`:** double weights and mean-field sigmoid gradients; four
  allocations per training step; no integer path. As an idea (a
  two-phase "data minus model" local learning rule) it is a known
  textbook method, not specific to this repo.
- **`ising_model.c`:** `int***` lattice (three pointer hops per spin),
  libc `rand()` (global state, differs between C libraries, not thread
  safe), `rand() / (double)RAND_MAX` probabilities, and the Metropolis flip
  hard-codes beta = 1. Swendsen-Wang is a correct idea for a world-field
  Ising sampler but the union-find is serial and data dependent.
- **`llg/` (LLG, exchange, demag):** double precision micromagnetics with
  physical constants (gamma = 1.76e11, dt = 1e-14 s) and 4 to 6 `calloc`s
  per step. No integer or fixed-point form; no role in the simulation.
- **NQS, MPS/DMRG/TEBD, Lanczos, Berry phase, toric code, Majorana,
  Fibonacci anyons, topological entropy (most of the 25 k lines):**
  double-complex research numerics for quantum many-body ground states.
  Not small per-creature modules, not deterministic integer code. Use (c)
  (2 to 6 qubit state vectors) and (e) (quantum geometric tensor) are not
  served by this repo; `qgtl_bridge.c` and `moonlab_bridge.c` are thin
  optional bridges to other repos, compiled out by default.
- **`qllm_bridge.*`: excluded by law.** It is a bridge to an LLM inference
  and training engine (`semiclassical_qllm`); disabled by default and of no
  use to us in any case.
- **`eshkol_bridge.*`, `noesis_bridge.*`, `engine_adapter.c`,
  `nn_backend.c`:** dormant scaffolding for external engines that are not
  present.
- **`matrix_neon.c`:** ARM NEON kernels; on x86-64 only the scalar
  fallback compiles. Nothing for use (g) on this machine.
- **`physics_loss.c` (heat, wave, Navier-Stokes, Maxwell "losses"):** PDE
  residual penalties for network training, not solvers. Nothing for use (i).
- **`neural_network.c`, `reinforcement_learning.c`, `quantum_mechanics.c`:**
  small double-precision demo code using `rand()`; not usable.
- **`vendor/libirrep` (SO(3) irreps, use (d)):** unfetched submodule, empty
  in this mirror; audit the libirrep mirror itself for that use.
- **`visualization*.c`:** SDL2 viewer; no rendering idea for use (k).
- **`scripts/*.py`:** two analysis scripts, tooling only, irrelevant.

## 6. Limits of this audit

- Only the five files in section 2 were read in full; the rest of the tree
  was classified from the README, headers, file list and targeted greps.
- The Philox known-answer values were recalled from the Random123 test
  vectors, not re-fetched (no web access was used); the implementation
  matched all three.
- No prior-art search was done. The pattern-space (popcount) form of the
  Hebbian Hopfield update is standard textbook algebra and is **not**
  claimed as new.
- Timings are single runs under parallel load on one machine.
- Capacity and recall numbers are for uniformly random patterns.

## 7. Verdict

**adapt.** The Hopfield and p-bit algorithms are directly useful for use
(f) (and (h) at colony level), and the repo code is a correct, tested
reference, but none of it can be used as is: it must be re-expressed as
integer, allocation-free, Philox-keyed kernels. The integer Hopfield port
in `S:\tsotchke\spin-nn\assoc_recall.c` is already checked bit-for-bit
against the repo. Everything else in the repo is reference only or not
applicable.
