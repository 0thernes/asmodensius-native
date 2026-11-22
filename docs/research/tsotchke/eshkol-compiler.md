# Tsotchke audit: eshkol-compiler (the installed Eshkol compiler as a tool)

Audit date 2026-11-17. Scope: the installed compiler at `Z:\Tools\eshkol`
(`current` -> `eshkol-v1.3.6-evolve-windows-x64-cuda`) and the docs in
`T:\Eshkol\eshkol_repo`. The C/C++ runtime kernels of the same repo are covered
by the sibling note `eshkol-runtime.md`; this note answers one question: can the
compiler be used here, today, as a build-time or Tier 2 language, and can C and
compiled Eshkol call each other.

Everything marked "ran" was run in this session from `S:\tsotchke\eshkol-compiler\`.
Nothing under `Z:\Tools\eshkol` or `T:` was modified. All timings are
**noisy, parallel load**.

## Short answer

| Question | Answer (ran) |
|---|---|
| Integer loop -> native Windows exe | Yes. Compiles in about 1.1 to 1.3 s; results equal an independent C reference. |
| Automatic differentiation example -> native exe | Yes. Compiles in about 1.6 s; four derivative/gradient values correct. |
| Relocatable object file | Yes: `-c` gives a COFF `pe-x86-64` object (about 0.2 MB) in about 0.9 s. It is not C-callable (internal calling convention, 94 undefined runtime symbols). |
| C calls compiled Eshkol | Not as shipped: `--shared-lib` writes a DLL with an **empty export table**. Works after adding `/EXPORT:` flags at link time through a per-process wrapper (no install change). |
| Eshkol calls C | Yes: `(extern i64 name i64)` plus a gcc-built object on the command line. Trap: `int` is 32-bit and silently truncates. |
| Runtime DLL dependencies | `cudart64_12.dll` (hard load-time import), `ntdll.dll`, `KERNEL32.dll`. Without the CUDA folder on PATH the exe does not start. |
| Determinism | Object file byte-identical across builds; exe differs in 1 byte (PE time stamp). Output identical across runs. Integers exact. Floats are compiled for the host CPU with FMA, so not bit-portable. |
| Speed of integer loops | About 165 to 180 ns per iteration against 2.2 ns in C (roughly 75 to 80 times slower). |
| Verdict | **Tooling only.** Useful as a build-time oracle (AD gradients, exact integers). Not a Tier 2 language: owner law 1 allows Lisp only as tooling, and the measurements agree. |

## Repo facts

| Item | Value |
|---|---|
| Docs repo | `T:\Eshkol\eshkol_repo`, HEAD `eb224260_2026-11-02` (`git -C T:/Eshkol/eshkol_repo log -1 --format=%h_%cs`) |
| License | MIT License, "Copyright (c) 2025 tsotchke" (`LICENSE`, same text in the install) |
| Main language | Eshkol (a Scheme dialect) compiled through LLVM; compiler and runtime in C++/C. 4860 tracked files: 2503 `.esk`, 269 `.cpp`, 163 `.h` |
| Size | Repo 314 MB on disk. Install 325 MB: `bin\eshkol-run.exe` 90 MB, `bin\eshkol-repl.exe` 90 MB, `lib\eshkol-runtime.lib` 6.2 MB, `lib\stdlib.o` 12.6 MB, `lib\stdlib.bc` 26 MB |
| Installed version | `Eshkol Compiler v1.3.6-evolve` (`eshkol-run --version`); `--features`: `llvm-backend=on gpu=on gpu-cuda=on blas=on tensor-dtypes=f64,f32,f16,bf16,i8` |
| Local source clone | `Z:\Tools\eshkol\source`, HEAD `d2f6a32f_2026-10-25` (read only, not used) |
| Code generation target | `x86_64-pc-windows-msvc`, CPU `arrowlake-s` (from `eshkol-run -d` output: "Set data layout for target: x86_64-pc-windows-msvc (cpu: arrowlake-s)") |
| Linker | The compiler shells out to the driver in `ESHKOL_CXX_COMPILER` (set on this box to `Z:\Tools\LLVM-21\bin\clang++.exe`) with `-fuse-ld=lld` |
| Headers | The install ships **no** C headers. `eshkol.h` exists only in the repo (`inc\eshkol\eshkol.h`, 3823 lines) |

## Candidates

"Kernel" here means a compiler capability, since the compiler is the subject.
Rows 4 and 5 are C files that belong to the sibling note; they are listed
because I cross-checked them with my own tests.

| # | Candidate | File and lines | Signature / form | Dependencies | Pure or stateful | Float or integer | Thread safety | Engine slot |
|---|---|---|---|---|---|---|---|---|
| 1 | Ahead-of-time compile to exe / object | `bin\eshkol-run.exe`; flags in `docs\API_REFERENCE.md` 6788-6861 | `eshkol-run prog.esk -o prog`; `-c -o prog.o` | clang++ + lld (LLVM 21), MSVC libs, `eshkol-runtime.lib`, `stdlib.o`, `cudart64_12.dll` at run time | Program holds a global arena (`get_global_arena`, `__global_arena`) | Tagged values: exact int64 with automatic bignum promotion, or double | Not assessed; one global arena, treat as single-threaded | none in the simulation; **tooling only** |
| 2 | Compiler-integrated AD | Language builtins `derivative`, `gradient` (`examples\autodiff.esk` 1-21) | `(derivative f x)`, `(gradient f x-or-vector)` | Same as row 1; AD tape in the runtime (`__current_ad_tape`, `arena_tape_add_node`) | Stateful tape in runtime | Double | Not assessed | (l) tooling: gradient oracle for hand-written C plasticity kernels |
| 3 | C <-> Eshkol boundary | `--shared-lib` thunks (`API_REFERENCE.md` 6817-6861); `(extern ...)` (`ESHKOL_LANGUAGE_GUIDE.md` 1198-1219) | `eshkol_tagged_value_t f(eshkol_tagged_value_t)`, 16-byte struct, Windows x64: hidden return pointer, argument by pointer; init with `__eshkol_lib_init__(get_global_arena())` | DLL of 2.1 MB carrying the runtime; same CUDA import | Stateful (arena) | Both | Not assessed | (l) tooling: call an Eshkol oracle from a C test harness |
| 4 | Fixed-point and exact integer dot (cross-check) | `lib\math\fixed_point\eshkol_fixed_point.c` 180-300 (fixed ops), 516-535 (accumulator, `esk_idot_*`), 549-612; `.h` 104-238 | `esk_fixed esk_fixed_mul(esk_fixed, esk_fixed, esk_round_mode, esk_overflow_mode, esk_rng*, bool*)`; `esk_i128 esk_idot_i8(const int8_t*, const int8_t*, size_t)`; `void esk_accum128_merge(esk_accum128*, const esk_accum128*)` | libc, `__int128` (gcc/clang, not MSVC) | Pure; RNG state passed by pointer | Integer core | Re-entrant, no globals | (b), (g): see `eshkol-runtime.md` |
| 5 | FNV-1a 64 (cross-check) | `lib\core\merkle.c` 24-35 | `int64_t eshkol_fnv1a_64(const char* data, int64_t len)` | none | Pure | Integer | Re-entrant | (j) non-cryptographic receipt hash: see `eshkol-runtime.md` |

## What I ran

Shell: Git Bash. `ESK=/z/Tools/eshkol/current/bin/eshkol-run.exe`. Sources are in
`S:\tsotchke\eshkol-compiler\`.

### 1. Integer loop -> native exe (ran)

`intloop.esk`: a 31-bit LCG folded into a checksum over 1,000,000 steps, and a
sum of squares to 100,000. `ref_intloop.c` is an independent C version.

```
cd /s/tsotchke/eshkol-compiler
time (timeout 90 $ESK intloop.esk -o intloop)
time ./intloop.exe
gcc -std=c2x -O2 -Wall -Wextra ref_intloop.c -o ref_intloop.exe && ./ref_intloop.exe
```

Output tails:

```
real    0m1.334s                      <- compile (second build 1.132 s)
lcg-sum(1000000)=451629688
sum-squares(100000)=333338333350000
real    0m0.232s                      <- run (process start alone is about 0.1 s)
--- C reference
lcg-sum(1000000)=451629688
sum-squares(100000)=333338333350000
```

Output kind: `intloop.exe`, 12,172,800 bytes, `pei-x86-64`, plus a leftover
`intloop.exe.tmp.o` (189,463 bytes).

### 2. Automatic differentiation -> native exe (ran)

`ad.esk`: forward derivative, reverse gradient, a transcendental chain, and a
two-variable gradient.

```
time (timeout 90 $ESK ad.esk -o ad)
time ./ad.exe
```

```
real    0m1.570s                      <- compile
d/dx(x^3)@2=12
grad(x^3+2x)@3=29
d/dx(sin*exp)@0.5=2.2373281197977843
grad(x^2 y + y^3)@(1,2)=#(4 13)
real    0m0.108s                      <- run
```

All four are right: 3x^2 = 12; 3x^2+2 = 29; e^0.5 (sin 0.5 + cos 0.5) = 2.23732812;
(2xy, x^2+3y^2) at (1,2) = (4, 13).

### 3. Runtime DLL dependencies (ran)

```
objdump -p intloop.exe | grep -i "DLL Name"
PATH=/c/Windows/System32 ./intloop.exe; echo "exit=$?"
```

```
        DLL Name: cudart64_12.dll
        DLL Name: ntdll.dll
        DLL Name: KERNEL32.dll
S:/tsotchke/eshkol-compiler/intloop.exe: error while loading shared libraries: ?: cannot open shared object file: No such file or directory
exit=127
```

The C runtime is linked statically. The CUDA runtime DLL is a hard import even
for a program that only adds integers; it is found on this box only because
`Z:\Tools\cuda-12.9-redist\bin` is on PATH. The same three imports appear in the
shared library (`kern.dll`).

### 4. Determinism (ran)

```
$ESK intloop.esk -o intloop ; $ESK intloop.esk -o intloop_b
sha256sum intloop.exe.tmp.o intloop_b.exe.tmp.o
cmp -l intloop.exe intloop_first.exe | wc -l      # same output name, built twice
for i in 1 2 3; do ./ad.exe | sha256sum | cut -c1-16; done
objdump -d kern.o | grep -c -i "vfmadd\|vfmsub\|vfnmadd"
```

```
8e4932cb08fb62a9e9d0391a0989b19c9d6dcbab0d2e77dfb5dc2f9766b54e11 *intloop.exe.tmp.o
8e4932cb08fb62a9e9d0391a0989b19c9d6dcbab0d2e77dfb5dc2f9766b54e11 *intloop_b.exe.tmp.o
1                                     <- exe differs in one byte (offset 129, the PE time stamp field)
ccf41613b1eb7b2a  (three times)       <- ad.exe output
568148a7d37bf0aa  (three times)       <- intloop.exe output
6                                     <- FMA instructions in kern.o
```

- Object code: byte-identical across two compiles of the same source.
- Executable: identical except the linker time stamp.
- Run to run: identical output.
- Integers: exact, and they do **not** wrap. Overflow promotes to a bignum
  (`eshkol_bignum_from_overflow` is among the object's undefined symbols), so
  32-bit or 64-bit wrap-around must be written with `modulo`.
- Floats: code is generated for the host CPU (`arrowlake-s`) and uses FMA and
  256-bit registers, so float results are not guaranteed bit-identical on
  another CPU. Not tested on another machine.
- Optimisation flag: `-O 0`, `-O 2`, `-O 3` gave 0.46 s, 0.39 s, 0.61 s wall for
  the same 1,000,000-step program; no useful difference inside the noise.

### 5. Object file and shared library (ran)

```
time (timeout 90 $ESK kern.esk -c -o kern.o)
time (timeout 90 $ESK kern.esk --shared-lib -o kern)
objdump -p kern.dll | grep -n -i "export"
gcc -std=c2x -O2 -Wall -Wextra host.c -o host.exe && ./host.exe kern.dll
```

```
real    0m0.944s                      <- object: kern.o 210,711 bytes, pe-x86-64, 94 undefined symbols
[eshkol-run] created shared library 'kern.dll'. Load it (dlopen/LoadLibrary), call `__eshkol_lib_init__(arena)` once, then call its exported functions.
real    0m1.054s                      <- kern.dll 2,085,888 bytes
44:Entry 0 0000000000000000 00000000 Export Directory [.edata (or where ever we found it)]
loaded kern.dll
GetProcAddress(__eshkol_lib_init__) = NOT FOUND
GetProcAddress(get_global_arena) = NOT FOUND
GetProcAddress(mix32) = NOT FOUND
GetProcAddress(mix-sum) = NOT FOUND
GetProcAddress(dcubic) = NOT FOUND
RESULT: 5 required exports missing; cannot call
```

**Defect in the shipped Windows build:** the DLL exports nothing. Cause found:
the generated object carries `/INCLUDE:` linker directives for the functions but
no `/EXPORT:` (`objdump -s -j .drectve kern.dll.tmp.o`), and the link line has no
export list.

### 6. C calls Eshkol, with the exports added (ran)

`cxxwrap.c` is a small pass-through that logs the link command and appends
extra arguments. It is selected for one process only, through environment
variables; the install is untouched.

```
gcc -O2 -Wall cxxwrap.c -o cxxwrap.exe
cd lk
CXXWRAP_EXTRA='-Wl,/EXPORT:mix32 -Wl,/EXPORT:mix-sum -Wl,/EXPORT:dcubic -Wl,/EXPORT:cubic -Wl,/EXPORT:__eshkol_lib_init__ -Wl,/EXPORT:get_global_arena' \
LLVM_HOME='Z:\Tools\LLVM-21' CXXWRAP_REAL='Z:\Tools\LLVM-21\bin\clang++.exe' \
CXXWRAP_LOG='S:\tsotchke\eshkol-compiler\lk\link.log' \
ESHKOL_CXX_COMPILER='S:\tsotchke\eshkol-compiler\cxxwrap.exe' \
  $ESK kern.esk --shared-lib -o kern
../host.exe  'S:\tsotchke\eshkol-compiler\lk\kern.dll'
../host2.exe 'S:\tsotchke\eshkol-compiler\lk\kern.dll'
```

Captured link command (`lk\link.log`):

```
-shared kern.dll.tmp.o Z:\Tools\eshkol\eshkol-v1.3.6-evolve-windows-x64-cuda\lib\eshkol-runtime.lib
-lAdvapi32 -lNtdll -lGdiplus -lbcrypt -lws2_32 -LZ:\Tools\cuda-12.9-redist\lib\x64 -lcudart
Z:\Tools\LLVM-21\lib\clang\21\lib\windows\clang_rt.builtins-x86_64.lib -fuse-ld=lld -o kern.dll
```

Output tails (gcc-built C host, struct layout copied from `eshkol.h`):

```
mix32(12345) type=1 flags=0x10 val=1011272156
mix-sum(100000) type=1 val=1338176
dcubic(3.0) type=2 val=29
RESULT: ok
--- host2
mix32 vs C reference over 100000 inputs: mismatches=0
mix-sum(1000000): eshkol=180062601 c=180062601 match=1
time eshkol loop: 164.081 ms (164.1 ns/iter); C loop: 2.167 ms (2.17 ns/iter)
1e6 boundary calls to mix32: 111.378 ms (111.4 ns/call), sum=2148786195104103
repeat mix-sum identical: 1
```

(The first host2 run gave 181.0 ns/iter and 113.8 ns/call.) So the C ABI thunks
are correct for gcc on Windows x64, a compiled Eshkol integer hash equals the C
one on 100,000 inputs, and AD is callable from C. The cost is high: the compiled
`mix32` is 6,272 bytes of machine code and `dcubic` is 10,496 bytes
(`nm -S kern.o`), because every operation is generic over tagged values with
overflow checks.

Note: `LoadLibraryA("kern.dll")` with a bare name loads the copy next to the
host exe; pass a full path when two builds exist.

### 7. Eshkol calls C (ran)

`cfn.c` (two functions, no C library use) compiled by gcc, then given to the
Eshkol compiler as a second input.

```
gcc -std=c2x -O2 -Wall -Wextra -c cfn.c -o cfn.o
time (timeout 90 $ESK callc.esk cfn.o -o callc) ; ./callc.exe        # (extern int c_mix32 int)
$ESK callc64.esk cfn.o -o callc64 ; ./callc64.exe                    # (extern i64 c_mix32 i64)
```

```
real    0m1.108s
--- with `int`
c_mix32(12345)=1011272156
mix-sum-c(1000000)=401759849          <- WRONG (C reference: 180062601)
c_axpy1(2.0,3.0,0.5)=6.5
--- with `i64`
c_mix32(12345)=1011272156
mix-sum-c(1000000)=180062601          <- correct
c_axpy1(2.0,3.0,0.5)=6.5
```

A mingw (gcc) COFF object links into the MSVC-target Eshkol program as long as
it needs no C library. The `int` extern type is 32-bit: a `uint32` result above
2^31 came back negative with no warning. Use `i64`.

### 8. Other probes (ran)

- `--profile freestanding-kernel-native --target x86_64-unknown-none-elf -c kern.esk -o kern_fs.o`
  wrote a 480-byte ELF object with no usable symbols. Not a route to a
  runtime-free Windows object. Without `--target` it refuses: "requires --target <triple>".
- `eshkol-run -e '(display (+ 1 2))'` (the JIT path) printed `3` in 1.56 s, with the
  warning "module defines __eshkol_init_parallel_workers but the symbol could not
  be resolved after JIT add". The JIT is forbidden inside the simulation anyway.
- Cross-check of the C fixed-point kernel (unmodified copy in `fp\`):
  `gcc -std=c2x -O2 -Wall -Wextra fp_test.c fp/eshkol_fixed_point.c -o fp_test.exe && ./fp_test.exe`

  ```
  Q16.16 1.5*2.25 raw=221184 (expect 221184) overflow=0
  saturate raw=2147483647 (expect 2147483647) overflow=1
  idot_i8 forward=481131 reversed=481131 blocked=481131 plain-C=481131
  stochastic rounding hash run1=-7216136888815025867 run2=-7216136888815025867
  esk_fixed_mul: 29.6 ns/op; esk_idot_i8: 0.315 ns/element
  RESULT: PASS
  ```
- Cross-check of FNV-1a (unmodified copy in `mk\`):
  `gcc -std=c2x -O2 -Wall -Wextra fnv_test.c mk/merkle.c -o fnv_test.exe && ./fnv_test.exe`

  ```
  fnv1a64("") = cbf29ce484222325 ok
  fnv1a64("a") = af63dc4c8601ec8c ok
  fnv1a64("foobar") = 85944171f73967e8 ok
  RESULT: PASS
  ```

## Proposed integration

Eshkol stays outside the simulation binary. Two tooling uses are worth having,
both run by the build or the test suite, never by a tick.

1. **Gradient oracle (tooling only).** For every hand-written C plasticity or
   field-update kernel that claims to be a derivative, keep a ten-line `.esk`
   file with the same formula and let `derivative` / `gradient` produce reference
   values on a fixed grid of inputs. The C test compares its fixed-point result
   against the oracle within the quantisation bound.
   - Engine side (C23, test build only):
     `int an_test_grad_oracle(const char *oracle_csv, an_grad_fn kernel, int32_t tol_q16);`
   - Runs: tooling only (build or CI step, about 1 to 2 s per oracle).
2. **Exact-integer table generator (tooling only).** Eshkol integers are exact
   and promote to bignums, so a script can compute constants (fixed-point
   reciprocal tables, polynomial coefficients, modular constants) without
   overflow and print a C header.
   - Engine side: a generated `an_tables_gen.h` checked in with its `.esk`
     source and the compiler version string.
   - Runs: tooling only.

If an in-process oracle is ever wanted, the wrapper route of section 6 works:
`eshkol_tagged_value_t f(eshkol_tagged_value_t)` through `LoadLibraryA` +
`GetProcAddress`, after `__eshkol_lib_init__(get_global_arena())`. Python or a
plain C program would do the same two jobs; the only thing Eshkol adds is AD
that needs no extra package on this box.

Tier 0, Tier 1, Tier 2 and world fields: **none**.

## Not usable, and why

- **Eshkol code inside the simulation, including Tier 2.** Owner law 1 names Lisp
  as tooling only. Independently: each call carries a 2 to 12 MB runtime with a
  growing global arena, tagged 16-byte values and bignum promotion; integer loops
  measured about 75 to 80 times slower than C; cache-resident SoA loops cannot be
  expressed. Colony-level low frequency does not rescue it, because the arena and
  the CUDA import would still live in the engine process.
- **`--shared-lib` as shipped on Windows.** The DLL has no exports, so the
  documented C host recipe fails until `/EXPORT:` flags are added by hand.
- **The `-c` object as a C-callable unit.** Internal calling convention, 94
  undefined runtime symbols; the docs say not to call it from C.
- **Distributing a compiled Eshkol exe.** It will not start without
  `cudart64_12.dll` on PATH, even for integer-only code.
- **Bit-exact floats across machines.** Host-tuned code with FMA. Only the
  integer results can be treated as reproducible evidence.
- **`(extern int ...)` for 64-bit or unsigned 32-bit data.** Silent truncation; use `i64`.
- **The freestanding profile** for a runtime-free object on Windows: produced an
  empty ELF object here.
- **The JIT (`-r`, `-e`) and the bytecode VM (`--emit-eskb`).** Forbidden by law 1
  inside the simulation; the JIT also printed an unresolved-symbol warning.
- **The agent, HTTP, SQLite, tree-sitter and model-loading libraries**
  (`lib\eshkol-agent-*.lib`, `share\eshkol\lib\agent`, `web`). Network and
  LLM-adjacent code: excluded by law 3. Not examined further.
- **`examples\consciousness.esk`** and similar naming: conflicts with law 7. Not reused.
- **GPU/CUDA tensor backend.** Not tested; no claim made.

## Verdict

`reference_only` for the engine: nothing from the compiler goes into the
simulation. As a development tool it works today on this box for AD oracles and
exact-integer table generation, at about 1 to 1.6 s per compile, with the
caveats above (CUDA DLL on PATH, export defect, 32-bit `int` trap).
