# 01 Index: table of contents of the project

> Compiled read-only on 2026-11-18 between 11:30 and 11:40 UTC from the files themselves (every claim
> cites path:line), then corrected by the desktop session for what changed afterwards: chem piece B
> landed, the suite reached 44/44 (then 45 of 45 (32 test programs, 13 gates) in 386 s on build\z), new reference hashes,
> the move to Z: only (no drive letters; `build\z` is the integration build), the live demo (`tools\az_live.c`,
> `demo\`), and, later on 2026-11-18, the worldgen landing (wave 2, module 10, built by the desktop session on the
> Claude plan after the API credit ran out): 46 of 46 (33 test programs, 13 gates) in 253 s on build\z and new
> reference hashes again.
> Line numbers in citations are as of the compile time; files have grown since.


Project root: `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native`. Every relative path below sits under that root. Older docs say `N:\`; that drive letter is gone (Z: only since 2026-11-18), so read `N:\` as this root. Old `V:\` is `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-v40`, old `T:\` is `Z:\[Vibe Coded (AI)]\(Tsotchke)` and old `W:\` is `Z:\[Vibe Coded (AI)]\CLAUDECODE`. Old `S:\` pointed into a C: scratchpad; its copied content is now `build\work\c-scratch-2026-11-17\`.

**Files that do not exist:** there is no `README.md` at the root and no `CHANGELOG` (the demo has `demo\README.md`). `ARCHITECTURE.md` is at `docs\ARCHITECTURE.md`, not at the root. The root holds `AGENTS.md` (since 2026-11-18), `CMakeLists.txt`, `THIRD_PARTY_NOTICES.md`, `.gitignore` and the folders `build core demo docs gpu kernels platform tests tools view`. Nothing is committed to git (`docs\handoff\STATUS_AND_PLAN.md:44,60`).

### 1. Documents

**Root files**
- `AGENTS.md`: created on 2026-11-18 by erratum E-08 (`00-MASTER-PROMPT.md` section 12.2; its text is section 12.3). It holds the laws, the path rules and the session start and end lists, and Codex CLI loads it at every start, so they survive a new session or a compaction.
- `CMakeLists.txt`: the one build file. It lists 22 modules (`CMakeLists.txt:65-66`). For each one it uses `core\az_<m>.c` if that file exists, and the stub otherwise (`:71-82`). The options `AZ_ONLY`, `AZ_TESTS`, `AZ_TOOLS`, `AZ_TWINS` and `AZ_COMPONENTS` give per-owner builds (`:12-19`). It also defines the protocol core set (`:128-148`), the gates as ctest tests (`:266-313`) and the UBSan option (`:48`). Read it before any build change. Since 2026-11-18 its comments give the Z: commands and its globs use `AZ_GLOB_ROOT`, so the bracketed path works; it registers the gates `cli_snap_modes` and `cli_live_smoke`.
- `THIRD_PARTY_NOTICES.md`: MIT provenance for libirrep, spin_nn, QGTL and Moonlab. It says what was taken, from which commit, and which `core\tk_*` and `tests\upstream\*` files derive from it. Read it before touching the tk kernels or the vendored oracles.
- `.gitignore`: 140 bytes; `build\` is ignored.

**docs\ (top level)**
- `docs\ARCHITECTURE.md`: the ABI v4 contract. The code wins if the two disagree (`:1-5`). Sections 4, 6 and 10-12 are the binding contracts (`docs\handoff\CONTINUE-BUILD.md:12`). Read it before writing any core module.
- `docs\GROKBOT-FACTCHECK.md` (75 KB): verdicts on the two GrokBot pastes, 77 claims in all. It records that the no-OS/UEFI direction was withdrawn by rule L6. Read it only for a rule question.
- `docs\TRIGGER-REGISTER.md`: the deferred capabilities. For each one it gives the seam that exists, the measurable trigger, the proof needed and the owner decision needed (TR-01 to TR-18 or more). Read it before any scaling or performance work.

**docs\handoff\ (start here)**
- `next-agent\`: this hand-off package. `00-MASTER-PROMPT.md` first (the mission to total completion); `00-PROMPT.md` is the orientation sheet; `01`-`05` are this index, the stages, the pipeline, the goals and the laws; `06-COMPLETION-PROGRAM.md` (milestones 5 to 10 as work packages), `07-SUITES-AND-BENCHMARKS-PROTOCOL.md` (the owner's suites to completion), `08-PERFORMANCE-AND-REALTIME-VIEWER.md` (pictures and ticks versus frames per second, the 60 FPS viewer, engine speed), `09-ISSUES-GAPS-BLINDSPOTS.md` (issue register, blindspots, closure), `10-DECISIONS-AND-DILEMMAS.md` (owner decisions, dilemmas, decision log).
- `build\handoff\` (generated, not a source): `asmodensius-next-agent-<date>.zip` and `ASMODENSIUS-HANDOFF-ALL-IN-ONE.md`, written by `build\work\scripts\make_handoff_bundle.py` (refuses to write if a file looks like it holds a key).
- `POC-MVP-REPORT.md`: what is built, working and verified on 2026-11-18.
- `CONTINUE-BUILD.md`: the directing document for the continuation run. Sections: 0 drives, 1 read order, 2 owner rules, 3 evidence/seeds/hash, 4 mission, 5 money discipline, 6 stop, 7 owner defaults. Read it after this package. Its section 0 says Z: only (2026-11-18) and defines `<root>`, which the rest of the file uses for the root.
- `STATUS_AND_PLAN.md` (about 252 KB) and `.xml`: generated by `build\work\scripts\make_status.py`. Do not hand-edit them.
  - section 0 and section 0a: state (`:13-69`)
  - section 1: catalog with VERIFIED/STUB/NOT WIRED labels (`:70-117`)
  - section 2: prompts, labelled (`:118-158`)
  - section 3: verified numbers
  - section 4: requirement sets (`:187-226`)
  - section 5: research (`:227-237`)
  - section 6: wave 1
  - section 7: review findings, looked up by id
  - section 8: milestone plan table (`:665-699`)
  - section 9: limits
  - section 10: self-check commands (`:709-731`)
- `PROMPTS.md` and `.xml`: every owner prompt word for word, P01 to P55 (P44 to P46 and P52 to P55 are older than P43 but were found later, so they carry later numbers; P51 is the newest; in P52 an API key and its key ID are markers); their words are in `docs\requirements\OWNER-WORDS.md` (section 5 for P43 to P55). Only the owner's typed words direct the work; the pastes inside P01 and P07 are exhibits.
- `START-CLI.md`: how to start the CLI build. It has a billing correction pointing to `build\work\api-run\Start-ApiRun.ps1`.

**docs\agents\ (module-owner briefs)**
- `COMMON.md`: the common brief, with laws 1-10. Every agent reads it first.
- `BUILD.md`: owner build brief. Stale: its header still says ABI version 3.
- `ABI-V3-NOTICE.md`: history only (the v2 to v3 change).
- `ABI-V4-NOTICE.md` (27 KB): the wave-2 freeze. section 5 holds the "nothing fixed" rules, and section 5.7 explains the speed cost. Read it before any module work.
- `W2-BRIEFS\README.md`: rules 13-20 for wave-2 owners.
- One brief per module in `W2-BRIEFS\`: `life`, `mate`, `tier1`, `metab`, `social`, `econ`, `colony`, `weather`, `chem`, `worldgen`, `sense`, `physics`, `stats` (`.md`). Each starts with a progress-file sentence pointing to `build\work\w2\<m>\PROGRESS.md`. Of those, only `w2\chem`, `w2\colony`, `w2\weather` and `w2\worldgen` exist as folders.
  - Briefs for unbuilt work: `sense.md`, `physics.md`, `stats.md`. (`worldgen.md` and the piece-B section of `chem.md` are built, both on 2026-11-18.)

**docs\requirements\ (38 files, harvested, never run against the engine)** (`STATUS_AND_PLAN.md:189,105`)
- Consciousness indicators (indicator-only):
  - `butlin14.json` (14) with its guide `butlin.md`
  - `consciousness10.json` (10 theories)
- CQM sets:
  - `cqm25.json`, `cqm25_framework.json`, `cqm25_rules.json` (19), `cqm_scrutiny25.json`, with the guide `cqm25.md`
  - `apex25.json` (25)
- Gates and mutants:
  - `antilarp13.json` and `antilarp13_v40impl.json` (13 release gates)
  - `antilarp_mutants5.json` (Leaky, Cosmetic, Oracle, Frozen, Shuffled)
  - `gates9.json` (the v40 9-gate pipeline)
- The 100s:
  - `passes100.json`, `passes100_suite.json`, `passes100_v40.json`
  - `benchmarks100.json` (109)
  - `bench_life100.json` (500)
  - `bench_novel100.json`, `bench_foundations.json` (99), `bench_freak100.json`
  - `bench_budgets.json` (20)
  - guide: `passes-bench.md`
- The 500 and the cube:
  - `checklist500.json`, `life100_areas.json`, `life100_v40_500.json`
  - `cube1000.json` (10x10x10)
  - guide: `checklist500.md`
- Laws: `laws_and_errata.json` (333), with the guide `errata-laws.md`
- Wishlist: `wishlist.json` (512), with the guide `wishlist-pdfs.md`
- Others: `nested_precision.json` (134), `frontier_upgrade_map.json` (272)
- The thousand reports: `thousand_reports.json` (150), `thousand_reports_index.json` (900), with the guide `thousand-reports.md`
- `checklists_index.json` (42): the index of every list found.
- `OWNER-WORDS.md`: every quality word of the owner's prompts (its header gives the range it covers) mapped to the place that carries it ("Rule 0"). Read it before any design decision.

Read these at milestone 8 (E2) only (`CONTINUE-BUILD.md:15`).

**docs\research\ (done, reference only)**
- `factcheck\`: six notes (`asm_kernel`, `baremetal`, `below_asm`, `cpu_memory`, `cuda`, `gpu`), the exhibits `grokbot_message_1/2.md`, and `grok_step_alife_avx2.asm` with its fix `step_alife_avx2_fixed.asm`.
- `platform\`:
  - `win_arena.md`, `nasm_vnni.md`, `cuda_ptx.md`
  - `uefi.md` (dropped by rule L6)
  - working samples in `samples\` (`cuda-ptx`, `nasm-vnni` with llvm-mca port reports, `win-arena`). These include prebuilt `.exe` files.
- `review\`: four contract reviews (`cache_perf`, `determinism`, `feature_fit`, `portability`). They led to ABI 3.
- `sota\`: `ga_indicators`, `game_econ`, `massive_alife`, `world_models` (web research, nothing run).
- `tsotchke\`: 18 audits covering all 27 repositories, each with a verdict.
- `v40\`: `creature001`, `determinism`, `dome`, `gates`. These describe the TypeScript lab that this engine rebuilds.

### 2. Source folders

Status key:
- REAL: built and tested in the suite
- STUB: reference semantics only, and linked in place of a missing module
- UNUSED-STUB: kept beside a real module, which overrides it

**core\ (freestanding C23, integers only, no libc)**
- Lead files (always built, `CMakeLists.txt:68`):
  - `az_world.c`: arena, world creation, column registry, hashing, audits. REAL.
  - `az_tick.c`: the tick pipeline. REAL.
  - `az_store.c`: the variable-size creature store. REAL.
- Contract headers and tables:
  - `az_abi.h` (109 KB, ABI v4)
  - `az_types.h`
  - `az_columns.def`: per-entity columns
  - `az_cells.def`: per-cell fields
  - `az_owners.def`: which module reads each trait and honours each lesion
  - `az_params.def`: 11 engine parameters
  - Only the lead edits these, between workflows.
- Real foundation modules, all REAL:
  - `az_rng.c` (Philox4x32-10)
  - `az_fx.c` (fixed point and rotors)
  - `az_grid.c` (binning, free list, reorder)
  - `az_scan.c` (27-cell neighbour scan)
  - `az_field.c` (grant and commit)
  - `az_genome.c` (variable diploid genome)
  - `az_brain.c` (per-creature sparse int8 brain)
  - `az_ledger.c` (canonical event chain)
  - `az_snapshot.c` (exact save and restore)
  - The catalog labels most of them "TESTS PASS, OWNER DID NOT REPORT" (`STATUS_AND_PLAN.md:72-80`).
- Wave-2 modules, 10 of 13, all REAL. The first nine were checked only by their author; worldgen had three adversarial reviews and a fixer inside its own workflow (`build\work\PROGRESS.md:1552-1556`). The wave-2 seam review and an independent review of all 13 modules never ran (`STATUS_AND_PLAN.md:81`; PROGRESS.md:1610-1611):
  - `az_life.c` (stages, broods, births, deaths)
  - `az_mate.c` (mate choice)
  - `az_tier1.c` (learning, memory, mood)
  - `az_metab.c` (feeding, respiration, growth)
  - `az_social.c` (attack, steal, share, groom, teach, recruit, signals)
  - `az_econ.c` (pantry and barter)
  - `az_colony.c` (colony as an organism)
  - `az_weather.c` (two suns, light, heat, wind, storms, vents)
  - `az_chem.c` (reactions, burial, phases, the medium; since 2026-11-18 also piece B: diffusion by phase, settling and rising along the cell's gravity, heat carried with matter, AZ_LESION_GRAVITY honoured)
  - `az_worldgen.c` (since 2026-11-18, the alien world: terrain from integer value noise over bedrock; a solvent ocean under carrier gas, sea level a parameter; 1 to 64 vents on the sea floor with their buried reservoir; toxin hot spots and reefs; temperature by depth, latitude and the nearest vent plume; two suns with coprime days, defaults 2000 and 3101 ticks; the `grav` field that bends (a downward pull, a lateral pull toward high terrain that fades with height, and a low-order swirl; the scalar gravity is its mean); 1 to 100 founder species, each with a niche (5 guilds, 5 habitat bands, all 8 mating modes once there are 8 or more species), one `az_genome_fit` prototype per species, founders varied on 3 loci and placed in habitat patches, never in bedrock. Every draw is on the world key, so wild worlds differ and lab seeds repeat. PROGRESS.md:1545-1611)
- Tsotchke-derived kernels, compiled in when present (`CMakeLists.txt:84`):
  - `tk_sh.c/.h` (spherical harmonics), `tk_hop.c/.h` (Hopfield), `tk_qgt.c/.h` (quantum geometric tensor), `tk_qsv.c/.h` (3-qubit state vector)
  - All tested, and all four run in the tick: `core\az_tier1.c` calls them on every Tier 1 visit (each creature once every `tier1_period` ticks, 32 by default):
    - `tk_qsv`: the context runs as a program of gates on three qubits, and a sample picks the lane the creature tries (step 4; lesion TIER1_Q);
    - `tk_hop`: associative memory, recall and store (step 5; lesion MEMORY);
    - `tk_sh`: the shape of the crowd around the creature, pulled into the lane gains by ATTENTION (step 6; lesion TIER1_SH);
    - `tk_qgt`: the Fubini-Study metric that preconditions the learning step (step 8; lesion METRIC).
  - `test_tier1` tests all four lesions, and each gives a different world. On the 64-tick lesion runs (seeds 1-24) each one changes the final state on 24 of 24 seeds but shows NO EFFECT on its primary measure yet.
  - Corrected 2026-11-18: earlier versions of this package and STATUS said NOT WIRED (an independent review pass caught it).
- `core\params\az_<m>.def`: world parameters for each module.
  - Filled for the 10 real wave-2 modules; `az_worldgen.def` holds the 46 WORLDGEN_* parameters (2026-11-18).
  - Placeholders of about 0.5 KB for `physics`, `sense` and `stats`.
- `core\stubs\` has a reference file for all 22 modules. The ones the engine links today, because no `core\az_<m>.c` exists, are:
  - `az_sense.c` STUB: a few interoceptive channels and world-frame directions
  - `az_physics.c` STUB: world-frame thrust and linear drag; no gravity, terrain or medium (so bodies can walk into worldgen's rock)
  - `az_stats.c` STUB: head counts only
- `core\stubs\az_worldgen.c` (a uniform world with random founders) is UNUSED-STUB since 2026-11-18; its old body lives on verbatim as `az_test_uniform` in `tests\az_test.h`, for the checks that pin the uniform world (PROGRESS.md:1562).
- The other 18 files in `core\stubs\` (19 UNUSED-STUB in all, with worldgen) are UNUSED-STUB in the full build but stay the semantic definition:
  - `az_scan.c` and `az_grid.c` are "the definition" that the fast versions must match bit for bit.
  - `az_brain.c` and `az_genome.c` are forwarding notices: asking for the stub gets the real module.
  - `az_snapshot.c` returns 0/NULL.

**kernels\ (dispatch plus NASM twins)**
- `az_kernels.c/.h`: CPUID dispatch. REAL.
- `az_dense.c/.h`: the old dense shared-weight network, kept as a C reference. REAL.
- `az_brain_vnni.asm`: AVX2 and AVX-VNNI brain-step twins. REAL, and the only twin the engine calls.
- `az_dense_vnni.asm`: twin of the dense network. REAL.
- `az_philox_avx2.asm`: REAL and tested, but no engine code calls it (`STATUS_AND_PLAN.md:87`).
- `win64.inc`: macros for the Microsoft x64 calling convention.
- `scan_avx2` and `physics_avx2` are named in CMake (`CMakeLists.txt:151`) but the files do not exist (PLANNED).

**platform\win\ (hosted)**
- `az_host_win.c/.h`: page-locked arena, worker pool with ideal processors, WaitOnAddress, ns clock. REAL; repaired 2026-11-17.
- `az_sink_win.c/.h`: SPSC ledger ring plus writer thread. REAL.

**view\ (hosted C, floats allowed)**
- `az_view.c/.h`: software renderer that reads a `const AzWorld*` between ticks. REAL.
- `az_png.c`: library-free PNG writer. REAL.
- Since 2026-11-18 a live view exists: `tools\az_live.c` (one world, rendered every few ticks into PNG frames, one JSON line per frame, commands on stdin) behind the local web GUI in `demo\`. The native Windows viewer of W3 is still PLANNED (`STATUS_AND_PLAN.md:687`).

**tools\**
- `az_run.c`: the main runner (hashes, audit, `--lab`/`--seed`, `--threads`, `--kernels`, `--ledger`, `--save/--load`). REAL. CMake also builds `az_run_proto` on the protocol core set.
- Other REAL tools:
  - `az_bench.c`: throughput benchmark
  - `az_brainbench.c`: ns per brain step
  - `az_budget.c`: byte budget
  - `az_ledgerdump.c`: ledger inspector
  - `az_lesion.c`: lesion evidence with signed-rank, Holm and d_z
  - `az_rngcheck.c`: RNG sanity check
  - `az_snap.c`: one PNG per world; needs the view component
  - `az_live.c`: the live world behind `demo\` (2026-11-18); needs the view component
- Headers: `az_statsutil.h` (header-only stats kit), `az_tools.h` (strict option parsing).
- Gates, all run by ctest:
  - `lint_core.py`, with `--self-test`
  - `check_core_obj.py`: no floating point, no writable data, only the mem* functions undefined
  - `check_avx_stack.py`: the aligned-AVX stack-move gate (GCC bug 54412)
  - `ubsan_check.py`
- `gen\gen_fx_tables.py` and `gen\gen_sh.py`: table generators.
- `membench\membench.c/.exe`: a memory probe with a prebuilt exe; not built by CMake.
- `tools\az_suite.c` (plan E2) is not written (`STATUS_AND_PLAN.md:92`).

**gpu\ (optional component, own CMakeLists)**
- Files: `az_gpu.c/.h` (CUDA driver-API host via LoadLibrary of nvcuda.dll), `az_gpu_dev.c` (clang to PTX), `az_gpu_ref.c`, `az_gpu_layout.h`, `az_gpu_embed.cmake`, `test_gpu.c`, `az_gpubench.c`, `README.md` (23 KB).
- Status: VERIFIED STANDALONE, NOT IN THE TICK. Philox and the brain step are bit-identical to the CPU (`STATUS_AND_PLAN.md:89`).
- The whole-tick GPU path is PLANNED (S2).

**tests\**
- 33 test programs, one exe and one ctest each: the 32 `tests\test_*.c` files plus `gpu\test_gpu.c`. Beside them: `az_test.h` (since 2026-11-18 it also holds `az_test_uniform`, the old worldgen stub body, used as a fixture by the lifeless benches of weather, chem, life, metab and tier1; no check was removed or loosened, PROGRESS.md:1562-1568) and `golden_hashes.txt`. The golden-hash file records the reference hashes and the cause of every change: latest lab 100k `73f1e9e013f8db50`, 1M `5d9c02d2c7dd5fb1`, engine matrix `24c923001a46b122` (worldgen, 2026-11-18; `tests\golden_hashes.txt:69-71`). The chem piece-B references before them (`2a0607a1d1151263`, `d7297084ce3e97ee`, `dd59809c915fc8ca`) are history.
- Tests by area:
  - core and foundation: `core`, `rng`, `fx`, `grid`, `scan`, `field`, `genome`, `brain`, `dense`, `store`, `snapshot`, `ledger`
  - wave-2 modules: `life`, `mate`, `tier1`, `metab`, `social`, `econ`, `colony`, `weather`, `chem`, `worldgen` (4034 checks)
  - tk kernels: `tk_sh`, `tk_hop`, `tk_qgt`, `tk_qsv`
  - host, protocol and tools: `host_win`, `protocol`, `wild`, `statsutil`, `tools`, `view`
- `ubsan_probe.c`: planted UB violations for the trap build.
- `tests\protocol\az_{social,econ,mate,life}.c`: TEST-ONLY interacting modules, linked only into `test_protocol` and `az_run_proto` (`CMakeLists.txt:128-148`).
- `tests\cli\determinism_matrix.py`, `ledger_resume.py`, `snap_modes.py` and `live_smoke.py`: ctest CLI gates.
- `tests\lint\*\core\fixture.c`: one planted violation per lint rule.
- `tests\upstream\`: byte-identical MIT oracles (libirrep, moonlab, qgtl, spin_nn), with LICENSE and PROVENANCE files.
- `tests\vectors\philox_v40.json`: cross-check vectors from the v40 lab.
- Last full run: 46 of 46 (33 test programs, 13 gates) in 253 s on build\z, 2026-11-18, after the worldgen landing (`build\work\logs\ctest_z_worldgen.txt`). The run before it, at chem piece B, was 45 of 45 (32 test programs, 13 gates) in 386 s.

### 3. build\ and build\work\

**Build folders under build\**
- `z`: the integration build, configured from the real Z: path on 2026-11-18: 19 real modules, 3 stubs (sense, physics, stats), 3 assembly twins; 0 warnings (`build\work\logs\build_z_worldgen.log`).
- `full`: the previous integration build, configured from `N:/`; it no longer works without N:.
- `release`, `skel` (the stubs-only build, for `test_core`), `ubsan` (the trap build), `clang` (the second compiler), `verify`, `lead`, `owners\<key>`.
- `clean-2026-11-17`.
- `dropped-uefi-2026-11-17`: the parked UEFI work, out of scope by rule L6.

**build\work\**
- `PROGRESS.md`: the run log and the authoritative "Next:" line.
  - State before the newest entry: the desktop session's entry "wave 2, module 10, worldgen landed" (2026-11-18, `PROGRESS.md:1545-1611`), after the API run's closing entry "THE STATE THE API RUN ENDS WITH" (invocation 5: chem piece B landed, PoC MVP recorded).
  - Newest entry: "2026-11-18 22:33 UTC, desktop session: the tk-kernels correction and hand-off package v2 (LANDED)" (`:1613-`); its Next: is WP-P0-01, master 9.1; the worldgen Next below is history.
  - Next of the worldgen entry (`:1610-1611`): sense, then physics (with its NASM twin), then stats; lint warning W1; the wave-2 seam review and an independent review of all 13 modules; then milestones 6 to 10.
- `m1\`: milestone 1, the serial review fixes (v01, v04, v09, v60 folders), a batch2 folder, guards/tools after-trees, and the old tree and build used as baselines for failing-then-passing proofs.
- `m2\`: milestone 2, the owners' requests. Holds `cli_ledger_resume`, the v16 and v18 mutant trees with their builds and logs, final ctest, m_check and object-gate logs.
- `m3\`: milestone 3, hardening:
  - `stack`, `su`: frame-limit work
  - `protocol`: the CLI matrices and the seven planted mutants
  - `ubsan`, `clang` (`clang_check.txt`), `lesion`, `matrix`, `verify`
- `m4\`: milestone 4, the ABI v4 freeze plus wave-2 landing:
  - the one-shot patch scripts `pa_*`, `pb_*`, `pc_*`, `pe_*`, `pf_*`, ..., and `q1-q17` for the life to econ landings
  - `verify_m4.ps1` and `m_check_summary.txt`
- `w2\`: wave-2 evidence (about 13,000 files, mostly mutant tree copies):
  - `chem\`, `colony\`, `weather\`: mutant runs, lesion runs, gdb and parallel runs
  - `worldgen\` (2026-11-18): `DESIGN.md` (the design), `tree\` (the staging tree it was built in), `review\` and `fix\` (the three adversarial reviews and the fixer), `mutants1\` and `mutants2\` with `mutants_worldgen.py` (planted defects; `mutants2\summary.txt`: 31 of 31 caught), `diag\`, the build, ctest, run and lesion logs, and its own `PROGRESS.md` (measurements and trade-off tables)
  - `lesion_1_24*.csv` tables and `mutants.py`
  - There are no folders for life, mate, tier1, metab, social or econ; their evidence is in `m4\` and `PROGRESS.md`.
- `fix-majors\`: the 2026-11-17 repair of the six major review findings. Holds before/after/orig trees, mutants, clang output, quiet and loaded pool measurements, `hog.c/.exe` (a CPU load generator), lesion CSVs and the full test logs.
- `guards\`, `tools\`: the wave-1 package workflows of milestone 1 (`guard-check`, `guard-lint`, `guard-tests-a/b`, `guard-view-snap`; `tools-bench`, `tools-check`, `tools-ledger`, `tools-run`, `tools-test`).
- `snapshots\`: 18 full-tree copies (counted 2026-11-18), `2026-11-17-pre-m1` through `2026-11-18-pre-sense-astra`, including `pre-abi-v4`, `pre-nothing-fixed`, `pre-/post-final-mvp`, and `2026-11-18-pre-worldgen` (506 files, 28.7 MB, the recovery copy taken first, PROGRESS.md:1580) and `2026-11-18-post-worldgen`. The newest, `2026-11-18-pre-sense-astra` (517 files, 28.9 MB, created 18:25:32 UTC), is GPT Astra's recovery point: the Codex desktop session took it with `snapshot.py pre-sense-astra`, re-ran the 100k reference and ctest (the two `sense_baseline_*` logs below), and wrote no sense code, so it is a recovery point and a reproduced baseline, not progress. One folder carries a doubled date: `2026-11-18-2026-11-18-post-m4`. Snapshot files (`.azsnap`) saved by the engine before the worldgen landing no longer load, because the parameter table grew.
- `logs\`:
  - `review_result.json` (535 KB): the max-effort review record. Look findings up by id; never read it whole.
  - `REVIEW_BRIEF.md`, `review_detail_2.txt`, `wave1_digest.txt`
  - early full/verify build logs and 100k/1M run logs
  - `demo_owner_world_2026-11-18_f226.json` (saved by the lead from the read of 2026-11-18 19:40:56 UTC): frame 226 of the same world, tick 13,560, 1,000,416 alive, 414.3 ms per tick, 4,261.3 ms per 1600x900 picture, the stage times and the per-frame history from tick 0; the figures most of the package quotes (08 section 2.5)
  - `demo_owner_world_2026-11-18.json`: a read-only capture of the owner's own live demo world (`/api/state`, 2026-11-18 19:42 UTC; run 20261118-103059-46dbdf, wild, cap 1,004,096). Its last frame (228, tick 13,680) holds 1,000,166 alive, 397.6 ms per tick and 4,383.3 ms to draw one 1600x900 picture
  - `sense_baseline_100k.txt`: GPT Astra's re-run of the 100k lab reference (`az_run --lab --n 100000 --ticks 60 --cap 400000 --grid 6,6,5 --threads 8`): final hash `73f1e9e013f8db50`, matter and energy exact
  - `sense_baseline_ctest.txt`: GPT Astra's ctest run on `build\z`: 100% passed, 0 failed out of 46, 238.52 s
- `prompts\`: per-session prompt extracts (`cli-7e5cd444`, `desktop-a1806fab`, and the second desktop extract `desktop-a1806fab-2026-11-18b.md` and `.xml`), the `PROMPTS.*.before*` backups, the merge scripts `add_p38.py` and `merge_p22.py`, `add_p44_p51.py` (the P44-P51 merge) and `verify_p44_p51.py` (its byte-for-byte check; historical: it expects 51 prompts and fails on the 55-prompt files), and `add_p52_p55.py` (the P52-P55 numbering; its byte-for-byte check was not saved to the tree). Kept in snapshots since 2026-11-18 (`snapshot.py` group `prompts`).
- `final\`: the closing evidence of the API run (clean build, ctest, reference worlds, demo command outputs).
- `api-run\`: the `claude -p` loop launcher on the API credit (Z: only since 2026-11-18). The credit is used up: invocation 5 ended at 12:04 UTC with "Credit balance is too low", and a new launcher's one-cent test at 14:30 UTC was refused too. Worldgen was built afterwards by the desktop session on the Claude plan, not on the credit.
  - `Start-ApiRun.ps1` (37 KB), `README.md`, `kickoff.md`
  - `P22.md`, `owner-leave-room.md` (the source of P38-P42)
  - `state.json`, `settings.json`, `system-append.md`, `logs\`
- `show\`: one PNG per milestone, about 2.7 MB each (`00-baseline` to `04-milestone4`, then `05-mvp-wild.png` and `05-mvp-lab.png` of the final build), from `az_snap`. The `.txt` files give the state hash before and after rendering.
- `scripts\`:
  - `make_status.py` (about 122 KB; generates STATUS_AND_PLAN)
  - `spend.py`, `usage_check.py`, `usage_one.py`
  - `run_tests.ps1`, `clang_check.ps1`, `m_check.ps1`, `measure.ps1`, `ab_time.ps1`
  - `snapshot.py`, `inventory.py`, `extract_prompts.py`, the digest scripts, `check_text.py`
  - `make_handoff_bundle.py` (2026-11-18): builds `build\handoff\` (the zip and the all-in-one) from the package; key scan and text check first
- `status_new\`: the generated STATUS copy, compared before it replaces the one in docs.
- `baseline\`: the CONTINUE-BUILD start baseline: `summary.txt`, test, lint and lesion logs, run logs.
- `tests\`, `tests-verify\`: per-test log and err files from `run_tests.ps1`.
- `c-scratch-2026-11-17\`: the relocated scratch that the old docs call `S:\` (`audit`, `factcheck`, `me`, `platform`, `req`, `review`, `sota`, `tsotchke`, `v40`, `work`). Reproduction programs are under `audit\verify\<id>\`.
- `baseline\` and the stray `m3_su_build.log` and `m3_su_cfg.log` at the `build\work` root are small leftovers.

### 4. Reading orders

**A new agent starts with `docs\handoff\next-agent\00-MASTER-PROMPT.md` and this package, then these 5 files, in order:**
1. `docs\handoff\CONTINUE-BUILD.md`: rules, money discipline, owner defaults. Read the drive section with the Z: mapping above.
2. `build\work\PROGRESS.md`: the header lines, then the worldgen entry and the last "Next:" (`:1545-1611`). Then `build\work\w2\worldgen\PROGRESS.md` and `DESIGN.md` as the worked example of how a module is landed now (design, staging tree, adversarial reviews, fixer, landing).
3. `docs\handoff\STATUS_AND_PLAN.md`, section 0/0a, section 1, section 8 and section 10 only (`:13-99`, `:655-717`). It is generated and lags behind the newest work; PROGRESS.md wins where they differ.
4. `docs\agents\COMMON.md`, then `docs\agents\ABI-V4-NOTICE.md` (section 5).
5. The brief for the next module, `docs\agents\W2-BRIEFS\sense.md` (then `physics.md` and `stats.md`), with `docs\ARCHITECTURE.md` section 4, 6 and 10-12 as the contract.

Also read `docs\requirements\OWNER-WORDS.md` before any design choice, and `docs\handoff\PROMPTS.md` for the owner's exact words.

**A reviewer starts here:**
1. `docs\handoff\STATUS_AND_PLAN.md` section 1 and section 7 (findings by id) and section 10, the commands to reproduce.
2. `CMakeLists.txt`, for what is real and what is stub, and which gates exist.
3. `core\az_abi.h` and `docs\ARCHITECTURE.md`, the contract.
4. The wave-2 modules that only their author has checked, `core\az_{life,mate,tier1,metab,social,econ,colony,weather,chem}.c`, and `core\az_worldgen.c` (reviewed only inside its own workflow), each with its `tests\test_<m>.c` and the evidence in `build\work\w2\` and `build\work\m4\`.
5. `tests\golden_hashes.txt`, `tests\cli\determinism_matrix.py` and `tests\protocol\`.
6. `docs\requirements\antilarp_mutants5.json`, the five negative mutants that audit A1 must catch.
7. `build\work\logs\review_result.json`, by id only.
### 5. The demo
- `demo\live_server.py`: local web server (Python standard library, 127.0.0.1 only) over `az_live`, `az_run` and `ctest`.
- `demo\live.html`: the page. `demo\Start-Demo.cmd`: double-click launcher; the page is at http://127.0.0.1:8765/. `demo\README.md`: how to use it.
- It runs the worldgen world on `build\z`, but the picture draws bodies only (no terrain, ocean, vents or suns yet; 08 RV-A adds them); its About tab already says worldgen is real. At 1M creatures one 1600x900 picture takes about 4.3 s and the world stands still while it is drawn (`build\work\logs\demo_owner_world_2026-11-18_f226.json`; 08 section 2.5).
- `build\demo\`: frames, saved worlds (`worlds\*.azsnap`) and logs (gitignored).
