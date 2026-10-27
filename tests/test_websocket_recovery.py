"""Burst coalescing and recovery with a local server, not host acceptance."""
import base64
import copy
import hashlib
import json
import os
from pathlib import Path
import socket
import struct
import subprocess
import sys
import tempfile
import time
from test_websocket_runtime import receive, quiet


def frame(message):
    raw = json.dumps(message, separators=(',', ':')).encode()
    n = len(raw)
    length = bytes([n]) if n < 126 else b'\x7e' + struct.pack('!H', n) if n < 65536 else b'\x7f' + struct.pack('!Q', n)
    return b'\x81' + length + raw


def accept(listener, reject=False):
    conn, _ = listener.accept()
    conn.settimeout(30)
    data = b''
    while b'\r\n\r\n' not in data:
        data += conn.recv(4096)
    if reject:
        conn.sendall(b'HTTP/1.1 503 Unavailable\r\nContent-Length: 0\r\n\r\n')
        conn.close()
        return
    key = next(line.split(b':', 1)[1].strip() for line in data.split(b'\r\n') if line.lower().startswith(b'sec-websocket-key:'))
    digest = base64.b64encode(hashlib.sha1(key + b'258EAFA5-E914-47DA-95CA-C5AB0DC85B11').digest())
    conn.sendall(b'HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ' + digest + b'\r\n\r\n')
    return conn


def expect(conn, value):
    assert receive(conn) == {'action': 'set', 'parameter': '/parameter/by-id/123', 'value': value}
    assert receive(conn) == {'action': 'get', 'parameter': '/parameter/by-id/123'}


def main():
    with tempfile.TemporaryDirectory() as folder, socket.socket() as listener:
        listener.bind(('127.0.0.1', 0)); listener.listen(); listener.settimeout(8)
        config = Path(folder) / 'server.xml'
        config.write_text(f'<ServerController enabled="1" port="{listener.getsockname()[1]}" address="0.0.0.0"/>')
        log = Path(folder) / 'recovery.jsonl'
        process = subprocess.Popen([sys.argv[1], str(config)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, text=True, env=dict(os.environ, RLC_WS_LOG=str(log)))
        try:
            assert process.stdout.readline().strip() == 'ready'
            process.stdin.write('capacity\n'); process.stdin.flush()
            assert process.stdout.readline().strip() == 'ok'
            accept(listener, reject=True)  # failure at startup must retry
            with accept(listener) as conn:
                state = {'layers': [{'id': 123, 'valuetype': 'ParamRange', 'value': 1}], 'active': True}
                conn.sendall(frame(state)); expect(conn, 2)
                # Drop connection without acknowledging the pending request.
            with accept(listener) as conn:
                # No replay until a fresh full snapshot, including when an old
                # response arrives before it. Latest desired replaces old 2.
                conn.sendall(frame({'type': 'parameter_get', 'id': 123, 'value': 2}))
                quiet(conn)
                state.update(desired=3)
                state['layers'][0]['value'] = 2
                conn.sendall(frame(state)); expect(conn, 3)
                conn.sendall(frame({'type': 'parameter_get', 'id': 123, 'value': 3}))
            with accept(listener) as conn:
                state.update(active=False)
                state['layers'][0]['value'] = 3
                conn.sendall(frame(state)); expect(conn, 1)  # original baseline survived both reconnects
                conn.sendall(frame({'type': 'parameter_get', 'id': 123, 'value': 1}))
                state['layers'][0]['value'] = 1
                conn.sendall(frame(state)); quiet(conn)
                # >32 ordinary replies are not silently discarded. Overflow
                # may occur within a single Asio read callback; recover from it.
                blocked = dict(state, pause_evaluation=True)
                conn.sendall(frame(blocked))
                assert process.stdout.readline().strip() == 'evaluating'
                conn.sendall(b''.join(frame({'type': 'diagnostic', 'value': i}) for i in range(500)))
                time.sleep(0.1)
            with accept(listener) as conn:
                state.update(active=True, desired=4)
                conn.sendall(frame(state)); expect(conn, 4)
                conn.sendall(frame({'type': 'parameter_get', 'id': 123, 'value': 4}))
                state['layers'][0]['value'] = 4
                quiet(conn)
                # Hold evaluation so the receiver must retain only the latest
                # snapshot across responses. Obsolete inactive snapshots must
                # not restore the baseline; the final active state wins.
                conn.sendall(frame(dict(state, pause_evaluation=True)))
                assert process.stdout.readline().strip() == 'evaluating'
                old = dict(state, active=False)
                burst = b''.join(frame({'type': 'parameter_update', 'path': '/composition/layers/1/clips/1/transport/position', 'value': i}) for i in range(500))
                burst += b''.join(frame(old) for _ in range(150))
                burst += frame({'type': 'parameter_get', 'id': 123, 'value': 4})
                burst += frame({'type': 'parameter_set', 'id': 999, 'error': 'preserve-through-coalescing'})
                burst += frame(state)
                conn.sendall(burst)
                time.sleep(0.3); quiet(conn)
                # 30 x 30 synthetic composition with realistic Clip/Effect
                # structure, including duplicated Group views (about MBs).
                conn.settimeout(30)
                large = dict(state)
                large['layers'] = []
                captured = json.loads((Path(__file__).parent / '../docs/evidence/ws-native-plugin-20261023.json').read_text())
                template = max((c for l in captured['layers'] for c in l['clips']), key=lambda c: len(json.dumps(c)))
                for layer in range(30):
                    clips = []
                    for col in range(30):
                        clip = copy.deepcopy(template)
                        clip.update(id=10000 + layer * 30 + col, connected={'value': 'Disconnected'})
                        clips.append(clip)
                    large['layers'].append({'id': 123 if layer == 0 else 1000 + layer,
                                             'valuetype': 'ParamRange', 'value': 4, 'clips': clips})
                large['layergroups'] = [{'layers': large['layers']}]
                start = time.monotonic()
                for _ in range(3):
                    conn.sendall(frame(large))
                # Marker proves the receiver drained the large burst and can
                # still reconcile a new desired value.
                large['desired'] = 5
                conn.sendall(frame(large)); expect(conn, 5)
                conn.sendall(frame({'type': 'parameter_get', 'id': 123, 'value': 5}))
                print(f'30x30 snapshot bytes={len(frame(large))}, burst elapsed={time.monotonic()-start:.3f}s')
                if os.name == 'nt':
                    import ctypes
                    from ctypes import wintypes
                    class Memory(ctypes.Structure):
                        _fields_ = [('cb', wintypes.DWORD), ('faults', wintypes.DWORD)] + [(name, ctypes.c_size_t) for name in
                            ('peak_working', 'working', 'peak_paged', 'paged', 'peak_nonpaged', 'nonpaged', 'pagefile', 'peak_pagefile')]
                    memory = Memory(); memory.cb = ctypes.sizeof(memory)
                    if ctypes.windll.psapi.GetProcessMemoryInfo(wintypes.HANDLE(int(process._handle)), ctypes.byref(memory), memory.cb):
                        print(f'test Runtime peak working set MiB={memory.peak_working / 1024**2:.1f}')
                # Shutdown interrupts a live read and a retry wait.
            process.stdin.write('quit\n'); process.stdin.flush()
            process.wait(timeout=3)
            assert process.returncode == 0
            rows = [json.loads(line) for line in log.read_text().splitlines()]
            assert any(r['event'] == 'reconnect_wait' for r in rows)
            assert any(r.get('replaced', 0) > 0 for r in rows), 'snapshots were not coalesced'
            assert any(r.get('positions_ignored', 0) == 500 for r in rows), 'position burst was not filtered'
            assert any('receive queue limit' in r.get('reason', '') for r in rows), 'overflow recovery not exercised'
            assert all('runtime' in r for r in rows)
            assert any(r['event'] == 'server_error' and r['message'].get('error') == 'preserve-through-coalescing' for r in rows)
            large_rows = [r for r in rows if r.get('bytes', 0) > 16 * 1024**2]
            assert large_rows
            print('large snapshot metrics:', json.dumps(large_rows[-1]))
            print('startup failure, disconnect, pending reset, baseline recovery, overflow, burst coalescing passed')
        except Exception:
            if log.exists():
                print('\n'.join(log.read_text().splitlines()[-12:]))
            raise
        finally:
            if process.poll() is None:
                process.kill(); process.wait()


if __name__ == '__main__':
    main()
