#!/usr/bin/env python3
"""Compose every 1-bit asset in assets/1bit/ into one labelled preview sheet."""
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
ONE = ROOT / "assets" / "1bit"
OUT = ROOT / "assets" / "preview-contact-sheet.png"
FONT = ROOT / "assets" / "fonts" / "manrope" / "Manrope-VariableFont_wght.ttf"

ORDER = [
    "cat-home", "cat-mood-rough", "cat-mood-low", "cat-mood-okay", "cat-mood-good",
    "cat-mood-great", "cat-energy", "cat-done", "cat-listening", "cat-saved", "cat-logo",
    "icon-focus", "icon-create", "icon-rest", "icon-move", "icon-connect", "icon-mic",
    "icon-battery-frame", "icon-heart", "icon-arrow-left", "icon-arrow-right",
    "icon-plus", "icon-close",
]
CELL_W, CELL_H, LABEL_H, PAD = 240, 190, 26, 12
COLS = 6


def main():
    rows = (len(ORDER) + COLS - 1) // COLS
    sheet = Image.new("L", (COLS * (CELL_W + PAD) + PAD, rows * (CELL_H + LABEL_H + PAD) + PAD), 255)
    d = ImageDraw.Draw(sheet)
    font = ImageFont.truetype(str(FONT), 15)
    total = 0
    for i, aid in enumerate(ORDER):
        im = Image.open(ONE / f"{aid}.png").convert("1")
        w, h = im.size
        total += w * h // 8
        zoom = max(1, min(3, CELL_W // w, CELL_H // h))
        big = im.resize((w * zoom, h * zoom), Image.NEAREST)
        cx = PAD + (i % COLS) * (CELL_W + PAD)
        cy = PAD + (i // COLS) * (CELL_H + LABEL_H + PAD)
        d.rectangle([cx - 4, cy - 4, cx + CELL_W + 4, cy + CELL_H + LABEL_H + 4], outline=200)
        sheet.paste(big, (cx + (CELL_W - w * zoom) // 2, cy + (CELL_H - h * zoom) // 2))
        d.text((cx, cy + CELL_H + 4), f"{aid}  {w}x{h}", font=font, fill=0)
    sheet.save(OUT)
    print(f"{len(ORDER)} assets, 1-bit total {total} B -> {OUT}")


if __name__ == "__main__":
    main()
