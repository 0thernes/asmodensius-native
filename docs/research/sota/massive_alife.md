# Massive artificial-life and agent simulations: prior art and speed techniques

Key: massive-alife. Date: 2026-11-17. Status: complete (web research only).

## How this note was made (honest evidence)

- Tools: WebSearch and WebFetch only. Nothing was compiled, benchmarked or run
  for this note, so there are no timings of ours here.
- Several sources are PDFs. WebFetch could not read them directly but saved a
  copy into the harness tool-results cache as a side effect; I extracted their
  text with `python -I` + PyMuPDF (`fitz`, already installed). No installs, no
  clones. PDFs read this way: Green (particles.pdf), Hoetzlein (GTC 2014
  slides), Ihmsen et al. 2011, Biomaker CA, Madrona (SIGGRAPH 2023), BioDynaMo
  (arXiv 2301.06984).
- Every number below is the source's own number with the source's hardware.
  Where I divide two source numbers the result is marked **(derived)**.
  "Not found" means my searches did not surface a measured figure; it does not
  mean none exists.
- Queries were generic technical queries (project names of the *surveyed*
  systems only).

## 1. Survey table

| System | Scale reached | Hardware | Speed | Core technique | Source |
|---|---|---|---|---|---|
| ALIEN (chrxh) | "millions of particles in real time" (README claim, no benchmark) | NVIDIA GPU, CUDA (README: compute capability 7.5+; older mirror says 6.0+) | not found | 2D CUDA particle engine (soft/rigid bodies, fluids); each cell runs a small neural net; genome-driven construction; all physics on GPU | https://github.com/chrxh/alien , https://chrxh.itch.io/alien |
| Origin (nvneik3d) | 300,000+ cells, each with a tiny net (23 in, up to 16 hidden, 9 out); dishes "up to about a million" | RTX 3090 | 60 fps at 300k+ cells (README) | WebGPU compute shaders; 2D-style dish | https://github.com/nvneik3d/Origin |
| Particle Life (Ventrella "Clusters" -> Mohr) | 10k-100k typical hobby GPU ports | Godot compute shader port: hardware not stated | ~450 fps at 10k, ~15 fps at 100k | asymmetric (non-Newtonian) pair forces between species; spatial binning | https://ventrella.com/Clusters/intro.html , https://gh.ali360.cloudns.ch/Subash-A-A/particle-life-gpu (mirror of the GitHub repo) |
| Particle Life, CuPy port | 115,000 particles | not stated | >33 fps | bin to grid, sort by cell index, binary search for cell starts, 3x3 lookup | https://www.foxhop.net/api/v1/export/threads/0b6d520b-7414-11f1-9565-040140774501.md |
| Pandora (Particle Lenia) | "up to millions of particles" (no count, fps or GPU given) | WebGPU browser; CUDA research backend | not found | cell size ~ kernel cutoff; Morton-interleaved cell index; 3 GPU passes: count per cell, prefix sum, scatter | https://pajouheshgar.github.io/blog/posts/pandora-artificial-life |
| Lenia | 128x128 ran faster on CPU than GPU (author); FFT wins for kernel radius above ~10 (author) or >= 32 (independent GTX 1050 Ti test) | GTX 1050 Ti (independent) | Wartets C++/OpenGL README self-reports 60+ fps at 512x512, R=13 | continuous CA; FFT convolution with precomputed kernel FFT; periodic boundary for free | https://groups.google.com/g/reaction-diffusion/c/W0SEV8u37QM , https://hegl.mathi.uni-heidelberg.de/continuous-cellular-automata , https://gh.hhj520.top/Wartets/Lenia |
| Flow-Lenia | 1024x1024 world, 144 creatures, 200k steps | Tesla T4 | ~255 us/step at 128x128, 1 channel, 10 kernels | JAX; mass conserved by reintegration tracking (each cell's mass moved as a unit-integral square distribution along the flow); rule parameters carried with the matter | https://ar5iv.labs.arxiv.org/html/2212.07906 |
| JaxLife | 128 agents + 32 bots, 2^16 steps; scaling runs 32-256 agents | one NVIDIA A100 | steps/s not reported | all-JAX; attention encoder + LSTM per agent; programmable robots | https://arxiv.org/html/2409.00853v1 |
| Biomaker CA | 2D grids; runs of 1,000-10,000 steps shown; grid sizes not extracted | GPU/TPU via JAX (model not stated) | not found | one vectorised agent logic for every cell; per-cell parameters differ; nutrient/energy economy | https://arxiv.org/pdf/2307.09320 |
| Microcosmos | "up to 500,000 particles on a single GPU" | single GPU (model not stated) | not found; linear step-time growth claimed | JAX; D2Q9 lattice Boltzmann + position-based filaments + immersed boundary; grid-based repulsion | https://www.alphaxiv.org/abs/2607.02954 |
| Avida | population capped by grid cells; studies use 3,600; a review cites up to ~12,000 | CPU | not found | virtual CPUs given small time slices; one organism per cell | https://arxiv.org/pdf/1604.06299 , https://pdf.arxiv.org/pdf/adap-org/9405003 |
| Tierra | soup of 60,000 instructions; 80-instruction ancestor | CPU (1991) | not found | time-sliced virtual machine; reaper queue kills oldest/most faulty | https://www.tomray.me/pubs/tierra/node9.html , https://www1.udel.edu/PR/Messenger/92/2/16.html |
| Polyworld | ~300 organisms, ~200 neurons each ("several hundred" of each per Yaeger) | 1990s workstation | not found | 3D-rendered colour vision feeding Hebbian nets; genome-specified architecture | https://webarchive.ars.electronica.art/en/archives/festival_archive/festival_documentations/1993/yaege.html , https://shinyverse.org/larryy/Yaeger.ALife3.pdf |
| The Bibites | no official figure found | Unity, CPU | user report only: 100x slider reaches ~40x with CPU at 30-40% (suggests a single-thread bound) | not documented in sources found | https://axebps-redlib.hf.space/r/TheBibites/comments/1jlwjsf/simulation_speed |
| Neural MMO 2.0 | 128 agents standard, 1-1,024 supported | CPU | ~3,000 agent-steps per core per second (was ~800-1,000) | Python/NumPy engine rewrite | https://arxiv.org/pdf/2311.03736 |
| Madrona | thousands of environments in one batch | i9-13900K + RTX 4090 | >1.9 M environment steps/s (hide and seek with physics + ray casts) | ECS (archetype tables) mapped to GPU batch execution; one kernel per system | https://madrona-engine.github.io/shacklett_siggraph23.pdf |
| FLAME GPU 2 | 1,000,000 agents (circles model); Sugarscape up to 16 M | V100 / A100 / H100 | 1 M agents, 200 steps, Spatial3D messaging: 0.944 s / 0.685 s / 0.551 s; brute force: 1,457 s / 1,071 s / 944 s. Each agent reads 204.5 messages on average | spatially binned message lists; agents talk only through messages (read old, write new) | https://rse.shef.ac.uk/blog/2023-08-18-benchmarking-flamegpu2-on-h100-a100-and-v100-gpus , https://eprints.whiterose.ac.uk/199416/ |
| BioDynaMo | 1.72 billion agents on one server | 4x Xeon E7-8890 v3, 72 physical cores, 504 GB | 26.8 M agents on 16 cores: 1.80 s per iteration (2x Xeon E5-2683 v3). 60.7-74.0x speed-up on 72 cores | optimised uniform grid; agents sorted in Morton order; custom allocator; NUMA-aware iteration; skips collision force for static agents | https://arxiv.org/pdf/2301.06984 |
| Noita (falling sand) | world size not stated | CPU, multi-thread | not found | 64x64 chunks, each with a dirty rect; 4 checkerboard rounds; a pixel may move at most 32 px outside its chunk, so no locks or atomics | https://80.lv/articles/noita-a-game-based-on-falling-sand-simulation/ |
| bhoult/evolve | 790 creatures, 3D articulated bodies, brains 64-4,096 neurons | Core Ultra 9 285K + RTX 5080 | ~71 steps/s at 790 creatures | Rapier multibody physics on CPU; all brains in batched GPU compute; claims bit-identical world on one or many threads; soil and water conserved | https://github.com/bhoult/evolve |

Derived sanity numbers (my arithmetic, not the sources'):

- FLAME GPU 2 on H100: 0.551 s / 200 steps = 2.8 ms per step at 1 M agents
  **(derived)**, if "simulation time" covers all 200 steps as the post states.
- BioDynaMo: 26.8 M / 1.80 s / 16 cores = ~0.93 M agent-updates per core-second
  **(derived)**, for a double-precision cell-mechanics model.
- Our target, 1 M creatures at 30 steps/s on 8 P-cores, is 3.75 M
  creature-steps per core-second **(derived)**: about four times that BioDynaMo
  rate. So the per-creature step must be small, or most creatures must be
  time-sliced.

## 2. Techniques, with measurements

### 2.1 Fixed-radius neighbour search on a uniform grid

- Green, "Particle Simulation using CUDA" (NVIDIA; draft 2007, revised May
  2010; Hoetzlein's slides cite it as both "Green 2008" and "2010").
  Cell size = particle diameter; each particle goes in one cell ("loose" grid)
  and the query scans 3x3x3 = 27 cells. Two build methods: (a) atomic counter
  per cell plus scattered write; (b) hash = cell id, radix sort the
  (hash, id) pairs, then find each cell's start by comparing neighbours in the
  sorted list. The position and velocity arrays are then physically reordered
  into sorted order. Sorting was fastest "on most hardware" because it improves
  memory coherence and reduces warp divergence. Measured: 65,536 colliding
  particles at ~175 fps on a GTX 280, ~460 fps on a GTX 480; cached texture
  reads gave up to 45%. Arrays are double-buffered so an update never reads a
  half-written neighbour.
  https://developer.download.nvidia.com/assets/cuda/files/particles.pdf
- Hoetzlein, "Fast Fixed-Radius Nearest Neighbors" (GTC 2014). Observation:
  only the bin matters, order inside a bin does not, so a full radix sort is
  wasted work. Replace it with a counting sort: count per bin (atomic add),
  prefix sum, scatter/copy. Kernel calls per frame drop from 15 to 4. Measured
  on a GeForce GTX Titan: about 400 million queries/s at 8 million points,
  "5-10x faster" than the radix-sort method; 4,194,304 particles at 4.2 fps and
  2,097,152 at 12 fps (full fluid). Same GTX 460M: 16,384 particles at 32 fps
  (old method) versus 193,487 at 32 fps (11x). Cell-size slide: cell = radius
  gives 3 cells per axis; smaller cells mean too many cells (5x5x5 = 125),
  larger cells too many particles per cell.
  https://ramakarl.com/pdfs/2014_Hoetzlein_Fast_Neighbors.pdf
- Ihmsen, Akinci, Becker, Teschner, "A parallel SPH implementation on
  multi-core CPUs" (CGF 2011). The best CPU reference found. Hardware: 4x
  six-core Xeon 7460 at 2.66 GHz (24 cores), 128 GB. Scene: 130K particles.
  Times in ms as construction / query / total, with particle reordering, and
  without it in brackets:
  - basic uniform grid 25.7 / 38.1 / 63.8 (27.5 / 105.6 / 133.1)
  - index sort (Green) 35.8 / 29.1 / 64.9 (38.2 / 29.9 / 77.3)
  - Z-index sort 16.5 / 26.6 / 43.1 (20.5 / 29.7 / 50.2)
  - spatial hashing 41.9 / 86.0 / 127.9 (44.1 / 89.9 / 134.0)
  - compact hashing 8.2 / 32.1 / 40.3 (9.4 / 55.2 / 64.6)

  Other results: reordering every 100 steps is enough; handles stay nearly
  sorted between steps, so insertion sort is cheap. Optimal cell size = the
  influence radius (halving it cut tested pairs from 25 M to 15.5 M but raised
  query time from 26.6 to 39.6 ms, because memory traffic cost more than the
  extra tests). On CPU, storing neighbour pairs once per step beat recomputing
  them when they are read several times (PCISPH 190.1 -> 105.4 ms). Scaling:
  404.1 ms on 1 core -> 40.3 ms on 24 (10x; matches Amdahl at 95% parallel).
  12 M particles: 5,193.6 ms neighbour search per step.
  https://cg.informatik.uni-freiburg.de/publications/2011_CGF_dataStructuresSPH.pdf
- FLAME GPU 2 shows the size of the win at 1 M agents: 0.55 s (spatial)
  against 944 s (brute force) on the same H100, about 1,700x **(derived)**.
  https://rse.shef.ac.uk/blog/2023-08-18-benchmarking-flamegpu2-on-h100-a100-and-v100-gpus

### 2.2 Morton / Z-order sorting

- Green already suggests a Z-order hash instead of the linear cell id "to
  improve the coherence of memory accesses" (same PDF as above).
- Ihmsen's Z-index sort is the fastest sort-based CPU variant in the table
  above (43.1 ms against 64.9 ms for linear index sort).
- BioDynaMo sorts agents in Morton order over the uniform grid and compared it
  with a Hilbert curve: Hilbert was only 0.54% better, so they kept Morton for
  simpler code. They also give a linear-time way to get Morton order on a
  non-cubic grid. https://arxiv.org/pdf/2301.06984
- Other reported gains (via a search summary, papers not read by me): ~2x for
  Hilbert ordering in an N-body/SPH code, ~20% for Morton in weakly
  compressible SPH. https://arxiv.org/pdf/2205.03074

### 2.3 Counting sort versus radix sort

- When the key is a cell index, counting sort is one histogram pass, one prefix
  sum and one scatter. Radix sort repeats that once per digit. Hoetzlein
  measured 5-10x on GPU (above).
- CPU evidence found is thin: one forum benchmark reports counting sort beating
  radix up to about 2^11 distinct values, where the counter array (~8 KB) still
  fits L1. Anecdotal, one machine.
  https://discourse.julialang.org/t/ironic-observation-about-sort-and-sortperm-speed-for-small-integers-vs-r/8715?page=2
- Consequence for a large grid: the counter array for a whole 3D world will not
  fit L1. A two-level scheme (coarse chunk first, then cells inside a chunk)
  keeps each histogram cache-sized. I found no measurement of this for ALife;
  it is an inference, to be tested.

### 2.4 Data-oriented design and ECS

- Unity Entities: archetype chunks are 16 KiB and hold at most 128 entities.
  Fragmentation warning from Unity's own docs: 100,000 entities each in a
  unique archetype allocate more than 1.5 GB of mostly empty chunks.
  https://docs.unity3d.com/Packages/com.unity.entities@1.3/manual/concepts-archetypes.html ,
  https://docs.unity3d.com/Packages/com.unity.entities@1.4/manual/performance-chunk-allocations.html
- flecs: C99, archetype/SoA storage, lockless scheduler; README claims
  "millions of entities every frame" with no benchmark number.
  https://github.com/SanderMertens/flecs
- EnTT: sparse sets; full-owning groups give "perfect SoA" and iteration close
  to archetypes. https://skypjack.github.io/2019-03-21-ecs-baf-part-2-insights
- Madrona shows the same archetype-table design carried to a GPU: >1.9 M
  environment steps/s on an RTX 4090.
  https://madrona-engine.github.io/shacklett_siggraph23.pdf
- Unity DOTS boids figures found are forum anecdotes with no hardware (for
  example 100,000 boids at 35 fps on "my laptop"); not usable as evidence.
  https://caseyhofland.itch.io/dots-sandbox/devlog/118083/first-jobs-prototype
- A social-force-model study found SoA and AoSoA beat AoS, and that AVX2
  auto-vectorisation failed where hand vectorisation worked.
  https://walkerchi.github.io/2023/06/23/ETHz/ETHz-ASL

### 2.5 Falling-sand engines

- Noita: 64x64 chunks, per-chunk dirty rectangle, bottom-to-top update, 4
  checkerboard rounds with a 32 px reach limit, so threads never touch the same
  pixel and no locks are needed.
  https://80.lv/articles/noita-a-game-based-on-falling-sand-simulation/
  (talk: https://gdcvault.com/play/1025695/Exploring-the-Tech-and-Design )

### 2.6 Determinism

- Deterministic lockstep sends only inputs and needs bit-identical simulation.
  Gaffer on Games: float results differ across compilers, OSes and instruction
  sets, so guarantees are "almost impossible" without tight constraints.
  https://gafferongames.com/post/deterministic_lockstep/ ,
  https://gafferongames.com/post/floating_point_determinism
- A GDC 2024 cross-platform determinism post-mortem advises fixed point (per
  search summary; slides not read).
  https://media.gdcvault.com/gdc2024/Slides/GDC+slide+presentations/Pollard_Bradley_CrossPlatformDeterminism+2024-03-26+09.34.19.pdf
- "1500 Archers" (Age of Empires): 1,500 entities, 8 players, inputs only (as
  cited in https://hdms.bsz-bw.de/files/7107/DeterministicLockstepInNetworkedGamesPaper.pdf ).
- Factorio: deterministic lockstep, CRC checks to detect desync.
  https://wiki.factorio.com/Desynchronization , https://cf-www.factorio.com/blog/post/fff-47
- Box2D v3 gets the same result for any thread count: each worker fills a local
  bit array, the main thread ORs them, and the set bits give a fixed order with
  no sort. Atomics stop races but do not fix order. For cross-platform floats:
  no fast-math, no FMA contraction, own sin/cos/atan2. CI checks a hash of body
  transforms. https://box2d.org/posts/2024/08/determinism/
- Box2D v3 also uses persistent graph colouring so constraints of one colour
  never share a body and can be solved in SIMD lanes without races.
  https://box2d.org/posts/2024/08/simd-matters/
- BioDynaMo's thesis lists what breaks determinism in a parallel agent
  simulator: thread-private RNG streams, uncommitted in-place updates, and
  non-associative (float) reductions. It suggests one counter-based generator
  per agent and committing changes at the end of the step.
  https://arxiv.org/pdf/2503.10796
- Event-keyed counter-based RNG (Philox/Threefry keyed by event identity, not
  by draw count). https://arxiv.org/abs/2603.11084
- bhoult/evolve reports CPU and GPU brain kernels agreeing only to 5e-6: float
  GPU kernels are not bit-identical to CPU ones.
  https://github.com/bhoult/evolve

## 3. Ten techniques most worth adopting

Target: 100k to 1M creatures, 8 P-cores plus one laptop GPU, integer state,
same hash for any thread count.

1. **Uniform grid rebuilt every step by counting sort.** Count per cell, prefix
   sum, scatter. Cell size = interaction radius, query 27 cells. (Hoetzlein
   5-10x over radix; Ihmsen: cell = radius is optimal.) Scatter in entity-index
   order so the result is identical for any thread count; do not use the
   atomic-counter build, whose slot order depends on timing.
2. **Physically reorder the hot SoA arrays into cell order**, not only an index
   list. This is the largest single CPU effect measured: plain grid query
   105.6 -> 38.1 ms (Ihmsen). Green reports the same on GPU.
3. **Morton cell keys, full re-sort only every ~100 steps.** Z-index sort was
   the fastest sort-based variant (43.1 ms). Hilbert is not worth it (0.54%,
   BioDynaMo). Between re-sorts the order is nearly sorted.
4. **SoA in fixed-size blocks sized to the cache, with few archetypes.** Unity
   uses 16 KiB chunks of at most 128 entities; fragmentation is the known
   failure (1.5 GB for 100k archetypes). Keep a small fixed set of layouts
   (castes/species), not per-entity component sets.
5. **Two-phase update: read last step's state, write the next, commit at the
   end.** Green double-buffers; FLAME GPU 2 makes agents interact only through
   message lists; BioDynaMo names commit-at-end as a determinism requirement.
   Evaluation order then cannot change results.
6. **Checkerboard or colouring for interactions that must change a neighbour**
   (eating, fighting, matter transfer). Noita: 4 rounds over 64x64 chunks with
   a bounded reach, no locks. Box2D: persistent graph colours. Both are
   race-free and deterministic by construction.
7. **Deterministic merge of per-thread output.** Thread-local buffers joined in
   a fixed order, or Box2D's OR-ed bit arrays. Integer sums are associative, so
   integer conservation totals do not depend on thread count; float sums do.
8. **Counter-based RNG keyed by (creature id, tick, purpose).** Suggested by
   BioDynaMo and by the event-keyed hashing paper. A draw then does not depend
   on which thread ran or how many draws came before.
9. **Skip work that cannot change anything.** Noita dirty rects; BioDynaMo
   omits collision force for static agents. For creatures the old ALife form of
   this is time slicing (Tierra, Avida): not every individual runs every tick.
   The 3.75 M creature-steps per core-second budget above makes this necessary
   at 1 M unless the per-creature step is tiny.
10. **Give the GPU the wide, uniform work; keep the integer ledger exact.**
    Batched per-creature controllers (Origin: 300k+ cells with nets at 60 fps
    on an RTX 3090; evolve runs all brains in one compute pass) and field
    updates (Flow-Lenia: 255 us/step at 128x128 on a T4) suit the GPU. For bit
    identity the GPU kernels must be integer-only, since float kernels differ
    from the CPU (5e-6 in evolve). Flow-Lenia's reintegration tracking is the
    reference for conserved transport on a grid; an integer version needs an
    explicit rule for remainders.

Also worth keeping in mind: on CPU, store the neighbour list once per step if
it is read more than once (Ihmsen: 190.1 -> 105.4 ms); FFT convolution only
pays above a kernel radius of roughly 10-32 cells (Chan; Heidelberg test).

## 4. Cautions

- "Millions" is often an unmeasured README claim (ALIEN, flecs, Pandora). Only
  FLAME GPU 2, Hoetzlein, Ihmsen, BioDynaMo, Green, Madrona, Flow-Lenia and
  Origin gave a count together with hardware and a time.
- Most numbers are GPU datacentre or 24-72 core server numbers. None is from a
  hybrid P+E laptop CPU. I found nothing on P/E-core scheduling for
  simulations: unknown.
- None of the large systems states thread-count-independent bit identity. The
  ones that do (Box2D, evolve) are at hundreds to thousands of bodies.
- Biomaker CA grid sizes, JaxLife steps/s, Avida and Tierra speeds, and Bibites
  population limits were not found. The DIRT paper (GPU populations above 10^5
  in a gridworld, per a search snippet) could not be fetched (file too large),
  so it is unread: https://wcrisdog.github.io/home/files/papers/DIRT_longer.pdf
- Secondary sources are marked as such above. Forum and Reddit figures are
  anecdotes.

## 5. What would be genuinely new in an engine like ours

Written conservatively. Each ingredient below is prior art on its own (grid +
counting sort, Morton order, SoA/ECS, checkerboard updates, counter-based RNG
per agent, fixed-point lockstep, mass-conserving CA, energy economies). Only
these combinations turned up no prior example in my searches:

1. **Exact integer conservation of matter and energy, plus the same state hash
   for any thread count, at 100k or more individually simulated creatures.**
   Searched: "artificial life simulation deterministic integer fixed-point
   exact conservation energy matter bit-reproducible independent of thread
   count million agents" (extended mode), plus the per-system reads above.
   Found: small deterministic fixed-point ALife kernels with no scale or
   conservation statement (https://github.com/ctadros1/The-Genesis-Engine ,
   https://github.com/omarpatel/connectome-life-sim ,
   https://github.com/Dicklesworthstone/rust_scriptbots at hundreds to
   thousands of agents); evolve with thread-count bit identity and conserved
   soil and water at ~790 creatures, energy not strictly conserved; and large
   systems (ALIEN, Origin, Pandora, FLAME GPU 2, BioDynaMo) with no such
   claims. The combination at this scale was not found.
2. **A 3D world with 100k or more creatures, each with its own evolved
   controller and lifetime plasticity.** Searched: "3D artificial life
   simulation hundred thousand OR million creatures each with evolved neural
   network brain real time GPU CPU individual-based" (extended) and "evolution
   simulator Hebbian plasticity lifetime learning evolved neural networks large
   population 100000 agents GPU". Found: that scale only in 2D (Origin, ALIEN)
   or gridworlds (DIRT, unread); 3D only at hundreds (Polyworld ~300, evolve
   790). Not found in 3D at that scale.
3. **An ALife engine whose hot loops are hand-written x86-64 assembly.**
   Searched: "artificial life simulator written in assembly language x86-64
   SIMD hand-written creatures neural network evolution". Found only systems
   where the *creatures* are assembly-like programs on a virtual machine
   (Tierra, Evochora), not the engine. This is an implementation choice, not a
   scientific result, and one search is weak evidence of absence.
4. **Cache residency as a measured design rule (block size tuned to L1/L2,
   page-locked arena) for an ALife workload.** Searched: "agent-based
   simulation cache blocking L1 L2 resident structure-of-arrays tiles agents
   per block AVX2 million agents CPU". Found AoSoA tiling for a pedestrian
   model and BioDynaMo's allocator and Morton sorting, but no source that sizes
   agent blocks against L1/L2 and reports the effect. This is standard HPC
   cache blocking applied to a new domain, so the claim is modest.

Not claimed as new: anything in section 3. Every item there has a cited prior
example.

Limits of these searches: a handful of English-language web queries on one day.
No Zenodo, patent or thesis-database search was done. Treat every item above as
"no prior example found", not "no prior example exists".
