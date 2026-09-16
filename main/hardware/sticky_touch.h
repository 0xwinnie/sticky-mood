#pragma once

#include <cstdint>

#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "gt911.h"

enum class TouchEventType {
    None,
    Tap,
    SwipeUp,
    SwipeDown,
    SwipeLeft,
    SwipeRight,
};

struct TouchEvent {
    TouchEventType type = TouchEventType::None;
    // Logical canvas coordinates (480x800 portrait), matching StickyDisplay.
    uint16_t x = 0;
    uint16_t y = 0;
};

// Polls the GT911 on a dedicated task and posts one gesture per finger lift.
// Driver callbacks must never refresh the panel: a full refresh takes seconds
// and will trip the watchdog.
class StickyTouch {
public:
    bool init();
    // Non-blocking. Returns a None event when nothing is pending.
    TouchEvent poll();

private:
    struct Point {
        uint16_t x = 0;
        uint16_t y = 0;
    };

    static void touch_task_entry(void *context);
    void touch_task();
    TouchEvent read_touch();

    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    GT911 controller_;
    QueueHandle_t event_queue_ = nullptr;
    bool touching_ = false;
    Point start_ = {};
    Point last_ = {};
};
