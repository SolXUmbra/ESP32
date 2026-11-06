// Display manager: the ONLY place that owns the LovyanGFX device and the
// LVGL display driver. All parameters come from sunisle/config.h (FROZEN).
#pragma once

#include <Arduino.h>
#include <lvgl.h>

namespace sunisle {
namespace display {

bool begin();
bool isReady();
void setBrightness(uint8_t v);   // delegates to backlight_manager
uint32_t flushCount();

// Diagnostic ONLY (serial "DIAG"): draws a raw LovyanGFX pattern with LVGL
// bypassed, to tell a hardware/SPI fault from a UI-layer fault.
void drawRawDiagnostic();

// Stage 2 hook: raw RGB565 rectangle push, used by the JPEG path.
void pushRect(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t* px);

}  // namespace display
}  // namespace sunisle
