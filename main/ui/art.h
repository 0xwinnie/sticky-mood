#pragma once

#include <cstddef>
#include <cstdint>

#include "ui/framebuffer.h"

// A 1-bit sprite loaded from the raw files emitted by tools/art/make_art_bin.py
// (u16 width, u16 height, then row-major MSB-first bits, set = black).
class Art {
public:
    bool load(const uint8_t *blob, size_t len);

    int width() const { return width_; }
    int height() const { return height_; }
    void draw(FrameBuffer &fb, int x, int y, bool black = true) const;
    void draw_centered(FrameBuffer &fb, int cx, int y, bool black = true) const;

private:
    const uint8_t *bits_ = nullptr;
    uint16_t width_ = 0;
    uint16_t height_ = 0;
};
