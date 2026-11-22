#!/bin/sh
# mca.sh - llvm-mca prediction for each kernel in kern.obj.
# The object is disassembled by llvm-objdump (AT&T syntax, which llvm-mca reads),
# one function at a time, without its ret.  llvm-mca then simulates the body
# repeated back to back, which matches the "throughput" row of test.exe
# (independent calls; every call reloads its accumulators from the bias).
#   sh mca.sh [mcpu ...]        default: arrowlake alderlake gracemont
BIN=/z/Tools/LLVM-21/bin
OBJ=kern.obj
CPUS="${*:-arrowlake alderlake gracemont}"
mkdir -p mca
"$BIN/llvm-objdump.exe" -d --no-show-raw-insn --no-leading-addr "$OBJ" > mca/all.dis
for fn in nn_mv32x64_vnni_c4x4 nn_mv32x64_vnni_c4x8 nn_mv32x64_vnni_c4x12 nn_mv32x64_int8_c4x8 nn_mv32x64_avx2_c4 nn_mv32x64_vnni_rm; do
    awk -v f="<$fn>:" '
        $0 ~ /^<[A-Za-z_0-9]+>:$/ { p = ($0 == f) }      # a new global function starts
        p && $0 !~ /^</ && $0 !~ /retq/ && $0 !~ /nop/ && NF { sub(/#.*/, ""); print }
    ' mca/all.dis > mca/$fn.s
    n=$(wc -l < mca/$fn.s)
    for cpu in $CPUS; do
        "$BIN/llvm-mca.exe" -mtriple=x86_64-pc-windows-msvc -mcpu=$cpu -iterations=1000 mca/$fn.s > mca/$fn.$cpu.txt 2> mca/$fn.$cpu.err
        if [ -s mca/$fn.$cpu.err ]; then echo "$fn $cpu: llvm-mca said: $(head -2 mca/$fn.$cpu.err | tr '\n' ' ')"; fi
        awk -v fn="$fn" -v cpu="$cpu" -v n="$n" '
            /^Iterations:/ {it=$2} /^Instructions:/ {ins=$2} /^Total Cycles:/ {cy=$3} /^Total uOps:/ {u=$3}
            /^uOps Per Cycle:/ {upc=$4} /^IPC:/ {ipc=$2} /^Block RThroughput:/ {rt=$3}
            END { if (it) printf "%-24s %-10s insns/call %3d  uops/call %6.1f  cycles/call %6.2f  uops/cycle %5.2f  IPC %5.2f  BlockRThroughput %5.1f\n",
                         fn, cpu, n, u/it, cy/it, upc, ipc, rt }' mca/$fn.$cpu.txt
    done
    # (-noalias=false was tried as a stand-in for the "latency" row.  It chains
    #  every load behind every earlier store and predicted 136..265 cycles, far
    #  from the measured ~50, so it is not used.)
done
# Morton batch loop body (one encode per iteration)
if [ -f morton_loop.s ]; then
    for cpu in $CPUS; do
        "$BIN/llvm-mca.exe" -mtriple=x86_64-pc-windows-msvc -mcpu=$cpu -iterations=1000 morton_loop.s > mca/morton_loop.$cpu.txt 2>/dev/null
        awk -v cpu="$cpu" '/^Iterations:/ {it=$2} /^Total Cycles:/ {cy=$3} /^Total uOps:/ {u=$3} /^Block RThroughput:/ {rt=$3}
            END { if (it) printf "%-24s %-10s uops/encode %4.1f  cycles/encode %5.2f  BlockRThroughput %4.1f\n", "morton3_pdep_batch loop", cpu, u/it, cy/it, rt }' mca/morton_loop.$cpu.txt
    done
fi
