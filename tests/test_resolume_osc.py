"""OSC decoding tests: no Resolume or network needed."""
import struct
import sys
from pathlib import Path
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from receive_resolume_osc import decode


def string(value):
    data = value.encode() + b'\0'
    return data + b'\0' * (-len(data) % 4)


class DecodeTests(unittest.TestCase):
    def test_values_and_repeated_bundle_messages(self):
        message = string('/observed') + string(',ifsTF') + struct.pack('!if', 1, 0.5) + string('value')
        expected = {'address': '/observed', 'tags': ',ifsTF', 'values': [1, 0.5, 'value', True, False]}
        bundle = b'#bundle\0' + struct.pack('!Q', 1) + (struct.pack('!I', len(message)) + message) * 2
        self.assertEqual(decode(bundle), [expected, expected])

    def test_unknown_tag_retains_address(self):
        self.assertIn('decode_note', decode(string('/observed') + string(',b'))[0])

    def test_invalid_bundle_length(self):
        with self.assertRaises(ValueError):
            decode(b'#bundle\0' + bytes(8) + struct.pack('!I', 200) + b'x')

    def test_truncated_value(self):
        with self.assertRaises(struct.error):
            decode(string('/observed') + string(',i'))


if __name__ == '__main__':
    unittest.main()
