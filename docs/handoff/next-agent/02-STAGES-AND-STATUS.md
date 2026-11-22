# 02 Stages and status

> Compiled read-only on 2026-11-18 between 11:30 and 11:40 UTC from the files themselves (every claim
> cites path:line), then corrected by the desktop session for what changed afterwards: chem piece B
> landed, the suite reached 44/44 (then 45 of 45 (32 test programs, 13 gates) in 386 s on build\z), new reference hashes,
> the move to Z: only (no drive letters; `build\z` is the integration build), the live demo (`tools\az_live.c`,
> `demo\`), and, later on 2026-11-18, the worldgen landing (wave 2, module 10, built by the desktop session on the
> Claude plan after the API credit ran out): 46 of 46 (33 test programs, 13 gates) in 253 s on build\z and new
> reference hashes again.
> Line numbers in citations are as of the compile time; files have grown since.

## The 10-milestone plan and its status

Sources: CONTINUE-BUILD.md:45-61 and STATUS_AND_PLAN.md:665-699 (section 8).

| M | Goal | Status (PROGRESS.md) |
|---|---|---|
| 1 F1 | First-order findings: v01, v04, v60, v07, v40, v44, v45, v42, v11, v09, v43/v10; v76/v77 reworded; tools v34/v56/v54/s4/v53/s2; guards v33/v32/v30/v52/v58 | **Done** 2026-11-17 ~21:20 UTC (:98-105). 19/19 tests. |
| 2 T5 | Owners' requests: philox C default, `--ledger`, sink_ctx, one ledger fold, test_core quick mode, libirrep path, THIRD_PARTY_NOTICES, v15/v73/v51/v18/v16 | **Done** 21:45 UTC, hash unchanged (:123-132) |
| 3 T6 | test_protocol, UBSan trap build, undefined-symbol gate, `-Wstack-usage=2048` | **Done** 2026-11-18 00:46 UTC (:191-198). 20/20 tests, 9/9 gates, clang equals gcc, 7/7 mutants caught. |
| 4 V4 | ABI v4 freeze, reworked for "nothing fixed" and wild/lab | **Done** (:404-459). 32/32 ctest; fixed record 140->124 B; lab hashes a40f5c14a80a1835 (100k) and c47f034602c1a53d (1M). STATUS_AND_PLAN.md:685 shows V4 "done" since the regeneration of 2026-11-18 (it said "open" before). |
| 5 W2 | Replace the 13 stubs | **In progress: 10 of 13 real (life, mate, tier1, metab, social, econ, colony, weather, chem, worldgen); still reference stubs: sense, physics, stats.** Worldgen landed 2026-11-18 (PROGRESS.md:1545-1611), built by the desktop session on the Claude plan through workflows: design (`build\work\w2\worldgen\DESIGN.md`), a staging tree, three adversarial reviews (all said "do not ship"), a fixer that reproduced and fixed every real finding, then the landing. test_worldgen 4034 checks; planted defects 31/31 caught; 46 WORLDGEN_* parameters. Build `build\z`: 19 real modules, 3 stubs, 3 assembly twins, 0 warnings. Suite 46/46 (33 programs, 13 gates) in 253 s. Lint 0 errors, 3 warnings (unchanged); check_text passed on the files that landing changed (`--since`); the whole root has 83 files with violations and 2 exempt (351 checked, 2026-11-18), the 83 all in docs\research and docs\requirements, the exempt two PROMPTS.md/.xml (the owner's words byte for byte, listed by name in check_text.py), none in the package (09 DOC-01/DOC-02). Tuning-seed lesion table (seeds 1-24, exploratory): 7 EFFECT, 14 NO EFFECT, 0 INERT (GRAVITY now changes the state on 24 of 24 seeds; WEATHER lost its effect). Current lab hashes: 100k 73f1e9e013f8db50, 1M 5d9c02d2c7dd5fb1, matrix 24c923001a46b122. Missing: sense, physics (with NASM twin), stats/classification, the cross-module seam review, and an independent review of all 13 modules (the first nine were checked only by their author; worldgen only inside its own workflow). Known worldgen limits: founder bodies come from the species' base genome (no ABI call rebuilds a body after the fit); the physics stub ignores gravity, rock and density, so bodies can walk into rock; weather relaxes the depth and latitude gradients; crowds of founders boil a few ocean cells; some worlds have no founder in the air. No world-level selection result yet (STATUS:59). History, chem piece B (earlier on 2026-11-18): 579 checks, 27/27 planted defects caught, suite 44/44. Before piece B: Landed: life (:461), mate (:512), tier1 (:555), metab (:606), social (:701), econ (:796), colony (:951), weather (:1076), chem piece A (:1197). Not built: chem piece B (diffusion, heat moving with matter, settling, AZ_LESION_GRAVITY; design at :1261-1283), worldgen, sense, physics (with NASM twin), stats/classification. Also missing: the cross-module seam review, and any review of the 9 modules by anyone but their author (:1256-1259). Suite 43/43 (32 programs, 11 gates). Lint 0 errors, 3 warnings (9 of 112 traits unread; GRAVITY not honoured; 4 lesions untested) (:1151). Tuning-seed lesion table (seeds 1-24, exploratory): 6 EFFECT, 14 NO EFFECT, 1 INERT (GRAVITY) (:1235-1241). Lab hashes then: 100k 854f23b5894d5130, 1M 28dd24b652597da9, matrix 0ff6b6aaef26b063 (:1228-1230). No world-level selection result yet (STATUS:59). |
| 6 W3 + M | Scan asm twin; Windows viewer; GPU fixes v23/v26/v57/v25; M1 note, M2 cache table, M3 speed work | Open. A live view exists since 2026-11-18 (`tools\az_live.c` and the web GUI in `demo\`); the native Windows viewer is not built. |
| 7 A1 | Full P09 audit plus the 5 mutants (Leaky, Cosmetic, Oracle, Frozen, Shuffled) | Open |
| 8 E1-E3 | "The 25 14 100 10" all over (CONTINUE-BUILD:54-59). E1: az_lesion on 5101-5124 plus 5001-5024. E2: trace matrix covering 25 = CQM-25, 14 = Butlin, 100 = passes100 + life100 areas + benchmarks100 (109 items), 10 = consciousness10, 500 = checklist500, 13 antilarp gates, gates9, laws_and_errata. E3: az_bench from 1 to 1M creatures. Stats rule: Wilcoxon, \|d_z\| >= 0.8, Holm p < 0.05; preregistration in `docs\prereg\`. | Open; nothing has been run against this engine (STATUS:60) |
| 9 G1 -> S1-S4, R2 | Foundation gate, then DDR5 tier, GPU whole tick, NPU probe, placement scheduler | Open; also needs the owner's yes on the law-2 wording |
| 10 R1 | FINAL_REPORT.md | Open |

**Next step** (PROGRESS.md:1610-1611, the worldgen entry's Next:; the last Next: is WP-P0-01, master 9.1): **sense first** (`docs\agents\W2-BRIEFS\sense.md`), then physics (with its NASM twin; the stub it replaces ignores gravity, rock and density, which is why bodies can walk into worldgen's rock), then stats; the lint warning W1 (BUILD, TRUST, VAL_2 and VAL_3 in az_econ.c); the wave-2 seam review and an independent review of all 13 modules; then milestones 6 to 10 (viewer, audit A1, the owner's suites E1-E3, the foundation gate and scaling, FINAL_REPORT). Worldgen is done; an earlier `Next:` line that names it first is history.

**Mode of work now.** The 200 USD API credit is used up (05, section 4). The owner asked the desktop session (Claude Opus 5.5 on the Claude plan) to finish to a safe stage and package this hand-off for OpenAI GPT Astra 6.2. The worldgen landing is the last work done before that hand-off; nothing is committed to git (PROGRESS.md:1491). The newest snapshot, `build\work\snapshots\2026-11-18-pre-sense-astra` (517 files, 28.9 MB, 18:25:32 UTC), was taken by the GPT Astra session in the Codex desktop app, which then re-ran the 100k reference (`73f1e9e013f8db50`, books exact, `build\work\logs\sense_baseline_100k.txt`) and ctest (46 of 46 in 238.52 s, `sense_baseline_ctest.txt`) and wrote no sense code: it is a recovery point and a reproduced baseline, not progress.

## Decisions waiting for the owner, with their defaults

Sources: CONTINUE-BUILD.md:83-87 and STATUS_AND_PLAN.md:673 (D1), :675 (D2), :692 (G1).
- **D1(a) first git commit.** Default: none, keep snapshot copies. GATE-01 of antilarp13 stays blocked until it happens. Commits go straight to main; no PR, no push.
- **D1(b) licence.** Default: none (notices file only).
- **D1(c) "Lock pages in memory" for 2 MB pages.** Default: VirtualLock without large pages.
- **D1(d) installs** (CUDA toolkit with ptxas/nvdisasm, Windows Performance Analyzer, OpenVINO). Default: nothing installed; list what is missing.
- **D2:**
  - law-2 wording for phase 2 (default: phase 1 holds; M9 waits);
  - NPU path (default: probe only; no NPU result in world state);
  - quiet-machine window for M2/E3 (default: measure and label the load);
  - sex locus feeding traits, v09 (default: marker loci never feed traits);
  - unused columns v59/v63 (default: decided in V4);
  - 64 paired seeds for checklist500 (default: the 48 preregistered seeds, deviation recorded);
  - delete `build\dropped-uefi-2026-11-17` (default: leave it);
  - W2 in parallel or serial (default: workflows of at most 5 agents, landed one module at a time; in practice the lead built the first nine serially for cost, PROGRESS:452-457, and worldgen through a workflow of design, staging tree, three reviews and a fixer, PROGRESS:1551-1556).
- **G1 gate:** needs M1-M8 done, no stubs, A1 clean, the invariants, M2 and E3, and the owner's yes on law 2.
- **Others:**
  - drive letters: settled on 2026-11-18. The newest request ("work just in Z Drive") wins over P32; the letters are gone and nothing may create them;
  - the memory note `asmodensius-native.md` was not updated by the API run, which was forbidden to touch C: (PROGRESS:146);
  - COMMON law 6 and the ARCHITECTURE "Deterministic" row still need rewording to match wild mode.
  - Money: the 200 USD API credit is used up (invocation 5 refused at 12:04 UTC, a one-cent test refused at 14:30 UTC). Any further paid run needs the owner; Console Billing has the real figure.
  - Worldgen's fixed structure, recorded as a limit rather than decided: the brief's layout (ocean always under air, gravity always -z, the guild-to-band map) and a few fixed shape numbers (octave doubling, the linear fade of the rock's pull, the pool scale). Read against P39-P42 ("nothing fixed"), these are candidates for world parameters; no owner decision exists yet (PROGRESS.md:1607-1609).
  - Snapshots saved before the worldgen landing no longer load (the parameter table grew; `tests\golden_hashes.txt:69`). The demo's saved worlds (`build\demo\worlds\*.azsnap`) are snapshots too.
