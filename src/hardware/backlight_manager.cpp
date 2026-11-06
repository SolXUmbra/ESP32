#include "hardware/backlight_manager.h"

#include "sunisle/config.h"
#include "utils/logger.h"

namespace sunisle {
namespace backlight {
namespace {

uint8_t s_value = BL_DEFAULT;

}  // namespace

void begin() {
  ledcSetup(BL_CHANNEL, BL_PWM_FREQ, BL_PWM_RES);
  ledcAttachPin(BL_PIN, BL_CHANNEL);
  ledcWrite(BL_CHANNEL, s_value);
  SUN_LOG("backlight", "GPIO%d PWM %luHz %ubit value=%u", BL_PIN,
          static_cast<unsigned long>(BL_PWM_FREQ), BL_PWM_RES, s_value);
}

void set(uint8_t value) {
  s_value = value;
  ledcWrite(BL_CHANNEL, s_value);
}

uint8_t get() { return s_value; }

}  // namespace backlight
}  // namespace sunisle
