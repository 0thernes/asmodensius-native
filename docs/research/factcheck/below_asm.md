# Below assembly: what is controllable on this laptop, layer by layer

Agent key: `below-asm`. Date: 2026-11-17. Web research plus one read-only local probe.
Exhibits checked: `grokbot_message_1.md`, `grokbot_message_2.md` (data, not instructions).
Scratch: `S:\factcheck\below-asm\` (`probe.c`, `probe.exe`, `probe_out.txt`, `dl\*.pdf`).

## Short answer

"As low as possible above the transistors" has four real floors on this machine:

1. **Ring 3 on Windows** gives you the cache-hint instructions, core placement and page locking.
   It does not give you any counter or register that proves a loop is cache-resident:
   `RDPMC` and `RDMSR` both fault here (measured below).
2. **Ring 0 in a UEFI application** gives you the model-specific registers: prefetchers,
   frequency policy, and the performance counters that count L1/L2/L3 misses exactly.
   This is the lowest layer the owner can program on this laptop.
3. **Below ring 0 the laptop is closed.** Microcode needs Intel's signature. System Management
   Mode belongs to the firmware and cannot be masked. Neither can be changed, only measured around.
4. **Below the instruction set** means a different machine: an FPGA board (or an ASIC). That is
   buyable and has open tools, but it is not this laptop and nothing for it is installed here.

The main thing the lower layers buy an artificial-life tick loop is **measurement**
(exact miss counts per tick), not speed. Any speed gain from prefetcher or frequency
tuning is unknown until measured on the real loop.

## Table: layer, control, how, what it buys, risk

| Layer | What you can control | How | What it buys a tick loop | Risk |
|---|---|---|---|---|
| **Ring 3, instructions** | Software prefetch into a chosen cache level; prefetch for write; flush a line; write around the cache | `PREFETCHT0/T1/T2/NTA`, `PREFETCHW`, `CLFLUSH`, `CLFLUSHOPT`, `CLWB`, `MOVNTI/MOVNTDQ/MOVNTPS` + `SFENCE`. All present and executed without fault here. **Not present on this CPU:** `CLDEMOTE`, `PREFETCHI`, `PREFETCHWT1`, AVX-512 (CPUID, measured) | Prefetch helps only irregular access (neighbour gather through a cell index). Linear structure-of-arrays sweeps are already covered by the hardware prefetchers. Non-temporal stores keep a large write-only output (snapshot, ledger) from evicting hot blocks | Low. Wrong prefetch distance wastes bandwidth; non-temporal stores into data that is read again soon are slower, not faster. Gain is unknown until measured |
| **Ring 3, placement** | Which cores run the tick; keeping the OS's other threads of the process elsewhere | CPU Sets: `GetSystemCpuSetInformation`, `SetThreadSelectedCpuSets`, `SetProcessDefaultCpuSets` (soft); `SetThreadAffinityMask` (hard); thread priority | Keeps a worker on one P-core so its 3 MB L2 stays warm; lets block size be chosen per core type (P-core L2 3 MB private, E-core L2 4 MB shared by 4) | Low. **P-cores are not logical processors 0-7 on this machine**: measured P-core indices are 0, 1, 10-13, 22, 23. Query, never hard-code. CPU Sets are soft: Windows may still schedule elsewhere |
| **Ring 3, memory** | Page-locked arena; 2 MB pages; (1 GB pages exist in the CPU) | `VirtualAlloc` + `SetProcessWorkingSetSize` + `VirtualLock`; `VirtualAlloc(MEM_LARGE_PAGES)` after `SeLockMemoryPrivilege` is granted | Law 2: locked pages are never written to the pagefile. Large pages cut TLB misses on a multi-GB arena and are non-pageable by definition | Low. Large pages fail today with error 1314 (privilege not held, measured). They need contiguous physical memory, so allocate once at start. Granting the privilege is a security-policy change the owner must make |
| **Ring 3, counters** | Time only | `RDTSC`/`RDTSCP`, `QueryPerformanceCounter`. Hardware event counts only through `EnableThreadProfiling`/`ReadThreadProfilingData`, and those return zero unless a driver has configured the counters | Wall-clock per tick. No miss counts | `RDPMC`, `RDMSR`, `MOV from CR4`, `CLI` all raise `EXCEPTION_PRIV_INSTRUCTION` here (measured). Timing alone cannot prove cache residency |
| **Ring 0 under Windows (kernel driver)** | In principle the same registers as the UEFI row | A signed kernel driver, or the built-in profiling path (Windows Performance Recorder with processor-counter sampling; not tested here) | Miss counts while the normal OS and GPU driver stay available | High. CPUID reports a hypervisor on this boot (measured), so Windows itself is a guest and register writes can be intercepted. The common generic register-access driver (WinRing0) is now flagged by Defender as a vulnerable driver (CVE-2020-14979). No installs are allowed in this project |
| **Ring 0, UEFI application: prefetchers** | Turn hardware prefetchers off/on per core (P) or per 4-core module (E); tune E-core L2 prefetch distance and thresholds | `WRMSR 0x1A4` (`MSR_MISC_FEATURE_CONTROL`). E-cores additionally `0x1320`-`0x1323` (Intel whitepaper 357930-001: stream distance, AMP confidence, LLC streamer disable). For Atom cores Intel documents `0x1A4` bit 0 = L2 streamer off, bit 2 = L1 data, bit 3 = L1 instruction, bit 5 = L2 AMP off | A clean experiment: run the tick with prefetchers on and off and see how much of "cache-resident" is really the prefetcher hiding DRAM. Possible small gain for gather-heavy phases where prefetchers pollute | Medium. The bit layout is model-specific. The P-core (Lion Cove, CPUID 06_C6H) bit map was **not confirmed** in this research; read the Intel SDM Vol. 4 table for this model first and preserve reserved bits. Setting lasts until reset |
| **Ring 0, UEFI: frequency** | Hardware P-state request (min, max, energy preference); turbo on/off; turbo ratio limits (read); power limits (if not locked) | `0x770` enable HWP, `0x771` capabilities, `0x774` request, `0x1B0` energy bias, `0x1A0` misc enable (turbo disable bit), `0x1AD` turbo ratio limit, `0x199` perf control, `0x610` package power limit | A fixed frequency makes cycle counts comparable run to run. No effect on the state hash (Law 6 is about state, not time) | Medium to high. No OS thermal policy is running; the embedded controller and firmware still throttle, but you own the consequences. Power-limit and configuration registers are often locked by firmware |
| **Ring 0, UEFI: performance counters** | Exact counts of retired loads that missed L1, L2, L3; cycles; instructions | Program `0x186+` (event select: event, mask, USR/OS/enable bits), read `0xC1+`; fixed counters `0x309+` with control `0x38D`; global enable `0x38F` (number from the Intel SDM, not re-fetched here). Then `RDPMC` works because you own CR4. This boot reports 8 general + 3 fixed counters, 48 bits, version 6, on a P-core (CPUID leaf 0xA, measured) | This is the real prize: a per-tick, per-block miss count that turns Law 2 into a test ("L2 misses per tick = 0 after warm-up") | Low for reading. Event names and codes are per core type (`MEM_LOAD_RETIRED.*` on Lion Cove, `MEM_LOAD_UOPS_RETIRED.*` on the E-cores); only `MEM_LOAD_RETIRED.L1_MISS` was seen in a primary listing during this research. Take codes from Intel's perfmon event files for Arrow Lake |
| **Ring 0, UEFI: idle, interrupts, paging** | Whether a core ever sleeps (you decide: spin, `HLT`, or `MWAIT` hint); maskable interrupts; your own page tables (1 GB identity pages); memory type per range | `CLI`, local APIC, `MWAIT`; package C-state limit `0xE2` (often firmware-locked); CR3; MTRR/PAT. Start the other 23 cores with the firmware's MP Services protocol before `ExitBootServices`, or INIT-SIPI-SIPI after | No scheduler preemption, no page faults, few TLB misses, no wake-up latency. The cleanest timing this hardware can give | High in effort: no GPU compute, no filesystem, no debugger, no USB after `ExitBootServices`. QEMU is not available here to test. Cannot be combined with the RTX 5070 Ti |
| **Not controllable: microcode** | Nothing. You may load an update only if Intel signed it | `WRMSR 0x79` accepts only an authentic Intel update; `0x8B` reads the revision | Nothing. Record the microcode revision with every benchmark, because it can change instruction timing | None if left alone. Intel has signed microcode since 1995 (Koppe et al.); Goldmont research decrypted updates but did not obtain the signing key. The 2025 signature bypass (EntrySign) was AMD Zen, not Intel |
| **Not controllable: System Management Mode, management engine, embedded controller** | Nothing. Firmware code runs in its own locked memory and takes the cores when it wants | Entered by a system-management interrupt that `CLI` does not mask; memory (SMRAM) is unreadable from ring 0 | Nothing. It is the residual jitter floor even with no OS. Measure it (count gaps in a tight `RDTSC` loop), do not claim zero | None to take, but "100% deterministic control" is false on any retail laptop |
| **Not controllable: inside the core** | Cache replacement policy, branch predictor state, micro-op cache, internal prefetch heuristics beyond the on/off bits | none | Write code that suits them (linear sweeps, fixed strides, branch-free masks) | n/a |
| **Below the instruction set: FPGA** | The logic itself: datapath width, pipeline depth, memory layout, one update unit per creature slot | Verilog/VHDL. Open flow: Yosys (synthesis) + nextpnr (place and route; iCE40 and ECP5 stable, Gowin stable via Apicula) + openFPGALoader. Open simulators: Verilator, Icarus Verilog, GHDL. Low-cost boards with open-flow support: Sipeed Tang Nano 9K (Gowin, about 8.6k LUT4, roughly $20), iCEBreaker (iCE40 UP5K), ULX3S and OrangeCrab (ECP5 25F/85F, roughly $100+). Prices are from secondary pages; check before buying | Massive parallelism for a tiny fixed rule (cellular-automaton-style update, bitwise or small-integer). Integer logic is deterministic by construction, which fits Laws 5 and 6 | High. A separate machine: not this laptop, nothing installed, no downloads allowed now. FPGA logic is about 3-4 times slower than an ASIC on the critical path (Kuon and Rose 2007) and clocks far below 5 GHz; the gain is parallel units, not signal speed. Small boards hold a few thousand logic cells and tens to hundreds of kilobits of block RAM: thousands of trivial cells, not a million rich creatures. Whether hardware description counts under Law 1 ("C23 and NASM") is the owner's call |
| **Below the instruction set: ASIC** | Everything | Open flows exist (OpenLane/OpenROAD on open process kits) but need a paid shuttle and months | Nothing for this project today | Out of scope: cost, lead time, no way to iterate |

## Cache latencies (load-to-use, in core clock cycles)

| Core | Level | Size | Cycles | Source and status |
|---|---|---|---|---|
| Lion Cove (P) | L0 data | 48 KB | 4 | Intel slide "Lion Cove Architecture", Tech Tour 2024, page 17. Intel-documented |
| Lion Cove (P) | L1 data | 192 KB | 9 | same slide. Intel-documented |
| Lion Cove (P) | L2 | 3 MB (Arrow Lake) / 2.5 MB (Lunar Lake) | 17 | same slide. Intel-documented |
| Lion Cove (P) | L3 | 36 MB shared | about 84 | Chips and Cheese, measured on a Core Ultra 9 285K. Third-party, not Intel |
| Lion Cove (P) | DRAM | - | over 565 | Chips and Cheese, same article. Third-party |
| Skymont (E) | L1 data | 32 KB | 4 | Chips and Cheese (3 on the previous E-core). Third-party; an Intel-published figure was not found |
| Skymont (E) | L2 | 4 MB per 4 cores | 19 | Chips and Cheese, measured on Lunar Lake; for Arrow Lake they say only "relatively fast". Third-party |
| Skymont (E) | L3 | 36 MB shared | about 69 | Chips and Cheese, Arrow Lake. Third-party |

Notes. CPUID leaf 4 on this machine names the 48 KB level "L1 data" and does not list the 192 KB
level at all (measured), so tools that read CPUID will under-report the P-core hierarchy.
Intel's Optimization Reference Manual was not retrieved; Skymont's Intel-documented latencies are
therefore **unknown** here. Converting cycles to time needs the actual frequency under load, which
was not measured.

Design consequence: a block that must be "L1-sized" on a P-core means 48 KB at 4 cycles; 192 KB
is still private and costs 9; anything up to 3 MB costs 17. On an E-core the fast level is 32 KB.

## Local measurement (ran now)

Command (Git Bash, in `/s/factcheck/below-asm`):

```
gcc -std=gnu2x -O1 -Wall -Wextra -D_WIN32_WINNT=0x0A00 -mclflushopt probe.c -o probe.exe && ./probe.exe
```

Output (complete, also in `probe_out.txt`; no timings taken):

```
brand: Intel(R) Core(TM) Ultra 9 275HX
leaf1 eax=000c0662 (family/model/stepping)
cache-control and low-level instruction support (CPUID):
  CLFLUSH        yes
  SSE2(MOVNT*)   yes
  MONITOR/MWAIT  yes
  hypervisor     yes
  CLFLUSHOPT     yes
  CLWB           yes
  AVX2           yes
  AVX512F        no
  RDSEED         yes
  PREFETCHWT1    no
  WAITPKG        yes
  CLDEMOTE       no
  MOVDIRI        yes
  MOVDIR64B      yes
  RDPID          yes
  SERIALIZE      yes
  HYBRID         yes
  PREFETCHI      no
  AVX-VNNI       yes
  PREFETCHW      yes
  1GB pages      yes
thermal/power leaf 6 eax=009fcff3
  TurboBoost     yes
  HWP            yes
  HWP_EPP        yes
  ThreadDirector yes
arch perfmon leaf 0xA: version=6 gp_counters=8 gp_width=48 fixed_counters=3 fixed_width=48
leaf 0x1A core type of the core this ran on: 0x40 (0x40=Core/P, 0x20=Atom/E)
deterministic cache parameters (CPUID leaf 4) on this core:
  L1 data: 48 KB, 12-way, line 64, shared by up to 2 logical cpus
  L1 inst: 64 KB, 16-way, line 64, shared by up to 2 logical cpus
  L2 unified: 3072 KB, 12-way, line 64, shared by up to 8 logical cpus
  L3 unified: 36864 KB, 12-way, line 64, shared by up to 128 logical cpus
RDMSR (ecx=0x10) from ring 3: EXCEPTION_PRIV_INSTRUCTION (0xC0000096)
RDPMC (fixed ctr 0) from ring 3: EXCEPTION_PRIV_INSTRUCTION (0xC0000096)
RDPMC (gp ctr 0) from ring 3: EXCEPTION_PRIV_INSTRUCTION (0xC0000096)
MOV RAX,CR4 from ring 3: EXCEPTION_PRIV_INSTRUCTION (0xC0000096)
CLI from ring 3: EXCEPTION_PRIV_INSTRUCTION (0xC0000096)
PREFETCHT0/NTA/W, CLFLUSH, CLFLUSHOPT, MOVNTI, SFENCE from ring 3: executed, no fault
GetLargePageMinimum = 2097152 bytes
VirtualAlloc(MEM_LARGE_PAGES): failed, GetLastError=1314 (1314 = privilege not held)
CPU sets: 24 total; EfficiencyClass counts: class0=16 class1=8 class2=0; parked=16 allocated-to-others=0
  EfficiencyClass 0 logical processor indices: 2 3 4 5 6 7 8 9 14 15 16 17 18 19 20 21
  EfficiencyClass 1 logical processor indices: 0 1 10 11 12 13 22 23
```

Reading it. The "shared by up to N" fields are CPUID maxima, not actual sharing. A higher
EfficiencyClass means a faster core, so class 1 is the 8 P-cores. The parked count changed
between two runs (0, then 16), so it reflects load at that moment. The probe reads only; the
privileged instructions were attempted to see them fault, and they did.

## Verdicts on the exhibits (claims that touch this layer)

Fairness check applied: message 2 section 4 lists trade-offs (compiler parity, neighbour
search cost, GPU versus CPU). None of them mentions firmware interrupts, paging in long mode,
or FPGA clock speed, so they do not qualify the claims marked below. Where the same message
does qualify a claim, that is noted.

| # | Claim (exhibit) | Verdict | Evidence | Source |
|---|---|---|---|---|
| 1 | A UEFI x86-64 `.efi` application can boot from USB straight into 64-bit long mode and call `ExitBootServices()` (msg 2, intro and 3.1) | correct | UEFI x64 handoff is long mode with paging on | https://uefi.org/specs/UEFI/2.10/02_Overview.html |
| 2 | Bare metal means "no virtual memory TLB misses" (msg 2, 3) | wrong | Long mode cannot run without paging. UEFI: "Paging mode is enabled and any memory space defined by the UEFI memory map is identity mapped". The TLB is still used; 1 GB pages reduce misses, they do not remove them | https://uefi.org/specs/UEFI/2.10/02_Overview.html |
| 3 | Bare metal means "no interrupts"; `CLI` gives hardware interrupts disabled (msg 2, table row 3 and 3.3) | misleading | UEFI hands over with interrupts enabled. `CLI` masks only maskable interrupts. System-management interrupts cannot be disabled by the OS or any ring-0 code and take CPU time | https://en.wikipedia.org/wiki/System_Management_Mode (cites Intel SDM Vol. 3, SMM chapter) |
| 4 | "You own 100% of physical RAM, L1/L2/L3 caches" and "100% deterministic control over the 275HX" (msg 2 table row 3; msg 1 section 2) | misleading | SMRAM is unreadable outside System Management Mode; firmware, microcode and SMM stay in charge below ring 0. You own what the UEFI memory map hands you | https://en.wikipedia.org/wiki/System_Management_Mode |
| 5 | The framebuffer is at `0xE0000000` (msg 2 table row 3) | misleading | The address is whatever `FrameBufferBase` returns for the active mode, and a mode may have no linear framebuffer (`PixelBltOnly`). Qualified by the same message: section 3.2 correctly says to query the Graphics Output Protocol for the pointer | https://uefi.org/specs/UEFI/2.10/12_Protocols_Console_Support.html |
| 6 | Query GOP, then write pixels directly to the linear framebuffer with no graphics API (msg 2, 3.2) | mostly_correct | Spec: the mode info lets software "draw directly to the frame buffer without using Blt()". Pixel layout is given by the mode (not necessarily RGBA) and there is no vertical-sync service, so "at hardware refresh speed" is loose | https://uefi.org/specs/UEFI/2.10/12_Protocols_Console_Support.html |
| 7 | Wake the other P-cores with INIT-SIPI-SIPI over the local APIC (msg 2, 3.3) | mostly_correct | That is the real mechanism (the firmware's own MP library does it). While boot services are alive the supported route is the MP Services protocol (`StartupAllAPs`), which is available only at boot time. There are 23 other cores, 7 of them P-cores | https://qemu.googlesource.com/edk2/+/refs/heads/UDK2017/MdePkg/Include/Protocol/MpService.h |
| 8 | 1.6 MB "fits entirely inside a single P-core's 2 MB L2 cache" (msg 2, 2) | mostly_correct | The figure is wrong, the conclusion holds: this P-core's L2 is 3072 KB (CPUID leaf 4, measured above); Intel's slide says 2.5 MB / 3 MB. Message 1 itself says 3 MB | local: `./probe.exe` output above; Intel Lion Cove slide p.17 (URL in sources) |
| 9 | Structure-of-arrays gives "zero cache misses" (msg 2, 2) | misleading | 1.6 MB does not fit the 48 KB (4-cycle) or 192 KB (9-cycle) levels, so every sweep misses those and is served from L2 at 17 cycles unless the prefetcher runs ahead. "No DRAM access after warm-up" would be the defensible claim, and it needs a counter to prove | Intel Lion Cove slide p.17: https://cdrdv2-public.intel.com/824430/2024_Intel_Tech%20Tour%20TW_Next%20Gen%20P-core%20The%20Lion%20Cove%20Architecture-4.pdf |
| 10 | Below assembly is an FPGA or ASIC: no CPU, no fetch-decode-execute, no instruction set; logic is gates and wires (msg 2, table row 4) | correct | That is what a hardware description compiles to (lookup tables and routing on an FPGA). Open flow exists | https://github.com/YosysHQ/nextpnr |
| 11 | In an FPGA "signals propagate at near-light speed through silicon combinational logic" (msg 2, table row 4) | misleading | Speed is set by gate and routing-switch delay, not wire propagation. Measured FPGA critical paths are roughly 3-4 times slower than the same design as an ASIC. The advantage is parallelism | https://www.eecg.utoronto.ca/~jayar/pubs/kuon/kuontcad06.pdf |
| 12 | OS-less bare metal loses CUDA on the RTX 5070 Ti because the GPU needs GSP firmware and a kernel driver (msg 1, section 2) | mostly_correct | NVIDIA's own kernel-module source says it "must be used with GSP firmware and user-space NVIDIA GPU driver components" and targets Linux only; nothing equivalent exists for a UEFI application. Only the firmware framebuffer remains. "Signed" was not checked | https://github.com/NVIDIA/open-gpu-kernel-modules |
| 13 | `VirtualLock` "prevents OS pagefile eviction to your SSD permanently" (msg 1, 4.1) | mostly_correct | Microsoft: locked pages "are guaranteed not to be written to the pagefile while they are locked". Not permanent: until unlock or process exit, and only after the working-set size is raised, because the default lock limit is "intentionally small" | https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtuallock |
| 14 | "Cores 0-7" are the 8 P-cores and "Cores 8-23" the 16 E-cores (msg 1, 4.3) | wrong | On this machine Windows reports the P-cores as logical processors 0, 1, 10, 11, 12, 13, 22, 23 | local: `./probe.exe` output above (EfficiencyClass 1 list) |
| 15 | User-space assembly runs under the Windows scheduler and virtual-memory paging (msg 2, table row 1) | correct | Ring 3 cannot mask interrupts or read control registers: `CLI`, `MOV from CR4`, `RDMSR`, `RDPMC` all fault here | local: `./probe.exe` output above |

Not judged here (other agents' ground): the timing arithmetic for `vfmadd231ps`, memory
bandwidth figures, GPU cache sizes, the 10x-30x GPU claim.

## What I would do with this (recommendation, not a claim of result)

1. Stay in ring 3 for the simulation. Use CPU Sets queried at start, a locked arena, and (once
   the owner grants the privilege) 2 MB pages.
2. Build one small UEFI measurement harness later, when an emulator is available: the same
   C23/NASM inner kernel, bracketed by performance-counter reads, with prefetchers on and off.
   Its only job is to answer "how many L1/L2/L3 misses per tick, per block size". That makes
   Law 2 a number instead of an assertion.
3. Treat the FPGA as a separate, optional experiment for one tiny integer rule. Do not plan the
   million-creature world around it.

## Sources

Primary or vendor:
- UEFI Specification 2.10, 2.3.4 x64 Platforms: https://uefi.org/specs/UEFI/2.10/02_Overview.html
- UEFI Specification 2.10, 12.9 Graphics Output Protocol: https://uefi.org/specs/UEFI/2.10/12_Protocols_Console_Support.html
- Intel, "Next Gen P-core: The Lion Cove Architecture" (Tech Tour 2024), page 17 (text extracted locally with PyMuPDF from the fetched PDF): https://cdrdv2-public.intel.com/824430/2024_Intel_Tech%20Tour%20TW_Next%20Gen%20P-core%20The%20Lion%20Cove%20Architecture-4.pdf
- Intel whitepaper 357930-001US, "Hardware Prefetch Controls for Intel Atom Cores", Dec 2023 (mirror; text extracted locally): https://kib.kiev.ua/x86docs/Intel/misc/357930-001.pdf
- Linux `msr-index.h` v6.12 (register numbers 0x1A0, 0x1A4, 0xE2, 0x1AD, 0x770, 0x771, 0x774, 0x199, 0x1B0, 0x79, 0x8B, 0x1FC, 0xCE, 0x610): https://github.com/torvalds/linux/blob/v6.12/arch/x86/include/asm/msr-index.h
- Linux `perf_event.h` v6.12 (0xC1, 0x186, 0x309, 0x38D; USR/OS/ENABLE bits): https://github.com/torvalds/linux/blob/v6.12/arch/x86/include/asm/perf_event.h
- Intel perfmon events, Arrow Lake (event lists; page did not load for me, named as the place to take codes from): https://perfmon-events.intel.com/arrowlake.html
- Linux perf Arrow Lake events patch (shows `cpu_core` `MEM_LOAD_RETIRED.L1_MISS`): https://lkml.iu.edu/hypermail/linux/kernel/2502.0/02404.html
- Microsoft, CPU Sets: https://learn.microsoft.com/en-us/windows/win32/procthread/cpu-sets
- Microsoft, Large-Page Support: https://learn.microsoft.com/en-us/windows/win32/memory/large-page-support
- Microsoft, VirtualLock: https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtuallock
- Microsoft, EnableThreadProfiling ("you need a driver to configure the counters"): https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-enablethreadprofiling
- RDPMC instruction reference (CR4.PCE rule): https://www.felixcloutier.com/x86/rdpmc.html
- EDK2 MP Services protocol header: https://qemu.googlesource.com/edk2/+/refs/heads/UDK2017/MdePkg/Include/Protocol/MpService.h
- NVIDIA open GPU kernel modules README: https://github.com/NVIDIA/open-gpu-kernel-modules
- nextpnr README (supported FPGA families): https://github.com/YosysHQ/nextpnr
- Kuon and Rose, "Measuring the Gap Between FPGAs and ASICs", IEEE TCAD 26(2), 2007: https://www.eecg.utoronto.ca/~jayar/pubs/kuon/kuontcad06.pdf

Reputable secondary:
- Chips and Cheese, Lion Cove memory subsystem in Arrow Lake: https://old.chipsandcheese.com/?p=34012
- Chips and Cheese, Skymont in desktop form: https://chipsandcheese.com/p/skymont-in-desktop-form-atom-unleashed
- Chips and Cheese, Skymont in Lunar Lake: https://chipsandcheese.com/p/skymont-intels-e-cores-reach-for-the-sky
- Wikipedia, System Management Mode: https://en.wikipedia.org/wiki/System_Management_Mode
- Koppe et al., "Reverse Engineering x86 Processor Microcode" (Intel signs updates since 1995): https://ar5iv.labs.arxiv.org/html/1910.00948
- Borrello et al., WOOT '23, Goldmont microcode ("signed and encrypted"): https://misc0110.net/files/cpu_woot23.pdf
- oss-security, AMD microcode signature flaw (EntrySign, AMD only): https://openwall.com/lists/oss-security/2025/03/06/2
- WinRing0 flagged by Defender, 2025: https://www.neowin.net/news/windows-1110-is-flagging-winring0-on-your-pc-monitoring-fan-control-apps-heres-why/
- FPGA board overview and prices (secondary, unverified today): https://www.cnx-software.com/2022/05/16/icesugar-nano-tiny-ice40lp1k-fpga-board-with-3x-pmod-connectors/

## Unknown or not done

- Lion Cove (P-core) bit layout of register 0x1A4 on Arrow Lake: not confirmed.
- Skymont latencies as published by Intel: not found; third-party measurements used.
- Exact Arrow Lake event codes for L2/L3 miss counters: not retrieved.
- Windows built-in processor-counter sampling: not tested.
- Nothing was built for UEFI or FPGA; no register was written; no timing was taken.
- FPGA board prices and block-RAM sizes: from secondary pages and general knowledge, not vendor pages.
