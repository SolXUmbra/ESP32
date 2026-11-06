#include "hardware/touch_manager.h"

#include "utils/logger.h"

namespace sunisle {
namespace touch {

void begin() {
  // Intentionally does NOT touch any GPIO and does NOT register an LVGL
  // input device. Display stability wins over an unproven touch controller.
  SUN_LOG("touch", "DISABLED - XPT2046 NOT CONFIRMED, shares LCD HSPI 14/13/12");
  SUN_LOG("touch", "no pinMode / no digitalWrite / no LVGL indev (would blank panel)");
}

bool isReady() { return false; }

const char* reason() { return "HELD OFF (shared HSPI bus)"; }

}  // namespace touch
}  // namespace sunisle
