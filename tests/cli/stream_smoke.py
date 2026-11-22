"""Independent binary and observer checks. Usage: stream_smoke.py AZ_LIVE AZ_RUN WORK_DIR."""
import collections
import io
import json
import os
from pathlib import Path
import queue
import re
import struct
import subprocess
import sys
import threading
import time

HEADER = struct.Struct("<4sHHII")
WORLD = ["--n", "2000", "--cap", "8000", "--grid", "6,6,5", "--threads", "2"]
LAB = ["--lab", "--seed", "7"]


def frames(stream):
    while True:
        raw = stream.read(HEADER.size)
        if not raw:
            return
        assert len(raw) == HEADER.size, "truncated frame header"
        magic, kind, version, size, seq = HEADER.unpack(raw)
        assert magic == b"AZST" and version == 1 and 1 <= kind <= 6, raw
        assert size <= 64 * 1024 * 1024, ("oversize smoke frame", size)
        payload = stream.read(size)
        assert len(payload) == size, "truncated payload"
        yield kind, seq, payload


def creatures(payload):
    tick, before, after, count, stride = struct.unpack_from("<QQQII", payload)
    assert before and before == after, ("observer hash mismatch", tick, before, after)
    assert struct.unpack_from("<I", payload, 52)[0] == 0, "observer failure"
    assert stride == 24 and len(payload) == 64 + count * stride
    records = list(struct.iter_unpack("<QHHHBbBBBBI", payload[64:]))
    assert all(r[0] < s[0] for r, s in zip(records, records[1:])), "UIDs out of order"
    assert all(r[-2] & 1 for r in records), "dead slot in live record list"
    return tick, count, records, before


def validate(batch, expected, ticks):
    kinds = [k for k, _, _ in batch]
    assert kinds[0] == 1 and kinds.index(3) < kinds.index(5) < kinds.index(2), kinds
    sequences = collections.defaultdict(list)
    events = []
    snapshots = []
    for kind, seq, payload in batch:
        sequences[kind].append(seq)
        if kind == 1:
            events.append(json.loads(payload))
        elif kind == 2:
            snapshots.append(creatures(payload))
        elif kind == 3:
            gx, gy, gz, cells, vents = struct.unpack_from("<5I", payload)
            assert cells == gx * gy * gz and len(payload) == 32 + 4 * vents + cells + 4 * gx * gy
            rock = payload[32 + 4 * vents:32 + 4 * vents + cells]
            heights = struct.unpack_from("<%dI" % (gx * gy), payload, len(payload) - 4 * gx * gy)
            for col, h in enumerate(heights):
                assert h <= gz * 256
                solid = [z for z in range(gz) if rock[col + z * gx * gy] == 255]
                if solid:
                    assert 256 * (1 + solid[-1]) <= h <= min(gz * 256, 256 * (1 + solid[-1]) + 255)
        elif kind == 4:
            _, gx, gy, gz, parts = struct.unpack_from("<Q4I", payload)
            assert parts == 3 and len(payload) == 32 + 4 * gx * gy + 4 * gx * gy * gz
        elif kind == 5:
            n, = struct.unpack_from("<I", payload)
            assert len(payload) == 4 + n * 4634
    assert all(all(a < b for a, b in zip(ns, ns[1:])) for ns in sequences.values())
    assert events[0].get("event") == "start" and events[-1].get("event") == "end", events[-1]
    end = events[-1]
    assert end["tick"] == ticks and end["observer_fail"] == 0 and end["unbalanced"] == 0
    if expected:
        assert end["hash"] == expected, (end, expected)
    for tick, count, _, _ in snapshots:
        assert any(e.get("tick") == tick and e.get("live") == count for e in events), ("statistics missing", tick)
    return next(p for k, _, p in batch if k == 3)


def interaction(live, work, sampled_wild=False):
    options = ["--observer-ms", "4294967295"] if sampled_wild else LAB
    proc = subprocess.Popen([live, "--dir", str(work), "--stream", "--tps", "2", "--sps", "60",
                             "--size", "320x180"] + WORLD + options,
                            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    incoming = queue.Queue()

    def read():
        try:
            for msg in frames(proc.stdout):
                incoming.put(msg)
        finally:
            incoming.put(None)

    def send(s):
        proc.stdin.write((s + "\n").encode("ascii"))
        proc.stdin.flush()

    def until(pred, timeout=25):
        limit = time.monotonic() + timeout
        while time.monotonic() < limit:
            frame = incoming.get(timeout=max(.01, limit - time.monotonic()))
            assert frame is not None, "EOF while waiting for engine response"
            if pred(*frame):
                return frame
        raise TimeoutError("timed out waiting for command")

    def event(name):
        return until(lambda k, _, p: k == 1 and json.loads(p).get("event") == name)

    def expect_no_snapshot(seconds, message):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            try:
                item = incoming.get(timeout=.08)
            except queue.Empty:
                continue
            assert item and item[0] != 2, message

    def compare_picture(tick, state_hash):
        send("picture")
        png = proof = None
        while png is None or proof is None:
            k, _, p = until(lambda k, _, p: k == 6 or (k == 1 and json.loads(p).get("event") == "picture"))
            if k == 6:
                png = p
            else:
                proof = json.loads(p)
        assert png[:8] == b"\x89PNG\r\n\x1a\n" and struct.unpack_from(">II", png, 16) == (320, 180)
        assert proof["tick"] == tick and int(proof["hash"], 16) == state_hash
        assert int(proof["hash_after_view"], 16) == state_hash

    threading.Thread(target=read, daemon=True).start()
    try:
        start = json.loads(event("start")[2])
        if sampled_wild:
            assert start["locked"] == 1 and start["mode"] == "wild" and start["observer_ms"] == 4294967295
        _, seq, _ = until(lambda k, *_: k == 2)
        send("pause")
        # Withhold credit across several tick opportunities, then acquire a fresh paused snapshot.
        expect_no_snapshot(1.2, "a blocked consumer received another creature frame")
        send("ack %d" % seq)
        _, paused_seq, payload = until(lambda k, *_: k == 2)
        paused_tick, _, _, paused_hash = creatures(payload)
        compare_picture(paused_tick, paused_hash)
        send("ack %d" % paused_seq)
        expect_no_snapshot(.25, "idle paused world emitted an unrequested creature frame")
        send("step 1")
        _, _, payload = until(lambda k, *_: k == 2)
        tick, _, records, state_hash = creatures(payload)
        assert tick == paused_tick + 1
        for record in records[:10]:
            uid = "%016x" % record[0]
            send("inspect " + uid)
            reply = json.loads(event("inspect")[2])
            assert reply["found"] and reply["tick"] == tick and reply["uid"].lower() == uid
            assert list(record[1:4]) == reply["pos"], ("stale records", uid, record[1:4], reply)
        send("inspect ffffffffffffffff")
        assert not json.loads(event("inspect")[2])["found"]
        compare_picture(tick, state_hash)
        if sampled_wild:
            print("locked wild observer-ms=4294967295 pause tick=%d hash=%016x step tick=%d hash=%016x picture pairs matched"
                  % (paused_tick, paused_hash, tick, state_hash), flush=True)
        send("quit")
        event("end")
        assert proc.wait(timeout=20) == 0
    finally:
        if proc.poll() is None:
            proc.kill()
        proc.wait()
        proc.stdin.close()
        proc.stdout.close()
        proc.stderr.close()


def main():
    live, run, work_arg = sys.argv[1:]
    work = Path(work_arg)
    work.mkdir(parents=True, exist_ok=True)
    ticks = 200
    ref = subprocess.run([run, "--ticks", str(ticks)] + WORLD + LAB, check=True, capture_output=True,
                         text=True, timeout=120).stdout
    expected = re.search(r"^final hash ([0-9a-f]{16})$", ref, re.M).group(1)
    geometry = None
    for sps, tps in ((1, 0), (15, 0), (60, 0), (1, 30), (15, 30), (60, 30)):
        result = subprocess.run([live, "--stream", "--dir", str(work), "--max-ticks", str(ticks),
                                 "--sps", str(sps), "--tps", str(tps)] + WORLD + LAB,
                                stdin=subprocess.DEVNULL, capture_output=True, timeout=120)
        assert result.returncode == 0, result.stderr.decode(errors="replace")[-1000:]
        static = validate(list(frames(io.BytesIO(result.stdout))), expected, ticks)
        assert geometry is None or static == geometry
        geometry = static
        print("lab sps=%d tps=%d final=%s" % (sps, tps, expected), flush=True)
    for options in ([], ["--lab", "--seed", "11"]):
        result = subprocess.run([live, "--stream", "--dir", str(work), "--max-ticks", "2"] + WORLD + options,
                                stdin=subprocess.DEVNULL, capture_output=True, check=True, timeout=60)
        static = validate(list(frames(io.BytesIO(result.stdout))), None, 2)
        assert static != geometry
    interaction(live, work)
    interaction(live, work, sampled_wild=True)
    print("PASS: framing, observer, deterministic pacing, books, terrain, credits, same-tick inspect and proof")


if __name__ == "__main__":
    main()
