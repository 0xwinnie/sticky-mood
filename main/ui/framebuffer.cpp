#include "ui/framebuffer.h"

#include <cstring>

namespace {

int isqrt_round(int v)
{
    if (v <= 0) {
        return 0;
    }
    int r = static_cast<int>(__builtin_sqrt(static_cast<double>(v)));
    while (r * r > v) {
        --r;
    }
    while ((r + 1) * (r + 1) <= v) {
        ++r;
    }
    return r;
}

}  // namespace

FrameBuffer::FrameBuffer()
{
    owned_ = new uint8_t[kBytes];
    pixels_ = owned_;
    clear();
}

FrameBuffer::FrameBuffer(uint8_t *backing)
    : pixels_(backing)
{
}

FrameBuffer::~FrameBuffer()
{
    delete[] owned_;
}

void FrameBuffer::clear(bool white)
{
    std::memset(pixels_, white ? 0x00 : 0xFF, kBytes);
}

void FrameBuffer::set_pixel(int x, int y, bool black)
{
    if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) {
        return;
    }
    uint8_t &byte = pixels_[y * kStride + x / 8];
    const uint8_t mask = 0x80 >> (x % 8);
    if (black) {
        byte |= mask;
    } else {
        byte &= ~mask;
    }
}

bool FrameBuffer::get_pixel(int x, int y) const
{
    if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) {
        return false;
    }
    return (pixels_[y * kStride + x / 8] >> (7 - x % 8)) & 0x01;
}

void FrameBuffer::hline(int x, int y, int len, bool black)
{
    for (int i = 0; i < len; ++i) {
        set_pixel(x + i, y, black);
    }
}

void FrameBuffer::vline(int x, int y, int len, bool black)
{
    for (int i = 0; i < len; ++i) {
        set_pixel(x, y + i, black);
    }
}

void FrameBuffer::fill_rect(int x, int y, int w, int h, bool black)
{
    for (int row = 0; row < h; ++row) {
        hline(x, y + row, w, black);
    }
}

void FrameBuffer::draw_rect(int x, int y, int w, int h, bool black)
{
    hline(x, y, w, black);
    hline(x, y + h - 1, w, black);
    vline(x, y, h, black);
    vline(x + w - 1, y, h, black);
}

void FrameBuffer::fill_rounded_rect(int x, int y, int w, int h, int r, bool black)
{
    for (int dy = 0; dy < h; ++dy) {
        int inset = 0;
        if (dy < r) {
            inset = r - isqrt_round(r * r - (r - 1 - dy) * (r - 1 - dy));
        } else if (dy >= h - r) {
            const int d = r - (h - dy);
            inset = r - isqrt_round(r * r - d * d);
        }
        if (w - 2 * inset > 0) {
            hline(x + inset, y + dy, w - 2 * inset, black);
        }
    }
}

void FrameBuffer::draw_rounded_rect(int x, int y, int w, int h, int r, bool black)
{
    hline(x + r, y, w - 2 * r, black);
    hline(x + r, y + 1, w - 2 * r, black);
    hline(x + r, y + h - 1, w - 2 * r, black);
    hline(x + r, y + h - 2, w - 2 * r, black);
    vline(x, y + r, h - 2 * r, black);
    vline(x + 1, y + r, h - 2 * r, black);
    vline(x + w - 1, y + r, h - 2 * r, black);
    vline(x + w - 2, y + r, h - 2 * r, black);
    for (int t = 0; t <= r; ++t) {
        const int c = isqrt_round(r * r - t * t);
        for (int k = 0; k < 2; ++k) {
            const int cc = c - k;
            if (cc < 0) {
                continue;
            }
            set_pixel(x + r - cc, y + r - t, black);
            set_pixel(x + r - t, y + r - cc, black);
            set_pixel(x + w - 1 - r + cc, y + r - t, black);
            set_pixel(x + w - 1 - r + t, y + r - cc, black);
            set_pixel(x + r - cc, y + h - 1 - r + t, black);
            set_pixel(x + r - t, y + h - 1 - r + cc, black);
            set_pixel(x + w - 1 - r + cc, y + h - 1 - r + t, black);
            set_pixel(x + w - 1 - r + t, y + h - 1 - r + cc, black);
        }
    }
}

void FrameBuffer::blit(const uint8_t *bits, int w, int h, int x, int y, bool black)
{
    const int row_bytes = (w + 7) / 8;
    for (int row = 0; row < h; ++row) {
        for (int col = 0; col < w; ++col) {
            const uint8_t byte = bits[row * row_bytes + col / 8];
            if ((byte >> (7 - col % 8)) & 0x01) {
                set_pixel(x + col, y + row, black);
            }
        }
    }
}
