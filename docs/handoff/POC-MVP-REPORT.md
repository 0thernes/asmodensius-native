# Asmodensius-Native: PoC MVP report

**Date:** 2026-11-18. **Written by:** the Claude desktop session, after the API run ended; updated the
same day after the desktop session landed worldgen. **Root:**
`Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native` (Z: only; there are no drive letters).

Everything marked **verified** was run on 2026-11-18, and the command and its log are named. Rows marked
"before worldgen" were run earlier that day on the tree before the worldgen landing; they stay as history.
Everything else says where the claim comes from.

---

## 1. Bottom line

**What exists:** a native artificial-life engine you can build, test, run and watch.
- It is a freestanding C23 core (integers only, no libc, no floating point), with NASM assembly twins and
  an optional PTX GPU component, running as a Windows program from the Z: Dev Drive.
- The engine builds from the real Z: path with 0 warnings, and its full suite passes: 46 of 46.
- A living world runs, renders itself, and can be watched and steered in a local web GUI. Creatures are
  born, eat, fight, steal, share, groom, teach, trade, mate, form colonies and die. Their genomes, brains
  and bodies are of variable size.
- **The world is now an alien world of its own** (worldgen, landed 2026-11-18 after the API credit ran
  out): terrain over bedrock, a solvent ocean under carrier gas, vents on the sea floor, toxin hot spots
  and reefs, temperature gradients, two suns, a grav field that bends, and 1 to 100 founder species, each
  with its own niche.
- Matter and energy balance exactly after every tick, in every run.
- Wild worlds, the default, never repeat. Lab worlds, the test bench, repeat bit for bit on any number of
  threads.

**Verdict:** this is a **shippable proof-of-concept MVP of the engine**. It is not the finished science.
- **Three modules are still stand-ins:** senses, physics and statistics. The physics stub ignores
  gravity, rock and density, so the world worldgen makes is only partly felt by the bodies in it.
- **Not shown yet:** any result that evolution selects a trait at the world level; open-ended evolution;
  any of the owner's checklists (14, 10, 25, 100, 500) run against this engine.
- **Not independently reviewed:** nine modules have been checked only by the agent that wrote them;
  worldgen had three adversarial reviews inside its own workflow, but no reproduce-first review by
  someone outside it.
- Read every number below as a measurement of an engine, not as a claim about life or mind.

## 2. See it and test it yourself

1. Double-click `demo\Start-Demo.cmd`. Or run:

   ```
   python -I "Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\demo\live_server.py" --open
   ```

2. Open <http://127.0.0.1:8765/>. The page is served by this computer only. It runs on `build\z` and runs
   the worldgen world, but draws only bodies (no terrain, ocean, vents or suns yet; 08 RV-A adds them).

| Tab | What you can do |
|---|---|
| Live world | Press **Start new world**. Choose Wild (default) or Lab with a seed, founders, founder species and slots (memory). Switch any of the 21 lesions off, or change any world parameter of `core\az_params.def` and `core\params\*.def` (worldgen adds 46 `WORLDGEN_*` parameters in `core\params\az_worldgen.def`). While it runs: pause, step 1/10/100, change the ticks per picture, the pictures per second and the picture size; drag the picture to orbit and use the wheel to zoom; save the world and load it again later. It shows proof badges (books exact, viewing changes nothing, fresh entropy, locked arena), 10 tiles (tick, alive, eggs, lineages, colonies, births, deaths, generation, mean energy, tick speed) and five charts with hover (population, births and deaths per tick, lineages, pair interactions by kind, speed). It also shows the largest lineages, how unlike each other the living are (genes, neurons, body parts, store units: lowest, mean, highest), where each tick's time goes, the event log and a data table. |
| Proof checks | One button each: **lab repeatability** (three runs, one on a single thread, must end on the same hash), **wild never-twice** (two runs must differ, and both must keep exact books), **lesion A/B** (one lab world with and without a chosen lesion) and **reference world** (a recorded hash from `tests\golden_hashes.txt`, run again). |
| Test suite | Runs the whole ctest suite of `build\z` and shows each test as it finishes. The last full run took 253 s. |
| About | What is real, what is a stub, and how to run each part without the page. It already says worldgen is real. |

The engine program behind the page is `tools\az_live.c`, built as `build\z\az_live.exe`. It prints one JSON
line per picture and takes commands on stdin. `demo\README.md` has the details.

**Note:** snapshots saved before the worldgen landing no longer load, because the parameter table grew.

## 3. What is built

| Layer | What it is | Status |
|---|---|---|
| Contract | `core\az_abi.h` (ABI v4), `az_columns.def`, `az_cells.def`, `az_owners.def`, `az_params.def` and `core\params\*.def` (now with `az_worldgen.def`, 46 parameters) | frozen at v4 for wave 2 |
| Lead core | `az_world.c` (arena, columns, hashing, audits), `az_tick.c` (the pipeline), `az_store.c` (variable-size creature store) | real |
| Foundation modules | rng (Philox4x32-10), fx (fixed point and rotors), grid, scan (27-cell neighbours), field, genome (variable diploid), brain (sparse int8 per creature), ledger (event chain), snapshot (exact save and load) | real |
| Wave 2 (10 of 13) | life (stages, broods, births, deaths), mate (8 modes, choice), tier1 (learning, memory, mood), metab (feeding, respiration, growth), social (attack, steal, share, groom, teach, recruit, signals), econ (pantry, barter), colony (colony as an organism), weather (two suns, light, heat, wind, storms, vents), chem (reactions, burial, phases, diffusion by phase, settling and rising, heat carried with matter), **worldgen** (`core\az_worldgen.c`: the alien world and its founder species; section 4a) | real; worldgen had three adversarial reviews in its workflow, the other nine only their author |
| Wave 2 stubs (3 of 13) | sense (a few channels), physics (thrust and drag; no gravity, rock or medium density), stats (head counts) | **stand-ins** |
| Tsotchke kernels | `tk_sh`, `tk_hop`, `tk_qgt`, `tk_qsv` (MIT-derived) | tested, and all four run in the tick inside tier-1 learning (`core\az_tier1.c`): state vector, associative memory, crowd shape, learning metric; their lesions are tested, but show no effect yet on the 64-tick lesion runs (corrected 2026-11-18; this row said "not wired") |
| Assembly twins | brain step AVX2/AVX-VNNI (the only one the tick calls), dense VNNI, Philox AVX2 | real, bit-identical to C |
| Windows host | page-locked arena, worker pool with ideal processors, ns clock, ledger sink | real |
| View | `view\az_view.c` (3D software renderer, an observer), `az_png.c` (PNG writer) | real |
| GPU | `gpu\` (CUDA driver API without a toolkit; clang to PTX) | verified standalone; not in the tick |
| Tools | `az_run`, `az_snap`, `az_live`, `az_lesion`, `az_bench`, `az_brainbench`, `az_budget`, `az_ledgerdump`, `az_rngcheck`, `az_gpubench` | real |
| Tests and gates | 33 test programs (now with `test_worldgen`, 4034 checks); 13 gates: lint and its self-test, object gate and its self-test, AVX stack gate and its self-test, ledger resume, three determinism matrices, UBSan trap build, `cli_snap_modes`, `cli_live_smoke` | **46 of 46 pass** |
| Demo | `demo\live_server.py`, `demo\live.html`, `demo\Start-Demo.cmd`, `demo\README.md` | used in the browser; runs the worldgen world on `build\z`; draws bodies only (no terrain, ocean, vents or suns yet) |
| Docs | ARCHITECTURE (contract), the agent briefs, 38 harvested requirement files, PROMPTS (P01 to P55), OWNER-WORDS, STATUS_AND_PLAN, the hand-off package `docs\handoff\next-agent\` | current except the generated STATUS, which needs regenerating |

## 4. What is working: verified on 2026-11-18

| Claim | Command or place | Result |
|---|---|---|
| It builds from the real Z: path | `cmake -S "<root>" -B "<root>\build\z" -G Ninja`, then `cmake --build "<root>\build\z"` (`build\work\logs\build_z_worldgen.log`) | **0 warnings**: 19 real modules, 3 stubs, 3 assembly twins. Lint 0 errors, 3 warnings (unchanged); `check_text.py` passed on the files that landing changed (`--since`); the whole root has 83 files with violations and 2 exempt (351 checked, 2026-11-18), the 83 all in docs\research and docs\requirements, the exempt two PROMPTS.md/.xml (the owner's words byte for byte, listed by name in check_text.py), none in the hand-off package (09 DOC-01/DOC-02). Before worldgen: 166 steps, 18 real modules, 4 stubs (`build_z.log`) |
| The full suite passes | `ctest --test-dir "<root>\build\z" --output-on-failure -j 6` (`build\work\logs\ctest_z_worldgen.txt`) | **100% of 46 passed in 253 s** (33 test programs, 13 gates), including the UBSan trap build, which configures a second build from the bracketed path. Before worldgen: 45 of 45 in 386 s (`ctest_z.txt`) |
| The reference world reproduces | `az_run --lab --n 100000 --ticks 60 --cap 400000 --grid 6,6,5` on 1, 8 and 24 threads (`build\work\logs\wg_ref100k_t1.txt`, `wg_ref100k_t8.txt`, `wg_ref100k_t24.txt`) | **`73f1e9e013f8db50`** on all three; 217,324 births, 89,855 deaths; 227,469 alive at tick 60 (`wg_ref100k_t8.txt`). Recorded in `tests\golden_hashes.txt`. The pre-worldgen reference was `2a0607a1d1151263` (293,112 alive at tick 60); it is now history |
| The 1M reference world | `az_run --lab` at 1M with `--store-units 52000000` (`build\work\logs\wg_ref1m.txt`) | **`5d9c02d2c7dd5fb1`**, books exact. The pre-worldgen reference was `d7297084ce3e97ee` |
| The engine matrix | `build\work\logs\wg_matrix.txt` | `24c923001a46b122` (printed, not pinned). The pre-worldgen value was `dd59809c915fc8ca` |
| Wild worlds never repeat | GUI check before worldgen: two wild runs of 3,000 founders and 120 ticks; after worldgen, a wild 100k run (`build\work\logs\wg_wild100k.txt`) | different hashes and origins, exact books in both; the wild 100k run ended on `984e07a7655176a2` with exact books |
| Lab worlds repeat | ctest determinism matrices (1 to 24 workers, reorder, splits, kernels on and off, snapshot resume); the 100k reference on 1, 8 and 24 threads; GUI lab repeatability check before worldgen (3,000 founders, 120 ticks, seed 42) | equal hashes on every variation; the GUI check gave `21803ecc34e8b6e8` three times, once on a single thread (before worldgen) |
| Every worldgen draw is on the world key | the worldgen landing (`build\work\PROGRESS.md`, entry "wave 2, module 10, worldgen landed"; `build\work\w2\worldgen\PROGRESS.md`) | wild worlds differ, lab seeds repeat |
| The books are exact | every `az_run`, `az_snap` and `az_live` run compares matter and energy with the start after every tick | exact in every run, wild and lab |
| Watching does not change the world | `az_live` and `az_snap` hash the state before and after drawing | equal on every picture |
| The live world is the engine's world | `cli_live_smoke`: `az_live --lab --seed 7` against `az_run --lab --seed 7` | passes in the 46 of 46 run. Before worldgen: same hash `292933ccf1ada692` (2,000 founders, 20 ticks, seed 7); 21 of 21 checks |
| The brain matters | GUI lesion A/B before worldgen, brain lesion, 3,000 founders, 120 ticks; after worldgen, the lesion table below | before worldgen: births 8,683 to **0**; alive 10,143 to 2,955; pair interactions 56,507 to 851. After worldgen: BRAIN d_z +3.20 |
| Lesion table (tuning seeds 1-24, exploratory) | `build\z\az_lesion.exe --seeds 1-24` (`build\work\logs\lesion_z_worldgen_1_24.txt`) | **7 EFFECT**: BRAIN d_z +3.20, SOCIAL -3.25, TRADE +1.31, MATING -2.00, MUTATION -1.82, KIN -0.84, TOXIN -0.84. **14 NO EFFECT**, among them WEATHER (-0.29, it lost its effect) and GRAVITY (+0.39; it now changes the state on 24 of 24 seeds). **0 INERT.** Before worldgen (`lesion_z_1_24.txt`): 6 EFFECT (BRAIN +4.62, SOCIAL -2.80, TRADE +1.56, MATING -0.87, MUTATION -3.23, WEATHER -1.04), 14 NO EFFECT, 1 INERT (GRAVITY) |
| A planted defect gets caught | `build\work\w2\mutants.py` | **worldgen 31 of 31**; earlier, the agent's runs: chem 27 of 27 (13 piece A + 14 piece B); colony 23 of 23; weather 16 of 16 |
| A big wild world lives | before worldgen, the agent's final demo: `az_run --n 100000 --ticks 60 --cap 400000 --grid 6,6,5` (`build\work\final\f_run_wild.txt`); after worldgen, `build\work\logs\wg_wild100k.txt` | before worldgen: 275,710 alive after 60 ticks; 188,395 births, 12,685 deaths; 3,707 lineages, 394 colonies; books exact; 56 ms per tick. After worldgen: 162,276 alive after 60 ticks; 78,667 births, 16,391 deaths; 3,714 lineages, 445 colonies; books exact |
| Nothing is fixed | the GUI's spread table, live (before worldgen); the 100k reference after worldgen (`wg_ref100k_t8.txt`) | before worldgen: genes 152-168 per creature, neurons 24-40, body parts 1-8, brain tiles up to 60, store blocks 31-52 units (one world at tick 172). After worldgen: genes 152-168, neurons 24-40, body parts 1-8, store blocks 29-55 units |

## 4a. Worldgen, landed after the credit

The API credit ran out before worldgen started (section 9). The owner then asked the Claude desktop
session (Claude Opus 5.5, max effort, ultracode, on the Claude plan, not the credit) to finish to a safe
stage. It built wave 2 module 10, worldgen (`core\az_worldgen.c`), through workflows:

1. **Design:** `build\work\w2\worldgen\DESIGN.md`.
2. **Implementation in a staging tree**, not in the live tree.
3. **Three adversarial reviews.** All three said "do not ship".
4. **A fixer** that reproduced every real finding first, then fixed it.
5. **Landing** into the tree, then the build, ctest, reference worlds, lesion table and speed above.

**What it makes:**
- terrain from integer value noise over bedrock;
- a solvent ocean under carrier gas (sea level is a parameter);
- 1 to 64 vents on the sea floor, each with its buried reservoir;
- toxin hot spots and reefs;
- temperature by depth, latitude and the nearest vent plume;
- two suns with coprime days (defaults 2000 and 3101 ticks);
- the grav field that bends: a downward pull, plus a lateral pull toward high terrain that fades with
  height, plus a low-order swirl; the scalar gravity is its mean;
- 1 to 100 founder species, each with a niche: 5 guilds (producer, chemotroph, grazer/decomposer,
  scavenger, predator); habitat bands (photic, mid-water, floor, vent floor, air); all 8 mating modes once
  there are 8 or more species; size, social style and life history drawn per world;
- one `az_genome_fit` prototype per species (a five-step fit cascade: niche, habitat, core); founders
  varied on 3 loci each, placed in habitat patches, never in bedrock;
- 46 `WORLDGEN_*` world parameters (`core\params\az_worldgen.def`).

Every draw is on the world key: wild worlds differ, lab seeds repeat.

**Evidence:**
- `build\work\PROGRESS.md`, the entry "wave 2, module 10, worldgen landed";
- `build\work\w2\worldgen\PROGRESS.md` and `build\work\w2\worldgen\DESIGN.md`;
- `build\work\logs\build_z_worldgen.log`, `ctest_z_worldgen.txt`, `wg_ref100k_t1.txt`, `wg_ref100k_t8.txt`,
  `wg_ref100k_t24.txt`, `wg_ref1m.txt`, `wg_matrix.txt`, `wg_wild100k.txt`, `lesion_z_worldgen_1_24.txt`;
- recovery snapshots `build\work\snapshots\2026-11-18-pre-worldgen` and
  `build\work\snapshots\2026-11-18-post-worldgen`. The newer `2026-11-18-pre-sense-astra` (517 files,
  28.9 MB, 18:25:32 UTC) was taken by the GPT Astra session in the Codex desktop app, which re-ran the
  100k reference (`73f1e9e013f8db50`, `build\work\logs\sense_baseline_100k.txt`) and ctest (46 of 46 in
  238.52 s, `sense_baseline_ctest.txt`) and wrote no sense code: a recovery point and a reproduced
  baseline, not progress.

## 5. What is operational: how to drive each part

| Part | How |
|---|---|
| The living world, headless | `"<root>\build\z\az_run.exe" --n 100000 --ticks 60` (wild). Add `--lab --seed 42` for the bench, `--save FILE` and `--load FILE` to keep a world, `--ledger FILE` for every event. A 1M world needs `--store-units 52000000`. |
| A picture | `"<root>\build\z\az_snap.exe" --out "<folder>\x.png" --n 100000 --ticks 60` |
| The live world | `demo\Start-Demo.cmd`, or `az_live --dir <folder> --n 5000` with stdin commands |
| Lesion evidence | `az_lesion --seeds 1-24` (tuning seeds only; never run it bare: that uses the preregistered reporting seeds) |
| Speed | `az_bench`, `az_brainbench`, `az_budget`; `build\work\scripts\measure.ps1` |
| Tests | ctest as above; `build\work\scripts\run_tests.ps1` as a script block; `m_check.ps1` |
| Recovery | `python -I "<root>\build\work\scripts\snapshot.py" <label>` (nothing is committed to git) |
| Status page | `build\work\scripts\make_status.py` regenerates `docs\handoff\STATUS_AND_PLAN.md` |

## 6. Speed

- **100k creatures, after worldgen, on an idle machine:** 73.7 ms per tick on 8 workers, 59.9 ms on 24
  (with the E cores), 372 ms on 1. That is about 35 percent slower than the 54.7 ms before worldgen,
  because the founders now start clustered in their habitats.
- **1M creatures, after worldgen:** 1339 ms per tick on 8 workers. The run is DRAM-bound by arithmetic:
  every run above about 35,000 creatures is beyond L2 plus L3 by estimate (08 section 2.4) until M2
  measures the knee.
- **Before worldgen, on a busy machine:** about 55 ms per tick at 100k on 8 workers (lab, lightly loaded),
  81 ms with the suite running beside it; 1M at about 0.5 to 0.9 s per tick.
- **The live view (measured before worldgen):** a tick of a 10,000-creature world took 3 to 13 ms, but
  drawing one 960x540 picture took 0.1 to 0.5 s. No log holds these numbers; 3 to 13 ms per tick
  is 77 to 333 ticks per second; PQ-0 re-measures them on the worldgen world. The renderer is
  single-threaded, and it is the bottleneck of the demo. Raise "ticks per picture" for faster evolution,
  or lower the picture size.
- **The owner's own 1M world in the demo (2026-11-18, read-only, 19:40-19:42 UTC):** wild, 250,000
  founders of 100 species, cap 1,004,096, 8 workers, 1600x900 pictures every 60 ticks. At frame 226 (tick
  13,560): 1,000,416 alive, 414.3 ms per tick (about 2.4 ticks per second; perceive 138.7 ms, grid 45.0,
  act 38.1, tier1 27.2, life 19.8), and 4,261.3 ms to draw one picture of 878,035 bodies, with the same
  state hash before and after drawing; books exact. `tools\az_live.c` ticks (:527) and draws (:534) on one
  thread, so the world stands still for each picture, and a new one came about every 29 s (by arithmetic,
  not measured). Frame 226: `build\work\logs\demo_owner_world_2026-11-18_f226.json`; frame 228 (tick 13,680):
  `build\work\logs\demo_owner_world_2026-11-18.json`, 397.6 ms per tick, 4,383.3 ms per picture. Bodies only.
- **Not measured yet:** E3 (1 to 1M creatures, the full benchmark) and M2 (the per-stage cache table).

## 7. What is not done

| Milestone | State |
|---|---|
| 1-4 (findings, owner requests, hardening, ABI v4 "nothing fixed" plus wild mode) | done |
| 5, wave 2 | in progress: 10 of 13 modules (worldgen landed 2026-11-18); sense, physics and stats are stubs; lint warning W1, the seam review and an independent review of all 13 modules are not done |
| 6, viewer and speed | the live view exists; the native Windows viewer, the scan twin, the GPU fixes and the M2 cache table do not |
| 7, audit A1 | not started; the 5 mutants (Leaky, Cosmetic, Oracle, Frozen, Shuffled) are not built |
| 8, the owner's suites (14, 10, 25, 100, 500), E1-E3 | not started; nothing preregistered; `tools\az_suite.c` not written |
| 9, foundation gate and scaling (DDR5 tier, GPU tick, NPU) | not started; needs the owner's yes on the law-2 wording |
| 10, FINAL_REPORT.md | not written |

**Honest limits:**
- **The world exists, but the bodies only partly feel it.** The physics stub ignores gravity, rock and
  density, so bodies can later walk into rock. GRAVITY now changes the state on 24 of 24 seeds, but it
  still shows NO EFFECT (+0.39) on the outcomes.
- **Founder bodies come from the species' base genome.** No ABI call rebuilds a body after the
  `az_genome_fit` step, so a founder's body does not reflect its fitted genome.
- **The gradients fade.** Weather relaxes the depth and latitude gradients over time, and WEATHER lost its
  lesion effect (-0.29) after worldgen.
- **Small world quirks.** Crowds of founders boil a few ocean cells; some worlds have no founder in the
  air.
- **Fixed numbers remain in worldgen:** shape numbers (octave doubling, the linear fade of the rock's
  pull, the pool scale) and the brief's fixed structure (ocean always under air, gravity always -z, the
  guild-to-band map).
- **Most switches show no effect yet.** 14 of 21 lesion switches show no effect at 64 ticks, among them
  memory, plasticity, recurrence, senses, colony and care. Either the stubs mask them or 64 ticks is too
  short. Milestone 8 must tell the two apart.
- **Lineages appear, selection is not shown.** There is no world-level evidence that a trait is selected,
  and none of open-ended growth.
- **Some parts are idle or quiet.** The Philox AVX2 twin is built, but no stage calls bulk Philox. The four Tsotchke-derived kernels do run in the tick, inside tier-1 learning: each creature gets one tier-1 visit every 32 ticks by default. Their lesions change the final state on 24 of 24 seeds, but none shows a significant effect yet on the 64-tick lesion runs. (Corrected 2026-11-18: an earlier version said these kernels were not called by the tick.)
- **Old snapshots and build folders.** Snapshots saved before the worldgen landing no longer load. The
  folders configured from `N:/` (`build\full` and the others) no longer work; `build\z` replaces them.

## 8. Z: only: what changed on 2026-11-18

The owner's rule ("work just in Z Drive not W X Y whatever") is now built in.

**The letters:** N:, T:, V:, W: and S: are gone, and nothing re-creates them.
- The launcher removed the four letters it had mapped, and it now maps none.
- `Start-ApiRun.ps1`, its kickoff and its appended system prompt use the real folders.

**The build:**
- `CMakeLists.txt` globs through `AZ_GLOB_ROOT`, the source path with `[ ]` escaped.
- It passes the NASM include folder as one quoted option.
- It builds `az_live` and its gate `cli_live_smoke`.

**The tools and scripts:**
- `tools\ubsan_check.py` uses `glob.escape`.
- `run_tests.ps1` and `measure.ps1` start programs through .NET.
- `m_check.ps1` and `clang_check.ps1` write logs with `Set-Content -LiteralPath` and default to `build\z`.
- `snapshot.py` finds the root from its own place.

**The docs:**
- `COMMON.md` has a new Paths section with the measured shell rules.
- `CONTINUE-BUILD.md` (section 0), `START-CLI.md` and `BUILD.md` say Z: only.

**Measured on PowerShell 7.6, and now written into the docs:**
- positional paths, `>`/`*>` redirection, `Start-Process` redirection and working folders, and `pwsh -File`
  all fail on the bracketed path;
- `-LiteralPath`, `Set-Content -LiteralPath`, script blocks and .NET processes work.

**Still to do:**
- the text inside `make_status.py`, then regenerating `STATUS_AND_PLAN`;
- the first line of each W2 brief;
- `gpu\README.md`;
- the historical command column of `tests\golden_hashes.txt` and `PROGRESS.md`. These two are history:
  read `N:\` there as the root.

## 9. Money

- **The API run on the 200 USD Ultra monthly API credit:** five `claude -p` invocations of Fable 5.2, from
  2026-11-17 to 2026-11-18. The launcher counted **190.66 USD**.
- **The last invocation:** it cost 8.73 USD. It landed chemistry piece B and recorded the PoC MVP.
- **How it ended:** at 12:04 UTC the API answered "Credit balance is too low". **The credit is used up.**
  Console Billing has the real figure; it may differ from the launcher's count.
- **The final test:** at 14:30 UTC a new launcher sent a one-cent test. It was refused too, and nothing
  changed.
- **What the agent left unspent:** it chose not to start worldgen with the money it believed was left
  (modules cost 8 to 15 USD each). The kickoff rule says never to start what cannot be finished.
- **The desktop session:** its own work (the demo, the Z: migration, this report, and then the worldgen
  landing on the owner's request) ran on the Claude plan (Claude Opus 5.5), not on the API credit.

## 10. What comes next

The hand-off package for the next agent (written for OpenAI GPT Astra 6.2) is `docs\handoff\next-agent\`.
Start with `00-MASTER-PROMPT.md` (the mission to total completion); the package also holds the orientation
sheet `00-PROMPT.md`, the index, stages, pipeline, goals and benchmarks, laws and requirements (01-05), and
the completion program, suites protocol, performance and real-time viewer plan, issue register with
blindspots, and decisions and dilemmas (06-10).

Stage A goes on with **sense** (`docs\agents\W2-BRIEFS\sense.md`, `core\stubs\az_sense.c`), then
**physics** with its NASM twin (it must honour the grav field, rock and density that worldgen now makes),
then **stats**. Then lint warning W1, the wave-2 seam review, and an independent reproduce-first review
of all 13 modules. After that come milestones 6 to 10: the viewer, audit A1, the owner's suites E1-E3, the
foundation gate and scaling, and FINAL_REPORT, in the order and with the gates written in the package.
