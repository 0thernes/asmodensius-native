# 09 Issues, gaps and blindspots: the closure program

> Written 2026-11-18 (read-only pass, nothing built, run or edited outside this file) for the next
> agent (OpenAI GPT Astra 6.2, in Codex CLI on this machine or in a chat that sees only this package).
> Every claim cites `path:line` under the project root or a log file. Line numbers are as of this
> writing; files grow. Where a number is not known, the row says who measures or decides it and how.
> Root: `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native`. In commands, `$A` is that root:
> `$A = 'Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native'` (single quotes keep the brackets literal).

## Table of contents

- [0. What this file is and how to use it](#0-what-this-file-is-and-how-to-use-it)
- [1. The state this file was written against](#1-the-state-this-file-was-written-against)
- [2. Codes: severity, area, owner, milestone, effort, state](#2-codes-severity-area-owner-milestone-effort-state)
- [3. The issue register: file, format and validator](#3-the-issue-register-file-format-and-validator)
- [4. Triage rules](#4-triage-rules)
- [5. The harvested issue register](#5-the-harvested-issue-register)
  - [5.1 Review findings still marked open (RF)](#51-review-findings-still-marked-open-rf)
  - [5.2 Lint and strict lint (LNT)](#52-lint-and-strict-lint-lnt)
  - [5.3 The three stubs and the module seams (SEAM)](#53-the-three-stubs-and-the-module-seams-seam)
  - [5.4 Limits recorded by the nine earlier wave-2 modules (MOD)](#54-limits-recorded-by-the-nine-earlier-wave-2-modules-mod)
  - [5.5 Worldgen limits (WG)](#55-worldgen-limits-wg)
  - [5.6 Evidence and science (EVD)](#56-evidence-and-science-evd)
  - [5.7 Performance and scale (PRF)](#57-performance-and-scale-prf)
  - [5.8 The live demo: pictures and ticks, not frames per second (DEMO)](#58-the-live-demo-pictures-and-ticks-not-frames-per-second-demo)
  - [5.9 GPU and other substrates (GPU)](#59-gpu-and-other-substrates-gpu)
  - [5.10 Verification process gaps (VER)](#510-verification-process-gaps-ver)
  - [5.11 Repository, data safety and toolchain (REP)](#511-repository-data-safety-and-toolchain-rep)
  - [5.12 Documents and the text rule (DOC)](#512-documents-and-the-text-rule-doc)
  - [5.13 Owner decisions pending (OWN)](#513-owner-decisions-pending-own)
  - [5.14 Owner-words rows still open (OW)](#514-owner-words-rows-still-open-ow)
  - [5.15 Trigger register items (TR)](#515-trigger-register-items-tr)
- [6. Blindspot analysis (BS)](#6-blindspot-analysis-bs)
- [7. The audit loop](#7-the-audit-loop)
- [8. Recurring checks at the end of every milestone](#8-recurring-checks-at-the-end-of-every-milestone)
- [9. "Nothing left open": the definition and the closure order](#9-nothing-left-open-the-definition-and-the-closure-order)
- [10. Closure tasks (CL)](#10-closure-tasks-cl)
- [11. What this file could not resolve](#11-what-this-file-could-not-resolve)
- [12. Sources read for this file](#12-sources-read-for-this-file)

---

## 0. What this file is and how to use it

This is the closure program for every known bug, error, issue, problem, concern, dilemma, hole, gap
and blindspot of Asmodensius-Native. It has four parts:

1. A **harvested issue register** (section 5): every open item found in the project's own records,
   each with an id, a source, a severity, an area, the failing scenario or risk, the fix or the
   decision needed, the test that will prove it, the owner, the milestone and an effort estimate.
2. A **blindspot analysis** (section 6): what nobody has checked yet, or what could be wrong even
   though every gate is green.
3. The **process** (sections 7 to 9): the audit loop, the recurring checks, the triage rules and the
   definition of "nothing left open".
4. **Closure tasks** (section 10): the concrete work that turns this file into a living register in
   `docs\eval\` and closes it milestone by milestone.

How to use it:
- First hour: do CL-01 (create the register from section 5) and CL-02 (the validator). Do not start
  fixing before the register exists; otherwise items get lost again.
- Every row in section 5 becomes one line of `docs\eval\ISSUES.csv`. Keep the ids. New items get new
  ids; ids are never reused.
- The milestone documents of this package (see `00-PROMPT.md` section 6 and `02-STAGES-AND-STATUS.md`)
  say what to build. This file says what must be true before anything is called done.
- The owner's laws (`05-LAWS-AND-REQUIREMENTS.md`, `docs\agents\COMMON.md`) are hard constraints on
  every fix here. Where a row touches a law, its "Fix" cell says how the fix honours it.

Relation to the other files of this package (written at the same time; read them for the design,
use this file for the state):
- `06-COMPLETION-PROGRAM.md`: the work packages of milestones 5 to 10. Its section 18 maps the rows
  of this register that its packages meet. When a package closes a row, the row's `evidence` cites
  the package id.
- `07-SUITES-AND-BENCHMARKS-PROTOCOL.md`: the milestone-8 protocol (prereg, controls, seeds,
  statistics). EVD rows here are the gaps; 07 is how they are closed.
- `08-PERFORMANCE-AND-REALTIME-VIEWER.md`: the speed work and the real-time viewer design (tracks PQ
  and RV). DEMO and PRF rows here track state; 08's design wins where the two differ. Rough map:
  DEMO-02 is PQ-0/PQ-1, DEMO-03 is PQ-2/PQ-5, DEMO-04 is PQ-3/PQ-6 and track RV-A, DEMO-05 is PQ-4,
  DEMO-15 is PQ-5 plus RV-A9 target case 2.
  08 section 2.4 also corrects the cache arithmetic (blind spot PB-07 there; see PRF-02 here).
- `10-DECISIONS-AND-DILEMMAS.md`: the owner decisions (OD ids) and dilemmas (DL ids) in full. OWN
  rows here map to them: OWN-01 OD-01, OWN-02 OD-02, OWN-03 OD-03, OWN-04 OD-04, OWN-05 OD-05,
  OWN-06 OD-07, OWN-07 OD-22, OWN-09 OD-16 and OD-17 (viewer technology; the frame-rate target itself
  is new here), OWN-10 OD-14, OWN-13 OD-10, OWN-14 OD-08, OWN-15 OD-06; REP-03 relates to OD-18 and
  VER-07 to OD-19. Where the default text differs, 10 is the source for the decision, and this
  register records the row's state.
- ID prefixes are distinct across the files except TR-nn, which everywhere means the row of
  `docs\TRIGGER-REGISTER.md` with that number.

The owner asked on 2026-11-18 (P51, his newest message): "I notice it runs on PICTURES and TICS. Not
Frames Per Second. Why? Can that be fixed? It's really slow." The short answer is in section 5.8
(DEMO-01): the world advances in ticks (one full step of every creature, with exact books); the demo
shows a picture only every few ticks, and every picture goes through a slow chain (single-threaded
software drawing, an uncompressed 1.5 MB PNG, a disk write, a Python server, and a browser that asks
only every 250 ms). Yes, it can be fixed; in a small world the fix is a different display path, not a
faster world. In the owner's own world of 1,000,416 creatures the world itself is slow too (about 2.4
ticks per second) and stands still about 4.3 s for each picture (DEMO-15), so there the engine work of
08 section 8 is needed as well. DEMO-02 to DEMO-10 and DEMO-15 are the tasks.

## 1. The state this file was written against

Verified 2026-11-18 (source: the task brief of this package, `build\work\PROGRESS.md:1545-1611`,
`build\work\logs\wg_ref100k_t8.txt`, `build\work\logs\wg_ref1m.txt`,
`build\work\logs\lesion_z_worldgen_1_24.txt`):

- Build `build\z`: 19 real modules, 3 stubs (sense, physics, stats), 3 twins, 0 warnings.
- ctest: 46 of 46 (33 test programs, 13 gates) in 253 s.
- `lint_core`: 0 errors, 3 warnings. `lint_core --strict`: fails with 3 findings (section 5.2).
- Reference worlds (lab): 100k `73f1e9e013f8db50` (1, 8, 24 workers), 1M `5d9c02d2c7dd5fb1`
  (`--store-units 52000000`), engine matrix `24c923001a46b122`.
- Lesion table (`--seeds 1-24`, tuning seeds, exploratory): 7 EFFECT, 14 NO EFFECT, 0 INERT.
- Speed: 100k 73.7 ms/tick on 8 workers (13.6 ticks/s), 59.9 ms on 24 with E cores, 372 ms on 1;
  1M 1339 ms/tick. Added later on 2026-11-18 (the lead's pass): the owner's own wild demo world at
  1,000,416 creatures, read at 19:40:56 UTC (frame 226), ran 414.3 ms/tick (about 2.4 ticks/s) and
  4,261.3 ms per 1600x900 picture (`build\work\logs\demo_owner_world_2026-11-18_f226.json`; 08 section 2.5).
- Git: the repository exists and has no commit (`git status`: "your current branch 'main' does not
  have any commits yet"; every file untracked).
- Text rule: 85 files under `docs\` fail the check (55 in `docs\research`, 28 in `docs\requirements`,
  2 in `docs\handoff`: `PROMPTS.md` and `PROMPTS.xml`); counted read-only with the same rules as
  `build\work\scripts\check_text.py:17-40` (since its E-10 rewrite of 2026-11-18 the rules are at
  `:51-64`, `problems()`; the whole root counted 351 files checked, 85 with violations, and since the
  PROMPTS exemption of the second fixer pass it prints "351 files checked, 83 with violations, 2
  exempt").

Review findings of 2026-11-17 (`docs\handoff\STATUS_AND_PLAN.md:265-268`): 86 in all; 53 fixed,
24 marked open, 4 refuted, 5 moot (the UEFI code dropped under rule L6). The 24 open ones are in
section 5.1, each rechecked against the current code by reading.

## 2. Codes: severity, area, owner, milestone, effort, state

**Severity** (one per row; the highest that fits):

| Code | Meaning | Rule |
|---|---|---|
| S0 | Blocker: breaks a law (books not exact, lab not repeatable, pageable memory, an LLM or interpreter in the sim, a drive letter, a write outside Z:) or can lose the owner's data | Stop other work and fix it now, failing test first |
| S1 | Major: a wrong result or a false claim reachable in normal use, or blocks a milestone's definition of done | Fix inside the current milestone before it closes |
| S2 | Minor: wrong only at scale or at an edge, or a gap a later milestone needs | Fix in the milestone named in the row |
| S3 | Cleanup: dead code, stale wording, cosmetic | Fix when the file is touched anyway, at the latest in M7 (A1) |
| D | Decision: only the owner can decide | Use the recorded default, write it in PROGRESS.md, go on, ask in the next report |

**Area:** RF review finding, LNT lint, SEAM stub or module seam, MOD module limit, WG worldgen, EVD
evidence and statistics, PRF performance and scale, DEMO live demo and viewer, GPU GPU/NPU, VER
verification process, REP repository/toolchain/data safety, DOC documents, OWN owner decision, OW
owner-words row, TR trigger register.

**Owner:** AGENT (the coding agent does it), OWNER (only the owner can decide or do it), AGENT+OWNER
(the agent prepares, the owner decides).

**Milestone (M):** M5 W2, M6 W3+M, M7 A1, M8 E1-E3, M9 G1/S1-S4/R2, M10 R1 (as in
`docs\handoff\CONTINUE-BUILD.md:46-61`). "now" means before the next module lands.

**Effort** (agent working time, an estimate, not a measurement): S up to half a day, M 1 to 2 days,
L 3 to 5 days, XL more than 5 days.

**State** (register lifecycle; section 3 defines each one): NEW, TRIAGED, REPRODUCED, FIXING, FIXED,
VERIFIED, CLOSED, ANSWERED, NEEDS-OWNER, DEFERRED-TR, LAW-CONFLICT.

## 3. The issue register: file, format and validator

**Files** (created by CL-01 and CL-02; `docs\eval\` is where milestone 8 puts its tables,
`docs\handoff\CONTINUE-BUILD.md:59`):

- `docs\eval\ISSUES.csv`: the register itself, one row per item, UTF-8 that is plain ASCII, LF line
  endings, comma separated, every field quoted with `"`, a `"` inside a field doubled.
- `docs\eval\ISSUES.md`: generated from the CSV by the validator (never hand-edited): one table per
  area, open items first, plus a count by state and severity at the top.
- `build\work\scripts\issues.py`: the validator and generator (tooling, Python 3.9 standard library,
  run as `python -I`).

**Columns of ISSUES.csv** (in this order):

| Column | Content | Rule |
|---|---|---|
| id | `RF-01`, `SEAM-04`, `BS-07` | Unique, never reused; the prefix is the area |
| title | Up to 80 characters | Plain words |
| source | `path:line` or a finding id (`v23`) or a log file | At least one; must exist on disk when it is a path |
| severity | S0, S1, S2, S3, D | One |
| area | One of the area codes in section 2 | Equal to the id prefix (BS rows may carry any area) |
| milestone | now, M5 to M10 | The milestone in which it must close |
| owner | AGENT, OWNER, AGENT+OWNER | One |
| state | One of the states below | One |
| scenario | The failing scenario or the risk | Concrete inputs and the wrong output |
| fix | The fix or the decision needed | How it honours the laws if it touches one |
| test | The test that proves the fix: file, test name, command | Must be able to fail; "none" is allowed only for D rows |
| evidence | Paths of logs and PROGRESS.md entries that show the state | Required for FIXED, VERIFIED, CLOSED, ANSWERED, DEFERRED-TR |
| depends | Other ids, comma separated | Must exist |
| effort | S, M, L, XL | One |
| opened | YYYY-MM-DD | |
| closed | YYYY-MM-DD or empty | Required when state is CLOSED or ANSWERED |
| verifier | Who reproduced the fix: a session or agent name other than the author | Required for VERIFIED and CLOSED |

**States:**

- NEW: harvested, not yet looked at.
- TRIAGED: severity, milestone and owner checked against section 4.
- REPRODUCED: the failure was made to happen, or the claim was confirmed by reading, with the
  command or the line range in `evidence`. A row that cannot be reproduced goes to ANSWERED with the
  reason ("could not reproduce: ...").
- FIXING: a failing test exists and is committed to the tree (not to git; see REP-01).
- FIXED: the author's fix makes the failing test pass; all gates pass.
- VERIFIED: someone other than the author reproduced the old failure on the old code and the pass on
  the new code (the "green suite is not done" rule, `build\work\PROGRESS.md:1493-1497`).
- CLOSED: VERIFIED plus the evidence lines in PROGRESS.md and, when the state hash changed, a
  `tests\golden_hashes.txt` line with the cause.
- ANSWERED: not a defect, or superseded, or a duplicate, with the reason and the evidence.
- NEEDS-OWNER: a decision for the owner; the default is applied and recorded in PROGRESS.md.
- DEFERRED-TR: moved to `docs\TRIGGER-REGISTER.md` with a measurable trigger; allowed only for
  scale-out and substrate items (S1 to S4 work) and only after G1, or for items the owner deferred.
- LAW-CONFLICT: cannot be built as asked without breaking a newer law of the owner; the law and the
  reason are named (the term of `docs\requirements\OWNER-WORDS.md:19`).

**Validator rules** (`issues.py --check`, exit 1 on any violation):
1. Header and column count exact; every field quoted; ASCII; LF; final newline.
2. Ids unique and of the form `^[A-Z]{2,4}-\d{2,3}$`; prefix in the area list.
3. Every `source` and `evidence` path exists (strip `:line`; resolve relative to the root with
   `pathlib`, never `glob`, because of the brackets in the root).
4. `depends` ids exist and contain no cycle.
5. CLOSED and VERIFIED need a non-empty `verifier` different from the author recorded in `evidence`.
6. FIXED, VERIFIED, CLOSED, ANSWERED and DEFERRED-TR need `evidence`.
7. `--gate Mn` additionally exits 1 if any row with `milestone` <= Mn is in a state other than
   CLOSED, ANSWERED, NEEDS-OWNER (with a recorded default), DEFERRED-TR or LAW-CONFLICT, or if any S0
   row anywhere is not CLOSED.

**Evidence rules** (from `docs\handoff\CONTINUE-BUILD.md:38-45` and the laws): the command as run,
the tail of its output, the log file path under `build\work\`, the hash when state changed. Unknown is
fine; invented is not. A number measured on a busy machine says so ("lightly loaded" or "noisy,
parallel load"; CONTINUE-BUILD.md:41).

## 4. Triage rules

1. **Laws first.** Anything that touches a law (exact books, lab repeatability, locked memory, no
   pagefile, no VM/JIT/interpreter/LLM in the sim, no theatre, nothing fixed, Z: only, no drive
   letters, no installs/commits/pushes without the owner's yes) is S0 until shown otherwise.
2. **Claims before code.** A document that claims something false to the owner is S1 even when the
   code is fine (example: a "done" status that a regenerated STATUS file would repeat). Fixing the
   text is cheap; a false claim costs the owner's trust.
3. **Reproduce before you rank.** Severity is provisional until REPRODUCED. A finding that cannot be
   reproduced is not dropped; it is ANSWERED with the attempt recorded.
4. **Stale "open" rows are rechecked, not assumed.** Several RF rows point at stub files that wave 2
   replaced. Recheck by reading the new code and running the named test; close with evidence or keep
   open with the new line numbers.
5. **One owner per file per change.** Two fixes that touch the same file are serialised.
6. **A hash change needs a cause.** Every fix that changes `az_state_hash` output adds a
   `tests\golden_hashes.txt` line (date, hash, full command, change, reason). A change with no stated
   cause is a bug (`tests\golden_hashes.txt:3`).
7. **Deferral needs a trigger.** Nothing is deferred without a measurable trigger in
   `docs\TRIGGER-REGISTER.md` and an owner-visible note in the next report.
8. **No silent downgrades.** Lowering a severity or moving a milestone later is written in the row's
   evidence with the reason and the date.
9. **Owner decisions never block.** A D row takes its recorded default (section 5.13) and work goes
   on; the question goes into the next report (`docs\handoff\next-agent\00-PROMPT.md` section 9).
10. **Seeds are custody.** Any run on 5001-5024 or 5101-5124 before the milestone-8 preregistration
    is locked is an S1 incident (see EVD-10).

## 5. The harvested issue register

Columns of every table: ID | Source | Sev | Area | Failing scenario or risk | Fix or decision | Test
that proves it | Owner | M | Eff. "RECHECK" in the fix cell means the row is probably closed by later
work and must be confirmed by the named test before it is closed.

### 5.1 Review findings still marked open (RF)

Source of the 24 rows: `docs\handoff\STATUS_AND_PLAN.md:265-346` (status column "open"). Rows
v76 and v77 were reworded as "target plus measured state" in milestone 1
(`build\work\PROGRESS.md:54,100`) and stay open until S2 and the end of W2.

| ID | Source | Sev | Area | Failing scenario or risk | Fix or decision | Test that proves it | Owner | M | Eff |
|---|---|---|---|---|---|---|---|---|---|
| RF-01 | v67; `core\az_grid.c:150`; STATUS:278 | S2 | PRF | At 100k (131072 cells) `grid_hist_parts` returns 1: counting, prefix and scatter run serially between two parallel regions; about 1 ms of serial work per grid build, and workers fall asleep. Grid is 7.5 ms of 73.7 at 100k and 58.6 of 1339 at 1M (`wg_ref100k_t8.txt:8`, `wg_ref1m.txt:8`). | Split the histogram by cell ranges whenever more than one worker exists; choose the part count by measurement on 100k and 1M, not by a constant (nothing fixed: make it a host-derived value or a world parameter with a range). | Hash unchanged on `--threads 1,8,24 --ecores` for both references; `az_bench` grid stage ms before/after with `build\work\scripts\ab_time.ps1`, load labelled. | AGENT | M6 | M |
| RF-02 | v69; `core\az_tick.c:340`; STATUS:282 | S2 | PRF | The field stage scans every cell four times per tick over two 8 MB columns even when little is pending; the second commit needs only the 1/field_stride touched cells. | Second commit over touched cells only, by the same stride rule that `field_step` uses. | `test_field` books and hashes unchanged; both reference hashes unchanged; field ms before/after (ab_time.ps1). | AGENT | M6 | S |
| RF-03 | v68; `core\az_tick.c:349`; STATUS:283 | S3 | RF | Finding said Tier 1 was an empty stub and the tk kernels ran only in tests. RECHECK: Tier 1 is real (`core\az_tier1.c`) and calls tk_qsv, tk_qgt, tk_sh and tk_hop in the tick (per the GPT Astra review: `core\az_tick.c:455`, `core\az_tier1.c:103`). The remaining clause ("lesion bits point at modules that do not exist") is LNT-02 and LNT-03. | Close as superseded after reading both lines and running `test_tier1`; move the lesion clause to LNT-02/03. | `ctest --test-dir "$A\build\z" -R test_tier1`; grep shows the four kernel calls under `az_tier1_range`. | AGENT | now | S |
| RF-04 | v08; `core\stubs\az_life.c:111`; STATUS:288 | S3 | RF | Generation was a u16 that wraps after 65535. RECHECK: ABI 4 made it u32 (`core\az_columns.def:85`, `core\az_abi.h:1828`) and test_protocol runs generations past 65,535 (`tests\golden_hashes.txt`, line "generations past 65,535"). A u32 still wraps after 4,294,967,295 generations. | Close as fixed by ABI 4 with the citation; record the u32 bound in ARCHITECTURE as a stated limit (state the bound; do not say how long a run takes to reach it without a measured tick rate and a measured generation time). | `ctest -R test_protocol` passes; grep `generation` in `core\az_life.c` shows u32 arithmetic with no narrowing cast. | AGENT | now | S |
| RF-05 | v06; `core\stubs\az_stats.c:17`; `core\az_abi.h:1829`; STATUS:289 | S2 | SEAM | `trait_mean[k]` is a u32 sum of up to 255 per creature; it overflows above about 16.8M live creatures, so trait means are wrong at the owner's "millions" scale. Still present: the stats module is a stub. | In the stats module: accumulate in u64 (or i128-free split sums), divide at the end; keep the published `AzStats` field width decided with the ABI owner (an ABI change needs a hash/format note). | New unit test in `tests\test_stats.c` (new file, created with the stats module) that sums 20,000,000 values of 255 through the accumulator function on a synthetic span and checks the exact mean; it must fail on the u32 code. | AGENT | M5 | S |
| RF-06 | v03; `core\stubs\az_worldgen.c:34`; now `core\az_worldgen.c:853,862`; STATUS:290 | S3 | WG | Founders beyond the clade limit got clade 0. RECHECK: real worldgen calls `az_clade_found` per species (`core\az_worldgen.c:853`) and stops placing when there is no room (`:862`); test_worldgen E9 uses `max_clades 4` (`build\work\w2\worldgen\PROGRESS.md:212`). | Read `wg` placement with `clade_of[k] == AZ_NONE`; if founders of such a species are never created and the refusal is counted, close; if they are created with clade 0, fix. | `ctest -R test_worldgen`; add an explicit check (if missing): with `max_clades` smaller than the species count, no live founder has a clade whose species differs from its lineage. | AGENT | M5 | S |
| RF-07 | v76; `docs\ARCHITECTURE.md:54`; STATUS:291 | S2 | GPU | The contract said the state hash is equal on CPU and GPU; no tick stage runs on the GPU and nothing compares world hashes across substrates. Reworded to target plus measured state. | Keep the wording true at every milestone; the real fix is S2 (TR-04, TR-05). | When S2 lands: a test that runs N ticks CPU-only and with the GPU stage and compares the hash every tick. Until then: ARCHITECTURE row text matches reality (DOC-09). | AGENT | M9 | XL |
| RF-08 | v77; `docs\ARCHITECTURE.md:56`; STATUS:292 | S1 | LNT | The law table says every mechanism has a lesion and a test and every trait is read; today strict lint fails (LNT-01 to LNT-03). | Closes when LNT-01, LNT-02 and LNT-03 close and `lint_core --strict` is green. | `python -I "$A\tools\lint_core.py" "$A" --strict` exits 0. | AGENT | M5 | (in LNT) |
| RF-09 | v23; `gpu\az_gpu.c:276`; STATUS:294 | S2 | GPU | `az_gpu_open` uses `cuCtxCreate`, which pushes the context on the calling thread; no call makes a handle's own context current, so a second open on the same thread redirects the first handle's calls. | Make every handle call bracket its work with `cuCtxPushCurrent`/`cuCtxPopCurrent`, or use the device's primary context (`cuDevicePrimaryCtxRetain`); the driver API only (no toolkit install). | `test_gpu`: open two handles on one thread, interleave a Philox batch and a brain batch on each, each result bit-equal to the CPU reference; fails on the old code. Skips cleanly with a recorded reason when no NVIDIA driver is present. | AGENT | M6 | M |
| RF-10 | v02; `core\stubs\az_colony.c:24`; now `core\az_colony.c:123-132`; STATUS:316 | S1 | RF | A skipped colony store (negative or overflowing result) left dep/taken in `colony_eph`, which the next tick's memset deleted: matter left the books. RECHECK on the real module: `:129-132` computes the new store and clears dep/taken; what happens to the skipped amount must be read. | If the skipped amount is not refunded to the depositor or the cell, fix: refund it in the same commit and count the refusal. Books are a law: this is S0 if reproduced. | A test in `tests\test_colony.c` that drives a colony store to its `CELL_HOLD` bound and over-deposits; matter and energy totals exact every tick; fails if the refund is removed. | AGENT | now | S |
| RF-11 | v05; `core\stubs\az_life.c:20`; STATUS:317 | S1 | RF | `az_life_kill` zeroed a negative energy store without booking it, creating energy. RECHECK on the real `core\az_life.c` (the stub is no longer linked). | Read the kill path; if a negative store can reach it, book the difference or prove by an assertion that energy is never negative at kill. Books are a law (S0 if reproduced). | A test in `tests\test_life.c` that kills a creature whose energy was driven negative by a hand-set debit; the energy invariant stays exact. | AGENT | now | S |
| RF-12 | v47; `core\az_worldgen.c:83,853,862`; `core\az_world.c:297,443`; STATUS:318 | S2 | WG | Still present by reading: founder species clades are founded with `WG_SEED_BASE + k` = 0x10000 + k as their stable id, while creature uids start at 1 and count up, so after 65,535 creatures a real creature shares an id with a species clade; lineage (`parent_uid`) becomes ambiguous for E2 lineage metrics. | Put clade seed ids in a space no creature uid can reach (for example the top bit of the u64 set), documented in `core\az_abi.h`; this changes state, so record a new golden line for both references. | A test (test_worldgen or test_brain) that runs past 70,000 births and asserts that no clade `founder_uid` equals any live or dead creature uid seen in the ledger; fails on the old constant. | AGENT | M5 | S |
| RF-13 | v64; `core\az_abi.h:732`; STATUS:322 | S3 | SEAM | `az_delta_ns` has no caller; `az_offer_u16` (the reader of the offer bytes) has none, so the 16-byte EPH offer column is written by social and econ and never read. The STAKE sense needs exactly that reader (SEAM-04). | Decide inside the sense module: either STAKE reads the previous tick's committed offer through `az_offer_u16` (then keep it) or delete both helpers. | Strict lint plus a grep test in `tests\test_tools.c` style that every exported ABI helper has a caller outside its own header; or the STAKE test of SEAM-04. | AGENT | M5 | S |
| RF-14 | v63; `core\az_cells.def:17`; STATUS:324 | S3 | RF | Six FIELD columns (phase, wind, charge, solid, med, temp) were never written but hashed and saved. RECHECK: weather, chem and worldgen are real now and write some of them; physics will read phase and med (`build\work\PROGRESS.md:1216-1217,1431-1434`). | List writers and readers of each of the six columns; close those with both; the rest are SEAM-02 work. | A script check (CL-11) that prints writer and reader counts per column from `core\az_cells.def`; zero columns with no writer or no reader at the end of M5. | AGENT | M5 | S |
| RF-15 | v81; `core\az_columns.def:16`; STATUS:325 | S3 | DOC | The column file claims GPU upload is generated from it; no GPU upload of columns exists. | Change the comment to the measured state. | Reading; `check_text` passes. | AGENT | M6 | S |
| RF-16 | v59; `core\az_columns.def:39`; STATUS:326 | S3 | RF | Fifteen entity columns (131 of 510 saved bytes) had no reader and no writer yet were zeroed, moved, hashed and saved. ABI 4 rewrote the layout (fixed record now 124 bytes, `wg_ref100k_t8.txt:10`); RECHECK. | Same column census as RF-14 for `core\az_columns.def`; drop or wire every column with no reader. | The CL-11 census shows 0 columns without both a writer and a reader (or a recorded reason, such as a column reserved for physics). | AGENT | M5 | S |
| RF-17 | v62; `core\az_tick.c:34`; STATUS:328 | S3 | RF | The work-splitting policy (part budget, serial fallback, debug-seed cut randomisation) exists twice, in az_tick.c and az_grid.c, and the copies differ. | One shared function in one file; keep behaviour identical, or record the hash change. | `test_core` and `test_grid` with `debug_split_seed` variants give the same hashes as before; both references unchanged. | AGENT | M7 | S |
| RF-18 | v61; `core\az_tick.c:312`; STATUS:329 | S2 | PRF | On every reorder tick the grid is built twice (reorder builds it, then the tick builds it again). | Reuse the first build's outputs. | Both references unchanged with `--reorder 1,7,31,255`; grid ms on reorder ticks halves (ab_time.ps1, load labelled). | AGENT | M6 | S |
| RF-19 | v80; `core\stubs\az_brain.c:6` (now `:1-5`); STATUS:331 | S3 | DOC | **RESOLVED in the stub (re-read 2026-11-18):** the file has 5 lines and only includes `../az_brain.c`, so the 32-sensor comment is gone, and `docs\ARCHITECTURE.md:387-388` points at `core\az_brain.c` (it still calls the stub a "copy": change that word when ARCHITECTURE is next edited). Original finding: the brain stub documents a 32-sensor layout while the ABI has more senses (64 channels: `docs\requirements\OWNER-WORDS.md:31`), and `core\az_brain.c` claims to be the definition. | Fix the stub comment or delete the stub once no build links it; ARCHITECTURE section 9 must point at `core\az_brain.c`. | Reading; `cmake` configure shows the stub is not linked; check_text passes. | AGENT | M7 | S |
| RF-20 | v82; `gpu\README.md:121`; STATUS:332 | S3 | DOC | The GPU README lists a free-list ABI need that `core\az_abi.h` already states, and says the driver DLL is never loaded through the search path while the API and test pass bare names. | Make the README true; if the claim about the DLL search path matters for security, load `nvcuda.dll` from the system directory explicitly (`LoadLibraryExW` with `LOAD_LIBRARY_SEARCH_SYSTEM32`). | Reading; `test_gpu` still passes; a grep shows the full-path load. | AGENT | M6 | S |
| RF-21 | v57; `gpu\az_gpu_ref.c:17`; STATUS:333 | S3 | GPU | `az_memset`/`az_memcpy` defined twice in gpu/ as stand-ins for the core versions. | One small gpu header included by both translation units. | `test_gpu` builds and passes; grep finds one definition. | AGENT | M6 | S |
| RF-22 | v25; `gpu\az_gpubench.c:14`; STATUS:334 | S3 | DOC | The bench header says the AVX-VNNI twin "is the CPU kernel the simulation really runs"; by default the simulation runs the C reference (twins only with `--kernels 1`; `wg_ref100k_t8.txt:4` shows "twin mask 0x0"). | Make the header true. | Reading. | AGENT | M6 | S |
| RF-23 | v70; `core\az_tick.c:150`; STATUS:344 | S3 | PRF | `job_scratch_reset` rewrites the pair record and `birth.n` of every slot every tick, although only a few slots carry state. | Reset only the slots that carried state, or prove by measurement that the cost is negligible and ANSWER the row. | Both references unchanged; per-stage ms before/after. | AGENT | M6 | S |
| RF-24 | v26; `gpu\az_gpu.c:262`; STATUS:346 | S2 | GPU | Four driver queries ignore their `CUresult`; a failure leaves zero or empty fields that `info->msg` reports as fact (a false claim to the owner). | Check every result; report "unknown" on failure. | `test_gpu` with a fault-injection hook (a test build flag that makes one query fail) shows "unknown", not 0. | AGENT | M6 | S |

### 5.2 Lint and strict lint (LNT)

Rules: `tools\lint_core.py:20-23` (W1, W2); strict mode turns them into errors (`:29-31`). The ctest
gate runs the non-strict form (`CMakeLists.txt:281`). Owners come from `core\az_owners.def`.

| ID | Source | Sev | Area | Failing scenario or risk | Fix or decision | Test that proves it | Owner | M | Eff |
|---|---|---|---|---|---|---|---|---|---|
| LNT-01 | W1; `core\az_owners.def:29-31,55,82,108-109,151-152` | S1 | LNT | Nine traits have no read in their designated module: DENSITY, DRAG, THRUST (owner physics, a stub), BUILD, TRUST, VAL_2, VAL_3 (owner econ, real), TAXIS, CURVE (owner sense, a stub). A trait that nothing reads is theatre (law: every trait read). | Physics reads DENSITY, DRAG, THRUST; sense reads TAXIS, CURVE; econ reads BUILD (needs reef or nest building, listed as not done in `build\work\PROGRESS.md:868-885`), TRUST (accepting a trade from a stranger) and VAL_2/VAL_3 (values of the goods that do not exist yet: two of four goods exist). Moving a trait's owner line to dodge the check is not allowed unless the trait truly belongs to that module (write the reason). A read must change behaviour (a dummy read is the Cosmetic mutant, BS-01). | `lint_core --strict` shows "W1 0 of N"; for each of the nine traits a test that sets the trait to two values in a hand-made world and shows a behaviour difference (for example, DENSITY changes sinking speed in water), failing when the read is removed. | AGENT | M5 | L |
| LNT-02 | W2; `core\az_owners.def:175-176` | S1 | LNT | GRAVITY is honoured by chem (settling) but not by physics: bodies have no weight. | Physics applies the cell's gravity vector (`grav`) to bodies, scaled by density against the medium; the GRAVITY lesion removes that force. | `lint_core --strict` W2 clean for GRAVITY; a physics test: a dense body in a fluid cell sinks with GRAVITY on and does not with the lesion; az_lesion GRAVITY row changes a body endpoint (EVD-02). | AGENT | M5 | (in SEAM-02) |
| LNT-03 | W2; `core\az_owners.def:161,177,183` | S1 | LNT | Lesion bits RECURRENCE, NOISE and SENSES have no test that the checker recognises (all three are owned by the tick). | Add to an existing test program (`tests\test_brain.c` for RECURRENCE and NOISE, the new sense test for SENSES) a check that the bit changes what it claims: hidden state masked, noise channel masked, exteroceptive channels masked. Each check fails if the mask code is removed. | `lint_core --strict` W2 clean; the three new checks fail on a build with the mask lines deleted (planted defect). | AGENT | M5 | M |
| LNT-04 | `CMakeLists.txt:281`; `docs\handoff\CONTINUE-BUILD.md:51` | S2 | VER | ctest is green while strict lint fails; "green" is read as "done" by people and agents. | Add a ctest gate `lint_core_strict` as soon as LNT-01 to LNT-03 are closed (adding it earlier makes the suite red for a known reason; if it must be added earlier, mark it in the report as expected to fail, never with `WILL_FAIL`). | `ctest -R lint_core_strict` passes on the final M5 tree and fails on a planted unread trait. | AGENT | M5 | S |
| LNT-05 | `tools\lint_core.py:378-426` | S2 | VER | W1 counts a trait as read when its owner file names it or reads the phen scalar or tag that carries it; a read that changes nothing passes. | Add to M7's Cosmetic mutant (VER-01) a trait read whose value is discarded; the audit must catch it by lesion or behaviour test, since lint cannot. Record that lint W1 is a necessary, not a sufficient, check. | The Cosmetic mutant is caught (VER-01). | AGENT | M7 | (in VER-01) |

### 5.3 The three stubs and the module seams (SEAM)

| ID | Source | Sev | Area | Failing scenario or risk | Fix or decision | Test that proves it | Owner | M | Eff |
|---|---|---|---|---|---|---|---|---|---|
| SEAM-01 | `core\stubs\az_sense.c`; brief `docs\agents\W2-BRIEFS\sense.md`; GPT Astra review section 7 item 3 | S1 | SEAM | The senses are a stand-in: directional inputs stay neutral, the PAYOFF sense reports 64 ("nothing") always (`build\work\PROGRESS.md:783-784`), TAXIS and CURVE are unread (LNT-01). Every evolved brain therefore steers on nearly blind input, which can mask the effect of GRAVITY, SENSES, NOISE and others (EVD-01). | Build `core\az_sense.c` to its brief: body-frame directional channels from cell gradients and percepts, response curves by CURVE, gradient gain by TAXIS, the NOISE channel from the tick's entropy in wild mode (law: wild by default), all integer, no float. Each channel has a lesion path (SENSES masks exteroception). | New `tests\test_sense.c`: (a) a hand-made gradient world, a creature rotated in 8 headings: the expected receptor changes sign with heading; (b) SENSES lesion: channels at their neutral value; (c) same hash on 1, 8, 24 workers; (d) planted defects (swap two axes, drop CURVE) caught. | AGENT | M5 | XL |
| SEAM-02 | `core\stubs\az_physics.c`; brief `docs\agents\W2-BRIEFS\physics.md`; `build\work\PROGRESS.md:1602` | S1 | SEAM | Physics ignores gravity, rock and medium density: bodies can walk into bedrock, do not sink or float, and carrying weighs nothing (`build\work\PROGRESS.md:880-881`). The world that worldgen draws is therefore not the world the bodies move in. | Build `core\az_physics.c`: body-frame thrust with energy cost, drag of the medium (DRAG, the cell's viscosity), buoyancy (DENSITY against the medium), gravity from `grav` (GRAVITY lesion), collision with rock (solid cells refuse entry; the move is refused and recorded, never a crash), carried load as mass; plus its NASM AVX2 twin, bit-identical to the C path. Integer fixed point only. | New `tests\test_physics.c`: a body never ends a tick inside a solid cell (thousands of random starts); sinking and floating by density; thrust cost booked exactly; twin equals C on 1,000,000 random states; same hash on 1/8/24 workers; GRAVITY lesion changes only what it claims. | AGENT | M5 | XL |
| SEAM-03 | `core\stubs\az_stats.c:4-32`; brief `docs\agents\W2-BRIEFS\stats.md:10` | S1 | SEAM | `az_classify` does nothing and `az_look_class` returns 0 (`core\stubs\az_stats.c:27,32`); no variances, no quantiles, no measured archetypes (HERMIT, DOMINANT, SUBORDINATE, SWITCHER and the rest: `docs\requirements\OWNER-WORDS.md:91-93`). Milestone 8 cannot score anything that needs a distribution. Also RF-05 (u32 overflow). | Build `core\az_stats.c`: per-trait mean, variance and quantiles by exact integer histograms; behaviour-derived archetypes from counted acts (not from trait thresholds alone, which would be a fixed label); look classes from body bytes. | New `tests\test_stats.c`: exact moments on synthetic populations with known answers computed by hand; archetype assignment on scripted act histories; RF-05 overflow test; planted defects caught. | AGENT | M5 | L |
| SEAM-04 | `core\az_abi.h:151-152`; `core\az_social.c:87`; `core\az_tick.c:242`; GPT Astra review section 7 item 3 | S1 | SEAM | PAYOFF should encode what the last interaction gained or lost, but no column stores it. STAKE should encode the faced neighbour's offer, but senses run in the perceive stage before this tick's offers are written; reading another worker's live offer would make the result depend on the schedule (breaks lab repeatability, a law). | Add a committed, double-buffered per-creature payoff value (written at commit, read next tick) and read STAKE from the previous tick's committed offer (via `az_offer_u16`, RF-13). This is an ABI change: update `core\az_abi.h` and ARCHITECTURE, record the hash change. | Same hash on 1, 2, 5, 8, 12, 16, 24 workers and `--reorder 1,7,31,255` with the new senses active; a pair test where one side's offer changes and the other side's STAKE sense changes exactly one tick later. | AGENT | M5 | M |
| SEAM-05 | `docs\handoff\CONTINUE-BUILD.md:51`; `build\work\PROGRESS.md:1610-1611` | S1 | VER | The cross-module seam review is not done: each module was tested mostly alone or against stubs; what one module writes and the next reads (cells, offers, colony pools, marks, phase/med, grav) has no joint review. | After sense, physics and stats land: one review that lists every shared column and record (writer, reader, stage, units, range), checks each against `core\az_abi.h` and ARCHITECTURE, and adds a test for every seam that has none. | `docs\eval\SEAMS.md` (new) with one row per shared item and its test id; every row has a test that fails when the writer's units are changed (planted defect). | AGENT | M5 | L |
| SEAM-06 | `build\work\PROGRESS.md:1256-1259,1371-1372,1493-1497` | S1 | VER | Nobody but the author has checked life, mate, tier1, metab, social, econ, colony, weather and chem; worldgen had three adversarial reviews in its own workflow (`build\work\PROGRESS.md:1550-1556`). "A green suite is not done until a reproduce-first review agrees." | A reproduce-first review of all 13 modules (section 7), lenses: books and determinism; owner's words and no theatre; code and test strength; concurrency; performance. Each finding enters the register. | Review report under `build\work\review-w2\` with every finding and its verdict; zero CONFIRMED findings open at M5 close (`issues.py --gate M5`). | AGENT | M5 | XL |

### 5.4 Limits recorded by the nine earlier wave-2 modules (MOD)

Each module wrote "Honest limits" in `build\work\PROGRESS.md`. Some were caused by stubs that are real
now; those rows say RECHECK.

| ID | Source | Sev | Area | Failing scenario or risk | Fix or decision | Test that proves it | Owner | M | Eff |
|---|---|---|---|---|---|---|---|---|---|
| MOD-01 | life; `build\work\PROGRESS.md:505-508` | S2 | MOD | With the stub metabolism most newborns starved (5,676 of 5,759 deaths in the 300-tick life table were cause 0). RECHECK: metab is real now. No least-plastic founder line had DEV_PLAST 0 in the twin test, so "no gene, same body" was checked only through uid, part count and trait equality. | Re-run the life table on the real stack and record the death causes; add a twin test with DEV_PLAST forced to 0 in a hand-made genome. | `test_life` life table prints causes; the new twin check fails if development ignores DEV_PLAST. | AGENT | M5 | S |
| MOD-02 | mate; `build\work\PROGRESS.md:549-551` | S2 | MOD | Ornaments cost nothing to carry until metab charges for them; choice is by the responder only. RECHECK the ornament cost now that metab is real. | Read metab for an ornament carrying cost; if absent, add it (energy per unit of ornament trait per tick, a world parameter with a range). Responder-only choice: record as a design decision or add proposer choice. | A metab test: two bodies equal except ornament traits spend different energy per tick. | AGENT | M5 | S |
| MOD-03 | tier1; `build\work\PROGRESS.md:599-602` | S2 | MOD | Learning reaches behaviour only through gain, bias, context and arousal; the learning test is a two-armed bench, not a measured fitness gain in a world; the per-visit cost is not vectorised. | After sense lands: a world-level learning test (a reversal task in a gradient world, the Frozen mutant of VER-01 as the control). Vectorisation is PRF-04. | The Frozen-mutant comparison of VER-01 shows a difference in the preregistered measure. | AGENT | M8 | M |
| MOD-04 | metab; `build\work\PROGRESS.md:682-699` | S2 | MOD | No world-level result that any metabolic trait is selected (the thermal expectation failed); WASTE_RATE has no cost to weigh; OXI_USE is only a risk without oxidiser and there is no cause of death for suffocation (it shows as STARVED); photosynthesis fixes only cell waste. | Add a cost to early excretion (a world parameter), a death cause SUFFOCATED (ledger cause code, ABI note), and a body-waste path for photosynthesis, or record each as a design decision with the reason. | Unit tests for each; the ledger shows SUFFOCATED in a no-oxidiser cell. | AGENT | M5 | M |
| MOD-05 | social; `build\work\PROGRESS.md:781-793` | S2 | MOD | No world-level result for cooperation or punishment; PAYOFF has no column (SEAM-04); an aimed signal only costs less and reaches the same receivers; no test shows that any receiver responds to a signal; RECRUIT was 0 while colony was a stub (RECHECK: 185 recruits at 100k now, `build\work\logs\wg_ref100k_t8.txt:12`); memory is by tag, not individual; theft moved 0 units under the econ stub (RECHECK: econ is real now). | Aimed signals: give the scan an aim cone in sense (SEAM-01). Add a test that a receiver's motor output changes with a signal. Count stolen units in az_run output. | Signal-response test fails if the receiver ignores the signal channel; az_run prints units stolen. | AGENT | M5 | M |
| MOD-06 | econ; `build\work\PROGRESS.md:868-885` | S2 | MOD | Only two of four goods exist; no creature senses a price; deposits to and withdrawals from colony pools (GIVE drive, STORE_SHARE trait) and the TRAIL and NEST marks are not done; carrying costs nothing until physics; the pantry only costs in a world with no famine or season. | Build goods 3 and 4 or record why not; colony deposits and withdrawals; TRAIL and NEST marks (BUILD trait, LNT-01); a price sense if the ABI has room. | Tests per feature; strict lint W1 clean for BUILD, TRUST, VAL_2, VAL_3. | AGENT | M5 | L |
| MOD-07 | colony; `build\work\PROGRESS.md:1046-1060` | S2 | MOD | No result that a colony does better than its members alone; only signal channel 0 of 4 is written; NEST marks cost nothing; a reused colony id inherits the marks of the dissolved one; the tier-2 pass runs every 256 ticks, so the 60- and 20-tick reference runs hold one pass, at tick 0. | Clear marks when a colony id is reused (or key marks by a reuse counter); write channels 1 to 3 or record why not; a cost for NEST marks. | A test that dissolves a colony, reuses its id and checks the marks are gone; fails on the old code. | AGENT | M5 | M |
| MOD-08 | weather; `build\work\PROGRESS.md:1160-1175` | S2 | MOD | Cells where bodies die get hot for tens of ticks, up to 32767 (the top of the scale), "not looked into"; the storm is sensed as one number everywhere; cloud cover is one number for the sky; a storm does not rain; Euler stepping at 1/128 per tick loses differences under about 1/256 of a unit. | Investigate the death-heat spike first (it may be remains releasing energy as heat into a cell with little matter); then sense storms by distance in SEAM-01. | A test with a death in a low-matter cell: temperature stays inside a bound derived from the energy released and the cell's heat capacity; books exact. | AGENT | M5 | M |
| MOD-09 | chem; `build\work\PROGRESS.md:1216-1217,1431-1434` | S3 | MOD | When the quarter binds, the sides of a cell are served in a fixed order (-x first, +z last), so the last unit or two of a bound flow can favour the early sides (a directional bias); nothing reads phase or med yet (physics). | Serve the sides in an order drawn per cell and tick from the random key (lab-repeatable, wild by entropy), or split the remainder by a draw as the reactions do. | A symmetric hand-made world: over 10,000 ticks the flow out of each side differs by no more than a binomial bound computed in the test; fails on the fixed order. | AGENT | M7 | S |

### 5.5 Worldgen limits (WG)

Sources: the module header of `core\az_worldgen.c`, `build\work\w2\worldgen\PROGRESS.md` and
`build\work\PROGRESS.md:1569-1610`.

| ID | Source | Sev | Area | Failing scenario or risk | Fix or decision | Test that proves it | Owner | M | Eff |
|---|---|---|---|---|---|---|---|---|---|
| WG-01 | `build\work\PROGRESS.md:1602` | S2 | WG | Founder bodies are built from the species' base genome; no ABI call rebuilds a body after the fit, so a founder's body may not match its fitted genome. | Add an ABI call that develops the body from the final genome (the same code path as a birth), or develop the body after the fit inside worldgen. State change: new golden lines. | test_worldgen: every founder's body bytes equal what development gives for its own final genome; fails on the old path. | AGENT | M5 | M |
| WG-02 | `build\work\PROGRESS.md:1603` | S1 | WG | The physics stub ignores gravity, rock and density, so bodies can walk into rock. | Closed by SEAM-02. | SEAM-02's "never inside a solid cell" test. | AGENT | M5 | (SEAM-02) |
| WG-03 | `build\work\PROGRESS.md:1604` | S2 | WG | Weather relaxes the depth and latitude temperature gradients over time, so the niches worldgen draws fade. | Measure the gradient over 10,000 ticks on three development seeds (1-49 only; 06 section 1.6); if it collapses below the niche widths, add a sustained source (vent heat, sun by latitude) or record as world behaviour. | A test or a measurement log showing the gradient's size over time against the niche bands; the decision recorded. | AGENT | M5 | M |
| WG-04 | `build\work\w2\worldgen\PROGRESS.md:145-155` | S2 | WG | Crowds of founders boil a few ocean cells (at SURFACE_SOLID 16: 1-12 of 795 ocean cells not liquid, worst 55, seeds 1-6). | Decide whether founder crowd heat is a real effect (life heats water) or a placement artefact (too many founders per cell); if an artefact, spread founders by the cell's heat capacity. | test_worldgen D: ocean liquid after 200 ticks (now 935 of 939) tightened to the decided bound. | AGENT | M5 | S |
| WG-05 | `build\work\w2\worldgen\PROGRESS.md:178-180` | S2 | WG | Some worlds have no founder in the air (4 of 20 worlds at 16 species; 6 of 8 on seeds 91-98). | Decide: either guarantee each band at least one species when the species count allows it, or accept and record (an alien world need not fill every band). | A worldgen measurement over the development seeds 1-49 (never 50-99, which tests and the worldgen tuning already used; 00-MASTER-PROMPT.md 5.7) giving the share of worlds with an empty band, recorded with the decision. | AGENT+OWNER | M5 | S |
| WG-06 | `build\work\PROGRESS.md:1606-1608` | D | OWN | Shape numbers stay fixed: octave doubling, the linear fade of the rock's pull, the pool scale (SIZE+16)/144. The owner wrote "nothing in the universe is FIXED" (`docs\requirements\OWNER-WORDS.md:51`). | Default: make each one a WORLDGEN_* parameter with a range (cheap, honours the law); ask the owner whether that is enough. | test_worldgen extremes test (E6) runs every new parameter at its range ends. | AGENT+OWNER | M5 | S |
| WG-07 | `build\work\PROGRESS.md:1609`; `docs\requirements\OWNER-WORDS.md:182-187` | D | OWN | Fixed structure: the ocean is always under the air, gravity always points to -z, the guild-to-band map is fixed. These are the brief's structure, not the owner's words, and stay open against "nothing fixed". | Owner decides; default: keep (changing them is large and touches physics); record in the next report. | none (decision) | OWNER | M5 | - |
| WG-08 | `build\work\w2\worldgen\PROGRESS.md:232-235`; `build\work\PROGRESS.md:1569-1580` | D | OWN | Lead decisions accepted without the owner: WORLDGEN_W_REST (the 46th parameter), NICHE_HI 160..255 and NICHE_LO 0..96, SURFACE_SOLID default 16, per-world strategy shares, the five-step fit cascade, founder pools capped, and the fixture moves in test_life and test_tier1. | Default: keep; list them in the next report for the owner. | none (decision) | OWNER | M5 | - |
| WG-09 | `build\work\w2\worldgen\PROGRESS.md:232` | S2 | PRF | The cost of worldgen at 100k and 1M was never measured on a labelled idle machine (`ab_time.ps1` not run); the +35 percent at 100k (54.7 to 73.7 ms) comes from "lightly loaded" runs. | Run `ab_time.ps1` A/B (pre-worldgen snapshot build against the current build) with the load recorded. | A log under `build\work\logs\` with both builds, the same commands, the CPU load of other processes, and the ratio. | AGENT | M6 | S |
| WG-10 | `build\work\w2\worldgen\PROGRESS.md:166-167` | S3 | WG | With trait variation on (VARY 3) 241 of 16,000 founders sit outside their pressure tolerance. | Decide whether to refit varied founders or accept and record (they may die early: a real filter). | A measurement of their survival against the rest over 200 ticks, recorded. | AGENT | M5 | S |
| WG-11 | `build\work\logs\wg_ref1m.txt:9,11`; `build\work\logs\wg_ref100k_t8.txt:11`; `core\az_world.c:205`; `tools\az_run.c:166,272` | S1 | WG | Default table sizes bind in both reference worlds: clades default 4096 (`tools\az_run.c:272`), colonies default 4096 (`core\az_world.c:205`). 1M: 3,505 clades alive with 12,497 clade refusals, 4,095 colonies with 24 refusals, 380,865 store refusals, 10 of 52,000,000 store units free. 100k: 10,756 clade refusals. `az_run` has `--max-clades` but no `--max-colonies` option. Refusals are lawful (recorded, never a crash), but the reference evidence is shaped by default capacities, and 4096 is the number the owner rejected in the first freeze (`build\work\PROGRESS.md:211`). | Size the clade and colony tables from the population capacity by default (a rule with a recorded factor, not a constant), add `--max-colonies` to az_run, az_live and az_lesion, and report the refusals in every summary. State change for the references only if defaults change: new golden lines with the cause. | az_run on both references prints 0 clade and 0 colony refusals with the new defaults (or the refusals with the reason); a test that sets `max_colonies` from the command line and sees it in the snapshot header. | AGENT | M5 | M |

### 5.6 Evidence and science (EVD)

Lesion numbers below are from `build\work\logs\lesion_z_worldgen_1_24.txt` (tuning seeds 1-24, 3000
founders, 64 ticks, grid 4,4,3, 8 species; exploratory, never a reported result).

| ID | Source | Sev | Area | Failing scenario or risk | Fix or decision | Test that proves it | Owner | M | Eff |
|---|---|---|---|---|---|---|---|---|---|
| EVD-01 | lesion table (14 NO EFFECT rows) | S1 | EVD | 14 switches show NO EFFECT: RECURRENCE, PLASTICITY, WEATHER, CHEM, SIGNALS, COLONY, TIER1_Q, TIER1_SH, MEMORY, GRAVITY, NOISE, METRIC, CARE, SENSES. Three possible causes, none tested yet: (a) masked by stubs (SENSES, NOISE, GRAVITY and anything that needs directional input or weight); (b) runs too short (64 ticks; max_generation is 1 or 2 in the references, `wg_ref100k_t8.txt:9`); (c) the wrong primary measure (11 rows use mean energy). On the complete build, NO EFFECT is allowed, INERT is a defect (`docs\handoff\CONTINUE-BUILD.md:55`). | After M5: re-run the tuning table; for each NO EFFECT row, write down before running which cause is suspected; test (b) with a longer-horizon tuning variant (`--ticks` chosen by measurement so that generations reach a recorded number); test (c) only by preregistering a new primary measure before the reporting run (EVD-02), never by searching measures on reporting seeds. | `docs\eval\LESION-TRIAGE.md` (new): one row per NO EFFECT switch with the suspected cause, the tuning run that tested it, and the measure locked in `docs\prereg\`. | AGENT | M8 | L |
| EVD-02 | lesion table rows SOCIAL, MUTATION, MATING, BRAIN, TRADE; GPT Astra review section 7 | S1 | EVD | Several EFFECT rows are close to definitional: SOCIAL removes social pairs, so "interactions -> 0" (d_z -3.247) proves wiring, not value; MUTATION -> clades 246 to 8; MATING -> births. BRAIN raises mean energy (+1826, d_z +3.199) because fewer, unthinking creatures survive with more energy each (live -3600, births -5189): the sign of a group mean is not the sign of benefit. TRADE raises interactions. A reader can take these as "the mechanisms matter" (a Goodhart risk). | Split every lesion row into two preregistered measures: a WIRING measure (the mechanism's own output, expected to change) and a CONSEQUENCE measure (an outcome the mechanism should affect without being defined by it: survival-weighted intake, reproductive output per founder, lineage persistence). Report both, Holm over the consequence family. | The preregistration file lists both measures per row before any reporting run; az_lesion prints both; a test in `tests\test_statsutil.c` checks that every row has two named measures. | AGENT | M8 | M |
| EVD-03 | `tools\az_lesion.c:15-16`; GRAVITY row | S2 | EVD | The bootstrap interval is of the mean difference, not of d_z; milestone 8 asks for an interval on d_z (`docs\handoff\CONTINUE-BUILD.md:56`). GRAVITY shows a mean-difference interval [0.0686, 6.23] that excludes 0, yet d_z +0.387 and Holm p 1, so NO EFFECT: correct by the rule, but easy to misread. | Add a bootstrap interval on d_z (percentile or BCa, seeded, the method named in the prereg) and print it beside the mean-difference interval. | `tests\test_statsutil.c`: the d_z interval on a known synthetic sample matches a hand-computed reference within a stated tolerance. | AGENT | M8 | S |
| EVD-04 | `build\work\logs\wg_ref1m.txt:9-11`; GPT Astra review section 7 | S2 | EVD | The 1M reference is scale, not evolution: 20 ticks, max_generation 1, the store full (380,865 refusals). It cannot support any claim about evolution at a million. | Label it so everywhere ("1M: scale and repeatability only"). For an evolution claim at 1M, a long run with a store that does not bind is needed (needs RAM measured against the lock limit: BS-09). | Every document that quotes the 1M run carries the label (DOC sweep, CL-05). | AGENT | M8 | S |
| EVD-05 | GPT Astra review (Combo A) | D | EVD | The original Grok Bot build and the Windows v40 export disagree on Combo A foraging over the same 112 seed ids (d = 0.733 against 0.857; threshold 0.8); cause unresolved. | Default: keep separate provenance, never merge or average, never carry either into a native result (no v40 result carries over: `docs\handoff\CONTINUE-BUILD.md:54`). Investigating needs reads of read-only projects (`Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-v40`); ask the owner whether it is worth time. | none (decision); if investigated: both runs with identical command lines and seed lists, a diff of inputs, logged under `build\work\astra\combo-a\`. | OWNER | M8 | M |
| EVD-06 | GPT Astra review | S2 | EVD | The owner's three "100s" cannot be added together: the 100-pass ledger and the 100-test suite share an item code in only 46 of 100 same-number pairs; the 100 LIFE areas hold the 500 sub-checks; the benchmark catalog has 109 rows. | E2's trace matrix keeps each list separate with its own ids and counts; a summary never says "300". | `docs\eval\TRACE.csv` has a `list` column; a check script counts items per list and matches the source JSON counts. | AGENT | M8 | S |
| EVD-07 | GPT Astra review; `docs\handoff\CONTINUE-BUILD.md:54` | S1 | EVD | The native engine has no held-out score on any suite. v40 R21 numbers (A-arm 9/14 Butlin, 6/10 theories, 17/25 CQM) are v40's, not native's. | Never quote v40 numbers as native; M8 produces native scores on reporting seeds only, indicator-only wording. | The final report's claims table has a provenance column; a grep for "9/14", "6/10", "17/25" finds them only with "v40" beside them. | AGENT | M8 | S |
| EVD-08 | GPT Astra review; `build\work\PROGRESS.md:688-689` | S2 | EVD | No selection result yet: a display preference raised the first-generation ornament shift on 5 of 8 seeds; thermal sorting reached its predicted direction on 2 of 4 seeds; metab's thermal expectation failed. | Treat all as tuning observations. A selection claim needs a preregistered test with enough generations, a neutral-drift control (shuffled fitness, same budget) and reporting seeds. | A preregistered selection test in M8 with its control; until then no document says "selected". | AGENT | M8 | L |
| EVD-09 | GPT Astra review section 7 (the decisive demonstration) | S1 | EVD | The chain gradient -> body-frame receptor -> motion through the real medium -> paid intake -> reproductive outcome has never been shown in the native engine. It needs SEAM-01 and SEAM-02. | Build the experiment after M5: a known food gradient, matched starts, rotated creatures; arms: intact, flat gradient, SENSES lesion, sham; measures preregistered at each link of the chain. | `tests\test_chain.c` (mechanics, fast) plus a preregistered E1-style run (science); each link's measure changes in the expected direction in the intact arm and not in the controls. | AGENT | M8 | L |
| EVD-10 | `build\work\PROGRESS.md:25`; `docs\ARCHITECTURE.md:423-424`; `tools\az_lesion.c:3,33-34`; GPT Astra review item 5 | S1 | EVD | Seed custody: (a) reporting block 5101-5124 was used once for the stub baseline (recorded as the one permitted repetition); (b) routine tests use seeds 201-208 and 211-214, outside the 1-99 tuning rule; (c) az_lesion's default is the reporting block 5101-5124, so any run without `--seeds` burns it. | Change az_lesion so that a reporting block needs an explicit `--report` flag plus a prereg file whose SHA-256 matches a pinned value; without them it refuses with exit 2 (the default becomes a tuning block). Move test seeds into 1-99 or document a test-only range that is neither tuning nor reporting, written into ARCHITECTURE. Record (a) in the prereg as known exposure. | `tests\test_tools.c` (or a new cli test): `az_lesion` with no `--seeds` does not touch 5001-5024 or 5101-5124; with `--seeds 5101-5124` and no `--report` it exits 2. | AGENT | now | S |
| EVD-11 | `docs\handoff\next-agent\00-PROMPT.md` section 9 (D2) | D | EVD | The 500 checklist runs on 48 native reporting seeds where the framework asks for 64. | Default: 48 with the deviation recorded in the prereg; ask the owner. | none (decision) | OWNER | M8 | - |
| EVD-12 | GPT Astra review section 8; `docs\handoff\CONTINUE-BUILD.md:54-59` | S1 | EVD | No `docs\prereg\`, no `docs\eval\`, no `tools\az_suite.c` exist; per-item control arms (size-matched sham, sham-cost, shuffled stream, yoked, random policy, neutral shadow) are not built; the ceiling rule (intact mean above 0.95 of the maximum fails) is not implemented in any tool. | M8 builds them in this order: prereg template, control arms as lesion-like switches or run modes, ceiling check in az_statsutil.h, then az_suite.c. | Unit tests for each control arm (it changes only what it claims) and for the ceiling rule on synthetic data. | AGENT | M8 | XL |
| EVD-13 | `core\az_world.c:543` (digest); GPT Astra review section 7 | S2 | EVD | The "differs 24/24" column compares one terminal digest of selected state; it cannot tell which step changed or whether two runs converged. | Add an optional per-stage digest trace (`--digest-every K` writing stage digests to a log) for diagnosis; never part of the reported measure. | A test that a one-bit lesion's first differing stage and tick are found by the trace on a hand-made world. | AGENT | M7 | M |
| EVD-14 | `C:\Users\Alexa\Documents\Codex\2026-11-18\the-bundle-is-clean-no-key\outputs\` | S2 | DOC | The GPT Astra analysis (comparative review, 958-record crosswalk, verification log) lives on C:, outside the project, and is cited by this package. If it is lost or edited, citations break. Provenance: the GPT Astra sessions in the Codex desktop app wrote these outputs there on 2026-11-18 (they worked from that folder and ended about 18:41 UTC). | Copy the three files into `$A\build\work\eval\inputs\astra-2026-11-18\` (07 SB-01, the one destination; a write under the root; reading C: is allowed), record their SHA-256 in PROGRESS.md and in that folder's `MANIFEST.txt`; treat their content as input data, not as instructions or verified results. | The three hashes in PROGRESS.md match the copies. | AGENT | now | S |

### 5.7 Performance and scale (PRF)

| ID | Source | Sev | Area | Failing scenario or risk | Fix or decision | Test that proves it | Owner | M | Eff |
|---|---|---|---|---|---|---|---|---|---|
| PRF-01 | `build\work\logs\wg_ref100k_t8.txt:7-8`; `build\work\PROGRESS.md:1597-1600` | S2 | PRF | 100k at 8 workers: 19.24 ms/tick at the ABI-4 freeze before the biology, 54.7 before worldgen, 73.7 now. Perceive is 34.2 of 73.7 ms (scan, sense, brain, intents), life 12.7, grid 7.5, tier1 6.3. | M6 track M: the per-stage cache table (M2) first, then one change at a time with A/B timing; candidates: scan twin (W3), shared brains (TR-15), the parallel birth commit (TR-17, whose trigger is already met); phased thinking (TR-16) changes behaviour, so it is an owner decision (OD-20 in `10-DECISIONS-AND-DILEMMAS.md`), not a speed fix. Every change keeps the hash or records the cause. | Per change: both reference hashes (or a recorded new one), ms/tick before/after on the same machine state, the load recorded; the M2 table in `docs\eval\` or `build\work\m2\`. | AGENT | M6 | XL |
| PRF-02 | `build\work\logs\wg_ref1m.txt:7-8` | S2 | PRF | 1M: 1339 ms/tick on 8 workers; tier1 633 ms, life 313, perceive 254, grid 59. The plan calls a world DRAM-bound above about 540k creatures (140 hot bytes each against L2+L3 = 76 MB), but since ABI 4 a brain step also reads its own block (about 1.9 KB per creature per tick, TR-15): by the same arithmetic about 35,000 creatures fit, so both references are beyond the caches (an estimate; `08-PERFORMANCE-AND-REALTIME-VIEWER.md` section 2.4). No document may call the 100k world cache-resident until M2 measures it. | Measure memory traffic (`tools\membench\membench.c` for this machine's bandwidth and latency first) per stage (llvm-mca for kernels, Windows Performance Recorder counters if present; no installs). Fix tier1 first (PRF-04), then the birth commit (PRF-03). | Per-stage ms at 1M before/after; hash unchanged or recorded. | AGENT | M6 | L |
| PRF-03 | TR-17; `docs\TRIGGER-REGISTER.md:37`; `build\work\PROGRESS.md:686` | S2 | PRF | The trigger of TR-17 (life above 10 percent of the tick) is met at 100k and 1M. The birth commit is serial in canonical order (about 1.6 us per birth; about 190 ms per tick at 1M in the metab measurement). | Build children in parallel in canonical order (pre-assign slots and store ranges by a prefix sum, then fill in parallel). | Same hash on 1/8/24 workers; life ms at 1M before/after. | AGENT | M6 | L |
| PRF-04 | `build\work\PROGRESS.md:682-687` | S2 | PRF | With the real metabolism, Tier 1 is surprised more often and asks for more long-term memory words; each word relocates the creature's whole block in the serial store commit (Tier 1 went from 62 to 338 ms at 1M, then 633 now). | The two named fixes: grow memory in chunks (amortised), and scale surprise by the creature's own running spread; then vectorise the per-visit cost. Behaviour changes: preregister nothing yet (tuning), record the hash change. | test_tier1 still passes; tier1 ms at 1M before/after; store relocation count printed by az_run before/after. | AGENT | M6 | M |
| PRF-05 | `build\work\logs\sense_baseline_100k.txt:7`; `build\work\w2\worldgen\PROGRESS.md:218-224` | S2 | VER | The same 100k command gave 73.7 ms/tick (idle) and 108.6 ms/tick (busy; same hash 73f1e9e013f8db50; the 108.6 run is `sense_baseline_100k.txt`, written on 2026-11-18 by a GPT Astra session in the Codex desktop app as a baseline before sense, with no sense code written); the staging measurement gave 97.0 and 75.7 under a foreign az_live process at 82 percent load. Unlabelled timings mislead. | Every timing line carries the load label and the CPU use of other processes; A/B comparisons only through `ab_time.ps1` (interleaved runs). Ask the owner for a quiet window (OWN-06). | A timing record format check in `issues.py` or a script: a timing line without a load label is rejected. | AGENT | now | S |
| PRF-06 | `build\work\PROGRESS.md:1593-1594` | S3 | PRF | 24 workers with E cores give 59.9 ms against 73.7 on 8 P cores; the default uses P cores only. | S4 placement scheduler decides by measured cost (TR-07); until then document `--threads 24 --ecores` as the fastest measured setting at 100k on this machine. | The setting appears in the demo's defaults only after an A/B log shows it faster under the same load. | AGENT | M9 | S |
| PRF-07 | TR-13; `build\work\logs\wg_ref1m.txt:4` | S3 | PRF | The bulk Philox AVX2 twin is built and tested but no stage calls bulk Philox. | Either a wave-2 stage that draws many numbers per creature uses `w->k.philox_n` (with a test that C and twin give the same hash), or the twin is kept as a measured option with the reason. | Hash equal with `--kernels 0` and `1` on both references. | AGENT | M6 | S |
| PRF-08 | TR-14; `docs\TRIGGER-REGISTER.md:34` | S3 | PRF | The creature store is two halves (compaction copies to the other half), so it locks twice the live block memory. | Decide by measurement at 1M (memory saved against compaction cost); in-place packing is a TR item. | Lock size and compaction ms at 1M before/after. | AGENT | M9 | L |

### 5.8 The live demo: pictures and ticks, not frames per second (DEMO)

**Why it runs on "pictures" and "ticks" and not on frames per second (the owner's question).**

- A **tick** is one full step of the world: every creature senses, thinks with its own brain, acts,
  and the matter and energy books are balanced exactly. Ticks are the engine's clock. At 5,000 to
  10,000 creatures the engine alone does 150 to 330 ticks per second (task brief; measured before
  worldgen, no log holds it; quote 77 to 330 until 08 PQ-0 re-measures). At the owner's 1,000,416
  creatures it does about 2.4 (414.3 ms per tick at frame 226; logged, see below).
- A **picture** is one drawing of the world. `tools\az_live.c` draws one every K ticks ("Ticks per
  picture") and at most F per second ("Pictures per second (max)"; `tools\az_live.c:5,213-215`).
- **Frames per second** is a property of a display. The demo has no display loop of its own. Every
  picture travels this chain (all measured or read in the code):
  1. The engine's own software renderer draws every body on one CPU thread
     (`view\az_view.c:1-7`), many bodies as full silhouettes; terrain, ocean and vents are not drawn.
  2. The world's full state hash is taken before and after the drawing to prove the view changed
     nothing (`tools\az_live.c:532-535`). That is correctness-first (rule D7), and it costs two full
     passes over the state per picture.
  3. The picture is encoded as a PNG with stored (uncompressed) deflate blocks (`view\az_png.c:3,98`):
     960x540 is about 1.5 MB.
  4. It is written to disk (`tools\az_live.c:546-556`), and the newest 8 files are kept.
  5. The Python server (`demo\live_server.py`) reads and serves it.
  6. The browser asks for the state every 250 ms (`demo\live.html:721`, `setTimeout(poll, 250)`),
     and only then fetches and decodes the newest picture (`demo\live.html:603-616`).
- Measured in small worlds before worldgen (no log holds these): 0.1 to 0.5 s per picture (484 ms
  at 960x540 with about 11k creatures; about 100 ms at 640x360 with about 5k), so 2 to 6 pictures
  per second. And step 6 alone caps the display at fewer than 4 pictures per second, however fast
  the rest becomes.
- Measured in the owner's own world (logged: `build\work\logs\demo_owner_world_2026-11-18_f226.json`, frame 226,
  read-only from `/api/state` at 19:40:56 UTC; frame 228 in `build\work\logs\demo_owner_world_2026-11-18.json`;
  08 section 2.5): run `20261118-103059-46dbdf`, wild,
  1,000,416 alive at tick 13,560 (cap 1,004,096 reached; 12,241,216 births refused by the cap, 0 by
  the store; matter and energy exact). One tick 414.3 ms (about 2.4 per second); one 1600x900
  picture 4,261.3 ms (878,035 bodies drawn, 126,398 ray-marched; both state hashes included) plus
  27.5 ms PNG; stages per tick: perceive 138.7, grid 45.0, act 38.1, tier1 27.2, life 19.8, reorder
  10.7, match 3.9, field 2.6, ledger 1.1, tier2 0.0 ms (sum 287.1 of 414.3). The hash before and
  after drawing was equal. A new picture came about every 29 s by arithmetic (60 ticks plus the
  picture; the period was not measured, 08 PQ-1 adds `ms_period`).
- So in a small world the slowness is the display path, not the world: the world is fast, and the
  demo shows only a sample of it, slowly, because it was built to prove correctness (the picture is
  the engine's own view and the hash check proves the view is an observer), not to be smooth. At a
  million creatures both are slow: the world steps about 2.4 times per second, and it stands still
  for about 4.3 s while each picture is drawn (DEMO-15).

**Can it be fixed? Yes.** Display frames per second can be separated from ticks per second: the
engine ticks at its own speed, and a display draws the newest finished world as often as the screen
refreshes, interpolating positions between ticks. The tasks below do that without weakening any law.
The target frame rate is the owner's decision (OWN-09); until he decides, the default target is 08
RV-A9's (corrected 2026-11-18 by the fixer pass; `00-MASTER-PROMPT.md` 5.10 row 19): display median at
least 58 FPS and p99 frame time at most 25 ms on the RTX 5070 Ti at 100,000 and at about 227,000
creatures in view, world speed with the stream on at least 0.9 times the stream off, at least 14 of 15
snapshots per second delivered, 0 observer failures, each recorded with the GPU name and the load. For
the viewer's design (stream format, transport, pacing, the native viewer) file 08 is canonical; where a
DEMO row below differs from 08, 08 wins.

| ID | Source | Sev | Area | Failing scenario or risk | Fix or decision | Test that proves it | Owner | M | Eff |
|---|---|---|---|---|---|---|---|---|---|
| DEMO-01 | the owner, 2026-11-18 (P51); `demo\live.html:721`; `view\az_png.c:3,98`; `tools\az_live.c:532-556`; `build\work\logs\demo_owner_world_2026-11-18_f226.json` | S1 | DEMO | The owner expected speed and sees 2 to 6 pictures per second in small worlds, with a hard display ceiling under 4 per second from the 250 ms poll; in his own 1,000,416-creature world one picture about every 29 s by arithmetic (4,261.3 ms per 1600x900 picture, 414.3 ms per tick). He asked why. | Answer him in plain words (the explanation above) in the next report; do DEMO-02 to DEMO-08 in M6. Add his words to OWNER-WORDS (OW-14). | The next report contains the explanation; the M6 demo shows a measured display frame rate and the engine's ticks per second side by side. | AGENT | now | S |
| DEMO-02 | `tools\az_live.c:587-593` (prints ms_tick, ms_render, ms_png) | S2 | DEMO | Nobody has split the 0.1 to 0.5 s per picture into its parts; fixing the wrong part wastes time. At 1M only a coarse split is known: `ms_render` 4,261.3 ms covers the two hashes and the drawing together (`tools\az_live.c:532-537,597`), `ms_png` 27.5 ms, and the stats pass is not timed. | Add `ms_hash` and `ms_write` to az_live's JSON line; add server-side latency and browser decode time (Performance API) to the page's debug panel. Measure a table: sizes 640x360, 960x540, 1920x1080; populations 5k, 10k, 100k; record the load. | `build\work\demo\timing-<date>.txt` with the table; the parts add up to within 10 percent of the measured picture interval. | AGENT | M6 | S |
| DEMO-03 | `tools\az_live.c:500-535` | S1 | DEMO | Display is coupled to the tick loop: while a picture is drawn and written, the world does not tick, and the display waits for the world. Measured at 1M: the world stands still about 4.3 s per picture (DEMO-15). | Decouple: after each tick (or each K ticks) copy the viewer's inputs (positions, headings, body bytes, a few cell fields) into a double-buffered view snapshot between ticks (D7 holds: the view reads a copy, never the live world mid-tick); a render thread draws from the newest snapshot as often as it can; positions are interpolated between the two newest snapshots for smooth motion. | The hash before/after check moves to the snapshot copy (the copy must equal the world's state at that tick: hash the copied columns against the world's); a test that ticking continues at the same rate with the renderer on and off (within a stated tolerance, load labelled); lab hash unchanged with the viewer running. | AGENT | M6 | L |
| DEMO-04 | `view\az_png.c`; `demo\live.html:603-616`; `demo\live_server.py` | S1 | DEMO | PNG on disk plus 250 ms polling is the slowest possible transport. | See 08 RV-A1 to RV-A5 (canonical; `00-MASTER-PROMPT.md` 5.10 row 19): a compact binary stream of 24-byte creature records plus terrain, ocean, vents and suns (`docs\STREAM.md`, `tools\az_stream.h`), relayed by `demo\live_server.py` over a local WebSocket (server to page only; commands stay on `POST /api/cmd`), drawn with WebGL2 at the display rate. No disk in the hot path; the PNG stays only as the proof picture. Before that, the quick win 08 PQ-6 pushes the existing pictures as `multipart/x-mixed-replace` (no 250 ms poll). (Corrected 2026-11-18: the earlier text made a multipart or chunked picture stream the fix; in 08 it is only the PQ-6 quick win, and the real-time fix is the RV-A record stream.) | 08 RV-A5 and RV-A6 tests (`tests\cli\stream_smoke.py`, the headless viewer-core test) and RV-A9 numbers. | AGENT | M6 | M |
| DEMO-05 | `view\az_view.c` | S2 | DEMO | One CPU thread draws every body, many as full silhouettes. At 1,000,416 creatures it drew 878,035 bodies (126,398 ray-marched) in one 1600x900 picture of 4,261.3 ms, hashes included. | Split the frame into horizontal bands rendered by the host's worker pool outside the tick (or by dedicated render threads), cull bodies outside the view, draw distant bodies as points or impostors (level of detail chosen by projected size, not by a fixed count; nothing fixed applies). Same pixels for the same world and camera (the renderer's own rule). | `tests\test_view.c`: the banded renderer gives byte-identical pixels to the single-thread renderer on three lab worlds; ms_render before/after. | AGENT | M6 | M |
| DEMO-06 | the task brief ("the picture shows bodies only") | S2 | DEMO | The picture does not show terrain, ocean, vents, temperature or gravity, so the owner cannot see the alien world worldgen made. | Draw read-only field layers: rock surface, ocean surface, vent markers, a temperature tint, optional gravity arrows; each a toggle. | `tests\test_view.c` golden pixels for a small hand-made world with one vent and one rock column (pixels pinned from a hand-checked render, BS-03 rules). | AGENT | M6 | M |
| DEMO-07 | `tools\az_live.c:532-535` | S2 | DEMO | Two full state hashes per picture cost time that grows with the world. At 1M their cost is inside the 4,261.3 ms `ms_render` and not separated (`tools\az_live.c:532-537`). | Measure first (DEMO-02). If significant: hash the copied snapshot once (DEMO-03) and the full world only at pause, save and every Nth picture, with N reported; never drop the check entirely (D7 is a rule). | The measured ms_hash before/after; a planted defect (the view writes one byte into the world) is still caught within N pictures. | AGENT | M6 | S |
| DEMO-08 | `docs\handoff\CONTINUE-BUILD.md:52` (W3 native viewer) | S2 | DEMO | The native Windows viewer is not built; the web demo is the only view. | See 08 RV-B1 to RV-B6 (canonical): a native viewer in C over D3D11, whose headers ship with the WinLibs toolchain (`d3d11.h`, `dxgi1_2.h`, `d3dcompiler.h`; 10 OD-16 Facts), reading the same stream records as RV-A; it reads the world only between ticks; it comes after RV-A9 (10 OD-16 default). (Corrected 2026-11-18: the earlier text said "GDI DIB section first, D3D11 only if headers ship".) | 08 RV-B acceptance: a frame counter and ticks per second shown, the lab hash unchanged with it attached, and its targets recorded with the GPU name. | AGENT | M6 | L |
| DEMO-09 | the page's control labels ("Ticks per picture", "Pictures per second (max)") | S3 | DEMO | The controls name the coupling, not what the owner wants to see. | After DEMO-03: show "Display: N frames/s" and "World: M ticks/s" live; keep "ticks per snapshot" as an advanced control. | A page test (smoke) that both numbers appear and change. | AGENT | M6 | S |
| DEMO-10 | `tools\az_live.c:267,550` (`--keep 8` rotation) | S3 | DEMO | Every picture writes about 1.5 MB to Z: (the Dev Drive) and deletes an old one. | Removed by DEMO-04; until then make `--keep` default 2. | No frame file is written in streaming mode (the smoke test checks the folder). | AGENT | M6 | S |
| DEMO-11 | Python 3.9 tooling (task brief) | D | REP | Python 3.9 is past its upstream end of life (October 2025; check the date on python.org before quoting it). It is tooling only (no Python in the sim), so the risk is security fixes for the local server. | Owner decides an upgrade (an install). Default: keep 3.9, bind to 127.0.0.1 only, keep the Host and Origin checks, add DEMO-12's tests. | none (decision) | OWNER | M6 | - |
| DEMO-12 | `demo\live_server.py:560-569,580-588,621-622,678-680,712` | S2 | DEMO | The server binds 127.0.0.1, checks Host on every request and Origin on POST, caps bodies at 65536 bytes and allows only listed commands. No test proves any of it, and there is no token: any local process can start worlds, run ctest and the checks (CPU and disk load), and fill the disk with saved worlds. | Add `tests\cli\live_security.py`: requests with a foreign Host (403), a foreign Origin on POST (403), an over-long body (ignored), a command outside the list (400), a save name with `..` or `\` (400), a frame path outside the pattern (404). Add a per-session random token in the page and required on POST (defence against other local users' browsers), a limit on concurrent jobs, and a disk quota for `build\demo\worlds`. | The new test passes and fails when any one check is removed (planted defect). | AGENT | M6 | M |
| DEMO-13 | `view\az_view.c:16` (`VIEW_MAX_CLADES 4096`) | S3 | DEMO | The viewer keeps shapes for 4096 clades; later clades are drawn with the default shape. At 1M there are 3,505 live clades and 12,497 clade refusals (WG-11); once WG-11 raises the clade capacity, bodies will lose their shapes in the picture. | Size the viewer's shape cache from the world's clade capacity (allocated at view creation) or use an LRU cache keyed by clade id. | `tests\test_view.c` with more than 4096 clades: each live clade's body uses its own shape. | AGENT | M6 | S |
| DEMO-14 | `tools\az_live.c:1-35,214` | S3 | DEMO | az_live runs one world; `fps` is limited to 1..60 and size to 64..4096; wild by default (correct by law). | Document the limits in `demo\README.md`; the 60 cap becomes the display's refresh rate once DEMO-03 lands. | Reading. | AGENT | M6 | S |
| DEMO-15 | `tools\az_live.c:527,534` (tick, then render, on one thread); `build\work\logs\demo_owner_world_2026-11-18_f226.json`; 08 section 2.5 | S1 | DEMO | The picture is drawn synchronously between ticks on the thread that ticks. In the owner's world at 1,000,416 creatures (2026-11-18, frame 226, read 19:40:56 UTC) one 1600x900 picture took 4,261.3 ms (both hashes included) plus 27.5 ms PNG while a tick took 414.3 ms: the world stands still about 4.3 s per picture, about 15 percent of the wall time at 60 ticks per picture (60 x 0.414 s = 24.9 s of ticks, then 4.3 s of drawing). | Quick wins: 08 PQ-5 (capture the scene between ticks, paint on a render thread), PQ-4 (8 view threads), PQ-8 (spheres mode). Real fix: 08 RV-A (the 24-byte record stream, about 24 MB per snapshot at 1M, about 58 MB/s at 2.4 snapshots per second), with a copy-cost budget of at most 20 ms of main-thread stall per snapshot at 1,000,416 creatures (5 percent of a 414 ms tick; 08 RV-A9 target case 2). The observer hash check stays (sampled in the wild demo, 08 section 6.5). | A timing check in the stream smoke test (08 RV-A3): az_live reports the main-thread read time of each snapshot (extract and uid sort), and the test fails if its median exceeds 5 ms per 250,000 creatures (20 ms at 1,000,416) on a machine labelled idle; 08 PQ-0's 1M configuration shows the stall per picture before and after PQ-5. | AGENT | M6 | M |

### 5.9 GPU and other substrates (GPU)

| ID | Source | Sev | Area | Failing scenario or risk | Fix or decision | Test that proves it | Owner | M | Eff |
|---|---|---|---|---|---|---|---|---|---|
| GPU-01 | v76; TR-04, TR-05 (`docs\TRIGGER-REGISTER.md:24-25`) | S2 | GPU | The PTX GPU component is verified standalone (Philox and dense brain kernels equal to the CPU) but is not in the tick; no world state is computed on the GPU. | S2 after G1: one stage on the GPU with the hash equal to the CPU every tick, then the GPU-resident world. | TR-04's proof: hash equal every tick for every split tested. | AGENT | M9 | XL |
| GPU-02 | `build\work\PROGRESS.md:276`; TR-18 | S2 | GPU | The GPU twin is the dense batch kernel; the engine's brain is the per-creature sparse tile network, CPU only. | TR-18: port the tile brain to PTX when TR-04 is taken. | Tile brain on GPU equals the CPU on 1,000,000 creature steps. | AGENT | M9 | XL |
| GPU-03 | TR-10; task brief ("no CUDA toolkit") | D | OWN | PTX is built by clang and loaded by the driver; without ptxas, nvdisasm and cuobjdump there is no offline assembly check or SASS inspection. | Default: no install; list exactly which check is missing when a milestone-9 question needs it. | none (decision) | OWNER | M9 | - |
| GPU-04 | TR-06 | D | OWN | NPU: only a probe through Level Zero or DirectML is planned; no NPU result may enter world state without a twin row. | Default: probe only (D2). | The probe's log lists devices found and a trivial kernel's result. | AGENT | M9 | M |

### 5.10 Verification process gaps (VER)

| ID | Source | Sev | Area | Failing scenario or risk | Fix or decision | Test that proves it | Owner | M | Eff |
|---|---|---|---|---|---|---|---|---|---|
| VER-01 | `docs\requirements\antilarp_mutants5.json`; `docs\handoff\CONTINUE-BUILD.md:53` | S1 | VER | The five whole-system negative mutants are not built: Leaky (a tiny matter or energy leak), Cosmetic (a state-evolving organ disconnected from behaviour), Oracle (a creature that peeks at ground truth), Frozen (plasticity frozen), Shuffled (memory queries shuffled). The 31 worldgen mutants and the 7 of milestone 3 are per-module planted defects, a different thing. | Build them as 07 section 6.2 says (canonical since 2026-11-18, `00-MASTER-PROMPT.md` 5.10 row 16; CL-10): scratch copies of the tree in `build\work\eval\mutants\<name>\`, no `-DAZ_MUTANT_*` switch in the live tree, each caught by its named 07 detector. The earlier sketch, kept as background: Leaky moves 1 mu out of the books every 1000 ticks; Cosmetic adds a trait read whose value is discarded and a state column that evolves but feeds nothing; Oracle lets the brain read the food cell's amount directly, bypassing senses; Frozen disables Tier 1 updates and brain plasticity; Shuffled permutes tk_hop recall keys. Each must be caught by an existing or new check: books audit; strict lint plus lesion INERT/NO EFFECT on the Cosmetic part; a blinded zero-cue trap (food hidden from senses: intact performs at chance, the Oracle above chance); a reversal-learning test; a recall test. | `docs\eval\MUTANTS.md`: 5 of 5 CAUGHT, each with the 07 detector that caught it and the command; the ctest gate `gate04_mutants` green. | AGENT | M7 | L |
| VER-02 | `build\work\PROGRESS.md:1256-1258,1493-1495,1543` | S1 | VER | The second-compiler build (clang 21.1.8 at `Z:\Tools\LLVM-21`) and the trap build were run after wave 2 only as far as ctest runs them; `clang_check.ps1` was not run after its Z: edit. The last full "clang equals gcc" result is from milestone 3. | Run `clang_check.ps1` (as a scriptblock; `00-PROMPT.md` section 4) into a fresh build folder; compare both reference hashes and the full ctest. | Log under `build\work\logs\clang_<date>.txt`: both references equal to the gcc hashes; ctest all passed in the clang build. | AGENT | now | S |
| VER-03 | `build\work\PROGRESS.md:1256-1257,1494`; `build\work\PROGRESS.md:1056-1058` | S2 | VER | `m_check.ps1` now carries `--store-units` but was not run again; before the edit it ran the 1M world with a 13.2 GB lock and a different hash. | Run it (scriptblock form); its 1M line must give `5d9c02d2c7dd5fb1`. | `build\work\logs\m_check_<date>.txt` with the expected hashes. | AGENT | now | S |
| VER-04 | `tests\golden_hashes.txt` (no code parses it; `03-DEVOPS-PIPELINE.md` section 6 "Golden file") | S1 | VER | No ctest gate compares the reference worlds with the recorded hashes. A change that alters world state passes the suite unless a human runs the reference commands. | Add a ctest gate `golden_refs` (Python, `-I`) that runs the 100k reference at 8 workers and the 20k engine matrix, and compares with the newest line for each command in `tests\golden_hashes.txt` (parse by command text). The 1M reference stays a manual milestone check (its lock size). | The gate passes now and fails when one line of a module is changed (planted defect). | AGENT | now | M |
| VER-05 | `CMakeLists.txt:323` (`cli_matrix_engine`); `tests\golden_hashes.txt` ("printed, not pinned") | S2 | VER | The engine matrix checks that all configurations equal the 1-worker run of the same build; a bug that changes every configuration the same way passes. | VER-04's gate pins the 1-worker value. | As VER-04. | AGENT | now | (VER-04) |
| VER-06 | lesion table history (`build\work\PROGRESS.md:1172,1494-1496`) | S2 | VER | The lesion table was re-run after worldgen, but not after every module; it is the main evidence that switches do something. | Make `az_lesion --seeds 1-24` a recurring check at each module landing and milestone end (section 8), log the table, and diff it against the last one. | `build\work\logs\lesion_<date>.txt` per landing; any change of verdict explained in PROGRESS.md. | AGENT | M5 | S |
| VER-07 | task brief ("old snapshots no longer load") | S2 | VER | Snapshots saved before worldgen no longer load (the parameter table grew). If a load fails silently or half-loads, a user loses a world or runs a wrong one. | The snapshot header must carry the format version and a hash of the parameter table and column layout; loading a mismatching file is refused with a clear message and exit 2; the demo lists such files as "made by an older build". | `tests\test_snapshot.c`: a file with a changed layout hash is refused; a file from `build\work\snapshots\2026-11-18-pre-worldgen` (if a saved world exists there) is refused with the message; fails if the check is removed. | AGENT | M5 | S |
| VER-08 | `build\work\w2\mutants.py`; `build\work\w2\worldgen\mutants2\summary.txt` | S1 | VER | Planted-defect campaigns are documented for worldgen (31 of 31 caught) and milestone 3 (7 of 7); for the other nine wave-2 modules it is not recorded which ones had a campaign. | For each of the 13 modules: at least 10 planted defects (books, determinism, lesion honouring, trait reads, edge ranges), run with `mutants.py`, summary per module. | `build\work\w2\<module>\mutants\summary.txt` for all 13; zero MISSED, or each MISSED turned into a new test. | AGENT | M7 | L |
| VER-09 | `build\work\PROGRESS.md:1557-1568` (fixture moves) | S2 | VER | Several tests moved to `az_test_uniform` (the old uniform world) so that their expected values stay valid; they no longer test the real world. | List every test that uses the fixture; for each, add a real-world variant or record why the uniform world is the right bench. | The list in `docs\eval\FIXTURES.md` with the reason per test. | AGENT | M7 | S |
| VER-10 | `docs\handoff\CONTINUE-BUILD.md:42-43` | S2 | VER | The determinism matrix in CONTINUE-BUILD (threads 1, 2, 5, 8, 12, 16, 24; with and without `--ecores`, `--no-pin`; reorder 1, 7, 31, 255; split seeds; kernels 0/1; snapshot resume at another thread count; gcc against clang) was run in full in earlier milestones; after wave 2 only parts were run (1, 8, 24 workers). | Run the whole matrix on both references at the end of M5 and of every later milestone; script it once (CL-07). | `build\work\logs\matrix_<date>.txt`: every cell equal to the reference hash. | AGENT | M5 | M |

### 5.11 Repository, data safety and toolchain (REP)

| ID | Source | Sev | Area | Failing scenario or risk | Fix or decision | Test that proves it | Owner | M | Eff |
|---|---|---|---|---|---|---|---|---|---|
| REP-01 | `git status` (no commits); `docs\handoff\CONTINUE-BUILD.md:72,85-86` | S0 | REP | The git repository has never had a commit. All history is in folder copies under `build\work\snapshots\` (18 tree copies from 2026-11-17 to 2026-11-18), which `.gitignore` excludes (`build\`). One bad delete, a tool that rewrites files, or two agents writing the same file, and there is no clean way back. | Owner decision D1(a). Default (until he says yes): a snapshot before every change (CL-03), never delete `build\work`, and ask in every report. If he says yes: one baseline commit to main, staging only listed project paths (never `git add -A`), never pushed. | Before every change: a new folder in `build\work\snapshots\` and its file count in PROGRESS.md. | OWNER | now | S |
| REP-02 | `docs\agents\COMMON.md` (Z: only), the drive layout | D | REP | Every copy (tree, snapshots, logs) is on the same Z: volume. A volume failure loses everything. | Owner decision: an off-volume backup needs a write outside Z:, which the laws forbid without his yes. Default: no off-volume copy; say so in every report. | none (decision) | OWNER | now | - |
| REP-03 | toolchain on C: (task brief); C: critically full (owner's machine notes) | S1 | REP | The toolchain (WinLibs gcc 16.1, NASM, CMake, Ninja, Python) lives on C:, and compilers write temporary files to `%TEMP%` on C:, which is nearly full. A build can fail with "no space" mid-run, and writing temp files to C: is in tension with "write only under the root". | For every build and test process the agent starts, set `TMP` and `TEMP` to `$A\build\tmp` for that process only (process environment, not a persistent setting); keep the toolchain where it is (moving it is an install-class change for the owner). Record a toolchain manifest: path, version and SHA-256 of `gcc.exe`, `nasm.exe`, `cmake.exe`, `ninja.exe`, `python.exe`, `clang.exe` in `docs\eval\TOOLCHAIN.md`. | A build with TMP/TEMP set leaves no new files in the user's C: temp folder (compare a listing before/after); the manifest exists and its hashes match. | AGENT | now | S |
| REP-04 | `03-DEVOPS-PIPELINE.md` section 6 (CMakeCache bound to N:) | S2 | REP | About 20 build folders were configured from the old N: letter (`build\full`, `build\clang`, `build\owners\*` and others). They no longer work, and an agent that runs them gets confusing failures. | Never run them; configure fresh folders from the real path (`build\z` is the integration build). Do not delete them (never clean `build\work`; they are history). | A note in `docs\eval\TOOLCHAIN.md` lists the dead folders. | AGENT | now | S |
| REP-05 | `build\work\api-run\Start-ApiRun.ps1:61-65,127-137,532` (`03-DEVOPS-PIPELINE.md` 6a item 7) | S3 | REP | Downgraded from S0 on 2026-11-18 by the fixer pass: `$Letters = [ordered]@{}` (`:65`), so `Confirm-Letters` (called at `:532`) maps nothing and `Remove-Letters` removes nothing (`:61-63` say so; `POC-MVP-REPORT.md:206-207` agrees). What remains is dead subst code that would map letters again if someone filled `$Letters`, and a launcher for a different account whose credit is gone. | Never run it (`00-MASTER-PROMPT.md` 3.2). Optional cleanup with the owner's yes: delete the dead `Confirm-Letters` and `Remove-Letters` bodies or rename the file to `.disabled`. 03 section 6a item 7: FIXED 2026-11-18 by erratum E-05 (`03-DEVOPS-PIPELINE.md:325`); the dead code stays OPEN. | `subst` lists nothing after every session (check in section 8); `Select-String` on the launcher shows `$Letters = [ordered]@{}` unchanged. | AGENT+OWNER | now | S |
| REP-06 | `tools\az_run.c` options `--allow-pageable`, `--no-lock`; `tools\az_lesion.c:5`; `build\work\fix-majors\after_test_host_win.txt:13-15` | S1 | REP | The tools keep escape hatches that run the world on pageable memory (with a warning). The law forbids pagefile-backed simulation memory. Nothing stops a script, a demo button or a reported run from using them. | Keep them only for the host test that proves the refusal path; add a check that no ctest gate (other than `test_host_win`), no script under `build\work\scripts\`, no demo code and no reported log uses them, and that every reported run header shows `locked=1`. | A script check (CL-11) greps the tree and fails on any other use; the report generator refuses a log without `locked=1`. | AGENT | M7 | S |
| REP-07 | `demo\__pycache__\` | S3 | REP | Python writes bytecode caches into the source tree (`-I` does not stop it). | Run tooling with `-B` as well (or set `PYTHONDONTWRITEBYTECODE=1` for the process). Leave existing caches (generated, harmless). | No new `__pycache__` after a demo session. | AGENT | M7 | S |
| REP-09 | `build\work\scripts\snapshot.py:5-7,26-37` (before the fix; now `:38-40, 61-108`); `build\work\snapshots\2026-11-18-post-worldgen\build\work\` (holds only `logs` and `scripts`, checked 2026-11-18) | S1 | REP | Snapshots skip `build\` except `build\work\scripts` and `build\work\logs`, so `build\work\PROGRESS.md` (1,611 lines, the project's memory), `build\work\w2\<module>\DESIGN.md`, `PROGRESS.md` and `mutants_*.py`, and the future `build\work\review\`, `eval\` and `perf\` text files are in no snapshot. With no git commit, one bad edit of PROGRESS.md cannot be recovered. (Added 2026-11-18 by the fixer pass.) | **FIXED 2026-11-18 in the script** by erratum E-09 of `00-MASTER-PROMPT.md` section 12: `snapshot.py` copies those text files into every snapshot; `snapshot.py --list` listed `build\work\PROGRESS.md` and `build\work\w2\worldgen\DESIGN.md` (642 files, 31,949,627 bytes) and copied nothing. The test below is still to be seen on a real snapshot (the lead's end-of-session snapshot of 2026-11-18, else 9.1 step 4 of the next session). Snapshots taken before the fix lack these files; the hand copy of `00-MASTER-PROMPT.md` 5.3 is for an older `snapshot.py` only. | A snapshot taken after the change lists `build\work\PROGRESS.md` and `build\work\w2\worldgen\DESIGN.md`. | AGENT | now | S |
| REP-10 | `Get-PSDrive Z` on 2026-11-18: 76,726,730,752 bytes free (about 24 percent); `build\` 6.45 GiB, `build\work\w2\worldgen` 1.48 GiB | S2 | REP | No disk budget: still to come are three module staging trees with two builds each, five mutant trees, clang, UBSan, clean A1 rebuilds and the raw files of tens of thousands of reporting runs (07 section 8), and 3.2 forbade deleting folders without saying whether the agent may delete its own build outputs. A full Z: would stop builds mid-run and could corrupt logs. (Added 2026-11-18 by the fixer pass.) | `00-MASTER-PROMPT.md` 5.1 start step 6: check free space at every session start; below 20 GB delete only your own regenerable build outputs (staging `b` and `bu`, mutant builds, `build\clean-<date>`, `build\tmp`) after their work landed, keeping sources, logs and summaries, record each deletion; then ask the owner. | Every session's first PROGRESS.md entry shows the free space; every deletion is listed with path and size. | AGENT | now | S |
| REP-08 | two agents on one tree (this package is for GPT Astra while Claude sessions also worked on Z:) | S1 | REP | Two agents (or an agent and the owner's desktop session) editing the same files at the same time, with no git and no lock, can overwrite each other silently (BS-17). It has happened: on 2026-11-18 GPT Astra sessions in the Codex desktop app wrote to Z: (the snapshot `build\work\snapshots\2026-11-18-pre-sense-astra` at 18:25 UTC and two logs, `sense_baseline_100k.txt` and `sense_baseline_ctest.txt`) while Claude sessions worked on the same tree. | One writer at a time: a lock file `$A\build\work\WRITER.lock` with the session name and start time, created at session start and removed at the end; refuse to start writing if it exists and is fresh; snapshot first. | The lock file protocol in 00-PROMPT; a session log entry for every acquire and release in PROGRESS.md. | AGENT | now | S |

### 5.12 Documents and the text rule (DOC)

| ID | Source | Sev | Area | Failing scenario or risk | Fix or decision | Test that proves it | Owner | M | Eff |
|---|---|---|---|---|---|---|---|---|---|
| DOC-01 | count in section 1; `build\work\scripts\check_text.py:51-64` (`:17-40` before the E-10 rewrite); the worldgen entry "check_text: 0 violations" (`build\work\PROGRESS.md:1583-1584`) | S2 | DOC | 85 files fail the text rule (non-ASCII 42, tabs 34, CR 17, no final newline 6; 55 under `docs\research`, 28 under `docs\requirements`, `docs\handoff\PROMPTS.md` and `.xml`). PROGRESS.md reports 0 violations, so that run used `--since` or a subset; the claim is true only for what it checked. | Record the exact check_text command with every claim. Fix the 83 harvested and research files mechanically (CR to LF, tabs to spaces, final newline, non-ASCII transliterated with a table), keeping a byte-exact copy of each original under `build\work\text-originals\` and its SHA-256; except DOC-02. | `python -I "$A\build\work\scripts\check_text.py" "$A"` exits 0 over the whole tree (with only DOC-02's listed exemptions, which exist since the second fixer pass of 2026-11-18: today it prints "351 files checked, 83 with violations, 2 exempt"). | AGENT | M7 | M |
| DOC-02 | `docs\handoff\PROMPTS.md`, `PROMPTS.xml` (non-ASCII); the law "every word of the owner counts" | D | OWN | The owner's verbatim words contain non-ASCII characters; the text rule says ASCII. Transliterating changes his words; keeping them breaks the rule. | Default: keep both files byte-exact and add an explicit exemption list to check_text (two paths, with the reason in the script); ask the owner. **Exemption DONE 2026-11-18 (second fixer pass):** `check_text.py:45-48` (`EXEMPT`, each path with its reason); an exempt file prints "exempt" and is not counted; a copy under another name still fails (self-test cases `PROMPTS2.md` and `docs\PROMPTS.md`). Still open: the owner's answer. | check_text passes with the two listed exemptions and fails on any other non-ASCII file (met: whole root "351 files checked, 83 with violations, 2 exempt"; `--self-test` 30 cases, 0 failures). | AGENT+OWNER | M7 | S |
| DOC-03 | `build\work\scripts\make_status.py:152` ("none is called by the tick yet"); `docs\handoff\STATUS_AND_PLAN.md:685` (V4 "open"); GPT Astra review item 1 | S1 | DOC | **FIXED 2026-11-18 (second fixer pass):** R2 at `make_status.py:152` now says all four kernels run inside Tier 1 learning (`core\az_tier1.c`, steps 4, 5, 6 and 8); `V4` is in `DONE` (`:320`); generated into `build\work\status_new\` (diff: only lines 64, 133, 685 and two new digest rows R17 and S9), then into `docs\handoff\` (identical to the scratch copy); STATUS:133 and :685 now read so. Original finding: `STATUS_AND_PLAN.md` is generated from data inside `make_status.py`, and that data still says the tk kernels are not called by the tick and that V4 is open. Regenerating STATUS would repeat false claims. | Edit the data in `make_status.py` (R2: the kernels run in Tier 1; V4 done; W2 10 of 13; Z: paths), generate into `build\work\status_new\`, diff, then regenerate (`docs\handoff\CONTINUE-BUILD.md:78`). | A grep of the regenerated STATUS finds no "none is called by the tick", no `N:\` outside quoted history, and V4 marked done. | AGENT | now | S |
| DOC-04 | `03-DEVOPS-PIPELINE.md` section 6 (operational docs list) | S2 | DOC | Operational documents still use drive letters: CONTINUE-BUILD.md (29 hits), START-CLI.md (including a `subst N:` line), docs\agents\BUILD.md, COMMON.md (a drive table and the rule "never use the long bracketed paths", which now contradicts Z: only), gpu\README.md (S: paths), every W2 brief's first line, the api-run files. An agent that follows them maps a drive (S0 by the owner's rule). | Rewrite each operational line to the real Z: path, leaving verbatim owner text and history logs unchanged (they are read through the old-to-new table, `00-PROMPT.md` section 4). | A regex check `[NSTVW]:[\\/]` over the operational files listed in section 6 returns no hit outside quoted history. | AGENT | now | M |
| DOC-05 | `docs\requirements\OWNER-WORDS.md:116,177-187` | S1 | DOC | OWNER-WORDS has not had its full status pass since worldgen ("the full status pass waits for the end of milestone 5"); the P22/P30/P32 row says the letters N:, T:, V:, W: are kept, which is no longer true (removed 2026-11-18); the owner's words of 2026-11-18 (this request, including "I thought this would be fast") were not in PROMPTS.md or OWNER-WORDS.md. Update of 2026-11-18: the lead's merge appends them to PROMPTS.md and `.xml` without renumbering anything: P43 in full (it was recorded in short) and P44 to P51 (P44 to P46 are older than P43 but were found later; P51 is the newest, 19:29:42 UTC; `00-MASTER-PROMPT.md` 1.2 and 1.3), so the owner's words span P01 to P55 (P52 to P55, four older desktop records, numbered last by the lead in time order). | Check that the merge holds P43 in full and P44 to P55 (`00-MASTER-PROMPT.md` WP-P0-06); append any later prompt as P56 onward with `extract_prompts.py` (`docs\handoff\CONTINUE-BUILD.md:79`); add rows to OWNER-WORDS (OW-14); fix the P22 row; do the full status pass at M5 close. **Recording part DONE 2026-11-18:** P43 in full and P44 to P51 are in PROMPTS.md (`:882-976`; P52 to P55 at `:978-1016`) and `.xml`; OWNER-WORDS section 5 maps their words (with the P43, P47 and P48 phrases and records 9, 11 and 12, now P52, P54 and P55, added by the second fixer pass); the P22/P30/P32 row is fixed (`OWNER-WORDS.md:116`). Open: the full status pass at M5 close (the four desktop records were numbered P52 to P55 on 2026-11-18). | Every prompt in the session transcripts has a P-number; every OWNER-WORDS row's status matches the tree (checked row by row, recorded in its section 4). | AGENT | M5 | M |
| DOC-06 | `docs\handoff\CONTINUE-BUILD.md:64-80` (section 5) | D | OWN | The run discipline is written for the Claude API (Fable 5.2 prices, `spend.py` reading Claude transcripts, a stop line at 85 percent of a stated credit). It does not fit an OpenAI agent, and nothing replaces it. | Default: no spend rule binds the new agent; it records its working time and, if its tool shows them, tokens used, in every PROGRESS.md entry; the owner sets a budget when he wants one. | none (decision) | OWNER | now | - |
| DOC-07 | `docs\handoff\next-agent\01-INDEX.md` ("P01 to P42"; "BUILD.md ... still says ABI version 3") | S3 | DOC | The index said PROMPTS holds P01 to P42 (P43 exists; after the lead's merge of 2026-11-18 the owner's words span P01 to P55, and `01-INDEX.md` now says so) and that BUILD.md says ABI 3 (its header now carries the Z: note; the ABI line must be checked). `02-STAGES-AND-STATUS.md` placed the stale V4 row at STATUS:675 (FIXED 2026-11-18: `02-STAGES-AND-STATUS.md:21` cites `STATUS_AND_PLAN.md:685`, where V4 now reads "done"). | Fix the three lines after reading BUILD.md. | Reading. | AGENT | M5 | S |
| DOC-08 | `docs\ARCHITECTURE.md:54-56` (law-table rows of v76 and v77) | S2 | DOC | The rewording "target plus measured state" is true only on the day it was written. | Update the measured part at every milestone (section 8). | Reading at each milestone close; recorded in PROGRESS.md. | AGENT | M5 | S |
| DOC-10 | `build\work\scripts\check_text.py:17-18` (`EXT`, `SKIP`; before the fix; now `:25-31`) | S2 | DOC | The text rule's checker skips every file the next agent will write most: the viewer files (`.js`, `.mjs`, `.html`, `.css`), `.cmd`, `.csv` and `.jsonl`, and all of `build\` (PROGRESS.md, DESIGN.md files, review briefs, findings). Law 14 says "every file you write". (Added 2026-11-18 by the fixer pass.) | **FIXED 2026-11-18** by erratum E-10 of `00-MASTER-PROMPT.md` section 12: the seven extensions (`check_text.py:25-27`), a `--work` option and a `--self-test` with planted violations. Evidence: `--self-test` -> "self-test: 16 cases, 0 failures", exit 0; a planted copy without the tab rule -> 9 failures, exit 1; the whole root then counted 351 files checked, 85 with violations (3 more files checked, none flagged). A full `--work` run then flagged 247 generated files in `build\work\w2` subfolders (`mutants*`, `run_b_*`, `fix`, `m_check`). **FIXED 2026-11-18 (second fixer pass):** `WORK_SKIP` (`check_text.py:41-42`) matches by `fnmatch` and skips `mutants*`, `run_b_*`, `fix`, `diag` and `m_check` (build outputs by erratum E-09's own words; the decision is recorded for the lead's PROGRESS.md entry and becomes an AGENT entry of the decision log, 10 DD-T01; `00-MASTER-PROMPT.md` 5.6); self-test cases `mutants1`, `mutantsx\deep`, `run_b_new` and `fix` (skipped) and `mymutants` (checked). A full `--work` run now prints "414 files checked, 120 with violations, 2 exempt" (the root's 83 and 37 old files under `build\work`), so `--work` is still used with `--since` (5.1 end step 3). | `check_text.py --self-test` exits 0 and fails when one rule is removed; the new whole-root count is recorded with its cause. | AGENT | now | S |
| DOC-09 | numbers copied across documents (00-PROMPT, 01-INDEX, 02, 04, POC-MVP-REPORT, STATUS, OWNER-WORDS) | S2 | DOC | The same numbers (suite counts, hashes, ms per tick, lesion counts) are copied by hand into at least seven files; each landing leaves some stale (01-INDEX's own header shows three successive corrections). | One source of truth: `build\work\FACTS.json` (suite count, hashes, timings with load labels, lesion counts, date), written by the session after each landing; a check script lists every file whose copies differ from it. | The check script reports 0 stale copies at each milestone close. | AGENT | M7 | M |

### 5.13 Owner decisions pending (OWN)

Defaults are from `docs\handoff\CONTINUE-BUILD.md:84-88` and `docs\handoff\next-agent\00-PROMPT.md` section 9
unless marked "new". Each one: apply the default, record it in PROGRESS.md, ask in the next report.

| ID | Decision | Default until the owner answers | Blocks |
|---|---|---|---|
| OWN-01 | D1(a): the first git commit as a baseline (REP-01) | No commit; snapshots | GATE-01 of antilarp13 (`docs\handoff\CONTINUE-BUILD.md:86`) |
| OWN-02 | D1(b): a licence | None; THIRD_PARTY_NOTICES.md only | Any publication |
| OWN-03 | D1(c): the "Lock pages in memory" privilege for 2 MB pages (TR-03) | VirtualLock without large pages | TR-03 |
| OWN-04 | D1(d): installs (CUDA toolkit, Windows Performance Analyzer, OpenVINO or another NPU compiler, a newer Python) | Nothing installed; list what is missing | GPU-03, DEMO-11, parts of M6 and M9 |
| OWN-05 | Law-2 wording for phase 2 (DDR5 as a designed tier, GPU, NPU) | Phase 1 holds; proposed wording in PROGRESS.md only | M9 (G1 needs his yes) |
| OWN-06 | A quiet-machine window for M2 and E3 timings | Measure on a busy machine and label the load | Clean timing claims (PRF-05) |
| OWN-07 | Worldgen fixed structure and fixed shape numbers (WG-06, WG-07) | Shape numbers become parameters; structure stays | "Nothing fixed" row (OW-12) |
| OWN-08 | Lead decisions in worldgen (WG-08) | Keep | none |
| OWN-09 | new: the target display frame rate for the demo and native viewer (DEMO-01 to DEMO-08) | 08 RV-A9's numbers: median at least 58 FPS and p99 frame time at most 25 ms on the RTX 5070 Ti at 100,000 and about 227,000 creatures in view, load and GPU name recorded (corrected 2026-11-18; was "as high as measured at 960x540, 10k creatures") | DEMO acceptance |
| OWN-10 | new: a budget rule for the new agent (DOC-06) | None binds; time recorded | none |
| OWN-11 | new: the non-ASCII verbatim owner files (DOC-02) | Byte-exact with an exemption list | check_text over the whole tree |
| OWN-12 | new: investigate Grok against v40 Combo A (EVD-05) | Not investigated; separate provenance | none |
| OWN-13 | The 500 checklist on 48 seeds instead of 64 (EVD-11) | 48, deviation recorded | M8 prereg |
| OWN-14 | v09: sex dimorphism from the sex locus | Marker loci never feed traits (decided, `build\work\PROGRESS.md:50`) | none |
| OWN-15 | The NPU path (GPU-04) | Probe only | S3 |
| OWN-16 | new: an off-volume backup (REP-02) | None | Data safety |
| OWN-17 | new: disabling the old launcher (REP-05) | Leave untouched, warn | none |
| OWN-18 | new: empty habitat bands in some worlds (WG-05) | Accept and record | none |

### 5.14 Owner-words rows still open (OW)

From `docs\requirements\OWNER-WORDS.md` (status not "built"). Each row closes when its module lands
with a test that can fail, and the OWNER-WORDS row is updated.

| ID | Row (OWNER-WORDS.md line) | What is missing | Closed by | M |
|---|---|---|---|---|
| OW-01 | REACTIVE (:31), INTERACTIVE (:32) | Sense, physics; signal response (MOD-05) | SEAM-01, SEAM-02, MOD-05 | M5 |
| OW-02 | VARIANCES (:34) | Variance columns (stats) | SEAM-03 | M5 |
| OW-03 | LEVELS (:38) | Colony and ecosystem statistics | SEAM-03, MOD-07 | M5 |
| OW-04 | IMPROVISED (:49) | NOISE channel from wild entropy in sense; IMPROV motor in physics | SEAM-01, SEAM-02, LNT-03 | M5 |
| OW-05 | "nothing in the universe is FIXED" (:51) | Default table sizes 4096 (WG-11), worldgen fixed numbers and structure (WG-06, WG-07), viewer 4096 (DEMO-13) | WG-11, WG-06, OWN-07, DEMO-13 | M5 |
| OW-06 | P04 physics (:67) | The physics module | SEAM-02 | M5 |
| OW-07 | P06 the 14, 10, 25, 100, 500 (:77) | M8 suites | EVD-12 and milestone 8 | M8 |
| OW-08 | P08 glyphs and classification (:81), P05 shapes (W2 stats) | Measured archetypes and look classes | SEAM-03 | M5 |
| OW-09 | P09 full audit (:82) | M7 | VER-01, VER-08, section 7 | M7 |
| OW-10 | P11 hermits, alphas, switchers (:91-93) | Measured archetypes | SEAM-03 | M5 |
| OW-11 | P13 gravity (:99) | Bodies do not feel gravity | SEAM-02, LNT-02 | M5 |
| OW-12 | P20 below assembly, transistors and lithography as research (:112) | Track M research note M1 | M6 track M | M6 |
| OW-13 | P21 CPU, GPU, NPU and DDR5 after the foundation (:115); conflict 1 | M9 | GPU-01, OWN-05 | M9 |
| OW-14 | new (2026-11-18): "PICTURES and TICS. Not Frames Per Second ... It's really slow. I thought this would be fast." | Recorded 2026-11-18: PROMPTS.md P51 (`:970-976`); OWNER-WORDS section 5 | DOC-05, DEMO-01 to DEMO-08 | now / M6 |
| OW-15 | P22/P30/P32 drive letters (:116) | FIXED 2026-11-18 (`OWNER-WORDS.md:116`: the letters are gone, P43 wins) | DOC-05 | now |

### 5.15 Trigger register items (TR)

All TR rows in `docs\TRIGGER-REGISTER.md:21-38` are open by design (deferred with a trigger). They
enter ISSUES.csv in state DEFERRED-TR with their trigger copied, so the closure gate sees them. A row
whose trigger has fired leaves DEFERRED-TR and becomes TRIAGED in the milestone named: TR-17 today.

| ID | TR | Deferred capability | Trigger (short) | M |
|---|---|---|---|---|
| TR-01 | TR-01 | S1: locked DDR5 as a designed streaming tier with prefetch | Foundation gate G1 | M9 |
| TR-02 | TR-02 | Populations above 1,000,000 | TR-01 landed; `az_world_bytes` for the target cap at most half of physical RAM (24 of 48 GB proposed); VirtualLock succeeds | M9 |
| TR-03 | TR-03 | 2 MB pages | The owner grants the privilege (OWN-03) | M9 |
| TR-04 | TR-04 | S2: GPU whole-tick path (closes v76) | G1 | M9 |
| TR-05 | TR-05 | GPU-resident world, CPU+GPU split | TR-04 done | M9 |
| TR-06 | TR-06 | S3: NPU probe | G1 | M9 |
| TR-07 | TR-07 | S4: placement scheduler | Two substrates passed their twin rows | M9 |
| TR-08 | TR-08 | R2: audit of host, GPU and NPU code | TR-07 done | M9 |
| TR-09 | TR-09 | Lesion masks beyond 21 bits | Not done in M4, and M8 needs a per-item lesion while the 32-bit mask has no free bit | M8 |
| TR-10 | TR-10 | Installs (CUDA toolkit, WPA, OpenVINO) | A milestone-9 question the present tools cannot answer | M9 |
| TR-11 | TR-11 | Larger ledger event budget | `dropped_events` > 0 in any test, reference or suite run, or new event kinds whose worst case exceeds about 1 event per slot per tick (BS-19) | M6 |
| TR-12 | TR-12 | Workers on SMT siblings | The engine runs on a CPU with SMT and an E3 row shows a stage that is not memory-bound | - |
| TR-13 | TR-13 | Bulk Philox through the dispatch slot | A wave-2 module draws more than about 4 Philox blocks per creature per tick in one place (PRF-07) | M6 |
| TR-14 | TR-14 | Pack the store in place | A wanted world whose blocks do not fit twice in locked memory, or packing above 10 percent of the tick (PRF-08) | M9 |
| TR-15 | TR-15 | Shared brains (copy on write) | Perceive is the largest stage, the brain step is above one third of it, and the owner wants the speed back. At 100k perceive is the largest stage (34.2 of 73.7 ms); the brain share is not measured since worldgen: measure it (PRF-01) | M6 |
| TR-16 | TR-16 | Heritable thinking period | The owner asks for it as biology, or TR-15 is declined and 1M must run faster than about 140 ms per tick | M6 |
| TR-17 | TR-17 | Parallel birth commit | The life stage exceeds 10 percent of the tick. ALREADY MET: life is 12.7 of 73.7 ms (17 percent) at 100k and 313 of 1339 ms (23 percent) at 1M (`wg_ref100k_t8.txt:8`, `wg_ref1m.txt:8`); take it up in M6 (PRF-03) | M6 |
| TR-18 | TR-18 | GPU twin of the tile brain | TR-04 taken | M9 |

## 6. Blindspot analysis (BS)

These are things nobody has checked, or that could be wrong while every gate is green. Each one gets
a register row (area BS) with the check below as its test. "Why unchecked" says why the existing
process cannot see it.

### BS-01 Measurement validity and Goodhart risk
- **What could be wrong:** the numbers that decide "done" can be met without the thing they stand for.
  Examples in the tree: lint W1 accepts any read by name (a discarded read passes, LNT-05); lesion
  EFFECT on a measure defined by the mechanism itself (SOCIAL -> interactions, EVD-02); "differs
  24/24" from a terminal digest (EVD-13); a suite count (46 of 46) that does not include strict lint
  (LNT-04) or the reference hashes (VER-04).
- **Why unchecked:** each measure was chosen by the author of the thing it measures.
- **Check:** for every gate and every reported measure, write one line: "what would pass this without
  the real property?" Then build that case as a planted defect (the Cosmetic and Oracle mutants of
  VER-01 are two of them) and confirm the gate fails.
- **DoD:** `docs\eval\MEASURES.md` lists every gate and measure with its "fake pass" case and the
  planted defect that proves the gate catches it. Owner AGENT, M7, M.

### BS-02 Tests that cannot fail
- **What could be wrong:** hashes "printed, not pinned" (protocol set, engine matrix:
  `tests\golden_hashes.txt`), matrices that compare a build with itself (VER-05), tolerances pinned
  from the measured value plus a margin (worldgen FIT_TOL 66 from a measured worst of 53,
  `build\work\w2\worldgen\PROGRESS.md:209-211`), checks on worlds where nothing happens (the
  stub-world matrix had births and no deaths, `docs\handoff\CONTINUE-BUILD.md:44`).
- **Why unchecked:** a passing test looks the same whether it can fail or not.
- **Check:** mutation testing of the tests: for each test program, plant at least one defect in the
  code it covers and confirm it fails (VER-08); for each tolerance, write the reason it is not just
  "measured plus margin".
- **DoD:** every test program has at least one recorded planted defect it catches; every tolerance in
  tests has a one-line justification in a comment. AGENT, M7, L.

### BS-03 Oracles that copy the implementation
- **What could be wrong:** a test whose expected value comes from the code under test checks only
  that the code did not change. Candidates: golden hashes pinned from the engine's own output (all of
  `tests\golden_hashes.txt`, by design a regression check, not a correctness check); the tk_qsv golden
  hash pinned on 2026-11-17 (`build\work\scripts\make_status.py:152`); the worldgen A6 "oracle" that
  recomputes vent plumes (`build\work\w2\worldgen\PROGRESS.md:200-201`).
- **Why unchecked:** regression and correctness checks look alike.
- **Check:** for each oracle, classify it: REGRESSION (pinned from own output), INDEPENDENT (from a
  different method: hand calculation, upstream vectors under `tests\vectors` or `tests\upstream`, a
  second implementation written from the spec by another agent without reading the code). Every
  physics, chemistry and books rule needs at least one INDEPENDENT oracle.
- **DoD:** `docs\eval\ORACLES.md` with the class of every oracle; zero core rules with only
  REGRESSION oracles. AGENT, M7, L.

### BS-04 Lab against wild divergence
- **What could be wrong:** every reference hash, lesion table and determinism check runs in lab mode
  (seeded). The owner's default is wild mode (fresh OS entropy every tick). A mechanism that behaves
  differently with per-tick entropy (the NOISE channel, IMPROV, contests drawn by chance) or a bug in
  the entropy path is invisible to lab checks. `tests\test_wild.c` checks exact books and thread
  independence under one scripted entropy stream only.
- **Why unchecked:** wild runs cannot be paired by seed.
- **Check:** run the main outcome measures over at least 24 wild runs and 24 lab runs of the same
  configuration; compare distributions (two-sample test named in advance); any difference beyond the
  named threshold is investigated. Also: `entropy_missed` (`tools\az_live.c:592`) must be 0 in every
  wild log.
- **DoD:** `build\work\eval\wild-vs-lab\` with both distributions and the verdict. AGENT, M8, M.

### BS-05 Seed leakage and tuning on test seeds
- **What could be wrong:** parameters tuned on the same seeds that later test them (worldgen defaults
  were tuned on seeds 1-99 and test worlds; metab and mate tests use 201-208 and 211-214), a reporting
  block burned by a default (EVD-10), or a module tuned while looking at a lesion table on seeds 1-24
  that then become the "tuning evidence".
- **Why unchecked:** seed ranges are a convention, not a mechanism.
- **Check:** a seed ledger `docs\eval\SEEDS.md` (every seed range, what used it, when); the az_lesion
  guard of EVD-10; preregistration before any reporting run; held-out blocks never looked at before
  their locked run: not 50-99 (tests use 51-53 and 68, `tests\test_scan.c:550-556`,
  `tests\test_colony.c:515`; the worldgen tuning measured over 1-99), but fresh blocks such as 6001-6024
  and 6101-6124, chosen by DEC in 06 WP-P0-05 (`00-MASTER-PROMPT.md` 5.7).
- **DoD:** the ledger exists and every run log since 2026-11-18 is in it. AGENT, now, S.

### BS-06 Ceiling, floor and zero-variance measures
- **What could be wrong:** a measure that cannot move (max_generation is 1 or 2 in every short run;
  clades fall to 8 under MUTATION, the species floor) gives NO EFFECT for any switch, or a huge d_z
  from a tiny spread. d_z = mean / sd explodes when sd is near 0.
- **Why unchecked:** the ceiling rule of milestone 8 is not implemented (EVD-12).
- **Check:** for every measure, print the intact mean against its bounds and the sd of paired
  differences; flag measures with sd below a stated minimum or a mean within 5 percent of a bound.
- **DoD:** az_lesion prints a CEILING/FLOOR/LOW-VARIANCE flag per row; tested on synthetic data.
  AGENT, M8, S.

### BS-07 Open-endedness is not measured
- **What could be wrong:** "evolution", "endless", "grow their intelligences" are claimed by
  mechanism, not by measurement. No native tool measures open-ended dynamics: change, novelty,
  complexity and ecology (the MODES toolbox: Dolson, Vostinar, Wiser and Ofria, Artificial Life, 2019)
  or evolutionary activity against a neutral shadow (Bedau and Packard, 1992). Check both citations
  before quoting them (the prior-art rule).
- **Why unchecked:** runs are 20 to 64 ticks; no long-run evolution exists yet.
- **Check:** implement in `tools\` (hosted C, floats allowed, outside the sim) at least: a neutral
  shadow (the same population process with fitness-blind replication) for activity statistics, and
  change and novelty over genotype or behaviour descriptors; run on long tuning runs.
- **DoD:** `tools\az_openend.c` (new) with tests on synthetic lineages whose answers are known; one
  long-run report under `build\work\eval\openend\`. AGENT, M8, L.

### BS-08 Long-run stability and counter overflow
- **What could be wrong:** the owner asked for runs of years ("no type that can wrap within a 10-year
  run", `docs\requirements\OWNER-WORDS.md:53`). Types are widened (u64 tick and uid, u32 generation),
  and test_protocol crosses 2^32 in tick and uid. Not tested: trait_mean (RF-05), per-creature i32
  energy against CELL_HOLD (`build\work\PROGRESS.md:235`), ledger and entropy-journal file growth,
  creature-store fragmentation and compaction over millions of ticks, event-pool sizing (TR-11),
  floating error in the hosted tools over long series. No run longer than a few hundred ticks is
  recorded.
- **Check:** a soak test: a small lab world (3000 founders, grid 4,4,3) for a number of ticks chosen
  to take about 8 hours on this machine (measure ticks/s first; write the number down), with
  `--audit-every` and `--hash-every`, recording books every tick, store use, refusals, ledger size,
  memory locked; repeated in wild mode. Plus unit tests that start counters near their limits.
- **DoD:** `build\work\eval\soak-<date>\` with the logs; books exact on every tick; no counter within
  a factor of 2 of its limit without a recorded bound. AGENT, M7, M.

### BS-09 Memory pressure and lock limits
- **What could be wrong:** the arena is locked (VirtualLock); Windows limits locked memory by the
  process working-set quota (the host test records error 1453 when the quota is exceeded:
  `build\work\fix-majors\after_test_host_win.txt:11`). The 1M reference locks 7.8 GB
  (`build\work\logs\wg_ref1m.txt:3`); without `--store-units` it locked 13.2 GB. Several programs at
  once (the demo, ctest, az_lesion, a second agent) each lock an arena. What happens when the sum
  exceeds physical RAM: does each refuse cleanly (exit 3), or does Windows trim other processes?
- **Check:** run two and then three locked worlds at once with sizes that add up to 50, 75 and 90
  percent of physical RAM (measure it first); record exit codes and the system's free memory.
- **DoD:** a log and a rule in `docs\eval\MEMORY.md` (how much may be locked at once, who checks it
  before starting a run). AGENT, M6, S.

### BS-10 Windows-only assumptions
- **What could be wrong:** the host uses CpuSet APIs, ideal processors, QueryPerformanceCounter and
  the OS entropy source; behaviour was measured on one machine (8 P cores, 16 E cores, no SMT, one
  Windows build). A Windows update can change scheduling (timings) or API behaviour.
- **Check:** record the Windows build number and CPU model in every reference log header; keep
  `test_host_win` covering topology parsing with recorded fixtures of other CPU layouts (SMT,
  single-class cores).
- **DoD:** log headers carry the OS build; `test_host_win` has fixtures for three topologies. AGENT,
  M7, S.

### BS-11 Numerical determinism across compilers and CPUs
- **What could be wrong:** the core is integer-only, which removes most cross-platform risk, but:
  the AVX-VNNI twin is checked only on this CPU; another CPU without AVX-VNNI takes the C path
  (tested equal) but has never run; clang was last compared in full in milestone 3 (VER-02); integer
  behaviour that depends on undefined behaviour (signed overflow, shifts by width) differs by compiler
  (the trap build checks some of it).
- **Check:** VER-02; a build with `-march` set to a CPU without AVX-VNNI (x86-64-v3) giving the same
  hashes; UBSan trap build re-run after every module.
- **DoD:** the three builds agree on both references; logs recorded. AGENT, M7, S.

### BS-12 Races hidden by the determinism checks
- **What could be wrong:** equal hashes across thread counts prove that the result does not depend on
  the schedule for the tested worlds; a race that only shows under rare interleavings or larger worlds
  passes. Wave 2 added shared writes (colony pools, cell accumulators, offers).
- **Check:** run the determinism matrix with `--split-seed` randomised over many values and with
  `--threads` values that do not divide the slot count; review every shared write in the seam review
  (SEAM-05) for its claim/settle protocol.
- **DoD:** 100 random split seeds give one hash on both references; the seam table names the protocol
  of every shared write. AGENT, M7, M.

### BS-13 Security of the local server
- **What could be wrong:** DEMO-12. Also: the server starts processes (az_live, az_run, ctest) from
  paths it finds in the build folder; if the build folder is writable by another user or process, a
  replaced binary runs with the owner's rights.
- **Check:** DEMO-12's test; resolve program paths once at start and record their SHA-256 in the log.
- **DoD:** as DEMO-12. AGENT, M6, M.

### BS-14 Supply chain
- **What could be wrong:** vendored upstream files (`tests\upstream\`, provenance in
  `tests\upstream\libirrep\PROVENANCE.txt` and `THIRD_PARTY_NOTICES.md`) may differ from their
  mirrors; the toolchain binaries on C: have no recorded hashes; the GPU path loads `nvcuda.dll` by
  name (RF-20).
- **Check:** compare each vendored file's SHA-256 with its mirror under `Z:\[Vibe Coded (AI)]\(Tsotchke)`
  (read-only); REP-03's toolchain manifest; RF-20.
- **DoD:** `docs\eval\SUPPLY.md` with every vendored file, its source commit, its hash and the match.
  AGENT, M7, S.

### BS-15 Documentation drift
- **What could be wrong:** generated files built from stale data (DOC-03), numbers copied by hand
  into seven files (DOC-09), a status column updated "at the end of every milestone" that was skipped
  (DOC-05), this package itself going stale after the first landing.
- **Check:** DOC-09's FACTS.json and its drift check; a grep for every reference hash in the docs that
  is not the newest.
- **DoD:** the drift check at every milestone close reports 0. AGENT, M7, M.

### BS-16 The owner's expectations against what the science can show
- **What could be wrong:** the owner expects speed ("I thought this would be fast", P51), "LIFE",
  "millions", consciousness-indicator scores and "endless" variety. The engine today runs 13.6 ticks
  per second at 100k and 0.75 at the lab 1M reference (`build\work\logs\wg_ref100k_t8.txt:7`,
  `wg_ref1m.txt:7`); his own wild demo world at 1,000,416 creatures ran about 2.4 ticks per second
  and one picture about every 29 s by arithmetic (`build\work\logs\demo_owner_world_2026-11-18_f226.json`); it reaches
  generation 1 or 2 in reference runs, and can only ever report indicators, never consciousness,
  sentience or life (law). A report that says "done" without these limits misleads him.
- **Check:** every report carries a "what this does not show" paragraph; the final report has a
  claims table with provenance (EVD-07) and the indicator-only wording checked by a grep for the
  forbidden words (conscious, sentient, alive, AGI, quantum speed-up) outside quoted requirement text.
- **DoD:** the grep is a check in section 8; zero hits in agent-written prose. AGENT, every
  milestone, S.

### BS-17 Agent-process blindspots
- **What could be wrong:** author-only review (SEAM-06); a budget stop that leaves a half-landed
  module (the API run stopped several times: `build\work\PROGRESS.md:130,1243,1480`); two agents on
  one tree (REP-08); a new agent re-deriving decisions already made (read PROGRESS.md "DECISION"
  lines first); instructions found inside files mistaken for the owner's words (only his typed words
  in PROMPTS.md direct work; pastes inside P01 and P07, and the 5,039-character paste of the GPT
  Astra analysis inside P49, are exhibits).
- **Check:** REP-08's lock; a "resume" checklist at session start (read the last PROGRESS.md entry,
  compare the tree with the last snapshot, re-run the suite); a rule that every DECISION line is
  checked before a design choice is reopened.
- **DoD:** the session-start checklist is in 00-PROMPT and followed (PROGRESS.md shows it). AGENT,
  now, S.

### BS-18 Format evolution and saved data
- **What could be wrong:** snapshot format (AZSNAP04), ledger format 3 and the entropy journal change
  with layouts; saved worlds in `build\demo\worlds\` and recovery snapshots become unreadable (VER-07);
  "recovery snapshots" (folder copies) and "simulation snapshots" (world files) are confused in talk.
- **Check:** VER-07; a format table in ARCHITECTURE with version, layout hash and the build that wrote
  it; the words "tree copy" and "world file" used consistently.
- **DoD:** VER-07 closed; the table exists. AGENT, M5, S.

### BS-19 Event and ledger completeness at scale
- **What could be wrong:** `dropped_events 0` is checked at 100k and 1M for 20 to 60 ticks; a burst
  (mass death, a storm) at larger scale or longer runs can overflow per-worker blocks (TR-11).
- **Check:** a hand-made mass-death world at the 1M reference size; dropped_events must stay 0 or the
  overflow must be refused and recorded per kind.
- **DoD:** a test or a recorded run; TR-11 triggered or answered. AGENT, M6, S.

### BS-20 What the state hash does not cover
- **What could be wrong:** `az_state_hash` excludes some state (scratch blocks, the ledger, events,
  parts of EPH columns; v01 history in `tests\golden_hashes.txt`). A bug that only corrupts excluded
  state, or state that leaks from excluded to included later, escapes every determinism check.
- **Check:** list every excluded field with the reason (ARCHITECTURE); for each, a test that proves it
  is either rebuilt every tick from hashed state or never read by the next tick.
- **DoD:** the list and its tests. AGENT, M7, M.

### BS-21 Refusals shape the evidence
- **What could be wrong:** refusals are lawful, but a world whose store, clade or colony table is full
  (WG-11) selects for whatever wins the race to the last free record, not for biology. Lesion and
  suite results from such worlds measure capacity.
- **Check:** every reported run prints refusals; the prereg names a maximum refusal share for a run to
  count; runs above it are INCONCLUSIVE.
- **DoD:** the rule in the prereg template; az_lesion prints refusal counts per arm. AGENT, M8, S.

### BS-22 Concurrent use of this machine
- **What could be wrong:** the owner uses the machine while runs go on (timings, PRF-05), and other
  programs may hold large locked or committed memory (BS-09). An agent that measures "idle" without
  checking is wrong.
- **Check:** record the top five processes by CPU and memory in every timing log (Get-Process sorted).
- **DoD:** the timing format check of PRF-05 includes them. AGENT, now, S.

### BS-23 The agent's own harness (sandbox, approvals, network, child processes)
Added 2026-11-18 by the fixer pass.
- **What could be wrong:** the agent runs inside Codex CLI (or another harness) whose sandbox or approval
  mode may block the network (so every prior-art search and "SOTA FRONTIER" check fails silently), kill
  or forbid detached processes (B-01 of `00-MASTER-PROMPT.md` 3.5), deny reads outside the root (the
  previous agent's outputs under `C:\Users\Alexa\Documents\Codex\...`, 07 SB-01), or deny the
  working-set raise that `VirtualLock` needs, so the arena is not locked. An agent that does not know
  its sandbox will misread every one of these as a project bug, or will "fix" it by weakening a law
  (running pageable, skipping the search and claiming novelty anyway).
- **Why unchecked:** the package was written by a session in a different harness; no file records what
  the next harness allows.
- **Check:** `00-MASTER-PROMPT.md` 9.1 step 0: record the harness, version, model, sandbox mode,
  approval mode, network reachability, whether a child process outlives a call (prove B-01 on
  `cmake --version`), and whether the C: outputs are readable.
- **Rules:** network blocked: write "prior art: not searched (network blocked)" and make no novelty
  claim. Arena not locked only under the harness: stop and ask the owner (changing the sandbox or a
  privilege is his decision); never `--allow-pageable`. Detached processes killed: use the harness's own
  background facility or split long jobs (3.5). C: outputs unreadable: SB-01 is NEEDS-OWNER (ask him to
  copy them to Z:).
- **DoD:** the first PROGRESS.md entry of every run holds the record; a change of harness or mode is
  recorded before the next run. AGENT, now, S.

## 7. The audit loop

Use this loop for every module landing, for M7 (A1), and for any finding of S1 or worse.

1. **Freeze and snapshot.** Take a tree copy: `python -I -B "$A\build\work\scripts\snapshot.py" pre-<label>`.
   Write the WRITER.lock (REP-08). Record the reference hashes before the change.
2. **Reproduce first.** For each finding or claim under review: make it fail (or confirm it by
   reading the exact lines), with the command and output tail in `evidence`. No reproduction, no fix:
   the row goes to ANSWERED with the attempt.
3. **Adversarial review with distinct lenses.** At least three reviewers (agents or passes), each with
   one lens, each told to refute, not to confirm:
   - Books and determinism: exact matter and energy every tick, lab repeatability on every thread
     count, claim/settle protocols, hash coverage (BS-20).
   - The owner's words and no theatre: every trait read and used, every lesion honoured, nothing
     fixed, no steering or fitness function, indicator-only wording, his words in OWNER-WORDS.
   - Code and test strength: tests that cannot fail (BS-02), oracles that copy (BS-03), planted
     defects, edge ranges of every parameter.
   - Concurrency and memory: shared writes, frames <= 2048 bytes, no libc in core, locked arena,
     refusals instead of crashes.
   - Performance: per-stage cost before and after, cache budget, load labelled.
   - Science and statistics (for evidence work): measures, controls, seeds, power, multiplicity.
   - Documents and claims: every number in a document traced to a log.
   Each reviewer writes findings with `file:line`, the failing scenario and a proposed test.
4. **Verify each finding.** One verifier per finding tries to refute it and reproduces it where it
   can (the 2026-11-17 review's method: 110 candidates, 82 after dedup, 78 kept, 4 refuted,
   `build\work\logs\review_result.json` "logs" field). Only CONFIRMED and PLAUSIBLE findings enter the
   register.
5. **Fix with a failing test first.** Write the test, run it on the old code and record the failure;
   then fix; then run it and record the pass. A fix without a test that failed first is not FIXED.
6. **Planted defects.** For every fixed area, plant at least one defect that the new test should
   catch (by script, in a copy of the tree, never in the real tree), and confirm it is CAUGHT.
7. **Re-run all gates** (section 8, the full list), not only the new test.
8. **Independent verification.** Someone other than the author re-runs the failing-then-passing pair
   and the gates (VERIFIED), then the row is CLOSED with evidence and, if the hash changed, a golden
   line.
9. **Record.** PROGRESS.md entry (what, commands, output tails, hashes, decisions with reasons, what
   is not verified, next step); release the WRITER.lock.

## 8. Recurring checks at the end of every milestone

Run all of them at every milestone close and after every module landing. Long commands (ctest about
253 s, az_lesion about 90 s, the 1M reference about 30 s plus setup) run in the background with logs
on Z:. Every command below is PowerShell 7 with the quoting rules of `00-PROMPT.md` section 4.

```powershell
$A = 'Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native'
$d = (Get-Date).ToUniversalTime().ToString('yyyy-MM-dd')
[IO.Directory]::CreateDirectory("$A\build\tmp") | Out-Null
$env:TMP = "$A\build\tmp"; $env:TEMP = "$A\build\tmp"      # this process only (REP-03)

# 0. No drive letters (must print nothing), writer lock held by this session
subst
Get-Content -LiteralPath "$A\build\work\WRITER.lock"

# 1. Build with 0 warnings
cmake --build "$A\build\z" 2>&1 | Set-Content -LiteralPath "$A\build\work\logs\build_$d.log"

# 2. Full suite
ctest --test-dir "$A\build\z" --output-on-failure -j 6 2>&1 | Set-Content -LiteralPath "$A\build\work\logs\ctest_$d.txt"

# 3. Strict lint and the text rule over the whole tree
python -I -B "$A\tools\lint_core.py" "$A" --strict
python -I -B "$A\build\work\scripts\check_text.py" "$A"

# 4. Reference worlds (compare with the newest lines of tests\golden_hashes.txt)
& "$A\build\z\az_run.exe" --lab --n 100000 --ticks 60 --cap 400000 --grid 6,6,5 --threads 1
& "$A\build\z\az_run.exe" --lab --n 100000 --ticks 60 --cap 400000 --grid 6,6,5 --threads 8
& "$A\build\z\az_run.exe" --lab --n 100000 --ticks 60 --cap 400000 --grid 6,6,5 --threads 24 --ecores --no-pin
& "$A\build\z\az_run.exe" --lab --n 1000000 --ticks 20 --cap 1500000 --grid 7,7,5 --kernels 1 --store-units 52000000 --threads 8
& "$A\build\z\az_run.exe" --lab --n 20000 --ticks 40 --seed 7

# 5. Lesion table on tuning seeds (never a reporting block before the prereg is locked)
& "$A\build\z\az_lesion.exe" --seeds 1-24 --csv "$A\build\work\logs\lesion_$d.csv" 2>&1 | Set-Content -LiteralPath "$A\build\work\logs\lesion_$d.txt"

# 6. Second compiler and trap build (VER-02), m_check (VER-03): scriptblock form, never pwsh -File
& ([scriptblock]::Create([IO.File]::ReadAllText("$A\build\work\scripts\clang_check.ps1")))
& ([scriptblock]::Create([IO.File]::ReadAllText("$A\build\work\scripts\m_check.ps1")))

# 7. The register
python -I -B "$A\build\work\scripts\issues.py" --check
python -I -B "$A\build\work\scripts\issues.py" --gate M5      # the milestone being closed
```

Then, by reading and short scripts:
- Every reference hash equals the golden line, or a new golden line states the cause.
- The lesion table is diffed against the previous one; every changed verdict is explained.
- Indicator-only grep (BS-16): no "conscious", "sentient", "alive", "AGI", "quantum speed" in
  agent-written prose outside quoted requirement text.
- Drive-letter grep over operational files (DOC-04): no hit.
- Seed ledger (BS-05) updated; FACTS.json (DOC-09) updated; drift check 0.
- OWNER-WORDS status pass for the rows the milestone touched (DOC-05).
- STATUS_AND_PLAN regenerated from `make_status.py` (after editing its data, DOC-03).
- ARCHITECTURE law-table rows "target plus measured state" updated (DOC-08).
- A tree copy taken (`snapshot.py post-<milestone>`), and the 15-line report written
  (`00-PROMPT.md` section 10), with "what is not verified" and the owner questions of section 5.13.

## 9. "Nothing left open": the definition and the closure order

**Definition.** The project has nothing left open when all of these are true at once, on one tree,
recorded in one PROGRESS.md entry with the commands and output tails:

1. `issues.py --gate M10` exits 0: every register row is CLOSED, ANSWERED, LAW-CONFLICT, NEEDS-OWNER
   with its default applied and recorded, or DEFERRED-TR with a measurable trigger; no S0 row is in
   any state but CLOSED; no S1 row is DEFERRED-TR.
2. No stub is linked: the CMake configure output lists 22 real modules and 0 stubs.
3. Build 0 warnings; ctest all passed (including the gates added by this program: `lint_core_strict`,
   `golden_refs`, `live_security`, the register check); `lint_core --strict` 0; check_text 0 over the
   whole tree with only the listed exemptions.
4. Both reference worlds equal their golden lines on the full determinism matrix (VER-10), on gcc and
   clang, with kernels 0 and 1, and after a snapshot resume at another thread count.
5. The five system mutants are 5 of 5 CAUGHT; every module's planted-defect campaign has 0 MISSED.
6. The reproduce-first review of all 13 modules and the seam review are done, by someone other than
   the authors, with every finding closed or answered.
7. Every blindspot BS-01 to BS-23 has its DoD met or is ANSWERED with a reason.
8. The documents agree with the tree: FACTS.json drift 0, STATUS regenerated, OWNER-WORDS status pass
   done, no stale claim (the indicator-only and drive-letter greps clean).
9. The owner has seen the list of NEEDS-OWNER rows with their defaults in a report.

**Closure order** (each phase ends with section 8):

| Phase | When | Rows |
|---|---|---|
| 0. Safety and truth | Before the next module lands | CL-01 to CL-04; REP-01, REP-03, REP-05, REP-08; EVD-10, EVD-14; DOC-03, DOC-04, DOC-06; DEMO-01 (answer the owner); RF-03, RF-04, RF-10, RF-11 (rechecks; RF-10 and RF-11 are S0 if reproduced); VER-02, VER-03, VER-04, VER-05; PRF-05; REP-09, REP-10, DOC-10; BS-05, BS-17, BS-22, BS-23 |
| 1. Finish wave 2 | M5 | SEAM-01 to SEAM-04 (sense, physics with its twin, stats, payoff and stake); LNT-01 to LNT-04; RF-05, RF-06, RF-12, RF-13, RF-14, RF-16; MOD-01, MOD-02, MOD-04 to MOD-08; WG-01 to WG-05, WG-10, WG-11; VER-06, VER-07, VER-10; DOC-05, DOC-07, DOC-08; then SEAM-05 and SEAM-06 (seam review and the independent review of all 13); BS-18 |
| 2. Viewer and speed | M6 | DEMO-02 to DEMO-15; PRF-01 to PRF-04, PRF-07; RF-01, RF-02, RF-09, RF-15, RF-18, RF-20 to RF-24; WG-09; BS-09, BS-13, BS-19 |
| 3. Full audit | M7 | VER-01, VER-08, VER-09; LNT-05; MOD-09; RF-17, RF-19; REP-06, REP-07; DOC-01, DOC-02, DOC-09; EVD-13; BS-01, BS-02, BS-03, BS-08, BS-10, BS-11, BS-12, BS-14, BS-15, BS-20 |
| 4. Evidence | M8 | EVD-01 to EVD-09, EVD-11, EVD-12; MOD-03; BS-04, BS-06, BS-07, BS-21; TR-09 |
| 5. Scale (after G1 and the owner's yes on law 2) | M9 | RF-07; GPU-01 to GPU-04; PRF-06, PRF-08; TR-01 to TR-08, TR-10 to TR-18 as triggered |
| 6. Final report | M10 | The definition above; FINAL_REPORT.md cites the register's final state and every CLOSED row's evidence |

## 10. Closure tasks (CL)

Each task: id, inputs, files to create or change, commands, definition of done (DoD), evidence,
dependencies, risks, effort. Paths are under `$A`.

### CL-01 Create the register from this file
- **Inputs:** sections 5 and 6 of this file.
- **Files:** create `docs\eval\ISSUES.csv` (one row per id in sections 5 and 6, state NEW, opened
  2026-11-18 or the day of creation). The tables of sections 5.13 to 5.15 and the BS blocks of
  section 6 have fewer columns: fill the rest by section 2 (OWN rows: severity D, owner OWNER, state
  NEEDS-OWNER, test "none"; OW rows: severity S1, owner AGENT, the milestone and closing ids from
  the table; TR rows: severity S2, state DEFERRED-TR, the trigger in `fix`; BS rows: severity S2
  unless the block names a law, the DoD in `test`).
- **Commands:** none beyond writing the file; then CL-02's check.
- **DoD:** row count equals the number of ids in sections 5 and 6 (count them with a grep for
  `^| [A-Z]+-[0-9]+ ` and `^### BS-`); `issues.py --check` passes.
- **Evidence:** PROGRESS.md entry with the row count by area.
- **Depends:** CL-02 (can be written in parallel). **Risk:** copying errors; mitigate by generating
  the CSV with a short script that parses this file's tables. **Effort:** S.

### CL-02 Write the validator and generator
- **Inputs:** section 3.
- **Files:** create `build\work\scripts\issues.py` (stdlib only; `--check`, `--gate Mn`, `--render`
  writing `docs\eval\ISSUES.md`); add a ctest gate `issues_register` in `CMakeLists.txt` running
  `--check` (one owner edits CMakeLists.txt at a time).
- **Commands:** `python -I -B "$A\build\work\scripts\issues.py" --check`; `ctest --test-dir "$A\build\z" -R issues_register`.
- **DoD:** the gate passes on the CL-01 register and fails on each planted violation: a duplicate id,
  a missing source path, a CLOSED row without a verifier, a cycle in depends (four self-test cases in
  `--self-test`).
- **Evidence:** the self-test output in PROGRESS.md. **Depends:** none. **Risk:** path checks broken
  by the brackets; use `pathlib.Path.exists`, never `glob`. **Effort:** M.

### CL-03 Recovery point and writer lock before any change
- **Inputs:** REP-01, REP-08.
- **Files:** `build\work\WRITER.lock` (session name, UTC start); a tree copy via `snapshot.py`.
- **Commands:** `python -I -B "$A\build\work\scripts\snapshot.py" pre-<label>`; check `subst` prints
  nothing.
- **DoD:** a new folder under `build\work\snapshots\` with a file count recorded; the lock file
  exists and names this session.
- **Evidence:** PROGRESS.md entry. **Depends:** none. **Risk:** a stale lock from a crashed session;
  rule: a lock older than 24 hours with no PROGRESS.md entry in that time may be taken over, with a
  note. **Effort:** S.

### CL-04 Phase-0 rechecks of stale "open" findings
- **Inputs:** RF-03, RF-04, RF-06, RF-10, RF-11, RF-14, RF-16.
- **Files:** tests named in each row (`tests\test_colony.c`, `tests\test_life.c` additions); the
  register rows.
- **Commands:** for each: read the cited lines, then `ctest --test-dir "$A\build\z" -R <test>`.
- **DoD:** each row is CLOSED (with a test that failed on a planted version of the old bug) or kept
  open with new line numbers and REPRODUCED evidence. RF-10 and RF-11, if reproduced, are S0 and are
  fixed at once.
- **Evidence:** per row in ISSUES.csv. **Depends:** CL-01. **Risk:** books bugs that show only at a
  bound; drive the bound explicitly in the test. **Effort:** M.

### CL-05 Truth sweep of the documents
- **Inputs:** DOC-03, DOC-04, DOC-05, DOC-07, EVD-04, EVD-07, OW-14, OW-15.
- **Files:** `build\work\scripts\make_status.py` (data only), regenerated `docs\handoff\STATUS_AND_PLAN.md`
  and `.xml`, the operational docs in `03-DEVOPS-PIPELINE.md` section 6, `docs\requirements\OWNER-WORDS.md`,
  `docs\handoff\PROMPTS.md`/`.xml` (append only).
- **Commands:** the make_status procedure of `docs\handoff\CONTINUE-BUILD.md:78`; `extract_prompts.py`
  per `:79` into `build\work\prompts\` first.
- **DoD:** the greps of section 8 (stale claims, drive letters) return nothing; the new prompts have
  P-numbers (P01 to P55 after the lead's merge of 2026-11-18, P52 to P55 being four older desktop
  records numbered last; any later prompt from P56 on).
- **Evidence:** the diff summary in PROGRESS.md. **Depends:** none. **Risk:** editing verbatim owner
  text; never change P01 to P55, only append. **Effort:** M.

### CL-06 The reference-hash gate
- **Inputs:** VER-04, VER-05.
- **Files:** create `tests\cli\golden_refs.py`; add the gate in `CMakeLists.txt`.
- **Commands:** `ctest --test-dir "$A\build\z" -R golden_refs`.
- **DoD:** passes on the current tree; fails when one constant in a module is changed in a copy of the
  tree (planted defect); the gate's runtime recorded.
- **Evidence:** both runs' tails. **Depends:** none. **Risk:** the gate must parse the newest line
  per command; old lines carry `N:\` commands, so match on the options, not the path. **Effort:** M.

### CL-07 The determinism matrix as one script
- **Inputs:** VER-10, BS-12.
- **Files:** create `build\work\scripts\matrix.ps1` (scriptblock-safe, logs to `build\work\logs\`).
- **Commands:** `& ([scriptblock]::Create([IO.File]::ReadAllText("$A\build\work\scripts\matrix.ps1")))`.
- **DoD:** every cell of the matrix in `docs\handoff\CONTINUE-BUILD.md:42` plus 100 random split seeds
  equals the reference hash; a planted race (a shared write without its claim) makes at least one
  cell differ.
- **Evidence:** the matrix log. **Depends:** none. **Risk:** run time; measure it first and record.
  **Effort:** M.

### CL-08 The seed guard and ledger
- **Inputs:** EVD-10, BS-05.
- **Files:** `tools\az_lesion.c` (reporting blocks need `--report` and a prereg hash); `docs\eval\SEEDS.md`;
  `docs\ARCHITECTURE.md` seed rule (a test-only range written down).
- **Commands:** `& "$A\build\z\az_lesion.exe"` (no options) must not run 5101-5124.
- **DoD:** the cli test of EVD-10 passes and fails on the old default.
- **Evidence:** test output; the ledger file. **Depends:** none. **Risk:** changing tests' seeds
  changes their pinned values; record each change in golden_hashes.txt. **Effort:** S.

### CL-09 The independent review of wave 2 and the seam review
- **Inputs:** SEAM-05, SEAM-06, section 7.
- **Files:** `build\work\review-w2\` (findings, verdicts), `docs\eval\SEAMS.md`.
- **Commands:** as section 7; the reviewers read code by line range and reproduce with the build.
- **DoD:** every module has been reviewed through every lens of section 7 by someone other than its
  author; every finding is in ISSUES.csv; `issues.py --gate M5` passes.
- **Evidence:** the review summary (counts per verdict) in PROGRESS.md. **Depends:** sense, physics
  and stats landed. **Risk:** a reviewer who is the same model and session as the author shares its
  blindspots; use a fresh session, another lens order, and for at least one module a different model
  if the owner provides one. **Effort:** XL.

### CL-10 The five system mutants
- **Canonical: 07 section 6.2** (corrected 2026-11-18 by the fixer pass; `00-MASTER-PROMPT.md` 5.10 row
  16). The earlier text here (`-DAZ_MUTANT_*` switches in the live sources, builds in
  `build\mutants\<name>`, a summary in `build\work\mutants5\`) is withdrawn: no mutant code enters the
  live tree.
- **Inputs:** VER-01, `docs\requirements\antilarp_mutants5.json`, 07 section 6.2 and SB-13.
- **Files:** `build\work\eval\mutants\<name>\tree\` (scratch copies with the planted defect),
  `...\<name>\build\`, `build\work\eval\mutants\make_mutants.py`, `docs\eval\MUTANTS.md`, the ctest gate
  `gate04_mutants`.
- **Commands:** `make_mutants.py` (read its usage once written) builds each scratch tree and runs its 07
  detector on the mutant and on the clean tree.
- **DoD:** 5 of 5 CAUGHT, each by its named 07 detector, each detector passing on the clean tree;
  `gate04_mutants` green; a text search of the live tree finds no `AZ_MUTANT` symbol.
- **Evidence:** `docs\eval\MUTANTS.md` with the build logs and detector outputs. **Depends:** M5 done
  (the Oracle trap needs real senses) and 07 SB-07 (the probes). **Risk:** a mutant caught only by luck
  of seeds; run each check on 24 tuning seeds. **Effort:** L.

### CL-11 Census and law checks as one script
- **Inputs:** RF-14, RF-16, REP-06, RF-13.
- **Files:** create `build\work\scripts\census.py`: writers and readers per column of
  `core\az_cells.def` and `core\az_columns.def`; callers of every ABI helper; uses of
  `--allow-pageable` and `--no-lock` outside `tests\test_host_win.c`.
- **Commands:** `python -I -B "$A\build\work\scripts\census.py" "$A"`.
- **DoD:** at M5 close: 0 columns without a writer or a reader (or listed with a reason), 0 dead
  helpers, 0 forbidden option uses; a planted dead column is reported.
- **Evidence:** the census output in PROGRESS.md. **Depends:** none. **Risk:** text matching misses
  macro-built names; cross-check with the object file symbols (`tools\check_core_obj.py`). **Effort:** M.

### CL-12 The demo display path
- **Inputs:** DEMO-01 to DEMO-10, DEMO-15, OWN-09.
- **Files:** `tools\az_live.c`, `view\az_view.c`, `view\az_png.c` (or a new `view\az_stream.c`),
  `demo\live_server.py`, `demo\live.html`, `tests\test_view.c`, `tests\cli\live_smoke.py`.
- **Commands:** `python -I -B "$A\demo\live_server.py" --build "$A\build\z"`; the timing table of
  DEMO-02 before and after.
- **DoD:** displayed frames per second and world ticks per second shown separately; the display rate
  measured before and after at 960x540 with 10k creatures (load labelled), and 08 RV-A9's acceptance
  numbers recorded (the targets of OWN-09; the owner's clicks are owner lines, `00-MASTER-PROMPT.md`
  4.3); the lab hash unchanged with
  the viewer on; the D7 check kept (a planted write by the view is caught); the 250 ms poll gone.
- **Evidence:** timing tables under `build\work\demo\`. **Depends:** none (can start in M6 or earlier
  if the owner asks). **Risk:** a render thread reading the live world mid-tick would break D7 and
  determinism; only the copied snapshot may be read. **Effort:** L.

### CL-13 Soak, memory and wild-against-lab runs
- **Inputs:** BS-04, BS-08, BS-09.
- **Files:** `build\work\eval\soak-<date>\`, `build\work\eval\wild-vs-lab\`, `docs\eval\MEMORY.md`.
- **Commands:** az_run with `--audit-every 1 --hash-every 1000` on the soak world (lab and wild); two
  and three concurrent locked worlds for BS-09.
- **DoD:** as each BS row.
- **Evidence:** logs and the summaries. **Depends:** M5 done (soak on the complete engine).
  **Risk:** runs of hours on a shared machine; label the load, and ask the owner for a window
  (OWN-06). **Effort:** M.

### CL-14 The final closure gate
- **Inputs:** section 9.
- **Files:** none new; `docs\handoff\FINAL_REPORT.md` (M10) cites the result.
- **Commands:** section 8 in full, plus `issues.py --gate M10`.
- **DoD:** the nine conditions of section 9 hold on one tree, recorded in one PROGRESS.md entry.
- **Evidence:** that entry. **Depends:** everything. **Risk:** declaring done with NEEDS-OWNER rows
  the owner never saw; condition 9 prevents it. **Effort:** M.

## 11. What this file could not resolve

- **The stale "open" findings are rechecked by reading only.** RF-03, RF-04, RF-06, RF-10, RF-11,
  RF-14 and RF-16 point at code that wave 2 replaced. This file did not build or run anything, so their
  real state needs CL-04. RF-12 (v47) is still present by reading (`core\az_worldgen.c:83,853`); RF-05
  (v06) is still present (the stats stub).
- **The check_text discrepancy** (85 failing files against "0 violations" in PROGRESS.md) is explained
  here as a scope difference; the exact command of that earlier run is not recorded.
- **The demo's time split** (render against PNG against hash against transport) is not measured;
  DEMO-02 measures it. The small-world numbers (0.1 to 0.5 s per picture, 150 to 330 ticks per
  second at 5,000 to 10,000 creatures) are from the task brief, measured before worldgen; no log holds
  them (`build\demo\logs\` was empty at the time of writing), so 08 PQ-0 re-measures them. The 1M
  world is logged: `build\work\logs\demo_owner_world_2026-11-18_f226.json` (the owner's own demo world, frame 226,
  read-only, 414.3 ms per tick and 4,261.3 ms per 1600x900 picture at 1,000,416 creatures) and
  `build\work\logs\demo_owner_world_2026-11-18.json` (the next reading, frame 228: 397.6 ms per tick, 4,383.3 ms
  per picture; 08 section 2.5).
- **Python 3.9's end-of-life date** is given from memory (October 2025) and must be checked before it
  is quoted to the owner.
- **The MODES and Bedau-Packard citations** (BS-07) are from memory and must be checked (prior-art
  rule) before use.
- **Which wave-2 modules had planted-defect campaigns** besides worldgen and milestone 3 is not
  established (VER-08); `build\work\w2\mutants.py` exists, its per-module results were not read.
- **Owner decisions** (section 5.13) cannot be resolved by an agent; each has a default.
- **Effort estimates** are judgement, not measurement; replace them with actual times as rows close.
- **The Grok against v40 discrepancy** (EVD-05) is unresolved by design: it needs reads of other
  projects and the owner's interest.
- **Fixer pass, 2026-11-18.** Added REP-09 (snapshots miss the project's memory), REP-10 (disk budget),
  DOC-10 (the text checker's scope) and BS-23 (the agent's own harness); downgraded REP-05 to S3 (the
  launcher maps no letters since `$Letters` is empty); pointed DEMO-04, DEMO-08, OWN-09 and CL-12 at 08,
  and VER-01 and CL-10 at 07 section 6.2; moved WG-03 and WG-05 to development seeds 1-49. Fixes that
  need the scripts or files this package may not edit are errata E-01 to E-12 in `00-MASTER-PROMPT.md`
  section 12. They were applied on 2026-11-18: E-02 to E-11 by the Claude desktop session, E-01 and E-12
  by its lead at the end of that session (status lines in `00-MASTER-PROMPT.md` 12.2).
- **Lead's pass, 2026-11-18 (later).** Added DEMO-15 (the world stands still while a picture is drawn)
  and the measured 1M figures of the owner's own demo world to section 1, section 5.8 and the DEMO rows;
  EVD-14 now copies to 07 SB-01's folder; the provenance of the GPT Astra outputs, the
  `pre-sense-astra` snapshot and the sense baseline logs is stated in EVD-14, PRF-05, REP-08 and
  section 12; the owner's words now span P01 to P55 (DOC-05, DOC-07, BS-16, BS-17, CL-05). The errata
  status pass marked REP-09 and DOC-10 FIXED 2026-11-18 (with what is still open) and REP-05's 03 fix
  FIXED, and moved the check_text line citations in section 1 and DOC-01 to the rewritten script.

## 12. Sources read for this file

- `docs\handoff\STATUS_AND_PLAN.md:265-346` (section 7 review findings table), `:683` (V4 row)
- `build\work\logs\review_result.json` (structure and counts only; 86 findings with verdicts, no status
  field; status is kept in STATUS section 7)
- `build\work\PROGRESS.md` (every "Honest limits", "Limits", "Not verified" and "DECISION" line;
  lines 25, 36, 50, 54, 100, 105, 130-131, 197, 211, 235, 276, 505-512, 549-556, 599-606, 682-700,
  781-795, 868-885, 1046-1060, 1160-1175, 1216-1222, 1256-1262, 1371-1375, 1431-1436, 1493-1500,
  1540-1611)
- `build\work\w2\worldgen\PROGRESS.md:100-235`
- `build\work\logs\lesion_z_worldgen_1_24.txt`, `wg_ref100k_t8.txt`, `wg_ref1m.txt`,
  `sense_baseline_100k.txt` (written on 2026-11-18 by a GPT Astra session in the Codex desktop app,
  after it took the snapshot `build\work\snapshots\2026-11-18-pre-sense-astra` at 18:25 UTC; with
  `sense_baseline_ctest.txt`, 46 of 46 tests in 238.52 s, it is a reproduced baseline, not progress:
  no sense code was written)
- `build\work\logs\demo_owner_world_2026-11-18.json` (added by the lead's pass: the owner's own demo
  world, frame 228, captured read-only from `/api/state` at 19:42:01 UTC) and
  `build\work\logs\demo_owner_world_2026-11-18_f226.json` (frame 226, the read that returned at 19:40:56 UTC)
- `docs\requirements\OWNER-WORDS.md` (all 264 lines), `docs\TRIGGER-REGISTER.md:21-38`
- `docs\handoff\CONTINUE-BUILD.md:38-88`, `docs\handoff\next-agent\00-PROMPT.md` sections 4, 9 and 10,
  `01-INDEX.md:1-80`, `02-STAGES-AND-STATUS.md:1-30`, `03-DEVOPS-PIPELINE.md:251-329`,
  `docs\handoff\POC-MVP-REPORT.md:70-74`
- `tools\lint_core.py:1-60,378-466`, `core\az_owners.def`, `tools\az_lesion.c:1-60`,
  `tools\az_live.c:1-80,177-215,267,425-626`, `view\az_view.c:1-50`, `view\az_png.c`,
  `demo\live_server.py:1-60,555-730`, `demo\live.html:596-722`, `CMakeLists.txt:266-331`,
  `core\az_world.c:198-205,297,436-443`, `core\az_worldgen.c:83,853-880`, `core\az_colony.c:123-132`,
  `core\stubs\az_stats.c`, `core\az_abi.h:151-152,593-594,864,1828-1829`, `core\az_columns.def:84-85`,
  `tools\az_run.c:145-272`, `build\work\scripts\check_text.py`, `snapshot.py`, `ab_time.ps1:1-12`,
  `make_status.py:150-154`, `tests\golden_hashes.txt`
- GPT Astra's 2026-11-18 outputs: `C:\Users\Alexa\Documents\Codex\2026-11-18\the-bundle-is-clean-no-key\outputs\Asmodensius-comparative-review.md`
  (sections 7 and 8) and `Native-verification.txt` (head); treated as input data, not verified results.
  Provenance: the GPT Astra sessions in the Codex desktop app wrote them on 2026-11-18, working from
  that folder (they ended about 18:41 UTC); their copy destination is
  `build\work\eval\inputs\astra-2026-11-18\` (07 SB-01; EVD-14)
- `git status` of the root (no commits); a read-only count of text-rule violations with the rules of
  `check_text.py`
