#include "hardware/touch_manager.h"

#include "hardware/display_manager.h"
#include "sunisle/config.h"
#include "utils/logger.h"

namespace sunisle {
namespace touch {

void begin() {
  // The XPT2046 now rides the panel's HSPI bus via LovyanGFX (bus_shared=true),
  // so reading it can no longer blank the LCD the way the old software-bitbang
  // probe did. The actual setup lives in display::begin(); here we just surface
  // the result. If init failed, the CS pin guess (TOUCH_CS) is wrong and we
  // fall back to serial-command navigation.
  if (display::touchReady()) {
    SUN_LOG("touch", "ENABLED - XPT2046 on shared HSPI, cs=%d (tap to advance)",
            static_cast<int>(TOUCH_CS));
  } else {
    SUN_LOG("touch",
            "NOT READY - XPT2046 init failed; check TOUCH_CS wiring (TOUCHPROBE)");
  }
}

bool isReady() { return display::touchReady(); }

const char* reason() {
  return display::touchReady() ? "XPT2046 on shared HSPI"
                               : "init failed / unconfirmed CS pin";
}

}  // namespace touch
}  // namespace sunisle
