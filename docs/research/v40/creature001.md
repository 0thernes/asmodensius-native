# Creature 001 — mechanism extraction for the native port

Agent key `creature001`, written 2026-11-17. Purpose: describe the existing TypeScript Creature 001
precisely enough that each mechanism can be ported as a compact integer analog in a population engine.

## 0. What was read, what was run

Read (all read-only on `V:\`):

- `V:\lab\src\adapters\c001.ts`, `V:\lab\src\interface.ts` (whole files)
- `V:\creatures\001\organism\organism.ts` — header, regions, constants (`TUNE, CX, M35..M50`), constructor,
  `step()` physiology / neural tick / metabolism / death, `choose()`; lines 1–115, 290–440, 700–1036,
  1108–1253, 1303–1362 (file is 1432 lines, 231 KB)
- `V:\creatures\001\organism\cleanslate_foundation.ts` — Philox, 11-pool ledger, Chemoton, XPBD shell,
  `CleanSlateFoundation.step` (lines 1–200, 360–1000, 1120–1267)
- `V:\creatures\001\organism\frontier_alife.ts` — the "10-organ substrate" (lines 102–360, 416–605)
- `V:\creatures\001\organism\substrate.ts`, `coupling.ts`, `V40-BUILD-REPORT.md` lines 14–45

NOT read (only their call sites in `organism.ts`): `develop.ts`, `affect.ts`, `v40mods.ts`,
`life_systems.ts` body, `novel_interior.ts`, `cognition_passes.ts`, `embodied_ontogeny.ts`,
`nestor_brain.ts`, `heptarch_council.ts`, `fp/*`, and most of the `Mind` class (only its fields and the
ignition rule). Anything said about those is from call sites and is marked as such.

Ran (probe script `S:\v40\creature001\probe.ts`, imports the V: sources, writes nothing on V:):

```
cd /s/v40/creature001 && timeout 90 bun probe.ts base
== default (seed 2000): NNEU=272 MW=13 CE=52 synapses=3454 plasticFlag=432 kinds=[[0,1675],[1,224],[2,802],[3,289],[5,48],[6,156],[7,156],[8,104]] csf=false fa=false ls=false
ticks=4320 ms=2819 (652.5 us/tick, noisy, parallel load) spikes/tick=403.6 of 1632 neuron-substeps
foraged=365 rewards=124 eatenKJ=272.80 heatKJ=262.887 heat/tick=0.06085 excreted=2.5779 maxLedgerRelErr=2.06e-15 dead=false
ranges: gut.glc[4.58e-10,9.70] gut.fat[0.000375,6.67] gut.prot[0.00000393,3.81] blood.glc[2.26,7.89] blood.ffa[0.00,1.63] blood.aa[1.01,4.11] blood.ket[0.00,0.190] blood.urea[0.000478,0.134] liverGly[15.4,39.6] fat[372,400] muscle[120,146] brainGlc[0.423,1.55] atp[1.97,1.99] tissue[10.0,10.0] hr[61.4,127] insulin[0.339,1.00] glucagon[1.31e-33,0.448] cortisol[0.106,0.596] ghrelin[0.00233,0.999] S[0.0949,0.858] nm.DA.P[0.340,0.947] nm.DA.E[0.0713,0.500] nm.5HT.P[0.466,1.28] nm.5HT.E[0.000275,0.417] nm.NE.P[0.0993,0.977] nm.NE.E[0.00833,0.491] nm.ACh.P[0.578,1.40] nm.ACh.E[0.000413,0.431] nm.Glu.P[0.250,0.995] nm.Glu.E[0.00193,0.795] nm.GABA.P[0.230,1.05] nm.GABA.E[0.00463,0.343] nm.eCB.P[0.0000736,0.527] nm.eCB.E[1.01e-7,0.221]

cd /s/v40/creature001 && timeout 95 bun probe.ts apex
== apexMind (chemoton + shell): ... csf=true fa=false ls=false
ticks=4320 ms=2884 (667.5 us/tick, noisy, parallel load) spikes/tick=387.5 of 1632 neuron-substeps
foraged=409 rewards=125 eatenKJ=275.00 heatKJ=257.813 ... maxLedgerRelErr=4.49e-15 dead=false
chemoton: divisions=27 lysed=false moiety=6.7989 shellStoredKJ=0.000e+0
ranges: ... chem.c0[1.26,1.41] chem.c1[0.579,1.14] chem.c2[0.546,0.952] chem.c3[0.826,1.39] chem.c4[0.877,1.71] chem.c5[0.268,0.374] chem.z1[0.0381,1.26] chem.z2[1.02,43.7] chem.osm[2.47e-323,0.0721] shell.catch[0.250,1.00] shell.volErr[0.00,0.00]

cd /s/v40/creature001 && timeout 95 bun probe.ts starve
== starve (no food gained): ...
ticks=60000 ms=16514 ... foraged=4607 rewards=0 eatenKJ=0.00 ... maxLedgerRelErr=3.36e-13 dead=true at t=9482 (protein depletion (organ failure))
ranges: ... blood.ket[0.00,5.48] ... fat[2.69e-25,400] muscle[9.22e-27,121] ... atp[-545,1.99] ...
```

Facts these runs establish (seed 2000 only, one seed, so treat as an example, not a distribution):

1. `NNEU = 272` at the default `MW = 13` (the build report's "512 neurons" does not match the code); 3454 synapses.
2. A default organism has **no Chemoton, no shell and no 10-organ substrate** (`csf=false fa=false`). They exist only
   with `apexMind: true` / `fullMind: true` / `ORG_FULL=1` / `ORG_APEX=1`.
3. The float ledger closes to ~1e-15 relative while alive, and to 3e-13 over 60000 ticks.
4. Starved from birth, the creature dies at tick 9482 (6.6 simulated days) by protein depletion.
5. `step()` keeps integrating after `dead = true`: ATP reached −545 kJ. Death is only a flag (see §9).
6. With `apexMind` but no locomotion input, the XPBD shell never steps (stored shell energy stayed 0).
7. The TS organism costs roughly 0.65 ms per tick under bun (noisy, parallel load).

## 1. Shape of the organism

- Time base: **1 tick = 1 simulated minute**; the spiking net runs `SUB = 6` substeps per tick; a day is 1440 ticks.
- Units: energy in **kJ** everywhere (pools, ATP, heat); hormones and autonomic tone dimensionless in [0, 1];
  neuromodulator vesicle pool `P` in [0, 1.5], extracellular `E` ≥ 0 (observed < 0.8); firing rate = spikes per
  substep in [0, 1]; heart rate in beats per minute.
- RNG: `mulberry32` closures, one stream per subsystem, seeded `seed*K + c` (main `7919/17`, entropy `104729/3`,
  topography `31337/5`, cortex `7777/11`, dev waves `4441/9`, affect `4051/11`, bus `6007/41`). Philox4x32-10
  exists only inside `cleanslate_foundation.ts`.
- Configuration layers (what is live by default in the lab adapter, which passes `opts` straight through):

| Layer | Switch | Default |
|---|---|---|
| Core: 272 spiking neurons, 7 neuromodulators, hormones, heart, kJ metabolism, sleep, foraging, death | always | on |
| Mind v40 (workspace, classifiers, Kalman, RLS), Affect, SensoryBus / SelfModel / Curvature / NeoHooks, developmental wiring | `mind` = "v40" | on |
| 10-organ substrate (`FrontierALife`) | `M50.fa` or `fullMind` | **off** |
| NovelInterior, LifeSystems, CognitionPasses, EmbodiedOntogeny | `M50.ni/ls/cp/eo` or `fullMind` | off |
| NESTOR, HEPTARCH, CleanSlate (Chemoton, XPBD shell, Philox, 11-pool ledger, Omni bus) | `apexMind` or `fullMind` or `M50.apex` | **off** |

- Float state size of the core (computed from the declared arrays, before JS object overhead): per neuron 13
  Float32 arrays + 1 byte spike + 2×4 Float64 e-prop traces ≈ 117 B × 272 ≈ 32 KB; per synapse
  `pre,post` i32 + `w,mask` f32 + `plastic,kind,pk` u8 = 19 B × 3454 ≈ 66 KB. The Mind layer adds several
  dense matrices (not sized here). A port cannot carry this per individual at 10^5–10^6 creatures.

## 2. Chemoton (`ChemotonTriad`, apex only)

**Essence.** A six-species reversible mass-action network (feedstock, two autocatalytic intermediates, membrane,
template, waste) whose three sub-networks must all keep running or the protocell lyses; reaching three thresholds
together triggers a division event.

**State.** 6 concentrations + `z1, z2` (antithetic controller) + `osmoticStress` + `divisions` + `lysed` = 9 f64, 1 int, 1 bool.

Species `c = [X, A1, A2, M, V, W]`, initial `[1.5, 1.2, 0.55, 1.0, 1.0, 0.25]`; aliases `autocatA = A1 + A2`,
`membraneM = M`, `templateV = V`. Moiety weights `l = [2, 1, 2, 1, 1, 1]`; `lᵀS = 0` (checked by hand for all
five columns), so `2X + A1 + 2A2 + M + V + W` is invariant under the reaction core.

| r | Reaction | k⁺ | k⁻ | forward flux J⁺ | reverse flux J⁻ | conductance g |
|---|---|---|---|---|---|---|
| R1 | A1 + X ⇌ A2 + W | 1.15 | 0.07 | g·k⁺·A1·X | g·k⁻·A2·W | M/(0.35+M) |
| R2 | A2 ⇌ 2 A1 | 0.96 | 0.12 | k⁺·A2 | k⁻·A1² | 1 |
| R3 | A1 ⇌ M | 0.52 | 0.14 | g·k⁺·A1 | g·k⁻·M | V/(0.30+V) |
| R4 | A2 ⇌ V + W | 0.40 | 0.10 | g·k⁺·A2 | g·k⁻·V·W | M/(0.35+M) |
| R5 | M ⇌ W | 0.08 | 0.05 | k⁺·M | k⁻·W | 1 |

Stoichiometry `S[species][reaction]`:

```
        R1  R2  R3  R4  R5
X      -1   0   0   0   0
A1     -1  +2  -1   0   0
A2     +1  -1   0  -1   0
M       0   0  +1   0  -1
V       0   0   0  +1   0
W      +1   0   0  +1  +1
```

Per-tick open step (`stepChemoton`, `dt = 0.05`, 4 substeps, `h = 0.0125`):

1. Boundary: `X += 0.35·(1.45·feed − X)` with `feed = clamp(atp/2, 0.1, 1.5)`; `W += 0.30·(0.12 − W)`.
2. Four times: `c += h·S·(J⁺ − J⁻)` (floor 1e-9), then dilution `A1 *= 1−0.085h`, `A2 *= 1−0.065h`,
   `M *= 1−0.070h`, `V *= 1−0.085h`.
3. Osmotic stress: `osm = 0.12 + A1 + A2 + 0.35W`; `encl = max(0.06, M·min(1.25, 0.22 + 0.78V))`;
   `stress = clamp(0.84·stress + 0.16·max(0, osm/encl − 1.75), 0, 2.5)`.
4. Lysis (permanent): `stress > 0.85` or `A1+A2 < 0.30` or `M < 0.28` or `V < 0.26`.
5. Division: `A1+A2 ≥ 1.95` and `M ≥ 1.18` and `V ≥ 1.15` → `A1, A2 *= 0.58`; `M, V *= 0.62`; `divisions++`.

Lesions: `chemotonA` zeroes g1, g2; `chemotonM` zeroes g3; `chemotonV` zeroes g4.

Antithetic controller (same class, driven by blood glucose `x`): `z1 ← (z1 + dt·μ)/(1 + dt·η·z2)`,
`z2 ← (z2 + dt·θ·x)/(1 + dt·η·z1)`, with μ = 3, θ = 1, η = 1.8, dt = 0.02; output `u = 0.5·(z1 − z2)`.

Couplings to the organism: `u > 0` adds `0.015·u` to glucagon, `u < 0` adds `0.015·|u|` to insulin; osmotic hazard
`clamp(stress/1.05)` adds `0.025·hazard` to cortisol; `lysed` blocks maternal provisioning; with
`requireCytokinesis`, each maternal provisioning event consumes one unspent `divisions` count.

Observed (apex run): 27 divisions in 4320 ticks, never lysed, `z2` wound up to 43.7 while `z1` fell to 0.038.

**Things to fix rather than copy.** The open step is not conservative: the chemostat relaxation creates and
destroys X and W, dilution destroys A1/A2/M/V, division throws away 42 % / 38 % of the pools, and the 1e-9 floors
create mass. The Chemoton is also not in the kJ ledger: `chemotonReserveKJ` is declared and summed but never
written. Feed depends on ATP level but takes nothing from ATP.

**Port plan: keep as integer pools.** Six `u16`/`u32` counts in one moiety unit; per substep compute integer
extents `ξ_r = (k_r · product) >> shift`, cap each so no reactant goes negative, apply `S·ξ` exactly (moiety
conserved by construction). Replace the three leaks with transfers: X is bought from the creature's matter/energy
pool at a fixed exchange rate, "dilution" becomes hydrolysis to W (A2 → 2 W keeps the weights), W is exported to
the world cell, and division hands the removed counts to the offspring record. Keep the three-threshold division
gate and the four-condition lysis rule as the reproduction and death gates of the population engine. The
antithetic pair is two more integers; clamp it (anti-windup) instead of copying the unbounded `z2`.

## 3. Energy and ATP ledger (core, always on)

**Essence.** Seventeen kJ pools connected by first-order transfers whose losses are booked to a heat pool, so
`energy() − E0 − eaten` is zero to rounding; ATP is a small buffer refilled by oxidising substrates at 40 % yield.

**State (17 f64 + bookkeeping).** Initial values in kJ:

| Pool | Init | Pool | Init |
|---|---|---|---|
| gut.glc / gut.fat / gut.prot | 4 / 2 / 1.5 | liverGly | 20 |
| blood.glc / ffa / aa / ket / urea | 3 / 1 / 2 / 0.2 / 0 | fat (adipose) | 400 |
| brainGlc | 0.6 | muscle | 120 |
| atp | 2 | tissue (integrity = tissue/10) | 10 |
| heat (cumulative sink) | 0 | excreted (cumulative sink) | 0 |

`energy()` = sum of all of the above (+ LifeSystems and CleanSlate stores when present).
`ledgerError() = |energy() − E0 − eaten| / (E0 + eaten)`; `eaten` is the signed exogenous input (feeding adds,
`drain`, `takeWaste`, maternal transfer subtract).

Feeding: `feed(kJ)` splits 6/11 to gut.glc, 3/11 to gut.fat, 2/11 to gut.prot. A successful forage is one 2.2 kJ meal.

All transfers use `mv(from, frac) = min(from, from·frac)` unless noted. Notation: `bg = blood.glc/3`,
`ins, glg, cort, symp` = insulin, glucagon, cortisol, sympathetic tone, `gly = liverGly`,
`fasted = clamp(1 − gly/5)`. Default (mind v40, `fullMind` off) rates per tick, in execution order:

| # | From → to | Fraction of source per tick | Split |
|---|---|---|---|
| 1 | gut.glc → blood.glc | 0.06·motility | |
| 2 | gut.fat → blood.ffa | 0.025·motility | |
| 3 | gut.prot → blood.aa | 0.035·motility | |
| 4 | blood.glc → liverGly | 0.03·ins·(1 − gly/60) | 95 % glycogen, 5 % heat |
| 5 | liverGly → blood.glc | 0.004·glg + 0.002·symp | |
| 6 | blood.glc → fat | 0.01·clamp(ins − 0.4)·clamp(gly/50) | 80 % fat, 20 % heat |
| 7 | fat → blood | 0.00012·(glg + symp + 0.5·cort)·(1 − 0.7·ins)·clamp(1 − ffa/6)·min(4, √(400/max(fat,10))) | 90 % ffa, 10 % glc |
| 8 | blood.ffa → fat | 0.01·ins | |
| 9 | blood.ffa → ketones | 0.02·clamp(0.5 − ins)·(1 + 4·fasted) | 80 % ket, 20 % heat |
| 10 | blood.aa → glucose | 0.004·(glg + cort)·(1 + 2·fasted) | 75 % glc, 10 % urea, 15 % heat |
| 11 | muscle → blood.aa | 0.00015·cort·(1 − ins)·(1 + 2·fasted)·1/(1 + 0.5·ket)·(fat < 20 ? 3 : 1) | |
| 12 | blood.aa → muscle | 0.005·ins·clamp(aa − 1, 0, 1)·msat, msat = clamp((180 − muscle)/60) | surplus (1 − msat) goes the way of row 10 |
| 13 | blood.urea → excreted | 0.01 | |
| 14 | blood.ket → excreted | 0.0008 | |
| 15 | blood.glc → brainGlc | absolute: min(blood.glc, max(0, 0.3·perfusion·(bg − brainGlc/0.6))) | |
| 16 | tissue ← blood.aa (repair) | absolute: min(10 − tissue, 0.01·ATPf·clamp(aa − 0.3), aa) | costs ATP 0.5× the amount |
| 17 | tissue → blood.aa (damage x) | absolute: min(tissue, 10·x) | |

`motility = clamp(0.2 + 0.5·para + 0.4·ensRate − 0.3·symp)`.

ATP demand per tick (kJ/min), `thm = max(0.6, 1 − fatDef)`, `fatDef = clamp(1 − fat/400)`:

- brain: `0.004·thm + 0.000012·nsp·(0.6 + 0.4·level) + 0.00003·decideSpikes·level + synthCost + cogCost`
  (`nsp` = spikes this tick, `level` ∈ 1..4, `synthCost = 0.002·Σ` neuromodulator synthesis, Mind cost: 0.0004 per ignition and small terms)
- body: `(0.006 + 0.00006·hr)·thm + 0.004 [if a decision was made] + 0.003 [forced wake] + repairATP + 0.002·motor + 0.003·max(0, genes[3] − 1)`
- `atp −= need; heat += need`.

Refill (oxidative phosphorylation): `refill = max(0, 2 − atp)·0.5`; substrates are taken in order until
`got ≥ refill`, each amount `s` giving `atp += 0.4·s`, `heat += 0.6·s`, with `want = (refill − got)/0.4`:

1. blood.ket for brain: `s ≤ want·min(0.7, 0.3 + 0.05·ket)`
2. brainGlc: `s ≤ want·0.4`
3. blood.ket: `s ≤ want·0.5`
4. blood.ffa: `s ≤ want`
5. blood.ket: `s ≤ want`
6. blood.glc: `s ≤ 0.5·blood.glc`

Observed: ATP sits in [1.97, 1.99] while fed; total heat ≈ 0.061 kJ/tick fed (demand plus conversion losses).

The 11-pool Neumaier ledger (apex only) is a second, purely observational accounting of the same pools
(`E_patch, E_gut, E_blood, E_stores, E_atp, E_tissue, E_kinetic, E_elastic, E_detritus, E_heat_phys, E_diss_num`;
tolerances abs 1e-11, rel 5e-14; a second-law check that the two heat pools never decrease). It changes nothing.

**Port plan: keep as integer pools, reduce the count.** One unit for every pool (proposal: 2⁻²⁰ kJ ≈ 0.95 J in
`u32`, range 4095 kJ; the smallest flow, one spike at 0.000012 kJ, is ≈ 12.6 units, so charge spikes in batches).
Every transfer becomes `f = min(from, mulhi(from, rate))`, then `to_a += (f·num) >> 8`, and the **remainder**
`f − to_a` goes to the last sink (heat), which makes each row exact with no compensation arithmetic. Heat and
excreted should not be per-creature accumulators: add them to the world cell so the world total is the invariant.
A reduced pool set that keeps the fasting sequence (glycogen → fat/ketones → protein → death): gut, blood glucose,
blood lipid (ffa + ket merged), blood amino acid, glycogen, fat, protein (muscle + tissue), ATP = 8 × `u32`.
First-order relaxations in integers stall when `|Δ|·k < 1`: carry a remainder or use Philox stochastic rounding.

## 4. Hormones, autonomic tone, sleep, clock (core)

**Essence.** A PID glucose controller and a handful of leaky hormone variables turn pool levels into the rate
multipliers of §3; a two-process sleep switch gates behaviour.

**State.** insulin, glucagon, cortisol, ghrelin, leptin, melatonin, symp, para, pidI, pidPrev, S (adenosine),
orx, scnPhase, ultra, stage, asleep ≈ 14 f64 + 2 small ints.

- Clock: `scnPhase += 2π/1440 + light·0.002·sin(2π·minute/1440 − scnPhase)`; `scnOut = cos(scnPhase − 2π·14/24)`;
  light = 1 for hours 6–18 unless set by the world; `ultra += 1/90 (mod 1)`.
- Glucose PID: `set = 1 + 0.08·scnOut`, `e = bg − set`, `pidI = clamp(pidI + 0.002·e, ±0.5)`, `dE = e − e_prev`.
- `insulin += 0.1·(clamp(0.3 + 1.5e + pidI + 3dE + 0.2·para) − insulin)`
- `glucagon += 0.1·(clamp(0.3 − 1.5e − pidI + 0.3·symp) − glucagon)`
- `cortisol += 0.02·(clamp(0.35 + 0.25·cos(2π(hour − 7)/24) + 0.6·clamp(0.85 − bg) + 0.2·forced + 0.4·clamp(0.8 − integrity)) − cortisol)`
- `ghrelin += 0.01·(clamp(1 − gutFill/6) − ghrelin)`; `leptin = clamp(fat/800)`;
  `melatonin += 0.05·((1 − light)·clamp(0.5 − scnOut) − melatonin)`
- `vagal = clamp(gutFill/25 + 0.2·ensRate)`
- `symp = clamp(0.1 + 0.8·R_LC + 0.4·cortisol + 0.4·clamp(0.8 − bg) + 0.2·R_HYP)`;
  `para = clamp(0.3 + 0.4·(1 − wake) + 0.25·vagal − 0.3·R_LC)` (`R_x` = region mean rate)
- Sleep: `C = 0.25·scnOut + 0.05·sin(2π·ultra)·[awake]`; `drive = S − C + 0.15·melatonin − 0.1·max(0, NE − 0.3)`;
  `thrS = 0.62 + 1.2·orx`, `orx += 0.02·(min(1, Σsens) − orx)`; fall asleep when `drive > thrS`, wake when
  `drive < 0.2` (or forced). Stage from `ultra`: <0.1 N1, <0.3 N2, then N3 (if `S > 0.35`) until
  `0.75 − 0.25·clamp(0.6 − S)`, then REM.
- Adenosine: awake `S += 0.0008·(brainD/0.0075)`; asleep `S −= (N3 ? 0.004 : 0.0022)·S`.

**Port plan: simplify.** Keep insulin, glucagon, cortisol, ghrelin as `u8` (0..255 = 0..1) with shift-based
relaxation; derive leptin, vagal, symp, para each tick instead of storing them; keep the PID integral as one
`i8`. Keep S and a 2-bit state (wake / light sleep / deep sleep / REM); take the circadian phase from the world
clock plus a per-creature `u8` offset. Melatonin, orexin and the ultradian phase can be dropped at first.

## 5. Organs: the 10-organ substrate (`FrontierALife`, off by default)

**Essence.** Ten small state machines and leaky integrators (survival mode, limb ganglia, peptides, breathing,
damage clearance, cortical memory traces, criticality, timing, one-shot memory, gut flora / heat / epigenetic marks)
that each return additive drives to brain regions and multiplicative gates on foraging, learning and ATP use.

**State.** About 420 f64 in total, of which 260 are the per-cortical-cell traces of organ 5.

| # | Organ (lesion flag) | State | Rule (per tick) | Effect on the rest | Port plan |
|---|---|---|---|---|---|
| 1 | PAG / LH survival mode (`pagLesion`) | `pagAct[4]`, `pagMode`, `pagThreatTrace` | threat trace `= clamp(0.989·tr + 5.2·intDrop)`; four raw signals (explore, forage, threat, recuperate) → `act += 0.32·(target − act)`, winner with +0.10 hysteresis | drives to HYP/LC/VTA/MOT, caution bonus `4.6·defend`, aversive-learning boost, forage gate | **keep**: 4 × `u8` + 2-bit mode; it is the cheapest behaviour selector in the creature |
| 2 | Four limb ganglia (`limbLesion`) | phase, V, tau, reflex, grip × 4 | CPG phase `+= f + 0.18·sin(opp − ph − π)`; leaky V with input-dependent τ; reflex jumps to `0.72` of the gap on injury, decays ×0.9935 | `damageShield ≤ 0.66`, MOT drive, ATP `0.00009·work` | simplify: one reflex `u8` (damage shield) + one gait phase |
| 3 | Neuropeptide bus (`peptideLesion`) | `pep[4]` = NPY, CCK/GLP-1, CRF, OXT | `pep += (release − pep)/τ`, τ = 36, 16, 34, 40 ticks | forage gate `1 + 0.26·NPY − 0.11·CCK`, cortisol calm, sleep drive | **keep**: 4 × `u8`, slow hunger / satiety / stress / calm |
| 4 | Respiration and ROS (`respLesion`, `autophagyLesion`) | o2, co2, pH, ventilation, rq, rosDamage, autophagy, lastNeed | `vo2 = 0.022·metRate`; ventilation relaxes (0.25) to a CO2/O2/pH error target; `o2 += 0.075·vent·(env − o2) − vo2`; `ros += 0.00018·metRate·… − 0.0022·autophagy·clamp(atp/1.6)` | `aeroEff` ∈ [0.35, 1] scales ATP refill; `proteostasis` scales repair | simplify: o2 `u8` + damage `u8`; hypoxia then limits ATP refill |
| 5 | Cortical state-space traces (`recLesion`) | 5 × 52 | fast / mid / slow per-cell traces → additive cortical input | working memory | leave for later (needs the cortex) |
| 6 | Criticality homeostat + astrocytes (`critLesion`) | `critSigma`, `prevCtxAct`, 3 × 4 | branching ratio estimate → `critMult` ∈ [0.72, 1.35] on cortical drive | keeps cortex from silence or saturation | simplify to one gain per brain, or drop |
| 7 | Cerebellar timing (`cbLesion`) | 8 basis + 2 × 8 weights + 3 | leaky basis with 8 time constants, clock speed `0.78 + 0.45·DA + 0.25·NE` | predicted meal timing → forage gate, MOT | leave for later |
| 8 | One-shot episodic traces (`btspLesion`) | 16 × 4 | strength ×0.9996 per tick; written on large RPE or shock | site bias at matching clock phase | leave for later |
| 9 | Flora, body heat, glymphatic waste (`floraLesion`, `thermoLesion`, `glymphLesion`) | 7 scalars | flora / dysbiosis slow integrators; `coreTemp += 0.04·(eq − T) + 0.32·BAT − vasoCool`, `eq = ambient + 16.4 + 0.6·need`; BAT heat costs `0.0014·rate` kJ | shade seeking, fever learning boost, sleep drive | keep core temperature as one `u8` if the world has temperature; drop the rest at first |
| 10 | Hormesis, allostatic load, 4 epigenetic marks (`epiLesion`) | 2 + 4 + 2 | marks `+= 0.0012·signal − 0.00025·mark` for scarcity, threat, thermal, volatility; inherited at 0.65× | thrift, vigilance, BAT boost; famine triage silences up to 40 % of cortex | **keep the 4 marks** (`u8`, inherited ×0.65): cheap transgenerational state |

ATP cost of the whole substrate: `ventATP + autoATP + limbATP + astroATP + batATP + cbATP` (each ≤ ~1e-4 kJ/tick).

## 6. Neuromodulators (core)

**Essence.** Seven transmitters, each a vesicle pool `P` and an extracellular level `E`, synthesised from blood
precursors and ATP, released in proportion to the firing of a source nucleus, cleared by saturating reuptake.

**State.** 7 × (P, E) = 14 f64; initial P = 1, E = 0.3 (eCB: P = 0.5, E = 0.1).

Per tick, for each transmitter with parameters `(src, pre, ksyn, vmax, km, kdeg)`:

```
sy  = ksyn · pre · clamp(ATPf) · (1 − P/1.5);   P += sy;   synthCost += 0.002·max(0, sy)
rel = 0.25 · src · P;                            P −= rel;  E += rel
up  = vmax · E/(km + E);                         E −= up + kdeg·E;   P += 0.7·up
```

| NM | src (release drive) | pre (precursor) | ksyn | vmax | km | kdeg |
|---|---|---|---|---|---|---|
| DA | R_VTA | prec = clamp(blood.aa/2, 0, 1.5) | 0.03 | 0.1 | 0.4 | 0.03 |
| 5HT | R_RAPHE | prec | 0.03 | 0.1 | 0.4 | 0.03 |
| NE | R_LC | 0.5·DA.P + 0.2·prec | 0.03 | 0.1 | 0.4 | 0.03 |
| ACh | R_BF | clamp(blood.glc/3 + blood.ket) | 0.04 | 0.1 | 0.4 | 0.03 |
| Glu | 1.5·3·R_CTX | clamp(brainGlc/0.6) | 0.06 | 0.5 | 0.5 | 0.02 |
| GABA | R_STR + 0.9·R_CTX | Glu.P | 0.04 | 0.4 | 0.5 | 0.02 |
| eCB | clamp(3·R_CTX − 0.3, 0, 2) | clamp(blood.ffa) | 0.02 | 0.2 | 0.3 | 0.1 |

`ATPf = clamp(atp/2, 0, 1.2)`. Phasic dopamine: on a rewarded forage `DA.E += 0.05·max(0, rpe)·prec`.

What each one does in the core:

- DA: striatal drive `0.1 + 0.3·DA`; gain `(0.5 + DA)` on three-factor plasticity; decision noise `σ = 0.35·(0.2 + 2·DA)·…`.
- NE: cortical gain `1 + 0.5·NE`; lowers workspace ignition threshold `×(1.3 − 0.6·NE)`; opposes sleep onset.
- ACh: cortical and hippocampal tone; `level = 1 + floor(3·ACh·ATPf·brainF)` (decision depth 1..4); gates STDP
  (`aMod = clamp(ACh/0.3, 0, 1.5)`), value learning rate `(0.5 + ACh)` and episode storage probability `0.3 + 0.6·ACh`.
- Glu (pool P, not E): excitatory synaptic scale `exc = clamp(Glu.P, 0.2, 1.2)·(1 − 0.5·eCB.E)·(0.6 + 0.4·brainF)`.
- GABA: inhibitory scale `inh = 0.6 + 0.6·GABA.E`.
- 5HT: sets one angle of the sampler in §10 and feeds Affect; little else in the core.

**Port plan: simplify.** Keep DA, NE, ACh, 5HT as `E` levels in `u8` with the release / saturating-uptake rule
(`E/(km+E)` as a 16-entry table); keep one shared vesicle-depletion byte per transmitter only if fatigue effects
are wanted. Fold Glu / GABA / eCB into a single excitation scale computed from brain fuel. The fuel dependence
(precursor × ATP) is the part worth keeping: it is what makes a starving creature's brain change.

## 7. Brain regions and their update rule (core)

**Essence.** 272 leaky integrate-and-fire neurons in 14 regions with sparse region-to-region wiring; each region
receives a body-state-dependent tonic drive, so hunger, fuel and stress act on behaviour through firing rates.

**State.** See §1 (≈ 32 KB neurons + 66 KB synapses).

Regions (count): SENS 16, THAL 16, CTX 64 (4 modules × 13 excitatory + 4 × 3 inhibitory), HIP 32, STR 32
(inhibitory), HYP 16, VTA 6, RAPHE 6, LC 6, BF 6, INS 8, SCN 8, MOT 8, ENS 48 (last 12 inhibitory).

Random wiring probabilities (pre → post): SENS→THAL .25, THAL→CTX .12, CTX→THAL .06, CTX→CTX .10, CTX→HIP .06,
HIP→CTX .06, HIP→HIP .05, CTX→STR .08, STR→MOT .15, CTX→MOT .05, HYP→VTA .20, HYP→LC .15, HYP→HYP .10,
INS→LC .25, INS→CTX .06, INS→HYP .15, VTA→STR .10, LC→CTX .08, RAPHE→CTX .05, BF→CTX .06, BF→HIP .10,
SCN→HYP .25, SCN→RAPHE .15, CTX→INS .06, ENS→ENS .12, ENS→HYP .05. Weight `0.35·exp(−0.7 + 0.9·N(0,1))`,
×(−1.6) if the presynaptic cell is inhibitory. With mind v40 the same projections are grown from a developmental
genome (`develop.ts`, not read) instead of drawn i.i.d. On top: one SENS→THAL line each (w = 3), a Gaussian
thalamocortical fan per module (w ≈ 1.2, σ = 0.55), E→I 0.5, I→E −0.6, within-module E→E 0.05, sparse
cross-module E→E (p = 0.05, w ≈ 2), cortex→thalamus feedback 0.2. Observed total: 3454 synapses.

Neuron update, each of 6 substeps:

```
I   = ext[i] + inp[i] + nmda[i] + nmdaS[i]·sdiv[module] + dend[i]·sigmoid((v[i] − 0.5)/0.15)
v   = 0.8·v + g·I                      g = 1 + 0.5·NE for cortex, 1 elsewhere
thr = th[i]·(1 + (1 − clamp(ATPf,0,1))) + (cortex ? 0.25·S : 0) + thA[i]
if v > thr: spike, v = 0
nmda *= 0.9;  nmdaS *= exp(−1/360);  xTr *= 0.7;  yTr *= 0.7;  inp = 0;  dend = 0
```

`th[i] = (1 + 0.1·u)·genes[1]`. A spike adds `w·exc` (excitatory) or `w·inh` (inhibitory) to `inp[post]` for the
next substep; within-module assembly synapses instead add `0.35·w·exc` to `nmda` and `0.2·w·exc` to the slow
`nmdaS` (divided by `1 + 10·13·modAct`); cross-module synapses also add to `dend`. Low ATP doubles thresholds;
adenosine raises cortical thresholds. `rate[i] = spikes/6`; `regionRate` = region mean.

Tonic drive `ext` per region, per tick (each neuron also gets `+0.15·N(0,1)`):

| Region | Drive |
|---|---|
| SENS | 0.08·wake·(0.5 + light) + 1.5·gate·sens[i]·attention (gate 1 awake, 0.2 REM, 0.05 NREM) |
| THAL | 0.1 awake, 0.3 REM, 0.08 NREM, + top-down prediction |
| CTX | 0.2 + 0.15·ACh (± slow-wave term in N3), excitatory cells −0.35 |
| HIP | 0.1 + 0.3·ACh, + 0.6·workspace broadcast on 8 cells |
| STR | 0.1 + 0.3·DA |
| HYP (hunger) | 0.4 + 0.5·ghrelin + 0.5·clamp(1 − bg) − 0.4·vagal − 0.3·leptin |
| VTA | (0.35 + 0.05·ghrelin)·prec·ATPf·clamp(brainF)·genes[0] |
| RAPHE | (REM ? 0.05 : 0.2 + 0.25·wake)·prec·ATPf + 0.1·vagal |
| LC | REM ? 0 : 0.15 + 0.25·wake + 0.3·cortisol + 1.2·κ + 0.3·predErr + 0.3·stim |
| BF | (awake or REM ? 0.5 : 0.08)·ATPf |
| INS | 0.1 + 0.8·κ + 0.3·clamp(1 − bg) + 0.002·|hr − 70| ± 1.5·(bg − 1) on alternating cells + 2·clamp(1 − integrity) on cell 0 |
| SCN | 0.3 + 0.3·scnOut |
| MOT | 0.1·wake + 0.8·Mind motor command |
| ENS | 0.1 + 0.05·gutFill + 0.3·para |

`brainF = clamp(brainGlc/0.6 + 0.12·ket, 0, 1.3)`; κ is the curvature of the trajectory of 12 region rates in a
3-component online PCA.

Plasticity: DA-gated three-factor rule on the 432 random CTX→CTX E synapses (pre spike:
`w −= 0.002·yTr[post]·(0.5 + DA)`; end of tick if post fired: `w += 0.003·xTr[pre]·(0.5 + DA) − 0.0005·(w − 0.35)`,
clamp [0.01, 2]); multiplicative STDP on thalamocortical and assembly synapses (aP 0.02, aM 0.021, × ACh gate);
inhibitory plasticity toward a target rate ρ = 0.12; per-tick normalisation (thalamic input sum 6.0 per cell,
recurrent sum ≤ 1.6); intrinsic threshold `thA += 0.02·gate·(rate − 0.12)`, clamp [−0.3, 2]; N3 down-scaling
`w += 0.0005·(0.35 − w)`.

Mind layer (only the ignition rule was read): module salience = thalamic contrast × attention × (0.6 + max
posterior); if the best exceeds `0.06·(1.3 − 0.6·NE)` that module wins the workspace, a change of winner is an
ignition (`predErr += 0.15`, cost 0.0004 kJ); the broadcast drives HIP.

**Port plan: simplify hard, keep the rule's form.** Per creature: (a) 14 region units as `u8` rates driven by
exactly the tonic-drive table above (that table is the body-to-brain interface and ports directly as integer
sums); (b) a small genome-grown LIF net (order 32–64 cells, 8-bit weights, spikes as one `u64` bitset) using
`v = (v·205 >> 8) + I`, threshold raised by low ATP and by sleep pressure, reset to 0; (c) the three-factor
rule as `w += (trace·(128 + DA)) >> k`. Leave for later: the Mind layer, STDP / inhibitory plasticity /
normalisation, NMDA and dendritic currents, learned time constants, developmental growth details.

## 8. Action selection, foraging, value learning (core)

**Essence.** Hunger sets the per-minute probability of a foraging attempt; a noisy integrate-to-threshold race over
up to 16 sites picks one; the outcome is a Bernoulli reward that updates the site value by prediction error.

- `hunger = R_HYP − 0.4·vagal`; `pForage = clamp((3·hunger − 0.1 + 4·fatDef)·genes[2])`; attempt if awake, not
  dead and `u < pForage`.
- Race: `W = 3 + 5·(level − 1)` steps; `keep = 1 − 0.4/level`, `gain = (1 − keep)/0.4`;
  `v[k] = keep·v[k] + gain·(1.6·(drive[k] − 0.2) + σ·n + noise)`; `v > 1` → count, reset; winner = most counts.
  `drive[k]` = learned value + uncertainty bonus − `(1 + cortisol)·wAv[k]·clamp(1.2 − 0.6·ghrelin)`.
- Outcome: reward with `pTrue[k] = 0.05 + 0.25·c/3 + 0.25·s/3` (4 colours × 4 shapes); success feeds 2.2 kJ
  (taken from the world patch if one is attached).
- Learning: `rpe = rew − wSite[k]`; `wSite[k] += (0.1 + 0.4·assocPH[k])·rpe·(0.5 + ACh)`;
  `assocPH[k] = 0.7·assocPH[k] + 0.3·|rpe|`; aversive `wAv[k] += 0.3·(1 − wAv[k])` on harm, `×0.95` otherwise;
  Beta counts `bA += rew`, `bB += 1 − rew`. Sleep replay nudges `wSite` toward stored outcomes (`0.04`) and fits a
  colour + shape factor model; REM writes low-confidence beliefs for unvisited sites.
- Genes: `[vtaGain, thrScale, forageGain, reach]`, each ≈ 1, mutated log-normally, clamp [0.25, 2.5].
- Adapter to the world: heading = `site/16·2π`; speed = `min(1, 4·R_MOT + …)`; radius `0.5·∛mass`; `maxSpeed 0.6`.

**Port plan: keep, as integers.** 16 (or fewer) site values and aversive values as `u8`, the race as an integer
accumulator with Philox noise, the reward update as a shift. Drop the Beta counts, replay and factor model at first.

## 9. Death rule

**Essence.** Two rules, both about fuel: two hours of low blood glucose or failed ATP, or loss of half the
structural protein.

```
if blood.glc < 0.9 or atp < 1.0: lowGlc++  else lowGlc = 0
if lowGlc > 120:  dead, cause "hypoglycaemia"
if muscle < 60 (and not yet dead):  dead, cause "protein depletion (organ failure)"
```

**State.** `dead` (bool), `lowGlc` (counter), `deathCause` (string).

`dead` only gates foraging, maternal transfer and the crevice squeeze; the adapter's `alive()` returns `!dead`.
`step()` keeps running, so a dead organism keeps spending (observed ATP −545 kJ). Chemoton lysis is not death.
Tissue damage alone never kills (it raises cortisol and INS drive). Ageing exists only in LifeSystems (off).

**Port plan: keep, and close the hole.** `lowGlc` as `u8` saturating counter, protein threshold as an integer
compare, plus Chemoton lysis as a third cause once the Chemoton is the reproduction gate. On death the creature's
remaining pools must be handed to the world cell (detritus) in the same tick and the record retired; no stepping
after death, and no pool may go negative.

## 10. Heart

**Essence.** Heart rate is a leaky function of sympathetic minus parasympathetic tone; it scales glucose delivery
to the brain and costs ATP.

- `hr += 0.2·(60 + 70·symp − 25·para − hr)`; `perfusion = hr/70`. Initial 70; observed 57–130.
- Effects: brain glucose uptake `∝ 0.3·perfusion` (§3 row 15); ATP cost `0.00006·hr` kJ/min; INS drive
  `0.002·|hr − 70|`. With LifeSystems an intrinsic pacemaker replaces the target (`hr += 0.25·(intrinsicHR − hr)`).

**State.** `hr`, `perfusion` (derived).

**Port plan: simplify.** Do not store it: compute `hr = 60 + (70·symp − 25·para)` scaled to `u8` when needed, or
keep one `u8` with a shift-relaxation if the lag matters. Keep the two causal uses (brain delivery, ATP cost).

## 11. XPBD shell (`ShellHydrostatXPBD`, apex only)

**Essence.** A 16-node closed 2-D shell with distance constraints and an area (volume) constraint whose internal
chords can be softened by a single "catch" parameter, so the body can deform through a gap while keeping its area.

**State.** 16 × (px, py, vx, vy) + rest lengths (16 edges, 16 + 16 chords, 8 diameters, 16 radii, area,
half-height) + 5 limb anchors / lengths / permutation + 16-value bioelectric field + 4 energy scalars + catch
softness ≈ 160 f64.

- Rest shape: `r(θ) = 1 + 0.18·cos(3θ + 0.4) + 0.11·sin(2θ − 0.7)`, θ = 2πi/16. Limbs at nodes [2, 5, 8, 11, 14],
  rest lengths [0.62, 0.88, 0.51, 0.95, 0.73] (carried, not simulated).
- Step (`dt = 0.02`, node mass 0.01): predict `x += dt·v`; then 14 Gauss–Seidel iterations of
  (a) 16 edge constraints, (b) 16 + 16 chord and 8 diameter constraints, (c) optional wall `|y| ≤ H(x)`,
  (d) area constraint `A(p) − A0 = 0` with gradient `½(y_next − y_prev, x_prev − x_next)`.
  Each distance constraint: `Δλ = −C/(2 + α/dt²)`, both endpoints moved by `Δλ` along the edge.
  Compliance terms: edge `0.002 + 0.028·s`, chord `1e-6·(1 − s) + 4·s²`, area `1e-7`, with `s` = catch softness.
  Velocity `v = 0.94·(x − x_prev)/dt`; kinetic energy is then rescaled so kinetic + elastic never exceeds the
  mechanical budget, and the difference is added to a dissipation pool.
- Nozzle wall: `H(x) = 2.6 − (2.6 − h_ap)·exp(−x²/1.15²)`. Crevice test: start at x = −2.35, 170 steps with thrust
  acceleration `6.5·0.8`; pass if the centre ends beyond x = 1.25 and area error stayed below 1 %.
- Coupling: catch softness relaxes (0.25) toward `clamp((atp − 1.05)/0.45)`; when awake, moving (> 0.005) and
  `atp > 1.25`, up to 0.00008 kJ of ATP per tick is injected as kinetic energy of node 0 and one XPBD step runs;
  `canSqueezeCrevice` charges up to 0.012 kJ and runs the crevice test on a scratch copy; shell proprioception
  `tanh(0.35·Σ|field[0..2]| + 0.15·s + 10·areaErr)` adds `0.15×` to INS drive.
- `simulateUnevenRamp` and `executeRemappedGoalNavigation` are scripted demonstrations (fixed formulas with
  constants per branch), not physics; do not port them.

Observed: with no locomotion input the shell never stepped; catch softness sat at 1.0 because ATP stays near 2.

**Port plan: leave for later, keep one scalar now.** 14 iterations × 56 constraints per creature per tick is not
affordable for a population. Keep `catch softness` (`u8`, ATP-dependent) and decide gap passage with an integer
compare (`aperture ≥ rigid_width·(1 − k·softness)`), charging ATP for the attempt. If a deformable body is wanted
later, run a fixed-point XPBD only for the few creatures in contact with an obstacle, as an event, the way the TS
runs its crevice test on demand.

## 12. Other mechanisms seen (short)

- **"Quantum layer"**: a classical 8-amplitude state-vector simulation of 3 qubits (angles `π·[DA, 5HT, NE]`,
  RY, two CX, RZ), sampled with the seeded PRNG to give `qOut ∈ 0..7`, which supplies decision-noise sign; the
  trace of a finite-difference metric scales noise by `(1 + 0.2·trace)`. It is a neuromodulator-shaped 8-outcome
  sampler. Port: a Philox draw through an 8-bin table, or drop; do not describe it as quantum.
- **Integrity / tissue**: `tissue` 10 kJ, `integrity = tissue/10`; damage moves tissue to blood amino acids,
  repair moves it back and costs ATP (§3 rows 16–17). Keep (merged into the protein pool plus a damage byte).
- **Lineage** (adapter `spawn001`): genes mutate log-normally (σ configurable), persona, developmental genome,
  epigenetic marks (0.65×), germline bias, soma gene (±0.03), HEPTARCH traits (±0.025) are inherited. Maternal
  provisioning moves fat (above 180 kJ) and glycogen (above 8 kJ) out of the parent, booked as negative intake.
- **Substrate tap** (`substrate.ts`): six host-machine channels (jitter, CPU, heap, GC, loop delay, dome) fed as
  senses; synthetic mode is seeded noise. Not a body organ; nothing to port.
- **Philox4x32-10** as implemented: multipliers `0xD2511F53`, `0xCD9E8D57`; key bumps `0x9E3779B9`, `0xBB67AE85`;
  counter `(tick, organ, draw, domain)`, key `(seed ^ domain·0x85EBCA6B, (entity+1)·0xC2B2AE35)`. This addressing
  scheme (entity, organ, tick, draw) is the right one for the native engine.

## 13. Proposed compact record (proposal, not measured)

| Group | Fields | Bytes |
|---|---|---|
| Energy pools, one unit (2⁻²⁰ kJ) | gut, blood glucose, blood lipid, blood amino, glycogen, fat, protein, ATP as `u32` | 32 |
| Chemoton | X, A1, A2, M, V, W as `u16`; z1, z2 as `u16` | 16 |
| Hormones / modulators | insulin, glucagon, cortisol, ghrelin, DA, NE, ACh, 5HT as `u8` | 8 |
| Body state | sleep pressure, sleep state, clock offset, low-glucose counter, damage, catch softness, core temp, PAG mode | 8 |
| Slow organs | 4 peptides, 4 epigenetic marks | 8 |
| Brain | 14 region rates `u8`, 16 site values + 16 aversive values `u8`, spike bitset `u64`, 2 bytes spare | 56 |
| Total hot record | | 128 |

A 128-byte record puts 256 creatures in a 32 KB L1 block; the small LIF net's weights (genome-derived, mostly
read-only) would sit in a separate per-lineage table. Heat and excretion go to world cells, not to the record.
