#!/usr/bin/env bash
# Regenerates the font and sprite blobs the firmware embeds and the simulator
# reads. Run this before idf.py build on a fresh clone or after touching
# assets/fonts, assets/1bit, or the rasterizers in tools/.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tools/font/make_font.py
python3 tools/art/make_art_bin.py
