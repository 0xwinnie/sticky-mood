#pragma once

#include "diag/diag.h"

// Individual spike tests. Each renders its own result screen(s), logs to
// ESP_LOGI, and returns to the menu when the user dismisses it. They are written
// to answer the open hardware questions in docs/QUESTIONS-FOR-SEEED.md.
namespace diag {

void spike_sd(DiagUi &);     // S1b: SDSPI mount on shared SPI2 + 1 MB write/read/verify (A1-A6)
void spike_mic(DiagUi &);    // S2:  5 s PDM capture, RMS/peak, optional WAV to SD (B1-B4)
void spike_rtc(DiagUi &);    // S4:  PCF8563 read over I2C1 (C6)
void spike_power(DiagUi &);  // S5:  wake-source report + deep-sleep enter/exit (C3-C5)
void spike_input(DiagUi &);  // S6a: button polarity + long-press measurement (C1-C2)
void spike_touch(DiagUi &);  // S6b: touch calibration grid + live coords (E1)

}  // namespace diag
