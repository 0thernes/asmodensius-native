#!/usr/bin/env python3
"""Asmodensius-Native live demo: a local web GUI over the engine's own programs.

    python -I demo/live_server.py [--port 8765] [--build DIR] [--open]

Python 3.9 or newer, standard library only. It serves demo/live.html on
http://127.0.0.1:PORT (this computer only; no other machine can reach it) and
drives three programs of a build folder:

  az_live  one living world, rendered every few ticks (PNG frames and one JSON
           line per frame; commands go to its stdin)
  az_run   the checks: lab repeatability, thread independence, wild
           uniqueness, a lesion A/B contrast, the reference worlds
  ctest    the full test suite of the build folder

Programs are started with argument lists, never through a shell. Everything a
run writes (frames, saved worlds, logs) goes under build/demo/, which git
ignores. The lesion switches and the world parameters are read from
core/az_abi.h and core/az_params.def, so the page always matches the engine.
"""
import argparse
import collections
import json
import os
import re
import shutil
import subprocess
import sys
import threading
import time
import uuid
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

DEMO = Path(__file__).resolve().parent
ROOT = DEMO.parent
WORK = ROOT / "build" / "demo"
FRAMES = WORK / "frames"
WORLDS = WORK / "worlds"
LOGS = WORK / "logs"
NO_WINDOW = getattr(subprocess, "CREATE_NO_WINDOW", 0)

NAME_RE = re.compile(r"^[A-Za-z0-9_-]{1,48}$")
RUN_RE = re.compile(r"^[0-9A-Za-z-]{1,64}$")
FRAME_RE = re.compile(r"^frame-\d{8}\.png$")
PARAM_RE = re.compile(r"^[A-Za-z0-9_]{1,40}=(0x[0-9A-Fa-f]{1,8}|\d{1,10})$")


# ---- what the engine declares ---------------------------------------------------

def read_lesions():
    """[(bit, NAME, what)] from the AZ_LESION_* enum of core/az_abi.h."""
    text = (ROOT / "core" / "az_abi.h").read_text(encoding="utf-8", errors="replace")
    out = []
    for m in re.finditer(r"AZ_LESION_([A-Z0-9_]+)\s*=\s*1u\s*<<\s*(\d+)\s*,?\s*/\*\s*(.*?)\s*\*/", text):
        out.append({"bit": int(m.group(2)), "name": m.group(1), "what": m.group(3)})
    return sorted(out, key=lambda d: d["bit"])


def read_params():
    """[{name, default, lo, hi, what, group}] from core/az_params.def."""
    path = ROOT / "core" / "az_params.def"
    if not path.exists():
        return []
    out = []

    def collect(source, group):
        for line in source.read_text(encoding="utf-8").splitlines():
            include = re.fullmatch(r'\s*#include "([a-zA-Z0-9_/]+\.def)"\s*', line)
            if include:
                child = source.parent / include.group(1)
                collect(child, child.stem.removeprefix("az_").capitalize())
                continue
            g = re.match(r"\s*/\*\s*-+\s*(.*?)\s*-+\s*\*/", line)
            if g:
                group = g.group(1)
                continue
            m = re.match(r'\s*AZ_PARAM\(\s*(\w+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*"((?:[^"\\]|\\.)*)"\s*\)', line)
            if m:
                out.append({"name": m.group(1), "default": int(m.group(2)), "lo": int(m.group(3)),
                            "hi": int(m.group(4)), "what": m.group(5), "group": group})
    collect(path, "")
    return out


def read_references():
    """The newest recorded hash for every az_run command in tests/golden_hashes.txt that runs a lab world."""
    path = ROOT / "tests" / "golden_hashes.txt"
    if not path.exists():
        return []
    refs = collections.OrderedDict()
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        parts = [p.strip() for p in line.split("|")]
        if len(parts) < 3:
            continue
        hfield = parts[1].split("->")[-1]          # "old -> new": the recorded hash is the new one
        h = re.search(r"\b([0-9a-f]{16})\b", hfield)
        m = re.search(r"az_run(?:\.exe)?\s+(.*)$", parts[2])
        if not h or not m:
            continue
        args = re.split(r"\s+", re.sub(r"\(.*?\)", "", m.group(1)).strip())
        if "--lab" not in args:
            continue                      # a wild run has no reference hash
        if not all(re.match(r"^(--[a-z-]+|[0-9A-Za-z,x]+)$", a) for a in args):
            continue
        key = " ".join(args)
        refs.pop(key, None)
        refs[key] = {"date": parts[0], "hash": h.group(1), "args": args, "note": parts[3] if len(parts) > 3 else ""}
    return list(refs.values())


# ---- build folder ---------------------------------------------------------------

def find_build(arg):
    cands = []
    if arg:
        cands.append(Path(arg))
    if os.environ.get("AZ_BUILD"):
        cands.append(Path(os.environ["AZ_BUILD"]))
    cands += [ROOT / "build" / "z", ROOT / "build" / "z-full"]
    for c in cands:
        if (c / "az_live.exe").exists():
            return c.resolve()
    found = [p.parent for p in (ROOT / "build").glob("*/az_live.exe")]
    found.sort(key=lambda p: (p / "az_live.exe").stat().st_mtime, reverse=True)
    return found[0].resolve() if found else None


def find_ctest():
    c = shutil.which("ctest")
    if c:
        return c
    for p in (Path(os.environ.get("ProgramFiles", r"C:\Program Files")) / "CMake" / "bin" / "ctest.exe",):
        if p.exists():
            return str(p)
    return None


# ---- the living world (az_live) -------------------------------------------------

COMPACT = ("tick", "live", "eggs", "clades", "colonies", "max_gen", "mean_energy", "births", "deaths",
           "cap_denied", "unbalanced", "ms_tick", "ms_render", "ticks")


class LiveRun:
    def __init__(self, build):
        self.build = build
        self.lock = threading.Lock()       # the state the readers update
        self.ctl = threading.Lock()        # start and stop, one at a time: two quick starts must not orphan a world
        self.proc = None
        self.run_id = None
        self.frames_dir = None
        self.start = None
        self.last = None
        self.history = collections.deque(maxlen=5000)
        self.events = collections.deque(maxlen=300)
        self.stderr = collections.deque(maxlen=60)
        self.exit_code = None
        self.idx = 0
        self.eseq = 0
        self.options = None
        self.pending_meta = {}

    # --- lifecycle
    def start_world(self, o):
        with self.ctl:
            return self._start_unlocked(o)

    def _start_unlocked(self, o):
        self._stop_unlocked()
        exe = self.build / "az_live.exe"
        run_id = time.strftime("%Y%m%d-%H%M%S") + "-" + uuid.uuid4().hex[:6]
        frames = FRAMES / run_id
        frames.mkdir(parents=True, exist_ok=True)
        self._clean_old_runs(keep=3, current=run_id)
        args = [str(exe), "--dir", str(frames), "--size", "%dx%d" % (o["width"], o["height"]),
                "--every", str(o["every"]), "--fps", str(o["fps"]), "--keep", "10", "--threads", str(o["threads"])]
        if o.get("load"):
            args += ["--load", str(WORLDS / (o["load"] + ".azsnap"))]
        else:
            args += ["--n", str(o["n"]), "--species", str(o["species"]), "--cap", str(o["cap"])]
            if o["lab"]:
                args += ["--lab", "--seed", str(o["seed"])]
            if o["lesion"]:
                args += ["--lesion", "0x%x" % o["lesion"]]
            if o["params"]:
                args += ["--param", ",".join(o["params"])]
            if o.get("grid"):
                args += ["--grid", o["grid"]]
        if o.get("kernels"):
            args += ["--kernels", "1"]
        if o.get("ecores"):
            args += ["--ecores"]
        if o.get("gpu"):
            args += ["--gpu", "1"]
        if o.get("prefetch") is not None:
            args += ["--prefetch", "1" if o["prefetch"] else "0"]
        if o.get("large_pages"):
            args += ["--large-pages"]
        proc = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                cwd=str(WORK), creationflags=NO_WINDOW, bufsize=0)
        with self.lock:
            self.proc, self.run_id, self.frames_dir = proc, run_id, frames
            self.start, self.last, self.exit_code = None, None, None
            self.history.clear()
            self.events.clear()
            self.stderr.clear()
            self.options = dict(o, argv=args[1:])
        threading.Thread(target=self._read_out, args=(proc, run_id), daemon=True).start()
        threading.Thread(target=self._read_err, args=(proc, run_id), daemon=True).start()
        return run_id

    def stop(self, timeout=4.0):
        with self.ctl:
            self._stop_unlocked(timeout)

    def _stop_unlocked(self, timeout=4.0):
        with self.lock:
            proc = self.proc
        if not proc or proc.poll() is not None:
            return
        self.send("quit")
        try:
            proc.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait(timeout=5)

    def send(self, cmd):
        with self.lock:
            proc = self.proc
            if not proc or proc.poll() is not None or not proc.stdin:
                return False
            try:
                proc.stdin.write((cmd + "\n").encode("utf-8"))
                proc.stdin.flush()
                return True
            except OSError:
                return False

    def running(self):
        with self.lock:
            return bool(self.proc and self.proc.poll() is None)

    # --- readers
    def _event(self, obj):
        self.eseq += 1
        obj["seq"] = self.eseq
        obj["at"] = time.strftime("%H:%M:%S")
        self.events.append(obj)

    def _read_out(self, proc, run_id):
        for raw in iter(proc.stdout.readline, b""):
            line = raw.decode("utf-8", "replace").strip()
            if not line:
                continue
            try:
                obj = json.loads(line)
            except ValueError:
                with self.lock:
                    if self.run_id == run_id:
                        self.stderr.append(line)
                continue
            with self.lock:
                if self.run_id != run_id:
                    return
                if "event" in obj:
                    if obj["event"] == "start":
                        self.start = obj
                    if obj["event"] == "saved":
                        self._write_meta(obj)
                    self._event(obj)
                else:
                    self.idx += 1
                    obj["idx"] = obj.get("idx", self.idx)
                    self.last = obj
                    rec = {k: obj.get(k) for k in COMPACT}
                    rec["idx"] = self.idx
                    rec["pairs"] = obj.get("pairs", {})
                    census = obj.get("census", {})
                    for key in ("species", "looks", "diet_unknown"):
                        if key in census:
                            rec[key] = census[key]
                    body = obj.get("body", {})
                    for k in ("genes", "neurons", "parts", "units"):
                        v = body.get(k)
                        rec[k] = v[2] if isinstance(v, list) and len(v) == 3 else None
                    self.history.append(rec)
        code = proc.wait()
        with self.lock:
            if self.run_id == run_id:
                self.exit_code = code
                self._event({"event": "exited", "code": code})

    def _read_err(self, proc, run_id):
        for raw in iter(proc.stderr.readline, b""):
            line = raw.decode("utf-8", "replace").rstrip()
            if line:
                with self.lock:
                    if self.run_id == run_id:
                        self.stderr.append(line)

    def _write_meta(self, saved):
        try:
            f = Path(saved.get("file", ""))
            if f.parent.resolve() != WORLDS.resolve():
                return
            meta = {"saved": saved, "start": self.start, "options": self.options,
                    "last": {k: (self.last or {}).get(k) for k in ("tick", "live", "clades", "births", "deaths")},
                    "when": time.strftime("%Y-%m-%d %H:%M:%S")}
            f.with_suffix(".json").write_text(json.dumps(meta, indent=1), encoding="utf-8")
        except OSError:
            pass

    def _clean_old_runs(self, keep, current):
        try:
            runs = sorted([p for p in FRAMES.iterdir() if p.is_dir() and p.name != current],
                          key=lambda p: p.stat().st_mtime, reverse=True)
        except OSError:
            return
        for p in runs[keep - 1:]:
            shutil.rmtree(p, ignore_errors=True)

    # --- state for the page
    def state(self, since, esince):
        with self.lock:
            return {
                "running": bool(self.proc and self.proc.poll() is None),
                "run_id": self.run_id,
                "start": self.start,
                "last": self.last,
                "options": self.options,
                "frames": [r for r in self.history if r["idx"] > since],
                "events": [e for e in self.events if e["seq"] > esince],
                "stderr": list(self.stderr)[-20:],
                "exit_code": self.exit_code,
            }


# ---- checks and the suite (az_run, ctest) ------------------------------------------

RUN_PATTERNS = {
    "hash": re.compile(r"^final hash ([0-9a-f]{16})", re.M),
    "matter": re.compile(r"^matter: (\d+) -> (\d+) \((\w+)\)\s+energy invariant: (-?\d+) -> (-?\d+) \((\w+)\)", re.M),
    "life": re.compile(r"^births (\d+) deaths (\d+) cap_denied (\d+) clades (\d+) colonies (\d+) max_generation (\d+)", re.M),
    "speed": re.compile(r"^ticks (\d+) in ([\d.]+) s = ([\d.]+) ticks/s; ([\d.]+) ms/tick; live (\d+)", re.M),
    "pairs": re.compile(r"^pairs:(.*)\(total (\d+)\)", re.M),
    "mode": re.compile(r"^mode: (WILD|LAB)\b.*?(?:origin ([0-9a-f]{16})|seed (\d+))", re.M),
}


def parse_run(text):
    r = {}
    m = RUN_PATTERNS["hash"].search(text)
    r["hash"] = m.group(1) if m else None
    m = RUN_PATTERNS["matter"].search(text)
    if m:
        r["matter_exact"] = m.group(3) == "exact"
        r["energy_exact"] = m.group(6) == "exact"
    m = RUN_PATTERNS["life"].search(text)
    if m:
        r.update(births=int(m.group(1)), deaths=int(m.group(2)), cap_denied=int(m.group(3)),
                 clades=int(m.group(4)), colonies=int(m.group(5)), max_gen=int(m.group(6)))
    m = RUN_PATTERNS["speed"].search(text)
    if m:
        r.update(ticks=int(m.group(1)), seconds=float(m.group(2)), ticks_per_s=float(m.group(3)),
                 ms_tick=float(m.group(4)), live=int(m.group(5)))
    m = RUN_PATTERNS["pairs"].search(text)
    if m:
        r["pairs"] = {k: int(v) for k, v in re.findall(r"(\w+) (\d+)", m.group(1))}
        r["pairs_total"] = int(m.group(2))
    m = RUN_PATTERNS["mode"].search(text)
    if m:
        r["mode"] = m.group(1).lower()
        r["origin"] = m.group(2)
        r["seed"] = m.group(3)
    return r


class Jobs:
    def __init__(self, build):
        self.build = build
        self.lock = threading.Lock()
        self.jobs = collections.OrderedDict()

    def list(self):
        with self.lock:
            return [dict(j, lines=j["lines"][-400:]) for j in self.jobs.values()]

    def busy(self, kind):
        with self.lock:
            return any(j["kind"] == kind and j["state"] == "running" for j in self.jobs.values())

    def _new(self, kind, title):
        jid = uuid.uuid4().hex[:8]
        job = {"id": jid, "kind": kind, "title": title, "state": "running", "started": time.strftime("%H:%M:%S"),
               "ended": None, "lines": [], "result": None, "verdict": None}
        with self.lock:
            self.jobs[jid] = job
            while len(self.jobs) > 30:
                self.jobs.popitem(last=False)
        return job

    def _line(self, job, s):
        with self.lock:
            job["lines"].append(s)

    def _finish(self, job, verdict, result):
        with self.lock:
            job.update(state="done", verdict=verdict, result=result, ended=time.strftime("%H:%M:%S"))

    def _az_run(self, job, args, timeout=900):
        cmd = [str(self.build / "az_run.exe")] + args
        self._line(job, "$ az_run " + " ".join(args))
        t0 = time.time()
        try:
            p = subprocess.run(cmd, cwd=str(WORK), capture_output=True, timeout=timeout, creationflags=NO_WINDOW)
            out = p.stdout.decode("utf-8", "replace") + p.stderr.decode("utf-8", "replace")
            code = p.returncode
        except subprocess.TimeoutExpired:
            out, code = "timed out after %d s" % timeout, -1
        for ln in out.splitlines():
            if ln.strip():
                self._line(job, "  " + ln)
        r = parse_run(out)
        r["exit"] = code
        r["wall_s"] = round(time.time() - t0, 2)
        return r

    def start(self, kind, o):
        titles = {"repeat": "Lab repeatability: same seed, same world, any thread count",
                  "wild": "Wild worlds: never the same twice, books still exact",
                  "lesion": "Lesion A/B: the same lab world with and without a lesion",
                  "reference": "Reference world: the recorded hash",
                  "ctest": "Full test suite (ctest)"}
        if kind not in titles:
            raise ValueError("unknown check")
        if kind == "ctest" and self.busy("ctest"):
            raise ValueError("the suite is already running")
        job = self._new(kind, titles[kind])
        threading.Thread(target=self._run, args=(job, kind, o), daemon=True).start()
        return job["id"]

    def _run(self, job, kind, o):
        try:
            getattr(self, "_check_" + kind)(job, o)
        except Exception as e:  # report, never kill the server
            self._line(job, "error: %r" % (e,))
            self._finish(job, "error", {"error": str(e)})

    def _world_args(self, o):
        return ["--n", str(o["n"]), "--ticks", str(o["ticks"]), "--cap", str(o["cap"]), "--species", str(o["species"])]

    def _check_repeat(self, job, o):
        base = ["--lab", "--seed", str(o["seed"])] + self._world_args(o)
        runs = [self._az_run(job, base), self._az_run(job, base), self._az_run(job, base + ["--threads", "1"])]
        hashes = [r.get("hash") for r in runs]
        exact = all(r.get("matter_exact") and r.get("energy_exact") for r in runs)
        same = hashes[0] is not None and len(set(hashes)) == 1
        self._finish(job, "pass" if same and exact else "fail",
                     {"hashes": hashes, "labels": ["run 1 (all workers)", "run 2 (all workers)", "run 3 (1 thread)"],
                      "same": same, "exact": exact, "runs": runs})

    def _check_wild(self, job, o):
        base = self._world_args(o)
        runs = [self._az_run(job, base), self._az_run(job, base)]
        hashes = [r.get("hash") for r in runs]
        exact = all(r.get("matter_exact") and r.get("energy_exact") for r in runs)
        differ = None not in hashes and hashes[0] != hashes[1]
        self._finish(job, "pass" if differ and exact else "fail",
                     {"hashes": hashes, "origins": [r.get("origin") for r in runs], "labels": ["wild run 1", "wild run 2"],
                      "differ": differ, "exact": exact, "runs": runs})

    def _check_lesion(self, job, o):
        base = ["--lab", "--seed", str(o["seed"])] + self._world_args(o)
        a = self._az_run(job, base)
        b = self._az_run(job, base + ["--lesion", "0x%x" % o["lesion"]])
        exact = all(r.get("matter_exact") and r.get("energy_exact") for r in (a, b))
        self._finish(job, "pass" if exact else "fail",
                     {"lesion": o["lesion"], "intact": a, "lesioned": b, "exact": exact,
                      "changed": a.get("hash") != b.get("hash")})

    def _check_reference(self, job, o):
        refs = read_references()
        pick = [r for r in refs if " ".join(r["args"]) == o.get("ref")] or refs[-1:]
        if not pick:
            self._finish(job, "error", {"error": "no lab reference in tests/golden_hashes.txt"})
            return
        ref = pick[0]
        r = self._az_run(job, ref["args"], timeout=1800)
        ok = r.get("hash") == ref["hash"] and r.get("matter_exact") and r.get("energy_exact")
        self._finish(job, "pass" if ok else "fail",
                     {"expected": ref["hash"], "got": r.get("hash"), "args": ref["args"], "date": ref["date"], "run": r})

    def _check_ctest(self, job, o):
        ctest = find_ctest()
        if not ctest:
            self._finish(job, "error", {"error": "ctest was not found (CMake's bin folder)"})
            return
        cmd = [ctest, "--test-dir", str(self.build), "--output-on-failure", "--timeout", "900"]
        self._line(job, "$ ctest --test-dir " + str(self.build) + " --output-on-failure")
        p = subprocess.Popen(cmd, cwd=str(self.build), stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                             creationflags=NO_WINDOW)
        passed, failed = [], []
        for raw in iter(p.stdout.readline, b""):
            ln = raw.decode("utf-8", "replace").rstrip()
            self._line(job, ln)
            m = re.search(r"Test\s+#\d+:\s+(\S+)\s+\.*\s*(\*{0,3}\s*)?(Passed|Failed|Not Run|Timeout|Exception|\*\*\*\w+)", ln)
            if m:
                (passed if m.group(3) == "Passed" else failed).append(m.group(1))
        code = p.wait()
        self._finish(job, "pass" if code == 0 and not failed else "fail",
                     {"passed": passed, "failed": failed, "exit": code})


# ---- HTTP -----------------------------------------------------------------------

def clamp_int(v, lo, hi, default):
    try:
        v = int(v)
    except (TypeError, ValueError):
        return default
    return max(lo, min(hi, v))


def auto_cap(n):
    if n <= 250000:
        return min(1004096, n * 4 + 4096)
    return max(n + 4096, 1004096)


def world_options(body, lesion_bits):
    n_val = clamp_int(body.get("n"), 1, 2000000, 5000)
    sp_val = clamp_int(body.get("species"), 1, 100, 100)
    o = {
        "n": n_val,
        "species": min(sp_val, n_val),
        "lab": bool(body.get("lab")),
        "seed": clamp_int(body.get("seed"), 0, 2 ** 63 - 1, 42),
        "every": clamp_int(body.get("every"), 1, 10000, 1),
        "fps": clamp_int(body.get("fps"), 1, 60, 8),
        "width": clamp_int(body.get("width"), 64, 4096, 960),
        "height": clamp_int(body.get("height"), 64, 4096, 540),
        "threads": clamp_int(body.get("threads"), 0, 64, 0),
        "ticks": clamp_int(body.get("ticks"), 1, 100000, 200),
        "kernels": bool(body.get("kernels", True)),
        "ecores": bool(body.get("ecores", True)),
        "prefetch": bool(body.get("prefetch", True)),
        "gpu": bool(body.get("gpu", False)),
        "large_pages": bool(body.get("large_pages", False)),
        "lesion": 0,
        "params": [],
        "grid": None,
        "load": None,
    }
    cap = clamp_int(body.get("cap"), 0, 0x7FFFFFC0, 0)
    o["cap"] = max(o["n"], cap) if cap else auto_cap(o["n"])
    for b in body.get("lesions", []) or []:
        b = clamp_int(b, 0, 31, -1)
        if b in lesion_bits:
            o["lesion"] |= 1 << b
    for p in body.get("params", []) or []:
        if isinstance(p, str) and PARAM_RE.match(p.strip()):
            o["params"].append(p.strip().upper().replace("0X", "0x"))
    g = body.get("grid")
    if isinstance(g, str) and re.match(r"^\d{1,2},\d{1,2},\d{1,2}$", g):
        o["grid"] = g
    ld = body.get("load")
    if isinstance(ld, str) and NAME_RE.match(ld) and (WORLDS / (ld + ".azsnap")).exists():
        o["load"] = ld
    ref = body.get("ref")
    if isinstance(ref, str) and len(ref) < 400:
        o["ref"] = ref
    return o


class App:
    def __init__(self, build, port, proxy_ports=()):
        self.build = build
        self.port = port
        self.proxy_ports = tuple(proxy_ports)
        self.live = LiveRun(build)
        self.jobs = Jobs(build)
        self.lesions = read_lesions()
        self.params = read_params()


def make_handler(app):
    allowed_hosts = {"127.0.0.1:%d" % app.port, "localhost:%d" % app.port}
    for pp in app.proxy_ports:
        allowed_hosts.add("127.0.0.1:%d" % pp)
        allowed_hosts.add("localhost:%d" % pp)

    class Handler(BaseHTTPRequestHandler):
        server_version = "asmodensius-demo/1"

        def log_message(self, fmt, *args):  # keep the console quiet
            return

        def _host_ok(self):
            return self.headers.get("Host", "") in allowed_hosts

        def _send(self, code, body, ctype="application/json; charset=utf-8", cache="no-store"):
            data = body if isinstance(body, bytes) else json.dumps(body).encode("utf-8")
            self.send_response(code)
            self.send_header("Content-Type", ctype)
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", cache)
            self.send_header("X-Content-Type-Options", "nosniff")
            self.end_headers()
            self.wfile.write(data)

        def _body(self):
            n = clamp_int(self.headers.get("Content-Length"), 0, 65536, 0)
            if not n:
                return {}
            try:
                v = json.loads(self.rfile.read(n).decode("utf-8"))
                return v if isinstance(v, dict) else {}
            except ValueError:
                return {}

        def do_GET(self):
            if not self._host_ok():
                return self._send(403, {"error": "host"})
            u = urlparse(self.path)
            q = parse_qs(u.query)
            if u.path in ("/", "/index.html", "/live.html"):
                return self._send(200, (DEMO / "live.html").read_bytes(), "text/html; charset=utf-8")
            if u.path == "/api/info":
                return self._send(200, self._info())
            if u.path == "/api/state":
                since = clamp_int((q.get("since") or ["0"])[0], 0, 2 ** 62, 0)
                esince = clamp_int((q.get("esince") or ["0"])[0], 0, 2 ** 62, 0)
                return self._send(200, app.live.state(since, esince))
            if u.path == "/api/jobs":
                return self._send(200, {"jobs": app.jobs.list()})
            if u.path == "/api/worlds":
                return self._send(200, {"worlds": list_worlds()})
            m = re.match(r"^/frame/([^/]+)/([^/]+)$", u.path)
            if m and RUN_RE.match(m.group(1)) and FRAME_RE.match(m.group(2)):
                f = FRAMES / m.group(1) / m.group(2)
                try:
                    data = f.read_bytes()
                except OSError:
                    return self._send(404, {"error": "frame gone"})
                return self._send(200, data, "image/png", "max-age=300")
            return self._send(404, {"error": "not found"})

        def do_POST(self):
            if not self._host_ok():
                return self._send(403, {"error": "host"})
            origin = self.headers.get("Origin")
            if origin and origin.replace("http://", "") not in allowed_hosts:
                return self._send(403, {"error": "origin"})
            u = urlparse(self.path)
            body = self._body()
            bits = {d["bit"] for d in app.lesions}
            try:
                if u.path == "/api/start":
                    if not app.build:
                        return self._send(409, {"error": "no build folder with az_live.exe"})
                    return self._send(200, {"run_id": app.live.start_world(world_options(body, bits))})
                if u.path == "/api/stop":
                    app.live.stop()
                    return self._send(200, {"ok": True})
                if u.path == "/api/cmd":
                    cmd = str(body.get("cmd", "")).strip()
                    if not valid_command(cmd):
                        return self._send(400, {"error": "command not allowed"})
                    return self._send(200, {"ok": app.live.send(cmd)})
                if u.path == "/api/save":
                    name = str(body.get("name", "")).strip()
                    if not NAME_RE.match(name):
                        return self._send(400, {"error": "a name of letters, digits, - and _ (1 to 48)"})
                    WORLDS.mkdir(parents=True, exist_ok=True)
                    return self._send(200, {"ok": app.live.send("save " + str(WORLDS / (name + ".azsnap")))})
                if u.path == "/api/delete-world":
                    name = str(body.get("name", "")).strip()
                    if not NAME_RE.match(name):
                        return self._send(400, {"error": "bad name"})
                    for ext in (".azsnap", ".json"):
                        p = WORLDS / (name + ext)
                        if p.exists():
                            p.unlink()
                    return self._send(200, {"ok": True})
                if u.path == "/api/check":
                    if not app.build:
                        return self._send(409, {"error": "no build folder"})
                    kind = str(body.get("kind", ""))
                    o = world_options(body, bits)
                    return self._send(200, {"job": app.jobs.start(kind, o)})
            except ValueError as e:
                return self._send(400, {"error": str(e)})
            return self._send(404, {"error": "not found"})

        def _info(self):
            exes = {}
            if app.build:
                for name in ("az_live", "az_run", "az_snap", "az_lesion", "az_bench"):
                    p = app.build / (name + ".exe")
                    exes[name] = time.strftime("%Y-%m-%d %H:%M", time.localtime(p.stat().st_mtime)) if p.exists() else None
            return {"root": str(ROOT), "build": str(app.build) if app.build else None, "exes": exes,
                    "lesions": app.lesions, "params": app.params, "references": read_references(),
                    "ctest": bool(find_ctest()), "python": sys.version.split()[0], "cpus": os.cpu_count()}

    return Handler


def valid_command(cmd):
    return bool(re.match(r"^(run|pause|quit|step( \d{1,6})?|every \d{1,5}|fps \d{1,2}|size \d{2,4}x\d{2,4}|"
                         r"camera -?\d{1,4}(\.\d{1,6})? -?\d(\.\d{1,6})? \d{1,7}(\.\d{1,3})?)$", cmd))


def list_worlds():
    out = []
    if not WORLDS.exists():
        return out
    for p in sorted(WORLDS.glob("*.azsnap"), key=lambda p: p.stat().st_mtime, reverse=True):
        meta = {}
        j = p.with_suffix(".json")
        if j.exists():
            try:
                meta = json.loads(j.read_text(encoding="utf-8"))
            except ValueError:
                meta = {}
        out.append({"name": p.stem, "bytes": p.stat().st_size,
                    "when": time.strftime("%Y-%m-%d %H:%M", time.localtime(p.stat().st_mtime)),
                    "tick": (meta.get("saved") or {}).get("tick"), "mode": (meta.get("start") or {}).get("mode"),
                    "live": (meta.get("last") or {}).get("live")})
    return out


def main():
    ap = argparse.ArgumentParser(description="Asmodensius-Native live demo server (this computer only)")
    ap.add_argument("--port", type=int, default=8765)
    ap.add_argument("--build", help="build folder that holds az_live.exe and az_run.exe (default: build/z)")
    ap.add_argument("--open", action="store_true", help="open the page in the default browser")
    ap.add_argument("--autostart", action="store_true", help="immediately boot a live wild world on startup")
    ap.add_argument("--n", type=int, default=5000, help="autostart founder count")
    ap.add_argument("--species", type=int, default=100, help="autostart founder species (1..100)")
    ap.add_argument("--every", type=int, default=2, help="autostart ticks per rendered picture")
    ap.add_argument("--fps", type=int, default=6, help="autostart max pictures per second")
    ap.add_argument("--width", type=int, default=1280, help="autostart picture width")
    ap.add_argument("--height", type=int, default=720, help="autostart picture height")
    ap.add_argument("--allow-proxy-port", type=int, action="append", default=[],
                    help="extra loopback-only host/origin port for local Hub proxying")
    a = ap.parse_args()
    for d in (WORK, FRAMES, WORLDS, LOGS):
        d.mkdir(parents=True, exist_ok=True)
    build = find_build(a.build)
    app = App(build, a.port, proxy_ports=a.allow_proxy_port)
    if a.autostart and build:
        opts = world_options({
            "n": a.n, "species": a.species, "lab": False,
            "every": a.every, "fps": a.fps, "width": a.width, "height": a.height
        }, {d["bit"] for d in app.lesions})
        rid = app.live.start_world(opts)
        print("autostarted wild world run_id=%s (n=%d, species=%d, %dx%d)" % (
            rid, opts["n"], opts["species"], opts["width"], opts["height"]), flush=True)
    srv = ThreadingHTTPServer(("127.0.0.1", a.port), make_handler(app))
    srv.daemon_threads = True
    url = "http://127.0.0.1:%d/" % a.port
    print("Asmodensius-Native live demo on %s" % url, flush=True)
    print("build folder: %s" % (build or "NONE FOUND (build az_live first; see demo/README.md)"), flush=True)
    print("run files:    %s" % WORK, flush=True)
    if a.open:
        webbrowser.open(url)
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        app.live.stop()
        srv.server_close()


if __name__ == "__main__":
    main()
