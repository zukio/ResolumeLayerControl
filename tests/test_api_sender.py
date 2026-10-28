"""APISender against a local RFC6455 server; no Resolume host required."""
import base64
import copy
import hashlib
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile

from test_websocket_runtime import send, receive, quiet


def connect(listener):
    conn = listener.accept()[0]
    conn.settimeout(3)
    headers = b""
    while b"\r\n\r\n" not in headers:
        headers += conn.recv(4096)
    key = next(line.split(b":", 1)[1].strip() for line in headers.split(b"\r\n")
               if line.lower().startswith(b"sec-websocket-key:"))
    accept = base64.b64encode(hashlib.sha1(key + b"258EAFA5-E914-47DA-95CA-C5AB0DC85B11").digest())
    conn.sendall(b"HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " + accept + b"\r\n\r\n")
    return conn


def run(executable, initially_active):
    with tempfile.TemporaryDirectory() as folder, socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        listener.listen()
        listener.settimeout(5)
        config = Path(folder) / "server.xml"
        config.write_text(f'<ServerController enabled="1" port="{listener.getsockname()[1]}" address="127.0.0.1"/>')
        log = Path(folder) / "api.jsonl"
        process = subprocess.Popen([executable, str(config)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, text=True, env=dict(os.environ, RLC_WS_LOG=str(log)))
        def command(text):
            process.stdin.write(text + "\n"); process.stdin.flush()
            assert process.stdout.readline().strip() == "ok"
        on = [{"action": "set", "parameter": "/composition/master", "value": 0.5},
              {"action": "trigger", "parameter": "/composition/columns/2/connect"}]
        off = {"action": "set", "parameter": "/composition/master", "value": 1}
        snapshot = {"layers": [{"id": 10, "clips": [{"id": 20, "connected": {"value": "Connected" if initially_active else "Disconnected"},
                    "video": {"effects": [{"bypassed": {"value": False}, "params": {"Instance Key": {"value": "api-test"}}}]}}]}]}
        clip = snapshot["layers"][0]["clips"][0]
        effect = clip["video"]["effects"][0]
        def active_batch(conn):
            assert [receive(conn), receive(conn)] == on
            quiet(conn)  # no automatic get, retry or baseline restore
        try:
            assert process.stdout.readline().strip() == "ready"
            with connect(listener) as conn:
                send(conn, snapshot)
                if initially_active: active_batch(conn)
                else:
                    quiet(conn)  # initial inactive never emits the off command
                    clip["connected"]["value"] = "Connected"
                    send(conn, snapshot); active_batch(conn)
                send(conn, snapshot); quiet(conn)  # own notifications / retrigger
                send(conn, {"type": "parameter_update", "path": "/composition/x", "value": 0}); quiet(conn)
                # An unresolved key must hold the last observed state.
                effect["params"]["Instance Key"]["value"] = "missing"
                send(conn, snapshot); quiet(conn)
                effect["params"]["Instance Key"]["value"] = "api-test"
                duplicate = copy.deepcopy(clip)
                snapshot["layers"][0]["clips"].append(duplicate)
                send(conn, snapshot); quiet(conn)
                snapshot["layers"][0]["clips"].pop()
                send(conn, snapshot); quiet(conn)
                clip["connected"]["value"] = "Previewing"
                send(conn, snapshot); assert receive(conn) == off; quiet(conn)
                send(conn, snapshot); quiet(conn)
                clip["connected"]["value"] = "Connected & previewing"
                send(conn, snapshot); active_batch(conn)
                effect["bypassed"]["value"] = True
                send(conn, snapshot); assert receive(conn) == off; quiet(conn)
                effect["bypassed"]["value"] = False
                send(conn, snapshot); active_batch(conn)
                command("disable"); send(conn, snapshot); assert receive(conn) == off; quiet(conn)
                command("enable"); send(conn, snapshot); active_batch(conn)
                command("edit"); send(conn, snapshot); quiet(conn)  # edits wait for the next edge
                clip["connected"]["value"] = "Disconnected"
                send(conn, snapshot); assert receive(conn) == off; quiet(conn)
                clip["connected"]["value"] = "Connected"
                send(conn, snapshot); assert receive(conn) == {"action": "get", "parameter": "/composition"}; quiet(conn)
                # Server errors do not replay arbitrary non-idempotent commands.
                send(conn, {"type": "parameter_set", "error": "test failure"})
                send(conn, snapshot); quiet(conn)
            # A fresh connection resets edge state and waits for a full Composition.
            with connect(listener) as conn:
                quiet(conn)
                send(conn, snapshot); assert receive(conn) == {"action": "get", "parameter": "/composition"}; quiet(conn)
                command("empty"); send(conn, snapshot); quiet(conn)
                clip["connected"]["value"] = "Disconnected"
                send(conn, snapshot); assert receive(conn) == off; quiet(conn)
                clip["connected"]["value"] = "Connected"
                send(conn, snapshot); quiet(conn)
                send(conn, snapshot); quiet(conn)
                process.stdin.write("quit\n"); process.stdin.flush()
                assert process.wait(timeout=5) == 0, process.stderr.read()
            rows = [json.loads(line) for line in log.read_text().splitlines()]
            assert not any(row["event"] in ("set_sent", "restore_sent", "invalid_state", "message_rejected") for row in rows), rows
            assert any(row["event"] == "api_commands_queued" and row["total"] == 0 for row in rows)
        finally:
            if process.poll() is None:
                process.kill(); process.wait()


if __name__ == "__main__":
    run(str(Path(sys.argv[1]).resolve()), False)
    run(str(Path(sys.argv[1]).resolve()), True)
    print("APISender integration passed (initial inactive and initial active)")
