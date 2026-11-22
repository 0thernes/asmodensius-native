#!/usr/bin/env python3
"""check_core_obj.py - object-level gate on the freestanding core library.

Development tooling, not simulation code. The core is not built with
-mgeneral-regs-only (az_field.c and az_scan.c use GCC integer vector types),
so this gate checks the compiled code itself (docs/ARCHITECTURE.md section 12):

  fp     0 floating-point instructions: scalar/packed float arithmetic,
         compares, conversions, FMA on ss/sd/ps/pd, MXCSR access, x87.
         Integer SIMD, moves, shuffles and bitwise ops (xorps, andps) pass.
  data   no writable data: no nm symbol of type D d B b C G g S s, and no
         .data/.bss/other writable section with a non-zero size.
  undef  the only symbols left undefined by the whole archive are
         memcpy, memset, memmove, memcmp (no ___chkstk_ms, no libgcc helper).

Uses objdump and nm from PATH (binutils). Prints the counts and every
offending instruction or symbol.

Usage:  python check_core_obj.py <path to libaz_core.a>
        python check_core_obj.py --self-test <path to libaz_core.a> [--work DIR]
        --self-test compiles planted objects with gcc in DIR (default: a
        check_core_obj_selftest folder next to the library), archives each
        with ar, requires each to fail for exactly its reason (one float
        operation, one global variable, one undefined libc call), a clean
        two-member archive to pass, and then the real library to pass.
Exit code 0 = pass.
"""
import os
import re
import shutil
import subprocess
import sys

ALLOWED_UNDEF = {"memcpy", "memset", "memmove", "memcmp"}
WRITABLE_TYPES = set("DdBbCGgSs")
PREFIXES = {"rep", "repz", "repnz", "repe", "repne", "lock", "data16", "data32", "addr32",
            "cs", "ds", "es", "ss", "fs", "gs", "notrack", "bnd", "xacquire", "xrelease"}
VREG_RE = re.compile(r"\b[xyz]mm\d+\b")
FP_SIMD_RE = re.compile(
    r"^v?(add|sub|mul|div|min|max|sqrt|rcp|rsqrt|rcp14|rsqrt14|round|rndscale|dp|hadd|hsub|addsub"
    r"|cmp[a-z]*|getexp|getmant|scalef|range|reduce|fixupimm)(ss|sd|ps|pd|sh|ph)$")
FP_OTHER_RE = re.compile(
    r"^(v?u?comis[sdh]|v?cvt[a-z0-9]*|vfn?m(add|sub|addsub|subadd)(132|213|231)?(ss|sd|ps|pd|sh|ph)"
    r"|v?ldmxcsr|v?stmxcsr|f[a-z0-9]*)$")


def run(cmd, cwd=None):
    p = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                       universal_newlines=True)
    if p.returncode != 0:
        raise RuntimeError(f"{' '.join(cmd)} failed ({p.returncode}): {p.stderr.strip()[:400]}")
    return p.stdout


def tool(name):
    path = shutil.which(name)
    if not path:
        raise RuntimeError(f"{name} not found on PATH (binutils/gcc needed)")
    return path


def is_fp(mnem, operands):
    if mnem.startswith("f"):
        return True  # x87 (fld, fstp, fadd, fnstcw, ...)
    if FP_SIMD_RE.match(mnem):
        return bool(VREG_RE.search(operands))  # 'cmps' string ops have no vector operands
    return bool(FP_OTHER_RE.match(mnem))


def scan_code(lib):
    """Return (n_instr, n_vreg, fp_list) from objdump -d."""
    out = run([tool("objdump"), "-d", "-M", "intel", lib])
    member, func = "?", "?"
    n_instr = n_vreg = 0
    fp = []
    for line in out.splitlines():
        m = re.match(r"^(\S+):\s+file format", line)
        if m:
            member = m.group(1)
            continue
        m = re.match(r"^[0-9a-f]+ <(.+)>:$", line)
        if m:
            func = m.group(1)
            continue
        parts = line.split("\t")
        if len(parts) < 3 or not re.match(r"^\s*[0-9a-f]+:$", parts[0]):
            continue
        toks = parts[2].split()
        while toks and (toks[0] in PREFIXES or toks[0].startswith("rex")):
            toks = toks[1:]
        if not toks:
            continue
        mnem, operands = toks[0], " ".join(toks[1:])
        n_instr += 1
        if VREG_RE.search(operands):
            n_vreg += 1
        if is_fp(mnem, operands):
            fp.append(f"{member} <{func}> {parts[0].strip()} {mnem} {operands}")
    return n_instr, n_vreg, fp


def scan_symbols(lib):
    """Return (writable symbols, undefined-outside-allow-list, all undefined)."""
    out = run([tool("nm"), lib])
    member = "?"
    defined, undef = set(), {}
    writable = []
    for line in out.splitlines():
        s = line.strip()
        if not s:
            continue
        if s.endswith(":") and " " not in s:
            member = s[:-1]
            continue
        toks = s.split()
        if len(toks) == 2 and toks[0] in ("U", "w"):
            undef.setdefault(toks[1], set()).add(member)
            continue
        if len(toks) != 3:
            continue
        typ, name = toks[1], toks[2]
        defined.add(name)
        if typ in WRITABLE_TYPES and not name.startswith("."):
            writable.append(f"{member}: {typ} {name}")
    unresolved = {n: m for n, m in undef.items() if n not in defined}
    bad = [f"{n} (from {', '.join(sorted(m))})" for n, m in sorted(unresolved.items())
           if n not in ALLOWED_UNDEF]
    return writable, bad, sorted(unresolved)


def scan_sections(lib):
    """Writable allocated sections with a non-zero size, from objdump -h."""
    out = run([tool("objdump"), "-h", lib])
    member = "?"
    bad = []
    lines = out.splitlines()
    for k, line in enumerate(lines):
        m = re.match(r"^(\S+):\s+file format", line)
        if m:
            member = m.group(1)
            continue
        m = re.match(r"^\s*\d+\s+(\S+)\s+([0-9a-fA-F]+)\s", line)
        if m and k + 1 < len(lines):
            name, size = m.group(1), int(m.group(2), 16)
            flags = lines[k + 1]
            if size and "ALLOC" in flags and "READONLY" not in flags and "CODE" not in flags:
                bad.append(f"{member}: section {name} size {size} ({flags.strip()})")
    return bad


def check(lib, verbose=True):
    """Run the three checks. Returns {reason: [offenders]} for failing reasons."""
    n_instr, n_vreg, fp = scan_code(lib)
    writable, bad_undef, all_undef = scan_symbols(lib)
    bad_sections = scan_sections(lib)
    if verbose:
        print(f"check_core_obj: {lib}")
        print(f"  instructions: {n_instr}, with vector registers: {n_vreg}, floating-point: {len(fp)}")
        for f in fp:
            print("    fp: " + f)
        print(f"  writable data symbols: {len(writable)}, writable non-empty sections: {len(bad_sections)}")
        for w in writable + bad_sections:
            print("    data: " + w)
        print(f"  undefined symbols: {', '.join(all_undef) or 'none'}; outside allow-list: {len(bad_undef)}")
        for u in bad_undef:
            print("    undef: " + u)
    reasons = {}
    if fp:
        reasons["fp"] = fp
    if writable or bad_sections:
        reasons["data"] = writable + bad_sections
    if bad_undef:
        reasons["undef"] = bad_undef
    return reasons


PLANTS = [
    ("fp", "fp", ["int fx_scale(int x) { return (int)((double)x * 1.25); }\n"]),
    ("global", "data", ["unsigned fx_hits;\nunsigned fx_bump(void) { return ++fx_hits; }\n"]),
    ("libc", "undef", ["#include <string.h>\n"
                       "unsigned long long fx_len(const char *s) { return strlen(s); }\n"]),
    ("clean", None, ["#include <string.h>\nstruct fx_big { char b[64]; };\n"
                     "unsigned fx_add(unsigned a, unsigned b) { return a + b; }\n"
                     "void fx_copy(struct fx_big *d, const struct fx_big *s) { memcpy(d, s, sizeof *d); }\n",
                     "unsigned fx_add(unsigned a, unsigned b);\n"
                     "unsigned fx_twice(unsigned a) { return fx_add(a, a); }\n"]),
]


def self_test(lib, work):
    os.makedirs(work, exist_ok=True)
    gcc, ar = tool("gcc"), tool("ar")
    fails = 0
    for name, want, sources in PLANTS:
        objs = []
        for k, src in enumerate(sources):
            c = os.path.join(work, f"plant_{name}_{k}.c")
            o = os.path.join(work, f"plant_{name}_{k}.o")
            with open(c, "w", newline="\n") as fh:
                fh.write(src)
            run([gcc, "-std=c2x", "-O2", "-ffreestanding", "-c", c, "-o", o])
            objs.append(o)
        a = os.path.join(work, f"libplant_{name}.a")
        if os.path.exists(a):
            os.remove(a)
        run([ar, "rcs", a] + objs)
        reasons = check(a, verbose=False)
        got = sorted(reasons)
        ok = got == ([want] if want else [])
        print(f"{'PASS' if ok else 'FAIL'} planted {name}: want {want or 'pass'}, got {got or 'pass'}"
              + (f" ({reasons[want][0]})" if want and want in reasons else ""))
        if not ok:
            fails += 1
            for r, items in reasons.items():
                for it in items:
                    print(f"    {r}: {it}")
    print("real library:")
    reasons = check(lib)
    ok = not reasons
    print(f"{'PASS' if ok else 'FAIL'} real library: failing checks {sorted(reasons) or 'none'}")
    if not ok:
        fails += 1
    print(f"check_core_obj self-test: {'PASSED' if fails == 0 else 'FAILED'} ({fails} failing cases)")
    return 1 if fails else 0


def main():
    args = sys.argv[1:]
    work = None
    if "--work" in args:
        k = args.index("--work")
        if k + 1 >= len(args):
            print(__doc__)
            return 2
        work = args[k + 1]
        del args[k:k + 2]
    selftest = "--self-test" in args
    args = [a for a in args if a != "--self-test"]
    if len(args) != 1 or args[0].startswith("--") or not os.path.isfile(args[0]):
        print(__doc__)
        return 2
    lib = os.path.abspath(args[0])
    try:
        if selftest:
            return self_test(lib, work or os.path.join(os.path.dirname(lib), "check_core_obj_selftest"))
        reasons = check(lib)
    except RuntimeError as e:
        print("check_core_obj: error:", e)
        return 2
    print(f"check_core_obj: {'PASS' if not reasons else 'FAIL'}"
          + (f" ({', '.join(sorted(reasons))})" if reasons else ""))
    return 1 if reasons else 0


if __name__ == "__main__":
    sys.exit(main())
