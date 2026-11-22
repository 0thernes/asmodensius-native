# GrokBot fact-check: what the two messages got right and wrong

Date: 2026-11-17. Written for the owner. Key: `docs-factcheck`.

> Owner's decision, 2026-11-17 (prompts P19 and P20, rule L6): the no-OS,
> UEFI and USB-boot direction of the two pasted messages is withdrawn; the
> engine runs only as a Windows program from Z:. The bare-metal verdicts
> below stay as a record.

This document settles the two chatbot messages the owner pasted on 2026-11-17.
It merges six research notes into one place. Nothing was re-measured for it.

- Exhibits: `research/factcheck/grokbot_message_1.md` (M1),
  `research/factcheck/grokbot_message_2.md` (M2), and the assembly listing from
  M2, `research/factcheck/grok_step_alife_avx2.asm`. They were read as data.
- Notes: `cpu_memory.md`, `gpu.md`, `asm_kernel.md`, `baremetal.md`, `cuda.md`,
  `below_asm.md`, all in `research/factcheck/`.
- Machine: Intel Core Ultra 9 275HX, RTX 5070 Ti Laptop GPU, 48 GB RAM,
  Windows 11.
- Verdict words: correct, mostly_correct, misleading, wrong, unverifiable.
- **Every timing in this document is "noisy, parallel load".** Other agents
  were using the machine during every run. Treat each figure as a lower bound
  with roughly +-20 % scatter, not as a specification.
- Every number below is copied from one of the six notes. Where two notes
  measured the same thing, both figures are shown with their note.
- Quotes are at most 14 words. Bold and math markup are removed, and
  typographic symbols are written in ASCII (x, -, u for micro).

Parts: 1 one-page summary. 2 full claim table. 3 measured tables.
4 the corrected assembly kernel. 5 what this means for the engine.
Then fairness notes, limits and sources.

---

## 1. One page: right, wrong, misleading

### What the messages got right

| Topic | What was said | What we found |
|---|---|---|
| CPU layout | 24 cores, 24 threads, 8 P + 16 E, 3 MB L2 per P core, 4 MB per E cluster, 36 MB L3 (M1) | All confirmed by Windows on this machine and by Intel. |
| GPU catalogue | GB205, 5,888 CUDA cores, 12 GB GDDR7, 672 GB/s | Driver reports 46 SMs x 128 = 5,888, 12227 MiB, and 192 bit x 28.002 Gbit/s = 672.05 GB/s. |
| Tick time of the toy kernel | 15 to 30 microseconds for 100,000 agents on one P core (M2) | 20.2 us best (`cpu_memory.md`), 20.8 to 21.8 us median (`asm_kernel.md`). Noisy, parallel load. True only after the loop count is fixed. |
| Where the cost is | Neighbour search, not arithmetic; use an O(N) cell grid (M2 s4) | Arithmetic is right (10^10 pairs). Both messages agree on the grid. |
| Compilers | Clang emits nearly the same loop (M2 s4) | It did, bit-identical, and ran faster: 15.2 to 16.4 us against 20.8 to 21.8 us. |
| GPU launch cost | 3 to 10 microseconds (M2) | Launch call median 3.1 us; launch plus wait median 8.2 us; p99 above 50 us. |
| GPU determinism | Float atomics are not repeatable, integer atomics are (M1 s5) | 8 runs gave 8 different float sums and 1 integer sum. |
| Bare metal and the GPU | No OS means no CUDA on this GPU; host on an OS with a locked arena (M1 s2) | Sound. NVIDIA's driver stack needs GSP firmware and a kernel driver. |
| Boot mechanics | UEFI application, `ExitBootServices`, GOP, INIT-SIPI-SIPI (M2 s3) | Real mechanisms. Prerequisites were left out (see below). |
| Tier arithmetic | 12.8 MB hot state; 1,562 heavy updates per tick at K = 64 (M1 s3) | Arithmetic is right. The design idea itself was not tested. |

### What was wrong or misleading, with the measured number

| # | What was said | Verdict | Measured or sourced here |
|---|---|---|---|
| 1 | GPU has a "48 MiB L2 cache" | wrong | Driver reports 37,748,736 bytes = 36.00 MiB. "3.7x inside" becomes 2.95x. The part that can be prioritised is 22.5 MiB (1.84x). No residency guarantee. |
| 2 | "DDR5-6400 (~102.4 GB/s)" | wrong | This laptop runs DDR5-5600 (ceiling 89.6 GB/s). One core streams DRAM at 12.5 to 21.6 GB/s read, 8.8 to 12.0 write. |
| 3 | "Cores 0-7" are the P cores | wrong | P cores are logical processors 0, 1, 10, 11, 12, 13, 22, 23. A 0-7 mask gives 2 P + 6 E. |
| 4 | The cliff is at 36 MB | misleading | P-core read: 422 GB/s (L1d) -> 204 (192 KB level) -> 115-159 (L2) -> 43-68 (L3) -> 12.5-17.6 (DRAM). The L3-to-DRAM step is the smallest (2.5-4x). |
| 5 | 12.8 MB fits "24 MB P-core L2" | misleading | That is eight private 3 MB caches. As one data set it sits in the shared L3 at 43-49 GB/s read per core, against 115-159 in L2. |
| 6 | The listing is "functional" | wrong | With count = 100000 it runs 100,000 iterations, not 12,500, and writes 2,800,000 bytes past the end of each output array. It also destroys xmm13, xmm14, xmm15. |
| 7 | "8 ... multiply-adds in 1 clock cycle" | misleading | FMA latency is 4 cycles. The loop takes 2.0-2.1 cycles per 8 agents from L1 and 8.3 from L2 (P core). |
| 8 | "zero cache misses" with SoA | misleading | 202 ps per agent at 100,000 agents (L2) against 50 ps at 2,000 agents (L1d): 4.0x. |
| 9 | "fast tanh/ReLU activation" | misleading | The code is min(max(x, 0), 5). It differs from tanh by more than 0.01 on 3752 of 4096 random inputs. |
| 10 | E cores never inside the tick | misleading | On this L2-bound loop an E core took 12.9 us and a P core 20.2 us (E is 1.56x faster). |
| 11 | P-core L2 is "2 MB" (M2) | wrong | 3 MB. M1 has it right. |
| 12 | Past the cache, "5x to 200x across DRAM and NVMe swap" | wrong for the GPU, mostly_correct for the CPU | CPU: 2.5-4x (L3), 7-12x (L2), 24-34x (L1d) for streaming. No swap: 64-512 MB sets ran from DRAM at 9-22 GB/s. GPU: about 2.4x in the only published data. |
| 13 | PTX gives "direct register/warp control" | wrong | PTX registers are virtual. The PTX clang emitted here uses `%r1 ... %r10`. |
| 14 | Bare metal: "no interrupts", "no ... TLB misses", "100% deterministic" | misleading / wrong | 64-bit mode needs paging. SMIs cannot be masked. Under Windows the kernel's best and worst timed batches differ by 4 %. |
| 15 | "boot directly from a USB stick" | misleading (omission) | Secure Boot is on here (`UEFISecureBootEnabled : 1`). An unsigned image will not start. AVX must also be enabled by the program. |
| 16 | "boost clocks ... above 2.2 GHz" | misleading | 2,220 MHz is the top of NVIDIA's range. Driver typical clock 1,515 MHz. Power limit in force 93.22 W. |
| 17 | Q16.16 `int32` fields | mostly_correct | In the probe the cell wrapped: true total about 5.37e8, stored 0x78d74000. Repeatable, and wrong as a conserved amount. |
| 18 | AVX-512 registers | misleading | This CPU has no AVX-512 (`AVX512F=0`). |

No source and no measurement exists for these: "~2.4 TB/s (L2)" on the GPU, GPU
beats CPU "10x-30x", hand ASM wins "~5-15%", the "~50k" CPU cap, the "4-second
swap-bound stall", and the 25 W CPU limit. Treat them as guesses.

---

## 2. Full claim table

Columns: claim (short quote), verdict, evidence, source.
"Where" gives the message and section. Source keys in brackets are listed in
the last section. Local measurement keys:

| Key | What | Note |
|---|---|---|
| L-topo | `Win32_Processor`, `Win32_PhysicalMemory`, `membench --topo` | `cpu_memory.md` s1 |
| L-mem | `membench` working-set sweep, one pinned core | `cpu_memory.md` s4 |
| L-mem8 | eight `membench` instances, one per P core | `cpu_memory.md` s4b |
| L-tick | `tick_harness` calling the exhibit listing | `cpu_memory.md` s4c |
| L-asm A-F | `harness.exe` tests A to F on exhibit and fixed kernel | `asm_kernel.md` |
| L-smi | `nvidia-smi -q` | `gpu.md` A |
| L-cuattr | `cuattr.exe`, CUDA driver attributes | `gpu.md` B |
| L-cuda | `host.exe` with clang-built PTX | `cuda.md` |
| L-cpuprobe | `cpuprobe.exe`, registry, display memory ranges | `baremetal.md` |
| L-probe | `probe.exe`, CPUID and ring-3 fault test | `below_asm.md` |

### A. CPU and memory

| # | Where | Claim | Verdict | Evidence | Source |
|---|---|---|---|---|---|
| A1 | M1 s1 | "24 physical cores and 24 threads" | correct | Windows reports 24 cores, 24 logical processors, one per core. Intel lists the same. | L-topo, [ARK] |
| A2 | M1 s1 | "8 Lion Cove P-cores", "16 Skymont E-cores" | correct | 8 class-1 and 16 class-0 logical processors here. Intel lists 8 P + 16 E. | L-topo, [ARK], [CC-ARL], [TPU] |
| A3 | M1 s1 | P cores "up to 5.4 GHz" | correct | Intel: P-core max turbo 5.4 GHz. Local clock under load: 4.65 to 5.18 GHz. | [ARK], L-mem |
| A4 | M1 s1 | E cores "up to 4.6 GHz" | correct | Intel: E-core max turbo 4.6 GHz. Local clock: 4.29 to 4.58 GHz. | [ARK], L-mem |
| A5 | M1 s1 | "3 MB dedicated L2 cache per core (24 MB total P-core L2)" | correct | Every P logical processor reports L2 3072 KB shared by 1. 8 x 3 MB = 24 MB. | L-topo, [CC-ARL] |
| A6 | M1 s1 | "four 4 MB clusters (16 MB E-core L2)" | correct | Every E logical processor reports L2 4096 KB shared by 4. | L-topo, [TPU] |
| A7 | M1 s1 | "36 MB shared L3 cache" | correct | L3 36864 KB shared by 24. Intel: 36 MB. | L-topo, [ARK] |
| A8 | M1 s1 | "backed by DDR5-6400 (~102.4 GB/s)" | wrong | The CPU supports up to DDR5-6400 and 102.4 GB/s is the right ceiling for that speed. This laptop has DDR5-5600 (ceiling 89.6 GB/s), unequal 32 GB + 16 GB modules. One core: read 12.5-21.6, write 8.8-12.0, copy 4.9-8.1 GB/s. | L-topo, L-mem, [ARK] |
| A9 | M1 s2 table | "~1.2 TB/s (P-Core L2)" | mostly_correct | True only as the sum of read-only streams on all 8 P cores, each in its own L2. Measured sum: read 1021 GB/s, write 613, copy 427. One core reads 115-159 GB/s and writes 70-81. | L-mem8, L-mem, [CC-LNC] |
| A10 | M1 s2 table | "~1.2 TB/s (Zero OS jitter)" | unverifiable | Same hardware limit as A9. No bare-metal run was possible. Nothing suggests the OS lowers L2 throughput. | L-tick |
| A11 | M1 s1 | "bandwidth drops by 5x to 200x across DRAM and NVMe swap" | mostly_correct | CPU half. Measured streaming: L3 to DRAM 2.5-4x, L2 to DRAM 7-12x, L1d to DRAM 24-34x. Published latency ratios for pointer chasing: 7x, 33x, 141x. Swap does not follow: 64-512 MB sets ran from DRAM at 9-22 GB/s with no paging. GPU half: see C12. | L-mem, [CC-ARL] |
| A12 | M1 s1 | "past 36 MB on CPU" | misleading | 36 MB is the right L3 size and a real step. It is not the boundary for a per-core hot loop: a P core loses 2.1x at 48 KB, 2.7-3.7x by 256 KB-2 MB, 6-10x on leaving its 3 MB L2. Throughput had already fallen at 32 MB because L3 is shared. | L-mem |
| A13 | M1 s3 | "or inside the 275HX's 24 MB P-core L2" | misleading | Eight private 3 MB caches, not one pool. 12.8 MB fits only as eight 1.6 MB slices, each touched by one pinned P core. Neighbour reads across slices break that. As one set it lives in L3. | L-topo, L-mem |
| A14 | M1 s4 | "Cores 0-7 (8 Lion Cove P-Cores)" | wrong | P cores here are logical processors 0, 1, 10-13, 22, 23 (x2APIC IDs 0, 8, 32, 40, 48, 56, 80, 88). Three notes measured this. | L-topo, L-cpuprobe, L-probe |
| A15 | M1 s5 | "Sustaining 160W on the 275HX" | correct | Intel: maximum turbo power 160 W (base 55 W). The laptop's own limits were not read. | [ARK] |
| A16 | M1 s3 | "only 1,562 organisms execute heavy state updates per tick" | correct | 100,000 / 64 = 1,562.5. | arithmetic |
| A17 | M1 s1, M2 s4 | "10^10 pairs at N = 100,000" | correct | 100,000^2 = 10^10 ordered pairs (5 x 10^9 unordered). | arithmetic |
| A18 | M2 intro | "(using AVX2/AVX-512 SIMD registers)" | misleading | True for x86-64 in general. This CPU has no AVX-512. The listing itself uses only AVX2 and FMA. | [ARK], L-cpuprobe, L-probe |
| A19 | M2 s2 | "4 arrays x 100,000 x 4 bytes = 1.6 MB total" | correct | Arithmetic. It fits one P core's 3 MB L2. | L-topo |
| A20 | M2 s2 | "a single P-core's 2 MB L2 cache" | wrong | It is 3 MB here. The conclusion still holds (A19). `asm_kernel.md` and `below_asm.md` rate the whole sentence mostly_correct for that reason. | L-topo, L-probe, [CC-ARL] |
| A21 | M2 s2 | "never touching DDR5 RAM" | mostly_correct | Once warm and if nothing evicts it. The first pass, and any pass after the core ran other work, loads from DRAM. | L-mem |
| A22 | M2 s2 | "executes 8 floating-point multiply-adds in 1 clock cycle" | misleading | The instruction has 4 cycles of latency; two can start per cycle. The loop is 12 instructions with 3 loads and 2 stores: 2.0-2.1 cycles per 8 agents from L1, 8.3 from L2 (P core). llvm-mca predicts 2.04. | [UOPS], L-asm E |
| A23 | M2 s2 | "~0.18 nanoseconds at 5.4 GHz" | correct | 1 / 5.4e9 = 0.185 ns. | arithmetic, [ARK] |
| A24 | M2 s1 | "processes 8 float32 neurons per clock cycle per core" | wrong | About 1 agent per cycle at 100,000 agents; about 4 per cycle when the data fits L1d. `cpu_memory.md` rates it misleading, `asm_kernel.md` wrong. | L-tick, L-asm E |
| A25 | M2 s2 | "(12,500 loop iterations)" | wrong | As written the listing decrements once per 8-agent block from the agent count. count = 100,000 runs 100,000 iterations. | L-asm A, exhibit lines 22, 48-50 |
| A26 | M2 s2 | "~15 to 30 microseconds (>30,000 ticks per second)" | mostly_correct | One P core with the loop count fixed: 20.2 us best for the exhibit called with count = 12,500 (L-tick), 20.8-21.8 us median for the fixed kernel (L-asm E). As written with count = 100000 it takes 264-271 us. `cpu_memory.md` rates the figure correct. | L-tick, L-asm E |
| A27 | M2 s2 | "zero cache misses if structured as SoA" | misleading | SoA removes wasted bytes, not misses. 1.6 MB does not fit the 48 KB or 192 KB levels. 202 ps per agent (L2) against 50 ps (L1d). "No DRAM access after warm-up" would be defensible. | L-mem, L-tick, [LNC-SLIDE] |
| A28 | M1 s4 | "never permitted inside a simulation tick barrier" | misleading | Said of E cores. For this L2-bound kernel an E core was 1.56x faster than a P core (12.9 against 20.2 us). E cores also write and copy faster for 64 KB-4 MB. With all four cores of a cluster busy: not measured. | L-mem, L-tick |
| A29 | M2 s3 | "your other 7 Lion Cove P-cores" | correct | 8 P cores; 100,000 / 8 = 12,500 per core. | L-topo |
| A30 | M2 s4 | "Hand-written ASM only wins by ~5-15%" | unverifiable | No source. For this one kernel the sign is reversed: clang output 15.2-16.4 us, hand-written loop 20.8-21.8 us. One kernel, noisy. | L-asm F |
| A31 | M1 s2 table | "Caps out past ~50k ultra-heavy raycasting agents" | unverifiable | No definition of the agent, no measurement. | none |
| A32 | M1 end | "a 4-second swap-bound stall into a sub-millisecond tick" | unverifiable | No evidence the existing stall is caused by swap. Sub-millisecond is measured only for the trivial kernel. | L-tick |

### B. The assembly listing (M2 section 2)

| # | Where | Claim | Verdict | Evidence | Source |
|---|---|---|---|---|---|
| B1 | M2 s2 | "an actual, functional x86-64 AVX2 Assembly kernel" | wrong | It assembles and the first `count` agents come out bit-identical to a C reference. But it overruns memory (A25) and breaks the calling convention (B2). | L-asm A, B, C |
| B2 | listing header | "(Windows x64 ABI)" | wrong | The listing overwrites xmm13, xmm14, xmm15 with no save or restore. Microsoft: XMM6 to XMM15 must be preserved by the callee. A caller's 1234.5 in xmm15 came back as 5.0. | L-asm B, [MS-ABI] |
| B3 | listing header | "[RSP+40] = uint64_t count" | correct | 8 bytes return address + 32 bytes shadow space. A compiled call site stores the value at `[rsp+0x20]` before `call`. | L-asm D, [MS-ABI] |
| B4 | M2 s2 | "weighted sensory dot-product + fast tanh/ReLU activation" | misleading | One multiply, not a dot product. ReLU and a clamp at 5, no tanh. The listing's own comment says `max(0.0, sense * weight)`, so the listing is honest and the sentence oversells. | L-asm C |
| B5 | M2 s4 | "will emit almost the exact same vfmadd231ps loop" | correct | clang 21.1.8 with the named flags emits the same four instructions on ymm registers, unrolled by 2, bit-identical. gcc 16.1.0 without fast-math does not vectorise it: 352-357 us, 17 times slower. | L-asm F |
| B6 | M2 s4 | "The Real ALife Bottleneck Isn't Neural Math" | mostly_correct | The pair count is right. The cost of a neighbour search was not measured. The listing has no neighbour term at all, which undercuts A26. | arithmetic, L-asm |

### C. GPU hardware

| # | Where | Claim | Verdict | Evidence | Source |
|---|---|---|---|---|---|
| C1 | M1 s1 | "(Blackwell GB205)" | correct | Driver: architecture Blackwell, device 0x2F1810DE. The PCI ID database names it GB205M. | L-smi, [PCI], [NBC] |
| C2 | M1 s1, M2 | "5,888 CUDA cores" | correct | NVIDIA lists 5888. Driver: 46 SMs; 46 x 128 = 5,888. A "CUDA core" is one lane in an SM, not an independent core. | [NV-CMP], L-cuattr |
| C3 | M1 s1 | "12 GB GDDR7" | correct | NVIDIA: 12 GB GDDR7. Driver: 12227 MiB total. | [NV-CMP], L-smi |
| C4 | M1 s1 | "672-677 GB/s" | mostly_correct | 192 bit x 28.002 Gbit/s / 8 = 672.05 GB/s. 677 has no derivation. It is an interface ceiling, not kernel throughput. | L-cuattr, [NV-CMP], [JEDEC] |
| C5 | M1 s1 | "5.8 MiB L1/shared SRAM" | mostly_correct | 46 SMs x 128 KiB = 5.75 MiB. Not one pool: one block per SM. A thread block can use at most 101,376 bytes of it. | [NV-BTG], L-cuattr, [NBC] |
| C6 | M1 s1, s3, end | "48 MiB L2 cache" | wrong | Driver: `L2_CACHE_SIZE` = 37,748,736 bytes = 36.00 MiB. The 48 comes from a third-party spec table; the chatbot repeated it, it did not invent it. | L-cuattr, [NBC] |
| C7 | M1 s1 | "992 INT8 TOPS" | mostly_correct | NVIDIA lists "AI TOPS" 992 without the word INT8. A peak figure for tensor cores. It says nothing about `dp4a` code or memory-bound work. | [NV-CMP], [NBC] |
| C8 | M1 s2 table | "~2.4 TB/s (L2)" | unverifiable | No source anywhere. NVIDIA publishes no L2 bandwidth. Not measured here. | [BW-MB] |
| C9 | M1 s5 | "115-140W on the 5070 Ti" | mostly_correct | NVIDIA: 60-115 W; 140 W with Dynamic Boost. Driver: max limit 140.00 W, limit in force 93.22 W, default 65.00 W. | [NV-CMP], [NBC], L-smi |
| C10 | M1 s5 | "GPU boost clocks hold steady above 2.2 GHz" | misleading | NVIDIA: boost clock 1447-2220 MHz. 2,220 is the maximum, not a floor. Driver typical clock 1,515 MHz. No load clock was measured. | [NV-CMP], L-cuattr |
| C11 | M1 s3, end | "fits 3.7x inside the 5070 Ti's 48 MiB L2 cache" | misleading | 36 MiB / 12.21 MiB = 2.95x. Prioritised set-aside is at most 22.5 MiB (1.84x). The hot array is not the whole tick. L2 is shared by all SMs and processes, with no residency guarantee. | L-cuattr, [NV-L2] |
| C12 | M1 s1 | "or 48 MiB on GPU" (then the 5x to 200x drop) | wrong | Past L2 a GPU reads GDDR7. The only published latency data shows about 2.4x (358 against 877 cycles, on a desktop RTX 5080, a different chip). There is no swap tier: 12.8 MB is about 0.1 % of VRAM. | [BW-MB], L-cuattr |
| C13 | M1 s2 | "requires proprietary NVIDIA GSP driver" | mostly_correct | NVIDIA: Blackwell is supported only by the open kernel modules, which depend on GSP firmware. The kernel driver is open source; the firmware and user-space stack are not. Conclusion stands: bare metal means CPU only. | [NV-GSP] |

### D. CUDA and PTX

| # | Where | Claim | Verdict | Evidence | Source |
|---|---|---|---|---|---|
| D1 | M1 s5 | "atomicAdd on float32 is non-associative due to warp scheduling order" | correct | 8 runs, 8 different float bit patterns. The wording is loose: the addition is non-associative, the arrival order is unspecified. | L-cuda, [ATOM] |
| D2 | M1 s5 | "integer atomicAdd is strictly associative and bit-exact across runs" | correct | 1 distinct `int32` result in 8 runs. Caveats not stated: only the final sum is order-free; the returned old value is not; clamped or take-if-left updates are order-dependent. | L-cuda |
| D3 | M1 s5 | "fixed-point int32 (Q16.16)" | mostly_correct | Fixed point is the right tool. Q16.16 holds integer parts in [-32768, 32767]. In the probe the true total (about 5.37e8) wrapped to 0x78d74000. Each field needs a proven bound or a 64-bit cell. | L-cuda |
| D4 | M1 s4 | "Execute a 4-stage CUDA Graph per tick" | mostly_correct | Graphs exist for this. The saving is small: NVIDIA measured 3.8 us per kernel without a graph and 3.4 us with one. The driver here exports the entry points; not exercised. | [NV-GRAPH], L-cuda |
| D5 | M1 s4 | "with zero PCIe copies back to DDR5" | mostly_correct | CUDA-Vulkan external memory is real. Left out: semaphores for access, same physical GPU (an Intel iGPU is also present). Not built or measured here. | [NV-EXT] |
| D6 | M1 s3 | "(dp4a / INT8 Tensor instructions)" | mostly_correct | `dp4a` is real and ran here (returned 16, expected 16). It is an ordinary scalar integer instruction, not a tensor-core one. Tensor-core INT8 forms are a separate family; not tested. | L-cuda, [PTX], [LLVM-NVPTX] |
| D7 | M1 s4 | "O(N) Radix Sort on 32-bit Morton Z-order spatial curve codes" | mostly_correct | Linear for fixed-width keys. 3D codes give 30 usable bits (10 per axis). M2's counting sort into cells is one pass and cheaper for a fixed grid. | [SORT] |
| D8 | M2 s1 | "zero warp divergence, explicit shared memory banking" | wrong | PTX registers are virtual. Warp scheduling is hardware behaviour. Divergence comes from control flow, not the language. M1 s5 itself says warps "will diverge" on sparse brains. | [PTX], L-cuda |
| D9 | M2 s1 | "CUDA Driver API dispatch (~3-10 us)" | correct | Launch call: min 2.0, median 3.1, p99 52 us. Launch plus wait: min 7.0, median 8.2, p99 59.3 us. So a GPU cannot win on a 15-30 us tick. | L-cuda, [NV-GRAPH] |
| D10 | M1 s4 | "Persistent Megakernel (CUDA Graphs + Inline PTX)" | misleading | A megakernel is one kernel. A 4-stage graph is four kernels, the opposite design. "Inline PTX" needs `nvcc`, which is not on this machine. Keeping state on the GPU is sound. | [MEGA] |
| D11 | M2 s4 | "will still beat bare-metal CPU ASM by 10x-30x" | unverifiable | Nothing was run for this GPU against this CPU. Published work in the same class reports about 12.5x. M2 limits the claim to "heavy" agents and offers it as an estimate. | [HOOMD] |
| D12 | M1 s5 | "keeping the 275HX under 25W duty" | unverifiable | The power figures around it are mostly right (A15, C9). No source for the 25 W figure or for "throttle within minutes". | none |

### E. Bare metal

| # | Where | Claim | Verdict | Evidence | Source |
|---|---|---|---|---|---|
| E1 | M2 s3 | "Write a UEFI x86-64 application (.efi binary)" | correct | Standard route. Firmware loads `\EFI\BOOT\BOOTx64.EFI` from removable media and is already in 64-bit mode. | [UEFI-3], [UEFI-7] |
| E2 | M2 s1 table | "the framebuffer (0xE0000000 GOP linear memory)" | misleading | The address is read at run time and a mode may have no linear framebuffer. No display memory range on this laptop is at 0xE0000000. M2 s3 says the correct thing. | [UEFI-12], L-cpuprobe |
| E3 | M2 s3 | "writes RGBA pixels directly into VRAM at hardware refresh speed" | mostly_correct | Direct writes are what GOP is for. Pixel order may be BGR. There is no vertical-sync service, so expect tearing. | [UEFI-12] |
| E4 | M2 s3 | "Send an INIT-SIPI-SIPI IPI" | mostly_correct | Right mechanism after `ExitBootServices`. Left out: each woken core starts in 16-bit real mode and needs a trampoline, its own stack and page tables. Core IDs are not 1 to 7 (A14). | [SDM], [MPSVC], L-cpuprobe |
| E5 | M2 s1 table | "No Windows, no pagefile, no interrupts, no background threads" | misleading | No scheduler and no pagefile is true. Firmware interrupts (SMI), machine-check and NMI still arrive; `CLI` does not mask them. | [HWLAT], [SMM] |
| E6 | M1 s2 | "100% deterministic control over the 275HX" | misleading | Firmware, microcode and thermal control stay in charge. M1 s5 itself says the chassis will throttle. Bit-exact replay comes from integer state and a counter RNG on any OS. | [HWLAT], [SMM] |
| E7 | M2 s3 | "no virtual memory TLB misses" | wrong | 64-bit mode cannot run with paging off. UEFI hands over with paging on and memory identity mapped. Large pages cut misses; they do not remove them. Three notes agree. | [UEFI-2], [SDM] |
| E8 | M2 s3 | (omission) AVX enabling is never mentioned | misleading | AVX instructions fault unless the program sets `CR4.OSXSAVE` and `XCR0`. Today Windows does it (`XCR0=0x7`). Each woken core needs it too. | [OSDEV-AVX], L-cpuprobe |
| E9 | M2 end | "boot directly from a USB stick on your laptop" | misleading | Secure Boot is enabled here (`UEFISecureBootEnabled : 1`). An unsigned image is not started until the owner changes firmware settings. | L-cpuprobe, [UEFI-32] |
| E10 | M2 s1 table | "You own 100% of physical RAM, L1/L2/L3 caches" | mostly_correct | After `ExitBootServices` the image owns unused memory. Not owned: runtime-services, ACPI, SMM memory, iGPU stolen memory. `below_asm.md` rates this misleading. | [UEFI-7], [SMM] |
| E11 | M1 s4 | "prevents OS pagefile eviction to your SSD permanently" | mostly_correct | Locked pages are not written to the pagefile while locked. Not permanent: until unlock or exit, and only after the working-set size is raised. | [MS-VLOCK] |
| E12 | M2 s1 table | "Windows/Linux scheduler & virtual memory paging" | correct | Ring 3 cannot mask interrupts or read control registers: `CLI`, `RDMSR`, `RDPMC` all fault here. | L-probe |

### F. Below assembly

| # | Where | Claim | Verdict | Evidence | Source |
|---|---|---|---|---|---|
| F1 | M2 s1 table | "No CPU, no fetch-decode-execute cycle, no ISA" | correct | True of custom logic by definition. On an FPGA the "gates" are look-up tables and routing switches. There is no FPGA in this machine. | [NEXTPNR] |
| F2 | M2 s1 table | "Signals propagate at near-light speed through silicon combinational logic" | misleading | Speed is set by gate and routing delay. FPGA critical paths are about 3 to 4 times slower than the same design as an ASIC. The gain is parallelism. `baremetal.md` rates the whole row misleading. | [KUON] |

### Verdict tally

Counted from the verdict column of the 77 rows A1 to F2 above, not from
memory.

| Verdict | A | B | C | D | E | F | Total |
|---|---:|---:|---:|---:|---:|---:|---:|
| correct | 13 | 2 | 3 | 3 | 2 | 1 | 24 |
| mostly_correct | 4 | 1 | 5 | 5 | 4 | 0 | 19 |
| misleading | 6 | 1 | 2 | 1 | 5 | 1 | 16 |
| wrong | 5 | 2 | 2 | 1 | 1 | 0 | 11 |
| unverifiable | 4 | 0 | 1 | 2 | 0 | 0 | 7 |
| split | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| Rows in the section | 32 | 6 | 13 | 12 | 12 | 2 | 77 |

Check: 24 + 19 + 16 + 11 + 7 + 0 = 77, and 32 + 6 + 13 + 12 + 12 + 2 = 77.
Each section column adds up to its row count.

Rows by verdict:

- correct (24): A1, A2, A3, A4, A5, A6, A7, A15, A16, A17, A19, A23, A29;
  B3, B5; C1, C2, C3; D1, D2, D9; E1, E12; F1.
- mostly_correct (19): A9, A11, A21, A26; B6; C4, C5, C7, C9, C13; D3, D4,
  D5, D6, D7; E3, E4, E10, E11.
- misleading (16): A12, A13, A18, A22, A27, A28; B4; C10, C11; D10; E2, E5,
  E6, E8, E9; F2.
- wrong (11): A8, A14, A20, A24, A25; B1, B2; C6, C12; D8; E7.
- unverifiable (7): A10, A30, A31, A32; C8; D11, D12.
- split (0): no row in this part carries a split verdict. The split verdicts
  of Part 1 are separate rows here. Part 1 #12 ("wrong for the GPU,
  mostly_correct for the CPU") is A11 (CPU half) and C12 (GPU half). Part 1
  #14 ("misleading / wrong") is E5 and E6 (misleading) and E7 (wrong).

In short: 43 of the 77 rows are correct or mostly_correct, 27 are misleading
or wrong, and 7 are unverifiable. Where two notes gave a claim different
verdicts (A20, A24, A26, E10), the row is counted under the verdict printed
in it; the fairness notes give the other one.

---

## 3. Measured tables

Every number in this part is copied from the note named under its table.
Nothing was re-run for this document. Every timing is noisy, parallel load.

### 3.1 CPU topology

| Logical processors | Kind | EfficiencyClass | L1d | L2 (shared by) | L3 (shared by) | x2APIC IDs |
|---|---|---:|---:|---|---|---|
| 0, 1 | P | 1 | 48 KB | 3072 KB (1) | 36864 KB (24) | 0, 8 |
| 2 to 9 | E | 0 | 32 KB | 4096 KB (4) | 36864 KB (24) | 16 to 30 |
| 10 to 13 | P | 1 | 48 KB | 3072 KB (1) | 36864 KB (24) | 32, 40, 48, 56 |
| 14 to 21 | E | 0 | 32 KB | 4096 KB (4) | 36864 KB (24) | 64 to 78 |
| 22, 23 | P | 1 | 48 KB | 3072 KB (1) | 36864 KB (24) | 80, 88 |

Sources: `cpu_memory.md` s1 (`membench --topo`); x2APIC IDs from
`baremetal.md` s12 (`cpuprobe.exe`); EfficiencyClass lists from
`below_asm.md`, "Local measurement" (`probe.exe`, CPU Sets). One logical
processor per core, no SMT. Supports A1, A2, A5, A6, A7, A14, E4.

| Item | Value | Note |
|---|---|---|
| `Win32_Processor` | 24 cores, 24 logical processors; `MaxClockSpeed` 2700; `L2CacheSize` 40960; `L3CacheSize` 36864 | `cpu_memory.md` s1 |
| `Win32_PhysicalMemory` | Micron 32 GB (ChannelA) and Lexar 16 GB (ChannelB), `Speed` and `ConfiguredClockSpeed` 5600 for both | `cpu_memory.md` s1 |
| CPUID leaf 4 on a P core | L1d 48 KB, L1i 64 KB, L2 3072 KB, L3 36864 KB; the 192 KB level is not listed | `below_asm.md`, Local measurement |
| CPUID features | AVX2 yes, AVX512F no, AVX-VNNI yes, 1 GB pages yes, hypervisor yes, CLDEMOTE no, PREFETCHI no | `below_asm.md`, Local measurement |
| AVX state under Windows | `OSXSAVE=1`, `XCR0=0x7` (SSE and AVX on); `AVX2=1 AVX512F=0` | `baremetal.md` s7 |
| Clock, 32-add chain, best of 5 (noisy, parallel load) | P core (LP 1): 4.65 GHz small run, 5.09 GHz large run. E core (LP 3): 4.29 and 4.58 GHz | `cpu_memory.md` s3 |

Supports A3, A4, A8, A18, E8.

### 3.2 Memory working-set sweep, one pinned core (noisy, parallel load)

Single thread; P core pinned to LP 1, E core to LP 3. Sequential read, write
and copy with 256-bit AVX2 kernels, best of 5 trials. GB/s = 1e9 bytes/s.
"fits" is the level Windows reports for that core. `*` = inside Lion Cove's
192 KB middle data cache, which Windows does not report.

| Working set | P fits | P read | P write | P copy | E fits | E read | E write | E copy |
|---:|---|---:|---:|---:|---|---:|---:|---:|
| 16 KB  | L1d  | 393.1 | 312.3 | 316.0 | L1d  | 197.1 | 140.1 | 142.7 |
| 32 KB  | L1d  | 422.1 | 323.7 | 318.0 | L1d  | 174.9 | 138.7 | 113.1 |
| 64 KB  | L2*  | 203.7 | 105.5 |  77.0 | L2   | 118.2 |  97.1 |  69.7 |
| 128 KB | L2*  | 204.2 | 108.5 |  77.2 | L2   | 120.4 |  96.2 |  68.8 |
| 256 KB | L2   | 159.4 |  71.5 |  49.8 | L2   | 118.8 |  95.4 |  69.7 |
| 512 KB | L2   | 148.1 |  79.6 |  56.3 | L2   | 119.1 |  95.3 |  67.3 |
| 1 MB   | L2   | 147.1 |  80.8 |  57.9 | L2   | 118.1 |  93.3 |  66.0 |
| 2 MB   | L2   | 114.9 |  70.4 |  48.8 | L2   | 119.3 |  95.4 |  68.1 |
| 4 MB   | L3   |  67.7 |  54.9 |  30.7 | L2   | 108.5 |  82.1 |  58.2 |
| 8 MB   | L3   |  48.9 |  46.3 |  24.0 | L3   |  60.3 |  47.0 |  30.2 |
| 16 MB  | L3   |  43.2 |  44.7 |  22.8 | L3   |  43.3 |  45.5 |  27.2 |
| 32 MB  | L3   |  25.9 |  11.7 |   9.1 | L3   |  15.2 |  11.7 |   7.7 |
| 64 MB  | DRAM |  12.5 |  10.8 |   4.9 | DRAM |  19.0 |  10.0 |   5.7 |
| 128 MB | DRAM |  17.6 |   9.3 |   5.9 | DRAM |  13.3 |   9.4 |   8.1 |
| 256 MB | DRAM |  17.4 |  11.0 |   6.4 | DRAM |  20.0 |  10.1 |   7.7 |
| 512 MB | DRAM |  16.0 |  12.0 |   6.0 | DRAM |  21.6 |   8.8 |   7.9 |

Ratios the note reads off the table: P-core read goes L1d 422 -> 192 KB level 204
(2.1x) -> L2 115-159 (2.7-3.7x) -> L3 43-68 (6-10x) -> DRAM 12.5-17.6
(24-34x below L1d). Leaving L3 for DRAM costs 2.5-4x for reads and 4-5x for
writes. Leaving a P core's L2 for DRAM costs 7-12x for reads. The drop shows
already at 32 MB, because the L3 is shared with 23 other cores, which were
busy. For 64 KB to 4 MB an E core writes and copies faster than a P core
(95 against 70-80 GB/s write, 68 against 49-58 GB/s copy). No paging was seen
at 64 to 512 MB on this 48 GB machine.

Source: `cpu_memory.md` s4 (commands in s3; the paging remark is from s6
row 13). Supports A8, A9, A11, A12, A13, A21, A27, A28.

### 3.3 All eight P cores at once (noisy, parallel load)

Eight concurrent `membench` instances, one per P logical processor, 1 MB
working set each, so each runs inside its own L2.

| LP | Clock (GHz) | Read (GB/s) | Write (GB/s) | Copy (GB/s) |
|---:|---:|---:|---:|---:|
| 0 | 4.08 | 118.0 | 73.3 | 48.9 |
| 1 | 4.40 | 122.3 | 76.3 | 52.2 |
| 10 | 4.34 | 128.4 | 77.4 | 54.7 |
| 11 | 4.74 | 127.4 | 77.9 | 53.7 |
| 12 | 4.66 | 126.5 | 77.4 | 54.8 |
| 13 | 4.62 | 135.3 | 77.1 | 57.1 |
| 22 | 4.84 | 135.4 | 77.2 | 53.4 |
| 23 | 4.84 | 127.5 | 76.7 | 52.2 |
| Sum over 8 P cores | | 1020.8 | 613.3 | 427.0 |

Source: `cpu_memory.md` s4b. Supports A9.

### 3.4 The M2 kernel, timed (noisy, parallel load)

`tick_harness` calls the exhibit listing, assembled unmodified, for 100,000
agents on one pinned core. It passes count = 12,500, because the listing
runs 8 times too far when it is given the agent count (A25), and it keeps
nothing in xmm13 to xmm15 (B2). 50,000 ticks timed.

| Core | Best ns per tick | Median | Worst batch | Ticks/s at best | ps per agent at best |
|---|---:|---:|---:|---:|---:|
| P (LP 1) | 20,158 | 20,438 | 20,968 | 49,608 | 201 |
| E (LP 3) | 12,925 | 12,963 | 13,255 | 77,369 | 129 |
| P (LP 12) | 20,186 | 20,360 | 21,412 | 49,539 | 201 |

The same kernel at other populations (`tick_harness2`):

| Core | Agents | Data size | Level | ns per tick (best) | ps per agent |
|---|---:|---:|---|---:|---:|
| P (LP 1) | 2,000 | 32 KB | L1d | 101 | 50 |
| P (LP 1) | 8,000 | 128 KB | 192 KB level | 1,296 | 162 |
| P (LP 1) | 100,000 | 1.6 MB | L2 | 20,272 | 202 |
| E (LP 3) | 2,000 | 32 KB | L1d | 208 | 104 |
| E (LP 3) | 100,000 | 1.6 MB | L2 | 12,925 | 129 |

On a P core the kernel costs 4.0x more per agent from L2 than from L1d. On
L1d-sized data the P core is 2x faster than the E core; on L2-sized data the
E core is 1.56x faster than the P core. The 100,000-agent P-core figure in
the second table comes from a separate run of `tick_harness2`.

Source: `cpu_memory.md` s4c. Supports A10, A24, A26, A27, A28.

### 3.5 Assembly tests A to F (exhibit and fixed kernel)

`harness.exe` runs the exhibit (assembled from a byte-identical copy) and the
fixed kernel `step_alife_avx2_fixed.asm` (Part 4). Tools: NASM 2.16.03,
gcc 16.1.0, clang and llvm-mca 21.1.8.

Test A, loop counter. Every array holds 8N + 64 floats, indices N and up hold
sentinels, and the kernel is called with count = N = 100000.

| Kernel | Agents written below N | Sentinels overwritten, N to 8N-1 | Guard after 8N overwritten | Bytes written past each output array |
|---|---|---|---:|---:|
| Exhibit | 100000 of 100000 | vel 700000 of 700000, pos 700000 of 700000 | 0 | 2,800,000 |
| Fixed | 100000 of 100000 | vel 0 of 700000, pos 0 of 700000 | 0 | 0 |

With count = 1024 the exhibit overwrote 7168 of 7168 sentinels in each
array. By reading, not run: count = 0 makes the exhibit's loop run 2^64
times.

Test B, Windows x64 ABI. A NASM caller loads known values, calls the kernel
and reads the registers back.

| Register | Before the call | Exhibit, after | Fixed, after |
|---|---|---|---|
| xmm13 | 13 (0x41500000) | 0.016 (0x3C83126F), clobbered | 13, preserved |
| xmm14 | 14 (0x41600000) | 0 (0x00000000), clobbered | 14, preserved |
| xmm15 | 1234.5 (0x449A5000) | 5 (0x40A00000), clobbered | 1234.5, preserved |

Test C, what the "activation" computes:

| x = sense*weight | kernel | min(max(x,0),5) | tanh(x) |
|---:|---:|---:|---:|
| -10 | 0 | 0 | -1.000000 |
| -2 | 0 | 0 | -0.964028 |
| -0.5 | 0 | 0 | -0.462117 |
| 0 | 0 | 0 | 0.000000 |
| 0.25 | 0.25 | 0.25 | 0.244919 |
| 0.5 | 0.5 | 0.5 | 0.462117 |
| 1 | 1 | 1 | 0.761594 |
| 2 | 2 | 2 | 0.964028 |
| 3 | 3 | 3 | 0.995055 |
| 5 | 5 | 5 | 0.999909 |
| 10 | 5 | 5 | 1.000000 |
| 100 | 5 | 5 | 1.000000 |

Random test, N = 4096. Fixed kernel against min(max(s*w,0),5): 0 bit
mismatches. Exhibit against the fixed kernel on the first N agents: 0. Each
of four C builds (gcc -O3, gcc -O3 -ffast-math, clang -O3 -ffast-math,
clang -O3) against the fixed kernel: 0. Kernel against tanh(s*w): 3752 of
4096 differ by more than 0.01, maximum difference 4.0001. 2050 inputs had
s*w < 0 (ReLU gives 0) and 197 had s*w > 5 (clamped).

Test D, fifth argument. A gcc -O1 call site does `sub rsp,0x38`, stores
100000 (`0x186a0`) at `[rsp+0x20]`, then calls. The callee sees the value at
`[rsp+0x28]`, which is `[rsp + 40]`.

Test E, timing (noisy, parallel load). P core = logical CPU 10, estimated
clock 4.87 GHz before and 5.17 GHz after the runs (64-bit IMUL chain, 3
cycles per step assumed). 200 warm-up ticks, then 2000 timed ticks
(2,000,000 for N = 1024); `QueryPerformanceCounter` resolution 100 ns.
Values are ns per tick.

| Code, P core, run 1 | N | min | median | p99 | max |
|---|---:|---:|---:|---:|---:|
| fixed asm | 100000 | 20000 | 21200 | 24300 | 741100 |
| gcc -O3 -march=native (scalar) | 100000 | 344700 | 357200 | 534500 | 1176600 |
| gcc -O3 -march=native -ffast-math | 100000 | 19500 | 23000 | 27500 | 29300 |
| clang -O3 -march=native -ffast-math | 100000 | 15100 | 16400 | 18100 | 21600 |
| clang -O3 -march=native | 100000 | 14900 | 15700 | 17600 | 23500 |
| fixed asm (again) | 100000 | 20000 | 21800 | 24100 | 30900 |
| exhibit as written (does 8N agents) | 100000 | 258400 | 271000 | 362000 | 620500 |
| exhibit called with count = N/8 | 12500 | 20000 | 22000 | 24400 | 26700 |
| fixed asm, cache-resident | 1024 | 51.5 | 55.5 | 62.0 | 67.5 |
| clang -O3 fast-math, cache-resident | 1024 | 50.6 | 51.0 | 60.3 | 387.7 |

Fixed asm at N = 100000: 8.25 cycles per 8-agent block, 47170 ticks/s;
cache-resident, 2.11 cycles per block. Exhibit as written: 13.19 cycles per
block, 3690 ticks/s. Run 2 medians agreed within 5 %, except gcc fast-math
at 14 %.

| Code, E core (logical CPU 4, estimated 3.2 to 3.3 GHz) | N | min | median | p99 | max |
|---|---:|---:|---:|---:|---:|
| fixed asm | 100000 | 12800 | 13700 | 14900 | 29400 |
| exhibit as written (does 8N agents) | 100000 | 229700 | 278100 | 1394400 | 3858300 |
| fixed asm, cache-resident | 1024 | 95.2 | 96.2 | 116.5 | 652.1 |

E core: 3.51 cycles per block at N = 100000 (72993 ticks/s), 2.41 cycles
per block cache-resident.

| Quantity | Claimed | Measured, median (noisy, parallel load) |
|---|---|---|
| 100k-agent tick, fixed kernel, one P core | 15 to 30 us | 20.8 to 21.8 us |
| Ticks per second, one P core | more than 30,000 | about 46,000 to 48,000 |
| Same, one E core | not claimed | 13.7 to 14.1 us (about 71,000 to 73,000 per second) |
| Exhibit as written, count = 100000, arrays 8x oversized | 15 to 30 us | 264 to 271 us (about 3,700 per second) |
| Cycles per 8-agent block, data in first-level cache | 1 | 2.0 to 2.1 (P), 2.4 (E) |
| Cycles per 8-agent block, 1.6 MB streamed from L2 | 1 | 8.3 (P), 3.5 (E) |

llvm-mca on the exhibit's loop body, a model and not a timing (LLVM 21 uses
its Alder Lake P-core model for `arrowlake-s`; it has no Lion Cove model):
12 instructions per iteration, 2038 cycles for 1000 iterations, IPC 5.89,
block reciprocal throughput 2.0. Predicted cycles per iteration:
arrowlake-s and alderlake 2.04, skylake 2.36, znver5 2.52. `vfmadd231ps` is
listed at latency 4 and throughput 0.5.

Test F, compiler parity (noisy, parallel load). P core, 100k tick, ranges
over the two runs:

| Code | Vectorised | us per tick | Output against the fixed kernel |
|---|---|---|---|
| clang 21.1.8 -O3 -march=native -ffast-math | yes: vmulps, vmaxps, vminps, vfmadd231ps on ymm, unrolled by 2 | 15.7 to 16.4 | bit-identical |
| clang 21.1.8 -O3 -march=native | yes | 15.2 to 15.7 | bit-identical |
| gcc 16.1.0 -O3 -march=native -ffast-math | yes | 19.8 to 23.0 | bit-identical |
| gcc 16.1.0 -O3 -march=native | no ("unsupported control flow in loop"); a scalar loop with a branch for the ReLU | 352 to 357 | bit-identical |
| hand-written loop (fixed kernel) | - | 20.8 to 21.8 | - |

Source: `asm_kernel.md`, Build, Tests A to F and "llvm-mca prediction".
Supports A22, A24, A25, A26, A27, A30, B1, B2, B3, B4, B5.

### 3.6 GPU attributes (no kernel run)

`nvidia-smi -q` with the GPU idle (performance state P8), selected lines:

| Field | Value |
|---|---|
| Product Name | NVIDIA GeForce RTX 5070 Ti Laptop GPU |
| Product Architecture | Blackwell |
| Driver Version, CUDA Version | 616.87, 13.4 |
| Driver Model | WDDM |
| GPU Part Number, Device Id | 2F18-750-A1, 0x2F1810DE |
| GSP Firmware Version | N/A |
| PCIe generation, max and current | 5, 1 |
| Link width, max and current | 16x, 8x |
| FB memory: total, reserved, used, free | 12227, 283, 402, 11543 MiB |
| Power limit: current, default, min, max | 93.22, 65.00, 5.00, 140.00 W |
| Instantaneous power draw | 5.48 W |
| Clocks, idle: graphics, SM, memory | 180, 180, 405 MHz |
| Max clocks: graphics, SM, memory | 3090, 3090, 14001 MHz |
| Temperature: current, target | 56 C, 87 C |

A second query in `cuda.md` (`nvidia-smi --query-gpu=...`, also idle, 55 C,
13.69 W draw) gave a default limit of 65.00 W, min 5.00 W, max 140.00 W, a
graphics clock of 1590 MHz and a maximum graphics clock of 3090 MHz; the
table view showed a 93 W cap at that moment. "Max Clocks 3090 MHz" is the
top of the driver's clock table, not a boost clock this laptop reaches. No
clock under load was measured.

Sources: `gpu.md` A; `cuda.md`, "Other local facts". Supports C1, C3, C9,
C10.

`cuattr.exe` loads `nvcuda.dll` and calls only `cuInit` and
`cuDeviceGetAttribute`; it creates no context and runs no kernel.

| Attribute | Value |
|---|---|
| TOTAL_MEM | 12792037376 bytes = 12199.4 MiB |
| COMPUTE_CAPABILITY_MAJOR, _MINOR | 12, 0 |
| MULTIPROCESSOR_COUNT | 46 |
| WARP_SIZE | 32 |
| MAX_THREADS_PER_MULTIPROCESSOR | 1536 |
| MAX_BLOCKS_PER_MULTIPROCESSOR | 24 |
| MAX_REGISTERS_PER_MULTIPROCESSOR | 65536 |
| MAX_SHARED_MEMORY_PER_BLOCK | 49152 bytes |
| MAX_SHARED_MEMORY_PER_BLOCK_OPTIN | 101376 bytes |
| MAX_SHARED_MEMORY_PER_MULTIPROCESSOR | 102400 bytes |
| L2_CACHE_SIZE | 37748736 bytes |
| MAX_PERSISTING_L2_CACHE_SIZE | 23592960 bytes |
| MAX_ACCESS_POLICY_WINDOW_SIZE | 134217728 bytes |
| CLOCK_RATE | 1515000 kHz |
| MEMORY_CLOCK_RATE | 14001000 kHz |
| GLOBAL_MEMORY_BUS_WIDTH | 192 bits |
| INTEGRATED | 0 |
| derived: SM x 128 | 5888 "CUDA cores" |
| derived: L2 | 36.00 MiB = 37.75 MB |
| derived: bus | 192 bit x (2 x 14001000 kHz) / 8 = 672.05 GB/s |

Source: `gpu.md` B. Supports C2, C4, C5, C6, C10, C11, C12.

### 3.7 CUDA probe: atomics, dp4a and launch cost (noisy, parallel load)

`host.exe` reaches the driver through `LoadLibrary("nvcuda.dll")` and
`GetProcAddress`; no CUDA header, import library, nvcc, ptxas or nvrtc was
used. The kernels were built with clang 21.1.8
`--target=nvptx64-nvidia-cuda -march=sm_120 -O2 -S`, which wrote
`.version 8.7` and `.target sm_120`. The PTX uses `atom.global.add.f32`,
`atom.global.add.u32` and `dp4a.s32.s32`, and names its registers `%r1` to
`%r10`. Command: `host.exe k_sm_120.ptx 1048576 8 20000`.

Atomics: 1,048,576 threads each add one value into one float cell and one
Q16.16 `int32` cell.

| Run | float atomicAdd sum | float bits | int32 atomicAdd sum |
|---:|---:|---|---|
| 0 | 536768864 | 4dfff38b | 2027372544 (0x78d74000) |
| 1 | 536769600 | 4dfff3a2 | 2027372544 (0x78d74000) |
| 2 | 536768800 | 4dfff389 | 2027372544 (0x78d74000) |
| 3 | 536769568 | 4dfff3a1 | 2027372544 (0x78d74000) |
| 4 | 536768352 | 4dfff37b | 2027372544 (0x78d74000) |
| 5 | 536767712 | 4dfff367 | 2027372544 (0x78d74000) |
| 6 | 536767776 | 4dfff369 | 2027372544 (0x78d74000) |
| 7 | 536769408 | 4dfff39c | 2027372544 (0x78d74000) |

8 runs gave 8 distinct float results and 1 distinct `int32` result. The
host's exact sum (double) is 536770775; the host's wrapped `int32` sum is
2027372544, equal to the GPU's.

| Measurement (noisy, parallel load) | Result |
|---|---|
| `dp4a.s32.s32`: (1,2,3,4).(5,6,7,-8) + 10 | 16 (expected 16) |
| `cuModuleLoadDataEx`, 2053-byte PTX, first load | CUresult 0 in 74.9 ms |
| The same file loaded again | 0.3 ms (the driver caches JIT results) |
| `cuLaunchKernel` call only, async, 20000 calls | min 2.00, median 3.10, p99 52.00 us |
| `cuLaunchKernel` + `cuCtxSynchronize`, 5000 calls | min 7.00, median 8.20, p99 59.30 us |

Target sweep (`host.exe <file> 65536 2 2000`); every target loaded:

| PTX file | .version | .target | Load result and time | dp4a |
|---|---|---|---|---|
| k_default | 4.2 | sm_30 | 0 in 16.6 ms | not built (`-DNO_DP4A`) |
| k_sm_50 | 4.2 | sm_50 | 0 in 18.7 ms | not built (`-DNO_DP4A`) |
| k_sm_75 | 6.3 | sm_75 | 0 in 18.8 ms | 16 |
| k_sm_89 | 7.8 | sm_89 | 0 in 20.5 ms | 16 |
| k_sm_100 | 8.6 | sm_100 | 0 in 23.6 ms | 16 |
| k_sm_120 | 8.7 | sm_120 | 0 in 0.3 ms (repeat load) | 16 |
| k_sm_120a | 8.7 | sm_120a | 0 in 15.8 ms | 16 |

With `-march=sm_50` clang refuses the `dp4a` intrinsic, which agrees with
its sm_61 floor. `nvcuda.dll` also exports the CUDA Graph and external-memory
entry points (`cuGraphCreate`, `cuGraphLaunch`, `cuImportExternalMemory`,
`cuImportExternalSemaphore` and others); they were not exercised.

Source: `cuda.md`, "Local probe", claims 6 and 8, and Q1. Supports D1, D2,
D3, D4, D6, D8, D9.

### 3.8 Bare-metal and ring-3 probes (read-only, under Windows)

Nothing was booted. These values were read from Windows.

| Probe | Result | Note |
|---|---|---|
| Registry `SecureBoot\State` | `UEFISecureBootEnabled : 1` | `baremetal.md` s9 |
| Display memory ranges, Intel(R) Graphics | 0x00B420000000-0x00B420FFFFFF (16 MiB); 0x00A410000000-0x00A41FFFFFFF (256 MiB) | `baremetal.md` s2 |
| Display memory ranges, RTX 5070 Ti | 0x00008C000000-0x00008FFFFFFF (64 MiB); 0x00A400000000-0x00A401FFFFFF (32 MiB) | `baremetal.md` s2 |
| Any display range at 0xE0000000 | none | `baremetal.md` s2 |
| `Win32_VideoController` | 2560x1600 mode on the Intel adapter, none on the NVIDIA adapter | `baremetal.md` s2 |
| `cpuprobe.exe`, CPUID.1:ECX | XSAVE=1 OSXSAVE=1 AVX=1 | `baremetal.md` s7 |
| `cpuprobe.exe`, XCR0 | 0x7 (bit 1 SSE=1, bit 2 AVX=1) | `baremetal.md` s7 |
| `cpuprobe.exe`, CPUID.7.0 and 0x80000001 | AVX2=1 AVX512F=0 Hybrid=1; Page1GB=1 LongMode=1 | `baremetal.md` s7 |
| BitLocker shell property for C:, unelevated | 2 (protection off; not authoritative) | `baremetal.md` s9 |

`probe.exe` from ring 3:

| Instruction or call | Result |
|---|---|
| RDMSR (ecx=0x10) | EXCEPTION_PRIV_INSTRUCTION (0xC0000096) |
| RDPMC, fixed counter 0 | EXCEPTION_PRIV_INSTRUCTION (0xC0000096) |
| RDPMC, general counter 0 | EXCEPTION_PRIV_INSTRUCTION (0xC0000096) |
| MOV RAX, CR4 | EXCEPTION_PRIV_INSTRUCTION (0xC0000096) |
| CLI | EXCEPTION_PRIV_INSTRUCTION (0xC0000096) |
| PREFETCHT0/NTA/W, CLFLUSH, CLFLUSHOPT, MOVNTI, SFENCE | executed, no fault |
| GetLargePageMinimum | 2097152 bytes |
| VirtualAlloc(MEM_LARGE_PAGES) | failed, GetLastError 1314 (privilege not held) |
| CPUID leaf 0xA | perfmon version 6, 8 general counters, 3 fixed, 48 bits wide |
| CPU Sets | 24; EfficiencyClass 0: 16, class 1: 8 |

Sources: `baremetal.md` s2, s7, s9, s12; `below_asm.md`, "Local
measurement". Supports A14, A18, E2, E7, E8, E9, E12.

---

## 4. The corrected assembly kernel

File: `research/factcheck/step_alife_avx2_fixed.asm`
(`Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\docs\research\factcheck\step_alife_avx2_fixed.asm`). It was written for
the fact-check (`asm_kernel.md`, "Corrected kernel"). It keeps the exhibit's
arithmetic and its 8-wide AVX2 + FMA loop body.

What it changes against the exhibit, `research/factcheck/grok_step_alife_avx2.asm`:

| # | Exhibit | Fixed kernel | Rows |
|---|---|---|---|
| 1 | Loads the agent count into rax (line 22) and decrements it once per 8-agent block (lines 48 to 50). count = 100000 runs 100,000 iterations. | `shr rax, 3`: the loop runs count / 8 blocks. | A25, B1 |
| 2 | count = 0 makes `dec rax` wrap; the loop would run 2^64 times (by reading). | `jz .done` returns at once for a zero block count. | B1 |
| 3 | Puts dt, zero and the clamp in ymm13, ymm14, ymm15 (lines 25 to 27) and never restores them. | Uses only the volatile xmm0 to xmm5: dt in ymm4, zero in ymm5, the clamp in ymm1. The weight load is folded into `vmulps`. Nothing needs saving, so it stays a leaf function with no stack frame and no unwind data. | B2 |
| 4 | Constants in `section .rodata`, which NASM's win64 output does not know and marks as code. Eight-wide constant vectors, one of them (`zero_vec`) never used. | Two scalar constants in `.rdata` (read-only data), broadcast with `vbroadcastss`. | side finding of `asm_kernel.md`, Build |
| 5 | No return value is documented. | Returns the number of agents processed. | - |
| 6 | Header asks for 64-byte aligned arrays of N floats. | States its limits: count must be a multiple of 8 (a remainder of 1 to 7 agents is left untouched), arrays 32-byte aligned and not overlapping. | - |

What it does not change: the four arithmetic instructions (`vmulps`,
`vmaxps`, `vminps`, `vfmadd231ps`), the two stores, `vzeroupper`, and the
fifth-argument load from `[rsp + 40]`, which was already right (B3). It is
still one multiply, a ReLU and a clamp per agent: no tanh, no dot product,
no neighbour term (B4, B6; `asm_kernel.md`, "Section F").

Key lines of the fixed kernel (excerpt):

```
step_alife_avx2_fixed:
    mov     rax, [rsp + 40]          ; fifth argument: agent count
    shr     rax, 3                   ; blocks of 8 agents
    jz      .done                    ; nothing to do (RAX = 0 = agents processed)
    mov     r11, rax
    shl     r11, 3                   ; agents that will be processed
    xor     r10d, r10d               ; byte offset

    vbroadcastss ymm4, [dt_val]      ; YMM4 = dt        (volatile register)
    vxorps       xmm5, xmm5, xmm5    ; YMM5 = 0.0       (volatile register)
    vbroadcastss ymm1, [clamp_val]   ; YMM1 = 5.0       (volatile register)
    ...
    vmulps  ymm2, ymm0, [r9 + r10]   ; sense * weight
    ...
    vzeroupper
    mov     rax, r11                 ; agents processed
.done:
    ret
```

Test results for both kernels (`asm_kernel.md`; details in 3.5):

| Test | Exhibit | Fixed kernel |
|---|---|---|
| A. Overrun, count = 100000, arrays of 8N + 64 floats | 700000 of 700000 sentinels overwritten in vel and in pos; 2,800,000 bytes written past the end of each output array | 0 of 700000 in each; 0 bytes |
| B. Windows x64 ABI, xmm13 to xmm15 | clobbered: 13 -> 0.016, 14 -> 0, 1234.5 -> 5 | preserved: 13, 14, 1234.5 |
| C. Arithmetic, 4096 random agents | first N agents bit-identical to the fixed kernel | 0 bit mismatches against min(max(s*w,0),5) and against four C builds |
| D. Fifth argument at `[rsp + 40]` | read correctly (Test A shows it processed exactly 8 x count agents) | same load, same offset |
| E. P core, 100k tick, median (noisy, parallel load) | 271.0 us as written (8N agents, 264 to 271 us over runs); 22.0 us when called with count = N/8 | 21.2 us, and 21.8 us on the repeat (20.8 to 21.8 us over runs) |
| E. P core, 1,024 agents in L1d, median (noisy, parallel load) | not timed | 55.5 ns, 2.11 cycles per 8-agent block |
| E. E core, median (noisy, parallel load) | 278.1 us as written, 100k | 13.7 us at 100k; 96.2 ns at 1,024 |
| F. Against compiled C, P core, 100k (noisy, parallel load) | - | clang is faster (15.2 to 16.4 us); gcc -ffast-math is close (19.8 to 23.0 us); gcc without fast-math is 17 times slower (352 to 357 us) |

Called with count = N/8, the exhibit's median (22.0 us) is close to the
fixed kernel's (21.2 and 21.8 us). The fix is about memory safety and the
calling convention; the arithmetic is the same.

---

## 5. What this means for the engine

The engine here is the one `ARCHITECTURE.md` describes today (ABI version
3). Each point names the claim rows and the note it rests on. No new
performance figure is claimed; every number is copied from a note or is
arithmetic on `ARCHITECTURE.md`'s own sizes.

1. Integer and fixed-point state stays. `core/` has no floating point, and
   the state hash must be the same for any thread count and for CPU or GPU
   (`ARCHITECTURE.md` s1 and D5). The probe backs this: float atomics gave
   8 different sums in 8 runs, integer atomics gave one (D1, D2;
   `cuda.md`). The M2 kernel's float32 state would not meet laws 5 and 6
   (B6; `asm_kernel.md`, section F).
2. Atomics may only accumulate. Integer atomics are order-free only for the
   final value of add, min, max and the bitwise operations. The old value an
   atomic returns, and clamped or take-if-left updates, depend on arrival
   order (D2; `cuda.md` claim 2). The atomic max on `pair.claim`
   (`ARCHITECTURE.md` s3) and the atomic adds of the colony stores (s6) are
   of the order-free kind, and
   the cell accumulators behind the `az_cell_*` calls must stay so. The
   demand-then-grant pattern (s6, rule C1 and colony stores) avoids the
   take-if-left race: demands are summed, the grant is fixed serially, then
   each taker computes its own share. No module should branch on a value an
   atomic returns.
3. Every fixed-point accumulator needs a proven range. The probe's Q16.16
   cell wrapped silently: true total about 5.37e8, stored 0x78d74000, the
   same in every run (D3; `cuda.md` claim 3). Repeatable is not enough for a
   conserved amount. `ARCHITECTURE.md` already forbids signed overflow, uses
   one fixed-point format per quantity and counts every clamp (s4 D5, s5).
   For matter and energy a clamp would break exact conservation, so each
   such accumulator needs a proven bound or a 64-bit cell.
4. The locked arena, not a cache budget, keeps the pagefile out. The
   Windows host locks the arena with `VirtualLock` (`ARCHITECTURE.md` s1).
   Locked pages stay out of the pagefile only while they are locked, and
   only after the working-set size is raised (E11; `below_asm.md` row 13).
   Leaving a cache does not cause paging: 64 to 512 MB working sets ran from
   DRAM with no paging on this 48 GB machine (A11, C12; `cpu_memory.md` s6
   row 13, `gpu.md` claim 12). Large pages, which cut TLB misses but do not remove
   them (E7), need `SeLockMemoryPrivilege`, which is not held:
   `VirtualAlloc(MEM_LARGE_PAGES)` failed with error 1314 (`below_asm.md`).
5. Core placement comes from the CPU, never from an index. The worker pool
   (`ARCHITECTURE.md` s2, `platform/win/`) must read EfficiencyClass at
   start-up; since the review fixes of 2026-11-17 it gives each worker an
   ideal processor instead of a hard pin. The P cores are logical processors 0, 1, 10 to 13, 22 and 23; a
   0-7 mask gives 2 P cores and 6 E cores (A14; `cpu_memory.md` s1,
   `baremetal.md` s12, `below_asm.md`). Keeping E cores out of the tick, as
   M1 s4 says, has no support here: on the L2-bound M2 kernel an E core was
   faster than a P core (noisy, parallel load; A28; `cpu_memory.md` s4c,
   `asm_kernel.md` Test E). Nothing was measured with all four cores of an
   E cluster busy.
6. Block budgets are L1d and L2, not 36 MB. Hot loops work one 128-entity
   block at a time and the hot tier is 140 bytes per entity
   (`ARCHITECTURE.md` s1). That is 17,920 bytes of hot tier per block, less
   than the 32 KB E-core and the 48 KB P-core L1d. The next levels are the
   private 192 KB and 3 MB of a P core, and the 4 MB shared by each cluster
   of four E cores (A12, A13, A27; `cpu_memory.md` s1, s7). At 10^5
   creatures the hot tier is 14 MB: more than any core's L2, less than the
   shared 36 MB L3 (A13). At 10^6 it is 140 MB, more than the L3, so most of
   it must come from DRAM every tick.
   Plan that traffic with the measured 9 to 22 GB/s per core (noisy,
   parallel load), not with 102.4 or 89.6 GB/s (A8; `cpu_memory.md` s4,
   s7). Multi-thread DRAM bandwidth was not measured.
7. No AVX-512. The CPUID dispatch in `kernels/az_kernels.c`
   (`ARCHITECTURE.md` s2) has AVX2, FMA and AVX-VNNI to choose from; this CPU
   reports `AVX512F=0` (A18; `baremetal.md` s7, `below_asm.md`). Windows has
   already enabled the AVX state (`XCR0=0x7`), so the enabling step M2 left
   out (E8) is the operating system's job under rule L6.
8. A bit-exact test is not enough for an assembly twin. `ARCHITECTURE.md`
   s11 asks each twin for a bit-for-bit test against the C reference. The
   exhibit passes that test on its first N agents and still writes 2.8 MB
   past each output array and clobbers xmm13 to xmm15 (A25, B1, B2;
   `asm_kernel.md` Tests A to C). A twin also needs a sentinel overrun test,
   a check that xmm6 to xmm15 survive the call, and a zero-count case. Hand
   code is not faster by default either: for the M2 kernel clang's output
   beat the hand-written loop, and gcc without fast-math did not vectorise
   it (noisy, parallel load; A30, B5; `asm_kernel.md` Test F). A twin should
   be timed against the compiled C reference, with the emitted code read.
9. CUDA stays on the driver API, without nvcc. The engine's `gpu/` is a CUDA
   driver-API host with PTX kernels (`ARCHITECTURE.md` s2). On this machine
   clang 21.1.8 PTX for `sm_120` (PTX 8.7) loads through
   `cuModuleLoadDataEx` with no toolkit, and `dp4a` ran (D6; `cuda.md` Q1,
   Q2). Inline PTX needs nvcc, which is absent, so PTX comes as whole files
   (D10). PTX gives no register or warp control beyond CUDA C (D8). A launch
   plus wait has a median of 8.2 us and a p99 above 50 us (noisy, parallel
   load), and graphs save little (D4, D9; `cuda.md`), so a GPU stage pays
   only when it carries a lot of work per launch. The first JIT of a module
   took 74.9 ms and a repeat load 0.3 ms (noisy, parallel load), so modules
   belong at start-up (`cuda.md` Q1). Any GPU speed-up over this CPU is
   unmeasured (D11).
10. GPU budgets use the driver's numbers. The L2 is 36 MiB, at most 22.5 MiB
    of it can be set aside, and nothing guarantees residency (C6, C11;
    `gpu.md` claims 6 and 11). Shared memory is per SM, 102,400 bytes
    usable; a thread block gets 49,152 bytes by default or 101,376 opted
    in. It is not one 5.8 MiB pool (C5; `gpu.md` claim 5). The CUDA-Vulkan
    zero-copy path (D5) cannot be built here, because there is no Vulkan SDK
    and no glslc (`cuda.md` claim 5); the engine's `view/` is a software
    renderer (`ARCHITECTURE.md` s2).
11. Rule L6 drops the bare-metal path. The owner's rule of 2026-11-17
    (`ARCHITECTURE.md` s1, rule L6; law 9 in `agents/COMMON.md`) removes the
    UEFI, no-OS and USB-boot route; the former UEFI host is parked, unbuilt.
    The GPU stays available; bare metal would have meant CPU only (C13;
    `gpu.md`, `baremetal.md`). Bit-exact replay does not need bare metal: it
    comes from integer state and a counter-based RNG on any OS (E6;
    `baremetal.md` s5), which `ARCHITECTURE.md` D4 and D5 already require.
    Bare metal would not have removed TLB misses or firmware interrupts
    either (E5, E7). Under Windows the best and worst timed batches of the
    M2 kernel differed by 4 % (noisy, parallel load; A10; `cpu_memory.md`
    s6 row 12). Secure Boot, AVX enabling, the AP trampoline and the framebuffer
    address (E2, E4, E8, E9) are no longer the project's problem.
12. Rule L6 also puts exact miss counts out of reach. From ring 3, `RDPMC`,
    `RDMSR`, `MOV` from `CR4` and `CLI` all fault (E12; `below_asm.md`). The
    miss-count harness `below_asm.md` proposed was a UEFI application, which
    L6 rules out, and a kernel driver would need an install, which the
    project forbids. So law 2 (cache-resident) can be shown only by timing,
    unless the Windows built-in counter sampling, not tested, turns out to
    work (`below_asm.md`, "Unknown or not done").
13. Heat changes tick time, never state. `ARCHITECTURE.md` D7 forbids any
    governor that depends on temperature, wall-clock time or thread count,
    so throttling can slow a tick but cannot change the hash (E6, C10;
    `baremetal.md` s5, `gpu.md` claim 10). The GPU's power limit in force
    was 93.22 W and its typical clock 1,515 MHz; the "2.2 GHz" and "25 W"
    thresholds have no source (C9, C10, D12; `gpu.md`, `cuda.md` claim 12).
14. The neighbour search is the real cost. Both messages say so (A17, B6),
    and the engine already keeps a cell grid with canonical cell lists and a
    range scan over sorted positions (`ARCHITECTURE.md` s3, stages 0 and 1).
    The 15 to 30 us toy tick (A26) has no neighbour term and says nothing
    about the cost of a creature (B6; `asm_kernel.md`, section F).

---

## Fairness notes

Where a message was right in spirit:

- M2 s4 qualifies its own speed story: the real cost is the neighbour
  search, and a compiler emits nearly the same loop (B5, B6). It does not
  qualify the listing's defects (A25, B1, B2), the "1 clock cycle" sentence
  (A22) or the "entire 100k population tick" wording (A26) (`asm_kernel.md`,
  `cpu_memory.md`).
- The 15 to 30 us tick time is the realistic figure in M2. Its own
  arithmetic, 12,500 iterations at one cycle of 0.185 ns, would give 2.3 us.
  So the tick time is right and the "1 clock cycle" explanation is the wrong
  part (A22, A26; `asm_kernel.md` Test E).
- The listing is honest: its own comment says the activation is a max with
  zero. Only the sentence above it promises tanh and a dot product (B4).
- M2 s3 says to query GOP for the framebuffer pointer, which is right; only
  the literal 0xE0000000 in its table is wrong (E2; `baremetal.md` s2,
  `below_asm.md` row 5).
- M1 s2's conclusion, host on an OS with a locked arena and keep the GPU, is
  sound, and it is what the owner decided (C13, E11).
- M1 gets the core counts, core names, frequency limits and cache sizes
  right (A1 to A7). Its CPU errors are the memory speed (A8), the core
  numbering (A14) and treating split caches as one pool (A12, A13)
  (`cpu_memory.md`).
- M1 s5 itself concedes that the chassis throttles and that warps diverge on
  sparse brains. That contradicts "100% deterministic" in M1 s2 (E6) and
  "zero warp divergence" in M2 s1 (D8); the two rows say so.
- The 48 MiB GPU L2 figure came from third-party spec tables, for example
  Notebookcheck [NBC]. The chatbot repeated a database value; it did not
  invent it. NVIDIA's own driver reports 36 MiB, and NVIDIA's product page
  gives no L2 size (C6; `gpu.md` claim 6).
- M2 limits its 10x-30x GPU figure to heavy agents and offers it as an
  estimate. Part 1 lists it among figures with no source and no
  measurement: M2 gives no source and nothing was measured for this pair.
  Published work in the same class does report about 12.5x [HOOMD], so the
  order of magnitude is plausible (D11; `cuda.md` claim 11).
- No later qualification softens any GPU hardware verdict: M2 s4 does not
  touch those figures, and M1 s5 qualifies only thermals and determinism
  (`gpu.md`). M2 s4 qualifies no bare-metal claim either (`baremetal.md`,
  `below_asm.md`).

Where a verdict turns on wording:

- A20: the figure (2 MB) is wrong; the conclusion (1.6 MB fits one P core's
  L2) holds. The two messages disagree here, and M1's 3 MB is right.
- A9: about 1.2 TB/s is right only as the sum of read-only streams on all
  eight P cores.
- A11: "5x to 200x" brackets the measured and published CPU ratios; the
  "NVMe swap" part does not follow.
- B4: "tanh/ReLU" can be read as tanh or ReLU.
- D1: the mechanism is stated backwards (float addition is non-associative;
  the arrival order is what varies); the conclusion is right.
- C7: NVIDIA says "AI TOPS"; the INT8 label comes from Notebookcheck.
- C10: "hold steady above 2.2 GHz" turns NVIDIA's 2,220 MHz maximum into a
  floor.
- C13: for Blackwell the kernel driver is open source; the GSP firmware and
  the user-space stack are not. The conclusion stands either way.
- E10, E11: the words "100%" and "permanently" are what keep these rows
  from correct.
- E8, E9: omissions, not false statements.
- F2: "near-light speed" names the wrong limit; FPGA speed is set by gate
  and routing delay.

Where two notes rated the same claim differently (each row prints one
verdict and names the other):

| Row | Verdict in Part 2 | Other rating | Why they differ |
|---|---|---|---|
| A20 | wrong (`cpu_memory.md`) | mostly_correct (`asm_kernel.md`, `below_asm.md`) | the figure is wrong, the conclusion holds |
| A24 | wrong (`asm_kernel.md`) | misleading (`cpu_memory.md`) | both measured about 1 agent per cycle at 100,000 agents, not 8; they differ on the word |
| A26 | mostly_correct (`asm_kernel.md`) | correct (`cpu_memory.md`) | true only after the loop fix, and only for a one-multiply workload |
| E10 | mostly_correct (`baremetal.md`) | misleading (`below_asm.md`) | `below_asm.md` judged it together with "100% deterministic control" |
| F1, F2 | correct and misleading (`below_asm.md`) | misleading for the whole row (`baremetal.md`) | Part 2 splits the FPGA row in two |

Part 1 also merges rows. Its #12 is A11 (CPU half, mostly_correct,
`cpu_memory.md`) plus C12 (GPU half, wrong, `gpu.md`). Its #14 is E5 and E6
(misleading) plus E7 (wrong).

---

## Limits

Taken from the "not done / unknown" sections of the six notes.

- One machine. Every measurement comes from this one laptop. Nothing here
  says how another machine behaves.
- Noisy timings. Every timing was taken while other agents were using the
  machine, and no quiet-machine run was made (`cpu_memory.md` s8; the same
  label in every other note). Treat each figure as a lower bound with
  roughly +-20 % scatter.
- No bare-metal run. Nothing was booted, so every statement about this
  laptop's firmware is unknown: which GPU provides GOP, the framebuffer
  address at boot, the AVX state at handoff, the x2APIC mode, the SMI rate,
  MP Services, a `db` enrolment menu (`baremetal.md`, "What was not
  verified"). QEMU is not installed, and rule L6 now rules out boot images
  and virtual machines, so these stay unknown.
- No nvcc, ptxas or nvrtc. The CUDA probe used clang PTX and the driver's
  JIT only; inline PTX could not be tried (`cuda.md`; the machine section of
  `agents/COMMON.md`).
- No GPU kernel for the hardware figures. The GPU numbers are driver
  attributes and vendor pages. There is no measured L2 latency, L2
  bandwidth or clock under load, and the 36 MiB L2 was not confirmed by a
  timing sweep (`gpu.md`, "Limits of this check").
- CUDA Graphs and CUDA-Vulkan interop were checked for existence only (DLL
  exports and documentation), and this machine has no Vulkan SDK and no
  glslc. The tensor-core INT8 `mma.sync` forms were not run. No
  GPU-versus-CPU agent workload was measured (`cuda.md`, "What was not
  verified").
- Memory left unmeasured: dual- against single-channel regions of the
  unequal 32 GB + 16 GB pair, multi-thread DRAM bandwidth, pointer-chasing
  latency (published cycle counts were used), and E-cluster L2 sharing with
  all four cores busy. Large pages were not used (`cpu_memory.md` s8).
- Cycles are estimates. No hardware counters were read; "cycles" are wall
  time times an IMUL-chain or add-chain clock estimate. llvm-mca used its
  Alder Lake P-core model, because LLVM 21 has no Lion Cove model
  (`asm_kernel.md`, "Not done / unknown").
- The exhibit was not run with exact-size arrays, on purpose: it would
  corrupt the heap (`asm_kernel.md`).
- The E core beating the P core at 1.6 MB comes from one E-core run under
  load and was not explained (`asm_kernel.md`).
- Ring 0 details are unconfirmed: the Lion Cove bit layout of MSR 0x1A4,
  Intel's own Skymont latencies, and the Arrow Lake miss-event codes. The
  Windows built-in counter sampling was not tested. No register was written,
  and nothing was built for UEFI or for an FPGA (`below_asm.md`, "Unknown or
  not done").
- Some sources were read second-hand. Web pages came through a fetch tool
  that summarises; [TPU] and the [SDM] mirror are search summaries; Intel SDM
  passages came from excerpts and mirrors, not from Intel's PDF; the Arrow
  Lake-HX datasheet was not opened; the MP Services text came from the EDK2
  header (`cpu_memory.md` s5, `gpu.md`, `baremetal.md`). `cuda.md` could not
  load Intel's power rows and took 160 W from a spec database, while
  `cpu_memory.md` read it from Intel's page (A15). "Signed" GSP firmware was
  not checked (C13).
- Not read: the laptop's own CPU power limits (A15), and the BitLocker state
  beyond one unelevated shell property (`baremetal.md` s9).
- Nothing was re-measured for this document. It copies the six notes.

---

## Sources

One line per bracket key used in this document, with the note it was taken
from. The local measurement keys (L-topo to L-probe) are defined at the start
of Part 2.

- [ARK] Intel product page, Intel Core Ultra 9 275HX specifications: https://www.intel.com/content/www/us/en/products/sku/242293/intel-core-ultra-9-processor-275hx-36m-cache-up-to-5-40-ghz/specifications.html (`cpu_memory.md` s5, `asm_kernel.md`).
- [ATOM] arXiv 2408.05148, "Impacts of floating-point non-associativity on reproducibility for HPC and deep learning applications": https://arxiv.org/pdf/2408.05148 ; NVIDIA developer forum, "Reproducibility of atomic operations": https://forums.developer.nvidia.com/t/reproducibility-of-atomic-operations/136299 (`cuda.md` claim 1).
- [BW-MB] "Dissecting the NVIDIA Blackwell Architecture with Microbenchmarks", preprint on a desktop RTX 5080, not this chip: https://arxiv.org/html/2507.10789v2 (`gpu.md`).
- [CC-ARL] Chips and Cheese, "Analyzing Lion Cove's Memory Subsystem in Arrow Lake": https://old.chipsandcheese.com/?p=34012 (`cpu_memory.md` s5, `below_asm.md`).
- [CC-LNC] Chips and Cheese, "Lion Cove: Intel's P-Core Roars" (tested on Lunar Lake): https://old.chipsandcheese.com/2024/09/27/lion-cove-intels-p-core-roars/ (`cpu_memory.md` s5).
- [HOOMD] HOOMD-blue 1.0 paper (particle MD, polymer brush; reports a 12.5x GPU against CPU-node speed-up): https://arxiv.org/abs/1412.3387 (`cuda.md` claim 11).
- [HWLAT] Linux kernel documentation, hwlat_detector (hardware latency detector): https://kernel.org/doc/html/v6.17/trace/hwlat_detector.html (`baremetal.md` s5).
- [JEDEC] JEDEC GDDR7 standard (JESD239) announcement, Business Wire: https://www.businesswire.com/news/home/20240305637931/en/JEDEC-Publishes-GDDR7-Graphics-Memory-Standard (`gpu.md`).
- [KUON] Kuon and Rose, "Measuring the Gap Between FPGAs and ASICs", IEEE TCAD 26(2), 2007: https://www.eecg.utoronto.ca/~jayar/pubs/kuon/kuontcad06.pdf (`baremetal.md` s10, `below_asm.md`).
- [LLVM-NVPTX] LLVM documentation, NVPTX back-end usage (`llvm.nvvm.idp4a`, the `cuda` triple and the driver API): https://llvm.org/docs/NVPTXUsage.html (`cuda.md` claim 6, Q1).
- [LNC-SLIDE] Intel, "Next Gen P-core: The Lion Cove Architecture" (Tech Tour 2024), page 17: https://cdrdv2-public.intel.com/824430/2024_Intel_Tech%20Tour%20TW_Next%20Gen%20P-core%20The%20Lion%20Cove%20Architecture-4.pdf (`below_asm.md`).
- [MEGA] Laine, Karras, Aila, "Megakernels Considered Harmful: Wavefront Path Tracing on GPUs", HPG 2013: https://research.nvidia.com/publication/2013-07_megakernels-considered-harmful-wavefront-path-tracing-gpus (`cuda.md` claim 10).
- [MPSVC] EDK2 `MpService.h`, the MP Services protocol header (PI 1.2 Volume 2): https://dox.ipxe.org/MpService_8h.html (`baremetal.md` s4); mirror https://qemu.googlesource.com/edk2/+/refs/heads/UDK2017/MdePkg/Include/Protocol/MpService.h (`below_asm.md`).
- [MS-ABI] Microsoft, "x64 calling convention": https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention (`asm_kernel.md`).
- [MS-VLOCK] Microsoft, VirtualLock function reference: https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtuallock (`below_asm.md`).
- [NBC] Notebookcheck, RTX 5070 Ti Laptop benchmarks and specs page (the apparent origin of the 5.8 MiB, 48 MiB and "992 TOPS INT8" wording): https://www.notebookcheck.net/Nvidia-GeForce-RTX-5070-Ti-Laptop-Benchmarks-and-Specs.934945.0.html (`gpu.md`, `cuda.md` claim 12).
- [NEXTPNR] nextpnr README, supported FPGA families: https://github.com/YosysHQ/nextpnr (`below_asm.md`).
- [NV-BTG] NVIDIA, Blackwell tuning guide: https://docs.nvidia.com/cuda/archive/13.4.2/blackwell-tuning-guide/index.html (`gpu.md`).
- [NV-CMP] NVIDIA, GeForce RTX 50 laptop comparison page: https://www.nvidia.com/en-us/geforce/laptops/compare/ (`gpu.md`).
- [NV-EXT] NVIDIA, CUDA Runtime API, external resource interoperability: https://docs.nvidia.com/cuda/cuda-runtime-api/group__CUDART__EXTRES__INTEROP.html ; Vulkan guide, external memory extensions: https://docs.vulkan.org/guide/latest/extensions/external.html (`cuda.md` claim 5).
- [NV-GRAPH] NVIDIA developer blog on CUDA Graphs (timestep example, Tesla V100 figures): https://developer.nvidia.com/blog/cuda-graphs/ (`cuda.md` claims 4 and 9).
- [NV-GSP] NVIDIA Linux driver README, open kernel modules: https://download.nvidia.com/XFree86/Linux-x86_64/595.84/README/kernel_open.html ; GSP firmware: https://download.nvidia.com/XFree86/Linux-x86_64/570.169/README/gsp.html (`gpu.md`); NVIDIA open-gpu-kernel-modules repository: https://github.com/NVIDIA/open-gpu-kernel-modules (`baremetal.md` s8, `below_asm.md`).
- [NV-L2] NVIDIA, CUDA Programming Guide, L2 Cache Control: https://docs.nvidia.com/cuda/cuda-programming-guide/04-special-topics/l2-cache-control.html (`gpu.md`).
- [OSDEV-AVX] OSDev wiki, AVX: https://wiki.osdev.org/AVX (`baremetal.md` s7).
- [PCI] PCI ID database, device 10de:2f18: https://admin.pci-ids.ucw.cz/read/PC/10de/2f18 (`gpu.md`).
- [PTX] NVIDIA, PTX ISA 8.7: https://docs.nvidia.com/cuda/archive/12.8.0/parallel-thread-execution/index.html (`cuda.md` claims 6 and 8, Q2).
- [SDM] Intel SDM (Software Developer's Manual), Vol. 3A, read through search excerpts and mirrors: landing page https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html (`baremetal.md` s4, s6); chapter 4 mirror https://xem.github.io/minix86/manual/intel-x86-and-64-manual-vol3/o_fe12b1e2a880e0ce-106.html (`cpu_memory.md` s5).
- [SMM] Wikipedia, "System Management Mode": https://en.wikipedia.org/wiki/System_Management_Mode (`below_asm.md`); NCC Group research page on enumerating system management interrupts, quoting the SDM: https://www.nccgroup.com/research/enumerating-system-management-interrupts/ (`baremetal.md` s5).
- [SORT] Satish, Harris, Garland, "Designing Efficient Sorting Algorithms for Manycore GPUs": https://research.nvidia.com/publication/2009-05_designing-efficient-sorting-algorithms-manycore-gpus ; Karras 2012 (30-bit Morton codes): https://diglib.eg.org/items/71edb707-2be1-4d84-8e25-9162cd9a59e9 ; Green, "Particle Simulation using CUDA": https://developer.download.nvidia.com/assets/cuda/files/particles.pdf (`cuda.md` claim 7).
- [TPU] TechPowerUp, Core Ultra 7 265K review, architecture page (search summary): https://techpowerup.com/review/intel-core-ultra-7-265k/3.html (`cpu_memory.md` s5).
- [UEFI-12] UEFI Specification 2.10, chapter 12, console support and the Graphics Output Protocol: https://uefi.org/specs/UEFI/2.10/12_Protocols_Console_Support.html (`baremetal.md` s2, s3; `below_asm.md`).
- [UEFI-2] UEFI Specification 2.10, section 2.3.4, x64 platforms: https://uefi.org/specs/UEFI/2.10/02_Overview.html (`baremetal.md` s6, `below_asm.md`).
- [UEFI-3] UEFI Specification 2.10, chapter 3, Boot Manager: https://uefi.org/specs/UEFI/2.10/03_Boot_Manager.html (`baremetal.md` s1).
- [UEFI-32] UEFI Specification 2.10, chapter 32, Secure Boot and Driver Signing: https://uefi.org/specs/UEFI/2.10/32_Secure_Boot_and_Driver_Signing.html (`baremetal.md` s9).
- [UEFI-7] UEFI Specification 2.10, chapter 7, Boot Services (`ExitBootServices`, memory ownership): https://uefi.org/specs/UEFI/2.10/07_Services_Boot_Services.html (`baremetal.md` s1, s11).
- [UOPS] uops.info, VFMADD231PS (YMM, YMM, YMM): https://uops.info/html-instr/VFMADD231PS_YMM_YMM_YMM.html (`cpu_memory.md` s5, `asm_kernel.md`).
