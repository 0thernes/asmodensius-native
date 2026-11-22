# NASM toolchain and the int8 neural kernel (key: nasm-vnni)

Date 2026-11-17. Machine: Intel Core Ultra 9 275HX, Windows 11, as in `docs/agents/COMMON.md`.
Everything is in `N:\docs\research\platform\samples\nasm-vnni\`. Every "ran" below was run
in this session; timings are **noisy, parallel load** (other agents share the machine).

## 1. Short answers

| Question | Answer |
|---|---|
| Does NASM 2.16.03 assemble VEX `vpdpbusd`? | **No.** `{vex} vpdpbusd` is rejected; the bare mnemonic silently emits the EVEX (AVX-512) form. The kernel uses a hand-encoding macro `VPDPBUSD_RM` / `VPDPBUSD_RR` (in `win64.inc`), checked byte for byte against clang. |
| Prologue/epilogue macros | `PROC` / `EPILOGUE` / `ENDPROC` in `win64.inc`. Callee-saved registers survive, stack is 16-aligned, and the macros also emit Windows unwind records, proven with `RtlVirtualUnwind` and a C++ throw. |
| CPUID | AVX2, FMA, BMI1, BMI2, AVX-VNNI all present and OS-enabled; also AVX-VNNI-INT8 and AVX-VNNI-INT16. AVX512F absent. |
| Kernel correctness | 102077 cases (2077 edge + 100000 random) x 6 SIMD variants: 0 mismatches against scalar C. |
| Fastest layout | **"c4": column-blocked, 4 input columns per dword, 8 output rows per 32-byte block, 8 accumulator chains.** 7.1 ns per call on a P core, 12.2 ns on an E core. |
| Morton | `pdep` 3 x 10 bit encode: 10010243 encodes match the scalar bit loop; ~3.0 core cycles per encode in a batch loop. |
| llvm-mca | Predicts 36.4 cycles per call for the recommended kernel; measured ~36.5 on a P core. Its `arrowlake`, `alderlake` and `gracemont` outputs are identical, so it has no separate E-core answer. |

## 2. NASM 2.16.03 and AVX-VNNI (the syntax question)

Ran `nasm -f win64 -DFORM=n probe_vex.asm` and a one-line probe per spelling, then
disassembled with `llvm-objdump -d`:

| Source line | NASM 2.16.03 result |
|---|---|
| `vpdpbusd ymm0,ymm1,ymm2` | assembles as **EVEX** `62 f2 75 28 50 c2` (AVX-512 VNNI form) |
| `{vex} vpdpbusd ymm0,ymm1,ymm2` | `error: instruction not encodable with {vex} prefix` |
| `{vex3}` / `{vex2} vpdpbusd ...` | same error |
| `{vex} vpdpbusds`, `{vex} vpdpwssd`, `{vex} vpdpwssds` | same error (the whole original AVX-VNNI group) |
| `vex vpdpbusd ymm0,ymm1,ymm2` (no braces) | **trap**: assembles, but `vex` is taken as a label and the bytes are EVEX |
| `{vex} vpdpbssd ymm0,ymm1,ymm2` | OK, `c4 e2 77 50 c2` (AVX-VNNI-INT8, signed x signed) |
| `{vex} vpdpbuud ymm0,ymm1,ymm2` | OK, `c4 e2 74 50 c2` (AVX-VNNI-INT8) |
| `{vex} vpmadd52luq`, `cmpbexadd`, `{vex} vaddps` | OK |

So this NASM build knows the newer VEX-only families but not the VEX form of the four
original VNNI instructions. The EVEX form must not run here: CPUID reports AVX512F = 0
(I did not execute an EVEX instruction to watch it fault; the claim rests on CPUID).

Three working routes, all tried:

1. **Hand-encoding macro (used).** `VEX.256.66.0F38.W0 50 /r`, emitted with `db`.
   `VPDPBUSD_RM dst, src_u8, base, disp` and `VPDPBUSD_RR dst, src_u8, src_s8`.
   Check: 24 forms (all 16 ymm numbers, 14 base registers, disp 0 / disp8 / disp32,
   negative displacements, rbp/r13 bases) assembled by NASM and the same 24 written as
   `{vex} vpdpbusd` for clang's assembler; bytes and operands compared.
   Limits: memory operand is `[base + disp]` only, no index, no rsp/r12 base.
2. **`{vex} vpdpbssd` natively in NASM** (kernel `nn_mv32x64_int8_c4x8`). Signed x signed
   gives identical sums because the input contract is 0..127. Needs AVX-VNNI-INT8, which
   this CPU has but older AVX-VNNI CPUs (Alder/Raptor Lake) do not. Same speed as route 1.
3. **Another assembler.** clang 21 accepts `{vex} vpdpbusd` (Intel syntax, `enc_ref.s`);
   MSVC `ml64` 14.44 accepts `vex vpdpbusd` and emits `c4 e2 75 50 c2` (`probe_ml64.asm`).
   yasm 1.3.0 does not know the instruction at all.

```
$ sh build.sh        (section 2 of its output)
== 2. hand-encoded VEX vpdpbusd vs clang's assembler (bytes and operands of 24 forms)
identical: 24 of 24 instructions  PASS
kern.obj: 256 vpdpbusd, 256 of them VEX; EVEX-encoded instructions in the object: 0
```

## 3. Prologue / epilogue macros (`win64.inc`)

```
PROC name, locals, nxmm [, gpr ...]   ; gpr: any of rbx rbp rsi rdi r12..r15; nxmm: xmm6 upward
    ...                               ; rsp is 16-aligned; STK_ARG(5) is the 5th argument
    EPILOGUE                          ; restore + ret
ENDPROC                               ; emits .pdata/.xdata unwind records
```

Alignment rule: at entry rsp = 8 (mod 16); after k pushes it is 8+8k; the frame is
`locals + 16*nxmm + (8 if k is even)`, so rsp ends at 0 (mod 16) and the XMM slots are
aligned for `vmovaps`. rbp is supported as a plain callee-saved register (the task list
omitted it; Win64 requires it). The XMM saves use `vmovaps`, so `nxmm > 0` needs AVX.
Frames of 4096 bytes or more are refused (no stack probe).

What was tested (all inside `test.exe notime`, output below):

- `abi_check(fn)` loads sentinels into rbx, rbp, rsi, rdi, r12..r15 and xmm6..xmm15, calls
  `fn`, and reports which came back changed. Five PROC shapes that destroy every register
  they save: 8 GPR + 10 XMM + locals, 3 GPR (odd) + 1 XMM, 1 GPR only, 2 GPR (even) + 3 XMM,
  0 GPR + 10 XMM. All return mask 0. Each body also records `rsp & 15`: 0 faults.
- Control: `clobber_bad` destroys r12 and xmm9 without saving; the mask is 0x00810 as
  expected, so the checker can see a violation.
- Unwind records: a callback called from inside each PROC body walks the stack with
  `RtlLookupFunctionEntry` + `RtlVirtualUnwind`. After unwinding out of the NASM frame the
  context holds all 18 sentinels although the live registers were destroyed. Covers
  UWOP_PUSH_NONVOL, UWOP_ALLOC_SMALL, UWOP_ALLOC_LARGE and UWOP_SAVE_XMM128.
- Control: assembled with `-DNO_UNWIND` (no records) the same walk fails with flag 0x02.
- `unwind_throw.exe`: a C++ exception thrown from the callback crosses two NASM frames to
  a `catch` in `main`, 1000 times, with main's locals intact.
- `llvm-objdump --unwind-info test.exe` decodes the records (`unwind_info.txt`), e.g. for
  the recommended kernel: `UOP_SaveXMM128 XMM8 [0x20]`, `XMM7 [0x10]`, `XMM6 [0x00]`,
  `UOP_AllocSmall 56`, prolog size 21.

Leaf functions that touch neither the stack nor callee-saved registers (`morton3_pdep`,
`spin_add8`) correctly use no PROC and need no unwind record.

## 4. CPUID detection (`cpu_features`, `cpuid_raw` in `kern.asm`)

Checks OSXSAVE and XCR0 bits 1-2 (via `xgetbv`) before reporting any AVX-class feature;
leaf 1 ECX (AVX, FMA), leaf 7.0 EBX (BMI1, AVX2, BMI2, AVX512F), leaf 7.1 EAX bit 4
(AVX-VNNI), leaf 7.1 EDX bits 4 and 10 (AVX-VNNI-INT8, -INT16). Cross-checked against
gcc's `<cpuid.h>`, `_xgetbv` and `__builtin_cpu_supports`. CPUID leaf 0x1A tells P from E
cores (0x40 / 0x20 in EAX[31:24]); on this machine logical CPU 0 is a P core and CPU 2 an E core.

## 5. The kernel

`y[r] = clamp((b[r] + sum_c W[r][c]*x[c]) >> shift, 0, 127)`, 32 outputs, 64 inputs.
Contract: x bytes 0..127, W signed bytes, |b| <= 2^30, shift 0..31 (arithmetic).
Signature (all variants): `void f(const uint8_t *x, const int8_t *W, const int32_t *b, uint32_t shift, uint8_t *y)`.

Variants in `kern.asm` (all straight-line, no branches):

| Name | Instruction | Weight layout | Chains | Saves |
|---|---|---|---|---|
| `nn_mv32x64_vnni_c4x4` | VEX vpdpbusd | c4 | 4 | none |
| `nn_mv32x64_vnni_c4x8` | VEX vpdpbusd | c4 | 8 | xmm6-8 |
| `nn_mv32x64_vnni_c4x12` | VEX vpdpbusd | c4 | 12 | xmm6-12 |
| `nn_mv32x64_int8_c4x8` | `{vex} vpdpbssd` | c4 | 8 | xmm6-8 |
| `nn_mv32x64_avx2_c4` | vpmaddubsw + vpmaddwd + vpaddd | c4 | 4 | xmm6 |
| `nn_mv32x64_vnni_rm` | VEX vpdpbusd + vphaddd reduction | row-major | per row | xmm6-13 |

Tail shared by all: `vpsrad` by the shift, `vpackssdw` (saturate to s16), `vpminsw` 127,
`vpackuswb` (floor at 0), `vpermd` to undo the lane interleave, one 32-byte store.

Why x must stay in 0..127: `vpmaddubsw` adds two u8*s8 products with signed 16-bit
saturation. With x <= 127 the extremes are 2*127*127 = 32258 and 2*127*(-128) = -32512,
so it never saturates and the AVX2 path is exact. The test shows the failure outside the
contract (x = 255, w = 127: scalar and VNNI give 64, AVX2 gives 0). Since the kernel's own
output is clamped to 0..127, layers can be chained without leaving the contract.

### Correctness run

```
$ sh build.sh        (gcc -std=c2x -O2 -Wall -Wextra test.c kern.obj -o test.exe ; ./test.exe notime ; ...)
cpu: Intel(R) Core(TM) Ultra 9 275HX
cpu_features() = 0x1ff  OSYMM=1 AVX=1 AVX2=1 FMA=1 BMI1=1 BMI2=1 AVX-VNNI=1 VNNI-INT8=1 VNNI-INT16=1 AVX512F=0
cpu: asm mask vs gcc <cpuid.h> + _xgetbv: 0x1ff vs 0x1ff; gcc builtin avx2=1 fma=1 bmi2=1 avxvnni=1  PASS
abi: clobber_all  (8 gpr, 10 xmm, 32 locals)    changed-register mask = 0x00000  PASS
abi: clobber_odd  (3 gpr, 1 xmm, 32 locals)     changed-register mask = 0x00000  PASS
abi: clobber_min  (1 gpr)                       changed-register mask = 0x00000  PASS
abi: clobber_even (2 gpr, 3 xmm, 48 locals)     changed-register mask = 0x00000  PASS
abi: clobber_xmm  (0 gpr, 10 xmm, 32 locals)    changed-register mask = 0x00000  PASS
abi: clobber_bad  (control: trashes r12, xmm9)     changed-register mask = 0x00810  PASS (detected, expected 0x00810)
abi: stack misaligned inside a PROC body: 0 times  PASS
unwind: RtlVirtualUnwind through clobber_all  -> abi_check: flags 0x00, ran 1  PASS (all 8 GPR + 10 XMM recovered)
unwind: RtlVirtualUnwind through clobber_odd  -> abi_check: flags 0x00, ran 1  PASS (all 8 GPR + 10 XMM recovered)
unwind: RtlVirtualUnwind through clobber_even -> abi_check: flags 0x00, ran 1  PASS (all 8 GPR + 10 XMM recovered)
unwind: RtlVirtualUnwind through clobber_xmm  -> abi_check: flags 0x00, ran 1  PASS (all 8 GPR + 10 XMM recovered)
morton3: 10010243 encodes vs scalar bit loop, pext round trip, batch of 4099: 0 mismatches  PASS
kernels: 102077 cases (2077 edge + 100000 random) x 6 variants vs scalar C: 0 mismatches  PASS
  reference output mix: 915362 zero, 1856294 inside 1..126, 494808 at 127
  out-of-contract x=255 w=127: scalar y[0]=64, vnni y[0]=64 (same), avx2 y[0]=0 (differs, expected: vpmaddubsw saturates)
RESULT: ALL PASS
c++ throw through 2 NASM frames: caught 1000 of 1000, locals intact: yes  PASS
== 4. negative controls (each must FAIL)
kernels: 102077 cases (2077 edge + 100000 random) x 6 variants vs scalar C: 81935 mismatches  FAIL
RESULT: FAIL
unwind: RtlVirtualUnwind through clobber_all  -> abi_check: flags 0x02, ran 1  FAIL
...
== done
```

Test content: edge cases include x = 127 with w = -128 (sum -1040384, and the same with a
bias that brings the result back to exactly 0, a ramp 0..124, and a ramp crossing 127),
x = 127 with w = 127, x = 0, +-2^30 biases with shift 31, alternating extremes inside each
byte pair and 4-byte group, and 2048 one-hot cases (one weight, one input, plus a decoy
weight that must not leak) that pin every (row, column) to its packed position. Of the
100000 random inputs, half use a bias derived from the scalar sum so every output lands
inside 0..127 (with shift 0 the byte is the exact sum offset, so a single wrong product
shows); the other half use random bias and shift to exercise both clamps. The control
(`-DMUTATE`, two packed weights swapped) produces 81935 mismatches.
Each kernel is also checked for writing nothing past y[31].

## 6. Timing (RDTSC) and llvm-mca

Method: thread pinned to one logical CPU, `lfence; rdtsc; lfence` around a batch of 512
calls, 4000 batches, minimum / median / 90th percentile per call. "throughput" = independent
calls back to back (how a loop over creatures runs). "latency" = each call's output is the
next call's input (how stacked layers of one creature run). The TSC ticks at a fixed
3072 MHz, not at the core clock, so ticks are converted with a clock ratio measured before
and after each row from a loop of 8 dependent `add reg,reg` (8 core cycles per iteration).
The cycle figures therefore carry that estimate's error (the ratio moved between 1.64 and
1.69 on the P core during one run); ticks and ns do not.

Ran `./test.exe` (the run saved in `run_full.txt`; an earlier run agreed within 1 % in
ticks; two header lines per block are omitted here):

```
== timing on logical cpu 0: P core (Intel Core)   [noisy, parallel load]
   TSC 3071.9 MHz (vs QPC); core clock ~ 1.58 cycles per TSC tick (~4849 MHz, from an 8-cycle add chain)
   kernel       mode         min tick  med tick  p90 tick  cyc/tick  min cycles   min ns
   vnni_c4x4    throughput      24.12     24.30     24.39     1.685       40.63    7.851
   vnni_c4x4    latency         42.52     42.66     45.72     1.665       70.79   13.842
   vnni_c4x8    throughput      21.85     22.04     22.38     1.670       36.49    7.113
   vnni_c4x8    latency         33.22     33.41     34.94     1.686       56.00   10.813
   vnni_c4x12   throughput      21.39     21.59     21.84     1.684       36.02    6.965
   vnni_c4x12   latency         34.53     34.73     34.91     1.657       57.23   11.241
   avx2_c4      throughput      40.47     40.68     40.82     1.679       67.93   13.175
   avx2_c4      latency         46.63     46.89     47.32     1.676       78.15   15.181
   vnni_rm      throughput      28.64     29.05     29.83     1.662       47.61    9.325
   vnni_rm      latency         47.49     47.79     48.06     1.679       79.76   15.460
   int8_c4x8    throughput      21.88     22.03     22.21     1.676       36.65    7.121
   int8_c4x8    latency         33.21     33.43     33.60     1.674       55.59   10.811
   scalar C -O2 throughput     202.12    204.88    206.38     1.651      333.73   65.799
   morton pdep  throughput       1.81      1.84      1.85     1.686        3.05    0.589   per encode, batch of 4096
   morton C     throughput      26.02     26.58     26.88     1.643       42.74    8.469   per encode, C bit loop

== timing on logical cpu 2: E core (Intel Atom)   [noisy, parallel load]
   TSC 3071.9 MHz (vs QPC); core clock ~ 1.49 cycles per TSC tick (~4582 MHz, from an 8-cycle add chain)
   kernel       mode         min tick  med tick  p90 tick  cyc/tick  min cycles   min ns
   vnni_c4x4    throughput      37.31     37.62     37.77     1.492       55.68   12.146
   vnni_c4x4    latency         44.23     44.53     44.66     1.492       65.97   14.399
   vnni_c4x8    throughput      37.62     37.89     37.99     1.490       56.06   12.245
   vnni_c4x8    latency         44.17     44.46     44.57     1.492       65.89   14.379
   vnni_c4x12   throughput      39.09     39.39     39.62     1.492       58.34   12.726
   vnni_c4x12   latency         44.18     44.63     44.78     1.492       65.92   14.380
   avx2_c4      throughput      90.70     91.06     91.22     1.488      134.91   29.524
   avx2_c4      latency         95.62     95.72     95.79     1.492      142.70   31.127
   vnni_rm      throughput      79.02     79.59     79.77     1.493      117.96   25.723
   vnni_rm      latency         89.32     89.69     89.83     1.490      133.05   29.076
   int8_c4x8    throughput      37.64     37.89     37.99     1.491       56.13   12.252
   int8_c4x8    latency         44.05     44.49     44.66     1.492       65.71   14.341
   scalar C -O2 throughput     279.12    282.75    284.00     1.493      416.64   90.863
   morton pdep  throughput       2.03      2.04      2.05     1.492        3.02    0.660   per encode, batch of 4096
   morton C     throughput      32.22     32.48     32.56     1.489       47.99   10.488   per encode, C bit loop
```

Ran `sh mca.sh arrowlake gracemont znver4` (llvm-mca 21.1.8, 1000 iterations of each
function body as disassembled from `kern.obj`; output in `mca_out.txt`, full reports in `mca/`):

```
nn_mv32x64_vnni_c4x4     arrowlake  insns/call 102  uops/call  168.0  cycles/call  41.10  uops/cycle  4.09  IPC  2.48  BlockRThroughput  35.0
nn_mv32x64_vnni_c4x8     arrowlake  insns/call 116  uops/call  185.0  cycles/call  36.41  uops/cycle  5.08  IPC  3.19  BlockRThroughput  35.0
nn_mv32x64_vnni_c4x12    arrowlake  insns/call 132  uops/call  205.0  cycles/call  38.06  uops/cycle  5.39  IPC  3.47  BlockRThroughput  35.0
nn_mv32x64_int8_c4x8     arrowlake  insns/call 116  uops/call  185.0  cycles/call  39.07  uops/cycle  4.73  IPC  2.97  BlockRThroughput  35.0
nn_mv32x64_avx2_c4       arrowlake  insns/call 234  uops/call  301.0  cycles/call  80.04  uops/cycle  3.76  IPC  2.92  BlockRThroughput  68.0
nn_mv32x64_vnni_rm       arrowlake  insns/call 184  uops/call  308.0  cycles/call  75.13  uops/cycle  4.10  IPC  2.45  BlockRThroughput  51.3
morton3_pdep_batch loop  arrowlake  uops/encode 12.0  cycles/encode  3.02  BlockRThroughput  3.0
```

Predicted against measured (P core, throughput, core cycles per call):

| Kernel | llvm-mca `arrowlake` | measured (min) | measured / predicted |
|---|---|---|---|
| vnni_c4x4 | 41.1 | 40.6 | 0.99 |
| vnni_c4x8 | 36.4 | 36.5 | 1.00 |
| vnni_c4x12 | 38.1 | 36.0 | 0.95 |
| int8_c4x8 | 39.1 | 36.7 | 0.94 |
| avx2_c4 | 80.0 | 67.9 | 0.85 |
| vnni_rm | 75.1 | 47.6 | 0.63 |
| morton batch, per encode | 3.02 | 3.05 | 1.01 |

Reading of the numbers:

- llvm-mca is trustworthy here for the recommended kernel (within the clock-ratio error),
  and pessimistic for the shuffle- and multiply-heavy variants. Its `arrowlake`,
  `alderlake` and `gracemont` reports are identical line for line, so LLVM 21 uses one
  P-core model for all three; **there is no llvm-mca prediction for the E core**
  (measured: 56 cycles). `znver4` output differs (48 cycles) and is only in `mca_out.txt`.
- The c4 kernel is at the model's port limit: 64 vpdpbusd plus the tail give
  BlockRThroughput 35 cycles; measured 36.5.
- A latency-mode prediction was attempted with `-noalias=false`; it gave 136..265 cycles
  against a measured ~56, so llvm-mca has no usable figure for the latency rows.

## 7. Recommended weight layout

**c4, with 8 accumulator chains (`nn_mv32x64_vnni_c4x8`).**

```
Wc4[ ((c/4)*4 + r/8)*32 + (r%8)*4 + c%4 ] = W[r][c]        r < 32, c < 64, 2048 bytes
```

Group the 64 inputs into 16 dwords. For input dword g, `vpbroadcastd` copies x[4g..4g+3]
into all 8 dword lanes. Each 32-byte weight block holds, for 8 consecutive output rows,
that row's 4 weights for those 4 inputs, so **one `vpdpbusd ymm_acc, ymm_x, [W]` updates
8 output rows** with the weights as a memory operand. 16 broadcasts + 64 vpdpbusd, read in
strictly increasing address order, no horizontal sums. The bias is the initial accumulator.

Evidence for the choice (min ns per call, from the table above):

| Compared with c4x8 | P core | E core |
|---|---|---|
| row-major + vphaddd (`vnni_rm`) | 9.33 vs 7.11 ns: c4 1.31x faster | 25.72 vs 12.25 ns: c4 2.10x faster |
| AVX2 fallback, same layout | 13.18 vs 7.11 ns: VNNI 1.85x faster | 29.52 vs 12.25 ns: VNNI 2.41x faster |
| scalar C, gcc -O2 | 65.8 vs 7.11 ns: 9.3x | 90.9 vs 12.25 ns: 7.4x |

Chains: with one accumulator per row block (4 chains) each chain is 16 vpdpbusd long and
the P core is latency-bound: 13.84 ns latency mode, 7.85 ns throughput. Splitting each row
block into 2 chains (even/odd input dwords) gives 10.81 and 7.11 ns. 12 chains gain a
further 2 % throughput (6.97 ns) but lose latency (11.24 ns) and save 7 XMM registers
instead of 3. On the E core chains make no difference (12.1 to 12.7 ns). 8 chains is the
best single choice for both core types.

The same layout serves the AVX2 fallback and the `{vex} vpdpbssd` variant unchanged, so
one packed weight image works for every code path.

Derived figure, not measured at scale: 7.1 ns per 32x64 layer is about 140 million layer
evaluations per second per P core, 82 million per E core, when the weights are in L1.

## 8. Morton code

`morton3_pdep(x, y, z)`: three `pdep` with masks 0x09249249, 0x12492492, 0x24924924, two
`or`; bit 3i = x bit i, 3i+1 = y, 3i+2 = z. `morton3_pext_x(code, axis)` decodes one axis.
`morton3_pdep_batch` is the loop form (uses `PROC` with 5 GPRs and a stack argument).
Checked: every 10-bit value on each axis with the other axes all-0 and all-1, 10 million
random triples against the scalar bit loop, pext round trip, bits above 10 ignored,
n = 0 batch is a no-op, no write past the output. ~3.0 core cycles per encode in the batch
loop on both core types (0.59 ns P, 0.66 ns E), 14x to 16x faster than the C bit loop.

## 9. Limits and unknowns

- Timing used one weight set that stays in L1. A population where every creature has its
  own 2 KB of weights was **not** measured; that is a cache-footprint question for the
  world design, not answered here.
- The kernel is fixed at 32x64 and fully unrolled (the c4x8 body is 830 bytes of code).
  Other sizes need new macro instances; a loop form was not written or timed.
- Core-cycle figures depend on the add-chain clock estimate; ns and TSC ticks do not.
- Only logical CPUs 0 (P) and 2 (E) were timed. Under parallel load the minimum is the
  least-disturbed batch; medians were within 2 % of it.
- Sparse inputs (skipping all-zero input dwords; 28 % of the test outputs were 0) were not
  tried; it would add data-dependent branches.
- Out-of-contract inputs (x > 127) are not detected by the kernels; the AVX2 and
  `vpdpbssd` paths then give different answers from vpdpbusd.
- `VPDPBUSD_RM` supports `[base + disp]` only.
- No prior-art search was done and nothing here is claimed as new. From memory (not
  checked in this session): broadcasting 4 input bytes against column-blocked weights and
  the vphaddd row reduction are both established techniques in chess-engine NNUE code.

## 10. Files (`N:\docs\research\platform\samples\nasm-vnni\`)

| File | What |
|---|---|
| `win64.inc` | `PROC` / `EPILOGUE` / `ENDPROC` / `STK_ARG`, `VPDPBUSD_RM` / `VPDPBUSD_RR` |
| `kern.asm` | CPUID, 6 kernels, Morton, ABI self-test functions, clock helper |
| `test.c` | scalar references, all tests, RDTSC timing |
| `unwind_throw.cpp` | C++ exception through NASM frames |
| `enc_check.asm`, `enc_ref.s` | encoding cross-check, NASM macro vs clang |
| `probe_vex.asm`, `probe_ml64.asm`, `probe_yasm.asm` | assembler syntax probes |
| `build.sh` | assemble + build + tests + controls (6.5 s) |
| `mca.sh`, `morton_loop.s` | llvm-mca runs |
| `build_out.txt`, `run_full.txt`, `mca_out.txt`, `unwind_info.txt`, `mca/` | captured outputs |
