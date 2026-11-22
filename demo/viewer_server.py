"""Local AZST v1 relay. Start with python -I demo/viewer_server.py --help."""
import argparse
import base64
import binascii
import collections
import hashlib
import json
import math
import os
import re
import socket
import struct
import subprocess
import sys
import threading
import time
import uuid
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlsplit

DEMO = Path(__file__).resolve().parent
ROOT = DEMO.parent
HEADER = struct.Struct("<4sHHII")
JSON, CREATURES, STATIC, FIELDS, SHAPES, PICTURE = range(1, 7)
SHAPE_BYTES = 4634
ORDER = (STATIC, SHAPES, JSON, FIELDS, CREATURES, PICTURE)
MAX_BODY = 1048576
POP_MIN = 1
POP_MAX = 1000000
CAP_MAX_UI = 2000000
ASSETS = {
    "/": ("viewer.html", "text/html; charset=utf-8"),
    "/viewer.html": ("viewer.html", "text/html; charset=utf-8"),
    "/viewer_gl.js": ("viewer_gl.js", "text/javascript"),
    "/viewer_core.js": ("viewer_core.js", "text/javascript"),
    "/viewer_decode.js": ("viewer_decode.js", "text/javascript"),
    "/viewer.css": ("viewer.css", "text/css"),
}


class Pool:
    """Recycle unreferenced storage only. Slow senders retain immutable frames."""
    def __init__(self):
        self.free = {}
        self.lock = threading.Lock()

    def take(self, kind, size):
        with self.lock:
            buf = self.free.pop(kind, None)
        if buf is not None and len(buf) >= size:
            return buf
        return bytearray(size)

    def put(self, kind, buf):
        with self.lock:
            if len(self.free.get(kind, ())) < len(buf):
                self.free[kind] = buf


class Frame:
    def __init__(self, kind, sequence, buffer, size, pool=None):
        self.kind = kind
        self.sequence = sequence
        self.buffer = buffer
        self.size = size
        self.pool = pool

    def wire(self):
        return memoryview(self.buffer)[:self.size]

    def __del__(self):
        if self.pool is not None:
            self.pool.put(self.kind, self.buffer)


def packed(kind, sequence, payload):
    buf = bytearray(HEADER.size + len(payload))
    HEADER.pack_into(buf, 0, b"AZST", kind, 1, len(payload), sequence)
    buf[HEADER.size:] = payload
    return Frame(kind, sequence, buf, len(buf))


def fill(stream, dest, clean_eof=False):
    offset = 0
    while offset < len(dest):
        n = stream.readinto(dest[offset:])
        if not n:
            if clean_eof and offset == 0:
                return False
            raise EOFError("truncated AZST frame")
        offset += n
    return True


def frames(stream):
    pool = Pool()
    header = bytearray(HEADER.size)
    while fill(stream, memoryview(header), True):
        magic, kind, version, size, sequence = HEADER.unpack(header)
        if magic != b"AZST" or version != 1 or kind not in ORDER or size > 0x7fffffff:
            raise ValueError("invalid AZST header")
        buf = pool.take(kind, HEADER.size + size)
        buf[:HEADER.size] = header
        fill(stream, memoryview(buf)[HEADER.size:HEADER.size + size])
        yield Frame(kind, sequence, buf, HEADER.size + size, pool)


class Proof:
    def __init__(self):
        self.done = threading.Event()
        self.meta = None
        self.frame = None
        self.error = None


class Save:
    def __init__(self, run_dir):
        name = "world-" + uuid.uuid4().hex
        self.path = run_dir / (name + ".azsnap")
        self.partial = run_dir / (name + ".partial")
        self.done = threading.Event()
        self.bytes = 0
        self.error = None


class Store:
    """One producer. Only JSON history and the cumulative clade table are retained."""
    def __init__(self):
        self.changed = threading.Condition()
        self.latest = {}
        self.shapes = {}
        self.json_frames = collections.deque(maxlen=64)
        self.events = collections.deque(maxlen=64)
        self.latest_events = {}
        self.closed = False
        self.proof = None
        self.inspections = {}

    def publish(self, frame):
        obj = None
        if frame.kind == JSON:
            obj = json.loads(bytes(frame.wire()[HEADER.size:]))
            if not isinstance(obj, dict):
                raise ValueError("AZST JSON must contain an object")
        elif frame.kind == SHAPES:
            data = frame.wire()[HEADER.size:]
            if len(data) < 4:
                raise ValueError("missing SHAPES count")
            count, = struct.unpack_from("<I", data)
            if len(data) != 4 + count * SHAPE_BYTES:
                raise ValueError("invalid SHAPES row count")
            for pos in range(4, len(data), SHAPE_BYTES):
                clade, = struct.unpack_from("<I", data, pos)
                self.shapes[clade] = bytes(data[pos:pos + SHAPE_BYTES])
            body = struct.pack("<I", len(self.shapes)) + b"".join(self.shapes.values())
            frame = packed(SHAPES, frame.sequence, body)
        with self.changed:
            if obj is not None:
                self.json_frames.append(frame)
                self.events.append(obj)
                self.latest_events[obj.get("event", "stats")] = obj
            if obj and obj.get("event") == "inspect":
                inspection = self.inspections.pop(obj.get("uid"), None)
                if inspection is not None:
                    inspection.meta = obj
                    inspection.done.set()
            if self.proof is not None:
                proof = self.proof
                if obj and obj.get("event") == "picture":
                    proof.meta = obj
                elif obj and obj.get("event") == "error" and obj.get("what") == "picture":
                    self.finish_proof(obj.get("detail", "engine refused proof"))
                elif frame.kind == PICTURE:
                    if proof.meta is None:
                        self.finish_proof("missing native picture metadata")
                    else:
                        proof.frame = frame
                        self.finish_proof()
            self.latest[frame.kind] = frame
            self.changed.notify_all()

    def finish_proof(self, error=None):
        # Caller holds changed. A request retains this immutable frame after newer
        # pictures replace the cache. Waiters never hold the producer lock.
        if self.proof is not None:
            self.proof.error = error
            self.proof.done.set()
            self.proof = None

    def end(self):
        with self.changed:
            self.closed = True
            self.finish_proof("engine ended before completing the picture")
            for inspection in self.inspections.values():
                inspection.error = "engine ended before completing the inspection"
                inspection.done.set()
            self.inspections.clear()
            self.changed.notify_all()

    def next_frame(self, kind, after):
        latest = self.latest.get(kind)
        if latest is None or after == latest.sequence:
            return None
        if kind == JSON:
            if after == -1:
                return self.json_frames[0]
            for i, item in enumerate(self.json_frames):
                if item.sequence == after:
                    return self.json_frames[i + 1]
            # The client fell behind the retained history.
            if newer(latest.sequence, after):
                return self.json_frames[0]
        return latest if newer(latest.sequence, after) else None

    def batch(self, cursors, wait=1.0):
        def ready():
            return [f for k in ORDER if k in cursors
                    for f in (self.next_frame(k, cursors[k]),) if f is not None]
        with self.changed:
            result = ready()
            if not result and not self.closed:
                self.changed.wait_for(lambda: self.closed or ready(), wait)
                result = ready()
            return result


def newer(sequence, after):
    return after == -1 or 0 < (sequence - after) % (1 << 32) < (1 << 31)


def accept_key(key):
    try:
        valid = len(base64.b64decode(key, validate=True)) == 16
    except (ValueError, binascii.Error):
        valid = False
    if not valid:
        raise ValueError("invalid WebSocket key")
    digest = hashlib.sha1((key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode("ascii")).digest()
    return base64.b64encode(digest).decode("ascii")


def ws_header(size, opcode=2):
    first = 0x80 | opcode
    if size < 126:
        return bytes((first, size))
    if size <= 65535:
        return struct.pack(">BBH", first, 126, size)
    return struct.pack(">BBQ", first, 127, size)


def valid_command(cmd):
    if not isinstance(cmd, str):
        return False
    if cmd in ("run", "pause", "step", "picture", "pace free", "pace picture", "quit"):
        return True
    if re.fullmatch(r"inspect [0-9a-fA-F]{16}", cmd):
        return True
    if cmd.startswith("camera "):
        words = cmd.split(" ")[1:]
        if len(words) not in (3, 6):
            return False
        number = r"[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?"
        if any(len(word) > 30 or not re.fullmatch(number, word) for word in words):
            return False
        yaw, pitch, distance, *target = map(float, words)
        return (-1000 <= yaw <= 1000 and -1.55 <= pitch <= 1.55 and 0 <= distance <= 1e6
                and all(math.isfinite(value) and abs(value) <= 3.4028234e38 for value in target))
    match = re.fullmatch(r"size (\d{2,4})x(\d{2,4})", cmd)
    if match:
        return all(64 <= int(v) <= 4096 for v in match.groups())
    ranges = {"sps": (1, 60), "fps": (1, 60), "tps": (0, 99999), "step": (1, 100000),
              "every": (1, 10000), "observer-ms": (0, 99999)}
    match = re.fullmatch(r"([a-z-]+) (\d{1,6})", cmd)
    if not match or match[1] not in ranges:
        return False
    lo, hi = ranges[match[1]]
    return lo <= int(match[2]) <= hi


def auto_cap(n):
    if n <= 250000:
        return min(1004096, n * 4 + 4096)
    return max(n + 4096, 1004096)


def parse_engine_options(options):
    cfg = {"n": 5000, "cap": 0, "species": 100, "lab": False, "seed": 42,
           "grid": None, "kernels": 0, "gpu": 0, "ecores": False, "prefetch": 1,
           "large_pages": False, "loaded": False}
    i = 0
    while i < len(options):
        tok = options[i]
        if tok == "--lab":
            cfg["lab"] = True
            i += 1
            continue
        if tok == "--ecores":
            cfg["ecores"] = True
            i += 1
            continue
        if tok == "--large-pages":
            cfg["large_pages"] = True
            i += 1
            continue
        if tok in ("--n", "--cap", "--species", "--seed", "--kernels", "--gpu", "--prefetch", "--grid", "--load") and i + 1 < len(options):
            val = options[i + 1]
            if tok == "--grid":
                cfg["grid"] = val
            elif tok == "--load":
                cfg["loaded"] = True
            else:
                try:
                    cfg[tok[2:]] = int(val)
                except ValueError:
                    pass
            i += 2
            continue
        i += 1
    return cfg


def rebuild_engine_options(base_options, n, cap, species, lab, seed, grid, kernels,
                           gpu=None, ecores=None, prefetch=None, large_pages=None):
    drop_with_val = {"--n", "--cap", "--species", "--seed", "--grid", "--load", "--kernels", "--gpu", "--prefetch"}
    drop_flags = {"--lab", "--ecores", "--large-pages"}
    preserved = []
    i = 0
    while i < len(base_options):
        tok = base_options[i]
        if tok in drop_flags:
            i += 1
            continue
        if tok in drop_with_val:
            i += 2
            continue
        preserved.append(tok)
        i += 1
    out = ["--n", str(n), "--cap", str(cap), "--species", str(species)]
    if grid:
        out.extend(["--grid", grid])
    if lab:
        out.extend(["--lab", "--seed", str(seed)])
    if kernels is not None:
        out.extend(["--kernels", "1" if kernels else "0"])
    if gpu is not None:
        out.extend(["--gpu", "1" if gpu else "0"])
    if prefetch is not None:
        out.extend(["--prefetch", "1" if prefetch else "0"])
    if ecores:
        out.append("--ecores")
    if large_pages:
        out.append("--large-pages")
    out.extend(preserved)
    return out


class App:
    def __init__(self, port, directory):
        self.port = port
        self.directory = directory
        self.run_id = uuid.uuid4().hex
        self.store = Store()
        self.error = None
        self.process = None
        self.reader = None
        self.write_lock = threading.Lock()
        self.lifecycle_lock = threading.Lock()
        self.run_dir = self.directory / "runs" / self.run_id
        self.pending_save = None
        self.engine = None
        self.options = []
        self.config = {"n": 5000, "cap": 0, "species": 100, "lab": False, "seed": 42,
                       "grid": None, "kernels": 0, "loaded": False}

    def start(self, engine, options):
        self.engine = Path(engine)
        self.options = list(options)
        self.config = parse_engine_options(self.options)
        run = self.run_dir
        run.mkdir(parents=True, exist_ok=True)
        self.log = run / "engine-stderr.log"
        command = [str(self.engine), "--stream", "--dir", str(run)] + self.options
        with self.log.open("wb") as log:
            proc = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                    stderr=log, cwd=str(run), bufsize=0,
                                    creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        self.process = proc
        store = self.store
        log_path = self.log
        self.reader = threading.Thread(target=self.read_engine, args=(proc, store, log_path), daemon=True)
        self.reader.start()

    def read_engine(self, proc=None, store=None, log_path=None):
        proc = proc if proc is not None else self.process
        store = store if store is not None else self.store
        log_path = log_path if log_path is not None else getattr(self, "log", None)
        err = None
        try:
            with proc.stdout as out:
                for frame in frames(out):
                    store.publish(frame)
                    if frame.kind == JSON and self.pending_save and self.store is store:
                        event = json.loads(bytes(frame.wire()[HEADER.size:]))
                        if event.get("what") == "save":
                            self.finish_save(event)
                    if frame.kind == CREATURES and self.process is proc:
                        self.send("ack %d" % frame.sequence)
        except (OSError, EOFError, ValueError, MemoryError) as exc:
            err = str(exc)
            if proc.poll() is None:
                proc.terminate()
        finally:
            code = proc.wait()
            if code and not err and log_path is not None and log_path.exists():
                with log_path.open("rb") as log:
                    log.seek(max(0, log_path.stat().st_size - 2000))
                    err = log.read().decode("utf-8", "replace").strip() or "engine exit %d" % code
            if err and self.store is store:
                self.error = err
            if self.store is store:
                self.finish_save({"event": "error", "detail": "engine ended before confirming the save"})
            store.end()

    def restart_world(self, n, cap=0, species=None, lab=None, seed=None, grid=None,
                      kernels=None, gpu=None, ecores=None, prefetch=None, large_pages=None):
        with self.lifecycle_lock:
            if self.engine is None or not self.engine.is_file():
                return False, "engine executable unavailable for restart"
            eff_n = int(n)
            eff_cap = int(cap) if cap else auto_cap(eff_n)
            if eff_cap < eff_n:
                eff_cap = eff_n
            base_sp = self.config.get("species", 100)
            eff_sp = int(species) if species is not None else base_sp
            eff_sp = max(1, min(100, eff_sp, eff_n))
            eff_lab = self.config.get("lab", False) if lab is None else bool(lab)
            eff_seed = self.config.get("seed", 42) if seed is None else int(seed)
            eff_kernels = self.config.get("kernels", 0) if kernels is None else (1 if kernels else 0)
            eff_gpu = self.config.get("gpu", 0) if gpu is None else (1 if gpu else 0)
            eff_ecores = self.config.get("ecores", False) if ecores is None else bool(ecores)
            eff_prefetch = self.config.get("prefetch", 1) if prefetch is None else (1 if prefetch else 0)
            eff_lp = self.config.get("large_pages", False) if large_pages is None else bool(large_pages)
            new_options = rebuild_engine_options(
                self.options, eff_n, eff_cap, eff_sp, eff_lab, eff_seed, grid, eff_kernels,
                gpu=eff_gpu, ecores=eff_ecores, prefetch=eff_prefetch, large_pages=eff_lp
            )
            old_proc = self.process
            old_reader = self.reader
            old_store = self.store
            if old_proc is not None:
                with self.write_lock:
                    if old_proc.poll() is None and old_proc.stdin and not old_proc.stdin.closed:
                        try:
                            old_proc.stdin.write(b"quit\n")
                            old_proc.stdin.flush()
                        except (BrokenPipeError, OSError):
                            pass
                        try:
                            old_proc.stdin.close()
                        except OSError:
                            pass
                try:
                    old_proc.wait(timeout=8)
                except subprocess.TimeoutExpired:
                    old_proc.kill()
                    old_proc.wait()
                if old_reader is not None and old_reader.is_alive():
                    old_reader.join(timeout=3)
            self.run_id = uuid.uuid4().hex
            self.run_dir = self.directory / "runs" / self.run_id
            self.error = None
            self.pending_save = None
            self.store = Store()
            old_store.end()
            self.start(self.engine, new_options)
            return True, {"ok": True, "run_id": self.run_id, "config": dict(self.config)}

    def save_world(self):
        with self.write_lock:
            if self.pending_save is not None:
                return None
            request = self.pending_save = Save(self.run_dir)
            # The native child runs in run_dir. Only this ASCII basename crosses stdin.
            if not self.write_commands(["save " + request.partial.name]):
                self.pending_save = None
                return None
            return request

    def finish_save(self, event):
        with self.write_lock:
            request = self.pending_save
            if request is None:
                return
            try:
                if event.get("event") != "saved":
                    raise OSError(event.get("detail", "engine could not write the snapshot"))
                size = request.partial.stat().st_size
                if size <= 0 or size != event.get("bytes"):
                    raise OSError("engine byte count did not match the snapshot")
                request.partial.rename(request.path)
                request.bytes = size
            except OSError as exc:
                request.error = str(exc)
                try:
                    request.partial.unlink(missing_ok=True)
                except OSError:
                    pass
            finally:
                self.pending_save = None
                request.done.set()

    def write_commands(self, commands):
        # Caller holds write_lock. Every accepted transaction is contiguous on stdin.
        if self.process is None or self.process.poll() is not None or self.process.stdin.closed:
            return False
        try:
            self.process.stdin.write(("\n".join(commands) + "\n").encode("ascii"))
            self.process.stdin.flush()
            return True
        except (BrokenPipeError, OSError):
            return False

    def request_proof(self, commands):
        with self.write_lock:
            with self.store.changed:
                if self.store.closed or self.store.proof is not None:
                    return None
                request = self.store.proof = Proof()
            if not self.write_commands(commands):
                with self.store.changed:
                    self.store.finish_proof("engine command pipe closed")
                return None
            return request

    def request_inspect(self, uid):
        with self.write_lock:
            with self.store.changed:
                if self.store.closed:
                    return None
                if uid in self.store.inspections:
                    return self.store.inspections[uid]
                request = self.store.inspections[uid] = Proof()
            if not self.write_commands(["inspect " + uid]):
                with self.store.changed:
                    self.store.inspections.pop(uid, None)
                return None
            return request

    def send(self, cmd):
        if cmd.startswith("inspect "):
            return self.request_inspect(cmd.split()[1].lower()) is not None
        if cmd == "picture":
            return self.request_proof([cmd]) is not None
        with self.write_lock:
            return self.write_commands([cmd])

    def state(self):
        proc = self.process
        store = self.store
        code = proc.poll() if proc is not None else None
        with store.changed:
            return {"running": proc is not None and code is None, "run_id": self.run_id,
                    "exit_code": code, "error": self.error, "config": dict(self.config),
                    "latest": dict(store.latest_events),
                    "events": list(store.events),
                    "frames": {k: f.sequence for k, f in store.latest.items()}}

    def stop(self):
        if self.process:
            self.send("quit")
            with self.write_lock:
                self.process.stdin.close()
            try:
                self.process.wait(timeout=8)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()
            self.reader.join(timeout=3)
        self.store.end()

    def save_perf(self, data):
        logs = self.directory / "logs"
        logs.mkdir(parents=True, exist_ok=True)
        name = "perf-%s-%s.json" % (self.run_id, uuid.uuid4().hex)
        encoded = json.dumps(data, allow_nan=False, ensure_ascii=True) + "\n"
        target = logs / name
        pending = target.with_suffix(".tmp")
        with pending.open("x", encoding="utf-8", newline="\n") as f:
            f.write(encoded)
        pending.replace(target)
        return name


def handler(app):
    class Handler(BaseHTTPRequestHandler):
        protocol_version = "HTTP/1.1"

        def log_message(self, fmt, *args):
            pass

        def response(self, status, data=b"", mime="application/json"):
            if isinstance(data, dict):
                data = json.dumps(data).encode()
            self.send_response(status)
            self.send_header("Content-Type", mime)
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-Content-Type-Options", "nosniff")
            self.end_headers()
            if data:
                self.wfile.write(data)

        def allowed(self, origin=False):
            hosts = {"127.0.0.1:%d" % app.port, "localhost:%d" % app.port}
            host = self.headers.get_all("Host") or []
            if len(host) != 1 or host[0] not in hosts:
                self.close_connection = True
                self.response(403, {"error": "host"})
                return False
            origins = self.headers.get_all("Origin") or []
            if (origin or origins) and origins != ["http://" + host[0]]:
                self.close_connection = True
                self.response(403, {"error": "origin"})
                return False
            return True

        def do_GET(self):
            u = urlsplit(self.path)
            if not self.allowed(u.path == "/ws"):
                return
            if u.path in ASSETS:
                file, mime = ASSETS[u.path]
                try:
                    data = (DEMO / file).read_bytes()
                except FileNotFoundError:
                    return self.response(404, {"error": "asset unavailable"})
                return self.response(200, data, mime)
            if u.path == "/stream/" + app.run_id:
                return self.picture_stream()
            if u.path == "/ws":
                return self.websocket()
            if u.path == "/api/state":
                return self.response(200, app.state())
            if u.path == "/api/snap":
                try:
                    q = parse_qs(u.query)
                    if q.get("run_id", [app.run_id])[0] != app.run_id:
                        return self.response(409, {"error": "world changed", "run_id": app.run_id})
                    kind = int(q["type"][0])
                    after = int(q.get("after", ["-1"])[0])
                    if kind not in ORDER or not -1 <= after < 1 << 32:
                        raise ValueError
                except (ValueError, KeyError):
                    return self.response(400, {"error": "invalid type or cursor"})
                found = app.store.batch({kind: after})
                if found:
                    return self.response(200, found[0].wire(), "application/octet-stream")
                if app.store.closed:
                    return self.response(410, {"ended": True, "error": app.error})
                return self.response(204)
            self.response(404, {"error": "unknown route"})

        def picture_stream(self):
            self.send_response(200)
            self.send_header("Content-Type", "multipart/x-mixed-replace; boundary=azframe")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Connection", "close")
            self.end_headers()
            self.close_connection = True
            sequence = -1
            try:
                while True:
                    ready = app.store.batch({PICTURE: sequence})
                    if ready:
                        frame = ready[0]
                        png = frame.wire()[HEADER.size:]
                        self.wfile.write(b"--azframe\r\nContent-Type: image/png\r\nContent-Length: ")
                        self.wfile.write(str(len(png)).encode("ascii") + b"\r\n\r\n")
                        self.wfile.write(png)
                        self.wfile.write(b"\r\n")
                        sequence = frame.sequence
                    elif app.store.closed:
                        self.wfile.write(b"--azframe--\r\n")
                        return
            except OSError:
                return

        def do_POST(self):
            if not self.allowed(True):
                return
            path = urlsplit(self.path).path
            limit = MAX_BODY if path == "/api/perf" else 4096
            lengths = self.headers.get_all("Content-Length") or []
            if (self.headers.get("Transfer-Encoding") or len(lengths) != 1 or
                    not re.fullmatch(r"[0-9]{1,10}", lengths[0])):
                self.close_connection = True
                return self.response(400, {"error": "a single Content-Length is required"})
            n = int(lengths[0])
            if n > limit:
                self.close_connection = True
                return self.response(413, {"error": "body too large"})
            if self.headers.get_content_type() != "application/json":
                self.close_connection = True
                return self.response(415, {"error": "expected application/json"})
            try:
                body = json.loads(self.rfile.read(n))
                if not isinstance(body, dict):
                    raise ValueError
                if path == "/api/cmd":
                    if not valid_command(body.get("cmd")):
                        return self.response(400, {"error": "invalid command"})
                    ok = app.send(body["cmd"])
                    return self.response(200 if ok else 409, {"ok": ok})
                if path == "/api/inspect":
                    uid = body.get("uid")
                    if not isinstance(uid, str) or not re.fullmatch(r"[0-9a-fA-F]{16}", uid):
                        return self.response(400, {"error": "expected 16 hex digits for uid"})
                    inspection = app.request_inspect(uid.lower())
                    if inspection is None:
                        return self.response(409, {"error": "engine has ended"})
                    if not inspection.done.wait(60):
                        return self.response(504, {"error": "engine did not complete the inspection in time"})
                    if inspection.error:
                        return self.response(409, {"error": inspection.error})
                    return self.response(200, inspection.meta)
                if path == "/api/save":
                    if body:
                        return self.response(400, {"error": "save takes no arguments"})
                    saved = app.save_world()
                    if saved is None:
                        return self.response(409, {"error": "engine ended or another save is in progress"})
                    if not saved.done.wait(120):
                        return self.response(504, {"error": "save is still pending", "path": str(saved.path)})
                    if saved.error:
                        return self.response(500, {"error": saved.error})
                    return self.response(200, {"ok": True, "path": str(saved.path), "bytes": saved.bytes})
                if path == "/api/proof":
                    camera, size = body.get("camera"), body.get("size")
                    if not (valid_command(camera) and camera.startswith("camera ") and
                            valid_command(size) and size.startswith("size ")):
                        return self.response(400, {"error": "invalid proof camera or size"})
                    proof = app.request_proof([size, camera, "picture"])
                    if proof is None:
                        return self.response(409, {"error": "engine ended or a previous proof is still pending"})
                    if not proof.done.wait(60):
                        return self.response(504, {"error": "native picture timed out"})
                    if proof.error:
                        return self.response(409, {"error": proof.error})
                    png = proof.frame.wire()[HEADER.size:]
                    self.send_response(200)
                    self.send_header("Content-Type", "image/png")
                    self.send_header("Content-Length", str(len(png)))
                    self.send_header("Cache-Control", "no-store")
                    meta = dict(proof.meta, sequence=proof.frame.sequence, run_id=app.run_id)
                    self.send_header("X-AZ-Proof", json.dumps(meta, ensure_ascii=True, separators=(",", ":")))
                    self.end_headers()
                    self.wfile.write(png)
                    return
                if path == "/api/world":
                    raw_n = body.get("n")
                    if isinstance(raw_n, bool) or not isinstance(raw_n, int) or not POP_MIN <= raw_n <= POP_MAX:
                        return self.response(400, {"error": "population n must be an integer from 1 to 1000000"})
                    raw_cap = body.get("cap", 0)
                    if isinstance(raw_cap, bool) or not isinstance(raw_cap, int) or not 0 <= raw_cap <= CAP_MAX_UI:
                        return self.response(400, {"error": "cap must be 0 (auto) or an integer up to 2000000"})
                    if raw_cap and raw_cap < raw_n:
                        return self.response(400, {"error": "cap must be 0 (auto) or at least n"})
                    raw_sp = body.get("species")
                    if raw_sp is not None and (isinstance(raw_sp, bool) or not isinstance(raw_sp, int) or not 1 <= raw_sp <= 100):
                        return self.response(400, {"error": "species must be an integer from 1 to 100"})
                    raw_lab = body.get("lab")
                    if raw_lab is not None and not isinstance(raw_lab, bool):
                        return self.response(400, {"error": "lab must be true or false"})
                    raw_seed = body.get("seed")
                    if raw_seed is not None and (isinstance(raw_seed, bool) or not isinstance(raw_seed, int) or raw_seed < 0 or raw_seed >= (1 << 63)):
                        return self.response(400, {"error": "seed must be a non-negative integer"})
                    raw_grid = body.get("grid")
                    if raw_grid is not None and (not isinstance(raw_grid, str) or not re.fullmatch(r"\d{1,2},\d{1,2},\d{1,2}", raw_grid)):
                        return self.response(400, {"error": "grid must be x,y,z bit widths or omitted for auto"})
                    raw_kernels = body.get("kernels")
                    if raw_kernels is not None and (isinstance(raw_kernels, bool) or raw_kernels not in (0, 1)):
                        return self.response(400, {"error": "kernels must be 0 or 1"})
                    raw_gpu = body.get("gpu")
                    if raw_gpu is not None and (isinstance(raw_gpu, bool) or raw_gpu not in (0, 1)):
                        return self.response(400, {"error": "gpu must be 0 or 1"})
                    raw_ecores = body.get("ecores")
                    if raw_ecores is not None and not isinstance(raw_ecores, bool):
                        return self.response(400, {"error": "ecores must be true or false"})
                    raw_prefetch = body.get("prefetch")
                    if raw_prefetch is not None and (isinstance(raw_prefetch, bool) or raw_prefetch not in (0, 1)):
                        return self.response(400, {"error": "prefetch must be 0 or 1"})
                    raw_lp = body.get("large_pages")
                    if raw_lp is not None and not isinstance(raw_lp, bool):
                        return self.response(400, {"error": "large_pages must be true or false"})
                    ok, result = app.restart_world(raw_n, cap=raw_cap, species=raw_sp, lab=raw_lab,
                                                   seed=raw_seed, grid=raw_grid, kernels=raw_kernels,
                                                   gpu=raw_gpu, ecores=raw_ecores, prefetch=raw_prefetch,
                                                   large_pages=raw_lp)
                    if not ok:
                        return self.response(409, {"error": result})
                    return self.response(200, result)
                if path == "/api/perf":
                    return self.response(200, {"ok": True, "file": app.save_perf(body)})
                if path == "/api/shutdown":
                    self.response(200, {"ok": True})
                    threading.Thread(target=self.server.shutdown, daemon=True).start()
                    return
            except (ValueError, RecursionError, OverflowError):
                return self.response(400, {"error": "invalid JSON object"})
            except OSError as exc:
                return self.response(500, {"error": str(exc)})
            self.response(404, {"error": "unknown route"})

        def websocket(self):
            version = self.headers.get("Sec-WebSocket-Version")
            tokens = [x.strip().lower() for x in self.headers.get("Connection", "").split(",")]
            if version != "13" or self.headers.get("Upgrade", "").lower() != "websocket" or "upgrade" not in tokens:
                return self.response(400, {"error": "invalid WebSocket upgrade"})
            try:
                key = accept_key(self.headers.get("Sec-WebSocket-Key", ""))
            except ValueError:
                return self.response(400, {"error": "invalid WebSocket key"})
            self.send_response(101)
            self.send_header("Upgrade", "websocket")
            self.send_header("Connection", "Upgrade")
            self.send_header("Sec-WebSocket-Accept", key)
            self.end_headers()
            self.close_connection = True
            done = threading.Event()
            writing = threading.Lock()

            def send(opcode, payload):
                with writing:
                    if not done.is_set():
                        self.connection.sendall(ws_header(len(payload), opcode))
                        if payload:
                            self.connection.sendall(payload)

            def sender():
                store = app.store
                cursor = dict.fromkeys(ORDER, -1)
                try:
                    while not done.is_set():
                        batch = store.batch(cursor, 0.5)
                        for frame in batch:
                            if done.is_set():
                                break
                            send(2, frame.wire())
                            cursor[frame.kind] = frame.sequence
                        if not batch and store.closed:
                            send(8, struct.pack(">H", 1000))
                            break
                except OSError:
                    pass
                finally:
                    done.set()
                    try:
                        self.connection.shutdown(socket.SHUT_RDWR)
                    except OSError:
                        pass

            thread = threading.Thread(target=sender, daemon=True)
            thread.start()
            try:
                while not done.is_set():
                    head = self.rfile.read(2)
                    if len(head) != 2:
                        break
                    command, length = head[0], head[1] & 127
                    if command not in (0x88, 0x89, 0x8a) or not head[1] & 128 or length > 125:
                        send(8, struct.pack(">H", 1002))
                        break
                    data = self.rfile.read(4 + length)
                    if len(data) != 4 + length:
                        break
                    data = bytes(b ^ data[i % 4] for i, b in enumerate(data[4:]))
                    if command == 0x88:
                        reply = data if length != 1 else struct.pack(">H", 1002)
                        send(8, reply)
                        break
                    if command == 0x89:
                        send(10, data)
            except OSError:
                pass
            finally:
                done.set()
                try:
                    self.connection.shutdown(socket.SHUT_RDWR)
                except OSError:
                    pass
                thread.join(timeout=2)

    return Handler


def main():
    p = argparse.ArgumentParser(description="Live AZST viewer on IPv4 loopback")
    p.add_argument("--engine", type=Path, help="absolute path to az_live.exe (default: build/zv/az_live.exe)")
    p.add_argument("--data-dir", type=Path, default=ROOT / "build" / "zv" / "viewer-data")
    p.add_argument("--port", type=int, default=8766)
    p.add_argument("--n", type=int)
    p.add_argument("--cap", type=int)
    p.add_argument("--threads", type=int)
    p.add_argument("--kernels", type=int, choices=(0, 1), help="native kernel dispatch (0 reference, 1 CPU twins)")
    p.add_argument("--grid", help="engine grid bits x,y,z")
    p.add_argument("--sps", type=int, default=10)
    p.add_argument("--tps", type=int, default=0)
    p.add_argument("--observer-ms", type=int)
    p.add_argument("--max-ticks", type=int)
    p.add_argument("--lab", action="store_true")
    p.add_argument("--seed", type=int)
    p.add_argument("--load", type=Path)
    a = p.parse_args()
    engine = a.engine or ROOT / "build" / "zv" / "az_live.exe"
    if a.port == 8765 or not 1 <= a.port <= 65535:
        p.error("choose a port from 1 to 65535 other than the existing demo port 8765")
    if not engine.is_absolute() or not engine.is_file():
        p.error("a built stream engine is required at --engine ABSOLUTE_PATH")
    if a.seed is not None and not a.lab:
        p.error("--seed requires --lab")
    if a.load and (not a.load.is_file() or any(v is not None for v in (a.n, a.cap, a.grid, a.seed)) or a.lab):
        p.error("--load requires an existing file and is incompatible with world-generation options")
    options = []
    if a.load:
        options.extend(["--load", str(a.load.resolve())])
    else:
        options.extend(["--n", str(a.n if a.n is not None else 5000)])
    if a.lab:
        options.append("--lab")
    for name in ("cap", "grid", "seed", "threads", "kernels", "sps", "tps", "max_ticks"):
        v = getattr(a, name)
        if v is not None:
            options.extend(["--" + name.replace("_", "-"), str(v)])
    if a.observer_ms is not None or not a.load:
        interval = a.observer_ms if a.observer_ms is not None else (0 if a.lab else 500)
        options.extend(["--observer-ms", str(interval)])
    app = App(a.port, a.data_dir.resolve())
    with ThreadingHTTPServer(("127.0.0.1", a.port), handler(app)) as server:
        try:
            app.start(engine, options)
            print("Serving http://127.0.0.1:%d/ with run %s" % (a.port, app.run_id), flush=True)
            server.serve_forever()
        except KeyboardInterrupt:
            pass
        finally:
            app.stop()


if __name__ == "__main__":
    main()
