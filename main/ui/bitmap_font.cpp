#include "ui/bitmap_font.h"

#include <cstring>

namespace {

// Decodes one UTF-8 code point, advancing *s. Returns 0 at end of string.
uint32_t next_cp(const char **s)
{
    const uint8_t *p = reinterpret_cast<const uint8_t *>(*s);
    if (*p == 0) {
        return 0;
    }
    uint32_t cp;
    int extra;
    if (*p < 0x80) {
        cp = *p;
        extra = 0;
    } else if ((*p & 0xE0) == 0xC0) {
        cp = *p & 0x1F;
        extra = 1;
    } else if ((*p & 0xF0) == 0xE0) {
        cp = *p & 0x0F;
        extra = 2;
    } else {
        cp = *p & 0x07;
        extra = 3;
    }
    ++p;
    for (int i = 0; i < extra; ++i) {
        cp = (cp << 6) | (*p++ & 0x3F);
    }
    *s = reinterpret_cast<const char *>(p);
    return cp;
}

}  // namespace

bool BitmapFont::load(const uint8_t *blob, size_t len)
{
    // face header: u32 face_id, u8 height, u8 ascent, u32 nglyph
    if (len < kHeaderSize) {
        return false;
    }
    uint32_t nglyph;
    std::memcpy(&nglyph, blob + 6, 4);
    if (len < kHeaderSize + static_cast<size_t>(nglyph) * kEntrySize) {
        return false;
    }
    height_ = blob[4];
    ascent_ = blob[5];
    nglyph_ = nglyph;
    index_ = blob + kHeaderSize;
    data_ = index_ + static_cast<size_t>(nglyph) * kEntrySize;
    blob_ = blob;
    len_ = len;
    return true;
}

bool BitmapFont::find(uint32_t cp, Glyph *out) const
{
    size_t lo = 0;
    size_t hi = nglyph_;
    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        const uint8_t *e = entry(mid);
        uint32_t mid_cp;
        std::memcpy(&mid_cp, e, 4);
        if (mid_cp == cp) {
            std::memcpy(&out->cp, e + 0, 4);
            std::memcpy(&out->off, e + 4, 4);
            out->w = e[8];
            out->h = e[9];
            out->adv = e[10];
            out->bx = static_cast<int8_t>(e[11]);
            out->by = static_cast<int8_t>(e[12]);
            return true;
        }
        if (mid_cp < cp) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return false;
}

int BitmapFont::width(const char *utf8) const
{
    int total = 0;
    const char *s = utf8;
    Glyph g;
    while (uint32_t cp = next_cp(&s)) {
        total += find(cp, &g) ? g.adv : height_ / 2;
    }
    return total;
}

int BitmapFont::draw(FrameBuffer &fb, int x, int y, const char *utf8, bool black) const
{
    int cursor = x;
    const char *s = utf8;
    Glyph g;
    while (uint32_t cp = next_cp(&s)) {
        if (!find(cp, &g)) {
            cursor += height_ / 2;
            continue;
        }
        if (g.w > 0 && g.h > 0) {
            fb.blit(data_ + g.off, g.w, g.h, cursor + g.bx, y + g.by, black);
        }
        cursor += g.adv;
    }
    return cursor - x;
}

void BitmapFont::draw_centered(FrameBuffer &fb, int y, const char *utf8, bool black) const
{
    draw(fb, (FrameBuffer::kWidth - width(utf8)) / 2, y, utf8, black);
}

void BitmapFont::draw_centered_at(FrameBuffer &fb, int center_x, int y, const char *utf8, bool black) const
{
    draw(fb, center_x - width(utf8) / 2, y, utf8, black);
}

void BitmapFont::draw_right(FrameBuffer &fb, int right_x, int y, const char *utf8, bool black) const
{
    draw(fb, right_x - width(utf8), y, utf8, black);
}
