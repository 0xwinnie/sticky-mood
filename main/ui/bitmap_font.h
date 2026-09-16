#pragma once

#include <cstddef>
#include <cstdint>

#include "ui/framebuffer.h"

// Renderer for the 1-bit font blobs produced by tools/font/make_font.py.
// One instance per face (e.g. zh16, latin24). Coordinates are top-left of the
// em box; bearings place the inked glyph inside it.
class BitmapFont {
public:
    bool load(const uint8_t *blob, size_t len);

    int height() const { return height_; }
    int ascent() const { return ascent_; }

    // Draws utf8 text with its em-box top at y. Returns the total advance.
    int draw(FrameBuffer &fb, int x, int y, const char *utf8, bool black = true) const;
    int width(const char *utf8) const;

    void draw_centered(FrameBuffer &fb, int y, const char *utf8, bool black = true) const;
    void draw_centered_at(FrameBuffer &fb, int center_x, int y, const char *utf8, bool black = true) const;
    void draw_right(FrameBuffer &fb, int right_x, int y, const char *utf8, bool black = true) const;

private:
    struct Glyph {
        uint32_t cp;
        uint32_t off;
        uint8_t w;
        uint8_t h;
        uint8_t adv;
        int8_t bx;
        int8_t by;
    };

    // Index entries are packed <IIBBBbb in the blob. Reading them through a
    // Glyph* would use the aligned 16-byte stride and desync from the data.
    static constexpr size_t kEntrySize = 13;
    static constexpr size_t kHeaderSize = 10;

    const uint8_t *entry(size_t i) const { return index_ + i * kEntrySize; }
    bool find(uint32_t cp, Glyph *out) const;

    const uint8_t *blob_ = nullptr;
    size_t len_ = 0;
    const uint8_t *index_ = nullptr;
    const uint8_t *data_ = nullptr;
    uint32_t nglyph_ = 0;
    uint8_t height_ = 0;
    uint8_t ascent_ = 0;
};
