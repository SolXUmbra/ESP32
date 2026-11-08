// QINGYU UI 2.0 tokens. sRGB values; the ST7789 quantises them to RGB565.
// No gradients, shadows, emoji or icon fonts.
#pragma once

#include <Arduino.h>
#include <lvgl.h>

namespace sunisle {
namespace ui {

lv_color_t bg();        // #F4F6FA
lv_color_t surface();   // #FFFFFF
lv_color_t ink();       // #171D2B
lv_color_t muted();    // #697586
lv_color_t brand();     // #5267F6
lv_color_t tech();      // #38B8C8
lv_color_t divider();   // #E6EAF0

lv_obj_t* label(lv_obj_t* parent, const char* text, const lv_font_t* font,
                lv_color_t color, lv_align_t align, int32_t x, int32_t y);
lv_obj_t* block(lv_obj_t* parent, int32_t w, int32_t h, lv_align_t align,
                int32_t x, int32_t y, lv_color_t color,
                lv_coord_t radius = 0, lv_opa_t opa = LV_OPA_COVER);

}  // namespace ui
}  // namespace sunisle
