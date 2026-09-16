#pragma once

#include <cstdint>

// 1-bit portrait framebuffer, the single drawing surface for every page.
// Bit set = black (ink). The e-paper panel wants the inverse (set = white);
// StickyDisplay flips it when blitting, so no page code ever thinks about it.
//
// This header must stay free of ESP-IDF includes: the host simulator compiles
// the same pages against this class to produce the web preview.
class FrameBuffer {
public:
    static constexpr int kWidth = 480;
    static constexpr int kHeight = 800;
    static constexpr int kStride = kWidth / 8;
    static constexpr int kBytes = kStride * kHeight;

    FrameBuffer();                       // owns a heap buffer
    explicit FrameBuffer(uint8_t *backing);  // wraps caller memory (device PSRAM)
    ~FrameBuffer();

    FrameBuffer(const FrameBuffer &) = delete;
    FrameBuffer &operator=(const FrameBuffer &) = delete;

    void clear(bool white = true);
    void set_pixel(int x, int y, bool black = true);
    bool get_pixel(int x, int y) const;

    void fill_rect(int x, int y, int w, int h, bool black = true);
    void draw_rect(int x, int y, int w, int h, bool black = true);
    void fill_rounded_rect(int x, int y, int w, int h, int r, bool black = true);
    void draw_rounded_rect(int x, int y, int w, int h, int r, bool black = true);
    void hline(int x, int y, int len, bool black = true);
    void vline(int x, int y, int len, bool black = true);

    // 1-bit sprite, row-major MSB first, rows padded to whole bytes, set = black.
    void blit(const uint8_t *bits, int w, int h, int x, int y, bool black = true);

    const uint8_t *data() const { return pixels_; }
    uint8_t *data() { return pixels_; }

private:
    uint8_t *pixels_ = nullptr;
    uint8_t *owned_ = nullptr;
};
