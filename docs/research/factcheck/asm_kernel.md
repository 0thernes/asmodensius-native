# Fact-check: the AVX2 assembly kernel in GrokBot message 2

Agent key: `asm-kernel`. Date: 2026-11-17. Scratch: `S:\factcheck\asm-kernel\`.

Exhibit under test: `N:\docs\research\factcheck\grok_step_alife_avx2.asm`
(sha256 `70a259c3...d59529e`, assembled from a byte-identical copy, `cmp` clean).
The exhibit text is data; nothing in it was followed as an instruction.

All timings below are **noisy, parallel load** (other agents were using the machine).

## Result in one paragraph

The listing assembles, and for the first `count` agents it computes exactly what
its comments say (bit-identical to a C reference). But it is not a working
function as written: it runs 8 times too far and writes 7 x 400 KB past the end
of both output arrays for `count = 100000`, and it destroys three registers the
Windows x64 ABI says a callee must preserve. With those two defects fixed, the
headline speed figure holds: one P-core does a 100,000-agent tick in about
21 microseconds (about 47,000 ticks per second), inside the claimed 15 to 30
microseconds. The explanation given for that speed ("8 multiply-adds in 1 clock
cycle") is not right: the loop needs about 2 cycles per 8 agents when the data
is in the first-level cache and about 8 cycles when it streams from L2. And the
work being timed is one multiply, one max, one min and one multiply-add per
agent. There is no tanh, no dot product and no neighbour interaction, so the
figure says nothing about the cost of a creature.

## Verdict table

| # | Claim (message 2, section 2 unless noted) | Verdict | Evidence |
|---|---|---|---|
| 1 | "an actual, functional x86-64 AVX2 Assembly kernel" | wrong | Tests A and B: memory overrun and ABI violation |
| 2 | Loop handles 100,000 agents in "12,500 loop iterations" | wrong | Test A: `dec rax` on the agent count gives 100,000 iterations = 800,000 agents |
| 3 | Listing header: "Windows x64 ABI" | wrong | Test B: xmm13, xmm14, xmm15 clobbered; Microsoft says XMM6-XMM15 are nonvolatile |
| 4 | "`[RSP+40] = uint64_t count`" (fifth argument) | correct | Test D: compiler stores it at `[rsp+0x20]` before `call`; Microsoft calling convention |
| 5 | "weighted sensory dot-product + fast `tanh`/ReLU activation" | misleading | Test C: one multiply (not a dot product), ReLU and clamp only, no tanh. The listing's own comment says `max(0.0, sense * weight)`, and the slash can be read as "or" |
| 6 | "`vfmadd231ps` executes 8 floating-point multiply-adds in 1 clock cycle" / comment "(1 single clock cycle!)" | misleading | uops.info: latency 4 cycles, throughput 0.5 cycle. Loop measured at 2.0 to 2.1 cycles per 8 agents in L1, llvm-mca predicts 2.04 |
| 7 | Table row 1: "processes 8 float32 neurons per clock cycle per core" | wrong | Same measurement: best case 2 cycles per 8 agents, 8.3 when streaming 1.6 MB from L2 |
| 8 | "~0.18 nanoseconds at 5.4 GHz" | correct | 1 / 5.4e9 = 0.185 ns; Intel lists the 275HX at up to 5.40 GHz. Measured effective clock here 4.9 to 5.2 GHz (estimate) |
| 9 | "a single P-core finishes the entire 100k population tick in ~15 to 30 microseconds (>30,000 ticks per second)" | mostly_correct | Test E: fixed kernel 20.8 to 21.8 microseconds median, about 46,000 to 48,000 ticks/s. True only after the fixes, and only for this one-multiply workload |
| 10 | "4 arrays x 100,000 x 4 bytes = 1.6 MB total" | correct | Arithmetic |
| 11 | "fits entirely inside a single P-core's 2 MB L2 cache" | mostly_correct | It fits, but the P-core L2 is 3 MB, not 2 MB (COMMON.md machine section; message 1 of the same exhibit says 3 MB) |
| 12 | "zero cache misses if structured as SoA" | misleading | 1.6 MB does not fit the 48 KB first-level data cache; the same loop is 4x slower per agent at 100k agents than at 1,024 agents on the P-core. True only in the sense "no DRAM access" |
| 13 | Section 4: Clang `-O3 -march=native -ffast-math` "will emit almost the exact same `vfmadd231ps` loop" | correct | Test F: clang 21.1.8 emits the same vmulps/vmaxps/vminps/vfmadd231ps ymm loop, unrolled by 2, bit-identical results |
| 14 | Section 4: "Hand-written ASM only wins by ~5-15% on tightly tuned inner kernels" | unverifiable | For this kernel the sign is reversed. Test F: compiler output was faster than the hand-written loop on the P-core (15.2 to 16.4 vs 20.8 to 21.8 microseconds). One kernel, noisy; not a general result |
| 15 | Section 4: "The Real ALife Bottleneck Isn't Neural Math - It's O(N^2) Spatial Queries" | mostly_correct | The count is right by arithmetic (100,000^2 = 10^10); the cost of a neighbour search was not measured here. It undercuts claim 9: the kernel has no neighbour term at all (section F of this note) |

Fairness note. Section 4 of the same message does qualify the picture: it says
compilers match this loop and that the real cost is neighbour queries. It does
not qualify the defects in rows 1 to 3, the "1 clock cycle" statement, or the
"entire 100k population tick" wording.

## Sources

- Microsoft, "x64 calling convention":
  https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention
  ("The x64 ABI considers registers RBX, RBP, RDI, RSI, RSP, R12, R13, R14, R15,
  and XMM6-XMM15 nonvolatile. They must be saved and restored by a function
  that uses them." and "The fifth and higher arguments are passed on the stack";
  "The caller must always allocate sufficient space to store four register
  parameters".)
- uops.info, VFMADD231PS (YMM, YMM, YMM):
  https://uops.info/html-instr/VFMADD231PS_YMM_YMM_YMM.html
  (Skylake, Ice Lake, Alder Lake-P, Arrow Lake-P: latency 4 cycles, throughput
  0.50 cycles, 1 uop. Arrow Lake-E: latency 4, throughput 0.50, 2 uops.)
- Intel instruction reference (mirror), VFMADD231PS operation:
  https://www.felixcloutier.com/x86/vfmadd132ps:vfmadd213ps:vfmadd231ps
- NASM manual, section 9.6.1, win32/win64 SECTION defaults ("Any other section
  name is treated by default like .text"): https://www.nasm.us/doc/nasm09.html
- Intel product page, Core Ultra 9 275HX (36M cache, up to 5.40 GHz):
  https://www.intel.com/content/www/us/en/products/sku/242293/intel-core-ultra-9-processor-275hx-36m-cache-up-to-5-40-ghz/specifications.html
- Local measurements: commands and output below.

## Build (ran)

Tools: `NASM version 2.16.03`, `gcc 16.1.0 (MinGW-W64 x86_64-ucrt-posix-seh)`,
`clang / llvm-mca 21.1.8` at `Z:\Tools\LLVM-21\bin`.

```
$ cd /s/factcheck/asm-kernel && sh build.sh
BUILD_OK
$ cmp grok_step_alife_avx2.asm /n/docs/research/factcheck/grok_step_alife_avx2.asm && echo EXHIBIT_IDENTICAL
EXHIBIT_IDENTICAL
```

`build.sh` runs:

```
nasm -f win64 grok_step_alife_avx2.asm -o grok.obj        # exhibit, verbatim
nasm -f win64 step_alife_avx2_fixed.asm -o fixed.obj      # corrected kernel
nasm -f win64 caller.asm -o caller.obj                    # NASM caller + clock probe
gcc -std=gnu2x -O3 -march=native -c step_c.c -o step_c.o
gcc -std=gnu2x -O3 -march=native -ffast-math -DFN=step_alife_c_fast -c step_c.c -o step_c_fast.o
clang --target=x86_64-w64-windows-gnu -O3 -march=native -ffast-math -DFN=step_alife_c_clang -c step_c.c -o step_c_clang.o
clang --target=x86_64-w64-windows-gnu -O3 -march=native -DFN=step_alife_c_clang_strict -c step_c.c -o step_c_clang_strict.o
gcc -std=gnu2x -O2 -Wall -Wextra -c harness.c -o harness.o
gcc -o harness.exe harness.o step_c.o step_c_fast.o step_c_clang.o step_c_clang_strict.o grok.obj fixed.obj caller.obj -lm
```

The exhibit assembles without a message (`nasm exit=0`). One side finding from
`objdump -h grok.obj`: its constants sit in a section flagged as code, because
NASM's win64 output does not know the name `.rodata`:

```
Idx Name          Size      VMA               LMA               File off  Algn
  0 .rodata       00000060  ...                                 00000064  2**5
                  CONTENTS, ALLOC, LOAD, READONLY, CODE
```

Harmless for the arithmetic; the fixed kernel uses `.rdata`.

## Test A. Loop counter: 8 times too far (ran)

The code:

```
mov     rax, [rsp + 40]          ; Load agent count (N = 100,000)
...
add     r10, 32                  ; Advance 8 floats
dec     rax                      ; one decrement per 8 agents
jnz     .loop_8_agents
```

Method: every array is allocated with `8*N + 64` floats. Indices `N` and up
hold sentinels (`vel` = bit pattern 0xDEADBEEF, `pos` = 1000.0). The kernel is
called with `count = N`. A correct kernel leaves every sentinel alone.

```
$ ./harness.exe overrun 100000
EXHIBIT step_alife_avx2        count=100000
  in range  [0,N):   100000 of 100000 vel written
  sentinels [N,8N):  vel overwritten 700000 of 700000, pos overwritten 700000 of 700000
  first overwritten index 100000, last 799999 (8N-1 = 799999); vel[N]=2 pos[N]=1000.031982
  guard     [8N,8N+64): 0 overwritten
  bytes written past the end of each output array: 2800000
FIXED   step_alife_avx2_fixed  count=100000
  in range  [0,N):   100000 of 100000 vel written
  sentinels [N,8N):  vel overwritten 0 of 700000, pos overwritten 0 of 700000
  guard     [8N,8N+64): 0 overwritten
  bytes written past the end of each output array: 0
exit=0
```

Same result with `count = 1024` (7168 of 7168 sentinels overwritten in each
array, guard untouched). So the exhibit processes exactly `8 * count` agents.
With arrays of the documented size ("N floats") a call with `count = 100000`
writes 2.8 MB past the end of `pos_x` and 2.8 MB past the end of `vel_x`, and
reads the same distance past `sense_in` and `weight_w`. That is heap corruption
or an access violation. Not run with exact-size arrays, on purpose.

Also by reading (not run): `count = 0` makes `dec rax` wrap, so the loop would
run 2^64 times.

## Test B. Windows x64 ABI: xmm13, xmm14, xmm15 destroyed (ran)

The exhibit writes `ymm13`, `ymm14`, `ymm15` and has no save or restore.
`caller.asm` (`abi_probe`) is an ABI-correct NASM caller that loads known
values into xmm13/14/15, calls the kernel, and stores what the registers hold
afterwards. It is in NASM so no compiler can move the values elsewhere.

```
$ ./harness.exe abi
EXHIBIT step_alife_avx2
  xmm13 before call = 13      (0x41500000 x4)  after call = 0.016     (0x3C83126F x4)  CLOBBERED
  xmm14 before call = 14      (0x41600000 x4)  after call = 0         (0x00000000 x4)  CLOBBERED
  xmm15 before call = 1234.5  (0x449A5000 x4)  after call = 5         (0x40A00000 x4)  CLOBBERED
FIXED   step_alife_avx2_fixed
  xmm13 before call = 13      (0x41500000 x4)  after call = 13        (0x41500000 x4)  preserved
  xmm14 before call = 14      (0x41600000 x4)  after call = 14        (0x41600000 x4)  preserved
  xmm15 before call = 1234.5  (0x449A5000 x4)  after call = 1234.5    (0x449A5000 x4)  preserved
exit=0
```

The caller's 1234.5 in xmm15 comes back as 5.0 (the clamp constant), 13.0 as
0.016 (dt), 14.0 as 0.0. A C or C++ caller compiled for Windows is entitled to
keep a live float or double in xmm6 to xmm15 across the call, so this produces
silent wrong results in the caller, depending on what the compiler chose to
keep there. The fix costs nothing: xmm0 to xmm5 are volatile and are enough
once the weight load is folded into `vmulps`.

## Test C. "tanh" in the text, ReLU and clamp in the code (ran)

The loop body has exactly four arithmetic instructions: `vmulps`, `vmaxps`
(with zero), `vminps` (with 5.0), `vfmadd231ps` (with dt). No tanh, no
approximation of one, no table.

```
$ ./harness.exe check
x = sense*weight | kernel 'activation' | min(max(x,0),5) | tanh(x)
       -10 |        0 |        0 | -1.000000
        -2 |        0 |        0 | -0.964028
      -0.5 |        0 |        0 | -0.462117
         0 |        0 |        0 |  0.000000
      0.25 |     0.25 |     0.25 |  0.244919
       0.5 |      0.5 |      0.5 |  0.462117
         1 |        1 |        1 |  0.761594
         2 |        2 |        2 |  0.964028
         3 |        3 |        3 |  0.995055
         5 |        5 |        5 |  0.999909
        10 |        5 |        5 |  1.000000
       100 |        5 |        5 |  1.000000
random test, N=4096 (fixed kernel reports 4096 agents processed)
  fixed kernel vel vs min(max(s*w,0),5): 0 bit mismatches
  exhibit vs fixed kernel (pos and vel, first N): 0 bit mismatches
  gcc -O3 C version vs fixed kernel (pos and vel): 0 bit mismatches
  gcc -O3 -ffast-math C version vs fixed kernel (pos and vel): 0 bit mismatches
  clang -O3 -ffast-math C version vs fixed kernel (pos and vel): 0 bit mismatches
  clang -O3 C version vs fixed kernel (pos and vel): 0 bit mismatches
  kernel vs tanh(s*w): 3752 of 4096 differ by more than 0.01, max |diff| = 4.0001
  inputs with s*w < 0 (ReLU gives 0): 2050; with s*w > 5 (clamped): 197
```

The output is `min(max(sense*weight, 0), 5)` bit for bit. It saturates at 5
instead of 1, is one-sided (all negative drive gives 0), and differs from tanh on
92% of random inputs. Fair reading: the prose says "`tanh`/ReLU", which may
mean "tanh or ReLU", and the listing's own comment says
`activation = max(0.0, sense * weight)`. So the listing is honest and the
sentence above it oversells.

"Weighted sensory dot-product" is also loose: each agent has one input and one
weight, so the "dot product" is a single multiply.

## Test D. Stack offset of the fifth argument (ran)

`[rsp + 40]` at function entry is right: 8 bytes return address, 32 bytes
shadow space for RCX, RDX, R8, R9, then the fifth argument.

```
$ gcc -O1 -c callsite.c -o callsite.o && objdump -d -M intel -r callsite.o
0000000000000000 <call_with_five_args>:
   0:	48 83 ec 38          	sub    rsp,0x38
   4:	48 c7 44 24 20 a0 86 	mov    QWORD PTR [rsp+0x20],0x186a0
   b:	01 00
   d:	e8 00 00 00 00       	call   12 <call_with_five_args+0x12>
			e: IMAGE_REL_AMD64_REL32	step_alife_avx2
```

The caller writes 100000 (0x186a0) at `[rsp+0x20]`; `call` pushes 8 bytes, so
the callee sees it at `[rsp+0x28]` = `[rsp+40]`. Test A confirms it in
practice: the exhibit processed exactly `8 * count` agents and left the guard
after index `8N` alone, so it read the right value.

## Test E. Timing (ran; noisy, parallel load)

Method: `harness.exe time p 100000 2000`. The thread is pinned with
`SetThreadAffinityMask` to one logical CPU whose `EfficiencyClass` (from
`GetLogicalProcessorInformationEx`) is the highest (P-core), priority raised,
200 warm-up ticks, then 2000 ticks each timed with a `QueryPerformanceCounter`
pair (resolution 100 ns). Inputs: sense in [-1,1], weight in [-8,8], so about
half the agents hit the ReLU floor and some hit the clamp. The core clock is
estimated from a serial 64-bit IMUL chain (3 cycles per step assumed), so
"cycles" below is an estimate.

P-core, run 1 (run 2 medians agreed within 5%, except gcc fast-math at 14%):

```
cores=24, efficiency classes 0..1, 8 cores in top class; pinned to logical CPU 10 (class 1 = P core), running on CPU 10
estimated core clock from a 64-bit IMUL chain (3 cycles per step assumed): 4.87 GHz
-- working set for N=100000: 1.60 MB in 4 arrays --
fixed asm (step_alife_avx2_fixed)   N=100000  ticks=2000  ns/tick: min  20000.0  median  21200.0  p99  24300.0  max 741100.0  wall-avg  22245.2
   median: 0.212 ns/agent, 1.696 ns per 8-agent block = 8.25 cycles/block at 4.87 GHz; 47170 ticks/s
gcc -O3 -march=native (scalar)      N=100000  ticks=2000  ns/tick: min 344700.0  median 357200.0  p99 534500.0  max 1176600.0 wall-avg 368686.2
gcc -O3 -march=native -ffast-math   N=100000  ticks=2000  ns/tick: min  19500.0  median  23000.0  p99  27500.0  max  29300.0  wall-avg  23189.4
clang -O3 -march=native -ffast-math N=100000  ticks=2000  ns/tick: min  15100.0  median  16400.0  p99  18100.0  max  21600.0  wall-avg  16518.8
clang -O3 -march=native             N=100000  ticks=2000  ns/tick: min  14900.0  median  15700.0  p99  17600.0  max  23500.0  wall-avg  15896.5
fixed asm (again)                   N=100000  ticks=2000  ns/tick: min  20000.0  median  21800.0  p99  24100.0  max  30900.0  wall-avg  21877.0
EXHIBIT as written (does 8N agents) N=100000  ticks=2000  ns/tick: min 258400.0  median 271000.0  p99 362000.0  max 620500.0  wall-avg 280099.8
   median: 0.339 ns/agent, 2.710 ns per 8-agent block = 13.19 cycles/block at 4.87 GHz; 3690 ticks/s
EXHIBIT called with count=N/8       N=12500   ticks=2000  ns/tick: min  20000.0  median  22000.0  p99  24400.0  max  26700.0  wall-avg  22157.0
-- working set for N=1024: 16.4 KB in 4 arrays (fits first-level data cache) --
fixed asm, cache-resident           N=1024    ticks=2000000 ns/tick: min 51.5  median 55.5  p99 62.0  max 67.5  wall-avg 55.8
   median: 0.054 ns/agent, 0.434 ns per 8-agent block = 2.11 cycles/block at 4.87 GHz; 18018018 ticks/s
clang -O3 fast-math, cache-resident N=1024    ticks=2000000 ns/tick: min 50.6  median 51.0  p99 60.3  max 387.7  wall-avg 52.1
estimated core clock after the runs: 5.17 GHz
```

E-core (`harness.exe time e 100000 2000`, logical CPU 4, estimated 3.2 to
3.3 GHz):

```
fixed asm (step_alife_avx2_fixed)   N=100000  ticks=2000  ns/tick: min 12800.0  median 13700.0  p99 14900.0  max 29400.0  wall-avg 13772.6
   median: 0.137 ns/agent, 1.096 ns per 8-agent block = 3.51 cycles/block at 3.20 GHz; 72993 ticks/s
EXHIBIT as written (does 8N agents) N=100000  ticks=2000  ns/tick: min 229700.0  median 278100.0  p99 1394400.0 max 3858300.0 wall-avg 346152
fixed asm, cache-resident           N=1024    ticks=2000000 ns/tick: min 95.2  median 96.2  p99 116.5  max 652.1  wall-avg 97.8
   median: 0.094 ns/agent, 0.752 ns per 8-agent block = 2.41 cycles/block at 3.20 GHz
```

Full outputs: `S:\factcheck\asm-kernel\time_p_run1.txt`, `time_p_run2.txt`,
`time_e_run1.txt`.

What the numbers say:

| Quantity | Claimed | Measured (median, noisy, parallel load) |
|---|---|---|
| 100k-agent tick, fixed kernel, one P-core | 15 to 30 microseconds | 20.8 to 21.8 microseconds |
| Ticks per second, one P-core | more than 30,000 | about 46,000 to 48,000 |
| Same, one E-core | not claimed | 13.7 to 14.1 microseconds (about 71,000 to 73,000 per second) |
| Exhibit exactly as written, `count=100000`, arrays 8x oversized so it cannot corrupt memory | 15 to 30 microseconds | 264 to 271 microseconds (about 3,700 per second), because it processes 800,000 agents and 12.8 MB |
| Cycles per 8-agent block, data in first-level cache | 1 | 2.0 to 2.1 (P-core), 2.4 (E-core) |
| Cycles per 8-agent block, 1.6 MB streamed from L2 | 1 | 8.3 (P-core), 3.5 (E-core) |

So the tick-time claim is right for the corrected loop. The exhibit's own
arithmetic does not support it, though: 12,500 iterations at "1 clock cycle"
and 0.185 ns would be 2.3 microseconds, not 15 to 30. The 15 to 30 figure is
the realistic one and the "1 clock cycle" figure is the wrong one.

An unexpected observation, reported as measured and not explained: at 1.6 MB
the E-core was faster than the P-core for this loop (13.7 vs 21 microseconds),
although the P-core is faster when the data fits the first-level cache
(55 vs 96 ns for 1,024 agents). I did not investigate the cause.

### llvm-mca prediction for the exhibit's loop body (ran)

`loop_mca.s` is the loop body transcribed one instruction for one into GAS
Intel syntax. LLVM 21 names the host `arrowlake-s` and uses its Alder Lake
P-core scheduling model for it (resource names `ADLPPort..`); it has no Lion
Cove model.

```
$ Z:/Tools/LLVM-21/bin/llvm-mca.exe -mtriple=x86_64-pc-windows-gnu -mcpu=arrowlake-s -iterations=1000 loop_mca.s
Iterations:        1000
Instructions:      12000
Total Cycles:      2038
Total uOps:        12000
Dispatch Width:    6
uOps Per Cycle:    5.89
IPC:               5.89
Block RThroughput: 2.0

[1]    [2]    [3]    [4]    [5]    [6]    Instructions:
 1      8     0.33    *                   vmovaps	ymm0, ymmword ptr [r8 + r10]
 1      8     0.33    *                   vmovaps	ymm1, ymmword ptr [r9 + r10]
 1      4     0.50                        vmulps	ymm2, ymm0, ymm1
 1      4     0.50                        vmaxps	ymm2, ymm2, ymm14
 1      4     0.50                        vminps	ymm2, ymm2, ymm15
 1      8     0.33    *                   vmovaps	ymm3, ymmword ptr [rcx + r10]
 1      4     0.50                        vfmadd231ps	ymm3, ymm2, ymm13
 2      12    0.50           *            vmovaps	ymmword ptr [rcx + r10], ymm3
 2      12    0.50           *            vmovaps	ymmword ptr [rdx + r10], ymm2
 0      1     0.00                        add	r10, 32
 0      1     0.00                        dec	rax
 1      1     0.50                        jne	.loop_8_agents
```

([1] uops, [2] latency, [3] reciprocal throughput.) Predicted cycles per
iteration: arrowlake-s and alderlake 2.04, skylake 2.36, znver5 2.52. The
model lists `vfmadd231ps` at latency 4, throughput 0.5, matching uops.info.
llvm-mca assumes every load hits the first-level cache, so its 2.04 should be
compared with the 1,024-agent measurement (2.0 to 2.1 cycles): they agree.

What "1 clock cycle" could mean, and why neither reading holds:
- Latency: the result of one `vfmadd231ps` is ready after 4 cycles, not 1.
- Throughput: a P-core can start two 8-wide FMAs per cycle, so the bare
  instruction is better than "8 per cycle". But the instruction never runs
  alone. This loop is 12 instructions with 3 loads and 2 stores per block, and
  that costs 2 cycles per 8 agents at best.

## Test F. Compiler parity (ran; noisy, parallel load)

Message 2, section 4 says a compiler emits nearly the same loop. Checked with
`step_c.c`, the same arithmetic in 6 lines of C:

- clang 21.1.8 `-O3 -march=native -ffast-math` (the flags the message names):
  emits `vmulps` / `vmaxps` / `vminps` / `vfmadd231ps` on ymm registers,
  unrolled to two blocks per iteration. Bit-identical output. 15.7 to 16.4
  microseconds per 100k tick on the P-core, faster than the hand-written loop
  (20.8 to 21.8).
- clang 21.1.8 `-O3 -march=native` without fast-math: also vectorised,
  15.2 to 15.7 microseconds.
- gcc 16.1.0 `-O3 -march=native -ffast-math`: vectorised, 19.8 to 23.0
  microseconds.
- gcc 16.1.0 `-O3 -march=native` without fast-math: **not vectorised**
  (`-fopt-info-vec-all`: "not vectorized: unsupported control flow in loop").
  It emits a scalar loop with a branch for the ReLU; on random-sign input that
  branch mispredicts and the tick takes 352 to 357 microseconds, 17 times
  slower. So "the compiler does it for you" is true for clang and conditional
  for gcc; check the emitted code.

## Section F. What the kernel computes against what a creature needs

Per agent, per tick, the exhibit computes:

```
thrust = min(max(sense_in[i] * weight_w[i], 0), 5)
pos_x[i] += thrust * 0.016
vel_x[i]  = thrust
```

That is all. Measured against the simplest creature step:

| A creature step needs | In the exhibit |
|---|---|
| Sensing: who and what is near me | Absent. `sense_in` is an input array that something else must fill. Filling it is the neighbour query that section 4 of the same message calls the real bottleneck |
| A brain: several inputs, several weights, at least one hidden stage | One input times one weight. No sum, so not a dot product; no recurrent state; no plasticity |
| A bounded two-sided activation (the text says tanh) | ReLU clamped to [0, 5]. An agent can only push in +x and can never brake or reverse |
| Motion in a world | One axis. No y, no z, no heading, no walls, no collisions |
| Velocity integration | None. `vel_x` is overwritten with the thrust each tick; there is no inertia, drag or mass. The comment "Integrate velocity & position" describes only the position line |
| Interaction with other agents | None. No agent reads any other agent's state. 100,000 agents here are 100,000 independent scalar updates |
| Energy, matter, birth, death | None |
| Determinism under this project's laws (integer or fixed-point state, exact conservation) | float32 state with FMA rounding. Repeatable on one binary, but not the integer state law 5 and law 6 ask for |

So "a single P-core finishes the entire 100k population tick" measures 100,000
multiply-clamp-add updates, about 0.2 ns each. It is a valid memory-bandwidth
and SIMD-throughput probe for this laptop, and that is useful to know: a
4-stream SoA pass over 1.6 MB costs about 14 to 21 microseconds on one core.
It is not evidence about how many creatures the machine can run. The same
message concedes the point in section 4 (neighbour search dominates), which is
why row 9 is "mostly_correct" for the number and the wording around it is
marked misleading.

## Corrected kernel

`N:\docs\research\factcheck\step_alife_avx2_fixed.asm`. Same arithmetic.
Changes: block count = `count / 8`; early return for a zero block count; only
volatile registers (xmm0 to xmm5), so nothing needs saving and it stays a leaf
function; constants in `.rdata`; returns the number of agents processed.
Verified above: no overrun (Test A), xmm13 to xmm15 preserved (Test B),
bit-identical to the reference and to the exhibit on the first N agents
(Test C). Limits kept: `count` must be a multiple of 8 (a remainder is left
untouched), arrays must be 32-byte aligned and must not overlap.

## Not done / unknown

- The exhibit was not run with exact-size arrays (it would corrupt the heap).
- No hardware performance counters were read; "cycles" come from wall time
  times an IMUL-chain clock estimate.
- The E-core-faster-than-P-core result at 1.6 MB comes from one E-core run
  (two measurements inside it) under parallel load and was not investigated.
- Lion Cove has no scheduling model in LLVM 21; the llvm-mca figure is for
  the Alder Lake P-core model.
- Sections 1 and 3 of message 2 (PTX, bare metal, UEFI) and all GPU figures
  were outside this task.

## Files

- `N:\docs\research\factcheck\asm_kernel.md` (this note)
- `N:\docs\research\factcheck\step_alife_avx2_fixed.asm` (corrected kernel)
- `S:\factcheck\asm-kernel\`: `build.sh`, `harness.c`, `caller.asm`,
  `step_c.c`, `callsite.c`, `loop_mca.s`, `harness.exe`, timing logs
