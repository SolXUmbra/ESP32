// Backlight: GPIO21 / PWM channel 0 / 5kHz / 8bit, default 220.
// Confirmed from the audited project, NOT guessed (HARDWARE_CONFIG.md §3).
#pragma once

#include <Arduino.h>

namespace sunisle {
namespace backlight {

void begin();
void set(uint8_t value);   // 0..255
uint8_t get();

}  // namespace backlight
}  // namespace sunisle
