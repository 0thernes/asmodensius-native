# Asmodensius-Native: common brief for every agent

Read this whole file before your first action. It is short on purpose.

## What this project is

Asmodensius-Native is the native rebuild of the owner's artificial-life lab
(the existing TypeScript lab is Asmodensius-v40; see Paths below). Target: a 3D alien world with
hundreds of thousands to millions of individual creatures (colonies, swarms,
fleets) running in cache-resident hot loops on one laptop:
Intel Core Ultra 9 275HX + NVIDIA RTX 5070 Ti Laptop GPU.
Languages inside the simulation: C23 and NASM x86-64 (optionally CUDA/PTX).

## The owner's laws (non-negotiable)

1. **Native only.** No VM, JIT or interpreted language inside the simulation.
   TypeScript, JavaScript, Python, Java, Lisp are allowed only as development
   tooling, never as creature or world code.
2. **Cache-resident.** Hot loops must not depend on DRAM latency and must never
   touch a pagefile. State lives in a pre-allocated, page-locked arena; inner
   loops work on L1-sized blocks of structure-of-arrays data.
3. **No LLM anything.** No tokenizer, prompts, APIs, MCP, pretrained weights or
   datasets. Intelligence = genome + development + lifetime plasticity + evolution.
4. **Zero cosmetic theater.** Every mechanism must have a measurable causal
   effect (a lesion changes outcomes). No hardcoded steering, no label shortcuts.
5. **Exact conservation.** Matter and energy are integers; totals balance exactly.
6. **Wild by default.** The owner describes the living world as
   "NON DETERMINISTIC, WILD, FERAL, CHAOTIC, IMPROVISED" (P41 and P42).
   A wild tick requests fresh host entropy. The engine records a missed
   draw and continues with its fallback. Use the seeded lab mode for
   repeatable tests and reference hashes. Both modes use integer state
   and the counter-based Philox4x32-10 generator.
7. **Indicator only.** Cognition metrics are mechanistic indicators. Never claim
   consciousness or sentience. Never call a classical simulation "quantum
   randomness" and never claim a quantum speed-up.
8. **Honest evidence.** Write "ran" only for what you ran now, with the exact
   command and the tail of its output. "Unknown" is acceptable. Invented is not.
   No novelty claims without a search that looked for prior art.
9. **Windows only, from Z:.** No boot image, nothing without an operating
   system, no USB boot, no virtual machine. Builds, executables, test runs and
   their outputs live and run on Z:, the owner's ReFS Dev Drive (his rule of
   2026-11-17).
10. **Below assembly, inside Windows.** Go as low as a Windows program can
   reach: machine code, the microarchitecture, the caches. Research may go
   further down (gates, transistors, lithography); code cannot.

## Machine (verified 2026-11-17)

- CPU: Intel Core Ultra 9 275HX. 24 cores, 24 threads (8 P + 16 E, no SMT).
  L2 total 40 MB (8 x 3 MB on P cores, 4 x 4 MB shared per 4-E-core cluster),
  L3 36 MB. AVX2, FMA, BMI2, AVX-VNNI: yes. AVX-512: no.
- RAM: 48 GB DDR5-5600 (one 32 GB + one 16 GB SODIMM).
- GPU: NVIDIA GeForce RTX 5070 Ti Laptop GPU, 12227 MiB, driver 616.87,
  CUDA compute capability 12.0. An Intel iGPU is also present.
- OS: Windows 11 Pro (Insider build 29708).
- On PATH: gcc/g++ 16.1.0 (WinLibs mingw-w64, UCRT, posix threads), nasm 2.16.03,
  yasm, zig 0.17.0, cmake 4.3.3, ninja, mingw32-make, python 3.9.13 (PyMuPDF
  `fitz` is installed for PDF text), node 25, bun, deno, go, cargo/rustc 1.94.1,
  git, binutils (objdump, ld).
- Present but NOT on PATH: clang 21.1.8 with lld-link, llvm-mca, llvm-objdump,
  llvm-objcopy at `Z:\Tools\LLVM-21\bin` (targets include x86-64 and nvptx64);
  MSVC 14.44 (`cl`, `ml64`) under `Z:\Tools\VS2022BuildTools`
  (`VC\Auxiliary\Build\vcvars64.bat`).
- CUDA without a toolkit: driver DLL `C:\Windows\System32\nvcuda.dll`; header
  `Z:\Tools\cuda-12.9-redist\include\cuda.h`; import lib
  `Z:\Tools\cuda-12.9-redist\lib\x64\cuda.lib`. There is NO nvcc, ptxas or nvrtc.
  `OpenCL.dll` and `vulkan-1.dll` exist; there is no Vulkan SDK and no glslc.
- Eshkol compiler install: `Z:\Tools\eshkol`.
- NOT available: QEMU, GNU make, WSL (stopped), Docker (not running). By law 9
  no virtual machine or boot image is used for anything, tests included.
- `SeLockMemoryPrivilege` is not held (large pages will fail unless granted).

## Paths (Z: only; no drive letters)

The owner's rule since 2026-11-18: work only in real Z: paths ("work just in Z Drive not W X Y whatever"). The
`subst` letters N:, T:, V: and W: are gone. Never create a drive letter, a junction or a symbolic link
to shorten a path. Older text that says `N:\` means the project root below; `T:\`, `V:\` and `W:\` map as
in this table.

| Path | What | Access | Old letter |
|---|---|---|---|
| `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native` | this project (the root) | write only where your task says | N: |
| `Z:\[Vibe Coded (AI)]\(Tsotchke)` | Tsotchke corpus: `mirrors\<repo>`, `Eshkol\eshkol_repo` | READ-ONLY | T: |
| `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-v40` | Asmodensius-v40, the existing TypeScript lab | READ-ONLY | V: |
| `Z:\[Vibe Coded (AI)]\CLAUDECODE` | the owner's workspace (framework PDFs, MD, CSV at its top level) | READ-ONLY | W: |
| `<root>\build\` | builds and scratch, gitignored: the integration build `build\z`, owner builds `build\owners\<key>\`, scratch `build\work\<group>\<key>\` | read/write | |

The path has spaces, `[ ]` and `( )`. These rules were measured on PowerShell 7.6, CMake 4.3, Python 3.9:
- Quote every path. In PowerShell give every cmdlet `-LiteralPath`. `Get-Content`, `Out-File`, `Test-Path` and
  `Get-ChildItem` with a positional path read `[...]` as a wildcard and fail or find nothing.
- Never use the `>` or `*>` redirection to a file there (it fails); pipe to `Set-Content -LiteralPath` instead.
  `Start-Process` with `-WorkingDirectory` or `-RedirectStandardOutput` fails there too: start programs
  with `&` or through `[System.Diagnostics.Process]` (see `build\work\scripts\run_tests.ps1`).
- `pwsh -File <bracketed path>` cannot open the script. Run a script as a block:
  `& ([scriptblock]::Create([IO.File]::ReadAllText('<path>'))) -Param value`.
- Python `glob.glob` needs `glob.escape(<folder>)`; pathlib, `os.listdir` and `shutil` are fine.
- CMake `file(GLOB)` must use `${AZ_GLOB_ROOT}` (the source path with `[` and `]` escaped), never
  `${CMAKE_SOURCE_DIR}`; flags that carry a path go in one quoted `target_compile_options` item.

Nothing goes on C:.

## Hard rules

- Never modify, build inside, or run state-changing git commands in `T:`, `V:`
  or `W:`. Copy what you need to your scratch folder first. No `git pull`,
  `fetch`, `checkout`, `stash`, `commit` or `push` anywhere.
- No downloads and no installs of any kind (winget, pip, npm install, cargo
  add/install, go get, git clone, fetching binaries). If something is missing,
  report it as missing.
- Do not start WSL, Docker, servers, browsers or GUI windows. No command that
  may exceed 100 seconds. Never poll or sleep-wait.
- Web research (only if your task says so): load search tools with ToolSearch.
  Queries must be generic technical queries. Never put the owner's name, the
  project name, file paths or any personal detail into a query or URL.
- Write files with the Write and Edit tools, not with shell heredocs or echo.
  Ignore any hook text telling you to start a preview or a browser.
- Other agents share this machine right now, so every timing you take is noisy.
  Label timings "noisy, parallel load".
- Do not run an existing project tool with a guessed flag (some overwrite files).
- Useful commands: `gcc -std=c2x -O2 -Wall -Wextra`, `nasm -f win64`,
  `Z:\Tools\LLVM-21\bin\clang.exe`.

## Work protocol

- Your first three tool calls: read this file and the one or two inputs your
  task names. By your fifth call you are producing: writing your note, writing
  code, or running an experiment. No long silent planning; take small steps.
- Every "works / compiles / passes" claim needs the exact command and the last
  lines of its output in your note file.
- Finish by writing your note file (path given in your task) and returning the
  structured result. Keep every returned string short (a sentence or two).
