# How the existing lab (V:) proves things, and the native equivalent of each proof

Key: `gates`. Written 2026-11-17. Scratch: `S:\v40\gates\`. V: was only read.

## 0. Evidence status

- **Read** (source, not run): `V:\pipeline\run_all.mjs`, `V:\docs\REPORT.md` lines 1-150, `V:\docs\decisions\0001-...md`,
  `V:\creatures\001\preregistrations\PREREG-V50-HELDOUT.json`, `scientific_rigor_suite.ts` (lines 1-520, 862-1478),
  `apex_ultimate_suite.ts` (1-110, 900-1139), `lab\conformance\{run,suite,originality}.ts`, `viability_v39.ts`,
  `lab\src\adapters\c001.ts` (30-320), `lab\src\instruments\{stats,lesionStats}.ts`, `docs\{CLAIMS-REGISTER,RISKS,INCIDENTS}.md`,
  heads and greps of `qualities_v40.ts`, `passes_100_suite.ts`, `tests_v40.ts`, `independent_scientific_audit.py`,
  `tests\run_grade_a_tests.mjs`, `scripts\lint.mjs`, `preregistrations\v4.0.md`, and the errata on file at
  `W:\ALife-Framework-v2.0-Errata.md` plus `W:\ALife-v3.0-Errata-Closure-Report.md`.
- **Not run:** the lab pipeline itself. `run_all.mjs` writes into `V:\lab\out` and `V:\creatures\001\results`, and V: is read-only.
  Every lab number below is **recorded by the lab** (artifact dated 2026-11-12T17:07:43Z), not reproduced by me.
- **Ran now** (section 8): a C23 exact signed-rank / Berger / Holm kernel in my scratch, checked against the 14 paired
  comparisons the lab recorded. Result: 0/14 mismatches, max Holm p = 1.0563552380e-02.
- **Path correction:** the task named `V:\creatures\001\results\PREREG-V50-HELDOUT.json`. The file is at
  `V:\creatures\001\preregistrations\PREREG-V50-HELDOUT.json` (SHA-256 `f075bbb3c022e4c8...5202`, computed now with `sha256sum`).
- The top 150 lines of `REPORT.md` are a product self-inspection ("NOT SHIPPABLE", 25 gate points open, all of them
  release, hosting, 2FA and agent-sandbox items). It contains no scientific method beyond pointing at the 9 gates, so it adds
  nothing to the tables below.

## 1. The 9 pipeline gates (`node pipeline/run_all.mjs`)

Each gate is a child process; a non-zero exit stops the pipeline. "Native equivalent" assumes the owner's laws: integer
state, exact conservation, Philox4x32-10, same hash for any thread count, C23 + NASM.

| # | Lab gate (script) | What it actually checks | Pass rule in the lab | Native equivalent test |
|---|---|---|---|---|
| 1 | Browser ESM bundle integrity (`esbuild` on `organism.ts`, `lab/src/index.ts`) | The two bundles compile. Nothing is executed. | esbuild exits 0 | `T-BUILD`: the engine builds warning-free (`-Wall -Wextra -Werror`) with gcc 16 and clang 21, NASM objects link, and both compilers' binaries give the **same state hash** on a fixed seed for 1, 2, 8 and 24 threads. |
| 2 | Conformance + C-NC + originality (`lab/conformance/run.ts`) | 17 required checks per creature (list in section 2) on 3 creatures, seed 2000; 6 broken mutants must be rejected (energy-creating `give`, `Math.random` metric, missing hook, cosmetic region / module / organ); a regex scan of tracked files for other projects' names, ported code, third-party licence headers. | all required pass, all 6 mutants rejected, 0 originality fails | `T-CONF`: same list on the native ABI (section 2). `T-MUT-CONF`: compile-time mutants (leaky transfer, clock-seeded RNG, dead organ) must fail. `T-ORIGIN`: source scan plus a symbol scan (`objdump -t`) for anything not in the allow-list. |
| 3 | Metabolic viability (`viability_v39.ts`, no arguments) | One "steady" run: 5 creatures, seed 2000, 12 simulated days, prints JSON (deaths, daily fat / glycogen / intake / expenditure, world ledger). | **None.** The script has no assertion and never sets an exit code (weakness W1). The header's "0/30 deaths, starvation cascade, refeeding recovery" is not enforced here. | `T-VIAB-STEADY`: N seeds, 0 deaths over the steady window, stores within a stated band. `T-VIAB-STARVE`: stage order glycogen < ketosis < fat exhaustion < death, each asserted. `T-VIAB-REFEED`: recovery after refeeding. All three assert and exit non-zero. |
| 4 | Functional and causal suite (`qualities_v40.ts`, 28 checks) | Per mechanism: an intact organism against a lesioned or control twin on one dev seed (100..400), a hand-set margin (example: seed 131, `cautionBonus` intact > lesioned + 1.0), and ledger error < 1e-11 for every organism touched. | 28/28 | `T-CAUSAL-<organ>`: one test per organ, but across a seed block with the exact test of section 4, not one seed and a margin (W11). Ledger residual must be exactly 0. |
| 5 | 100-pass benchmark (`passes_100_suite.ts`, 100 checks) | Same pattern over 100 items (dev seeds 101..999); tracks the worst ledger error of every organism and dome. | 100/100 | Fold into `T-CAUSAL-*` and `T-REGRESS-*`; keep only items that state task, metric, threshold and control (errata H-03). |
| 6 | Apex 13-gate + mutant-catch (`apex_ultimate_suite.ts`, 25 checks) | 13 internal gates (section 3), 5 negative mutants, paired Wilcoxon (normal approximation) on dev seeds 301..324 with Holm across the stochastic checks, observer independence over 1000 ticks, default-parity hash `15f21609` for seed 42 after 300 ticks. | 25/25, 13/13, 5/5 mutants, max p_Holm <= 0.05, worst ledger < 1e-11 | `T-MUT-1..5` (section 3), `T-OBSERVER` (full-state hash equal with every read-only probe hammered each tick), `T-GOLDEN` (golden full-state hash per seed, per version). |
| 7 | Proof + held-out suite (`scientific_rigor_suite.ts`, 9 checks) | 7 deterministic "theorem" checks and 2 held-out empirical checks (section 4, 5). Reads seeds from the prereg file, writes `nobel_fields_alife_proofs.json`. | 9/9, max p_Holm <= 0.05, world ledger < 1e-12 | `T-HELDOUT-H*`: the five-arm design on a held-out block with the C kernel of section 8. Of the theorem checks only chemoton conservation, RNG parity and the wire-format check carry over (section 4). |
| 8 | Independent Python audit (`independent_scientific_audit.py`, 9 checks) | Re-derives gate 7's numbers from the JSON gate 7 wrote, with 80-digit `Decimal` and exact `Fraction`, sharing no code: prereg hash and seed ranges, Mittag-Leffler, Turing d_crit, stoichiometry rank over Q, CHSH, Blahut-Arimoto, exact 2^N p-values for 14 comparisons + Holm + Price, Philox vectors and binary formats, Morris-Lecar RK4. It does not rerun any simulation (W15). | 9/9 | `T-AUDIT`: a second implementation in a different language (dev tooling, allowed outside the simulation) recomputes from the dumped event log: Philox outputs, ledger totals as big integers, the exact p-values, and the state hash of a dumped snapshot. |
| 9 | Lint + strict types + tests + coverage + secrets (`scripts/lint.mjs`, `tests/run_grade_a_tests.mjs`) | `tsc --noEmit` with zero output; `node --check` on every `.mjs`; static scan for dangerous constructs and workstation paths; `node --test` on `grade_a_suite.ts` (T-NUM-001..004, T-REL-001, T-SEC, T-SIM-001, T-UI); V8 line coverage >= 90.0% on core modules; secret scan. | all clean, coverage >= 90% | `T-STATIC`: zero warnings on two compilers; source scan for forbidden constructs in simulation code (float or double state, `rand`, `time`, heap allocation in hot loops); `objdump` scan proving the hot objects import no libm or allocator symbol. `T-COV`: `gcov` line coverage threshold on the engine core. `T-SNAPSHOT`: snapshot, restore, continue; hash must equal the uninterrupted run (lab T-REL-001). |

## 2. Gate 2 in detail: the conformance list

Source: `lab\conformance\suite.ts` (seed 2000). Required unless marked.

| Lab check | Method | Tolerance | Native equivalent |
|---|---|---|---|
| identity, parts, couplings reference parts | structural | exact | `T-CONF-STRUCT`: every organ and coupling registered once, sizes > 0 |
| deterministic per seed | two builds of seed s give equal `metrics()` after 200 steps; seed s+1 differs | `===` on metrics only | `T-DET`: full-state hash equal for the same seed, different for another seed, for every thread count |
| required metrics finite | 6 metrics after 50 steps | finite | `T-CONF-METRICS`: no saturation or overflow flag set |
| readState / writeState round trip | copy full state of every region into a twin, run 50 more steps, compare | 1e-12 | `T-ROUNDTRIP`: write-back of a dumped organ leaves the hash unchanged, exactly |
| stimulate changes the part | stimulated twin differs after 3 steps | 1e-12 | `T-STIM` |
| lesionCoupling changes the trajectory | at least 1 coupling, when zeroed, moves some region within 200 steps | 1e-12 | `T-LESION-MOVES`: **every** coupling, not at least one |
| no cosmetic parts (C-NC) | drain 340 kJ, run 390 steps, perturb one part, and within 120 steps another region, the motor vector or a metric must differ from the twin | 1e-12 state, 1e-11 metrics | `T-CNC`: same, exact inequality of hashes that exclude the perturbed organ. This is a wiring check only (errata H-04). |
| motor vector, biosphere hooks present, body spec valid | structural | | `T-CONF-ABI` |
| energy ledger exact in a dome (1 day = 1440 ticks) | world and creature ledger error | < 1e-9 | `T-LEDGER-DAY`: residual == 0 |
| God hooks conserve energy | 1000 ticks with toxicity, gravity, parasites, recycling, radiation, temperature, O2, "quantum kicks", a game | < 1e-9 | `T-LEDGER-PERTURB`: every world perturbation enabled at once, residual == 0 |
| each energy hook books exactly | give, drain, work, harm, takeWaste, shiftModulator, rewire, senseLines called directly | < 1e-9 | `T-LEDGER-HOOK`: one test per transfer function, residual == 0 |
| forages in a dome, moves in a dome | eaten > 0 in a day; path > 0 in 600 ticks | > 0 | `T-ALIVE-SMOKE` |
| spikes() binary, routing() (optional) | | | not needed |

## 3. Gate 6 in detail: 13 internal gates and the 5 mutants

| Apex gate | Lab criterion | Native equivalent |
|---|---|---|
| GATE_01 determinism, Philox replay | two checks pass | `T-DET`, `T-PHILOX-KAT` (known-answer vectors; the lab compares TS, WASM and Python on 512 vectors, counters `[i, 7i+3, 13i+5, 42]`, keys `[0x9e3779b9^i, 0x85ebca6b]`; I did not check them against the reference implementation's vector file) |
| GATE_02 originality, LLM-free | scan passes | `T-ORIGIN` |
| GATE_03 First and Second Law, 11 pools | worst ledger < 1e-11 | `T-LEDGER-*` with residual == 0; `T-ENTROPY`: the dissipated pool never decreases |
| GATE_04 five mutants caught | 5/5 | `T-MUT-1..5` below |
| GATE_05 hydrostat XPBD, GATE_06 chemoton and homeostasis | one check each | `T-BODY-*`, `T-CHEMOTON` |
| GATE_07 causal ablation and lesions | three checks | `T-CAUSAL-*` |
| GATE_08 Holm FWER | max p_Holm <= 0.05 over >= 5 checks | `T-STATS` (section 8 kernel) |
| GATE_09 safety rollback | one check | `T-SNAPSHOT` |
| GATE_10 seed firewall | a regex on the suite's own source finds no `seed: N` with N >= 1224 | `T-FIREWALL`: the held-out seed list is a build input with a pinned hash; the binary refuses a held-out seed unless a signed "official run" flag is set, and logs each use |
| GATE_11 observer independence | hash and JSON snapshot equal after 1000 ticks with every telemetry hook called | `T-OBSERVER` |
| GATE_12 hardware tiers, multirate | two checks | `T-TIERS` (same hash on every tier; fidelity never depends on the camera) |
| GATE_13 default parity | seed 42, 300 ticks, hash `15f21609`; previous 24 checks all pass | `T-GOLDEN` |

The five negative mutants (`apex_ultimate_suite.ts` 914-1022):

| Mutant | Lab implementation | Caught when | Native equivalent |
|---|---|---|---|
| 1 Leaky | `Ledger11Pool.transfer(4, 9, 0.005, -2e-11)` 800 times | `leakDetected` (absolute tol 1e-11, relative 5e-14, or any decrease of the irreversible pools) | `T-MUT-1`: a build flag drops **one energy quantum** in one transfer; the ledger test must fail on that tick. One quantum is the smallest possible leak, which answers errata H-08. |
| 2 Cosmetic | a `FAKE_CORTEX` region that evolves each tick but touches nothing | the C-NC check names it | `T-MUT-2`: an organ with live state and no outgoing coupling must fail `T-CNC` |
| 3 Oracle | compares a legitimate chooser (learning frozen, hidden payoffs re-randomised) with one that reads the hidden `pTrue` | oracle - legit > 0.12 and legit < 0.46 | `T-MUT-3`: a policy with read access to hidden world state must beat the creature on a blinded task by a stated margin; and a link-time check that creature code cannot reference world-private symbols |
| 4 Frozen | reversal task (site 0 goes 0.02, site 3 goes 0.95), 420 ticks, plasticity on or off | plastic picks rich >= 0.75, frozen <= 0.25, frozen weight unchanged | `T-MUT-4`: plasticity disabled; post-reversal choice must stay at the pre-reversal level |
| 5 Shuffled | Hopfield bank of 8 patterns, masked recall with a permuted, negated query | cos(intact) > 0.95 and > cos(shuffled) + 0.35 | `T-MUT-5`: memory keys permuted; recall must collapse |

## 4. Gate 7 in detail: the nine checks

| Check | Threshold in code (`scientific_rigor_suite.ts` 1198-1376) | Carries over to native? |
|---|---|---|
| THM_01 Caputo L1 on a graded mesh against Mittag-Leffler | order within 0.08 of 1.55; graded beats uniform by 0.35; error at N=256 < 5e-4 | No. It tests a solver the native engine need not have. |
| THM_02 Schnakenberg-Turing dispersion | homogeneous Re < 0; supercritical max Re > 1; subcritical < 0; L2 grows > 20x at d=25, decays < 0.05x at d=2 | Only if the world has a reaction-diffusion field: `T-FIELD-DISPERSION` against the discrete dispersion relation. |
| THM_03 Morris-Lecar | 0 spikes at 30, >= 5 at 105, monotone f-I, 1 spike at 2 ms gap, 2 at 80 ms | Only if that neuron model is used. The general form is `T-NEURON-FI`: rheobase, monotone f-I, refractory block for the native integer neuron. |
| THM_04 chemoton + nozzle | l^T S == 0 exactly; moiety drift < 1e-12 over 8000 steps; entropy production >= 0, > 5 at start, < 1e-20 at the end; rigid shell does not pass a 0.55 aperture, softened shell does; throat volume error < 1e-4 | Yes. `T-CHEMOTON`: integer stoichiometry, moiety total exactly constant; `T-BODY-NOZZLE`: stiff body blocked, softened body passes, with no pass/fail branch on the lesion flag (ADR-0001 decision 2). |
| THM_05 Bell-CHSH, Mermin, QFI | analytic values to 1e-14; 16,000-round game win rate > 0.835 | No. It is a statevector identity, unrelated to creatures. Law 7 also forbids presenting it as a property of the simulation. |
| THM_06 Blahut-Arimoto, Hoel, Pearl | duality gap < 1e-12; capacity > 0.80 bits; fixed 4x4 examples | No as a gate. The estimators may be reused as instruments, each with its own unit test. |
| THM_07 WASM parity + binary formats | 0 mismatches on 512 Philox vectors; chemoton flux and dot product equal bit for bit; snapshot length == 12 + 32*8*8; 32 sensor-log frames replay | Yes. `T-PHILOX-KAT`, `T-ASM-PARITY` (NASM kernel == C reference on random inputs, bit for bit), `T-WIRE` (snapshot length and replay hash). |
| EMP_08 Price equation + neutral shadow (H4) | Price identity error < 1e-13 on every transition; score p < 0.01 and trait p < 0.01 | Yes. `T-EVO-SHADOW`: adaptive lineage against a neutral shadow (uniform reproductive weights, same mutation), paired by seed. `T-PRICE`: the identity holds **exactly** in integer or rational arithmetic; treat it as a bookkeeping check, not evidence (W10). |
| EMP_09 five-arm IUT + Holm (H1, H2, H3, H5) | max p_Holm <= 0.05 and worst world ledger < 1e-12 | Yes. `T-HELDOUT-H1..H5` (section 5 and 6). |

## 5. Control designs and how each is implemented

All arms of one comparison share the creature seed and the world seed (paired by seed).

| Control | Lab implementation (file:line) | What it removes | Native equivalent |
|---|---|---|---|
| **Intact** | `{apexMind, spatialChemotaxis, forceWake}` | nothing | reference arm |
| **Passive-Body** | `c001.ts:274`: `act()` returns speed 0, no forage, no heading. The brain still runs and pays its cost. | all behaviour | `ARM_PASSIVE`: motor output gated to zero after the brain step; metabolic cost unchanged. Report it, but do not count it as evidence (W7). |
| **Shuffled-Stream** | `c001.ts:158-169`: every `WorldSense` record is queued. For the first 24 ticks only the food-plume angle and hazard-gradient angle are replaced by rotating angles (`t*2.39996`, `t*1.71828` mod 2 pi). After that the creature receives one of its **own past** records, index `(t*13+7) % (len-8)`, queue capped at 96. No RNG is involved. | the temporal link between world and input, for every sensory field at once (W8) | `ARM_SHUFFLE`: permute the tick index of the tested channel only, with a Philox stream in its own domain; all other channels intact. Add `ARM_YOKED`: replay the intact twin's recorded stream to a second body. |
| **Targeted-Lesion** | H1: `aversiveLesion` (hazard no longer drives LC / INS or the escape tuning, `c001.ts:186`; aversive learning off, `organism.ts:1139`), `pagLesion` (alarm speed term 0, `c001.ts:298`), `topoScramble` (thalamo-cortical topography randomised at build, `organism.ts:727`). H2: `apexLesion: "catchSoftening"`. H3: `apexLesion` = `chemotonA`, `chemotonM` or `chemotonV` on the founder and every descendant. | the named mechanism | `ARM_LESION(organ)`: coupling gain mask set to 0 (`lesionCoupling(id, 0)` in the lab). The lesion must act through the dynamics, never through a result branch. |
| **Size-Matched-Sham** | `c001.ts:56-64, 285-290`: a seeded Fisher-Yates permutation (`mulberry(seed*9137+29)`) of which ring-basis vector each of the CE cortical neurons maps to in the population-vector readout. Same neurons, same spikes, same energy cost. | the decoder's map | This is a decoder scramble, not a sham lesion (W6). Native: `ARM_SHAM_LESION` removes the **same number of synapses or units elsewhere** (errata C-08) and is expected to stay near Intact; the specificity claim is Lesion < Sham. Keep the decoder scramble as a separate arm, `ARM_READOUT_SCRAMBLE`. |
| **Sham-cost** | **Not present under that name.** Nearest: the sham arm above (cost-neutral), "sham stimulation" (`tests.ts:31`), the sham twin by snapshot / restore without the pulse (`tests_v36.ts:29`, `lab/src/instruments/rpt.ts`), matched-rate noise bursts (`v40mods.ts:20`). Whether a lesioned organ keeps paying its metabolic cost: unknown, not checked. | | `ARM_SHAM_COST`: the organ is lesioned **and** its running cost is still debited (and the mirror arm: organ intact, cost waived). Without it a lesion deficit can be an energy-budget effect. |
| **Random-policy** | **Not an arm of the held-out design.** Present only as "random opt-out at the matched per-seed rate" (`tests_v40.ts:79-94`: the control opts out with probability equal to the intact arm's measured rate for that seed, using a separate world RNG so the world is identical across arms) and a confidence-shuffled arm. | information in the decision, keeping the action rate | `ARM_RANDOM_POLICY`: actions drawn from Philox (own domain) at the intact arm's per-seed action rates and the same energy cost. |
| **Neutral shadow** (H4) | `scientific_rigor_suite.ts:918-920`: reproductive weights all 1.0 instead of `0.15 + intake^1.5` (alive) or 0.01 (dead); same mutation sigma 0.16, population 8, 8 generations, 120 ticks each. | selection | `ARM_NEUTRAL`: identical run with fitness-blind parent sampling |
| **Chemoton cuts** (H3) | metabolism, membrane or template subsystem removed | one of three autocatalytic subsystems | `ARM_LESION(chemoton-*)`; note the recorded result is 0 buds on 24/24 seeds in all three arms (W7) |
| **Matched world** | separate world RNG, identical across arms (`tests_v40.ts:11, 26`) | | Philox domains: world draws keyed by (run seed, world, entity, tick), never by creature state |

## 6. Statistics

### Held-out design (gate 7, prereg v5.0.0, locked 2026-11-12)

- **Test:** exact one-sided paired Wilcoxon signed-rank ("greater"). Differences with |d| <= 1e-15 are dropped; ties
  (|a-b| <= 1e-15) get midranks; twice-midranks are integers, so the null distribution over the 2^n sign assignments is
  counted exactly by subset-sum dynamic programming in BigInt. p = tail / 2^n.
- **Effect size:** Hodges-Lehmann median of the Walsh averages (d_i + d_j)/2, i <= j. No minimum effect size is required.
- **Conjunction over controls:** Berger intersection-union, p_IUT = max over the control arms; no penalty inside one hypothesis.
- **Family-wise error:** Holm step-down over M = 5 hypotheses, alpha = 0.05.
- **Seeds:** 24 per hypothesis. H4 uses 5001..5024; H1, H2, H3 and H5 all use 5101..5124. Smallest attainable p = 2^-24 = 5.96e-8.
- **Run lengths:** H1, H2, H5: 480 ticks, one creature. H3: 300 ticks, founder with 410 kJ fat, max population 8. H4: 8 x 120 ticks, population 8.
- **Metrics:** H1 `eaten - 25*(1 - integrity)`. H2 `eaten`. H3 number of autonomous buds. H4 `meanIntake + 15*meanTrait` (and the trait alone).
  H5 `T(S->M) + T(M->S)` in bits: plug-in transfer entropy, alphabet 4 (heading and world-gradient angle in quadrants), history 1, 479 transitions.
- **Extra thresholds:** H4 needs p < 0.01 on both score and trait. EMP_09 needs world ledger < 1e-12.
- **Seed firewall** (prereg): dev 1..499 and 2000..2007; legacy bench 1200..1247; held-out >= 5000. Rule: "No parameter, threshold, weight,
  or constitutive constant may be tuned on seeds >= 5000."
- **Recorded result** (lab artifact, not rerun):

| Hypothesis | Raw p (exact) | Holm p | Controls: p, intact mean vs control mean |
|---|---|---|---|
| H1 hazard chemotaxis | 1444/2^24 = 8.61e-5 | 2.58e-4 | passive 1/2^24 (22.7 vs -11.5); shuffled 14/2^23 (12.2); lesion 1444/2^24 (9.58); sham 5/2^23 (6.58) |
| H2 crevice refugia | 1626/2^23 = 1.94e-4 | 3.88e-4 | passive 1/2^24 (24.4 vs 0.73); shuffled 106/2^24 (17.8); lesion 7/2^23 (16.8); sham 1626/2^23 (11.8) |
| H3 chemoton budding | 1/2^24 = 5.96e-8 | 2.98e-7 | all three cuts: 2.17 buds vs 0 on every seed |
| H4 neutral shadow | 2/2^24 = 1.19e-7 | 4.77e-7 | trait alone: 14928/2^24 = 8.9e-4; Price error 4.3e-16 over 336 transitions |
| H5 transfer entropy | 177227/2^24 = 1.06e-2 | 1.06e-2 | HL = 0.116 bits |

### Other statistics in the lab

| Where | Test | Seeds | Threshold |
|---|---|---|---|
| Gate 6 (`apex_ultimate_suite.ts` 56-108) | Paired Wilcoxon, **normal approximation** with continuity and tie correction; within-pair dominance `(pos - neg)/N`; Holm | dev 301..324 (24) | p_Holm <= 0.05 |
| Prereg v3.x - v4.0 (`preregistrations/v4.0.md` section 0) | Cohen's d and Welch t against **every** control; non-ceiling rule (a bounded metric with mean > 0.95 max fails) | 48 test seeds per round (for example 1064..1111), each block retired after use | d >= 0.8 and t >= 2.75 |
| Same, freezing rule on dev | pooled d over two 16-seed blocks (2000..2031) | 32 | pooled d >= 1.0 and d >= 0.8 in each block |
| `lab/src/instruments/stats.ts` | Monte Carlo paired sign-flip test (B = 5000, two-sided, p = (c+1)/(B+1)), paired Cohen dz, Holm, Benjamini-Hochberg, percentile bootstrap CI (B = 2000), Welch, Spearman | caller's | reporting only |
| Gates 4 and 5 | none: one seed, a hand-set margin | 1 per check | margin |

### Native equivalent

`T-STATS` uses the C kernel in section 8 (integer counts, exact fractions). Because native metrics are integers, ties and
zeros are exact and the 1e-15 tolerance disappears. The plan must state per hypothesis: the primary contrast (errata C-06),
the seed count from a power or precision argument (errata C-03; the v3.0 closure text gives >= 24 for large effects, 48-64
medium, 96-120 subtle, and "inconclusive" when under-powered), the minimum effect size, and burn-on-fail for a held-out
block (errata A-06). Regression floors use a one-sided non-inferiority test with a preregistered margin, not TOST (errata C-05).

## 7. Conservation checks and tolerances

| Quantity | Lab definition | Tolerance | Native equivalent |
|---|---|---|---|
| Creature energy ledger | `ledgerError = |energy() - E0 - eaten| / (E0 + eaten)`; `energy()` sums gut, blood, liver glycogen, fat, muscle, brain glucose, ATP, **heat, excreted**, tissue and sub-module stores (`organism.ts:760-761`), all Float64 | < 1e-11 (gates 4, 5, 6), < 1e-9 (conformance) | `sum(pools) + heat + excreted - E0 - intake == 0` in 64-bit integer quanta, checked every tick in debug builds and every N ticks in release |
| World ledger | `|total - E0 - grown - born| / E0`; total = patch food + soil + creature energy + detritus + parasites + grazers + grazer heat (`dome.ts:94, 221`) | < 1e-9 (conformance), < 1e-12 (gate 7); recorded worst 9.33e-15; claims register quotes < 2e-14 | exact integer equality of world total against E0 + logged solar input; per-thread partial sums merged in a fixed order so the result is thread-count independent |
| 11-pool ledger | Neumaier-compensated pools; `verifyStep(absTol = 1e-11, relTol = 5e-14)`; Second-Law fault if the irreversible pools (9, 10) decrease by more than 1e-15 or pool 10 goes below -1e-15 (`cleanslate_foundation.ts:116-190`) | as stated; recorded relative error 2.31e-15 | no compensation needed with integers; `T-ENTROPY`: the dissipation counter is monotone, exactly |
| Chemoton moiety | l = [2,1,2,1,1,1], l^T S == 0 as integers; closed-vessel drift over 8000 steps | exact for l^T S; < 1e-12 drift (recorded 4.45e-13) | exact with integer molecule counts; `T-CHEMOTON-RANK`: rank of S over Q equals 5, computed once by the audit tool |
| Entropy production | sum (J+ - J-) ln(J+/J-) >= 0, relaxing below 1e-20 | >= 0 | if rates are integer or fixed-point: forward and reverse counts obey detailed balance at equilibrium within a stated count bound |
| Soft-body volume | throat volume error fraction in the nozzle test | < 1e-4 | fixed-point area error bound stated per mesh; numerical dissipation booked to its own pool, never negative (errata A-03) |
| Price identity | `|dz - (Cov(w,z) + E[w dz])|` with w_bar = 1 | < 1e-13 (recorded 4.30e-16) | exact in integers scaled by population size |
| Determinism | equal metrics after 200 steps; hash equality (`hash()` mixes scalar state and **every 97th synaptic weight**, `organism.ts:1424`); JSON snapshot equality in the observer test | exact | full-state hash over the whole arena (every byte of every SoA block, fixed order), equal across thread counts, compilers and runs |
| Cross-runtime numeric parity | WASM == TS on Philox, dot product, chemoton flux | 0 ULP | NASM == C reference, bit for bit; no libm call inside the simulation (lab risk R-03) |

## 8. What I ran: a native statistics kernel checked against the lab's record

Files: `S:\v40\gates\exact_wsr.c` (C23: exact signed-rank DP with `uint64_t` counts, Berger max, Holm),
`S:\v40\gates\dump_arms.py` (dev tooling: copies the 14 recorded arm pairs out of the lab's JSON as hex floats),
`S:\v40\gates\arms.txt`.

```
cd /s/v40/gates
python -I dump_arms.py V:/creatures/001/results/sc-port/nobel_fields_alife_proofs.json S:/v40/gates
gcc -std=c2x -O2 -Wall -Wextra exact_wsr.c -o exact_wsr.exe && ./exact_wsr.exe arms.txt | tail -7; echo "exit=${PIPESTATUS[0]}"
```

```
h4Trait/NeutralShadow                                n=24 nz=24 p=933/1048576 (lab 14928/16777216) MATCH
holm H1 (IUT max of 4)      raw=8.606911e-05 adj=2.582073e-04
holm H2 (IUT max of 4)      raw=1.938343e-04 adj=3.876686e-04
holm H3 (IUT max of 3)      raw=5.960464e-08 adj=2.980232e-07
holm H4 score               raw=1.192093e-07 adj=4.768372e-07
holm H5 transfer entropy    raw=1.056355e-02 adj=1.056355e-02
max Holm-adjusted p = 1.0563552380e-02 ; mismatches vs lab record = 0/14
exit=0
```

The build printed no warning. This shows the lab's p-value arithmetic is correct for the arms it recorded. It says nothing
about whether those arms come from a sound experiment (section 9). One trap met on the way: `%Le` prints garbage with the
mingw-w64 UCRT `printf`; print `long double` as `double`.

## 9. Known weaknesses

### 9a. Flagged in the owner's own records

| Source | Weakness | Status there |
|---|---|---|
| `INCIDENTS.md` item 1 | H2's first metric `eaten + 8*creviceEntries` rewarded the shuffled control for oscillating across the crevice rim (a proxy that could be gamed); uniform flora confounded H1; a static position angle confounded H5 | fixed by changing the metric, adding oases and food plumes to `dome.ts`, and redefining H5's sensor |
| `RISKS.md` R-01 | cosmetic pass/fail: `if (!lesioned)` branches, hard-coded coordinate steering, doorway-oscillation gaming | "mitigated and continuously gated" |
| `RISKS.md` R-02 | floating-point energy drift, silent Second-Law violation | mitigated by compensated sums |
| `RISKS.md` R-03 | 1-ULP differences in `tanh` / `exp` between runtimes | only integer and rational kernels are bit-exact; trajectories are compared within bounds |
| ADR-0001 context | four failure modes: lesion shortcuts, law drift, normal-approximation p-values on small N without multiplicity control or with tuning on evaluation seeds, strict types against hot loops | decisions 1-5 |
| `preregistrations/v4.0.md` | AE1 is "a noise-level borderline test" (the winner flips across three 48-seed pools); AINF, RPT-2, HOT-3 stay reported as failing; CQM12 marginal | disclosed |
| Errata A-03 | "exact" FP64 conservation is not exact; XPBD dissipates; tolerances must be relative to cumulative flow, not E0 | spec changed in v3.0. In the code the dome ledger still divides by E0 (`dome.ts:221`). |
| Errata A-04 | "every part shows a lesion effect" conflicts with "behaviour survives lesions" | closure cites a reconciliation in the v3.0 spec |
| Errata A-05, H-08 | the five mutants were never defined; the leak mutant sat seven orders above the tolerance | mutants now defined in code; leak mutant now -2e-11 per transfer against a 1e-11 tolerance |
| Errata A-06 | Holm status is provisional when items are locked one at a time; say what happens to seeds after a failed test | closure: burn-on-fail. Not followed on 2026-11-12 (W4). |
| Errata C-01..C-07 | Holm called "FDR"; no test named; 16 seeds give almost no power; effect size ill-defined for pairs; TOST misused; contrasts per item undefined; which items need arms | closure: exact Wilcoxon + Holm FWER, power-sized N, one primary contrast |
| Errata C-08 | no sham lesion | closure: mandatory size-matched sham arm. Implemented as a decoder scramble (W6). |
| Errata E-08 | the RNG key must include the run seed and the unit index; no draw may depend on how many came before | native Philox keying should follow this |
| Errata H-03, H-04 | most items state no quantity or threshold; several lesion tests are decided in advance ("removing X abolishes X"), so they are wiring checks, not capability tests | H3 and Passive-Body still have this form (W7) |

### 9b. Found by reading the code (my reading; not flagged by the owner, not run)

| ID | Weakness | Evidence | Native rule |
|---|---|---|---|
| W1 | Gate 3 asserts nothing | `viability_v39.ts` has no `process.exit` / `exitCode`; the pipeline calls it with no arguments | every test asserts and returns non-zero on failure; a test runner check fails any test with zero assertions |
| W2 | Gate 1 proves only that code compiles | `run_all.mjs` 82-85 | count it as a build check, not evidence |
| W3 | The prereg hash is not pinned | `scientific_rigor_suite.ts:1186` hashes the file and writes the hash; the Python audit compares against that same output (`.py:314`). An edited prereg passes both. The fixed value appears only in docs and old results. | the expected hash is a constant in the test binary and in a file outside the repo; a mismatch aborts |
| W4 | The held-out block was used for design iteration | `INCIDENTS.md` item 1: metric, world and sensor were changed after the first result on 5101..5124, the same day the prereg was locked. The prereg's own rule forbids tuning on seeds >= 5000. | burn-on-fail: after any change to metric, world or arms, draw a new block; log every use of a held-out seed |
| W5 | Lesion arms differ from the prereg | prereg H1: `aversiveLesion+topoScramble`; code adds `pagLesion` (line 1032). Prereg H2: `catchSoftening+topoScramble`; code has `catchSoftening` only (line 1099). | arm definitions are generated from the prereg file, not typed twice |
| W6 | The "sham" is another lesion, and specificity is never tested | it permutes the readout map; the test demands Intact > Sham. Recorded H1 means: sham 6.58 < lesion 9.58 (the sham hurts more than the targeted lesion). | test Lesion < Sham and Sham close to Intact (non-inferiority) |
| W7 | Two controls lose by construction | Passive-Body cannot move or forage; the chemoton cuts gave 0 buds on 24/24 seeds, so p = 2^-24 is automatic | label abolition controls as wiring checks; evidence must come from comparative controls |
| W8 | Shuffled-Stream scrambles every channel, is deterministic, and is not yoked | `c001.ts:158-169` | per-channel shuffle with its own RNG domain, plus a yoked arm |
| W9 | H5 uses a plug-in transfer-entropy estimate (history 1, 4 symbols, 479 samples, 64 cells) with no bias correction; it is the weakest result and sets the family maximum | `scientific_rigor_suite.ts:636-675, 1049-1079` | report against a surrogate distribution per seed; state the estimator and its bias |
| W10 | H4's score contains the selected trait, and selection weights are a function of intake; the Price identity is algebra | lines 918-920, 979 | primary metric must be measured on a transfer task not used for selection |
| W11 | Gates 4 and 5 (128 checks) are single-seed margins tuned on the seed they test | `qualities_v40.ts:123-135`, `passes_100_suite.ts:671-681` | seed blocks and the exact test; margins preregistered |
| W12 | Gate 6 still uses the normal approximation that ADR-0001 rejects; its firewall check is a regex on its own source | `apex_ultimate_suite.ts:56-108, 1077` | one statistics kernel everywhere; firewall enforced by the binary |
| W13 | The state hash covers a sample of the state | `organism.ts:1424` (every 97th weight) | hash every byte |
| W14 | "First Law" is bookkeeping closure in floats with heat and excreta counted as stores; four different tolerances | section 7 | integer quanta; one rule: residual == 0 |
| W15 | Gate 8 audits arithmetic on gate 7's output; it does not rerun any experiment and reads whatever JSON is on disk | `.py:293-305` | the audit tool recomputes from a raw event log and checks the log's run id and hash |
| W16 | No minimum effect size and no power statement for H1-H5 | prereg JSON has neither | state both in the prereg |

## 10. Native test list implied by the above

Build and static: `T-BUILD`, `T-STATIC`, `T-COV`, `T-ORIGIN`.
Determinism: `T-DET`, `T-GOLDEN`, `T-OBSERVER`, `T-SNAPSHOT`, `T-TIERS`, `T-PHILOX-KAT`, `T-ASM-PARITY`, `T-WIRE`.
Conservation: `T-LEDGER-DAY`, `T-LEDGER-PERTURB`, `T-LEDGER-HOOK`, `T-ENTROPY`, `T-CHEMOTON`, `T-CHEMOTON-RANK`, `T-PRICE`.
Anti-theater: `T-CNC`, `T-LESION-MOVES`, `T-MUT-1..5`, `T-MUT-CONF`, `T-FIREWALL`.
Viability: `T-VIAB-STEADY`, `T-VIAB-STARVE`, `T-VIAB-REFEED`, `T-ALIVE-SMOKE`.
Causal and held-out: `T-CAUSAL-<organ>`, `T-HELDOUT-H1..H5`, `T-EVO-SHADOW`, with arms `ARM_PASSIVE`, `ARM_SHUFFLE`,
`ARM_YOKED`, `ARM_LESION`, `ARM_SHAM_LESION`, `ARM_SHAM_COST`, `ARM_READOUT_SCRAMBLE`, `ARM_RANDOM_POLICY`, `ARM_NEUTRAL`.
Statistics and audit: `T-STATS` (kernel verified in section 8), `T-AUDIT`.

Unknowns: whether a lesioned organ in the lab keeps paying its metabolic cost; the content of stage 3 of
`run_grade_a_tests.mjs`; whether the lab's Philox matches the reference known-answer vectors (the lab only shows three of
its own implementations agree).
