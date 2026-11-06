// SUNISLE visual language. Exact palette from the task book section 6.
// No emoji, no icon font, no heavy rounded cards, no shadows.
#pragma once

#include <Arduino.h>
#include <lvgl.h>

namespace sunisle {
namespace ui {

lv_color_t bg();        // #F7F4EC cream white
lv_color_t sand();      // #EEE8DC
lv_color_t ink();       // #353A36
lv_color_t muted();     // #858477
lv_color_t orange();    // #E9A86D
lv_color_t sage();      // #9EAA91
lv_color_t mist();      // #B9CBD0
lv_color_t divider();   // #E5E0D5

const lv_font_t* cn();          // live CJK subset, 16 px
const lv_font_t* brand();       // "晴屿" only, 28 px
const lv_font_t* latinSmall();  // Montserrat 12
const lv_font_t* latinBody();   // Montserrat 14

// Flat, borderless helpers. Alignment is always relative to the parent.
lv_obj_t* label(lv_obj_t* parent, const char* text, const lv_font_t* font,
                lv_color_t color, lv_align_t align, int32_t x, int32_t y);
lv_obj_t* block(lv_obj_t* parent, int32_t w, int32_t h, lv_align_t align,
                int32_t x, int32_t y, lv_color_t color,
                lv_coord_t radius = 0, lv_opa_t opa = LV_OPA_COVER);
void rule(lv_obj_t* parent, int32_t y, int32_t w, lv_color_t color,
          int32_t x = 0);

}  // namespace ui
}  // namespace sunisle
