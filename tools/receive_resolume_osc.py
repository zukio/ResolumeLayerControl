"""Passive OSC capture. Never sends commands to Resolume. Uses Python stdlib only."""
import argparse
import json
from pathlib import Path
import socket
import struct
import time


def osc_string(packet, offset):
    end = packet.index(b'\0', offset)
    following = (end + 4) & ~3
    if following > len(packet):
        raise ValueError('Truncated OSC string padding')
    return packet[offset:end].decode('utf-8', errors='replace'), following


def decode(packet, depth=0):
    if depth > 8:
        raise ValueError('Nested bundle limit exceeded')
    if packet.startswith(b'#bundle\0'):
        if len(packet) < 16:
            raise ValueError('Truncated bundle')
        messages, offset = [], 16
        while offset < len(packet):
            length = struct.unpack_from('!I', packet, offset)[0]
            offset += 4
            if length == 0 or offset + length > len(packet):
                raise ValueError('Invalid bundle element length')
            messages.extend(decode(packet[offset:offset + length], depth + 1))
            offset += length
        return messages
    address, offset = osc_string(packet, 0)
    tags, offset = osc_string(packet, offset)
    if not address.startswith('/') or not tags.startswith(','):
        raise ValueError('Invalid OSC address or tags')
    values = []
    for tag in tags[1:]:
        if tag in 'ifhd':
            fmt = {'i': '!i', 'f': '!f', 'h': '!q', 'd': '!d'}[tag]
            values.append(struct.unpack_from(fmt, packet, offset)[0])
            offset += struct.calcsize(fmt)
        elif tag == 's':
            value, offset = osc_string(packet, offset)
            values.append(value)
        elif tag in 'TFNI':
            values.append({'T': True, 'F': False, 'N': None, 'I': 'impulse'}[tag])
        else:
            # Raw packet is always retained; unsupported tags are not guessed.
            return [{'address': address, 'tags': tags, 'decode_note': 'Unsupported tag ' + tag}]
    if offset != len(packet):
        raise ValueError('Unexpected OSC trailing bytes')
    return [{'address': address, 'tags': tags, 'values': values}]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True, type=int)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    if not 1 <= args.port <= 65535:
        parser.error('Port must be 1..65535')
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind(('127.0.0.1', args.port))
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024 * 1024)
        with args.output.open('x', encoding='utf-8') as file:
            print('Listening:', sock.getsockname(), 'Saving:', args.output, flush=True)
            print('Ctrl+C to finish. Every datagram is retained; UDP delivery is not guaranteed.', flush=True)
            try:
                count = 0
                while True:
                    packet, peer = sock.recvfrom(65535)
                    count += 1
                    record = {'packet': count, 'wall_ms': time.time_ns() // 1_000_000,
                              'peer': list(peer), 'raw_hex': packet.hex()}
                    try:
                        record['messages'] = decode(packet)
                    except (ValueError, struct.error) as error:
                        record['decode_error'] = str(error)
                    file.write(json.dumps(record, ensure_ascii=False) + '\n')
                    file.flush()
                    # Show a bounded preview; save full content including duplicates.
                    if count <= 10 or count % 100 == 0:
                        print(count, record.get('messages', record.get('decode_error')), flush=True)
            except KeyboardInterrupt:
                print('\nSaved', count, 'datagrams:', args.output)


if __name__ == '__main__':
    main()
