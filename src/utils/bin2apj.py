#!/usr/bin/env python3
"""Convert a raw binary firmware to ArduPilot .apj format."""
import json
import base64
import sys
import os
import zlib

if len(sys.argv) < 3:
    print(f"Usage: {sys.argv[0]} <input.bin> <output.apj> [board_id]")
    print(f"  board_id defaults to 56 (Pixhawk 6C / FMUv6C)")
    sys.exit(1)

input_bin = sys.argv[1]
output_apj = sys.argv[2]
board_id = int(sys.argv[3]) if len(sys.argv) > 3 else 56  # FMUv6C = 56

with open(input_bin, 'rb') as f:
    firmware = f.read()

image_size = len(firmware)
crc = zlib.crc32(firmware) & 0xFFFFFFFF

desc = {
    "board_id": board_id,
    "magic": "PX4FWv1",
    "description": "INAV firmware for Pixhawk 6C",
    "image_size": image_size,
    "image": base64.b64encode(zlib.compress(firmware, 9)).decode('ascii'),
    "summary": "INAV",
    "board_revision": 0,
}

with open(output_apj, 'w') as f:
    json.dump(desc, f)

print(f"Created {output_apj}")
print(f"  Board ID: {board_id}")
print(f"  Image size: {image_size} bytes ({image_size/1024:.1f} KB)")
print(f"  CRC32: 0x{crc:08X}")
