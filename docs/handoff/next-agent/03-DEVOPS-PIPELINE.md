# 03 DevOps and pipeline

> Compiled read-only on 2026-11-18 between 11:30 and 11:40 UTC from the files themselves (every claim
> cites path:line), then corrected by the desktop session for what changed afterwards: chem piece B
> landed, the suite reached 44/44 (then 45 of 45 (32 test programs, 13 gates) in 386 s on build\z), new reference hashes,
> the move to Z: only (no drive letters; `build\z` is the integration build), the live demo (`tools\az_live.c`,
> `demo\`), and, later on 2026-11-18, the worldgen landing (wave 2, module 10, built by the desktop session on the
> Claude plan after the API credit ran out): 46 of 46 (33 test programs, 13 gates) in 253 s on build\z and new
> reference hashes again.
> Line numbers in citations are as of the compile time; files have grown since.


Project root, written as `$A` below: `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native`. Everything here comes from reading files only. Nothing was built or run for this section.

**Live state at 11:28-11:36 UTC (history: invocation 5 ended at 12:04 UTC, see section 4).** Invocation 5 of the API run was then active. Its log is `$A\build\work\api-run\logs\run-05-20261118-032845.jsonl`, and the last lines of `launcher.log` show it working. Since my first read it has done four things:
- rewrote `$A\tools\az_snap.c` (11:30 UTC);
- added `$A\tests\cli\snap_modes.py` and a 12th gate, `cli_snap_modes`, at `$A\CMakeLists.txt:284-289`. Every CMakeLists line after 283 has moved down by 6.
- made a new clean build folder, `$A\build\clean-2026-11-18`;
- started a 44-test ctest run. `$A\build\work\final\ctest_final.txt` was at 7/44 when I last read it.

### 1. Toolchain (as recorded in the CMake caches and `docs\agents\COMMON.md:43-67`)

| Tool | Version | Where | Evidence and notes |
|---|---|---|---|
| gcc (WinLibs mingw-w64, UCRT, posix threads) | 16.1.0 | `C:\Users\Alexa\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\gcc.exe` | `$A\build\full\CMakeCache.txt` (CMAKE_C_COMPILER); `$A\build\full\CMakeFiles\4.3.3\CMakeCCompiler.cmake` (VERSION 16.1.0). Main compiler of every build. |
| NASM | 2.16.03 | same WinLibs `bin\nasm.exe` | COMMON.md:52; CMakeCache CMAKE_ASM_NASM_COMPILER. Output format win64 (CMakeLists.txt:165). Of the 5 twin names (CMakeLists.txt:151), only 3 `.asm` files exist: `az_brain_vnni`, `az_dense_vnni`, `az_philox_avx2`. `scan_avx2` and `physics_avx2` are skipped because their files are missing (CMakeLists.txt:157). |
| clang (+ lld-link, llvm-mca, llvm-objdump) | 21.1.8 | `Z:\Tools\LLVM-21\bin\clang.exe`, not on PATH | Used for 2 jobs: the second-compiler build (`clang_check.ps1:15-16`, target x86_64-w64-windows-gnu, `-lpthread`, `AZ_COMPONENTS=view`) and the GPU device code (`gpu\CMakeLists.txt:18,70`). Version is in `$A\build\clang\CMakeFiles\*\CMakeCCompiler.cmake`. |
| MSVC (`cl`, `ml64`) | 14.44 | `Z:\Tools\VS2022BuildTools` (vcvars64.bat) | COMMON.md:58-59. Installed but **no build file uses it**: the flags are GNU-style (`-ffreestanding`, `-Wa,...`). |
| CUDA / PTX | no toolkit (no nvcc, ptxas or nvrtc) | header `Z:\Tools\cuda-12.9-redist\include\cuda.h`; driver `C:\Windows\System32\nvcuda.dll` | COMMON.md:60-62. Device code is built with clang `--target=nvptx64-nvidia-cuda -march=sm_80` into PTX text (`gpu\CMakeLists.txt:20,68-79`). `gpu\az_gpu_embed.cmake:16-32` checks the PTX (entry points present, no extern function, no floating point) and embeds it as C bytes. `gpu\az_gpu.c:3,249-251` loads nvcuda.dll at run time with LoadLibrary. The build does not use the redist header. The GPU runs only the Philox and dense-brain batches; no part of the world tick runs there (ARCHITECTURE.md:79). |
| CMake | 4.3.3 (minimum 3.20) | `C:\Program Files\CMake\bin\cmake.exe` | CMakeCache CMAKE_COMMAND; CMakeLists.txt:1 |
| Ninja | version not recorded | `C:\Users\Alexa\AppData\Local\Programs\Python\Python39\Scripts\ninja.exe` (pip package) | CMakeCache CMAKE_MAKE_PROGRAM |
| Python | 3.9.13 | `C:\Users\Alexa\AppData\Local\Programs\Python\Python39\python.exe` | AZ_PYTHON in the caches; `find_program` at CMakeLists.txt:268; version from COMMON.md:53. Python 3.12 also exists (the cache of `build\clean-2026-11-17`). Python is allowed for tooling only. |
| binutils (objdump, nm) | from PATH (WinLibs) | - | `tools\check_core_obj.py:16`, `tools\check_avx_stack.py:55` |
| PowerShell 7 (pwsh) | version not recorded | - | Required: under 5.1, `run_tests.ps1` loses the exit codes (STATUS_AND_PLAN.md:718), and `Start-ApiRun.ps1:42` refuses to run below 7. |

The whole toolchain lives on C:. The "nothing on C:" rule (`docs\handoff\CONTINUE-BUILD.md:23`) covers outputs only.

**What is not there:**
- no `CMakePresets.json`;
- no CI;
- `.git` exists, but nothing has ever been committed (STATUS_AND_PLAN.md:44,60);
- no README at the project root.

**Build model** (`$A\CMakeLists.txt`):
- Language and target: C23 with GNU extensions (:22-24), Release by default (:25-27), `-march=x86-64-v3` (:34).
- Owner-select options, each ALL, NONE or a list: AZ_ONLY, AZ_TESTS, AZ_TOOLS, AZ_TWINS, AZ_COMPONENTS (:29-33). A module not selected is built from its stub in `core\stubs\` (:71-82).
- Core flags: `-ffreestanding -fno-strict-aliasing -fwrapv -O2`, plus `-Werror=stack-usage=2048` with gcc or `-Wframe-larger-than=2048` with clang (:116-123).
- Tests and tools: `-Wa,-muse-unaligned-vector-move` on gcc for Windows (:40-43, :205); linked `-static` with gdi32, user32, psapi, synchronization and bcrypt (:180, :206-207).
- `az_core_proto` replaces social, econ, mate and life with the test-only modules in `tests\protocol\` (:135-148). It is linked only into `test_protocol` and `az_run_proto`.
- The GPU component is a subdirectory (:330 after the shift; :326 when first read).

**Build folders:**

| Folder | What it is |
|---|---|
| `$A\build\z` | the integration build since 2026-11-18, AZ_*=ALL, configured from the real Z: path |
| `$A\build\full` | the lead's old integration build, AZ_*=ALL; bound to N:/, so it no longer works |
| `$A\build\skel` | stubs only: AZ_ONLY=NONE, tests `core`, tools `run` |
| `$A\build\ubsan` | manual trap build (AZ_UBSAN=ON) |
| `$A\build\full\ubsan` | trap build made by ctest |
| `$A\build\clang` | second-compiler build |
| `$A\build\clean-2026-11-17`, `clean-2026-11-18` | clean-build checks |
| `lead`, `verify`, `release` | older; `release` has stale options (AZ_ASM, an empty AZ_ONLY) |
| `owners\*` | wave-1 owner builds |
| `dropped-uefi-2026-11-17` | parked UEFI work, not built |

### 2. Commands

**Verified 2026-11-18 from the real Z: path**, twice. First after the fixes in section 6a: `cmake -S "$A" -B "$A\build\z" -G Ninja` configured in 2 s (18 real modules, 4 stubs, 3 assembly twins), `cmake --build "$A\build\z"` built 166 steps with 0 warnings, and ctest gave 45 of 45 (32 test programs, 13 gates) in 386 s. Then at the worldgen landing (the current state): the same configure gives 19 real modules (worldgen added), 3 stubs (sense, physics, stats) and 3 assembly twins; the build has 0 warnings (`$A\build\work\logs\build_z_worldgen.log`); ctest gives 46 of 46 (33 test programs, 13 gates) in 253 s (`$A\build\work\logs\ctest_z_worldgen.txt`); lint 0 errors, 3 warnings (unchanged); check_text passed on the files that landing changed (`--since`), while the whole root has 83 files with violations and 2 exempt (351 checked, 2026-11-18), the 83 all in docs\research and docs\requirements, the exempt two PROMPTS.md/.xml (the owner's words byte for byte, listed by name in check_text.py), none in the package (09 DOC-01/DOC-02). No junction or drive letter is used or allowed.

`$A` must be a real Z: path. An existing build folder cannot be pointed at a different source path, so configure into new folders.

| Step | Command (PowerShell 7; `$A` as above) | Last recorded result |
|---|---|---|
| Configure (integration build) | `cmake -S "$A" -B "$A\build\z" -G Ninja` (the old `build\full` is bound to N:/) | done 2026-11-18: 19 real modules, 3 stubs, 3 twins; 0 warnings (build_z_worldgen.log) |
| Configure (owner build) | `cmake -S "$A" -B "$A\build\owners\<key>" -G Ninja -DAZ_ONLY="..." -DAZ_TESTS="core;..." -DAZ_TOOLS="run;..." -DAZ_TWINS="..." -DAZ_COMPONENTS="..."` | BUILD.md:27-39 |
| Build | `cmake --build "$A\build\z"` (clean build: `--clean-first`, or a new `build\clean-<date>` folder, CONTINUE-BUILD:74) | - |
| All test programs at once | `& ([scriptblock]::Create([IO.File]::ReadAllText("$A\build\work\scripts\run_tests.ps1"))) -BuildDir "$A\build\z" -LogDir "$A\build\work\tests"`. Use this form because `pwsh -File` fails on bracketed paths (api-run README.md:31). The script starts every `test_*.exe` in parallel and kills each after 900 s (run_tests.ps1:1-21). | Last recorded through this script: 32 programs, all exit 0 (PROGRESS.md:1227, before worldgen). Since then all 33 programs pass inside ctest (ctest_z_worldgen.txt). |
| Every ctest item | `ctest --test-dir "$A\build\z" --output-on-failure -j 6` | 46 of 46 (33 test programs, 13 gates) in 253 s on build\z, 2026-11-18, after the worldgen landing (`$A\build\work\logs\ctest_z_worldgen.txt`). Before: 45 of 45 in 386 s (`ctest_z.txt`). |
| Gates only | `ctest --test-dir "$A\build\z" -E "^test_"` | 13 gates (cli_snap_modes and cli_live_smoke were added on 2026-11-18), all passing in ctest_z_worldgen.txt. STATUS section 10 (regenerated 2026-11-18) agrees. |
| Trap (UBSan) build | `python -I "$A\tools\ubsan_check.py" "$A" "$A\build\ubsan"` (options `--limit N`, `--no-build`). Inside ctest it is `ubsan_trap_build`, which writes into `<build>\ubsan`. | ubsan_trap_build passed, 252.5 s (ctest_z_worldgen.txt); STATUS section 10 agrees. |
| Second compiler | `& ([scriptblock]::Create([IO.File]::ReadAllText("$A\build\work\scripts\clang_check.ps1"))) -Out "$A\build\work\clang_check" [-SkipMillion]`. Since 2026-11-18 it uses Z: paths (`build\clang-z` against `build\z`) and writes its logs through `Set-Content -LiteralPath`; not run since that edit. | Equal hashes and 19/19 clang-built programs pass (STATUS:728, before wave 2). Not re-run after wave 2 (PROGRESS.md:1256). |
| lint_core | `python -I "$A\tools\lint_core.py" "$A"` (also `--self-test`, and `--strict` for the trait and lesion gate, ARCHITECTURE.md:81) | 71 files, 0 errors, 3 warnings, the same three as before; none names worldgen (PROGRESS.md:1583) |
| Check after a change | `& ([scriptblock]::Create(...m_check.ps1...)) -Out <dir> [-SkipMillion]`. Since 2026-11-18 it defaults to `build\z` and runs run_tests.ps1 as a script block; not run since that edit. | Not run since it was last edited (PROGRESS.md:1257) |

**Reference worlds.** Run these in `--lab` mode. Wild mode is now the default, so a run without `--lab` has no fixed hash (`tests\golden_hashes.txt:35`). Every reference-world command in STATUS section 10 uses `--lab` (regenerated 2026-11-18).

| World | Command | Last recorded hash |
|---|---|---|
| 100k | `"$A\build\z\az_run.exe" --lab --n 100000 --ticks 60 --cap 400000 --grid 6,6,5`, optionally with `--threads 1`, `--threads 8`, `--threads 24 --ecores --no-pin` or `--kernels 1` | **73f1e9e013f8db50** (worldgen, 2026-11-18; the same on `--threads 1`, `8` and `24 --ecores --no-pin`; 217,324 births, 89,855 deaths; books exact; 372, 73.7 and 59.9 ms per tick on an idle machine; `golden_hashes.txt:69`, logs `wg_ref100k_t1/t8/t24.txt`). Before: 2a0607a1d1151263 (chem piece B; 200,869 births, 7,757 deaths, 54.7 ms per tick), now history. |
| 1M | `az_run.exe --lab --n 1000000 --ticks 20 --cap 1500000 --grid 7,7,5 --kernels 1 --store-units 52000000`. `--store-units` is part of the hash; the default store would lock 12.3 GB (golden_hashes.txt:31). | **5d9c02d2c7dd5fb1** (worldgen; 1339 ms per tick on 8 workers, DRAM-bound by arithmetic; `golden_hashes.txt:70`, `wg_ref1m.txt`). Before: d7297084ce3e97ee (chem piece B), now history. |
| Engine matrix | `az_run.exe --lab --n 20000 --ticks 40 --seed 7` | **24c923001a46b122** (worldgen; printed, not pinned; `golden_hashes.txt:71`, `wg_matrix.txt`). Before: dd59809c915fc8ca (chem piece B), now history. |
| Wild 100k (no `--lab`) | `az_run.exe` without `--lab`; the run in `wg_wild100k.txt` used 100,000 founders, cap 400000, grid 6,6,5 and 100 species. Wild mode has no fixed hash by design | One run: 984e07a7655176a2 (origin 72612431ac3909f9), books exact (`wg_wild100k.txt`; PROGRESS.md:1593). Any other wild run gives another hash. |
| Protocol set | `az_run_proto.exe --lab --n 20000 --ticks 60 --seed 7 --cap 200000`; the same with `--cap 20224` | ac9f0c9367e6b98d / fd952281d7d8540e (golden_hashes.txt:32; last recorded, printed, not pinned; golden_hashes.txt has no newer line for them, and the matrices passed in ctest_z_worldgen.txt) |
| test_protocol reference world | - | eb14462839360008 (golden_hashes.txt:34) |
| Full matrix on one world | `python -I "$A\tests\cli\determinism_matrix.py" <az_run.exe> <work_dir> [--interacting] [--expect-denied] [--expect HASH] [--light] -- <world options incl. --ticks>` | determinism_matrix.py:13-15 |

**Tools:**
- `az_run`: all options are at `tools\az_run.c:4-10`. Wild mode is the default; `--lab` gives the seeded mode; `--seed` without `--lab` is refused. Other options: `--save`, `--load`, `--ledger`, `--entropy-journal`, `--param name=value`. A failed memory lock gives exit 3 unless `--allow-pageable` is given (az_run.c:12-40).
- `az_snap`: `az_snap --out FILE.png [--label T] [--n] [--ticks] [--lab [--seed]] [--grid] [--cap] [--species] [--threads] [--kernels] [--size 1280x720]`. It prints the state hash before and after rendering, and the two must be equal. Exit codes: 0 ok; 1 the hash changed, the balance broke or no PNG was written; 2 bad options; 3 the arena could not be locked (az_snap.c:3-21). Wild by default since 2026-11-18, `--lab` for the bench. The gate `cli_snap_modes` (`tests\cli\snap_modes.py`) passes.
- `az_live` (2026-11-18): `az_live --dir DIR [--n 20000] [--lab [--seed S]] [--grid x,y,z] [--cap C] [--species K] [--lesion BITS] [--param n=v,...] [--load FILE] [--threads T] [--kernels 0|1] [--size WxH] [--every K] [--fps F] [--keep K] [--max-ticks T]`. Renders the world every K ticks into `DIR\frame-NNNNNNNN.png` and prints one JSON line per frame (tick, live, eggs, clades, colonies, births, deaths, books, the state hash before and after drawing, pairs, body spread, stage times). Commands on stdin: `run`, `pause`, `step K`, `every K`, `fps F`, `camera YAW PITCH DIST`, `size WxH`, `save FILE`, `quit`. The gate `cli_live_smoke` checks it (21 checks, including: its lab hash equals `az_run --lab`'s). The GUI is `demo\` (`demo\README.md`).
- `az_lesion`: `az_lesion.exe --seeds 1-24` uses the tuning seeds. **Without `--seeds` it uses the preregistered reporting block 5101-5124, so never run it bare casually** (az_lesion.c:33-35; STATUS:730). Last table (after worldgen, `$A\build\work\logs\lesion_z_worldgen_1_24.txt`): 7 EFFECT, 14 NO EFFECT, 0 INERT, on tuning seeds only, so exploratory (PROGRESS.md:1595-1598). The one before it, at chem piece A, was 6 / 14 / 1 (GRAVITY INERT).
- `az_bench`: `az_bench [--n 10000,50000,100000] [--threads 1,P,PE] [--kernels 0] [--ticks 200] [--warmup 50] [--repeats 1] [--seed 42] [--csv file] ...` (az_bench.c:3-7). It points to `docs/BENCH.md` (az_bench.c:14), which **does not exist**.
- Other tools: `az_ledgerdump <file> [--chain 0x..] [--first N] [--kind k]`, `az_rngcheck [seed] [--control]`, `az_budget [--workers 8] [--cap-mode run|exact] [--csv f]`, `az_brainbench [--pcpu 0] [--ecpu 2] [--steps] [--batches]`, `az_gpubench [reps]`.

### 3. Scripts and tools

**`$A\build\work\scripts\`**

| Script | Purpose |
|---|---|
| run_tests.ps1 | Starts every `test_*.exe` of a build folder in parallel through .NET (Start-Process fails on the bracketed path); logs and tails; kills after 900 s. Defaults `build\z` and `build\work\tests` (Z:, 2026-11-18). |
| m_check.ps1 | Build `build\z` (its default since 2026-11-18; it named `full` before), run all tests, the 100k lab world at 1, 8 and 24 workers, the 1M lab world and lint; prints a summary. |
| clang_check.ps1 | Configure and build `build\clang-z` (was `build\clang`) with clang 21, run its tests, and compare 4 world hashes with the gcc build. |
| measure.ps1 | Timing table of 8 az_run configurations into `logs\measure_z.txt` (Z: defaults and .NET processes since 2026-11-18). |
| ab_time.ps1 | Interleaved A/B timing of two az_run executables; medians; CSV. |
| snapshot.py | Recovery copy of the tree, plus `build\work\scripts` and `logs`, into `build\work\snapshots\<date>-<label>`. Finds the root from its own place (2026-11-18). |
| check_text.py | ASCII, LF, no tabs, final newline over all source and doc files (skips build, .git and upstream). |
| make_status.py | Generates `docs\handoff\STATUS_AND_PLAN.md` and `.xml` from the data inside it (105 KB). |
| make_handoff_bundle.py | Builds `build\handoff\asmodensius-next-agent-<date>.zip` (the package, the sources it cites, `docs\requirements\`, a SHA-256 manifest) and `ASMODENSIUS-HANDOFF-ALL-IN-ONE.md`. Refuses to write if a file looks like it holds a key, or if the all-in-one breaks the text rules. `python -I "<root>\build\work\scripts\make_handoff_bundle.py"` (2026-11-18). |
| extract_prompts.py | Pulls the owner's typed prompts from a transcript `.jsonl` into md and xml. |
| digest_review.py | One line per finding from the review result JSON. |
| digest_w1.py | Prints a wave-1 workflow result JSON. |
| inventory.py | Counts the requirement sets and research notes for the catalog. |
| spend.py | Spend estimate of a run from the Claude transcripts (`--find <marker> --since <ISO>` or `--session`); prints ESTIMATE_USD. Default projects folder is `C:/Users/Alexa/.claude/projects`. |
| usage_check.py / usage_one.py | Token usage of transcripts priced at the Fable 5.2 rates. |
| gen\, membench\ | Folders, not opened for this section. |

**`$A\tools\`**

| File | Purpose |
|---|---|
| az_run.c | Runner (see section 2). |
| az_snap.c | One PNG of a world through `view\`. |
| az_lesion.c | Paired intact vs lesioned worlds for every lesion bit: Wilcoxon, Holm, d_z, bootstrap. |
| az_bench.c | Tick throughput by population, threads and kernels. |
| az_brainbench.c | ns per brain step for the C, AVX2 and VNNI versions on a P core and an E core. |
| az_budget.c | Byte budget from the column registry against the caches (arithmetic, not measured). |
| az_ledgerdump.c | Validate and print ledger files and their chain. |
| az_rngcheck.c | Philox sanity check (not a statistical battery). |
| az_tools.h | Shared strict option parsing and helpers. |
| az_statsutil.h | Statistics used by lesion and bench. |
| lint_core.py | Core rules E1-E9 (no float, libc, mutable globals, ...), owners.def and lesion checks; `--self-test`, `--strict`. |
| check_core_obj.py | Object gate: no floating-point instructions, no writable data; only memcpy, memset, memmove and memcmp left undefined. |
| check_avx_stack.py | No aligned ymm stack move without stack realignment (GCC bug 54412); `--dir` looks only at the `.exe` and `.a` files directly in the folder (line 100). |
| ubsan_check.py | Configure, build and probe the trap build, then run every test program there. |
| gen\gen_fx_tables.py | Generates and checks the constant tables of `core\az_fx.c`. |
| gen\gen_sh.py | Constants for `tk_sh` and vendoring of the libirrep oracle. |
| membench\membench.c | Per-core memory throughput against working-set size (a prebuilt `membench.exe` sits beside it). |

`$A\tests\cli\` holds `determinism_matrix.py`, `ledger_resume.py`, `snap_modes.py` and `live_smoke.py` (2026-11-18).

### 4. The run harness: `$A\build\work\api-run\Start-ApiRun.ps1`

**Update 2026-11-18:** invocation 5 ended at 12:04 UTC with "Credit balance is too low": the API credit is used up (launcher count 190.66 USD; Console Billing has the real figure). A new launcher's one-cent test at 14:30 UTC was refused too, and nothing changed. The worldgen landing after that was done by the desktop session on the Claude plan, not through this harness. On exit the launcher removed N:, T:, V: and W:. Since then the script maps no letters (`$Letters` is empty), passes `--add-dir` the real Z: folders, and its kickoff and system prompt say Z: only. Its self-test passes. Item 3 of `kickoff.md` (the budget) is historical and must be rewritten before any new run.


It runs the build as a loop of `claude -p` sessions billed to an API key (README.md:3).

**Startup:**
- Requires pwsh 7 (:42).
- Hard-codes `$Project` and `$Workspace` as Z: paths (:48-49) and a drive-letter map: N: is the project, T: is `Z:\[Vibe Coded (AI)]\(Tsotchke)`, V: is `<internal-ts-prototype>`, W: is the CLAUDECODE folder (:60-65).
- Keeps the PC awake and turns off QuickEdit in the console (:98-111). Clears inherited ANTHROPIC_, CLAUDE and MCP_ environment variables (:112-119).

**Key handling:**
- Asks for the API key with `Read-Host -AsSecureString` and rejects anything that is not shaped like a usable key (:237-245, :463-492).
- Free check: GET `/v1/models/{claude-fable-5-2 | claude-opus-5-5}` (:247-265). Falls back to the workspace ID from the Console sign-in (:266-273).
- Puts the key only in the process environment (:494) and clears it in `finally` (:573).

**Takeover:**
- A one-cent Haiku test in `--bare` mode with a 0.25 USD cap (:506-518).
- Stops the Ultra CLI build PID after its running build or test step finishes (:172-196), then waits 2 minutes to make sure nothing else writes PROGRESS.md.

**Loop (:525-568):**
- Stops when a STOP file exists, at 40 invocations, or when less than 6 USD is left below `-CapUsd` (default 180).
- Calls `Confirm-Letters`, which **re-creates any missing N:, T:, V: or W: with subst** (:127-138, :532) (no longer: `$Letters = [ordered]@{}` at :65 since 2026-11-18, so it maps nothing; see section 6a item 7; never run the launcher).
- Fills `{{N}}`, `{{SPENT}}`, `{{REMAINING}}`, `{{CAP}}` and `{{RUNSTART}}` into `kickoff.md`.
- Runs claude (:276-297) with `-p --model claude-fable-5-2 --fallback-model claude-opus-5-5 --effort max --dangerously-skip-permissions --permission-prompts none --max-budget-usd <remaining> --output-format stream-json --verbose --setting-sources project,local --settings settings.json --strict-mcp-config --autocompact 400k --append-system-prompt-file system-append.md --name asmodensius-api-NN --add-dir N:\ T:\ V:\ W:\`, with the CLAUDECODE folder as working directory (:327).
- Streams the output to `logs\run-NN-<stamp>.jsonl`.
- Scrubs key-shaped text from `docs` and `build\work`, snapshots excluded (:216-234).
- Spend is the larger of the sum of `total_cost_usd` plus the preflight, and the `spend.py` estimate (:543-546).
- The ending is read from the last line of the result: ALL-DONE, BUDGET-STOP or CONTINUE (:344-353). It also stops on `error_max_budget_usd`, on fatal billing or auth errors, on 3 transient errors in a row (retry after 5 minutes), and when PROGRESS.md has not changed for 2 invocations (:553-566).

**Exit:** `Remove-Letters` runs unless an interactive Claude CLI is still open (:139-143, :574). The state is written to `state.json`.

**Modes:** `-SelfTest` and `-DryRun` make no API call (:364-428).

**Recorded runs (state.json):**

| Invocation | Ending | Notes |
|---|---|---|
| 1 | NO-RESULT | - |
| 2 | NO-RESULT | - |
| 3 | BUDGET-STOP | 302.7 min, 115.05 USD |
| 4 | BUDGET-STOP | 112.3 min; run total 181.93 USD; ended 11:15:48Z |
| 5 | credit used up | started 11:28:45Z; ended 12:04 UTC with "Credit balance is too low"; launcher count 190.66 USD (Console Billing has the real figure) |

A new preflight ran at 11:28:40Z and invocation 5 started at 11:28:45Z. `state.json` still says capUsd 180: `Read-State` sets the cap only when it creates the file (:199-204). So the cap actually used for invocation 5 cannot be read from the files. Not verified.

`kickoff.md:17-25` now carries the owner's final-15-USD rule: stop at 198 USD, aim for a shippable PoC MVP, and build no GUI and change no paths in this run.

### 5. Test programs and ctest gates

**Test programs: 33.** These are the 32 files `$A\tests\test_*.c` plus `$A\gpu\test_gpu.c` (test_worldgen added 2026-11-18):

- base: core, rng, fx, grid, scan, field, brain, dense, genome, store, ledger, snapshot, view, tools, statsutil, host_win
- Tsotchke kernels: tk_hop, tk_qgt, tk_qsv, tk_sh
- interacting set: protocol
- modes: wild
- wave 2: life, mate, tier1, metab, social, econ, colony, weather, chem, worldgen
- GPU: gpu (built only with the GPU component)

`tests\ubsan_probe.c` holds the planted violations for the trap build. It is not a ctest item (CMakeLists.txt ~:322).

**Gates (ctest items that are not test programs): 13** (2026-11-18).

| Gate | Checks |
|---|---|
| lint_core, lint_core_selftest | core rules, and the planted violations in `tests\lint\` |
| core_obj_gate, core_obj_gate_selftest | the compiled core |
| avx_stack_gate, avx_stack_gate_selftest | aligned ymm stack moves |
| cli_ledger_resume | ledger across save and load |
| cli_matrix_protocol, cli_matrix_protocol_cap | determinism matrix on the protocol set |
| cli_matrix_engine | determinism matrix on the engine |
| ubsan_trap_build | the trap build as a test |
| cli_snap_modes | az_snap and az_run in both modes (passes) |
| cli_live_smoke | az_live: modes, exact books, the view an observer, lab hash = az_run's, stdin commands, save and load (21 checks, passes) |

All gates exist only when AZ_TESTS=ALL and Python is found (CMakeLists.txt:267-269).

**Last complete ctest:** 46 of 46 (33 test programs, 13 gates) in 253 s on build\z, 2026-11-18, after the worldgen landing (`$A\build\work\logs\ctest_z_worldgen.txt`; test_worldgen 23 s, ubsan_trap_build 252.5 s). Before that: 45 of 45 (32 test programs, 13 gates) in 386 s on build\z, and 44/44 on `N:/build/full` at the end of the API run (383.74 s), and 43/43 in 298 s at chem piece A. The slowest items were test_protocol (254 s) and ubsan_trap_build (298 s) (`$A\build\work\w2\logs\ctest_chem1.txt`; PROGRESS.md:1211, 1227). Planted defects caught: colony 23/23, weather 16/16, chem 13/13 (piece A) and 27/27 (piece B), worldgen 31/31 (`$A\build\work\w2\worldgen\mutants2\summary.txt`).

**Recorded check counts:**

| Test | Checks | Source |
|---|---|---|
| test_genome | 436,178 | STATUS:55 |
| test_brain | 1,297,994 | STATUS:55 |
| test_dense | 6,225,307 | STATUS:55 |
| test_store | 248 | STATUS:55 |
| test_wild | 84 | STATUS:55 |
| test_protocol | 874,495 | STATUS:55 |
| test_core | 497 | STATUS:50; older figure |
| test_host_win | 187 | STATUS:50; older figure |
| test_chem | 153 (piece A); 579 after piece B | PROGRESS.md:1209; 02-STAGES-AND-STATUS.md |
| test_worldgen | 4034 | PROGRESS.md:1561 |

Per-test counts after wave 2 are not collected in one place.

**Status record:** section 10 of STATUS_AND_PLAN.md was regenerated on 2026-11-18 after the worldgen landing: 46 of 46, 13 gates, the hashes 73f1e9e013f8db50 / 5d9c02d2c7dd5fb1 / 24c923001a46b122 and `--lab` commands. It is generated from the check table in `make_status.py`; change it there, never by hand.

### 6. MIGRATION LIST (regex `[NSTVW]:[\\/]`, case-sensitive)

**Done on 2026-11-18 by the desktop session (verified by the Z: build and ctest):** CMakeLists.txt globs through `AZ_GLOB_ROOT` and the NASM include folder as one quoted option (6a items 1 and 2); `tools\ubsan_check.py` uses `glob.escape` (item 4); `run_tests.ps1`, `measure.ps1`, `m_check.ps1`, `clang_check.ps1` and `snapshot.py` use Z: paths; the launcher maps no letters (item 7); `kickoff.md`, `system-append.md`, COMMON.md (Paths), CONTINUE-BUILD.md (section 0), START-CLI.md and BUILD.md say Z: only; the letters N:, T:, V: and W: are gone. Item 5 (test_statsutil through `__FILE__`) is fixed by building from Z:. **Still open:** the text in `make_status.py` (then regenerate STATUS_AND_PLAN), the first line of each W2 brief, the ABI notices, `gpu\README.md`, and the historical command column of `tests\golden_hashes.txt` and PROGRESS.md (history: leave it). The list below is as compiled before the fixes.


Real targets of the old letters:

| Letter | Real path |
|---|---|
| N: | `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native` |
| T: | `Z:\[Vibe Coded (AI)]\(Tsotchke)` |
| V: | `<internal-ts-prototype>` |
| W: | `Z:\[Vibe Coded (AI)]\CLAUDECODE` |

S: was an old scratch drive and is no longer used.

**Code: no hits.** None in core\, kernels\, platform\, view\, tools\ (C or Python), tests\*.c, tests\cli\, tests\protocol\ or gpu\*.c. `ARCHITECTURE.md` has none either.

**Build and launch files:**

| File | Lines | What is hard-coded |
|---|---|---|
| `$A\CMakeLists.txt` | 8, 9, 10, 13, 44 | comments only (`N:/build/...`) |
| `$A\build\work\scripts\run_tests.ps1` | 1 | default `N:\build\full`, `N:\build\work\tests` |
| `$A\build\work\scripts\clang_check.ps1` | 2, 10, 11, 15, 20, 28 | build folders and its call to run_tests.ps1 |
| `$A\build\work\scripts\m_check.ps1` | 1, 7, 9, 16, 40 | build folder, run_tests.ps1, lint |
| `$A\build\work\scripts\measure.ps1` | 1 | default exe and output paths |
| `$A\build\work\scripts\snapshot.py` | 5-7 (docstring), 17 | `root = 'N:\\'` |
| `$A\build\work\scripts\make_status.py` | 140, 250, 282-295, 336, 541, 543, 554, 555, 612 (S:) | the text that STATUS_AND_PLAN is generated from |
| `$A\build\work\api-run\Start-ApiRun.ps1` | 23, 60-65, 127-143, 140, 294, 370, 460, 579 | the letter map, Confirm-Letters / Remove-Letters, `--add-dir N:\ T:\ V:\ W:\`, a self-test fixture, messages |
| `$A\build\work\api-run\kickoff.md` | 3, 7, 8, 10, 16, 26, 27, 31 | the prompt sent to every invocation |
| `$A\build\work\api-run\system-append.md` | 2, 4 | appended system prompt |
| `$A\build\work\api-run\README.md` | 20 | - |
| `$A\build\work\api-run\owner-leave-room.md` | 23 | - |
| `$A\build\work\api-run\P22.md` | 3, 9, 21 | owner prompts, verbatim |
| `$A\build\work\PROGRESS.md` | 82 hits | run log; historical |

**Golden file:** `$A\tests\golden_hashes.txt` has 41 lines (6-63) whose command column reads `N:\build\full\...`. They are documentation only: no code parses this file.

**CMakePresets:** none.

**CMakeCache.txt:** every build cache has `CMAKE_HOME_DIRECTORY:INTERNAL=N:/`, `asmodensius_native_SOURCE_DIR:STATIC=N:/` and `CMAKE_CACHEFILE_DIR:INTERNAL=n:/build/<folder>`. Affected folders:
- `build\clang`, `clean-2026-11-17`, `clean-2026-11-18`, `full`, `full\ubsan`, `lead`, `release`, `skel`, `ubsan`, `verify`
- `owners\guard-lint`, `guard-tests-a`, `guard-tests-b`, `guard-view-snap`, `tools-bench`, `tools-ledger`, `tools-run`, `tools-test`
- `work\m1\old-build` and `work\m2\old-build` (source `N:/build/work/m*/old-tree`)
- `work\m3\su`

The generated files are bound to N: as well (for example `build\full\CTestTestfile.cmake` holds 88 `N:/` paths). All of these folders work only with N: mapped and must be configured again into new folders.

**Operational docs (listed separately):**

| File | Lines |
|---|---|
| `$A\docs\handoff\CONTINUE-BUILD.md` | 6, 7, 10-15, 18, 23, 28, 30, 33, 41, 47, 48, 50, 51, 54, 59, 61, 67-69, 72, 74, 77, 78, 81, 86 (29 hits) |
| `$A\docs\handoff\START-CLI.md` | 3, 5, 11 (the `subst N:` line), 24, 36, 47, 49, 51 |
| `$A\docs\agents\BUILD.md` | 4, 6, 7, 8, 19, 25, 31, 40, 42, 51, 69, 90 |
| `$A\docs\agents\COMMON.md` | 8, 69-79 (the drive table and the "never use the long bracketed paths" rule) |
| README | no root README. `$A\gpu\README.md:131,184,202` (`S:/build/gpu`, `S:/work/gpu`); `$A\build\work\api-run\README.md:20`; `$A\docs\agents\W2-BRIEFS\README.md:13` |
| Other agent docs | `$A\docs\agents\ABI-V3-NOTICE.md:4,11,12,14,34`; `ABI-V4-NOTICE.md:150`; `W2-BRIEFS\<module>.md:1` (13 files: life, mate, tier1, metab, social, econ, colony, weather, chem, worldgen, sense, physics, stats) |
| `$A\docs\handoff\STATUS_AND_PLAN.md` / `.xml` | 49 / 41 hits; regenerate them from make_status.py, do not edit by hand |
| Keep unchanged (verbatim or provenance) | `$A\docs\handoff\PROMPTS.md` / `.xml` (13 / 13, the owner's words); `$A\THIRD_PARTY_NOTICES.md:8,18` and `$A\tests\upstream\libirrep\PROVENANCE.txt:3` (`T:\mirrors`) |
| Data, not operational | `$A\docs\requirements\*` (about 6,190 hits in 51 JSON and md files, mostly V:\ and W:\ source citations); `docs\GROKBOT-FACTCHECK.md` (1 hit) |

`docs\research` and the snapshots were excluded as instructed.

**6a. Files that depend on the path itself.** The real path contains `[ ]`, `( )` and spaces. STATUS_AND_PLAN.md:715 says to quote it and give every cmdlet -LiteralPath. Items 1, 2 and 4 were inferred from the code; they are fixed since 2026-11-18 (erratum E-05).

1. FIXED: every `file(GLOB)` goes through `${AZ_GLOB_ROOT}` (`CMakeLists.txt:32-37, 94, 186, 196, 233, 253`). **`file(GLOB ...)` on `${CMAKE_SOURCE_DIR}`** at CMakeLists.txt:84 (tk_*.c), 175 (platform/win/*.c), 185 (view/*.c), 222 (tools/az_*.c) and 242 (tests/test_*.c). CMake globbing treats `[...]` as a character class. If the configure step uses the bracketed path, these lists come back empty: az_host gets no sources, and the tools, tests and tk kernels drop out.
2. FIXED: the NASM include is one quoted `target_compile_options` item (`CMakeLists.txt:182`). **NASM include flag in a space-separated string** at CMakeLists.txt:166: `set(CMAKE_ASM_NASM_FLAGS "... -I${CMAKE_SOURCE_DIR}/kernels/")`. The spaces in "Vibe Coded (AI)" split this flag. The GPU twin rule (gpu\CMakeLists.txt:129) quotes its argument and is safe.
3. **Regex on the source dir** at gpu\CMakeLists.txt:26-27: `string(REGEX REPLACE "/+$" "" AZ_GPU_ROOT "${CMAKE_SOURCE_DIR}")`. It exists because the source was the root `N:/`. Harmless on a Z: path.
4. FIXED: `glob.escape` (`tools\ubsan_check.py:94-95`). **`$A\tools\ubsan_check.py:94`** calls `glob.glob(os.path.join(bld, "test_*.exe"))` without `glob.escape`. Under the bracketed path it finds no programs and fails with "no test program" (line 95-97), which breaks the ubsan_trap_build gate.
5. **`$A\tests\test_statsutil.c:414-422`** finds `core\az_abi.h` and `tools\az_lesion.c` through `__FILE__`. Binaries compiled from N: contain `N:/tests/...`, so `CHECK(abi != NULL)` fails once N: is gone. Rebuilding from a Z: path fixes it. `tests\az_test.h:21,32` print `__FILE__` too, which is only cosmetic.
6. **`pwsh -File` with a bracketed path** is avoided by design (api-run README.md:31), so a script started by path needs the `[scriptblock]::Create` form.
7. FIXED in effect: `Start-ApiRun.ps1:65` sets `$Letters = [ordered]@{}`, so `Confirm-Letters` (`:127-137`, called at `:532`) maps nothing (`:61-63` say so). The dead subst code remains (09 REP-05, S3). Never run the launcher (00-MASTER-PROMPT.md section 3.2).

No toolchain file exists, and no other CMake regex or EXISTS check depends on the drive letter. `list(FILTER ... REGEX "/az_${m}\\.c$")` at :141 is path-neutral.

The drive letters are gone and the code fixes of items 1, 2 and 4 are the path taken. A junction or symbolic link is forbidden (00-MASTER-PROMPT.md law 11).
