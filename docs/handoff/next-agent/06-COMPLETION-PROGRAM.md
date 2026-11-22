# 06 Completion program: milestones 5 (rest) to 10, to total completion

> Written 2026-11-18 by the desktop session (read-only: nothing was built, tested or run for this file;
> files were only read). Every number below is copied from a cited file or was counted from the files
> while writing (marked "counted 2026-11-18"). Numbers marked "proposed" are this file's proposals; the
> text says who decides them and how. Line numbers in citations are as of 2026-11-18 and drift as files
> grow.

## Table of contents

- [0. Read this first](#0-read-this-first)
  - [0.1 What this file is](#01-what-this-file-is)
  - [0.2 The owner's newest words and where each is handled](#02-the-owners-newest-words-and-where-each-is-handled)
  - [0.3 Ids, effort scale, evidence and decisions](#03-ids-effort-scale-evidence-and-decisions)
- [1. Rules for every work package](#1-rules-for-every-work-package)
  - [1.1 The laws as hard constraints](#11-the-laws-as-hard-constraints)
  - [1.2 Shell and path rules](#12-shell-and-path-rules)
  - [1.3 Standard commands C-01 to C-22](#13-standard-commands-c-01-to-c-22)
  - [1.4 The module landing checklist L-01 to L-18](#14-the-module-landing-checklist-l-01-to-l-18)
  - [1.5 The evidence entry](#15-the-evidence-entry)
  - [1.6 Seeds](#16-seeds)
  - [1.7 What counts as an independent review](#17-what-counts-as-an-independent-review)
  - [1.8 Money, time and stop rules](#18-money-time-and-stop-rules)
- [2. Starting point](#2-starting-point)
  - [2.1 Verified state on 2026-11-18](#21-verified-state-on-2026-11-18)
  - [2.2 Constraints found while writing this file](#22-constraints-found-while-writing-this-file)
  - [2.3 Why the demo shows pictures and ticks, not frames per second](#23-why-the-demo-shows-pictures-and-ticks-not-frames-per-second)
- [3. Phase map](#3-phase-map)
- [4. P0 Baseline and environment check](#4-p0-baseline-and-environment-check)
- [5. P1 Milestone 5: sense](#5-p1-milestone-5-sense)
- [6. P2 Milestone 5: physics and its NASM twin](#6-p2-milestone-5-physics-and-its-nasm-twin)
- [7. P3 Milestone 5: stats and classification](#7-p3-milestone-5-stats-and-classification)
- [8. P4 Milestone 5: closure](#8-p4-milestone-5-closure)
- [9. P5 Milestone 5: the decisive chain demonstration](#9-p5-milestone-5-the-decisive-chain-demonstration)
- [10. P6 Milestone 6: viewer, scan twin, GPU fixes, TM1, TM2, TM3](#10-p6-milestone-6-viewer-scan-twin-gpu-fixes-tm1-tm2-tm3)
- [11. P7 Milestone 7: the full audit A1 and the 5 mutants](#11-p7-milestone-7-the-full-audit-a1-and-the-5-mutants)
- [12. P8 Milestone 8: E1, E2, E3](#12-p8-milestone-8-e1-e2-e3)
- [13. P9 Milestone 9: G1, then S1 to S4 and R2](#13-p9-milestone-9-g1-then-s1-to-s4-and-r2)
- [14. P10 Milestone 10: FINAL_REPORT.md](#14-p10-milestone-10-final_reportmd)
- [15. Dependency graph](#15-dependency-graph)
- [16. Critical path and order](#16-critical-path-and-order)
- [17. Milestone acceptance checklist](#17-milestone-acceptance-checklist)
- [18. Holes, gaps, blind spots and dilemmas register](#18-holes-gaps-blind-spots-and-dilemmas-register)
- [19. Owner decisions this program needs](#19-owner-decisions-this-program-needs)
- [20. Definition of total completion](#20-definition-of-total-completion)
- [21. What this file could not resolve](#21-what-this-file-could-not-resolve)

---

## 0. Read this first

### 0.1 What this file is

This is the work plan from today's state to the end of milestone 10. It turns the 10-milestone plan
(`docs\handoff\CONTINUE-BUILD.md:45-62`, `docs\handoff\STATUS_AND_PLAN.md:665-699`) into phases P0 to
P10 and numbered work packages (WP). Each package says what to read, which files to write, the design
questions with their defaults, the tests, the planted defects, the lesion and its expected effect, a
definition of done (DoD) that can fail, the gates, the evidence, the dependencies, the risks and an
effort estimate.

How it fits the rest of the package in `docs\handoff\next-agent\`:

| File | Use it for |
|---|---|
| `00-PROMPT.md` | orientation, laws, path rules, your first hour |
| `01-INDEX.md` | where every file is |
| `02-STAGES-AND-STATUS.md` | milestone status table and the owner decisions with defaults |
| `03-DEVOPS-PIPELINE.md` | toolchain, scripts, the test programs and gates |
| `04-GOALS-AND-BENCHMARKS.md` | the owner's numbers (14, 10, 25, 100, 500) and the statistics rules |
| `05-LAWS-AND-REQUIREMENTS.md` | the laws in full and the owner's words |
| **`06-COMPLETION-PROGRAM.md`** (this file) | **what to do, in which order, and how to know it is done** |
| `07-SUITES-AND-BENCHMARKS-PROTOCOL.md` | the detail of the milestone-8 suites (E1, E2, E3, every item). Where this file says "see 07", the item lives there. If 07 is missing, use `04-GOALS-AND-BENCHMARKS.md` sections 2-3 and `CONTINUE-BUILD.md:55-60` |
| `08-...` (the real-time viewer file, written beside this one) | the design of the real-time viewer that replaces "pictures and ticks". If 08 is missing, use section 2.3 and WP-P6-01 below |
| `09-ISSUES-GAPS-BLINDSPOTS.md` | the full register of issues, gaps and blind spots; section 18 here maps the ones this program meets to work packages |
| `10-DECISIONS-AND-DILEMMAS.md` | the decisions and dilemmas in depth; section 19 here lists the owner decisions this program reaches, with defaults |

When this file and an older plan disagree, the owner's newest words win, then this file, then
CONTINUE-BUILD.md. When this file and a law disagree, the law wins and the conflict is a defect of this
file: record it in `build\work\PROGRESS.md` and follow the law.

### 0.2 The owner's newest words and where each is handled

His message P51 of 2026-11-18 19:29:42 UTC, byte for byte (his double spaces kept; the same text as
`00-MASTER-PROMPT.md` 1.3, which also lists his messages P44-P50):

````text
Make me a massive prompt and package for it to continue working on the rest of the milestones 10 total to do and the real actual goals and checklists and benchmarks to total completion and after handles all bugs errors issues problems concerns dilemmas and any holes gaps blindspots.  I notice it runs on PICTURES and TICS.  Not Frames Per Second.  Why?  Can that be fixed?  It's really slow.  I thought this would be fast.
````

| His words | Where this program carries them |
|---|---|
| "the rest of the milestones 10 total" | phases P1 to P10 (section 3), one per milestone part |
| "the real actual goals and checklists and benchmarks to total completion" | the DoD of every WP, section 17 (acceptance checklist), section 20 (definition of total completion), 07 for the suites |
| "handles all bugs errors issues problems concerns dilemmas and any holes gaps blindspots" | section 18 (register H-01 to H-48, each assigned to a WP), P4 (closure), P7 (full audit), section 21 |
| "runs on PICTURES and TICS. Not Frames Per Second. Why?" | section 2.3 (the measured cause) |
| "Can that be fixed?" | yes: WP-P6-01 and file 08 (the viewer), done when frames per second are measured |
| "It's really slow. I thought this would be fast." | the engine's own speed: WP-P6-06 (TM2, where the time goes) and WP-P6-07 (TM3, speed work with targets); the viewer is a separate cost (section 2.3) |

This message is P51 in `docs\handoff\PROMPTS.md` (the owner's words span P01-P55; WP-P0-06 checks that
the file holds them) and gets its rows in `docs\requirements\OWNER-WORDS.md`.

### 0.3 Ids, effort scale, evidence and decisions

- Phases: P0 to P10. Work packages: `WP-P<phase>-<nn>`. Design questions: `DQ-<module>-<n>`. Holes:
  `H-<nn>`. Standard commands: `C-<nn>`. Landing checklist items: `L-<nn>`.
- Track M of milestone 6 is written TM1, TM2, TM3 here (M1 research note, M2 per-stage cache table, M3
  speed work), so that it is never confused with milestones 1, 2 and 3.
- Effort (an estimate for one strong coding agent, not a measurement): **S** under half a day; **M**
  half a day to 2 days; **L** 2 to 5 days; **XL** more than 5 days.
- "Ran" means run in your session and quoted with the exact command and the tail of its output
  (`CONTINUE-BUILD.md:39`). "Unknown" is allowed; invented is not.
- A decision that belongs to you (the lead) is written in `build\work\PROGRESS.md` as a line that starts
  `DECISION:` with the options, the choice, the reason and a citation. A decision that belongs to the
  owner takes the default in section 19, is written as `DECISION (default, owner may change):`, and the
  work goes on.
- `<R>` and `$R` below mean `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native`.

---

## 1. Rules for every work package

### 1.1 The laws as hard constraints

Full text: `docs\agents\COMMON.md` laws 1-10, `docs\handoff\next-agent\05-LAWS-AND-REQUIREMENTS.md`,
`CONTINUE-BUILD.md:21-36`. Every WP below names the laws it touches. How every WP honours them:

| Law | What every WP must do | Gate that catches a breach |
|---|---|---|
| Native only (C23, NASM, PTX; no VM, JIT or interpreter in the simulation) | engine code in `core\`, `kernels\`, `gpu\` only in those languages; Python and JS only as tools | review; `core_obj_gate` (no fp, no writable data, only memcpy, memset, memmove, memcmp undefined) |
| Cache first, locked arena; no pagefile, virtual RAM on NVMe, memory compression, GPU oversubscription | never pass `--no-lock` or `--allow-pageable` in a reported, benchmark, matrix, lesion or suite run (`CONTINUE-BUILD.md:30`); a failed lock is a failed run, never rerun pageable | every evidence entry shows "locked=1" or the tool's lock line |
| No LLM in the engine | no tokenizer, prompt, API, pretrained weights, on any device | review |
| No theatre | every mechanism has a lesion with a measured effect; every trait is read and matters; no hard-coded steering; no fitness function | `lint_core --strict` (W1, W2), `test_life.c` `test_traits` (MATTERS), `az_lesion` (INERT is a defect), planted defects |
| Exact integer books every tick | `CHECK_EQ` on `az_total_matter` and `az_total_energy` after every tick in every module test; `field_underflow` 0; `dropped_events` 0 | module tests; `az_run` exit 1 on a difference |
| Wild by default, lab only for tests | modules draw only with `az_rand`, `az_rand4`, `az_rand_below` on their own streams; never branch on `cfg.mode` (`docs\agents\W2-BRIEFS\README.md:116-118`); every evaluation and reference run passes `--lab` | `test_wild`, `cli_snap_modes`; review |
| Nothing fixed | every number of the biology is a trait, a block property or a world parameter in `core\params\az_<module>.def`; no size assumed (R1); refusals counted, never a crash (R3) (`docs\agents\ABI-V4-NOTICE.md:195-221`) | review; tests at the edges (no hidden neuron, one part, empty memory) |
| Indicator only | never claim consciousness, sentience, life, AGI or quantum effects; tk_qsv is a classical simulation | review of every report |
| Honest evidence | command plus output tail; prior-art search (Zenodo, web, GitHub, by distinctive numbers) before any novelty claim, with scope and date; a green suite is not done until a reproduce-first review agrees | review; section 1.7 |
| Windows only, from Z:; no boot image, USB or VM; no drive letters | `subst` prints nothing before and after every session | WP-P0-01 |
| No installs, downloads, git commits, pushes or privilege changes without the owner's yes | ask in the report; take the default (section 19) | review |
| Every word of the owner counts | new owner words go into PROMPTS.md and OWNER-WORDS.md before work on them | WP-P0-06 |
| ASCII, LF, no tabs | after every file write | C-11 |

### 1.2 Shell and path rules

The root has spaces, `[ ]` and `( )` (`docs\agents\COMMON.md`, Paths; `00-PROMPT.md:106-133`).

- Use PowerShell 7 (`pwsh`). Start every session with
  `$R = 'Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native'` (single quotes).
- Quote every path. Give every cmdlet `-LiteralPath`. Never redirect with `>` or `*>` into the tree:
  pipe to `Set-Content -LiteralPath`. Set-Content writes CRLF: run C-11 after it, or write files from
  Python.
- Run a `.ps1` file as a script block: `& ([scriptblock]::Create([IO.File]::ReadAllText("<file>")))
  <args>`. `pwsh -File` cannot take the bracketed path.
- Python: `python -I`; inside scripts use `glob.escape` on the root. CMake globs go through
  `AZ_GLOB_ROOT` (already in `CMakeLists.txt`).
- Never create a drive letter, junction or symbolic link. Never backtick-escape a bracketed path.
- Commands that take more than about 100 s (ctest, the UBSan build, mutant runs, az_lesion at long
  horizons, az_bench, the suites) run in the background with their log on Z:
  (`<R>\build\work\logs\...`). Poll with short calls; never block a tool call for minutes.
- Never run a project tool with a guessed flag: read its usage first (some tools overwrite files,
  `CONTINUE-BUILD.md:33`).
- Before a long rewrite and every 15-20 minutes: look for other agents (Codex, Claude, ChatGPT
  processes) and fresh file times under `<R>`; adopt what they delivered and touch only your own files
  (`CONTINUE-BUILD.md:35`).

### 1.3 Standard commands C-01 to C-22

All assume `$R` is set (1.2). Each WP names the ones it needs.

| Id | What | Command | Expected today (2026-11-18) |
|---|---|---|---|
| C-01 | configure and build the integration build | `cmake -S "$R" -B "$R\build\z" -G Ninja ; cmake --build "$R\build\z"` | 19 real modules, 3 stubs, 3 twins, 0 warnings. Re-configure after adding any `core\az_<module>.c` (CMake checks EXISTS at configure time, `build\work\w2\worldgen\DESIGN.md:977-980`) |
| C-02 | everything ctest knows (background, about 4 to 7 minutes) | `ctest --test-dir "$R\build\z" --output-on-failure -j 6 2>&1 \| Set-Content -LiteralPath "$R\build\work\logs\ctest_<label>.txt"` | 46 of 46 (33 test programs, 13 gates), 253 s (`STATUS_AND_PLAN.md:717`) |
| C-03 | all test programs through the script | `& ([scriptblock]::Create([IO.File]::ReadAllText("$R\build\work\scripts\run_tests.ps1"))) -BuildDir "$R\build\z" -LogDir "$R\build\work\tests"` | every program exits 0 (last run before worldgen: 32 of 32, `STATUS_AND_PLAN.md:718`) |
| C-04 | the 100k reference world | `& "$R\build\z\az_run.exe" --lab --n 100000 --ticks 60 --cap 400000 --grid 6,6,5 --threads 8` | final hash 73f1e9e013f8db50, books exact |
| C-05 | the 1M reference world (background) | `& "$R\build\z\az_run.exe" --lab --n 1000000 --ticks 20 --cap 1500000 --grid 7,7,5 --kernels 1 --store-units 52000000` | final hash 5d9c02d2c7dd5fb1, DRAM-bound by arithmetic |
| C-06 | thread equality of the 100k world | C-04 with `--threads 1`, then `--threads 8 --no-pin`, then `--threads 13 --ecores --reorder 7 --split-seed 99`, then `--threads 24 --ecores --no-pin` | the same hash every time (`STATUS_AND_PLAN.md:722`) |
| C-07 | the whole determinism matrix (background) | `python -I "$R\tests\cli\determinism_matrix.py" "$R\build\z\az_run.exe" "$R\build\work\matrix" -- --lab --n 100000 --ticks 60 --cap 400000 --grid 6,6,5` | last line "ok" with the number of runs |
| C-08 | lint | `python -I "$R\tools\lint_core.py" "$R"` | 0 errors, 3 warnings |
| C-09 | strict lint (the M5 gate) | `python -I "$R\tools\lint_core.py" "$R" --strict` | FAILS today with 3 findings (section 2.1) |
| C-10 | the object gate | part of C-02 (`core_obj_gate`) | pass |
| C-11 | text rule | `python -I "$R\build\work\scripts\check_text.py" "$R"` (or `--since 120` for files of the last two hours) | `check_text --since <minutes>` exits 0 for your files; the whole root had 85 files with violations on 2026-11-18, all in `docs\research`, `docs\requirements` and `PROMPTS.md`/`.xml` (09 DOC-01/DOC-02); since erratum E-10 (applied 2026-11-18) run it with `--since` and `--work` together (`00-MASTER-PROMPT.md` 5.1 end step 3) |
| C-12 | lesion table on tuning seeds (background, about 90 s at 64 ticks) | `& "$R\build\z\az_lesion.exe" --seeds 1-24 --csv "$R\build\work\logs\lesion_<label>.csv" 2>&1 \| Set-Content -LiteralPath "$R\build\work\logs\lesion_<label>.txt"` | 7 EFFECT, 14 NO EFFECT, 0 INERT (`PROGRESS.md:1595-1598`). Never run az_lesion without `--seeds` (the default is the reporting block 5101-5124) |
| C-13 | recovery snapshot | `python -I "$R\build\work\scripts\snapshot.py" <label>` | a new folder under `build\work\snapshots\` |
| C-14 | UBSan trap build | part of C-02 (`ubsan_trap_build`, about 253 s), or alone: `python -I "$R\tools\ubsan_check.py" "$R" "$R\build\z\ubsan"` | 6 planted violations trap; every test program of the trap build passes |
| C-15 | second compiler (background) | `& ([scriptblock]::Create([IO.File]::ReadAllText("$R\build\work\scripts\clang_check.ps1"))) -Out "$R\build\work\clang_check\<label>"` | gcc 16.1 and clang 21.1.8 give the same reference and protocol hashes; every clang-built program passes. Not run since wave 2 (`STATUS_AND_PLAN.md:728`) |
| C-16 | one check after a change | `& ([scriptblock]::Create([IO.File]::ReadAllText("$R\build\work\scripts\m_check.ps1"))) -Out "$R\build\work\m_check\<label>"` (add `-SkipMillion` for speed) | short summary; not run since its Z: edit (`STATUS_AND_PLAN.md:729`) |
| C-17 | planted defects for a module | `python -I "$R\build\work\w2\mutants.py" "$R" <module> "$R\build\work\w2\<module>\mutants1"` with the list in `build\work\w2\<module>\mutants_<module>.py` | every mutant CAUGHT; BUILD FAILED does not count (`DESIGN.md:829-834`) |
| C-18 | interleaved A/B timing | `& ([scriptblock]::Create([IO.File]::ReadAllText("$R\build\work\scripts\ab_time.ps1"))) -A "<old az_run.exe>" -B "<new az_run.exe>" -Csv "$R\build\work\logs\ab_<label>.csv" -Rounds 6 -- --lab --n 100000 --ticks 60 --cap 400000 --grid 6,6,5 --threads 8` | medians of A and B with the CPU load printed |
| C-19 | regenerate the status (never by hand) | `python -I "$R\build\work\scripts\make_status.py" "$R\build\work\status_new\STATUS_AND_PLAN.md" "$R\build\work\status_new\STATUS_AND_PLAN.xml" "$R\build\work\logs\review_result.json"`, compare, then the same with the `docs\handoff\` paths | the plan rows match PROGRESS.md |
| C-20 | stubs-only invariants | `cmake -S "$R" -B "$R\build\skel-z" -G Ninja -DAZ_ONLY=NONE ; cmake --build "$R\build\skel-z" ; & "$R\build\skel-z\test_core.exe"` | 0 failures (497 checks at milestone 3; not run since wave 2) |
| C-21 | the live smoke test | part of C-02 (`cli_live_smoke`) | 21 of 21 |
| C-22 | host layer | `& "$R\build\z\test_host_win.exe"` | 187 checks, 0 failures |

### 1.4 The module landing checklist L-01 to L-18

Model: `build\work\w2\worldgen\DESIGN.md:942-992` (staging, three adversarial reviews, a fixer, landing).
Every module WP (sense, physics, stats) and every change to a landed module runs this list. A box that
cannot be ticked blocks the landing.

- **L-01** Recovery snapshot before the work (C-13, label `pre-<module>`).
- **L-02** A design file `build\work\w2\<module>\DESIGN.md` with: ground facts cited path:line, outputs,
  order of operations, integer algorithms with references, the world parameters with default and range,
  contracts, the tests, the planted defects, the integration impact, open questions each with a
  default. Write it before code.
- **L-03** A staging tree, so `<R>` stays green: copy `core kernels platform tests tools view gpu` and
  `CMakeLists.txt` to `$R\build\work\w2\<module>\tree` (`Copy-Item -LiteralPath ... -Recurse`); build in
  `...\<module>\b` and a UBSan build in `...\<module>\bu` (`-DAZ_UBSAN=ON`) (`DESIGN.md:944-962`).
- **L-04** The module's own test passes in the normal and the UBSan build; `test_core` passes with the
  module in.
- **L-05** Books exact after every tick of the module's test world (`CHECK_EQ`), `field_underflow` 0,
  `dropped_events` 0.
- **L-06** Equal hashes for 1, 3 and 8 workers and reorder periods 1 and 9 in the test; then C-06 and
  C-07 after landing.
- **L-07** Each trait it owns MATTERS in `test_traits` (it lives in `tests\test_life.c:738`, not in a
  file `tests\test_traits.c` as `ABI-V4-NOTICE.md:76` says).
- **L-08** Each lesion bit it owns is honoured and named in its test (lint W2), and changes a measured
  outcome in that test.
- **L-09** C-08: no error, and no warning that names the module. C-11: `check_text --since <minutes>`
  exits 0 for your files; the whole root had 85 files with violations on 2026-11-18, all in
  `docs\research`, `docs\requirements` and `PROMPTS.md`/`.xml` (09 DOC-01/DOC-02); since erratum E-10
  (applied 2026-11-18), with `--work` as well. `core_obj_gate`: no fp, no writable data, frames under 2,048 bytes.
- **L-10** Planted defects (C-17): all CAUGHT; `summary.txt` quoted in PROGRESS.md.
- **L-11** Three adversarial reviews in fresh contexts (section 1.7): books and determinism; the owner's
  words and no theatre; code and test strength. Each finding reproduced before it is fixed; each fix
  with a failing-then-passing test.
- **L-12** Copy only the module's files and the listed integration edits from the staging tree to `<R>`
  after a whole-tree hash comparison shows exactly those differences (as worldgen did,
  `PROGRESS.md:1558-1568`). No check removed or loosened; no expected value edited without a written
  cause.
- **L-13** Re-configure and build `build\z` (C-01); full ctest (C-02); C-03.
- **L-14** New reference hashes: C-04, C-05, C-06; one new line each in `tests\golden_hashes.txt` with
  date, hash, full command, the change and the reason. New world parameters change the hashes
  (`ABI-V4-NOTICE.md:294-295`) and make older snapshots unloadable: say so, including the demo's saved
  worlds in `build\demo\worlds\`.
- **L-15** C-12 on tuning seeds: the verdict of every lesion bit the module touches, with the
  prediction written in PROGRESS.md before the run. INERT is a defect.
- **L-16** Timing before and after with C-18; the CPU load labelled.
- **L-16b** A cost budget, written in DESIGN.md before any code (added 2026-11-18 by the fixer pass: the
  owner said "It's really slow", and every landing so far added time, 19.24 ms at the ABI-4 freeze to
  73.7 ms at 100k today, 08 section 2.2). State it as ns per live creature per tick added at the 100k lab
  reference (C-04 world, 8 workers, C-18 interleaved medians, load labelled), and also as a share of
  today's tick. Proposed default, which you confirm or change in the design with a DEC entry: at most +10
  percent of the tick before the landing for each of sense and physics, at most +3 percent for stats
  (a measurement module). L-16's numbers are compared with it. A landing over budget needs a DEC entry
  (why the cost is needed, what ES work wins it back) and a line in the next report; it does not block the
  landing by itself.
- **L-17** PROGRESS.md entry (section 1.5) and `build\work\w2\<module>\PROGRESS.md` updated step by step.
- **L-18** C-19 (status regenerated); C-13 (label `post-<module>`); the OWNER-WORDS rows the module
  carries set to built; the header comment of the module says what was chosen and why, with references
  (`W2-BRIEFS\README.md:59-63`).

### 1.5 The evidence entry

Every step ends with an entry in `<R>\build\work\PROGRESS.md` in this form (the file's top lines keep the
start time, the stop line, the effort rule, the one-owner-per-file rule and the next step current,
`CONTINUE-BUILD.md:19`):

```
## <UTC date and time>, <phase>, <WP id>: <title> (<LANDED | PARTIAL | FAILED | BLOCKED>)
Files: <every file created or changed>
DECISION: <options> -> <choice>, because <reason> (<citation>)
Evidence (logs in build\work\logs\ unless named):
- `<exact command>` -> "<tail of its output>" (<log file>)
Hashes: <new lines of tests\golden_hashes.txt, or "unchanged">
Not verified: <what was not run or not checked, and why>
Next: <the next WP id and its first step>
```

### 1.6 Seeds

- Tune only on seeds 1-99. Reporting blocks 5001-5024 and 5101-5124 only after a preregistration is
  locked with its SHA-256 pinned (`CONTINUE-BUILD.md:41`, `:55`).
- **Development seeds 1-49; held-out blocks outside 1-99** (corrected 2026-11-18 by the fixer pass;
  `00-MASTER-PROMPT.md` section 5.7 and 5.10 row 21). The earlier proposal (50-73 held out, 74-99 spare)
  is withdrawn: `tests\test_scan.c:550-556` runs seeds 51-53, `tests\test_colony.c:515` runs seed 68,
  and the worldgen tuning measured over seeds 1-99, 78 and 91-98 (`build\work\w2\worldgen\PROGRESS.md:28,
  31,53,57,160,162,180,183`). Proposed (decided by you in WP-P0-05 with a DEC entry, after the seed
  survey): CHAIN-01 on **6001-6024**, CHAIN-02 on **6101-6124** (only if CHAIN-01 burns). Every
  development measurement (WP-P4-04, 09 WG-03 and WG-05, every tuning look) uses seeds 1-49. Reason:
  milestone 5 needs a confirmatory run before milestone 8 opens the reporting blocks, and a block anyone
  has looked at cannot confirm anything.
- `az_lesion` always gets `--seeds` (1-24 during development). Any run that touches a reporting seed
  before its lock is logged in PROGRESS.md as a use of that block.

### 1.7 What counts as an independent review

The owner's rule: a green suite is not done until a reproduce-first review agrees (memory note
"green-suite-is-not-done"; earlier reviews found 18 to 53 defects behind green suites,
`CONTINUE-BUILD.md:54`). You work alone, so independence has to be made:

- A reviewer is a **fresh session with no shared history** (a new Codex session, or any other agent
  session the owner runs), started with a written brief `build\work\review\<package>\BRIEF.md` that
  lists the files to read (by path and line range), the contracts, and the checks. It is not told what
  you expect it to find.
- Every candidate finding must be **reproduced** before it counts: a failing test, a command and its
  output, or a byte comparison. A candidate that cannot be reproduced is recorded as "not reproduced"
  with the attempt.
- A **skeptic** pass (another fresh session) tries to refute each reproduced finding and checks whether
  the code or the docs already handle it (memory note "critique-fairness-pass").
- Findings are written as JSON lines in `build\work\review\<package>\findings.jsonl` with `id`, `file`,
  `line`, `summary`, `reproduction`, `severity`, `verdict` (CONFIRMED, REFUTED, NOT-REPRODUCED), `fix`,
  `test`.
- Record in PROGRESS.md which model and which session id did each review. A review you did yourself in
  the same session is "self-check", never "independent".
- If you cannot start a fresh session, say so in PROGRESS.md and in the report and ask the owner to run
  the brief; mark the module "reviewed by author only" until then (the state of the first nine modules
  today, `PROGRESS.md:1497`).

### 1.8 Money, time and stop rules

- The owner's OpenAI credit for this run is not written in any file this program read. Ask for it or
  read it from his prompt; write it at the top of PROGRESS.md with a stop line at 85 percent of it (the
  rule of `CONTINUE-BUILD.md:72`). `build\work\scripts\spend.py` reads Claude transcripts only, so it
  does not measure your spend; record the spend your own platform reports.
- Stop cleanly when the stop line is reached, when the next package's projection would cross it, or
  when a milestone fails twice: finish the current package or record it as PARTIAL, update PROGRESS.md,
  regenerate the status (C-19), snapshot (C-13) and report (`CONTINUE-BUILD.md:81-82`).
- A module that cannot be finished with the money left is not started (`PROGRESS.md:1480-1483`).

---

## 2. Starting point

### 2.1 Verified state on 2026-11-18

Sources: `PROGRESS.md:1545-1611`, `STATUS_AND_PLAN.md:709-731`, `02-STAGES-AND-STATUS.md:16-29`.

- Milestones 1 (F1), 2 (T5), 3 (T6) and 4 (V4, ABI version 4 with "nothing fixed" and wild mode) are
  done. Milestone 5 (W2): 10 of 13 modules real (life, mate, tier1, metab, social, econ, colony, weather,
  chem, worldgen); stubs: sense, physics, stats. Milestones 6 to 10: not started.
- Build `build\z`: 19 real modules, 3 stubs, 3 assembly twins (brain AVX2 and AVX-VNNI, dense VNNI,
  Philox AVX2; `kernels\`), 0 warnings. ctest 46 of 46 (33 test programs, 13 gates) in 253 s.
- Lint: 0 errors, 3 warnings. `--strict` fails with 3 findings:
  1. W1: nine traits are not read by their owner module: econ `BUILD`, `TRUST`, `VAL_2`, `VAL_3`
     (`core\az_owners.def:55`, `:82`, `:108-109`); sense `TAXIS`, `CURVE` (`:151-152`); physics
     `DENSITY`, `DRAG`, `THRUST` (`:29-31`).
  2. W2: `GRAVITY` is not honoured by physics (`az_owners.def:175`; `ABI-V4-NOTICE.md:136`).
  3. W2: `NOISE`, `RECURRENCE` and `SENSES` have no test that names them (honoured in
     `core\az_tick.c:224-226`).
- Reference worlds (lab): 100k 73f1e9e013f8db50 (same on 1, 8 and 24 workers); 1M 5d9c02d2c7dd5fb1
  (with `--store-units 52000000`); engine matrix 24c923001a46b122 (printed, not pinned). Snapshots from
  before worldgen do not load.
- Lesion table (`az_lesion --seeds 1-24`; 3000 founders, 64 ticks, grid 4,4,3, 8 species; tuning seeds,
  exploratory): 7 EFFECT (BRAIN +3.20, SOCIAL -3.25, TRADE +1.31, MATING -2.00, MUTATION -1.82, KIN -0.84,
  TOXIN -0.84), 14 NO EFFECT (among them WEATHER -0.29, CHEM, GRAVITY +0.39 which changes the state on 24
  of 24 seeds, TIER1_Q, TIER1_SH, MEMORY, METRIC, SENSES, NOISE, RECURRENCE, PLASTICITY, CARE, COLONY,
  SIGNALS), 0 INERT.
- Speed (idle machine): 100k at 73.7 ms per tick on 8 workers (13.6 ticks/s), 59.9 ms on 24 with the E
  cores, 372 ms on 1; 1M at 1339 ms per tick. At the ABI-4 freeze before the biology: 19.24 ms at 100k
  (`ABI-V4-NOTICE.md:409`). Worldgen made it about 35 percent slower (founders start clustered,
  `PROGRESS.md:1599-1600`).
- The four Tsotchke-derived kernels (tk_qsv, tk_hop, tk_sh, tk_qgt) run inside tier-1 learning
  (`core\az_tier1.c`), each creature once every 32 ticks; their lesions are tested in test_tier1; no
  significant world-level effect at 64 ticks.
- The previous agent's analysis (outputs in
  `C:\Users\Alexa\Documents\Codex\2026-11-18\the-bundle-is-clean-no-key\outputs\`, read-only data):
  both references reproduced on 1, 8 and 24 workers; the 1M world recorded 380,865 store refusals and only
  reached generation 1 (scale, not evolution); the decisive next demonstration is a known food gradient
  -> a body-frame sensory change -> motion through the actual medium -> paid intake -> reproductive
  outcome (P5 below).

### 2.2 Constraints found while writing this file

These are facts in the code that the remaining modules must work around. Each has an owner decision or
a design question below.

1. **Parameter budget.** `AzWorld.param` has 256 slots (`core\az_abi.h:1020`). Counted 2026-11-18 (grep
   `^AZ_PARAM\(`): 11 in `core\az_params.def` and 222 in `core\params\*.def` (chem 35, colony 16, econ
   10, life 16, mate 6, metab 30, social 18, tier1 8, weather 37, worldgen 46; sense, physics, stats 0) =
   233. **23 slots are left for sense, physics, stats and any later need.** Proposed split (you decide in
   WP-P1-01, DECISION line): sense at most 8, physics at most 10, stats at most 3, 2 in reserve. Growing
   the table is an ABI change (snapshot layout, `AzWorld` size) and needs an ABI notice.
2. **`TAXIS` and `CURVE` are WARM row traits that sense needs every tick** (`az_owners.def:151-152`).
   Rule D8 allows the WARM row only on pair paths, at birth and death, in Tier 1, in serial passes or
   behind `az_phase_due` with a period of at least 4 ticks (`ABI-V4-NOTICE.md:62-67`), and sense writes
   nothing but its outputs (`W2-BRIEFS\sense.md:9-10`). See DQ-SEN-1.
3. **`archetype` is a hashed WARM column that no world module reads.** It is written by `az_classify`
   (called from Tier 1, `core\az_tier1.c:319`; the stub writes nothing) and read only by
   `az_stats_collect`. A hashed state that nothing in the world reads is what the "Cosmetic" mutant
   plants. See DQ-STA-1.
4. **The interoceptive table has 13 channels, the sense brief says ten.** `core\az_tick.c:145-149` keeps
   ENERGY, HEALTH, PAIN, AGE, CARRY, READY, WASTE, RANK, NOISE, AROUSAL, MASS, PARTS and PAYOFF live under
   `AZ_LESION_SENSES`; `W2-BRIEFS\sense.md:48-49` says ten. The tick table is the contract; fix the brief
   in WP-P1-01.
5. **`AzStats` has fixed bins** (`archetype[32]`, `look[64]`, `species_live[128]`) while the stats brief
   asks for "no fixed bins" and quantiles (`W2-BRIEFS\stats.md:15-18`). `AzStats` is not state, so
   adding fields changes no hash, but it is an ABI change of az_abi.h (yours). See DQ-STA-2.
6. **No `az_develop_body` exists.** Founder bodies are built from the species' base genome, not from the
   fitted genome (`DESIGN.md:998-1004`, Q1). See WP-P4-03.
7. **Bedrock is a fraction.** `c_solid` is 0..255 and only 255 is impassable (`core\az_cells.def`,
   `solid` line). See DQ-PHY-2.
8. **Gravity is a per-cell vector field** in velocity units per tick (`c_grav`, i8 x 4, written by
   worldgen; `az_cells.def`, `grav` line). The physics stub ignores it and the rock
   (`core\stubs\az_physics.c`).
9. **No AVX2 twin of the integrator yet.** `kernels\az_kernels.c` installs `az_kin_axis_c` with the
   comment "no twin yet (wave 2 adds one with az_physics.c)".
10. **The demo saved worlds and all older snapshots stop loading** at every new world parameter
    (`tests\golden_hashes.txt:69` per `02-STAGES-AND-STATUS.md:56`).

### 2.3 Why the demo shows pictures and ticks, not frames per second

Measured by the desktop session on 2026-11-18 (how it works: `tools\az_live.c:1-33`, `view\az_view.h`,
`demo\live_server.py`, `demo\live.html`):

- `az_live` advances the world in ticks. Every K ticks ("Ticks per picture") it renders a picture with
  the engine's own **single-threaded software renderer** (`view\az_view.c`; every body is drawn, many as
  full silhouettes), encodes it as an **uncompressed PNG** (stored deflate blocks, `view\az_png.c`: a
  960x540 picture is about 1.5 MB), **writes it to disk**; the Python server (`demo\live_server.py`)
  serves the file; the browser **polls every 250 ms** and decodes it.
- Cost: 0.1 to 0.5 s per picture (484 ms at 960x540 with about 11,000 creatures; about 100 ms at 640x360
  with about 5,000), so 2 to 6 pictures per second, while the engine alone does 150 to 330 ticks per
  second at 5,000 to 10,000 creatures. (Both measured before worldgen, no log; the PoC report's "3 to
  13 ms" per tick at 10,000 is 77 to 333 ticks per second; quote 77 to 330 until PQ-0 re-measures,
  `00-MASTER-PROMPT.md` 1.5.) These small-world figures still have no log.
- The owner's own world is logged (captured read-only from the running demo on 2026-11-18, frame 226
  in `build\work\logs\demo_owner_world_2026-11-18_f226.json`, frame 228 in `build\work\logs\demo_owner_world_2026-11-18.json`; wild mode,
  8 workers, grown from 250,000 founders): 1,000,416 creatures (its cap) at tick 13,560; 414.3 ms per
  tick (about 2.4 ticks per second); 4,261.3 ms per 1600x900 picture (878,035 bodies drawn); a new
  picture about every 29 s by arithmetic (60 ticks plus the picture; not measured, 08 PQ-1).
  `tools\az_live.c` calls `az_tick` (`:527`) and then `az_view_render` (`:534`) on the same thread, so the
  world stands still while a picture is drawn. The real-time viewer fixes the picture part; the tick rate
  at 1M is engine speed work (WP-P6-06, WP-P6-07).
- Why it was built that way: correctness first. The picture is the engine's own view, and the state hash
  is compared before and after every drawing (ARCHITECTURE rule D7: the view is an observer). Speed was
  not a design goal of the demo.
- It also draws bodies only: no terrain, ocean, vents or gravity field.
- **Can it be fixed? Yes.** At demo sizes none of the cost is in the simulation (at 1M the tick itself
  takes 0.41 s; see the item above). The fix is a real-time viewer: draw on
  the GPU or with a multi-threaded renderer, send the creatures' positions (a few bytes each) instead of
  encoded pictures, never touch the disk, and push frames to the browser instead of polling. File 08 has
  the design; WP-P6-01 is the package. The observer rule stays: the viewer reads the world only between
  ticks and the hash before and after must stay equal.
- **The engine's own slowness is a second, separate question.** At 100,000 creatures the tick takes 73.7
  ms on 8 workers; it took 19.24 ms at the ABI-4 freeze before the biology was real. TM2 (WP-P6-06)
  measures where the time goes per stage; TM3 (WP-P6-07) wins it back without changing a single bit of
  the result.

---

## 3. Phase map

| Phase | Milestone | Content | WPs | Effort |
|---|---|---|---|---|
| P0 | (all) | baseline, environment, records, decisions of section 19 | WP-P0-01 to WP-P0-07 | M |
| P1 | 5 (W2) | sense | WP-P1-01 to WP-P1-04 | L |
| P2 | 5 (W2) | physics with its NASM twin | WP-P2-01 to WP-P2-05 | XL |
| P3 | 5 (W2) | stats and classification | WP-P3-01 to WP-P3-03 | M |
| P4 | 5 (W2) | closure: lint W1 and strict, stub findings, bodies, worldgen limits, seams, independent review of all 13, lesion horizons, law-text rewording | WP-P4-01 to WP-P4-09 | XL |
| P5 | 5 (W2) | the decisive chain demonstration (preregistered) | WP-P5-01 to WP-P5-04 | XL |
| P6 | 6 (W3 + track M) | real-time viewer, scan twin, GPU fixes, TM1, TM2, TM3 | WP-P6-01 to WP-P6-07 | XL |
| P7 | 7 (A1) | full P09 audit with skeptics, the 5 mutants, repairs, re-run | WP-P7-01 to WP-P7-05 | XL |
| P8 | 8 (E1-E3) | preregistrations, az_suite, E1, E3, E2 suites, interim report (detail in 07) | WP-P8-01 to WP-P8-07 | XL |
| P9 | 9 (G1, S1-S4, R2) | foundation gate, then DDR5 tier, 2 MB pages, GPU whole tick, NPU probe, placement scheduler, R2 | WP-P9-01 to WP-P9-08 | XL |
| P10 | 10 (R1) | FINAL_REPORT.md, status, hand-back | WP-P10-01 to WP-P10-03 | M |

---

## 4. P0 Baseline and environment check

Goal: start from a measured state, not from this file's word. Nothing in P0 changes engine code.

### WP-P0-01 Environment and other agents
- Inputs: `00-MASTER-PROMPT.md` section 9.1 steps 0-4b (it supersedes `00-PROMPT.md` section 11);
  `00-PROMPT.md` section 4; `CONTINUE-BUILD.md:5-8`, `:35`.
- Also: record the harness, sandbox and approval mode (09 BS-23); set `TMP` and `TEMP` to
  `$R\build\tmp` for the session's processes (09 REP-03); exclude your own process and its parent from
  the agent check and treat only a process with files changed in the last 30 minutes as a live writer
  (10 DD-T02); verify errata E-01 to E-12 of `00-MASTER-PROMPT.md` section 12 after the snapshot (applied on
  2026-11-18; run each DoD check and apply only what fails, 9.1 step 4b).
- Files: only `build\work\PROGRESS.md` (new run heading at the end; refresh the top lines).
- Commands:
  - `subst` (must print nothing); `Get-PSDrive -PSProvider FileSystem | Select Name, Root` (no N:, S:,
    T:, V:, W:).
  - `gcc --version ; nasm -v ; cmake --version ; ninja --version ; python -I --version ;
    & 'Z:\Tools\LLVM-21\bin\clang.exe' --version ; $PSVersionTable.PSVersion` (expected: gcc 16.1, NASM
    2.16.03, CMake 4.3.3, clang 21.1.8, PowerShell 7.6, Python 3.9).
  - `Get-Process | Where-Object { $_.Name -match 'codex|claude|chatgpt' } | Select Name, Id, StartTime`.
  - `Get-ChildItem -LiteralPath $R -Recurse -File | Where-Object { $_.FullName -notmatch '\\build\\' } |
    Sort-Object LastWriteTime -Descending | Select-Object -First 20 FullName, LastWriteTime`.
  - `git -C "$R" status --short | Select-Object -First 30` (read only; nothing is committed).
  - `Get-PSDrive Z | Select Used, Free`.
- DoD: each line recorded; no drive letter; any file newer than the last PROGRESS.md entry explained
  (adopted or reported) before any write.
- Laws: Z: only; no drive letters; no git commit.
- Effort: S. Depends on: nothing.

### WP-P0-02 Baseline run of every check
- Commands, in this order (background for the long ones): C-01, C-02, C-03, C-04, C-04 with
  `--kernels 1`, C-06, C-05, C-07, C-08, C-09, C-11, C-12, C-15, C-16 (`-SkipMillion` first), C-20, C-22.
- C-04 with `--kernels 1`: `tests\golden_hashes.txt:69` records the worldgen 100k hash only for the
  default kernels 0 (`tools\az_run.c:145`), yet 08 RV-A9, PQ-7 and G-P2 compare a `--kernels 1` run
  against it. Record the result on a new golden line (cause: "baseline check with --kernels 1, no code
  change"); a hash other than 73f1e9e013f8db50 is a twin-mismatch finding.
- Evidence: one PROGRESS.md entry with every command and its tail; logs in `build\work\logs\` with the
  label `p0`.
- DoD (fails if any line differs from section 2.1 without a stated cause): hashes 73f1e9e013f8db50 and
  5d9c02d2c7dd5fb1; the `--kernels 1` golden line recorded; 46 of 46; lint 0 errors and the 3 strict
  findings exactly; lesion counts 7/14/0;
  clang equals gcc on the reference and protocol worlds (first run since wave 2: if it differs, that is
  finding H-27, not a reason to stop P0); `skel-z` test_core 0 failures.
- Risks: C-15, C-16 and C-20 have not run since their Z: edits; a failure there is recorded as a finding
  with its log and fixed in WP-P0-04 or assigned.
- Effort: M. Depends on: WP-P0-01.

### WP-P0-03 Recovery snapshot
- Command: C-13 with label `p0-baseline`.
- DoD: the folder exists and its file count is in PROGRESS.md.
- Effort: S.

### WP-P0-04 Tooling repairs found by the baseline
- Only if WP-P0-02 found a broken tool (for example `clang_check.ps1` or `m_check.ps1` after the Z:
  edit, or the stale `gpu\README.md` that still says ABI version 3). Fix the tool, show the failing and
  then the passing run.
- DoD: every C-xx command of 1.3 runs as written. Effort: S to M.

### WP-P0-05 Decisions of section 19 and the seed split
- First the seed survey: search `tests\*.c`, `tools\*.c`, `build\work\logs\` and every
  `build\work\w2\*\PROGRESS.md` for seeds 1-99 and for 6001-6124 (for example `Select-String
  -LiteralPath <file> -Pattern '\bseeds?\b'` per file, then read each hit), and write
  `docs\eval\SEEDS.md`: one row per range with its user, file:line, date and purpose. Check by reading
  their usage that `az_run`, `az_lesion` and the planned `az_chain` accept seeds 6001-6124 (no guard
  limits lab seeds to 1-99 or the reporting blocks); record what you find.
- Write one `DECISION (default, owner may change):` line per owner decision in section 19 that this run
  will reach, and the `DECISION:` line (with its DEC entry) on the seed blocks of 1.6 (development 1-49;
  CHAIN-01 6001-6024; CHAIN-02 6101-6124) and the parameter split of 2.2 item 1.
- DoD: `docs\eval\SEEDS.md` exists and lists every hit; no proposed held-out block appears in it as
  used; PROGRESS.md has the lines; the top of PROGRESS.md names the next WP. Effort: S.

### WP-P0-06 The owner's newest words on record
- The owner's words span P01-P55. The hand-off pass re-extracted every prompt of the desktop session with
  `build\work\scripts\extract_prompts.py` and decided the numbers: P43 holds his full 1,099-character
  text (11:24:50 UTC, "Ok it's at 181.93  Use $15 more. ..."); P44-P50 are the later-found messages
  listed at the end of `00-MASTER-PROMPT.md` 1.2 (P44-P46 are older than P43 but were found later, so they
  carry later numbers); P51 is his newest message (section 0.2); P52-P55 are four older desktop records
  (2026-11-17 23:46 to 2026-11-18 00:07 UTC) numbered last, in time order (master 1.2; the API key and
  its key ID in P52 are markers). Check that `docs\handoff\PROMPTS.md` and `.xml` hold P01-P55 with these
  numbers. Later prompts get P56 onward.
- If they still end at P43: run `extract_prompts.py` on the transcript, writing to `build\work\prompts\`
  (never onto PROMPTS.md, `CONTINUE-BUILD.md:79`), and append P44-P55 with the numbers above, never
  renumbering. If the transcript is not reachable from your environment, copy P51 from section 0.2 and
  P44-P50 from `00-MASTER-PROMPT.md` 1.2 with the note "copied from the hand-off package, transcript not
  read" (for P44-P50 add "cut quotes, full text not recovered"), and P52-P55 from records P09-P12 of
  `build\work\prompts\desktop-a1806fab-2026-11-18b.md:169-200` (the key and key ID stay markers).
- Add rows to `docs\requirements\OWNER-WORDS.md` for "frames per second", "fast", "total completion",
  "holes gaps blindspots".
- DoD: P01-P55 appear in both files with these numbers; P51 is byte-identical to section 0.2; no file
  holds the key or key ID of P52. Effort: S.

### WP-P0-07 Money and the stop line
- Section 1.8. DoD: the credit and the stop line are at the top of PROGRESS.md, or "unknown, asked" is.
  Effort: S.

---

## 5. P1 Milestone 5: sense

Goal: real body-frame senses that read the gradients worldgen makes (food, temperature, light, gravity,
density, signals), so that a creature can know which way to go. The brain is the only reader of the
channels (`W2-BRIEFS\sense.md:7-10`).

Read first: `docs\agents\W2-BRIEFS\README.md` (all), `W2-BRIEFS\sense.md`, `ABI-V4-NOTICE.md` sections 5.1
to 5.4, `core\az_abi.h:119-153` (the 64 channels), `:536-582` (`AzPercept`, `AzCellSense`), `:1410-1415`
(`az_rotor_apply_inv`, `az_tetra4`), `core\az_tick.c:144-149` and `:216-282` (where sense is called and
masked), `core\stubs\az_sense.c`, `core\params\az_sense.def`, `core\az_owners.def:151-152`, `:183`.

### WP-P1-01 Design (`build\work\w2\sense\DESIGN.md`)
- Files: `build\work\w2\sense\DESIGN.md`, `build\work\w2\sense\PROGRESS.md`; fix the text of
  `docs\agents\W2-BRIEFS\sense.md` (the "ten interoceptive channels" and "sens[48]" lines; 2.2 item 4).
- Design questions (each answered in DESIGN.md with the default below unless measurement says
  otherwise; you decide; DECISION line):
  - **DQ-SEN-1 How sense reads `TAXIS` and `CURVE` every tick.** Options: (a) the lead adds a small
    sensory-gain field to the creature's MIND segment, written at birth (traits do not change in life),
    read every tick from the creature's own block, which the brain step reads anyway; an ABI amendment
    (a new section 6 in `ABI-V4-NOTICE.md`) and a hash change; (b) read the WARM trait row every tick
    and amend rule D8 for these two bytes; (c) read the row behind `az_phase_due(w, uid, AZ_RS_SENSE, 2)`
    and apply the gain only on those ticks (makes taxis work one tick in four: rejected unless (a) and
    (b) both fail); (d) a phen scalar: none is free (16 of 16 used, `az_abi.h:175-192`). Default (a).
    Measure (a) and (b) with C-18; if (b) costs no measurable time, (b) is simpler: you decide on the
    measurement and record both medians.
  - **DQ-SEN-2 The response curve.** Graded, integer only, no threshold (`W2-BRIEFS\README.md:105-107`):
    output = a blend between the linear response and an integer log2 response (Weber-Fechner), weighted
    by `CURVE`/255. Alternative: an integer power law (Stevens). Default: the blend; cite both.
  - **DQ-SEN-3 Gradient scale.** The four food gradients and the temperature and light gradients are
    multiplied by the `TAXIS` gain and shifted into `az_tetra4`. The base shift is a world parameter
    (`SENSE_...` in `core\params\az_sense.def`) with a default measured on seeds 1-49 so that a typical
    vent plume (worldgen defaults) does not saturate at 127 near the vent. You pick the default from the
    measured distribution and record the percentile you aimed at.
  - **DQ-SEN-4 Rock neighbours.** A face neighbour with `c_solid == 255` counts like a missing one (the
    cell itself), so no gradient points into rock and nothing is smelled through it. Default yes; tested.
  - **DQ-SEN-5 Which way is up.** `UP` compares the body's vertical axis with world up. World up is
    -grav where the cell's gravity vector is not zero (worldgen bends it), +z otherwise. `GRAV_AHEAD` is
    the gravity vector projected on the facing. Default yes; tested on a bent field.
  - **DQ-SEN-6 Interoception.** The 13 channels of the tick table depend on the body only (2.2 item 4).
  - **DQ-SEN-7 Sensor parts.** Body parts of kind `AZ_PART_SENSOR` scale acuity: the gain of the
    directional channels rises with the matter in sensor parts, through a saturating integer function
    (graded, no threshold). The creature's own block is read (allowed for its own slot). `SENSE_ACUITY`
    stays social's trait (`az_owners.def:35`); do not take it over.
  - **DQ-SEN-8 Noise.** One draw per creature and tick on `AZ_RS_NOISE` (as the stub). Keep.
  - **DQ-SEN-9 Day, season and storm.** Keep the stub's mapping (gstate words), made graded by `CURVE`.
  - **DQ-SEN-10 Parameters.** At most the sense share of 2.2 item 1; every number of the biology is a
    parameter or a trait (R2).
  - **DQ-SEN-11 A gradient-only lesion for P5.** Add `AZ_LESION_TAXIS` (proposed name; one of the
    unnamed bits 21..31, `ABI-V4-NOTICE.md:102-104`): the `TAXIS` gain is forced to 0 on the same code
    path, so every gradient and direction-of-field channel reads its neutral value (0 on the tetrahedral
    channels, 64 on the AHEAD and SIDE channels) while everything else stays. Owner: sense. This is an ABI
    change (yours): az_abi.h (the bit, and `AZ_LESION_ALL`, which is defined from the highest bit,
    `az_abi.h:407`), `az_owners.def` (`AZ_LESION_OWNER(TAXIS, sense)`), a row in `tools\az_lesion.c`
    (`tests\test_statsutil.c` checks that every bit has one), a line in the ABI notice.
- DoD: DESIGN.md covers every item of L-02, every DQ above has a decision, and the parameter list fits
  the budget. Effort: M. Depends on: P0.

### WP-P1-02 Module and test
- Files (module owner): `core\az_sense.c` (new; replaces `core\stubs\az_sense.c` once it exists),
  `tests\test_sense.c` (new), `core\params\az_sense.def`. Lead: `core\az_abi.h`, `core\az_owners.def`,
  `tools\az_lesion.c` for DQ-SEN-1 (a) and DQ-SEN-11; `core\az_brain.c` or `core\az_genome.c` if the
  MIND field is written at birth.
- `az_sense_cell(w, c, cs)`: from the cell and its six face neighbours (missing above or below, and rock,
  count as the cell itself): the amount and gradient of the four food channels through `az_field_avail`;
  toxin; `az_cell_pressure`; temperature and its gradient; light, its gradient and `lvec`; the medium's
  density and viscosity; wind; the four marks, `mark_id`, the gradient of mark 0; `grav`. Pure function
  of the cells.
- `az_sense(w, slot, p, cs, sens)`: every channel of `az_abi.h:123-153` as `W2-BRIEFS\sense.md:21-42`
  and its amendment (line 5) say; directions through `az_rotor_apply_inv` into the body frame, then
  `az_tetra4` or the AHEAD/SIDE projection (64 = nothing either way); FOOD weighted by the diet
  affinities `AZ_P_DIET_L/X/Y/W`; HOME from the colony centroid or mark 0; every channel clamped to
  0..127.
- Tests (`tests\test_sense.c`, on hand-made worlds with `az_test_shell`, lab mode):
  1. Cause matrix: for each cause (food per diet channel, temperature, light, gravity, density, wind,
     marks, signals A and B, threat, prey, kin, mate, bond, family, home, colony, mass, parts, season,
     storm, payoff, stake) move or change that cause alone; check that exactly the expected channels
     change and no other. The table of cause -> channels is in DESIGN.md.
  2. Rotation: turning the body by a rotor turns the direction channels the same way (tetrahedral
     channels permute and blend as the rotor says; AHEAD and SIDE follow the projection).
  3. Range: 1,000,000 random inputs (`AzPercept`, `AzCellSense`, HOT state, rotors including extremes):
     every channel in 0..127.
  4. Purity and firewall (the Oracle guard of P7): changing any cell outside the six face neighbours,
     any other creature's block, or any WARM column of another creature never changes `sens`.
  5. Diet: a chemotroph and a phototroph in the same cell get different FOOD directions when NUT_X and
     LIGHT point different ways.
  6. Rock: a rock neighbour adds no gradient; bent gravity changes `UP` and `GRAV_AHEAD`.
  7. Traits: `TAXIS` 16 versus 240 changes the magnitude of the gradient channels in a fixed world;
     `CURVE` 16 versus 240 changes their shape (L-07 then checks MATTERS at world level).
  8. Lesions: in a full world, `AZ_LESION_SENSES` changes the final hash and leaves the 13 interoceptive
     channels equal on tick 0; `AZ_LESION_NOISE` makes `AZ_S_NOISE` 0 and changes the hash;
     `AZ_LESION_TAXIS` leaves every non-directional channel equal and sets the directional ones neutral.
     The test names `AZ_LESION_SENSES`, `AZ_LESION_NOISE` and `AZ_LESION_TAXIS` (lint W2).
  9. Determinism: equal hashes for 1, 3 and 8 workers, reorder 1 and 9.
  10. Books: `CHECK_EQ` matter and energy after every tick (sense moves nothing; the check guards the
      integration).
- Planted defects (`build\work\w2\sense\mutants_sense.py`, form of `DESIGN.md:831-834`), every one must be
  CAUGHT by test_sense: world frame instead of body frame (skip `az_rotor_apply_inv`); gradient sign
  flipped; diet weights ignored; a neighbour beyond the six read (the cell of the world's richest food:
  an oracle); rock counted as food; up fixed to +z; noise drawn on another stream; noise keyed on the
  tick instead of the uid (every creature the same); a missing clamp (a channel reaches 128 or more);
  `TAXIS` ignored; `CURVE` ignored; an interoceptive channel made to depend on the cell; MATE AHEAD in
  the world frame; the TAXIS lesion masking a non-directional channel.
- Lesion and expected effect (written in PROGRESS.md before C-12 runs): SENSES today NO EFFECT on the
  64-tick table with stub senses. With real senses the prediction is a change of the BRAIN-like
  measures in the direction of the BRAIN lesion (fewer births, less intake), smaller in size; NOISE: no
  prediction of size, not INERT; TAXIS: a lower diet-weighted food enrichment at creature positions
  (the P5 measure). A NO EFFECT verdict is allowed at 64 ticks; it is then studied at longer horizons in
  WP-P4-08.
- DoD: L-01 to L-18 all ticked; C-09 no longer lists `TAXIS`, `CURVE`, `NOISE` or `SENSES`; no channel of
  `az_abi.h:123-153` is constant over a full lab world of 100k at 60 ticks (a channel that never moves
  is a finding: either a defect or a recorded reason).
- Gates: C-02 (all), C-08, C-09 (partial), C-11, C-14, C-17.
- Laws: no theatre (every channel has a cause and a test), nothing fixed (parameters, graded curve),
  integer only, wild by default (draws through `az_rand` only), indicator only (senses are inputs, never
  described as perception or experience).
- Risks: (1) the parameter budget; (2) DQ-SEN-1 (a) changes the block layout: all hashes change; (3)
  real senses change behaviour everywhere, so other modules' tests that pinned stub-sense behaviour may
  fail: investigate each one, never loosen a check (`DESIGN.md:874-881`); (4) frames over 2,048 bytes
  (`AzCellSense` is large; keep big buffers in `az_scratch`).
- Effort: L. Depends on: WP-P1-01.

### WP-P1-03 Reviews and fixer
- Three fresh-context reviews (L-11) with briefs; a fixer pass; every finding reproduced.
- DoD: `build\work\review\sense\findings.jsonl` has every finding with a verdict; every CONFIRMED one is
  fixed with a failing-then-passing test. Effort: M. Depends on: WP-P1-02.

### WP-P1-04 Landing
- L-12 to L-18. Evidence: new 100k and 1M hashes, C-06, C-07, C-12 with the prediction quoted.
- DoD: `cmake` prints 20 real modules and 2 stubs; ctest all pass; C-09 shows only the physics and econ
  items and RECURRENCE. Effort: S. Depends on: WP-P1-03.

---

## 6. P2 Milestone 5: physics and its NASM twin

Goal: bodies feel the gravity field, bedrock is impassable, buoyancy follows body density against the
medium, drag follows viscosity, and locomotion depends on the medium (swimming, flying, crawling); the
integrator has an AVX2 twin bit-identical to C; the GRAVITY lesion is honoured.

Read first: `W2-BRIEFS\physics.md` (all), `ABI-V4-NOTICE.md:119-121` (decision 16), `:166-221`, `:255-262`
(body parts), `core\stubs\az_physics.c`, `core\az_tick.c:279` (steer in phase 1) and `:326-352` (range in
phase 3, after metab and before life), `core\az_columns.def:41-50` and `:111`, `core\az_cells.def`
(`solid`, `med`, `grav`, `wind`), `core\az_abi.h` at `AzKernels` and `az_pos_step` (`:1136`),
`kernels\az_kernels.c`, `kernels\win64.inc`, `kernels\az_philox_avx2.asm` (a working twin to copy the
form from), `tools\check_avx_stack.py`.

### WP-P2-01 Design (`build\work\w2\physics\DESIGN.md`)
- Design questions (defaults; you decide; DECISION lines):
  - **DQ-PHY-1 Weight and buoyancy.** `dv_weight = grav(cell) * (DENSITY_body - density_medium) >>
    PHYSICS_BUOY_SHIFT` per axis, with the cell's gravity vector (not -z), 0 under `AZ_LESION_GRAVITY`
    (Archimedes). The shift is a world parameter. Default yes.
  - **DQ-PHY-2 Partial rock.** Only `c_solid == 255` is impassable (the contract,
    `W2-BRIEFS\physics.md:30-31`); a fraction below 255 adds drag linearly (graded, README rule 15).
    Default yes.
  - **DQ-PHY-3 Rock on every axis.** `kin_axis` cannot see cells. Default: after the x and y integration
    by `w->k.kin_axis` and the z integration in C, a C pass checks the destination cell axis by axis in
    the fixed order x, y, z; an axis whose step would enter a cell with `c_solid == 255` keeps its old
    position and reverses that velocity component. Contract: nobody ends a tick inside bedrock
    (`physics.md:53-54`). Diagonal corner cases are tested.
  - **DQ-PHY-4 Locomotion by medium.** Effective thrust = the brain's thrust times `THRUST` times a
    graded coupling to the medium: fin matter (`AZ_PART_FIN`) times medium density for swimming and
    flying, plus a contact term with rock below or beside the body for crawling. No switch between modes;
    each term is a product of things the world has. Default yes; the coupling scales are parameters.
  - **DQ-PHY-5 Paying for thrust.** Energy paid = `|thrust| * THRUST * PHYSICS_THRUST_COST >> shift`,
    taken from `energy` (own slot) and posted as heat to the creature's cell with `az_cell_post_heat`, so
    the books stay exact. A body with energy 0 or less gets no thrust. Kinetic energy is not an energy
    store in the books (velocity is not booked): write that in the header as a modelling choice. Drag
    removes velocity without booking (consistent with that choice).
  - **DQ-PHY-6 Drag.** Linear (Stokes) from `DRAG` and viscosity `c_med[2c + 1]` plus the rock fraction;
    semi-implicit Euler. Quadratic drag only if a test shows linear drag lets bodies exceed `AZ_VEL_MAX`
    often (count clamps in `sat[AZ_SAT_VELOCITY]`).
  - **DQ-PHY-7 The integrator definition.** `az_kin_axis_c` is the definition the twin must equal
    (`ABI-V4-NOTICE.md:119-121`). If the design changes its arithmetic (today: clamp, `v -= (v * drag) >>
    8`, `az_pos_step`), change it first, freeze it, then write the twin.
  - **DQ-PHY-8 Rotation.** `ang_*` from `AZ_M_PITCH/YAW/ROLL` with damping (a parameter), `az_rotor_step`,
    renormalised.
  - **DQ-PHY-9 Eggs, cysts, dormant bodies.** No thrust and no turning; they still drift, sink and float.
  - **DQ-PHY-10 Wind.** The pull of `c_wind` scaled by the medium's density.
  - **DQ-PHY-11 Travel counter.** Behind `az_phase_due(w, uid, AZ_RS_PHYS, 4)` add the distance since
    the last phase to `AZ_ST_TRAVEL`.
  - **DQ-PHY-12 A locomotion lesion for P5.** Add `AZ_LESION_THRUST` (proposed name, an unnamed bit):
    the brain's thrust is multiplied by 0 on the same path; passive motion (weight, wind, drift) stays.
    Owner: physics. ABI change as in DQ-SEN-11.
  - **DQ-PHY-13 Parameters.** At most the physics share of 2.2 item 1.
- DoD: as WP-P1-01. Effort: M. Depends on: WP-P1-04 (sense landed first: the order of
  `PROGRESS.md:1610`).

### WP-P2-02 The C module and its test
- Files (module owner): `core\az_physics.c` (new), `tests\test_physics.c` (new),
  `core\params\az_physics.def`. Lead: az_abi.h, az_owners.def, az_lesion.c for DQ-PHY-12.
- `az_physics_steer` (phase 1, HOT only): `acc[3]` in the world frame = the brain's body-frame thrust
  turned by the rotor, times the coupling of DQ-PHY-4, plus the contact push from the percept; clamped
  to i16.
- `az_physics_range` (phase 3, one block at a time): gather `dv` (acc, weight, wind) and `drag` per axis
  into `az_scratch` arrays; `w->k.kin_axis` for x and for y (they wrap); z in C with reflection at floor
  and ceiling; the rock pass of DQ-PHY-3; the energy payment of DQ-PHY-5; rotation; the travel counter;
  velocity clamps counted.
- Tests (`tests\test_physics.c`): a still body in still medium stays; a dense body sinks to the floor and
  rests; a light one rises; under `AZ_LESION_GRAVITY` neither moves (mean z after 200 ticks equal to the
  start, exactly, with no wind and no thrust); drag stops a coasting body sooner in a viscous medium; a
  finned body swims faster than an unfinned one in a dense medium; without fins there is no lift in a
  thin gas; on a rock floor the contact term gives traction; thrust moves a body along its own heading
  and costs exactly the booked energy, which appears as cell heat; bedrock is never entered (random
  worlds with rock, 100,000 bodies, many ticks: no body ends a tick in a cell with `c_solid == 255`);
  x and y wrap; z stays inside `1 << 16 .. 2^32 - 1 - (1 << 16)`; `|vel| <= AZ_VEL_MAX`; eggs do not
  thrust; under `AZ_LESION_THRUST` passive sinking stays and up-gradient motion stops; books exact every
  tick; equal hashes for 1, 3 and 8 workers, reorder 1 and 9, kernels 0 and 1. The test names
  `AZ_LESION_GRAVITY` and `AZ_LESION_THRUST`.
- Planted defects (`build\work\w2\physics\mutants_physics.py`): buoyancy sign flipped; gravity fixed to
  -z instead of the field; the GRAVITY lesion not honoured; rock checked only on z; rock checked on the
  old cell instead of the destination; thrust free (no energy taken); energy taken but not posted as heat
  (a leak the books must catch); velocity clamp removed; eggs thrust; thrust in the body frame without the
  rotor; x not wrapping; drag sign flipped; fin matter ignored; the THRUST lesion also stopping sinking.
- DoD: L-04 to L-10 in the staging tree; C-09 no longer lists DENSITY, DRAG, THRUST or GRAVITY.
- Effort: L. Depends on: WP-P2-01.

### WP-P2-03 The NASM twin `az_kin_axis_avx2`
- Files (module owner): `kernels\az_kin_avx2.asm` (Win64 ABI, `%include "win64.inc"`, AVX2,
  `vzeroupper` before return, callee-saved XMM registers saved if used). Lead: `kernels\az_kernels.c`
  (install under `#ifdef AZ_HAVE_KIN_AVX2` when `want & AZ_K_KIN_AVX2` and `az_twin_ok_avx2(f)`),
  `kernels\az_kernels.h` (`AZ_K_KIN_AVX2 = 1u << 3` and the declaration). CMake defines
  `AZ_HAVE_<TWIN>` when the `.asm` file exists (`kernels\az_kernels.c:1-2`).
- Test (in `tests\test_physics.c`): the twin equals `az_kin_axis_c` on 1,000,000 random blocks including
  the extremes of every input (velocity at plus and minus `AZ_VEL_MAX`, `dv` at the i16 limits, drag 0
  and 255, positions near 0 and 2^32 - 1 for every `shift`), for `n` = 0, 1, 2, 7, 8, 9, 127 and 128 and
  every pointer misalignment of 0 to 31 bytes; byte comparison of `pos` and `vel` and of 64 guard bytes
  on each side of every array.
- Planted defects: shift off by one; the tail loop of the last `n mod 8` lanes skipped; drag applied
  before the clamp; signed versus unsigned compare; a missing `vzeroupper`; a clobbered callee-saved
  register (must fail `avx_stack_gate` or the test).
- DoD: test passes; `avx_stack_gate` passes; C-04 with `--kernels 1` gives the same hash as `--kernels
  0`; az_run's kernels line names the twin. Effort: M. Depends on: WP-P2-02 (frozen C definition).

### WP-P2-04 Reviews and fixer
- As WP-P1-03, with a fourth review of the twin by a fresh session that is given only the C definition,
  the `.asm` file and `win64.inc`. Effort: M.
- DoD: four files `build\work\review\physics\<lens>\findings.jsonl` (books and determinism; owner's words
  and no theatre; code and test strength; the twin), each finding with a verdict; every CONFIRMED finding
  fixed with a test that failed before the fix and passes after it (both runs in PROGRESS.md); the twin
  reviewer's brief names only the three files. If a review could not be started, the gate line is an owner
  line (`00-MASTER-PROMPT.md` 4.3) and the module is "reviewed by author only".

### WP-P2-05 Landing
- L-12 to L-18. Expected: new hashes; the GRAVITY verdict at 64 ticks recorded with its prediction
  (prediction: the vertical distribution of bodies changes and a measure moves; EFFECT is likely, not
  guaranteed); the worldgen limit "bodies can walk into rock" (`PROGRESS.md:1603`) closed with the
  random-rock test as evidence.
- DoD: `cmake` prints 21 real modules, 1 stub, 4 twins; ctest all pass. Effort: S.

---

## 7. P3 Milestone 5: stats and classification

Goal: honest measurement for tools and the viewer: distributions with quantiles, variances, look classes
and a behavioural classification, without feeding a census back into the world
(`ABI-V4-NOTICE.md:113-116`, decision 14).

Read first: `W2-BRIEFS\stats.md`, `core\stubs\az_stats.c`, `core\az_abi.h` at `AzStats` (the struct with
`archetype[32]`, `look[64]`, `species_live[128]`), `:269-283` (`AZ_ST_*`, `AZ_ARCH_*`), `core\az_tier1.c`
around line 319, `core\az_columns.def:89` and `:93`, finding v06 (trait_mean overflow) and v08
(generation wrap) in `build\work\logs\review_result.json` by id only.

### WP-P3-01 Design (`build\work\w2\stats\DESIGN.md`)
- **DQ-STA-1 Who reads `archetype`?** Today no world module (2.2 item 3). Options: (a) give it a reader
  with a lesion (for example colony role assignment in Tier 2, or a partner's behavioural record in a
  social pair score), which adds a mechanism that must then pass every no-theatre check; (b) keep the
  column but never write it from the tick, and compute the classification on demand as a measurement in
  `az_stats_collect` and the viewer (an ABI change of the `az_classify` signature to a pure function;
  Tier 1 stops calling it); (c) drop the column (an ABI and snapshot change). Default (b): it adds no
  mechanism the owner did not ask for and leaves no hashed state that nothing reads. Ask the owner in the
  report whether he wants classification to feed back (for example, creatures recognising thieves);
  until he answers, (b).
- **DQ-STA-2 No fixed bins.** Add to `AzStats` (yours; not state, no hash change) quantiles (min, 5, 25,
  50, 75, 95 percent, max) of genome length, hidden neurons, tiles, body parts, body matter, memory words,
  colony size and lineage depth; per-trait variance (`trait_var[AZ_TRAITS]`, from u64 sums of squares);
  counts beyond the fixed arrays (`n_species_beyond`, `n_looks_beyond`) so a fixed-size summary never
  hides data. Keep the existing fields.
- **DQ-STA-3 Exact quantiles in core without memory.** `core\` has no allocator and frames stay under
  2,048 bytes; `az_stats_collect` has no worker argument for scratch. Default: radix select by bytes
  (one pass per byte of a u32 with a 256-bin count on the stack, 1 KB) for each distribution; exact and
  integer. Cost is a few passes over the population per call; it runs between ticks for tools only.
- **DQ-STA-4 Classification.** Graded scores per archetype from the behaviour counters `stat[]`
  normalised by age; the class is the highest score, ties to the lower id; `AZ_ARCH_SWITCHER` from how
  often the top class changed (needs a history: under (b) of DQ-STA-1 there is none in the world, so
  SWITCHER is measured by the tool across calls, or dropped from the per-call result with a recorded
  reason). Never keyed on species.
- **DQ-STA-5 Look class.** `az_look_class` 0..63 from the appearance traits over the clade's body plan
  and the glyph bytes; a stable function of the creature, independent of slot order.
- **DQ-STA-6 Overflows.** All sums in u64 (closes v06); generation depth read without assuming u16 does
  not wrap (v08: check what the life module stores now and report it).
- DoD: `build\work\w2\stats\DESIGN.md` covers every item of L-02 and states the L-16b cost budget; each
  of DQ-STA-1 to DQ-STA-6 has a decision with its DEC id (DQ-STA-1 with the Cosmetic risk written out if
  (b) is chosen); any new parameter fits the room DD-T05 left; the planted-defect list of WP-P3-02 is in
  the design. A reviewer can fail it by finding one DQ without a decision. Effort: S. Depends on:
  WP-P2-05.

### WP-P3-02 Module and test
- Files (module owner): `core\az_stats.c` (new), `tests\test_stats.c` (new), `core\params\az_stats.def`
  (only if a parameter is needed). Lead: az_abi.h (`AzStats`, and the `az_classify` signature under
  DQ-STA-1 (b)), `core\az_tier1.c` (stop calling `az_classify` under (b)), the tools that print stats
  (`tools\az_run.c`, `tools\az_live.c`, `demo\live_server.py`) to show the new fields.
- Tests: `az_stats_collect` changes no byte of the world (hash before and after, and a byte comparison of
  every column and the store); counts add up to `n_live`; every quantile equals a brute-force sort in the
  test (hosted code may sort); results equal for 1, 3 and 8 workers and any reorder period; a synthetic
  population of 20,000,000 trait values gives the right mean (no u32 overflow; v06); a creature at each
  edge (no hidden neuron, one part, empty memory, a missing gene family) is counted right (R1); look
  class equal under slot permutation.
- Planted defects: eggs skipped in a count; a quantile index off by one; a u32 sum; classification that
  depends on slot order; collect writing to the world; a class keyed on species; a look class reading the
  slot number.
- Lesion: none owned. Hash: unchanged under DQ-STA-1 (b) unless the ABI change moves bytes; record.
- DoD: L-04 to L-18; `cmake` prints 22 real modules, 0 stubs. Effort: M.

### WP-P3-03 Reviews and landing
- Three fresh-context reviews, as L-11 requires (corrected 2026-11-18; this package once planned two):
  (1) books, determinism and measurement purity (collect changes no byte of the world; equal results for
  1, 3 and 8 workers and any reorder period); (2) the owner's words and no theatre (no fixed bins; no
  class keyed on species; indicator only; the DQ-STA-1 choice and its Cosmetic risk); (3) code and test
  strength, including the quantile and overflow arithmetic (v06, v08). Then a fixer and the landing
  (L-11 to L-18).
- DoD: three files `build\work\review\stats\<lens>\findings.jsonl`, each finding with a verdict; every
  CONFIRMED finding fixed with a failing-then-passing test; C-02 passes; hashes recorded
  (unchanged or with their cause). Effort: S. Depends on: WP-P3-02.

---

## 8. P4 Milestone 5: closure

Goal: the definition of done of milestone 5 (`CONTINUE-BUILD.md:52`): every module real, every trait
read and mattering, every lesion honoured and tested (`--strict` passes), determinism and conservation
pass, and a seam review finds nothing open. Plus the independent review that no module except worldgen
has had.

### WP-P4-01 Lint W1 for econ and the untested lesions; strict lint as a gate
- Files: `core\az_econ.c` (read `BUILD`, `TRUST`, `VAL_2`, `VAL_3` as `az_owners.def:55`, `:82`,
  `:108-109` say: ROW, on a pair path or a phased branch), `tests\test_econ.c`; `tests\test_brain.c` or
  `tests\test_core.c` naming `AZ_LESION_RECURRENCE` with a measured change (hidden state cleared changes
  motors over time); `CMakeLists.txt` (yours): a ctest gate `lint_core_strict` running C-09.
- Design question DQ-ECO-1: what each of the four traits does. `BUILD` turns carried structure into reef
  or nest (with `AZ_M_BUILD`); `TRUST` is accepting a trade from a stranger; `VAL_2` and `VAL_3` are the
  valuations of inventory resources 2 and 3 (as `VAL_0`, `VAL_1` are used today; read `az_econ.c` first).
  Each through a graded response.
- Tests: each trait MATTERS (L-07); RECURRENCE named and measured.
- Planted defects: each trait read but its value ignored (four mutants).
- DoD: C-09 exits 0; `lint_core_strict` is in ctest and passes. Effort: M. Depends on: WP-P3-03.

### WP-P4-02 The stub findings, closed with evidence
- Findings v02 (colony store skip loses matter), v03 (founders beyond the clade limit), v05 (negative
  energy zeroed without booking), v06 (trait_mean overflow), v08 (generation wraps), v47 (clade
  founder_uid in the entity uid space) were filed against stubs (`CONTINUE-BUILD.md:52`); the landed
  modules may have closed them, but no file read for this program records it.
- For each: write a test that reproduces the original failure scenario (from `review_result.json` by id)
  against the real module; it must pass now; then plant the old behaviour as a mutant and show the test
  fails. Record FIXED with the test name, or OPEN with the failing run and fix it.
- DoD: six entries in PROGRESS.md and FIXED entries in `make_status.py` data. Effort: M.

### WP-P4-03 Founder bodies from the fitted genome (`az_develop_body`)
- Files (lead: genome is core): `core\az_genome.c` (export `az_develop_body(w, slot)`, named in
  `ABI-V4-NOTICE.md:308`), `core\az_abi.h`, `core\az_worldgen.c` (call it after the fit),
  `tests\test_genome.c`, `tests\test_worldgen.c`.
- Test: a founder's parts follow the fitted genome's `SEGMENT` and part kinds, not the base genome's;
  books exact; `AZ_LESION_MUTATION` does not change tick 0 (the reason the buds alternative was rejected,
  `DESIGN.md:1003-1004`).
- DoD: test passes; new hashes recorded. Effort: M.

### WP-P4-04 Worldgen limits
- Measure and then fix or record each limit of `PROGRESS.md:1601-1609`:
  1. Weather relaxes the depth and latitude gradients: measure the temperature profile by depth at
     ticks 0, 1000 and 10000 on seeds 1-8; if it flattens, decide with the weather owner (you) whether
     the vent and sun forcing is too weak (a parameter) or the relaxation too strong.
  2. Crowds of founders boil a few ocean cells: count boiling cells at tick 0 and 100; fix the founder
     pool cap or record.
  3. Some worlds have no founder in the air: count over seeds 1-49; if the air band is empty in more than
     the share the parameters predict, fix the band sampling.
  4. Fixed structure (ocean always under air, gravity always -z on average, the guild-to-band map, octave
     doubling, the linear fade of the rock's pull, the pool scale `(SIZE+16)/144`): an owner question
     (section 19, O-09); default: record as limits; propose which become world parameters, counting the
     parameter budget.
  Every measurement here uses development seeds 1-49 only (1.6); never 50-99, which tests and the
  worldgen tuning already used.
- DoD: one PROGRESS.md entry per limit with the measurement; fixes with tests. Effort: M.

### WP-P4-05 The cross-module seam review
- Goal: find fields renamed or reinterpreted between owners (`CONTINUE-BUILD.md:52`).
- Files: `build\work\review\seams\seams.py` (a tool that lists, for every entity column, cell column,
  gstate word, offer byte, `AzPub` field, colony field, event kind and world parameter, which file writes
  it and which files read it, by grep over `core\*.c`), its output `seams.csv`, and
  `build\work\review\seams\SEAMS.md`.
- Checks for every seam: (1) the writer is the one `az_columns.def`, `az_cells.def` or `ABI-V4-NOTICE.md`
  names; (2) units agree at both ends (temperature through `az_temp_of`; pressure; velocity in 1/65536
  cell per tick; `c_grav` in velocity units per tick; light in `AZ_LIGHT_EU`; medium density on the body
  density scale; eat and gather); (3) the reader honours the staleness rule (a derived column may be up
  to `field_stride - 1` ticks old, decision 9); (4) multiple writers of `energy` in phase 3 (metab,
  physics, social and others, each on its own slot) run in the fixed order of `az_tick.c:346-350`;
  (5) parameters read across modules (worldgen reads `METAB_PRESS_UNIT` and `LIFE_BIRTH_HEALTH`,
  `DESIGN.md:1024-1026`) are read-only and documented; (6) the docs match the code (known: the ABI
  notice names `tests\test_traits.c`, which does not exist; the sense brief's ten interoceptive channels).
- Each finding reproduced by a test; fixed with failing-then-passing tests.
- DoD: SEAMS.md lists every seam with its verdict; no CONFIRMED seam finding open. Effort: L. Depends on:
  WP-P4-01 to WP-P4-04.

### WP-P4-06 Independent reproduce-first review of all 13 modules
- Packages, one fresh session each (section 1.7), with a brief per package:
  1. life and mate; 2. tier1 with tk_qsv, tk_hop, tk_sh, tk_qgt; 3. metab and chem; 4. social, econ and
  colony; 5. weather and worldgen; 6. sense, physics (with the twin) and stats; 7. the engine they sit
  on: `az_tick.c`, `az_scan.c`, `az_genome.c`, `az_brain.c`, `az_store.c`, `az_world.c`, `az_snapshot.c`,
  `az_ledger.c`.
- Checklist per module (the reviewer gets it in the brief): books (C1 to C7, every conversion booked);
  determinism (no hidden order dependence, pair draws keyed on the smaller uid); D8 (no WARM reads on
  the common path); R1 to R4; no constant of the biology outside a `.def` file; graded responses; every
  owned trait read the way `az_owners.def` says; every owned lesion a mask on the same path; frames;
  narrowing and overflow (`sat[]` counted); the header comment matches the code; test strength (could the
  test pass with the mechanism removed? the reviewer writes one mutant per claim it doubts and runs C-17).
- Skeptic pass on every reproduced finding; fixes by you with failing-then-passing tests.
- DoD: seven `findings.jsonl` files; every CONFIRMED finding fixed or answered with a reason; PROGRESS.md
  names the reviewer session of each package; full ctest and C-07 pass after the fixes. Effort: XL.

### WP-P4-07 Second compiler and stubs-only build after wave 2
- C-15 and C-20 on the complete build. DoD: clang equals gcc on both references and both protocol worlds;
  every clang-built program passes; `skel-z` test_core 0 failures (with `-DAZ_ONLY=NONE` every module is
  a stub again: the stubs must still build and pass until they are deleted or kept as fixtures; decide
  and record). Effort: S.

### WP-P4-08 Lesion horizons
- Problem: 14 switches are NO EFFECT at 64 ticks, including the four tier-1 kernels, plasticity, memory,
  care, colony and signals (2.1). A mechanism that never shows a world-level effect is either too slow
  for the horizon or theatre.
- Commands (exploratory, tuning seeds): C-12 at `--ticks 64`, then `--ticks 512`, then `--ticks 2048`
  (and `--n` as the default 3000), each in the background with its own csv.
- For every switch that stays NO EFFECT at 2048 ticks: write in PROGRESS.md the mechanism-level test that
  shows it acts (its module test, lint W2) and a hypothesis why the world does not show it; assign it to
  P7 (A1) as a theatre candidate. INERT at any horizon is a defect to fix now.
- Do not change az_lesion's primary measures after seeing results (`tools\az_lesion.c:20-23`); a new
  measure is a new row or a new tool (az_suite, P8).
- DoD: a table of switch x horizon in `docs\eval\LESION-HORIZONS.md` with the commands. Effort: M.

### WP-P4-09 Law texts that lag wild mode
- COMMON law 6 and the ARCHITECTURE "Deterministic" row still describe a deterministic engine
  (`02-STAGES-AND-STATUS.md:53`). Reword both to state wild by default and lab for tests, quoting
  OWNER-WORDS.md; this implements his words, it does not change a law. Also reword the ARCHITECTURE rows
  behind v76 and v77 with the measured state after M5 (traits read, lesions honoured).
- DoD: the texts match `ABI-V4-NOTICE.md:371-399`; C-11. Effort: S.

---

## 9. P5 Milestone 5: the decisive chain demonstration

Goal: show, on held-out seeds and with a preregistered analysis, the whole causal chain the previous
agent named: a known food gradient -> a body-frame sensory change -> motion through the actual medium ->
paid intake -> reproductive outcome, and that a lesion of each link that can be lesioned breaks the
chain. This is the first world-level selection result the project would have (`STATUS_AND_PLAN.md:59`
says there is none).

Laws touched: no theatre (each link causal), exact books (intake and thrust cost are booked), wild by
default (the demonstration runs in lab mode for repeatability; a wild run is shown once as illustration,
never as evidence), indicator only (taxis is behaviour, never "wanting" or "knowing"), honest evidence
(preregistration before the held-out run; prior-art statement before calling anything new).

### WP-P5-01 The world, the links and the measures
- **World.** A lab world made by worldgen with one vent plume as the known gradient: `--param
  WORLDGEN_VENTS=1` (the parameter is "vents per 1024 ocean columns, at least 1",
  `core\params\az_worldgen.def:33`), other parameters at their defaults; population, grid, species and
  the horizon T chosen on development seeds 1-49 and frozen in the preregistration. The gradient is
  "known" because the tool measures it every tick from the cell columns.
- **Tool.** `tools\az_chain.c` (new, hosted C like `tools\az_lesion.c`, using `tools\az_statsutil.h`; the
  lead registers it in CMakeLists.txt). It creates worlds, ticks them and reads them between ticks; it
  never writes the world. It computes, per creature and tick, from the cells and the creature's columns
  and block (read only):
  - L0 gradient: the diet-weighted food availability of each cell (the same quantity the creature can eat,
    by `AZ_P_DIET_*`) and its gradient, computed independently of `az_sense_cell` (a second
    implementation is the Oracle cross-check of P7).
  - L1 sense: the agreement between the creature's own FOOD channels (the first `AZ_SENS` bytes of its
    MIND segment hold the last senses, `core\az_tick.c:250-254`) and the true gradient rotated into its
    body frame: the fraction of creature-ticks with the right sign of the AHEAD component.
  - L2 motion: the velocity component along the true gradient (up-gradient speed).
  - L3 intake: the uptake booked for the creature's diet channels (`AZ_ST_EAT_*` counters, `az_abi.h:269-
    271`).
  - L4 paid: intake energy minus the thrust energy physics booked for the creature (DQ-PHY-5) minus basal
    cost, from the books.
  - L5 reproduction: offspring (`AZ_ST_OFFSPRING`) per creature, and at the population level the mean
    `TAXIS` trait and the share of brains whose FOOD inputs drive the forward motor positively.
- **Primary measure (one, preregistered):** the enrichment ratio at tick T: the mean diet-weighted food
  availability at the cells of the living creatures divided by the mean over all open cells, per seed.
- **Secondary measures (described, not in the Holm family):** L1 to L5 above, the change of mean `TAXIS`
  from tick 0 to T, the dose-response of offspring on intake within each arm.
- **Arms:** A0 intact; A1 `AZ_LESION_TAXIS` (breaks L0 -> L1); A2 `AZ_LESION_SENSES` (all exteroceptive
  channels; broader); A3 `AZ_LESION_THRUST` (breaks L1 -> L2 while passive motion stays); A4
  `AZ_LESION_BRAIN` (motors zero; the existing bit); A5 `AZ_LESION_EAT` (proposed new bit owned by metab:
  uptake demand multiplied by 0 on the same path, `core\az_metab.c:110-126`; breaks L2 -> L3); A6 a
  sham-cost arm: the same number of channels masked as A1 but on channels unrelated to food (for example
  `SIG_B` and `STAKE`), so that "any mask hurts" is ruled out (control arm of `CONTINUE-BUILD.md:57`).
- **The links that cannot be lesioned.** L3 -> L4 -> L5 (intake pays for offspring) is enforced by
  conservation: a lesion that let creatures reproduce without the matter and energy would break the
  books, which the law forbids. These links are shown by the within-arm dose-response (offspring rise
  with net intake) and by A5 (no intake, no offspring). Write this in the preregistration; it is the
  honest limit of the design.
- Design questions (you decide on development seeds and record): DQ-CH-1 the horizon T (long enough for
  at least one generation: read `max_generation` from the stats); DQ-CH-2 population and grid; DQ-CH-3
  whether the tool needs a world-parameter flattening arm (all food spread evenly) as a further sham; if
  it cannot be built without a new mechanism, leave it out and say why.
- DoD: `build\work\w2\chain\DESIGN.md` with all of the above; `AZ_LESION_EAT` landed through the ABI
  steps of DQ-SEN-11 with its own test in `tests\test_metab.c`. Effort: L. Depends on: P4.

### WP-P5-02 Preregistration
- Files: `docs\prereg\PREREG-CHAIN-01.txt` (new folder; it does not exist,
  `04-GOALS-AND-BENCHMARKS.md:19`) and its hash line in `docs\prereg\PINS.txt` (07 section 3.11's form;
  `00-MASTER-PROMPT.md` 5.10 row 3). Earlier drafts named `CHAIN-01.md` and `SHA256SUMS.txt`.
- Contents: hypotheses (H1: A0 enrichment > A1, A2, A3, A4, A5 and A6 is not below A0 by the minimum
  effect; H2: within A0, offspring rise with net intake; H3: mean TAXIS rises from 0 to T in A0 more than
  in A1); the primary measure and its exact formula; direction; arms; seeds 6001-6024 (held out, 1.6); T, n,
  grid, species and every parameter; minimum effect |d_z| >= 0.8; exact paired Wilcoxon, Holm over the
  arm comparisons of H1 (m = 6); bootstrap interval of d_z; the non-ceiling rule; INCONCLUSIVE when
  under-powered; what will be reported whatever the result.
- Command: `Get-FileHash -LiteralPath "$R\docs\prereg\PREREG-CHAIN-01.txt" -Algorithm SHA256` and the hash
  written into PINS.txt; the same line in PROGRESS.md.
- DoD: the hash is recorded before any run on seeds 6001-6024. Any change afterwards makes the block burned
  for this item (`CONTINUE-BUILD.md:55`). Effort: S.

### WP-P5-03 The held-out run
- Command (form; exact options from the prereg): `& "$R\build\z\az_chain.exe" --lab --seeds 6001-6024
  --csv "$R\build\work\eval\chain01\chain01.csv" ... 2>&1 | Set-Content -LiteralPath
  "$R\build\work\eval\chain01\chain01.txt"` (background).
- Tests of the tool itself before the run (`tests\test_tools.c` or a new `tests\test_chain.c`): the tool
  does not change the world (hash before and after reading); the L0 computation equals `az_sense_cell`'s
  gradient on hand-made cells; the enrichment ratio is 1 for a uniform world.
- Planted defects for the tool: the ratio computed over all cells including rock; L1 in the world frame;
  intake read from the wrong diet channel; the arms run with different seeds.
- DoD (can fail): the result is reported exactly as the prereg says, whatever it is. "Chain shown" only if
  H1 passes for every arm A1 to A5, A6 is not below A0 by the minimum effect, and H2 holds. Otherwise the
  report says which link failed and the next step is a design investigation, never a re-run on the same
  seeds with changes.
- Evidence: `docs\eval\CHAIN-01-RESULT.md` with the table of arms, d_z, intervals, Holm p, the verdicts,
  the commands and the csv path. Effort: M.

### WP-P5-04 Illustration
- One wild run of the chain world shown in the viewer (after WP-P6-01) and one picture series saved under
  `build\work\show\`, labelled "illustration, not evidence". Effort: S.
- DoD: `build\work\show\chain-wild-<date>\` holds the pictures and a `README.txt` whose first line is
  "illustration, not evidence" and which names the command, the wild run's printed origin and the
  observer result (hash equal before and after each picture); no document cites the series as a result.
  Fails if any picture's observer check failed or the label is missing.

---

## 10. P6 Milestone 6: viewer, scan twin, GPU fixes, TM1, TM2, TM3

Goal (`CONTINUE-BUILD.md:53`): each item measured or demonstrated and recorded. The GPU whole tick is not
here (it moved to S2).

### WP-P6-01 The real-time viewer (design and targets: file 08)
- Why: section 2.3. The owner's words: "Not Frames Per Second ... Can that be fixed? It's really slow."
- Files: as 08 specifies (expected: `tools\az_live.c` and `demo\` grown, or a native Windows viewer
  program run from Z:, `view\` for any renderer change). No file in `core\`.
- Contracts that do not change: the viewer reads the world only between ticks (rule D7); the state hash
  before and after every frame is equal and checked; the arena stays locked; the reference hashes do not
  change (the simulation is untouched).
- Tests: `cli_live_smoke` stays green; a new check that runs the viewer path for N frames and compares the
  hash before and after each frame; the terrain, ocean, vents and gravity drawing checked against the
  cells (a frame of a hand-made world with one vent shows the vent where the cells have it).
- DoD (can fail): frames per second and ticks per second are measured and printed separately at 5,000,
  10,000 and 100,000 creatures, at the sizes 08 names, against 08's targets; the browser or window shows
  frames at that rate; the owner's controls say frames per second, not pictures per second; the
  measurement shows the CPU load. Today's numbers to beat: 2 to 6 pictures per second at 5,000 to 11,000
  creatures, and one 1600x900 picture about every 29 s (by arithmetic) in the owner's 1M world
  (section 2.3).
- Order: this WP touches no simulation file, so it may run right after P0, before P1 (owner's newest
  words); default: do it first if 08 is complete, and never let it delay P1 by more than its own effort.
- Laws: observer only; native only for anything in the simulation; the viewer may use floats (hosted C)
  and the GPU for drawing only (a result outside the hashed state, `CONTINUE-BUILD.md:29`); no install
  without the owner's yes.
- Effort: L to XL (08 decides).

### WP-P6-02 The scan AVX2 twin
- Order: after TM2 (WP-P6-06) shows which loop of `core\az_scan.c` is hot (the scan, senses and intents
  took about 4.7 of 16.77 ms at 100k with the twins at the freeze, `ABI-V4-NOTICE.md:413-415`).
- Files: lead first extracts the hot inner loop into a C definition with a fixed signature and adds it to
  `AzKernels` (an ABI change with a notice line), then `kernels\az_scan_avx2.asm`, `kernels\az_kernels.c`
  and `.h`, `tests\test_scan.c`.
- Test: twin equals C on 1,000,000 random batches with extremes, every `n` from 0 to the batch size,
  misalignment 0 to 31; same world hash with `--kernels 0` and `1`.
- Planted defects: as WP-P2-03.
- DoD: twin bit-identical; measured gain with C-18 recorded (no gain is a valid, recorded result; then
  keep or drop the twin by a DECISION). Effort: L.

### WP-P6-03 GPU fixes v23, v26, v57, v25
- v23 (`gpu\az_gpu.c:276`): a second `az_gpu_open` on one thread redirects the first handle's calls; make
  each handle's context current on every call through it (or use the primary context with retain and set
  current). Test in `gpu\test_gpu.c`: two handles opened on one thread, each runs its kernel and gets its
  own result.
- v26 (`gpu\az_gpu.c:262`): check the CUresult of `cuDriverGetVersion`, `cuDeviceGetName`, both
  `cuDeviceGetAttribute` calls and `cuDeviceTotalMem`; report "unknown" on failure. Test with a fault
  injection hook (a function pointer the test replaces).
- v57 (`gpu\az_gpu_ref.c:17`): one small header for `az_memset` and `az_memcpy` used by `az_gpu_ref.c`
  and `az_gpu_dev.c`. Test: test_gpu still bit-exact.
- v25 (`gpu\az_gpubench.c:14`): correct the header comment (the C reference runs unless `--kernels`).
- Also update `gpu\README.md` from ABI version 3.
- DoD: each fix with a failing-then-passing test where a test can fail; test_gpu passes on the RTX 5070
  Ti; findings marked FIXED in `make_status.py` data. Effort: M. Depends on: P0.

### WP-P6-04 TM1: the research note
- File: `docs\research\M1-machine-levels.md`, building on `docs\research\factcheck\below_asm.md`, as
  `STATUS_AND_PLAN.md:674` describes (what a Windows program on Lion Cove and Skymont cores and the RTX
  5070 Ti can and cannot control; PTX versus SASS; gates and transistors as background only).
- DoD: every claim cites a source (generic web queries only) or a measurement on this machine. Effort: M.
  Any time.

### WP-P6-05 Viewer check of the chain world
- The chain world of P5 shown in the viewer with the vent, the gradient and the creatures; the hash check
  on. DoD: one recorded session with fps. Effort: S. Depends on: WP-P6-01, WP-P5-01.

### WP-P6-06 TM2: the per-stage cache table
- Goal: name the bottleneck of every tick stage on the complete world (`STATUS_AND_PLAN.md:676`).
- Commands (background; load labelled; `--lab`): `& "$R\build\z\az_bench.exe" --n 10000,50000,100000,
  262144,540000,1000000 --threads 1,P,PE --kernels 0 --ticks 200 --warmup 50 --repeats 3 --csv
  "$R\build\work\eval\tm2\bench.csv"` and the same with `--kernels 1` (read `az_bench` usage first;
  `tools\az_bench.c:1-6`). The 1M row needs enough store: check that `az_bench` sizes the store for the
  population or add an option (lead), so the row does not measure refusals (H-03).
- Per stage (`w->prof_ns` names): bytes touched per 128-creature block (computed from the columns and
  block segments the stage reads and writes, listed per stage in the table), cycles per creature, the
  slope of ns per creature across population sizes (the knees at L2 and L3: L1D 48 KB and L2 3 MB per P
  core, L2 4 MB per E-core cluster, L3 36 MB, `STATUS_AND_PLAN.md:676`), and `llvm-mca -mcpu=arrowlake-s`
  on the hot loops (brain step, scan, Philox, the integrator).
- Cache misses per creature: Windows gives no counter programming without a driver
  (`STATUS_AND_PLAN.md:704`). If Windows Performance Recorder (present) can record hardware counters in a
  non-elevated session, use it; otherwise derive misses from bytes touched and the slopes and label the
  column "derived, not counted".
- File: `docs\eval\M2-CACHE-TABLE.md` (08's name; `00-MASTER-PROMPT.md` 5.10 row 4) with commands, raw
  csv paths and the table.
- DoD: one row per stage with bytes, cycles per creature, the bottleneck named (compute, latency,
  bandwidth, serial dispatch). The knee measured from the slopes is stated; every row above it is
  labelled DRAM-bound. Never label a row from about 35,000 to 540,000 creatures cache-resident: the old
  "540,000" knee counted only the 140-byte hot tier (08 section 2.4). Effort: L.
  Depends on: P5 (the complete world).

### WP-P6-07 TM3: speed work with targets
- Scope (`CONTINUE-BUILD.md:53`): behaviour-preserving speed work: v61 (the grid built twice on reorder
  ticks), v62 (the work-splitting policy duplicated), v67 (serial grid histogram at the default world),
  v69 (four full cell scans per tick), v70 (scratch reset of every slot), the 7 us cost of an empty stage
  on 8 threads; plus what TM2 names; plus the deferred items whose triggers TM2 may meet: TR-13 (bulk
  Philox), TR-14 (store packing in place), TR-15 (shared brains, copy on write), TR-17 (parallel birth
  commit). TR-16 (creatures that do not think every tick) changes behaviour and is the owner's decision.
- Rule: every change keeps the hash bit-identical (C-04, C-05, C-07 before and after) and shows its gain
  with C-18 (`STATUS_AND_PLAN.md:677`).
- Targets (proposed; you fix them after TM2 with a DECISION line, the owner may change them): (1) the
  100k lab reference on 8 workers back to the 54.7 ms per tick it had before worldgen
  (`PROGRESS.md:1599`); (2) then toward the 19.24 ms of the ABI-4 freeze (`ABI-V4-NOTICE.md:409`), as far
  as TM2 shows it is reachable without changing behaviour; (3) no stage slower in parallel than serial
  (`STATUS_AND_PLAN.md:682`). Today: 73.7 ms.
- DoD (can fail): each change has before and after medians, the hash unchanged, and an entry in the TM2
  table; the targets are met or the report says by how much they were missed and why. Effort: XL.
  Depends on: WP-P6-06.

---

## 11. P7 Milestone 7: the full audit A1 and the 5 mutants

Goal (`CONTINUE-BUILD.md:54`, P09): on the complete build, reproduce-first finders over every folder,
file and line; skeptics; the 5 negative mutants; repairs; a full re-run. Done when no confirmed finding is
open and each mutant is caught.

Size of the audit (counted 2026-11-18, `.c .h .asm .inc .def .py .html .cmake`, without
`tests\upstream`): core 71 files, 14,777 lines; kernels 8, 1,092; platform 4, 1,118; tools 18, 5,849;
view 3, 1,108; gpu 8, 1,565; tests 53, 23,741; demo 2, 1,552. Plus `docs\`.

### WP-P7-01 Finder packages
- One fresh session per package (section 1.7), each with a brief and a file list: (1) `core\az_abi.h`,
  the `.def` files, `az_types.h`; (2) engine core (`az_world.c`, `az_tick.c`, `az_grid.c`, `az_scan.c`,
  `az_field.c`, `az_store.c`, `az_snapshot.c`, `az_ledger.c`, `az_rng.c`, `az_fx.c`); (3) biology part 1
  (genome, brain, life, mate, tier1, tk_*); (4) biology part 2 (metab, chem, weather, worldgen, sense,
  physics); (5) game and levels (social, econ, colony, stats); (6) kernels and platform; (7) tools and
  view and demo; (8) gpu; (9) tests (can each test fail? one mutant per suspicious test); (10) docs
  against code (every claim in ARCHITECTURE.md, the ABI notice, the briefs and the handoff files checked
  against the code).
- DoD: ten `build\work\a1\<package>\findings.jsonl` files. Effort: XL.

### WP-P7-02 Skeptics and repairs
- Every CONFIRMED finding goes to a skeptic session; survivors are fixed by you, one file owner at a time,
  each with a failing-then-passing test. Effort: XL.
- DoD: every finding in the ten `build\work\a1\<package>\findings.jsonl` files has a skeptic verdict
  (CONFIRMED, REFUTED or NOT-REPRODUCED) from a session other than its finder; every CONFIRMED finding
  has a `fix` and a `test` field, and PROGRESS.md shows the test failing before the fix and passing after;
  `issues.py --check` passes with each one CLOSED or ANSWERED. Fails on any CONFIRMED finding without a
  test.

### WP-P7-03 The 5 mutants (GATE-04), defined natively
- **Canonical definition: 07 section 6.2** (`00-MASTER-PROMPT.md` 5.10 row 16). Build the mutants as
  07 says: scratch copies of the tree in `build\work\eval\mutants\<name>\tree\`, built in
  `...\<name>\build\`, no mutant code or `-DAZ_MUTANT_*` switch in the live tree; the table in
  `docs\eval\MUTANTS.md`; the definitions and detectors of 07's table (M-LEAKY: one energy quantum at one
  tick per run, caught by G-BOOKS-10K and every az_suite run; M-COSMETIC: strict lint W1 plus the E1-style
  contrast; M-ORACLE: P-BLIND; M-FROZEN: P-REVERSAL; M-SHUFFLED: P-DELAYED-CUE). The list below is the
  earlier draft of this file: keep its extra detectors (`tools\az_conform.c`, the sense firewall test,
  the tk_hop bank test) only as second detectors, never in place of 07's; its "every 800 ticks" Leaky
  variant may be a second Leaky case.
- Source: `docs\requirements\antilarp_mutants5.json` (v40 wording with float criteria). Native versions
  (exact books make some criteria stricter):
  1. **Leaky**: a planted loss of 1 matter unit (and, separately, 1 energy unit) once per creature every
     800 ticks in one module. Caught by: the per-tick `CHECK_EQ` in module tests and `az_run`'s exact
     comparison (exit 1). Under exact books any leak greater than 0 must be caught (not only 2e-11).
  2. **Cosmetic**: a planted "fake organ": a per-creature state (a memory word or a new column written in
     Tier 1) that evolves every tick and that nothing reads. Caught by: a new state-conformance check
     (`tools\az_conform.c` or a mode of az_suite): for every written hashed state element, zero it each
     tick and require a change in the other state at T; plus lint W1 and az_lesion INERT for bits.
  3. **Oracle**: a planted peek: sense reads the location of the world's richest food (or another
     creature's block). Caught by: the firewall test of WP-P1-02 item 4 and a blinded zero-cue trap (a
     world where the food position carries no sensory cue: intact creatures must not beat chance by the
     minimum effect; the oracle does).
  4. **Frozen**: plasticity that writes the same value every time. Caught by: a reversal-learning task in
     tier1 (reward location reverses at mid-run; plastic learners adapt, frozen ones do not). Note: today
     PLASTICITY is NO EFFECT at 64 ticks (2.1); if the intact engine fails the reversal task, that is a
     finding about the engine, not a mutant success.
  5. **Shuffled**: the associative-memory query shuffled across creatures (or memory words permuted each
     tick). Caught by: recall in a multi-pattern tk_hop bank collapses against intact (test_tk_hop holds
     the bank; the in-world version compares recall-driven behaviour).
- Files: `build\work\eval\mutants\make_mutants.py` and the five scratch trees (07 SB-13), the catching
  tests and tools, `docs\eval\MUTANTS.md`, and a ctest gate `gate04_mutants` (yours, added at a landing
  boundary) that runs the detectors against the mutant builds (in the background if long).
- DoD: 5 of 5 CAUGHT on the complete build, each by its named 07 detector, each detector passing on the
  clean tree; `gate04_mutants` green; 07 section 6.2's DoD. Effort: L.

### WP-P7-04 Full re-run
- C-01 (clean, in a new folder `build\clean-<date>`), C-02, C-03, C-04 to C-07, C-09, C-11, C-12, C-14,
  C-15, C-16, C-20, C-22, and every module's C-17 again.
- DoD: all pass; new hashes only with recorded causes. Effort: M.

### WP-P7-05 Audit report
- File: `docs\eval\A1-AUDIT.md`: finders, skeptics, findings with verdicts, fixes with tests, the mutant
  table, the re-run. DoD: no CONFIRMED finding open. Effort: S.

---

## 12. P8 Milestone 8: E1, E2, E3

Goal (`CONTINUE-BUILD.md:55-60`): the owner's suites "all over" on the complete build; tune on 1-99;
lock one preregistration per suite; run on 5001-5024 and 5101-5124. **The detail of every suite and item
lives in 07.** This section names the packages, their order and their gates.

### WP-P8-01 Preregistrations
- `docs\prereg\PREREG-<SUITE>.txt` per suite in 07 section 3.11's format (E1, E3 where a hypothesis
  exists, CQM25, BUTLIN14, THEORIES10, PASSES100, LIFE500, GATES13, MUTANTS5, CHAIN-01 v2 on reporting
  seeds), each with primary measure, direction, arms, seed block, minimum effect, Holm family; its SHA-256
  as one line in `docs\prereg\PINS.txt` (`00-MASTER-PROMPT.md` 5.10 row 3; earlier drafts said
  `<SUITE>.md` and `SHA256SUMS.txt`).
- DoD: every PINS.txt hash recorded (with the PROGRESS.md entry time) before its first reporting run;
  `Get-FileHash -LiteralPath "<file>" -Algorithm SHA256` of each PREREG file equals its PINS.txt line at
  the time of the run (az_audit.py checks it, 07 section 4.10). Effort: L.

### WP-P8-02 `tools\az_suite.c` and the gate runner
- az_lesion generalised (`CONTINUE-BUILD.md:57`): arms (intact, targeted lesion, size-matched sham,
  sham-cost, shuffled stream, yoked, random policy, neutral shadow), the non-ceiling rule, INCONCLUSIVE,
  more than 64 seeds where an item needs it, lesions beyond the 21 bits through `lesion_ext` (an ABI
  naming change, yours). A gate runner that exits non-zero on the first failure.
- Tests: `tests\test_statsutil.c` extended; planted analysis defects (Holm over the wrong family,
  unpaired seeds, a ceiling ignored).
- DoD: the DoDs of 07 SB-05, SB-06, SB-07 and SB-12; `tests\test_suite.c` catches each of the three
  planted analysis defects above (each run shown failing on the defect and passing without it); the
  gate runner exits non-zero on a planted failing gate and 0 on the clean tree. Effort: XL.

### WP-P8-03 E1: lesions on reporting seeds
- `az_suite` (or az_lesion) on 5101-5124 with replication on 5001-5024, every switch including the new
  TAXIS, THRUST and EAT. INERT is a defect. File: `docs\eval\E1-LESIONS.md`. Effort: M.
- DoD: `E1-LESIONS.md` holds both blocks' tables, every row with a 07 section 3.5 verdict; no row is
  INERT; the SHA-256 of `PREREG-E1.txt` at run time equals its `PINS.txt` line; the csv paths and the
  exact commands are listed; T-AUDIT (`az_audit.py`) agrees. 07 section 6.3's DoD.

### WP-P8-04 E3: the scaling benchmark
- `az_bench` from 1 to 1,000,000 creatures at 1, P and P+E threads, kernels 0 and 1: ticks per second,
  per-stage ms, cache budget, page faults, memory lock, load labelled; every row above the knee measured
  by ES-1 (M2) labelled DRAM-bound (until M2 exists: every row above about 35,000 creatures "beyond L2+L3
  by estimate (DRAM-bound)", 08 section 2.4); store sized so the top rows do not measure refusals (H-03).
  File: `docs\eval\BENCH-SCALE.md` with raw outputs under `build\work\eval\E3-<date>\` (07 SB-16's
  folder). Effort: L.
- DoD: every row of 07 section 6.4's grid is in BENCH-SCALE.md with its load label, its store refusals,
  its live count, ns per live creature per tick and its knee label; each row's raw file exists under
  `build\work\eval\E3-<date>\`; every run header shows the arena locked. Fails on a row without a raw
  file, a load label or a refusal count.

### WP-P8-05 E2: the trace matrix and the scored suites
- `docs\eval\TRACE.md` and `TRACE.csv`: every requirement id to a test or a reason code (NOT-BUILT,
  N/A-ENGINE, LAW-CONFLICT <law>, IDEA-BANK, OUTSIDE-ENGINE, DUPLICATE-OF <id>, SUPERSEDED-BY <id>,
  NEEDS-OWNER <decision>, INDICATOR-ONLY). Order (`CONTINUE-BUILD.md:56`): engineering gates and the 5
  mutants; E1, then E3; CQM-25; Butlin 14; the 10 theories; the 100 passes; the 500 checklist (about 10^5
  world runs, in the background with logs on Z:); rollups and trace-only sets; T-AUDIT last.
- Keep separate provenance where the old records disagree (the v40 export and the Grok Bot build on Combo
  A foraging, d = 0.733 against 0.857 over the same 112 seed ids, threshold 0.8; cause unresolved): no
  v40 result carries over (`CONTINUE-BUILD.md:55`). The three "100" lists cannot be added together (only
  46 of 100 same-number pairs share an item code; see 07 and `04-GOALS-AND-BENCHMARKS.md:41-45`).
- The idea-bank catalogs (07 section 6.15) are traced with reason code IDEA-BANK unless the owner picks
  items to score (10 OD-25).
- DoD: `python -I "$R\tools\az_trace.py" --check` exits 0 (every id of every catalog traced; no score
  column; no total); every TESTED row has its locked prereg, exactly one reporting run per block and its
  raw files; `az_audit.py --all` exits 0. Effort: XL.

### WP-P8-06 Held-out score
- The engine has no held-out score today. Every scored item reports its reporting-block result only;
  tuning-seed results are never mixed in. DoD: TRACE.csv has a column "seed block" for every verdict.
  Effort: S (inside WP-P8-05).

### WP-P8-07 Interim report
- `docs\handoff\INTERIM-REPORT.md` for the owner (`CONTINUE-BUILD.md:60`): the suites' verdicts, the
  chain result, the speed, the open items, and the question on the law-2 wording (G1). If he has not
  said yes to the law-2 wording, record "milestone 9 waits for OD-05", continue with the work phase 1
  allows (open register rows, viewer, TM3), then P10 (`00-MASTER-PROMPT.md` 4.2, 7.3, TC-01 and 5.10 row
  17: `CONTINUE-BUILD.md:61`'s "stop after milestone 8 and ask for it in the interim report" stops
  milestone 9, not the program; milestone 10 is still owed, `CONTINUE-BUILD.md:62`). Effort: S.
- DoD: `docs\handoff\INTERIM-REPORT.md` holds the parts of `00-MASTER-PROMPT.md` 7.3 (every suite's
  verdict table with indicator-only items marked, the chain result, the speed and viewer numbers, every
  G1 condition with its run id and PASS or FAIL, the open items) and quotes the OD-05 and OD-13 questions
  word for word from 10 section 3; it passes the text rule; the report date is in the OD-05 and OD-13
  register rows.

---

## 13. P9 Milestone 9: G1, then S1 to S4 and R2

Every substrate enters as a twin of the CPU C reference: the same `az_state_hash` every tick, exact
conservation, an off switch, a measured gain, and its row in `docs\TRIGGER-REGISTER.md` updated
(`TRIGGER-REGISTER.md:8-14`). A result that is not bit-exact feeds only outputs outside the hashed state.
Banned in every phase: the pagefile, any virtual RAM on NVMe, memory compression of state, disk-backed
mapped memory, GPU oversubscription, VMs, pretrained models.

### WP-P9-01 G1: the foundation gate
- Conditions (`STATUS_AND_PLAN.md:692`, `CONTINUE-BUILD.md:61`), each recorded in `docs\eval\G1-GATE.md`
  with its run, command and output tail:
  1. milestones 1 to 8 done (section 17 ticked);
  2. no stubs (`cmake` prints 22 real modules, 0 stubs);
  3. every finding closed or answered (`review_result.json` ids and every A1, P4 and module finding);
  4. A1 clean and 5 of 5 mutants caught;
  5. the invariants of `CONTINUE-BUILD.md:43` on both reference configs: threads 1, 2, 5, 8, 12, 16, 24
     (with `--ecores`, with and without `--no-pin`), reorder 1, 7, 31, 255, several split seeds, kernels 0
     and 1, a snapshot saved at tick k and resumed at another thread count, gcc against clang; every tick
     exact books, `field_underflow` 0, `dropped_events` 0, equal ledger chains;
  6. TM2 table and E3 measured;
  7. `--strict` lint and C-11 clean;
  8. the owner's yes on the law-2 wording (your proposed wording in PROGRESS.md; COMMON.md and the
     ARCHITECTURE law table change only after his yes, `CONTINUE-BUILD.md:87`).
- DoD: all eight recorded; without 8, record "milestone 9 waits for OD-05" and go to P10 (WP-P8-07).
  Effort: M.

### WP-P9-02 S1: locked DDR5 as a designed streaming tier (TR-01, TR-02)
- Trigger (TR-01, proposed threshold): E3 shows ns per creature per tick at 1,000,000 at least 1.5 times
  the value at 262,144.
- Work: prefetch of the next 128-creature block in every hot stage that walks slots; block ordering;
  populations above 1,000,000 up to the largest that fits locked (`az_world_bytes` at most half of the 48
  GB of RAM, TR-02, proposed); store sizing so large worlds evolve instead of refusing (H-03).
- Off switch: a run option (name proposed: `--stream 0|1` in az_run, az_bench, az_lesion, az_suite).
- DoD: same hash with the switch on and off on both references; exact books; 0 page faults after start-up
  on the largest run; gain in ms per tick measured with C-18; TR-01 and TR-02 rows closed with run ids.
  Effort: L.

### WP-P9-03 S1b: 2 MB pages (TR-03)
- Needs the owner to grant "Lock pages in memory" (D1c). Without it: record "blocked by D1c" and go on.
- DoD (when granted): same hash with and without large pages; gain measured (TR-03 proposed: at least 5
  percent at 1,000,000). Effort: S.

### WP-P9-04 S2: the GPU whole tick (TR-04, TR-05, TR-18; closes v76)
- Order: (1) the GPU twin of the per-creature tile brain, equal to `az_brain_step_c` on at least 1,000,000
  random brains and steps (TR-18); (2) every stage of the tick as a device kernel, clang-built PTX loaded
  by the driver (no nvcc, ptxas or nvrtc exists, `CONTINUE-BUILD.md:29`); (3) a GPU-resident world; (4)
  a deterministic CPU+GPU split.
- Off switch: `--gpu 0|1` (name proposed).
- Test: `--hash-every 1` on CPU and GPU runs of both references; the first differing tick is a defect.
- DoD: GPU hash equal to the CPU hash every tick for both references over all their ticks; exact books;
  measured ms per tick; TR-04, TR-05, TR-18 closed. Effort: XL.

### WP-P9-05 S3: the NPU probe (TR-06)
- Present (checked 2026-11-17): `ze_loader.dll`, `DirectML.dll` 1.15.6, and in the NPU driver store
  `npu_level_zero_umd.dll`, `npu_d3d12_umd.dll`, `openvino_intel_npu_compiler.dll`; no Level Zero headers
  under `Z:\Tools` (`TRIGGER-REGISTER.md:26`). Without headers the probe may need an install: that is
  TR-10 and the owner's yes. Default: probe only through what is present; if it cannot be done, record
  "blocked by TR-10" and ask.
- Question: does the integer brain step run bit-exact through the NPU over 1,000,000 steps?
- Law: NPU code is a graph the driver compiles, not C23, NASM or PTX; no NPU result enters world state
  without the owner's yes on that reading of law 1. If not bit-exact, the NPU serves only analytics
  outside the hashed state.
- DoD: the probe result recorded and the owner asked. Effort: M.

### WP-P9-06 S4: the placement scheduler (TR-07)
- Trigger: at least two substrates passed their twin rows. Work: place each stage on P cores, E cores, GPU
  or NPU by measured cost (`prof_ns`); placement never changes the hash.
- DoD: every placement gives the CPU hash; a table of placement against ms per tick. Effort: L.

### WP-P9-07 R2: audit of the new code and E1 to E3 per substrate (TR-08)
- A short reproduce-first audit (section 1.7) of the new host, GPU and NPU code; E1 to E3 re-run per
  substrate. DoD: no CONFIRMED finding open; E1 to E3 tables per substrate. Effort: L.

### WP-P9-08 Trigger register upkeep
- Every S package updates its TR row; rows never triggered stay open with the reason. Effort: S.
- DoD: every TR-01..TR-18 row of `docs\TRIGGER-REGISTER.md` has a state (CLOSED with a run id, OPEN, or
  DEFERRED-TR) and a trigger that a measurement can decide (a number, a command and the threshold), and
  the matching 09 TR register rows agree with it. Fails on a row with no state or an untestable trigger.

---

## 14. P10 Milestone 10: FINAL_REPORT.md

### WP-P10-01 The report
- File: `docs\handoff\FINAL_REPORT.md`. Contents, in this order:
  1. For the owner, in plain words: what the engine is and does, what was shown, what was not.
  2. What is built: every module, twin, tool and the viewer, with its status (real, reviewed by whom).
  3. Verified claims: each with a run id, the exact command, the output tail and the log path.
  4. Not verified, and why (for example: timings never on a quiet machine unless the owner gave a window;
     the NPU path if blocked).
  5. The decisive chain (CHAIN-01): the result exactly as preregistered.
  6. The owner's numbers: a verdict table per suite (link to `docs\eval\`), indicator-only items marked
     so; INCONCLUSIVE items listed.
  7. The lesion table on reporting seeds (E1).
  8. Speed: E3, TM2, TM3 before and after, the viewer's frames per second.
  9. Scaling: S1 to S4 and R2 results, or why they were not done (owner decisions).
  10. The laws: one row per law with how it is enforced and the gate that checks it.
  11. Decisions taken by default and the owner's decisions, with dates.
  12. Deviations from any preregistration.
  13. Known limits (section 18 items still open, with reasons).
  14. Spend.
  15. How to reproduce: the C-xx commands of this file as they stand at the end.
  16. Prior-art statement before any novelty claim: scope and date of the search (Zenodo, web, GitHub, by
      distinctive numbers), or "no novelty claimed".
  17. The indicator-only statement: no claim of consciousness, sentience, life, AGI or quantum effects.
- DoD (can fail): every claim in the report has a run id; a fresh-context session given only the report
  and the tree re-runs three claims chosen at random and gets the same outputs. Effort: M.

### WP-P10-02 Status and records
- C-19 (status regenerated), C-13 (snapshot `final`), PROGRESS.md final entry, OWNER-WORDS rows set,
  TRIGGER-REGISTER rows current. DoD: STATUS section 8 and PROGRESS.md agree. Effort: S.

### WP-P10-03 Hand-back
- A short report to the owner (at most 15 lines, `00-PROMPT.md` section 10) with the path of the final
  report; the memory note update is the desktop session's job if your environment cannot write it.
  Effort: S.

---

## 15. Dependency graph

Arrows mean "must be done before". Items on one line joined by `|` may run in either order.

```
P0 (WP-P0-01..07)
 |-> WP-P6-01 viewer (optional early; touches no simulation file)
 |-> WP-P6-03 GPU fixes | WP-P6-04 TM1 note   (any time after P0)
 v
P1 sense: P1-01 design -> P1-02 module+test -> P1-03 reviews -> P1-04 landing
 v
P2 physics: P2-01 design -> P2-02 C module -> P2-03 NASM twin -> P2-04 reviews -> P2-05 landing
 v
P3 stats: P3-01 -> P3-02 -> P3-03
 v
P4 closure: P4-01 lint W1+strict | P4-02 stub findings | P4-03 develop_body | P4-04 worldgen limits
            -> P4-05 seam review -> P4-06 independent review of all 13 -> P4-07 clang+skel -> P4-08 horizons
            P4-09 law texts (any time in P4)
 v
P5 chain: P5-01 world+links+EAT lesion -> P5-02 prereg (SHA-256) -> P5-03 held-out run -> P5-04 illustration
 v
P6: P6-06 TM2 -> P6-07 TM3 ; P6-06 -> P6-02 scan twin ; P6-01 + P5-01 -> P6-05
 v
P7 A1: P7-01 finders -> P7-02 skeptics+repairs ; P7-03 five mutants ; -> P7-04 full re-run -> P7-05 report
 v
P8: P8-01 preregs -> P8-02 az_suite -> P8-03 E1 -> P8-04 E3 -> P8-05 E2 suites (+P8-06) -> P8-07 interim
 v
P9: P9-01 G1 (needs owner's law-2 yes) -> P9-02 S1 | P9-03 S1b (needs D1c) | P9-04 S2 | P9-05 S3 (needs owner)
     -> P9-06 S4 (needs 2 substrates) -> P9-07 R2 ; P9-08 throughout
 v
P10: P10-01 report -> P10-02 records -> P10-03 hand-back
```

If the owner does not say yes to the law-2 wording, the path goes P8-07 -> P10 and the report says that
milestone 9 waits for him.

## 16. Critical path and order

The critical path (each step waits for the one before): P0 -> P1 -> P2 (XL, the twin) -> P3 -> P4 (XL,
the independent review) -> P5 (XL, the chain) -> TM2 -> TM3 (XL) -> P7 (XL) -> P8 (XL, the 500 checklist
alone is about 10^5 world runs) -> G1 -> S2 (XL) -> S4 -> R2 -> P10.

Order rules:
1. One package at a time on the critical path; side packages (viewer, GPU fixes, TM1) fill waits (long
   background runs).
2. Lane C (this critical path) first; the small PQ quick wins of 08 in week 1; RV-A in the waits of lane
   C (`00-MASTER-PROMPT.md` 4.1 and 5.10 row 18; this replaces "the viewer first if file 08 is
   complete").
3. Never start a package whose inputs are not landed: physics waits for sense, the chain waits for all
   three modules and the closure, TM2 waits for the complete world.
4. A failure of a DoD stops that path: fix it or record PARTIAL and the reason; do not start the next
   package on the critical path on top of a failed one.
5. Snapshot (C-13) before every package that changes `core\`.

## 17. Milestone acceptance checklist

Tick each box with a PROGRESS.md entry that holds its evidence.

**Milestone 5 (W2) is done when:**
- [ ] sense, physics (with `az_kin_axis_avx2`) and stats are real; `cmake` prints 22 real modules, 0
      stubs, 4 twins (or 5 with the scan twin later).
- [ ] C-09 (`--strict`) exits 0 and is a ctest gate.
- [ ] every trait MATTERS in `test_traits`; every lesion bit (21 plus TAXIS, THRUST, EAT) honoured and
      named in a test.
- [ ] C-02, C-03, C-06, C-07, C-14, C-15, C-20 pass; books exact in every module test.
- [ ] planted defects of sense, physics, stats and the chain tool: all CAUGHT.
- [ ] v02, v03, v05, v06, v08, v47 each FIXED with a test.
- [ ] founder bodies follow the fitted genome (`az_develop_body`).
- [ ] the worldgen limits measured and fixed or recorded.
- [ ] SEAMS.md: no CONFIRMED seam finding open.
- [ ] the independent review of all 13 modules and the engine core done by fresh sessions; every
      CONFIRMED finding fixed.
- [ ] lesion horizons table; no INERT switch at any horizon.
- [ ] CHAIN-01 preregistered (SHA-256) and run on seeds 6001-6024; reported exactly as preregistered.
- [ ] new reference hashes in `tests\golden_hashes.txt` with causes; status regenerated; snapshot.

**Milestone 6 (W3 + track M) is done when:**
- [ ] the real-time viewer shows frames per second at the targets of 08, with the hash check on.
- [ ] the scan twin is bit-identical with its gain recorded, or dropped by a recorded DECISION.
- [ ] v23, v26, v57, v25 fixed with tests where a test can fail; test_gpu passes.
- [ ] TM1 note written with sources.
- [ ] TM2 table: every stage with bytes, cycles per creature and its bottleneck.
- [ ] TM3: each change hash-identical with its gain; the targets met or the miss explained.

**Milestone 7 (A1) is done when:**
- [ ] ten finder packages and their skeptic passes done by fresh sessions.
- [ ] no CONFIRMED finding open.
- [ ] Leaky, Cosmetic, Oracle, Frozen and Shuffled: 5 of 5 CAUGHT, `gate04_mutants` green.
- [ ] full re-run on a clean build passes; `docs\eval\A1-AUDIT.md` written.

**Milestone 8 (E1-E3) is done when:**
- [ ] every suite has a locked preregistration with its SHA-256.
- [ ] `az_suite` and the gate runner exist and are tested.
- [ ] E1 table on 5101-5124 with replication on 5001-5024; no INERT.
- [ ] E3 `BENCH-SCALE.md` from 1 to 1,000,000 with load labels and DRAM-bound labels.
- [ ] `TRACE.md` and `TRACE.csv` cover every requirement id with a verdict or a reason code; scorecards
      written; indicator-only items labelled.
- [ ] interim report given to the owner.

**Milestone 9 (G1, S1-S4, R2) is done when:**
- [ ] G1 recorded with all eight conditions, including the owner's yes.
- [ ] each substrate that went in has: equal hash every tick, exact books, an off switch, a measured gain,
      a closed TR row; each that did not go in has the reason.
- [ ] S4 placement table; R2 audit clean; E1 to E3 per substrate.

**Milestone 10 (R1) is done when:**
- [ ] `FINAL_REPORT.md` holds the 17 parts of WP-P10-01; three random claims re-run by a fresh session.
- [ ] status regenerated; final snapshot; the owner has the report.

## 18. Holes, gaps, blind spots and dilemmas register

Each row names where it is handled. A row is closed only with evidence in PROGRESS.md.

| Id | Hole, gap, blind spot or dilemma | Source | Handled in |
|---|---|---|---|
| H-01 | No world-level selection result exists | `STATUS_AND_PLAN.md:59` | P5 |
| H-02 | The senses stub leaves directional inputs neutral, so no creature can follow a gradient | stub `core\stubs\az_sense.c`; previous agent's analysis | P1 |
| H-03 | The 1M world recorded 380,865 store refusals and reached only generation 1: scale, not evolution | previous agent's analysis | WP-P6-06, WP-P8-04, WP-P9-02 (store sizing; label refusals) |
| H-04 | The physics stub carries bodies through bedrock and ignores gravity and density | `PROGRESS.md:1603` | P2 |
| H-05 | 14 lesion switches NO EFFECT at 64 ticks, including all four tier-1 kernels: possible theatre or too short a horizon | `PROGRESS.md:1595-1598` | WP-P4-08, P7 |
| H-06 | WEATHER lost its effect after worldgen | `PROGRESS.md:1597` | WP-P4-04 (gradients), WP-P4-08 |
| H-07 | GRAVITY changes the state on 24 of 24 seeds but is NO EFFECT | `PROGRESS.md:1597` | P2, L-15 |
| H-08 | Founder bodies come from the base genome | `DESIGN.md:998-1004` | WP-P4-03 |
| H-09 | Weather relaxes the depth and latitude gradients | `PROGRESS.md:1604` | WP-P4-04 |
| H-10 | Crowds of founders boil a few ocean cells | `PROGRESS.md:1605` | WP-P4-04 |
| H-11 | Some worlds have no founder in the air | `PROGRESS.md:1606` | WP-P4-04 |
| H-12 | Worldgen's fixed structure against "nothing fixed" | `PROGRESS.md:1607-1609`, `02-STAGES-AND-STATUS.md:55` | WP-P4-04, owner O-09 |
| H-13 | Only 23 parameter slots left | 2.2 item 1 | WP-P1-01, WP-P2-01, WP-P3-01 |
| H-14 | `TAXIS`, `CURVE` are row traits needed every tick (rule D8) | 2.2 item 2 | DQ-SEN-1 |
| H-15 | `archetype` hashed but read by nothing in the world (Cosmetic risk) | 2.2 item 3 | DQ-STA-1 |
| H-16 | Interoceptive table 13 channels, brief says ten | 2.2 item 4 | WP-P1-01 |
| H-17 | `AzStats` fixed bins against "no fixed bins" | 2.2 item 5 | DQ-STA-2 |
| H-18 | Stub findings v02, v03, v05, v06, v08, v47 have no recorded closure | `CONTINUE-BUILD.md:52` | WP-P4-02 |
| H-19 | No module except worldgen was reviewed by anyone but its author | `PROGRESS.md:1497` | WP-P4-06 |
| H-20 | One agent cannot review itself independently | section 1.7 | section 1.7, owner O-10 |
| H-21 | The ABI notice names `tests\test_traits.c`, which does not exist (it is in `test_life.c:738`) | `ABI-V4-NOTICE.md:76`, `tests\test_life.c:738` | WP-P4-05 |
| H-22 | Kinetic energy is not in the books | DQ-PHY-5 | WP-P2-01 (documented choice) |
| H-23 | Partial rock (`c_solid` below 255) has no defined effect | 2.2 item 7 | DQ-PHY-2 |
| H-24 | The chain's last links (intake -> offspring) cannot be lesioned without breaking the books | WP-P5-01 | WP-P5-01 (dose-response; stated limit) |
| H-25 | Using a reporting block in milestone 5 would burn it | `CONTINUE-BUILD.md:55` | 1.6 (held-out 6001-6024) |
| H-26 | COMMON law 6 and the ARCHITECTURE "Deterministic" row lag wild mode | `02-STAGES-AND-STATUS.md:53` | WP-P4-09 |
| H-27 | clang_check, m_check and the stubs-only build not run since wave 2 or since their Z: edits | `STATUS_AND_PLAN.md:721`, `:726-727` | WP-P0-02, WP-P4-07 |
| H-28 | run_tests.ps1 last ran before worldgen | `STATUS_AND_PLAN.md:718` | WP-P0-02 |
| H-29 | The demo shows pictures, not frames | section 2.3 | WP-P6-01, 08 |
| H-30 | The engine is 3.8 times slower at 100k than at the ABI-4 freeze (73.7 against 19.24 ms) | 2.1 | WP-P6-06, WP-P6-07 |
| H-31 | The Philox AVX2 twin is built but no stage calls bulk Philox | `TRIGGER-REGISTER.md:33` | WP-P6-07 (TR-13) |
| H-32 | Store packing costs about 250 ms at 1M every 16 ticks | `TRIGGER-REGISTER.md:34` | WP-P6-07 (TR-14) |
| H-33 | The GPU is not in the tick; the ARCHITECTURE row was reworded, not closed (v76) | `PROGRESS.md:54` | WP-P9-04 |
| H-34 | Older snapshots and the demo's saved worlds stop loading at each new parameter | `02-STAGES-AND-STATUS.md:56` | L-14 (say so each time) |
| H-35 | The engine matrix hash is printed, not pinned | `STATUS_AND_PLAN.md:719` | WP-P7-04 (pin it once the build is complete, or record why not) |
| H-36 | Timings were never taken on a quiet machine | `PROGRESS.md:1493` | every timing labelled; owner O-04 |
| H-37 | No git commit: snapshots are the only recovery; GATE-01 blocked | `02-STAGES-AND-STATUS.md:36` | C-13 before every core change; owner O-01 |
| H-38 | Other agents may edit the same tree | `CONTINUE-BUILD.md:35` | 1.2 |
| H-39 | The spend of this run cannot be measured by spend.py | section 1.8 | WP-P0-07 |
| H-40 | The Combo A foraging records disagree (d 0.733 against 0.857, threshold 0.8) | previous agent's analysis | WP-P8-05 (separate provenance; no carry-over) |
| H-41 | The three "100" lists cannot be added | previous agent's analysis; `04-GOALS-AND-BENCHMARKS.md:41-45` | WP-P8-05, 07 |
| H-42 | The native engine has no held-out score | previous agent's analysis | WP-P5-03, WP-P8-06 |
| H-43 | Selection hints so far are weak (display preference raised the ornament shift on 5 of 8 seeds; thermal sorting reached its predicted direction on 2 of 4) | previous agent's analysis | P5 and P8 report them as hints only, never as results |
| H-44 | Wild is the default, so an evaluation run without `--lab` is not repeatable | `ABI-V4-NOTICE.md:371-399` | 1.1; az_suite refuses a wild world (WP-P8-02) |
| H-45 | The NPU probe may need headers that are not installed | `TRIGGER-REGISTER.md:26` | WP-P9-05, owner O-07 |
| H-46 | The owner's newest prompts may not all be in PROMPTS.md (numbers decided 2026-11-18: P01-P55; the four older desktop records were numbered P52-P55 the same day) | WP-P0-06 | WP-P0-06 |
| H-47 | The demo draws each picture on the thread that ticks: in the owner's 1M world the world stands still about 4.3 s per picture (09 DEMO-15, 08 PB-36) | 08 section 2.5 | WP-P6-01 (08 PQ-5; the copy-cost budget of RV-A9 target case 2) |
| H-48 | About 31 percent of the owner's 1M tick (127.2 of 414.3 ms) is in no named stage time (08 PB-37) | 08 section 2.5 | WP-P6-06 (08 ES-1) |

## 19. Owner decisions this program needs

Defaults from `CONTINUE-BUILD.md:84-87` and `02-STAGES-AND-STATUS.md:33-56`. Take the default, record
it, go on; ask in each report.

| Id | Decision | Default | Blocks |
|---|---|---|---|
| O-01 | First git commit (D1a) | none; snapshots | GATE-01 only |
| O-02 | Licence (D1b) | none; notices file only | nothing |
| O-03 | "Lock pages in memory" for 2 MB pages (D1c) | VirtualLock without large pages | WP-P9-03 |
| O-04 | A quiet-machine window for TM2 and E3 (D2) | measure and label the load | nothing (labels) |
| O-05 | Installs: CUDA toolkit, WPA, OpenVINO (D1d, TR-10) | none; list what is missing | parts of TM2, WP-P9-05 |
| O-06 | The law-2 wording for phase 2 (D2) | phase 1 holds | all of P9 |
| O-07 | The NPU path (D2) | probe only; no NPU result in world state | WP-P9-05 |
| O-08 | Classification feeding back into the world | no (DQ-STA-1 (b)) | nothing |
| O-09 | Worldgen's fixed structure as parameters | recorded as limits | nothing |
| O-10 | Fresh-session reviewers paid from his credit | yes within the stop line; ask | WP-P4-06, P7 |
| O-11 | 64 paired seeds for the 500 checklist | the 48 preregistered seeds, deviation recorded | nothing |
| O-12 | TR-16, creatures that do not think every tick (biology) | not done | nothing |
| O-13 | Sex dimorphism from the sex locus (v09) | marker loci never feed traits | nothing |
| O-14 | Delete `build\dropped-uefi-2026-11-17` | leave it | nothing |

## 20. Definition of total completion

The project is complete when all of these hold at once, each with evidence in PROGRESS.md and in
`FINAL_REPORT.md`:

1. Every milestone box of section 17 is ticked (milestone 9 either done or explicitly waiting on the
   owner's recorded answer to O-06, with the report saying so).
2. No stub remains; `--strict` lint passes as a ctest gate; every trait matters; every lesion bit is
   honoured, tested and not INERT on the reporting seeds.
3. The full ctest, the determinism matrix on both references, the second compiler and the UBSan build
   pass on a clean build; books exact every tick; `field_underflow` and `dropped_events` 0.
4. Every finding (review, module reviews, seams, A1, R2) is FIXED with a test or ANSWERED with a reason;
   no CONFIRMED finding open; 5 of 5 mutants caught.
5. The decisive chain was preregistered and run on held-out seeds, and its result is reported exactly as
   preregistered (shown or not shown; either is a completed result).
6. Every requirement id of the owner's suites has a verdict on a reporting block or a reason code in
   `TRACE.csv`; every preregistration's SHA-256 matches its file.
7. The viewer shows frames per second at the targets of 08 with the observer check on; the engine's speed
   work met its targets or the report says by how much it missed and why.
8. Every section 18 row is closed or carried into the final report as a known limit with its reason.
9. Every owner decision of section 19 is either answered by him or recorded as taken by default.
10. `FINAL_REPORT.md` exists with its 17 parts, three random claims reproduced by a fresh session, the
    status regenerated, a final snapshot taken, and the owner told where it is.

"Done" never means "green tests" alone: a module or milestone is done only after the reproduce-first
review of section 1.7 agrees.

## 21. What this file could not resolve

- **File 08 did not exist when this file was finished** (checked 2026-11-18; 07, 09 and 10 appeared while
  it was written and are named in 0.1, but were not read for this file, so this file does not check its
  ids or targets against theirs). WP-P6-01 points to 08 by number; read the folder listing for its name.
  If it is missing, use the fallbacks of 0.1. Where 07, 09 or 10 and this file disagree, record the
  difference in PROGRESS.md and follow the more specific one for its topic (07 for the suites, 08 for the
  viewer, this file for order and done-criteria).
- **The owner's OpenAI credit** for the next run is not in any file read here (1.8).
- **Whether v02, v03, v05, v06, v08 and v47 are closed** in the landed modules: no file read for this
  program records it (WP-P4-02).
- **Whether `az_bench` sizes the store for 1,000,000 creatures**: not checked (WP-P6-06).
- **Whether Windows Performance Recorder can record hardware counters** in a non-elevated session on this
  machine: not checked (WP-P6-06).
- **The exact names of the run options** (`--stream`, `--gpu`) are proposals; you choose and record them.
  The lesion names `AZ_LESION_TAXIS`, `AZ_LESION_THRUST` and `AZ_LESION_EAT` are fixed (07 section 4.4;
  `00-MASTER-PROMPT.md` 5.10 row 20).
- **The numeric defaults of the new world parameters** of sense, physics and stats: they are measured on
  seeds 1-49 by you and recorded with the measurement, never guessed.
- **The targets of TM3** are proposed from two measured historical figures (54.7 and 19.24 ms); whether
  the second is reachable without changing behaviour is only known after TM2.
- **The NPU probe** may be impossible without an install (TR-10); the owner decides.
- **Independent reviewers** need sessions this program cannot start for you; if none can be started, the
  owner is asked (O-10) and the modules stay "reviewed by author only". `00-MASTER-PROMPT.md` 5.8 now
  says how to try a non-interactive Codex run first, and 4.3 lets the gate pass as PASS-PENDING-OWNER.
- **Fixer pass, 2026-11-18.** Changed here: L-16b (a cost budget per module), section 1.6 (held-out
  blocks 6001-6024 and 6101-6124 instead of 50-73 and 74-99), WP-P0-01, -02 (C-04 with `--kernels 1`)
  and -05 (seed survey), DoDs for WP-P2-04, P3-01, P5-04, P7-02, P8-01 to P8-05, P8-07 and P9-08,
  WP-P5-02 and P8-01 file names (07's `PREREG-<SUITE>.txt` and `PINS.txt`), WP-P6-06 and P8-04 (the
  knee), WP-P7-03 (07 section 6.2 canonical), WP-P8-07 and P9-01 (no stop of the program), section 16
  rule 2 (lane C first). The lesion name `AZ_LESION_EAT` is fixed now (07 section 4.4).
- **Lead's errata pass, 2026-11-18.** Changed here: C-11 and L-09 (the text rule is checked with
  `--since` on your own files, not as "0 violations" over the whole root), WP-P3-03 (three reviews, as
  L-11 requires), section 0.2 (P51 byte for byte), WP-P0-06 and H-46 (the owner's words span P01-P55),
  section 2.3 and WP-P6-01 (the owner's own 1M world, logged), WP-P0-01 (the errata of master section 12
  were applied on 2026-11-18, so the first session verifies them instead of applying them).
- **Second fixer pass, 2026-11-18.** Section 2.3 now cites the frame-226 record
  (`demo_owner_world_2026-11-18_f226.json`) and gives the picture period as about 29 s by arithmetic (the
  earlier "20 to 30 s" rested on a misread 40 s); H-47 (DEMO-15, PB-36) and H-48 (PB-37) added; the WP-P8-01
  DoD hashes with `Get-FileHash -LiteralPath`; the tool-version line uses `python -I`.
