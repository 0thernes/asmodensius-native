# gpu/ - the GPU path

Two parts, kept apart on purpose:

- **Part 1, FACT**: what exists, what was run, what it printed.
- **Part 2, DESIGN**: how the whole tick could move to the device. Nothing
  in Part 2 is built or measured.

Written 2026-11-17 against ABI version 3 (`core/az_abi.h`).

---

# Part 1 - FACT: what runs on the GPU today

## What it is

Two integer kernels run on the NVIDIA GPU and give the same bytes as the CPU:

| Kernel | Device entry | CPU reference |
|---|---|---|
| n Philox4x32-10 blocks | `az_gpu_k_philox` | `az_philox4x32_n` (`core/az_rng.c`) |
| one brain step for n creatures | `az_gpu_k_brain` | `az_dense_step_c` (`core/az_brain.c`) |

Plus `az_gpu_k_layout` (ABI guard, below) and `az_gpu_k_empty` (launch cost).

Nothing else of the simulation runs on the GPU. `az_tick` does not call this
code. The world's own columns are not on the device. This is a verified
building block, not a GPU simulation.

## Files

| File | What |
|---|---|
| `az_gpu.h`, `az_gpu.c` | host: loads `nvcuda.dll` with `LoadLibrary`, context, module load, device buffers, launches |
| `az_gpu_dev.c` | device translation unit: includes the core C files, adds the per-thread indexing |
| `az_gpu_ref.c` | CPU reference: the same includes, compiled by gcc with the flags of `az_core` |
| `az_gpu_layout.h` | the layout words host and device must agree on |
| `az_gpu_embed.cmake` | checks the PTX and turns it into a C byte array |
| `CMakeLists.txt` | the build (component `gpu`) |
| `test_gpu.c` | GPU == CPU, bit for bit |
| `az_gpubench.c` | timings |

## How it is built

No CUDA toolkit: no nvcc, no ptxas, no nvrtc, no `cuda.lib`, no CUDA header.

1. CMake writes `az_gpu_dev_srcs.h` with two `#include` lines:
   `core/az_rng.c` and `core/az_brain.c` if those files exist, else the stubs
   in `core/stubs/` (override: `-DAZ_GPU_BRAIN_SRC=`, `-DAZ_GPU_RNG_SRC=`).
2. A custom command runs clang (default `Z:/Tools/LLVM-21/bin/clang.exe`,
   `-DAZ_GPU_CLANG=`) on `az_gpu_dev.c`:
   `--target=nvptx64-nvidia-cuda -march=sm_80 -O2 -std=gnu23 -ffreestanding -fwrapv -fno-strict-aliasing -S`
   and gets PTX text (`.version 7.0`, `.target sm_80`; `-DAZ_GPU_ARCH=` changes it).
3. `az_gpu_embed.cmake` fails the build unless the PTX has the four entry
   points, no `.extern .func` (no unresolved function) and no `.f16`/`.f32`/
   `.f64` (no floating point). Then it embeds the text in `libaz_gpu.a`.
4. At run time `az_gpu_open` hands the text to the driver
   (`cuModuleLoadDataEx`), which compiles it.

If clang is not installed the library is built without device code and
`az_gpu_open` returns `AZ_GPU_NO_PTX`.

### One definition of the semantics

`az_gpu_dev.c` contains no copy of the brain step or of Philox. It includes
the core source files inside

```c
#pragma clang attribute push (__attribute__((internal_linkage)), apply_to = function)
...
#pragma clang attribute pop
```

so every function of those files becomes internal: the compiler inlines what
a kernel reaches and drops the rest (clade development, anything that takes
an `AzWorld`). No list of function names is kept, so a change of `az_abi.h`
does not break it. `az_gpu_ref.c` includes the same generated header for the
CPU side. The test therefore compares two compilations of one source text.

### ABI guard

`az_gpu_layout.h` defines 16 words (magic, `AZ_ABI_VERSION`,
`sizeof(AzDenseNet)`, the offsets of `w1`, `w2`, `b1`, `b2`, `shift1`, `shift2`,
the brain dimensions, pointer size). The device kernel `az_gpu_k_layout`
returns its compiler's values; `az_gpu_open` compares them with the host's
and refuses the module (`AZ_GPU_BAD_MODULE`) on any difference.

## API in short (`az_gpu.h`)

- `az_gpu_open(&g, opt, &info)` / `az_gpu_close(g)`; `info.msg` always says
  what happened. `az_gpu_set_block(g, threads)` changes the threads per
  block (default 256); results do not depend on it (tested with 256 and 1024).
- Device buffers: `az_gpu_alloc`, `az_gpu_free`, `az_gpu_upload`,
  `az_gpu_download` (offset and size are range-checked), `az_gpu_sync`.
- Page-locked host memory from the driver: `az_gpu_host_alloc`, `az_gpu_host_free`.
- Kernels on device buffers: `az_gpu_philox(out, ctr, key, n)`,
  `az_gpu_brain_step(&bufs, n)`, `az_gpu_empty`. Each launch first checks
  that every buffer is large enough for n.
- Whole trips on host arrays: `az_gpu_philox_host`, `az_gpu_brain_step_host`.

Brain buffers have the layout of the CPU columns: `sens[n*48]` (u8),
`hid[n*32]` (u8, updated in place), `gain[n*8]` (u8), `bias[n*8]` (i8),
`clade[n]` (u16), `motor[n*16]` (i8), and the clade table as raw `AzDenseNet`
bytes. A clade index outside the table is not followed: that creature gets a
zero motor vector and keeps its hidden state.

All calls on one `AzGpu` must come from the thread that opened it.

## No NVIDIA driver

`az_gpu_open` never aborts. It returns:

| Status | When |
|---|---|
| `AZ_GPU_NO_DRIVER` | `nvcuda.dll` is not in the system directory, or lacks a needed symbol |
| `AZ_GPU_NO_DEVICE` | `cuInit` fails or the driver reports no device |
| `AZ_GPU_NO_PTX` | the build has no device code |
| `AZ_GPU_BAD_MODULE` | the driver refuses the PTX, or the layout words differ |

`test_gpu` and `az_gpubench` print one line and exit 0 for the first three.
By default the DLL is loaded by full path from the system directory. A caller
that sets `AzGpuOpt.dll_path` (`test_gpu` also takes it from the environment
variable `AZ_GPU_DLL`) gets exactly that string handed to `LoadLibraryA`: a
bare name there goes through the normal DLL search path. `test_gpu` passes
the bare name `kernel32.dll` as "a DLL that is not a CUDA driver" (finding
v82: this file used to say the search path is never used).

## What was run (2026-11-17, this machine)

Machine: RTX 5070 Ti Laptop GPU, compute capability 12.0, driver API 13040;
gcc 16.1.0; clang 21.1.8. Build folder: a GPU-only build folder on the old
scratch drive (removed 2026-11-18; that folder was not kept). Today the
integration build `"Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\z"` builds the same
programs, with the GPU sub-build in `"Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\z\gpu"`.
The GPU-only build was configured with
`-DAZ_ONLY="NONE" -DAZ_TESTS="core" -DAZ_TOOLS="run" -DAZ_TWINS="NONE" -DAZ_COMPONENTS="gpu"`.

### `test_gpu.exe`: 135 checks, 0 failures

```
no-driver path : status=1 "no NVIDIA driver: LoadLibrary(Z:\no-such-folder\nvcuda.dll) failed, Windows error 126"
wrong-dll path : status=1 "kernel32.dll lacks the symbol cuGetErrorName (driver too old or not a CUDA driver)"
bad-module path: status=4 "the driver refused the PTX (.version , .target ): CUresult 200 (CUDA_ERROR_INVALID_IMAGE). error   : Can't loa..."
gpu            : NVIDIA GeForce RTX 5070 Ti Laptop GPU, compute 12.0, 12199 MiB, driver API 13040; PTX 21851 bytes (.version 7.0, .target sm_80) loaded in 304 us
philox         : 1000000 blocks, gpu vs cpu differing blocks = 0, run 1 vs run 2 = 0
brain          : 1000000 steps over 320 clades; differing creatures gpu vs cpu: 0 of 875006 in contract, 0 of 124994 beyond it; run 1 vs run 2 identical: yes
brain digest   : cpu 0b96c6a551fde068 gpu 0b96c6a551fde068 (hid changed in 999981 creatures, 9437313 of 16000000 motor values at the clamp)
brain recurrent: 100000 creatures x 8 steps on device buffers; differing hid = 0, motor = 0 (state after step 8 differs from step 1 in 83876 creatures)
brain guards   : out-of-table clade and short buffers handled; control (one bias word changed per clade) differs in 806 of 4096 creatures
ok: 135 checks, 0 failures
```

What the checks cover:

- Philox: three published known-answer vectors on CPU and GPU; 1,000,000
  blocks (random counters mixed with simulation-style counters) equal to the
  CPU; a second run with another block size (1024 instead of 256) equal to
  the first; a changed key changes all 4096 compared blocks.
- Brain, 1,000,000 creatures, 320 clades: 200 developed with
  `az_dense_develop` from random neural programs (shifts 1..15), 100 raw
  random networks at the edge of the contract in `core/az_brain.c` (any int8
  weight, |bias| up to 2^20, shifts 0..15, six hand-made extremes), 20 beyond
  the contract (any i32 bias, shifts 0..31). Seven of eight creatures have
  inputs in 0..127; every eighth has any byte. Hidden state and motors equal
  the CPU for all of them; two GPU runs (block 256 and 1024) are equal.
- Batch sizes 1, 2, 255, 256, 257, 1000, 65537: equal to the CPU, and 64
  guard creatures after n are untouched.
- Eight recurrent steps with the hidden state staying on the device.
- Guards: out-of-table clade index, too-small buffers (`AZ_GPU_BAD_ARG`
  before any launch), out-of-range upload and download.
- The comparison can fail: one changed bias word per clade on the device
  changes 806 of 4096 creatures.

Two separate processes printed identical output except the load-time line
(`diff` of the two outputs without that line is empty).

Other configurations that passed the same 135 checks: `-DAZ_GPU_ARCH=sm_120`
(PTX 8.7). With `-DAZ_GPU_CLANG=` pointing at a missing file, and with the
environment variable `AZ_GPU_DLL` pointing at a missing DLL, the test printed
`SKIPPED ...` and exited 0 (11 checks, 0 failures).

### `az_gpubench.exe`: timings, **noisy, parallel load**

Other agents were using the CPU and the GPU during every run. Min and median
of 9 repetitions. 64 developed clades, inputs in 0..127. Every GPU result was
compared with the CPU bytes (equal in all repetitions).

Run A (`az_gpubench.exe 9` of that build, today
`"Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\z\az_gpubench.exe" 9`; output kept in
`"Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\work\c-scratch-2026-11-17\work\gpu\bench_A.txt"`):

| n | What | min ms | median ms | ns / creature (median) |
|---|---|---|---|---|
| 100,000 | CPU, 1 thread, C reference (`-O2 -march=x86-64-v3`) | 27.22 | 49.76 | 497.6 |
| 100,000 | GPU kernel only (launch + sync) | 1.35 | 1.40 | 14.0 |
| 100,000 | GPU round trip, pageable host memory | 5.07 | 6.21 | 62.1 |
| 100,000 | GPU round trip, page-locked host memory | 2.39 | 2.99 | 29.9 |
| 1,000,000 | CPU, 1 thread, C reference | 399.54 | 411.53 | 411.5 |
| 1,000,000 | GPU kernel only (launch + sync) | 12.98 | 13.04 | 13.0 |
| 1,000,000 | GPU round trip, pageable host memory | 30.85 | 34.27 | 34.3 |
| 1,000,000 | GPU round trip, page-locked host memory | 18.74 | 18.97 | 19.0 |

A round trip uploads sens, hid, gain, bias, clade (98 MB at n = 1,000,000)
and downloads hid and motor (48 MB). Empty kernel, launch + sync: min 7.6 us,
median 10.2 us.

Run B (a second build with `-DAZ_GPU_BENCH_TWIN=ON`, which also links
`kernels/az_brain_vnni.asm`; output kept in
`"Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\work\c-scratch-2026-11-17\work\gpu\bench_B.txt"`):

| n | What | min ms | median ms | ns / creature (median) |
|---|---|---|---|---|
| 100,000 | CPU, 1 thread, C reference | 22.45 | 35.39 | 353.9 |
| 100,000 | CPU, 1 thread, AVX-VNNI twin | 4.21 | 4.27 | 42.7 |
| 100,000 | GPU kernel only | 1.34 | 1.35 | 13.5 |
| 100,000 | GPU round trip, pageable | 3.82 | 4.77 | 47.7 |
| 100,000 | GPU round trip, page-locked | 2.12 | 2.39 | 23.9 |
| 1,000,000 | CPU, 1 thread, C reference | 408.47 | 491.06 | 491.1 |
| 1,000,000 | CPU, 1 thread, AVX-VNNI twin | 51.78 | 58.18 | 58.2 |
| 1,000,000 | GPU kernel only | 13.57 | 13.97 | 14.0 |
| 1,000,000 | GPU round trip, pageable | 45.62 | 51.63 | 51.6 |
| 1,000,000 | GPU round trip, page-locked | 23.38 | 25.72 | 25.7 |

Empty kernel in run B: min 7.1 us, median 8.2 us.

The CPU rows swing with the load. Two earlier runs of the same binaries the
same hour (output not kept) gave, at n = 1,000,000: C reference min 213.8 ms,
median 240.6 ms; AVX-VNNI twin min 41.4 ms, median 42.1 ms; GPU kernel median
12.9 to 13.0 ms.

What these numbers say and do not say:

- Across these runs the GPU kernel took 13 to 14 ns per creature, the C
  reference on one thread 220 to 500 ns, the assembly twin on one thread 42
  to 58 ns.
- With the copies, the GPU took 19 to 30 ns per creature from page-locked
  memory and 34 to 62 ns from pageable memory. That is about twice as fast
  as, or about equal to, ONE CPU thread running the twin. The machine has
  24 cores. A multi-threaded CPU brain pass was not measured here, so there
  is no measured claim that the GPU brain step with round trips beats the
  CPU path, and the arithmetic suggests it would not.
- Not measured: multi-threaded CPU, asynchronous copies, streams, a kernel
  tuned for the GPU (the device code is the plain C reference).

## Limits (fact)

- One GPU, one driver version, Windows only.
- Not connected to `az_tick`. No world column lives on the device.
- Synchronous copies, default stream, one context, one thread.
- `cuCtxCreate_v2` is used; whether later drivers keep that export is unknown.
- "No driver" was rehearsed by pointing at a missing or wrong DLL; the code
  was not run on a machine that really has no NVIDIA driver.
- Shift counts of 32 or more are undefined in C and behave differently on
  x86 (masked) and in PTX (clamped). The contract forbids them (shifts 0..15)
  and the test stays below 32.
- The device kernel is unoptimised for a GPU: byte loads, per-thread local
  arrays, one thread per creature.
- No prior-art search was done. Driver API plus clang NVPTX is a known,
  documented route; nothing here is claimed as new.

---

# Part 2 - DESIGN: moving the whole tick to the device

**Status: design only. Not implemented, not measured, not verified.** Every
statement below is a plan or an expectation unless it cites Part 1.

## Why the whole tick, not single kernels

Part 1 shows the price of a round trip: at n = 1,000,000 the copies added
about 6 to 12 ms (page-locked) or 21 to 38 ms (pageable) to a 13 to 14 ms
kernel (medians), for this one stage and its 146 MB. A tick that
keeps its columns on the CPU and ships one stage to the GPU pays that price
for every stage. The plan is therefore: the state lives on the device, the
tick runs there as a sequence of kernels, and only small things come back
(events, statistics, the state hash, and positions when a viewer asks).
The fixed cost of a launch was 7 to 10 us (Part 1, median), so some tens of kernels
per tick cost well under a millisecond of overhead.

## Device image of the world

1. **Columns as device buffers with the same layout.** The world is one
   arena. Allocate one device buffer of `arena.size` bytes and upload the
   arena as it is: every column keeps its element type, its `per` and its
   offset. Nothing is converted, so a download is a valid CPU world again
   and CPU and GPU can be compared column by column with `memcmp`.
   The column registry (`az_n_columns` / `az_column`, and the cell pair)
   gives name, element size and tier of every column for partial uploads,
   downloads and comparisons.
2. **A device-side table of pointers.** `AzWorld` itself is that table. Copy
   the struct, call `az_world_rebase(copy, device_base - host_base)` (the
   lead wrote it for this: it shifts every column pointer and clears the
   host-only members), upload the copy. Every kernel then takes one
   parameter, the device address of the `AzWorld`, plus its range, exactly
   like a CPU job takes `(w, begin, end, worker)`. Module code that says
   `w->energy[slot]` compiles unchanged.
3. **Layout guard.** Extend `az_gpu_layout.h` from 16 words to the whole
   struct: `sizeof(AzWorld)`, the offset of every column pointer (generated
   from the two `.def` files), `sizeof` of `AzPub`, `AzPair`, `AzPercept`,
   `AzCellSense`, `AzColony`, `AzTally`. The ABI rule "fixed-width members,
   explicit padding, no pointers" for structs sent to a GPU makes this pass
   by construction; the guard catches a mistake.
4. **Same source.** The device translation unit includes the module sources
   under the internal-linkage pragma, as it does today for the brain and
   Philox. `worker` is 0 on the device (the ABI already says so).

Memory: the arena for 1,000,000 creatures is far below the 12 GB of this
GPU (hot tier 140 bytes per creature; the exact figure is `az_world_bytes`).

## Three kinds of stage

- **Direct**: one thread per entity, cell or block runs the same C function.
  Correct on the GPU for the same reason it is correct on N CPU threads:
  rules D1 and D2 (write only your own slot, read others only through
  frozen data) plus integer atomics. Integer atomic add matched the CPU bit
  for bit in the research sample (`docs/research/platform/cuda_ptx.md`).
- **GPU-specific algorithm**: the CPU does it serially and a serial loop is
  not acceptable on the device; a different parallel algorithm must produce
  the same bytes.
- **Serial fallback**: any serial stage can first be ported as a kernel
  launched with ONE thread. It is slow but it is the same code, so it is
  correct by construction, and it keeps the state on the device. Replace it
  with a parallel algorithm later, when the state hash proves them equal.

## Stage by stage

| # | Stage | Kind | Plan |
|---|---|---|---|
| 0 | cell of each entity, public record `pub[]`, role bit | direct | pure function of the entity's own columns |
| 0 | cell lists `cell_start[]`, `order[]` | **GPU-specific: counting sort** | see below |
| 0 | reorder of all HOT and WARM columns | direct after the sort | one gather kernel per column through the permutation, into a second buffer; the registry lists the columns |
| 0 | free list of dead slots | **GPU-specific** | stream compaction (prefix sum over "dead"); the k-th dead slot links to the k+1-th; must reproduce the order `az_grid_build` defines |
| 1 | `az_sense_cell` | direct, restructured | one thread per occupied cell into an `AzCellSense[n_cells]` buffer; the CPU computes it lazily, the values are the same |
| 1 | scan, sense, brain, intents, demand, steer | direct | one thread per sorted position; the brain step is today's kernel; demand goes to cells through atomic adds |
| 2a | claims | direct | atomic max of `(score, ~uid)`; max does not depend on arrival order |
| 2b | settle; cell grants | direct | one winner per target; one thread per cell |
| 2b | `az_colony_grant` | serial fallback, then one thread per colony | at most 4096 colonies |
| 3 | pair outcomes and the `*_range` functions | direct | one thread per 128-slot block running the range functions as the CPU does |
| 3 | tallies | **GPU-specific** | see below |
| 3 | `az_colony_commit` | serial fallback, then one thread per colony | |
| 4 | commit of entity deltas, chem and weather on the strided cells, flux commit | direct | the stride already guarantees that no two face-adjacent cells are stepped together |
| 4 | `az_weather_global` | serial fallback | small global state |
| 5 | **life commit** | **GPU-specific: serial on the CPU** | see below |
| 6 | Tier 1 | direct | only entities with `(uid + tick) % K1 == 0` do work |
| 6 | Tier 2 colonies | serial fallback, then per colony | |
| 6 | ledger | host | download the event buffers; the host sorts and writes them |
| - | state hash, matter and energy totals | direct + reduction | per-entity terms summed with atomic adds; lets a test compare CPU and GPU every tick without downloading the world |

### The counting sort (stage 0)

The CPU builds the cell lists with a serial counting sort and orders each
cell by uid. On the device:

1. histogram: one thread per entity, atomic add on `count[cell]` (exact);
2. exclusive prefix sum over the cells -> `cell_start[]` (a standard
   parallel scan; the result is unique, so any correct scan gives the CPU's
   bytes);
3. placement: each entity takes a position inside its cell with an atomic
   cursor. This order depends on thread timing, so it is NOT the result yet;
4. canonical order: sort each cell's entries by uid (one thread per cell, a
   small in-place sort; uids are unique, so the result is unique). A cell
   with very many entities needs a parallel sort of its own;
5. `spos[order[p]] = p`.

Alternative: one deterministic stable radix sort on the key (cell, uid) and
a boundary search for `cell_start[]`. Either way the output is a pure
function of the keys, which is what makes it bit-identical to the CPU.

### Tallies and events (stage 3)

On the CPU each worker adds to its own `AzTally` without atomics. On the
device thousands of threads share the token `worker = 0`, so a plain `+=`
on one tally would lose counts (the research sample lost 93 percent of its
counts without the atomic). Options: (a) one tally per thread block, summed
by a reduction kernel; (b) atomic adds. Both give the CPU's totals because
integer sums commute. Either needs the tally update to go through a call
the device build can redefine. Events have the same problem; an atomic
cursor into one buffer works if the ledger's sort key is total, so that the
sorted list does not depend on arrival order. Both points need the lead.

### The serial life commit (stage 5)

The CPU applies births in (mother uid, n) order, hands out uids one after
another, takes slots from the free list and updates clades. Parallel plan:

1. compact the birth requests (prefix sum over the per-mother counts);
2. sort them by (mother uid, n);
3. child uid = `uid_next` + rank in that order: the same numbers the serial
   loop hands out;
4. child slot = the rank-th entry of the free list (the list must exist as
   an array in its canonical order); births whose rank is beyond the free
   count are denied, as on the CPU;
5. genome, development and placement of each child: one thread per child
   (meiosis and mutation draw from Philox keyed by uids, so children are
   independent);
6. clade reference counts: atomic adds; founding a new clade by
   macro-mutation allocates clade slots in serial order and is rare: keep
   it in a one-thread kernel.

First step in practice: the whole life commit as a one-thread kernel.

## What can differ between CPU and GPU, and how each is held

| Risk | Held by |
|---|---|
| floating point | none in `core/`; the embed step rejects any float type in the PTX |
| signed overflow | `-fwrapv` on both sides; rule D5 forbids it anyway |
| shift count >= width (x86 masks, PTX clamps) | rule D5; found by comparing on random inputs |
| division by zero | rule D5 |
| order of atomic updates | only add, max, min are used across threads; all are order-independent on integers |
| iteration order inside a cell | always by uid (D3); the device sort must end in uid order |
| function pointers (`w->k.brain_step`) | the device calls the C reference directly; the rebased world has the pointers cleared |
| struct layout | the layout guard |
| recursion, libc | not in `core/`; an unresolved call stops the build |

## Order of work and proof at each step

1. Device image: upload, rebase, download; `memcmp` of the arena after a
   round trip with no kernel.
2. State hash and the two totals on the device; equal to the CPU on the
   reporting seeds.
3. One stage at a time, in pipeline order. For each: start CPU and GPU from
   the same snapshot, run the stage on both, download the columns the stage
   writes, `memcmp`. A stage is "on the GPU" only when this passes for
   several seeds and world sizes, including n not a multiple of the block.
4. Whole tick on the device: state hash equal to the CPU after every tick
   of a long run, with serial fallbacks where needed.
5. Only then replace fallbacks by parallel algorithms and tune kernels
   (for example a PTX twin of the brain step using the four-way byte dot
   product instruction, held to the same bit-identity test as the assembly
   twins), measuring each change.

## What this design needs from other owners (not requested yet)

- The per-entity bodies of the tick jobs in `core/az_tick.c` are `static`
  functions inside that file. To keep one definition, the device needs them
  in a form it can include (an inline header shared by `az_tick.c` and
  `gpu/az_gpu_dev.c`).
- Tally and event updates through a call the device build can redefine.

Already in the ABI (this list used to ask for it; finding v82): the canonical
order of the dead-slot free list. `core/az_abi.h` states it at
`az_grid_build`: the list holds every dead slot, chained through `spos[]`
from the highest slot down.
