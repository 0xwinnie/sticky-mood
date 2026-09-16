#!/bin/bash
# Host build of the web preview: fonts -> art -> sim render -> clickable HTML.
set -e
cd "$(dirname "$0")/../.."
mkdir -p build/sim

/opt/homebrew/bin/python3 tools/font/make_font.py
/opt/homebrew/bin/python3 tools/art/make_art_bin.py

g++ -std=c++17 -Wall -Wextra -Imain -o build/sim/sim \
    main/ui/framebuffer.cpp \
    main/ui/bitmap_font.cpp \
    main/ui/art.cpp \
    main/ui/resources.cpp \
    main/pages/pages.cpp \
    tools/sim/sim_main.cpp

./build/sim/sim .
/opt/homebrew/bin/python3 tools/sim/make_preview.py
echo "open build/sim/index.html"
