"""WinHTTP against a local RFC6455 test server; NOT a Resolume acceptance test."""
import base64
import hashlib
import json
import os
from pathlib import Path
import socket
import shutil
import struct
import subprocess
import sys
import tempfile
import time


def exact(conn, size):
    result = b""
    while len(result) < size:
        part = conn.recv(size - len(result))
        if not part:
            raise RuntimeError("unexpected close")
        result += part
    return result


def receive(conn):
    first, second = exact(conn, 2)
    assert first == 0x81, (first, second)
    size = second & 127
    if size == 126:
        size = struct.unpack("!H", exact(conn, 2))[0]
    elif size == 127:
        size = struct.unpack("!Q", exact(conn, 8))[0]
    assert second & 128, "client frame must be masked"
    mask = exact(conn, 4)
    raw = exact(conn, size)
    return json.loads(bytes(value ^ mask[i % 4] for i, value in enumerate(raw)))


def send(conn, message, fragment=False):
    raw = json.dumps(message).encode()
    def frame(data, opcode):
        length = bytes([len(data)]) if len(data) < 126 else b"\x7e" + struct.pack("!H", len(data))
        conn.sendall(bytes([opcode]) + length + data)
    if fragment:
        middle = len(raw) // 2
        frame(raw[:middle], 0x01)
        frame(raw[middle:], 0x80)
    else:
        frame(raw, 0x81)


def quiet(conn):
    conn.settimeout(0.25)
    try:
        value = conn.recv(1)
        raise AssertionError(f"unexpected data/close: {value!r}")
    except socket.timeout:
        pass
    finally:
        conn.settimeout(3)


def main():
    executable = str(Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory() as folder, socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        listener.listen()
        listener.settimeout(5)
        log = Path(folder) / "runtime.jsonl"
        env = dict(os.environ, RLC_WS_PORT=str(listener.getsockname()[1]), RLC_WS_LOG=str(log))
        if "--config" in sys.argv:
            copied = Path(folder) / "runtime_smoke.exe"
            shutil.copy2(executable, copied)
            executable = str(copied)
            copied.with_suffix(".json").write_text(json.dumps({"port": listener.getsockname()[1], "log": str(log)}))
            env.pop("RLC_WS_PORT", None)
            env.pop("RLC_WS_LOG", None)
        process = subprocess.Popen([executable], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, text=True, env=env)
        try:
            assert process.stdout.readline().strip() == "ready"
            with listener.accept()[0] as conn:
                conn.settimeout(3)
                headers = b""
                while b"\r\n\r\n" not in headers:
                    headers += conn.recv(4096)
                assert headers.startswith(b"GET /api/v1 HTTP/1.1")
                key = next(line.split(b":", 1)[1].strip() for line in headers.split(b"\r\n")
                           if line.lower().startswith(b"sec-websocket-key:"))
                accept = base64.b64encode(hashlib.sha1(key + b"258EAFA5-E914-47DA-95CA-C5AB0DC85B11").digest())
                conn.sendall(b"HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " + accept + b"\r\n\r\n")
                snapshot = {"layers": [{"value": 1, "id": 123, "valuetype": "ParamRange"}], "active": False}
                send(conn, snapshot)
                quiet(conn)
                snapshot["active"] = True
                send(conn, snapshot, fragment=True)
                expected = {"action": "set", "parameter": "/parameter/by-id/123", "value": 2}
                assert receive(conn) == expected
                assert receive(conn) == {"action": "get", "parameter": "/parameter/by-id/123"}
                send(conn, snapshot)  # same stale state while request pending
                quiet(conn)
                send(conn, {"type": "parameter_set", "path": "/parameter/by-id/123", "value": 2})
                send(conn, snapshot)  # acknowledgement alone is not readback
                quiet(conn)
                send(conn, {"type": "parameter_get", "path": "/parameter/by-id/123", "value": 2})
                snapshot["layers"][0]["value"] = 2
                send(conn, snapshot)
                quiet(conn)
                send(conn, {"type": "parameter_update", "path": "/composition/layers/1/clips/1/transport/position", "value": 0})
                quiet(conn)
                snapshot["layers"][0]["value"] = 3
                send(conn, snapshot)
                assert receive(conn) == expected
                assert receive(conn)["action"] == "get"
                process.stdin.write("disable\n")
                process.stdin.flush()
                assert process.stdout.readline().strip() == "ok"
                send(conn, {"type": "parameter_get", "path": "/parameter/by-id/123", "value": 2})
                send(conn, snapshot)
                # No active source remains: return to the value seen before the first change.
                assert receive(conn) == {"action": "set", "parameter": "/parameter/by-id/123", "value": 1}
                assert receive(conn) == {"action": "get", "parameter": "/parameter/by-id/123"}
                send(conn, snapshot)  # restore still pending
                quiet(conn)
                start = time.monotonic()
                process.stdin.write("quit\n")
                process.stdin.flush()
                process.wait(timeout=5)  # cancel while receive is blocked
                assert process.returncode == 0, process.stderr.read()
                assert time.monotonic() - start < 5
            rows = [json.loads(line) for line in log.read_text().splitlines()]
            assert sum(row["event"] == "set_sent" for row in rows) == 2
            assert sum(row["event"] == "restore_sent" for row in rows) == 1
            assert sum(row["event"] == "readback" for row in rows) == 2
            assert not any(row["event"] == "failure" for row in rows), rows
            print("WinHTTP integration passed: fragmented receive, difference-only set, readback, no loop, inactive/disabled, cancellation")
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()


if __name__ == "__main__":
    main()
