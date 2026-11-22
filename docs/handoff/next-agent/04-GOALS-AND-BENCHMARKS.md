# 04 Goals, checklists and benchmarks

> Compiled read-only on 2026-11-18 between 11:30 and 11:40 UTC from the files themselves (every claim
> cites path:line), then corrected by the desktop session for what changed afterwards: chem piece B
> landed, the suite reached 44/44 (then 45 of 45 (32 test programs, 13 gates) in 386 s on build\z), new reference hashes,
> the move to Z: only (no drive letters; `build\z` is the integration build), the live demo (`tools\az_live.c`,
> `demo\`), and, later on 2026-11-18, the worldgen landing (wave 2, module 10, built by the desktop session on the
> Claude plan after the API credit ran out): 46 of 46 (33 test programs, 13 gates) in 253 s on build\z and new
> reference hashes again.
> Line numbers in citations are as of the compile time; files have grown since.


Root of every relative path below: `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\`. In the harvest files, `V:/...` means `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-v40\...` and `W:/...` means `Z:\[Vibe Coded (AI)]\CLAUDECODE\...`. `S:/req/...` (the extraction scripts) pointed into a C: scratchpad and is not on Z:. I read files only and ran nothing.

### 0. Bottom line
- **Not one suite has been run against the native engine.** STATUS_AND_PLAN says "None of these has been evaluated against Asmodensius-Native yet" (`docs\handoff\STATUS_AND_PLAN.md:189`). It also says "no requirement list has been run" (`:68`).
- **Milestone 8 starts from zero.** It says "no v40 result carries over" (`docs\handoff\CONTINUE-BUILD.md:54`). Every PASS or 100/100 figure below is a TypeScript v40-lab record. None of them is native evidence.
- **None of the milestone 8 tooling exists yet:**
  - `docs\prereg\` does not exist, so no preregistration is locked.
  - `docs\eval\` does not exist (no TRACE.md, TRACE.csv, scorecards or BENCH-SCALE.md).
  - `tools\az_suite.c` does not exist.
  - The only evaluation tool that exists is `tools\az_lesion.c`, the lesion tool (E1).
- **The work order puts the suites late.** Milestone 8 is meant to run only after milestones 5 to 7 (`CONTINUE-BUILD.md:51-56`), and milestone 5 is not finished:
  - 10 of 13 wave-2 modules are real. Still stubs: sense, physics and stats.
  - chem piece B and then worldgen landed on 2026-11-18 (PROGRESS.md:1545-1611).
  - Milestones 6 and 7 (W3/M2 and the A1 audit) are not started.

### 1. What the owner's numbers mean
- **P06** (`docs\handoff\PROMPTS.md:301`): "14 butlin and 10 consciousness and 25 CQM and 100 things and 100 checklists and 100 benchmarks ... and 500 checklist too and more things I forgot".
- **P21** (`PROMPTS.md:512`): "do the 25 14 100 10 whatever all over once the build is done".
- **P23** (`PROMPTS.md:526`): "14 10 25 100 500 whatever checklists benchmarks standards requirements".
- Milestone 8 text (`PROMPTS.md:557` and `CONTINUE-BUILD.md:54-59`).
- OWNER-WORDS gives the P06 row the status "later (M8)" (`docs\requirements\OWNER-WORDS.md:76`).

| Owner number | Reading in force | File(s) |
|---|---|---|
| 10 | The 10 consciousness theories ("frameworks") | `consciousness10.json` |
| 14 | Butlin 14 indicator properties | `butlin14.json` |
| 25 | CQM-25 capability law, also scored as framework Suite 3 | `cqm25.json`, `cqm25_framework.json`, `cqm25_rules.json` |
| 25 (look-alikes, not "the 25") | APEX-25, the 25-point CQM scrutiny scorecard, and the framework's 25 precisions, 25 model families and 25 geometries | `apex25.json`, `cqm_scrutiny25.json`; the last three are indexed only (`checklists_index.json:398-436`) |
| 100 "things" / "100 passes" | The Creature 1 100-pass ledger, plus the executed 100-pass suite | `passes100.json` (= `passes100_v40.json`), `passes100_suite.json` |
| 100 "checklists" | No artifact has that name (`checklists_index.json:17`, `checklist500.md:18`). Closest: the 100 LIFE areas, each a 5-item checklist | `life100_areas.json` |
| 100 "benchmarks" | No list has that name or size (`passes-bench.md:12`). Closest: the 109-item Creature1-Benchmarks scored list | `benchmarks100.json` |
| Other 100-row lists | FREAK-100, NOVEL-100, Foundations (99 items), and the 100-milestone build DAG M001-M100 (indexed, not extracted, `checklists_index.json:184-196`) | `bench_freak100.json`, `bench_novel100.json`, `bench_foundations.json` |
| 500 | LIFE-100 x 5. Two wordings: v3.0 (with ids) and v40 (with status labels). A third copy is the PDF version | `checklist500.json`, `life100_v40_500.json`, `bench_life100.json` |
| "laws" / "requirements" | Owner laws and errata (333 items), the 13 release gates, the 5 mutants, 9 v40 gates and 20 v40 budgets | `laws_and_errata.json`, `antilarp13*.json`, `antilarp_mutants5.json`, `gates9.json`, `bench_budgets.json` |

The framework's full scoreboard is the 825-check set: 14 + 10 + 25 + 100 + 500 + 100 + 12 + 40 + 24. It has 5 declared mirrors, so it holds 820 unique checks (`checklists_index.json:19,170-181`).

### 2. The three statistics rules (they differ; milestone 8 uses only the last one)
- **v40 lab (OFFICIAL-R21)**
  - Rule: Cohen d >= 0.8 **and** Welch t >= 2.75 against every control.
  - A bounded metric with an intact mean above 0.95 x max fails (non-ceiling rule).
  - Seeds: 48, 1064-1111.
  - Source: `butlin14.json:18`, `consciousness10.json:19`, `passes-bench.md:23`.
- **Framework v3.0**
  - Rule: exact paired Wilcoxon p_Holm <= 0.05, **and** delta_pair >= 0.33, **and** LCB95 > -Delta_NI.
  - Arms: Intact, Passive-Body, Shuffled-Stream, Targeted-Lesion, Size-Matched-Sham.
  - Seeds and Holm family: 24 seeds with m = 25 for CQM (`cqm25_framework.json`); 64 seeds with m = 500 for the 500 (`checklist500.md:16`); m = 13 for the gates (`antilarp13.json`).
- **Native milestone 8 rule (binding)**, `CONTINUE-BUILD.md:54-57`:
  - Test: exact paired Wilcoxon, d_z with a bootstrap interval, Holm per suite.
  - EFFECT = |d_z| >= 0.8 **and** Holm p < 0.05 on the preregistered measure, against every control arm the item needs (size-matched sham, sham-cost, shuffled stream, yoked, random policy, neutral shadow).
  - The non-ceiling rule (> 0.95 of max fails) applies.
  - An under-powered item is INCONCLUSIVE, not FAIL.
  - Butlin 14, the 10 theories and CQM axes 13 and 19-22 are indicator-only scorecards.
- **Seeds for the native rule** (`CONTINUE-BUILD.md:40`):
  - Tuning uses seeds 1-99 only.
  - Reporting uses 5001-5024 and 5101-5124, 48 seeds in all.
  - One preregistration per suite, with its SHA-256 pinned: primary measure, direction, arms, seed block, minimum effect, Holm family.
  - Any change after a reporting run burns that seed block for the item.
  - The 500 checklist runs on 48 seeds instead of the framework's 64, and the deviation is recorded (default, `CONTINUE-BUILD.md:86`).
- **Gaps in the existing tool.** `tools\az_lesion.c:25-35` implements only EFFECT, NO EFFECT and INERT:
  - It compares intact against lesion only; there are no sham, yoked or shuffled arms.
  - It has no non-ceiling check and no INCONCLUSIVE verdict (grep for ceiling, inconclusive, sham, yoked and shuffled finds nothing).
  - It takes at most 64 seeds.
  - Per-item lesions beyond the 21 lesion bits need an ABI change (`CONTINUE-BUILD.md:56`).
- **Mode.** Wild, non-deterministic mode is now the default (`OWNER-WORDS.md:44,133-138`). Every reporting run must therefore pass `--lab`.

### 3. One row per suite

"Harvested" means the list was extracted into `docs\requirements\`, but no test against the native engine exists.

| Suite | Source paper or document | File | What it measures | Statistics rule | Status now | Passing would show / would not show |
|---|---|---|---|---|---|---|
| **CQM-25 law** | CQM-25 v1.0.0, ADR 0018 (2026-10-22); `W:/Cosmogonic Quantum Mechalogodrom/docs/alife/cqm-capability-profile.json` | `cqm25.json` (25 axes, each with measures, an acceptance protocol and a falsifier) | 25 capability axes: heredity, innovation, coupling, morphology, embodied dynamics ... visual/embodied scale | Native rule, Holm m = 25. Rules RULE-4 and RULE-6 of `cqm25_rules.json`: scores stay null until a calibrated protocol runs, and no aggregate life or consciousness score is ever derived | Harvested, never run. The v40 PDF scored 17/25 (`passes-bench.md:23`; `benchmarks100.json` CQM rows: 17 Passing, 3 Partial, 5 Not yet) | Would show: an axis-specific mechanism with a causal lesion effect. Would not show: life, AGI or consciousness. Axes 13 and 19-22 are indicator-only |
| CQM-25 as framework Suite 3 | `W:/ALife-Framework-v3.0-Master-Checklists.csv` L26-50 | `cqm25_framework.json` (CQM01-CQM25) | The same constructs in creature-level wording with numeric gates (for example h^2 > 0.25) | v3.0 rule: 24 seeds, m = 25 | Harvested. The CQMnn-to-axis crosswalk is inferred from order and name only (`cqm25.md:24-26`). CQM05 assumes an FP64 1e-12 ledger, but the native ledger is exact | Same as the row above. Construct drift at items 09, 14, 24 and 25 |
| CQM rules (7 + 12) | ADR 0018 | `cqm25_rules.json` | Governance and evaluation protocol | Not scored | Harvested; procedural only | n/a |
| CQM scrutiny 25 | `W:/.../2026-07-01-25-POINT-SCRUTINY-SCORECARD.md` | `cqm_scrutiny25.json` | A scored audit, not pass/fail | None | Harvested; no criteria filled | Audit only |
| **Butlin 14** | Butlin et al. indicator properties, owner wording in `W:/Creature1-Benchmarks.pdf` p.3 | `butlin14.json` (RPT-1/2, GWT-1..4, HOT-1..4, AST-1, PP-1, AE-1/2) | Mechanistic indicator properties, each tested against a lesion control | Native rule. The v40 lab used the d/t rule; the Template's Wilcoxon/Holm form is kept in `template_criterion` | Harvested, never run natively. v40 R21: 9/14 PASS, 3 PARTIAL, 2 FAIL (`butlin14.json:19`). The owner's own audit flags HOT-4 and AE-1 (`butlin.md:35`) | Would show: indicator mechanisms that are causally load-bearing. Would never show consciousness (`butlin.md:3`; `STATUS_AND_PLAN.md:707`) |
| **10 theories** | Creature1-Benchmarks.pdf p.4 | `consciousness10.json` (thaler9, iit4, activeInference, attentionSchema, fieldIntegration, ual, sensorimotor, projective, ctm, butlin14) | Nine theory tests plus one rollup (butlin14 passes only at 14/14) | Native rule; depends on Butlin (ctm = GWT-3) | Harvested, never run. v40: 6/10 PASS, 2 PARTIAL, 2 FAIL (`consciousness10.json:20`). Audit: iit4 circular, thaler9 forced, attentionSchema baseline too easy | Indicator only. CQM13 = HOT-2, CQM19 = RPT-1, CQM20 = GWT-3 and CQM21 = iit4 are duplicates (`butlin.md:34`) |
| **100 passes (ledger)** | `V:/creatures/001/PASS-100-LEDGER.md` L26-125 | `passes100.json` (PASS-001..100) and `passes100_v40.json` (same list) | Target item, weakness fixed, lesion/control verification | Native rule per item | Harvested, never run natively. A development ledger of the TS lab, not a forward requirement list (`checklists_index.json:589`) | Would show: each fixed weakness survives a lesion. Ledger and suite numbering differ (only 46/100 share a code, `passes-bench.md:10`) |
| 100 passes (executed suite) | `V:/creatures/001/organism/passes_100_suite.ts` | `passes100_suite.json` | `ok :=` expressions plus "ledger error < 1e-11" | v40 float tolerance | v40 recorded 100/100, worst ledger 3.62e-15. Does not carry over: the native law is exact integer equality | n/a for native until re-specified |
| **LIFE-100 areas** | Framework v3.0 CSV L151-650 | `life100_areas.json` (LIFE-L001..L100) | 100 biology areas of 5 items each | Through the 500 | Harvested | Grouping only |
| **500 checklist** | Framework v3.0 suite L5_LIFE500 | `checklist500.json` (LIFE-L001-A1 .. L100-A5) | Organism biology at the sub-check level | v3.0: one uniform rule for all rows, 64 seeds, m = 500. Native: 48 seeds with the deviation recorded | Harvested; every row "Not Yet". About 10^5 world runs (`CONTINUE-BUILD.md:56`). Rows L001-A2, L017-A5 (FP64 ledger) and L052 (WebGPU, Tab 19) assume the old stack (`checklist500.md:61`) | Would show: broad organ- and pathway-level causal depth. With m = 500 the Holm threshold is harsh |
| 500 (v40 wording) | `V:/creatures/001/LIFE-100.md` | `life100_v40_500.json` | Same 100 area slots, different sub-checks (0/500 verbatim matches) | none | Lab labels: 202 BUILT, 27 PARTIAL, 106 PLANNED, 165 MISSING | Historical only |
| 500 (PDF wording) | Creature1-Benchmarks.pdf section 5 | `bench_life100.json` | Same list as the PDF copy | none | 0 real passes | Historical only |
| **"100 benchmarks"** | Creature1-Benchmarks.pdf, OFFICIAL-R21, SCORECARD | `benchmarks100.json` (109 items: Butlin 14, frameworks 10, CQM 25, R variants 14, 12 capabilities, 34 DELARP qualities) | Union of the scored lists | d/t rule for the first 49 items. No criteria for the 12 capabilities or the 34 DELARP qualities | Harvested. The 14 R variants are "reference only, never counted" | Mostly duplicates of the rows above |
| FREAK-100 / NOVEL-100 / Foundations-99 | PDF sections 4 and 6 | `bench_freak100.json`, `bench_novel100.json`, `bench_foundations.json` | Idea banks | None (the criterion field is empty) | Trace only, without scoring (`CONTINUE-BUILD.md:56`). F49 is missing from the source | Nothing; these are idea banks |
| 825-suite parts not extracted | WSE-12, SOFT-40, HW-24, the framework versions of FREAK and NOVEL, the 100 milestones | Indexed only (`checklists_index.json:114-157,184-196`) | | | **Not extracted** into `docs\requirements` | Gap for the E2 trace matrix |
| **13 release gates** | Framework v3.0 CSV L927-939 | `antilarp13.json` (GATE-01..13) | Seed firewall, zero allocation, ledger closure, mutants, chemoton, hydrostat, brain offload, Holm FWER, WSE-12 rollback, ontogeny, observer independence, originality, provenance | "100% Green", m = 13 | Harvested, not run as gates. GATE-01 is blocked until the first git commit (`CONTINUE-BUILD.md:85`). GATE-02 and GATE-03 use WASM/FP64 wording | Would show release hygiene. Many gates need native re-wording (exact integer ledger) |
| 13 gates as built in v40 | `apex_ultimate_suite.ts` L1084-1096 | `antilarp13_v40impl.json` | Different names and scope from the framework list | `ok:` expressions | v40 only | Not the framework list |
| **5 mutants (GATE-04)** | apex_ultimate_suite.ts; Template | `antilarp_mutants5.json` (Leaky -2e-11, Cosmetic, Oracle, Frozen, Shuffled) | Whether the checks catch deliberately fake mechanisms | 5/5 must be caught | **Not built natively.** The native planted-defect runs are per module and different (colony 23/23, weather 16/16, chem 13/13 caught, `PROGRESS.md:1227`; chem piece B 27/27; worldgen 31/31, `PROGRESS.md:1587`). The 5 are planned in A1 (milestone 7) and milestone 8 | Would show the tests can catch theatre. Leaky must be redefined: under exact integer books, any leak > 0 fails |
| 9 v40 gates | `V:/pipeline/run_all.mjs` | `gates9.json` | TS bundle, conformance, viability, 100-pass, apex, rigor, Python audit, lint | Exit 0 | v40 only; trace without scoring | n/a natively |
| APEX-25 | `V:/.../apex_ultimate_v50.json` | `apex25.json` | v40 synthesis suite | `ok:` expressions | v40 recorded 25/25; not re-run | Historical |
| v40 budgets | `V:/docs/BUDGETS.md` | `bench_budgets.json` (BUD-1.1..3.9) | Float ledger error, seed firewall (>= 5000), Holm <= 0.05, mutants 5/5, WASM/bundle/latency sizes | target and hard limit | v40-measured only. Most rows are N/A to a C/NASM engine | Only BUD-2.x carry over in spirit |
| Laws and errata | COMMON.md laws, v2.0 errata, v3.0 closure | `laws_and_errata.json` (333 items; 89 marked N/A-ENGINE) | Constraints | DO / AVOID / TEST derivations | Harvested. Captures LAW-1..8 only; COMMON.md now has 10 laws (`docs\agents\COMMON.md:16-39`), so laws 9 and 10 are missing from the set | Compliance, not capability |
| Idea banks | Cube 1000, wishlist 512, NESTOR 134, frontier map 272, thousand reports 150/900 | `cube1000.json`, `wishlist.json`, `nested_precision.json`, `frontier_upgrade_map.json`, `thousand_reports*.json` | Taxonomies and ideas | None | Trace only | Nothing |
| **Native engineering gates** (exist and run) | `CMakeLists.txt`, ctest | 33 test programs (test_worldgen added 2026-11-18) + 13 gates (lint and its self-test, object gate and its self-test, ledger resume, 3 determinism matrices, UBSan trap build, AVX stack gate and its self-test, cli_snap_modes, cli_live_smoke) | Determinism, exact conservation, stack, UB, both world modes, the live view | Pass/fail | **Built and green: 46 of 46 (33 test programs, 13 gates) in 253 s on build\z, 2026-11-18, after the worldgen landing** (`build\work\logs\ctest_z_worldgen.txt`). Lint: 0 errors, 3 warnings (unchanged); check_text passed on the files that landing changed (`--since`); the whole root has 83 files with violations and 2 exempt (351 checked, 2026-11-18), the 83 all in docs\research and docs\requirements, the exempt two PROMPTS.md/.xml (the owner's words byte for byte, listed by name in check_text.py), none in the package (09 DOC-01/DOC-02). Before: 45 of 45 in 386 s | Would show the code is correct and conserving. Would not show capability |
| **E1** | `CONTINUE-BUILD.md:55`, `STATUS_AND_PLAN.md:689` | `tools\az_lesion.c` | Every lesion switch, intact vs lesioned | EFFECT/NO EFFECT/INERT, Holm over the 21 switches | **Partly run**, tuning seeds only (section 4; latest 7 EFFECT, 14 NO EFFECT, 0 INERT after worldgen). Reporting block 5101-5124 used once, at the stub-world baseline (`PROGRESS.md:25`). "INERT on the complete build is a defect" | Would show each switch is causal. Proves little until the stubs and lab-only limits are gone |
| **E2** | `CONTINUE-BUILD.md:56`, `STATUS:690` | to be TRACE.md / TRACE.csv in `docs\eval\` | Every requirement id mapped to a test or a reason code (NOT-BUILT, N/A-ENGINE, LAW-CONFLICT, IDEA-BANK, OUTSIDE-ENGINE, DUPLICATE-OF, SUPERSEDED-BY, NEEDS-OWNER, INDICATOR-ONLY) | n/a | **Never started**; `az_suite.c` and the gate runner do not exist | Coverage, not results |
| **E3** | `CONTINUE-BUILD.md:58`, `STATUS:691` | `tools\az_bench.c` exists; BENCH-SCALE.md does not | 1 to 1M creatures at 1, P and P+E threads, kernels 0 and 1: ticks/s, per-stage ms, cache budget, page faults, lock | Measured and load-labelled; every row above the knee measured by ES-1 (M2) is labelled DRAM-bound; until then every row above about 35,000 creatures is labelled 'beyond L2+L3 by estimate (DRAM-bound)' (08 section 2.4) | **Never run** as E3. Only ad-hoc timings exist (section 5) | Speed and scaling evidence |
| **M2** | `CONTINUE-BUILD.md:52`, `STATUS:676` | none yet | Per-stage cache table against L1D 48 KB, L2 3 MB per P core / 4 MB per E cluster, L3 36 MB; misses per creature; llvm-mca | n/a | **Never run** (it waits for the end of W2) | Names the bottleneck per stage |
| **A1** | P09; `CONTINUE-BUILD.md:53`, `STATUS:688` | none yet | Full reproduce-first audit with skeptics, the 5 mutants, repairs, re-run | No confirmed finding open; 5/5 mutants caught | **Never started.** Nobody but the author has checked colony, weather or chem (`PROGRESS.md:1259`). Worldgen had three adversarial reviews and a fixer inside its own workflow (`PROGRESS.md:1554-1556`), which is not A1; the independent review of all 13 modules is still to do (`PROGRESS.md:1610-1611`) | Catches defects behind green suites (earlier reviews found 18 to 53) |
| G1 | `STATUS:692` | n/a | Foundation gate before scaling (S1-S4) | n/a | Blocked: needs milestones 1-8 and the owner's yes on the law-2 wording | |

### 4. Lesion table: latest one (after worldgen, 2026-11-18)

Run `az_lesion.exe --seeds 1-24` on `build\z`, lab mode, 3,000 founders, 64 ticks, grid 4,4,3, 8 species, 8 workers, C reference, arena 80.8 MB locked, 88.2 s. Source: `build\work\logs\lesion_z_worldgen_1_24.txt`, summarised at `PROGRESS.md:1595-1598`. These are tuning seeds: exploratory, not a result. Every lesion changes the final state on 24 of 24 seeds.

| Lesion | Primary measure | Intact | Lesioned | d_z | Holm p | Verdict |
|---|---|---|---|---|---|---|
| BRAIN | energy | 1659 | 3484 | +3.20 | 2.5e-6 | EFFECT |
| SOCIAL | interactions | 22,850 | 0 | -3.25 | 2.5e-6 | EFFECT |
| TRADE | interactions | 22,850 | 26,230 | +1.31 | 2.5e-6 | EFFECT |
| MATING | births | 5189 | 2949 | -2.00 | 2.5e-6 | EFFECT |
| MUTATION | clades | 246.1 | 8 | -1.82 | 2.5e-6 | EFFECT |
| KIN | interactions | 22,850 | 16,240 | -0.84 | 0.0127 | EFFECT |
| TOXIN | deaths | 1666 | 1580 | -0.84 | 0.000687 | EFFECT |
| WEATHER | energy | 1659 | 1624 | -0.29 | 1 | NO EFFECT (lost its effect) |
| CHEM | energy | 1659 | 1667 | +0.45 | 0.64 | NO EFFECT |
| NOISE | energy | 1659 | 1669 | +0.44 | 0.959 | NO EFFECT |
| GRAVITY | energy | 1659 | 1662 | +0.39 | 1 | NO EFFECT (was INERT; now changes the state on 24 of 24 seeds) |
| CARE | deaths | 1666 | 1680 | +0.27 | 1 | NO EFFECT |
| METRIC | energy | 1659 | 1652 | -0.24 | 1 | NO EFFECT |
| COLONY | live | 6523 | 6518 | -0.17 | 1 | NO EFFECT |
| SIGNALS | interactions | 22,850 | 22,790 | -0.13 | 1 | NO EFFECT |
| RECURRENCE | energy | 1659 | 1667 | +0.11 | 1 | NO EFFECT |
| PLASTICITY | energy | 1659 | 1665 | +0.09 | 1 | NO EFFECT |
| TIER1_SH | energy | 1659 | 1660 | +0.06 | 1 | NO EFFECT |
| SENSES | energy | 1659 | 1661 | +0.05 | 1 | NO EFFECT |
| TIER1_Q | energy | 1659 | 1659 | +0.02 | 1 | NO EFFECT |
| MEMORY | energy | 1659 | 1659 | +0.01 | 1 | NO EFFECT |

**Totals: 7 EFFECT, 14 NO EFFECT, 0 INERT.** (The interaction counts are the log's 2.285e+04, 2.623e+04, 1.624e+04 and 2.279e+04, written out.)

### 4a. Lesion table before worldgen (history)

Run `az_lesion.exe --seeds 1-24`, lab mode, 3,000 founders, 64 ticks, grid 4,4,3, 8 species, 8 workers, C reference, arena 80.8 MB locked, 58.9 s. Sources: `build\work\w2\chem\lesion_run.txt:1-27` and `build\work\w2\lesion_1_24_chem.csv`, summarised at `PROGRESS.md:1235-1241`. These are tuning seeds: exploratory, not a result.

| Lesion | Primary measure | Intact | Lesioned | d_z | Holm p | Verdict |
|---|---|---|---|---|---|---|
| BRAIN | energy | 3364 | 9100 | +4.58 | 2.5e-6 | EFFECT |
| SOCIAL | interactions | 16,800 | 0 | -2.80 | 2.5e-6 | EFFECT |
| TRADE | interactions | 16,800 | 18,450 | +1.57 | 2.5e-6 | EFFECT |
| MATING | births | 5303 | 4226 | -0.885 | 1.8e-4 | EFFECT |
| MUTATION | clades | 325.8 | 8 | -3.04 | 2.5e-6 | EFFECT |
| WEATHER | energy | 3364 | 3164 | -0.977 | 0.00102 | EFFECT |
| CHEM | energy | 3364 | 3367 | +0.447 | 0.108 | NO EFFECT |
| SIGNALS | interactions | | | -0.409 | 1 | NO EFFECT |
| COLONY | live | 7951 | 7949 | -0.147 | 1 | NO EFFECT |
| TIER1_Q | energy | | | -0.248 | 1 | NO EFFECT |
| TIER1_SH | energy | | | -0.002 | 1 | NO EFFECT |
| MEMORY | energy | | | +0.099 | 1 | NO EFFECT |
| NOISE | energy | | | -0.261 | 1 | NO EFFECT |
| KIN | interactions | | | -0.159 | 1 | NO EFFECT |
| TOXIN | deaths | | | -0.421 | 1 | NO EFFECT |
| METRIC | energy | | | -0.310 | 1 | NO EFFECT |
| CARE | deaths | | | -0.164 | 1 | NO EFFECT (23/24 seeds differ) |
| SENSES | energy | | | -0.099 | 1 | NO EFFECT |
| RECURRENCE | energy | | | -0.058 | 1 | NO EFFECT |
| PLASTICITY | energy | | | -0.101 | 1 | NO EFFECT |
| GRAVITY | energy | | | 0 | 1 | INERT (0/24 seeds differ; module is a stub) |

**Totals: 6 EFFECT, 14 NO EFFECT, 1 INERT.** (Chem piece A; superseded by the table in section 4.)

History of the lesion table:
- Stub-world baseline on reporting seeds 5101-5124 (the one permitted use): 1 EFFECT, 5 NO EFFECT, 15 INERT (`PROGRESS.md:25`).
- Milestone 3, tuning seeds: 1 / 5 / 15 (`:186`).
- After 6 real modules: 5 / 14 / 2, with COLONY and GRAVITY INERT (`:899-912`).
- After colony: COLONY INERT becomes NO EFFECT (`:1042`).
- After chem piece A: 6 / 14 / 1, the table in section 4a.
- After worldgen (2026-11-18): 7 / 14 / 0, the table in section 4. KIN and TOXIN became EFFECT, WEATHER lost its effect, and GRAVITY is no longer INERT.

The status documents were regenerated on 2026-11-18 after the worldgen landing: STATUS_AND_PLAN.md (sections 0 and 10) gives this 7 / 14 / 0 table and the new reference hashes. The record behind them is `build\work\logs\lesion_z_worldgen_1_24.txt` and the PROGRESS.md entry "wave 2, module 10, worldgen landed".

### 5. Performance benchmarks: latest numbers

E3 and M2 have never been run. All numbers below are ad-hoc, lab-mode, 8 workers unless stated, and lightly or noisily loaded, except the worldgen rows, which were measured on an idle machine.

| World | Latest | Ticks/s | Source |
|---|---|---|---|
| **100k founders x 60 ticks, after worldgen (current)** (`--lab --n 100000 --ticks 60 --cap 400000 --grid 6,6,5`) | **73.7 ms/tick on 8 workers**; 59.9 ms on 24 workers with E cores (`--threads 24 --ecores --no-pin`); 372 ms on 1 worker; hash 73f1e9e013f8db50 on all three | about 13.6 (8 workers), 16.7 (24), 2.7 (1) | `build\work\logs\wg_ref100k_t1/t8/t24.txt`; `PROGRESS.md:1589-1590,1599-1600` |
| **1M founders x 20 ticks, after worldgen (current)** (`--cap 1500000 --grid 7,7,5 --kernels 1 --store-units 52000000`) | **1339 ms/tick on 8 workers**, hash 5d9c02d2c7dd5fb1; DRAM-bound by arithmetic | about 0.75 | `build\work\logs\wg_ref1m.txt`; `golden_hashes.txt:70` |
| 100k, chem piece B | 54.7 ms/tick on 8 workers (lightly loaded), hash 2a0607a1d1151263 | about 18.3 | `golden_hashes.txt`; `PROGRESS.md:1599` |
| 100k founders x 60 ticks (`--lab --n 100000 --ticks 60 --cap 400000 --grid 6,6,5`), after chem piece A | 46.3 ms/tick, hash 854f23b5894d5130 | about 21.6 | `tests\golden_hashes.txt` (last 100k row); `PROGRESS.md:1212` |
| 100k, econ landing | 49.7 ms (40.3 with the assembly twins) | about 20 (25) | `PROGRESS.md:866` |
| 1M founders x 20 ticks (`--cap 1500000 --grid 7,7,5 --kernels 1 --store-units 52000000`) | Chem piece A: hash 28dd24b652597da9, **not timed**. Weather landing: 931 ms/tick with colonies, 767 ms with the colony lesion (loaded machine). Econ landing: 456.9 ms (life 180, perceive 104, tier1 90 ms) | about 1.1 (loaded); about 2.2 at econ | `PROGRESS.md:1061,866`; STATUS 0a says "about 0.46 s per tick" (`STATUS_AND_PLAN.md:59`) |
| 1M, metab landing | 708 ms (tier1 62 -> 338 ms; serial birth commit about 190 ms, TR-17) | about 1.4 | `PROGRESS.md:683` |
| ABI 4 freeze, before wave 2 | 100k: C 19.24 ms, twins 16.77 ms. 1M twins: 140.99 ms (was 72.42 at the first freeze) | 52 / 60 / 7.1 | `docs\agents\ABI-V4-NOTICE.md:404-408` |
| Pre-ABI 4 stub world (ABI 3) | 1.06M, 16 threads with E cores: 69.4 ms (best 60.8). 100k, 16 threads: 11.0 ms | 14.4 / 91 | `STATUS_AND_PLAN.md:180` |

**Cache budget, measured with az_budget:**
- Fixed record: 124 B per creature, down from 140 (`PROGRESS.md:424`).
- WARM 325 B, EPH 170 B; each creature's own store block averages about 2,740 B (30-55 units of 64 B).
- 1M creatures hold 2.64 GB of live blocks. The measured run locked a 7.8 GB arena with `--store-units 52000000`; the default store locks 13.2 GB (`ABI-V4-NOTICE.md:420-422`; `golden_hashes.txt`).
- At 1M the store fills up: 288,625 births refused at the weather landing.

**Machine cache limit:**
- L2 40 MB plus L3 36 MB gives 76 MB. History of the arithmetic: the 140-byte hot tier alone would put the knee near 540k; with each creature's own brain read every tick (about 1.9 KB) it is near 35,000 (08 section 2.4, an estimate).
- Every row above the knee measured by ES-1 (M2) is labelled DRAM-bound; until then every row above about 35,000 creatures is labelled 'beyond L2+L3 by estimate (DRAM-bound)' (08 section 2.4).
- The 1M reference world is DRAM-bound and must be labelled so, never reported as cache-resident (`CONTINUE-BUILD.md:28`, `STATUS_AND_PLAN.md:702`).

**Why worldgen is slower.** 100k at 8 workers went from 54.7 ms per tick (chem piece B, lightly loaded) to 73.7 ms (worldgen, idle machine): about 35 percent slower. The founders now start clustered in their habitat patches instead of spread over a uniform world, so perception and pair interactions are denser (`PROGRESS.md:1599-1600`). The 1M world, at 1339 ms per tick, stays DRAM-bound by arithmetic (see the cache limit above).

**Reference hashes now (worldgen, 2026-11-18; `tests\golden_hashes.txt:69-71`):**
- 100k lab: 73f1e9e013f8db50 (the same on 1, 8 and 24 threads; 217,324 births, 89,855 deaths; books exact).
- 1M lab: 5d9c02d2c7dd5fb1 (with `--store-units 52000000`).
- Engine matrix: 24c923001a46b122 (printed, not pinned).
- A wild 100k run (no `--lab`, no fixed hash by design): 984e07a7655176a2, books exact.

History: the chem piece-B references 2a0607a1d1151263 (100k), d7297084ce3e97ee (1M) and dd59809c915fc8ca (matrix) are superseded. Snapshots saved before the worldgen landing no longer load, because the parameter table grew.

These hashes change again with each later module; record the cause of every change.
