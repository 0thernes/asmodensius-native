# 07 Suites and benchmarks protocol: the owner's goals, checklists and benchmarks to total completion

> Written 2026-11-18 for OpenAI GPT Astra 6.2 (or any strong coding agent working alone), read-only, from
> the files themselves. Every claim cites `path:line` as of 2026-11-18; files grow, so search for the quoted
> text if a line has moved. Read `00-PROMPT.md` and `04-GOALS-AND-BENCHMARKS.md` first. This file adds the
> design, the rules in full and the task list for milestone 8 (E1, E2, E3) and the suite parts of M2, A1
> and R1.
>
> Binding sources win over this file: `docs\handoff\CONTINUE-BUILD.md` section 4 item 8 (lines 55-60),
> section 3 (line 41) and section 7 (lines 85-89); `docs\ARCHITECTURE.md` sections 10 and 11 (lines
> 397-451); the owner's typed words in `docs\handoff\PROMPTS.md`. If you find a conflict, follow the binding
> source, record the conflict in `build\work\PROGRESS.md`, and go on.
>
> Root of every relative path: `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native`. In commands below,
> `$R` is that root, set once per PowerShell 7 session with
> `$R = 'Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native'`. Never map a drive letter.

## Table of contents

- [0. The answer in twelve lines](#0-the-answer-in-twelve-lines)
- [1. Ground truth on 2026-11-18](#1-ground-truth-on-2026-11-18)
- [2. Hard constraints and how this protocol honours them](#2-hard-constraints-and-how-this-protocol-honours-them)
- [3. The statistics (binding rules in full)](#3-the-statistics-binding-rules-in-full)
- [4. Tool design: az_suite and its companions](#4-tool-design-az_suite-and-its-companions)
- [5. Joining the lists correctly](#5-joining-the-lists-correctly)
- [6. Per-suite protocols](#6-per-suite-protocols)
- [7. Order of work](#7-order-of-work)
- [8. Run counts and machine time](#8-run-counts-and-machine-time)
- [9. Honesty rules](#9-honesty-rules)
- [10. The final scorecard and the definition of total completion](#10-the-final-scorecard-and-the-definition-of-total-completion)
- [11. Holes, blind spots and dilemmas, and how this protocol closes each](#11-holes-blind-spots-and-dilemmas-and-how-this-protocol-closes-each)
- [12. Task list SB-00 to SB-30](#12-task-list-sb-00-to-sb-30)
- [13. Decisions that belong to the owner](#13-decisions-that-belong-to-the-owner)
- [Appendix A. Item spec grammar](#appendix-a-item-spec-grammar)
- [Appendix B. A worked preregistration](#appendix-b-a-worked-preregistration)
- [Appendix C. The 100 LIFE areas with candidate native owners](#appendix-c-the-100-life-areas-with-candidate-native-owners)
- [Appendix D. File map: every file this protocol creates or changes](#appendix-d-file-map-every-file-this-protocol-creates-or-changes)

## 0. The answer in twelve lines

1. Not one owner suite has been run against the native engine. Every PASS figure in `docs\requirements\` is
   from the old TypeScript lab (`04-GOALS-AND-BENCHMARKS.md:16-17`).
2. Milestone 8 runs on the complete build only: no stubs (sense, physics and stats are still stubs), A1
   clean, the engine frozen (`CONTINUE-BUILD.md:55`). The tools can be built and tuned earlier.
3. One rule for everything statistical: exact paired Wilcoxon, d_z with a bootstrap interval, Holm per
   suite; EFFECT = |d_z| >= 0.8 and Holm p < 0.05 against every control arm the item needs; non-ceiling;
   INCONCLUSIVE when under-powered (`CONTINUE-BUILD.md:58`). Section 3 spells out every edge case.
4. Seeds: tune on 1-99 only; report on 5001-5024 and 5101-5124 (48 seeds) after one preregistration per
   suite is locked in `docs\prereg\` with its SHA-256 pinned; one look per item and block, then the block is
   spent (`CONTINUE-BUILD.md:41,55`).
5. Build `tools\az_suite.c` (az_lesion generalised: item specs as data, arms, probes, measures, verdicts,
   scorecards), `tools\az_trace.py` (the E2 trace matrix), `tools\az_gate.py` (the gate runner) and
   `tools\az_audit.py` (T-AUDIT, a second implementation in Python).
6. The ABI needs one lead-only batch: names for the 64 `lesion_ext` bits (`core\az_abi.h:875`), the
   control-arm switches and a conserving probe API, so tasks can be set up inside the world.
7. Join the lists by item code and source id, never by row number. Only 46 of 100 same-number ledger/suite
   pairs share a code (`docs\requirements\passes-bench.md:10`).
8. Keep the Grok Bot (public) and the Windows v40 export as separate provenance. Their AE1 Combo A
   values differ (d = 0.733 against d = 0.857 over the same 112 seed ids; the cause is unresolved).
9. Butlin 14, the 10 theories and CQM axes 13 and 19-22 are indicator-only. No aggregate life,
   consciousness, sentience or AGI score is ever computed (`cqm25_rules.json` RULE-4 and RULE-6).
10. Many indicator items have no native mechanism yet: a search of `core\` on 2026-11-18 for workspace,
    broadcast, confidence, efference, forward model, opt-out and metacog found nothing. They get NOT-BUILT,
    never a forced pass.
11. Planning machine time for all reporting runs: about 21 to 53 hours on 8 workers, plus tuning and E3
    (section 8). Measure the constant first (task SB-14).
12. Total completion means every id of every list has an evidenced status from a closed set, every
    statistical verdict is reproduced by T-AUDIT and by an independent reviewer, and nothing is summed into
    a score (section 10). It does not mean everything passes.

## 1. Ground truth on 2026-11-18

Verify each line yourself in task SB-00 before relying on it. If a number has changed, use the new one and
record the cause.

| Fact | Value | Source |
|---|---|---|
| Native suite results | none; `docs\prereg\`, `docs\eval\` and `tools\az_suite.c` do not exist | `04-GOALS-AND-BENCHMARKS.md:18-22`; checked again 2026-11-18 with `Test-Path -LiteralPath` (False, False, False) |
| Evaluation tool that exists | `tools\az_lesion.c`: intact against one lesion, 21 switches, verdicts EFFECT, NO EFFECT, INERT only | `tools\az_lesion.c:25-35,78-100` |
| What az_lesion lacks | sham, yoked, shuffled and random-policy arms; non-ceiling; INCONCLUSIVE; a d_z interval (its bootstrap interval is of the mean difference); more than 64 seeds | `tools\az_lesion.c:104,450`; `tools\az_statsutil.h:30,327-362` |
| Its default seeds | the reporting block 5101-5124: running it without `--seeds` spends that block | `tools\az_lesion.c:205,303` |
| Statistics kit | exact two-sided Wilcoxon signed-rank (128-bit counts, up to 64 pairs), Holm, d_z, Welch t, seeded bootstrap of the mean difference, quartiles | `tools\az_statsutil.h:7-15,156-212,234-252,261-279,334-362` |
| Lesion switches | 21 named bits; bits 21..31 and `lesion_ext[2]` (64 more) have no names; a config that sets one is refused | `core\az_abi.h:385-407,874-875`; `docs\ARCHITECTURE.md:409-411` |
| Latest lesion table | tuning seeds 1-24, 3000 founders, 64 ticks, grid 4,4,3, 8 species, 8 workers: 7 EFFECT, 14 NO EFFECT, 0 INERT, 88.2 s for 528 runs | `04-GOALS-AND-BENCHMARKS.md:116-144`; `build\work\logs\lesion_z_worldgen_1_24.txt` |
| Reporting block 5101-5124 | used once, at the stub-world baseline, as the rules allowed ("once, with nothing changed because of it") | `build\work\PROGRESS.md:25`; `CONTINUE-BUILD.md:41` |
| Stubs | sense, physics, stats; the sense stub leaves directional receptors neutral, the physics stub ignores gravity, rock and density, the stats stub only counts | `00-PROMPT.md:41-46`; `docs\agents\W2-BRIEFS\sense.md`, `physics.md`, `stats.md` |
| Strict lint | fails with 3 findings: W1 nine traits not read in their designated module (econ BUILD, TRUST, VAL_2, VAL_3, others owned by sense and physics); W2 GRAVITY not honoured by physics; W2 NOISE, RECURRENCE, SENSES untested | `build\work\PROGRESS.md:1502,1610`; `Asmodensius-comparative-review.md:149` (path in section 5.8) |
| Tier 1 learning signal | reward = energy gained since the last visit; surprise = reward - rbase (a reward prediction error) | `core\az_tier1.c:10-14,205-208` |
| Entropy hooks | `AzHost.entropy` feeds a wild world; `AzHost.entropy_log` records 16 bytes per tick, and "nothing ever reads it back into a run" | `core\az_abi.h:922-931` |
| Seed rule in the architecture | tune on 1..99; report on 5001..5024 and 5101..5124; unit tests use tuning seeds only | `docs\ARCHITECTURE.md:423-428` |
| Seeds used by unit tests outside 1-99 | test_mate 201-208, test_metab 211-214 (not reporting seeds, but outside the tuning range) | `Asmodensius-comparative-review.md:152` |
| The three statistics rules | v40 R21 (d >= 0.8 and Welch t >= 2.75, seeds 1064-1111); framework v3.0 (Wilcoxon, Holm, delta_pair >= 0.33, LCB95 > -Delta_NI); native M8 (binding) | `04-GOALS-AND-BENCHMARKS.md:50-71` |
| The 825 scoreboard | 14 + 10 + 25 + 100 + 500 + 100 + 12 + 40 + 24, 5 mirrors, 820 unique; only Butlin, theories, CQM and the 500 were extracted | `docs\requirements\checklists_index.json:17-19,170-181` |
| Crosswalk workbook | 7 sheets: Guide (36 rows), Requested items (958), Supplementary items (4,854), Framework 825 (825), Framework mirrors (5), List index (42), Catalog sources (30); every Requested row says "Unrun at M8" | `C:\Users\Alexa\Documents\Codex\2026-11-18\the-bundle-is-clean-no-key\outputs\Asmodensius-evidence-crosswalk.xlsx` (read 2026-11-18) |
| Master sources on Z: (read-only) | `Z:\[Vibe Coded (AI)]\CLAUDECODE\ALife-Framework-v3.0-Master-Checklists.csv`, `...\ALife-Framework-v3.0-Master-Spec.json`, `...\Creature1-Benchmarks.pdf`, `<internal-ts-prototype>\preregistrations\OFFICIAL-R21.md` exist | `Test-Path -LiteralPath`, 2026-11-18 (all True) |

## 2. Hard constraints and how this protocol honours them

| Law or owner word | What it forbids or demands in the suites | How the protocol honours it |
|---|---|---|
| Native only (law 1) | no VM, JIT or interpreter in the simulation | az_suite is hosted C that only creates, ticks and reads worlds, like az_lesion. Python (`az_trace.py`, `az_audit.py`, `az_gate.py`) is development tooling and never enters a world |
| Cache-first, locked arena (law 2) | no pageable arena in any reported run | az_suite refuses `--no-lock` and `--allow-pageable` together with `--prereg`; every run prints "arena locked"; a lock failure is a failed run, never re-run pageable (`CONTINUE-BUILD.md:30`) |
| No LLM (law 3) | no model, dataset or API in the engine or in a measure | measures are integer readouts of the world; no classifier is trained outside the creatures; no LLM judges a result |
| No theatre (law 4) | no mechanism without a causal lesion; no hard-coded steering; no fitness function | every TESTED item names its mechanism and its lesion; probes set up situations through conserving interventions applied identically in every arm, and never reward or steer a creature; NOT-BUILT is used when no mechanism exists |
| Exact books (law 5) | equality every tick, never a tolerance | every run audits matter and energy at the start and end (as `tools\az_lesion.c:279-296`) and az_suite adds a per-tick audit option used in all reporting runs; any FP64 tolerance in a source item is re-specified as exact equality (section 6.1) |
| Wild by default, lab for tests (law 6; OWNER-WORDS.md:45-46) | the living world stays wild; the seeded lab mode is the test bench | reporting runs use `AZ_MODE_LAB` (a paired contrast needs the same world twice); a descriptive wild sensitivity block is run per suite (section 3.12); no default of az_run is changed |
| Nothing fixed (law 7) | every number a gene or a world parameter | arm switches are data masks; the sham size is computed from the target mask at run time; probe sizes are world parameters or item-spec values fixed in the prereg, never constants in core code |
| Indicator only (law 8) | never claim consciousness, sentience, life, AGI, quantum effects | honesty classes and banners (section 9); no aggregate score; CQM RULE-4 and RULE-6 |
| Honest evidence (law 9) | command plus output tail; unknown is fine; a green suite is not done until a reproduce-first review agrees | every verdict has a run id, the command and raw files; T-AUDIT recomputes; SB-29 is the independent review |
| Z: only, no installs, no commits (laws 10-12) | | every path is under the root; the only external inputs are copied to `build\work\eval\inputs\`; GATE-01 stays blocked until the owner allows a commit (`CONTINUE-BUILD.md:86`) |
| Every word counts (OWNER-WORDS.md) | the numbers 10, 14, 25, 100, 100, 100, 500 and "more things I forgot" (P06, `PROMPTS.md:301`) | every list in `docs\requirements\` and the 5 unextracted framework suites get a TRACE row per item; nothing is dropped silently |
| ASCII, LF, no tabs | | every file this protocol writes passes `build\work\scripts\check_text.py` |

## 3. The statistics (binding rules in full)

### 3.1 Experimental unit and pairing

- The unit is one world run. Creatures inside one world are not independent (they share a lineage and a
  world), so a world gives one number per measure (CQM25-PROT-05 in `cqm25_rules.json`).
- A pair is two runs with the same seed and the same configuration that differ only in the arm switches.
  Lab mode (`cfg.mode = AZ_MODE_LAB`) makes the world a function of the seed (`core\az_abi.h:853-855`), so
  the pair differs only by the arm.
- Thread count, pinning, reorder period and kernels must not change the state hash (the determinism
  invariant, `CONTINUE-BUILD.md:43`). az_suite may therefore run arms on different worker counts or in
  parallel jobs; it records them anyway.

### 3.2 Test, effect size, interval

For item i and required control arm a, over the n usable seed pairs s:

- difference `x_s = sign_i * (m_intact(s) - m_arm(s))`, where `sign_i` is +1 if the preregistered direction
  says intact exceeds the arm and -1 if it says intact is below the arm;
- test: exact two-sided Wilcoxon signed-rank on the pairs, zero differences dropped, ties by midranks
  (`azs_wsr_exact`, `tools\az_statsutil.h:156`); with integer measures pass `tie_tol = 0`;
- effect size: `d_z = mean(x) / sd(x)` (`azs_cohen_d_paired`, `tools\az_statsutil.h:261`); with the sign
  convention above a positive d_z is in the preregistered direction;
- interval: percentile bootstrap of d_z over the n pairs, 10,000 resamples, 95 percent, seeded by the first
  8 bytes of SHA-256 of the text `<item id>|<arm id>` (new function `azs_boot_ci_dz`, task SB-05). A
  resample whose differences are all equal gives +-inf or 0 exactly as `azs_cohen_d_paired` defines.

### 3.3 Control arms

Every item names the arms it needs. An arm is a data mask on the same code path, never another algorithm
(`docs\ARCHITECTURE.md:399-401`). Arm ids and their native meaning:

| Arm id | Meaning | Native implementation | Exists now? |
|---|---|---|---|
| INTACT | the full engine | `cfg.lesion = 0`, `lesion_ext = {0,0}` | yes |
| TARGET:<switch> | the item's mechanism off | one named `AZ_LESION_*` bit or a new `lesion_ext` bit | 21 bits yes; per-item bits need SB-04 |
| PASSIVE | the body without a brain (framework "Passive-Body (0-Brain)") | `AZ_LESION_BRAIN` (motor outputs forced to zero) | yes |
| SHAM_SIZE | a size-matched sham: the same number of units, weights or channels masked as the target, drawn from a pool the item does not implicate | new bit `AZ_LX_SHAM` plus world parameter `SUITE_SHAM_OF` (the lesion whose size it matches); the mask is drawn by Philox keyed on the world key and the uid, and its size is computed from the target mask at run time | no (SB-04) |
| SHAM_COST | the target is off, but its energy cost is still charged (booked as heat, so the books stay exact): a difference cannot then come from saved energy | new bit `AZ_LX_SHAM_COST`, honoured by each module that owns a costed mechanism | no (SB-04) |
| SHUFFLE_SENSE | the exteroceptive input keeps its marginal statistics but loses its contingency: each creature gets the previous-tick sensory vector of a Philox-chosen creature in the same block | new bit `AZ_LX_SHUFFLE_SENSE` (previous tick, so the result is schedule-independent) | no (SB-04) |
| SHUFFLE_SIGNAL | emitted signal values permuted among emitters (communication content scrambled, signalling rate kept) | new bit `AZ_LX_SHUFFLE_SIGNAL` | no (SB-04) |
| SHUFFLE_MEMORY | associative recall keys permuted (recall still runs and costs the same) | new bit `AZ_LX_SHUFFLE_MEMORY` (tk_hop path in `core\az_tier1.c`) | no (SB-04) |
| YOKED | the learning signal is decoupled from the creature's own actions: Tier 1 uses the reward of a Philox-paired creature from that creature's previous visit | new bit `AZ_LX_YOKED` | no (SB-04) |
| RANDOM_POLICY | actions decoupled from perception with matched marginals: each motor channel is drawn by Philox with the population mean and spread of that channel from the previous tick | new bit `AZ_LX_RANDOM_POLICY`; the implementer documents the exact draw and shows the per-channel mean matches within one quantum | no (SB-04) |
| NEUTRAL | the neutral shadow (Bedau's evolutionary-activity shadow; `T-EVO-SHADOW` in `docs\research\v40\gates.md:254`): births are paid by the real parent, but the genome passed on is that of a Philox-chosen live creature of the same species, so inheritance no longer follows success | new bit `AZ_LX_NEUTRAL_PARENT` (life and mate) | no (SB-04) |
| FROZEN_LEARN | plasticity off | `AZ_LESION_PLASTICITY` | yes |
| FROZEN_HEREDITY | no new variation | `AZ_LESION_MUTATION` | yes |
| HIDDEN_OFF | brain hidden units masked, direct sensor-to-motor paths only (for conjunction tests) | new bit `AZ_LX_HIDDEN` (`core\az_brain.c`) | no (SB-04) |
| RBASE_FROZEN | Tier 1's reward baseline frozen at its birth value: no reward prediction | new bit `AZ_LX_RBASE` (`core\az_tier1.c:205-208`) | no (SB-04) |
| SHAPE_FIXED | body growth and resorption off (motors GROW and SHED masked) | new bit `AZ_LX_NO_GROW` | no (SB-04) |
| NO_BUILD | creature writes to cells (marks, deposits, BUILD) are discarded at the post stage, matter returned to the creature, books exact | new bit `AZ_LX_NO_BUILD` | no (SB-04) |
| SURROGATE:<name> | an analysis-level control computed from the intact run itself, no extra world (for example parent identities permuted for heredity, or a degree-preserving rewiring of the brain graph for modularity) | az_suite measure code; the permutation is seeded like the bootstrap | no (SB-07) |

Rules for arms:
- Every arm in an item's list is required (intersection-union, section 3.4).
- The size-matched sham is required for every TARGET arm that masks a subset of brain units or weights
  (RECURRENCE, HIDDEN_OFF and any per-pathway mask), so that "any damage of this size" is excluded. A switch
  that turns off a whole module or a whole channel class (SENSES, MEMORY, TIER1_SH) has no meaningful
  size-matched sham; its content control is a SHUFFLE arm instead.
- SHAM_COST is required for every TARGET whose mechanism has an energy cost (the module's brief or code
  says it charges energy).
- SHUFFLE_* and RANDOM_POLICY are required where the claim is about content or contingency, not about the
  mere presence of activity.
- NEUTRAL is required for every evolutionary claim (CQM 01, 02, 04; LIFE areas 79 and 81).
- REPLAY of a recorded environment is not offered: overwriting cells from a tape would break the exact
  books. Environmental-coupling claims use SENSES (world to creature) and NO_BUILD (creature to world)
  instead. Record this choice in each prereg that needs it.

### 3.4 Intersection-union across arms, Holm across items

- An item is supported only if it beats every required arm. This is an intersection-union test (Berger
  1982; the v40 lab also used "5-Arm Berger IUT + Holm", `docs\requirements\bench_budgets.json` BUD-2.2).
  The item's p-value is therefore `p_i = max over required arms of p_(i,a)`. No correction across arms is
  needed or applied.
- The Holm family is one suite: the TESTED items of that suite, fixed in the preregistration before any
  reporting run. `m` = their number. Holm adjusts the `p_i` (`azs_holm`, `tools\az_statsutil.h:234`).
- An item added after the lock goes into a new family (an addendum preregistration with its own SHA-256 and
  its own m), never into the locked one.
- Planned families: E1 lesions (m = number of named switches, 21 now), Butlin (m <= 14), theories (m <= 9,
  the rollup is computed, not tested), CQM-25 (m <= 25 minus the gate axes 23 and 24 and the measurement
  axis 25), passes (m = tested passes), LIFE-500 (m = tested sub-checks, at most 500).

### 3.5 Verdicts (closed set; az_suite computes them, T-AUDIT recomputes them)

Per item and arm, first:

| Arm result | Condition |
|---|---|
| arm-EFFECT | `d_z(i,a) >= 0.8` (preregistered direction) |
| arm-NONE | the bootstrap interval's upper bound of `d_z(i,a)` is below 0.8: the data rule out an effect of the required size against this control |
| arm-REVERSED | `d_z(i,a) <= -0.8` and `p_(i,a) < 0.05 / m` (Bonferroni, deliberately strict) |
| arm-OPEN | none of the above |

Then per item, in this order (the first matching row wins):

| Verdict | Condition |
|---|---|
| FAIL-INVARIANT | any run of the item broke matter or energy equality, or had `field_underflow` or `dropped_events` above 0. This is an engine defect: stop the suite, fix, re-freeze (the item's block is spent, section 3.10) |
| FAIL-CEILING | the measure is bounded, and the intact mean is above 0.95 of its preregistered maximum (section 3.6) |
| INCONCLUSIVE-N | fewer than `n_min` usable pairs, or the smallest attainable p (2 / 2^n_nz) times m is not below 0.05 |
| EFFECT | every required arm is arm-EFFECT and the Holm-adjusted `p_i < 0.05` |
| REVERSED | some arm is arm-REVERSED |
| NO EFFECT | some arm is arm-NONE |
| INCONCLUSIVE | everything else (the interval still reaches 0.8 against some control, or Holm p >= 0.05 with large d_z) |
| INERT (E1 only) | the lesioned final state hash equals the intact hash on every seed; on the complete build this is a defect (`CONTINUE-BUILD.md:56`) |

Engineering checks that are not statistical (gates, mutants, reproducibility) use PASS or FAIL, and the
mutants use CAUGHT or MISSED. Never mix these with the statistical verdicts in one count.

### 3.6 Non-ceiling rule

- Every measure declares in the prereg whether it is bounded and, if so, its minimum and maximum (for
  example a fraction of focal creatures, maximum 1; an accuracy over k trials, maximum k).
- If the intact mean over the usable pairs is above 0.95 times the maximum, the item is FAIL-CEILING
  (`CONTINUE-BUILD.md:58`; the v40 rule in `butlin14.json` pass_rule). A redesigned, harder probe is a
  new item version with a new prereg; its block for the old version is spent.
- Unbounded measures (energy, counts) are exempt; the prereg says so.

### 3.7 Power and INCONCLUSIVE

- `n_min` (usable pairs out of 48) is fixed in each prereg before reporting. Proposed default 40; the agent
  may choose more, never fewer after a look. A pair is usable when both runs completed with exact books and
  the measure is defined (for example at least one focal creature met the probe).
- Planning power for a true |d_z| of exactly 0.8, worst Holm step (alpha = 0.05 / m), normal approximation
  with the Wilcoxon asymptotic relative efficiency 3/pi (computed for this file; az_suite reports the
  observed numbers, these are only for planning):

| pairs n | m = 1 | m = 14 | m = 21 | m = 25 | m = 100 | m = 500 |
|---|---|---|---|---|---|---|
| 24 | 0.97 | 0.82 | 0.79 | 0.77 | 0.64 | 0.48 |
| 48 | 1.00 | 0.99 | 0.99 | 0.99 | 0.97 | 0.94 |

- Consequence: every suite reports on the pooled 48 seeds (5001-5024 and 5101-5124). E1 alone keeps
  5101-5124 as the primary table and 5001-5024 as a separate replication table, as written in
  `CONTINUE-BUILD.md:56`. Its 24-seed table has power about 0.79 at m = 21; say so in the E1 prereg.
- The smallest exact two-sided p is 2 / 2^24 = 1.19e-7 for 24 pairs and 2 / 2^48 = 7.1e-15 for 48 pairs,
  so Holm at m = 500 is reachable (`tools\az_lesion.c:35` states the 24-pair value).
- `AZS_WSR_MAX` is 64 (`tools\az_statsutil.h:30`); 48 pairs fit.

### 3.8 Direction

Each item states its direction in the prereg (intact greater than arm, or intact less than arm). The test
stays two-sided; the direction decides the sign of d_z and the REVERSED verdict. Never change a direction
after seeing a reporting result. The E1 table of today prints signed differences (lesioned minus intact,
`tools\az_lesion.c:441`); in the E1 prereg state each switch's expected direction, so BRAIN (+3.20 on
energy in the tuning table) is not read as "the brain hurts" without a stated hypothesis
(`Asmodensius-comparative-review.md:142`).

### 3.9 Invariant guard on every run

Every reporting run checks, at every tick: total matter equal, total energy equal, `field_underflow == 0`,
`dropped_events == 0` (`CONTINUE-BUILD.md:43`). az_lesion checks start against end only
(`tools\az_lesion.c:279-296`); az_suite adds `--audit-every 1` and uses it in every reporting run. One
failure makes the item FAIL-INVARIANT (section 3.5).

### 3.10 Seeds: custody, one look, burned blocks

- Tuning: seeds 1-99 only, any number of looks, results labelled exploratory (`tools\az_lesion.c:368`
  prints the same label).
- Reporting: 5001-5024 and 5101-5124, only through `az_suite --prereg <file>` (and `az_lesion --prereg
  <file>` after SB-08). The tool checks, before the first tick:
  1. the prereg file's SHA-256 equals its line in `docs\prereg\PINS.txt`;
  2. the engine manifest SHA-256 written in the prereg equals the manifest of the tree being run (SB-09);
  3. `docs\prereg\RUNS.csv` has no earlier reporting run for this (item, block); otherwise it refuses with
     exit code 5.
- After the run the tool appends one line per (item, block) to `docs\prereg\RUNS.csv`: prereg SHA-256,
  item, block, run id, UTC time, verdict.
- A block is spent ("burned") for an item when anything about the item changes after a reporting run on
  it: the probe, the measure, the arms, the direction, n_min, the config, or the engine manifest. Record
  each burn in `docs\prereg\BURNED.csv` (item, block, date, reason, old prereg SHA-256). A burned item is
  reported as BURNED with its last valid verdict, and it is not re-run on that block.
- New reporting blocks are the owner's decision (section 13, D-SB-2). Until he names them, a burned item
  stays BURNED. Propose 5201-5224 and 5301-5324 in the interim report; do not use them before a yes.
- Disclose in the E1 prereg that 5101-5124 was used once at the stub-world baseline
  (`build\work\PROGRESS.md:25`) and that nothing was tuned on it.
- A finding for A1: unit tests that use seeds 201-208 and 211-214 (`Asmodensius-comparative-review.md:152`)
  break the "unit tests use tuning seeds only" rule (`docs\ARCHITECTURE.md:424`). Move them into 1-99 where
  the pinned values allow, or list them in PROGRESS.md as the rule asks.

### 3.11 Preregistration: content, format, lock

- One file per suite: `docs\prereg\PREREG-<SUITE>.txt` (grammar in Appendix A). Suites: `E1`, `CQM25`,
  `BUTLIN14`, `THEORIES10`, `PASSES100`, `LIFE500`, plus addenda `PREREG-<SUITE>-ADD<n>.txt`.
- It contains, per item: ids in every catalog it answers, honesty class, mechanism (file and function),
  probe and its arguments, world config (founders, ticks, grid, species, cap, store units, periods),
  primary measure with bounds, direction, required arms, minimum effect (|d_z| >= 0.8), n_min, seed block,
  the Holm family and m; per suite: engine manifest SHA-256, az_suite version, deviations from the source
  rule (for example "48 seeds, not 64, for the 500", the default in `CONTINUE-BUILD.md:87`), and the
  disclosure lines of section 3.10.
- Lock procedure (SB-08):
  1. finish tuning on seeds 1-99; copy the tuned spec into `docs\prereg\PREREG-<SUITE>.txt`;
  2. run `az_suite --prereg ... --dry-run` (parses, resolves every arm and measure, runs nothing);
  3. pin: `(Get-FileHash -LiteralPath "$R\docs\prereg\PREREG-CQM25.txt" -Algorithm SHA256).Hash` and
     append `<hash>  PREREG-CQM25.txt  <UTC time>` to `docs\prereg\PINS.txt` (edit with your editor tool;
     no `>` redirection);
  4. write the same line into `build\work\PROGRESS.md` and into the TRACE rows of the suite;
  5. only then run the reporting seeds.
- A locked prereg is never edited. A correction is an addendum with its own SHA-256, and it burns what it
  changes.

### 3.12 Lab and wild

- Reporting runs are lab runs. That is what lab mode is for (`OWNER-WORDS.md:45`: "Lab mode (seeded) is only
  the test bench").
- Each suite also gets a wild sensitivity block, which never enters a Holm family: 24 wild runs per arm with
  fresh entropy, unpaired, reported as means, standard deviations and Cohen d from `azs_welch_t`, labelled
  "wild, descriptive, not a test". It shows whether a lab effect survives in the creatures' own world.
- A paired wild design (one recorded entropy tape fed to both arms of a pair through a tool-side
  `AzHost.entropy`) is possible in code, but it reads entropy back into a run, which the ABI comment rules
  out for the journal (`core\az_abi.h:927-931`). It needs the owner's yes (D-SB-1). Default: not used.

### 3.13 Never allowed

- tuning on a reporting seed; a second look at an (item, block); picking seeds; dropping a seed for any
  reason not written in the prereg (only unusable pairs as defined there);
- changing a primary measure, direction, arm list or n_min after a reporting run;
- counting an R variant, a re-worked probe or a secondary measure as the item's result;
- a pageable arena in any reported run;
- reporting a v40, R21, SC-PORT or framework PASS as native evidence;
- summing verdicts across suites, or turning counts into a percentage, a grade or a "score".

## 4. Tool design: az_suite and its companions

### 4.1 Overview

| Component | File(s) | Language | Owner rule |
|---|---|---|---|
| Statistics additions | `tools\az_statsutil.h` (extend), `tests\test_statsutil.c` (extend) | hosted C | one owner; the test file already checks the lesion table (`tools\az_lesion.c:37-40`) |
| SHA-256 | `tools\az_sha256.h` (new, header-only, FIPS 180-4) | hosted C | used by az_suite for pins, bootstrap seeds and run ids |
| Probe API (in the core) | `core\az_probe.c` (new), declarations in `core\az_abi.h`, event kind `AZ_EV_PROBE`, `tests\test_probe.c` (new) | freestanding C23, integers only | lead only (ABI) |
| New arm switches | `core\az_abi.h` (`lesion_ext` names), `core\az_owners.def` (owners), the owning modules, their tests | C23 | lead names them; module owners honour them |
| Suite runner | `tools\az_suite.c` (new) | hosted C | built like az_lesion; registered in `CMakeLists.txt` by the lead |
| Tool-side probes and measures | `tools\az_probe_tool.h`, `tools\az_probe_tool.c` (new) | hosted C | reads `const AzWorld*`; calls the core probe API between ticks |
| Suite tests | `tests\test_suite.c` (new) | hosted C | registered by the lead |
| Trace matrix | `tools\az_trace.py` (new), `docs\eval\dispositions.csv` (new), outputs `docs\eval\TRACE.csv`, `docs\eval\TRACE.md` | Python 3.9, `python -I` | development tooling |
| Second implementation (T-AUDIT) | `tools\az_audit.py` (new) | Python, exact `fractions` | development tooling |
| Gate runner | `tools\az_gate.py` (new) | Python | development tooling |
| Engine manifest | `tools\az_manifest.py` (new) | Python | development tooling |

### 4.2 Additions to az_statsutil.h (SB-05)

- `azs_boot_ci_dz(a, b, n, sign, resamples, seed, conf, out)`: percentile interval of d_z of
  `sign * (a - b)`, deterministic as `azs_boot_ci_paired`.
- `azs_iut_p(const double *p, int k)`: the maximum of k p-values (refuses NAN).
- `azs_min_p_two(int n_nz)`: `2 / 2^n_nz` capped at 1.
- `azs_verdict(...)`: the decision table of section 3.5 as one pure function over (d_z per arm, interval per
  arm, p per arm, Holm p, n, n_min, m, bounded flag, max, intact mean, invariant flag), returning an enum.
  az_suite and test_suite call this one function; T-AUDIT reimplements it in Python.
- Tests in `tests\test_statsutil.c`: hand-computed Wilcoxon tables for n = 1..8; Holm on the textbook
  example (p = 0.01, 0.04, 0.03, 0.005 gives 0.03, 0.06, 0.06, 0.02); d_z bootstrap bit-identical on two
  calls; every row of the verdict table reached by a constructed input and every row able to fail (change
  one input by one step and the verdict changes).

### 4.3 The probe API in the core (SB-04, lead only)

A probe sets up a task inside the world between ticks. It must conserve, be logged and be identical in
every arm.

- `bool az_probe_move(AzWorld *w, u32 from_cell, u32 to_cell, u32 kind, u64 amount)`: move matter of one
  field kind from one cell to another; refuse (return false, count a refusal) if `from_cell` lacks it.
- `bool az_probe_heat(AzWorld *w, u32 from_cell, u32 to_cell, i64 amount)`: move heat; same rules.
- `bool az_probe_rotate(AzWorld *w, u64 uid, AzRotor r)`: turn one body (no energy change).
- `bool az_probe_place(AzWorld *w, u64 uid, u32 to_cell)`: move one body to a free, non-bedrock cell.
- `bool az_probe_mark(AzWorld *w, u32 cell, u32 mark, u32 value)`: set a mark (marks are not matter).
- `bool az_probe_drain(AzWorld *w, u64 uid, i64 amount)`: move energy from one creature to its cell as
  heat (for need-switching probes); refuse if the creature has less.
- `bool az_probe_prune(AzWorld *w, u64 uid, u32 permille)`: remove that share of one creature's brain
  weights, chosen by Philox on the world key and uid (for repair probes; no energy change).
- `bool az_probe_nudge(AzWorld *w, u64 uid, u32 unit, i32 delta)`: change one hidden unit of one brain by
  `delta` quanta (for perturbation probes such as P-PERTURB-FORK; no energy change).
- Each call appends `AZ_EV_PROBE` to the ledger with its arguments, so the run record and the ledger chain
  show every intervention. Serial only, never inside a tick.
- `tests\test_probe.c`: books exact after every probe; refusals counted; the state hash with probes is the
  same for 1, 8 and 24 workers; a probe sequence replayed from the ledger gives the same hash.

### 4.4 Names for the extra switches (SB-04)

Bits 21..31 of `lesion` and the 64 bits of `lesion_ext` are free (`docs\ARCHITECTURE.md:409-411`). Put the
arm switches of section 3.3 in `lesion_ext[0]`, keep `lesion` bits 21..31 for future mechanism lesions
(for example the sense and physics owners' own switches), and add any per-item mechanism lesions found in
SB-03 to `lesion_ext[1]`. Each new switch needs: an owner line in `core\az_owners.def`, a row in the
az_lesion table (so `tests\test_statsutil.c` keeps passing; `tools\az_lesion.c:37-40`), an honouring module,
and a lesion test in that module's test file. Run strict lint afterwards and record its counts.

Names fixed now (fixer pass, 2026-11-18; `00-MASTER-PROMPT.md` 5.10 row 20): the mechanism switches the
remaining modules and the chain add are `AZ_LESION_TAXIS` (sense, WP-P1-02), `AZ_LESION_THRUST` (physics,
WP-P2-02) and `AZ_LESION_EAT` (metab: uptake demand times 0, created in 06 WP-P5-01), in the free bits
21..31 of `lesion`. Every table of this file uses these names; "NO_EAT" in older drafts means
`AZ_LESION_EAT`.

### 4.5 tools\az_suite.c

Command line (every option checked, as `tools\az_tools.h` and az_lesion do):

```
az_suite --spec FILE | --prereg FILE      item specs (tuning) or a locked prereg (reporting)
         [--seeds A-B[,C-D]]              tuning only; a prereg carries its own seeds
         --out DIR                        run folder under build\work\eval\ (created; must not exist)
         [--only ID,ID] [--jobs J] [--threads K] [--ecores] [--no-pin] [--kernels 0|1]
         [--audit-every N]                default 1 with --prereg
         [--dry-run] [--list] [--quiet] [--wild-block]
```

- `--prereg` refuses reporting seeds unless the pin, the manifest and RUNS.csv checks pass (section 3.10).
- `--spec` refuses any seed outside 1-99.
- `--jobs J` runs J worlds at once, each with its own locked arena and `--threads K` workers. The hash does
  not depend on it; record J and K. Each 3,000-founder world locked 80.8 MB in the lesion run
  (`04-GOALS-AND-BENCHMARKS.md:118`); check that J arenas lock (the working-set quota is a risk).
- Run loop per seed: build the INTACT world and every arm world from the same config; apply the probe
  schedule at the listed ticks through the probe API, identically in every arm; tick; read measures at the
  listed ticks; audit per `--audit-every`.
- Outputs in `--out`:
  - `runs.csv`: seed, item, arm, every measure value (integers as integers, ratios as num/den), state hash,
    conserved flag, field_underflow, dropped_events, refusals, ticks, wall seconds;
  - `stats.csv`: per (item, arm): n, n_nz, d_z, interval, exact p; per item: IUT p, Holm p, verdict;
  - `stdout.txt` (the tool writes it itself; no shell redirection), `env.txt` (command line, engine manifest
    SHA-256, prereg SHA-256, host workers, arena size and lock state, UTC start and end, CPU use of other
    processes sampled every 10 s so the load can be labelled per `CONTINUE-BUILD.md:40`);
  - `SCORECARD-<SUITE>.md` and `.csv` (section 10.1), which the lead copies into `docs\eval\` after T-AUDIT.
- Exit codes: 0 done; 1 an invariant failed in some run; 2 bad arguments or setup; 3 arena not locked; 4 pin
  or manifest mismatch; 5 seed custody refused; 6 spec error.
- Run id: `RUN-<SUITE>-<UTC yyyymmddThhmm>-<first 8 hex of the prereg or spec SHA-256>`, which is also the
  `--out` folder name under `build\work\eval\`.

### 4.6 Measures

- Every measure is computed from integer world state read between ticks: columns, `totals`, `stat[]`
  counters bumped with `az_stat_bump` (`docs\agents\W2-BRIEFS\stats.md:11`), the ledger, positions and
  cells. Sums and counts stay integers; a ratio is kept as an integer numerator and denominator until the
  statistics step.
- Focal creatures: when a probe needs individuals (a creature that meets a food patch), az_suite picks them
  with SplitMix64 seeded by SHA-256 of `<item>|<seed>`, among live creatures matching the probe's
  predicate at the probe tick, identically in every arm. If an arm has fewer matching creatures, the
  difference is part of the result, and the measure must be defined so that it stays comparable (for
  example a per-world fraction, not a per-creature mean over different creatures).
- The measure catalogue (ids, definitions, bounds) lives in `docs\eval\MEASURES.md`, written in SB-07. Each
  measure has a unit test on a hand-made world (`az_test_shell`) that can fail.

### 4.7 The trace matrix (E2)

`docs\eval\TRACE.csv`, one row per (catalog, source item id). Columns, in this order:

`catalog, item_id, harvest_record, source_ref, canonical_id, relation, disposition, reason_arg, test_id,
prereg_file, prereg_sha256, seed_block, verdict, d_z_min_arm, ci_lo, ci_hi, p_iut, p_holm, n_pairs,
run_id, raw_dir, honesty, historical_label, provenance, notes`

- `harvest_record` is the JSON pointer into `docs\requirements\<catalog>.json` (the workbook's "Harvest
  record" column uses the same form, `docs/requirements/butlin14.json#/items/0`).
- `relation`: SELF, DUPLICATE-OF, MIRROR-OF, SUPERSEDED-BY, ROLLUP-OF, PART-OF.
- `disposition`: TESTED (a statistical item in a prereg), GATE (an automated PASS/FAIL check), MUTANT
  (one of the five), MEASURED (a reported number with its unit, no test), REVIEWED (a rule that only a
  reviewer can check; `test_id` names the A1 or SB-29 review record), or one reason code (section 4.8).
- `verdict`: a section 3.5 verdict, PASS/FAIL, CAUGHT/MISSED, a number with its unit for MEASURED rows, or
  empty for a reason code.
- `historical_label` and `provenance` keep the old status with its origin (for example
  `R21 arm A | PASS (arm A d 1.04 / t 5.10)` with provenance `v40 R21 seeds 1064-1111`); never in `verdict`.
- `tools\az_trace.py` builds TRACE.csv from the 30 catalogs in `docs\requirements\`, the framework-825
  extraction (SB-02) and `docs\eval\dispositions.csv` (hand-written: one line per item or per id pattern
  with its disposition and reason), plus the scorecard CSVs. `--check` exits non-zero when: an id is
  missing or appears twice per catalog; a DUPLICATE-OF, MIRROR-OF or SUPERSEDED-BY target does not exist;
  a TESTED row lacks a prereg SHA-256 or a run id; a reason code is outside the closed set; or a row's
  catalog count differs from the catalog's item count.
- `docs\eval\TRACE.md` is generated: counts per catalog and disposition, then one table per catalog.

### 4.8 Reason codes (closed set)

| Code | Use when | Required argument | Who may assign |
|---|---|---|---|
| NOT-BUILT | the item needs a mechanism the engine does not have | the missing mechanism, and the milestone or owner decision that would build it | the agent, after SB-03 shows no mechanism (file search plus ABI read) |
| N/A-ENGINE | the item is about the old stack or a document, not about any engine behaviour (WASM, TypeScript bundles, browser vitals, PDF wording) | the stack element | the agent; `laws_and_errata.json` already marks 89 items so (`docs\requirements\errata-laws.md:21`) |
| LAW-CONFLICT | the item, as worded, would break a law | the law number and the native re-specification if one exists (then also add the re-specified row as TESTED) | the agent; never used to avoid a hard test |
| IDEA-BANK | the list is an idea bank (FREAK, NOVEL, Foundations, cube, wishlist, nested precision, frontier map, thousand reports) | none | fixed by `CONTINUE-BUILD.md:57` ("trace the idea banks without scoring them") |
| OUTSIDE-ENGINE | the item concerns tooling around the engine (the demo server, the web page, the status generator) | the component | the agent |
| DUPLICATE-OF | the same test as another row that is tested | the canonical id | the agent, following section 5 |
| SUPERSEDED-BY | a later version of the item replaces it (an R variant, a v40 wording of a v3.0 row) | the superseding id | the agent, following section 5 |
| NEEDS-OWNER | a decision only the owner can take blocks the item | the decision id of section 13 | the agent |
| INDICATOR-ONLY | the item is an indicator-only scorecard row, used together with TESTED or NOT-BUILT in `honesty`, never alone as a reason to skip a test | none | fixed by `CONTINUE-BUILD.md:58` |

INDICATOR-ONLY is a class, not an excuse: an indicator item is still tested when a mechanism exists. In
TRACE it goes in the `honesty` column; `disposition` then says TESTED or NOT-BUILT.

### 4.9 Gate runner (tools\az_gate.py)

- Runs, in order, and stops with a non-zero exit at the first failure: the build of `build\z` with 0
  warnings; `lint_core.py` (and `--strict` from the end of M5 on); `check_text.py`; every ctest test; the new
  engineering gates of section 6.1; the mutant set of section 6.2; `az_trace.py --check`.
- Prints one line per gate with its command, exit code and the log path under `build\work\eval\gates\`.
- Long gates run as separate processes with logs on Z:; the runner is itself started in the background for
  the full set (`CONTINUE-BUILD.md:74,77`).

### 4.10 T-AUDIT (tools\az_audit.py)

T-AUDIT is "a second implementation in a different language ... recomputes from the dumped event log"
(`docs\research\v40\gates.md:38`). Here: from each run's `runs.csv`, recompute with Python `fractions` and
exact integer counts: every exact Wilcoxon p (by enumeration of the subset-sum distribution), every d_z,
the IUT p, Holm, and the verdict table; recompute the bootstrap with the same SplitMix64 stream and compare
to 1e-12 relative (the only float tolerance in this protocol, and it concerns tooling, not the world);
check that every state hash in `runs.csv` appears in `stdout.txt`; check that the prereg SHA-256 in
`env.txt` equals `PINS.txt`. Any mismatch fails the suite's T-AUDIT gate. T-AUDIT never runs a world.

### 4.11 Tests for the tooling (tests\test_suite.c)

- Spec parser: every key of Appendix A accepted, every malformed line refused with exit 6, unknown keys
  refused.
- Seed custody: a reporting seed with `--spec` is refused; `--prereg` with a wrong pin is refused (exit 4); a
  second run on the same (item, block) is refused (exit 5), on a scratch copy of RUNS.csv.
- Arms: each arm id resolves to the documented switch; an unknown arm is refused.
- Verdicts: a synthetic item whose measure is a known function of the arm (constructed with the probe
  API on a hand-made world) gives EFFECT; a constant gives NO EFFECT or INCONCLUSIVE as the table says; a
  bounded measure at 0.96 of max gives FAIL-CEILING.
- Planted defects (run the test against each, it must fail on every one; record which check caught it, as
  `docs\ARCHITECTURE.md:429-432` requires): Holm applied across arms instead of items; d_z computed
  unpaired; a seed dropped silently; the pin check skipped; the ceiling compared against the lesioned mean.

## 5. Joining the lists correctly

### 5.1 The canonical list per owner number

| Owner number | Canonical catalog (the row that gets tested) | Other catalogs that point to it |
|---|---|---|
| 14 | `butlin14.json` (RPT-1 .. AE-2) | `benchmarks100.json` rows RPT-1 .. AE-2 (DUPLICATE-OF), framework L1_BUTLIN14 (DUPLICATE-OF, different criterion wording; `template_criterion` in butlin14.json) |
| 10 | `consciousness10.json` (thaler9 .. butlin14) | `benchmarks100.json` theory rows (DUPLICATE-OF), framework L2_FRAMEWORKS10 F1..F10 (DUPLICATE-OF, one to one per `butlin.md:30-31`) |
| 25 | `cqm25.json` axis ids (the law, `cqm25.md:18`) | `cqm25_framework.json` CQM01..CQM25 and `benchmarks100.json` CQM01..CQM25 (DUPLICATE-OF by position, inferred: `cqm25.md:24-26`; drift at 09, 14, 24, 25, so note the drift in `notes`) |
| 100 things | `passes100.json` (the ledger) and `passes100_suite.json` (the executed suite) | `passes100_v40.json` is the same list as `passes100.json` (DUPLICATE-OF) |
| 100 checklists | `life100_areas.json` (groupings; an area is complete when its 5 sub-checks are) | none |
| 100 benchmarks | `benchmarks100.json` (109 rows) | its rows point into the 14, 10 and 25 (section 5.6) |
| 500 | `checklist500.json` (v3.0, ids LIFE-Lnnn-Ak) | `life100_v40_500.json` and `bench_life100.json` (SUPERSEDED-BY nothing automatically; see 5.5) |

### 5.2 The three 100s: never by row number

- Same-number pairs of ledger and suite are different items: ledger PASS-001 is osmoregulation and ADH,
  suite pass 1 is the CQM10 satiation brake, which the ledger lists as Pass 013; passes 22-25 are shifted by
  one; only 46 of 100 same-number pairs share an item code (`docs\requirements\passes-bench.md:10`).
- Join by item code. Ledger rows carry `extra.target_codes`; suite rows carry `extra.codes`. Measured on
  2026-11-18 with a read-only script over the two files: 70 ledger rows and 61 suite rows carry at least one
  code; 92 distinct codes occur in both; 47 ledger rows have exactly one suite partner by code (43 of them at
  the same number), 3 have several, and 47 suite rows have exactly one ledger partner. Repeat this count in
  SB-20 and use the numbers you get.
- Join algorithm (SB-20, in `tools\az_trace.py`):
  1. build the bipartite graph ledger row - suite row when they share a code;
  2. a one-to-one component is a pair: the suite row gets `relation = DUPLICATE-OF <ledger id>` when it
     tests the ledger's target item (check the wording by hand; record the check in `notes`), otherwise
     `PART-OF`;
  3. a component with several partners is resolved by hand, one line per row in dispositions.csv, with the
     reason;
  4. rows without a code stay SELF; never pair them by number. The ledger's `suite_same_number_id` field is
     kept for reference only.
- The 100 LIFE areas are headers of the 500, not 100 extra experiments: an area's verdict line is the list
  of its 5 sub-check verdicts, never a sixth test (`Asmodensius-comparative-review.md:102`).
- benchmarks100 (109) overlaps the 14, 10 and 25; its last 46 rows have no numeric acceptance criterion
  (`Asmodensius-comparative-review.md:103`).
- Never add the three 100s into one figure (`Asmodensius-comparative-review.md:107`: the 958 requested rows
  "cannot be summed into a completion percentage").

### 5.3 The three CQM wordings

The law (`cqm25.json`, axis ids such as `reproduction_heredity`), the framework suite (`CQM01..CQM25` with
numeric gates and an FP64 ledger in CQM05) and the R21 organism tests (`benchmarks100.json` CQM rows, status
labels such as "Passing") are three different criteria for the same 25 positions. Test the law's axes
natively; map the other two to the axis by position with `relation = DUPLICATE-OF`, and copy their
criteria into `notes` so the reader sees the drift. Where a framework gate is stricter and native-compatible
(for example CQM01 h^2 > 0.25), add it as a secondary measure in the prereg, described but not tested.

### 5.4 Mirrors and duplicates

The framework declares five mirrors (workbook sheet "Framework mirrors"): MIR-01 CQM05 = HW18 (FP64 ledger),
MIR-02 CQM13 = HOT-2, MIR-03 CQM19 = RPT-1, MIR-04 CQM20 = GWT-3, MIR-05 CQM21 = F2 (PCI*). The Butlin guide
adds CQM21 = iit4 and ctm overlaps GWT-3 (`docs\requirements\butlin.md:34`). Rule: test once under the
canonical id (Butlin or theory id for indicator mirrors; the native books gate for MIR-01), give the other
row `MIRROR-OF <id>` and the same verdict, and count it once in any count of unique checks (825 slots, 820
unique; `checklists_index.json:19`).

### 5.5 The three versions of the 500

v3.0 (`checklist500.json`) is the one to test. The v40 wording (`life100_v40_500.json`) and the PDF copy
(`bench_life100.json`) keep the same 100 area slots, but 0 of 500 texts match verbatim and only 32 of 100
area names match by number (`docs\requirements\checklist500.md:19`). Do not map them row by row by
position. Trace them as historical rows: `relation = PART-OF LIFE-Lnnn` (the area, which does match by
number), `disposition = SUPERSEDED-BY LIFE-Lnnn` (the v3.0 area), with the v40 status label in
`historical_label`.

### 5.6 benchmarks100 (109 rows) decomposed

| Rows | Ids | Disposition |
|---|---|---|
| 14 | RPT-1 .. AE-2 | DUPLICATE-OF the butlin14 row with the same id |
| 10 | thaler9 .. butlin14 | DUPLICATE-OF the consciousness10 row with the same id |
| 25 | CQM01 .. CQM25 | DUPLICATE-OF the cqm25 axis at the same position (inferred, note the drift) |
| 14 | RPT2R, RPT2RL5, HOT3RM, HOT3RM-P, GWT-3R, AE1R, PROJR, ctm-R, C16R, CQM18R, CQM08R, CQM03R, CQM01R, K1R | "reference only, never counted" (their category in `benchmarks100.json`): `relation = PART-OF <original id>`, `disposition = DUPLICATE-OF <original id>`, verdict empty, `notes` = "R variant; its v40 result is historical and never counted". K1R and C16R have no original in the 14, 10 or 25: give them `DUPLICATE-OF` the passes100_suite row that carries the same code (C16R is suite pass 2) or, if none, NEEDS-OWNER D-SB-5 |
| 12 | CAP-E, CAP-R, CAP-B, CAP-N, CAP-L, CAP-M, CAP-S, CAP-C, CAP-I, CAP-D, CAP-W, CAP-O | MEASURED as covered or not covered with evidence pointing to native rows: CAP-E to CQM 01/02, CAP-R to LIFE-L077, CAP-B to physics (NOT-BUILT until physics lands), CAP-N to the brain, CAP-L to CQM 07, CAP-M to metab, CAP-S to social, CAP-C NOT-BUILT (no connectome; the owner withdrew literal copying, `Asmodensius-comparative-review.md:64`), CAP-I to the Butlin scorecard, CAP-D to the determinism matrix, CAP-W N/A-ENGINE (browser), CAP-O NEEDS-OWNER (publication; default: not published) |
| 34 | Q-PRED .. Q-ROBUST (DELARP qualities) | no criteria in the source ("not yet scored"); map each to the Butlin or CQM row whose `quality_id` names it (butlin14.json and consciousness10.json carry `quality_id`) as DUPLICATE-OF; the rest NEEDS-OWNER D-SB-5 (a criterion) |

### 5.7 Grok Bot (public) against the Windows v40 export: separate provenance

- Facts (GPT Astra review, `Asmodensius-comparative-review.md:12,67,72-73`): the public main revision
  `c72a4ec9a0cb` and the local v40 tree share files but no commit SHA; the public SC-PORT (code pin
  `bc6ad6a`, preregistration `cb3aaed`) records AE1 pooled foraging d = 0.73, t = 5.48, below 0.8; the local
  v40 scorecard records d = 0.86, t = 6.41; recomputed from each archive's 112 raw observations per arm:
  0.73267 and 0.85702; the lesioned control means differ too (0.30370 and 0.29797). The cause is not known.
- Rule: never merge, average or pick one. In TRACE the `historical_label` names the archive, for example
  `SC-PORT public c72a4ec9a0cb / bc6ad6a | AE1 d 0.73` and, on a second line in `notes`,
  `SC-PORT local v40 Windows export | AE1 d 0.86`. Neither is native evidence. The native AE-1 result comes
  only from its own prereg.
- Not needed for native completion: finding the cause. If the owner asks, it is a separate investigation
  (compare the two organism sources and the two preregistration files; the review says they differ).

### 5.8 Using the GPT Astra crosswalk workbook

- Files: `C:\Users\Alexa\Documents\Codex\2026-11-18\the-bundle-is-clean-no-key\outputs\` holds
  `Asmodensius-comparative-review.md` (53,102 bytes), `Asmodensius-evidence-crosswalk.xlsx` (846,147 bytes)
  and `Native-verification.txt` (19,051 bytes). Copy them (SB-01) to
  `build\work\eval\inputs\astra-2026-11-18\` with `Copy-Item -LiteralPath` and record their SHA-256; read
  only the copies afterwards. Nothing of the project lives on C:. Provenance: the GPT Astra sessions in the
  Codex desktop app wrote these three files on 2026-11-18, working from that C: folder (they ended about 18:41
  UTC); the same sessions took the snapshot `build\work\snapshots\2026-11-18-pre-sense-astra` and wrote the
  baseline logs `build\work\logs\sense_baseline_100k.txt` and `sense_baseline_ctest.txt`, a recovery point
  and a reproduced baseline, not progress (no sense code).
- Sheets: "Requested items" (958 rows: the 8 requested catalogs, 11 columns: Catalog, Source item ID, Item
  wording, Group or category, Source criterion, Historical label and origin, Relationship, Native M8,
  Original source locator, Harvest record, Additional source fields), "Supplementary items" (4,854 rows: the
  other 22 catalogs), "Framework 825" (825 rows with suite, check id, specification, controls, source gate,
  status, cost tag, paired seeds, family size, prerequisites, mirror, CSV row and JSON pointer), "Framework
  mirrors" (5), "List index" (42), "Catalog sources" (30), "Guide".
- Use it as a cross-check, never as the source: `az_trace.py --check-workbook <copy>` compares, per catalog,
  the set of ids and the item count with TRACE.csv and reports differences. The source of truth stays
  `docs\requirements\*.json` and the master CSV on Z:.
- Use "Framework 825" to verify the SB-02 extraction of the five unextracted suites (counts 100, 100, 12, 40,
  24; ids; JSON pointers).
- Do not copy the workbook's "Unrun at M8" or its relationship texts into verdicts; they describe the
  snapshot of 2026-11-18.
- Python 3.9 on this machine may lack an xlsx library; reading the sheets with `zipfile` and the XML is
  enough (that is how it was read for this file). No install.

### 5.9 The parts of the 825 not yet extracted

L4 FREAK100 (framework form), L6 NOVEL100 (framework form), L7 WSE12, L8 SOFT40, L9 HW24, and the lists
MILESTONES_100, MIRRORS_5, CITATIONS_15, ERRATA_95 (the closure register CSV) are indexed only
(`checklists_index.json:68-157,184-209,331-397`). SB-02 extracts them from
`Z:\[Vibe Coded (AI)]\CLAUDECODE\ALife-Framework-v3.0-Master-Checklists.csv` (lines 51-150, 651-826,
827-926) and the Master-Spec JSON, into `docs\requirements\framework825_<list>.json` with the same five keys
as the other sets, and checks them against the workbook.

## 6. Per-suite protocols

Every statistical suite follows the same five steps: (1) mechanism inventory (SB-03) and dispositions;
(2) item specs in `docs\eval\tuning\<suite>.spec`; (3) tuning on seeds 1-99 until each probe is
well-defined, non-ceiling and not invariant-breaking; (4) lock the prereg (section 3.11); (5) one reporting
run on 48 seeds, T-AUDIT, scorecard, TRACE update. The probe ids used below are defined in SB-07 and
`docs\eval\PROBES.md`:

| Probe | Situation set up through the probe API (identical in every arm) |
|---|---|
| P-FORAGE-PATCH | a food patch made by moving matter into a region (az_probe_move), later relocated |
| P-DELAYED-CUE | a cue (a mark, or a light or food gradient) shown for k ticks near a food patch, then removed; a delay; the food returns |
| P-REVERSAL | two patches; the one that holds food swaps at tick R |
| P-CONJUNCTION | food is safe only where cue A and cue B co-occur; A-only and B-only patches carry toxin |
| P-CUE-DISCRIM | a food patch and a toxin patch with different cues; cue positions in reporting runs differ from tuning |
| P-DETOUR | food behind a bedrock wall (needs real physics, which forbids entering bedrock) |
| P-HEAT-PULSE | heat moved into a region (az_probe_heat), then left to relax |
| P-DEPRIVATION | food moved out of a region (conserving: it goes to a far region) |
| P-NEED-SWITCH | energy drained from focal creatures (az_probe_drain) |
| P-SIGNAL | a threat or food event placed out of the receivers' sensing range, in the emitters' range |
| P-TOXIN-TRADEOFF | a rich patch next to a toxin hot spot and a poor clean patch |
| P-REPAIR | a share of focal brains pruned (az_probe_prune) |
| P-PERTURB-FORK | snapshot at tick t; in the fork one hidden unit nudged (az_probe_nudge); both forks run k ticks |
| P-PEDIGREE | no intervention: births with parent ids from the ledger, analysed after the run |
| P-NOVELTY-HORIZON | no intervention: long runs read at horizons H1 < H2 < H3 |
| P-BLIND | a probe with its cue removed: the success expected is chance (used by the Oracle mutant detector) |

### 6.1 Engineering gates (first in the order)

Existing: 33 test programs and 13 ctest gates, 46 of 46 green in 253 s on `build\z`
(`04-GOALS-AND-BENCHMARKS.md:108`). Add these gates in SB-12; each must be able to fail (plant a defect and
show it):

| Gate | Re-specifies (catalog ids) | Check | Command (read each tool's `--help` first) |
|---|---|---|---|
| G-BOOKS-10K | GATE-03, BUD-1.1, BUD-1.2, BUD-1.3, CQM05 (law part), MIR-01, HW18, LIFE-L001-A2, LIFE-L017-A5, passes100_suite global ledger rule and pass 100 | 10,000 ticks, lab, 3000 founders, grid 4,4,3: matter and energy equal at every tick, `field_underflow == 0`, `dropped_events == 0`; also once wild | `& "$R\build\z\az_run.exe" --lab --seed 7 --n 3000 --ticks 10000 --grid 4,4,3`, plus `--cap` and the per-tick audit option exactly as `az_run --help` names them (never a guessed flag, `CONTINUE-BUILD.md:33`) |
| G-NOALLOC-10K | GATE-02, BUD-3.x memory rows in spirit | no undefined symbol but mem* in the core (existing object gate); arena locked; page-fault count unchanged between tick 100 and tick 10,000 (needs the page-fault readout E3 adds to az_bench) | az_bench with the page-fault column |
| G-OBSERVER-10K | GATE-11, A-01 (laws_and_errata), CQM24 part | hash before and after every rendering equal; a 10,000-tick headless run and the same run rendered every 100 ticks end with the same hash | new `tests\cli\observer_10k.py` (az_snap), registered by the lead |
| G-SEEDLIT | GATE-01 (seed firewall half), BUD-2.1 | no literal 5001..5024 or 5101..5124 used as a seed in `core\`, `kernels\`, `platform\`, `tests\` except the documented exceptions | new `tools\az_seedlit.py` |
| G-PROVENANCE | GATE-12, LIFE-L097 | every file under `core\`, `kernels\`, `tests\upstream\` is either written here or listed in `THIRD_PARTY_NOTICES.md` with its LICENSE beside it; no mesh, sprite or connectome asset files | new `tools\az_provenance.py` |
| G-STRICT-LINT | LAW-4 trace, M5 DoD | `python -I "$R\tools\lint_core.py" "$R" --strict` exits 0 | existing tool |
| G-TWINS | BUD-1.5 (native form), CQM23 part | every assembly twin equal to its C reference (existing twin tests; physics adds `az_kin_axis_avx2`) | ctest |
| G-COVERAGE | BUD-2.4 (native form) | line coverage of `core\` reported from a separate `build\coverage` folder with gcc `--coverage` (gcov ships with gcc; check it exists; install nothing) | report only; threshold is D-SB-4 |
| G-TRACE | E2, GATE-13 part | `python -I "$R\tools\az_trace.py" --check` exits 0 | new tool |
| G-AUDIT | GATE-08, T-AUDIT | `python -I "$R\tools\az_audit.py" --all "$R\build\work\eval"` exits 0 | new tool |

### 6.2 The five mutants (GATE-04), natively

**This section is canonical** for the five mutants' definitions, detectors and method
(`00-MASTER-PROMPT.md` 5.10 row 16): 06 WP-P7-03 and 09 CL-10 point here. No mutant code and no
`-DAZ_MUTANT_*` switch ever enters the live tree; one folder (`build\work\eval\mutants\`), one table
(`docs\eval\MUTANTS.md`) and one ctest gate (`gate04_mutants`, added by the lead at a landing boundary,
running each named detector against the mutant builds and the clean tree). Extra detectors from 06's
earlier draft (a state-conformance tool, the sense firewall test, the tk_hop bank test) may be added as
second detectors, never as replacements.

A1 (milestone 7) builds them; milestone 8 reuses the detectors. One implementation, used by both. Each
mutant is a scratch copy of the tree in `build\work\eval\mutants\<name>\tree\`, built in
`build\work\eval\mutants\<name>\build\` (configured from the real path; CMake globs use `AZ_GLOB_ROOT`).
Mutants run on tuning seeds only.

| Mutant | Planted defect in the native engine | Detector that must fail on it (and pass on the clean tree) | Native catch rule |
|---|---|---|---|
| M-LEAKY | one module books one energy quantum less than it moves, at one tick per run | G-BOOKS-10K and every az_suite run (exit 1) | any leak > 0 fails: the v40 "-2e-11 kJ" threshold has no meaning under exact integer books (`04-GOALS-AND-BENCHMARKS.md:102`) |
| M-COSMETIC | a new state-evolving column (a "fake cortex" driven by noise) that no module reads | strict lint (W1: written, never read) and the E1-style contrast of its new switch: NO EFFECT or INERT on every measure | the C-NC idea of `gates9.json` [2/9]: a disconnected organ must be rejected |
| M-ORACLE | a creature reads the probe's hidden food position, which no receptor carries | P-BLIND: intact success above the SENSES arm on a blinded trial; and on cued trials the SENSES arm loses no performance | "performance without information": success when the sensory path carries nothing is an oracle |
| M-FROZEN | the plasticity path returns early while claiming to learn | the P-REVERSAL item (AE-1 / CQM07): the clean tree's EFFECT against FROZEN_LEARN becomes NO EFFECT or INCONCLUSIVE in the mutant tree on the same tuning seeds | the frozen learner fails reversal |
| M-SHUFFLED | associative recall keys permuted (another pattern is returned) | the P-DELAYED-CUE memory item (CQM08): the clean tree's EFFECT against TARGET:MEMORY disappears | recall collapses |

DoD: 5 of 5 caught, each by its named detector, each detector passing on the clean tree; table in
`docs\eval\MUTANTS.md` with the build logs and detector outputs; `gate04_mutants` green in ctest. M-ORACLE, M-FROZEN and M-SHUFFLED need the
probes (SB-07) and the real sense and physics modules.

### 6.3 E1: the lesion battery

- Tool: `tools\az_lesion.c`, as `CONTINUE-BUILD.md:56` names it, after SB-08 adds `--prereg`, the
  INCONCLUSIVE and d_z-interval logic (through `azs_verdict`) and any direct endpoint a switch needs.
- Items: every named switch (21 now, plus each new mechanism switch from SB-04 and from the sense and
  physics modules). The arm switches of section 3.3 (SHAM, SHUFFLE and so on) are controls, not E1 items,
  but each still gets its own lesion test in its module.
- Primary measure per switch: chosen in tuning, before the lock. Today's measures (live, births, deaths,
  energy, generation, clades, interactions; `tools\az_lesion.c:59-66`) are weak for GRAVITY, SENSES,
  TIER1_Q and others that change the state on 24 of 24 seeds with a tiny energy effect
  (`Asmodensius-comparative-review.md:143`). Add a direct endpoint where the mechanism has one (for example
  the mean height change of bodies denser than their medium for GRAVITY, once physics honours it), as a new
  row of the measure enum in SB-08.
- Claim scope: E1 claims only "switch X causally changes measure Y". It is never a capability claim.
- Runs: 22 runs per seed (intact plus 21) on 5101-5124 (primary table) and on 5001-5024 (replication table):
  1,056 runs.
- DoD: both tables, no INERT row, every row with a verdict from section 3.5, CSV via `--csv`, T-AUDIT
  agreement, `docs\eval\E1-LESIONS.md`. INERT on the complete build is a defect to fix before E1 can close
  (`CONTINUE-BUILD.md:56`).

### 6.4 E3: speed and scale, and M2

- Tool: `tools\az_bench.c`; read its usage first. Grid: populations 1, 10, 100, 1,000, 10,000, 100,000,
  540,000 and 1,000,000; threads 1, 8 (the P cores) and 24 (P and E, `--ecores`); kernels 0 and 1; three
  repeats each; report ticks per second, per-stage ms, the cache budget (`az_budget`), page faults and the
  lock state (`CONTINUE-BUILD.md:59`; STATUS line 689).
- Label every row above the knee that ES-1 (M2) measures "DRAM-bound"; until M2 exists, label every row
  above about 35,000 creatures "beyond L2+L3 by estimate (DRAM-bound)" (08 section 2.4: with each
  creature's own brain read every tick, about 2.2 KB per creature, 76 MB of L2 + L3 holds about 35,000;
  the older "about 540,000" of `04-GOALS-AND-BENCHMARKS.md:208-210` counted only the 140-byte hot tier).
  Never call a row cache-resident before M2 says so. Label every row with the load measured during it.
- Report separately, never mixed with ticks per second: the live demo's pictures per second and its
  latency from a tick to a picture on screen (the owner's complaint, P51: about 2 to 6 pictures per second
  while the engine alone runs 150 to 330 ticks per second at 5,000 to 10,000 creatures; both measured before
  worldgen with no log, quote 77 to 330 until PQ-0, `00-MASTER-PROMPT.md` 1.5; in his own wild world of
  1,000,416 creatures, logged: 414.3 ms per tick and 4,261.3 ms per 1600x900 picture,
  `build\work\logs\demo_owner_world_2026-11-18_f226.json`, 08 section 2.5). This is CQM axis 25's
  "latency and frame rate at stated hardware" and is OUTSIDE-ENGINE for the engine's own scorecard.
- M2 (STATUS line 674) is a milestone 6 deliverable: the per-stage cache table against L1D 48 KB, L2 3 MB
  per P core and 4 MB per E cluster, L3 36 MB. E3 links to it; G1 needs both.
- DoD: `docs\eval\BENCH-SCALE.md` with every command and the raw outputs under
  `build\work\eval\E3-<date>\`.

### 6.5 CQM-25 (canonical: the law axes in `cqm25.json`)

Class: CAPABILITY unless marked INDICATOR (axes 13 and 19-22) or ENGINEERING (23-25). "Needs" names the
stub modules the item cannot run without. The disposition column is the expectation from the 2026-11-18
reading; SB-03 decides.

| Axis (position) | Native item and primary measure | Required arms | Mechanism | Needs | Expected disposition |
|---|---|---|---|---|---|
| reproduction_heredity (01) | P-PEDIGREE: midparent-offspring regression slope (x1000) of one heritable phenotype scalar chosen in tuning (for example `AZ_P_DENS`) | SURROGATE:pedigree-permuted (parents permuted within species and birth window), NEUTRAL | genome inheritance, life, mate | stats (distributions) | TESTED |
| evolutionary_innovation (02) | P-NOVELTY-HORIZON: behaviour archetypes (`az_classify`) first seen after H1 that persist P ticks, counted at H3; clade counts are branching, not function (`Asmodensius-comparative-review.md:114`) | NEUTRAL, FROZEN_HEREDITY | mutation and selection | stats (classify is a no-op in the stub) | TESTED |
| environmental_coupling (03a, world to creature) | P-FORAGE-PATCH with relocation: fraction of focal creatures whose intake rate recovers within W ticks | TARGET:SENSES, SHUFFLE_SENSE | sense, brain, metab | sense, physics | TESTED |
| environmental_coupling (03b, creature to world) | matter deposited or built by creatures per 1,000 ticks and its effect on other creatures' intake | NO_BUILD, SHAM_COST | econ BUILD, metab EXCRETE | none beyond lint W1 (BUILD is unread today, `PROGRESS.md:1502`) | TESTED after W1 is fixed |
| morphology_development (04) | travel per energy unit of creatures in a viscous niche | SHAPE_FIXED, SHAM_COST, NEUTRAL | parts (GROW, SHED), physics thrust and drag | physics, stats | TESTED |
| embodied_dynamics (05) | GATE: G-BOOKS-10K plus the invariants at field_stride 2, 4, 8, 16 and tier1_period 16, 32, 64; the FP64 1e-12 ledger of CQM05 is re-specified as exact equality | none (gate) | the whole engine | none | GATE |
| perception (06) | P-CUE-DISCRIM: share of approaches that go to the food patch | TARGET:SENSES, SHUFFLE_SENSE | sense, brain | sense, physics | TESTED |
| learning (07) | P-REVERSAL: focal intake in the window after the swap | FROZEN_LEARN, YOKED, SHAM_COST if plasticity is costed | Tier 1 plasticity | sense, physics | TESTED |
| memory (08) | P-DELAYED-CUE: share of focal creatures that return to the cued place | TARGET:MEMORY, SHUFFLE_MEMORY, SHAM_COST | tk_hop in Tier 1 | sense, physics | TESTED |
| predictive_causal_models (09) | reward prediction only: decline of mean absolute surprise over repeated identical patches; intervention prediction has no candidate mechanism | RBASE_FROZEN, SHUFFLE_SENSE | Tier 1 rbase (`core\az_tier1.c:10-14`) | sense | TESTED (narrow; state the scope) plus NOT-BUILT for intervention prediction |
| autonomous_goals (10) | P-NEED-SWITCH: share of drained creatures that switch to foraging within W ticks | RANDOM_POLICY, PASSIVE | brain, Tier 1 | sense, physics | TESTED |
| reasoning_planning (11) | P-DETOUR: share of focal creatures that reach the food within W ticks | RANDOM_POLICY, SHUFFLE_SENSE, TARGET from SB-03 | none found ("planning" may be emergent) | physics, sense | NOT-BUILT unless SB-03 names a candidate |
| transfer_generalization (12) | held-out compound of two learned cue-action pairs | FROZEN_LEARN, HIDDEN_OFF | brain, Tier 1 | sense, stats | decide in SB-03 |
| metacognition_self_model (13) | MIRROR-OF HOT-2 | | | | INDICATOR; NOT-BUILT expected |
| social_communication (14a) | P-SIGNAL: receivers' response to an event they cannot sense, through emitters | TARGET:SIGNALS, SHUFFLE_SIGNAL, SHAM_COST | social signals | sense (SIG channels are partly filled by the stub) | TESTED |
| social_communication (14b, games) | a partner-dependent strategy: a measured strategy change when the partner's stake changes; SOCIAL silences only the counter (`Asmodensius-comparative-review.md:118`) | SHUFFLE_SENSE restricted to STAKE and PAYOFF (a channel-set parameter added in SB-04), RANDOM_POLICY | social, econ | sense (PAYOFF and STAKE are neutral in the stub, `Asmodensius-comparative-review.md:150`) | TESTED after sense |
| individuality (15) | decide in SB-03; candidate: identity kept through store-block turnover while intake continues | `AZ_LESION_EAT` (section 4.4) or a candidate from SB-03 | store, metab | stats | decide in SB-03 |
| resource_maintenance (16) | P-DEPRIVATION: survival share of focal creatures over W ticks | PASSIVE, SHAM_COST | metab, brain | physics | TESTED |
| homeostasis (17) | P-HEAT-PULSE: share of ticks focal creatures spend in cells inside their temperature band | PASSIVE, SHUFFLE_SENSE | metab, brain | sense, physics | TESTED |
| repair_resilience (18) | P-REPAIR: intake recovery after pruning relative to before | FROZEN_LEARN | plasticity as compensation; no repair module found | sense, physics | TESTED (compensation only) |
| recurrent_processing (19) | MIRROR-OF RPT-1 | | | | INDICATOR |
| global_availability (20) | MIRROR-OF GWT-3 | | | | INDICATOR; NOT-BUILT expected |
| causal_integration (21) | MIRROR-OF iit4 (P-PERTURB-FORK) | | | | INDICATOR |
| valence_tradeoffs (22) | P-TOXIN-TRADEOFF: rich-patch choice share as a function of energy (devaluation when satiated) | SHUFFLE_SENSE, TARGET from SB-03 (candidate: the PAIN channel) | brain, metab, sense | sense, physics | INDICATOR; TESTED |
| substrate_pluralism (23) | GATE: the same lab hash on kernels 0 and 1, on 1, 8 and 24 workers, on gcc and clang builds; the GPU component bit-identical standalone. Scope: execution substrates, not minds on other substrates | none (gate) | | | GATE |
| instrumentation_reproducibility (24) | GATE: every reporting run archived; lab replay and snapshot resume give equal hashes; T-AUDIT agrees; wild runs keep their entropy journal (recorded, not replayed) | none (gate) | | | GATE |
| visual_embodied_scale (25) | MEASURED: per-entity degrees of freedom (112 named traits, 64 sense channels, 24 motors, `Asmodensius-comparative-review.md:85`; body-part distribution from stats), population capacity (E3), ticks per second (E3) and pictures per second (E3, separate) | none | | stats | MEASURED, never aggregated |

### 6.6 Butlin 14 (canonical: `butlin14.json`; class INDICATOR for every row)

The 2026-11-18 grep of `core\*.c` found no workspace, broadcast, confidence monitor, forward model or
efference copy; Tier 1 has a reward prediction error, lanes, Hopfield memory, an attention gain pulled by
the crowd shape (`core\az_tier1.c:39,263-268`) and the brain has recurrence. SB-03 repeats the search with
more terms and the ABI before any NOT-BUILT is written.

| Id | Native item and primary measure | Required arms | Mechanism | Needs | Expected |
|---|---|---|---|---|---|
| RPT-1 | P-DELAYED-CUE: share of focal creatures nearer the cued place at the end of the delay than at cue offset | TARGET:RECURRENCE, SHAM_SIZE (matched to the recurrent weights) | brain recurrence | sense, physics | TESTED |
| RPT-2 | P-CONJUNCTION: share of approaches to the A-and-B patch among all approaches | HIDDEN_OFF, SHAM_SIZE | brain hidden units | sense, physics | TESTED if hidden units exist |
| GWT-1 | structural: modularity Q (x1000) of evolved brain graphs at tick H | SURROGATE:degree-preserving rewiring | brain topology | stats | TESTED (structural only); functional module selectivity NOT-BUILT |
| GWT-2 | capacity-limited workspace | | none found | | NOT-BUILT |
| GWT-3 | global broadcast | | none found | | NOT-BUILT |
| GWT-4 | state-dependent attention: change of lane gains when arousal changes (P-NEED-SWITCH) | TARGET:TIER1_SH, SHAM_COST if tk_sh is costed | tk_sh attention in Tier 1 (driven by the crowd and ATTENTION, not by a workspace) | sense | TESTED (partial; say so) |
| HOT-1 | top-down generative perception | | none found | | NOT-BUILT |
| HOT-2 | metacognitive monitoring | | none found | | NOT-BUILT |
| HOT-3 | acting on confidence (opt-out) | | none found | | NOT-BUILT |
| HOT-4 | Spearman rho (x1000) between stimulus distance and hidden-state distance over probe stimuli; bounded, so the ceiling rule applies | SHUFFLE_SENSE | brain | sense, stats | TESTED |
| AST-1 | attention schema | | none found | | NOT-BUILT |
| PP-1 | reward prediction only (as CQM 09): sensory next-input prediction has no candidate | RBASE_FROZEN | Tier 1 rbase | sense | TESTED (narrow) |
| AE-1 | P-REVERSAL, as its own item (not a mirror of CQM 07: a different family) | FROZEN_LEARN, YOKED, RANDOM_POLICY | Tier 1 RPE plasticity | sense, physics | TESTED |
| AE-2 | forward body model | | none found | | NOT-BUILT |

Rule for NOT-BUILT here: the owner decides whether explicit cognitive architecture (a workspace, a
confidence monitor, a forward model) is to be built, or whether only emergent properties of the evolved
brain are to be tested (D-SB-3). Default: test emergent properties with the best existing candidate lesion;
where none can isolate the property, NOT-BUILT. Never build a mechanism only to pass an indicator.

### 6.7 The 10 theories (canonical: `consciousness10.json`; class INDICATOR)

| Id | Native item | Required arms | Expected |
|---|---|---|---|
| thaler9 | noise-driven generation checked by an internal critic: NOISE exists, no critic found | | NOT-BUILT |
| iit4 | P-PERTURB-FORK: an integer perturbation index (units of the brain whose state differs after k ticks, times the number of distinct response patterns across nudge sites). It must not read the coupling weights: the v40 proxy was circular (`docs\requirements\butlin.md:35`) | TARGET:RECURRENCE, HIDDEN_OFF (a partition), SURROGATE:noise-matched nudge | TESTED |
| activeInference | an epistemic (uncertainty-seeking) term | | NOT-BUILT |
| attentionSchema | a model of another agent's attention | | NOT-BUILT |
| fieldIntegration | neural field or phase coupling | | NOT-BUILT |
| ual | trace conditioning: anticipatory approach to the cue place during the gap before food (P-DELAYED-CUE variant) | FROZEN_LEARN; an eligibility-trace switch only if SB-03 finds traces | TESTED (credited to plasticity, not to eligibility traces unless a trace switch exists) |
| sensorimotor | efference copy | | NOT-BUILT |
| projective | self/other ownership via efference copy | | NOT-BUILT |
| ctm | competition and broadcast; also overlaps GWT-3 | | NOT-BUILT; MIRROR-OF GWT-3 |
| butlin14 | ROLLUP-OF butlin14: "passes only at 14/14" (`consciousness10.json`); printed as "k of 14 EFFECT, j NOT-BUILT, ..." and never tested | | ROLLUP, computed after Butlin |

The theories family has m = the tested items only (iit4 and ual today, plus any SB-03 adds).

### 6.8 The 100 passes (ledger `passes100.json` and suite `passes100_suite.json`)

- The ledger rows are a development record of the TypeScript lab, not a forward requirement list
  (`checklists_index.json:589`). Each row still gets a native disposition: find the native mechanism for
  its target item (`extra.target_item`); if one exists, write an item whose contrast is the row's
  "Lesion / Control" cell (`criterion`) translated into native arms; if none, NOT-BUILT; if the row is about
  the v40 pipeline or documents, N/A-ENGINE or SUPERSEDED-BY a native gate.
- Suite rows join to ledger rows by code (section 5.2). The suite's `ok :=` margins were tuned on the seed
  they test (`docs\research\v40\gates.md:240`, weakness W11), so they are never reused as native
  thresholds; the native item uses the M8 rule.
- Suite row 99 (ledger and origins documents exist) is N/A-ENGINE; suite row 100 (all passed and global
  ledger below 1e-11) is SUPERSEDED-BY G-BOOKS-10K plus the scorecard itself.
- Waves and their native areas (the first pass of SB-03):

| Wave | Ledger source | Suite title | Native area | Likely dispositions |
|---|---|---|---|---|
| 01 | life_systems.ts | Homeostasis, drive arbitration and viscera | metab, Tier 1 | TESTED or NOT-BUILT per organ |
| 02 | life_systems.ts | Immunity, DNA repair, pain, autotomy, armor and save points | metab, genome; save points are snapshots | immunity NOT-BUILT; save points GATE |
| 03 | cognition_passes.ts | Dendritic XOR, working memory, cascade and Hopfield | brain, tk_hop | TESTED where Butlin rows test the same |
| 04 | cognition_passes.ts | Blink/masking, AST, PCI, agency, conditioning and goal | brain, Tier 1 | mostly MIRROR-OF Butlin and theory rows |
| 05 | cognition_passes.ts | Quantum geometry, chance literacy, geodesics and anomalies | tk_qgt, tk_qsv | TESTED with law 7 wording (classical simulation) |
| 06 | embodied_ontogeny.ts | Adaptive body, limbs, bioelectric, grid/place and play | physics, parts | after physics |
| 07 | embodied_ontogeny.ts | Shared dreams, cryptobiosis, GRN, ontogeny and sex/kin | life, mate, genome, KIN | mixed |
| 08 | ecology_passes.ts, alife_metrics.ts | Culture, empathy, skill market, tools, caches and Ashby | social, econ | mixed |
| 09 | ecology_passes.ts, alife_metrics.ts | Predators, parasites, Daisyworld, RD, archipelago and QD | social, weather, chem, worldgen | mixed |
| 10 | pipeline and hardening | Pipeline, C-NC, snapshot fix, stress, 34 qualities and audit | gates | SUPERSEDED-BY native gates, N/A-ENGINE |

- Holm family: PASSES100, m = tested rows (ledger rows and unjoined suite rows together; joined suite rows
  are duplicates and not counted twice).

### 6.9 The 100 LIFE areas and the 500 (`life100_areas.json`, `checklist500.json`)

- 500 items, 48 seeds (the framework says 64; the deviation is the owner default, `CONTINUE-BUILD.md:87`;
  write it into the prereg). Holm family LIFE500, m = tested sub-checks. The framework's uniform contrast is
  "Intact vs Organ/Pathway Lesion + First-Law Guard" with five arms (`checklist500.md:16`); natively that is
  TARGET plus SHAM_SIZE plus the guard of section 3.9, and further arms where section 3.3 requires them.
- Each sub-check is one item. Reuse a probe and a world config across the five sub-checks of an area where
  possible, so the intact runs are shared (section 8).
- Rows that assume the old stack (`checklist500.md:60-62`):
  - LIFE-L001-A2 (11-pool FP64 ledger, < 1e-12) and LIFE-L017-A5 (exact FP64 body-to-detritus handoff):
    LAW-CONFLICT law 5; re-specified as G-BOOKS-10K and as a death-tick handoff check (the matter of a dying
    body reaches the detritus field in the same tick, books exact), added to `tests\test_life.c` by the
    life owner;
  - LIFE-L052-A1 (live profiler in "Tab 19"): N/A-ENGINE; its native form is the per-stage profile of E3 and
    M2 (MEASURED);
  - LIFE-L052-A2 (WebGPU/CUDA population kernels): NOT-BUILT until S2, which waits for the owner's law-2
    wording (NEEDS-OWNER D2 of `00-PROMPT.md:209-210`);
  - LIFE-L040 (quantum-inspired internal sense, for example A1 "Born-rule readout"): test tk_qsv as a
    classical mechanism with TIER1_Q; never call it quantum (law 8 of `00-PROMPT.md:89-90`).
- Appendix C lists the 100 areas with candidate native owners and stub dependencies, as the starting point
  of SB-03.

### 6.10 The 109-item benchmark catalog (`benchmarks100.json`)

Trace only, with the mapping of section 5.6. Nothing in it is run separately: its 49 scored rows duplicate
the 14, 10 and 25; its 12 capabilities are MEASURED from native rows; its 34 qualities have no criterion.

### 6.11 The 13 release gates (`antilarp13.json`)

| Gate | Native disposition |
|---|---|
| GATE-01 git and seed firewall | the git half NEEDS-OWNER D1(a) (blocked until the first commit, `CONTINUE-BUILD.md:86`); the seed half is G-SEEDLIT |
| GATE-02 zero-alloc SoA, RAM fit | G-NOALLOC-10K; the WASM and BigInt parts N/A-ENGINE |
| GATE-03 FP64 first and second law | G-BOOKS-10K for the first law; the second-law part ("0 heat-to-ATP backflow") needs a native definition: no energy may move from a heat pool into creature energy except through a declared reaction kind, counted from ledger event kinds; write it in SB-12 with a planted mutant that moves heat into energy |
| GATE-04 five mutants | section 6.2 |
| GATE-05 chemoton, antithetic control, 3-phase fasting | NOT-BUILT for the chemoton and the antithetic controller unless SB-03 finds them in chem; fasting survival is CQM 16 |
| GATE-06 XPBD soft hydrostat | NOT-BUILT (the physics brief is a rigid point body in a medium, `docs\agents\W2-BRIEFS\physics.md`); the "0 NaN" part is N/A-ENGINE (integers have no NaN) |
| GATE-07 packed multi-precision brain, 50 percent offload | NOT-BUILT (the brain is a sparse int8 network); GPU offload waits for S2 |
| GATE-08 multi-arm Holm FWER | GATE: test_suite plus G-AUDIT (this protocol) |
| GATE-09 WSE-12 rollback | LAW-CONFLICT law 4 for edits inside a reported world (external weight edits are steering); NEEDS-OWNER D-SB-6 for a lab-only editing tool |
| GATE-10 ontogeny, metamorphosis, heredity | heredity is CQM 01; Kleiber allometry is MEASURED (metabolic rate against mass, exponent reported with its interval, no claim); metamorphosis NOT-BUILT unless life stages qualify (SB-03) |
| GATE-11 observer independence | G-OBSERVER-10K; the governor part N/A-ENGINE (no governor; A-01 says render-only) |
| GATE-12 clean-room originality | G-PROVENANCE (not a novelty claim) |
| GATE-13 provenance, citations, errata closure | G-TRACE over laws_and_errata plus the CIT-01..15 list after SB-02 extracts it |

`antilarp13_v40impl.json` is the v40 wiring: SUPERSEDED-BY the framework gate of the same topic, one row
each, by name and not by number (their numbering differs, `docs\requirements\cqm25.md:33`).

### 6.12 The v40 gates9 and budgets

| Row | Native disposition |
|---|---|
| gates9 [1/9] bundles | N/A-ENGINE; native analogue: the 0-warning build |
| [2/9] conformance and C-NC | SUPERSEDED-BY G-STRICT-LINT and M-COSMETIC |
| [3/9] metabolic viability | SUPERSEDED-BY CQM 16 and the LIFE body-economy rows |
| [4/9] 28 functional checks | SUPERSEDED-BY the suites |
| [5/9] 100-pass suite | SUPERSEDED-BY section 6.8 |
| [6/9] APEX and mutants | SUPERSEDED-BY section 6.2; APEX-25 itself is historical (`apex25.json`, N/A-ENGINE) |
| [7/9] held-out rigor | SUPERSEDED-BY this protocol |
| [8/9] independent Python audit | SUPERSEDED-BY T-AUDIT (which, unlike the v40 audit, recomputes from raw run records, weakness W15 in `docs\research\v40\gates.md:244`) |
| [9/9] lint, coverage, secret scan | G-STRICT-LINT, G-COVERAGE, and a scan for API-key-like strings (`00-PROMPT.md:220`) in the new `tools\az_provenance.py` |
| BUD-1.1 to 1.3 | SUPERSEDED-BY G-BOOKS-10K (exact equality) |
| BUD-1.4 entropy production rate | NEEDS a native definition; NOT-BUILT until chem declares one |
| BUD-1.5 WASM vs TS parity | SUPERSEDED-BY G-TWINS |
| BUD-1.6, 1.7 | N/A-ENGINE (Python decimal constants; Morris-Lecar is not in the engine) |
| BUD-2.1 seed firewall | SUPERSEDED-BY G-SEEDLIT and the custody checks |
| BUD-2.2 IUT and Holm | SUPERSEDED-BY section 3.4 |
| BUD-2.3 mutants 5/5 | SUPERSEDED-BY GATE-04 |
| BUD-2.4 coverage | G-COVERAGE |
| BUD-3.1, 3.3, 3.4, 3.5, 3.8 | N/A-ENGINE (WASM and browser bundles, web vitals) |
| BUD-3.2 snapshot size | MEASURED: `.azsnap` size for the 100k and 1M reference worlds |
| BUD-3.6, 3.7 | OUTSIDE-ENGINE (the demo server); MEASURED in E3 if the demo is in scope |
| BUD-3.9 pipeline wall clock | MEASURED: the gate runner's total time |

### 6.13 Laws and errata (333) and the missing laws 9 and 10

- `laws_and_errata.json` holds 333 rows; 89 are marked `N/A-ENGINE:` in their criterion
  (`docs\requirements\errata-laws.md:21`). Those keep N/A-ENGINE.
- LAW-1 .. LAW-8 map to gates: LAW-1 G-PROVENANCE plus the object gate; LAW-2 the arena lock check and
  G-NOALLOC-10K; LAW-3 a scan for weight and dataset files and network imports (in az_provenance.py); LAW-4
  G-STRICT-LINT and E1 (no INERT); LAW-5 G-BOOKS-10K; LAW-6 the determinism matrices (now read with the
  wild-default amendment); LAW-7 a wording scan of docs and code for "quantum randomness", "speed-up",
  "conscious", "sentient" used as claims (REVIEWED where a hit needs judgement); LAW-8 REVIEWED by SB-29.
- The set holds laws 1-8 only; COMMON.md has 10 (`docs\agents\COMMON.md:35-41`). Add two supplementary
  TRACE rows, `COMMON-LAW-9` (Windows only, from Z:) and `COMMON-LAW-10` (below assembly, inside Windows),
  without editing the harvested file. Law 9 maps to a check that every build and log path is under the
  root; law 10 is REVIEWED.
- Every other row (errata A-J, K1-K3, CAW, NC, ALF, MD, AG, BUD, ADR): read its derived criterion (DO, AVOID,
  TEST). TEST becomes a GATE or a TESTED item; DO and AVOID become a lint rule, a gate, or a REVIEWED row with
  the A1 record id. "Correct as written: do not fix" rows (CAW) are REVIEWED with "no action".
- Known defects of the source: the closure report's title and evidence columns describe different topics in
  many rows; 15 errata are cited by no closure row (`errata-laws.md:38-42`). Derive from the evidence column
  and say so in `notes`.

### 6.14 The framework's 825-check scoreboard

| Suite | Rows | Native disposition |
|---|---|---|
| L1_BUTLIN14 | 14 | DUPLICATE-OF butlin14 rows |
| L2_FRAMEWORKS10 | 10 | DUPLICATE-OF consciousness10 rows (F1..F10 one to one) |
| L3_CQM25 | 25 | DUPLICATE-OF cqm25 axes by position |
| L4_FREAK100 | 100 | IDEA-BANK |
| L5_LIFE500 | 500 | SELF: these are the rows of checklist500.json |
| L6_NOVEL100 | 100 | IDEA-BANK |
| L7_WSE12 | 12 | LAW-CONFLICT law 4 in a reported world; NEEDS-OWNER D-SB-6 for a lab tool |
| L8_SOFT40 | 40 | NOT-BUILT (no soft-body continuum); rows that name WASM or XPBD solvers N/A-ENGINE |
| L9_HW24 | 24 | per row: a GATE where a native gate checks the same thing (HW01 SoA arena: the object gate and the lock; HW18: MIRROR-OF G-BOOKS-10K), N/A-ENGINE for TypedArray, WGSL or WASM rows |

Output: `docs\eval\SCOREBOARD-825.md`, 825 rows, counts per suite and disposition, and the unique count (820
when the 5 mirrors count once). Never a percentage.

### 6.15 Idea banks and historical sets (trace only, never scored)

| Catalog | Rows | Disposition |
|---|---|---|
| bench_freak100.json | 100 | IDEA-BANK |
| bench_novel100.json | 100 | IDEA-BANK |
| bench_foundations.json | 99 (F49 is missing in the source, `passes-bench.md:29`) | IDEA-BANK |
| cube1000.json | 1000 | IDEA-BANK |
| wishlist.json | 512 | IDEA-BANK |
| nested_precision.json | 134 | IDEA-BANK |
| frontier_upgrade_map.json | 272 | IDEA-BANK |
| thousand_reports.json, thousand_reports_index.json | 150, 900 | IDEA-BANK |
| apex25.json | 25 | N/A-ENGINE (a v40 synthesis suite; its 25 cannot replace the CQM 25, `Asmodensius-comparative-review.md:74`) |
| cqm_scrutiny25.json | 25 | REVIEWED: the SB-29 reviewer may score the 25 audit points for the native repo, labelled as an audit, never pass/fail |
| cqm25_rules.json | 19 | GATE or REVIEWED: RULE-4 and RULE-6 are enforced by az_trace.py --check (TRACE has no score column and the scorecards no totals); the protocol clauses PROT-01..12 map to sections 3 and 4 |
| antilarp13_v40impl.json, passes100_v40.json, life100_v40_500.json, bench_life100.json | 13, 100, 500, 500 | SUPERSEDED-BY or DUPLICATE-OF the canonical rows (sections 5.1, 5.5, 6.11) |

An idea-bank row may carry `notes = implemented-as <native mechanism>` when one exists; that is a pointer,
not a score.

"Trace only, never scored" follows the plan (`docs\handoff\CONTINUE-BUILD.md:57`: "trace the idea banks
without scoring them"), but the owner's P06 ends with "and more things I forgot", which these 3,267
rows (the IDEA-BANK rows above) may be. So it is an owner decision with a default: 10 OD-25 (default:
trace only; asked in the second report, `00-MASTER-PROMPT.md` 7.4). If he picks items to score, each
picked item moves to the class it fits (TESTED or REVIEWED) with its own spec in SB-P1 and its own
prereg; the rest stay IDEA-BANK.

### 6.16 A1, R1 and the interim report

- A1 (milestone 7) and milestone 8 share the five mutants and the review discipline; A1 must be clean before
  any reporting run (`CONTINUE-BUILD.md:54-55`).
- The interim report for the owner (`CONTINUE-BUILD.md:60`) is written when the scorecards exist: what ran,
  the counts per suite, every NOT-BUILT with what would build it, the owner decisions of section 13.
- R1 (`docs\handoff\FINAL_REPORT.md`) cites the scorecards by run id; it never restates a verdict without
  its run id and command.

## 7. Order of work

The binding order for reporting runs is `CONTINUE-BUILD.md:57`: the engineering gates and the 5 mutants;
E1, then E3; CQM-25, then Butlin 14; the 10 theories (their rollup and ctm depend on Butlin); the 100
passes; the 500 checklist (in the background, logs on Z:); the rollups and trace-only sets; T-AUDIT last.
Building the tools does not have to wait; reporting does.

| Phase | Starts when | Tasks |
|---|---|---|
| P0, tools and trace | now, in parallel with milestones 5 and 6, touching only new files under `tools\`, `tests\`, `docs\eval\`, `docs\prereg\`, `build\work\eval\` | SB-00, SB-01, SB-02, SB-03 (first pass), SB-05, SB-06 (runner without probes), SB-09, SB-10, SB-11, SB-12 (the gates that need no stub: G-BOOKS-10K, G-OBSERVER-10K, G-SEEDLIT, G-PROVENANCE, G-TRACE), SB-14 |
| P1, ABI batch and probes | sense, physics and stats are real and strict lint passes (end of milestone 5) | SB-04 (lead only, one change, new reference hashes recorded), SB-07, SB-08, SB-03 (final), tuning specs for every suite on seeds 1-99 |
| P2, freeze and report | A1 is clean (milestone 7), the 5 mutants are caught (SB-13), the engine manifest is frozen (SB-09) | lock and run in the binding order: SB-12 (all gates), SB-13, SB-15, SB-16, SB-17, SB-18, SB-19, SB-20, SB-21; then SB-22 to SB-27 (trace-only); SB-11 over everything; SB-28 (wild block) |
| P3, review and close | every suite has a scorecard | SB-29 (independent reproduce-first review), SB-30 (final scorecards, interim report, input to R1) |

Any engine change after the freeze (a fix found by a suite) ends P2 for every item it can affect: re-freeze,
record the new manifest, and treat every already-reported item it affects as BURNED (section 3.10).

## 8. Run counts and machine time

Measured constant: the post-worldgen lesion run did 528 world runs (3000 founders, 64 ticks, grid 4,4,3, 8
species, 8 workers, C reference) in 88.2 s, so 0.167 s per run (`04-GOALS-AND-BENCHMARKS.md:118`). Planning
rule: `t_run(T) = 0.167 s x T / 64` for T ticks. It is a lower bound: populations grow during a run (3000
founders became about 6,500 live by tick 64 in that table), world creation is included, and the machine was
idle. Re-measure in SB-14 at the real probe configs before you trust any line below.

Run count per suite: `R = S x (C + sum over items of A_i)`, with S seeds (48), C distinct intact
configurations (one shared intact run per seed and config) and A_i the non-intact arms of item i.

| Suite | Assumptions (to be replaced by the preregs) | Runs | Time at 8 workers |
|---|---|---|---|
| E1 | 22 runs per seed, 64 ticks, 2 blocks of 24 | 1,056 | about 3 min |
| Butlin 14 | all 14 run (upper bound), 3 arms each, one intact config, 512 ticks | 48 x 43 = 2,064 | about 46 min |
| 10 theories | 9 run (upper bound), 3 arms, 512 ticks | 48 x 28 = 1,344 | about 30 min |
| CQM-25 | 19 short items x 3 arms at 512 ticks, plus 2 long items (01, 02) x 3 arms at 16,384 ticks | 2,784 + 336 | about 62 min + 4.0 h |
| 100 passes | 100 items x 2 arms, 10 intact configs (one per wave), 512 ticks | 48 x 210 = 10,080 | about 3.75 h |
| LIFE-500 | 500 items x 2.2 arms, 100 intact configs (one per area), 256 or 1,024 ticks | 48 x 1,200 = 57,600 | 10.7 h (256 ticks) to 42.7 h (1,024 ticks) |
| E3 | 8 populations x 3 thread settings x 2 kernels x 3 repeats | 144 | measure; 1M runs dominate |
| Total reporting (without E3 and tuning) | | about 75,000 | about 21 h to 53 h |

- Tuning costs at least as much again (seeds 1-99, many looks).
- `CONTINUE-BUILD.md:57` estimates about 10^5 world runs for the 500 alone; the table above gives 57,600
  with shared intact runs, which is the same order. Use whichever the measured preregs give.
- `--jobs J` (several worlds at once) may shorten this: 3,000-founder worlds do not fill 8 workers well.
  Measure J = 1, 3, 8 with K = 8, 3, 1 workers in SB-14 and record the speed-up; check that J arenas lock.
- Never let one tool call block for more than 4 minutes; start long runs in the background with logs on Z:
  and poll with short calls (`CONTINUE-BUILD.md:77`).
- Suite runs never render pictures, so the demo's picture rate does not slow them.

## 9. Honesty rules

1. Indicator-only: Butlin 14, the 10 theories and CQM axes 13 and 19-22 are indicators of mechanisms.
   Every scorecard of theirs starts with the banner: "Indicator-only scorecard. A result here says whether a
   mechanism in the engine is causally necessary for a measured behaviour. It is not evidence of
   consciousness, sentience, experience or life." (`CONTINUE-BUILD.md:58`; `butlin.md:3`.)
2. No aggregate score of any kind: no life, consciousness, sentience, AGI or superintelligence score
   (`cqm25_rules.json` RULE-6), no numeric capability score per axis until a calibrated protocol with
   independent raters exists (RULE-4), no sum across suites, no percentage, no grade. Counts per verdict per
   suite are allowed; they are not a score.
3. Never tune on reporting seeds; never look twice; never pick seeds (section 3.10).
4. A NOT-BUILT is a result, not a failure to hide. Never build a mechanism only to pass an indicator, and
   never re-word an item to fit the engine; re-specify only where a law forces it (LAW-CONFLICT), and keep
   the original row with its reason.
5. A NO EFFECT, INCONCLUSIVE or REVERSED result is reported as it is, with its numbers. It is never turned
   into a zero score or dropped (`Asmodensius-comparative-review.md:168`).
6. Historical results (v40 R21, SC-PORT public and local, framework PASS labels, APEX 25/25, 100/100) stay in
   `historical_label` with their provenance. They are never native evidence.
7. Quantum words: tk_qsv and tk_qgt are classical simulations; never "quantum randomness" or "quantum
   speed-up" (law 8 of `00-PROMPT.md`; errata D-10 and K1-21 in `errata-laws.md:33-34`).
8. Holm, not FDR: never call the Holm procedure "FDR" (errata C-01).
9. Novelty: no claim that a result is new without a prior-art search with its scope and date stated
   (`CONTINUE-BUILD.md:27`).
10. Every number in a report has a run id, the command and the raw file; "unknown" is acceptable.
11. A green suite is not done until the independent review of SB-29 agrees (law 9).

## 10. The final scorecard and the definition of total completion

### 10.1 One scorecard per suite

`docs\eval\SCORECARD-<SUITE>.md` and `.csv`, written by az_suite into the run folder and copied after
T-AUDIT. Header lines:

```
Suite: CQM25            Class banner: (section 9 rule 1 where it applies)
Prereg: docs\prereg\PREREG-CQM25.txt  SHA-256 <64 hex>
Engine manifest: docs\eval\ENGINE-MANIFEST-<date>.txt  SHA-256 <64 hex>
Seeds: 5001-5024, 5101-5124 (48); n_min 40; Holm family m = <k>
Run id: RUN-CQM25-<yyyymmddThhmm>-<8 hex>; raw: build\work\eval\<run id>\
Machine: 24 logical processors; workers <K>; jobs <J>; arena locked; load: <label from env.txt>
T-AUDIT: PASS (tools\az_audit.py, <date>)
```

Table, one row per item (CSV with the same columns):

`item | catalog ids | class | disposition | verdict | measure (unit, bounds) | direction | n usable | intact
mean | per arm: mean, d_z, 95% interval, p | IUT p | Holm p | ceiling check | notes`

Footer: counts per verdict (EFFECT, NO EFFECT, INCONCLUSIVE, INCONCLUSIVE-N, REVERSED, FAIL-CEILING,
FAIL-INVARIANT, BURNED) and per reason code, and the sentence "Counts are not a score; see section 9."

### 10.2 The roll-up

`docs\eval\SCORECARD-ALL.md`: one row per suite and per traced catalog with its counts, the run ids and
links; the E1 tables; the five mutants; the gates; BENCH-SCALE; the 825 scoreboard. No total line.

### 10.3 Total completion of the suites (all must hold; each can fail)

1. `python -I "$R\tools\az_trace.py" --check` exits 0: every id of every catalog in `docs\requirements\` (30
   catalogs) and the SB-02 extractions has exactly one TRACE row per catalog with a disposition from the
   closed set, and the row counts equal the catalog counts (for example 14, 10, 25, 25, 19, 25, 100, 100,
   100, 100, 500, 500, 500, 109, 13, 13, 5, 9, 20, 333 plus 2, 25, the 825, 100, 100, 99, 1000, 512, 134,
   272, 150, 900).
2. Every TESTED row has a locked prereg (SHA-256 in `PINS.txt`), exactly one reporting run per block in
   `RUNS.csv`, a verdict from section 3.5 and raw files under `build\work\eval\`.
3. `python -I "$R\tools\az_audit.py" --all "$R\build\work\eval"` exits 0.
4. E1: both tables exist and no switch is INERT.
5. The five mutants are each CAUGHT by their named detector.
6. `python -I "$R\tools\az_gate.py"` exits 0 on the frozen tree (every engineering gate PASS).
7. `docs\eval\BENCH-SCALE.md` and the M2 table exist.
8. Every NOT-BUILT row names the missing mechanism and what would build it; every NEEDS-OWNER row names its
   decision id and appears in the interim report.
9. SB-29's independent review re-ran at least 10 percent of the TESTED items (picked by a seeded draw that
   the reviewer records), every EFFECT in the indicator suites, every REVERSED and every FAIL-*, and agrees;
   every disagreement is resolved in writing.
10. The interim report is written and `build\work\PROGRESS.md` has the entry with every run id.

### 10.4 What completion is not

It is not "everything passes", not a percentage, not a score, and not a claim about consciousness or life.
An honest NOT-BUILT, NO EFFECT or INCONCLUSIVE is a completed row.

## 11. Holes, blind spots and dilemmas, and how this protocol closes each

| # | Hole or dilemma | Where seen | How it is closed |
|---|---|---|---|
| 1 | az_lesion defaults to the reporting block, so one careless run spends it | `tools\az_lesion.c:205` | SB-08: reporting seeds only with `--prereg`; RUNS.csv one-look check |
| 2 | Energy as the primary endpoint hides real causal effects (GRAVITY, SENSES, TIER1_Q change 24/24 states, d_z near 0) | `Asmodensius-comparative-review.md:143` | direct endpoints per switch, chosen before the lock (section 6.3) |
| 3 | A counter-incrementing path tests reachability, not benefit (SOCIAL removes the interactions it counts) | `Asmodensius-comparative-review.md:118,140` | CQM 14 items measure a benefit or a strategy change, with sham and shuffle arms |
| 4 | Signed effects misread (BRAIN off raises mean energy) | `Asmodensius-comparative-review.md:142` | preregistered direction and the REVERSED verdict |
| 5 | The bootstrap interval is of the mean difference, not of d_z | `tools\az_statsutil.h:327`; review line 151 | `azs_boot_ci_dz` (SB-05) |
| 6 | No sham, yoked, shuffled or random-policy arms exist | `04-GOALS-AND-BENCHMARKS.md:72-76` | SB-04 arm switches |
| 7 | Tasks need interventions, but tools only observe; an intervention could break the books | `tools\az_lesion.c:42-44` | the conserving, logged probe API (section 4.3) |
| 8 | Harness tests measure the fixture, not the creature (why the TypeScript refactor began) | `Asmodensius-comparative-review.md:64` | probes act inside the world on real creatures; no external model computes the behaviour |
| 9 | Founders start clustered since worldgen, so interaction-heavy measures shift | `04-GOALS-AND-BENCHMARKS.md:212` | probes place focal regions explicitly; configs are fixed in the prereg |
| 10 | The 1M world hits store refusals (380,865) and reaches generation 1 only | `Asmodensius-comparative-review.md:132` | no evolutionary claim from the 1M world; long-horizon items use small worlds; E3 reports refusals |
| 11 | Uniform-world fixtures in unit tests isolate mechanisms but say nothing about the full world | `Asmodensius-comparative-review.md:92` | every suite item runs on worldgen worlds |
| 12 | The wild default and the paired design pull in opposite directions | `core\az_abi.h:843-855` | lab for the tests, a descriptive wild block, taped wild only with D-SB-1 |
| 13 | A terminal state hash cannot tell which step changed or whether trajectories converged | `Asmodensius-comparative-review.md:136` | `--hash-every` traces in tuning; measures read at several ticks |
| 14 | Per-item lesions beyond 21 bits need an ABI change | `CONTINUE-BUILD.md:57` | SB-04 names `lesion_ext` bits (64 free) |
| 15 | Strict lint fails (unread traits, GRAVITY not honoured by physics, untested lesions) | `build\work\PROGRESS.md:1610` | G-STRICT-LINT must pass before P1 ends |
| 16 | Unit tests use seeds 201-208 and 211-214, outside 1-99 | `Asmodensius-comparative-review.md:152` | A1 finding; G-SEEDLIT lists or forbids them |
| 17 | The three 100s and the three CQM wordings invite joins by row number | `passes-bench.md:10`; `cqm25.md:24-26` | joins by code and by axis position with drift notes (section 5) |
| 18 | Two archives of the TypeScript lab disagree on AE1 | `Asmodensius-comparative-review.md:73` | separate provenance; neither is native evidence |
| 19 | Five framework suites and several lists were never extracted | `checklists_index.json` | SB-02 |
| 20 | Many indicator items have no mechanism; the temptation is to bolt one on | section 6.6 | NOT-BUILT and owner decision D-SB-3; honesty rule 4 |
| 21 | Statistical tooling itself can be wrong | | T-AUDIT in another language, planted defects in test_suite |
| 22 | A fix found during reporting changes the engine mid-suite | | re-freeze and burn (section 7) |
| 23 | Machine time is planned from one measurement | section 8 | SB-14 re-measures before any lock |
| 24 | Many parallel locked arenas may exceed the working-set quota | | SB-14 tests J arenas; a failed lock is a failed run, never pageable |
| 25 | The 34 DELARP qualities and two R variants have no criterion or original | `passes-bench.md:27` | mapping by quality_id; NEEDS-OWNER D-SB-5 for the rest |
| 26 | GATE-01 cannot pass without a commit, which needs the owner | `CONTINUE-BUILD.md:86` | NEEDS-OWNER D1(a); the seed half is tested anyway |
| 27 | The demo's picture rate could be mistaken for engine speed | the owner's complaint (P51) | E3 and CQM 25 report ticks per second and pictures per second as separate rows |

## 12. Task list SB-00 to SB-30

Effort: S under half a day, M one to two days, L three to five days, XL more than a week of agent time
(machine time in section 8 comes on top). Every task ends with a `build\work\PROGRESS.md` entry: what
changed, the commands with output tails, decisions with reasons, what is not verified, and the `Next:` line.
Every new text file passes `check_text.py` (read its usage first). Before every task that edits shared
files, take a snapshot: `python -I "$R\build\work\scripts\snapshot.py" <label>`.

**SB-00 Re-verify the ground truth** (S; deps none)
- Inputs: section 1 of this file; `04-GOALS-AND-BENCHMARKS.md`; `build\work\PROGRESS.md` (last entry).
- Do: check each section 1 row by reading the cited lines; `Test-Path -LiteralPath` for `docs\prereg`,
  `docs\eval`, `tools\az_suite.c`; build and ctest per `00-MASTER-PROMPT.md` 9.1 step 6 (the old
  first-hour list of `00-PROMPT.md` section 11 was superseded on 2026-11-18, erratum E-02).
- DoD: a PROGRESS.md entry that lists every section 1 fact as confirmed or changed (with the new value and
  cause). Fails if any fact is left unchecked.
- Risk: another agent (Codex) works in the same folders; check file times first (`CONTINUE-BUILD.md:35`).

**SB-01 Copy the external inputs to Z:** (S; deps none)
- Inputs: the three GPT Astra outputs (section 5.8; written on 2026-11-18 by the GPT Astra sessions in the
  Codex desktop app). This folder is the one copy destination; 09 EVD-14 and 10 DL-09 point here.
- Files: `build\work\eval\inputs\astra-2026-11-18\` (the three files) and `MANIFEST.txt` (name, bytes,
  SHA-256).
- Commands: `[void][IO.Directory]::CreateDirectory("$R\build\work\eval\inputs\astra-2026-11-18")`;
  `Copy-Item -LiteralPath '<C: file>' -Destination "$R\build\work\eval\inputs\astra-2026-11-18\"`;
  `(Get-FileHash -LiteralPath '<copy>' -Algorithm SHA256).Hash`.
- DoD: three copies whose SHA-256 equals the originals'; byte counts 53,102, 846,147 and 19,051.

**SB-02 Extract the unextracted parts of the 825** (M; deps SB-01)
- Inputs: `Z:\[Vibe Coded (AI)]\CLAUDECODE\ALife-Framework-v3.0-Master-Checklists.csv` (lines 51-150, 651-926),
  `...\ALife-Framework-v3.0-Master-Spec.json`, `...\ALife-v3.0-Errata-Closure-Register.csv`, the workbook copy.
- Files: `build\work\eval\scripts\extract_825.py`; `docs\requirements\framework825_freak100.json`,
  `framework825_novel100.json`, `framework825_wse12.json`, `framework825_soft40.json`,
  `framework825_hw24.json`, `framework825_milestones100.json`, `framework825_mirrors5.json`,
  `framework825_citations15.json`, `framework825_errata95.json` (keys id, text, category, criterion,
  source_ref as in the other sets); `docs\requirements\framework825.md` (what was extracted, counts, checks).
- Command: `python -I "$R\build\work\eval\scripts\extract_825.py"` (writes its own files; no redirection).
- DoD: counts 100, 100, 12, 40, 24, 100, 5, 15, 95; ids unique; the id set of each equals the workbook's
  "Framework 825" or "Framework mirrors" sheet; the script exits non-zero on any mismatch. Source hashes
  recorded (the master CSV's SHA-256 begins `80fbe1623f5997ce` per `checklist500.md:49`; a different value
  means the source changed: stop and record).
- Risk: CSV cells with embedded newlines; parse with the `csv` module.

**SB-03 Mechanism inventory** (L; first pass deps none, final pass deps M5 done)
- Inputs: `core\*.c`, `core\az_abi.h`, `core\az_owners.def`, the W2 briefs, the item lists of sections 6.5-6.9
  and Appendix C.
- Files: `docs\eval\MECHANISMS.csv` (columns: catalog, item, mechanism file:function, lesion switch or
  "none", stub dependencies, evidence lines, proposed disposition, proposed probe) and `docs\eval\MECHANISMS.md`.
- Do: for each Butlin, theory, CQM item, each pass target item and each LIFE sub-check, search the code with
  at least three terms (for example "workspace", "broadcast", "global"), read the hits, decide.
- DoD: every row of those catalogs has a MECHANISMS line; every "none" lists the search terms used; a second
  reader (SB-29, or a checker agent) samples 20 lines and agrees.

**SB-04 ABI batch for milestone 8 (lead only)** (XL; deps M5 done, strict lint clean)
- Files: `core\az_abi.h` (names for `lesion_ext` bits, probe declarations, `AZ_EV_PROBE`), `core\az_owners.def`,
  `core\params\*.def` (the `SUITE_SHAM_OF` parameter, with a default and a range), `core\az_probe.c`,
  `CMakeLists.txt` (register az_probe.c and the new tests), the honouring modules (`core\az_brain.c`,
  `core\az_tier1.c`, `core\az_social.c`, `core\az_life.c`, `core\az_mate.c`, `core\az_econ.c`, `core\az_tick.c`
  as the switches require), their tests, `tests\test_probe.c`, the az_lesion table rows.
- Do: one change, then build, full ctest, strict lint, the reference worlds; record the new hashes and the
  cause in `tests\golden_hashes.txt` ("ABI 4 M8 batch: arm switches and probe API").
- DoD: every new switch honoured and tested (each lesioned run differs from intact in a measured outcome on
  tuning seeds); probe API books exact; equal hashes across 1, 8, 24 workers; strict lint 0; ABI notice
  `docs\agents\ABI-V4-NOTICE.md` extended with the batch.
- Risk: hash change; schedule-dependence of SHUFFLE arms (use previous-tick values only).

**SB-05 Statistics additions** (M; deps none)
- Files: `tools\az_statsutil.h`, `tools\az_sha256.h`, `tests\test_statsutil.c`.
- Do: section 4.2 functions; SHA-256 with the FIPS 180-4 test vectors ("abc", the empty string, the
  448-bit message).
- Command: build, then `ctest --test-dir "$R\build\z" --output-on-failure -R test_statsutil`.
- DoD: all new checks pass; each planted defect (one per function: off-by-one in the bootstrap index, a
  wrong sign in d_z, a skipped Holm monotone step, a verdict row reordered, a SHA-256 round constant changed)
  fails the test; the record lists which check caught each.

**SB-06 tools\az_suite.c, the runner** (XL; deps SB-05)
- Files: `tools\az_suite.c`, `tests\test_suite.c`; `CMakeLists.txt` registration by the lead.
- Do: section 4.5; spec parser (Appendix A); arms resolved to switches; jobs; outputs; exit codes; verdicts via
  `azs_verdict`; scorecard writer. Probes come in SB-07; until then only the measures of az_lesion.
- DoD: test_suite passes and fails on each planted defect of section 4.11; a tuning spec that re-states the
  21 E1 rows with INTACT and TARGET reproduces `build\work\logs\lesion_z_worldgen_1_24.txt` exactly on seeds
  1-24 (same means, d_z and Holm p to the printed precision).

**SB-07 Probes and measures** (XL; deps SB-04, SB-06)
- Files: `tools\az_probe_tool.h`, `tools\az_probe_tool.c`, `docs\eval\PROBES.md`, `docs\eval\MEASURES.md`,
  checks in `tests\test_suite.c`.
- Do: every probe of section 6 and every measure named in sections 6.5-6.7; each measure unit-tested on a
  hand-made world.
- DoD: each probe runs on tuning seeds with exact books; each measure's test fails when the measure is
  changed by one step; the clean tree gives the expected tuning results for the M-FROZEN and M-SHUFFLED
  detectors (needed by SB-13).

**SB-08 Seed custody, preregistration folder, az_lesion upgrade** (M; deps SB-05, SB-06)
- Files: `docs\prereg\README.md` (sections 3.10-3.11 in short), `docs\prereg\PINS.txt`, `docs\prereg\RUNS.csv`,
  `docs\prereg\BURNED.csv` (headers only), `tools\az_lesion.c` (`--prereg`, refusal of reporting seeds
  without it, `azs_verdict`, new endpoints), `tests\test_statsutil.c` (table check still passes).
- DoD: az_lesion without `--seeds` and without `--prereg` exits 2 instead of using 5101-5124; with a wrong pin
  exits 4; a second look exits 5; all shown by test runs on scratch copies of the custody files.
- Risk: the lesion ctest and the docs that quote the old default must be updated in the same change.

**SB-09 Engine manifest and freeze** (S; deps none)
- Files: `tools\az_manifest.py`; outputs `docs\eval\ENGINE-MANIFEST-<date>.txt` (sorted relative path and
  SHA-256 of every file under `core\`, `kernels\`, `platform\`, `tools\`, `gpu\`, `CMakeLists.txt`) and its own
  SHA-256.
- DoD: two runs on an unchanged tree give the same SHA-256; touching one byte of one core file changes it.

**SB-10 The trace matrix** (L; deps SB-02, SB-03 first pass)
- Files: `tools\az_trace.py`, `docs\eval\dispositions.csv`, `docs\eval\TRACE.csv`, `docs\eval\TRACE.md`.
- Do: section 4.7; the joins of section 5; `--check` and `--check-workbook`.
- DoD: `--check` exits 0 with every catalog's count matched; `--check-workbook` reports 0 differences for
  the 30 catalogs; planted errors (a missing id, a duplicate, a dangling DUPLICATE-OF, an unknown code) each
  make `--check` fail.

**SB-11 T-AUDIT** (M; deps SB-06)
- Files: `tools\az_audit.py`.
- DoD: on the SB-06 reproduction run it agrees with az_suite on every p, d_z and verdict; a hand-edited copy
  of runs.csv (one measure changed) makes it fail.

**SB-12 Gate runner and new gates** (M; deps SB-09, SB-10; the stats-dependent gates after P1)
- Files: `tools\az_gate.py`, `tools\az_seedlit.py`, `tools\az_provenance.py`, `tests\cli\observer_10k.py`;
  ctest registration by the lead.
- DoD: each gate of section 6.1 runs, passes on the clean tree and fails on a planted defect; the runner
  stops at the first failure with a non-zero exit and a log path.

**SB-13 The five mutants** (L; deps SB-07, A1 scope agreed)
- Files: `build\work\eval\mutants\<name>\` (tree, build, logs), `build\work\eval\mutants\make_mutants.py`,
  `docs\eval\MUTANTS.md`.
- DoD: section 6.2 table filled with each detector's output on clean and mutant trees; 5 of 5 caught.

**SB-14 Calibration of machine time** (S; deps SB-06)
- Do: time one intact run per planned config (founders, ticks, grid) on seeds 1-3; time J = 1, 3, 8 jobs;
  record the load.
- Files: `docs\eval\MACHINE-TIME.md`.
- DoD: section 8's table recomputed from measured constants; every arena locked.

**SB-15 E1 on the reporting blocks** (M; deps P2 gate, SB-08)
- Files: `docs\prereg\PREREG-E1.txt`, `docs\eval\E1-LESIONS.md`.
- Command (after the lock): `& "$R\build\z\az_lesion.exe" --prereg "$R\docs\prereg\PREREG-E1.txt" --csv
  "$R\build\work\eval\<run id>\e1_5101.csv"` and the replication block as the prereg names it.
- DoD: section 6.3.

**SB-16 E3 and the link to M2** (L; deps SB-12 page-fault column, M2 from milestone 6)
- Files: `docs\eval\BENCH-SCALE.md`, raw outputs in `build\work\eval\E3-<date>\`.
- DoD: section 6.4; every row labelled with load and, above the knee measured by ES-1 (until then above
  about 35,000 creatures, "beyond L2+L3 by estimate"), "DRAM-bound"; every row with its store refusals.

**SB-17 CQM-25** (XL; deps P2 gate, SB-07)
- Files: `docs\eval\tuning\cqm25.spec`, `docs\prereg\PREREG-CQM25.txt`, `docs\eval\SCORECARD-CQM25.md/.csv`.
- Command (reporting): `& "$R\build\z\az_suite.exe" --prereg "$R\docs\prereg\PREREG-CQM25.txt" --out
  "$R\build\work\eval\<run id>" --jobs <J> --threads <K>`.
- DoD: every axis row of section 6.5 has a verdict, a gate result, a measurement or a reason code; T-AUDIT
  PASS; TRACE updated for cqm25, cqm25_framework and the benchmarks100 CQM rows.

**SB-18 Butlin 14** (L; deps SB-17 done, as the order says)
- Files: `docs\eval\tuning\butlin14.spec`, `docs\prereg\PREREG-BUTLIN14.txt`, `docs\eval\SCORECARD-BUTLIN14.md/.csv`.
- DoD: section 6.6; banner; T-AUDIT PASS; mirrors credited (CQM 13, 19, 20).

**SB-19 The 10 theories** (L; deps SB-18)
- Files: `docs\eval\tuning\theories10.spec`, `docs\prereg\PREREG-THEORIES10.txt`,
  `docs\eval\SCORECARD-THEORIES10.md/.csv`.
- DoD: section 6.7; the rollup computed from SB-18; T-AUDIT PASS.

**SB-20 The 100 passes** (XL; deps SB-10, SB-19)
- Files: `docs\eval\tuning\passes100.spec`, `docs\prereg\PREREG-PASSES100.txt`, `docs\eval\PASSES-JOIN.md` (the
  code join with its counts), `docs\eval\SCORECARD-PASSES100.md/.csv`.
- DoD: the join counts recomputed and recorded; every ledger and suite row traced; T-AUDIT PASS.

**SB-21 The 100 areas and the 500** (XL; deps SB-20; runs for days in the background)
- Files: `docs\eval\tuning\life500.spec`, `docs\prereg\PREREG-LIFE500.txt`, `docs\eval\SCORECARD-LIFE500.md/.csv`,
  `docs\eval\LIFE-AREAS.md` (each area's five verdicts, no sixth test).
- DoD: section 6.9; the 48-seed deviation written in the prereg; T-AUDIT PASS.

**SB-22 benchmarks100** (M; deps SB-17 to SB-19) - DoD: every one of the 109 rows traced per section 5.6.

**SB-23 The 13 release gates** (M; deps SB-12, SB-13) - DoD: section 6.11 table with gate results or codes.

**SB-24 gates9 and budgets** (S; deps SB-12) - DoD: section 6.12 table in TRACE.

**SB-25 Laws and errata** (L; deps SB-12, A1 records) - DoD: section 6.13; 333 rows plus COMMON-LAW-9 and
COMMON-LAW-10 traced.

**SB-26 The 825 scoreboard** (M; deps SB-02, SB-17 to SB-21) - Files: `docs\eval\SCOREBOARD-825.md`. DoD:
section 6.14; unique count stated.

**SB-27 Idea banks and historical sets** (M; deps SB-10) - DoD: section 6.15; every row traced, none scored.

**SB-28 Wild sensitivity blocks** (M; deps each suite's reporting run) - Files: `docs\eval\WILD-<SUITE>.md`.
DoD: section 3.12; labelled descriptive; no Holm.

**SB-29 Independent reproduce-first review** (L; deps every scorecard)
- Who: an agent other than the one that ran the suites, or a fresh session with no access to the first
  agent's reasoning.
- Do: section 10.3 item 9; also check five random TRACE rows per catalog against their sources, re-derive one
  verdict per suite by hand from runs.csv, and try to refute each EFFECT (look for a confound the arms miss).
- Files: `docs\eval\REVIEW-<date>.md`.
- DoD: agreement or written resolution of every disagreement; findings fixed or answered.

**SB-30 Final scorecards and the interim report** (M; deps SB-29)
- Files: `docs\eval\SCORECARD-ALL.md`, the interim report in `build\work\PROGRESS.md` and in
  `docs\eval\INTERIM-REPORT.md`; inputs for R1.
- DoD: section 10.3 holds in full; the report lists every owner decision of section 13 with its default.

## 13. Decisions that belong to the owner

Take the default, record it in PROGRESS.md, ask in the interim report.

| Id | Decision | Default until he answers |
|---|---|---|
| D-SB-1 | May a recorded entropy tape be fed back into wild runs, to pair them? | no; the wild block is descriptive |
| D-SB-2 | Fresh reporting blocks after a burn | none; burned items stay BURNED; propose 5201-5224 and 5301-5324 |
| D-SB-3 | Build explicit cognitive architecture (workspace, broadcast, confidence monitor, forward model, efference copy), or test only emergent properties of the evolved brain | test emergent properties; NOT-BUILT where nothing isolates the property |
| D-SB-4 | A coverage threshold for the core | report only |
| D-SB-5 | Criteria for the 34 DELARP qualities without a mapped row, and for K1R | NEEDS-OWNER rows, traced, not tested |
| D-SB-6 | A lab-only weight-editing tool for WSE-12 and GATE-09 | none; LAW-CONFLICT law 4 |
| D1(a) | The first git commit (GATE-01) | no commit; GATE-01 blocked (`CONTINUE-BUILD.md:85-86`) |
| D2 / law 2 | Phase-2 wording (GPU whole tick, DDR5 tier) | phase 1 holds; LIFE-L052-A2 NOT-BUILT |
| 64 seeds | 64 paired seeds for the 500 as the framework says | 48 with the deviation recorded (`CONTINUE-BUILD.md:87`) |
| Quiet window | A quiet-machine window for E3 and M2 | measure and label the load |
| CAP-O | Publishing the code | not published |
| OD-25 (10) | The idea-bank catalogs of section 6.15 (3,267 rows): trace only, or select items to score | trace only (IDEA-BANK); asked in the second report, not only in the interim report |

## Appendix A. Item spec grammar

ASCII, LF, no tabs. `#` starts a comment line. Sections in square brackets. One `key = value` per line;
lists are comma-separated; whitespace around `=` and commas is ignored; unknown keys are an error (exit 6).

```
[suite]
id            = CQM25                      # E1 | CQM25 | BUTLIN14 | THEORIES10 | PASSES100 | LIFE500 | <id>-ADD<n>
title         = CQM-25 capability law, native reporting run
holm_family   = CQM25
seed_blocks   = 5001-5024, 5101-5124       # tuning specs: a range inside 1-99
n_min         = 40
engine_manifest_sha256 = <64 hex>
az_suite_version = <text printed by az_suite --version>
deviations    = <free text, one line>
disclosures   = <free text, one line>

[item CQM-07]
catalog_ids   = cqm25:learning, cqm25_framework:CQM07, benchmarks100:CQM07
class         = CAPABILITY                 # CAPABILITY | INDICATOR | ENGINEERING
mechanism     = core\az_tier1.c:az_tier1_range
probe         = P-REVERSAL
probe_args    = patch_matter=<int>, swap_tick=<int>, window=<int>, focal=<int>
n             = 3000
ticks         = 512
grid          = 4,4,3
species       = 8
cap           = 8192
store_units   = 0                          # 0 = the engine default
tier1_period  = 32
tier2_period  = 256
field_stride  = 8
reorder       = 16
measure       = M-INTAKE-AFTER-SWAP        # an id from docs\eval\MEASURES.md
bounds        = none                       # none | <min>,<max>
direction     = intact_gt                  # intact_gt | intact_lt
arms          = FROZEN_LEARN, YOKED, SHAM_COST
min_dz        = 0.8
read_ticks    = 512                        # ticks at which the measure is read (the last is primary)
notes         = <free text, one line>
```

Every `<int>` is fixed during tuning and copied into the prereg; nothing is left to a default inside the
tool. The tool prints the fully resolved spec (every default filled) into `env.txt`, and that text is what
the reviewer compares.

## Appendix B. A worked preregistration

The shape only; every value marked `<...>` is decided in tuning on seeds 1-99 by the agent and frozen at the
lock. No number below is a result.

```
# docs\prereg\PREREG-BUTLIN14.txt  (locked <UTC>; SHA-256 in PINS.txt)
[suite]
id            = BUTLIN14
title         = Butlin 14 indicator properties, native, indicator-only
holm_family   = BUTLIN14
seed_blocks   = 5001-5024, 5101-5124
n_min         = 40
engine_manifest_sha256 = <64 hex>
az_suite_version = <version>
deviations    = native M8 rule replaces v40 R21 (d and Welch t) and framework v3.0 (delta_pair, LCB95)
disclosures   = 5101-5124 used once at the stub-world baseline (PROGRESS.md:25); nothing tuned on it

[item RPT-1]
catalog_ids   = butlin14:RPT-1, benchmarks100:RPT-1, cqm25:recurrent_processing, cqm25_framework:CQM19
class         = INDICATOR
mechanism     = core\az_brain.c:<recurrent step function>
probe         = P-DELAYED-CUE
probe_args    = cue_ticks=<int>, delay=<int>, patch_matter=<int>, focal=<int>
n             = 3000
ticks         = <int>
grid          = 4,4,3
species       = 8
cap           = <int>
store_units   = 0
tier1_period  = 32
tier2_period  = 256
field_stride  = 8
reorder       = 16
measure       = M-NEARER-CUED-PLACE
bounds        = 0,1000                     # per-mille of focal creatures: the ceiling rule applies at 950
direction     = intact_gt
arms          = TARGET:RECURRENCE, SHAM_SIZE
min_dz        = 0.8
read_ticks    = <int>
notes         = the v40 RPT-1 result (R21 arm A d 1.04 / t 5.10) is historical and not used
```

## Appendix C. The 100 LIFE areas with candidate native owners

Area names from `docs\requirements\life100_areas.json`. "Candidate owner" is where SB-03 should look first;
it is not a finding. "Needs" lists the stub modules an area's probes need. Every row is checked in SB-03.

| Area | Name | Candidate owner | Needs |
|---|---|---|---|
| L001 | Metabolism | metab | - |
| L002 | Feeding / appetite | metab, brain (EAT) | sense |
| L003 | Digestion | metab | - |
| L004 | Excretion / waste | metab (EXCRETE, WASTE) | - |
| L005 | Thermoregulation | metab, weather | sense, physics |
| L006 | Circulation | none found yet | - |
| L007 | Respiration / gas exchange | metab, chem (medium) | - |
| L008 | Homeostasis | metab, Tier 1 | sense |
| L009 | Allostasis / stress load | Tier 1 (arousal, mood) | sense |
| L010 | Injury / damage | social (attack), metab (health) | physics |
| L011 | Repair / healing | metab (health), to confirm | - |
| L012 | Immunity | none found yet | - |
| L013 | Pain / nociception | sense (PAIN) | sense |
| L014 | Disease / dysregulation | genome, metab (TOXIN) | - |
| L015 | Ageing / senescence | life (stages) | - |
| L016 | Death | life | - |
| L017 | Decay / decomposition | chem (burial), life | - |
| L018 | Toxicity | genome, metab (TOXIN), worldgen hot spots | - |
| L019 | Viability monitoring | sense (interoception), Tier 1 | sense |
| L020 | Circadian clock | weather (two suns), sense (DAY) | sense |
| L021 | Sleep | brain (REST), life (dormancy) | - |
| L022 | Dreaming | none found yet | - |
| L023 | Synaptic downscaling | Tier 1 plasticity, to confirm | - |
| L024 | Ultradian / interval timing | none found yet | - |
| L025 | Body schema | none found yet | physics |
| L026 | Locomotion | physics | physics |
| L027 | Peripheral limb ganglia | none found yet | physics |
| L028 | Manipulation / appendages | parts (GROW, SHED), econ (BUILD) | physics |
| L029 | Eyes / active vision | sense (sensor parts and acuity) | sense, physics |
| L030 | Morphing / variable geometry | parts (GROW, SHED) | physics, stats |
| L031 | Camouflage and signaling skin | stats (look class), signals | stats |
| L032 | Ontogenetic growth | life (stages), parts | stats |
| L033 | Continuum skin / boundary | none found yet (no soft body) | physics |
| L034 | Unified sensory bus | sense | sense |
| L035 | Interoception | sense | sense |
| L036 | Chemosense / olfaction | sense (FOOD gradients), chem | sense |
| L037 | Audition and vibrations | none found yet; signals to confirm | sense |
| L038 | Touch and proprioception | sense (TOUCH, UP) | sense |
| L039 | Electro- and magnetoreception | sense (GRAV_AHEAD), to confirm | sense |
| L040 | Quantum-inspired internal sense | tk_qsv in Tier 1 (classical; law 8 wording) | - |
| L041 | Replayable substrate transducers | none found yet | - |
| L042 | Long-range attenuated field sense | sense (gradients) | sense |
| L043 | Bioelectric / acoustic social link | signals (SIG_A, SIG_B) | sense |
| L044 | Spiking cortical sheet | none found yet (the brain is a sparse int8 network) | - |
| L045 | Thalamic gating hub | none found yet | - |
| L046 | Basal-ganglia action gate | none found yet (GATE is a learning gate) | - |
| L047 | Hippocampal episodic system | tk_hop (associative memory), partial | sense |
| L048 | 7-channel neuromodulation | Tier 1 (mood, arousal), partial | - |
| L049 | Biophysical cell diversity | brain, to confirm | - |
| L050 | Glial-vascular support | none found yet | - |
| L051 | Multi-timescale plasticity | Tier 1 plasticity and metric (tk_qgt) | - |
| L052 | Multi-tier brain scaling | Tier 1 and Tier 2 periods; A1 N/A-ENGINE, A2 NOT-BUILT until S2 | - |
| L053 | Decentralized ganglia | none found yet | - |
| L054 | Generative world model | none found yet | - |
| L055 | Global workspace | none found yet | - |
| L056 | Selective attention | Tier 1 attention (tk_sh) | sense |
| L057 | Metacognition | none found yet | - |
| L058 | Decision and policy search | brain, Tier 1 lanes (tk_qsv lane choice) | sense |
| L059 | Curiosity and exploration | NOISE, IMPROV | sense, physics |
| L060 | Embodied self-model | none found yet | physics |
| L061 | In-world regime inference | none found yet | sense |
| L062 | Latent state geometry | tk_qgt metric | - |
| L063 | Imagination and recombination | none found yet | - |
| L064 | Associative learning | Tier 1 plasticity, tk_hop | sense |
| L065 | Working memory | brain recurrence | sense |
| L066 | Causal agency | Tier 1 RPE plasticity | sense, physics |
| L067 | Non-linguistic communication | signals | sense |
| L068 | Spatial cognition | sense (HOME, mark 0 gradient) | sense, physics |
| L069 | Discrete affective modes | Tier 1 mood | - |
| L070 | Slow mood baselines | Tier 1 mood | - |
| L071 | Visceral feeling readouts | sense (interoception) | sense |
| L072 | Incentive motivation | Tier 1 reward | sense |
| L073 | Individual temperament | genome (traits) | stats |
| L074 | Acute stress response | Tier 1 arousal | sense |
| L075 | Learned social attitudes | social (TRUST is unread today, `PROGRESS.md:1502`) | sense |
| L076 | Reinforcement architecture | Tier 1 RPE | - |
| L077 | Autonomous reproduction | life, mate | - |
| L078 | Developmental genome | genome | - |
| L079 | Mutational operators | genome (MUT_RATE, GENE_DUP, GENE_DEL, GENE_NEW, NEURO_*) | stats |
| L080 | Multicellular ontogeny | life (stages), parts | stats |
| L081 | Natural selection and lineages | clades, life | stats |
| L082 | Transgenerational epigenetics | none found yet | - |
| L083 | Parental care | CARE (life, social) | - |
| L084 | Juvenile life stage | life | - |
| L085 | Ecological resource use | metab, worldgen guilds | - |
| L086 | Social organization | social, colony | - |
| L087 | Multimodal signaling | signals | sense |
| L088 | Resource competition | social, econ | - |
| L089 | Predator-prey dynamics | social (attack), sense (THREAT, PREY) | sense, physics |
| L090 | Biosphere physics | chem, weather, worldgen | physics |
| L091 | Cultural transmission | social (teach) | - |
| L092 | Affective contagion and aid | social (share, groom) | - |
| L093 | Mate choice | mate | - |
| L094 | Collective swarm behavior | sense (ALIGN), colony | sense, physics |
| L095 | Causal wiring integrity | strict lint, E1 (no INERT) | - |
| L096 | Controlled stochasticity | Philox, wild and lab modes | - |
| L097 | Non-copied xenobiology | G-PROVENANCE | - |
| L098 | Volumetric soft mechanics | none found yet (no soft body) | physics |
| L099 | Sandboxed in-world levers | none found yet | - |
| L100 | Preregistered lab rigor | this protocol (GATE) | - |

## Appendix D. File map: every file this protocol creates or changes

| Path | New or changed | Task | Owner rule |
|---|---|---|---|
| `tools\az_statsutil.h` | changed | SB-05 | one owner |
| `tools\az_sha256.h` | new | SB-05 | |
| `tools\az_suite.c` | new | SB-06 | |
| `tools\az_probe_tool.h`, `tools\az_probe_tool.c` | new | SB-07 | |
| `tools\az_lesion.c` | changed | SB-08 | keep the table rule of `tools\az_lesion.c:37-40` |
| `tools\az_trace.py`, `tools\az_audit.py`, `tools\az_gate.py`, `tools\az_manifest.py`, `tools\az_seedlit.py`, `tools\az_provenance.py` | new | SB-09 to SB-12 | Python 3.9, `python -I` |
| `core\az_abi.h`, `core\az_owners.def`, `core\params\*.def`, `core\az_probe.c`, honouring modules | changed or new | SB-04 | lead only, one change, between workflows |
| `CMakeLists.txt` | changed | SB-04, SB-06, SB-12 | lead only |
| `tests\test_statsutil.c`, `tests\test_suite.c`, `tests\test_probe.c`, `tests\cli\observer_10k.py`, module test files | changed or new | SB-04 to SB-12 | registration by the lead |
| `tests\golden_hashes.txt` | changed | SB-04 | every hash change with its cause |
| `docs\requirements\framework825_*.json`, `docs\requirements\framework825.md` | new | SB-02 | the other harvested files are never edited |
| `docs\prereg\README.md`, `PINS.txt`, `RUNS.csv`, `BURNED.csv`, `PREREG-*.txt` | new | SB-08, SB-15 to SB-21 | a locked prereg is never edited |
| `docs\eval\` (TRACE, dispositions, MECHANISMS, PROBES, MEASURES, MUTANTS, MACHINE-TIME, ENGINE-MANIFEST, E1-LESIONS, BENCH-SCALE, SCORECARD-*, SCOREBOARD-825, LIFE-AREAS, PASSES-JOIN, WILD-*, REVIEW-*, INTERIM-REPORT, tuning\*.spec) | new | SB-03 to SB-30 | |
| `build\work\eval\` (inputs, scripts, run folders, mutants, gates, E3) | new | all | gitignored with `build\` |
| `docs\agents\ABI-V4-NOTICE.md` | changed | SB-04 | lead |
| `build\work\PROGRESS.md` | appended | every task | append only |
