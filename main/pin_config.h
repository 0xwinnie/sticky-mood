#pragma once

// reTerminal Sticky GPIO map.
// Verified against the Seeed hardware overview and the pin_config.h of a
// shipping app in Seeed-Projects/reterminal-sticky-playground-registry.
// See docs/HARDWARE.md for the bus topology and the unverified pins.

// --- Power ---
#define PIN_POWER_BTN       4   // AI/Power button; also deep-sleep EXT1 wake source (low)
#define PIN_POWER_HOLD      45  // Must be driven high to latch the main rail on
#define PIN_POWER_LOCK      46  // Pulsed low->high->low to commit a hold/release

// Battery charger (BQ25616)
#define PIN_BAT_CHG_EN      39  // EN_BAT_CHGn, active low
#define PIN_CHARGE_STATE    40  // Charge status input
#define PIN_EXTERNAL_POWER  9   // High when USB/external power is present

// --- I2C1: sensor bus (SHT40, LSM6DS3TR-C, PCF8563, BQ27220) ---
#define PIN_SENSOR_SCL      0
#define PIN_SENSOR_SDA      1
#define PIN_SENSOR_INT      7   // Ambiguous: documented as IMU int, used as fuel-gauge int

// Battery fuel gauge (BQ27220)
#define BQ27220_I2C_ADDR    0x55

// --- I2C0: touch panel (GT911) ---
#define PIN_TOUCH_SCL       2
#define PIN_TOUCH_SDA       3
#define PIN_TOUCH_INT       21
#define PIN_TOUCH_RST       41
#define PIN_TOUCH_EN        42

// --- SPI2: ePaper (SSD1677) and microSD, shared bus, distinct chip selects ---
#define PIN_EPD_MOSI        14
#define PIN_EPD_CLK         13
#define PIN_EPD_MISO        12
#define PIN_EPD_CS          15
#define PIN_EPD_DC          16
#define PIN_EPD_RST         17
#define PIN_EPD_BUSY        18  // Active level 1
#define PIN_EPD_EN          47  // Panel power enable
#define PIN_SD_CS           8

// --- Buttons ---
#define PIN_BTN_UP          5   // Previous page
#define PIN_BTN_DOWN        6   // Next page
#define PIN_BTN_OK          PIN_POWER_BTN

// --- Audio ---
#define PIN_MIC_CLK         19  // PDM microphone
#define PIN_MIC_DATA        20
#define PIN_BUZZER          48  // LEDC PWM

// --- USB serial ---
#define PIN_USB_TX          43
#define PIN_USB_RX          44
