#!/usr/bin/env python3
"""Build-time LZ4 correctness and block-format boundary checks (stdlib only)."""
import importlib.util
from pathlib import Path
import random
import unittest

SOURCE = Path(__file__).resolve().parents[1]/'tools/compact_nwa.py'
SPEC = importlib.util.spec_from_file_location('compact_nwa', SOURCE)
packing = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(packing)


class CompressionTests(unittest.TestCase):
    def check_block(self, raw):
        block = packing.compress_lz4(raw)
        self.assertEqual(packing.decode_lz4(block, len(raw)), raw)
        # Walk token boundaries independently to check the LZ4 end constraints.
        pos, decoded, last_match, final_literals = 0, 0, None, None
        while pos < len(block):
            token = block[pos]
            pos += 1
            literal = token >> 4
            if literal == 15:
                while block[pos] == 255:
                    literal += 255
                    pos += 1
                literal += block[pos]
                pos += 1
            pos += literal
            decoded += literal
            if pos == len(block):
                final_literals = literal
                break
            distance = int.from_bytes(block[pos:pos+2], 'little')
            self.assertGreater(distance, 0)
            self.assertLessEqual(distance, decoded)
            pos += 2
            match = (token & 15) + 4
            if (token & 15) == 15:
                while block[pos] == 255:
                    match += 255
                    pos += 1
                match += block[pos]
                pos += 1
            last_match = decoded
            decoded += match
        self.assertEqual(decoded, len(raw))
        self.assertIsNotNone(final_literals)
        if last_match is not None:
            self.assertGreaterEqual(final_literals, 5)
            self.assertLessEqual(last_match, len(raw)-12)
        return block

    def test_lengths_and_random_literals(self):
        rng = random.Random(6140)
        for size in (0, 1, 4, 5, 11, 12, 15, 19, 20, 254, 255, 270, 1024):
            with self.subTest(size=size):
                self.check_block(bytes(rng.randrange(256) for _ in range(size)))

    def test_long_overlapping_matches_and_extension_bytes(self):
        for pattern in (b'A', b'\x00\xff', bytes(range(32))):
            raw = (pattern*packing.ICON_BYTES)[:packing.ICON_BYTES]
            with self.subTest(pattern=pattern):
                self.assertLess(len(self.check_block(raw)), 150)

    def test_repeated_pixel_rows(self):
        row = b'\x08\xc5'*20 + b'\xff\xff'*15 + b'\x08\xc5'*20
        self.check_block(row*56)

    def test_known_raw_block(self):
        self.assertEqual(packing.decode_lz4(b'\x13A\x01\x00\x50hello', 13), b'A'*8+b'hello')

    def test_truncated_invalid_and_oversized_output(self):
        for block, expected in ((b'',1),(b'\xf0',1),(b'\x10',1),
                (b'\x00\x00\x00',4),(b'\x10A\x02\x00',5),
                (b'\x10A',2),(b'\x13A\x01\x00\x50hello',12)):
            with self.subTest(block=block):
                with self.assertRaises(ValueError):
                    packing.decode_lz4(block,expected)

    def test_repeatable_and_bounded_input(self):
        raw = b'abcd'*100+b'last five'
        self.assertEqual(self.check_block(raw),self.check_block(raw))
        with self.assertRaises(ValueError):
            packing.compress_lz4(b'x'*65536)


if __name__ == '__main__':
    unittest.main()
