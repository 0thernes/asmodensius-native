"""ctest cli_live_smoke: az_live, the live world behind demo/live.html.

Checks, on a small world (2,000 founders, 20 ticks):
- --seed without --lab is refused with exit code 2;
- a wild run with no command pipe runs to --max-ticks and ends with exit code
  0; its first line says mode "wild"; every frame line keeps matter and energy
  equal to the start (unbalanced 0) and has the same state hash before and
  after the view drew it; every frame is a PNG; the end line agrees;
- two --lab --seed 7 runs end on the same state hash, and that hash equals
  the final hash of az_run --lab with the same options: the live world is the
  engine's world, not a copy of it;
- commands on a pipe: pause, step 3, camera, save FILE and quit are obeyed in
  order (a frame of exactly 3 ticks while paused, a saved event, an end
  event, exit code 0), and az_run --load takes the saved world.

Usage: python -I live_smoke.py AZ_LIVE AZ_RUN WORK_DIR
Exit code 0 when every check holds, 1 otherwise. Files stay in WORK_DIR.
"""
import json
import os
import queue
import re
import shutil
import subprocess
import sys
import threading

FAILS = []
WORLD = ["--n", "2000", "--cap", "8000", "--grid", "6,6,5"]
TICKS = 20


def check(cond, what):
    print(("ok    " if cond else "FAIL  ") + what)
    if not cond:
        FAILS.append(what)


def is_png(path):
    try:
        with open(path, "rb") as f:
            return f.read(8) == b"\x89PNG\r\n\x1a\n" and os.path.getsize(path) > 1000
    except OSError:
        return False


def fresh(path):
    shutil.rmtree(path, ignore_errors=True)
    os.makedirs(path)
    return path


def lines_of(text):
    out = []
    for ln in text.splitlines():
        ln = ln.strip()
        if ln.startswith("{"):
            try:
                out.append(json.loads(ln))
            except ValueError:
                out.append({"bad_json": ln})
    return out


def batch(live, frames, extra):
    """az_live with no command pipe, to --max-ticks; returns (code, start, frames, end, stderr)."""
    p = subprocess.run([live, "--dir", frames, "--every", "5", "--fps", "60", "--size", "320x180",
                        "--max-ticks", str(TICKS)] + WORLD + extra,
                       stdin=subprocess.DEVNULL, capture_output=True, text=True, timeout=600)
    objs = lines_of(p.stdout)
    start = next((o for o in objs if o.get("event") == "start"), None)
    end = next((o for o in objs if o.get("event") == "end"), None)
    fr = [o for o in objs if "frame" in o]
    return p.returncode, start, fr, end, p.stderr


def final_hash(run, extra):
    p = subprocess.run([run, "--ticks", str(TICKS)] + WORLD + extra, capture_output=True, text=True, timeout=600)
    m = re.search(r"^final hash ([0-9a-f]{16})", p.stdout, re.M)
    return p.returncode, m.group(1) if m else None


def main():
    if len(sys.argv) != 4:
        print(__doc__)
        return 2
    live, run, work = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(work, exist_ok=True)

    p = subprocess.run([live, "--dir", fresh(os.path.join(work, "refused")), "--seed", "7"] + WORLD,
                       stdin=subprocess.DEVNULL, capture_output=True, text=True, timeout=120)
    check(p.returncode == 2, "--seed without --lab is refused (exit %d)" % p.returncode)

    # a wild run
    d = fresh(os.path.join(work, "wild"))
    code, start, fr, end, err = batch(live, d, [])
    check(code == 0, "wild run exit code 0 (got %d; %s)" % (code, err.strip()[-200:]))
    check(bool(start) and start.get("mode") == "wild" and bool(start.get("origin")), "wild run starts in mode wild with an origin")
    check(len(fr) == TICKS // 5 + 1, "wild run printed %d frame lines (frame 0 at tick 0, then every 5 ticks)" % len(fr))
    check(bool(fr) and fr[0].get("tick") == 0 and fr[-1].get("tick") == TICKS, "frames run from tick 0 to tick %d" % TICKS)
    check(all(f.get("unbalanced") == 0 for f in fr), "matter and energy equal the start after every tick")
    check(all(f.get("hash") == f.get("hash_after_view") for f in fr), "the view never changed the state hash")
    check(all(is_png(os.path.join(d, f.get("file", ""))) for f in fr), "every frame is a PNG")
    check(bool(end) and end.get("unbalanced") == 0 and end.get("observer_fail") == 0 and end.get("write_fail") == 0,
          "the end line reports no broken books, no observer change and no failed write")
    check(bool(fr) and fr[-1].get("live", 0) > 0 and fr[-1].get("births", 0) > 0, "the world is alive (births, living creatures)")

    # two lab runs and az_run
    lab = ["--lab", "--seed", "7"]
    c1, _, f1, e1, _ = batch(live, fresh(os.path.join(work, "lab1")), lab)
    c2, _, f2, e2, _ = batch(live, fresh(os.path.join(work, "lab2")), lab)
    h1, h2 = (e1 or {}).get("hash"), (e2 or {}).get("hash")
    check(c1 == 0 and c2 == 0, "two lab runs exit 0")
    check(h1 is not None and h1 == h2, "two lab runs end on the same hash (%s, %s)" % (h1, h2))
    rc, hr = final_hash(run, lab)
    check(rc == 0 and hr == h1, "az_run --lab with the same options ends on the same hash (%s)" % hr)
    wc, wh = (lambda r: (r[0], r[3]))(batch(live, fresh(os.path.join(work, "wild2")), []))
    check(wc == 0 and bool(end) and (wh or {}).get("hash") != end.get("hash"), "a second wild run ends on a different hash")

    # commands on a pipe
    d = fresh(os.path.join(work, "pipe"))
    snap = os.path.join(work, "pipe.azsnap")
    if os.path.exists(snap):
        os.remove(snap)
    proc = subprocess.Popen([live, "--dir", d, "--every", "1", "--fps", "60", "--size", "320x180"] + WORLD + lab,
                            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, bufsize=1)
    q = queue.Queue()

    def reader():
        for ln in proc.stdout:
            q.put(ln)
        q.put(None)

    threading.Thread(target=reader, daemon=True).start()

    def wait_for(pred, seconds=60):
        while True:
            try:
                ln = q.get(timeout=seconds)
            except queue.Empty:
                return None
            if ln is None:
                return None
            ln = ln.strip()
            if not ln.startswith("{"):
                continue
            try:
                o = json.loads(ln)
            except ValueError:
                continue
            if pred(o):
                return o

    def send(cmd):
        proc.stdin.write(cmd + "\n")
        proc.stdin.flush()

    try:
        check(wait_for(lambda o: o.get("event") == "start") is not None, "pipe run: start line")
        send("pause")
        send("step 3")
        f3 = wait_for(lambda o: o.get("ticks") == 3 and o.get("paused") is True)
        check(f3 is not None, "pipe run: pause then step 3 gives one frame of 3 ticks while paused")
        send("camera 1.2 0.3 0")
        fc = wait_for(lambda o: "frame" in o and o.get("camera", [0])[0] == 1.2)
        check(fc is not None and fc.get("ticks") == 0, "pipe run: a camera change redraws without ticking")
        send("save " + snap)
        sv = wait_for(lambda o: o.get("event") in ("saved", "error"))
        check(bool(sv) and sv.get("event") == "saved" and sv.get("bytes", 0) > 0, "pipe run: save writes a snapshot")
        send("quit")
        en = wait_for(lambda o: o.get("event") == "end")
        check(en is not None and en.get("unbalanced") == 0, "pipe run: quit ends with an end line, books exact")
        code = proc.wait(timeout=60)
        check(code == 0, "pipe run: exit code 0 (got %d)" % code)
    finally:
        if proc.poll() is None:
            proc.kill()
    p = subprocess.run([run, "--load", snap, "--ticks", "5", "--hash-every", "1"], capture_output=True, text=True, timeout=600)
    check(p.returncode == 0 and "loaded:" in p.stdout, "az_run --load takes the saved world (exit %d)" % p.returncode)

    trace = re.findall(r"^tick \d+ live (\d+) hash [0-9a-f]{16}$", p.stdout, re.M)
    final = re.search(r"^ticks 5 .*; live (\d+) \(slots", p.stdout, re.M)
    check(bool(trace and final) and trace[-1] == final[1], "last hash trace counts end-of-tick living slots")
    zero = subprocess.run([run, "--load", snap, "--ticks", "0"], capture_output=True, text=True, timeout=60)
    loaded = re.search(r"^loaded: .* at tick (\d+), live (\d+), hash ([0-9a-f]{16})$", zero.stdout, re.M)
    check(zero.returncode == 0 and bool(fc and loaded) and
          (int(loaded[1]), int(loaded[2]), loaded[3]) == (fc["tick"], fc["live"], fc["hash"]),
          "zero-tick checkpoint count, tick and hash match the saved picture")
    check("no simulation steps timed" in zero.stdout and
          not re.search(r"\b(?:nan|inf)\b", zero.stdout, re.I),
          "a zero-tick inspection reports its count without undefined timing ratios")

    print("%s: %d checks failed" % ("FAILED" if FAILS else "ok", len(FAILS)))
    return 1 if FAILS else 0


if __name__ == "__main__":
    sys.exit(main())
