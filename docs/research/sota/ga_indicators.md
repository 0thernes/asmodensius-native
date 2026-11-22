# SOTA survey: GA kinematics in fixed point, the 14 indicator properties, NNUE-style int8 brains, the QGT as a metric

Key: ga-indicators. Date: 2026-11-17. Method: web research only (WebSearch + WebFetch).
Nothing was compiled, run or measured on this machine, so this note contains no "works"
claims of its own. Every number below is quoted from the linked source; where a source did
not state its hardware, that is said. Passages marked **[derived here]** are my own
arithmetic or reasoning, not a sourced fact, and need a test before anyone relies on them.

Source-access problems (honest list): `raw.githubusercontent.com` closed the socket twice
(the same NNUE document was read through the jsDelivr mirror instead, characters
20000-120000 of 157214; the last 37 k characters were not read). PubMed returned a cookie
wall. Papers marked "(not re-read)" are cited from memory of their existence and DOI only.

---

## 1. Geometric algebra for 3D kinematics in fixed point

### 1.1 What the objects are

- A 3D rotor (even element of the 3D geometric algebra) carries the same four numbers and
  the same product as a unit quaternion; the even subalgebra of 3D projective GA
  (PGA, R(3,0,1)), the "motor algebra", is isomorphic to the dual quaternions and packs
  into 8 numbers. Source: Klein library README, https://github.com/jeremyong/Klein
  (it states the motor algebra is isomorphic to dual quaternions and is computed with SSE,
  and claims performance "competitive" with quaternion/vector libraries, without figures).
- Normalisation, square root, exp and log for these algebras have closed forms. De Keninck
  and Roelfs, "Normalization, Square Roots, and the Exponential and Logarithmic Maps in
  Geometric Algebras of Less than 6D" (2022), https://arxiv.org/abs/2206.07496, give for
  3D PGA (their section 8.2, read via https://ar5iv.labs.arxiv.org/html/2206.07496):
  - normalise a motor: 23 multiplications, 10 additions, 1 square root, 1 division;
  - logarithm: 14 mul, 5 add, 1 div, 1 arccos, 1 sqrt;
  - exponential: 17 mul, 8 add, 2 div, 1 sincos, 1 sqrt;
  - square root of a motor = Normalize(1 + M).
  A motor M is on the motion manifold when M~M = 1. In PGA the product M~M of a drifted
  motor is a "Study number" s + t*I (scalar plus pseudoscalar), so dividing by sqrt(s)
  alone is not enough: the paper's fix uses alpha = 1/sqrt(s) and beta = t/(2*sqrt(s^3)).
  The paper motivates this by numerical integration drifting off the manifold and offers
  it as a replacement for SVD / Gram-Schmidt re-orthogonalisation. It does not analyse how
  fast drift accumulates or how often to renormalise.

### 1.2 Cost against quaternions

Sourced counts (floating point, scalar code), from F. Giesen, "Rotating a single vector
using a quaternion", https://fgiesen.wordpress.com/2019/02/09/rotating-a-single-vector-using-a-quaternion/ :

| Operation | Multiplies | Adds |
|---|---|---|
| rotate vector, direct sandwich formula | 26 | 17 |
| rotate vector, t = 2(q_xyz x v); v' = v + q_w t + q_xyz x t | 15 | 15 |
| rotate vector with a ready 3x3 matrix | 9 | 6 |

Giesen's conclusion: build the matrix when many vectors share one rotation.

**[derived here]** counts for composition, from the standard product formulas:

| Operation | Stored values | Multiplies |
|---|---|---|
| rotor * rotor (= quaternion product) | 4 | 16 |
| motor * motor (= dual-quaternion product: real = r1 r2, dual = r1 d2 + d1 r2) | 8 | 48 |
| "quaternion + position vector" compose (q = q1 q2; p = p1 + rotate(q1, p2)) | 7 | 16 + 15 = 31 |

So for pure rotation a rotor costs exactly what a quaternion costs (it is the same
arithmetic under another name). A full PGA motor costs about 1.5 times the multiplies of
"quaternion + integer position" per composition and stores one more value. What the motor
buys is uniformity (points, lines and planes transform by one sandwich; screw-motion
interpolation through log/exp; joint chains compose by one product). I found no published
measured benchmark of Klein or any PGA library against a quaternion library with numbers
and hardware (search 1.5-b below); the Klein README claim is unquantified.

### 1.3 Doing it with integers

What I found:

- Fixed-point quaternion and vector libraries built for lockstep determinism exist and are
  in use: Q48.16 integer-only math with quaternion and matrix types
  (https://github.com/mas-bandwidth/fixed), a C# fixed-point quaternion library
  (FixedMathSharp, found through a mirror of its GitHub page), Q16.16/Q32.32 types in a
  Rust crate (https://docs.rs/crate/gizmo-math/0.1.0/source/src/fixed.rs). A fixed-point
  port of BEPUphysics reported that sloppy Atan2/Acos approximations made Slerp jerky and
  that a solver change fixed platform-specific blow-ups
  (https://forum.bepuentertainment.com/viewtopic.php?f=9&t=2507): the transcendental
  approximations matter as much as the number format.
- A Rust GA crate ships "deterministic" 2D rotor types (`DetRotor2`, `DetVector2`,
  `DetF32`) with a lockstep/rollback/replay example:
  https://docs.rs/crate/amari/latest/source/examples/networked_physics.rs . The example
  does not reveal whether `DetF32` is fixed point or a constrained float; it is 2D rotors
  only, not 3D motors.
- GA on hardware: a TU Darmstadt report on GA on reconfigurable hardware
  (https://www.esa.tu-darmstadt.de/assets/publications/materials/2009/fccm09-TR.pdf) is
  summarised by the search engine as noting earlier FPGA GA coprocessors with 24-bit
  integer coefficients, later replaced by floating point; I did not open the PDF myself,
  so treat that detail as second-hand. A Palermo group built embedded GA coprocessors with
  a 32-bit floating-point variant
  (https://iris.unipa.it/bitstream/10447/167505/1/Franchini_AGACSE_2015.pdf, also second-hand).
- Exact integer rotations exist as a limiting case: quaternions with integer (Lipschitz)
  or half-integer (Hurwitz) components have integer norm
  (https://en.wikipedia.org/wiki/Hurwitz_quaternion), and rational rotation matrices are
  exactly the rotations generated by Lipschitz quaternions (Springer chapter on digitized
  rotations, https://link.springer.com/chapter/10.1007%252F978-3-319-39441-1_4, abstract
  level only). The Hurwitz unit group has 24 elements (binary tetrahedral group), the
  Lipschitz unit group 8.
- Unit caution from the dual-quaternion literature: the real part of a unit dual quaternion
  is unitless and the dual part carries units of length
  (https://arxiv.org/html/2203.13653v5), so the two halves of a motor need different
  fixed-point scalings.

**[derived here]** design consequences for an integer engine (untested):

1. Rotor in Q1.15 or Q2.30: every product rounds each component by up to half an ulp, so
   the squared norm does a random walk of order ulp per composition. Because drift per step
   is tiny, a single Newton step of the inverse square root around 1 is enough:
   `q <- q * (3 - |q|^2) / 2`. That is 4 multiplies for the norm, 4 for the scaling, one
   shift, no division and no square root, and it is deterministic. It is only valid while
   | |q|^2 - 1 | is small, so it must run every step or every few steps, not "when needed".
2. Motor: the De Keninck-Roelfs correction has two parts. With M~M = s + t*I and s near 1,
   the first-order integer version is alpha = (3 - s)/2 and beta = t/2 (their
   beta = t/(2 s^(3/2)) with s^(3/2) ~ 1). The beta term removes the pseudoscalar leak
   (rotation part no longer orthogonal to the translation part); skipping it leaves a motor
   that is "unit" but not a rigid motion.
3. Exact integer quaternions do not remove the problem: the norm is multiplicative, so
   components grow by about the bit-length of each factor per composition and must be
   rescaled, which rounds again. Exactness is only free for the finite unit groups
   (8 or 24 orientations) or for a bounded number of compositions.
4. For hundreds of thousands of bodies the cheapest representation is "rotor (4 x int16 or
   int32) + integer position (3 x int32)", composing in 31 multiplies and keeping position
   in exact world units; reserve full 8-component motors for articulated chains where the
   single-product composition and line/plane transforms pay for themselves.

### 1.5 Searches run for this section

- (a) "fixed-point geometric algebra rotor motor integer arithmetic implementation FPGA embedded"
- (b) "Klein PGA library SSE motor rotor apply point benchmark versus quaternion matrix operation count"
- (c) "fixed-point quaternion normalization drift integer Q15 renormalize after multiplication error accumulation"
- (d) "fixed-point dual quaternion FPGA kinematics implementation integer arithmetic word length"
- (e) "deterministic lockstep physics engine fixed-point quaternion Q16.16 rotation normalize cross-platform determinism"
- (f) "integer quaternion exact rational rotation matrix Lipschitz Hurwitz quaternions lattice rotations ..."

Search (c) surfaced Mascarenhas, "Fast and accurate normalization of vectors and
quaternions", https://arxiv.org/abs/1606.06508 (metadata only, not read), and a patent text
warning that renormalising after every integration step itself injects a systematic drift
(https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/6061611). No result gave a
Q15-specific error analysis.

---

## 2. Butlin et al. 2023: the 14 indicator properties

Source: P. Butlin, R. Long, et al., "Consciousness in Artificial Intelligence: Insights from
the Science of Consciousness", arXiv:2308.08708, https://arxiv.org/abs/2308.08708 . Table 1
read through https://ar5iv.labs.arxiv.org/html/2308.08708 (first 100 k characters of 276 k).
The wording below is my paraphrase, one line each; consult Table 1 for the authors' text.

| Id | Theory family | Indicator (paraphrased) |
|---|---|---|
| RPT-1 | Recurrent processing | Input (perception) modules that use algorithmic recurrence |
| RPT-2 | Recurrent processing | Input modules that produce organised, integrated perceptual representations |
| GWT-1 | Global workspace | Several specialised subsystems (modules) able to run in parallel |
| GWT-2 | Global workspace | A limited-capacity workspace: an information bottleneck plus a selective-attention mechanism |
| GWT-3 | Global workspace | Global broadcast: workspace contents are available to all modules |
| GWT-4 | Global workspace | State-dependent attention, so the workspace can query modules one after another to carry out complex tasks |
| HOT-1 | Computational higher-order | Perception modules that are generative, top-down or noisy |
| HOT-2 | Computational higher-order | Metacognitive monitoring that tells reliable perceptual representations from noise |
| HOT-3 | Computational higher-order | Agency driven by a general belief-formation and action-selection system that strongly tends to update beliefs from the metacognitive monitor's output |
| HOT-4 | Computational higher-order | Sparse and smooth coding that yields a "quality space" |
| AST-1 | Attention schema | A predictive model that represents the current state of attention and enables control over it |
| PP-1 | Predictive processing | Input modules that use predictive coding |
| AE-1 | Agency and embodiment | Agency: learning from feedback and choosing outputs to pursue goals, especially with flexible response to competing goals |
| AE-2 | Embodiment | Modelling output-input contingencies (how own outputs change own inputs), including systematic effects, and using that model in perception or control |

Count check: 2 + 4 + 4 + 1 + 1 + 2 = 14.

How the paper says to use them (same source):

- The list is a provisional rubric, expected to be revised as the science moves.
- They are indicators, not conditions. Each is held necessary by at least one theory and
  some subsets are held jointly sufficient by some theory, but the authors do not assert
  that any single indicator or any combination is necessary or sufficient.
- The reading is graded: a system with more of the indicators is a more likely candidate.
  Any credence should combine (a) how closely the system's actual processing matches what
  the theory describes, (b) confidence in that theory, and (c) confidence in computational
  functionalism, which the report adopts only as a working hypothesis and calls disputed.
- Their own assessment: no current AI system appears to be a strong candidate; they see no
  obvious technical barrier to building systems that satisfy the indicators; satisfying
  them would still not establish consciousness.
- The assessment is architectural (what the system computes and how), not behavioural:
  the report applied the rubric to language models and to embodied agents (PaLM-E, a
  "virtual rodent", DeepMind's AdA) according to a press summary,
  https://the-decoder.com/researchers-identify-potential-indicators-of-consciousness-in-ai-systems/
  (second-hand; I did not read those case-study sections).

Consequence for this project (law 7, "indicator only"): each indicator can be reported as a
yes/partial/no architectural property backed by a lesion test, and never summed into a
claim. Note that the paper's rubric presupposes computational functionalism; reporting an
indicator as present says nothing if that hypothesis is false.

Related prior work on lesioning such mechanisms: Y. J. Phua, "Can We Test Consciousness
Theories on AI? Ablations, Markers, and Robustness" (Dec 2025),
https://arxiv.org/abs/2512.19155 . Hand-built agents with a global workspace and a
self-model; removing the self-model abolishes metacognitive calibration while first-order
task performance survives; a full workspace lesion collapses access markers and partial
lesions degrade them gradually; a perturbational-complexity proxy moved the "wrong" way
under the workspace bottleneck. The author states the agents are not conscious. The
abstract does not mention the Butlin list.

---

## 3. NNUE-style int8 networks and integer-friendly plasticity

### 3.1 NNUE as a precedent for tiny fast brains

Sources: the Stockfish trainer's design document,
https://github.com/official-stockfish/nnue-pytorch/blob/master/docs/nnue.md (read through
https://cdn.jsdelivr.net/gh/official-stockfish/nnue-pytorch@master/docs/nnue.md), and
https://www.chessprogramming.org/NNUE .

- Origin: Yu Nasu, 2018, for computer shogi; ported to Stockfish in 2020 (Stockfish 12).
  Chessprogramming reports Stockfish's search speed roughly halved against the hand-written
  evaluation while playing strength rose (no Elo figure on that page).
- Shape: shallow (2 to 4 layers); nearly all parameters sit in a very wide first layer whose
  inputs are sparse binary features (input sparsity about 0.1 % in current best nets).
  Example in the document: HalfKP has 64*64*5*2 = 40960 features; a 256x2 -> 32 -> 32 -> 1
  net has about 10 million first-layer parameters, about 20 MB quantised. Chessprogramming
  says top engines now use accumulators of 1024 to 3072 neurons.
- Incremental update: the first layer's pre-activation sum (the "accumulator") is kept from
  the previous position; a move subtracts the weight columns of removed features and adds
  the columns of added ones (2 features for a quiet move, 3 for a capture, 4 for castling).
  Only the first layer can be updated this way because of the non-linearity after it. When
  every feature changes (HalfKP: a king move) the accumulator is rebuilt from the bias.
- Quantisation (the document's "Stockfish quantization scheme"): accumulator int16 with
  weights and biases scaled by 127; clipped ReLU clamps to 0..127 and narrows to int8;
  later layers have int8 weights with scale 64 (so the largest representable weight is
  127/64, about 1.98; the trainer clamps to that) and int32 biases added after the dot
  product.
- Kernels named in the document: AVX2 path `_mm256_maddubs_epi16` + `_mm256_madd_epi16`;
  VNNI path a single `_mm256_dpbusd_epi32`; clipped ReLU by saturating packs
  (`_mm256_packs_epi16`). Chessprogramming gives the common alternative QA = 255, QB = 64
  with int16 weights and `_mm256_madd_epi16` for squared-clipped-ReLU nets.
- Measured numbers: the design document says it contains no experimental results; its
  design target is "million(s) of evaluations per second per thread". The only concrete
  figures I found are forum-reported: 2,428,489 nodes/s and 2.03 million NNUE evaluations/s
  for an AVX2 Stockfish build, 2,545,210 nodes/s for a re-implementation
  (https://talkchess.com/viewtopic.php?p=948872). **Hardware is not stated in that thread**,
  so these are order-of-magnitude only.

**[derived here]** what does and does not transfer to creature brains:

- The speed comes from three things together: sparse binary inputs, few input changes per
  step, and a shared weight table that stays in cache. A creature with dense analogue
  senses gets no incremental benefit unless senses are discretised into sparse one-hot
  features that are temporally coherent (few bits flip per tick).
- One shared weight table per genome/species is cache friendly; per-creature plastic
  weights are not: at 1 KB of private weights each, a million creatures need about 1 GB,
  far beyond the 36 MB L3 stated in COMMON.md. Lifetime plasticity therefore has to be
  small per individual (tens to hundreds of bytes) or expressed as a low-rank / sparse
  delta on shared weights.
- The int8-weight / int16-accumulator / int32-bias layout and the maddubs or dpbusd kernels
  map directly onto the machine's AVX2 + AVX-VNNI (COMMON.md). The exact intrinsic name for
  the VEX-encoded (non-AVX-512) VNNI form with GCC 16 was not checked in this session.

### 3.2 Integer-friendly plasticity rules

- Three-factor ("neo-Hebbian") rules: a Hebbian coincidence of pre- and post-synaptic
  activity sets a decaying per-synapse eligibility trace; a later global third factor
  (reward, surprise, novelty) converts the trace into a weight change. Review with
  experimental support: Gerstner, Lehmann, Liakoni, Corneil, Brea (2018),
  https://arxiv.org/abs/1801.05219 . The third factor is most often modelled as "reward
  minus expected reward" (https://neuronaldynamics.epfl.ch/online/Ch19.S4.html).
- Distal reward: Izhikevich, "Solving the Distal Reward Problem through Linkage of STDP and
  Dopamine Signaling", Cerebral Cortex 17:2443-2452 (2007),
  https://www.izhikevich.org/publications/dastdp.htm . Millisecond-scale STDP events open a
  window of a few seconds during which dopamine can still act on those synapses, so a
  delayed reward credits the right synapses despite ongoing background firing.
- Frémaux, Sprekeler and Gerstner analyse a generic class of reward-gated Hebbian rules and
  find most are a competition between a reward-sensitive part and an unsupervised Hebbian
  part (https://pmc.ncbi.nlm.nih.gov/articles/PMC2778872, via search summary only).
  Practical reading: unless the unsupervised term is cancelled (for example by subtracting
  a reward baseline), the rule drifts regardless of reward.
- Evolved local rules without any reward signal: Najarro and Risi, "Meta-Learning through
  Hebbian Plasticity in Random Networks", NeurIPS 2020, https://arxiv.org/abs/2007.02686 .
  Evolution searches synapse-specific Hebbian coefficients (over 450 K plasticity
  parameters); networks start from random weights each lifetime; a simulated quadruped
  adapts to unseen morphological damage within about 100 timesteps. Floating point.
- Stabilising Hebbian growth in evolved animats by saturating the plasticity signal and
  adding slower homeostasis: https://arxiv.org/abs/2201.11742 (search summary only).
- Integer precedent in silicon: Intel Loihi. Its learning rule is a sum of products of
  pre/post spikes and traces; traces are first-order filters held as integers 0..127
  (7 bits) with stochastic rounding, and weights are stochastically rounded to the
  programmed precision. Sources: Brian2Loihi emulator paper,
  https://arxiv.org/abs/2109.12308 (abstract: on-chip learning emulated "with reasonable
  discrepancy due to stochastic rounding"; trace details from the search summary of its
  full text), and the nengo-loihi discretisation source,
  https://www.nengo.ai/nengo-loihi/v0.9.0/_modules/nengo_loihi/discretize.html . Intel's
  Lava framework has a reward-modulated STDP tutorial for the Loihi learning engine with
  eligibility-trace and reward-trace dynamics and a per-synapse "tag" variable:
  https://lava-nc.github.io/lava/notebooks/in_depth/three_factor_learning/tutorial01_Reward_Modulated_STDP.html
  (page title and table of contents seen; the arithmetic details were not read).

**[derived here]** integer recipe implied by the above (untested):

    e  <- e - (e >> k) + pre * post          (eligibility, int8/int16, decay 2^-k)
    dw <- (M * e) >> s                       (M = reward - baseline, small signed int)
    w  <- sat8(w + dw)

Known trap: `e >> k` is zero once e < 2^k, so a truncating trace never decays to zero and
tiny updates are lost entirely. Loihi's answer is stochastic rounding, which keeps the rule
unbiased in expectation; with a counter-based generator keyed by (tick, synapse id) that
rounding is still bit-reproducible for any thread count. Each claim here needs a lesion
test (zero the third factor, zero the trace) before it counts as a mechanism (law 4).

---

## 4. The quantum geometric tensor as the metric of natural-gradient updates

- Definition. For a parametrised state psi(theta),
  G_ij = <d_i psi | d_j psi> - <d_i psi | psi><psi | d_j psi>; the Fubini-Study metric is
  g = Re G. Update: solve g (theta' - theta) = -eta grad L (pseudo-inverse in principle).
  Source: Stokes, Izaac, Killoran, Carleo, "Quantum Natural Gradient", Quantum 4, 269
  (2020), https://arxiv.org/abs/1909.02108 (Eq. 7 and Eqs. 10-13, read via
  https://ar5iv.labs.arxiv.org/html/1909.02108).
- Classical limit. For a state with real non-negative amplitudes,
  psi = sum_x sqrt(p_theta(x)) |x>, the QGT equals one quarter of the classical Fisher
  information matrix of p_theta (same paper, Sec. 2.1 / App. A). So for anything that is a
  probability distribution without phases, "QGT preconditioning" is Amari's natural
  gradient up to a constant factor (Amari 1998, Neural Computation 10:251,
  https://doi.org/10.1162/089976698300017746, not re-read).
- Stochastic reconfiguration (SR) in variational Monte Carlo is the same step:
  S_ik = <O_i* O_k> - <O_i*><O_k> with O_k the log-derivatives of the wavefunction (a
  covariance estimated from samples), then solve S dW = F with F the covariance of local
  energy and log-derivatives. The sampled S is often near-singular, so a diagonal shift of
  about 1e-5 to 1e-2 is added. Dense factorisation is recommended up to roughly 1000-5000
  parameters, iterative solvers beyond; a sample-space reformulation (minSR) inverts an
  N_samples x N_samples matrix instead. Source: NetKet documentation,
  https://netket.readthedocs.io/en/latest/user-guides/sr.html . The SR / natural-gradient
  equivalence is stated in https://arxiv.org/pdf/2306.16877 (search summary). MinSR trained
  networks with up to 10^6 parameters: Chen and Heyl, Nature Physics 20, 1476 (2024),
  https://arxiv.org/abs/2302.01941 . (Origin of SR: Sorella 1998,
  https://doi.org/10.1103/PhysRevLett.80.4558, not re-read.)
- Cheap approximations. Stokes et al. give a block-diagonal metric (one block per circuit
  layer) and a diagonal one, g_ii = variance of the generator; they report the diagonal
  version beating plain gradient descent in iterations with wall time comparable to it
  (their experiments were small circuits; they skipped the block-diagonal classical
  post-processing at 11 qubits because of its overhead).
- Classical uses that already exist:
  - Natural Evolution Strategies: keep a parametrised search distribution over genomes and
    follow the natural gradient of expected fitness (Wierstra, Schaul, Glasmachers, Sun,
    Schmidhuber, https://arxiv.org/abs/1106.4487).
  - Natural-gradient synaptic plasticity: Kreutzer, Petrovici, Senn, "Natural-gradient
    learning for spiking neurons", eLife 2021, https://elifesciences.org/articles/66526
    (arXiv 2011.11710). Uses the Fisher information of the neuron's output distribution as
    the metric, derives a rule with locally available terms, and reports faster convergence
    than the Euclidean rule. Floating point; reviewers found the Fisher-metric motivation
    hard to follow.

**[derived here]** the smallest sensible classical use:

1. Per-synapse diagonal Fisher. Keep one extra small integer per plastic weight, a decayed
   running mean of the squared eligibility (the squared score), and divide the three-factor
   update by it. In integers, use the position of its top bit so the division is a shift.
   This is the diagonal of the covariance S above with a "diagonal shift" equal to the
   minimum shift count. It costs one multiply, one leading-zero count and one shift per
   update.
2. Population level: a separable NES step on the genome distribution (diagonal Fisher of a
   factorised Gaussian), which is where a metric has the clearest measurable effect.
3. A full (dense) metric is only defensible for tens of parameters per individual; at
   NetKet's own guidance dense solves stop being sensible around a few thousand parameters,
   and per-creature solves at population scale are out of the cache budget.

Naming discipline (law 7): with real, phase-free quantities this object is the classical
Fisher metric. Calling it "quantum geometric tensor" is only accurate where complex
amplitudes with phases are actually being simulated, and even then it is a classical
simulation with no speed-up. Any claimed benefit must be shown by ablation (metric on vs
off) on the same seeds.

---

## Cautions

- Section 1 operation counts for composition and all fixed-point renormalisation formulas
  are derived here, not measured; no published fixed-point error analysis for rotors or
  motors was found.
- The only NNUE throughput numbers found have no stated hardware.
- Several sources were read only as abstracts or search-engine summaries; they are marked.
- The Butlin table wording is paraphrased; quote the paper's Table 1 if exact wording is
  needed. Only the first 100 k characters of the paper were read (Table 1 and the framing),
  not the per-theory chapters or case studies.
- The last 37 k characters of the NNUE design document were not read.

---

## What would be genuinely new in an engine like ours

Written conservatively. Every component above has prior art; only specific combinations
were not found, and a handful of web searches is weak evidence of absence. No search of
Zenodo, patents or non-English literature was made.

1. **Integer 3D PGA motors with a division-free two-part renormalisation (scalar and
   pseudoscalar terms) under a reproducible state hash.** Found: float PGA (Klein), the
   float normalisation formulas (De Keninck-Roelfs), fixed-point quaternions for lockstep
   games, a deterministic 2D rotor type in a Rust crate, second-hand mention of 24-bit
   integer GA coprocessors. Not found: any fixed-point 3D motor implementation or drift
   analysis. Searches: 1.5 (a), (c), (d), (e). Modest novelty; for most bodies
   "rotor + integer position" is cheaper anyway.
2. **NNUE-style incremental sparse first layer used as a creature brain outside board-game
   search, combined with per-individual integer three-factor plasticity.** Found: NNUE only
   in chess/shogi-type engines; evolved Hebbian controllers only in floating point; integer
   three-factor learning only on neuromorphic chips. Not found: the combination. Searches:
   "artificial life evolved creatures neural network int8 quantized incremental update
   NNUE-style Hebbian plasticity integer"; "\"efficiently updatable\" neural network
   incremental accumulator applied outside chess shogi agents simulation reinforcement
   learning int8".
3. **Scoring evolved, non-LLM creatures against all 14 Butlin indicators with a lesion test
   per indicator.** Found: the Butlin rubric applied by its authors to existing AI systems;
   Phua 2025 lesioning hand-built GWT/HOT agents (not evolved, not the full list). Not
   found: an evolved artificial-life population assessed indicator by indicator. Search:
   "indicator properties consciousness Butlin applied to artificial life agents evolved
   agents assessment global workspace lesion". This would be new as an evaluation practice,
   not as evidence of anything beyond the indicators themselves.
4. **Power-of-two diagonal Fisher preconditioning of an integer three-factor rule.** Found:
   natural-gradient plasticity in float (Kreutzer et al.), diagonal metric approximations
   (Stokes et al.), NES. Not found: an integer/shift-only variant, or any use of the
   QGT/Fisher metric in artificial life. Searches: "natural gradient Fisher information
   local Hebbian plasticity rule online synaptic \"natural gradient\" spiking neurons
   integer"; "\"quantum geometric tensor\" OR \"Fubini-Study\" natural gradient
   neuroevolution OR \"artificial life\" OR \"Hebbian\" classical small network diagonal
   approximation". Likely marginal: second-moment scaling of updates is commonplace in
   machine-learning optimisers, so this is an engineering variant, not a new idea.

Nothing in this list should be described as new in any outward-facing text until a wider
prior-art search (Zenodo, Google Scholar by distinctive terms, GitHub code search) has been
done.
