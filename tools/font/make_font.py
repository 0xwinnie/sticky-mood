#!/usr/bin/env python3
"""Rasterize the project fonts into indexed 1-bit blob files.

The same blobs are consumed by the firmware (embedded) and by the host
simulator (read from disk), so what the web preview shows is pixel-identical
to what the panel will draw.

Blob layout (little endian):
  u32 magic 'SMF1'
  u32 nfaces
  per face:
    u32 face_id          # px_height * 10 + kind  (kind 0=latin, 1=zh)
    u8  height           # em box height in px
    u8  ascent           # baseline offset from glyph top
    u32 nglyph
    nglyph * 13-byte index entries, sorted by cp:
        u32 cp, u32 off, u8 w, u8 h, u8 adv, i8 bear_x, i8 bear_y
        off is relative to the start of this face's glyph data
    glyph data: row-major, 1 bit/px MSB first, each row padded to whole bytes,
                bit set = black (ink); w=0 means no ink (space)
"""
import struct
import sys
from pathlib import Path

from fontTools.ttLib import TTFont
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "build" / "font_data"

ZH_FONT = ROOT / "assets" / "fonts" / "gensen-rounded" / "GenSenRounded2TC-M-gb2312.otf"
ZH_BOLD = ROOT / "assets" / "fonts" / "gensen-rounded" / "GenSenRounded2TC-B-gb2312.otf"
LATIN_FONT = ROOT / "assets" / "fonts" / "manrope" / "Manrope-VariableFont_wght.ttf"

ZH_SIZES = (16, 18, 24, 28)
LATIN_SIZES = (14, 16, 18, 20, 24, 28)


def gb2312_codepoints():
    cps = set(range(0x20, 0x7F))
    cps |= {0x2018, 0x2019, 0x201C, 0x201D, 0x2026, 0x2014, 0x00B7, 0x2013, 0x2022, 0x00D7}
    for hi in range(0xA1, 0xF8):
        for lo in range(0xA1, 0xFF):
            try:
                cps.add(ord(bytes([hi, lo]).decode("gb2312")))
            except UnicodeDecodeError:
                pass
    return sorted(cps)


def rasterize(font_path, size, codepoints, weight=None):
    """Return (ascent, [(cp, w, adv, raw_bytes, bear_x, bear_y)])."""
    font = ImageFont.truetype(str(font_path), size)
    if weight is not None:
        font.set_variation_by_axes([weight])
    ascent = font.getmetrics()[0]
    glyphs = []
    for cp in codepoints:
        ch = chr(cp)
        pad = size
        img = Image.new("1", (size * 4, size * 4), 0)
        d = ImageDraw.Draw(img)
        d.text((pad, pad), ch, font=font, fill=1)
        bbox = img.getbbox()
        adv = int(round(font.getlength(ch)))
        if bbox is None:  # space or empty glyph
            glyphs.append((cp, 0, 0, adv, b"", 0, 0))
            continue
        x0, y0, x1, y1 = bbox
        w = x1 - x0
        h = y1 - y0
        sub = img.crop((x0, y0, x1, y1))
        px = sub.load()
        rowbytes = (w + 7) // 8
        raw = bytearray()
        for yy in range(h):
            for b in range(rowbytes):
                byte = 0
                for bit in range(8):
                    xx = b * 8 + bit
                    if xx < w and px[xx, yy]:
                        byte |= 0x80 >> bit
                raw.append(byte)
        glyphs.append((cp, w, h, adv, bytes(raw), x0 - pad, y0 - pad))
    return ascent, glyphs


def pack_face(face_id, height, ascent, glyphs):
    idx = bytearray()
    data = bytearray()
    for cp, w, h, adv, raw, bx, by in glyphs:
        off = len(data)
        data += raw
        idx += struct.pack("<IIBBBbb", cp, off, w, h, adv, bx, by)
    head = struct.pack("<IBBI", face_id, height, ascent, len(glyphs))
    return head + bytes(idx) + bytes(data)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    faces = []

    for size in LATIN_SIZES:
        cps = list(range(0x20, 0x7F))
        weight = 700 if size >= 24 else None
        ascent, glyphs = rasterize(LATIN_FONT, size, cps, weight)
        faces.append((f"latin{size}", pack_face(size * 10 + 0, size, ascent, glyphs)))

    zh = gb2312_codepoints()
    for size in ZH_SIZES:
        ascent, glyphs = rasterize(ZH_BOLD if size >= 24 else ZH_FONT, size, zh)
        faces.append((f"zh{size}", pack_face(size * 10 + 1, size, ascent, glyphs)))

    for name, blob in faces:
        (OUT / f"font_{name}.bin").write_bytes(blob)
        print(f"font_{name}.bin  {len(blob)//1024} KB")

    total = sum(len(b) for _, b in faces)
    print(f"total {total//1024} KB")


if __name__ == "__main__":
    sys.exit(main())
