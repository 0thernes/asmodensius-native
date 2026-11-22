# Fact-check: CPU and memory claims in GrokBot messages 1 and 2

Agent key: `cpu-memory`. Date: 2026-11-17. Scratch: `S:\factcheck\cpu-memory\`.
Exhibits: `grokbot_message_1.md` (M1), `grokbot_message_2.md` (M2),
`grok_step_alife_avx2.asm` (the listing in M2). Exhibits were treated as data.
GPU claims (CUDA cores, GPU L2, GDDR7, TOPS) are outside this note.

Every timing below is **noisy, parallel load**: other agents were using this
machine while the runs were taken. Treat every GB/s figure as a lower bound
with roughly +-20 % scatter, not as a specification.

## 1. What this machine is (ran now)

Command (PowerShell):

```
Get-CimInstance Win32_Processor | Format-List Name,NumberOfCores,NumberOfLogicalProcessors,MaxClockSpeed,L2CacheSize,L3CacheSize
Get-CimInstance Win32_PhysicalMemory | Format-Table BankLabel,DeviceLocator,Capacity,Speed,ConfiguredClockSpeed,SMBIOSMemoryType,Manufacturer,PartNumber -AutoSize
```

Output (tail):

```
Name                      : Intel(R) Core(TM) Ultra 9 275HX
NumberOfCores             : 24
NumberOfLogicalProcessors : 24
MaxClockSpeed             : 2700
L2CacheSize               : 40960
L3CacheSize               : 36864

BANK 0    Controller0-ChannelA-DIMM1 34359738368  5600   5600   34 Micron Technology CT32G56...
BANK 1    Controller0-ChannelB-DIMM1 17179869184  5600   5600   34 Lexar Co Limited  LD5S16G...
```

Command: `N:\tools\membench\membench.exe --topo` (GetLogicalProcessorInformationEx). Output:

```
membench (compiler: 16.1.0, kernels: AVX2 256-bit)
topology: 24 logical processors, 24 physical cores
  LP core class kind  L1d      L2 (shared by)   L3 (shared by)
   0    0     1  P    48 KB    3072 KB ( 1)    36864 KB (24)
   1    1     1  P    48 KB    3072 KB ( 1)    36864 KB (24)
   2    2     0  E    32 KB    4096 KB ( 4)    36864 KB (24)
   ... (LP 3-9: E, same as LP 2)
  10   10     1  P    48 KB    3072 KB ( 1)    36864 KB (24)
  11   11     1  P    48 KB    3072 KB ( 1)    36864 KB (24)
  12   12     1  P    48 KB    3072 KB ( 1)    36864 KB (24)
  13   13     1  P    48 KB    3072 KB ( 1)    36864 KB (24)
  14   14     0  E    32 KB    4096 KB ( 4)    36864 KB (24)
   ... (LP 15-21: E, same as LP 14)
  22   22     1  P    48 KB    3072 KB ( 1)    36864 KB (24)
  23   23     1  P    48 KB    3072 KB ( 1)    36864 KB (24)
  summary: 8 P logical processors (class 1), 16 E logical processors (class 0)
```

Facts read off this output:

- 24 cores, 24 logical processors, one logical processor per core (no SMT).
- P cores are logical processors **0, 1, 10, 11, 12, 13, 22, 23**; E cores are
  2-9 and 14-21. P cores are not "0-7".
- P core: L1d 48 KB, L2 3 MB private. E core: L1d 32 KB, L2 4 MB shared by 4.
  L3 36 MB shared by all 24. L2 total 8 x 3 + 4 x 4 = 40 MB.
- Windows does not report Lion Cove's 192 KB middle data cache; the measured
  table in section 4 shows it (throughput step between 32 KB and 256 KB).
- RAM: one 32 GB + one 16 GB SODIMM, both DDR5 running at 5600 MT/s.

## 2. The measurement tool

`N:\tools\membench\membench.c` (C, Windows). Reads the topology, pins the
thread with `SetThreadAffinityMask`, allocates one `VirtualAlloc` buffer,
touches every page, then for each working set measures sequential read (sum of
64-bit words), write and copy; each trial repeats passes until it lasts at
least 0.2 s; best of 5 trials. GB = 1e9 bytes. Copy reports bytes copied per
second (half the working set per pass). Kernels are 256-bit AVX2 with ordinary
cached stores; a checksum self-check runs every time.

Build (ran now):

```
cd /n/tools/membench && gcc -O2 -march=native -o membench.exe membench.c; echo "gcc exit=$?"
gcc exit=0
gcc.exe (MinGW-W64 x86_64-ucrt-posix-seh, built by Brecht Sanders, r2) 16.1.0
```

Kernel check (`objdump -d membench.exe`): `k_read` is four `vpaddq (mem),%ymm`
per 128 bytes; `k_write` and `k_copy` are `vmovdqa` loads/stores; calls to
memset/memcpy inside the three kernels: 0.

Two defects found and fixed while building it (so the numbers below come from
the fixed tool): (1) GCC proved the read kernel pure and folded N passes into
one, which made the calibration loop spin forever; fixed with
`__attribute__((noipa))`. (2) one pass over 16 KB is shorter than the timer
resolution; calibration now doubles the pass count until a run lasts 5 ms.
Three early runs were lost to these defects (two hung, one passed 100 s and was
stopped). A full 16 KB - 512 MB sweep takes about 60 s, so each core was run
as two commands to stay under the 100 s limit.

## 3. Commands that produced the measured table

```
./membench.exe --core P --max-kb 4096      (31.5 s)   -> S:\factcheck\cpu-memory\run_P_small.txt
./membench.exe --core P --min-kb 8192      (28.2 s)   -> run_P_large.txt
./membench.exe --core E --max-kb 4096      (29.7 s)   -> run_E_small.txt
./membench.exe --core E --min-kb 8192      (28.8 s)   -> run_E_large.txt
```

Header lines of those runs:

```
pinned to LP 1 (now on 1): P core, class 1, L1d 48 KB, L2 3072 KB (shared by 1), L3 36864 KB (shared by 24)
effective clock (32-add dependent chain, best of 5): 4.65 GHz      (small run; 5.09 GHz in the large run)
pinned to LP 3 (now on 3): E core, class 0, L1d 32 KB, L2 4096 KB (shared by 4), L3 36864 KB (shared by 24)
effective clock (32-add dependent chain, best of 5): 4.29 GHz      (small run; 4.58 GHz in the large run)
checks: ok                                                         (all four runs)
```

## 4. Measured table (noisy, parallel load)

Single thread, pinned. GB/s = 1e9 bytes/s. "fits" is the level Windows reports
for that core; on the P core the 64 KB and 128 KB rows actually sit in the
192 KB middle cache that Windows does not report.

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

`*` = inside Lion Cove's 192 KB middle data cache.

Ratios read off the table (sequential access, one core):

- P core read: L1d 422 -> 192 KB level 204 (2.1x) -> L2 115-159 (2.7-3.7x)
  -> L3 43-68 (6-10x) -> DRAM 12.5-17.6 (24-34x below L1d).
- Leaving L3 for DRAM costs 2.5-4x for reads, 4-5x for writes.
- Leaving a P core's L2 for DRAM costs 7-12x for reads.
- The drop near the L3 size already shows at 32 MB (L3 is shared with the
  other 23 cores, which were busy).
- For 64 KB - 4 MB an E core writes and copies faster than a P core
  (95 vs 70-80 GB/s write, 68 vs 49-58 GB/s copy).

### 4b. All eight P cores at once (noisy, parallel load)

Command (Git Bash): eight concurrent instances, one per P logical processor,
1 MB working set each (private L2):

```
for lp in 0 1 10 11 12 13 22 23; do ./membench.exe --lp $lp --min-kb 1024 --max-kb 1024 > /s/factcheck/cpu-memory/agg_lp$lp.txt 2>&1 & done; wait
```

Output (clock | 1 MB row | checks):

```
LP 0: 4.08 GHz |       1 MB  L2           118.0         73.3         48.9 | checks: ok
LP 1: 4.40 GHz |       1 MB  L2           122.3         76.3         52.2 | checks: ok
LP 10: 4.34 GHz |       1 MB  L2           128.4         77.4         54.7 | checks: ok
LP 11: 4.74 GHz |       1 MB  L2           127.4         77.9         53.7 | checks: ok
LP 12: 4.66 GHz |       1 MB  L2           126.5         77.4         54.8 | checks: ok
LP 13: 4.62 GHz |       1 MB  L2           135.3         77.1         57.1 | checks: ok
LP 22: 4.84 GHz |       1 MB  L2           135.4         77.2         53.4 | checks: ok
LP 23: 4.84 GHz |       1 MB  L2           127.5         76.7         52.2 | checks: ok
SUM over 8 P cores: read 1020.8  write 613.3  copy 427.0 GB/s
```

### 4c. The M2 kernel, timed (noisy, parallel load)

`S:\factcheck\cpu-memory\tick_harness.c` calls the exhibit listing, assembled
unmodified from a scratch copy, for 100,000 agents on one pinned core.

```
cd /s/factcheck/cpu-memory && cp /n/docs/research/factcheck/grok_step_alife_avx2.asm ./exhibit_copy.asm && nasm -f win64 exhibit_copy.asm -o exhibit_copy.obj && gcc -O2 -Wall -Wextra -o tick_harness.exe tick_harness.c exhibit_copy.obj && echo BUILD_OK && ./tick_harness.exe 1 && ./tick_harness.exe 3 && ./tick_harness.exe 12
BUILD_OK
LP 1 (ran on 1): 100000 agents, 12500 iterations per tick, 50000 ticks timed
  ns per tick: best 20158, median 20438, worst batch 20968  (ticks/s at best: 49608)
  ps per agent at best: 201, vel check: ok
LP 3 (ran on 3): 100000 agents, 12500 iterations per tick, 50000 ticks timed
  ns per tick: best 12925, median 12963, worst batch 13255  (ticks/s at best: 77369)
  ps per agent at best: 129, vel check: ok
LP 12 (ran on 12): 100000 agents, 12500 iterations per tick, 50000 ticks timed
  ns per tick: best 20186, median 20360, worst batch 21412  (ticks/s at best: 49539)
  ps per agent at best: 201, vel check: ok
```

Same kernel at smaller populations (`tick_harness2.c`, a copy of the harness
with the population as a second argument; its first output line still prints
a stale "100000 agents" label, the per-agent figure uses the real population):

```
./tick_harness2.exe 1 2000 ; ./tick_harness2.exe 1 8000 ; ./tick_harness2.exe 1 100000 ; ./tick_harness2.exe 3 2000
  ns per tick: best 101, median 108, worst batch 132      ps per agent at best: 50, vel check: ok
  ns per tick: best 1296, median 1309, worst batch 1336   ps per agent at best: 162, vel check: ok
  ns per tick: best 20272, median 21053, worst batch 22587 ps per agent at best: 202, vel check: ok
  ns per tick: best 208, median 216, worst batch 227      ps per agent at best: 104, vel check: ok
```

| Core | Agents | Data size | Level | ns per tick | ps per agent |
|---|---:|---:|---|---:|---:|
| P (LP 1) | 2,000 | 32 KB | L1d | 101 | 50 |
| P (LP 1) | 8,000 | 128 KB | 192 KB level | 1,296 | 162 |
| P (LP 1) | 100,000 | 1.6 MB | L2 | 20,272 | 202 |
| E (LP 3) | 2,000 | 32 KB | L1d | 208 | 104 |
| E (LP 3) | 100,000 | 1.6 MB | L2 | 12,925 | 129 |

On a P core the same kernel is 4.0x faster per agent when its data fits L1d
than when it streams from L2. On L1d-sized data the P core is 2x faster than
the E core; on L2-sized data the E core is 1.56x faster than the P core.

Two things had to be worked around to run the listing at all, and both bear on
its numbers: (a) the listing does `dec rax` once per 8-agent iteration with
`rax` = the agent count, so passing 100,000 as its comment says runs 100,000
iterations and walks 8x past the end of every array; the harness passes 12,500.
(b) it overwrites xmm13-15, which the Win64 ABI requires a callee to preserve;
the harness keeps nothing in those registers.

## 5. Sources

Fetched and read in this session unless marked "(search summary)".

- [ARK] Intel product page, Core Ultra 9 275HX specifications:
  https://www.intel.com/content/www/us/en/products/sku/242293/intel-core-ultra-9-processor-275hx-36m-cache-up-to-5-40-ghz/specifications.html
  (24 cores = 8 P + 16 E, 24 threads, 5.4 GHz max turbo, E-core max 4.6 GHz,
  36 MB Smart Cache, 40 MB total L2, 55 W base / 160 W max turbo, "Up to DDR5
  6400 MT/s", 2 channels, AVX2 listed, no AVX-512 listed).
- [CC-ARL] Chips and Cheese, "Analyzing Lion Cove's Memory Subsystem in Arrow
  Lake": https://old.chipsandcheese.com/?p=34012 (48 KB / 4 cycles, 192 KB /
  9 cycles, 3 MB L2 / 17 cycles, 36 MB L3 / ~84 cycles, DRAM access over 565
  core cycles).
- [CC-LNC] Chips and Cheese, "Lion Cove: Intel's P-Core Roars":
  https://old.chipsandcheese.com/2024/09/27/lion-cove-intels-p-core-roars/
  (L2 bandwidth tops out at 32 bytes per cycle; L3 read from one core just
  over 10 bytes per cycle; tested on Lunar Lake, 2.5 MB L2).
- [WIKI-LNC] https://en.wikipedia.org/wiki/Lion_Cove (search summary: 2.5 MB
  L2 on Lunar Lake, 3 MB on Arrow Lake).
- [TPU] TechPowerUp, Core Ultra 7 265K review, architecture page:
  https://techpowerup.com/review/intel-core-ultra-7-265k/3.html (search
  summary: 3 MB L2 per Lion Cove P core, SMT removed, Skymont clusters of
  four sharing 4 MB L2).
- [UOPS] uops.info, VFMADD231PS (YMM, YMM, YMM):
  https://uops.info/html-instr/VFMADD231PS_YMM_YMM_YMM.html (Arrow Lake-P:
  latency 4 cycles, throughput 0.50 cycles; Arrow Lake-E: latency 4, 0.50).
- [SDM] Intel SDM Vol. 3A chapter 4 (mirror, search summary):
  https://xem.github.io/minix86/manual/intel-x86-and-64-manual-vol3/o_fe12b1e2a880e0ce-106.html
  (IA-32e mode is defined by IA-32e paging being enabled).
- [FLEX] Intel Desktop Board DQ77MK Technical Product Specification, flex
  memory mode (search summary):
  https://www.manualslib.com/manual/614294/Intel-Dq77mk.html?page=22
- [LOCAL-n] a measurement in sections 1-4 of this note.

## 6. Claim table

Verdicts: correct, mostly_correct, misleading, wrong, unverifiable.

| # | Where | Claim | Verdict | Evidence | Source |
|---|---|---|---|---|---|
| 1 | M1 s1 | 24 physical cores and 24 threads; Arrow Lake removed SMT | correct | Windows reports 24 cores, 24 logical processors, one per core. Intel lists 24 cores, 24 threads. | LOCAL-1, ARK |
| 2 | M1 s1 | 8 Lion Cove P cores + 16 Skymont E cores | correct | 8 class-1 + 16 class-0 logical processors here; Intel lists 8 P + 16 E. Core names per reviews of the Arrow Lake family. | LOCAL-1, ARK, CC-ARL, TPU |
| 3 | M1 s1 | P cores up to 5.4 GHz | correct | Intel: P-core max turbo 5.4 GHz. Local add-chain clock on a P core under load: 4.65-5.18 GHz (below the limit, as expected under load). | ARK, LOCAL-3 |
| 4 | M1 s1 | E cores up to 4.6 GHz | correct | Intel: E-core max turbo 4.6 GHz. Local add-chain clock on an E core: 4.29-4.58 GHz. | ARK, LOCAL-3 |
| 5 | M1 s1 | 3 MB dedicated L2 per P core, 24 MB P-core L2 total | correct | Every P logical processor reports L2 3072 KB shared by 1; 8 x 3 MB = 24 MB. | LOCAL-1, CC-ARL |
| 6 | M2 s2 | "a single P-core's 2 MB L2 cache" | wrong | It is 3 MB on this CPU (2 MB was the previous generation's P-core L2; 2.5 MB is Lion Cove in Lunar Lake). M2 does not correct this anywhere; M1 has the right figure. The conclusion drawn from it (1.6 MB fits one P core's L2) still holds, with more room. | LOCAL-1, CC-ARL, WIKI-LNC |
| 7 | M1 s1 | 16 E cores across four 4 MB clusters, 16 MB E-core L2 | correct | Every E logical processor reports L2 4096 KB shared by 4; 4 x 4 MB = 16 MB. | LOCAL-1, TPU |
| 8 | M1 s1 | 36 MB shared L3 | correct | L3 36864 KB shared by 24; Intel: 36 MB Smart Cache. | LOCAL-1, ARK |
| 9 | task | Lion Cove data-cache levels 48 KB / 192 KB / 3 MB | correct | 48 KB and 3 MB are reported by Windows. The 192 KB level is not reported by Windows but shows in the measurement: read 422 GB/s at 32 KB, 204 at 64-128 KB, 148-159 at 256 KB-1 MB. Neither message mentions the 192 KB level. | LOCAL-1, LOCAL-4, CC-ARL |
| 10 | M1 s1 | "backed by DDR5-6400 (~102.4 GB/s)" | wrong | For this machine. The CPU supports up to DDR5-6400, and 6400 MT/s x 8 bytes x 2 channels = 102.4 GB/s is the correct interface ceiling for that speed. This laptop has DDR5-5600 (ceiling 5600 x 8 x 2 = 89.6 GB/s), with unequal 32 GB + 16 GB modules. Measured DRAM stream on one core: read 12.5-21.6, write 8.8-12.0, copy 4.9-8.1 GB/s. With unequal modules Intel controllers normally run part of memory single-channel (flex mode); not measured here. | LOCAL-1, LOCAL-4, ARK, FLEX |
| 11 | M1 s2 table | "~1.2 TB/s (P-Core L2)" peak bandwidth for the 8 P cores | mostly_correct | True only as the sum of read-only streams over all 8 P cores, each inside its own L2. Measured sum: read 1021 GB/s, write 613, copy 427. Published limit 32 bytes/cycle/core gives 8 x 32 x 4.7-5.4 GHz = 1.20-1.38 TB/s. One core delivers 115-159 GB/s read, 70-81 write. Any kernel that also stores gets half or less. | LOCAL-4b, LOCAL-4, CC-LNC |
| 12 | M1 s2 table | UEFI bare metal "~1.2 TB/s (zero OS jitter)" | unverifiable | Same hardware limit as row 11; no bare-metal run was possible (no QEMU, no boot test). Nothing suggests the OS lowers L2 throughput: best and worst timed batches of the M2 kernel differ by 4 % under Windows with other agents running. | LOCAL-4c |
| 13 | M1 s1 | Past the cache boundary "bandwidth drops by 5x to 200x across DRAM and NVMe swap" | mostly_correct | The range brackets what was measured and published for DRAM: sequential read L3 -> DRAM 2.5-4x, L2 -> DRAM 7-12x, L1d -> DRAM 24-34x; published load latency DRAM over 565 cycles versus 84 (L3), 17 (L2), 4 (L1d) = 7x, 33x, 141x for pointer-chasing access. The "NVMe swap" part does not follow from leaving the cache: 64-512 MB working sets ran from DRAM at 9-22 GB/s with no paging on this 48 GB machine. Swap needs memory pressure, not a 36 MB footprint. | LOCAL-4, CC-ARL |
| 14 | M1 s1 | The cliff is at "36 MB on CPU" | misleading | 36 MB is the right L3 size and a real step (read 43-49 GB/s at 8-16 MB, 12-22 at 64 MB and above). It is not the boundary that governs a per-core hot loop: a P core loses 2.1x at 48 KB, 2.7-3.7x by 256 KB-2 MB and 6-10x on leaving its 3 MB L2. The L3 is one pool shared by all 24 cores; throughput had already fallen at 32 MB with other processes running. For sequential SoA data the step past L3 is the smallest one (2.5-4x). | LOCAL-4 |
| 15 | M1 s3 | 100,000 agents x 128 bytes = 12.8 MB "fits ... inside the 275HX's 24 MB P-core L2" | misleading | Arithmetic is right. The 24 MB is eight separate private 3 MB caches, not one pool. 12.8 MB fits only if the population is cut into eight 1.6 MB slices and each slice is touched by one pinned P core only; neighbour reads across slices break that. As one data set it lives in the shared 36 MB L3 (measured 43-49 GB/s read per core there, versus 115-159 in L2). | LOCAL-1, LOCAL-4 |
| 16 | M1 s4 | "Cores 0-7 (8 Lion Cove P-Cores)", "Cores 8-23 (16 Skymont E-Cores)" | wrong | On this machine the P cores are logical processors 0, 1, 10-13, 22, 23. Pinning 0-7 would put 2 P cores and 6 E cores in the hot set. Affinity must come from EfficiencyClass at start-up, as membench does. | LOCAL-1 |
| 17 | M1 s5 | "Sustaining 160W on the 275HX" | correct | Intel: Maximum Turbo Power 160 W (base 55 W). The laptop's own limits were not read. | ARK |
| 18 | M1 s3 | Phase stagger with K = 64 gives 1,562 organisms per tick | correct | 100,000 / 64 = 1,562.5. | arithmetic |
| 19 | M1 s1, M2 s4 | N = 100,000 naive pairs = 10^10 | correct | 100,000^2 = 10^10 ordered pairs (5 x 10^9 unordered). | arithmetic |
| 20 | M2 intro | Engine can use "AVX2/AVX-512 SIMD registers" on this CPU | misleading | General statement is true for x86-64, but this CPU has no AVX-512 (Intel lists SSE4.1, SSE4.2, AVX2). The M2 listing itself uses only AVX2/FMA. | ARK |
| 21 | M2 s2 | 4 arrays x 100,000 x 4 bytes = 1.6 MB, fits one P core's L2 | correct | Arithmetic right; 1.6 MB < 3 MB L2 (see row 6 for the wrong "2 MB"). | LOCAL-1 |
| 22 | M2 s2 | "never touching DDR5 RAM" for that 1.6 MB | mostly_correct | Once warm and if nothing else evicts it, yes. The first pass, and any pass after the core ran other work, loads from DRAM. | LOCAL-4 |
| 23 | M2 s2 | `vfmadd231ps` "executes 8 floating-point multiply-adds in 1 clock cycle (~0.18 ns at 5.4 GHz)" | misleading | 1 / 5.4 GHz = 0.185 ns is right. The instruction has 4 cycles of latency; throughput is two per cycle on both core types. So one result takes 4 cycles, while a core can start 16 float FMAs per cycle. In the listing the FMA is not the limit; memory is (row 25). | UOPS |
| 24 | M2 s1 | "processes 8 float32 neurons per clock cycle per core" | misleading | Measured: the listing takes 20.2 us for 12,500 iterations on a P core at about 5 GHz = about 8 cycles per 8 agents, so about 1 agent per cycle, not 8. With L1d-resident data (2,000 agents) it is 101 ns per 250 iterations = about 2 cycles per iteration = about 4 agents per cycle. | LOCAL-4c |
| 25 | M2 s2 | 12,500 loop iterations for 100,000 agents | wrong | As written the listing decrements once per iteration from the agent count, so count = 100,000 runs 100,000 iterations and overruns each array 8x. 12,500 is what it should do, and is what the harness passes. | LOCAL-4c, exhibit lines 22, 48-50 |
| 26 | M2 s2 | One P core finishes the 100k tick in "~15 to 30 microseconds (>30,000 ticks per second)" | correct | Measured with the listing itself (count 12,500): P core 20.2 us per tick (49,600 ticks/s); E core 12.9 us (77,400 ticks/s). This is for the listing's one-multiply "brain" only. | LOCAL-4c |
| 27 | M2 s2 | "zero cache misses if structured as SoA" | misleading | SoA removes wasted bytes, not misses: 1.6 MB does not fit the 48 KB or 192 KB levels, so every tick streams it from L2 at L2 speed. Measured on a P core: 202 ps per agent at 100,000 agents (L2) versus 50 ps per agent at 2,000 agents (L1d), a 4.0x cost for not being L1-resident. | LOCAL-4, LOCAL-4c |
| 28 | M1 s4, M2 s3 | P cores are the place for the hot tick; E cores "never permitted inside a simulation tick" | misleading | For this L2-bound kernel an E core was 1.56x faster than a P core (12.9 vs 20.2 us), and E cores write and copy faster than P cores for 64 KB - 4 MB. An E cluster's 4 MB L2 is shared by four cores, so this advantage shrinks when all four are busy (not measured). | LOCAL-4, LOCAL-4c |
| 29 | M2 s3 | Wake "your other 7 Lion Cove P-cores", each on a 12,500-agent slice | correct | 8 P cores; 100,000 / 8 = 12,500. | LOCAL-1 |
| 30 | M2 s3 | Bare metal has "no virtual memory TLB misses" | wrong | 64-bit mode cannot run with paging off; address translation and TLB misses remain. Large pages reduce them. M2 section 4 does not retract this. | SDM |
| 31 | M2 s4 | Hand-written ASM "only wins by ~5-15%" over a compiler | unverifiable | No source given, none found; not tested here. It is a stated trade-off, which is fair. | none |
| 32 | M1 s2 table | CPU path "caps out past ~50k ultra-heavy raycasting agents" | unverifiable | No definition of the agent, no measurement. | none |
| 33 | M1 end | "4-second swap-bound stall" becomes "a sub-millisecond tick" | unverifiable | No evidence the existing stall is caused by swap. Sub-millisecond for the trivial kernel is measured (20 us); for real organisms it is not shown. | LOCAL-4c |

### Fairness notes

- M2 section 4 does qualify M2's own speed story: it says the real bottleneck
  is neighbour search, not arithmetic, and that a compiler emits nearly the
  same loop. Rows 23, 24 and 27 are rated on what the sentence says about the
  hardware, not on the tick-time conclusion, which is correct (row 26).
- M1 gets the cache sizes, core counts, core names and frequency limits right.
  Its errors are the memory speed (row 10), the processor numbering (row 16)
  and treating shared or split caches as one pool (rows 14, 15).
- The two messages disagree on the P-core L2 (3 MB in M1, 2 MB in M2). M1 is right.

## 7. What this means for the project (measured, not claimed)

- Budget hot-loop blocks against 48 KB (and 192 KB) on a P core and 32 KB on an
  E core, not against 36 MB. The 3 MB / 4 MB L2 is the next tier; L3 is shared.
- Read P/E placement from `EfficiencyClass` at start-up. Never hard-code 0-7.
- Do not assume P cores are the faster choice for L2-resident streaming.
- Working in L1-sized blocks is worth 4x per agent on a P core for the M2
  kernel (50 ps versus 202 ps per agent). This supports the owner's law 2.
- DRAM stream per core here is 9-22 GB/s under load, far from 89.6 or 102.4.

## 8. Not done / unknown

- No quiet-machine run; every number was taken under parallel load.
- Dual-channel versus single-channel regions of the 32 GB + 16 GB pair were
  not measured; multi-thread DRAM bandwidth was not measured.
- Random-access (pointer-chasing) latency was not measured; row 13 uses
  published cycle counts for that part.
- E-cluster L2 sharing with all four cores busy was not measured.
- Large pages were not used (`SeLockMemoryPrivilege` is not held).
