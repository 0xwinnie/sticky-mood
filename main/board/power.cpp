#include "board/power.h"

#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "pin_config.h"

namespace board {
namespace {

void commit_latch(int hold_level)
{
    gpio_hold_dis(static_cast<gpio_num_t>(PIN_POWER_HOLD));
    gpio_set_level(static_cast<gpio_num_t>(PIN_POWER_HOLD), hold_level);
    gpio_set_level(static_cast<gpio_num_t>(PIN_POWER_LOCK), 0);
    esp_rom_delay_us(10);
    gpio_set_level(static_cast<gpio_num_t>(PIN_POWER_LOCK), 1);
    esp_rom_delay_us(10);
    gpio_set_level(static_cast<gpio_num_t>(PIN_POWER_LOCK), 0);
}

}  // namespace

void power_on_hold()
{
    gpio_config_t config = {};
    config.pin_bit_mask = (1ULL << PIN_POWER_HOLD) | (1ULL << PIN_POWER_LOCK);
    config.mode = GPIO_MODE_OUTPUT;
    config.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&config);

    commit_latch(1);
}

void power_off()
{
    commit_latch(0);
}

}  // namespace board
