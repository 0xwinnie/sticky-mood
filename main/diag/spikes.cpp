#include "diag/spikes.h"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "diag/diag.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2s_pdm.h"
#include "driver/rtc_io.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hardware/sticky_display.h"
#include "hardware/sticky_touch.h"
#include "pin_config.h"
#include "sdmmc_cmd.h"
#include "ui/framebuffer.h"
#include "ui/layout.h"
#include "ui/resources.h"

namespace diag {
namespace {

constexpr const char *kTag = "diag";

// Grows result text one line at a time and presents it on the panel.
struct Lines {
    char b[kMaxLines][kLineLen];
    int n = 0;

    void add(const char *fmt, ...)
    {
        if (n >= kMaxLines) {
            return;
        }
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(b[n], kLineLen, fmt, ap);
        va_end(ap);
        ++n;
    }
    void present(DiagUi &ui, const char *title)
    {
        const char *p[kMaxLines];
        for (int i = 0; i < n; ++i) {
            p[i] = b[i];
        }
        diag::show(ui, title, p, n);
    }
};

int64_t now_ms()
{
    return esp_timer_get_time() / 1000;
}

uint8_t *psram_alloc(size_t bytes)
{
    uint8_t *p = static_cast<uint8_t *>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (p == nullptr) {
        p = static_cast<uint8_t *>(heap_caps_malloc(bytes, MALLOC_CAP_8BIT));
    }
    return p;
}

// --- shared SD mount (SPI2 is already up from StickyDisplay::init) ---
sdmmc_card_t *sd_mount(char *err, size_t errlen)
{
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;
    host.max_freq_khz = 10000;  // assumption A3: same 10 MHz as the panel

    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.gpio_cs = static_cast<gpio_num_t>(PIN_SD_CS);
    slot.host_id = SPI2_HOST;

    esp_vfs_fat_mount_config_t cfg = {};
    cfg.format_if_mount_failed = false;
    cfg.max_files = 5;
    cfg.allocation_unit_size = 16 * 1024;

    sdmmc_card_t *card = nullptr;
    const esp_err_t ret = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot, &cfg, &card);
    if (ret != ESP_OK) {
        snprintf(err, errlen, "%s", esp_err_to_name(ret));
        return nullptr;
    }
    return card;
}

void sd_unmount(sdmmc_card_t *card)
{
    if (card != nullptr) {
        esp_vfs_fat_sdcard_unmount("/sdcard", card);
    }
}

int bcd(int v)
{
    return ((v >> 4) & 0x0F) * 10 + (v & 0x0F);
}

}  // namespace

// ---------------------------------------------------------------------------
// S1b: microSD on the shared SPI2 bus (QUESTIONS A1-A6)
// ---------------------------------------------------------------------------
void spike_sd(DiagUi &ui)
{
    Lines busy;
    busy.add("Mounting /sdcard ...");
    busy.add("If this hangs, suspect A2/A6");
    busy.present(ui, "SD CARD");

    char err[48];
    sdmmc_card_t *card = sd_mount(err, sizeof err);

    Lines out;
    if (card == nullptr) {
        ESP_LOGE(kTag, "SD mount failed: %s", err);
        out.add("Mount FAILED: %s", err);
        out.add("Likely A2 (no power EN),");
        out.add("A3 (clock) or no card.");
        out.add("Retrying at 400 kHz may help.");
        out.present(ui, "SD CARD");
        wait_exit(ui);
        return;
    }

    uint64_t total = 0, fre = 0;
    esp_vfs_fat_info("/sdcard", &total, &fre);
    out.add("Mount OK @10 MHz");
    out.add("Total %llu MB  Free %llu MB",
            (unsigned long long)(total / (1024 * 1024)),
            (unsigned long long)(fre / (1024 * 1024)));
    ESP_LOGI(kTag, "SD mounted: total=%llu free=%llu", (unsigned long long)total, (unsigned long long)fre);

    constexpr size_t kChunk = 32 * 1024;
    constexpr size_t kTotal = 1024 * 1024;  // 1 MB
    uint8_t *buf = psram_alloc(kChunk);
    if (buf == nullptr) {
        out.add("Buffer alloc FAILED");
        out.present(ui, "SD CARD");
        sd_unmount(card);
        wait_exit(ui);
        return;
    }

    bool verify_ok = true;
    size_t fail_off = 0;

    FILE *f = fopen("/sdcard/diag.bin", "wb");
    int write_ms = 0, read_ms = 0;
    if (f == nullptr) {
        out.add("Open for write FAILED");
    } else {
        const int64_t t0 = now_ms();
        size_t off = 0;
        while (off < kTotal) {
            for (size_t i = 0; i < kChunk; ++i) {
                buf[i] = static_cast<uint8_t>((off + i) * 31 + ((off + i) >> 8));
            }
            if (fwrite(buf, 1, kChunk, f) != kChunk) {
                out.add("Write FAILED @%u", (unsigned)off);
                verify_ok = false;
                break;
            }
            off += kChunk;
        }
        fclose(f);
        write_ms = (int)(now_ms() - t0);

        f = fopen("/sdcard/diag.bin", "rb");
        if (f == nullptr) {
            out.add("Reopen for read FAILED");
            verify_ok = false;
        } else {
            const int64_t t1 = now_ms();
            size_t off2 = 0;
            while (off2 < kTotal) {
                if (fread(buf, 1, kChunk, f) != kChunk) {
                    out.add("Read FAILED @%u", (unsigned)off2);
                    verify_ok = false;
                    break;
                }
                for (size_t i = 0; i < kChunk; ++i) {
                    const uint8_t expect = static_cast<uint8_t>((off2 + i) * 31 + ((off2 + i) >> 8));
                    if (buf[i] != expect) {
                        verify_ok = false;
                        fail_off = off2 + i;
                        break;
                    }
                }
                if (!verify_ok) {
                    break;
                }
                off2 += kChunk;
            }
            fclose(f);
            read_ms = (int)(now_ms() - t1);
        }
    }

    heap_caps_free(buf);
    remove("/sdcard/diag.bin");

    if (write_ms > 0) {
        out.add("Write 1MB: %d ms (%d KB/s)", write_ms, (int)(kTotal / 1024 * 1000 / write_ms));
    }
    if (read_ms > 0) {
        out.add("Read  1MB: %d ms (%d KB/s)", read_ms, (int)(kTotal / 1024 * 1000 / read_ms));
    }
    if (verify_ok) {
        out.add("Verify: OK");
    } else {
        out.add("Verify: FAIL @%u", (unsigned)fail_off);
    }
    out.add("Panel+SD concurrency: run a");
    out.add("refresh during next test (A5).");

    ESP_LOGI(kTag, "SD result: write=%dms read=%dms verify=%d", write_ms, read_ms, verify_ok ? 1 : 0);
    sd_unmount(card);
    out.present(ui, "SD CARD");
    wait_exit(ui);
}

// ---------------------------------------------------------------------------
// S2: PDM microphone capture (QUESTIONS B1-B4)
// ---------------------------------------------------------------------------
void spike_mic(DiagUi &ui)
{
    Lines busy;
    busy.add("Capturing 5 s @16 kHz mono");
    busy.add("SPEAK NOW ...");
    busy.present(ui, "MICROPHONE");

    constexpr uint32_t kRate = 16000;
    constexpr size_t kBytes = kRate * 2 * 5;  // 5 s of 16-bit mono = 160000
    int16_t *pcm = reinterpret_cast<int16_t *>(psram_alloc(kBytes));

    Lines out;
    if (pcm == nullptr) {
        out.add("Capture buffer alloc FAILED");
        out.present(ui, "MICROPHONE");
        wait_exit(ui);
        return;
    }

    i2s_chan_handle_t rx = nullptr;
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    esp_err_t ret = i2s_new_channel(&chan_cfg, nullptr, &rx);
    if (ret == ESP_OK) {
        i2s_pdm_rx_config_t pdm = {};
        pdm.clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(kRate);
        pdm.slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
        pdm.gpio_cfg.clk = static_cast<gpio_num_t>(PIN_MIC_CLK);
        pdm.gpio_cfg.din = static_cast<gpio_num_t>(PIN_MIC_DATA);
        pdm.gpio_cfg.invert_flags.clk_inv = false;
        ret = i2s_channel_init_pdm_rx_mode(rx, &pdm);
    }

    if (ret != ESP_OK) {
        ESP_LOGE(kTag, "PDM RX init failed: %s", esp_err_to_name(ret));
        out.add("I2S PDM init FAILED: %s", esp_err_to_name(ret));
        out.add("Check B2 (slot/clk config).");
        if (rx) {
            i2s_del_channel(rx);
        }
        heap_caps_free(pcm);
        out.present(ui, "MICROPHONE");
        wait_exit(ui);
        return;
    }

    i2s_channel_enable(rx);
    size_t got = 0;
    const int64_t t0 = now_ms();
    while (got < kBytes) {
        size_t want = kBytes - got;
        if (want > 4096) {
            want = 4096;
        }
        size_t br = 0;
        if (i2s_channel_read(rx, reinterpret_cast<char *>(pcm) + got, want, &br, 1000) != ESP_OK) {
            break;
        }
        got += br;
        if (br == 0 && now_ms() - t0 > 7000) {
            break;  // no data flowing
        }
    }
    i2s_channel_disable(rx);
    i2s_del_channel(rx);
    const int elapsed = (int)(now_ms() - t0);

    const size_t samples = got / 2;
    int64_t sumsq = 0;
    int32_t peak = 0;
    for (size_t i = 0; i < samples; ++i) {
        const int32_t s = pcm[i];
        sumsq += (int64_t)s * s;
        const int32_t a = s < 0 ? -s : s;
        if (a > peak) {
            peak = a;
        }
    }
    int rms = 0;
    if (samples > 0) {
        const int64_t mean = sumsq / (int64_t)samples;
        // Integer Newton sqrt of the mean square.
        int64_t r = mean > 0 ? mean : 1;
        for (int i = 0; i < 24; ++i) {
            r = (r + mean / r) / 2;
        }
        rms = (int)r;
    }

    out.add("Captured %u samples (%d ms)", (unsigned)samples, elapsed);
    out.add("RMS %d   Peak %d", rms, (int)peak);
    out.add(rms > 300 ? "Mic looks ALIVE (>300)" : "Quiet/dead: check B1/B3");

    // Save a WAV so the recording can be fed to Qwen ASR (V4).
    char err[48];
    sdmmc_card_t *card = sd_mount(err, sizeof err);
    if (card != nullptr && samples > 0) {
        FILE *f = fopen("/sdcard/diag_mic.wav", "wb");
        if (f != nullptr) {
            const uint32_t data_bytes = (uint32_t)(samples * 2);
            uint8_t hdr[44];
            memcpy(hdr, "RIFF", 4);
            uint32_t riff = data_bytes + 36;
            memcpy(hdr + 4, &riff, 4);
            memcpy(hdr + 8, "WAVEfmt ", 8);
            uint32_t fmtlen = 16;
            memcpy(hdr + 16, &fmtlen, 4);
            uint16_t audio = 1, ch = 1, bits = 16;
            memcpy(hdr + 20, &audio, 2);
            memcpy(hdr + 22, &ch, 2);
            uint32_t rate = kRate;
            memcpy(hdr + 24, &rate, 4);
            uint32_t byterate = kRate * 2;
            memcpy(hdr + 28, &byterate, 4);
            uint16_t align = 2;
            memcpy(hdr + 32, &align, 2);
            memcpy(hdr + 34, &bits, 2);
            memcpy(hdr + 36, "data", 4);
            memcpy(hdr + 40, &data_bytes, 4);
            fwrite(hdr, 1, 44, f);
            fwrite(pcm, 1, data_bytes, f);
            fclose(f);
            out.add("WAV -> /sdcard/diag_mic.wav");
        } else {
            out.add("WAV open FAILED");
        }
        sd_unmount(card);
    } else if (card == nullptr) {
        out.add("WAV skipped: SD %s", err);
    }

    ESP_LOGI(kTag, "mic result: samples=%u rms=%d peak=%d elapsed=%dms", (unsigned)samples, rms, (int)peak, elapsed);
    heap_caps_free(pcm);
    out.present(ui, "MICROPHONE");
    wait_exit(ui);
}

// ---------------------------------------------------------------------------
// S4: PCF8563 RTC on the sensor I2C1 bus (QUESTION C6)
// ---------------------------------------------------------------------------
void spike_rtc(DiagUi &ui)
{
    Lines out;
    i2c_master_bus_handle_t bus = nullptr;
    i2c_master_bus_config_t bc = {};
    bc.i2c_port = I2C_NUM_1;
    bc.sda_io_num = static_cast<gpio_num_t>(PIN_SENSOR_SDA);
    bc.scl_io_num = static_cast<gpio_num_t>(PIN_SENSOR_SCL);
    bc.clk_source = I2C_CLK_SRC_DEFAULT;
    bc.glitch_ignore_cnt = 7;
    bc.flags.enable_internal_pullup = 1;

    esp_err_t ret = i2c_new_master_bus(&bc, &bus);
    if (ret != ESP_OK) {
        out.add("I2C1 bus FAILED: %s", esp_err_to_name(ret));
        out.present(ui, "RTC");
        wait_exit(ui);
        return;
    }

    i2c_device_config_t dc = {};
    dc.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dc.device_address = 0x51;  // PCF8563
    dc.scl_speed_hz = 100000;
    i2c_master_dev_handle_t dev = nullptr;
    ret = i2c_master_bus_add_device(bus, &dc, &dev);
    if (ret != ESP_OK) {
        out.add("Add PCF8563 FAILED: %s", esp_err_to_name(ret));
        i2c_del_master_bus(bus);
        out.present(ui, "RTC");
        wait_exit(ui);
        return;
    }

    uint8_t reg = 0x02;  // VL_seconds .. years (7 bytes)
    uint8_t t[7] = {0};
    ret = i2c_master_transmit_receive(dev, &reg, 1, t, sizeof t, pdMS_TO_TICKS(200));
    if (ret != ESP_OK) {
        out.add("Read FAILED: %s", esp_err_to_name(ret));
        out.add("No ACK -> wrong addr/wiring.");
    } else {
        const int sec = bcd(t[0] & 0x7F);
        const int min = bcd(t[1] & 0x7F);
        const int hour = bcd(t[2] & 0x3F);
        const int day = bcd(t[3] & 0x3F);
        const int mon = bcd(t[5] & 0x1F);
        const int year = bcd(t[6]);
        const bool vl = (t[0] & 0x80) != 0;
        out.add("PCF8563 @0x51 on I2C1");
        out.add("20%02d-%02d-%02d  %02d:%02d:%02d", year, mon, day, hour, min, sec);
        out.add(vl ? "VL=1 clock was LOST" : "VL=0 clock valid");
        out.add(vl ? "-> no backup cell (C6)" : "-> backup cell present");
        out.add("Power-cycle & re-run to");
        out.add("measure drift / VL change.");
        ESP_LOGI(kTag, "RTC 20%02d-%02d-%02d %02d:%02d:%02d VL=%d", year, mon, day, hour, min, sec, vl ? 1 : 0);
    }

    i2c_master_bus_rm_device(dev);
    i2c_del_master_bus(bus);
    out.present(ui, "RTC");
    wait_exit(ui);
}

// ---------------------------------------------------------------------------
// S5: power / wake sources / deep sleep (QUESTIONS C3-C5)
// ---------------------------------------------------------------------------
void spike_power(DiagUi &ui)
{
    gpio_config_t in = {};
    in.pin_bit_mask = (1ULL << PIN_EXTERNAL_POWER) | (1ULL << PIN_CHARGE_STATE);
    in.mode = GPIO_MODE_INPUT;
    in.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&in);

    const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    const char *cause_s = "cold boot / reset";
    switch (cause) {
    case ESP_SLEEP_WAKEUP_EXT1: cause_s = "EXT1 (AI key)"; break;
    case ESP_SLEEP_WAKEUP_EXT0: cause_s = "EXT0"; break;
    case ESP_SLEEP_WAKEUP_TIMER: cause_s = "timer"; break;
    case ESP_SLEEP_WAKEUP_GPIO: cause_s = "GPIO"; break;
    default: break;
    }

    Lines out;
    out.add("Wake cause: %s", cause_s);
    out.add("Ext power (GPIO9): %d", gpio_get_level(static_cast<gpio_num_t>(PIN_EXTERNAL_POWER)));
    out.add("Charge state (GPIO40): %d", gpio_get_level(static_cast<gpio_num_t>(PIN_CHARGE_STATE)));
    out.add("OK / Tap = enter deep sleep");
    out.add("UP / DOWN = back to menu");
    out.add("After sleep, AI key wakes and");
    out.add("returns to this menu.");
    out.present(ui, "POWER");
    ESP_LOGI(kTag, "power: cause=%s ext=%d chg=%d", cause_s,
            gpio_get_level(static_cast<gpio_num_t>(PIN_EXTERNAL_POWER)),
            gpio_get_level(static_cast<gpio_num_t>(PIN_CHARGE_STATE)));

    int tx, ty;
    for (;;) {
        const Event e = wait_event(ui, &tx, &ty);
        if (e == Event::Up || e == Event::Down) {
            return;
        }
        if (e == Event::Ok || e == Event::Tap) {
            break;
        }
    }

    Lines s;
    s.add("Entering DEEP SLEEP ...");
    s.add("Press AI key (GPIO4) to wake.");
    s.add("Panel keeps this image (C4).");
    s.present(ui, "POWER");

    ui.display->panel_sleep();
    arm_return_after_sleep();

    rtc_gpio_init(static_cast<gpio_num_t>(PIN_POWER_BTN));
    rtc_gpio_set_direction(static_cast<gpio_num_t>(PIN_POWER_BTN), RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_en(static_cast<gpio_num_t>(PIN_POWER_BTN));
    rtc_gpio_pulldown_dis(static_cast<gpio_num_t>(PIN_POWER_BTN));
    esp_sleep_enable_ext1_wakeup(1ULL << PIN_POWER_BTN, ESP_EXT1_WAKEUP_ANY_LOW);

    ESP_LOGI(kTag, "deep sleep now");
    esp_deep_sleep_start();
}

// ---------------------------------------------------------------------------
// S6a: button polarity + long-press measurement (QUESTIONS C1-C2)
// ---------------------------------------------------------------------------
void spike_input(DiagUi &ui)
{
    const gpio_num_t pins[3] = {
        static_cast<gpio_num_t>(PIN_BTN_OK),
        static_cast<gpio_num_t>(PIN_BTN_UP),
        static_cast<gpio_num_t>(PIN_BTN_DOWN),
    };
    const char *names[3] = {"AI/OK", "UP", "DOWN"};

    Lines intro;
    intro.add("Idle levels: AI=%d UP=%d DN=%d",
              gpio_get_level(pins[0]), gpio_get_level(pins[1]), gpio_get_level(pins[2]));
    intro.add("Expect idle=1 (active low).");
    intro.add("Press & hold each button;");
    intro.add("duration shows on release.");
    intro.add("Tap the screen to exit.");
    intro.present(ui, "BUTTONS");

    bool down[3] = {false, false, false};
    int64_t t0[3] = {0, 0, 0};
    char hist_name[4][12];
    int hist_dur[4];
    int hist_n = 0;

    for (;;) {
        for (int b = 0; b < 3; ++b) {
            const int lvl = gpio_get_level(pins[b]);
            if (lvl == 0 && !down[b]) {
                down[b] = true;
                t0[b] = now_ms();
            } else if (lvl != 0 && down[b]) {
                down[b] = false;
                const int dur = (int)(now_ms() - t0[b]);
                ESP_LOGI(kTag, "button %s press %d ms", names[b], dur);
                snprintf(hist_name[hist_n % 4], sizeof hist_name[0], "%s", names[b]);
                hist_dur[hist_n % 4] = dur;
                ++hist_n;

                Lines out;
                out.add("Idle: AI=%d UP=%d DN=%d",
                        gpio_get_level(pins[0]), gpio_get_level(pins[1]), gpio_get_level(pins[2]));
                const int shown = hist_n < 4 ? hist_n : 4;
                for (int i = 0; i < shown; ++i) {
                    const int idx = (hist_n - shown + i) % 4;
                    out.add("%-6s %d ms%s", hist_name[idx], hist_dur[idx],
                            hist_dur[idx] >= 600 ? "  (long)" : "");
                }
                out.add("Tap screen to exit.");
                out.present(ui, "BUTTONS");
            }
        }

        if (ui.touch != nullptr) {
            const TouchEvent t = ui.touch->poll();
            if (t.type == TouchEventType::Tap) {
                break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ---------------------------------------------------------------------------
// S6b: touch calibration grid + live coordinates (QUESTION E1)
// ---------------------------------------------------------------------------
void spike_touch(DiagUi &ui)
{
    FrameBuffer &fb = *ui.fb;

    auto draw_grid = [&](int last_x, int last_y, bool have) {
        fb.clear(true);
        fb.draw_rect(8, 8, FrameBuffer::kWidth - 16, FrameBuffer::kHeight - 16, true);
        // corner targets
        const int c = 36;
        fb.draw_rect(24, 24, c, c, true);
        fb.draw_rect(FrameBuffer::kWidth - 24 - c, 24, c, c, true);
        fb.draw_rect(24, FrameBuffer::kHeight - 24 - c, c, c, true);
        fb.draw_rect(FrameBuffer::kWidth - 24 - c, FrameBuffer::kHeight - 24 - c, c, c, true);
        // center cross
        const int cx = FrameBuffer::kWidth / 2, cy = FrameBuffer::kHeight / 2;
        fb.hline(cx - 30, cy, 60, true);
        fb.vline(cx, cy - 30, 60, true);

        ui.res->font[layout::kLatin20].draw_centered(fb, 40, "TOUCH CALIBRATION", true);
        char buf[kLineLen];
        if (have) {
            snprintf(buf, sizeof buf, "last tap: x=%d y=%d", last_x, last_y);
            fb.fill_rect(last_x - 6, last_y - 6, 13, 13, true);  // marker at the tap point
        } else {
            snprintf(buf, sizeof buf, "tap the corners + center");
        }
        ui.res->font[layout::kLatin18].draw_centered(fb, FrameBuffer::kHeight - 90, buf, true);
        ui.res->font[layout::kLatin16].draw_centered(fb, FrameBuffer::kHeight - 60,
                                                     "press any button to exit", true);
    };

    draw_grid(0, 0, false);
    flush(ui);

    int last_x = 0, last_y = 0;
    for (;;) {
        // Exit on any physical button.
        if (gpio_get_level(static_cast<gpio_num_t>(PIN_BTN_UP)) == 0 ||
            gpio_get_level(static_cast<gpio_num_t>(PIN_BTN_DOWN)) == 0 ||
            gpio_get_level(static_cast<gpio_num_t>(PIN_BTN_OK)) == 0) {
            break;
        }
        if (ui.touch != nullptr) {
            const TouchEvent t = ui.touch->poll();
            if (t.type == TouchEventType::Tap) {
                last_x = t.x;
                last_y = t.y;
                ESP_LOGI(kTag, "touch tap logical (%d, %d)", last_x, last_y);
                draw_grid(last_x, last_y, true);
                // Partial refresh keeps the grid; only the marker/text change.
                ui.display->blit_ui(fb.data());
                ui.display->refresh_partial();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

}  // namespace diag
