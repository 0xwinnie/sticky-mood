#!/usr/bin/env python3
"""Draw the 12 UI icons as deterministic 1-bit pixel art into assets/1bit/.

Icons are 20-48 px; AI output does not survive 1-bit conversion at that size, so
they are drawn programmatically at 4x supersample and box-downscaled, which gives
exact stroke widths and clean diagonals. Strokes follow docs/ASSET-SPEC.md §0.
"""
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "assets" / "1bit"
S = 4  # supersample factor


class Pen:
    def __init__(self, w, h):
        self.img = Image.new("L", (w * S, h * S), 255)
        self.d = ImageDraw.Draw(self.img)
        self.w, self.h = w, h

    def _x(self, v):
        return v * S

    def line(self, pts, w):
        flat = [(self._x(p[0]), self._x(p[1])) for p in pts]
        self.d.line(flat, fill=0, width=w * S, joint="curve")

    def circle(self, box, w=None, fill=False):
        b = [self._x(v) for v in box]
        if fill:
            self.d.ellipse(b, fill=0)
        else:
            self.d.ellipse(b, outline=0, width=w * S)

    def arc(self, box, a0, a1, w):
        self.d.arc([self._x(v) for v in box], a0, a1, fill=0, width=w * S)

    def poly(self, pts, w=None, fill=False):
        p = [(self._x(a), self._x(b)) for a, b in pts]
        if fill:
            self.d.polygon(p, fill=0)
        else:
            self.d.polygon(p, outline=0, width=w * S)

    def rrect(self, box, r, w):
        self.d.rounded_rectangle([self._x(v) for v in box], self._x(r), outline=0, width=w * S)

    def rect_fill(self, box):
        self.d.rectangle([self._x(v) for v in box], fill=0)

    def cut_below(self, y):
        a = np.array(self.img)
        a[y * S:, :] = 255
        self.img = Image.fromarray(a)
        self.d = ImageDraw.Draw(self.img)

    def cut_mask(self, other_mask):
        a = np.array(self.img)
        a[other_mask] = 255
        self.img = Image.fromarray(a)
        self.d = ImageDraw.Draw(self.img)

    def finish(self):
        small = self.img.resize((self.w, self.h), Image.BOX)
        a = np.asarray(small) < 128
        return a


def icon_focus():
    p = Pen(32, 32)
    p.circle((3, 3, 28, 28), 2)
    p.circle((10, 10, 21, 21), 2)
    p.circle((14, 14, 17, 17), fill=True)
    return p.finish()


def icon_create():
    p = Pen(32, 32)
    p.poly([(7, 21), (21, 7), (25, 11), (11, 25)], 2)          # barrel
    p.poly([(7, 21), (5, 27), (11, 25)], 2)                    # sharpened tip
    p.line([(9, 19), (13, 23)], 2)                             # tip separator
    p.line([(19, 9), (23, 13)], 2)                             # eraser band
    return p.finish()


def icon_rest():
    p = Pen(32, 32)
    outer = np.zeros((32 * S, 32 * S), bool)
    cut = np.zeros((32 * S, 32 * S), bool)
    yy, xx = np.mgrid[0:32 * S, 0:32 * S]
    outer[((xx - 15 * S) ** 2 + (yy - 16 * S) ** 2) <= (12 * S) ** 2] = True
    cut[((xx - 20 * S) ** 2 + (yy - 11 * S) ** 2) <= (10 * S) ** 2] = True
    crescent = outer & ~cut
    img = np.where(crescent, 0, 255).astype(np.uint8)
    small = Image.fromarray(img).resize((32, 32), Image.BOX)
    return np.asarray(small) < 128


def icon_move():
    p = Pen(32, 32)
    p.circle((17, 3, 23, 9), fill=True)                        # head
    p.line([(19, 10), (15, 19)], 3)                            # torso
    p.line([(16, 13), (23, 16)], 3)                            # front arm
    p.line([(16, 14), (9, 11)], 3)                             # back arm
    p.line([(15, 19), (22, 23), (23, 29)], 3)                  # front leg
    p.line([(15, 19), (10, 24), (6, 27)], 3)                   # back leg
    return p.finish()


def icon_connect():
    p = Pen(32, 32)
    p.circle((5, 5, 13, 13), 2)
    p.circle((19, 5, 27, 13), 2)
    p.circle((2, 16, 16, 30), 2)
    p.circle((16, 16, 30, 30), 2)
    p.cut_below(28)
    return p.finish()


def icon_mic():
    p = Pen(28, 28)
    p.rrect((9, 2, 18, 15), 4, 2)                              # capsule
    p.line([(12, 5), (12, 12)], 2)                             # grille
    p.line([(15, 5), (15, 12)], 2)
    p.arc((5, 6, 22, 20), 0, 180, 2)                           # cradle
    p.line([(13, 20), (13, 24)], 2)                            # stem
    p.line([(9, 24), (18, 24)], 2)                             # base
    return p.finish()


def icon_battery_frame():
    p = Pen(48, 24)
    p.rrect((1, 4, 42, 19), 4, 2)
    p.rect_fill((43, 9, 46, 14))                               # terminal nub
    return p.finish()


def icon_heart():
    p = Pen(20, 20)
    p.circle((2, 3, 10, 11), fill=True)
    p.circle((9, 3, 17, 11), fill=True)
    p.poly([(3, 8), (16, 8), (10, 17)], fill=True)
    return p.finish()


def icon_arrow_left():
    p = Pen(24, 24)
    p.line([(15, 5), (8, 12), (15, 19)], 3)
    return p.finish()


def icon_arrow_right():
    p = Pen(24, 24)
    p.line([(9, 5), (16, 12), (9, 19)], 3)
    return p.finish()


def icon_plus():
    p = Pen(24, 24)
    p.line([(12, 5), (12, 19)], 3)
    p.line([(5, 12), (19, 12)], 3)
    return p.finish()


def icon_close():
    p = Pen(24, 24)
    p.line([(6, 6), (18, 18)], 3)
    p.line([(18, 6), (6, 18)], 3)
    return p.finish()


ICONS = {
    "icon-focus": icon_focus,
    "icon-create": icon_create,
    "icon-rest": icon_rest,
    "icon-move": icon_move,
    "icon-connect": icon_connect,
    "icon-mic": icon_mic,
    "icon-battery-frame": icon_battery_frame,
    "icon-heart": icon_heart,
    "icon-arrow-left": icon_arrow_left,
    "icon-arrow-right": icon_arrow_right,
    "icon-plus": icon_plus,
    "icon-close": icon_close,
}


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    total = 0
    for aid, fn in ICONS.items():
        mask = fn()
        h, w = mask.shape
        Image.fromarray(np.where(mask, 0, 255).astype(np.uint8), "L").convert("1").save(OUT / f"{aid}.png")
        total += w * h // 8
        print(f"{aid:20s} {w:2d}x{h:<2d} black={mask.sum():4d}px")
    print(f"icons 1-bit total: {total} B")


if __name__ == "__main__":
    main()
