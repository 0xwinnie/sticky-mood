#include "ui/art.h"

#include <cstring>

bool Art::load(const uint8_t *blob, size_t len)
{
    if (len < 4) {
        return false;
    }
    std::memcpy(&width_, blob, 2);
    std::memcpy(&height_, blob + 2, 2);
    bits_ = blob + 4;
    return len >= 4 + static_cast<size_t>((width_ + 7) / 8) * height_;
}

void Art::draw(FrameBuffer &fb, int x, int y, bool black) const
{
    if (bits_ != nullptr) {
        fb.blit(bits_, width_, height_, x, y, black);
    }
}

void Art::draw_centered(FrameBuffer &fb, int cx, int y, bool black) const
{
    draw(fb, cx - width_ / 2, y, black);
}
