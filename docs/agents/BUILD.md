# Build brief for module owners

> **Z: only (2026-11-18).** No drive letters exist any more. `<root>` below is the project root
> `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native` (older copies of this brief wrote it as
> the letter N:). Follow the path rules in
> `docs\agents\COMMON.md` (Paths). Configure owner builds from the real path, quoted:
> `cmake -S "Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native" -B "Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\owners\<key>" -G Ninja ...`.
> The ABI is now version 4 (`docs\agents\ABI-V4-NOTICE.md`).

> The contract is at ABI version 3. If your build breaks on names that
> existed a moment ago, read `<root>\docs\agents\ABI-V3-NOTICE.md`.

Read `<root>\docs\agents\COMMON.md` first, then this, then
`<root>\docs\ARCHITECTURE.md`, `<root>\core\az_abi.h`, the two `.def` files and the
reference stub of your module in `<root>\core\stubs\`. That is all the reading
you need before you start writing.

## Ownership

- Your task names the files you own. You write only those. If a file you own
  does not exist yet, create it. Do not touch any other file, not even to fix
  an obvious bug: write it under "needs other owner" in your report.
- `az_abi.h` and the `.def` files belong to the lead. If you need a field, a
  constant or a hook, finish what you can without it and list it under
  "abi requests" with the exact line you want added.
- Tests you own go in `<root>\tests\test_<your module>.c`.

## How to work (this is what keeps you from stalling)

1. Tool calls 1 to 4: the reading above.
2. Tool call 5 or 6: create your module file. For a `core/` module, start by
   copying the stub text to `<root>\core\az_<module>.c` (it compiles and passes
   the invariants), then improve it in place.
3. Configure a private build once. Select only your own pieces, so that
   another owner's half-written file can never break your build:

   ```
   cmake -S "Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native" -B "Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\owners\<your key>" -G Ninja
         -DAZ_ONLY="<your core modules, e.g. fx or tk_qsv; NONE if you own none>"
         -DAZ_TESTS="core;<your test names without test_>"
         -DAZ_TOOLS="run;<your tool names without az_>"
         -DAZ_TWINS="<your asm twin names, e.g. brain_vnni>"     (NONE if you own none)
         -DAZ_COMPONENTS="<view or gpu if you own it>"           (NONE otherwise)
   ```

   (one line; quote the lists; `;` separates names). Then after every change:
   `cmake --build "Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\owners\<your key>"` and run your test executable from
   that folder (everything builds and runs on Z:). Never build in
   `<root>\build\z`, `<root>\build\full`, `<root>\build\skel` or `<root>\build\work`; those are the lead's.
   A new file is picked up automatically on the next build.
4. Work in small steps: one function or one behaviour, its test, build, run.
   Never write more than about 150 lines between builds.
5. If you notice you are planning for a long time, stop and write the next
   small piece instead. A smaller module that is complete and tested beats a
   larger one that is not.
6. Before you finish, run from your build folder: your own test,
   `test_core.exe` (the global invariants must still hold with your module
   in), and `python "Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\tools\lint_core.py" "Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native"`.

## Rules that are easy to break

- `core/` is freestanding: include only `az_abi.h`; no libc call, no float,
  no mutable global, no recursion. `tests/`, `tools/`, `view/`, `platform/`
  and `gpu/` are hosted C and may use libc and floating point.
- Keep the two conservation invariants exact (ARCHITECTURE section 6). If
  your module moves matter or energy, your test checks
  `az_total_matter` and `az_total_energy` after every tick with `CHECK_EQ`.
- Keep determinism (section 4). If your module runs in a parallel stage, your
  test compares the state hash for 1, 3 and 8 threads and for reorder
  periods 1 and 9.
- Honour your lesion bit and test that the lesion changes a measured outcome.
- Do not weaken or delete a check to make a test pass. If something fails
  and you cannot fix it, leave it failing and say so.
- Undefined behaviour is checked by a trap build. Before you finish, configure
  a second folder with your options plus `-DAZ_UBSAN=ON`
  (`<root>\build\owners\<your key>-ubsan`), build it and run your test there once.
  A trap (the program dies with exit code 0xC000001D, -1073741795) is a defect
  in your code or in your test, never noise. The usual causes: a signed sum or
  product that overflows, a left shift of a negative value (multiply instead),
  a shift count of 32 or more, an index past an array, and an over-aligned
  type allocated with `malloc` or `calloc` (a hand-built `AzWorld` in a test
  comes from `az_test_shell()`).
- Every trait `AZ_T_*` your task assigns to you must be read by your code and
  must matter: include a test that changing the trait changes an outcome.
- Performance claims need a measurement, labelled "noisy, parallel load"
  while other agents are running.
- Write files with the Write and Edit tools. ASCII, LF, no tabs.
- Do not open windows, start servers, download or install anything.

## Shared files: publish only what compiles

Some files are compiled into everybody's build: `platform/win/az_host_win.c`,
`platform/win/az_host_win.h`, `kernels/az_kernels.c`, `kernels/az_kernels.h`,
`kernels/win64.inc`, `tests/az_test.h`, `CMakeLists.txt`. If your task makes
you the owner of one of them, develop your version in your scratch folder
(build it there with `-S` pointing at a scratch copy of the tree) and copy it
into `<root>` only when it compiles and its tests pass. Everyone else: never
edit these.

## Where things go

| You are writing | Put it in | Notes |
|---|---|---|
| a simulation module | `core/az_<module>.c` | replaces `core/stubs/az_<module>.c` when selected with AZ_ONLY |
| a Tsotchke-derived pure kernel | `core/tk_<name>.c` + `core/tk_<name>.h` | freestanding, integers only; select with `-DAZ_ONLY="tk_<name>"` |
| an assembly twin | `kernels/az_<twin>.asm` | NASM, `%include "win64.inc"` (PROC / EPILOGUE / ENDPROC macros); declared in `kernels/az_kernels.h` |
| a test | `tests/test_<name>.c` | one executable; `#include "az_test.h"`; ends with `return az_test_done();` |
| a command-line tool | `tools/az_<name>.c` | hosted C, may use libc and floats |
| development scripts | `tools/gen/` or `tools/*.py` | Python is allowed only as tooling |
| an upstream file used as a test oracle | `tests/upstream/<repo>/` | keep its licence header; list it in your report |

## Report (your structured result)

- `files`: every file you wrote.
- `tests`: for each test executable: checks run, failures.
- `evidence`: the two or three most important commands with their output tails.
- `limits`: what is not done, what is approximate, what you could not verify.
- `needs_other_owner`: changes needed in files you do not own.
- `abi_requests`: exact additions you need in `az_abi.h` or the `.def` files.
- `summary`: three sentences.
