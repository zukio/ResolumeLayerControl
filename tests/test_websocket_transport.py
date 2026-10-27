"""RFC6455 boundary and cancellation tests; no host acceptance claims."""
import base64
import hashlib
from pathlib import Path
import queue
import socket
import struct
import subprocess
import sys
import threading


def run_case(executable, case):
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        listener.listen()
        listener.settimeout(5)
        process = subprocess.Popen([executable, str(listener.getsockname()[1])],
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, text=True)
        lines = queue.Queue()
        reader = threading.Thread(target=lambda: [lines.put(line.strip()) for line in process.stdout])
        reader.start()
        try:
            with listener.accept()[0] as conn:
                conn.settimeout(5)
                headers = b""
                while b"\r\n\r\n" not in headers:
                    data = conn.recv(4096)
                    assert data
                    headers += data
                if case == "cancel_handshake":
                    process.stdin.write("quit\n")
                    process.stdin.flush()
                    process.wait(timeout=2)
                elif case == "reject_upgrade":
                    conn.sendall(b"HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n")
                    assert lines.get(timeout=5) == "failed"
                else:
                    key = next(line.split(b":", 1)[1].strip() for line in headers.split(b"\r\n")
                               if line.lower().startswith(b"sec-websocket-key:"))
                    accept = base64.b64encode(hashlib.sha1(key + b"258EAFA5-E914-47DA-95CA-C5AB0DC85B11").digest())
                    conn.sendall(b"HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " + accept + b"\r\n\r\n")
                    assert lines.get(timeout=5) == "connected"
                    if case == "ping_text":
                        conn.sendall(b"\x89\x01x\x81\x02ok")
                        assert lines.get(timeout=5) == "text 2"
                        pong = conn.recv(64)
                        assert pong[0] == 0x8A
                    elif case == "binary":
                        conn.sendall(b"\x82\x01x")
                        assert lines.get(timeout=5) == "failed"
                    elif case == "oversize":
                        conn.sendall(b"\x81\x7f" + struct.pack("!Q", 64 * 1024 * 1024 + 1))
                        # The library sends a close frame without allocating the announced payload.
                        close = conn.recv(128)
                        assert close[0] == 0x88
                        conn.sendall(b"\x88\x02\x03\xe8")
                        assert lines.get(timeout=5) in ("ended", "failed")
                    elif case == "peer_close":
                        conn.sendall(b"\x88\x02\x03\xe8")
                        assert conn.recv(128)[0] == 0x88
                        conn.shutdown(socket.SHUT_WR)
                        assert lines.get(timeout=5) == "ended"
                if process.poll() is None:
                    process.stdin.write("quit\n")
                    process.stdin.flush()
                    process.wait(timeout=2)
                assert process.returncode == 0, process.stderr.read()
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()
            reader.join(timeout=2)


if __name__ == "__main__":
    for case in ("cancel_handshake", "reject_upgrade", "ping_text", "binary", "oversize", "peer_close"):
        run_case(str(Path(sys.argv[1]).resolve()), case)
        print(case, "passed")
