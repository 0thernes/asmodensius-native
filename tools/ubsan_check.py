"""The UBSan trap build as one check (plan T6).

Configures a second build folder with -DAZ_UBSAN=ON (the core, the hosts and
the tests compiled with -fsanitize=undefined -fsanitize-trap=undefined and
without -fwrapv), builds it, shows with planted violations that the build
traps (tests/ubsan_probe.c), then runs every test program there. Undefined
behaviour executes an illegal instruction, so the program dies with exit code
0xC000001D; that, any other non-zero exit and a timeout all fail the check.

Usage: python ubsan_check.py SOURCE_DIR BUILD_DIR [--limit SECONDS] [--no-build]
  --limit N    seconds allowed for all test programs together (default 1800)
  --no-build   use BUILD_DIR as it is (already configured and built)
Exit code 0 when everything holds, 1 otherwise, 2 on a usage error. Logs go to
BUILD_DIR/ubsan_check/ (keep BUILD_DIR on Z:).
"""
import glob
import os
import subprocess
import sys
import time

TRAP = 0xC000001D          # STATUS_ILLEGAL_INSTRUCTION: what -fsanitize-trap executes
PROBES = ["overflow", "shift", "bounds", "divzero", "null", "align"]


def code_name(code):
    code &= 0xFFFFFFFF
    if code == TRAP:
        return "TRAP (illegal instruction: undefined behaviour)"
    if code == 0xC0000005:
        return "access violation (0xC0000005)"
    return f"exit {code}" if code < 0x80000000 else f"exit 0x{code:08X}"


def main():
    args = sys.argv[1:]
    limit, build = 1800, True
    pos = []
    i = 0
    while i < len(args):
        if args[i] == "--limit" and i + 1 < len(args) and args[i + 1].isdigit():
            limit = int(args[i + 1])
            i += 2
        elif args[i] == "--no-build":
            build = False
            i += 1
        elif args[i].startswith("--"):
            print(f"unknown option {args[i]}\n{__doc__}")
            return 2
        else:
            pos.append(args[i])
            i += 1
    if len(pos) != 2:
        print(__doc__)
        return 2
    src, bld = pos
    logs = os.path.join(bld, "ubsan_check")
    os.makedirs(logs, exist_ok=True)
    fails = []

    if build:
        for name, cmd in (("configure", ["cmake", "-S", src, "-B", bld, "-G", "Ninja", "-DAZ_UBSAN=ON"]),
                          ("build", ["cmake", "--build", bld])):
            with open(os.path.join(logs, name + ".log"), "w") as f:
                rc = subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT).returncode
            print(f"{name}: exit {rc}", flush=True)
            if rc != 0:
                print(f"FAILED: the UBSan {name} step failed, see {os.path.join(logs, name + '.log')}")
                return 1

    cache = os.path.join(bld, "CMakeCache.txt")
    is_ubsan = os.path.isfile(cache) and "AZ_UBSAN:BOOL=ON" in open(cache, errors="replace").read()
    if not is_ubsan:
        print(f"FAILED: {bld} is not configured with AZ_UBSAN=ON")
        return 1

    # 1. the build must trap on planted violations, and must not trap on defined code
    probe = os.path.join(bld, "ubsan_probe.exe")
    if not os.path.isfile(probe):
        print("FAILED: ubsan_probe.exe was not built")
        return 1
    rc = subprocess.run([probe, "none"], capture_output=True, timeout=60).returncode
    print(f"probe none: {code_name(rc)}")
    if rc != 0:
        fails.append("probe none")
    for case in PROBES:
        rc = subprocess.run([probe, case], capture_output=True, timeout=60).returncode
        trapped = (rc & 0xFFFFFFFF) == TRAP
        print(f"probe {case}: {code_name(rc)}" + ("" if trapped else "   <-- NOT TRAPPED"))
        if not trapped:
            fails.append("probe " + case)

    # 2. every test program, all at once (as run_tests.ps1 does)
    # glob.escape: the Z: path has [ ] in it, which glob would read as a set of characters
    exes = sorted(glob.glob(os.path.join(glob.escape(bld), "test_*.exe")))
    if not exes:
        print("FAILED: no test program in " + bld)
        return 1
    procs = []
    for exe in exes:
        name = os.path.splitext(os.path.basename(exe))[0]
        out = open(os.path.join(logs, name + ".log"), "w")
        procs.append((name, subprocess.Popen([exe], cwd=bld, stdout=out, stderr=subprocess.STDOUT), out, time.time()))
    deadline = time.time() + limit
    done = {}
    while len(done) < len(procs) and time.time() < deadline:
        for name, p, out, t0 in procs:
            if name not in done and p.poll() is not None:
                done[name] = (p.returncode, time.time() - t0)
        time.sleep(0.2)
    for name, p, out, t0 in procs:
        if name not in done:
            p.kill()
            p.wait()
            done[name] = (None, limit)
        out.close()
        rc, secs = done[name]
        with open(os.path.join(logs, name + ".log"), errors="replace") as f:
            lines = f.read().splitlines()
        tail = lines[-1].strip() if lines else ""
        state = f"KILLED after {limit} s" if rc is None else code_name(rc)
        print(f"{name}: {state}, {secs:.0f} s; {tail}")
        if rc != 0:
            fails.append(name)

    print(f"{'FAILED' if fails else 'ok'}: {len(PROBES)} planted violations, {len(exes)} test programs, "
          f"{len(fails)} failures" + (": " + ", ".join(fails) if fails else ""))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
