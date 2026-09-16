#pragma once

#include <cstdint>

class StickyDisplay;
class FrameBuffer;
class StickyTouch;
struct Resources;

// Boot-time service mode. The app firmware checks diag::requested() once before
// the normal loop; when the user held UP at power-on (or we woke from a
// diag-initiated deep sleep) it runs the on-e-ink spike menu instead of the app.
//
// Every result is written twice: to the panel and to ESP_LOGI, so a borrow-test
// can be read off the screen with no serial cable, or captured in full over USB.
namespace diag {

// Shared handles the spikes use to draw, refresh and read input.
struct DiagUi {
    StickyDisplay *display = nullptr;
    FrameBuffer *fb = nullptr;
    const Resources *res = nullptr;
    StickyTouch *touch = nullptr;
};

enum class Event { None, Up, Down, Ok, Tap };

constexpr int kMaxLines = 9;
constexpr int kLineLen = 56;

// True when UP was held at power-on or a diag deep-sleep test asked to come back.
bool requested();

// Latches an RTC-memory flag so the next deep-sleep wake re-enters this menu.
// Called by the power spike right before esp_deep_sleep_start().
void arm_return_after_sleep();

// Enters the service menu. Returns only when the user picks "Reboot to app";
// the caller should then esp_restart().
void run(StickyDisplay &, FrameBuffer &, const Resources &, StickyTouch &);

// --- helpers shared by the spikes ---
void init_input();
// One debounced input event. On Tap, *tx/*ty hold logical 480x800 coords.
Event poll(DiagUi &, int *tx, int *ty);
// Blocks until the next event and returns it (30 ms poll).
Event wait_event(DiagUi &, int *tx, int *ty);
// Renders title bar + body lines and does a full refresh.
void show(DiagUi &, const char *title, const char *const *lines, int n);
void flush(DiagUi &);
// Waits for OK or a tap (used to dismiss a finished result screen).
void wait_exit(DiagUi &);

}  // namespace diag
