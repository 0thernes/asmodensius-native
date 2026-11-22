# Fact-check: GPU claims in GrokBot messages 1 and 2

Date: 2026-11-17. Key: `gpu`. Scratch: `S:\factcheck\gpu\`.
Exhibits: `grokbot_message_1.md` (M1), `grokbot_message_2.md` (M2). They are treated as data.
Verdict scale: correct / mostly_correct / misleading / wrong / unverifiable.

## Summary

The catalogue numbers (GB205, 5,888 CUDA cores, 12 GB GDDR7, 672 GB/s, 992 AI TOPS,
60-115 W + boost to 140 W) are right or nearly right. They match NVIDIA's own
comparison page and this machine's driver.

The cache story built on top of them is not. The driver on this machine reports an
L2 cache of **36 MiB, not 48 MiB**. The "2.4 TB/s L2" figure has no source anywhere.
"The whole population fits inside the GPU L2 cache" describes a CPU-style residency
guarantee that a GPU L2 does not give, and the "5x to 200x drop past 48 MiB" cliff
does not exist on the GPU side.

## Verdict table

| # | Claim (where) | Verdict |
|---|---|---|
| 1 | Chip is "Blackwell GB205" (M1 s1) | correct |
| 2 | 5,888 CUDA cores (M1 s1, s2; M2 s1, s4) | correct |
| 3 | 12 GB GDDR7 (M1 s1) | correct |
| 4 | "672-677 GB/s" (M1 s1, s2; M2 s4 says 672) | mostly_correct |
| 5 | "5.8 MiB L1/shared SRAM" (M1 s1) | mostly_correct |
| 6 | "48 MiB L2 cache" (M1 s1, s3, closing line) | wrong |
| 7 | "992 INT8 TOPS" (M1 s1) | mostly_correct |
| 8 | "~2.4 TB/s (L2)" peak bandwidth (M1 s2 table) | unverifiable |
| 9 | "115-140W on the 5070 Ti" (M1 s5) | mostly_correct |
| 10 | "GPU boost clocks hold steady above 2.2 GHz" (M1 s5) | misleading |
| 11 | "entire population's hot loop fits 3.7x inside the 48 MiB L2" (M1 s3, closing) | misleading |
| 12 | Past "48 MiB on GPU, bandwidth drops by 5x to 200x across DRAM and NVMe swap" (M1 s1) | wrong (GPU half) |
| 13 | Bare metal loses the GPU because Blackwell needs GSP firmware + kernel driver (M1 s2) | mostly_correct |

Fairness check done: M2 section 4 ("honest trade-offs") qualifies hand-written ASM,
the O(N^2) bottleneck and the 10x-30x GPU-vs-CPU estimate. It does not qualify any
of the GPU hardware figures above. M1 section 5 qualifies thermals and determinism,
not the cache figures. So no verdict below is softened by a later qualification.

## Local measurements

### A. `nvidia-smi -q` (ran now; full output in `S:\factcheck\gpu\nvidia-smi-q.txt`)

Command: `nvidia-smi -q`

Relevant lines, verbatim:

```
Driver Version                                         : 616.87 [Deprecated; ...]
CUDA Version                                           : 13.4 [Deprecated; ...]
    Product Name                                       : NVIDIA GeForce RTX 5070 Ti Laptop GPU
    Product Architecture                               : Blackwell
    Driver Model
        Current                                        : WDDM
    VBIOS Version                                      : 98.05.31.00.3b
    GPU Part Number                                    : 2F18-750-A1
    GSP Firmware Version                               : N/A
        Device Id                                      : 0x2F1810DE
            PCIe Generation
                Max                                    : 5
                Current                                : 1
            Link Width
                Max                                    : 16x
                Current                                : 8x
    Performance State                                  : P8
    FB Memory Usage
        Total                                          : 12227 MiB
        Reserved                                       : 283 MiB
        Used                                           : 402 MiB
        Free                                           : 11543 MiB
    Temperature
        GPU Current Temp                               : 56 C
        GPU Target Temperature Specification           : 87 C
    GPU Power Readings
        Instantaneous Power Draw                       : 5.48 W
        GPU Ceiling Power Limit
            Current Power Limit                        : 93.22 W
            Default Power Limit                        : 65.00 W
        Min Power Limit                                : 5.00 W
        Max Power Limit                                : 140.00 W
    Clocks
        Graphics                                       : 180 MHz
        SM                                             : 180 MHz
        Memory                                         : 405 MHz
    Max Clocks
        Graphics                                       : 3090 MHz
        SM                                             : 3090 MHz
        Memory                                         : 14001 MHz
```

Notes. The GPU was idle (P8, 5.48 W, 180 MHz), so the "Clocks" block shows idle
clocks, not load clocks. "Max Clocks 3090 MHz" is the top of the driver's clock
table, not a boost clock this laptop reaches. `nvidia-smi -q` prints no cache
sizes, no core count and no TOPS figure.

### B. CUDA Driver API attribute query (ran now)

`nvidia-smi` cannot answer the cache, SM-count and bus-width questions, but the
driver can. `S:\factcheck\gpu\cuattr.c` loads `nvcuda.dll`, calls `cuInit` and
`cuDeviceGetAttribute` only. It creates no context and runs no kernel. Attribute
numbers were read from `Z:\Tools\cuda-12.9-redist\include\cuda.h`.

Commands (in `S:\factcheck\gpu\`):

```
gcc -std=c2x -O2 -Wall -Wextra cuattr.c -o cuattr.exe      (no warnings, no output)
./cuattr.exe
```

Output, complete:

```
cuInit rc=0
cuDeviceGetCount rc=0 count=1
device 0: NVIDIA GeForce RTX 5070 Ti Laptop GPU
  TOTAL_MEM = 12792037376 bytes = 12199.4 MiB
  COMPUTE_CAPABILITY_MAJOR                       : 12
  COMPUTE_CAPABILITY_MINOR                       : 0
  MULTIPROCESSOR_COUNT                           : 46
  WARP_SIZE                                      : 32
  MAX_THREADS_PER_MULTIPROCESSOR                 : 1536
  MAX_BLOCKS_PER_MULTIPROCESSOR                  : 24
  MAX_REGISTERS_PER_MULTIPROCESSOR               : 65536
  MAX_SHARED_MEMORY_PER_BLOCK (bytes)            : 49152
  MAX_SHARED_MEMORY_PER_BLOCK_OPTIN (bytes)      : 101376
  MAX_SHARED_MEMORY_PER_MULTIPROCESSOR (bytes)   : 102400
  L2_CACHE_SIZE (bytes)                          : 37748736
  MAX_PERSISTING_L2_CACHE_SIZE (bytes)           : 23592960
  MAX_ACCESS_POLICY_WINDOW_SIZE (bytes)          : 134217728
  CLOCK_RATE (kHz)                               : 1515000
  MEMORY_CLOCK_RATE (kHz)                        : 14001000
  GLOBAL_MEMORY_BUS_WIDTH (bits)                 : 192
  INTEGRATED                                     : 0
  derived: SM x 128 = 5888 "CUDA cores"
  derived: L2 = 36.00 MiB = 37.75 MB
  derived: bus 192 bit x (2 x 14001000 kHz) / 8 = 672.05 GB/s
```

What was NOT measured: no kernel was run, so there is no measured L2 hit latency,
no measured bandwidth and no measured clock under load. Those would need a PTX
microbenchmark (a working-set sweep), which is a separate experiment.

## Claim by claim

### 1. "RTX 5070 Ti Laptop Topology (Blackwell GB205)" - correct

- Local: `Product Architecture : Blackwell`, `Device Id : 0x2F1810DE`.
- The PCI ID database names device 10de:2f18 "GB205M [GeForce RTX 5070 Ti Mobile]":
  https://admin.pci-ids.ucw.cz/read/PC/10de/2f18
- Notebookcheck names the chip GB205 (codename GN22-X7):
  https://www.notebookcheck.net/Nvidia-GeForce-RTX-5070-Ti-Laptop-Benchmarks-and-Specs.934945.0.html

### 2. "5,888 CUDA cores" - correct

- NVIDIA comparison page, row "NVIDIA CUDA Cores": 5888.
  https://www.nvidia.com/en-us/geforce/laptops/compare/
- Local: 46 streaming multiprocessors (SMs); 46 x 128 = 5,888.

Caveat that matters for design. A "CUDA core" is one scalar lane inside an SM, not
an independent core. The real units are 46 SMs, each holding at most 1,536 resident
threads (48 warps of 32). M2's "5,888 cores ... out-scales 8 P-cores" by "10x-30x"
compares unlike things and cites no benchmark; that ratio is **unverifiable** here
(nothing was run), and M2 itself presents it as an estimate.

### 3. "12 GB GDDR7" - correct

- NVIDIA page, "Standard Memory Configuration": 12 GB GDDR7 (same URL as claim 2).
- Local: `FB Memory Usage Total : 12227 MiB`; `cuDeviceTotalMem` = 12199.4 MiB.
  283 MiB is reserved and 402 MiB was in use by other processes at the time.

### 4. "12 GB GDDR7 at 672-677 GB/s" - mostly_correct

Derivation from bus width and data rate:

- Bus width: 192 bits. NVIDIA page "Memory Interface Width: 192-bit"; local
  `GLOBAL_MEMORY_BUS_WIDTH = 192`. (Six 32-bit GDDR7 devices of 2 GB each.)
- Data rate: local `MEMORY_CLOCK_RATE = 14,001,000 kHz` and nvidia-smi
  `Max Clocks Memory : 14001 MHz`. Two bits per clock gives 28.002 Gbit/s per pin,
  i.e. the 28 Gbps GDDR7 speed grade. GDDR7 uses PAM3 signalling (JEDEC JESD239):
  https://www.businesswire.com/news/home/20240305637931/en/JEDEC-Publishes-GDDR7-Graphics-Memory-Standard
- 192 x 28.002e9 / 8 = **672.05e9 bytes/s = 672 GB/s** (= 625.9 GiB/s).

So 672 GB/s is right. The upper figure **677 has no derivation**: it would need
28.2 Gbps per pin, which neither NVIDIA nor the driver reports. Also, 672 GB/s is
the theoretical peak rate of the interface. It is a ceiling, not the throughput a
kernel sees (address/command overhead, ECC, read/write turnaround and access
pattern all reduce it).

### 5. "5.8 MiB L1/shared SRAM" - mostly_correct

- NVIDIA's Blackwell tuning guide gives 128 KB of shared memory capacity per SM for
  compute capability 12.0, and a 99 KB maximum per thread block:
  https://docs.nvidia.com/cuda/archive/13.4.2/blackwell-tuning-guide/index.html
- 46 SMs x 128 KiB = 5,888 KiB = 5.75 MiB, which rounds to the quoted 5.8 MiB.
  Notebookcheck's table shows the same "L1 5.8 MiB" (URL in claim 1).
- Local: usable shared memory is 102,400 bytes per SM, 49,152 bytes per block by
  default, 101,376 bytes per block when opted in.

The number is an honest total, but it is **not one 5.8 MiB pool**. It is 46
separate 128 KiB blocks, one per SM, each split between L1 data cache, texture
cache and shared memory. A thread block can address at most 99 KiB of it, and
only the block that owns it can see it. M1's "Stage B: 3x3 neighborhood sensory
gather + raycast inside shared SRAM" therefore has a hard budget of about 99 KiB
per block, not megabytes.

### 6. "48 MiB L2 cache" - wrong (for this machine, per NVIDIA's driver)

- Local: `CU_DEVICE_ATTRIBUTE_L2_CACHE_SIZE = 37,748,736 bytes = 36.00 MiB`
  (cuda.h: "Size of L2 cache in bytes").
- Where 48 comes from: third-party spec tables list "L2 48 MiB" for this GPU, for
  example Notebookcheck (URL in claim 1). The chatbot did not invent the number;
  it repeated a database value. That value disagrees with what NVIDIA's own driver
  reports for the device in this laptop, and the driver is the primary source.
- NVIDIA publishes no L2 size on its product page (checked: the comparison page
  has no cache rows).

Consequences: every figure in M1 derived from 48 MiB is off. "3.7x inside" becomes
36 MiB / 12.21 MiB = **2.95x** (12.8 MB = 12.21 MiB). The CPU comparison also
changes: this GPU's L2 (36 MiB) equals the CPU's L3 (36 MB class), it is not larger.

### 7. "992 INT8 TOPS" - mostly_correct

- NVIDIA comparison page, row "AI TOPS": 992 (URL in claim 2).
- NVIDIA's page does not say "INT8" and carries no footnote defining the metric
  (checked). The "INT8" label appears on Notebookcheck ("992 TOPS INT8").
- It is a theoretical peak for the tensor cores. It says nothing about integer
  `dp4a`-style code running on ordinary CUDA cores, which is what M1's "INT8 reflex
  weights" would mostly be, and nothing about memory-bound simulation speed.

### 8. "~2.4 TB/s (L2)" - unverifiable

- No source found. NVIDIA publishes no L2 bandwidth for any GeForce part. A search
  for measured GPU L2 throughput returned no figure for GB205.
- The nearest published measurement is a microbenchmark preprint on the desktop
  RTX 5080 (GB203, a different and larger chip). It reports latencies, not an L2
  bandwidth: L2 hit about 358 cycles, DRAM miss about 877 cycles.
  https://arxiv.org/html/2507.10789v2
- Not measured here (no kernel was run). The same table's "~1.2 TB/s (P-core L2)"
  is a CPU claim and belongs to the CPU fact-check.

### 9. "115-140W on the 5070 Ti" - mostly_correct

- NVIDIA page, "GPU Subsystem Power (W)": 60 - 115 W (URL in claim 2). 115 W is the
  top of the range; laptop makers choose a point inside it.
- 140 W is 115 W plus Dynamic Boost; Notebookcheck states a 140 W maximum
  including Dynamic Boost (URL in claim 1).
- Local: `Max Power Limit : 140.00 W`, so this laptop is a top-range design.
  But `Current Power Limit : 93.22 W` and `Default Power Limit : 65.00 W` at the
  time of the query. The GPU reaches 115-140 W only in the vendor's highest
  performance profile; it is not the default state.

The "160W on the 275HX" half of the sentence is a CPU claim, not checked here.

### 10. "GPU boost clocks hold steady above 2.2 GHz" - misleading

- NVIDIA page, "Boost Clock (MHz)": 1447 - 2220 MHz (URL in claim 2). 2,220 MHz is
  the **maximum** of the range, for the highest-power configuration. "Steady above
  2.2 GHz" turns a ceiling with 20 MHz of headroom into a floor.
- Local: the driver's "typical clock frequency" is `CLOCK_RATE = 1,515,000 kHz`
  (1,515 MHz). The idle clock was 180 MHz. No load clock was measured.
- The underlying advice (keep CPU heat low so the shared cooler does not throttle
  the GPU) is reasonable. The number attached to it is not something this GPU
  holds, and core clock is not the limit for a memory-bound simulation anyway.

### 11. "the entire population's hot loop fits 3.7x inside the 48 MiB L2" - misleading

Is "fits inside the GPU L2 cache" a meaningful statement? Partly.

What is real:

- The GPU L2 is a genuine data cache in front of GDDR7, and repeatedly accessed
  global memory can be served from it with lower latency. NVIDIA documents this and
  even gives an API to prefer data in L2 ("persisting" accesses with a set-aside
  region), for compute capability 8.0 and later:
  https://docs.nvidia.com/cuda/cuda-programming-guide/04-special-topics/l2-cache-control.html
- This GPU supports that API: local `MAX_PERSISTING_L2_CACHE_SIZE = 23,592,960`
  bytes (22.5 MiB) and a 128 MiB maximum access-policy window.
- A 12.8 MB array is smaller than the 36 MiB L2, so repeated passes over it will
  mostly hit L2.

What makes the claim misleading:

1. **Wrong size.** 36 MiB, not 48 MiB (claim 6). The set-aside that can be
   prioritised is at most 22.5 MiB, i.e. 1.84x the 12.21 MiB hot array.
2. **The hot array is not the whole tick.** M1's own pipeline adds Morton keys, a
   radix sort (which needs a second buffer of keys and indices), cell start/end
   tables, neighbour gathers, and then Vulkan drawing from the same buffer. All of
   that competes for the same L2. If the SoA is double-buffered (read old state,
   write new state), the footprint is already 2 x 12.21 = 24.4 MiB.
3. **No residency guarantee.** L2 is one cache shared by all SMs, all kernels and
   all processes using the GPU. Per NVIDIA's guide the persisting selection is
   probabilistic (`hitRatio`), concurrent kernels share the set-aside, and
   persisting lines can linger and take space from later kernels. Two Chrome
   processes were using this GPU during the query (nvidia-smi process list).
4. **A GPU is built to stream from DRAM.** Thousands of threads hide memory
   latency by running other warps. Leaving L2 costs a factor of a few in latency,
   not the order-of-magnitude cliff a CPU sees. The RTX 5080 preprint measures
   about 358 cycles for an L2 hit against about 877 for a DRAM miss, a 2.4x step
   (URL in claim 8). An L2 hit on a GPU is itself roughly ten times slower than an
   L1 hit (about 30-40 cycles in the same paper), so "in L2" does not mean "fast"
   in the CPU sense.
5. **"Fits in cache" is a CPU idea.** The CPU claim "fits in P-core L2" is about
   a private per-core cache. The GPU has no per-SM megabyte cache; the private
   level is 128 KiB per SM (claim 5).

Useful part to keep: make the hot state small and sequential, sort by Morton
code, keep it on the GPU. Those help regardless of the L2 size. What should not be
kept is any budget rule of the form "stay under 48 MiB and the tick is
sub-millisecond"; that is not a property of this hardware, and M1's closing
"4-second swap-bound stall into a sub-millisecond tick" is not backed by any
measurement.

### 12. Past "48 MiB on GPU, bandwidth drops by 5x to 200x across DRAM and NVMe swap" - wrong (GPU half)

- Past the L2, a GPU access goes to GDDR7 at a rated 672 GB/s (claim 4). On the
  chatbot's own numbers the step is 2.4 TB/s to 677 GB/s, which is 3.5x, not 5x.
  The only published latency data (claim 8) shows about 2.4x.
- Crossing a cache size never causes swapping. Swapping needs the working set to
  exceed physical memory. A 12.8 MB working set on a 12 GB card is about 0.1% of
  VRAM. There is no "NVMe swap" tier between the GPU L2 and GDDR7.
- The 36 MB CPU half of the sentence is for the CPU fact-check.

### 13. Bare metal loses the GPU: "Blackwell ... requires signed GSP firmware and kernel driver initialization" - mostly_correct

- NVIDIA's Linux driver README says Blackwell and later are supported only by the
  open kernel modules, and those depend on the GPU System Processor (GSP):
  https://download.nvidia.com/XFree86/Linux-x86_64/595.84/README/kernel_open.html
  https://download.nvidia.com/XFree86/Linux-x86_64/570.169/README/gsp.html
- So an OS-less UEFI program has no supported way to bring up CUDA on this GPU.
  The practical conclusion ("host on an OS, keep the GPU") is sound.
- Local: `GSP Firmware Version : N/A` under the Windows WDDM driver, so the local
  tool neither confirms nor contradicts GSP use on Windows. "Signed" was not
  separately verified.

## GPU statements seen but not given a verdict (outside the assigned list, not researched)

- M2 s1: "CUDA Driver API dispatch (~3-10 us)". No source given; not measured here.
- M2 s1: PTX gives "zero warp divergence". Divergence comes from control flow, not
  from the language used; PTX has branches. Not researched further.
- M1 s5: float `atomicAdd` ordering and integer fixed-point determinism. This is a
  software claim, not a hardware specification.
- M1 s4: "zero PCIe copies" with CUDA-Vulkan interop. For reference only, the link
  reported `PCIe Generation Max 5, Current 1; Link Width Max 16x, Current 8x`
  while idle.

## Sources

Primary:
- NVIDIA, GeForce RTX 50 laptop comparison page:
  https://www.nvidia.com/en-us/geforce/laptops/compare/
- NVIDIA, CUDA Programming Guide, L2 Cache Control:
  https://docs.nvidia.com/cuda/cuda-programming-guide/04-special-topics/l2-cache-control.html
- NVIDIA, Blackwell tuning guide:
  https://docs.nvidia.com/cuda/archive/13.4.2/blackwell-tuning-guide/index.html
- NVIDIA, Linux driver README (open kernel modules; GSP firmware):
  https://download.nvidia.com/XFree86/Linux-x86_64/595.84/README/kernel_open.html
  https://download.nvidia.com/XFree86/Linux-x86_64/570.169/README/gsp.html
- JEDEC, GDDR7 standard announcement:
  https://www.businesswire.com/news/home/20240305637931/en/JEDEC-Publishes-GDDR7-Graphics-Memory-Standard
- Local: `nvidia-smi -q`; `cuattr.exe` (CUDA Driver API), both in `S:\factcheck\gpu\`.

Secondary:
- PCI ID database, 10de:2f18: https://admin.pci-ids.ucw.cz/read/PC/10de/2f18
- Notebookcheck, RTX 5070 Ti Laptop page (the apparent origin of the 5.8 MiB,
  48 MiB and "992 TOPS INT8" wording):
  https://www.notebookcheck.net/Nvidia-GeForce-RTX-5070-Ti-Laptop-Benchmarks-and-Specs.934945.0.html
- "Dissecting the NVIDIA Blackwell Architecture with Microbenchmarks" (preprint,
  desktop RTX 5080, not this chip): https://arxiv.org/html/2507.10789v2

Not reachable: the TechPowerUp GPU database page returned HTTP 403, so it was not used.

## Limits of this check

- Web pages were read through a fetch tool that summarises; the NVIDIA figures
  (992, 5888, 12 GB GDDR7, 60-115 W, 1447-2220 MHz, 192-bit) were returned as
  direct row values and agree with the local driver where the driver exposes them.
- The 36 MiB L2 value is what the driver reports. It was not confirmed by a
  working-set timing sweep.
- No GPU kernel was run, so nothing here is a performance measurement.
