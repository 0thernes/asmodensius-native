# GPU compute without the CUDA toolkit: clang PTX + nvcuda.dll

Agent key: `cuda-ptx`. Date: 2026-11-17. Everything below marked "ran" was run
in this session on the project machine (RTX 5070 Ti Laptop GPU, driver 616.87).
All timings are **noisy, parallel load** (other agents share the machine).

Sample folder: `N:\docs\research\platform\samples\cuda-ptx\`

| File | What |
|---|---|
| `host.c` | C host. `LoadLibraryA("C:\Windows\System32\nvcuda.dll")` + `GetProcAddress`; own prototypes; no import lib, no CUDA header at build time |
| `kernel.cu` | device code, CUDA dialect, own attribute macros (route A) |
| `kernel_c.c` | same kernels as plain C (route B) |
| `kernel.ptx`, `kernel_c.ptx`, `kernel_sm120.ptx` | PTX text emitted by clang |
| `run1.txt`, `run2.txt`, `run3_routeB.txt`, `run4_sm120.txt` | captured program output |

## Verdict

**The first route works.** No nvcc, no ptxas, no nvrtc, no `cuda.lib` needed:

1. clang 21.1.8 emits PTX text (`-x cuda -nocudainc -nocudalib --cuda-gpu-arch=sm_80 --cuda-device-only -S`).
2. The driver DLL JIT-compiles that text itself (`cuModuleLoadDataEx`).
3. The integer histogram kernel matches the CPU bit-for-bit, in every run.

Route B (plain C, `--target=nvptx64-nvidia-cuda`) also works. Hand-written PTX
and OpenCL were **not needed and not attempted** (only the `.version`/`.target`
lines of clang's PTX were hand-edited, for the acceptance probe below).

## PTX version and target accepted

- Emitted and used for the main result: **`.version 7.0`, `.target sm_80`, `.address_size 64`**.
- Device reports compute capability 12.0; `cuDriverGetVersion` = 13040.
- What clang 21.1.8 emits per arch, and whether this driver loaded it (ran, load-only probe):

| clang arch flag | emitted header | driver |
|---|---|---|
| (none) | `.version 4.2` `.target sm_30` | accepted |
| `sm_50` | 4.2 / sm_50 | accepted |
| `sm_75` | 6.3 / sm_75 | accepted |
| `sm_80` | 7.0 / sm_80 | accepted (main result) |
| `sm_89`, `sm_90` | 7.8 | accepted |
| `sm_100` | 8.6 / sm_100 | accepted |
| `sm_120` | 8.7 / sm_120 | accepted, full test PASS (`run4_sm120.txt`) |
| `sm_121` | clang: `unknown target CPU 'sm_121'` | n/a |

- Hand-edited header on the same PTX body (ran): `.version` 8.5, 8.7, 8.8, 9.0,
  9.1, 9.2, 9.3, 9.4 with a matching target are accepted; **9.5 and 10.0 are
  rejected** with `CUDA_ERROR_UNSUPPORTED_PTX_VERSION` (222). So this driver's
  ceiling is PTX 9.4. `.version 7.0` + `.target sm_120` is rejected
  (`PTX .version 7.0 does not support .target sm_120`); `.version 2.0` is rejected.
- The JIT error log names "ptxas", i.e. the driver carries its own PTX assembler.

## Commands (exact, ran)

Run from `N:\docs\research\platform\samples\cuda-ptx` (Git Bash):

```
# device code -> PTX (route A, CUDA dialect)
/z/Tools/LLVM-21/bin/clang.exe -x cuda -nocudainc -nocudalib --cuda-gpu-arch=sm_80 --cuda-device-only -O2 -S kernel.cu -o kernel.ptx

# device code -> PTX (route B, plain C; kernel attribute nvptx_kernel or device_kernel)
/z/Tools/LLVM-21/bin/clang.exe --target=nvptx64-nvidia-cuda -march=sm_80 -O2 -S -DKATTR=nvptx_kernel kernel_c.c -o kernel_c.ptx

# host
gcc -std=c2x -O2 -Wall -Wextra host.c -o host.exe      # no warnings

# run (twice), and load-only probe
./host.exe kernel.ptx
./host.exe kernel.ptx
./host.exe some.ptx probe
```

Flag notes (all observed here):

- `--cuda-device-only` is required. Without it clang also builds the host side and
  fails: `fatbinary command failed` / `program not executable` (no toolkit).
- Route B: `-mcpu=sm_80` is rejected (`unsupported option '-mcpu=' for target
  'nvptx64-nvidia-cuda'`); use `-march=sm_80` (or `-Xclang -target-cpu -Xclang sm_80`).
  With no arch flag clang emits sm_30 / PTX 4.2.
- Kernel annotation: route A `__attribute__((global))` on an `extern "C"` function;
  route B `__attribute__((nvptx_kernel))` or `__attribute__((device_kernel))`
  (both produce `.visible .entry` in clang 21.1.8).
- No CUDA headers, so thread indices come from clang builtins
  `__nvvm_read_ptx_sreg_ctaid_x()`, `_ntid_x()`, `_tid_x()`.
- Atomic add: `__atomic_fetch_add(&hist[b], 1u, __ATOMIC_RELAXED)` compiles to
  `atom.global.add.u32 %r17, [%rd4], 1;` (line 45 of `kernel.ptx`).
- `grep -cE '\.f32|\.f64'` on all three PTX files gives 0: integer instructions only.

## Driver API used (symbols resolved with GetProcAddress)

Prototypes were written by hand in `host.c` and checked against
`Z:\Tools\cuda-12.9-redist\include\cuda.h` (CUDA_VERSION 12090). Versioned names
are the ones that header's `#define`s map to:

`cuInit`, `cuDriverGetVersion`, `cuDeviceGetCount`, `cuDeviceGet`, `cuDeviceGetName`,
`cuDeviceGetAttribute`, `cuDeviceTotalMem_v2`, **`cuCtxCreate_v2`**, `cuCtxDestroy_v2`,
`cuCtxSynchronize`, `cuModuleLoadDataEx`, `cuModuleGetFunction`, `cuModuleUnload`,
`cuMemAlloc_v2`, `cuMemFree_v2`, `cuMemcpyHtoD_v2`, `cuMemcpyDtoH_v2`,
`cuLaunchKernel`, `cuGetErrorName`. All 19 resolved. Types: `CUdevice` = int,
`CUdeviceptr` = unsigned long long, handles are opaque pointers, `CUDAAPI` = `__stdcall`.

## The experiment

N = 1,000,000 elements, 4096 bins. Each GPU thread computes
`i = blockIdx.x*blockDim.x + threadIdx.x`, hashes it with the murmur3 32-bit
finaliser, and atomically adds 1 to `hist[hash & 4095]`. The CPU does the same
loop serially. Comparison is `memcmp` over the 16384-byte histogram plus an
FNV-1a-64 digest. Three GPU launches per process: block 256 twice, then block 1024.

Output of the first process (`run1.txt`, ran):

```
loaded C:\Windows\System32\nvcuda.dll
cuDriverGetVersion=13040 devices=1 device0="NVIDIA GeForce RTX 5070 Ti Laptop GPU" compute=12.0 mem=12199 MiB
ptx file=kernel.ptx bytes=2642 .version=7.0 .target=sm_80
cuModuleLoadDataEx OK (driver JIT of PTX text) in 31976 us (noisy, parallel load)
cpu  histogram: N=1000000 bins=4096 fnv1a64=c7c227f2ba8f0d70
gpu run 1: grid=3907 block=256 sum=1000000 fnv1a64=c7c227f2ba8f0d70 memcmp_vs_cpu=IDENTICAL  launch+sync 667 us (noisy, parallel load)
gpu run 2: grid=3907 block=256 sum=1000000 fnv1a64=c7c227f2ba8f0d70 memcmp_vs_cpu=IDENTICAL  launch+sync 67 us (noisy, parallel load)
gpu run 3: grid=977 block=1024 sum=1000000 fnv1a64=c7c227f2ba8f0d70 memcmp_vs_cpu=IDENTICAL  launch+sync 66 us (noisy, parallel load)
gpu run 1 vs run 2: IDENTICAL
bins[0..7] cpu: 208 255 263 255 235 257 213 230
bins[0..7] gpu: 208 255 263 255 235 257 213 230
lesion (non-atomic increment): sum=64102 of 1000000, lost=935898, memcmp_vs_cpu=DIFFERENT
empty kernel launch+sync over 2000 iters: min 7.1 us, median 10.2 us, mean 15.4 us, p99 79.1 us, max 152.4 us (noisy, parallel load)
RESULT: PASS
```

Second process (`run2.txt`, ran), tail:

```
gpu run 1: grid=3907 block=256 sum=1000000 fnv1a64=c7c227f2ba8f0d70 memcmp_vs_cpu=IDENTICAL  launch+sync 466 us (noisy, parallel load)
gpu run 2: grid=3907 block=256 sum=1000000 fnv1a64=c7c227f2ba8f0d70 memcmp_vs_cpu=IDENTICAL  launch+sync 68 us (noisy, parallel load)
gpu run 3: grid=977 block=1024 sum=1000000 fnv1a64=c7c227f2ba8f0d70 memcmp_vs_cpu=IDENTICAL  launch+sync 66 us (noisy, parallel load)
gpu run 1 vs run 2: IDENTICAL
lesion (non-atomic increment): sum=63273 of 1000000, lost=936727, memcmp_vs_cpu=DIFFERENT
empty kernel launch+sync over 2000 iters: min 7.1 us, median 8.2 us, mean 11.9 us, p99 61.4 us, max 437.7 us (noisy, parallel load)
RESULT: PASS
```

Identical-output evidence (ran): the digest `c7c227f2ba8f0d70` appears for the CPU
and for all 3 GPU launches in each of 4 processes (2 x route A sm_80, 1 x route B,
1 x route A sm_120) = 12 GPU launches, all `memcmp` IDENTICAL to the CPU. A `diff`
of `run1.txt` vs `run2.txt` with the timing and lesion lines removed is empty.

Lesion control: the same kernel with a plain `hist[b] = hist[b] + 1` loses about
93.6% of the counts (sum 64102 / 63273 / 64849 of 1,000,000 in three processes,
different each time). So the atomic is what makes the result exact and repeatable.

## Timings (all noisy, parallel load)

| What | Observed |
|---|---|
| Empty kernel, `cuLaunchKernel` + `cuCtxSynchronize`, 1 thread, 2000 iters after 200 warm-up | min 7.1 us; median 8.2 to 10.2 us; mean 11.0 to 15.4 us; p99 57 to 79 us; max 97 to 438 us (5 processes) |
| Histogram kernel, 1,000,000 threads, launch + sync, warm | 65 to 73 us |
| Histogram kernel, first launch in a process | 412 to 846 us |
| `cuModuleLoadDataEx` (PTX JIT), first time this PTX text was seen | 19 ms, 20 ms and 32 ms (three different PTX texts) |
| `cuModuleLoadDataEx`, same PTX text in a later process | 0.29 and 0.37 ms |

(The very first run of the session, before the lesion kernel was added, was not
captured to a file; it contributes the 19 ms load and one of the five empty-kernel
samples. `kernel_c.ptx` has the same size as that first PTX, which fits its 0.37 ms load.)

The drop from about 20-30 ms to about 0.3 ms on a repeat load suggests the driver
caches compiled PTX between processes. I did not locate or verify that cache.
The empty-kernel samples cluster at 7.1 and 8.2 us; I did not investigate why.

## Limits and unknowns

- One GPU, one driver version. Nothing here says other drivers accept the same PTX ceiling.
- `cuCtxCreate_v2` is what the 12.9 header maps to and it works on this driver
  (API version 13040). Whether a future driver drops the `_v2` export is unknown.
- Only 1D launches, the default stream, pageable host memory, synchronous copies.
  No pinned memory, streams, events or async copies were tested.
- Copy cost (`cuMemcpyHtoD` / `DtoH`) was not timed separately.
- The kernel is integer-only by construction; no claim is made about floating-point
  determinism on the GPU.
- OpenCL through `OpenCL.dll`: not tried, because the CUDA driver route worked.
- No prior-art search was done; nothing here is claimed as novel (driver-API + clang
  NVPTX is a documented, known path).
