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

// Touch bring-up: init the shared-bus XPT2046 (cs=33) and read raw ADC coords.
// No LVGL indev is registered (avoids the white-screen regression). intPin=-2
// keeps the default; -1 = polled (no INT gate); 26/36 = gate on that GPIO.
bool   touchInit(int8_t csPin = -2, int8_t intPin = -2);
int8_t touchIntPin();
int8_t touchCsPin();
int    touchRead(uint16_t* x, uint16_t* y);
int    touchRaw(uint16_t* x, uint16_t* y);   // raw ADC, bypasses press filter

}  // namespace display
}  // namespace sunisle
