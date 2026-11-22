"""Exercise the AZST relay via socketpair only, without a listening TCP server."""
import base64
import contextlib
import importlib.util
import io
import json
import os
import socket
import struct
import sys
import tempfile
import threading
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

sys.dont_write_bytecode = True
path = Path(__file__).resolve().parents[2] / "demo" / "viewer_server.py"
spec = importlib.util.spec_from_file_location("viewer_server", path)
srv = importlib.util.module_from_spec(spec)
spec.loader.exec_module(srv)
KEY = "dGhlIHNhbXBsZSBub25jZQ=="
ADDRESS = "127.0.0.1:8766"
ORIGIN = "http://" + ADDRESS


def read_frame(stream):
    h = stream.read(2)
    if len(h) != 2:
        raise EOFError("short WebSocket header")
    assert h[0] & 0x80 and h[0] & 0x70 == 0 and h[1] & 128 == 0
    length = h[1]
    if length == 126:
        length = int.from_bytes(stream.read(2), "big")
    elif length == 127:
        length = int.from_bytes(stream.read(8), "big")
    data = stream.read(length)
    if len(data) != length:
        raise EOFError("short WebSocket payload")
    return h[0] & 15, data


def masked(op, data=b""):
    key = b"\x12\x34\x56\x78"
    return bytes((128 | op, 128 | len(data))) + key + bytes(b ^ key[i % 4] for i, b in enumerate(data))


def http(method, route, host=ADDRESS, origin=ORIGIN, body=None, ws=False):
    headers = [method + " " + route + " HTTP/1.1", "Host: " + host]
    if origin is not None:
        headers.append("Origin: " + origin)
    if ws:
        headers += ["Upgrade: websocket", "Connection: keep-alive, Upgrade", "Sec-WebSocket-Version: 13",
                    "Sec-WebSocket-Key: " + KEY]
    else:
        headers.append("Connection: close")
    if body is not None:
        headers += ["Content-Type: application/json", "Content-Length: %d" % len(body)]
    return ("\r\n".join(headers) + "\r\n\r\n").encode("ascii") + (body or b"")


@contextlib.contextmanager
def connection(app, request):
    client, server = socket.socketpair()
    client.settimeout(5)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 4096)
    wrapper = srv.handler(app)
    errors = []

    def handle():
        with server:
            try:
                wrapper(server, ("127.0.0.1", 12345), SimpleNamespace())
            except (BrokenPipeError, ConnectionResetError):
                return
            except BaseException as exc:
                errors.append(exc)

    thread = threading.Thread(target=handle, daemon=True)
    thread.start()
    try:
        with client, client.makefile("rb") as stream:
            client.sendall(request)
            first = stream.readline().split()
            status = int(first[1])
            headers = {}
            while True:
                line = stream.readline()
                if line == b"\r\n":
                    break
                if not line:
                    raise EOFError("short HTTP header")
                key, val = line.decode("ascii").split(":", 1)
                headers[key.lower()] = val.strip()
            yield client, stream, status, headers
    finally:
        app.store.end()
        thread.join(timeout=3)
        if thread.is_alive():
            raise RuntimeError("handler did not stop")
        if errors:
            raise errors[0]


class Tests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.app = srv.App(8766, Path(self.temp.name))

    def test_served_modules_and_launcher_options(self):
        for asset in ("/viewer_gl.js", "/viewer_core.js", "/viewer_decode.js", "/viewer.css"):
            with connection(self.app, http("GET", asset)) as (_, r, status, headers):
                self.assertEqual(status, 200, asset)
                body = r.read(int(headers["content-length"]))
                self.assertEqual(body, (srv.DEMO / asset[1:]).read_bytes())
        engine = Path(self.temp.name) / "az_live.exe"
        engine.touch()
        for dispatch in ("0", "1"):
            with patch.object(sys, "argv", ["viewer_server.py", "--engine", str(engine), "--lab",
                                            "--kernels", dispatch, "--data-dir", self.temp.name]), \
                 patch.object(srv, "ThreadingHTTPServer"), patch.object(srv.App, "start") as start:
                with contextlib.redirect_stdout(io.StringIO()):
                    srv.main()
            engine_arg, options = start.call_args.args
            self.assertEqual(engine_arg, engine)
            self.assertEqual(options[options.index("--kernels") + 1], dispatch)

    def test_known_answer_and_all_lengths(self):
        self.assertEqual(srv.accept_key(KEY), "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=")
        with self.assertRaises(ValueError):
            srv.accept_key("bad")
        for n in (0, 125, 126, 65535, 65536, 6000064):
            data = bytes(n)
            opcode, out = read_frame(io.BytesIO(srv.ws_header(n) + data))
            self.assertEqual((opcode, len(out)), (2, n))

    def test_host_origin_and_controls(self):
        for options in ({"host": "example.invalid"}, {"origin": "http://example.invalid"}, {"origin": None}):
            with connection(self.app, http("GET", "/ws", ws=True, **options)) as (_, _, status, _):
                self.assertEqual(status, 403)
        self.app.store = srv.Store()
        with connection(self.app, http("GET", "/ws", ws=True)) as (s, r, status, headers):
            self.assertEqual(status, 101)
            self.assertEqual(headers["sec-websocket-accept"], "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=")
            s.sendall(masked(9, b"hello"))
            self.assertEqual(read_frame(r), (10, b"hello"))
            s.sendall(masked(8, b"\x03\xe8"))
            self.assertEqual(read_frame(r), (8, b"\x03\xe8"))
        self.app.store = srv.Store()
        with connection(self.app, http("GET", "/ws", ws=True)) as (s, r, status, _):
            s.sendall(b"\x82\0")
            self.assertEqual(read_frame(r), (8, b"\x03\xea"))

    def test_latest_wins_on_socket(self):
        payload = bytes(300000)
        self.app.store.publish(srv.packed(srv.CREATURES, 0, payload))
        with connection(self.app, http("GET", "/ws", ws=True)) as (s, r, status, _):
            self.assertEqual(status, 101)
            # Wait until frame zero is in flight, then leave the receiver idle.
            prefix = r.read(10)
            self.assertEqual(int.from_bytes(prefix[2:10], "big"), len(payload) + 16)
            self.app.store.publish(srv.packed(srv.CREATURES, 1, payload))
            self.app.store.publish(srv.packed(srv.CREATURES, 2, payload))
            data = r.read(len(payload) + 16)
            self.assertEqual(int.from_bytes(data[12:16], "little"), 0)
            op, data = read_frame(r)
            self.assertEqual((op, int.from_bytes(data[12:16], "little")), (2, 2))
            s.sendall(masked(8))
            self.assertEqual(read_frame(r), (8, b""))

    def test_stream_parser_and_credit(self):
        class Fragmented(io.BytesIO):
            def readinto(self, view):
                return super().readinto(view[:3])

        src = srv.packed(2, 135, b"abc")
        one, = srv.frames(Fragmented(src.wire()))
        self.assertEqual(bytes(one.wire()), bytes(src.wire()))
        with self.assertRaises(EOFError):
            list(srv.frames(Fragmented(src.wire()[:-1])))
        with self.assertRaises(ValueError):
            list(srv.frames(io.BytesIO(bytes(16))))
        proc = SimpleNamespace(stdout=Fragmented(src.wire()), stdin=io.BytesIO(), poll=lambda: None, wait=lambda: 0)
        self.app.process = proc
        self.app.read_engine()
        self.assertEqual(proc.stdin.getvalue(), b"ack 135\n")
        self.assertTrue(self.app.store.closed)

    def test_late_join_shape_union_and_replacement(self):
        def shape(idx, char):
            return struct.pack("<I", idx) + bytes([char]) * (srv.SHAPE_BYTES - 4)
        for seq, row in enumerate((shape(4, 1), shape(7, 5), shape(4, 9))):
            self.app.store.publish(srv.packed(5, seq, struct.pack("<I", 1) + row))
        expected = struct.pack("<I", 2) + shape(4, 9) + shape(7, 5)
        with connection(self.app, http("GET", "/api/snap?type=5&after=-1")) as (_, r, status, h):
            self.assertEqual(status, 200)
            wire = r.read(int(h["content-length"]))
            self.assertEqual(srv.HEADER.unpack_from(wire), (b"AZST", 5, 1, len(expected), 2))
            self.assertEqual(wire[16:], expected)
        with connection(self.app, http("GET", "/api/snap?type=5&after=2")) as (_, _, status, _):
            self.assertEqual(status, 410)
        self.assertTrue(srv.newer(0, 0xffffffff))
        self.assertFalse(srv.newer(3, 4))

    def test_delayed_proof_metadata_and_png_stay_together(self):
        self.app.process = SimpleNamespace(poll=lambda: None, stdin=io.BytesIO())
        replies = []
        for tick in (101, 102):
            ticket = self.app.request_proof(["size 320x180", "camera 1 0 100", "picture"])
            self.assertIsNotNone(ticket)
            self.assertIsNone(self.app.request_proof(["picture"]))
            meta = {"event": "picture", "tick": tick, "hash": str(tick)}
            self.app.store.publish(srv.packed(srv.JSON, tick, json.dumps(meta).encode()))
            self.app.store.publish(srv.packed(srv.PICTURE, tick - 101, bytes([tick])))
            replies.append(ticket)
        # The delayed first reader now observes both streams after the cache advanced.
        for i, ticket in enumerate(replies):
            self.assertTrue(ticket.done.is_set())
            self.assertIsNone(ticket.error)
            self.assertEqual(ticket.meta["tick"], 101 + i)
            self.assertEqual(ticket.frame.sequence, i)
            self.assertEqual(bytes(ticket.frame.wire()[16:]), bytes([ticket.meta["tick"]]))
        self.assertEqual(self.app.process.stdin.getvalue(), b"size 320x180\ncamera 1 0 100\npicture\n"*2)

    def test_inspect_survives_stats_history_and_terminal_poll(self):
        self.app.process = SimpleNamespace(poll=lambda: None, stdin=io.BytesIO())
        uid = "0000000000000042"
        request = self.app.request_inspect(uid)
        self.assertIs(request, self.app.request_inspect(uid))
        self.app.store.publish(srv.packed(srv.JSON, 0, json.dumps({"event":"inspect","uid":uid,"tick":2}).encode()))
        for i in range(1, 70):
            self.app.store.publish(srv.packed(srv.JSON, i, b'{"event":"stats"}'))
        self.assertEqual(request.meta["tick"], 2)
        self.assertTrue(request.done.is_set())
        self.assertFalse(any(x.get("event") == "inspect" for x in self.app.store.events))
        waiting = self.app.request_inspect("0000000000000001")
        self.app.store.end()
        self.assertTrue(waiting.done.is_set())
        self.assertIsNotNone(waiting.error)
        with connection(self.app, http("GET", "/api/snap?type=1&after=69")) as (_, r, code, h):
            self.assertEqual(code, 410)
            self.assertTrue(json.loads(r.read(int(h["content-length"])))["ended"])
        with connection(self.app, http("GET", "/api/snap?type=2&after=0&run_id=old")) as (_, _, code, _):
            self.assertEqual(code, 409)

    def test_engine_confirmed_saves_and_refusal(self):
        self.app.run_dir.mkdir(parents=True)
        self.app.process = SimpleNamespace(poll=lambda: None, stdin=io.BytesIO())
        save = self.app.save_world()
        self.assertIsNone(self.app.save_world())
        self.assertEqual(self.app.process.stdin.getvalue(), ("save "+save.partial.name+"\n").encode())
        self.assertEqual(save.path.parent, self.app.run_dir)
        self.assertFalse(save.path.exists())
        save.partial.write_bytes(b"example")
        self.app.finish_save({"event":"saved","what":"save","bytes":7})
        self.assertTrue(save.done.is_set())
        self.assertEqual(save.path.read_bytes(), b"example")
        self.assertEqual(save.bytes, 7)
        failed = self.app.save_world()
        self.assertNotEqual(save.path, failed.path)
        failed.partial.write_bytes(b"bad")
        self.app.finish_save({"event":"saved","what":"save","bytes":100})
        self.assertTrue(failed.done.is_set())
        self.assertTrue(failed.error)
        self.assertFalse(failed.path.exists())
        self.assertFalse(failed.partial.exists())
        with connection(self.app, http("POST", "/api/save", body=b'{"path":"user-chosen"}')) as (_,_,code,_):
            self.assertEqual(code, 400)

    def test_commands_and_perf(self):
        self.assertFalse(srv.valid_command("ack 42"))
        self.assertFalse(srv.valid_command("sps 61"))
        self.assertFalse(srv.valid_command("pause\nquit"))
        self.assertTrue(srv.valid_command("inspect 000000001234abcd"))
        self.assertTrue(srv.valid_command("observer-ms 0"))
        for command in ("camera 0.25 0 200", "camera 1.2 -.8 80 10 -20.5 1e-3"):
            self.assertTrue(srv.valid_command(command))
        for command in ("camera 0 1.56 2", "camera 0 0 -1", "camera 0 0 10 1 2",
                        "camera 0 0 10 1 2 nan", "camera 0 0 10 1 2 1e88"):
            self.assertFalse(srv.valid_command(command))
        for route, body, status in (("/api/cmd", b'{"cmd":"ack 4"}', 400),
                                    ("/api/perf", b'{"fps":55.1}', 200),
                                    ("/api/perf", b'{"fps":NaN}', 400)):
            with connection(self.app, http("POST", route, body=body)) as (_, r, code, headers):
                self.assertEqual(code, status)
                data = json.loads(r.read(int(headers["content-length"])))
                if code == 200:
                    self.assertEqual(json.loads((Path(self.temp.name) / "logs" / data["file"]).read_text()),
                                     {"fps": 55.1})
        self.assertEqual(len(list((Path(self.temp.name) / "logs").iterdir())), 1)

    def test_json_cursor_and_multipart(self):
        for i in range(70):
            self.app.store.publish(srv.packed(1, i, b'{"event":"stats"}'))
        self.assertEqual(self.app.store.batch({1: 2})[0].sequence, 6)
        self.assertEqual(self.app.store.batch({1: 40})[0].sequence, 41)
        self.app.store.publish(srv.packed(6, 0, b"PNGdata"))
        self.app.store.end()
        with connection(self.app, http("GET", "/stream/" + self.app.run_id)) as (_, r, code, headers):
            self.assertEqual(code, 200)
            self.assertIn("boundary=azframe", headers["content-type"])
            self.assertEqual(r.read(), b"--azframe\r\nContent-Type: image/png\r\nContent-Length: 7\r\n\r\n"
                                       b"PNGdata\r\n--azframe--\r\n")

    def test_world_restart_validation_and_auto_cap(self):
        self.assertEqual(srv.auto_cap(1), 4100)
        self.assertEqual(srv.auto_cap(5000), 24096)
        self.assertEqual(srv.auto_cap(250000), 1004096)
        self.assertEqual(srv.auto_cap(1000000), 1004096)
        for bad in (b'{"n":0}', b'{"n":1000001}', b'{"n":true}', b'{"n":100,"cap":50}', b'{"n":100,"species":101}'):
            with connection(self.app, http("POST", "/api/world", body=bad)) as (_, _, code, _):
                self.assertEqual(code, 400, bad)
        engine = Path(self.temp.name) / "az_live.exe"
        engine.touch()
        self.app.engine = engine
        self.app.options = ["--n", "5000", "--cap", "24096", "--species", "100", "--sps", "10", "--tps", "14"]
        self.app.config = srv.parse_engine_options(self.app.options)
        old_run = self.app.run_id
        with patch.object(srv.App, "start", autospec=True) as mock_start:
            def fake_start(inst, eng, opts):
                inst.engine = Path(eng)
                inst.options = list(opts)
                inst.config = srv.parse_engine_options(opts)
            mock_start.side_effect = fake_start
            with connection(self.app, http("POST", "/api/world", body=b'{"n":1000000,"cap":0,"kernels":1}')) as (_, r, code, h):
                self.assertEqual(code, 200)
                data = json.loads(r.read(int(h["content-length"])))
                self.assertTrue(data["ok"])
                self.assertNotEqual(data["run_id"], old_run)
                self.assertEqual(data["config"]["n"], 1000000)
                self.assertEqual(data["config"]["cap"], 1004096)
                self.assertEqual(data["config"]["kernels"], 1)
            with connection(self.app, http("POST", "/api/world", body=b'{"n":1,"cap":0}')) as (_, r, code, h):
                self.assertEqual(code, 200)
                data = json.loads(r.read(int(h["content-length"])))
                self.assertEqual(data["config"]["n"], 1)
                self.assertEqual(data["config"]["cap"], 4100)
                self.assertEqual(data["config"]["species"], 1)


if __name__ == "__main__":
    # socketpair on Windows internally uses loopback in the standard library.
    # The application must never enter its server constructor from these tests.
    with patch.object(srv, "ThreadingHTTPServer", side_effect=AssertionError("unexpected listener")):
        unittest.main(verbosity=2)
