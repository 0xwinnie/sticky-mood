#pragma once

#include <cstdint>

#include "driver/spi_master.h"
#include "epaper_panel.h"

// Owns the SSD1677 panel and a 1-bit framebuffer in PSRAM.
//
// ORIENTATION: the panel is natively 800x480 landscape. This class exposes a
// 480x800 portrait canvas rotated 270 degrees, which is the transform verified
// on production hardware together with mirror_x=true. The rotation lives in
// exactly one place (draw_pixel) and StickyTouch applies the matching inverse
// transform. If you change one you must change the other, or the touch layer
// comes out rotated or mirrored relative to what is drawn.
class StickyDisplay {
public:
    static constexpr int kWidth = 480;
    static constexpr int kHeight = 800;

    bool init();

    void clear(bool white = true);
    void draw_pixel(int x, int y, bool black = true);
    void draw_rect(int x, int y, int width, int height, bool black = true);
    void fill_rect(int x, int y, int width, int height, bool black = true);

    // Copies a whole UI-core frame (480x800 portrait, row-major MSB first,
    // kStride=60 bytes per row, set = black) into the native panel buffer,
    // applying the 270-degree rotation and the set=white inversion here so no
    // page code ever thinks about panel orientation or polarity.
    void blit_ui(const uint8_t *portrait);

    // Full refresh flashes the panel and clears ghosting. Slow (seconds), blocks.
    bool refresh_full();
    // Partial refresh compares whole frames and leaves identical pixels alone,
    // so erase the area you are replacing before drawing. Accumulates ghosting:
    // force a full refresh roughly every 20 partials and always on page change.
    bool refresh_partial();

    // Puts the controller to sleep before deep sleep. The last committed image
    // stays visible while the device is powered down.
    void panel_sleep();

private:
    bool refresh(seeed_epaper_refresh_mode_t mode);

    static constexpr int kNativeWidth = 800;
    static constexpr int kNativeHeight = 480;
    static constexpr int kStrideBytes = kNativeWidth / 8;
    static constexpr int kBufferSize = kStrideBytes * kNativeHeight;

    spi_device_handle_t spi_device_ = nullptr;
    seeed_epaper_panel_handle_t panel_ = nullptr;
    uint8_t *framebuffer_ = nullptr;
};
