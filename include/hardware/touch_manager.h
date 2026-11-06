// Touch is deliberately DISABLED in SUNISLE stage 1.
//
// HARDWARE FACT (audited project, 16:29 real-board confirmation):
// XPT2046 shares the LCD HSPI pins 14/13/12 (only CS differs). Reconfiguring
// those pins with pinMode()/digitalWrite() detaches them from the ESP32 SPI
// peripheral and BLANKS the panel. No LVGL indev is registered.
// Task book: "触摸不稳定时优先保证自动运行" -> navigation uses serial commands.
#pragma once

#include <Arduino.h>

namespace sunisle {
namespace touch {

void begin();
bool isReady();       // always false in stage 1
const char* reason();

}  // namespace touch
}  // namespace sunisle
