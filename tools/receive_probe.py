"""Diagnostic receiver only. Choose an unused UDP port, not Resolume's port."""
import argparse
import socket
import struct
import time
parser = argparse.ArgumentParser()
parser.add_argument('--port', required=True, type=int)
args = parser.parse_args()
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(('127.0.0.1', args.port))
print('Listening on', sock.getsockname(), flush=True)
while True:
    packet, peer = sock.recvfrom(65535)
    if len(packet) == 20 and packet[:16] == b'/rlc/probe\0\0,i\0\0':
        print(time.time_ns(), peer, 'sequence', struct.unpack('!I', packet[16:])[0], flush=True)
    else:
        print('Unexpected packet:', peer, packet.hex(), flush=True)
