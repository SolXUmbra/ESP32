// Weather glyphs and the empty-state illustration, drawn with native LVGL
// primitives (circles / thin bars). NO emoji, NO icon font, NO bitmap assets.
// Unified style: 2px outlines, rounded, low saturation, same palette as UI.
#pragma once

#include <Arduino.h>
#include <lvgl.h>

namespace sunisle {
namespace ui {

enum class Glyph : uint8_t { UNSET = 0, SUN, CLOUD, RAIN, SNOW, FOG };

// Top-bar icon, 20x20. UNSET renders a quiet muted ring (no fake condition).
lv_obj_t* weatherGlyph(lv_obj_t* parent, Glyph g, int32_t x, int32_t y);

// Hero empty-state illustration: a soft sun over a quiet horizon.
// Explicitly an EMPTY STATE, never presented as a photograph.
void emptyScene(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h);

}  // namespace ui
}  // namespace sunisle
