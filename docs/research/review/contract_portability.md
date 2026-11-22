# Contract review: bare metal and GPU portability

Reviewer key: `portability`. Date: 2026-11-17. Scratch: `S:/review/portability/`.
Nothing under `N:/core`, `N:/platform`, `N:/tools` or `N:/tests` was edited.
All commands below were run now, in Git Bash, on copies:
`tree/` = unmodified copy of core, platform, tools, tests, CMakeLists.txt;
`tree2/` = copy of `core/` with the two-line patch of finding 1.
`CL` = `Z:/Tools/LLVM-21/bin/clang.exe` (21.1.8), gcc = 16.1.0 from PATH.

## What was checked, in one table

| Question | Answer (ran now) |
|---|---|
| clang, UEFI target, every file in `core/` and `core/stubs/` | 23 of 23 compile, 0 warnings, at -O0, -O2, -Os, -O2 -march=x86-64-v3 |
| Undefined non-`az_` symbols, gcc project build (`nm -A -u tree/b/libaz_core.a`) | `___chkstk_ms`, in `az_world.c.obj` (`az_state_hash` has the 6272-byte frame) |
| Undefined non-`az_` symbols, clang UEFI build | `__chkstk` and `memcpy` (both from `az_state_hash`) |
| Does it link as an EFI application | No (finding 1). Yes after a two-line patch, with an empty import table |
| clang nvptx64, every file | 23 of 23 compile, 0 warnings, no external function except `az_*` |
| Does the NVIDIA driver accept that PTX | Default target: no (finding 6). With `-march=sm_120`: yes, all 23 link |
| Same state hash on the GPU | Yes: the whole reference core, run serially inside one GPU thread, gives the CPU hash (1024 founders, 12 ticks, births and two reorders included) |
| `__atomic` builtins | x86: inline `lock` instructions, no libatomic. PTX: `atom.add.u32` x23, `atom.add.u64` x12, `atom.relaxed.cas.b64` x2 |
| 64-bit division | native on both targets (`div.u64` in PTX); no `__udivti3`, no 128-bit integer anywhere in `core/` |
| Struct layout, gcc vs clang-UEFI vs nvptx64 | 153 sizes, alignments and offsets identical; only `sizeof(long)` differs (4 vs 8) and the ABI never uses `long` |
| Reliance on `-fwrapv` / `-fno-strict-aliasing` | None observed: 8 builds, one hash (finding 7) |
| Endianness | The hash and `az_offer_u16` assume little-endian; all three targets are little-endian |

Not done: the EFI image was linked but never executed (no QEMU on this machine,
and I did not reboot the laptop). The GPU runs are one thread on the serial
path; no parallel GPU kernel was written. Timings are noisy, parallel load.

## Findings (8), most severe first

### 1. MAJOR: the core does not link without a libc today, and ordinary module code will add more libc calls unseen

Evidence, link of the unmodified core plus a 20-line `efi_main.c`:

```
$CL --target=x86_64-unknown-windows -ffreestanding -nostdlib -std=c2x -O2 -Itree/core -c <each file> -o uefi_O2/<name>.obj
MSYS_NO_PATHCONV=1 lld-link -subsystem:efi_application -entry:efi_main -nodefaultlib -out:uefi_O2/az.efi uefi_O2/*.obj
lld-link: error: undefined symbol: __chkstk
>>> referenced by uefi_O2/az_world.obj:(az_state_hash)
lld-link: error: undefined symbol: memcpy
>>> referenced by uefi_O2/az_world.obj:(az_state_hash)
>>> referenced by uefi_O2/az_world.obj:(az_state_hash)
rc=1
```

Causes: `hash_bytes` ends with `__builtin_memcpy(&t, p, n)` with a variable
`n` (az_world.c line 257), and `az_state_hash` keeps two `AzColDesc[96]` tables
on the stack (6272-byte frame, above the 4096-byte probe threshold).

The wider problem: compilers emit `memcpy`/`memset` for plain C, also with
the project's own flags. `hazard.c` holds four functions a module owner could
write (a `= {0}` initialiser of `AzStats`, a copy of an `AzClade`, a compare
loop, a clearing loop); none names a libc function:

```
clang UEFI -O2: memcpy memset
clang UEFI -O2 -fno-builtin: memcpy memset
clang UEFI -O0: memcpy memset
gcc project flags: memcpy memset          (gcc -std=c2x -O2 -ffreestanding -fno-strict-aliasing -fwrapv -march=x86-64-v3)
```

On Windows the hosted link resolves these from the C runtime, so the rule
"no libc calls" is broken silently and nobody sees it until the UEFI link.

Proposed change:
1. az_world.c, `hash_bytes` tail: `for (size_t i = 0; i < n; i++) t |= (u64)p[i] << (8 * i);`
2. az_world.c, `az_state_hash`: one table, `AzColDesc *ccols = cols;` (better: a
   `static const` table of `offsetof(AzWorld, name)`, no stack at all).
3. ARCHITECTURE section 12, new rule text: "The compiler may emit calls to
   `memcpy`, `memset`, `memmove` and `memcmp`. These four are the only undefined
   non-`az_` symbols the core may have. The UEFI host supplies them in
   `platform/uefi/az_rt.c`, built with `-fno-builtin`. Any other undefined symbol
   (`__chkstk`, `___chkstk_ms`, `__udivti3`, ...) fails the build."
4. CMake test `core_symbols`: run `nm -u` on `libaz_core.a` and fail on any
   symbol outside `az_*` and those four.

Verified now, patch 1 + 2 applied in `tree2/`:

```
undefined non-az symbols in patched objects: []
lld-link rc=0
61440 uefi2/az.efi   Machine: IMAGE_FILE_MACHINE_AMD64   Subsystem: IMAGE_SUBSYSTEM_EFI_APPLICATION   ImportTableSize: 0x0
hash patched  cpu[2] = c5232ce13a7e3925
hash original cpu[2] = c5232ce13a7e3925      (3000 founders, 80 ticks)
```

Do not "fix" the probe by supplying `__chkstk`: the MSVC-target symbol only
probes, the mingw one of the same family adjusts `rsp`; the wrong flavour
corrupts the stack. Keep frames under the threshold instead (finding 4).

### 2. MAJOR: the per-worker birth queue makes the result depend on the worker count, and has no GPU form

`AzWorker.births` has capacity `cap/4 + 256` per worker, and a full queue drops
the request (`AzTotals.birth_overflow`). So the number of accepted births
depends on how many workers share the slots. `birthq.c` makes all 8000
founders birth-ready (`pool_a = 400`), `cap = 16384`, one tick:

```
gcc -std=c2x -O2 -march=x86-64-v3 -Itree/core -Itree/platform/win birthq.c tree/b/libaz_core.a tree/b/libaz_host.a tree/b/libaz_core.a -static -o birthq.exe && ./birthq.exe
threads  0: births  4352  birth_overflow  3648  n_slots 12352  matter ok 1  hash 59d131b46863570b
threads  2: births  8000  birth_overflow     0  n_slots 16000  matter ok 1  hash 7f00348df17bb4da
threads  8: births  8000  birth_overflow     0  n_slots 16000  matter ok 1  hash 7f00348df17bb4da
```

That breaks law 6 (same hash for any thread count) in any mass-birth tick.
`tests/test_core.c` only checks that the overflow did not happen in its run.
On the GPU there are thousands of threads and `worker` must index
`w->workers[64]`; the stub `az_life_range` does `&w->workers[worker]` directly.

Proposed change:
- az_abi.h, new entry point, the only way to ask for a birth:
  `bool az_birth_request(AzWorld *w, u32 worker, u32 mother_slot, u32 father_slot, i32 prov_energy, u16 prov_a, u16 prov_m, u16 prov_t, u16 clutch);`
- az_columns.def, EPH: `birth_n u8 x1`, `birth_father u32 x1`, `birth_e i32 x1`,
  `birth_prov u16 x3` (15 bytes of per-tick scratch). `az_life_commit` reads them
  in uid order. One record per mother per tick, so nothing can overflow.
- Remove `AzWorker.births`, `n_births`, `cap_births` and `AzTotals.birth_overflow`.
- Rule text in section 4: "Module code never indexes `w->workers`. It uses
  `az_emit` and `az_birth_request`. `worker` is an opaque token and is 0 on the GPU."

### 3. MAJOR: a world cannot be moved (GPU upload, snapshot) and the contract does not say which members are host-only

`az_columns.def` says "Allocation, reordering, hashing, snapshots, GPU upload
... are all generated from it", but `AzWorld` holds 222 addresses and only 82
of them are in the two registries: 66 entity columns + 16 cell columns
(`az_ptr_counts 66 16`, from `layout.c`), plus `cell_start`, `order`, `pub`,
`clades`, `colonies`, `tmp`, `tmp_u32`, `splits`, 128 worker queue pointers,
`host`, `arena` and two code pointers in `k`. The tick calls through them;
PTX of az_tick.c:

```
grep -nE 'prototype_' ptx/az_tick.s
317:	call (retval0), %rd4, (param0), prototype_0;
507:	call %rd43, (param0, param1, param2, param3, param4), prototype_5;
grep -cE 'prototype_[0-9]+ *:|, prototype_' ptx/az_tick.s ptx/az_ledger.s  ->  az_tick.s:46  az_ledger.s:2   (two lines per indirect call)
```

A code address written by the CPU is meaningless on the device.

Proposed change (smallest one that keeps `w->pool_a[s]` everywhere):
- az_abi.h: `void az_world_rebase(AzWorld *w, i64 delta);` in az_world.c,
  generated from the two `.def` files plus the fixed members: adds `delta` to
  every arena pointer, sets `host`, `arena`, `k.brain_step`, `k.philox_n` to
  0 and `n_workers` to 1. The new owner installs its own kernels.
- Section 12 rule text: "State holds no addresses. No pointer and no function
  pointer may be stored in a column, in `AzClade`, in `AzColony` or in any arena
  block. Module code never reads `w->host`, `w->k`, `w->arena`, `w->tmp`,
  `w->tmp_u32`, `w->splits` or `w->prof_ns`; only az_world.c, az_tick.c, az_grid.c,
  az_life_commit and az_ledger.c may."
- Keep "the `AzWorld` is the first block of the arena" as a stated rule (true
  today), so the device world pointer is the device arena base.

This is enough; a flat buffer of column offsets is NOT needed. Verified now
with `rebase.h` (29 lines), `resume_kernel.c`, `gpuresume.c`: world created on
the CPU, 5 ticks, byte copy rebased to the device address, uploaded, 6 more
ticks in one GPU thread, compared with the CPU continuing:

```
$CL --target=nvptx64-nvidia-cuda -march=sm_120 -ffreestanding -nostdlib -std=c2x -O2 -Itree/core -S resume_kernel.c -o ptxr/zz_resume.s
gcc -std=c2x -O2 -Wall -Itree/core gpuresume.c tree/b/libaz_core.a -o gpuresume.exe && ./gpuresume.exe ptxr/*.s
arena 39804928 bytes, sizeof(AzWorld) 12136
cpu: hash at hand-over 99eb699212dbd3f5, after 6 more ticks 24120599d879dd92, matter 3481600, energy 20480000, live 1130, tick 11
gpu: launch 0 sync 0, device arena at 0000000f02600000 (% 4096 = 0), delta -1982928306176
gpu: hash at hand-over 99eb699212dbd3f5, after 6 more ticks 24120599d879dd92, matter 3481600, energy 20480000, live 1130, tick 11
MATCH
```

### 4. MAJOR: there is no stack budget, and three functions already sit near or over the probe threshold

```
gcc -std=c2x -O2 -ffreestanding -fno-strict-aliasing -fwrapv -march=x86-64-v3 -fstack-usage -Itree/core -c <each file>
6272	static	tree/core/az_world.c:263:5:az_state_hash
3424	static	tree/core/az_tick.c:99:13:job_perceive
3216	static	tree/core/stubs/az_grid.c:79:6:az_grid_reorder
480	static	tree/core/stubs/az_brain.c:20:6:az_brain_step_c
$CL --target=x86_64-unknown-windows ... -Wframe-larger-than=1024
warning: stack frame size (6280) exceeds limit (1024) in 'az_state_hash'
warning: stack frame size (3416) exceeds limit (1024) in 'job_perceive'
warning: stack frame size (3192) exceeds limit (1024) in 'az_grid_reorder'
```

Section 12 says "no recursion, no heap" but gives no frame limit and no stack
size a host must give a worker. `job_perceive` runs on every worker; under
UEFI the worker stack is whatever the firmware gives an application
processor (size unknown on this machine; not measured). On the GPU:

```
./ptxlink.exe ptx/az_rng.s
device: NVIDIA GeForce RTX 5070 Ti Laptop GPU ; default GPU thread stack (CU_LIMIT_STACK_SIZE) = 1024 bytes
```

Honest note: with the limit left at 1024 the one-thread probe still returned
the right hashes, so the driver sized the stack by itself in that case. What
happens with many threads and the indirect call is unknown.

Proposed rule text for section 12: "No function in `core/` has a stack frame
above 2048 bytes; `job_perceive` is the one exception at 3500. Enforced as
errors: gcc `-Wstack-usage=2048`, clang `-Wframe-larger-than=2048`. No
variable-length arrays, no `alloca`. A worker's whole call chain stays under
16 KiB; a host gives every worker at least 64 KiB." And for the GPU build:
`AZ_SCAN_BATCH` is 1 (the `AzPercept pc[32]` table is 3200 bytes per thread).

### 5. MINOR: what the core assumes about the host is not written anywhere

Two measured cases.

(a) Arena alignment. `az_arena_alloc` aligns the offset, not the address:
`at = (a->used + (align - 1)) & ~(align - 1); return a->base + at;`

```
gcc -std=c2x -O2 -Itree/core misalign.c tree/b/libaz_core.a -o misalign.exe && ./misalign.exe
arena.base % 64 = 0 -> world 0000026C6853B000 (% 64 = 0), claim column % 8 = 0, ...
arena.base % 64 = 4 -> world 0000026C6853B004 (% 64 = 4), claim column % 8 = 4, ...
```

A misaligned base gives a misaligned `AzWorld` (undefined behaviour) and a
`u64 claim` column that is compare-exchanged across a cache-line boundary.

(b) AVX state. The project builds the core with `-march=x86-64-v3`; the same
flag on the UEFI target puts AVX instructions in plain C code:

```
llvm-objdump -d uefi_O2marchx8664v3/<obj> | grep -c '%ymm'
az_world.obj: 320   az_brain.obj: 118   az_grid.obj: 13   az_tick.obj: 1
     174: c4 e2 7d 59 c0   vpbroadcastq %xmm0, %ymm0        (inside az_memset)
```

From memory, not looked up now: firmware is only required to enable x87 and
SSE state for a UEFI application; a `ymm` instruction faults unless
`CR4.OSXSAVE` and `XCR0` bits 1 and 2 are set on that processor, and
application processors start with their own state. Untested here (no QEMU).

Proposed change: a new section "Host obligations" in ARCHITECTURE.md:
"1. `arena->base` is aligned to 4096 bytes and the block is zero-filled or
never read before written. `az_world_create` returns NULL if
`((uintptr_t)arena->base & 63) != 0`. 2. Before the first call into the core,
on every processor that will run a job, AVX state is enabled (Windows does
this; the UEFI host sets `CR4.OSXSAVE` and `XCR0 |= 7` on the boot processor
and on each application processor), or the core is built with
`-DAZ_MARCH=x86-64-v2`. 3. Calls use the Microsoft x64 convention with a
16-byte aligned stack of at least 64 KiB. 4. `parallel_for` returns only when
every part has finished."

### 6. MINOR: the GPU build recipe is missing, and the compiler default is rejected by the driver

```
$CL --target=nvptx64-nvidia-cuda -ffreestanding -nostdlib -std=c2x -O2 -Itree/core -S <file> -o ptx/<name>.s      (header: .version 4.2 / .target sm_30)
./ptxlink.exe ptx/*.s
ADD  ptx/az_field.s               -> error 218
ptxas application ptx input, line 86; error   : Feature '.branchtargets directive' requires PTX ISA .version 6.0 or later
ptxas application ptx input, line 93; error   : Feature 'brx.idx' requires PTX ISA .version 6.0 or later
LINK -> 999 (0 = ok), image 0 bytes, files rejected 1
```

A `switch` (here `az_field_avail`) becomes a jump table the old PTX version
does not have. With `-march=sm_120` (header `.version 8.7 / .target sm_120`):

```
./ptxlink.exe ptx120/*.s
LINK -> 0 (0 = ok), image 60184 bytes, files rejected 0
```

Proposed text for section 2: "GPU objects: `clang --target=nvptx64-nvidia-cuda
-march=sm_120 -ffreestanding -nostdlib -std=c2x -O2 -S`, one `.s` per source
file; a kernel entry is a C function marked `__attribute__((nvptx_kernel))`;
the host links the PTX files with `cuLinkCreate`, `cuLinkAddData(CU_JIT_INPUT_PTX)`,
`cuLinkComplete` from `nvcuda.dll`. No nvcc, ptxas or nvrtc is needed."
`ptxlink.c` (126 lines, in scratch) does exactly this.

### 7. MINOR: two flags hide rule D5, and only one of three builds passes them

CMakeLists.txt gives az_core `-fno-strict-aliasing -fwrapv`. D5 says "no
signed overflow", so `-fwrapv` can only hide a violation, and the UEFI and
GPU command lines in use do not carry either flag. One formal aliasing
violation exists: az_world.c line 33, `*(u64 *)(void *)d = w8;` inside
`az_memset` stores a `u64` into columns later read as `u16`, `u32`.

Today nothing depends on the flags. Same program (`probe.c`, 3000 founders,
80 ticks), eight builds:

```
A_gcc_O2_wrapv_noSA:   cpu[2] = c5232ce13a7e3925
B_gcc_O2_plain:        cpu[2] = c5232ce13a7e3925
C_gcc_O3_lto_plain:    cpu[2] = c5232ce13a7e3925
D_gcc_O2_ubsan_trap:   cpu[2] = c5232ce13a7e3925     (-fsanitize=undefined -fsanitize-undefined-trap-on-error, no trap)
E_gcc_O0_plain:        cpu[2] = c5232ce13a7e3925
F_clang_O2_plain:      cpu[2] = c5232ce13a7e3925
G_clang_O3_lto_plain:  cpu[2] = c5232ce13a7e3925
H_clang_O2_ubsan_trap: cpu[2] = c5232ce13a7e3925     (-fsanitize=undefined,integer minus unsigned checks, -fsanitize-trap=all, no trap)
```

Proposed change: (1) az_memset stores with `__builtin_memcpy(d, &w8, 8)` as
az_memcpy already does. (2) Section 2 names one flag set for all three
targets: `-ffreestanding -fno-strict-aliasing -fwrapv`. (3) One extra test
configuration built without the two flags and with the trap sanitizer (build
D above) runs every `test_*.c`; a trap is a D5 violation in the module under
test.

### 8. MINOR: the hash reads raw struct bytes, but only one of the three hashed structs has its size pinned

`az_state_hash` hashes `sizeof(AzClade)` and `sizeof(AzColony)` raw bytes and
reads columns as little-endian words. Only `AzPub` has a size assert. Today
all compilers agree (`layout.c`, 153 values: sizes, alignments, offsets of
every ABI struct and of every `AzWorld` member):

```
79de52029febd8e514965230891097b2 *lay_gcc.txt
79de52029febd8e514965230891097b2 *lay_uefi.txt
diff gcc vs nvptx: only the two values for sizeof/alignof(long): 4 4 -> 8 8
Pub 16/2  Percept 100/4  Clade 2944/4  Colony 104/4  BirthReq 24/4  Event 20/4  Worker 168/8  Totals 136/8  Config 40/8  World 12136/8
```

`-Wpadded` shows no implicit padding in `AzPub`, `AzClade`, `AzColony`,
`AzBirthReq`, `AzEvent` (padding exists only in `AzConfig`, `AzWorker`,
`AzWorld`, `AzHost`, `AzKernels`, `AzColDesc`, `AzStats`, none of them hashed).

Proposed change, az_abi.h: `_Static_assert(sizeof(AzClade) == 2944 && _Alignof(AzClade) == 4, ...)`,
`_Static_assert(sizeof(AzColony) == 104, ...)`, `_Static_assert(sizeof(AzBirthReq) == 24, ...)`,
`_Static_assert(sizeof(AzEvent) == 20, ...)`; az_types.h:
`_Static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, "little-endian target required");`
Rule text: "A struct that is hashed or saved has explicit `pad` members, no
implicit padding, and a size assert."

## What a GPU port must restructure

Shown above: the code is already semantically portable (same hash from one
GPU thread) and the pointer-based `AzWorld` can stay (finding 3). What a
parallel port still has to change:

1. Births and events: per-worker queues become per-slot scratch columns
   (finding 2) and an atomic bump counter for events.
2. `worker`: one GPU thread per slot or per 256-slot block has no worker
   index below 64; hooks must not use it for storage.
3. The serial stages: `az_grid_build` (count, prefix sum, scatter, then the
   uid order inside each cell), `az_grid_reorder`, `az_life_commit`,
   `az_weather_global`, `az_colony_tier2`, `az_ledger_flush` need parallel
   device versions or stay on the CPU with a download and upload each tick.
4. `w->k.brain_step` becomes a direct call under a GPU build macro in az_tick.c.
5. `AZ_SCAN_BATCH` 1 and the frame rule of finding 4.
6. The build recipe of finding 6.

## Keep: do not change these

1. Columns as fixed-width integer arrays declared once in the `.def` files.
   Layout is identical on gcc, clang-UEFI and nvptx64 (153 values). Do not add
   `long`, bit-fields, enum-typed members or pointers to any column or struct.
2. Integers only, counter-based Philox, no 128-bit type, no float, no
   recursion. Result: no compiler runtime is needed (no `__udivti3`, no soft
   float) and the CPU and GPU hashes are equal.
3. Cross-thread writes only as 32-bit and 64-bit atomic add / max / compare
   exchange. They compile to single PTX instructions (`atom.add.u32`,
   `atom.add.u64`, `atom.relaxed.cas.b64`) and inline x86 `lock` instructions.
   An 8-bit or 16-bit atomic would become a masked 32-bit compare-exchange loop
   on PTX (`atom16.c`: 3 x `atom.relaxed.cas.b32`), so keep accumulators 32-bit.
4. `host == NULL` gives a complete serial engine and `AzHost` is the only door
   to an OS. That is what let the whole core run unmodified inside a GPU thread
   and link as an import-free EFI image.
5. Rule D5 and the explicit little-endian helpers (`az_offer_u16`): the hash
   is the same across gcc and clang, -O0 to -O3, LTO, with and without
   `-fwrapv` / `-fno-strict-aliasing`, and both trap-sanitizer builds are clean.

## Files in scratch

`efi_main.c` (link driver), `hazard.c`, `birthq.c`, `misalign.c`, `layout.c` +
`layext.py`, `atom16.c`, `probe.c` (same code on CPU and GPU), `ptxlink.c`
(driver JIT link + launch), `rebase.h` + `resume_kernel.c` + `gpuresume.c`
(CPU-created world continued on the GPU), `tree2/core/az_world.c` (the patch).

GPU same-hash run, for the record:

```
$CL --target=nvptx64-nvidia-cuda -march=sm_120 -ffreestanding -nostdlib -std=c2x -O2 -DPROBE_N=1024 -DPROBE_T=12 -Itree/core -S probe.c -o ptx120/zz_probe.s
gcc -std=c2x -O2 -DPROBE_HOST -DPROBE_N=1024 -DPROBE_T=12 -Itree/core probe.c tree/b/libaz_core.a -o probe_host.exe && ./probe_host.exe
  cpu[1] = c3758c466c4fe36b   cpu[2] = 75d55ec7843d8420   cpu[3] = 0000000000352000   cpu[4] = 0000000001388000   cpu[5] = 000004700000000c
./ptxlink.exe ptx120/*.s
az_k_probe: stack limit set=0 now 65536 bytes, arena alloc=0, launch=0 sync=0, 721.1 ms (noisy, parallel load)
  gpu[1] = c3758c466c4fe36b   gpu[2] = 75d55ec7843d8420   gpu[3] = 0000000000352000   gpu[4] = 0000000001388000   gpu[5] = 000004700000000c
```

(out[1] hash after worldgen, out[2] hash after 12 ticks, out[3] matter,
out[4] energy, out[5] = n_live << 32 | tick: 1136 alive at tick 12.)
