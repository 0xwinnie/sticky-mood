#!/usr/bin/env python3
"""Convert assets/1bit/*.png into raw 1-bit sprites for the simulator/firmware.

Output per asset: build/art_data/<id>.bin = u16 width, u16 height, then
row-major MSB-first bytes with bit set = black (same convention as FrameBuffer).
"""
import struct
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "assets" / "1bit"
OUT = ROOT / "build" / "art_data"


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    total = 0
    for png in sorted(SRC.glob("*.png")):
        im = Image.open(png).convert("1")
        w, h = im.size
        px = im.load()
        raw = bytearray()
        for y in range(h):
            for b in range((w + 7) // 8):
                byte = 0
                for bit in range(8):
                    x = b * 8 + bit
                    if x < w and px[x, y] == 0:  # PIL: 0 = black
                        byte |= 0x80 >> bit
                raw.append(byte)
        (OUT / f"{png.stem}.bin").write_bytes(struct.pack("<HH", w, h) + bytes(raw))
        total += len(raw) + 4
    print(f"{len(list(SRC.glob('*.png')))} sprites, {total} B -> {OUT}")


if __name__ == "__main__":
    sys.exit(main())
