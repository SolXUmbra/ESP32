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

// Touch (XPT2046 on shared HSPI bus). Always safe to call; reflects whether the
// controller actually initialised (init fails if the CS pin is wrong).
bool touchReady();
bool touchRead(uint16_t* x, uint16_t* y);
bool touchProbe(int8_t cs);      // runtime CS re-probe (TOUCHPROBE command)

// Diagnostic ONLY (serial "DIAG"): draws a raw LovyanGFX pattern with LVGL
// bypassed, to tell a hardware/SPI fault from a UI-layer fault.
void drawRawDiagnostic();

// Raw RGB565 rectangle push (kept for diagnostics / future overlays).
void pushRect(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t* px);

}  // namespace display
}  // namespace sunisle
