"""The command-line determinism matrix (CONTINUE-BUILD section 3; plan T6).

One world, run through az_run (or az_run_proto, the same program on the
test-only interacting module set of tests/protocol/) under every execution
variation that must not change the result: worker counts 1, 2, 5, 8, 12, 16
and 24 (E cores included, pinned and unpinned), reorder periods 7, 31 and 255,
random work splits, the assembly kernels on and off, and a snapshot saved
mid-run and resumed at another worker count. Every run must end with the
reference run's world hash, its hash trace (every 5th tick), its ledger chain
and its counters, with exact matter and energy at every tick, no field
underflow, no dropped event, and the arena locked in RAM.

Usage:
  python determinism_matrix.py AZ_RUN WORK_DIR [--interacting] [--expect-denied]
         [--expect HASH] [--light] -- WORLD OPTIONS (as for az_run; --ticks is required)

  --interacting    the world must show deaths and resolved pairs of the four
                   kinds the protocol set makes (attack, share, trade, mate),
                   and a run with every pair lesioned (--lesion 0x8) must show
                   no pair and another hash
  --expect-denied  the world must refuse births for want of a slot (a binding
                   capacity), and still be equal everywhere
  --expect HASH    the reference run must end with this hash (a recorded one)
  --light          a short matrix for large worlds (1, 8 and 24 workers,
                   reorder 7, one split, the kernels, one resume)

Exit code 0 when every check holds, 1 otherwise, 2 on a usage error. Logs,
ledgers and the snapshot stay in WORK_DIR (on Z:); commands.txt lists every
command that ran.
"""
import os
import re
import subprocess
import sys
import time

FAILS = []
NOTES = []


def check(cond, what):
    if not cond:
        FAILS.append(what)
        print("FAIL  " + what, flush=True)
    return cond


def parse_opts(tokens):
    """az_run options as an ordered dict: option -> value, or None for a flag"""
    flags = {"--ecores", "--no-lock", "--allow-pageable", "--large-pages", "--no-pin", "--quiet", "--lab"}
    d, i = {}, 0
    while i < len(tokens):
        t = tokens[i]
        if t in flags:
            d[t] = None
            i += 1
        else:
            if i + 1 >= len(tokens):
                raise SystemExit(f"option {t} needs a value")
            d[t] = tokens[i + 1]
            i += 2
    return d


def to_args(d):
    out = []
    for k, v in d.items():
        out.append(k)
        if v is not None:
            out.append(v)
    return out


class Run:
    def __init__(self, label, code, out, err, secs, cmd):
        self.label, self.code, self.out, self.err, self.secs, self.cmd = label, code, out, err, secs, cmd

        def f(pattern, cast=int):
            m = re.search(pattern, out, re.M)
            return cast(m.group(1)) if m else None

        self.hash = f(r"^final hash ([0-9a-f]{16})", str)
        self.births = f(r"^births (\d+) ")
        self.deaths = f(r"^births \d+ deaths (\d+) ")
        self.denied = f(r"^births \d+ deaths \d+ cap_denied (\d+) ")
        self.workers = f(r"workers=(\d+)")
        self.locked = f(r"^arena: .*locked=(\d)")
        self.live = f(r"; live (\d+) \(slots")
        self.ms = f(r"; ([0-9.]+) ms/tick;", float)
        m = re.search(r"^pairs:((?: [a-z]+ \d+)+) \(total (\d+)\)", out, re.M)
        self.pairs = dict((k, int(v)) for k, v in re.findall(r" ([a-z]+) (\d+)", m.group(1))) if m else None
        self.pairs_total = int(m.group(2)) if m else None
        m = re.search(r"^matter: \d+ -> \d+ \((\w+)\)\s+energy invariant: -?\d+ -> -?\d+ \((\w+)\)", out, re.M)
        self.exact = bool(m) and m.group(1) == "exact" and m.group(2) == "exact"
        m = re.search(r"^field_underflow (\d+) dropped_events (\d+) audit_failures (\d+)", out, re.M)
        self.clean = bool(m) and all(int(x) == 0 for x in m.groups())
        self.trace = re.findall(r"^tick (\d+) live (\d+) hash ([0-9a-f]{16})", out, re.M)
        m = re.search(r"^ledger: (\d+) records written to .*?, (\d+) dropped, (\d+) write errors; "
                      r"chain ([0-9a-f]{16}) (equals|DIFFERS FROM) the world's", out, re.M)
        self.records = int(m.group(1)) if m else None
        self.chain = m.group(4) if m else None
        self.ledger_ok = bool(m) and m.group(2) == "0" and m.group(3) == "0" and m.group(5) == "equals"

    def counters(self):
        return (self.births, self.deaths, self.denied, self.live, self.pairs_total)


class Matrix:
    def __init__(self, exe, work):
        self.exe, self.work, self.n = exe, work, 0
        self.cmds = open(os.path.join(work, "commands.txt"), "w", newline="\n")

    def run(self, label, opts, ledger=True):
        self.n += 1
        stem = os.path.join(self.work, f"run{self.n:02d}")
        d = dict(opts)
        if ledger:
            d["--ledger"] = stem + ".ldg"
        cmd = [self.exe] + to_args(d)
        self.cmds.write(" ".join(cmd) + "\n")
        self.cmds.flush()
        t0 = time.time()
        try:
            p = subprocess.run(cmd, capture_output=True, text=True, timeout=1800)
            code, out, err = p.returncode, p.stdout, p.stderr
        except subprocess.TimeoutExpired:
            code, out, err = -999, "", "timeout after 1800 s"
        with open(stem + ".log", "w", newline="\n") as f:
            f.write("$ " + " ".join(cmd) + "\n" + out + ("\n[stderr]\n" + err if err else "") + f"\n[exit {code}]\n")
        r = Run(label, code, out, err, time.time() - t0, cmd)
        check(code == 0, f"{label}: az_run exit {code}" + (f" ({err.strip()[:200]})" if err.strip() else ""))
        check(r.exact, f"{label}: matter and energy exact")
        check(r.clean, f"{label}: field_underflow, dropped_events and audit_failures all 0")
        check(r.locked == 1, f"{label}: arena locked in RAM (locked={r.locked})")
        if ledger:
            check(r.ledger_ok, f"{label}: ledger complete and on the world's chain")
        return r


def main():
    argv = sys.argv[1:]
    if "--" not in argv or len(argv) < 4:
        print(__doc__)
        return 2
    cut = argv.index("--")
    head, world_tokens = argv[:cut], argv[cut + 1:]
    if len(head) < 2:
        print(__doc__)
        return 2
    exe, work, switches = head[0], head[1], head[2:]
    interacting = "--interacting" in switches
    expect_denied = "--expect-denied" in switches
    light = "--light" in switches
    expect = None
    known = {"--interacting", "--expect-denied", "--light", "--expect"}
    i = 0
    while i < len(switches):
        if switches[i] not in known:
            print(f"unknown switch {switches[i]}\n{__doc__}")
            return 2
        if switches[i] == "--expect":
            if i + 1 >= len(switches) or not re.fullmatch(r"[0-9a-f]{16}", switches[i + 1]):
                print("--expect needs a 16-digit lower-case hexadecimal hash")
                return 2
            expect = switches[i + 1]
            i += 1
        i += 1
    world = parse_opts(world_tokens)
    for banned in ("--no-lock", "--allow-pageable", "--quiet", "--threads", "--save", "--load", "--ledger",
                   "--hash-every", "--audit-every"):
        if banned in world:
            print(f"the world options may not contain {banned}: the matrix sets it (or the laws forbid it)")
            return 2
    if "--ticks" not in world or not world["--ticks"].isdigit() or int(world["--ticks"]) < 10:
        print("the world options need --ticks T with T >= 10")
        return 2
    ticks = int(world["--ticks"])
    # The matrix compares state hashes between runs, and only a lab world can
    # be run twice: az_run is wild unless --lab is given (no seed decides a
    # wild run, and no two wild runs are the same).
    world = {"--lab": None, **{k: v for k, v in world.items() if k != "--lab"}}
    os.makedirs(work, exist_ok=True)
    mx = Matrix(exe, work)
    base = dict(world)
    base["--hash-every"] = "5"
    base["--audit-every"] = "1"
    kern_other = "0" if base.get("--kernels", "0") == "1" else "1"

    def variant(**kv):
        d = dict(base)
        for k, v in kv.items():
            key = "--" + k.replace("_", "-")
            if v is False:
                d.pop(key, None)
            else:
                d[key] = None if v is True else str(v)
        return d

    print("world: " + " ".join(to_args(world)) + f"   ({os.path.basename(exe)})", flush=True)
    ref = mx.run("reference: 1 worker", variant(threads=1))
    if ref.hash is None or ref.code != 0:
        print("FAILED: the reference run did not finish; nothing to compare")
        return 1
    m = re.search(r"^cpu: (\d+) logical, (\d+) P cores, (\d+) E cores", ref.out, re.M)
    pcores, ecores = (int(m.group(2)), int(m.group(3))) if m else (1, 0)
    print(f"reference: hash {ref.hash}, live {ref.live}, births {ref.births}, deaths {ref.deaths}, "
          f"refused for want of a slot {ref.denied}, pairs {ref.pairs_total}, ledger records {ref.records}, "
          f"chain {ref.chain}; {ref.ms} ms/tick; machine {pcores} P + {ecores} E cores", flush=True)
    if ref.pairs_total:
        print("  pairs: " + ", ".join(f"{k} {v}" for k, v in ref.pairs.items() if v))
    check(len(ref.trace) == ticks // 5, f"reference: {len(ref.trace)} trace lines for {ticks} ticks")
    if expect:
        check(ref.hash == expect, f"reference hash {ref.hash} is the recorded {expect}")

    def workers_opts(n, pin=True):
        kv = {"threads": n}
        if n > pcores:
            kv["ecores"] = True
        if not pin:
            kv["no_pin"] = True
        return kv

    plan = []     # (label, option overrides, workers asked)
    counts = [2, 8, 24] if light else [2, 5, 8, 12, 16, 24]
    for n in counts:
        plan.append((f"{n} workers", workers_opts(n), n))
    plan.append(("8 workers, unpinned", workers_opts(8, pin=False), 8))
    plan.append(("24 workers, unpinned", workers_opts(24, pin=False), 24))
    if light:
        plan.append(("reorder 7, 8 workers", dict(workers_opts(8), reorder=7), 8))
        plan.append(("split seed 777, reorder 9, 6 workers", dict(workers_opts(6), reorder=9, split_seed=777), 6))
        plan.append((f"kernels {kern_other}, 8 workers", dict(workers_opts(8), kernels=kern_other), 8))
    else:
        for rp in (7, 31, 255):
            plan.append((f"reorder {rp}, 1 worker", dict(threads=1, reorder=rp), 1))
        plan.append(("reorder 7, 8 workers", dict(workers_opts(8), reorder=7), 8))
        plan.append(("reorder 31, 16 workers", dict(workers_opts(16), reorder=31), 16))
        plan.append(("reorder 255, 24 workers, unpinned", dict(workers_opts(24, pin=False), reorder=255), 24))
        plan.append(("split seed 12345, 1 worker", dict(threads=1, split_seed=12345), 1))
        plan.append(("split seed 777, reorder 9, 6 workers", dict(workers_opts(6), reorder=9, split_seed=777), 6))
        plan.append(("split seed 99, reorder 13, 24 workers", dict(workers_opts(24), reorder=13, split_seed=99), 24))
        plan.append((f"kernels {kern_other}, 1 worker", dict(threads=1, kernels=kern_other), 1))
        plan.append((f"kernels {kern_other}, 8 workers, reorder 7", dict(workers_opts(8), kernels=kern_other, reorder=7), 8))

    def compare(r, label):
        ok = check(r.hash == ref.hash, f"{label}: final hash {r.hash}, reference {ref.hash}")
        first = next((a[0] for a, b in zip(ref.trace, r.trace) if a != b), None)
        ok &= check(r.trace == ref.trace,
                    f"{label}: hash trace equal" + (f" (first difference at tick {first})" if first else
                                                    f" ({len(r.trace)} lines against {len(ref.trace)})"))
        ok &= check(r.chain == ref.chain and r.records == ref.records,
                    f"{label}: ledger chain {r.chain} ({r.records} records), reference {ref.chain} ({ref.records})")
        ok &= check(r.counters() == ref.counters(), f"{label}: births, deaths, refusals, live and pairs equal "
                    f"({r.counters()} against {ref.counters()})")
        return ok

    seen_workers = {1}
    for label, kv, asked in plan:
        r = mx.run(label, variant(**kv))
        if r.workers != asked:
            # a machine with fewer cores gives fewer workers: say so, and count the run for what it was
            NOTES.append(f"{label}: asked for {asked} workers, the machine gave {r.workers}")
        if r.workers:
            seen_workers.add(r.workers)
        ok = compare(r, label)
        print(f"  {label:<44} {r.hash}  {'equal' if ok else 'DIFFERS'}  {r.ms} ms/tick, {r.workers} workers", flush=True)

    # a snapshot saved mid-run and resumed at another worker count
    resumes = [(ticks // 2, 8, 3)] if light else [(ticks // 2, 8, 3), (ticks // 3, 1, 12), ((ticks * 7) // 10, 5, 1)]
    for k, w_a, w_b in resumes:
        k -= k % 5                      # keep the trace lines of both halves on the reference's ticks
        if k <= 0 or k >= ticks:
            continue
        label = f"saved at tick {k} on {w_a}, resumed on {w_b}"
        snap = os.path.join(work, f"at{k}.snap")
        a = dict(variant(**workers_opts(w_a)))
        a["--ticks"] = str(k)
        a["--save"] = snap
        ra = mx.run(label + " (first part)", a)
        b = {"--load": snap, "--ticks": str(ticks - k), "--threads": str(w_b), "--hash-every": "5", "--audit-every": "1"}
        if w_b > pcores:
            b["--ecores"] = None
        rb = mx.run(label + " (second part)", b)
        ok = check(rb.hash == ref.hash, f"{label}: final hash {rb.hash}, reference {ref.hash}")
        ok &= check(ra.trace + rb.trace == ref.trace, f"{label}: the two parts' hash traces make the reference's")
        ok &= check(rb.chain == ref.chain, f"{label}: ledger chain {rb.chain}, reference {ref.chain}")
        ok &= check(None not in (ra.records, rb.records) and ra.records + rb.records == ref.records,
                    f"{label}: ledger records {ra.records} + {rb.records} = {ref.records}")
        ok &= check(rb.counters() == ref.counters(), f"{label}: births, deaths, refusals, live and pairs equal "
                    f"({rb.counters()} against {ref.counters()})")
        print(f"  {label:<44} {rb.hash}  {'equal' if ok else 'DIFFERS'}", flush=True)

    check(len(seen_workers) >= 4, f"at least 4 different worker counts ran (ran: {sorted(seen_workers)})")

    if expect_denied:
        check(ref.denied is not None and ref.denied > 0, f"capacity binds: {ref.denied} births refused for want of a slot")
    if interacting:
        n0 = int(world.get("--n", "100000"))
        check(ref.deaths is not None and ref.deaths > n0 // 10, f"deaths {ref.deaths} > a tenth of the {n0} founders")
        check(ref.births is not None and ref.births > 0, f"births {ref.births} > 0")
        for kind in ("attack", "share", "trade", "mate"):
            got = (ref.pairs or {}).get(kind, 0)
            check(got > 0, f"resolved {kind} pairs {got} > 0")
        les = mx.run("every pair lesioned (--lesion 0x8)", variant(threads=1, lesion="0x8"))
        check(les.pairs_total == 0, f"lesion 0x8: {les.pairs_total} pairs resolved (expected 0)")
        check(les.hash is not None and les.hash != ref.hash, f"lesion 0x8: hash {les.hash} differs from the intact {ref.hash}")
        print(f"  every pair lesioned: {les.pairs_total} pairs, hash {les.hash} (intact {ref.hash})")
    else:
        print(f"  note: this world has deaths {ref.deaths} and {ref.pairs_total} resolved pairs; equality on a world "
              "without them is weak evidence (CONTINUE-BUILD section 3)")

    for n in NOTES:
        print("  note: " + n)
    print("  note: ms/tick includes the audit of every tick, the hash trace and the ledger file; it is not a benchmark")
    mx.cmds.close()
    print(f"{'FAILED' if FAILS else 'ok'}: {mx.n} runs, {len(FAILS)} failures")
    return 1 if FAILS else 0


if __name__ == "__main__":
    sys.exit(main())
