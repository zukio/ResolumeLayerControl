"""Common WebSocket against a local RFC6455 test server; NOT a Resolume acceptance test."""
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


def rejected_settings(executable):
    with tempfile.TemporaryDirectory() as folder, socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        listener.listen()
        port = listener.getsockname()[1]
        cases = [
            (f'<ServerController enabled="0" port="{port}" address="0.0.0.0"/>', "disabled"),
            (f'<ServerController enabled="1" port="{port}" address="0.0.0.0">', "failure"),
            ('<ServerController enabled="1" port="0" address="0.0.0.0"/>', "failure"),
            (None, "failure"),
        ]
        for index, (xml, expected) in enumerate(cases):
            config = Path(folder) / f"server-{index}.xml"
            if xml is not None:
                config.write_text(xml)
            log = Path(folder) / f"runtime-{index}.jsonl"
            env = dict(os.environ, RLC_WS_PORT=str(port), RLC_WS_LOG=str(log))
            process = subprocess.Popen([executable, str(config)], stdin=subprocess.PIPE,
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env=env)
            try:
                deadline = time.monotonic() + 5
                rows = []
                while time.monotonic() < deadline:
                    if log.exists():
                        lines = log.read_text().splitlines()
                        try:
                            rows = [json.loads(line) for line in lines]
                        except json.JSONDecodeError:
                            pass  # writer may be between write and flush
                        if any(row["event"] == expected for row in rows):
                            break
                    time.sleep(0.02)
                assert any(row["event"] == expected for row in rows), rows
                assert not any(row["event"] == "connected" for row in rows), rows
                listener.settimeout(0.1)
                try:
                    conn, _ = listener.accept()
                    conn.close()
                    raise AssertionError("rejected settings attempted a connection")
                except socket.timeout:
                    pass
                process.communicate("quit\n", timeout=5)
                assert process.returncode == 0
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait()


def main():
    executable = str(Path(sys.argv[1]).resolve())
    rejected_settings(executable)
    with tempfile.TemporaryDirectory() as folder, socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        listener.listen()
        listener.settimeout(5)
        log = Path(folder) / "runtime-\u65e5\u672c\u8a9e.jsonl"
        # Inject only into the test executable; never change the real host preferences.
        config = Path(folder) / "server.xml"
        config.write_text(f'<ServerController enabled="1" port="{listener.getsockname()[1]}" address="127.0.0.1"/>')
        copied = Path(folder) / ("runtime_smoke" + Path(executable).suffix)
        shutil.copy2(executable, copied)
        # Obsolete settings must be ignored, even if present and invalid.
        copied.with_suffix(".json").write_text("not JSON")
        env = dict(os.environ, RLC_WS_PORT="invalid-obsolete-setting", RLC_WS_LOG=str(log))
        process = subprocess.Popen([str(copied), str(config)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
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
            print("Common WebSocket integration passed: fragmented receive, difference-only set, readback, no loop, inactive/disabled, cancellation")
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()


if __name__ == "__main__":
    main()
