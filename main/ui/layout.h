#pragma once

#include "ui/framebuffer.h"

// Layout grid for the 480x800 portrait canvas. See docs/ASSET-SPEC.md §5.
namespace layout {

constexpr int kMargin = 24;
constexpr int kContentWidth = FrameBuffer::kWidth - 2 * kMargin;  // 432

constexpr int kStatusBarTop = 0;
constexpr int kStatusBarHeight = 40;
constexpr int kContentTop = 40;
constexpr int kNavTop = 740;
constexpr int kNavHeight = 60;

// Font face ids, matching tools/font/make_font.py output names.
enum Face {
    kLatin14,
    kLatin16,
    kLatin18,
    kLatin20,
    kLatin24,
    kLatin28,
    kZh16,
    kZh18,
    kFaceCount,
};

}  // namespace layout
