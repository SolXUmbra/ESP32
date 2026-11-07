// Touch: XPT2046 on the LCD's shared HSPI bus, driven through LovyanGFX's
// bus_shared transaction arbitration (set up in display_manager.cpp). This is
// the bus-safe path the audit called for and no longer blanks the panel.
//
// HARDWARE FACT: the touch shares LCD HSPI pins 14/13/12 (only CS differs).
// The CS pin is a best-guess (TOUCH_CS=33, see HARDWARE_CONFIG.md) and is
// confirmed at runtime via the TOUCH / TOUCHPROBE serial commands.
#pragma once

#include <Arduino.h>

namespace sunisle {
namespace touch {

void begin();
bool isReady();       // true once LovyanGFX initialised the controller
const char* reason();

}  // namespace touch
}  // namespace sunisle
