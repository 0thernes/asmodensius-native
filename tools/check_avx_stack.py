"""Gate: no aligned 256-bit move on a stack that is not known to be 32-byte aligned.

Why: on Windows x64 a function is entered with the stack aligned to 16 bytes.
GCC for MinGW can emit an aligned AVX move (vmovdqa, vmovaps, vmovapd with a
ymm register) to or from a stack slot in a function that has not realigned the
stack to 32 bytes (GCC bug 54412: by-value struct temporaries, spills). Such
an instruction faults whenever the process happens to start with its stack 16
bytes off, about every second run: found 2026-11-18 in tests/test_colony.c
(an Opt struct passed by value; access violation in 9 of 12 runs).

The build avoids it by letting the assembler encode every aligned vector move
as an unaligned one (-Wa,-muse-unaligned-vector-move, CMakeLists.txt). This
script checks the result: it disassembles the given objects, archives or
executables and reports every function that holds an aligned ymm move through
rsp or rbp and no 'and $-32, %rsp' (or stricter).

Usage: python check_avx_stack.py FILE [FILE ...]      exit 1 when anything is found
       python check_avx_stack.py --dir FOLDER         every .exe and .a in the folder
       python check_avx_stack.py --self-test          the planted cases must be told apart
"""
import os
import re
import subprocess
import sys

FUNC = re.compile(r'^[0-9a-f]+ <([^>]+)>:')
ALIGNED = re.compile(r'\bv?mov(dqa|aps|apd)(64|32)?\s+.*%ymm\d+')
STACK = re.compile(r'\((%rsp|%rbp)[,)]')
REALIGN = re.compile(r'\band\s+\$0xffffffffffffff(e0|c0|80|00),%rsp')


def scan_text(text):
    """-> list of (function, instruction line) that break the rule"""
    bad, name, hits, realigned = [], None, [], False

    def close():
        if name is not None and hits and not realigned:
            bad.extend((name, h) for h in hits)

    for line in text.splitlines():
        m = FUNC.match(line)
        if m:
            close()
            name, hits, realigned = m.group(1), [], False
            continue
        if REALIGN.search(line):
            realigned = True
        elif ALIGNED.search(line) and STACK.search(line):
            hits.append(line.strip())
    close()
    return bad


def scan_file(path):
    out = subprocess.run(['objdump', '-d', '--no-show-raw-insn', path], capture_output=True, text=True, errors='replace')
    if out.returncode != 0:
        raise SystemExit('objdump failed on %s: %s' % (path, out.stderr.strip()))
    return scan_text(out.stdout)


def self_test():
    planted_bad = '''
0000000000000000 <takes_struct>:
   0:\tsub    $0x98,%rsp
   7:\tvmovdqa %ymm0,0x50(%rsp)
   d:\tret
'''
    planted_ok = '''
0000000000000000 <realigned>:
   0:\tpush   %rbp
   1:\tmov    %rsp,%rbp
   4:\tand    $0xffffffffffffffe0,%rsp
   8:\tvmovdqa %ymm0,0x20(%rsp)
   e:\tret
0000000000000010 <unaligned_move>:
  10:\tvmovdqu %ymm0,0x50(%rsp)
  16:\tret
0000000000000020 <heap_not_stack>:
  20:\tvmovdqa (%rcx),%ymm0
  24:\tret
'''
    a, b = scan_text(planted_bad), scan_text(planted_ok)
    ok = len(a) == 1 and a[0][0] == 'takes_struct' and not b
    print('self-test: planted violation %s, three lawful cases %s' %
          ('found' if len(a) == 1 else 'MISSED', 'passed' if not b else 'WRONGLY REPORTED'))
    return 0 if ok else 1


def main():
    if len(sys.argv) == 2 and sys.argv[1] == '--self-test':
        return self_test()
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    files = sys.argv[1:]
    if files[0] == '--dir':
        if len(files) != 2:
            print(__doc__)
            return 2
        files = sorted(os.path.join(files[1], n) for n in os.listdir(files[1]) if n.endswith(('.exe', '.a')))
        if not files:
            print('no .exe or .a in ' + sys.argv[2])
            return 2
    total = 0
    for path in files:
        bad = scan_file(path)
        total += len(bad)
        funcs = sorted({f for f, _ in bad})
        if bad:
            print('%s: %d aligned 256-bit stack moves in %d functions without a 32-byte stack' % (path, len(bad), len(funcs)))
        for f in funcs[:40]:
            first = next(h for g, h in bad if g == f)
            print('    %s: %s' % (f, first))
    print('avx stack gate: %s (%d such moves in %d files)' % ('FAILED' if total else 'ok', total, len(files)))
    return 1 if total else 0


if __name__ == '__main__':
    sys.exit(main())
