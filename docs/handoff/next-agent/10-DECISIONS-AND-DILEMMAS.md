# 10 Decisions and dilemmas

> Written 2026-11-18 (UTC) for OpenAI GPT Astra 6.2, or any coding agent that continues
> Asmodensius-Native alone. Read-only research: every claim cites path:line as of the time of
> writing; nothing was built, tested or run for this file. Where I counted something myself
> (files, parameters), the command is given.
>
> Root, written `<root>` below: `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native`.
> Commands use the full path, quoted. PowerShell 7: `-LiteralPath` on every cmdlet, no `>`
> redirection under the root, scripts as `[scriptblock]::Create(...)` blocks
> (`docs\handoff\next-agent\00-PROMPT.md:106-119`).

## Table of contents

1. How to use this file
2. The owner's decisions: summary table
3. The owner's decisions in detail (OD-01 to OD-25)
   - 3.1 Plan D1: commit, licence, large pages, installs (OD-01 to OD-04)
   - 3.2 Plan D2: law 2 phase 2, NPU, quiet window, sex locus, columns, seeds, UEFI folder, wave 2 (OD-05 to OD-12)
   - 3.3 G1, the foundation gate (OD-13)
   - 3.4 Money: the API and credit situation (OD-14)
   - 3.5 Speed and the GPU: the tick, the viewer, the demo (OD-15 to OD-17)
   - 3.6 Machine and files: the C: toolchain, old world snapshots (OD-18, OD-19)
   - 3.7 Biology choices the trigger register gives the owner (OD-20 to OD-22)
   - 3.8 Rules about irreversible acts (OD-23, OD-24)
   - 3.9 The owner's "more things I forgot": the idea-bank catalogs (OD-25)
4. The dilemmas, each with a reasoned resolution (DL-01 to DL-14)
5. Tasks that come out of this file (DD-T01 to DD-T10)
6. The decision log: `docs\eval\DECISIONS.md` format and rule
7. What I could not resolve

---

## 1. How to use this file

The previous lead's rule still holds: work on your own, never wait for a reply. Where a decision
belongs to the owner, take the default, record it, and go on (`docs\handoff\CONTINUE-BUILD.md:3`,
`:84-88`; `00-PROMPT.md:205-217`).

Five rules for every decision in this file:

1. **Never block.** Take the default and keep working. A question to the owner goes into the
   end-of-session report (at most 15 lines, `00-PROMPT.md:221-222`) and into the "Open questions"
   table of `docs\eval\DECISIONS.md` (section 6). Ask at most three questions per report, in the
   priority order of section 2.
2. **One short question.** Each OD below has an "Ask" line: one sentence that the owner can answer
   with yes, no, or a choice of two. Do not send the reasoning with it; point to the DEC id.
3. **Log before you act.** Every default taken, every law reading and every design choice that a
   reviewer could question gets a `DEC-nnnn` entry in `docs\eval\DECISIONS.md` before the work that
   depends on it lands. The `PROGRESS.md` entry for that work cites the DEC id.
4. **The owner's words win; the newest word wins.** Only his typed words direct the work
   (`00-PROMPT.md:29-30`). Where two of his prompts conflict, the newer one wins
   (`docs\requirements\OWNER-WORDS.md:128-151`). When he answers a question, quote his words
   verbatim in the DEC entry and append them to `docs\handoff\PROMPTS.md` as the next P number
   (P56 onward: after the lead's merge of 2026-11-18 his words span P01 to P55, the newest being P51
   of 19:29:42 UTC; P52-P55 are four older desktop records numbered last; `00-MASTER-PROMPT.md` 1.2 and
   1.3), with the date, the channel and "typed".
5. **Irreversible acts never happen by default.** Commit, push, install, download, privilege change,
   deletion of a folder, publication, and any claim of novelty wait for his yes. The default for each
   is "not done" (`CONTINUE-BUILD.md:31`, `:86`; `00-PROMPT.md:97-98`).

How the ids work: `OD-nn` is an owner decision in this file; `DL-nn` a dilemma; `DD-Tnn` a task;
`DEC-nnnn` an entry in the live log. Section 6.4 maps every OD and DL to the DEC id to create first.

---

## 2. The owner's decisions: summary table

Priority: the order in which to ask (1 = ask in the first report). "Now" means the default is
already in force today.

| id | Decision | Default the agent takes | Priority | Ask when |
|---|---|---|---|---|
| OD-16 | Real-time viewer technology | WebGL2 in the existing browser demo first, the native D3D11 viewer second (W3) | 1 | first report |
| OD-14 | OpenAI budget and stop line | no known budget: work in small steps, stop cleanly on any refusal | 2 | first report |
| OD-01 | First git commit | no commit; snapshot copies | 3 | first report |
| OD-17 | May the viewer use the GPU | yes, outside the hashed state | - | not asked (law reading, logged) |
| OD-15 | GPU in the tick before the gate | no; phase 1 holds | 4 | if he repeats "it is slow" |
| OD-05 | Law-2 wording for phase 2 | phase 1 holds; milestone 9 waits | 5 | interim report (end of milestone 8) |
| OD-13 | G1 gate passed, go to scaling | wait | 5 | interim report |
| OD-06 | NPU path | probe only; no NPU result in world state | 6 | after the S3 probe |
| OD-07 | Quiet-machine window | measure the load and label it | 7 | before M2 and E3 |
| OD-02 | Licence | none; THIRD_PARTY_NOTICES.md only | 8 | with OD-01 |
| OD-03 | "Lock pages in memory" for 2 MB pages | VirtualLock without large pages | 9 | at M3 |
| OD-04 | Installs (CUDA toolkit, WPA, OpenVINO, Level Zero headers) | nothing installed; list what is missing | 10 | when a named question needs one |
| OD-10 | 64 paired seeds for the 500 checklist | the 48 preregistered seeds; deviation recorded | 11 | at the milestone-8 preregistration |
| OD-18 | Move the C: toolchain to Z: | leave it on C: | 12 | any time, low priority |
| OD-19 | Convert old world snapshots | keep the files, do not convert | 13 | only if he asks for an old world |
| OD-11 | Delete `build\dropped-uefi-2026-11-17` | leave it | 14 | any time, low priority |
| OD-08 | Sex dimorphism from the sex locus | marker loci never feed traits (decided in M1) | - | already default; ask only if a module needs it |
| OD-09 | Drop unused columns (v59, v63) | no column dropped (decided in M4) | - | closed by default; log only |
| OD-12 | Wave 2 in parallel or one by one | one module at a time | - | closed by default; log only |
| OD-20 | Creatures that do not think every tick (TR-16) | not built | 15 | only if he asks for speed at 1M |
| OD-21 | Replay a wild run from the entropy journal | no replay | 16 | only if a wild event needs it |
| OD-22 | Worldgen fixed structure as parameters | convert the biology parts after physics lands (DL-02) | - | not asked; his words P39-P42 already decide it |
| OD-23 | Push, publish, public claims | never | - | never asked unprompted |
| OD-24 | Spending on anything other than tokens | never | - | never asked unprompted |
| OD-25 | Idea-bank catalogs (07 section 6.15, 3,267 rows): trace only or score a selection | trace only (IDEA-BANK), per `CONTINUE-BUILD.md:57` | - | second report (with 08 O-3; `00-MASTER-PROMPT.md` 7.4) |

---

## 3. The owner's decisions in detail

Each entry gives: the question, the default and why, what changes if the owner decides otherwise,
the "Ask" line, the laws it touches and how the default honours them, and the DEC id (section 6.4).

### 3.1 Plan D1: commit, licence, large pages, installs

Source for all four: `CONTINUE-BUILD.md:85-86`, `docs\handoff\STATUS_AND_PLAN.md:673`,
`docs\handoff\next-agent\02-STAGES-AND-STATUS.md:36-39`.

#### OD-01 D1(a): the first git commit

- **Facts.** `.git` exists, nothing has ever been committed, and there is no remote
  (`00-PROMPT.md:65-66`). Recovery points are full copies made by
  `build\work\scripts\snapshot.py` in `build\work\snapshots\` (18 copies exist on 2026-11-18, the
  newest `2026-11-18-pre-sense-astra`, made 18:25 UTC by a GPT Astra session in the Codex desktop app
  as a recovery point before sense; 517 files, 28.9 MB). GATE-01 of `antilarp13.json` stays blocked
  until the first commit (`CONTINUE-BUILD.md:86`).
- **Default.** No commit. Before each module, each milestone and each risky change, run
  `python -I "Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\work\scripts\snapshot.py" <label>`
  (label: letters, digits, `-`, `_`; it refuses to overwrite).
- **Why.** A commit is the owner's act (`CONTINUE-BUILD.md:86`: "Commits happen only when he asks").
  Snapshots give recovery without it. They cost about 8 to 29 MB each
  (`CONTINUE-BUILD.md:75`; `build\work\PROGRESS.md:1580` records 28.7 MB for pre-worldgen).
- **If he says yes.** Commit directly to `main`, never a pull request, never pushed
  (`CONTINUE-BUILD.md:86`). Steps: `git -C "<root>" status --porcelain`; check that `.gitignore`
  excludes `build\`; stage named top-level paths only (never `git add -A`); review the staged list
  for any file over 10 MB or anything that looks like a key (`sk-...`); commit with a message that
  names the reference hashes of `tests\golden_hashes.txt:69-71`; record the commit id in PROGRESS and
  in the DEC entry. GATE-01 can then be evaluated in milestone 8. Snapshots continue anyway (they
  also hold `build\work\scripts` and `build\work\logs`, which git does not).
- **Ask.** "May I make the first git commit of the source tree to main (no push, no remote)?"
- **Laws.** Honest evidence (law 8): the commit id becomes the citation for "this code ran".
- **Effort if yes.** S.

#### OD-02 D1(b): a licence

- **Facts.** No `LICENSE` file exists at the root (checked 2026-11-18 with `Test-Path`).
  `THIRD_PARTY_NOTICES.md` exists and covers QGTL, libirrep, spin_nn and Moonlab (MIT), with the
  upstream LICENSE files copied beside each vendored copy (`STATUS_AND_PLAN.md:683`).
- **Default.** No licence: all rights stay with the owner; the notices file stays.
- **Why.** A licence is a legal choice about his work. Choosing one by default would be an
  irreversible act once anything is shared.
- **If he chooses one.** Write `LICENSE` with the exact text of the licence he names and the
  copyright line exactly as he gives it (never guess his name or the year). Keep
  `THIRD_PARTY_NOTICES.md`. Check that the chosen licence is compatible with the MIT components
  (MIT is compatible with nearly all licences; say so only after reading the chosen licence's text).
- **Ask.** "Do you want a licence on the code, and if so which one?"
- **Effort if yes.** S.

#### OD-03 D1(c): "Lock pages in memory" for 2 MB pages

- **Facts.** The account is an administrator, the session is not elevated, and the privilege is not
  granted (`STATUS_AND_PLAN.md:703`, `:703`). `--large-pages` and `AZ_WIN_LARGE_PAGES` exist and fail
  cleanly today (`docs\TRIGGER-REGISTER.md:23`, TR-03).
- **Default.** VirtualLock without large pages. Every reported run shows the arena locked; a failed
  lock is a failed run, never rerun pageable (`CONTINUE-BUILD.md:30`).
- **Why.** Only the owner can change Local Security Policy (`STATUS_AND_PLAN.md:673`), and the gain
  is unmeasured. TR-03 is a phase-2 row: it needs the foundation gate as well.
- **If he grants it.** He sets it himself (Local Security Policy, User Rights Assignment, "Lock pages
  in memory", his account). A new logon is usually needed before it takes effect; check with
  `whoami /priv` and look for `SeLockMemoryPrivilege`. Then TR-03's done-when applies: the same hash
  with and without `--large-pages`, and a measured gain (the 5 percent at 1,000,000 creatures in
  TR-03 is the previous lead's proposed number, not the owner's).
- **Ask.** "Will you grant your account 'Lock pages in memory' so the arena can try 2 MB pages?"
- **Laws.** Law 2: large pages are still locked RAM; nothing pageable.
- **Effort if yes.** M (TR-03 measurements).

#### OD-04 D1(d): installs

- **Facts.** Missing on 2026-11-17: ptxas, nvdisasm, cuobjdump (the CUDA toolkit), the Windows
  Performance Analyzer (the Recorder is present), Level Zero headers under `Z:\Tools`, OpenVINO
  (`STATUS_AND_PLAN.md:705`; `CONTINUE-BUILD.md:61`). Present: clang-built PTX loaded by the CUDA
  driver, llvm-mca (LLVM 21, knows arrowlake-s), Windows Performance Recorder, ze_loader.dll,
  DirectML.dll 1.15.6 and the NPU driver's own compiler DLL (`TRIGGER-REGISTER.md:26`, `:31`).
- **Default.** Nothing installed, nothing downloaded. Keep a list "missing tool, the question it
  would answer, the milestone that needs it" in the DEC entry.
- **Why.** `CONTINUE-BUILD.md:31` ("No installs, downloads or privilege changes without the owner's
  yes (none given yet)"). TR-10's trigger is a named question that present tools cannot answer
  (`TRIGGER-REGISTER.md:31`); none is named yet.
- **If he says yes to one.** Install it on Z: (for example `Z:\Tools\<name>`), never into the
  project tree; record version, source and install path; answer the named question with the tool's
  output; update TR-10.
- **Ask (only when a named question exists).** "May I install <tool> on Z:\Tools to answer
  <question>?"
- **Effort.** S per tool.

### 3.2 Plan D2: further decisions with defaults

Source for all eight: `CONTINUE-BUILD.md:87`, `STATUS_AND_PLAN.md:675`,
`02-STAGES-AND-STATUS.md:40-48`.

#### OD-05 D2(a): the law-2 wording for phase 2

- **Facts.** Law 2 today: "Cache-resident. Hot loops must not depend on DRAM latency and must never
  touch a pagefile. State lives in a pre-allocated, page-locked arena; inner loops work on L1-sized
  blocks of structure-of-arrays data" (`docs\agents\COMMON.md:19-21`). The owner sequenced it in P21:
  L1/L2/L3 first, then "CPU GPU NPU DDR5 all of it once the foundation is figured out"
  (`05-LAWS-AND-REQUIREMENTS.md:82`; `OWNER-WORDS.md:130-134`).
- **Default.** Phase 1 holds. Milestone 9 (S1 to S4) does not start without his yes
  (`CONTINUE-BUILD.md:61`, `:87`; `STATUS_AND_PLAN.md:692`). Write the proposed wording into
  PROGRESS.md and the DEC entry; change COMMON.md and the ARCHITECTURE law table only after his yes.
- **Why.** The wording widens a law he called "a law" twice (P03, P10). His P21 points the way,
  but the exact wording is his.
- **The proposed wording (put this to him, verbatim, in the interim report):**

  > 2. Cache-first, locked memory. Phase 1 (until the foundation gate): hot loops never wait on DRAM;
  > all state lives in one pre-allocated arena locked in RAM; inner loops work on L1-sized blocks of
  > structure-of-arrays data. Phase 2 (after the gate, one substrate at a time, each only after it
  > passes its row in docs/TRIGGER-REGISTER.md): the locked arena may be a designed streaming tier in
  > DDR5 with prefetch; it may use 2 MB pages if the privilege is granted; the GPU may compute world
  > state through PTX loaded by the CUDA driver, with the state hash equal to the CPU C reference
  > every tick; the NPU may compute world state only if the owner separately accepts a
  > driver-compiled graph under law 1 and its result is bit-exact. In every phase: no pagefile, no
  > virtual or NVMe-backed RAM, no compressed or disk-backed state, no GPU oversubscription; a run
  > whose hot tier exceeds L2 + L3 is labelled DRAM-bound.

- **If he says yes.** Edit `docs\agents\COMMON.md:19-21` and the "Cache-resident" row of the
  ARCHITECTURE law table (`docs\ARCHITECTURE.md:75`) to his approved text; record his words;
  milestone 9 may start once the rest of G1 (OD-13) holds. If he edits the wording, use his edit
  verbatim.
- **If he says no.** Milestone 9 is cut to measurements only (E3 rows labelled DRAM-bound), R1
  reports phase 2 as "not authorised", and the trigger rows TR-01 to TR-08 stay open with that
  reason.
- **Ask.** "May law 2 read as written in DEC-0005, so scaling to DDR5, 2 MB pages and the GPU can
  start after the gate?"
- **Effort.** S (the ask); the work it unlocks is milestone 9.

#### OD-06 D2(b): the NPU path

- **Facts.** NPU code is a graph that the driver compiles at load, not C23, NASM or PTX, the only
  languages law 1 names (`CONTINUE-BUILD.md:61`; `TRIGGER-REGISTER.md:26`, TR-06).
- **Default.** S3 is a probe only, through Level Zero or DirectML (both present): is the integer
  brain step bit-exact over 1,000,000 steps against `az_brain_step_c`? No NPU result enters world
  state. If it is not bit-exact, the NPU may serve only analytics outside the hashed state.
- **Why.** Two separate owner calls are involved: the law-1 reading (is a driver-compiled graph
  "native"?) and law 3 (no LLM: only the creatures' own evolved integer weights may run there,
  `CONTINUE-BUILD.md:23`). The probe answers the technical half without deciding either.
- **If he says yes (after a bit-exact probe).** The NPU stage becomes a twin with an off switch, the
  hash equal to the CPU every tick, and an S4 placement option (TR-07).
- **Ask (after the probe).** "The NPU ran the brain step bit-exact (or not, with numbers): may NPU
  results enter world state?"
- **Effort.** Probe M; twin L.

#### OD-07 D2(c): a quiet-machine window for M2 and E3

- **Facts.** The speeds on record were taken on an idle but not reserved machine (`00-PROMPT.md:63-64`).
  The rule: record the CPU use of other processes during each timed run and label it "lightly
  loaded" or "noisy, parallel load"; "quiet" only inside a window the owner gave
  (`CONTINUE-BUILD.md:40`).
- **Default.** Measure and label. Never call a run "quiet".
- **Why.** Only the owner can promise that nothing else runs (he runs other agents on this machine,
  DL-08).
- **If he gives a window.** Run M2 and E3 inside it, label the rows "quiet (owner window <start>-<end>
  UTC)", and keep the labelled rows from other times beside them.
- **Ask.** "Can you give me a time window when the machine is left alone for benchmarks?"
- **Effort.** S.

#### OD-08 D2(d): sex dimorphism from the sex locus (v09)

- **Facts.** Decided by default in milestone 1: the sex locus stays the sex-determination locus,
  sex is the allele parity of marker locus 0, and it feeds no trait; any dimorphism must come from a
  module that reads `sex` on purpose (`build\work\PROGRESS.md:50`).
- **Default.** Stands. Log it as DEC-0008 with status DEFAULT-IN-FORCE.
- **If he wants dimorphism from the locus.** A module (mate, life or physics) reads `sex` and
  modulates a trait, with a lesion and a test; the marker rule (marker loci feed no trait) does not
  change. Reference hashes change by design; record the new lines.
- **Ask.** Only if a module brief needs it: "Should males and females differ in body through the sex
  locus?"

#### OD-09 D2(e): dropping unused columns (v59, v63)

- **Facts.** Decided in milestone 4: no column is dropped (`docs\agents\ABI-V4-NOTICE.md:55`).
- **Default.** Closed. Log only.

#### OD-10 D2(f): 64 paired seeds for the 500 checklist

- **Facts.** The preregistered reporting blocks are 5001-5024 and 5101-5124: 48 seeds
  (`CONTINUE-BUILD.md:41`). `az_lesion` takes at most 64 seeds. The 500 checklist is about 10^5 world
  runs (`CONTINUE-BUILD.md:57`).
- **Default.** Run on the 48 preregistered seeds and record the deviation from 64 in the
  preregistration file (`docs\prereg\`) before any reporting run.
- **If he wants 64.** Declare a third reporting block of 16 seeds in the preregistration before any
  run touches it (the block ids are the agent's proposal; for example 5201-5216), pin the SHA-256, and
  budget about one third more runs.
- **Ask (at the preregistration).** "The 500 checklist runs on 48 seeds; do you want 64 (about a third
  more run time)?"

#### OD-11 D2(g): deleting `build\dropped-uefi-2026-11-17`

- **Facts.** 26 files, 5,608,524 bytes (counted 2026-11-18 with
  `Get-ChildItem -LiteralPath "<root>\build\dropped-uefi-2026-11-17" -Recurse -File | Measure-Object Length -Sum`).
  The ARCHITECTURE law table says the UEFI host is parked there (`05-LAWS-AND-REQUIREMENTS.md:49`).
  It is under `build\`, so no snapshot holds a copy.
- **Default.** Leave it.
- **Why.** Deletion cannot be undone, and his P19/P35 ban concerns building or booting it, which
  parking does not do.
- **If he says delete.** Grep the docs for `dropped-uefi` and change each mention to "deleted on
  <date> on the owner's word"; then `Remove-Item -LiteralPath "<root>\build\dropped-uefi-2026-11-17" -Recurse`;
  record it.
- **Ask.** "May I delete the parked UEFI folder (5.6 MB, no copy kept)?"

#### OD-12 D2(h): wave 2 in parallel or one module at a time

- **Default.** One module at a time, each landed into `build\z` with its own reference hash, planted
  defects and lesion test (`CONTINUE-BUILD.md:87`, `:52`). A single agent works serially anyway.
- **Closed by default. Log only.**

### 3.3 G1, the foundation gate (OD-13)

- **Facts.** G1 needs: milestones 1 to 8 done, no stubs left, every finding closed or answered, A1
  clean, the invariants of `CONTINUE-BUILD.md` section 3 passing, the M2 table and E3 measured; plus
  the owner's yes on the law-2 wording (`STATUS_AND_PLAN.md:692`).
- **Default.** The agent judges the technical conditions itself, each recorded in PROGRESS.md with
  its run. The owner part is OD-05. Without his yes, stop after milestone 8, write the interim
  report, ask OD-05 and OD-13 there, and continue only with work that phase 1 allows (R1 can be
  written as "phase 2 not authorised").
- **Ask (interim report).** "All gate conditions hold (list attached): may scaling start?"
- **Never** declare G1 passed with a stub left or a confirmed finding open; "green tests" alone do not
  pass it (`CONTINUE-BUILD.md:54`, A1).

### 3.4 Money: the API and credit situation (OD-14)

- **Facts.** The 200 USD Claude API credit is used up: invocation 5 was refused with "Credit balance
  is too low" at 12:02 UTC on 2026-11-18, and a one-cent test at 14:30 UTC was refused too
  (`build\work\PROGRESS.md:1508`, `:1548`). The launcher counted 190.66 USD; Console Billing has the
  real figure (`05-LAWS-AND-REQUIREMENTS.md:179`). The worldgen landing was done afterwards on the
  Claude plan, with no API spend (`05-LAWS-AND-REQUIREMENTS.md:181`). The owner's OpenAI credit resets
  this week (stated in the hand-off request of 2026-11-18). No balance or stop line for it is on
  record. `build\work\scripts\spend.py` reads Claude transcripts only (`CONTINUE-BUILD.md:70`): it
  cannot measure an Astra or Codex session.
- **Default.**
  - Work in steps small enough that a credit stop loses at most one step: every step ends with a
    PROGRESS.md entry and its `Next:` line (`00-PROMPT.md:176-177`).
  - Record per session: start and end time (UTC), the model, and the usage figure if the client
    shows one; otherwise write "cost unknown". Never estimate a cost you cannot measure.
  - On any refusal of a paid call: stop the step, write PROGRESS, write the stop report, do not retry
    in a loop.
  - Never copy anything that looks like a key into a file (`00-PROMPT.md:220`).
  - Do not use the Claude API or launcher (`build\work\api-run\`) from this agent: it is a different
    account and its credit is gone.
- **Why.** No stop line exists, and inventing one would be inventing a number. Small steps make any
  stop safe.
- **If he gives a budget.** Set a stop line at 85 percent of it, as the previous runs did
  (`CONTINUE-BUILD.md:72`), and write it at the top of PROGRESS.md.
- **Ask.** "What OpenAI budget should this build use, and where should I stop?"
- **Laws.** Law 3: the agent's use of a model is development, never engine code; nothing from a model
  enters the simulation.

### 3.5 Speed and the GPU: the tick, the viewer, the demo

These three answer the owner's question of 2026-11-18 (P51, his newest message): "I notice it runs on
PICTURES and TICS. Not Frames Per Second. Why? Can that be fixed?" The plain answer is in DL-10. The
decisions are here.

#### OD-15 The GPU in the tick before the foundation gate

- **Facts.** The GPU whole-tick path is S2, after G1 (`STATUS_AND_PLAN.md:694`; TR-04 and TR-05,
  `TRIGGER-REGISTER.md:24-25`). Today `gpu\` holds Philox and brain kernels equal to the CPU on
  1,000,000 blocks and steps, outside the tick (`TRIGGER-REGISTER.md:24`). Four GPU fixes (v23, v26,
  v57, v25) are still open (`STATUS_AND_PLAN.md:687`). Three modules (sense, physics, stats) are still
  stubs.
- **Default.** No GPU in the tick before G1.
- **Why.**
  - The demo's slowness is the picture path, not the tick (DL-10): at 5,000 to 10,000 creatures the
    engine alone does 150 to 330 ticks per second, while the demo shows 2 to 6 pictures per second. A
    GPU tick would not change the picture path at all. In the owner's own world of 1,000,416 creatures
    the tick is slow too (414.3 ms, about 2.4 ticks per second; `build\work\logs\demo_owner_world_2026-11-18_f226.json`,
    08 section 2.5). That is CPU work for track M3 and the TR items first, still not a reason for a GPU
    tick before G1.
  - A GPU twin must give the CPU hash every tick. While sense, physics and stats are still stubs, the
    CPU tick will change three more times; a GPU twin written now would be rewritten three times.
  - Speed that keeps the hash is allowed now through track M3 (layout, blocking, the scan twin,
    placement; `STATUS_AND_PLAN.md:677`) and TR-14, TR-15, TR-17 (`TRIGGER-REGISTER.md:34-37`).
- **If he says yes.** Fix v23, v26, v57, v25 first; then move stages to the GPU one at a time, each
  with the hash equal to the CPU every tick on both reference configs, exact books, an off switch and
  a measured gain (TR-04's done-when); keep the CPU path as the reference. Record that the milestone
  order changed (DEC entry) and that every module landing after that point needs its GPU twin
  updated too.
- **Ask (only if he repeats that the engine is slow after the viewer fix).** "Do you want the GPU to
  compute the world before the remaining modules and the audit are done?"
- **Laws.** Law 2 (phase 1), law 6 (lab hash equality), law 1 (PTX is allowed).

#### OD-16 The real-time viewer technology

- **Facts.**
  - Today `tools\az_live.c` ticks the world and every `--every` ticks renders a picture with
    `az_view_render`, writes `DIR/frame-NNNNNNNN.png` and prints one JSON line
    (`tools\az_live.c:8-12`). The view is an observer: each line carries the state hash taken before
    and after drawing (`tools\az_live.c:20-24`; rule D7, `docs\ARCHITECTURE.md:192-194`).
  - The browser polls every 250 ms (`demo\live.html:721`); the controls are "Ticks per picture" and
    "Pictures per second (max)" (`demo\live.html:222-223`). Frames and saved worlds live under
    `build\demo\` (`demo\live_server.py:39-41`).
  - Measured (hand-off request, 2026-11-18): 0.1 to 0.5 s per picture (484 ms at 960x540 with about
    11,000 creatures; about 100 ms at 640x360 with about 5,000; measured before worldgen, no log); an
    uncompressed (stored-deflate) PNG of 1.5 MB at 960x540.
  - Measured and logged in the owner's own wild world at 1,000,416 creatures: 4,261.3 ms per 1600x900
    picture plus 27.5 ms PNG, 414.3 ms per tick, and the world stands still while each picture is drawn
    (`tools\az_live.c:527`, `:534`; `build\work\logs\demo_owner_world_2026-11-18_f226.json`; 08 section 2.5).
  - The WinLibs toolchain ships `d3d11.h`, `dxgi1_2.h`, `d3dcompiler.h`, `GL\gl.h` and `GL\glext.h`,
    and Windows has `d3dcompiler_47.dll` and `opengl32.dll` (checked 2026-11-18 by listing the
    toolchain's include folder and `Test-Path` in `C:\Windows\System32`). A native D3D11 or OpenGL
    viewer needs no install.
  - W3 plans "the Windows viewer (a program run from Z: that reads the world only between ticks; you
    open it)" (`STATUS_AND_PLAN.md:687`).
- **Options.**
  - A. WebGL2 in the existing browser demo. `az_live` writes a compact binary record per creature
    between ticks instead of a PNG; `live.html` draws instanced points or billboards at the display
    rate. Keeps the owner's familiar page (http://127.0.0.1:8765/), its controls and its proof tab.
    Cost: one hop through `demo\live_server.py` (Python's HTTP server) per frame.
  - B. A native Windows viewer in C with D3D11 (or OpenGL), a separate program on Z: that reads a
    shared, read-only copy of the between-tick state. No browser or Python hop; scales further.
  - C. Both: A first (it fixes the complaint soonest), B as the W3 deliverable.
- **Default.** C: A first, then B.
- **Why.**
  - A is the smallest change that fixes what the owner sees: the expensive steps (a single-threaded
    software render of every body, a 1.5 MB PNG, a disk write, a 250 ms poll and a PNG decode) all
    disappear, and the data per frame shrinks to a few bytes per creature (at the 24 bytes per
    creature of 08 RV-A1's record, about 264 KB at 11,000 creatures against 1.5 MB per PNG, and about
    24 MB at 1,000,416 creatures; corrected 2026-11-18 from an earlier "16 bytes"; 08 is canonical for
    the layout).
  - B matches W3 and the owner's taste for native code (P10, `PROMPTS.md:442`: "creatures in slow
    languages are not allowed ... intelligence and speed"). It goes second because it is a new
    program, a larger change.
  - Neither touches the simulation, so neither can break a law of the engine (DL-11).
- **What stays in both.** The engine's own renderer (`view\az_view.c`) stays as the "proof picture":
  a button or command that renders one picture with the hash check, exactly as today. The live view
  shows two separate numbers, "ticks per second" (the world) and "frames per second" (the display),
  and never calls one the other.
- **If he picks A only.** Skip B; record that the W3 viewer item is met by the browser viewer.
- **If he picks B only.** Skip A; the browser demo keeps its PNG path as the proof page; B becomes the
  live view.
- **Ask.** "For the live view: fast browser view first and a native window later, or only one of
  them?"
- **Laws.** D7 (the viewer only reads between ticks); law 1 applies to creature and world code, not
  to the viewer (DL-11).
- **Effort.** A: M. B: L.

#### OD-17 May the demo use the GPU

- **Facts.** "A result that is not bit-exact may feed only outputs outside the hashed state (viewer,
  reports, logs)" (`CONTINUE-BUILD.md:29`; `TRIGGER-REGISTER.md:10-12`). GPU oversubscription is
  banned in every phase (`CONTINUE-BUILD.md:30`).
- **Default.** Yes. The viewer may render on the GPU (WebGL2 through the browser, or D3D11 or OpenGL
  in the native viewer). Not asked: this is a law reading, logged as DEC-0017 (kind LAW-READING).
- **Conditions.** The viewer never writes to the world; it reads a copy taken between ticks. Its
  buffers fit in device memory with no fallback to shared system memory (the oversubscription ban).
  Display-only interpolation between two ticks is allowed and is labelled "interpolated" with a switch
  to turn it off. The state hash before and after the copy is printed, as today.
- **If he objects.** Fall back to the CPU: the native viewer draws with GDI or a software rasteriser
  into a window; still no PNG and no disk.

### 3.6 Machine and files

#### OD-18 The C: toolchain

- **Facts.** gcc 16.1 (WinLibs, UCRT, posix), NASM 2.16.03, CMake 4.3.3, Ninja, Python 3.9 and
  PowerShell 7.6 live on C:; clang 21.1.8 lives at `Z:\Tools\LLVM-21` (hand-off request,
  2026-11-18). The rule "Nothing on C:" concerns builds, runs, logs and snapshots
  (`CONTINUE-BUILD.md:24`), and the hand-off says of C: "the toolchain lives there; leave it alone"
  (`00-PROMPT.md:132`).
- **Default.** Leave the toolchain where it is. Everything the project produces stays on Z:.
- **Why.** Moving it means reinstalling (OD-04 applies), and a different compiler build could change
  code generation; the reference hashes must then be re-checked.
- **If he says move it.** Install the same versions under `Z:\Tools`; configure a new build folder
  from the real Z: path (never reuse `build\z`); check 0 warnings, the full ctest, and the three
  reference hashes (`tests\golden_hashes.txt:69-71`) unchanged; record the new paths in
  `03-DEVOPS-PIPELINE.md` style in PROGRESS and the DEC entry.
- **Ask.** "Should the compilers move from C: to Z:\Tools (a reinstall)?"
- **Effort if yes.** M.

#### OD-19 Old world snapshots

- **Facts.** World files saved before the worldgen landing no longer load
  (`00-PROMPT.md:53-54`). The loader refuses a file whose ABI version or format differs
  (`core\az_snapshot.c:543`) and a parameter outside its range or a non-zero unused slot
  (`core\az_snapshot.c:433-435`). The demo saves to `build\demo\worlds\*.azsnap`
  (`demo\live_server.py:41`, `:645`). Recovery copies of the source tree in `build\work\snapshots\`
  are a different thing and are not affected.
- **Default.** Keep the old files; do not convert them. Check that the refusal names its reason
  (DD-T08).
- **Why.** A converted world would be a world that never existed: it would carry the new
  parameters' defaults but none of the terrain, ocean and founders that worldgen makes. Calling it
  the old world would be invented evidence (law 8).
- **If he wants an old world back.** Write `tools\az_snapconv.c` that fills new parameter slots with
  their defaults, label every converted file "converted <date>, not the original world", and keep the
  original file.
- **Ask (only if he asks for an old world).** "That world was saved before the terrain existed; do
  you want a labelled conversion?"

### 3.7 Biology choices the trigger register gives the owner

#### OD-20 Creatures that do not think every tick (TR-16)

- **Facts.** TR-16: a heritable thinking period, larger brains slower; "it changes behaviour, so it
  is a design decision, not an optimisation"; owner decision column: "The owner decides: it is
  biology" (`TRIGGER-REGISTER.md:36`).
- **Default.** Not built.
- **If he asks for it.** Build it as a gene with a lesion and a test, through the existing
  `az_phase_due`; measure ms per tick before and after; reference hashes change by design.
- **Ask (only if he asks for speed at 1,000,000).** "Should creatures with bigger brains think less
  often (it is biology and changes behaviour)?"

#### OD-21 Replaying a wild run from the entropy journal

- **Facts.** The journal (`az_run --entropy-journal FILE`, 24 bytes per tick) "is a record for
  looking into a surprising event afterwards. Nothing reads it back into a run"
  (`ABI-V4-NOTICE.md:391-393`); the owner's rule as recorded: an optional journal "is never fed back"
  (`05-LAWS-AND-REQUIREMENTS.md:110`).
- **Default.** No replay tool. A surprising wild event is studied by reproducing the phenomenon in lab
  mode on tuning seeds 1-99, never by replaying the wild run.
- **If he asks.** A replay tool would be a debugging instrument outside the living world, labelled
  so; it needs his explicit yes because it contradicts the recorded rule.

#### OD-22 Worldgen's fixed structure

- **Facts.** "The brief's structure stays fixed: ocean always under air, gravity always -z, the
  guild-to-band map" (`build\work\PROGRESS.md:1609`); "no owner decision exists yet"
  (`02-STAGES-AND-STATUS.md:55`).
- **Default.** His words P39 to P42 already decide the direction ("Nothing in the US is FIXED FIXED",
  `PROMPTS.md:869`): convert the biology parts to parameters after physics lands, keep the physics.
  The reasoning and the split are in DL-02; the task is DD-T07. Not asked.

### 3.8 Rules about irreversible acts

#### OD-23 Push, publication, public claims

Never push (there is no remote, and `CONTINUE-BUILD.md:86` says never pushed). Never publish a
paper, a page or a release. Never claim novelty without a wide prior-art search stated with its scope
and date (`CONTINUE-BUILD.md:27`). None of these is asked unprompted.

#### OD-24 Spending on anything other than tokens

No purchases, subscriptions or paid services. Never asked unprompted.

### 3.9 The owner's "more things I forgot": the idea-bank catalogs (OD-25)

#### OD-25 Trace only, or score a selection of the idea banks

Added 2026-11-18 by the fixer pass: 07 section 6.15 classed these catalogs as trace-only without an
owner decision on record.

- **Facts.** 07 section 6.15 lists `bench_freak100.json` (100 rows), `bench_novel100.json` (100),
  `bench_foundations.json` (99), `cube1000.json` (1,000), `wishlist.json` (512),
  `nested_precision.json` (134), `frontier_upgrade_map.json` (272) and `thousand_reports.json` with its
  index (150 and 900): 3,267 rows, all in `docs\requirements\`. The plan says "trace the idea banks
  without scoring them" (`CONTINUE-BUILD.md:57`). The owner's P06 names "100 benchmarks ... and more
  things I forgot" (`PROMPTS.md:301`).
- **Options.** A. Trace only: every row gets reason code IDEA-BANK (with `implemented-as` notes where a
  mechanism exists); nothing is scored. B. He picks items (by catalog or by id) to score; each picked
  item gets a spec in SB-P1, a preregistration and reporting runs like any TESTED item. C. Score every
  row: not feasible (each needs a measurable spec and runs; 07 section 8's run counts already reach
  about 10^5 world runs for the 500 alone).
- **Default.** A, trace only, until he answers.
- **Why.** It follows the plan, it keeps the M8 run budget, and tracing every row still shows him that
  nothing he wrote was dropped.
- **Ask.** "Your idea-bank lists, 3,267 rows: trace them only, or pick some for me to score as well?"
  (second report, `00-MASTER-PROMPT.md` 7.4).
- **Laws.** Every word of the owner counts (law 12): tracing every row is the floor. No aggregate score
  (law 8): a picked item gets a verdict, never a total.
- **Blocks.** Nothing; a "pick" adds SB-P1 tuning and SB-P2 runs for the picked items.
- **Effort.** A: inside SB-10. B: S to L per picked item.

---

## 4. The dilemmas, each with a reasoned resolution

Each dilemma: the two sides with their sources, the resolution, how to apply it, and how to tell if
it is broken.

### DL-01 Wild (non-deterministic) versus reproducible science

- **Side A.** The owner: "NON DETERMINISTIC, WILD, FERRAL, CHAOTIC, IMPROVIZED" (`PROMPTS.md:869`);
  "nothing in the universe is like FIXED FIXED" (`PROMPTS.md:861`). A Claude session once made the
  living world deterministic and was corrected; "that is not to happen again"
  (`OWNER-WORDS.md:138-143`).
- **Side B.** Science needs repeatable contrasts: lesion A/B pairs, reference hashes, determinism
  matrices, preregistered statistics (`CONTINUE-BUILD.md:41-44`, `:55-58`).
- **Resolution.** Two modes, two jobs, never mixed (`ABI-V4-NOTICE.md:376-393`):
  - **Wild** is the creature's world and the default of `az_run`, `az_snap` and `az_live`: fresh
    operating-system entropy every tick, no seed, never twice the same. Books exact every tick.
  - **Lab** is the test bench: the suite, `az_lesion`, `az_bench`, the reference hashes, every
    milestone-8 suite run. A lab world never asks for entropy.
  - **The journal** is a record, never an input (OD-21).
  - **Claims.** A claim about the wild world is a claim about a distribution: run N wild worlds
    (N preregistered), report the distribution of the measure with its interval, and never compare
    two single wild runs as if one were a control. A causal claim (a lesion changes an outcome) is
    made in lab mode, where the pairs share a seed, and then checked for direction in wild runs.
  - **Exactness is not determinism.** Matter and energy balance exactly in both modes; that is law 5
    and holds in wild runs too.
- **How to apply.** Every evidence table carries a "mode" column (lab or wild). Never pass `--lab` to
  a demo default. Never add a seed option to a wild path (`az_run` refuses `--seed` without `--lab`;
  keep it so).
- **Broken when.** A living-run default becomes seeded; a wild result is cited as a reference hash; a
  lab lesion contrast uses unpaired seeds.
- **Documentation lag.** `COMMON.md:27-28` still says "Deterministic ... Same seed gives the same state
  hash" with no mention of wild mode (`05-LAWS-AND-REQUIREMENTS.md:37`). The owner already decided
  this (P41, P42); the rewording is a task, not a question (DD-T03).

### DL-02 "Nothing fixed" versus the brief's fixed structures

- **Side A.** P39 "Life isn't fixed or fixed sizes or fixed anything" (`PROMPTS.md:829`); P40 "Yes,
  nothing fixed" (`:853`); P13 "this isn't EARTH ... it's an alien weird crazy wild environment"
  (`:460`). The reading in force: only the physics is fixed; every other number is a gene or a world
  parameter; running out is a recorded refusal, never a crash (`OWNER-WORDS.md:51`;
  `00-PROMPT.md:85-88`).
- **Side B.** Five structures are fixed today:
  1. **Ocean under air.** Bands FLOOR, PHOTIC, MID, AIR by depth
     (`build\work\w2\worldgen\DESIGN.md:464-480`).
  2. **Gravity always -z.** The vertical pull is clamped to [-127, -1]: "down is -z and never ..."
     (`DESIGN.md:352`); the lateral bend and the strength are parameters (`DESIGN.md:346-350`).
  3. **The guild-to-band map.** PRODUCER to PHOTIC, CHEMOTROPH to FLOOR at vents, and so on
     (`DESIGN.md:489-498`); which species gets which guild already rotates per world.
  4. **The 100-species cap.** `cfg.n_species` "1..100" (`core\az_abi.h:1850`), and
     `core\az_worldgen.c:897` clamps a larger value to `WG_MAX_SPECIES` **silently**. The tools
     disagree on the allowed range: `az_run`, `az_snap` and `az_live` accept 1 to 65535
     (`tools\az_run.c:160`, `tools\az_snap.c:104`, `tools\az_live.c:303`), `az_bench` 1 to 512
     (`tools\az_bench.c:401`, "512 = default clade limit"), `az_lesion` 1 to 1000
     (`tools\az_lesion.c:215`). The number 100 is not in the owner's prompts (a search of
     `PROMPTS.md` for "species" finds nothing); it comes from the plan text
     (`CONTINUE-BUILD.md:52`).
  5. **The 256-slot parameter table.** `u32 param[256]` (`core\az_abi.h:1020`). 233 slots are used
     (count: `AZ_PARAM(` lines in `core\az_params.def` (11) and `core\params\*.def` (222), counted
     2026-11-18 with `Select-String`); 23 are free. The three stubs have 0 parameters each
     (`core\params\az_sense.def`, `az_physics.def`, `az_stats.def`); the landed modules use 6 (mate)
     to 46 (worldgen).
- **Resolution.** Sort each structure by what it is:
  - **Physics stays fixed** (the owner allows that): denser fluid lies below lighter fluid; "down" is
    a frame convention. So:
    - Gravity -z stays as a convention, documented as such: a world with the pull reversed is the
      same world mirrored, with no new biology. Its strength and bend stay parameters.
    - Ocean under air stays until physics has densities; then the stacking order is computed from
      the two media's density parameters instead of being written into the layout, with defaults that
      reproduce today's worlds (DD-T07).
  - **Biology and ecology become parameters or genes**: the guild-to-band map becomes a per-world
    drawn preference with weights in `core\params\az_worldgen.def` (DD-T07). Founders only start
    there; evolution moves them once sense and physics are real.
  - **Bounds become run parameters with recorded refusals**: the species bound gets one value shared
    by every tool, and a request above it is refused with a recorded reason or lifted, never clamped
    silently (DD-T04). The parameter table gets room before sense, physics and stats need it
    (DD-T05).
- **Why this order.** Turning the biology parts into parameters before physics honours rock and
  density would let founders be seeded into habitats they cannot live in; the physics stub carries
  bodies through bedrock (`00-PROMPT.md:44-46`). So physics first, then DD-T07.
- **Broken when.** A new module adds a buried constant of biology; a refusal happens without a
  counter or ledger event; a tool accepts a value the core clamps.

### DL-03 Indicator-only honesty versus the owner's wish for consciousness, life and AGI

- **Side A.** The owner wants the consciousness lists met ("the 14 butlin and 10 consciousness and
  25 CQM ...", P06, `PROMPTS.md:301`) and wants to "create LIFE" (P11, `PROMPTS.md:448`); "SOTA
  FRONTIER SI" (P35, `PROMPTS.md:739`).
- **Side B.** Law 7: never claim consciousness, sentience, life, AGI, quantum randomness or a quantum
  speed-up (`COMMON.md:29-31`; `00-PROMPT.md:89-90`). Butlin 14, the 10 theories and CQM axes 13 and
  19-22 are indicator-only scorecards (`CONTINUE-BUILD.md:58`).
- **Resolution.** Build what the indicators probe; score it honestly; never claim what it would mean.
  - Every indicator item gets a mechanism in the engine (or a reason code), a lesion that removes it,
    a preregistered measure, and a verdict EFFECT, NO EFFECT, INCONCLUSIVE or a reason code
    (`CONTINUE-BUILD.md:57-58`).
  - Report wording, always: "indicator <id> met under <measure> with |d_z| = x, Holm p = y, on seeds
    <block>". Never "conscious", "sentient", "alive", "AGI", "understands", "feels". The same holds in
    the viewer's labels and in the final report.
  - "Life" in the owner's words is honoured as the ALife research sense: self-maintenance, heredity,
    variation, selection, open-ended change, each measured. Say "life-like process X measured" and
    name X.
  - The quantum-derived kernels (`tk_qsv`, `tk_qgt`) are classical integer simulations of small
    state vectors and give no speed-up (`STATUS_AND_PLAN.md:706`).
- **How to tell the owner.** In plain words, once per report that touches the suites: "These lists
  test whether the mechanisms the theories point to are present and causal. Passing one is evidence of
  that mechanism, not of a mind."
- **Broken when.** Any document or label uses one of the banned words as a property of the engine;
  an indicator passes without a lesion contrast.

### DL-04 Speed versus exactness

- **Side A.** "intelligence and speed" (P10, `PROMPTS.md:442`); the owner says it is slow (2026-11-18).
  The ABI-4 "nothing fixed" layout costs about 1.3 to 1.9 times the tick of the first freeze
  (`OWNER-WORDS.md:172-173`); 100,000 creatures went from 19.24 ms per tick at the ABI-4 freeze to
  73.7 ms on 8 workers after the biology and worldgen (hand-off request; `PROGRESS.md` worldgen entry).
- **Side B.** Integer books every tick (`==`, never a tolerance); no float in the core; every SIMD,
  GPU or NPU twin bit-exact against the C reference (`CONTINUE-BUILD.md:25`, `:29`, `:43`).
- **Resolution.** Exactness is never traded. Speed is bought only in ways that keep the hash:
  - layout and blocking (M3, `STATUS_AND_PLAN.md:677`); packing the store in place (TR-14); shared
    brains with copy on write (TR-15); a parallel birth commit (TR-17)
    (`TRIGGER-REGISTER.md:34-37`);
  - twins (AVX2, AVX-VNNI, the scan twin of W3), each proven equal on the determinism matrices;
  - placement (P and E cores; later the GPU after G1).
  - A speed-up that changes behaviour (TR-16, thinking less often) is biology and needs the owner
    (OD-20).
  - The viewer is outside the books: it may use float and the GPU freely (OD-17). Most of what the
    owner calls slow is there (DL-10).
- **How to apply.** Every speed change records ms per tick before and after on the same machine
  load label, and the three reference hashes unchanged (`00-PROMPT.md:168-170`).
- **Broken when.** A tolerance appears in a conservation check; a twin is installed without its
  bit-exact test; a hash changes in a "speed only" change.

### DL-05 The owner's massive scale versus the cache law

- **Side A.** "grow to millions of entities ... colonies and swarms and fleets in a massive sim"
  (P06, `PROMPTS.md:301-302`).
- **Side B.** At 140 bytes of hot tier per creature, more than about 540,000 creatures cannot fit in
  L2 + L3 (76 MB) (`build\work\PROGRESS.md:29`; `STATUS_AND_PLAN.md:702`). That count is too kind: each
  creature's own brain (about 1.9 KB) is read every tick, so about 2.2 KB per creature is touched and the
  knee is near 35,000 creatures by the same arithmetic (08 section 2.4, an estimate; ES-1 measures it).
  Both reference worlds (100k and 1M) are beyond the caches by this estimate. The 1,000,000 world takes
  1339 ms per tick on 8 workers (PROGRESS worldgen entry). The previous agent's analysis found that
  the 1M world recorded 380,865 store refusals and reached only generation 1 (hand-off request,
  2026-11-18): scale, not evolution.
- **Resolution.**
  - Runs above the cache budget are allowed as tests and measurements and are always labelled
    "DRAM-bound" (`CONTINUE-BUILD.md:29`). Never "cache-resident". Until M2 measures the knee, every run
    above about 35,000 creatures is labelled "beyond L2+L3 by estimate (DRAM-bound)"
    (`00-MASTER-PROMPT.md` 5.10 row 15).
  - Report scale and evolution as two different numbers: population and ms per tick on one side;
    generations reached, births, deaths and refusals on the other. A "massive" claim needs both.
  - The designed path to more creatures is S1 (a DDR5 streaming tier with prefetch) and TR-02
    (populations above 1M, the largest that fits locked), both after G1 and OD-05.
  - Store refusals are recorded, never a crash (law 7); a run with many refusals says so in its
    first line of results.
- **Broken when.** A 1M result is reported without the label, or without its generation count.

### DL-06 Breadth (hundreds of checklist items) versus depth (a few decisive demonstrations)

- **Side A.** The owner's numbers: 10, 14, 25, 100, 100, 100 (109 rows), 500, plus 13 gates, 5
  mutants and 333 laws and errata (`00-PROMPT.md:183-196`). The previous agent's crosswalk holds 958
  records behind the requested lists (hand-off request). The 500 checklist alone is about 10^5 world
  runs (`CONTINUE-BUILD.md:57`).
- **Side B.** No world-level result yet shows a trait being selected (`00-PROMPT.md:60-61`). The
  previous agent named the decisive next demonstration: a known food gradient, then a body-frame
  sensory change, then motion through the actual medium, then paid intake, then a reproductive
  outcome (hand-off request). Today the senses stub leaves directional inputs neutral and the physics
  stub carries bodies through bedrock, so that chain cannot run.
- **Resolution.** Depth first, then breadth, and breadth never at the cost of honesty.
  1. Land sense and physics (milestone 5), then run the decisive chain as one preregistered lab
     experiment with lesions at each link (SENSES, the physics medium, intake, MATING). This is the
     first piece of evidence that the engine does what the lists ask for.
  2. Then the suites in the order `CONTINUE-BUILD.md:57` gives: engineering gates and the 5 mutants;
     E1, then E3; CQM-25, then Butlin 14; the 10 theories; the 100 passes; the 500 checklist in the
     background; rollups last.
  3. Every item gets a verdict or a reason code (NOT-BUILT, N/A-ENGINE, LAW-CONFLICT, IDEA-BANK,
     OUTSIDE-ENGINE, DUPLICATE-OF, SUPERSEDED-BY, NEEDS-OWNER, INDICATOR-ONLY;
     `CONTINUE-BUILD.md:57`). A reason code is an honest answer, not a failure to hide.
- **Broken when.** An item is marked PASS without a run id; the 500 checklist starts before the
  decisive chain has a verdict; an under-powered item is reported FAIL instead of INCONCLUSIVE.

### DL-07 Autonomy (never block) versus the owner's right to decide

- **Side A.** "I dont want to prompt anymore ... it's all you now" (P37, `PROMPTS.md:813`); "Figure it
  out. Get after it." (P35, `PROMPTS.md:739`).
- **Side B.** Some decisions are his alone (sections 2 and 3), and he reacted strongly when a session
  overrode his words ("Stop ignoring my words", P42, `PROMPTS.md:869`).
- **Resolution.**
  - Reversible choices inside the laws: decide, log (DEC), go on.
  - Owner choices: take the recorded default, log it, ask one short question in the report, go on
    with work that does not depend on the answer.
  - Irreversible acts: never by default (section 1, rule 5).
  - His words versus a document: his words win; a document that contradicts a newer prompt is
    corrected (as DD-T03 does for law 6).
  - When an answer arrives, apply it at the next step boundary, record it, and re-check every DEC
    entry that depended on the default.
- **Broken when.** Work stops waiting for a reply; a default is taken without a DEC entry; an
  irreversible act happens without his words in the DEC entry.

### DL-08 Codex and Claude sessions in parallel on one tree

- **Facts.** "Codex may be working on the same folders with the same instructions. Before a long
  rewrite and every 15-20 minutes, look for codex or ChatGPT processes and fresh file times under
  `<root>`; adopt what it already delivered and add only new files" (`CONTINUE-BUILD.md:35`). Nothing
  is committed, so `git diff` cannot show who changed what. It has happened: the recovery copy
  `build\work\snapshots\2026-11-18-pre-sense-astra` (517 files, 28.9 MB, 18:25:32 UTC on 2026-11-18)
  was taken by a GPT Astra session in the Codex desktop app, which then ran a 100k baseline
  (`build\work\logs\sense_baseline_100k.txt`: final hash 73f1e9e013f8db50, matter and energy exact) and
  ctest (`build\work\logs\sense_baseline_ctest.txt`: 46 of 46 passed in 238.52 s), working from
  `C:\Users\Alexa\Documents\Codex\2026-11-18\the-bundle-is-clean-no-key` and ending about 18:41 UTC. It
  wrote no sense code: no `core\az_sense.c`, no `build\work\w2\sense\` (checked 2026-11-18). The
  snapshot is a recovery point and the logs a reproduced baseline, not progress.
- **Resolution.** One writer at a time on the source tree, enforced by a lock file and checked by
  file times (DD-T02):
  - At session start: check processes, the lock, and file times; write the lock only if no other
    live writer holds it.
  - While holding it: re-check every 15 to 20 minutes and before any multi-file change.
  - If another writer is active: do read-only work (reviews, reading, planning in your own
    `build\work\<group>\<key>\` folder) and say so in the report. Never edit a file the other session
    touched in the last hour.
  - If the other session already delivered the same piece: adopt it, review it, add only new files.
  - Two "canonical" versions of one module are worse than one: never land a second sense module next
    to another session's.
- **Broken when.** Two sessions write under `core\`, `tests\` or `tools\` within the same hour; a
  snapshot is missing before a change.

### DL-09 The old TypeScript lab results versus native evidence

- **Facts.** "No v40 result carries over" (`CONTINUE-BUILD.md:55`); "every PASS figure in the harvest
  is from the old TypeScript lab" (`00-PROMPT.md:202-203`). The previous agent found the original Grok
  Bot build and the Windows v40 export disagree on Combo A foraging over the same 112 seed ids
  (d = 0.733 against 0.857, threshold 0.8; cause unresolved), and the v40 R21 A-arm scored 9 of 14
  Butlin, 6 of 10 theories and 17 of 25 CQM; native has no held-out score yet (hand-off request).
- **Resolution.**
  - Keep three provenances apart in every table: `native-lab`, `native-wild`, `v40-ts` (and
    `grokbot` where it differs from v40). Never sum, average or compare across provenances in one
    verdict.
  - Old results may be cited only as "prior lab, different engine" context, never as evidence that
    the native engine meets an item.
  - The Combo A disagreement stays recorded as unresolved, with both numbers, until someone reproduces
    both builds; it is not the native engine's problem to solve.
  - The previous agent's outputs sit outside Z: (`C:\Users\Alexa\Documents\Codex\2026-11-18\the-bundle-is-clean-no-key\outputs\`;
    written on 2026-11-18 by the GPT Astra sessions in the Codex desktop app); copy them into
    `build\work\eval\inputs\astra-2026-11-18\` (07 SB-01, the one destination) before citing them, and
    cite the copy (Z: only).
- **Broken when.** A native scorecard shows a v40 number in a native column; a "PASS" has no native
  run id.

### DL-10 The picture pipeline: correctness-first view versus real-time frames

This is the owner's question of 2026-11-18 (P51). The answer, in his terms:

- **Why it shows pictures and ticks.** The world moves in ticks: one tick is one step of the whole
  simulation (every creature senses, thinks, moves, eats, mates; matter and energy are balanced).
  The demo was built to prove the world is real, not to be fast: every few ticks the engine draws a
  full picture itself, on one CPU thread, every body drawn; writes it as an uncompressed 1.5 MB PNG
  file; checks that drawing did not change the world (the state hash before and after); then the
  Python server hands the file to the browser, which asks every 250 ms and decodes it
  (`tools\az_live.c:8-24`; `demo\live.html:721`). Each picture takes 0.1 to 0.5 s, so the page shows
  2 to 6 pictures per second, while the engine alone does 150 to 330 ticks per second at 5,000 to
  10,000 creatures. In a small world the slow part is the picture path, not the world. In his own
  world of 1,000,416 creatures both are slow: one picture takes 4.3 s, during which the world stands
  still, and the world steps about 2.4 times per second (08 section 2.5).
- **Can it be fixed.** Yes, without touching the simulation. Send the positions, not pictures: a few
  bytes per creature between ticks, drawn by the graphics card at the screen's rate (OD-16, OD-17).
  The world then runs as fast as it can, and the screen shows smooth frames.
- **The dilemma.** The picture is the engine's own view and is hash-checked; a GPU view is a second
  renderer that could show something the engine does not have.
- **Resolution.**
  - The live view draws only what it is given: one record per creature copied between ticks. It
    invents nothing; display-only interpolation is labelled and can be switched off.
  - The proof picture stays: one click renders `az_view_render`'s picture with the hash check, as
    today.
  - Two counters, never mixed: ticks per second (world) and frames per second (screen).
  - At 100,000 creatures the world does about 13.6 ticks per second on 8 workers (hand-off request),
    so frames will be smooth but the creatures will move 13.6 steps per second there. Say so on the
    page; that is the world's real speed.
  - At his own 1,000,416 creatures the world does about 2.4 ticks per second: the camera stays smooth
    at 60 frames per second, the creatures get about 2.4 computed positions per second, and the stream
    may stop the world for at most 20 ms per snapshot (08 RV-A9 target case 2), not 4.3 s as a picture
    does today.
- **Broken when.** The live view changes the state hash; a frame counter is labelled "ticks"; the
  viewer reads during a tick (D7).

### DL-11 Law 1 (native only) versus non-C artefacts around the engine

- **Facts.** Law 1: "No VM, JIT or interpreted language inside the simulation ... TypeScript,
  JavaScript, Python ... allowed only as development tooling, never as creature or world code"
  (`COMMON.md:16-18`). Today Python and JavaScript already run the demo (`demo\live_server.py`,
  `demo\live.html`). A WebGL2 viewer adds GLSL shaders compiled by the browser; a D3D11 viewer adds
  HLSL compiled by `d3dcompiler_47.dll`; the NPU path would add a driver-compiled graph.
- **Resolution.**
  - Law 1 governs creature and world code: everything that changes the hashed state. The viewer,
    the server, the reports and the tooling are outside it, as the law's own text says.
  - Shaders and browser code may exist only in the viewer and may never compute anything that feeds
    back into the world.
  - The NPU graph is different because it would compute world state: that is why OD-06 needs the
    owner.
- **Broken when.** Any non-C, non-NASM, non-PTX code computes a value that enters
  `az_state_hash`.

### DL-12 "SOTA FRONTIER" versus "no installs, no downloads"

- **Side A.** "keep building it but with the SOTA FRONTIER SI. Figure it out." (P35, `PROMPTS.md:739`).
- **Side B.** No installs or downloads without his yes; web queries generic only
  (`CONTINUE-BUILD.md:31`).
- **Resolution.** State of the art enters as ideas, not packages: read papers and documentation
  through generic web queries (never his name, the project's name or paths), cite them, implement the
  idea by hand in C23 or NASM, and measure it against the C reference (`00-PROMPT.md:218-219`). A
  library that cannot be reimplemented goes into an OD-04 request with the question it answers.
- **Broken when.** A file from the web lands in the tree; a query names the project.

### DL-13 Lesions with NO EFFECT versus "no theatre"

- **Facts.** The exploratory table (`az_lesion --seeds 1-24`, 3000 founders, 64 ticks): 7 EFFECT, 14
  NO EFFECT, 0 INERT; NO EFFECT includes WEATHER, CHEM, GRAVITY (which changes the state on 24 of 24
  seeds), TIER1_Q, TIER1_SH, MEMORY, METRIC, SENSES, NOISE, RECURRENCE, PLASTICITY, CARE, COLONY,
  SIGNALS (hand-off request; `build\work\PROGRESS.md` worldgen entry). The Tsotchke-derived kernels run
  once every 32 ticks per creature (hand-off request), so a 64-tick run gives each creature about two
  updates. lint `--strict` fails with three findings, among them GRAVITY not honoured by physics and
  NOISE, RECURRENCE, SENSES untested (hand-off request).
- **Side A.** Law 4: every mechanism has a measurable causal effect (`COMMON.md:24-25`).
- **Side B.** A 64-tick exploratory run is short, and two modules that the switches act through
  (sense, physics) are still stubs.
- **Resolution.**
  - INERT (the switch changes nothing in the state) on the complete build is a defect
    (`CONTINUE-BUILD.md:56`).
  - NO EFFECT on a short exploratory run is not a verdict. For E1, the preregistration sets, per
    lesion and before any reporting run, the run length and the measure on which the mechanism should
    act (for a 32-tick kernel, at least enough ticks for many updates per creature), with the reason.
  - NO EFFECT at the preregistered length on reporting seeds is reported as NO EFFECT. Then the
    mechanism is repaired, or the claim that it matters is withdrawn. Never search measures after the
    data until one moves (that burns the block, `CONTINUE-BUILD.md:55`).
  - Re-run the exploratory table after sense and after physics land; SENSES and GRAVITY are expected
    to change then, and that expectation is written down before the run.
- **Broken when.** A NO EFFECT row is cited as proof of no theatre; a measure is chosen after seeing a
  reporting run.

### DL-14 "Every word counts" versus a coherent, finite plan

- **Side A.** "EVERYTHING IN THE PROMPT!!!" (P42, `PROMPTS.md:869`); "think of ways 100 times beyond
  this" (P11, `PROMPTS.md:448`).
- **Side B.** One agent, a limited budget (OD-14), ten milestones.
- **Resolution.** Every word gets a row in `docs\requirements\OWNER-WORDS.md` with where it lives and
  its status (built, waiting on a module, later, or a reason) (`OWNER-WORDS.md:23-53`). Nothing is
  dropped or softened; what is not built yet says "not built yet" and names the milestone. Before any
  design choice, re-read that file (`00-PROMPT.md:99-101`). Breadth of ideas goes into the idea banks,
  traced but not scored (`CONTINUE-BUILD.md:57`).
- **Broken when.** A word of his has no row; a row says "built" for something a stub still does.

---

## 5. Tasks that come out of this file

Common to every task: snapshot first
(`python -I "Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\work\scripts\snapshot.py" <label>`);
check the writer lock (DD-T02); afterwards, the text check
(`python -I "Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\work\scripts\check_text.py" "Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native" --since 60`,
exit 0) and a PROGRESS.md entry. Code tasks also need, from `00-PROMPT.md:162-178`:
- `cmake --build "Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\z"` with 0 warnings;
- `ctest --test-dir "Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\z" --output-on-failure -j 6`,
  everything passing, started in the background with a log on Z: (a call blocked over 4 minutes is
  forbidden, `CONTINUE-BUILD.md:77`);
- the 100k reference world
  `& "Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\z\az_run.exe" --lab --n 100000 --ticks 60 --cap 400000 --grid 6,6,5`,
  final hash `73f1e9e013f8db50` unless the task changes it by design (then a new line in
  `tests\golden_hashes.txt` with the command, the change and the reason).

### DD-T01 Create the decision log

- **Inputs.** This file (sections 3, 4 and 6); `CONTINUE-BUILD.md:84-88`; `build\work\PROGRESS.md`
  (defaults already taken: `:50` sex locus; `:1569-1584` worldgen decisions).
- **Files.** Create `docs\eval\DECISIONS.md` (the folder `docs\eval\` does not exist yet; create it).
- **Steps.** Write the header and rule (section 6.1), the index table, the "Open questions" table,
  and one entry per row of section 6.4 (DEC-0001 to DEC-0040), each with every field of section 6.2.
  Copy the worldgen lead decisions (`PROGRESS.md:1569-1578`) as DEC-0041 onward with kind AGENT and
  status DEFAULT-IN-FORCE.
- **Commands.** The text check above. A field count:
  `(Select-String -LiteralPath "Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\docs\eval\DECISIONS.md" -Pattern '^### DEC-').Count`
  must equal the number of rows of the index table.
- **DoD (can fail).** Every entry has all 15 fields of section 6.2; the index row count equals the
  entry count; every "Ask" text in "Open questions" is one sentence of at most 25 words; check_text
  exit 0.
- **Evidence.** PROGRESS entry with both command tails.
- **Depends on.** Nothing. **Risk.** Drift between this file and the log: after DD-T01 the log is the
  live record and this file is history; say so in the log's header. **Effort.** S.

### DD-T02 One writer at a time: the lock file

- **Inputs.** `CONTINUE-BUILD.md:35`; DL-08.
- **Files.** `build\work\WRITER.lock` (plain ASCII text, written and removed by each session).
- **Format (one field per line).**
  ```
  holder: <tool and model, for example Codex CLI / GPT Astra 6.2>
  pid: <process id of the agent's shell, if known, else unknown>
  since: <UTC ISO time, for example 2026-11-19T03:10>
  task: <DD-Tnn, module name or milestone>
  files: <the paths or folders this session may write>
  progress: <path of the session's PROGRESS.md entry or work folder>
  ```
- **Session-start check (PowerShell 7).** Corrected 2026-11-18 by the fixer pass: the earlier check
  always found the agent itself and the owner's Claude desktop app, and had no rule for telling a live
  writer from an idle app.
  ```
  $R = 'Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native'
  $parent = (Get-CimInstance Win32_Process -Filter "ProcessId=$PID").ParentProcessId
  Get-Process | Where-Object { $_.ProcessName -match 'codex|ChatGPT|claude' -and $_.Id -ne $PID -and $_.Id -ne $parent } | Select-Object ProcessName, Id, StartTime
  Get-Content -LiteralPath "$R\build\work\WRITER.lock" -ErrorAction SilentlyContinue
  $cut = (Get-Date).AddMinutes(-30)
  foreach ($d in 'core','tests','tools','view','demo','docs','gpu','kernels','build\work') { Get-ChildItem -LiteralPath (Join-Path $R $d) -Recurse -File -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -gt $cut -and $_.FullName -notmatch '\\build\\work\\(snapshots|logs)\\' } | Select-Object FullName, LastWriteTime }
  $pkg = Get-ChildItem -LiteralPath "$R\docs\handoff\next-agent" -File | Select-Object Name, Length, LastWriteTime
  ```
  (`-ErrorAction SilentlyContinue` covers a folder that does not exist.)
- **Reading the check.** Your own shell and its parent are excluded; other processes of your harness,
  and the owner's Claude desktop app, still match the pattern and are not by themselves writers. A
  **live writer** is a process for which files changed in the last 30 minutes that you did not write,
  or that a fresh lock names. With a live writer this session is read-only (`00-MASTER-PROMPT.md` 5.1
  start step 3). **Before the first write**, list `docs\handoff\next-agent\` again and compare with
  `$pkg`: a changed file means a live writer; re-read the file before relying on it.
- **Rules.** Write the lock only if none exists, or the existing one is stale: older than 12 hours,
  no file under the folders above newer than its `since`, and no matching live process (12 hours is
  this file's proposal; the owner may change it). Record a takeover in PROGRESS with the old lock's
  text. Remove the lock at session end. Never write under `core\`, `tests\` or `tools\` without
  holding it.
- **DoD (can fail).** Each session's PROGRESS entry shows the check output and "lock taken" or "lock
  held by <holder>: read-only session"; the lock file is absent between sessions (a reviewer can
  check the file time against the PROGRESS entries).
- **Depends on.** Nothing. **Risk.** Another tool that does not know the lock: the file-time check
  still catches it; say in the report that the lock protects only sessions that read it.
  **Effort.** S.

### DD-T03 Reword law 6 for wild mode (the owner already decided)

- **Inputs.** P41, P42 (`PROMPTS.md:861`, `:869`); `OWNER-WORDS.md:138-143`;
  `ABI-V4-NOTICE.md:376-393`; `05-LAWS-AND-REQUIREMENTS.md:37` (not reworded yet).
- **Files.** `docs\agents\COMMON.md:27-28` (law 6); the "Deterministic" row of the ARCHITECTURE law
  table (`docs\ARCHITECTURE.md:79`). Change nothing else in either file.
- **Proposed text for law 6.**
  > 6. **Wild by default, lab for tests.** A living run draws fresh operating-system entropy every
  > tick: no seed decides it and no two runs are the same. The seeded lab mode (`--lab`) is the test
  > bench only (suite, lesion contrasts, reference hashes, benchmarks); there, integer or fixed-point
  > state and the counter-based RNG (Philox4x32-10) give the same state hash for the same seed on any
  > thread count. Matter and energy balance exactly in both modes.
- **Commands.** `Select-String -LiteralPath "Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\docs\agents\COMMON.md" -Pattern 'Wild by default'`
  returns one line; the text check.
- **DoD (can fail).** The new wording is present in both files; a line-by-line comparison against the
  pre-task snapshot shows only those lines changed; a DEC entry (kind LAW-READING, the owner's words
  P41/P42 cited) exists.
- **Laws.** Law 6 itself, aligned with the owner's newest words. **Effort.** S.

### DD-T04 The founder-species bound: no silent clamp, one range for every tool

- **Inputs.** DL-02 item 4; `core\az_worldgen.c:897`; `core\az_abi.h:862`, `:1850`; the five tool
  lines cited there.
- **Decision to log first (kind AGENT).** Option A: keep a bound, make it one shared value, and refuse
  a larger request at `az_world_create` with a stated reason (the tools print it). Option B: lift the
  bound to a run parameter sized by memory. Default: A now (small, safe); B when a module or the
  owner needs more than the bound. Read every use of `WG_MAX_SPECIES` in `core\az_worldgen.c` first: if
  arrays are sized by it, B needs arena scratch and stays within the 2048-byte frame rule.
- **Files.** `core\az_worldgen.c` (the clamp); `core\az_abi.h` (only if a refusal counter or a shared
  constant is added; lead-only file, change between pieces of work and record the ABI change);
  `tools\az_run.c`, `tools\az_snap.c`, `tools\az_live.c`, `tools\az_bench.c`, `tools\az_lesion.c` (the
  ranges); `tests\test_worldgen.c` (new checks).
- **Test first (fails before the fix).** Create a lab world with `n_species = 101`: today it silently
  gets 100 founder species; the new check requires a refusal with its reason (A) or 101 species (B).
  A second check: every tool's accepted range equals the core's.
- **DoD (can fail).** The new checks fail on the pre-task tree and pass after; ctest all green; the
  100k reference hash unchanged (default `--species` is unaffected); `tools\*` print the same range in
  their usage.
- **Evidence.** Both test runs (before and after) in PROGRESS; DEC entry.
- **Depends on.** DD-T02. **Risk.** `az_bench`'s 512 is "the default clade limit": check what the
  clade store does above it before choosing B. **Effort.** M.

### DD-T05 Room in the parameter table before sense, physics and stats

- **Inputs.** DL-02 item 5; `core\az_abi.h:1020`; `core\az_snapshot.c:212`, `:319`, `:433-435`, `:488`;
  `docs\agents\W2-BRIEFS\sense.md`, `physics.md`, `stats.md`.
- **Steps.**
  1. Count the parameters each of the three briefs implies (every number of biology they name must be
     a parameter or a gene). Write the three counts in PROGRESS.
  2. If the total exceeds 23 (the free slots), or leaves fewer than 16 free (a margin proposed by this
     file), grow the table in one ABI change: either a larger fixed array (rounded up to a power of
     two) or a size read from the `.def` count at build time. Bump the snapshot format version.
  3. Find out whether `param` is part of `az_state_hash` (read the hash function); if it is, the
     reference hashes change by design: record the new lines in `tests\golden_hashes.txt` with the
     cause.
  4. Add a build-time check that fails when the `.def` files define more parameters than the table
     holds (for example a `_Static_assert` on the generated count), with a planted-violation self-test
     if the lint gets the rule.
- **Files.** `core\az_abi.h` (lead-only), `core\az_snapshot.c`, `tests\test_snapshot.c`, possibly
  `tools\lint_core.py`; `tests\golden_hashes.txt` if hashes change.
- **DoD (can fail).** The three counts recorded; the table has the room they need plus the margin;
  the static check fails on a planted extra parameter and passes on the real tree; ctest green;
  reference hashes unchanged or re-recorded with cause.
- **Depends on.** DD-T02; before the sense module lands. **Risk.** Old snapshots refuse again (OD-19:
  expected; say so). **Effort.** M.

### DD-T06 The law-2 proposal and the gate questions in the interim report

- **Inputs.** OD-05, OD-13; `STATUS_AND_PLAN.md:692-698`; `TRIGGER-REGISTER.md:21-38`.
- **Files.** `build\work\PROGRESS.md` (the proposed wording, verbatim from OD-05); `docs\eval\DECISIONS.md`
  (DEC-0005, DEC-0013); the interim report written at the end of milestone 8 (its file is named by the
  milestone-8 plan; if none, `docs\eval\INTERIM-REPORT.md`).
- **DoD (can fail).** The interim report lists every G1 condition with its run id and PASS or FAIL,
  and ends with the two questions of OD-05 and OD-13 word for word; COMMON.md law 2 is unchanged until
  his yes (a comparison against the snapshot shows no change).
- **Depends on.** Milestones 6 to 8. **Effort.** S.

### DD-T07 Worldgen's biology structure as parameters (after physics lands)

- **Inputs.** DL-02; `build\work\w2\worldgen\DESIGN.md:464-498`; `core\params\az_worldgen.def`;
  the physics module once landed.
- **Steps.**
  1. Guild-to-band map: replace the fixed map with a per-world draw weighted by new parameters (one
     weight per guild and band pair, or a smaller set with a stated reason), whose defaults put all
     weight on today's map so that the default world is unchanged.
  2. Layer order: once physics has media densities, compute the stacking order from them; defaults
     reproduce ocean under air.
  3. Gravity -z: document in `DESIGN.md` and the module header as a frame convention, not a limit.
- **Files.** `core\az_worldgen.c`, `core\params\az_worldgen.def`, `tests\test_worldgen.c`,
  `build\work\w2\worldgen\DESIGN.md`.
- **Test.** At default parameters the 100k reference hash is unchanged (if the draw consumes no new
  random numbers at default weights; otherwise record the new hash with cause). A non-default weight
  set produces founders in another band, checked by a test; a lesion or parameter contrast on tuning
  seeds shows the change reaches an outcome.
- **DoD (can fail).** The three items done; tests pass; parameter count within DD-T05's room; the
  "structure stays fixed" line in the module header replaced by the new reading.
- **Depends on.** Physics landed; DD-T05. **Risk.** Founders seeded where they cannot live: the
  niche-fit cascade must still hold (`PROGRESS.md:1569-1578`). **Effort.** L.

### DD-T08 Old snapshots: a refusal that says why

- **Inputs.** OD-19; `core\az_snapshot.c:433-435`, `:543`.
- **Steps.** Load one pre-worldgen `.azsnap` (from `build\demo\worlds\` if one exists; otherwise make a
  test file with an out-of-range parameter) with `az_run --load`. Record what is printed. If the
  message does not name the reason (version, format, or parameter out of range with its name), make
  the loader report a reason code that the tools print.
- **Files.** `core\az_snapshot.c`, the loading tools, `tests\test_snapshot.c`.
- **DoD (can fail).** A test loads a file with one parameter out of range and checks that the reason
  names the parameter; the load still refuses (no crash, no partial world).
- **Effort.** S.

### DD-T09 Provenance and mode columns in every evidence table

- **Inputs.** DL-01, DL-09.
- **Files.** Every table created under `docs\eval\` from now on; the preregistration template in
  `docs\prereg\`.
- **Rule.** Columns `provenance` (native-lab, native-wild, v40-ts, grokbot) and `run id` are mandatory;
  a verdict uses rows of one provenance only.
- **DoD (can fail).** A grep for tables in `docs\eval\` without a `provenance` header finds none.
- **Effort.** S (and ongoing).

### DD-T10 The lesion run-length rule in the preregistration template

- **Inputs.** DL-13.
- **Files.** The preregistration template in `docs\prereg\` (milestone 8).
- **Rule to write.** Per lesion: the mechanism's update period (for example every 32 ticks for the
  tier-1 kernels), the run length chosen and why, the measure and its direction, all before any
  reporting run; and the statement that NO EFFECT at that length is reported as such.
- **DoD (can fail).** Every lesion row of the E1 preregistration has the four fields; its SHA-256 is
  pinned before the first reporting run.
- **Depends on.** Milestone 8. **Effort.** S.

---

## 6. The decision log: `docs\eval\DECISIONS.md`

### 6.1 The rule

- Every default taken on an owner decision, every law reading, and every design choice that a reviewer
  could question gets an entry **before** the work that depends on it lands.
- The PROGRESS.md entry for that work cites the DEC id ("decision: DEC-0016").
- Entries are append-only. Never edit the "Taken" or "Why" of an entry after the fact; add a dated line
  under "History" and change "Status". A reversed decision gets a new entry that supersedes the old one.
- When the owner answers, his words go verbatim into "Answer" (with the P number added to
  `docs\handoff\PROMPTS.md`), the status becomes OWNER-CONFIRMED or OWNER-CHANGED, and every entry that
  depended on the old default is re-checked and noted.
- A decision that changes a reference hash names the line added to `tests\golden_hashes.txt`.
- The file follows the project text rule: ASCII, LF, no tabs, final newline.

### 6.2 The fields of one entry

```
### DEC-0016 Real-time viewer technology
- Date (UTC): 2026-11-19T03:10
- Session: Codex CLI / GPT Astra 6.2 (PROGRESS.md heading "2026-11-19, viewer step 1")
- Kind: OWNER | AGENT | LAW-READING
- Status: DEFAULT-IN-FORCE | OWNER-CONFIRMED | OWNER-CHANGED | SUPERSEDED-BY DEC-nnnn | WITHDRAWN
- Question: one sentence, as it would be asked
- Options: A ...; B ...; C ...
- Taken: C (A first, then B)
- Why: two to five sentences, with numbers that were measured and cited
- Owner words and laws: P10 (PROMPTS.md:442); law 1; rule D7
- Sources: path:line, path:line
- If decided otherwise: what changes, in one to three sentences
- Reversible: yes (how) | no (why)
- Hash impact: none | tests/golden_hashes.txt line <n> (<old> -> <new>)
- Asked: <report file and date> | not yet | not to be asked (law reading)
- Answer: <his words verbatim, Pnn> | none yet
- History: <date> <what changed>
```

### 6.3 The tables at the top of the file

```
| DEC | Title | Kind | Status | Asked | Answer |
|---|---|---|---|---|---|

## Open questions (at most three go into each report, in priority order)
| Priority | DEC | Ask (one sentence) | First asked | Times asked |
|---|---|---|---|---|
```

Ask the same question at most once per report, and stop repeating a question after three reports
without an answer: keep the default and mark it "asked 3 times, default kept".

### 6.4 The entries to create first

| DEC | From | Title | Kind | Status at creation |
|---|---|---|---|---|
| DEC-0001 | OD-01 | First git commit | OWNER | DEFAULT-IN-FORCE (no commit) |
| DEC-0002 | OD-02 | Licence | OWNER | DEFAULT-IN-FORCE (none) |
| DEC-0003 | OD-03 | Lock pages in memory | OWNER | DEFAULT-IN-FORCE (VirtualLock) |
| DEC-0004 | OD-04 | Installs | OWNER | DEFAULT-IN-FORCE (none; missing list) |
| DEC-0005 | OD-05 | Law-2 wording for phase 2 | OWNER | DEFAULT-IN-FORCE (phase 1 holds) |
| DEC-0006 | OD-06 | NPU path | OWNER | DEFAULT-IN-FORCE (probe only) |
| DEC-0007 | OD-07 | Quiet-machine window | OWNER | DEFAULT-IN-FORCE (labelled load) |
| DEC-0008 | OD-08 | Sex locus | OWNER | DEFAULT-IN-FORCE (decided M1, PROGRESS.md:50) |
| DEC-0009 | OD-09 | Unused columns v59, v63 | OWNER | DEFAULT-IN-FORCE (decided M4) |
| DEC-0010 | OD-10 | Seeds for the 500 checklist | OWNER | DEFAULT-IN-FORCE (48) |
| DEC-0011 | OD-11 | Delete the UEFI folder | OWNER | DEFAULT-IN-FORCE (leave) |
| DEC-0012 | OD-12 | Wave 2 order | OWNER | DEFAULT-IN-FORCE (one at a time) |
| DEC-0013 | OD-13 | G1 gate and the go to scale | OWNER | DEFAULT-IN-FORCE (wait) |
| DEC-0014 | OD-14 | Budget and stop line | OWNER | DEFAULT-IN-FORCE (small steps, stop on refusal) |
| DEC-0015 | OD-15 | GPU in the tick before G1 | OWNER | DEFAULT-IN-FORCE (no) |
| DEC-0016 | OD-16 | Viewer technology | OWNER | DEFAULT-IN-FORCE (WebGL2 first, D3D11 second) |
| DEC-0017 | OD-17 | Viewer may use the GPU | LAW-READING | DEFAULT-IN-FORCE (yes, outside the hash) |
| DEC-0018 | OD-18 | C: toolchain | OWNER | DEFAULT-IN-FORCE (leave) |
| DEC-0019 | OD-19 | Old world snapshots | OWNER | DEFAULT-IN-FORCE (keep, do not convert) |
| DEC-0020 | OD-20 | Thinking period (TR-16) | OWNER | DEFAULT-IN-FORCE (not built) |
| DEC-0021 | OD-21 | Journal replay | OWNER | DEFAULT-IN-FORCE (no replay) |
| DEC-0022 | OD-22 | Worldgen fixed structure | AGENT | DEFAULT-IN-FORCE (DL-02 split; DD-T07) |
| DEC-0023 | OD-23 | Push, publish, claims | OWNER | DEFAULT-IN-FORCE (never) |
| DEC-0024 | OD-24 | Non-token spending | OWNER | DEFAULT-IN-FORCE (never) |
| DEC-0025 | DL-01 | Wild versus lab: roles and claims | LAW-READING | DEFAULT-IN-FORCE |
| DEC-0026 | DL-02 | Physics fixed, biology parameters, bounds refused | LAW-READING | DEFAULT-IN-FORCE |
| DEC-0027 | DL-03 | Indicator-only wording | LAW-READING | DEFAULT-IN-FORCE |
| DEC-0028 | DL-04 | Speed only with an unchanged hash | LAW-READING | DEFAULT-IN-FORCE |
| DEC-0029 | DL-05 | DRAM-bound label; scale and evolution reported apart | LAW-READING | DEFAULT-IN-FORCE |
| DEC-0030 | DL-06 | Decisive chain before breadth | AGENT | DEFAULT-IN-FORCE |
| DEC-0031 | DL-07 | Autonomy rules | LAW-READING | DEFAULT-IN-FORCE |
| DEC-0032 | DL-08 | One writer at a time (DD-T02) | AGENT | DEFAULT-IN-FORCE |
| DEC-0033 | DL-09 | Provenance kept apart | AGENT | DEFAULT-IN-FORCE |
| DEC-0034 | DL-10 | Live view from a between-tick copy; proof picture kept | AGENT | DEFAULT-IN-FORCE |
| DEC-0035 | DL-11 | Law 1 covers world code, not the viewer | LAW-READING | DEFAULT-IN-FORCE |
| DEC-0036 | DL-12 | State of the art as ideas, not packages | LAW-READING | DEFAULT-IN-FORCE |
| DEC-0037 | DL-13 | Lesion run length preregistered | AGENT | DEFAULT-IN-FORCE |
| DEC-0038 | DL-14 | Every word gets a row | LAW-READING | DEFAULT-IN-FORCE |
| DEC-0039 | DD-T04 | Species bound: refuse (A) now, lift (B) later | AGENT | DEFAULT-IN-FORCE |
| DEC-0040 | DD-T05 | Parameter table growth and margin | AGENT | DEFAULT-IN-FORCE |

DEC-0041 onward: the worldgen lead decisions copied from `build\work\PROGRESS.md:1569-1578`
(WORLDGEN_W_REST, the NICHE_HI and NICHE_LO gap, SURFACE_SOLID 16, per-world strategy shares, the
five-step fit cascade, founder pools capped by the body, the narrower test fixtures).

---

## 7. What I could not resolve

- **The owner's OpenAI budget and stop line** are not on record; OD-14's default avoids inventing one.
- **Whether `param` is inside `az_state_hash`** was not checked; it decides whether DD-T05 changes the
  reference hashes.
- **Whether the 100-species bound has an owner source.** A search of `PROMPTS.md` for "species" found
  nothing, so I treated it as the plan's number (`CONTINUE-BUILD.md:52`). If the owner said "100
  species" somewhere outside PROMPTS.md (a pasted text, a requirement file), DD-T04 option A still
  holds, but the DEC entry should cite it.
- **Who made the `2026-11-18-pre-sense-astra` snapshot** (18:25 UTC): resolved by the lead's pass of
  2026-11-18. A GPT Astra session in the Codex desktop app took it (its Codex logs show
  `snapshot.py pre-sense-astra`, then the 100k baseline and ctest of DL-08); it is not recorded in
  PROGRESS.md, and no sense code exists yet. The next session still runs DD-T02's check before starting
  sense.
- **The viewer default (OD-16)** is my judgement; the owner's preference between a browser page and a
  native window is unknown. Both options need no install (headers and DLLs checked).
- **The large-page logon behaviour** (OD-03: a new logon is usually needed) is general Windows
  knowledge, not checked on this machine; `whoami /priv` settles it.
- **The numbers quoted from the hand-off request** (picture times, 150-330 ticks per second, 380,865
  refusals, the Combo A values, the lesion d values) come from the computed task text of 2026-11-18 and
  the previous agent's outputs, which sit outside Z:; I did not re-run or re-read those outputs. The
  worldgen numbers match `build\work\PROGRESS.md` (worldgen entry, `:1545` onward). The small-world
  picture times and tick rates have no log; the owner's 1M demo world does
  (`build\work\logs\demo_owner_world_2026-11-18_f226.json` and `build\work\logs\demo_owner_world_2026-11-18.json`, 08 section 2.5).
- **The stale-lock age (12 hours) and the parameter margin (16 slots)** are this file's proposals, not
  the owner's numbers.
- **Fixer pass, 2026-11-18.** OD-16's per-creature size corrected to 08's 24 bytes; DL-05's knee
  corrected to about 35,000 creatures (08 section 2.4); DD-T02's check now excludes the agent's own
  processes and defines a live writer by files changed in the last 30 minutes; OD-25 added (the idea-bank
  catalogs). The 30-minute window is a proposal, not the owner's number.
- **Second fixer pass, 2026-11-18.** The PROMPTS.md line citations moved with its 21-line header
  (old to new: 275 to 296, 416 to 437, 422 to 443, 713 to 734, 787 to 808, 803 to 824, 819 to 856, 827
  to 864); the STATUS citations moved with its regeneration; the owner's 1M world figures cite the
  frame-226 record (`demo_owner_world_2026-11-18_f226.json`).
- **P52-P55 numbering, 2026-11-18.** PROMPTS.md gained one header-note line and four index rows, so
  every P01-P51 citation here moved by 5 (296 to 301, 437 to 442, 443 to 448, 734 to 739, 808 to 813,
  824 to 829, 856 to 861, 864 to 869); two older ones of DL-02 Side A were corrected at the same time
  (P40 811 to 853, P13 434 to 460).
