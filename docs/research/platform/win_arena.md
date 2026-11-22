# Windows host layer proof: locked arena, topology, pinning, TSC

Agent key: `win-arena`. Date: 2026-11-17. All timings are **noisy, parallel load**
(other agents were using the machine during every run).

- Sample: `N:\docs\research\platform\samples\win-arena\win_arena.c` (one C file, 560 lines)
- Raw outputs of four runs: `run1.txt` .. `run4.txt` in the same folder
  (run1 is an earlier build with five jitter trials; run2-run4 have seven; run4 adds the LFENCE check)

## What was run

```
cd /n/docs/research/platform/samples/win-arena
gcc -O2 -std=c2x -Wall -Wextra win_arena.c -o win_arena.exe     # gcc 16.1.0, no output, exit 0
./win_arena.exe > run4.txt 2>&1                                  # exit 0; 7 x 3 s trials + 1 s calibration
```

The process ran **not elevated** (`elevated=0 elevation_type=3`), normal priority class (0x20).

## Verdict per task item

| Item | Result |
|---|---|
| 1a. 256 MB arena, page-locked | **Works** without admin rights. Order matters: raise the working set first. |
| 1b. Large pages | **Fails** as expected: error 1300 on the privilege, 1314 on the allocation. |
| 2. Topology (P/E, caches, sharing) | **Works.** 8 P at LP {0-1,10-13,22-23}, 16 E at LP {2-9,14-21}. |
| 3. Pin + priority + jitter | **Works** with `SetThreadAffinityMask`. `SetThreadSelectedCpuSets` held on a P core but **did not hold on an E core** (see below). |
| 4. RDTSC vs QPC | **Works.** 3,071,994,576 to 3,071,994,719 Hz over four runs, window spread 0.24-0.70 ppm, invariant TSC bit set. |

## Part 1: page-locked arena (run4, verbatim)

```
== PART 1: page-locked arena ==
  page size=4096 allocation granularity=65536
  token: elevated=0 elevation_type=3 (1=default 2=full 3=limited)
  token holds 5 privileges:
    SeShutdownPrivilege                disabled
    SeChangeNotifyPrivilege            enabled
    SeUndockPrivilege                  disabled
    SeIncreaseWorkingSetPrivilege      disabled
    SeTimeZonePrivilege                disabled
  working set at start: min=200 KB max=1380 KB flags=0xa
  VirtualAlloc(256 MB, RESERVE|COMMIT) ok at 000001C513030000
  control: VirtualLock BEFORE raising working set: FAILED 1453 Insufficient quota to complete the requested service.
  enable SeIncreaseWorkingSetPrivilege: ok (code 0 )
  SetProcessWorkingSetSizeEx(min=288 MB, max=768 MB, HARDWS_MIN_ENABLE|HARDWS_MAX_DISABLE) ok
  working set after raise: min=294912 KB max=786432 KB flags=0x9 HARD_MIN
  VirtualLock(256 MB) ok in 23.6 ms (noisy, parallel load)
  page first  000001C513030000: valid=1 locked=1 largepage=0 node=0
  page middle 000001C51B030000: valid=1 locked=1 largepage=0 node=0
  page last   000001C52302F000: valid=1 locked=1 largepage=0 node=0
  touched 65536 pages in 24.83 ms, page faults during touch=0 (checksum 8355840), working set now 268320 KB
  total alloc+lock path 50.4 ms (noisy, parallel load)
-- large pages --
  GetLargePageMinimum() = 2097152 bytes
  enable SeLockMemoryPrivilege: FAILED (code 1300 Not all privileges or groups referenced are assigned to the caller.)
  VirtualAlloc(256 MB, MEM_LARGE_PAGES) FAILED: 1314 A required privilege is not held by the client.
```

Facts from this:

- The default minimum working set is 200 KB, so `VirtualLock` of the arena fails with
  **1453 ERROR_WORKING_SET_QUOTA** until the minimum is raised. This was the control experiment
  and it failed the same way in all four runs.
- `SeIncreaseWorkingSetPrivilege` is present in the ordinary (limited) token, disabled by default,
  and `AdjustTokenPrivileges` enables it. No elevation needed.
- After `SetProcessWorkingSetSizeEx(arena + 32 MB, arena + 512 MB, HARDWS_MIN_ENABLE | HARDWS_MAX_DISABLE)`
  the lock succeeds. `QueryWorkingSetEx` reports `valid=1 locked=1` for the first, middle and last page,
  and touching all 65,536 pages caused **0 page faults**.
- Lock time for 256 MB was 14.1 to 23.6 ms across the four runs (noisy).
- Not measured: the smallest slack that still works (32 MB was the only value tried), and behaviour
  when physical memory is short.

### Large pages: what the owner would do to enable them

Observed: the token does not hold `SeLockMemoryPrivilege` (error **1300 ERROR_NOT_ALL_ASSIGNED** when
enabling it), and `VirtualAlloc(..., MEM_LARGE_PAGES)` returns **1314 ERROR_PRIVILEGE_NOT_HELD**.
`GetLargePageMinimum()` is 2 MB.

Steps (standard Windows procedure; **not executed here**, no setting was changed):

1. Press Win+R, run `secpol.msc` (Local Security Policy; available on Windows 11 Pro).
2. Security Settings -> Local Policies -> User Rights Assignment -> **Lock pages in memory**.
3. Add User or Group -> enter the owner's account name (or a group it belongs to) -> OK.
4. Sign out and sign in again (or reboot). The privilege is put in the token at logon.
5. Because this account's normal token is a UAC-limited token (`elevation_type=3`, only five
   privileges listed above), expect the privilege to appear only in the elevated token: start the
   program from a terminal opened with "Run as administrator". (Expectation from Windows
   documentation, not tested here.) Check with `whoami /priv`: `SeLockMemoryPrivilege` must be listed.
6. The program then enables the privilege with `AdjustTokenPrivileges` (it is disabled by default
   even when held) and calls `VirtualAlloc(NULL, size, MEM_RESERVE|MEM_COMMIT|MEM_LARGE_PAGES, PAGE_READWRITE)`
   with `size` a multiple of 2 MB. The sample already does both calls, so after the grant the same
   binary reports the result.
7. Large-page memory is never paged, but the allocation can still fail when physical memory is
   fragmented, so allocate at start-up and keep the 4 KB locked path as the fallback.
   (Documented behaviour, not tested here.)

## Part 2: topology (run4; identical in all runs)

```
  records: cores=24 packages=1 numa nodes=1 caches=61 processor groups=1 logical processors=24
  per logical processor:
    lp core eff type cpuset  L1D_KB  L2_KB L2_shared_by      L3_KB L3_shared_by
     0    0   1   P     256      48   3072 {0           }   36864 {0-23}
     1    1   1   P     257      48   3072 {1           }   36864 {0-23}
     2    2   0   E     258      32   4096 {2-5         }   36864 {0-23}
     3    3   0   E     259      32   4096 {2-5         }   36864 {0-23}
     4    4   0   E     260      32   4096 {2-5         }   36864 {0-23}
     5    5   0   E     261      32   4096 {2-5         }   36864 {0-23}
     6    6   0   E     262      32   4096 {6-9         }   36864 {0-23}
     7    7   0   E     263      32   4096 {6-9         }   36864 {0-23}
     8    8   0   E     264      32   4096 {6-9         }   36864 {0-23}
     9    9   0   E     265      32   4096 {6-9         }   36864 {0-23}
    10   10   1   P     266      48   3072 {10          }   36864 {0-23}
    11   11   1   P     267      48   3072 {11          }   36864 {0-23}
    12   12   1   P     268      48   3072 {12          }   36864 {0-23}
    13   13   1   P     269      48   3072 {13          }   36864 {0-23}
    14   14   0   E     270      32   4096 {14-17       }   36864 {0-23}
    15   15   0   E     271      32   4096 {14-17       }   36864 {0-23}
    16   16   0   E     272      32   4096 {14-17       }   36864 {0-23}
    17   17   0   E     273      32   4096 {14-17       }   36864 {0-23}
    18   18   0   E     274      32   4096 {18-21       }   36864 {0-23}
    19   19   0   E     275      32   4096 {18-21       }   36864 {0-23}
    20   20   0   E     276      32   4096 {18-21       }   36864 {0-23}
    21   21   0   E     277      32   4096 {18-21       }   36864 {0-23}
    22   22   1   P     278      48   3072 {22          }   36864 {0-23}
    23   23   1   P     279      48   3072 {23          }   36864 {0-23}
  P cores (EfficiencyClass 1): LP {0-1,10-13,22-23}  count=8
  E cores (lower class):       LP {2-9,14-21}  count=16
```

Cache records reported by `GetLogicalProcessorInformationEx(RelationCache)`:

| Core type | L1D | L1I | L2 | L3 |
|---|---|---|---|---|
| P (8) | 48 KB, 12-way, private | 64 KB, 16-way | 3072 KB, 12-way, private | 36864 KB, 12-way, shared by LP 0-23 |
| E (16) | 32 KB, 8-way, private | 64 KB, 8-way | 4096 KB, 16-way, shared by a cluster of 4 | same L3 |

E-core L2 clusters: {2-5}, {6-9}, {14-17}, {18-21}. Line size 64 bytes everywhere.
This matches COMMON.md (8 x 3 MB + 4 x 4 MB = 40 MB L2, 36 MB L3).

`GetSystemCpuSetInformation`: CPU set ids are 256 + LP index (256..279), one group, one NUMA node,
one last-level-cache index, `EfficiencyClass` 1 for P and 0 for E (agrees with the core records),
nothing parked or allocated. `SchedulingClass` is 2 for LP 0 and LP 13, 1 for the other six P cores,
0 for all E cores. (Unverified reading: the two class-2 cores are the firmware's favoured cores.)

Consequences for the simulation:

- **P cores are not LP 0..7.** The ids are {0,1,10,11,12,13,22,23}. Never derive core type from the index.
- **L1D differs by core type: 48 KB on P, 32 KB on E.** An "L1-sized block" must be sized per core
  type, or to the 32 KB of the E cores if a block may run on either.
- The four E cores of a cluster share one 4 MB L2; treat a cluster as one L2 domain.

## Part 4: RDTSC against QueryPerformanceCounter (run4, verbatim)

```
== PART 4: RDTSC vs QueryPerformanceCounter ==
  QueryPerformanceFrequency = 10000000 Hz
  CPUID 80000007h EDX bit 8 (invariant TSC) = 1
  CPUID 15h: denominator=2 numerator=160 crystal_hz=38400000  => nominal TSC 3072000000.0 Hz
  CPUID 16h: base=0 MHz max=0 MHz bus=0 MHz
    window 0: 210.839 ms  tsc=3071994041.9 Hz  (qpc read bracket 52 / 58 ticks)
    window 1: 203.553 ms  tsc=3071995383.0 Hz  (qpc read bracket 58 / 62 ticks)
    window 2: 201.999 ms  tsc=3071994682.2 Hz  (qpc read bracket 62 / 66 ticks)
    window 3: 202.632 ms  tsc=3071994929.7 Hz  (qpc read bracket 66 / 58 ticks)
    window 4: 200.996 ms  tsc=3071994378.0 Hz  (qpc read bracket 58 / 64 ticks)
    per-window mean 3071994683.0 Hz, min 3071994041.9, max 3071995383.0, spread 0.44 ppm
    whole span (5 x 200 ms): 3071994678.9 Hz = 0.325521 ns per tick
  cost per call (noisy, parallel load): QueryPerformanceCounter 15.1 ns, RDTSC 8.0 ns (sink 1)
```

| Run | Whole-span TSC Hz | Window spread |
|---|---|---|
| run1 | 3,071,994,714.5 | 0.42 ppm |
| run2 | 3,071,994,719.3 | 0.70 ppm |
| run3 | 3,071,994,575.8 | 0.24 ppm |
| run4 | 3,071,994,678.9 | 0.44 ppm |

- Measured rate is 1.7 ppm below the CPUID 15h nominal 3,072,000,000 Hz. 1 tick = 0.325521 ns.
- Method: each sample is a QPC read bracketed by two RDTSC reads, best of 32 (bracket 52-66 ticks, about 20 ns).
- Caveat: QPC at 10 MHz on this OS is probably itself derived from the TSC, so this is a
  consistency check of two views of one clock, not a comparison with an independent clock. Unknown here.
- CPUID 16h returns zeros on this CPU; do not rely on it.

## Part 3: jitter of a 3 s RDTSC spin loop (run4, verbatim)

The loop reads RDTSC back to back and records every gap of 1 us or more. P core = LP 23, E core = LP 21.
"TIME_CRITICAL" is `THREAD_PRIORITY_TIME_CRITICAL` inside the normal priority class (base priority 15).

```
== PART 3: RDTSC spin-loop jitter, 3 s per trial (noisy, parallel load) ==
  process priority class 0x20; chosen P core LP 23 (cpuset id 279), E core LP 21 (cpuset id 277)
  [A unpinned, THREAD_PRIORITY_NORMAL]
    pin: none (err 0)  priority set err 0, readback 0  ran on LP {0-2,4-23}  migrations=3356
    iterations=364099710 in 3.000 s  mean gap=8.24 ns  min gap=2.60 ns (8 ticks)
    gaps: [1,10)us=5796 [10,100)us=2704 [100us,1ms)=186 [1,10)ms=5 >=10ms=0  time in gaps>=1us: 149.694 ms (4.9898 %)
    largest gaps (us @ s): 5969.3@2.63 3013.2@0.28 1716.4@1.58 1359.6@2.33 1127.2@1.64 959.8@1.94 896.6@1.23 814.2@1.65
  [B unpinned, THREAD_PRIORITY_TIME_CRITICAL]
    pin: none (err 0)  priority set err 0, readback 15  ran on LP {0-6,8-15,20,22-23}  migrations=252
    iterations=397759576 in 3.000 s  mean gap=7.54 ns  min gap=0.65 ns (2 ticks)
    gaps: [1,10)us=3542 [10,100)us=2222 [100us,1ms)=143 [1,10)ms=0 >=10ms=0  time in gaps>=1us: 138.368 ms (4.6123 %)
    largest gaps (us @ s): 961.9@2.88 881.2@0.65 726.5@0.25 717.6@1.83 678.8@0.34 637.5@1.88 619.0@2.35 597.6@0.92
  [C P core via SetThreadSelectedCpuSets, TIME_CRITICAL]
    pin: SetThreadSelectedCpuSets (err 0)  priority set err 0, readback 15  ran on LP {23}  migrations=0
    iterations=421228396 in 3.000 s  mean gap=7.12 ns  min gap=5.86 ns (18 ticks)
    gaps: [1,10)us=2095 [10,100)us=18 [100us,1ms)=3 [1,10)ms=1 >=10ms=0  time in gaps>=1us: 9.288 ms (0.3096 %)
    largest gaps (us @ s): 3393.3@2.99 139.7@0.14 138.2@0.02 100.2@0.00 88.4@1.78 71.2@2.29 27.8@0.33 20.5@0.33
  [D P core via SetThreadAffinityMask, TIME_CRITICAL]
    pin: SetThreadAffinityMask (err 0)  priority set err 0, readback 15  ran on LP {23}  migrations=0
    iterations=412843527 in 3.000 s  mean gap=7.27 ns  min gap=5.86 ns (18 ticks)
    gaps: [1,10)us=2118 [10,100)us=23 [100us,1ms)=10 [1,10)ms=0 >=10ms=0  time in gaps>=1us: 9.741 ms (0.3247 %)
    largest gaps (us @ s): 879.4@2.64 809.6@2.68 511.3@2.75 490.7@2.77 375.3@2.74 256.9@1.44 181.3@1.99 177.6@1.34
  [E E core via SetThreadSelectedCpuSets, TIME_CRITICAL]
    pin: SetThreadSelectedCpuSets (err 0)  priority set err 0, readback 15  ran on LP {0-1,10-13,21-22}  migrations=88
    iterations=432160210 in 3.000 s  mean gap=6.94 ns  min gap=0.65 ns (2 ticks)
    gaps: [1,10)us=3059 [10,100)us=487 [100us,1ms)=131 [1,10)ms=0 >=10ms=0  time in gaps>=1us: 64.131 ms (2.1377 %)
    largest gaps (us @ s): 846.9@2.40 701.6@1.66 675.3@0.11 629.8@1.44 624.0@1.85 616.0@2.68 602.4@1.34 597.3@0.72
  [F E core via SetThreadSelectedCpuSets, THREAD_PRIORITY_NORMAL]
    pin: SetThreadSelectedCpuSets (err 0)  priority set err 0, readback 0  ran on LP {0-1,10-13,21-23}  migrations=2953
    iterations=404623060 in 3.000 s  mean gap=7.41 ns  min gap=0.65 ns (2 ticks)
    gaps: [1,10)us=4046 [10,100)us=2207 [100us,1ms)=345 [1,10)ms=11 >=10ms=0  time in gaps>=1us: 205.073 ms (6.8358 %)
    largest gaps (us @ s): 4746.8@0.66 3816.4@2.14 2645.5@1.56 1805.5@1.58 1574.3@1.56 1505.6@0.59 1469.3@1.60 1122.1@1.62
  [G E core via SetThreadAffinityMask, TIME_CRITICAL]
    pin: SetThreadAffinityMask (err 0)  priority set err 0, readback 15  ran on LP {21}  migrations=0
    iterations=455866585 in 3.000 s  mean gap=6.58 ns  min gap=0.65 ns (2 ticks)
    gaps: [1,10)us=2067 [10,100)us=91 [100us,1ms)=51 [1,10)ms=0 >=10ms=0  time in gaps>=1us: 19.008 ms (0.6336 %)
    largest gaps (us @ s): 767.0@2.48 661.8@1.75 632.2@0.74 429.4@2.76 241.9@2.18 229.9@1.58 217.2@0.82 210.4@0.09
  back-to-back RDTSC delta over 4,000,000 reads (hard affinity):
    P core LP 23 (on LP 23): plain min=18 ticks (zero deltas=0), with LFENCE min=32 ticks (10.42 ns)
    E core LP 21 (on LP 21): plain min=2 ticks (zero deltas=0), with LFENCE min=10 ticks (3.26 ns)
```

### Across the four runs (largest gap in us / share of the 3 s lost in gaps of 1 us or more / migrations)

| Trial | run1 | run2 | run3 | run4 |
|---|---|---|---|---|
| A unpinned, normal | 25504 / 6.93 % / 3353 | 2930 / 2.99 % / 1420 | 18504 / 7.31 % / 3493 | 5969 / 4.99 % / 3356 |
| B unpinned, TIME_CRITICAL | 800 / 2.94 % / 247 | 2619 / 3.41 % / 269 | 779 / 3.64 % / 248 | 962 / 4.61 % / 252 |
| C P core, CPU set, TIME_CRITICAL | 9768 / 0.84 % / 0 | 3819 / 0.34 % / 0 | 161 / 0.18 % / 0 | 3393 / 0.31 % / 0 |
| D P core, affinity mask, TIME_CRITICAL | 89 / 0.16 % / 0 | 210 / 0.22 % / 0 | 86 / 0.19 % / 0 | 879 / 0.32 % / 0 |
| E E core, CPU set, TIME_CRITICAL | 511 / 1.47 % / 179 | 1212 / 2.36 % / 153 | 1633 / 1.69 % / 7 | 847 / 2.14 % / 88 |
| F E core, CPU set, normal | - | 5453 / 2.70 % / 204 | 3806 / 5.47 % / 2817 | 4747 / 6.84 % / 2953 |
| G E core, affinity mask, TIME_CRITICAL | - | 758 / 0.87 % / 0 | 412 / 0.67 % / 0 | 767 / 0.63 % / 0 |

What the numbers support:

1. **Pinning to a P core plus TIME_CRITICAL cut lost time from 2.9-7.3 % (unpinned) to 0.16-0.84 %**
   and migrations from 250-3500 per 3 s to 0. Four runs each, loaded machine.
2. **Pinning does not remove millisecond stalls.** A pinned P-core thread still saw a gap of
   3.4 to 9.8 ms in three of the four CPU-set runs (trial C) and 0.9 ms in one affinity run (D).
   The simulation must not assume a hard real-time step on this OS.
3. **`SetThreadSelectedCpuSets` is not a hard pin.** Asked for the single E-core id 277 (LP 21),
   the call returned success, yet in all seven such trials (E and F, runs 1-4) the thread ran on
   LP 21 **and on P cores** ({0-1,10-13,21-23} in most), migrating 7 to 2953 times.
   Asked for the P-core id 279 it stayed on LP 23 in all four runs.
   `SetThreadAffinityMask` held on both core types in all runs (trials D and G: one LP, 0 migrations).
   Why the scheduler leaves the E-core set is not established here (guess: it prefers P cores for a
   busy foreground thread and treats the CPU set as a preference).
4. With only four runs the difference between C and D (CPU set vs affinity mask on a P core) is
   **not established**; the three worst outliers happened to be in C.
5. A floor of about 2,100 gaps of 1-10 us per 3 s (2078-2122 in every pinned P-core trial, about
   700 per second) remains under hard pinning. Source unknown (interrupts are the obvious candidate).
6. Plain RDTSC deltas are not a cost measure: on the E core two consecutive reads were 2 ticks apart
   (0.65 ns), with LFENCE 10 ticks; on the P core 18 ticks plain, 32 with LFENCE. Use
   `LFENCE; RDTSC` when timing short code. Mean loop iteration was 6.6-8.2 ns everywhere.

Not tried (deliberately): `REALTIME_PRIORITY_CLASS` (could starve the other agents and system
threads), `HIGH_PRIORITY_CLASS`, disabling power throttling per thread, MMCSS, interrupt steering.

## Recommended host-layer API

Names and behaviour only; the sample contains working code for each call marked "exercised".
The host layer affects timing only. It must never change simulation state (law 6).

```c
// ---- privileges -----------------------------------------------------------
// Enable a privilege in the process token. 0 = enabled; otherwise the Win32
// error (1300 = the token does not hold it). Exercised: host_enable_privilege.
uint32_t host_privilege_enable(const char *name);

// ---- arena ----------------------------------------------------------------
enum { HOST_ARENA_LOCKED = 1, HOST_ARENA_LARGE = 2, HOST_ARENA_ALLOW_UNLOCKED = 4 };
typedef struct {
    void    *base;
    size_t   bytes;        // rounded up to page_bytes
    size_t   page_bytes;   // 4096, or 2 MB when large pages were granted
    uint32_t flags;        // what was actually achieved (LOCKED, LARGE)
    uint32_t os_error;     // last Win32 error of the step that failed, 0 if none
    uint32_t large_error;  // why large pages were not used (1300 / 1314 / 1450), 0 if used
} host_arena_t;

// One call at start-up, before any worker thread exists.
// 1. If want & HOST_ARENA_LARGE: host_privilege_enable("SeLockMemoryPrivilege"), then
//    VirtualAlloc(MEM_RESERVE|MEM_COMMIT|MEM_LARGE_PAGES), size rounded to GetLargePageMinimum().
//    On failure record large_error and fall through. Never fatal.
// 2. Otherwise: host_privilege_enable("SeIncreaseWorkingSetPrivilege");
//    read GetProcessWorkingSetSizeEx; SetProcessWorkingSetSizeEx(min = current_min + bytes + slack,
//    max = min + headroom, QUOTA_LIMITS_HARDWS_MIN_ENABLE | QUOTA_LIMITS_HARDWS_MAX_DISABLE);
//    VirtualAlloc(MEM_RESERVE|MEM_COMMIT); VirtualLock.   (slack 32 MB, headroom 480 MB were used here)
// 3. If the lock fails the call FAILS (law 2: never run from pageable memory) unless
//    HOST_ARENA_ALLOW_UNLOCKED is passed. Returns 0 on success.
// Exercised for path 2 (success) and path 1 (failure 1300 / 1314).
int  host_arena_create(host_arena_t *a, size_t bytes, uint32_t want);

// Sample every `stride`-th page with QueryWorkingSetEx; returns the number of sampled
// pages that are not (valid && locked). 0 = proof the arena is resident. Exercised (3 pages).
size_t host_arena_verify(const host_arena_t *a, size_t stride);

void host_arena_destroy(host_arena_t *a);          // VirtualUnlock + VirtualFree(MEM_RELEASE)

// ---- topology -------------------------------------------------------------
typedef struct {
    uint16_t lp;            // logical processor index in group 0
    uint16_t core;          // physical core index
    uint8_t  eff_class;     // EfficiencyClass as reported
    uint8_t  is_p;          // eff_class == highest class on the machine
    uint8_t  sched_class;   // SchedulingClass from the CPU set record
    uint32_t cpuset_id;     // for SetThreadSelectedCpuSets
    uint32_t l1d_bytes, l2_bytes, l3_bytes;
    uint64_t l2_share, l3_share;   // LP masks sharing that cache
} host_cpu_t;
typedef struct {
    uint32_t   n_lp, n_p, n_e, n_groups;
    uint64_t   p_mask, e_mask;
    uint32_t   min_l1d_bytes;      // 32 KB here: the safe hot-block size for any core
    host_cpu_t cpu[64];
} host_topology_t;

// GetLogicalProcessorInformationEx(RelationAll) + GetSystemCpuSetInformation.
// Fails (returns nonzero) if n_groups > 1 rather than guessing. Exercised: host_topology.
int host_topology_query(host_topology_t *t);

// ---- threads --------------------------------------------------------------
enum { HOST_PIN_HARD = 0, HOST_PIN_SOFT = 1 };
// HARD = SetThreadAffinityMask(1 << lp): the only mode that held on both core types here.
// SOFT = SetThreadSelectedCpuSets(cpuset_id): a preference; it did NOT keep a thread on an
//        E core in these runs. Use HARD for simulation workers.
// Applies to the calling thread; returns 0 or the Win32 error. Exercised: host_pin_thread.
uint32_t host_thread_pin(const host_topology_t *t, uint32_t lp, int mode);
uint32_t host_thread_unpin(void);

enum { HOST_PRIO_NORMAL = 0, HOST_PRIO_HIGH = 1, HOST_PRIO_CRITICAL = 2 };
// CRITICAL = THREAD_PRIORITY_TIME_CRITICAL in the normal class (exercised). The layer does not
// offer REALTIME_PRIORITY_CLASS.
uint32_t host_thread_priority(int level);

// Convenience for the worker pool: workers 0..n_p-1 go on the P cores in LP order, the rest
// on E cores cluster by cluster, each hard-pinned. Worker index -> LP is a pure function of the
// topology, so placement is reproducible. (Proposed, not exercised.)
uint32_t host_worker_lp(const host_topology_t *t, uint32_t worker_index);

// ---- clock ----------------------------------------------------------------
typedef struct {
    double   tsc_hz;        // measured against QueryPerformanceCounter
    double   ns_per_tick;
    double   spread_ppm;    // (max - min) of the per-window rates
    uint32_t invariant;     // CPUID 80000007h EDX bit 8
    double   nominal_hz;    // from CPUID 15h, 0 if the leaf is empty
} host_clock_t;

// windows x window_ms sleeps, each end sampled as best-of-32 RDTSC-bracketed QPC reads.
// 5 x 200 ms gave 0.24-0.70 ppm spread here. Returns nonzero if invariant == 0 or
// spread_ppm > 50 (then callers must use host_clock_qpc_ns). Exercised: host_tsc_calibrate.
int host_clock_calibrate(host_clock_t *c, uint32_t windows, uint32_t window_ms);

static inline uint64_t host_clock_ticks(void);          // RDTSC, for coarse spans
static inline uint64_t host_clock_ticks_serial(void);   // LFENCE; RDTSC, for short code
uint64_t host_clock_qpc_ns(void);                        // QueryPerformanceCounter fallback

// Timing is for profiling and frame pacing only. It must never feed simulation state.
```

Start-up order that this sample proved: `host_arena_create` -> `host_topology_query` ->
`host_clock_calibrate` -> create workers, each calling `host_thread_pin(HARD)` then
`host_thread_priority`.

## Limits of this evidence

- One machine, one boot, four runs within about four minutes, under parallel load from other agents.
- Large pages, elevated runs and the UAC behaviour in step 5 were not tested (no setting was changed).
- Single processor group assumed (true here: 24 LPs, 1 group). The sample reads group 0 masks only.
- No independent clock was available to check QPC itself.
