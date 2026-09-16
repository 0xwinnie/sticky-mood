#!/usr/bin/env python3
"""Binarize line-art sources in assets/src/ into 1-bit assets in assets/1bit/.

Hard rules from docs/ASSET-SPEC.md §0: pure black/white only, stroke >= 2 px at
the target size, >= 2 px white margin on all four sides.
"""
import sys
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "assets" / "src"
OUT = ROOT / "assets" / "1bit"

CAT_TARGETS = {
    "cat-home": (220, 140),
    "cat-mood-rough": (96, 80),
    "cat-mood-low": (96, 80),
    "cat-mood-okay": (96, 80),
    "cat-mood-good": (96, 80),
    "cat-mood-great": (96, 80),
    "cat-energy": (200, 100),
    "cat-done": (200, 150),
    "cat-listening": (160, 140),
    "cat-saved": (180, 140),
    "cat-logo": (140, 60),
}
MOOD_ORDER = ["cat-mood-rough", "cat-mood-low", "cat-mood-okay", "cat-mood-good", "cat-mood-great"]


def load_mask(path: Path) -> np.ndarray:
    g = np.asarray(Image.open(path).convert("L"))
    return g < 128


def crop(mask: np.ndarray) -> np.ndarray:
    ys, xs = np.where(mask)
    return mask[ys.min():ys.max() + 1, xs.min():xs.max() + 1]


def slice_sheet(mask: np.ndarray, n: int = 5) -> list:
    """Split a row of n faces by empty column gaps."""
    cols = mask.sum(axis=0)
    busy = cols > 0
    faces, start = [], None
    for x, b in enumerate(busy):
        if b and start is None:
            start = x
        elif not b and start is not None:
            faces.append((start, x))
            start = None
    if start is not None:
        faces.append((start, len(busy)))
    # merge clusters separated by tiny gaps (< 1% of width)
    gap = max(4, mask.shape[1] // 100)
    merged = [faces[0]]
    for s, e in faces[1:]:
        if s - merged[-1][1] < gap:
            merged[-1] = (merged[-1][0], e)
        else:
            merged.append((s, e))
    if len(merged) != n:
        sys.exit(f"mood sheet: expected {n} faces, found {len(merged)} at {merged}")
    return [crop(mask[:, s:e]) for s, e in merged]


def runs(line: np.ndarray) -> list:
    out, cur, prev = [], 0, None
    for v in line:
        if v == prev:
            cur += 1
        else:
            if prev is not None:
                out.append((prev, cur))
            prev, cur = v, 1
    out.append((prev, cur))
    return out


def quality(mask: np.ndarray):
    """(hairline fraction, 1-px white-sliver fraction).

    A hairline is a black pixel with background on both sides horizontally or
    vertically, i.e. a 1-px-thick line segment. Tapered caps and whisker-root
    junctions produce a few of these even in good art, so we measure fractions
    rather than demanding zero.
    """
    pad = np.pad(mask, 1)
    l, r = pad[1:-1, :-2], pad[1:-1, 2:]
    u, d = pad[:-2, 1:-1], pad[2:, 1:-1]
    hair = mask & (((~l) & (~r)) | ((~u) & (~d)))
    n_black = int(mask.sum())
    sliver = n_sliver = n_gap = 0
    for line in list(mask) + list(mask.T):
        for i, (v, n) in enumerate(runs(line)):
            if not v and 0 < i < len(runs(line)) - 1:
                n_gap += 1
                if n == 1:
                    n_sliver += 1
    return (hair.sum() / max(n_black, 1), n_sliver / max(n_gap, 1))


def fit(mask: np.ndarray, w: int, h: int):
    """Downscale into (w-4, h-4) keeping strokes >= 2 px; returns placed mask."""
    box = (w - 4, h - 4)
    scale = min(box[0] / mask.shape[1], box[1] / mask.shape[0])
    dw, dh = max(1, round(mask.shape[1] * scale)), max(1, round(mask.shape[0] * scale))
    cov = np.asarray(Image.fromarray((mask * 255).astype(np.uint8)).resize((dw, dh), Image.BOX),
                     dtype=np.float32) / 255.0
    best = None
    for thr in (0.50, 0.42, 0.35, 0.28, 0.22, 0.16, 0.12):
        cand = cov >= thr
        hf, sf = quality(cand)
        if hf <= 0.02 and sf <= 0.05:
            best = (cand, thr, hf, sf)
            break
    if best is None:
        cand = cov >= 0.12
        hf, sf = quality(cand)
        best = (cand, 0.12, hf, sf)
    cand, thr, hf, sf = best
    out = np.zeros((h, w), dtype=bool)
    y, x = (h - dh) // 2, (w - dw) // 2
    out[y:y + dh, x:x + dw] = cand
    return out, thr, hf, sf


def save(mask: np.ndarray, aid: str) -> int:
    img = Image.fromarray(np.where(mask, 0, 255).astype(np.uint8), "L").convert("1")
    img.save(OUT / f"{aid}.png")
    return mask.shape[1] * mask.shape[0] // 8


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    total = 0
    report = []
    sheet = load_mask(SRC / "cat-mood-sheet.png")
    faces = dict(zip(MOOD_ORDER, slice_sheet(sheet)))
    sources = {aid: faces[aid] for aid in MOOD_ORDER}
    for aid in CAT_TARGETS:
        if aid not in sources:
            sources[aid] = crop(load_mask(SRC / f"{aid}.png"))
    for aid, (w, h) in CAT_TARGETS.items():
        out, thr, hf, sf = fit(sources[aid], w, h)
        total += save(out, aid)
        flag = "" if (hf <= 0.02 and sf <= 0.05) else "  <-- CHECK"
        report.append(f"{aid:16s} {w:3d}x{h:<3d} thr={thr:.2f} hair={hf*100:4.1f}% sliver={sf*100:4.1f}%{flag}")
    print("\n".join(report))
    print(f"cats 1-bit total: {total} B")


if __name__ == "__main__":
    main()
