# World models for a coarse-voxel alien world on an integer engine

Key: `world-models`. Written 2026-11-17. Web research plus one small local probe.

How to read this note:

- Every external fact carries a URL. Most pages were read through a fetch tool
  that summarises the page with a small model, so numbers quoted from web pages
  are second-hand unless marked **[read]** (I extracted the PDF text myself).
- **[unverified]** means my search did not confirm it from a primary source.
- **[PROPOSAL]** marks my own design suggestions. They are not sourced facts.
- "Ran" appears only in section 7, with the command and output.

---

## 1. Falling-sand games: what they do and how they keep matter

| System | Cell state | Update scheme | Conservation |
|---|---|---|---|
| Noita (2019 GDC talk, summarised by 80.lv) | one material per pixel | bottom-up scan; world split into 64x64 chunks, each with a dirty rectangle; chunks updated in a 4-pass checkerboard so a pixel can move up to 32 px out of its chunk without locks | movement is by swapping pixels (liquids compare density), so moves keep counts; fire and explosions convert or remove pixels. https://80.lv/articles/noita-a-game-based-on-falling-sand-simulation/ (one forum commenter says 32x32 regions instead: https://news.ycombinator.com/item?id=31309616) |
| Sandspiel | 4 bytes per cell: species, two registers, a clock byte | scan order with a per-tick clock byte to stop double updates; random horizontal offset in {-1,0,1}; wind is a separate GPU Navier-Stokes solve coupled both ways; 300x300 grid | none claimed. https://maxbittker.com/making-sandspiel |
| The Powder Toy | particles plus coarse air grids for pressure, velocity x/y and ambient heat (floats) | pressure from velocity divergence, velocity from pressure gradient, 3x3 kernel smoothing, semi-Lagrangian advection, vorticity confinement (default 0.1), Boussinesq buoyancy; values clamped | not conservative: explicit loss and clamp terms. https://github.com/The-Powder-Toy/The-Powder-Toy/blob/master/src/simulation/Air.cpp (the time-step and loss constants live in another header that I could not fetch) |
| Oxygen Not Included | one element per cell with mass and temperature (floats) | per 0.2 s tick: q = dT x dt x k x 1000; k is the geometric mean of the two conductivities (minimum for insulated tiles); multipliers x25 gas-solid, x625 liquid-liquid | heat flow is skipped below 1 degree or 0.1 DTU and capped so neither side moves more than a quarter of the difference per tick; a float-precision rule cancels the exchange when one side would not change. https://oxygennotincluded.wiki.gg/wiki/Thermal_Conductivity |
| Dwarf Fortress | liquid depth 0..7 per tile; integer temperatures (water freezes at 10000, boils at 10180) | "pressure" is a movement rule: liquid tries sideways and down first and may teleport through full tiles, never above the first 7/7 tile on the path; 1/7 liquid stops moving and evaporates | integer, coarse, deliberately lossy (evaporation). https://dwarffortresswiki.org/index.php/Flow , https://dwarffortresswiki.org/index.php/Pressure , https://dwarffortresswiki.org/Temperature |
| Hahm, "A Framework of Artificial Matter" (2023) **[read]** | one element per voxel, 3D | 2x2x2 Margolus blocks, two staggered partitions alternated each step; rules Gravity, Slide, Flow, Unbound; solids displace liquids, liquids displace gases; reactions such as Lava + Water -> 2 Stone | strictly conserves voxel count. Energy is not conserved; the author names that as the main open issue. https://cis.temple.edu/tagit/publications/TAGIT-TR-18.pdf |

Conservation tricks worth taking:

1. **Move by swap or by antisymmetric transfer.** If the only primitive is
   "A loses n, B gains n", totals cannot drift.
2. **Block (Margolus) partitioning.** Space is cut into 2x2 (or 2x2x2) blocks,
   the partition shifts every step, and the rule maps a block to a block. A
   block rule that permutes its contents conserves particle number by
   construction; if it is a bijection the automaton is reversible.
   https://en.wikipedia.org/wiki/Block_cellular_automaton
   The plain Margolus sandpile conserves particles but makes sand stick to
   left-hand slopes; a paper fixes that with an extra shift so the partition
   cycles over four positions. https://arxiv.org/pdf/2008.06341
3. **Disjoint regions per pass** (Noita's 4-pass checkerboard) remove write
   conflicts without locks. For us the same idea also gives thread-count
   independence, because nothing in a pass depends on visiting order.
4. **A per-tick clock mark** (Sandspiel) is the cheap fix for double updates in
   scan-order schemes; partitioned schemes do not need it.

Theory behind "counts are kept":

- Number-conserving cellular automata have a necessary and sufficient
  condition (Boccara and Fuks). https://arxiv.org/abs/adap-org/9905004
- Such rules can be written as sums of two-cell "flows" of integers.
  https://arxiv.org/abs/0809.0355

## 2. Exactly conservative integer transport

### 2.1 Prior art (this is an old, well-studied problem)

- **Discrete load balancing.** Tokens are indivisible, each node averages with
  neighbours, fractions must be rounded. Rabani, Sinclair and Wanka (1998)
  compare the integer process with the ideal continuous one; with
  always-round-down the leftover imbalance can be as large as the graph
  diameter **[unverified wording, from a search summary]**. Randomized rounding
  closes most of the gap: Friedrich and Sauerwald report roughly a square-root
  improvement, and Sauerwald and Sun (FOCS 2012) show the integer and continuous
  processes then differ very little. https://arxiv.org/pdf/1201.2715 ,
  https://typeset.io/pdf/near-perfect-load-balancing-by-randomized-rounding-3rcjuqh0j2.pdf
- **Balancing through matchings** (each node pairs with one neighbour per
  round): a 2025 paper reports constant discrepancy in the same number of
  rounds as the continuous case. https://arxiv.org/pdf/2510.15473v1
- **Rotor-router (Propp machine).** A deterministic replacement for random
  walk; chip counts stay within a constant of the expected random-walk counts
  on Z^d (Cooper and Spencer), about 2.29 on the infinite path.
  https://arxiv.org/abs/math/0602300
- **Integer lattice gases** (Boghosian, Yepez, Alexander, Margolus, Phys. Rev.
  E 55, 4137, 1997): each lattice direction holds an integer particle count.
  Conservation is exact and the scheme is unconditionally stable; the paper
  contrasts this with lattice Boltzmann, where real-number round-off breaks
  exact conservation and stability becomes conditional.
  https://arxiv.org/pdf/comp-gas/9602001
- **Quantised numerical methods** (Park, Ha, Kang, arXiv April 2026): the
  conservative operator is applied as an antisymmetric integer transfer rule;
  the abstract claims exact conservation, monotonicity and TVD for scalar laws
  with monotone flux splitting, tested on Burgers and a Sod shock tube.
  https://arxiv.org/abs/2604.06947
- **Mass-conserving continuous automata.** Flow-Lenia keeps total mass per
  channel constant by moving mass with reintegration tracking; a follow-up
  (MaCE) notes the flow must stay about one cell per step or mass is lost or
  the flow clipped. Both are floating point.
  https://arxiv.org/pdf/2212.07906 , https://arxiv.org/pdf/2507.12306

### 2.2 The two failure modes of naive integer diffusion

Take flux = k x (u_i - u_j) on each face, in integers.

- **Dead band.** Truncation stops all flow once neighbouring differences are
  below 1/k. A frozen staircase remains. Section 7 measures a leftover spread
  of 216 units across 32 cells with k = 1/8.
- **Negative cells.** Any rounding that can round up lets a nearly empty cell
  send more than it has when several faces round up together. Section 7
  observes -4.

### 2.3 Schemes that avoid both **[PROPOSAL, tested in section 7]**

- **Pairwise sweeps (dimension exchange).** One tick is six sweeps: x-even,
  x-odd, y-even, y-odd, z-even, z-odd pairs. Each cell is in at most one pair
  per sweep. The pair moves `t = round(c x (hi - lo))` units from the fuller to
  the emptier cell with `c <= 1/2`. Both cells stay between their old values, so
  no cell can go negative and no value can overflow. Pairs are disjoint, so
  the result cannot depend on thread count or visiting order.
- **Rounding.** Round the odd half-unit up or down with one bit from the
  counter-based RNG keyed by (tick, pair id). No extra state.
- **Alternative with state:** keep a signed remainder per face and add it to
  next tick's difference (error diffusion). It removes the dead band too, but
  costs 3 bytes per cell and is not provably non-negative.
- **Sweep-order bias.** A fixed order favours one axis and one parity. Rotate
  the order with the tick counter (same idea as the four-phase shift in
  https://arxiv.org/pdf/2008.06341).

### 2.4 Moving matter must move its energy

When n units of species s leave cell i they must carry heat. Take
`e = floor(E_i x (n x c_s) / C_i)` from the donor and add the same `e` to the
receiver. The temperature of the donor then changes only by rounding. The same
antisymmetric rule keeps the energy ledger exact.

## 3. Heat and phase transitions

- **Store energy, derive temperature.** The conserved field is integer
  thermal energy E. Temperature is `T = E / C` with `C = sum(m_s x c_s)`.
  Temperature is never written directly.
- **Conduction between unequal cells.** Pair exchange of
  `q = k_face x (T_i - T_j)`, capped at the amount that would equalise the two
  temperatures, `(T_i - T_j) x C_i x C_j / (C_i + C_j)`. Oxygen Not Included
  uses a cruder cap (a quarter of the difference) and a geometric-mean
  conductivity. https://oxygennotincluded.wiki.gg/wiki/Thermal_Conductivity
- **Avoid thresholds that delete flow.** ONI's "no transfer under 1 degree or
  0.1 DTU" rule is a dead band by design. Randomized rounding makes it
  unnecessary.
- **Latent heat as formation enthalpy [PROPOSAL].** Give every species slot an
  integer enthalpy h_s per unit. Total energy = E + sum(m_s x h_s). A phase
  change is then just a reaction: `1 solid -> 1 liquid` with
  `dE = -(h_liquid - h_solid)`. Melting cools the cell, freezing warms it,
  condensation releases heat aloft, and the ledger balances with no special
  case. Combustion and photosynthesis-like reactions use the same rule.
- **Separate slots for phases that matter.** Dwarf Fortress makes melting and
  freezing points coincide exactly (https://www.dwarffortresswiki.org/index.php/Heat).
  Keeping liquid and vapour as separate slots with a rate-limited transition
  permits supersaturation, which a storm model needs.

## 4. Artificial chemistries and integer reaction networks

- **Definition.** An artificial chemistry is a triple: a set of molecules, a
  set of collision rules, and a reactor algorithm (Dittrich, Ziegler, Banzhaf,
  2001). https://typeset.io/pdf/artificial-chemistries-a-review-2jvfmhq9oc.pdf
- **Hutton 2002, Squirm3 [read].** Atoms sit on a 2D grid, one per square.
  Six fixed types (a to f), ten states. Atoms random-walk, bonded atoms must
  stay inside each other's Moore neighbourhood, reactions happen between von
  Neumann neighbours. Eight rule schemas (188 concrete reactions out of 7200
  possible input pairs) give template replication of any chain e8-(a..d)1-f1.
  Atoms are never created or destroyed by reactions, so replication consumes
  the free "soup"; a 20x20 world with 75 extra atoms stalls at 11 copies after
  3544 steps because e0 and b0 atoms run out. Evolution toward shorter chains
  appears with a periodic flood (T = 2000 to 20000, 100x100 world).
  Replicators arise from a random soup when states are randomised at 1e-5 per
  atom per step. After the shortest replicators win, nothing further evolves.
  https://faculty.cc.gatech.edu/~turk/bio_sim/articles/hutton_rep_molecules.pdf
- **Fontana's AlChemy.** Molecules are lambda-calculus expressions; a reaction
  applies one to another and reduces to normal form with a step bound. It is a
  well-stirred, non-spatial reactor with no matter conservation.
  https://arxiv.org/pdf/0901.0318 **[attribution from a search summary]**
- **Gray-Scott (Pearson 1993).** U + 2V -> 3V, V -> P; Du = 2e-5, Dv = 1e-5,
  domain 2.5 x 2.5, periodic, forward Euler, 200000 steps; a 256x256 grid is
  reported by a secondary summary **[unverified]**.
  https://arxiv.org/pdf/patt-sol/9304003
  It is an open system: U is fed and V removed at rate constants. In a closed
  ledger the feed and the waste must come from and go to explicit reservoirs.
- **Reactive lattice gases** (Boon, Dab, Kapral, Lawniczak, Physics Reports
  273, 1996): particle-level automata that reproduce bistable, excitable,
  oscillating and Turing-pattern chemistry. https://arxiv.org/abs/comp-gas/9512001
- **Integer counts and negative populations.** In stochastic chemical
  kinetics, plain Poisson tau-leaping can fire a reaction more often than its
  reactants allow; Cao, Gillespie and Petzold (2005) and the earlier binomial
  variants bound the firings. https://www.escholarship.org/uc/item/04x399h1

Rules for our reactions **[PROPOSAL]**:

1. Each species has an integer composition vector over a few conserved
   elements. The loader rejects any reaction whose stoichiometry does not
   balance every element.
2. Extent per cell per tick = min over reactants of `floor(m_r / nu_r)`, then
   scaled by a rate table indexed by temperature, then capped so thermal energy
   cannot go below zero for endothermic reactions and so products fit.
3. Rounding of the rate uses the counter RNG keyed by (tick, cell, reaction).

## 5. Toy weather on a coarse grid

What exists:

- **Cellular cloud models.** Dobashi et al. (SIGGRAPH 2000), extending Nagel
  and Raschke: three boolean fields per voxel (humidity, activation, cloud),
  boolean transition rules, wind as a shift of the fields. Fast, but nothing
  is conserved. https://history.siggraph.org/?p=118250 ,
  https://main.h.kobe-u.ac.jp/kobe_studio_seminar/articles/published/20120330_dobashi/animation_dobashi.pdf
- **Lattice gases.** FHP recovers incompressible Navier-Stokes only if the
  lattice has enough symmetry (hexagonal in 2D); the 3D version needs the
  face-centred hypercubic lattice. https://complex-systems.com/abstracts/v01_i04_a07 ,
  https://cis.temple.edu/tagit/publications/TAGIT-TR-18.pdf (section 2.1)
  On a cubic 64x64x32 grid a classic lattice gas would be anisotropic and very
  noisy; the integer variant reduces noise but stays viscous.
  https://arxiv.org/pdf/comp-gas/9602001
- **Lattice Boltzmann.** The fastest route to real fluid motion, but floating
  point. FluidX3D reports, for D3Q19 on a 256^3 empty box: RTX 4090 5624
  MLUPs/s (FP32), RTX 5090M laptop GPU 4830, RTX 5070 desktop 3658, Core
  i7-13700K CPU 504; 55 to 93 bytes per cell. https://github.com/ProjectPhysX/FluidX3D
  It breaks laws 5 and 6 (floats, no exact conservation) and exceeds the byte
  budget.
- **Practitioner advice.** A coarse fluid solve is usually easier to control
  than invented cellular rules, and ad hoc rules can remove all clouds from a
  world. https://redlib.hbubli.cc/r/proceduralgeneration/comments/1htspwa/2d_weather_simulation_with_cellular_automata

Recommended ladder **[PROPOSAL]**:

- **Level 0, pressure-driven exchange (start here).** Gas moves between paired
  cells in proportion to the pressure difference, with pressure derived from
  gas amount and temperature. There is no momentum, so it behaves like a
  heavily damped atmosphere. It is exact, cannot go negative, and has no time
  step limit. "Wind" is the signed gas flux through each face in the last
  tick, which is a real, measurable transport.
- **Level 1, moist convection.** Vapour rides the gas flux. Where vapour
  exceeds a saturation table at the local temperature it condenses to liquid
  droplets, releasing enthalpy (section 3), which warms the cell, lowers
  density and drives more upward exchange. Droplets fall in the vertical
  sweeps. This is the causal core of a storm.
- **Level 2, charge and lightning.** An integer charge field with zero world
  total. Falling condensate carries charge downward. When the charge
  difference along a column exceeds a threshold, a discharge neutralises it,
  deposits energy along the path and converts some gas to a high-enthalpy
  "ion" species that decays back and releases the heat.
- **Level 3, momentum (optional, has an open accounting problem).** A signed
  momentum per axis gives inertia, jets and rotation. Then the time step is
  limited: the general explicit bound is dt x sum(u_i / dx_i) <= C_max, with
  C_max about 1. https://en.wikipedia.org/wiki/Courant%E2%80%93Friedrichs%E2%80%93Lewy_condition
  The relevant speed is the pressure-wave speed (sqrt(g x H) for shallow water,
  **[unverified]** in this search). Kinetic energy must then enter the energy
  ledger or friction will appear to create or destroy heat.

## 6. Stability limits in one table

| Mechanism | Limit | Source |
|---|---|---|
| Explicit simultaneous diffusion, 3D, equal spacing | alpha x dt / dx^2 <= 1/6 (1/2 in 1D, 1/4 in 2D) | https://ignite.byu.edu/cbe541/lectures/pde_stability , https://en.wikipedia.org/wiki/Von_Neumann_stability_analysis |
| Same, integers | also need outflow <= content after rounding; round-up rounding violates it near empty cells | section 7, row B |
| Pairwise sweeps | coefficient <= 1/2 per pair; no further limit | section 7, rows D and E |
| Waves with momentum | dt x sum(u_i / dx_i) <= about 1 | CFL link above |
| Semi-Lagrangian mass transport | flow about one cell per step or mass is lost or clipped | https://arxiv.org/pdf/2507.12306 |
| Conduction between unequal heat capacities | cap at the equalising amount | section 3 |
| Reactions | extent <= floor(m_r / nu_r); thermal energy stays >= 0 | section 4 |
| Field width | see section 9 for the overflow bounds | section 9 |

## 7. Local probe: five integer diffusion schemes (ran 2026-11-17)

File: `S:\sota\world-models\intdiff.c`. One scalar field on a closed 64x64x32
box, lower half 3000 units per cell, upper half empty, 8000 ticks. The random
bits come from a murmur-style counter hash standing in for Philox.

Command (Git Bash, in `S:\sota\world-models`):

```
gcc -std=c2x -O2 -Wall -Wextra intdiff.c -o intdiff.exe   (no warnings)
./intdiff.exe
```

Output:

```
A jacobi r=1/8 truncate         sum 196608000->196608000 EXACT  min 1392 max 1608 spread 216  worst_min_during_run 0  3.54 s (3.4 ns/cell/tick)
B jacobi r=1/8 random rounding  sum 196608000->196608000 EXACT  min 1496 max 1504 spread 8  worst_min_during_run -4  7.15 s (6.8 ns/cell/tick)
C jacobi r=1/8 face remainder   sum 196608000->196608000 EXACT  min 1499 max 1501 spread 2  worst_min_during_run 0  4.77 s (4.6 ns/cell/tick)
D pair sweeps  truncate         sum 196608000->196608000 EXACT  min 1485 max 1515 spread 30  worst_min_during_run 0  4.30 s (4.1 ns/cell/tick)
E pair sweeps  random rounding  sum 196608000->196608000 EXACT  min 1499 max 1501 spread 2  worst_min_during_run 0  5.67 s (5.4 ns/cell/tick)
```

Reading:

- All five conserve the total exactly, as any antisymmetric transfer must.
- A (truncate) freezes with a spread of 216, about 7 units per cell over 31
  cells. This is the dead band.
- B (random rounding on simultaneous fluxes) flattens to a spread of 8 but
  drove a cell to -4. It is unusable for unsigned matter without a limiter.
- C (per-face remainder) and E (pair sweeps with one random bit) both reach a
  spread of 2. E is non-negative by construction; C merely did not go negative
  in this one run.
- Timings are noisy, parallel load, single thread, scalar `int32`, Core Ultra
  9 275HX. Rows B and C include a per-tick minimum scan. 3.4 to 6.8 ns per
  cell per tick is 0.45 to 0.9 ms per tick per field before any SIMD.
- Not tested: more than one field, unequal capacities, sweep-order bias,
  thread-count independence, Philox itself.

## 8. Real exotic environments to draw on

| Setting | Facts | Use in the world |
|---|---|---|
| Tidally locked planet | A climate-model study (Lobo et al., ApJ 945, 2023) finds a temperate band at the day-night terminator on water-limited, land-rich planets, but not on ocean worlds, because abundant water evens out day and night temperatures. https://www.sciencedaily.com/releases/2023/03/230316124434.htm , https://aasnova.org/?p=11530 | A permanent hot edge and cold edge with a habitable ring; keep the working fluid scarce or the gradient washes out. |
| Two suns | Kepler-16b: a Saturn-like planet on a 229-day orbit around two stars (0.69 and 0.20 solar masses) that orbit each other in 41 days. https://en.wikipedia.org/wiki/Kepler-16b | Two light sources with different periods give beat-pattern seasons and double shadows. |
| Titan | Surface about 1.5 bar and 90 to 95 K; methane forms clouds, rain, rivers, lakes and seas, the only active hydrologic cycle known besides Earth's; the surface liquid is a small fraction of the atmospheric methane. https://arxiv.org/html/2502.02556v1 , https://annualreviews.org/content/journals/10.1146/annurev-earth-060115-012428 | A non-water rain cycle where most of the working fluid is in the air. |
| Ammonia and hydrocarbon solvents | Ammonia is liquid from about 195 to 240 K at 1 atm and over a wider range under pressure; the 2007 National Academies report treats ammonia, and ammonia-water mixtures as antifreeze, as candidate habitats and rates methane and other exotic solvents as less likely on little data. https://www.nationalacademies.org/read/11919/chapter/8 , https://arxiv.org/pdf/2101.10341 | A cold brine that stays liquid far below water's freezing point. |
| Supercritical fluids | Supercritical CO2 has been proposed as a life-supporting solvent; a plausible window is above 31 C and about 100 bar; Venus's surface CO2 is supercritical but far too hot. https://www.ncbi.nlm.nih.gov/pmc/articles/PMC4206850/ , https://arxiv.org/pdf/2401.07296 | A deep layer where the liquid-gas boundary vanishes: one species slot with no surface tension rule. |
| Lightning | Juno saw "shallow lightning" on Jupiter from high clouds of ammonia-water solution, where ammonia acts as antifreeze, plus ammonia-water "mushballs" that carry ammonia downward. https://www.jpl.nasa.gov/news/shallow-lightning-and-mushballs-reveal-ammonia-to-nasas-juno-scientists/ | Storm charge separation in a non-water cloud; slush hail as a vertical transport path. |
| Tidal heating | Io's melting by tidal dissipation was predicted days before Voyager saw its volcanoes (Peale, Cassen, Reynolds, Science 203, 1979); later heat-flow estimates run 1.5 to 4 W/m^2 **[source page not pinned down]**. Enceladus emits about 15.8 GW from its south-polar fractures, far above the 1.1 GW long-term tidal estimate. https://ntrs.nasa.gov/citations/19790041081 , https://www.sciencedaily.com/releases/2011/03/110308144714.htm | A heat source from below that pulses with orbital phase, independent of sunlight; cryovolcanic vents. |

## 9. Recommended field design for 64x64x32 **[PROPOSAL]**

Grid: 64 x 64 x 32 = 131072 cells, z up, closed walls, explicit reservoirs for
star, space and interior. Structure of arrays, one plane per field.

### 9.1 Per-cell fields (32 bytes; 28 conserved, 4 derived)

| Field | Type | Bytes | Meaning | In state hash |
|---|---|---|---|---|
| `m[0..7]` | 8 x u16 | 16 | units of each species, each 0..4095 | yes |
| `E` | u32 | 4 | thermal energy | yes |
| `q` | i16 | 2 | electric charge (world total is zero) | yes |
| `w[0..2]` | 3 x i16 | 6 | level 0: signed gas flux through the +x, +y, +z faces last tick (a record). Level 3: momentum (state) | level 3 only |
| `T` | u16 | 2 | derived temperature E / C | no |
| `light` | u8 | 1 | derived light reaching the cell | no |
| `flags` | u8 | 1 | derived: dominant phase, saturation, discharge mark | no |

Totals: 32 bytes x 131072 = 4.19 MB. That is larger than one P-core L2 (3 MB)
and well inside L3 (36 MB). One u16 plane of one z-layer is 8 KB, so a sweep
that touches four or five planes of one layer works on about 40 KB at a time.

Lean variant (16 bytes, 2.1 MB, fits one P-core L2): `m` as 8 x u8, `E` u32,
`w` 3 x i8, `q` i8, no derived planes. The cost is coarse quantisation: with
255 levels the dead band and rounding noise are sixteen times larger relative
to full scale. Start with 32 bytes; shrink only if measurement demands it.

### 9.2 Number ranges (so nothing overflows)

- Species amount <= 4095 per slot; condensed (solid plus liquid) total per
  cell <= 4095, enforced by limiting transfers to the receiver's free room.
  The unsent part simply stays with the donor.
- Heat capacity per unit c_s in 1..15. Temperature 0..4095 in fixed steps
  (for example 0.25 K, covering 0 to 1024 K).
- Then E <= 8 x 4095 x 15 x 4095, about 2.0e9, below 2^32 (4.29e9).
- World totals fit u64. Keep running totals of each element, of total energy
  (thermal plus enthalpy) and of charge, and assert them every tick in debug
  builds.

### 9.3 Eight species (illustrative, not Earth-like)

| Slot | Name | Phase | Role |
|---|---|---|---|
| 0 | REG | solid | inert regolith, high heat capacity |
| 1 | CRY | solid | ice of the brine (ammonia-water analogue); terrain that can melt |
| 2 | BRN | liquid | brine; subsurface ocean and cryolava from tidal vents |
| 3 | LIQ | liquid | hydrocarbon (methane analogue); lakes and rain |
| 4 | VAP | gas | vapour of LIQ; the weather fluid |
| 5 | ATM | gas | inert carrier gas that sets pressure |
| 6 | ORG | solid | energy-rich organic haze that settles; the food base |
| 7 | ION | plasma | high-enthalpy product of lightning and hard light; decays to heat |

All four phases are present. Phase pairs that matter (CRY/BRN, LIQ/VAP) are
separate slots joined by enthalpy-carrying reactions. Compositions over two
or three abstract elements must be chosen so that every reaction balances;
real Titan photochemistry was not researched for this note.

### 9.4 Tick order (every step is a set of disjoint operations)

1. Reservoir exchange: light in on the lit side, heat out to space by a table
   in T, tidal heat in from below. Each is a transfer to or from a counted
   reservoir.
2. Vertical pair sweeps (z-even, z-odd): condensed matter settles, gases
   exchange by density, falling condensate carries charge.
3. Horizontal pair sweeps (x-even, x-odd, y-even, y-odd): gas by pressure
   difference, liquid by column height, small diffusion of every species. Moved
   matter carries its share of E (section 2.4). Record flux in `w`.
4. Conduction pair sweeps on E (can share the passes of steps 2 and 3).
5. Per-cell reactions and phase changes (section 4 rules).
6. Charge relaxation and discharge.
7. Recompute derived planes T, light, flags.

Rotate the sweep order with the tick counter. Random bits are drawn only from
the counter RNG keyed by (tick, cell or pair, purpose).

### 9.5 Lesion tests that should change outcomes (law 4)

- Set every h_s to zero: storms should weaken, because condensation no longer
  heats the air.
- Remove the tidal reservoir: brine should freeze and vents stop.
- Replace randomized rounding with truncation: gradients should freeze
  (section 7, rows A and D).

## 10. Cautions

- Bulk kinetic energy is outside the ledger at levels 0 to 2. That is exact
  for what is counted, but adding momentum later needs a rule for where its
  energy comes from.
- Pair sweeps with coefficient 1/2 mix one axis fully before the next. The
  bias from a fixed order was not measured here.
- Randomized rounding makes fields jitter by about one unit forever (spread 2
  in the probe). Anything that thresholds on a single unit will flicker.
- Gray-Scott style patterns need a feed and a drain. In a closed world they
  exist only where reservoirs or light supply the feed.
- Squirm3's lesson: a conservative chemistry starves replicators unless
  matter is recycled, and evolution stopped once the shortest replicator won.
- Web numbers here are second-hand except the two PDFs marked [read]. The
  Powder Toy time-step and loss constants, the 256x256 Pearson grid, the Io
  heat-flow source and the shallow-water wave-speed bound were not confirmed.

## 11. What would be genuinely new in an engine like ours

Written conservatively. The building blocks are all prior art: antisymmetric
integer transfer (lattice gases, number-conserving automata, the 2026
quantised method), randomized rounding and matchings (load balancing),
partitioned updates (Margolus), enthalpy bookkeeping (ordinary
thermochemistry), boolean cloud automata (Dobashi).

Items for which my search found no prior example:

1. **One voxel world in which matter, thermal energy, latent heat, chemical
   energy and charge are all integers in a single ledger that balances exactly
   every tick, including the external reservoirs.** The closest found: Hahm
   2023 conserves voxel count only and lists energy as future work; Oxygen Not
   Included and The Powder Toy use floats with clamps, thresholds and loss
   terms; integer lattice gases conserve mass and momentum exactly but carry no
   chemistry or phase change; the 2026 quantised method treats scalar laws and
   gas dynamics test problems.
2. **A coarse-grid storm chain (condensation heat, convection, charge
   separation, discharge, plasma species) run on exact integer accounting.** I
   found boolean cloud automata and floating-point fluid solvers, and nothing
   with a conserved ledger. This rests on few queries; confidence is low.
3. **The same world state hash for any thread count, obtained from disjoint
   pair sweeps plus a counter-based generator, across a multi-species
   reacting voxel world.** Each ingredient is known; I found no system that
   states this property for such a world. Noita's checkerboard is for speed
   and makes no determinism claim in the source I read.

What I searched (about 35 queries, generic terms only, one in extended mode):
falling-sand engines (Noita, Sandspiel, Powder Toy, Oxygen Not Included,
Dwarf Fortress); Margolus and block automata; number-conserving automata;
integer lattice gases; discrete load balancing with rounding; rotor-router;
Flow-Lenia mass conservation; "deterministic integer fixed-point voxel
simulation exact conservation of mass and energy"; "falling sand conservation
of energy latent heat integer exact"; reactive lattice gases; tau-leaping
negative populations; cloud cellular automata and coarse-grid game weather;
lattice Boltzmann performance. I did not search patents, game-developer
conference archives beyond one talk, or non-English sources, and I did not
read most papers in full. Treat items 1 to 3 as "not found", not as "does not
exist".
