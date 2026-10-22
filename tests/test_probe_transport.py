"""Run after compiling probe_worker_smoke.cpp; argument is the executable path."""
import os
from pathlib import Path
import socket
import struct
import subprocess
import sys
import tempfile
with tempfile.TemporaryDirectory() as folder:
    log = Path(folder) / 'probe.csv'
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as receiver:
        receiver.bind(('127.0.0.1', 0))
        receiver.settimeout(1)
        env = dict(os.environ, RLC_PROBE_PORT=str(receiver.getsockname()[1]), RLC_PROBE_LOG=str(log))
        subprocess.run([sys.argv[1]], env=env, check=True, timeout=5)
        packet, _ = receiver.recvfrom(256)
        assert packet == b'/rlc/probe\0\0,i\0\0' + struct.pack('!I', 1), packet
        try:
            receiver.recvfrom(256)
        except socket.timeout:
            pass
        else:
            raise AssertionError('Duplicate datagram')
    content = log.read_text()
    assert 'send_bytes=20,dropped=0' in content
    assert 'a=1.25,b=2.5,c=3.75,text=host version' in content
    assert content.count(',set_time,') == 2
    assert 'wall_ms=' in content
print('PASS: OSC payload, single datagram, log and shutdown')
