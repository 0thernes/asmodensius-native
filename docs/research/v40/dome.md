# The Dome biosphere of the v40 lab (2D world): rules, constants, and what a 3D million-creature world must generalise

Agent key: `dome`. Date: 2026-11-17. Sources read (all read-only, `V:\`):

| File | Lines | Role |
|---|---|---|
| `V:\lab\src\biosphere\dome.ts` | 223 | the world: grid, light, hazards, tick, games, ledger, snapshot |
| `V:\lab\src\biosphere\god.ts` | 74 | world dials, creature dials, presets, ladder, payoff tables, dice |
| `V:\lab\src\biosphere\moonlabDice.ts` | 12 | optional dice source (simulated statevector, classical-seeded) |
| `V:\lab\src\biosphere\ecology_passes.ts` | 844 | `EcologyEngine` + `Archipelago` (passes 071-088), an optional second layer |
| `V:\lab\src\interface.ts` | 85 | Creature Lab Interface: the only surface between world and creature |
| `V:\lab\src\adapters\c001.ts` | 357 (hooks only) | how creature 001 implements the dome-facing hooks |
| `V:\creatures\001\organism\affect.ts` | 77 | persona and social attitudes |
| `V:\creatures\001\organism\organism.ts` | lines 1250, 1284-1340 only | death rule, honest signal, maternal provision, crevice squeeze |
| `V:\lab\viewer\server.ts` | 1068 | live viewer server: boot population, God-panel HTTP operations, persistence |

`V:\docs\BIOSPHERE.md` does not exist (the header of `dome.ts` cites "BIOSPHERE.md v0.1"; a search of `V:\docs` found
the word only inside REPORT/SUMMARY/RUNBOOK and others). The code is therefore the specification; everything below is
extracted from code, with line numbers. Nothing here was inferred from prose docs.

Legend for each rule: **Essence** (what the rule does causally), **Constants** (exact numbers), **3D / 10^6** (what the
native world must generalise, under the owner's laws: integer state, exact conservation, determinism for any thread
count, cache-resident SoA blocks).

---

## 0. One-paragraph summary

The Dome is a flat disk of radius 12 "units" overlaid by a 24 x 24 grid of 1-unit food patches (448 live cells). One world
tick is one simulated minute; a day is 1440 ticks. Food (kJ) regrows toward a per-cell cap at a rate proportional to
light, creatures eat directly from the cell under their body, and the only energy source is that photosynthetic growth
(plus energy injected when a creature is born). The world steps creatures **sequentially in array order**, calling a
fixed set of hooks on each (sense, exchange, harm, work, step, act, takeWaste, bud). Everything else (hazards, wind,
seasons, parasites, games, grazers, signals, crevices) is an optional dial that defaults to off. The default viewer
runs 5 creatures; the hard population ceiling anywhere in the code is 64.

## 1. Geometry and units

**Essence.** A bounded 2D arena with a coarse scalar field (food, soil) and continuous creature positions.

**Constants** (`dome.ts:8`, `DEFAULT_DOME`):

| Quantity | Value | Note |
|---|---|---|
| radius | 12 units | disk centred on (0,0) |
| cell | 1 unit | grid `n = ceil(2*radius/cell) = 24`, 576 cells, row-major `j*n+i` |
| live cells | 448 | cell centre inside the disk (computed, see section 20) |
| movement radius R | radius - 0.5 = 11.5 | creature positions are clamped to this |
| touchRadius | 1.2 units | contact / meeting distance |
| neighbour radius | 3 x touchRadius = 3.6 | social neighbour list |
| signal radius | 3.5 x touchRadius = 4.2 | honest-signal broadcast |
| tick | 1 simulated minute | `light()` uses `t % 1440`; "same 1 tick = 1 min clock as creature 001's SCN" |
| energy unit | kJ | patch food, creature stores, payoffs, work |
| temperature unit | deg C | neutral 20 |
| speed unit | units / tick | `speed (0..1) x body.maxSpeed / sqrt(gravity)` |
| time step of the viewer | 100 ms wall timer x `speed` ticks (default 10) | so 100 sim-min per wall-second by default |

Cell lookup clamps to the grid (`cellAt`, line 83), so a creature near the rim always maps to a valid cell, which may be
an "outside" cell holding food 0.

**3D / 10^6.** (a) The space becomes a 3D volume or a 2.5D terrain with height; the patch grid becomes a sparse voxel
or brick grid. At 1 creature per ~90 cells (5 on 448) the same density for 10^6 creatures means ~10^8 cells, which is
too many for cache residence if stored densely as doubles: use 16-bit or 32-bit integer fields per cell, bricked 8^3 or
16^3 so one brick fits L1/L2, and only tick bricks that are "awake". (b) Units must become integers: position in
fixed-point (e.g. Q16.16 units), energy in integer micro-joules or an abstract quantum so totals balance exactly (law 5).
(c) The disk wall becomes a general boundary (signed-distance or solid voxels). (d) The radii (touch 1.2, neighbour 3.6,
signal 4.2) define the spatial-hash cell size: a uniform grid with cell edge >= the largest interaction radius makes
every query a 27-cell (3D) lookup.

## 2. Tick order (the exact sequence of `Dome.step()`, `dome.ts:98-196`)

**Essence.** One world tick = environment update, then one pass over creatures in array order where each creature is
fully processed (sense, eat, damage, think, move, excrete, bud) before the next one starts, then world-level
post-passes.

1. `t++`; compute light `L` once; compute season factors (`seasonCos`, `seasonGrowth`, `seasonTemp`).
2. Scent marks decay: `neoMarks[k] *= 0.999` for the 16 shared sites.
3. **Patch pass** over all live cells: waste-garden mineralisation, then regrowth toward the cap (section 3).
4. **Creature pass**, `for hi in 0..popN-1` with `popN` frozen at the start (newborns wait in a queue):
   1. Death / respawn check (section 11). A dead creature is skipped entirely.
   2. Read the patch under the body; count `touch` (all other inhabitants within touchRadius, **dead ones included**).
   3. Meetings and neighbours (live others only): `meet(id)` if d < 1.2; neighbour record `{id, dx, dy}` if d < 3.6.
   4. `readMarks(neoMarks)`; optional `setLight(L)`.
   5. Local temperature, hazard level and vent angle, food plume (plume only when crevices or vents are enabled).
   6. `sense({light, foodHere, temperature, touch, t, hazardHere, hazardGradAngle, foodPlumeMag, foodPlumeAngle})`.
   7. `exchange({foodPatch})`: the creature takes food **directly out of the shared cell object**.
   8. Dial-driven insults in this order: toxicity harm, vent harm, electric stimulation, sensory load, dice kick,
      modulator shift, sensory-line noise, thermoregulation work, hypoxia work, CO2 arousal, radiation (harm + rewire),
      parasite drain, pH drain.
   9. Game-learning bookkeeping (`learnTick`, before or after the step depending on `rewardPhasic`).
   10. `creature.step(k)`: the brain/body runs `k` internal ticks (`k = timeWarp` inside the warp radius, else 1).
   11. Every 15 ticks, if `honestSignals`: `emitSignal(0.04)` and broadcast to live others within 4.2 units.
   12. `act()` -> heading and speed; locomotion work is charged; position integrates; wall, crevice, wind and
       singularity corrections are applied in that order.
   13. Waste: to the current cell's soil (waste garden), or to global detritus (recycling), or kept by the creature.
   14. Budding check every 60 ticks (section 10).
5. Append queued newborns to the population.
6. Wind direction random-walks: `windDir += 0.05 * (dice - 0.5)`.
7. Grazers step (if any).
8. Detritus recycling into occupied cells.
9. Every 60 ticks, if a game is selected: play contact games.

Two consequences that matter for the port:

- The pass is **Gauss-Seidel**: creature `i` eats from a cell before creature `i+1` senses the same cell, and creature
  `i+1` sees creature `i`'s **new** position while creature `i` saw `i+1`'s **old** position. Results depend on array
  order. Respawn reuses the dead creature's slot, so order is stable over time.
- Two RNG streams exist: `r` (mulberry32, seed = cfg.seed) for placement, heading jitter, kin choice, grazers, bud
  angle; and `dice` (mulberry32 of `(seed+101) * 2654435761`) for God-dice effects (kicks, sensory-line noise, rewire,
  wind, teleports, game pairing and learned strategy). Both are sequential streams consumed in creature order.

**3D / 10^6.** Sequential order-dependent updates cannot be both parallel and deterministic. The native world needs a
**two-phase (Jacobi) tick**: phase A reads only the previous tick's world state and writes per-creature *intents*
(eat request in quanta, move vector, emit signal, bud request); phase B resolves intents per spatial cell with a
deterministic rule (for example: sort claimants by creature id, grant proportional integer shares, remainder to the
lowest ids). Random draws must be counter-based: `Philox(seed, creature_id, tick, purpose)` so no draw depends on
iteration order or thread count. Slow cadences (15-tick signals, 60-tick budding and games) should stay as cadences,
ideally staggered by `id mod period` so the cost is spread instead of spiking on one tick.

## 3. Food patches (the only producer)

**Essence.** Each live cell relaxes toward a carrying capacity at a rate proportional to light; creatures and grazers
deplete it. This is a light-gated linear (not logistic) regrowth: `dF = growth * L * (cap - F)`.

**Constants.**

| Quantity | Value | Where |
|---|---|---|
| foodCap | 30 kJ per cell | `DEFAULT_DOME` |
| initial food | `cap * foodCapScale * refMul * toxMul * (0.5 + 0.5u)`, u uniform | constructor, line 24 |
| growth | 0.004 per tick at L = 1 | `DEFAULT_DOME` |
| regrowth | `g = max(0, growth * growthScale * refGrowMul * seasonGrowth * fert * L * (capEff - food))` | line 108 |
| capEff | `foodCap * foodCapScale * refCapMul * (1 + 0.5*min(2, soil))` (soil term only with wasteGarden) | line 108 |
| fert | `1 + 1.8 * min(2, soil)` (wasteGarden only) | line 105 |
| mineralisation | `0.008 * soil` per tick moves soil -> food (wasteGarden only) | line 104 |
| mean light over a default day | 0.3183 (= 1/pi; sum 458.4 over 1440 ticks, computed) | section 20 |
| crevice refugia on | cap x2.2 and growth x2.5 inside a crevice; cap x0.08 and growth x0.12 outside | lines 106-107 |
| vents on | cap and growth multiplier 2.4 in a safe oasis, 0.45 in a vent zone, 0.08 elsewhere | `ventFloraMultiplier` |

Growth is logged in `grown`; it is the world's only energy input besides births. Food is never destroyed by the world
(over-cap food simply stops growing because `g` is clamped at 0).

**3D / 10^6.** (a) Producers must be an integer field: `F` in quanta, growth computed as
`(rate_q * L_q * (cap - F)) >> shift` with the truncated remainder carried in a per-cell or per-brick fractional
accumulator so that long-run growth is exact and the logged input equals the sum of applied increments. (b) In 3D,
light must attenuate with depth/occlusion (a column or cone transmittance), so production is not uniform; this creates
vertical niches for free. (c) The single food scalar generalises to a small vector of conserved species (for example
light-fixed sugar, mineral, detritus), each an integer per cell. (d) Carrying capacity and growth multipliers by
"zone" (crevice, oasis, vent) generalise to a per-brick material/biome id indexing a small table, instead of distance
tests against hard-coded centres.

## 4. Light, day, season

**Essence.** One global scalar `L(t)` drives production and (optionally) the creatures' circadian input; a slow cosine
modulates growth and temperature.

**Constants** (`light()`, line 84; season, lines 99-101):

- Day = 1440 ticks. `dayLength` hours of light centred on noon (tick 720): `L = lightScale * sin(pi * (m - (720 - h)) / (2h))`
  with `h = dayLength * 30`, else 0. Default dayLength 12 gives dawn 06:00, dusk 18:00, peak 1 at noon.
- `lightMode`: "cycle", "LL" (constant `lightScale`), "DD" (constant 0).
- `lightToCreatures` (default false): when true the dome pushes `L` into the creature with `setLight`; when false the
  creature runs its own internal photoperiod, and `L` reaches it only through `sense().light`.
- Season: `seasonCos = cos(2*pi*t / seasonPeriod)`; growth multiplier `max(0.05, 1 + seasonAmp * seasonCos)`;
  temperature offset `8 * seasonAmp * seasonCos` deg C. Off by default (`seasonPeriod = 0`).

**3D / 10^6.** `L` becomes a field: sun direction + per-column transmittance, so light differs by position and depth.
Use a 1440-entry (or 256-entry) integer lookup table for the day curve and another for the season; no transcendental
functions in the hot loop. The "does the world entrain the creature or does the creature free-run" switch is worth
keeping as a lesion-able experiment (law 4).

## 5. Temperature

**Essence.** Temperature is not a field; it is a per-creature scalar computed from four terms, and its only effect is
an energy cost (work, ATP -> heat) proportional to the distance from the comfort point.

**Constants** (lines 123-124, 133):

- `localTemp = temperature + 5*L + seasonTemp - thermalGrad * (r / radius)` (radial term only when `thermalGrad != 0`).
- Cost: `work(0.0005 * |localTemp - (20 + 5L)|)` kJ per tick, charged only when `temperature != 20` or a gradient or
  season is active. The comfort point tracks the daily +5 L swing, so the day/night swing itself is free.
- The value is also passed to `sense().temperature`.

**3D / 10^6.** Make temperature a real coarse field (one value per brick) with sources (light, vents), sinks (depth,
altitude) and optionally creature heat. The lab already books grazer respiration as "heat" but never feeds it back;
in a conserved integer world, heat is the terminal sink of the energy ledger and can warm the local brick, which gives
huddling a real thermodynamic payoff.

## 6. Hazards

**Essence.** Three kinds: (i) uniform insults set by dials (toxicity, radiation, hypoxia, CO2, pH, parasites), (ii) two
spatial toxic vents with a linear fall-off and a sensed direction, (iii) movement anomalies (wind, singularity,
time-warp).

**Constants.**

| Hazard | Rule | Line |
|---|---|---|
| toxicity | `harm(toxicity)` per tick | 128 |
| toxic vents | centres (-4.5, 4.5) and (4.5, -4.5), radius 3.4; `level = sum(1 - d/3.4)`; `harm(toxicHazardZones * level)`; sensed `hazardHere`, `hazardGradAngle` (angle **toward** the vent) | 53-65, 129 |
| safe oases | centres (4.5, 4.5) and (-4.5, -4.5), radius 2.4 (food only) | 13-14 |
| radiation | `harm(radiation)` and `rewire(radiation, dice)` (silences that fraction of synapses) per tick | 136 |
| hypoxia | `work(0.01 * (0.21 - o2) / 0.21)` when o2 < 0.21 | 134 |
| CO2 | `stimulate("LC", 0.5 * co2 / 0.04)` when co2 > 0.0004 | 135 |
| parasites | `drain(parasites)` kJ per tick into world `parasiteBiomass` (never returned) | 137 |
| pH | `drain(0.004 * |ph - 7|)` into detritus | 138 |
| gravity | speed divided by `sqrt(gravity)`; locomotion work `0.002 * gravity * mass * sp` kJ | 153 |
| wind | drift `wind` units/tick along `windDir`, then clamp to R | 173 |
| singularity | pull `min(r, singularity / r)` toward the centre; inside 0.7 units the dice teleport the creature to a uniform random point of the disk | 174-175 |
| time-warp | inside `timeWarpR` (default 4) the creature runs `timeWarp` internal ticks per world tick (0 freezes it) | 141 |
| electric / sensoryLoad / quantumKicks / sensoryLinesR | direct drive into named brain parts or the 16 sensory lines | 129-132 |

**Crevice refugia (a terrain gate, `creviceRefugia`).** Four pockets at (+/-5.5, +/-5.5), radius 1.9, hold the rich
food (section 3). When a move would carry a creature from outside to inside a pocket, the world asks
`canSqueezeCrevice(0.55)` (aperture 0.55 of body width). Creature 001 answers by running its soft-body shell through a
nozzle test and paying up to 0.012 kJ of ATP as heat; it refuses if ATP < 1.15 or the soft-body module is lesioned
(`organism.ts:1326-1340`). On refusal the creature is placed 0.06 units outside the pocket edge, facing outward
(`dome.ts:155-172`); entries and blocks are counted. Leaving a pocket is free. With crevices on, founders are placed
within `min(3.2, 0.35 * radius)` of the centre instead of 0.8 x radius. In 3D this generalises to passability decided
by body geometry against voxel apertures (a per-creature size class compared with a per-voxel clearance value), with
the deformation work charged to the creature.

`ecologicalGradientAt(x, y)` (lines 40-52) combines the food plume with a vent repulsion of strength `2.5 + 8 * level`;
no caller exists in `V:\lab` (grep), so it is an unused helper.

**3D / 10^6.** Vents become point or volume sources in a diffusing toxin field (integer concentration per cell, with
decay), so "level" and "gradient" are read from the field in O(1) instead of looping over sources. Uniform insults stay
as global dials (cheap, branch-free multipliers). Direct brain stimulation dials address named parts of one specific
brain; in the native world they should address genome-defined channels, or be dropped as creature-specific. Parasite
biomass is a pure sink in the lab; for exact conservation it must be an explicit ledger account (and, better, a real
population that can die back into detritus).

## 7. Plumes, scent marks and signals

**Essence.** Three distinct "information at a distance" channels:

1. **Food plume** (`foodPlumeAt`, lines 66-82): a screened inverse-square sum over every cell,
   `w = F * exp(-d / screenLen) / (d^2 + 0.64)`, summed as vectors; returns magnitude and direction. `screenLen` is 4.5
   with crevices or vents on, 2.4 otherwise. Computed per creature per tick only when crevices or vents are on. Cost is
   O(creatures x cells).
2. **Scent marks** (`neoMarks`, lines 86-88): a shared array of 16 floats, **not spatial**. A creature hook may deposit
   +0.3 (cap 1) at one site at most once per 60 ticks; all marks decay x0.999 per tick (half-life about 693 ticks);
   every creature reads the whole array each tick. This is the only world state a creature-side "sandbox" hook may
   write. Grants and denials are counted.
3. **Honest signals** (lines 143-151): every 15 ticks each live creature may `emitSignal(0.04)`; if it returns
   `{site, conf, paidKJ}` it is delivered to every live creature within 4.2 units via `receiveSignal`. The signal is
   priced in kJ by the sender (the creature decides whether to pay).

**3D / 10^6.** (1) The plume must become a **diffused scalar field**: food cells emit an odour quantity, a few Jacobi
diffusion + decay sweeps per tick over awake bricks (integer, conservative or explicitly decaying), and a creature
reads the local value and a 6-neighbour finite-difference gradient: O(1) per creature, independent of world size.
(2) The 16 global marks become a small number of pheromone channels deposited at the creature's position into the same
kind of field (stigmergy), with integer decay; "16 sites" can survive as 16 chemical species only if memory allows, so
2 to 4 channels is more realistic. (3) Signals become events binned by spatial-hash cell: emit in phase A, deliver in
phase B to creatures in the 27 neighbouring cells within radius, with a fixed per-receiver cap (keep the k strongest,
ties by id) so cost is bounded in dense swarms. The energy price must be debited from the sender's integer ledger and
credited to heat.

## 8. Touch, neighbours and meetings

**Essence.** Three proximity relations, all computed by brute-force O(N^2) loops per tick:

| Relation | Radius | Includes dead? | Delivered as | Line |
|---|---|---|---|---|
| touch | 1.2 | yes | integer count in `sense().touch` | 118 |
| meeting | 1.2 | no | `meet(otherId)` once per other, every tick while in range | 119 |
| neighbours | 3.6 | no | `neighbors([{id, dx, dy}])`, the full list every tick | 119 |

There is **no collision**: bodies overlap freely; `body().radius` is not used by the world for separation. Identity is
by string id, which lets a creature keep per-individual memory (section 12).

**3D / 10^6.** A uniform spatial hash rebuilt each tick by counting sort (cell edge >= 3.6 units scaled) gives all three
relations from one 27-cell scan. Per-individual identity does not scale: a creature cannot hold a table over 10^6 ids.
Replace with (a) a small fixed-size associative memory per creature keyed by a hash of the other id (for example 8 to
16 slots, evict the weakest), and/or (b) recognition by *tag*: a few genome-derived or lineage-derived bits that are
sensed, so attitudes generalise to kin/colony/species rather than individuals. The neighbour list must be capped
(k nearest, deterministic tie-break by id). Deciding whether bodies exclude each other (soft repulsion) is new work:
the lab has none.

## 9. Energy exchange, waste, recycling, and the world ledger

**Essence.** The world tracks every kJ in a small set of accounts and checks that their sum changes only by logged
inputs.

- Accounts (`total()`, line 94): sum of patch food + patch soil + sum of creature `ledger().energy` + `detritus` +
  `parasiteBiomass` + `grazerBiomass` + `grazerHeat`.
- Logged inputs: `grown` (photosynthesis) and `born` (energy of creatures created by spawn, minus maternal provision).
- Check (`ledger()`, line 221): `worldLedgerError = |total - E0 - grown - born| / E0`. The viewer's `/healthz` reports
  "ready" only while this is below 1e-6. It is a floating-point residual, not an exact identity.
- Eating: `exchange({foodPatch})` hands the creature the cell object; the creature decrements `food` itself.
- Gifts and losses: `give(kJ)` (payoff, God "feed") books intake; `drain(kJ)` books negative intake and returns the
  amount actually removed.
- Work: `work(kJ)` moves ATP to heat inside the creature (locomotion, thermoregulation, hypoxia).
- Waste: `takeWaste()` returns excreted kJ. With `wasteGarden` it goes to the **soil of the cell the creature now
  stands on**; else with `recycling > 0` it goes to global `detritus`; else it is not collected (stays in the
  creature's own books).
- Recycling (line 195): each tick `recycling * detritus` is split **equally among the cells currently occupied by
  inhabitants** (dead ones included) and added to their food.
- Corpses: on respawn the corpse's whole energy goes to `detritus` (line 114). Without respawn a corpse keeps its
  energy forever.

**3D / 10^6.** This is the part to keep most faithfully, but exact: every account is a 64-bit integer; every transfer
is a (debit, credit) pair applied in the resolve phase; the invariant `sum(accounts) == E0 + inputs` is checked as
integer equality every N ticks, per brick and globally, and its value is part of the state hash. The "creature mutates
the world cell" pattern must go: the creature emits an eat *request*, the world grants. Heat must be an explicit sink
account (in the lab, creature heat stays inside the creature's energy figure and grazer heat is a world account).
Detritus should be local (a per-cell field deposited where the death or excretion happened) instead of one global pool
teleported to wherever creatures stand; local detritus + decomposer rate gives nutrient cycling with real spatial
structure. Parasite biomass and corpses need decay paths so no account is an eternal sink.

## 10. Reproduction: autonomous budding

**Essence.** A well-fed creature that has completed an internal cell-division event may split off one offspring next
to itself; the mother pays a fixed provision out of fat and glycogen. Reproduction is asexual in the dome
(`spawn001` accepts an optional `mate` for ontogeny recombination, but the dome never passes one).

**Constants** (`dome.ts:179-190`; `organism.ts:1303-1323`):

| Quantity | Value |
|---|---|
| check cadence | every 60 ticks, per creature, in array order |
| world gates | `budEnergyThreshold > 0`, `spawn` set, `pop + queued newborns < maxPop` |
| creature gate (world side) | `metrics().stores >= budEnergyThreshold` |
| provision asked | `maternalProvision(140, true)`: all-or-nothing 140 kJ |
| creature gate (body side) | alive; chemoton not lysed; one **unconsumed cytokinesis division** available; `max(0, fat - 180) + max(0, liverGly - 8) >= 140` |
| who pays | fat and liver glycogen pro rata; booked on the mother as intake -140 |
| child seed | `8000000 + cfg.seed * 1000 + births` |
| child energy | whatever the factory builds; the world logs `born += childEnergy - 140` |
| placement | 0.6 units from the mother at a uniform random angle, clamped to +/-R on each axis |
| child id / dials | `<motherId>.b<budCount>`; same species and creature dials as the mother |
| activation | newborns are appended after the creature pass, so they first act on the next tick |

Important: the child is constructed with its **full default energy** and only 140 kJ of that is paid by the mother; the
rest is created and logged as `born`. In the saved viewer state a newborn carries 566.3 kJ (`born / births`,
section 20), so a bud would be about one quarter parent-funded.

**3D / 10^6.** Offspring must be **entirely parent-funded** (law 5): the child's initial integer stores equal exactly
what the parent(s) debit; `born` then exists only for the founding population. Budding requests are intents resolved
in phase B; the population cap becomes a memory-arena fact (free slots in the SoA arena) plus ecological limitation,
not a dial. Placement must respect solid voxels and a per-cell occupancy bound. Stagger the 60-tick cadence by id. Sexual
reproduction (two-parent genome recombination on contact) is a natural extension the lab only stubs.

## 11. Death and respawn

**Essence.** The creature decides that it is dead (its own physiology); the world then freezes the body, and optionally
replaces it after a delay with a newborn in the same slot, keeping the population constant.

**Constants.**

- Creature death rule (001, `organism.ts:1250`): blood glucose < 0.9 or ATP < 1.0 for more than 120 consecutive ticks
  ("hypoglycaemia"); further causes exist (for example protein depletion). Reported through `alive()`.
- World (`dome.ts:112-117`): on first detection set `deathT`, `deaths++`, append `{id, seed, t, age}` to `deathLog`.
  A dead creature is not sensed, stepped or moved. It is still counted by `touch`, still occupies a cell for
  recycling, and `pairs()`/`playGames()` do not filter it out.
- Respawn: if `respawnAfter > 0` (dome default 0; viewer default 60 ticks, bounds 0..10000) and
  `t - deathT >= respawnAfter`: corpse energy -> `detritus`; `births++`; a parent may be chosen (section 12); the
  newborn gets seed `7000000 + cfg.seed * 1000 + births`, its energy is added to `born`, and it **takes over the dead
  creature's id, position, heading and dials** (`path` resets to 0).
- `deathRule: "off"` disables the check (a dead organism keeps being called).
- Observed in the saved viewer state (t = 192188, 5 creatures): 75 deaths, 75 births; age at death min 428,
  median 8043, max 67217 ticks (section 20).

**3D / 10^6.** No respawn-in-place: it is an immigration subsidy that hides extinction and injects energy. A corpse
becomes local detritus (its integer stores move to the cell, possibly over several ticks as decay), its arena slot goes
to a free list, and new individuals arise only from reproduction. For experiments that need a constant population, keep
a "Moran mode" switch but label it as a control condition. Slot reuse must be deterministic (lowest free index, or a
free list ordered by index) so the state hash is thread-independent, and ids must be generation-tagged so stale
references (attitudes toward a dead individual) cannot alias a newborn.

## 12. Lineage, inheritance and epigenetics

**Essence.** Lineage exists only if the installed `spawn` function accepts a third `parent` argument. On respawn the
parent is drawn uniformly at random from the same-species **survivors** (viability selection in a constant-size,
Moran-like population); with `neutralShadow` the draw includes the dead (a neutral-drift control after Bedau and
Packard). On budding the parent is the mother. The world never computes fitness.

**Wiring facts.**

- `dome.ts:115`: `kin = spawn.length >= 3 ? same-species others that are alive (or all, if neutralShadow) : []`.
- The **viewer** installs a 2-argument spawn (`server.ts:320`), so in the live viewer respawned creatures are fresh,
  unrelated seeds: no lineage, no inheritance. Lineage runs exist only in experiment scripts
  (`creatures/001/organism/qualities_v40.ts:567`, `scientific_rigor_suite.ts:1151`, `lab/affect-panel/main.ts`).
- Inheritance is implemented creature-side in `spawn001(seed, parent, baseOpts, mate)` (`c001.ts:328-356`):

| Channel | Rule | Mutation |
|---|---|---|
| genes (4 scalars) | copied from `parent.genes()` | x `exp(sigma * N(0,1))`, clamp [0.25, 2.5], only if `mutateGenesSigma > 0` |
| persona (6 traits) | `Affect.birth(rand, parentPersona, 0.1)` | additive Gaussian, scale `0.1 * (0.3 + 2.5 * hedge)`; see section 13 |
| developmental genome | `mutateGenome(parentDevGenome)` | creature-defined |
| epigenetic marks | parent's exported chromatin-mark array handed to the child as `inheritedEpi` | none (copied state) |
| germline bias | parent's `germlineBias` array handed over as `inheritedGermline` | none |
| soma-allocation gene | parent value + uniform(-0.03, 0.03), clamp [0.2, 0.9] | yes |
| embodied ontogeny pack | `recombineOffspring(parent, mate or null, ...)` | creature-defined |
| HEPTARCH temperament | each weight + uniform(-0.025, 0.025), clamp [0.7, 1.3] | yes |

  Each channel derives its own small PRNG from the child seed with a distinct multiplier
  (`seed*4049+7`, `seed*4073+3`, `seed*8831+i*97+13`, `seed*7901+19`, `seed*6151+i`), which is already a
  counter-style scheme.
- God "creature dials" at birth (`god.ts:69-74`, `birthOpts`): `mutationSigma` draws 4 genes as `exp(sigma * N(0,1))`;
  `lesionDice` flips one of 10 named mechanism lesions; `rewireFrac` silences that fraction of synapses on `add`.

**3D / 10^6.** (a) Genome = fixed-length integer array in SoA columns; mutation = `Philox(child_id, locus)` so it is
order-free. (b) Keep the two-layer inheritance explicitly: genome (mutates, recombines) and a parental-state vector
(epigenetic marks, provisioning) that is copied and then decays within the child's life; both must have lesion
switches so their causal contribution is measurable (law 4). (c) Selection must stay implicit (differential survival
and reproduction); no fitness function. (d) Keep a neutral-shadow control: a parallel run, or a tagged neutral locus
carried by every genome, so adaptive change can be told from drift. (e) Lineage records for 10^6 creatures: parent id
and birth tick as two integer columns; a full genealogy is an append-only log outside the hot loop.

## 13. Persona and social attitudes

**Essence.** Each creature carries a heritable six-trait temperament that drifts slowly with lived mood, plus a learned
valence toward every individual it has touched. The world's only roles are to report contacts (`meet`), report
neighbour offsets (`neighbors`), and read `persona()` at spawn time for inheritance.

**Constants** (`creatures/001/organism/affect.ts`):

- Persona fields: `bold`, `curious`, `reactive`, `vuln`, `hedge`, `riskScar`.
- Founders: `bold = clamp(0.5 + 0.2g, 0.05, 1)`, `curious = exp(0.25g)`, `reactive` as bold, `vuln = clamp(0.3 + 0.2g, 0, 1)`,
  `hedge = clamp(0.3 + 0.15g)`, `riskScar = clamp(0.5 + 0.3g, 0, 2)`, g standard normal.
- Inherited: additive noise `m * g` with `m = 0.1 * (0.3 + 2.5 * parent.hedge)` (a high-hedge parent scatters its
  offspring more widely: bet-hedging); `curious` is multiplied by `exp(m * g)` with floor 0.2.
- Lifetime drift every 60 ticks: `bold += 2e-4 * mood.valSlow`, `reactive += 2e-4 * (mood.aroSlow - 0.3)`.
- Social attitude: on every tick within touch radius, `others[id] += 0.1 * (feltValence - others[id])`, where
  `feltValence = delight + 0.5 * calm - fear - 0.5 * frustration` (how the creature feels right now, whatever the cause).
- Use in behaviour (`c001.ts:272`, `socialHeading`): among neighbours within 3.6 units pick the one with the largest
  `|stance|` above 0.05; head toward it if positive, directly away if negative. This is the **last** fallback heading,
  used only when no foraging site, no blindsight target and no plume heading is available.
- Touch also drives the insula: `stimulate("INS", 0.2 * touch)` (`c001.ts:172`); `neighbors` sets `localPeerCount`.
  The gaze-following branch in `neighbors` needs a `heading` field the dome does not send, so it never fires in the dome.
- The `others` map is keyed by id string and is never pruned.

**3D / 10^6.** Temperament traits become genome loci (integers) with the same three properties: heritable, mutation
width itself heritable (`hedge`), and slow lifetime drift. Per-individual attitudes become a bounded associative
memory (section 8) or attitudes toward sensed *tags*; valence must still be learned from the creature's own state at
contact time, never assigned by the world. Since respawn reuses ids in the lab, attitudes toward a dead individual
transfer to the unrelated newborn; generation-tagged ids remove that artefact.

## 14. Population limits

**Essence.** Population is fixed by the boot specification and kept constant by respawn; it can grow only through
budding up to `maxPop`.

| Limit | Value | Where |
|---|---|---|
| viewer boot population `N` | default 5, allowed 1..64 | `server.ts:278` |
| per-species boot count | parsed 0..64, clamped to 32 in `reset()` | `server.ts:237, 323` |
| `maxPop` (budding cap) | default 16, dial bounds 1..64 | `god.ts:54`, `server.ts:188` |
| snapshot restore | rejected if more than 128 inhabitants | `server.ts:259` |
| grazers | dial bounds 0..500 | `server.ts:180` |
| predators (EcologyEngine) | at most 6 | `ecology_passes.ts:581` |

Cost structure that sets the practical limit: touch, meetings/neighbours, signal delivery and contact-game pairing are
all O(N^2) per tick; the plume is O(N x 576); each creature step is a full spiking brain. The viewer's exact JSON
snapshot is 26.5 MB for 5 creatures (4.5 to 6.5 MB each, section 20).

**3D / 10^6.** Five to sixty-four individuals becomes 10^5 to 10^6: every per-creature world interaction must be O(1)
amortised (spatial hash + fields), per-creature state must fit a fixed SoA row (the lab's 5 MB JSON per creature must
become on the order of kilobytes or less: at 10^6 creatures, 1 KB each is already 1 GB), and "population cap" becomes
arena capacity. Carrying capacity should emerge from the energy budget: default light input here is about
`0.004 * 0.318 * (cap - F)` per cell per tick, a number the native world should reproduce per unit area so that
densities are comparable.

## 15. Contact games and world-side strategy learning (optional dial)

**Essence.** Every 60 ticks, pairs of creatures in contact play a 2x2 game whose payoffs are **harvested from the food
of the cell under the first player**; if the cell cannot cover both payoffs the round is skipped.

- Pairing: all pairs with distance <= 1.2 (O(N^2)); with `oneShot`, a dice shuffle pairs everyone at random dome-wide.
- Payoffs in kJ (row, column): PD `CC 2.2/2.2, CD 0/3.3, DC 3.3/0, DD 0.6/0.6`; stag hunt
  `CC 3.3/3.3, CD 0/1.1, DC 1.1/0, DD 1.1/1.1`.
- Strategy "fixed": cooperate iff the creature's dopamine level `DA > 0.25`.
- Strategy "learned": `p(C) = sigmoid(w)`, `w` starts at 0; the payoff also arrives as `stimulate("VTA", 0.5 * pay)`;
  after a 15-tick window `w += 30 * (meanDA - daBase) * (C ? 1 - p : -p)`, clamp +/-6; `daBase` is an EMA (0.02).
  The weight `w` lives in a `Map` **inside the dome**, not inside the creature.

**3D / 10^6.** Keep the principle "social payoffs are drawn from the shared local resource, never created". Drop the
explicit game table and the world-held policy: cooperation and defection should be expressed by ordinary actions
(share, take, guard) whose energy flows are resolved by the same integer transfer rules, with any learning inside the
creature's own plastic state (laws 3 and 4).

## 16. Grazers (optional dial)

**Essence.** A second, non-neural consumer that competes for patch food and feeds the detritus loop.

- `grazers` agents appear at uniform random points; each tick: random step +/-0.1 per axis, clamp to R, eat
  `min(0.03, food)` kJ from its cell into a pooled `grazerBiomass`.
- Pool dynamics per tick: 1 % of biomass respired into `grazerHeat`, 0.2 % shed into `detritus`.
- Grazers are not born and do not die; the dial sets their number.

**3D / 10^6.** A cheap "brainless" trophic level is valuable at scale: implement as a field (grazer density per cell
with integer biomass, diffusion and logistic consumption) or as minimal particles sharing the creature arena with a
null brain. Heat goes to the terminal sink account.

## 17. The God panel (operator operations)

**Essence.** A set of world dials (global scalars read by the tick), presets, time controls, and five per-creature
interventions. The design rule in the code is that every operation acts through the creature interface and conserves
energy ("they move energy between stores, never create it").

**17.1 World dials** (`god.ts` `WorldDials`; HTTP bounds from `server.ts:161-198`). Default in brackets.

| Dial | Bounds | Effect |
|---|---|---|
| foodCapScale [1] | 0..10 | multiplies the per-cell food cap |
| growthScale [1] | 0..20 | multiplies regrowth rate |
| dayLength [12] | 0..24 h | length of the lit part of the day |
| lightScale [1] | 0..5 | peak light |
| lightMode [cycle] | cycle / LL / DD | cycle, constant light, constant dark |
| lightToCreatures [false] | bool | world light entrains the creature's clock |
| gravity [1] | 0.05..20 | speed / sqrt(g); locomotion cost x g |
| temperature [20] | -30..80 C | thermoregulation cost |
| thermalGrad [0] | -30..30 C | centre-to-rim temperature drop |
| seasonPeriod [0], seasonAmp [0] | 0..100000 ticks, 0..1 | seasonal growth and temperature swing |
| toxicity [0] | 0..0.2 | tissue damage per tick |
| toxicHazardZones [0] | 0..0.5 | enables the two vents (and the oasis/vent food map); damage scale |
| radiation [0] | 0..0.2 | damage + synapse silencing per tick |
| o2 [0.21], co2 [0.0004] | 0..1 | hypoxia cost; CO2 arousal drive |
| ph [7] | 0..14 | digestive loss into detritus |
| parasites [0] | 0..0.5 kJ/tick | drain into parasite biomass |
| recycling [0] | 0..1 | fraction of detritus returned to occupied cells per tick |
| wasteGarden [false] | bool | waste fertilises the local cell's soil |
| grazers [0] | 0..500 | number of grazer agents |
| wind [0] | 0..2 units/tick | drift along a slowly turning direction |
| singularity [0] | 0..10 | central attractor + dice teleport |
| timeWarp [1], timeWarpR [4] | 0..20, 0..30 | internal ticks per world tick inside the radius |
| electric [null] | part id + amount 0..20 | constant drive into one brain part |
| sensoryLoad [0] | 0..10 | drive into the sensory sheet |
| sensoryLinesR [0] | 0..1 | dice noise written to the 16 sensory lines |
| quantumKicks [0] | 0..1 | per-tick probability of a dice pulse (up to 3) into one of 8 brain parts |
| game [none], gameStrategy [fixed] | none/pd/stag, fixed/learned | contact games |
| honestSignals [false] | bool | priced peer signals every 15 ticks |
| creviceRefugia [false] | bool | four food-rich pockets that require a body squeeze |
| budEnergyThreshold [0], maxPop [16] | 0..2000 kJ, 1..64 | autonomous budding and its cap |
| deathRule [organism], respawnAfter [0; viewer 60] | organism/off, 0..10000 | life cycle |
| neutralShadow [false] | bool | uniform (drift-control) parent sampling |
| oneShot [false] | not settable over HTTP | random dome-wide game pairing |

**17.2 Presets and ladder** (`god.ts:56-64`). `utopia` (cap x2, growth x3, gravity 0.5), `baseline`, `harsh`
(cap 0.3, growth 0.3, gravity 2, toxicity 0.0005), `hell` (cap 0.05, growth 0.05, gravity 3, toxicity 0.003,
sensoryLoad 2, quantumKicks 0.2). The 7-rung `LADDER` interpolates linearly from utopia to hell in `x = i/6`:
cap `2 -> 0.05`, growth `3 -> 0.05`, gravity `0.5 -> 3`, toxicity `0 -> 0.003`, temperature `20 -> 45`, o2
`0.21 -> 0.09`, radiation `0 -> 0.001`, parasites `0 -> 0.004`. Applying a preset first resets all dials to base.

**17.3 Per-creature operations** (`POST /god`, `server.ts:812-860`):

| act | Parameters | Effect |
|---|---|---|
| stimulate | part (must exist), amount 0..50 | inject drive into a named part |
| shiftModulator | DA / 5HT / ACh / NE, amount -2..2 | shift a neuromodulator level |
| harm | amount (0, 1] | tissue damage |
| feed | amount (0, 50] | `min(amount, food under the creature)` moves from the cell to the creature (conserving) |
| lesion | coupling id, gain 0..2 | scale one named pathway (0 = cut) |

**17.4 Time and session controls.** `POST /control`: `speed` 0..200 ticks per 100 ms frame, `paused`, `reset` (rebuild
the dome from the seed). `GET /inspect?tool=`: state, viability, apex, weights, motorLens, pci, qrng (the last three
build fresh replicas from the creature's seed). `GET /stream`: a full JSON world frame every 100 ms. `GET /healthz`:
ready only while the world ledger error is below 1e-6. State is saved every 30 s as an exact snapshot and restored on
start. Creature dials (`modShift`, `rewireFrac`, `lesionDice`, `mutationSigma`) exist in `god.ts` but have no HTTP
endpoint.

**17.5 Dice.** God randomness comes from a `DiceSource` with a provenance label: "classical-seeded" (default
mulberry32), "moonlab-sim" (a simulated 3-qubit statevector whose outcome is drawn with a seeded classical stream; the
file states it is not a hardware random source), and a planned "qrng" label that is not wired.

**3D / 10^6.** (a) Dials become one small integer struct, double-buffered: the operator writes the next struct, the
tick swaps at a tick boundary, and every change is appended to an **input log (tick, dial, value)** so a run replays
bit-exactly from seed + log. (b) Global scalars are not enough for a large world: add region-scoped dials (a brush
volume or a biome id) with the same semantics. (c) Per-creature acts need selection by volume, lineage or tag, not by
typing one id; all must be conserving transfers (the lab's `feed` is the model). (d) Brain-part stimulation and named
pathway lesions assume one fixed brain plan; with genome-built brains these become "stimulate channel k" and
"lesion gene/mechanism flag", applied to a selected cohort so lesion statistics come from thousands of individuals.
(e) The viewer must read a decimated, read-only copy (per-cell aggregates and a sampled subset of creatures), never
per-creature JSON. (f) One dice source only: Philox with a labelled counter domain for God effects; the label
"classical-seeded" is the honest one (law 7).

## 18. The optional second layer: `EcologyEngine` and `Archipelago` (passes 071-088)

Not called by `Dome.step()` or by the viewer; exported from `lab/src/index.ts` and exercised only by
`creatures/001/organism/passes_100_suite.ts` (grep). It is a library of extra world passes. Summary:

| Pass | Rule and constants | Port verdict |
|---|---|---|
| observational culture | within 4.2 units an awake observer copies a demonstrator's best foraging-site weight (rate 0.08) and aversions (0.10); 4-value "dialect" vectors converge (0.03 + drift 0.004) | writes brain weights from outside: redo as a sensed cue + creature-side learning |
| alarm / consolation / sharing | alarm costs 0.015 kJ, raises cortisol +0.12 and NE +0.10 within 6 units; consolation at touch lowers cortisol 0.10; a donor with fat > 320 gives 3 kJ to a peer with fat < 170 within 1.8 units, recorded in a credit ledger | the 3 kJ transfer is a good conserving primitive; hormone writes must become signals |
| social sleep sync | sleep pressure relaxes 0.06 toward the mean of peers within 3.6; one sentinel stays awake if predators exist | field of a sensed "sleep" chemical instead |
| skill market | 4 kJ payment for copying site knowledge (0.40 blend); debt up to 12 kJ; kin credit | drop (direct knowledge copy) or redo as teaching by demonstration |
| module graft | accepted if immune-signature distance <= 0.38, else cortisol +0.25 | creature-specific, out of world scope |
| stones and caches | stones (mass 1.5) pushed 0.55/tick at 0.012 kJ per mass-unit; caches hold kJ taken from a cell, spoil 0.05 %/tick into detritus; owner eats 2.5, thief 1.8 per tick; owner within 3 units repels | keep: movable objects and stored food are real, conserved, and scale as voxel/item fields |
| flocking, castes, trails | Reynolds blend (0.45 align, 0.30 cohere, 0.25 separate; radii 0.9 and 4.5; 18 % heading blend per tick); per-cell trail conductance +0.12^1.2 per visit, decay 0.992, up to 35 % less drag | heading is overwritten by the world: flocking must instead emerge from sensed neighbours; the **trail field** is a keeper |
| Ashby homeostat | after 8 crisis ticks, random heading and NE shift | world-side steering: drop |
| predators and parasites | predator biomass seeded from patches (18 kJ), respiration 0.025 x trait factor, detection radius 7.5 with camouflage and shelter, bite 0.8 to 1.6 kJ at 1.15 units + harm 0.04, reproduces above 34 kJ (child 14 kJ, traits mutate +/-0.04), dies below 1.5 kJ into detritus, at most 6; parasite load drains 0.008 x load and is purged 80 % at a medicinal patch | keep the closed trophic ledger; predators should be ordinary evolved creatures, not a scripted class |
| Daisyworld | two flora covers per cell (albedo 0.25 / 0.75, bare 0.5), growth parabola peaking at 22.5 C, death 0.12; sets the global temperature dial | keep as a field-level climate feedback test |
| reaction-diffusion substrate | conservative exchange between a mineral pool and food (Du 0.14, Dv 0.05, x0.2), autocatalysis `min(0.25u, 0.08 u v^2)`, back-flow 3 % | keep: it is the template for a conservative integer field update (flux through faces) |
| storm learning, warp niche | world-side EMA of storm intervals steers creatures to stones | world-side steering: drop |
| Archipelago | several domes in a ring; a creature beyond x > 0.8 R moves to the next dome; books `-E` in the source `born` and `+E` in the destination | generalises to regions/shards with exact hand-over of integer energy across boundaries |

General finding: roughly half of these passes set `heading`, hormone levels or learned weights of creatures directly
from world code. Under law 4 ("no hardcoded steering") the native world may only (i) move conserved quantities,
(ii) expose sensory channels, and (iii) apply physics; behaviour must come from the creature.

## 19. Persistence and determinism as implemented

- `snapshotFull()` serialises every own field, both PRNG states and each creature's own `snapshot()`; `fromSnapshot`
  rebuilds an identical dome. The viewer writes it atomically (tmp + rename) every 30 s.
- Determinism holds for a fixed seed on one thread with IEEE doubles and a fixed iteration order. There is no
  thread-count independence because there are no threads; conservation is a float residual (< 1e-6 relative), not an
  integer identity.

**3D / 10^6.** The page-locked arena *is* the snapshot: a save is a copy of the arena plus the dial struct and input
log; a state hash over the arena after each N ticks is the regression test for "same seed, any thread count".

## 20. What I ran (2026-11-17)

Only read-only computations; no lab code was executed and nothing on `V:` was modified.

Scripts are in `S:\v40\dome\` (`geom.js`, `state.js`). Command, run in Git Bash:

```
cd /s/v40/dome && node geom.js && node state.js
```

Output of `geom.js` (recomputes grid geometry and the daily light integral from the constants in `dome.ts`; object
printed on one line here):

```
{ n: 24, cells: 576, inside: 448, crev: 36, oasis: 42, vent: 74, area: 452.3893421169302 }
lightSumPerDay 458.3655088839061 mean 0.31830938116937924
```

(448 live cells; with crevices on, 36 cells are inside a refuge; with vents on, 42 cells are oasis and 74 are vent
zone; mean light over a default day is 1/pi.)

Output of `state.js` (parses `V:\lab\viewer\dome_state.json`, 26,529,847 bytes, saved 2026-11-12; objects printed on
fewer lines here, values verbatim):

```
{ savedAt: '2026-11-12T18:05:00.090Z', t: 192188, pop: 5, births: 75, deaths: 75, buds: 0,
  grown: 121416.48178430254, born: 42472.50000000002, detritus: 4.4545464930918435, speed: 10 }
{ recycled: 85105.34472036177, E0: 12945.653784564696, food: 84311.83465886768, soil: 0,
  cellsWithFood: 451, maxCellFood: 9445.307196955982, parasite: 3709.7716666726715,
  grazerBiomass: 493.69127438792304, grazerHeat: 69893.37862995267, kicks: 158697, games: 0, signals: 0,
  neoGrants: 776, neoDenied: 0 }
death ages ticks min/median/max 428 8043 67217
bornPerBirth 566.3000000000003 foodCapScale 2 recyclingDialNow 0
bytes per creature snapshot [ 4470139, 4857623, 5774399, 6498980, 4894072 ]
```

Readings: 133.5 simulated days; 566.3 kJ per newborn. Total patch food is 84,312 kJ, an average of 188 kJ per live
cell, and **one cell holds 9,445 kJ** although the cap in force is 60 kJ (30 x foodCapScale 2): the cap limits growth
only, and recycling piles detritus into whichever cells creatures occupy with no bound. **451 cells hold food although
only 448 are inside the disk**: `cellAt` clamps rim positions onto outside cells, so recycled food leaks into cells
that never grow. The counters show recycling, parasites, grazers and dice kicks were enabled at some point in that run
although the saved dials have them off. I did not check the ledger identity on this file (it needs each creature's
energy, which requires running lab code).

## 21. Defects and oddities in the lab world (so the port does not copy them)

1. Order dependence: sequential creature pass; results depend on array index.
2. Food cap is not a bound: recycling and waste can push a cell far above its cap (observed mean 188 kJ and maximum
   9,445 kJ against a cap of 60), and food leaks into 3 cells outside the disk through the clamped cell lookup.
3. Budding and respawn create energy (`born`); only 140 kJ of a bud is parent-paid.
4. Dead creatures still count as touch, still receive recycled food under them, and are not excluded from games.
5. Respawn reuses the dead creature's id and position, so learned attitudes and the game-policy weight carry over to
   an unrelated newborn.
6. Detritus is one global pool, returned wherever creatures happen to stand, not where the matter was shed.
7. Scent marks are 16 global slots with no position.
8. `ecologicalGradientAt` has no caller; the viewer's spawn has arity 2, so lineage is silently off in the viewer;
   `oneShot` cannot be set over HTTP; `/dials` accepts `deathRule: "creature"`, which the type does not define.
9. Parasite biomass, grazer heat and unrespawned corpses are permanent sinks.
10. The game-strategy learner and most `EcologyEngine` passes are world-side policy acting on creature internals.
11. Per-individual memory keyed by string id is unbounded.
12. The world conserves energy only as a floating-point residual; creature heat stays inside the creature's energy figure.

## 22. Port checklist: the dome rules restated for a 3D world of 10^6 creatures

| Dome rule | Native generalisation |
|---|---|
| disk + 24x24 patch grid | bricked sparse voxel fields (integer), spatial hash with cell edge >= largest interaction radius |
| 1 tick = 1 min, cadences 15 / 60 / 1440 | same integer clock; cadences staggered by `id mod period`; LUTs for day and season |
| sequential creature pass | two-phase tick: read old state + write intents, then deterministic per-cell resolve |
| food relaxes to cap under light | integer producer field with remainder accumulators; light attenuated by depth/occlusion; hard cap enforced on every inflow |
| creature mutates the cell | eat requests granted by the world; proportional integer shares, remainder by id order |
| per-creature temperature formula | coarse temperature field with heat as the terminal energy sink |
| vents by distance test | toxin sources into a diffusing, decaying integer field; gradient by finite difference |
| plume = sum over all cells | diffused odour field, O(1) read per creature |
| 16 global scent marks | positional pheromone channels in the field (stigmergy) |
| signal to all within 4.2 | events binned by hash cell, capped receivers, sender pays into heat |
| O(N^2) touch / meet / neighbours | one 27-cell scan; k-nearest cap; tag-based recognition; bounded attitude memory |
| float ledger, residual check | 64-bit integer accounts, exact equality check, part of the state hash |
| global detritus pool | local detritus field with decomposer rate |
| budding with created energy | fully parent-funded offspring; founders are the only `born` |
| respawn in place | none by default; optional constant-population control mode |
| lineage via spawn arity | parent id + birth tick columns; genome + parental-state vector; neutral marker locus |
| persona traits + drift | genome loci with heritable mutation width and slow lifetime drift |
| God dials (global) | double-buffered integer dial struct + region brushes + replayable input log |
| `/god` per-creature acts | cohort operations by volume/lineage/tag, all conserving |
| JSON snapshot (5 MB per creature) | arena copy + hash; viewer reads decimated aggregates |
| mulberry32 streams consumed in order | Philox4x32-10 keyed by (seed, entity id, tick, purpose) |
