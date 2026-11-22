"""ctest cli_ledger_resume: az_run --ledger across --save and --load
(milestone 2, owners' request T5; finding s2).

A fresh 40-tick run, and a 20-tick run saved and resumed for 20 more ticks,
must end with the same world hash. The resumed ledger must start from the saved
run's chain and end on the fresh run's chain, the two halves must hold as many
records as the whole, and az_ledgerdump must find every file clean with the
chain each run reported.

Usage: python ledger_resume.py AZ_RUN AZ_LEDGERDUMP WORK_DIR
Exit code 0 when every check holds, 1 otherwise. Files stay in WORK_DIR (on Z:).
"""
import os
import re
import subprocess
import sys

FAILS = []


def check(cond, what):
    print(("ok    " if cond else "FAIL  ") + what)
    if not cond:
        FAILS.append(what)


def run(args):
    p = subprocess.run(args, capture_output=True, text=True, timeout=600)
    return p.returncode, p.stdout, p.stderr


def field(pattern, text):
    m = re.search(pattern, text)
    return m.group(1) if m else None


def az_run(exe, args, label):
    code, out, err = run([exe] + args)
    final = field(r"final hash ([0-9a-f]{16})", out)
    m = re.search(r"ledger: (\d+) records written to .*?, (\d+) dropped, (\d+) write errors; "
                  r"chain ([0-9a-f]{16}) (equals|DIFFERS FROM) the world's", out)
    check(code == 0, f"{label}: az_run exit {code}" + (f" (stderr: {err.strip()})" if code else ""))
    check(m is not None and m.group(5) == "equals", f"{label}: ledger line reports the world's chain")
    if not m:
        return final, None, None
    check(m.group(2) == "0" and m.group(3) == "0", f"{label}: nothing dropped, no write errors")
    return final, int(m.group(1)), m.group(4)


def dump(exe, path, chain, label):
    code, out, err = run([exe, path, "--chain", "0x" + chain, "--first", "0"])
    check(code == 0, f"{label}: az_ledgerdump exit {code} on {os.path.basename(path)}")
    check(f"expected  {chain}  MATCH" in out, f"{label}: file chain {chain} MATCH")   # not MISMATCH
    return field(r"start     chain ([0-9a-f]{16})", out)


def main():
    if len(sys.argv) != 4:
        print(__doc__)
        return 2
    exe_run, exe_dump, work = sys.argv[1:4]
    os.makedirs(work, exist_ok=True)
    f_full, f_a, f_b, snap = (os.path.join(work, n) for n in ("full.ldg", "first.ldg", "second.ldg", "mid.snap"))
    world = ["--lab", "--n", "3000", "--seed", "7", "--quiet"]   # lab mode: the two halves must repeat the whole

    h_full, n_full, c_full = az_run(exe_run, world + ["--threads", "3", "--ticks", "40", "--ledger", f_full], "fresh 40")
    h_a, n_a, c_a = az_run(exe_run, world + ["--threads", "3", "--ticks", "20", "--ledger", f_a, "--save", snap],
                           "first 20 + save")
    h_b, n_b, c_b = az_run(exe_run, ["--load", snap, "--threads", "2", "--ticks", "20", "--ledger", f_b, "--quiet"],
                           "load + second 20")

    check(h_full is not None and h_full == h_b, f"world hash after 40 ticks: fresh {h_full}, resumed {h_b}")
    check(c_full is not None and c_full == c_b, f"ledger chain after 40 ticks: fresh {c_full}, resumed {c_b}")
    check(None not in (n_full, n_a, n_b) and n_a + n_b == n_full, f"records: {n_a} + {n_b} = {n_full}")

    if c_full:
        start = dump(exe_dump, f_full, c_full, "fresh 40")
        check(start == "0" * 16, "fresh ledger starts from chain 0")
    if c_a:
        dump(exe_dump, f_a, c_a, "first 20")
    if c_b:
        start = dump(exe_dump, f_b, c_b, "second 20")
        check(start is not None and start == c_a, f"resumed ledger starts from the saved chain {c_a} (header: {start})")

    print(f"{'FAILED' if FAILS else 'ok'}: {len(FAILS)} failures")
    return 1 if FAILS else 0


if __name__ == "__main__":
    sys.exit(main())
