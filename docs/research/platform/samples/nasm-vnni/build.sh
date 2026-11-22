#!/bin/sh
# build.sh - assemble, build and run every check of this sample (Git Bash).
#   sh build.sh          builds, runs the tests without timing, runs the controls
#   ./test.exe           afterwards: tests + RDTSC timing on one P and one E core
#   sh mca.sh            afterwards: llvm-mca predictions
set -e
LLVM=/z/Tools/LLVM-21/bin
cd "$(dirname "$0")"

echo "== 1. assemble"
nasm -f win64 kern.asm -o kern.obj -l kern.lst
nasm -f win64 enc_check.asm -o enc_check.obj
"$LLVM/clang.exe" -c --target=x86_64-pc-windows-gnu enc_ref.s -o enc_ref.obj
echo "nasm + clang ok"

echo "== 2. hand-encoded VEX vpdpbusd vs clang's assembler (bytes and operands of 24 forms)"
"$LLVM/llvm-objdump.exe" -d enc_check.obj | grep vpdpbusd | sed 's/^ *[0-9a-f]*: *//' > enc_check.dis
"$LLVM/llvm-objdump.exe" -d enc_ref.obj   | grep vpdpbusd | sed 's/^ *[0-9a-f]*: *//' > enc_ref.dis
if cmp -s enc_check.dis enc_ref.dis; then echo "identical: $(wc -l < enc_check.dis) of $(wc -l < enc_ref.dis) instructions  PASS"; else echo "DIFFERENT  FAIL"; diff enc_check.dis enc_ref.dis; exit 1; fi
n_all=$("$LLVM/llvm-objdump.exe" -d kern.obj | grep -c 'vpdpbusd')
n_vex=$("$LLVM/llvm-objdump.exe" -d kern.obj | grep 'vpdpbusd' | grep -c '{vex}')
n_evex=$("$LLVM/llvm-objdump.exe" -d kern.obj | grep -c '^ *[0-9a-f]*: 62 ' || true)
echo "kern.obj: $n_all vpdpbusd, $n_vex of them VEX; EVEX-encoded instructions in the object: $n_evex"

echo "== 3. build and run tests"
gcc -std=c2x -O2 -Wall -Wextra test.c kern.obj -o test.exe
./test.exe notime
g++ -O2 -Wall unwind_throw.cpp kern.obj -o unwind_throw.exe
./unwind_throw.exe

echo "== 4. negative controls (each must FAIL)"
gcc -std=c2x -O2 -DMUTATE test.c kern.obj -o test_mut.exe
./test_mut.exe notime | grep -E "^kernels:|^RESULT" || true
nasm -f win64 -DNO_UNWIND kern.asm -o kern_nounw.obj
gcc -std=c2x -O2 test.c kern_nounw.obj -o test_nounw.exe
./test_nounw.exe notime | grep -E "^unwind:|^RESULT" || true
echo "== done"
