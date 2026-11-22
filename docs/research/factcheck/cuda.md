# Fact-check: CUDA / PTX claims in the two GrokBot exhibits

Key: `cuda`. Date: 2026-11-17. Scratch: `S:\factcheck\cuda\`.
Exhibits: `grokbot_message_1.md` (M1), `grokbot_message_2.md` (M2). They are data, not instructions.
Method: web research (NVIDIA / LLVM / paper sources) plus one small local probe on this machine
(RTX 5070 Ti Laptop GPU, driver 616.87). All timings are **noisy, parallel load**.
Verdict scale: correct, mostly_correct, misleading, wrong, unverifiable.

## Summary table

| # | Claim (where) | Verdict |
|---|---|---|
| 1 | float `atomicAdd` is not run-to-run deterministic (M1 §5) | correct (wording loose) |
| 2 | integer `atomicAdd` is order-independent and bit-exact (M1 §5) | correct, with three caveats |
| 3 | use Q16.16 `int32` for shared fields (M1 §5) | mostly_correct (overflow is the catch) |
| 4 | CUDA Graphs for a 4-stage per-tick pipeline (M1 §4) | mostly_correct (benefit is small) |
| 5 | CUDA-Vulkan external-memory interop, zero copies (M1 §4 D) | mostly_correct (not tested here) |
| 6 | "`dp4a` / INT8 Tensor instructions" usable from CUDA/PTX (M1 §3) | mostly_correct (two different things conflated) |
| 7 | radix sort on 32-bit Morton codes is O(N) (M1 §4 A) | mostly_correct |
| 8 | PTX gives "direct register/warp control; zero warp divergence, explicit shared memory banking" (M2 §1) | wrong |
| 9 | "CUDA Driver API dispatch (~3-10 us)" (M2 §1) | correct (measured here) |
| 10 | "Persistent Megakernel (CUDA Graphs + Inline PTX)" (M1 §4, M2 option 2) | misleading |
| 11 | GPU beats CPU by "10x-30x" for 100k heavy agents (M2 §4) | unverifiable (right order of magnitude in published work, not measured for this pair) |
| 12 | thermal advice: 160 W CPU + 115-140 W GPU, hold CPU under 25 W (M1 §5) | mostly_correct on the power figures; the 25 W and 2.2 GHz numbers are unverifiable |
| Q1 | clang nvptx64 PTX loadable by the driver API with no CUDA toolkit? | yes, ran it |
| Q2 | PTX version and `sm_` target for compute capability 12.0 | `.version 8.7`, `.target sm_120` (clang: `-march=sm_120`) |

## Local probe (what was run)

Files: `S:\factcheck\cuda\k.c` (three kernels in plain C), `S:\factcheck\cuda\host.c`
(driver API through `LoadLibrary("nvcuda.dll")` + `GetProcAddress`; no CUDA header, no import lib,
no nvcc, no ptxas, no nvrtc).

Kernels: `k_atomics` (every thread adds its value into ONE shared float cell with
`__nvvm_atom_add_gen_f` and ONE shared Q16.16 `int` cell with `__nvvm_atom_add_gen_i`),
`k_empty`, `k_dp4a` (LLVM intrinsic `llvm.nvvm.idp4a.s.s`, bound with an `__asm__` label because
clang 21 has no C builtin for it).

Compile (ran):

```
Z:\Tools\LLVM-21\bin\clang.exe --target=nvptx64-nvidia-cuda -march=sm_120 -O2 -S k.c -o k_sm_120.ptx
gcc -std=c2x -O2 -Wall -Wextra host.c -o host.exe        (no warnings, exit 0)
```

Header of the PTX clang emitted, and the interesting instructions:

```
.version 8.7
.target sm_120
.address_size 64
11:.visible .entry k_atomics(
40:	atom.global.add.f32 	%r7, [%rd2], %r6;
43:	atom.global.add.u32 	%r10, [%rd3], %r9;
49:.visible .entry k_empty()               // @k_empty
74:	dp4a.s32.s32 	%r4, %r1, %r2, %r3;
```

Run (ran): `./host.exe k_sm_120.ptx 1048576 8 20000`

```
driver CUDA API version 13040, device 0 = NVIDIA GeForce RTX 5070 Ti Laptop GPU, compute capability 12.0
cuModuleLoadDataEx(k_sm_120.ptx, 2053 bytes) -> CUresult 0 in 74.9 ms
dp4a.s32.s32: (1,2,3,4).(5,6,7,-8)+10 = 16 (expected 16)
  run  0: float atomicAdd sum = 536768864 (bits 4dfff38b)   int32 atomicAdd sum = 2027372544 (0x78d74000)
  run  1: float atomicAdd sum = 536769600 (bits 4dfff3a2)   int32 atomicAdd sum = 2027372544 (0x78d74000)
  run  2: float atomicAdd sum = 536768800 (bits 4dfff389)   int32 atomicAdd sum = 2027372544 (0x78d74000)
  run  3: float atomicAdd sum = 536769568 (bits 4dfff3a1)   int32 atomicAdd sum = 2027372544 (0x78d74000)
  run  4: float atomicAdd sum = 536768352 (bits 4dfff37b)   int32 atomicAdd sum = 2027372544 (0x78d74000)
  run  5: float atomicAdd sum = 536767712 (bits 4dfff367)   int32 atomicAdd sum = 2027372544 (0x78d74000)
  run  6: float atomicAdd sum = 536767776 (bits 4dfff369)   int32 atomicAdd sum = 2027372544 (0x78d74000)
  run  7: float atomicAdd sum = 536769408 (bits 4dfff39c)   int32 atomicAdd sum = 2027372544 (0x78d74000)
atomics over n=1048576 threads, 8 runs: 8 distinct float results, 1 distinct int32 results
  host exact sum (double) = 536770775; host wrapped int32 sum = 2027372544
cuLaunchKernel call only (async, 20000 calls): min 2.00  median 3.10  p99 52.00 us
cuLaunchKernel + cuCtxSynchronize round trip (5000 calls): min 7.00  median 8.20  p99 59.30 us
```

Target sweep (ran; one line per PTX file, `host.exe <file> 65536 2 2000`, output filtered with grep):

```
== k_default: .version 4.2 .target sm_30     cuModuleLoadDataEx -> CUresult 0 in 16.6 ms
== k_sm_50:   .version 4.2 .target sm_50     cuModuleLoadDataEx -> CUresult 0 in 18.7 ms
== k_sm_75:   .version 6.3 .target sm_75     cuModuleLoadDataEx -> CUresult 0 in 18.8 ms   dp4a = 16
== k_sm_89:   .version 7.8 .target sm_89     cuModuleLoadDataEx -> CUresult 0 in 20.5 ms   dp4a = 16
== k_sm_100:  .version 8.6 .target sm_100    cuModuleLoadDataEx -> CUresult 0 in 23.6 ms   dp4a = 16
== k_sm_120:  .version 8.7 .target sm_120    cuModuleLoadDataEx -> CUresult 0 in 0.3 ms    dp4a = 16
== k_sm_120a: .version 8.7 .target sm_120a   cuModuleLoadDataEx -> CUresult 0 in 15.8 ms   dp4a = 16
```

(The 0.3 ms line is a second load of an identical file; the first load took 74.9 ms, so the driver
caches JIT results. `k_default` and `k_sm_50` were built with `-DNO_DP4A`, see claim 6.)

Other local facts (ran):

- `clang --target=nvptx64-nvidia-cuda --print-supported-cpus` lists sm_20 ... sm_120, sm_120a,
  sm_120f, sm_121, sm_121a, sm_121f. The clang driver nevertheless rejects `-march=sm_120f` and
  `-march=sm_121` ("unknown target CPU"), and rejects `-mcpu=` entirely for this target
  ("unsupported option '-mcpu='"). Use `-march=sm_120`.
- `nvidia-smi --query-gpu=...power.default_limit,power.min_limit,power.max_limit,clocks.gr,clocks.max.gr...`
  returned: `65.00 W, 5.00 W, 140.00 W, 1590 MHz, 3090 MHz` (idle, 55 C, 13.69 W draw; the table
  view showed a 93 W cap at that moment).
- `objdump -p C:\Windows\System32\nvcuda.dll` exports `cuGraphCreate`, `cuGraphAddKernelNode_v2`,
  `cuGraphInstantiateWithFlags`, `cuGraphLaunch`, `cuGraphExecKernelNodeSetParams_v2`,
  `cuGraphConditionalHandleCreate`, `cuImportExternalMemory`, `cuExternalMemoryGetMappedBuffer`,
  `cuImportExternalSemaphore`, `cuSignalExternalSemaphoresAsync`, `cuWaitExternalSemaphoresAsync`,
  `cuLinkCreate_v2`, `cuLinkAddData_v2`. The local `cuda.h` (12.9) declares them, with
  `CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32 = 2`. Existence only; graphs and interop were not exercised.

## Claims in detail

### 1. Float `atomicAdd` non-determinism — correct (wording loose)

M1: "Standard CUDA `atomicAdd` on `float32` is non-associative due to warp scheduling order, which
destroys strict seed-replay determinism."

- Measured here: 8 runs, 8 different float bit patterns for the same input (output above).
- The mechanism is stated backwards: float *addition* is non-associative; the *order* in which
  atomics from different threads land is unspecified and changes run to run. The conclusion stands.
- Sources: arXiv 2408.05148 "Impacts of floating-point non-associativity on reproducibility for HPC
  and deep learning applications" (https://arxiv.org/pdf/2408.05148); NVIDIA forum "Reproducibility
  of atomic operations" (https://forums.developer.nvidia.com/t/reproducibility-of-atomic-operations/136299).

### 2. Integer atomics are order-independent — correct, three caveats

M1: "integer `atomicAdd` is strictly associative and bit-exact across runs."

- Measured here: 1 distinct `int32` result in 8 runs, equal to the host's wrapped sum (2027372544).
- Caveats the message does not state:
  1. Only the **final sum** is order-independent. The value each `atomicAdd` call *returns* (the old
     value) depends on arrival order, so any logic that branches on it is not deterministic.
  2. Only plain add (and and/or/xor/min/max) commute. "Take food if any is left", clamped or
     saturating updates, and compare-and-swap loops are order-dependent even on integers.
  3. Wraparound is deterministic but silent (see claim 3).
- Source: local measurement above; same forum thread and paper as claim 1.

### 3. Q16.16 `int32` fields — mostly_correct

- Fixed point is the right tool and matches this project's laws 5 and 6.
- Catch: Q16.16 holds integer parts in [-32768, 32767]. In the probe, 1,048,576 adds of values
  in [0, 1024) summed to a true total of about 5.37e8, which does not fit; the `int32` cell wrapped
  to 0x78d74000. The result is reproducible and wrong as a conserved quantity. Each field needs a
  proven bound per cell, or a 64-bit cell (PTX has 64-bit `atom.add`; clang exposes
  `__nvvm_atom_add_gen_ll`), or a different Q split.
- Source: local measurement above (range statement is arithmetic: 32 - 16 fractional bits - sign).

### 4. CUDA Graphs for the per-tick pipeline — mostly_correct

- CUDA Graphs exist for exactly this pattern (define once, launch every timestep). NVIDIA's own
  example is a timestep loop: https://developer.nvidia.com/blog/cuda-graphs/
- The benefit is modest. NVIDIA's numbers (Tesla V100, CUDA 10.1, 20 kernels per timestep):
  9.6 us per kernel with a sync after each launch, 3.8 us with one sync per timestep, 3.4 us with a
  graph, against 2.9 us of kernel execution. Graph creation cost about 400 us, first launch about
  33% slower. With only 4 kernels per tick the saving is a few microseconds per tick.
- Limits not mentioned: node parameters and grid sizes are baked in unless updated
  (`cuGraphExecKernelNodeSetParams`), see
  https://developer.nvidia.com/blog/employing-cuda-graphs-in-a-dynamic-environment/ ; a sort stage
  taken from a library that allocates scratch memory is awkward to capture.
- The driver DLL on this machine exports the graph entry points (listed above). Not exercised.

### 5. CUDA-Vulkan external memory, zero copies — mostly_correct (not tested here)

- The mechanism is real: Vulkan allocates exportable memory (`VK_KHR_external_memory` +
  `VK_KHR_external_memory_win32`), CUDA imports it (`cuImportExternalMemory`) and maps a buffer
  (`cuExternalMemoryGetMappedBuffer`); both APIs then address the same device allocation.
  Sources: https://docs.nvidia.com/cuda/cuda-runtime-api/group__CUDART__EXTRES__INTEROP.html and
  https://docs.vulkan.org/guide/latest/extensions/external.html
- Left out by the message: access must be synchronised with imported semaphores
  (`cuImportExternalSemaphore`, signal/wait); both APIs must be on the same physical GPU (this laptop
  also has an Intel iGPU, so the Vulkan device must be chosen explicitly); overlapping mappings need
  volatile access per the NVIDIA page above.
- "Zero PCIe copies" applies to the simulation state. It was not measured, and this machine has no
  Vulkan SDK and no `glslc`, so nothing here was built.

### 6. "`dp4a` / INT8 Tensor instructions" — mostly_correct, but two things are conflated

- `dp4a` is real and reachable from PTX: "dp4a.atype.btype d, a, b, c; Four-way byte dot product
  which is accumulated in 32-bit result. Introduced in PTX ISA version 5.0. Requires sm_61 or
  higher." Source: PTX ISA 8.7,
  https://docs.nvidia.com/cuda/archive/12.8.0/parallel-thread-execution/index.html . LLVM documents
  `llvm.nvvm.idp4a.[us].[us]` as mapping directly to it: https://llvm.org/docs/NVPTXUsage.html
- Ran here: clang emitted `dp4a.s32.s32`, the driver JIT accepted it, the GPU returned 16 (expected
  16). With `-march=sm_50` clang fails with "Cannot select: intrinsic %llvm.nvvm.idp4a.s.s", which
  agrees with the sm_61 floor.
- `dp4a` is an ordinary scalar integer instruction. It is **not** a tensor-core instruction.
- Tensor-core INT8 instructions also exist in PTX, as a separate family: warp-level `mma.sync`
  with `.s8/.u8` operands and an `.s32` accumulator (shape m8n8k16 from sm_75; m16n8k16 and m16n8k32
  from sm_80). The PTX 8.7 contents list "Warp-level matrix multiply-accumulate" (9.7.14) and the
  integer forms appear in CUTLASS:
  https://ipd.graylab.jhu.edu/rfdiffusion2/cutlass-3.5.1/docs/mma__sm75_8h_source.html
  (secondary source for the shape table; I did not get the body of 9.7.14 to load). These operate on
  a whole warp cooperating on one matrix tile, so they fit batched dense layers, not a per-agent
  scalar reflex. Not tested here.
- Being integer, both are deterministic, which suits law 6.

### 7. Radix sort on Morton codes is O(N) — mostly_correct

- LSD radix sort on fixed-width keys does O(k N) work for k digit passes; for 32-bit keys k is a
  constant (typically 4 to 8), so it is linear in N. This is the standard GPU method: Satish, Harris,
  Garland, "Designing Efficient Sorting Algorithms for Manycore GPUs"
  (https://research.nvidia.com/publication/2009-05_designing-efficient-sorting-algorithms-manycore-gpus);
  Morton-code sorting for spatial structures: Karras 2012
  (https://diglib.eg.org/items/71edb707-2be1-4d84-8e25-9162cd9a59e9), which notes codes are limited
  to 30 bits in a 32-bit integer, i.e. 10 bits per axis in 3D (a 1024-cubed grid); sort-by-cell-hash
  particle grids: Green, "Particle Simulation using CUDA"
  (https://developer.download.nvidia.com/assets/cuda/files/particles.pdf).
- Qualifiers: "O(N)" hides the constant (several full passes over keys and payload every tick); the
  message calls the codes "32-bit" but 3D gives 30 usable bits; M2 §4 proposes a counting sort into
  cell buckets instead, which is one pass and is the cheaper choice when the grid is fixed. The two
  messages are consistent in spirit.

### 8. PTX "direct register/warp control; zero warp divergence, explicit shared memory banking" — wrong

M2 §1 table, row 2.

- Registers: PTX registers are virtual. The PTX spec says "The number of registers is limited, and
  will vary from platform to platform. When the limit is exceeded, register variables will be
  spilled to memory" (section 5.1.1, same PTX ISA URL as claim 6). Physical allocation is done by
  the backend (ptxas or the driver JIT), not by the PTX author. The PTX clang produced here uses
  `%r1 ... %r10` virtual names.
- Warps: the spec describes warp scheduling as hardware behaviour ("At every instruction issue
  time, the SIMT unit selects a warp that is ready to execute", section 3.1). PTX offers the same
  warp-level primitives CUDA C++ already exposes; there is no scheduling control.
- "Zero warp divergence" is a property of the control flow and data, not of the language. M1 §5
  itself says warps "will diverge" on pointer-linked sparse brains and prescribes fixed-size masked
  tiles, so the exhibits contradict this table cell.
- "Shared memory banking": the PTX spec has no shared-memory bank concept. Its only "bank" text is
  the deprecated constant-memory banks (section 5.1.3.1).
- Fairness check: M2 §4 concedes compiler parity for CPU assembly ("almost the exact same loop",
  "5-15%") but does not withdraw the PTX claim.

### 9. "CUDA Driver API dispatch (~3-10 us)" — correct

- Measured here (WDDM, noisy, parallel load): `cuLaunchKernel` call alone min 2.0 / median 3.1 /
  p99 52 us; launch plus `cuCtxSynchronize` min 7.0 / median 8.2 / p99 59 us.
- NVIDIA's published figures agree: 9.6 us per kernel with sync, 3.8 us without
  (https://developer.nvidia.com/blog/cuda-graphs/); forum: about 5 us per null launch, about 20 us
  with a sync (https://forums.developer.nvidia.com/t/too-much-time-for-kernel-launch-latency/233645).
- Consequence the message does not draw: M2 §2 claims a 15-30 us CPU tick for its toy kernel. A GPU
  round trip alone costs about 8 us here, with a tail above 50 us, so the GPU cannot win on a tick
  that small. The GPU case rests on heavy per-agent work (claim 11).

### 10. "Persistent Megakernel (CUDA Graphs + Inline PTX)" — misleading

- In the literature a megakernel is ONE large kernel containing every stage. M1 then describes a
  "4-stage CUDA Graph", which is four kernels: the opposite (staged, wavefront-like) design. The
  label and the design do not match.
- The reference paper argues against megakernels for divergent work: Laine, Karras, Aila,
  "Megakernels Considered Harmful: Wavefront Path Tracing on GPUs", HPG 2013
  (https://research.nvidia.com/publication/2013-07_megakernels-considered-harmful-wavefront-path-tracing-gpus):
  divergent control flow and high register use hurt a single big kernel. A 2026 comparison found a
  wavefront renderer about 16% faster than a megakernel (https://arxiv.org/pdf/2605.27323).
- "Inline PTX" needs `nvcc` (`asm()` in CUDA C++), which this machine does not have. With clang the
  route is whole-file PTX, as in the probe.
- The sound part: keep state resident on the GPU and avoid readbacks. That is ordinary practice and
  M1 states the trade-off itself ("PCIe readbacks kill speed if not rendered in-place").

### 11. "10x-30x" over CPU for 100k heavy agents — unverifiable

- No measurement exists for this GPU against this CPU on this workload, and none was taken here.
- Published measurements in the same class land in that order of magnitude, with wide spread:
  - HOOMD-blue 1.0 (particle MD, polymer brush): "effective GPU vs. CPU node speed-up of 12.5x",
    one GPU against a 16-core CPU node: https://arxiv.org/abs/1412.3387
  - FLAME GPU 2 reports millions of agents and Sugarscape at 16M agents, but its published 3.5x and
    10x are against a GPU baseline, not a CPU: https://eprints.whiterose.ac.uk/199416/ . The FLAME
    GPU comparison repository (https://github.com/FLAMEGPU/ABM_Framework_Comparisons) publishes
    scripts for Mesa, NetLogo and Agents.jl but no result table on its front page.
- Problems with the claim as worded: the baseline is "8 P-cores", ignoring the 16 E-cores of the
  275HX; "5,888 cores" are not comparable to CPU cores; the ratio depends on branchiness (see
  claim 10 on divergence). M2 does restrict the claim to "heavy" agents, which is fair.
- Treat as a hypothesis to measure, not a number to plan around.

### 12. Thermal advice — mostly_correct on the power figures, the thresholds are unverifiable

- 160 W: the 275HX maximum turbo power is listed as 160 W (base 55 W):
  https://techpowerup.com/cpu-specs/core-ultra-9-275hx.c4096 (Intel's own page did not return the
  power rows in this session:
  https://www.intel.com/content/www/us/en/products/sku/242293/intel-core-ultra-9-processor-275hx-36m-cache-up-to-5-40-ghz/specifications.html).
- 115-140 W: 60-115 W TGP plus up to 25 W Dynamic Boost, 140 W maximum:
  https://www.notebookcheck.net/Nvidia-GeForce-RTX-5070-Ti-Laptop-Benchmarks-and-Specs.934945.0.html .
  Confirmed locally: `power.max_limit` = 140.00 W.
- Direction is sound: Dynamic Boost moves budget between CPU and GPU, so CPU load does reduce GPU
  headroom, and no laptop of this class sustains about 300 W.
- No source for "throttle within minutes" (chassis dependent), for the 25 W CPU figure, or for
  "boost clocks above 2.2 GHz". Locally `clocks.max.graphics` reads 3090 MHz and the idle clock
  1590 MHz; neither confirms 2.2 GHz. The power cap in force was 93 W, not 115-140 W, so the
  machine's current power profile already sits below the message's assumption.
- The advice does not block CPU-side determinism checks (law 6): they can run while the GPU loop
  is paused instead of alongside it.

## Q1. Can clang-nvptx64 PTX be loaded by the driver API without the CUDA toolkit? — yes (ran)

- Ran end to end on this machine: clang 21.1.8 `--target=nvptx64-nvidia-cuda -march=sm_120 -O2 -S`
  to PTX text, then `cuModuleLoadDataEx` from `nvcuda.dll` returned 0, `cuModuleGetFunction` found
  the three entries and the kernels produced correct results. No nvcc, ptxas, nvrtc, libdevice or
  CUDA header was used.
- Documentation agrees: LLVM says the `cuda` OS triple "makes the generated PTX compatible with the
  CUDA Driver API" (https://llvm.org/docs/NVPTXUsage.html); NVIDIA documents `cuModuleLoadData` /
  `cuModuleLoadDataEx` compiling PTX images
  (https://docs.nvidia.com/cuda/archive/12.6.1/cuda-driver-api/group__CUDA__MODULE.html).
- Practical notes found while doing it:
  - Mark kernels with `__attribute__((nvptx_kernel))` in C (emits `.visible .entry`). LLVM's doc
    names the `ptx_kernel` calling convention for IR.
  - Thread indices: `__nvvm_read_ptx_sreg_tid_x()`, `..._ctaid_x()`, `..._ntid_x()`.
  - Atomics: `__nvvm_atom_add_gen_i / _ll / _f`.
  - Pass the PTX as a NUL-terminated string.
  - No libdevice means no libm on the device; integer / fixed-point code does not need it.
  - First JIT of a 2 KB module took about 75 ms, a repeat load 0.3 ms.

## Q2. Which PTX version and `sm_` target for compute capability 12.0?

- Use `-march=sm_120`. clang 21.1.8 then writes `.version 8.7` and `.target sm_120`.
- PTX ISA 8.7 is the version that "Adds support for sm_120 target architecture" and for `sm_120a`
  (CUDA 12.8): https://docs.nvidia.com/cuda/archive/12.8.0/parallel-thread-execution/index.html .
  LLVM maps sm_120 to PTX 8.7 and the family-specific `f` variants to 8.8:
  https://lists.llvm.org/pipermail/llvm-commits/Week-of-Mon-20250421/1615281.html
- The driver here (616.87, CUDA API 13040) also accepted every older target tried: sm_30/4.2,
  sm_50/4.2, sm_75/6.3, sm_89/7.8, sm_100/8.6, and sm_120a/8.7. So an older target is not an error,
  it only withholds newer instructions (dp4a needs sm_61 or higher).
- Avoid `sm_120a` unless an architecture-specific feature is needed: LLVM describes the `a` suffix
  as features "supported on the specified architecture only" (search snippet of NVPTX.td,
  https://llvm.googlesource.com/llvm-project/+/09fb20ec7d81c1b370dd3d259250c20e505ebeed/llvm/lib/Target/NVPTX/NVPTX.td),
  so it is not forward compatible.
- Not tested: a `.target` above the device (clang rejected `sm_121`), and PTX 8.8 / 9.x headers.

## What was not verified

- Vulkan interop and CUDA Graphs were checked for existence only (docs + DLL exports), not run.
- The tensor-core `mma.sync` INT8 forms were not run; the shape/architecture table is from a
  secondary source.
- No GPU-versus-CPU speed measurement for an agent workload was taken.
- Intel's own power figures for the 275HX did not load; the 160 W figure is from a spec database.
- The arXiv 1311.3890 figure (speed-ups 12 to 27 against 8 CPU cores) appeared only in a search
  summary; the abstract does not contain it, so it is not used as evidence above.
