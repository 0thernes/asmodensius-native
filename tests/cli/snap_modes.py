"""ctest cli_snap_modes: az_snap in both modes of the world (owner prompts P42
and P43: wild is the living world and the default, --lab is the test bench).

Checks, on a small world (2,000 founders, 20 ticks):
- --seed without --lab is refused with exit code 2 and writes no picture;
- two wild runs (no --lab) both end with exit code 0, say "mode: WILD", keep
  matter and energy equal to the start after every tick, have a state hash
  that the renderer did not change, write a PNG, and differ from each other
  in origin and in state hash;
- two --lab --seed 7 runs are equal in state hash and view hash, and their
  state hash equals the final hash of az_run --lab with the same options;
- a wild run of az_run with the same options says "mode: WILD" as well and
  reports exact matter and energy.

Usage: python snap_modes.py AZ_SNAP AZ_RUN WORK_DIR
Exit code 0 when every check holds, 1 otherwise. Files stay in WORK_DIR (on Z:).
"""
import os
import re
import subprocess
import sys

FAILS = []
WORLD = ["--n", "2000", "--ticks", "20", "--cap", "8000", "--grid", "6,6,5"]


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


def is_png(path):
    try:
        with open(path, "rb") as f:
            return f.read(8) == b"\x89PNG\r\n\x1a\n" and os.path.getsize(path) > 1000
    except OSError:
        return False


def snap(exe, work, name, extra):
    png = os.path.join(work, name + ".png")
    if os.path.exists(png):
        os.remove(png)
    code, out, err = run([exe, "--out", png, "--size", "640x360"] + WORLD + extra)
    m = re.search(r"state hash before render ([0-9a-f]{16}) after ([0-9a-f]{16}) \((equal|CHANGED)\)", out)
    res = {
        "code": code, "out": out, "err": err, "png": png,
        "before": m.group(1) if m else None,
        "after": m.group(2) if m else None,
        "view": field(r"view hash ([0-9a-f]{16})", out),
        "origin": field(r"\(origin ([0-9a-f]{16})", out),
        "balanced": re.search(r"matter \d+ and energy -?\d+ equal to the start after every one of 20 ticks", out)
        is not None,
        "births": field(r"life: births (\d+)", out),
    }
    check(code == 0, f"{name}: az_snap exit {code}" + (f" (stderr: {err.strip()})" if code else ""))
    check(m is not None and m.group(3) == "equal" and res["before"] == res["after"],
          f"{name}: the renderer left the state hash as it was ({res['before']})")
    check(res["balanced"], f"{name}: matter and energy equal to the start after every tick")
    check(res["births"] is not None and int(res["births"]) > 0, f"{name}: the world lives (births {res['births']})")
    check(is_png(png), f"{name}: a PNG was written")
    return res


def main():
    if len(sys.argv) != 4:
        print(__doc__)
        return 2
    az_snap, az_run, work = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(work, exist_ok=True)

    refused = os.path.join(work, "refused.png")
    if os.path.exists(refused):
        os.remove(refused)
    code, out, err = run([az_snap, "--out", refused, "--seed", "7"] + WORLD)
    check(code == 2, f"--seed without --lab: exit {code}, wanted 2")
    check("--seed needs --lab" in err, "--seed without --lab: stderr says that --seed needs --lab")
    check(not os.path.exists(refused), "--seed without --lab: no picture written")

    w1 = snap(az_snap, work, "wild_1", [])
    w2 = snap(az_snap, work, "wild_2", [])
    for name, r in (("wild_1", w1), ("wild_2", w2)):
        check("mode: WILD" in r["out"], f"{name}: the default mode is WILD")
        check(r["origin"] is not None, f"{name}: an origin drawn from entropy is printed ({r['origin']})")
        check("0 ticks had no fresh entropy" in r["out"], f"{name}: every tick drew fresh entropy")
    check(w1["origin"] != w2["origin"], "two wild worlds have different origins")
    check(w1["before"] is not None and w1["before"] != w2["before"],
          f"two wild worlds end in different states ({w1['before']}, {w2['before']})")

    l1 = snap(az_snap, work, "lab_1", ["--lab", "--seed", "7"])
    l2 = snap(az_snap, work, "lab_2", ["--lab", "--seed", "7"])
    for name, r in (("lab_1", l1), ("lab_2", l2)):
        check("mode: LAB" in r["out"] and "(seed 7)" in r["out"], f"{name}: the mode is LAB with seed 7")
    check(l1["before"] is not None and l1["before"] == l2["before"],
          f"two lab worlds of one seed end in one state ({l1['before']}, {l2['before']})")
    check(l1["view"] is not None and l1["view"] == l2["view"], "two lab pictures of one seed have one view hash")
    try:
        with open(l1["png"], "rb") as a, open(l2["png"], "rb") as b:
            same_png = a.read() == b.read()
    except OSError:
        same_png = False
    check(same_png, "two lab pictures of one seed are the same file, byte for byte")

    code, out, err = run([az_run, "--lab", "--seed", "7", "--quiet"] + WORLD)
    final = field(r"final hash ([0-9a-f]{16})", out)
    check(code == 0, f"az_run --lab: exit {code}")
    check(final is not None and final == l1["before"],
          f"az_snap --lab shows the world az_run --lab ends with ({final})")

    code, out, err = run([az_run] + WORLD)
    wild_final = field(r"final hash ([0-9a-f]{16})", out)
    check(code == 0, f"az_run (wild): exit {code}")
    check("mode: WILD" in out, "az_run without --lab: the mode is WILD")
    check(re.search(r"matter: (\d+) -> \1 \(exact\)\s+energy invariant: (-?\d+) -> \2 \(exact\)", out) is not None,
          "az_run (wild): matter and energy exact")
    check(wild_final is not None and wild_final not in (final, w1["before"], w2["before"]),
          f"az_run (wild) ends in a state of its own ({wild_final})")

    print(f"{len(FAILS)} failures")
    return 1 if FAILS else 0


if __name__ == "__main__":
    sys.exit(main())
