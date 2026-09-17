#include "app/app_state.h"
#include "app/device_resources.h"
#include "board/power.h"
#include "board/storage.h"
#include "diag/diag.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hardware/sticky_display.h"
#include "hardware/sticky_touch.h"
#include "pages/pages.h"
#include "pin_config.h"
#include "ui/framebuffer.h"
#include "ui/resources.h"

namespace {

constexpr const char *kTag = "sticky_mood";

// Holding the AI key this long starts a voice note; releasing ends it.
constexpr int64_t kAiLongPressMs = 600;
// Partial refreshes accumulate ghosting; clean the panel periodically.
constexpr int kPartialBeforeFullRefresh = 20;

void init_ai_button()
{
    gpio_config_t config = {};
    config.pin_bit_mask = 1ULL << PIN_BTN_OK;
    config.mode = GPIO_MODE_INPUT;
    // Polarity is assumed active-low with a pull-up; confirm on hardware.
    config.pull_up_en = GPIO_PULLUP_ENABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&config);
}

bool ai_button_down()
{
    return gpio_get_level(static_cast<gpio_num_t>(PIN_BTN_OK)) == 0;
}

int64_t now_ms()
{
    return esp_timer_get_time() / 1000;
}

// Applies one state-machine event; reports whether the page changed so the
// caller knows a full refresh is needed.
bool apply_tap(AppState &state, const char *id)
{
    AppState next = state;
    if (!pages::on_tap(next, id)) {
        return false;
    }
    const bool page_changed = next.page != state.page;
    state = next;
    return page_changed;
}

bool tap_region(const AppState &state, int x, int y, const char **id)
{
    HitRegion regions[pages::kMaxRegions];
    const int n = pages::regions(state, regions, pages::kMaxRegions);
    for (int i = 0; i < n; ++i) {
        const HitRegion &r = regions[i];
        if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) {
            *id = r.id;
            return true;
        }
    }
    return false;
}

}  // namespace

extern "C" void app_main()
{
    // Without the latch the rail drops the moment the power button is released.
    board::power_on_hold();
    ESP_LOGI(kTag, "Power rail latched");

    // The e-paper driver registers a BUSY-pin ISR handler; the ISR service must
    // exist first or init logs "GPIO isr service is not installed".
    gpio_install_isr_service(0);

    init_ai_button();

    // Internal flash is the PRIMARY mood-record store (works with no SD card).
    // Mount it early so the journal is ready before any page can save.
    if (!board::storage_init()) {
        ESP_LOGE(kTag, "Internal storage mount failed; recording disabled");
    }

    StickyDisplay display;
    if (!display.init()) {
        ESP_LOGE(kTag, "Display init failed; halting");
        return;
    }

    Resources resources;
    if (!load_embedded_resources(resources)) {
        ESP_LOGE(kTag, "Resource load failed; halting");
        return;
    }

    FrameBuffer fb;
    AppState state;

    StickyTouch touch;
    if (!touch.init()) {
        ESP_LOGE(kTag, "Touch init failed; touch input unavailable");
    }

    // Holding UP at power-on (or waking from a diag deep-sleep test) drops into
    // the on-panel service menu instead of the app. run() only returns on the
    // explicit "Reboot to app" choice, so restart cleanly afterwards.
    if (diag::requested()) {
        diag::run(display, fb, resources, touch);
        esp_restart();
    }

    pages::render(fb, state, resources);
    display.blit_ui(fb.data());
    display.refresh_full();

    int partial_count = 0;
    bool ai_down = false;
    bool ai_holding = false;
    int64_t ai_press_ms = 0;

    while (true) {
        bool dirty = false;
        bool want_full = false;

        const TouchEvent event = touch.poll();
        if (event.type == TouchEventType::Tap) {
            const char *id = nullptr;
            if (tap_region(state, event.x, event.y, &id)) {
                ESP_LOGI(kTag, "tap %s", id);
                want_full = apply_tap(state, id);
                dirty = true;
            }
        }

        const bool down = ai_button_down();
        if (down && !ai_down) {
            ai_press_ms = now_ms();
        } else if (down && !ai_holding && now_ms() - ai_press_ms >= kAiLongPressMs) {
            ai_holding = true;
            if (state.page != Page::Listening) {
                want_full = apply_tap(state, "talk") || want_full;
                dirty = true;
            }
        } else if (!down && ai_down && ai_holding) {
            ai_holding = false;
            if (state.page == Page::Listening) {
                want_full = apply_tap(state, "release") || want_full;
                dirty = true;
            }
        }
        ai_down = down;

        if (dirty) {
            ++partial_count;
            pages::render(fb, state, resources);
            display.blit_ui(fb.data());
            if (want_full || partial_count >= kPartialBeforeFullRefresh) {
                display.refresh_full();
                partial_count = 0;
            } else {
                display.refresh_partial();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(30));
    }
}
