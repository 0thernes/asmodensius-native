# Tsotchke audit: tiny-numerics (asteroids, simple_mnist, PINN, PIMC)

Audit date 2026-11-17. Key `tiny-numerics`. Scratch: `S:\tsotchke\tiny-numerics\`.
Everything marked **ran** was run in this session; the command and the tail of its
output are quoted. All timings are **noisy, parallel load** (other agents share the machine).
Nothing in `T:` was modified or built in place; sources were copied to scratch first.

## 1. Repo facts

| Repo | HEAD (`git log -1 --format=%h_%cs`) | License | Language | Size (tracked) |
|---|---|---|---|---|
| `T:/mirrors/asteroids` | `a84fa16_2024-12-25` | MIT (LICENSE file, "Copyright (c) 2024 Tsotchke") | C (POSIX terminal) | 4 files, 14.7 kB; `asteroids.c` 345 lines |
| `T:/mirrors/simple_mnist` | `1982424_2025-03-20` | MIT (LICENSE file, "Copyright (c) 2025 tsotchke") | C | 10 files, 61 kB; 1154 lines of `.c` |
| `T:/mirrors/PINN` | `8ec2936_2024-10-16` | **no LICENSE file**; README line 148 says MIT | C (+ one Python plot script) | 13 files, 36 kB; 491 lines of `.c` |
| `T:/mirrors/PIMC` | `39222e8_2024-10-09` | **no LICENSE file**; README says MIT | Java only (+ a PDF) | 5 files, 246 kB (PDF dominates); 358 lines of Java |

Tool versions used: gcc 16.1.0 (WinLibs UCRT), javac/java 21.0.10 (tooling only).

## 2. What each repo actually is

### 2.1 simple_mnist: the layer math (verified by gradient check)

One file does the work: `src/simple_mnist.c`. `neon_ops.c` is **not called** by the
network (only `mnist_loader.c` includes its header) and on x86 every function in it
takes the scalar fallback branch (`neon_available()` returned 0 here).

Network, float32, row-major weights `W[out*in]`:

```
h1 = relu(W1 x  + b1)        W1: 512 x 784
h2 = relu(W2 h1 + b2)        W2: 256 x 512
p  = softmax(W3 h2 + b3)     W3: 10 x 256   (softmax with max-subtraction)
loss = -log p[label]
```

Backward pass (`backward_pass`, lines 245-302), exact backprop, per sample, in place:

```
d3 = p - onehot(label)
d2 = (W3^T d3) .* [h2 > 0]
d1 = (W2^T d2) .* [h1 > 0]
Wk -= lr * (dk (x) a_{k-1} + 2e-5 * Wk)      bk -= lr * dk     (all deltas computed before any update)
```

Around it: init uniform in ±sqrt(2/fan_in) (hidden) and ±sqrt(2/(fan_in+fan_out)) (output),
biases 0; Fisher-Yates shuffle; 50 % chance of a ±2 pixel shift augmentation; cosine
annealing `lr = 0.01 * 0.5 * (1 + cos(pi * epoch/epochs))`; stop at 99 % test accuracy.
`BATCH_SIZE 128` is cosmetic: the update is plain per-sample SGD. RNG is libc `rand()`
seeded with `time(NULL)`, so no run is reproducible.
535 818 parameters = 2.04 MB of float32 per network.

The README claim of ">99 % accuracy" is **not verified**: the repo ships no data
(`download_mnist.sh` fetches it; downloads are forbidden here and law 3 forbids datasets).

### 2.2 PINN: there is no PDE solver

The name and README promise physics-informed training for Schrödinger, Maxwell, heat,
wave and Navier-Stokes. The code does not do that:

- Network: 2 inputs, 5 hidden, 3 outputs, doubles (`neural_network.h`).
- `train_neural_network` (lines 143-209) trains on **one hard-coded sample**
  `{1.0, 2.0} -> 1.0` for every epoch.
- `update_weights` (lines 106-141) is a plain delta rule on `target - output`. The
  "physics loss" value is computed, written to a log file and **never used** in the update.
  The hidden derivative is always `1 - tanh(h)^2` applied to the already-activated value,
  whatever activation was selected.
- The loss functions (`loss_functions.c` 17-93) are scalar formulas on single numbers:
  no spatial grid, no collocation points, no derivative of the network with respect to
  its inputs. The "heat equation loss with Crank-Nicolson scheme" is
  `(alpha * (u - 2*u_t + (u - u_t))/dx^2 - (u - u_t)/dt)^2` with
  `alpha = K_B / (ETA * ELECTRON_MASS)`: not a discretised PDE.
- `adaptive_normalization(&loss, 1)` reads `loss` inside its own initialiser
  (uninitialised read, undefined behaviour) in the Schrödinger, heat and wave losses.
- CLI parameters `--thermal_conductivity`, `--wave_speed` are parsed and ignored.

Evidence (**ran**): training with `--loss heat` and with `--loss wave` gives a byte-identical
weight file, and the logged heat loss changes with the optimisation level (3.4e28 at -O0,
7.1e37 at -O2), which is the undefined behaviour showing.

### 2.3 PIMC: the algorithm, precisely (Java, `PIMC.java`)

Intent: primitive-action path-integral Monte Carlo for one particle in 1D, units
hbar = m = omega = 1. The imaginary-time path is a ring of `N` beads `x[0..N-1]`
(`x[N]` mirrors `x[0]`), `dtau = tau/N`, defaults N = 200, tau = 10, delta = 1.

Potentials (`V`, `VTrial`, lines 87-121): 1 harmonic `x^2/2`; 2 Morse `(1-e^-x)^2`;
3 double well `x^4/4 - x^2/2`; 4 anharmonic `x^4 + x^2/2`.

`changePath()` (lines 175-226), one call = N+1 single-bead Metropolis trials:

```
j  = round(U * (N-1))                        // site; ends get half weight, N-1 max
xt = x[j] + delta * (2U - 1)                 // symmetric uniform proposal
dE = 1/2 * [ ((x[j+1]-xt)/dtau)^2 + ((xt-x[j-1])/dtau)^2 + V(xt)
           - ((x[j+1]-x[j])/dtau)^2 - ((x[j]-x[j-1])/dtau)^2 - V(x[j]) ]   // j=0 uses x[N-1] on the left
accept if dE < 0 or U <= exp(-dtau * dE)
histogram x[j] into bins of width 0.1 -> pdist, pdistNorm, Psi0 = sqrt(pdistNorm/(dx*2pi))
```

Correct algorithm to re-implement (what I built and verified, section 4.2):

```
S(x) = sum_j (x[j+1]-x[j])^2 / (2 dtau) + dtau * V(x[j])        (periodic)
for each bead j (even beads, then odd beads: neighbours are frozen, so order-free):
    xt = x[j] + delta*(2U-1)
    dS = [(x[j+1]-xt)^2 + (xt-x[j-1])^2 - (x[j+1]-x[j])^2 - (x[j]-x[j-1])^2]/(2 dtau) + dtau*(V(xt)-V(x[j]))
    accept if dS <= 0 or U < exp(-dS)
optional centroid move: shift all beads by d; dS = dtau * sum_j (V(x[j]+d) - V(x[j]))
observables: <x^2>; virial energy E = <V(x) + x V'(x)/2>  (harmonic: E = <x^2>)
check: harmonic <x^2> = (1/N) sum_k 1/[(2-2cos(2 pi k/N))/dtau + dtau]
```

Defects found in the Java (each one confirmed by the Java run or the faithful C port, section 4.3):

1. **Potential weighted by 1/2.** `V` sits inside the `1/2 * [...]` bracket, so the chain
   samples `V/2`. For the harmonic case that is omega_eff = 1/sqrt(2) and
   `<x^2>` near 0.708 instead of 0.500.
2. **Bead 0 is almost frozen.** In the `else` branch `x[0] = x[N]` copies the stale mirror
   back over an accepted move of bead 0 on the next trial. Bead 0 only really moves when
   site 0 is drawn twice in a row. Measured: x[0] ended a sweep changed in 659 of
   400 000 sweeps (Java) and 621 of 400 000 (C port).
3. **The walker code is dead.** `int M = N/5` is a field initialiser evaluated while
   N == 0, so M = M0 = 0 and `walk()` (a diffusion-Monte-Carlo birth/death step) never
   iterates. Measured: `M = 0, M0 = 0, pref = 0, pave = 0` after 400 000 sweeps.
4. **`E0` is not an energy estimator.** `E0 = pdistNorm[bin] * T(s,bin) * V(s,bin)` indexes
   the path by a histogram bin. Measured value after 400 000 sweeps: 0.000019
   (the harmonic ground-state energy is 0.5).
5. `initialize()` calls `V(i, s)` with the arguments swapped; site choice by `Math.round`
   under-samples the ends; `Math.random()` is unseedable, so no run is reproducible.
6. `PIMCApp.java` needs the Open Source Physics GUI library (not installed, not needed).

### 2.4 asteroids

A terminal game (termios raw mode, ANSI redraw at 20 FPS, 80x24 char buffer). The numeric
content is two textbook integer rasterisers (Bresenham line, midpoint circle), a float
toroidal wrap and a float circle-circle test with `sqrt`. It does not build on this
toolchain (**ran**, section 4.4): `termios.h` and `sys/select.h` do not exist in mingw-w64.

## 3. Candidate kernels

| # | Kernel | File and lines | Signature | Deps | Pure / stateful | Number type | Thread safety | Engine slot |
|---|---|---|---|---|---|---|---|---|
| 1 | Ring-polymer Metropolis sweep (PIMC) | `PIMC/PIMC.java` 175-226 (+ potentials 87-121) | `void changePath()` on instance fields | `Math.random`, `Math.exp`, `Math.pow` | stateful object, global RNG | double | not safe, not reproducible | (h) Monte Carlo sampler |
| 2 | MLP forward | `simple_mnist/src/simple_mnist.c` 168-220 | `void forward_pass(Network*, const float *input, float *hidden1, float *hidden2, float *output)` | `expf` | pure given caller buffers (reads net) | float32 | safe for concurrent inference | (g) GEMV |
| 3 | MLP backprop + SGD + L2 | same file 245-302 | `void backward_pass(Network*, const float *input, const float *hidden1, const float *hidden2, const float *output, int label, float learning_rate)` | none | mutates the network in place; 3 kB of stack arrays | float32 | not safe on a shared net | (g) / lifetime plasticity reference |
| 4 | Fallback GEMV / GEMM (i-k-j) | `simple_mnist/src/neon_ops.c` 79-114, 29-41 | `void neon_matrix_vector_multiply(const float *A, const float *x, float *y, int M, int N)`; `void neon_matrix_multiply(const float *A, const float *B, float *C, int M, int N, int K)` | `memset` | pure | float32 | safe | (g), trivial |
| 5 | Bresenham line / midpoint circle | `asteroids/asteroids.c` 51-63, 66-90 | `void drawLine(int x1, int y1, int x2, int y2)`; `void drawCircle(int centerX, int centerY, int radius)` | global `display[24][80]` | stateful (global sink) | int | not safe as written | (b) integer kernel, perception line-of-sight |
| - | NEON exp2 polynomial | `neon_ops.c` 166-204 | `static float32x4_t exp_ps(float32x4_t)` | `arm_neon.h` | pure | float32 | safe | idea only, cannot be compiled on x86 |
| - | PINN delta rule and losses | `PINN/src/neural_network.c` 85-141, `loss_functions.c` 17-93 | `void forward_pass(NeuralNetwork*, double input[2], double output[3], ActivationFunction)` | `exp`, `tanh`, `pow` | forward writes `nn->hidden_outputs` | double | not safe on a shared net | none |

Categories (a) RNG, (c) state-vector simulation, (d) SO(3), (e) quantum geometric tensor,
(f) Ising/Hopfield, (j) codecs: **nothing** in these four repos. (i) PDE solver: **nothing
usable** (PINN, see 2.2). (k) rendering: ASCII rasterisers only, no skin or shader idea.

## 4. Build and run evidence

All commands were run from `S:\tsotchke\tiny-numerics` (Git Bash).

### 4.1 simple_mnist build + layer-math test (**ran**)

```
gcc -Wall -Wextra -O3 -march=native -Isrc_mnist/include -o build/simple_mnist.exe \
    src_mnist/src/simple_mnist.c src_mnist/src/mnist_loader.c src_mnist/src/neon_ops.c -lm
./build/simple_mnist.exe ./no_such_dir
```
```
(one warning: unused parameter 'use_neon'; build/simple_mnist.exe 86624 bytes)
Error: Failed to open ./no_such_dir/train-images-idx3-ubyte
Failed to load MNIST dataset
```

The trainer itself was not run: no data. The unmodified layer functions were linked into
my harness `mnist_layer_test.c` (gradient check, synthetic learning task generated with
Philox, fallback GEMV comparison):

```
gcc -O3 -march=native -Wall -Wextra -Isrc_mnist/include -I. -Dmain=mnist_main -c \
    -o build/simple_mnist_lib.o src_mnist/src/simple_mnist.c
gcc -O3 -march=native -Wall -Wextra -Isrc_mnist/include -I. -o build/mnist_layer_test.exe \
    mnist_layer_test.c build/simple_mnist_lib.o src_mnist/src/mnist_loader.c src_mnist/src/neon_ops.c -lm
./build/mnist_layer_test.exe
```
```
gradient check (central difference eps=1e-2, 5 largest-|g| entries per tensor)
  W1 (512x784)  analytic +0.064256 numeric +0.064261   worst rel.err of 5 = 6.38e-05
  b1            analytic +0.064273 numeric +0.064277   worst rel.err of 5 = 6.28e-05
  W2 (256x512)  analytic +0.181407 numeric +0.181402   worst rel.err of 5 = 1.60e-05
  b2            analytic +0.112768 numeric +0.112767   worst rel.err of 5 = 2.23e-05
  W3 (10x256)   analytic -0.687072 numeric -0.687067   worst rel.err of 5 = 7.03e-06
  b3            analytic -0.931617 numeric -0.931618   worst rel.err of 5 = 1.16e-05
synthetic 10-class test: accuracy before 0.158 -> after 300 SGD steps 1.000 ; mean CE loss first10 2.3243 -> last10 0.2340
neon_available() = 0 ; neon_matrix_vector_multiply (fallback) vs forward_pass layer 1: max |diff| = 0
parameters = 535818 floats = 2.04 MB ; forward 219.9 us/sample ; forward+backward 371.7 us/sample (noisy, parallel load)
```

Reading: the backprop is correct (relative error 1e-5 class, float32 limit), it learns a
synthetic 10-class task, and the scalar forward pass costs about 0.4 ns per multiply-add
(535 k MACs in 220 us, noisy), which is what a scalar float dot product gives; I did not
inspect the generated assembly.

### 4.2 PIMC: C re-implementation (**ran**)

Files: `philox.h` (Philox4x32-10 written from the published algorithm), `pimc_ring.c`
(A: line-by-line port of the Java including its defects; B: corrected double version;
C: integer-only version, Q12 positions, exact integer action difference, exp table built
by an integer recurrence, no libm in the hot path).

```
gcc -std=c2x -O2 -Wall -Wextra -o build/pimc_ring.exe pimc_ring.c -lm      (no warnings)
./build/pimc_ring.exe
```
```
philox4x32-10 KAT zero : 6627e8d5 e169c58d bc57ac4c 9b00dbd8 (expect 6627e8d5 e169c58d bc57ac4c 9b00dbd8)
philox4x32-10 KAT ones : 408f276d 41c83b0e a20bc7c6 6d5451fd (expect 408f276d 41c83b0e a20bc7c6 6d5451fd)
philox4x32-10 KAT pi   : d16cfe09 94fdcceb 5001e420 24126ea1 (expect d16cfe09 94fdcceb 5001e420 24126ea1)
exp table: e[0]=4294967295 e[64]=1580030168 (exp(-1)*2^32 = 1580030168.7) e[1536]=0
A java_faithful  N=200 tau=10 delta=1 sweeps=400000
  <x^2> all beads = 0.8437 +- 0.0442   mid beads = 0.6838 +- 0.0454
  exact ring, curvature 1/2 (what the Java action samples) = 0.7082 ; curvature 1 (intended) = 0.4999
  acceptance = 0.252 ; sweeps in which x[0] ended changed = 621 of 400000 ; x[0] init=0.2346 final=0.9720
B ring_double    N=200 tau=10 delta=1 sweeps=100000
  <x^2> = 0.4769 +- 0.0165   exact discretised = 0.4999   continuum 0.5*coth(tau/2) = 0.5000   z = -1.39   acc = 0.252
B ring_double    N=64 tau=8 delta=1 sweeps=1000000
  <x^2> = 0.4997 +- 0.0025   exact discretised = 0.4994   continuum 0.5*coth(tau/2) = 0.5003   z = +0.12   acc = 0.389
C ring_fixed     P=64 dt=1/8 (tau=8) Q12 integer, sweeps=1000000
  <x^2> = 0.5000 +- 0.0019   exact discretised = 0.4994   z = +0.35   acc = 0.389
  state hash after 1000000 sweeps = f9a73af8b1e3b6e2
C ring_fixed     P=64 dt=1/8 (tau=8) Q12 integer, sweeps=1000000
  <x^2> = 0.4975 +- 0.0018   exact discretised = 0.4994   z = -1.06   acc = 0.389
  state hash after 1000000 sweeps = 14fc96bc8479bf95
C ring_fixed decision element: V = x^4/4 - x^2/2 - h*x, P=64, 64 rings x 4000 sweeps each
    tilt h  P(centroid>0)   flips/ring
     -0.20         0.2176       491.06
     -0.10         0.3482       678.53
     -0.05         0.4239       746.45
     +0.00         0.4974       777.95
     +0.05         0.5753       747.70
     +0.10         0.6472       681.62
     +0.20         0.7755       502.53
bench: 512 rings x 2000 sweeps x 64 beads, scalar, 1 thread: 19.5 ns per bead update, 1.25 us per ring sweep (noisy, parallel load); hash 7e9ea8b8285c8c9f
```

Reproducibility (**ran**): a second run gave byte-identical output on all 27 non-timing
lines, and a `-O0` build (`gcc -std=c2x -O0 -o build/pimc_ring_O0.exe pimc_ring.c -lm`)
printed the same three integer hashes (`f9a73af8b1e3b6e2`, `14fc96bc8479bf95`,
`7e9ea8b8285c8c9f`). Thread-count independence was **not run**; it follows by construction
(each ring update is a function of ring state, seed, ring id and sweep number only).

Reading:
- The three Philox known-answer vectors were quoted from memory of the Random123 test file
  and all three matched; treat that as strong but not independently sourced confirmation.
- The integer sampler reproduces the exact discretised harmonic value 0.4994 within
  1.1 standard errors on two seeds. Error bars are block estimates (20 blocks) and are
  rough.
- The N = 200 double run (z = -1.39) is consistent but poorly converged: single-bead
  moves relax a 200-bead ring slowly. Do not read more into it.
- The tilted double well behaves as a graded two-choice element: P(centroid > 0) rises
  monotonically from 0.22 to 0.78 as the tilt goes from -0.2 to +0.2.

### 4.3 PIMC: the real Java, headless (**ran**, tooling only)

`PIMC.java` was compiled unmodified with my `Driver.java` (same defaults as `PIMCApp.reset()`):

```
javac -encoding UTF-8 -d build/java src_pimc/PIMC/PIMC.java src_pimc/PIMC/Driver.java
java -cp build/java PIMC.Driver
```
```
JAVA PIMC N=200 tau=10.0 delta=1.0 sweeps=400000
  <x^2> all beads = 0.6860 +- 0.0330   mid beads = 0.6338 +- 0.0300
  walkers M = 0, M0 = 0, pref = 0.0000, pave = 0.0000, E0 field = 0.000019, mcs = 400000
  sweeps in which x[0] ended changed = 659 of 400000 ; x[0] init=-0.9345 final=1.3200
```

Java gives `<x^2>` of 0.63 to 0.69, the faithful C port 0.68 to 0.84; the intended answer
is 0.50 and the answer for the half-weighted potential is 0.71. Both implementations sit
well above 0.50 (4 to 8 quoted standard errors), which supports defect 1. The two do not
agree tightly with each other or with 0.71: the nearly frozen bead 0 makes these runs
history dependent and the quoted errors are underestimates.

### 4.4 PINN and asteroids (**ran**)

```
gcc -o build/pinn.exe src_pinn/src/main.c src_pinn/src/neural_network.c src_pinn/src/loss_functions.c \
    src_pinn/src/utils.c -Isrc_pinn/include -Wall -Wextra -lm
gcc -o build/test_loss_functions.exe src_pinn/tests/test_loss_functions.c src_pinn/src/loss_functions.c -Isrc_pinn/include -Wall -Wextra -lm
gcc -o build/test_neural_network.exe src_pinn/tests/test_neural_network.c src_pinn/src/loss_functions.c \
    src_pinn/src/neural_network.c src_pinn/src/utils.c -Isrc_pinn/include -Wall -Wextra -lm
./build/test_loss_functions.exe ; ./build/test_neural_network.exe
(cd run_pinn/heat && ../../build/pinn.exe --loss heat --thermal_conductivity 1.0 --activation tanh --epochs 200 --learning_rate 0.01; tail -2 log_heat.txt)
(cd run_pinn/wave && ../../build/pinn.exe --loss wave --wave_speed 1.0 --activation tanh --epochs 200 --learning_rate 0.01; tail -2 log_wave.txt)
cmp run_pinn/heat/model_parameters.txt run_pinn/wave/model_parameters.txt && echo IDENTICAL
```
```
Schrödinger Loss: 2600000000.002500
Maxwell Loss: 12755648402316714835968.000000
Heat Equation Loss: 17529577408457664777595492770726805504.000000
Wave Equation Loss: 0.009615
Navier-Stokes Loss: 171.822653
Neural Network Initialized and Validated.
Forward Pass Output: -2.667001
Epoch 199: Loss:  34023717983073895459699294208.00000, Validation Loss: 1152385106436597542050378272750113390592.00000   (heat)
Epoch 199: Loss:  7195.13272, Validation Loss: 225368151.51939                                                          (wave)
IDENTICAL
```
Same heat run from an `-O2` build of the same sources: `Epoch 199: Loss: 70965243537353748893057945706759716864.00000`
(different from the -O0 value above: undefined behaviour). The tests only print; they assert nothing.

```
gcc -std=gnu11 -O2 -Wall -Wextra -o build/asteroids.exe src_asteroids/asteroids.c -lm
```
```
src_asteroids/asteroids.c:8:10: fatal error: termios.h: No such file or directory
compilation terminated.
```

The two integer rasterisers were lifted (stepping logic unchanged, pixel sink replaced by an
occupancy lookup) into `los_test.c`:

```
gcc -std=c2x -O2 -Wall -Wextra -o build/los_test.exe los_test.c && ./build/los_test.exe
```
```
LOS (10,10)->(70,10) through wall : first hit after 30 steps (expect 30)
LOS (10,2)->(70,2) above wall     : -1 (expect -1 = clear)
LOS (10,10)->(39,20) short of wall: -1 (expect -1)
LOS (70,10)->(10,10) reversed     : first hit after 30 steps (expect 30)
LOS (75,10)->(85,10) wraps torus  : -1 (expect -1)
LOS (75,10)->(85,10) wrap, blocker at x=2: first hit after 7 steps (expect 7)
ring_count centre (37,10) r=4: 6 occupied ring cells (wall crosses the ring; duplicates at octant seams count twice)
ring_count centre (20,10) r=4: 0 (expect 0)
```

## 5. Proposed integration

These are proposals. None has been run inside the engine; each needs a lesion experiment
before it earns a place (law 4).

### 5.1 Integer ring Metropolis sampler (from PIMC), category (h)

```c
/* P beads of Q12 int32 per unit, structure-of-arrays; P is a power of two.
   Counter = (tick, bead, unit_id, move_kind), key = (world_seed, domain tag). */
void an_ring_sweep_q12(int32_t *restrict x, uint32_t P, int32_t tilt_q12,
                       uint32_t world_seed, uint32_t unit_id, uint32_t tick);
int  an_ring_centroid_sign(const int32_t *x, uint32_t P);      /* the read-out */
void an_exp_table_init(uint32_t table_q32[1537]);              /* integer recurrence, once at start */
```

- **Where:** Tier 1, phased every 32 to 64 ticks, as a stochastic bistable decision or
  short-term memory element per creature (tilt = integer evidence from the creature's own
  network; read-out = sign of the centroid). Alternative: Tier 2, one ring per colony.
- **Why it could be causal:** the read-out is a graded, noisy function of the tilt
  (section 4.2 table) and it holds its state between sweeps (hysteresis from the barrier).
  Lesions to run: (1) freeze the ring, (2) P = 1, i.e. a plain noisy bistable unit.
  **If P = 1 does the same job, the ring is decoration and must not be adopted.** That
  comparison has not been run.
- **Cost:** 264 bytes per ring at P = 64 (too much for a million creatures: 264 MB);
  P = 8 or 16 gives 32 to 64 bytes. Measured scalar cost about 18 to 20 ns per bead update
  (noisy), dominated by one Philox call per bead. Even and odd beads are independent given
  their neighbours, so 8 beads fit one AVX2 lane group; that SIMD version is **not written**.
- **Law check:** integer state, Philox counters, exp table from an integer recurrence with
  a fixed Q64 constant: bit-exact across -O0/-O2 and across runs (ran). It carries no
  matter or energy, so conservation is untouched. Wording rule: it is a classical Markov
  chain over a ring of numbers. Do not call it quantum and do not call its noise quantum.
- **As a world-field tool (tooling only):** the same Metropolis kernel can generate
  spatially correlated integer fields at world creation (a ring or lattice with a nearest-
  neighbour coupling). Lower value; plain Philox noise plus an integer blur does the same.

### 5.2 Dense layer + delta rule (from simple_mnist), category (g)

```c
/* y = relu(W x + b), int8 weights and activations, int32 accumulators, AVX-VNNI dot products */
void an_dense_relu_i8(const int8_t *restrict W, const int8_t *restrict x, const int16_t *restrict b,
                      int8_t *restrict y, uint32_t rows, uint32_t cols, uint32_t shift);
/* lifetime plasticity: W += (delta (x) pre) >> lr_shift - W >> decay_shift, saturating */
void an_dense_delta_i8(int8_t *restrict W, const int8_t *restrict delta, const int8_t *restrict pre,
                       uint32_t rows, uint32_t cols, uint32_t lr_shift, uint32_t decay_shift);
```

- **Where:** forward at Tier 0 for a tiny per-creature net (for example 16 x 16 = 256 bytes
  of weights); the plasticity step at Tier 1.
- **What is taken:** the verified equations of section 2.1 (delta propagation through
  ReLU, all deltas before any update, L2 decay folded into the step) and the init scale
  rule. **No code is taken as is:** it is float32, heap-allocated, 2.04 MB per network,
  `rand()`-seeded and scalar.
- **Open problem:** the error signal. simple_mnist uses a supervised label; a creature has
  none. The delta must come from the creature's own reward or prediction error, which this
  repo says nothing about.

### 5.3 Integer line-of-sight (from asteroids), category (b)

```c
/* walks cells from (x0,y0,z0) toward (x1,y1,z1) on a toroidal grid; returns steps to the first
   occupied cell or -1. Integer only. Needs a 3D extension of the 2D stepping tested here. */
int32_t an_los_i32(const uint8_t *occ, uint32_t log2_w, uint32_t log2_h, uint32_t log2_d,
                   int32_t x0, int32_t y0, int32_t z0, int32_t x1, int32_t y1, int32_t z1);
```

- **Where:** Tier 1 perception (can creature A see creature B or a resource), on the
  coarse world occupancy grid.
- This is the textbook Bresenham walk; the repo adds nothing beyond it. Listed because the
  engine needs exactly this primitive and the 2D form was tested here.

### 5.4 World fields (category i)

Nothing from PINN. The engine's heat, diffusion and flow fields need an explicit integer
conservative stencil (flux form, so totals balance exactly under law 5). That kernel is not
in any of these four repos and must be written new.

## 6. What is NOT usable, and why

- **PINN, all of it.** No PDE is discretised or solved, the physics loss never reaches the
  weight update, three loss functions read an uninitialised variable, and the tests assert
  nothing. Also no LICENSE file. There is no numeric idea here that the engine lacks.
- **PIMC Java code as code.** Java is forbidden inside the simulation (law 1) and the
  class has the six defects of section 2.3, two of which change the sampled distribution.
  Only the algorithm, corrected, is reusable; the re-implementation is in scratch.
- **PIMC `walk()` and `E0`, `Psi0` outputs.** Dead code and non-estimators.
- **simple_mnist data path** (`mnist_loader.c`, `download_mnist.sh`): a dataset loader and
  a downloader; law 3 forbids datasets. The ">99 %" claim is unverified here.
- **simple_mnist network as shipped:** float32 (law 6), 2.04 MB per instance (law 2 for
  per-creature use), `rand()` seeded by wall clock, softmax/cross-entropy needs labels.
- **neon_ops.c SIMD paths:** ARM NEON only. On this x86 machine every function is the
  scalar fallback; `exp_ps` cannot even be compiled. The fallback GEMV/GEMM are three-line
  loops with no reuse value. The engine needs AVX2/AVX-VNNI int8 kernels written new.
- **asteroids game loop, terminal I/O, float physics:** POSIX-only (does not build), float
  positions, `rand()`; `checkCollision` uses `sqrt` where an integer squared-distance
  compare is exact. No rendering or skin idea.
- **Limits of my own scratch code:** the ring sampler was verified only for the harmonic
  potential at P = 64, dt = 1/8; the double-well run shows behaviour, not correctness
  against a reference. The position clamp at |x| = 8 breaks proposal symmetry at the wall
  (never reached in these runs). The exp table is linearly interpolated (relative error
  about 3e-5 by the convexity bound, not measured separately). Block error bars are rough.
  No multi-thread run, no SIMD version, no in-engine lesion test.
- **Novelty:** none claimed. Primitive-action PIMC, Metropolis, Bresenham and backprop are
  textbook; no prior-art search was run because nothing here is presented as new.

## 7. Verdicts

| Repo | Verdict | One line |
|---|---|---|
| asteroids | reference_only | Textbook integer line and circle stepping; POSIX terminal game that does not build here. |
| simple_mnist | adapt | Backprop equations verified by gradient check; must be re-written as a tiny int8 layer; dataset path excluded. |
| PINN | reference_only | Builds and runs, but contains no PDE solver and nothing the engine can use. |
| PIMC | excluded_by_law | Exists only in Java and is defective; the corrected algorithm was re-implemented in integer C in scratch and checked against the exact harmonic result. |

## 8. Scratch inventory (`S:\tsotchke\tiny-numerics\`)

- `philox.h`, `pimc_ring.c`, `mnist_layer_test.c`, `los_test.c`: my test code.
- `src_pimc/PIMC/Driver.java`: headless Java driver (tooling only); `PIMC.java` beside it is the unmodified copy.
- `src_asteroids/`, `src_mnist/`, `src_pinn/`: unmodified copies of the repo sources.
- `build/`: executables; `run_pinn/`: PINN logs and weight files; `run_pimc_1.txt`, `run_pimc_2.txt`: the two full outputs.
