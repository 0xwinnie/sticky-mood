#include "diag/diag.h"

#include <cstdio>

#include "diag/spikes.h"
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hardware/sticky_display.h"
#include "hardware/sticky_touch.h"
#include "pin_config.h"
#include "ui/framebuffer.h"
#include "ui/layout.h"
#include "ui/resources.h"

namespace diag {
namespace {

constexpr const char *kTag = "diag";
constexpr uint32_t kDiagMagic = 0x44494147;  // "DIAG"

// Survives deep sleep so the power spike can come back to this menu; zeroed on a
// true cold boot, where it must not force us into diagnostics.
RTC_DATA_ATTR uint32_t g_diag_magic = 0;

constexpr int kTitleBarH = 64;
constexpr int kMenuTop = 156;
constexpr int kItemH = 76;
constexpr int kItemBoxH = 66;

const char *const kItems[] = {
    "SD card  (R/W)",
    "Microphone (PDM)",
    "RTC (PCF8563)",
    "Power (deep sleep)",
    "Buttons",
    "Touch (calibrate)",
    "Reboot to app",
};
constexpr int kItemCount = sizeof(kItems) / sizeof(kItems[0]);

void dispatch(DiagUi &ui, int sel)
{
    ESP_LOGI(kTag, "run item %d: %s", sel, kItems[sel]);
    switch (sel) {
    case 0: spike_sd(ui); break;
    case 1: spike_mic(ui); break;
    case 2: spike_rtc(ui); break;
    case 3: spike_power(ui); break;
    case 4: spike_input(ui); break;
    case 5: spike_touch(ui); break;
    case 6: esp_restart(); break;
    default: break;
    }
}

void render_menu(DiagUi &ui, int sel)
{
    FrameBuffer &fb = *ui.fb;
    fb.clear(true);

    fb.fill_rect(0, 0, FrameBuffer::kWidth, kTitleBarH, true);
    ui.res->font[layout::kLatin24].draw_centered(fb, 20, "DIAGNOSTIC MODE", false);
    ui.res->font[layout::kLatin14].draw_centered(fb, kTitleBarH + 14,
                                                 "UP/DOWN move  -  OK run  -  tap works", true);

    for (int i = 0; i < kItemCount; ++i) {
        const int y = kMenuTop + i * kItemH;
        const int text_y = y + (kItemBoxH - 20) / 2;
        if (i == sel) {
            fb.fill_rounded_rect(layout::kMargin, y, layout::kContentWidth, kItemBoxH, 10, true);
            ui.res->font[layout::kLatin20].draw(fb, layout::kMargin + 20, text_y, kItems[i], false);
        } else {
            ui.res->font[layout::kLatin20].draw(fb, layout::kMargin + 20, text_y, kItems[i], true);
        }
    }
    flush(ui);
}

}  // namespace

void arm_return_after_sleep()
{
    g_diag_magic = kDiagMagic;
}

void init_input()
{
    gpio_config_t config = {};
    config.pin_bit_mask = (1ULL << PIN_BTN_UP) | (1ULL << PIN_BTN_DOWN) | (1ULL << PIN_BTN_OK);
    config.mode = GPIO_MODE_INPUT;
    // Polarity is assumed active-low with internal pull-up; the Buttons spike
    // exists precisely to confirm this on hardware (QUESTIONS-FOR-SEEED C1).
    config.pull_up_en = GPIO_PULLUP_ENABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&config);
}

bool requested()
{
    if (g_diag_magic == kDiagMagic) {
        return true;
    }
    init_input();
    // Debounce: UP must read low (pressed) on several consecutive samples.
    for (int i = 0; i < 5; ++i) {
        if (gpio_get_level(static_cast<gpio_num_t>(PIN_BTN_UP)) != 0) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return true;
}

Event poll(DiagUi &ui, int *tx, int *ty)
{
    if (tx) *tx = 0;
    if (ty) *ty = 0;

    static int last_up = 1;
    static int last_down = 1;
    static int last_ok = 1;

    const int up = gpio_get_level(static_cast<gpio_num_t>(PIN_BTN_UP));
    const int down = gpio_get_level(static_cast<gpio_num_t>(PIN_BTN_DOWN));
    const int ok = gpio_get_level(static_cast<gpio_num_t>(PIN_BTN_OK));

    Event e = Event::None;
    if (last_up == 1 && up == 0) {
        e = Event::Up;
    } else if (last_down == 1 && down == 0) {
        e = Event::Down;
    } else if (last_ok == 1 && ok == 0) {
        e = Event::Ok;
    }
    last_up = up;
    last_down = down;
    last_ok = ok;

    if (e == Event::None && ui.touch != nullptr) {
        const TouchEvent t = ui.touch->poll();
        if (t.type == TouchEventType::Tap) {
            e = Event::Tap;
            if (tx) *tx = t.x;
            if (ty) *ty = t.y;
        }
    }
    return e;
}

Event wait_event(DiagUi &ui, int *tx, int *ty)
{
    Event e;
    while ((e = poll(ui, tx, ty)) == Event::None) {
        vTaskDelay(pdMS_TO_TICKS(30));
    }
    return e;
}

void flush(DiagUi &ui)
{
    ui.display->blit_ui(ui.fb->data());
    ui.display->refresh_full();
}

void show(DiagUi &ui, const char *title, const char *const *lines, int n)
{
    FrameBuffer &fb = *ui.fb;
    fb.clear(true);
    fb.fill_rect(0, 0, FrameBuffer::kWidth, kTitleBarH, true);
    ui.res->font[layout::kLatin24].draw_centered(fb, 20, title, false);

    int y = kTitleBarH + 36;
    for (int i = 0; i < n && i < kMaxLines; ++i) {
        ui.res->font[layout::kLatin16].draw(fb, layout::kMargin, y, lines[i], true);
        y += 40;
    }
    flush(ui);
}

void wait_exit(DiagUi &ui)
{
    int tx, ty;
    for (;;) {
        const Event e = wait_event(ui, &tx, &ty);
        if (e == Event::Ok || e == Event::Tap) {
            return;
        }
    }
}

void run(StickyDisplay &display, FrameBuffer &fb, const Resources &res, StickyTouch &touch)
{
    g_diag_magic = 0;  // consume the wake flag so a later cold boot is clean

    DiagUi ui;
    ui.display = &display;
    ui.fb = &fb;
    ui.res = &res;
    ui.touch = &touch;

    init_input();
    ESP_LOGI(kTag, "entering diagnostic service mode");

    int sel = 0;
    for (;;) {
        render_menu(ui, sel);
        int tx, ty;
        const Event e = wait_event(ui, &tx, &ty);
        if (e == Event::Up) {
            sel = (sel + kItemCount - 1) % kItemCount;
        } else if (e == Event::Down) {
            sel = (sel + 1) % kItemCount;
        } else if (e == Event::Ok) {
            dispatch(ui, sel);
        } else if (e == Event::Tap) {
            const int idx = (ty - kMenuTop) / kItemH;
            if (idx >= 0 && idx < kItemCount && ty >= kMenuTop) {
                sel = idx;
                dispatch(ui, sel);
            }
        }
    }
}

}  // namespace diag
