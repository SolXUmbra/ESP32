#include "ui/ui_theme.h"

#include "fonts/sunisle_cn.h"
#include "sunisle/config.h"

namespace sunisle {
namespace ui {

lv_color_t bg()      { return lv_color_hex(0xF7F4EC); }
lv_color_t sand()    { return lv_color_hex(0xEEE8DC); }
lv_color_t ink()     { return lv_color_hex(0x353A36); }
lv_color_t muted()   { return lv_color_hex(0x858477); }
lv_color_t orange()  { return lv_color_hex(0xE9A86D); }
lv_color_t sage()    { return lv_color_hex(0x9EAA91); }
lv_color_t mist()    { return lv_color_hex(0xB9CBD0); }
lv_color_t divider() { return lv_color_hex(0xE5E0D5); }

const lv_font_t* cn()          { return &sunisle_cn_16; }
const lv_font_t* brand()       { return &sunisle_cn_28; }
const lv_font_t* latinSmall()  { return &lv_font_montserrat_12; }
const lv_font_t* latinBody()   { return &lv_font_montserrat_14; }

lv_obj_t* label(lv_obj_t* parent, const char* text, const lv_font_t* font,
                lv_color_t color, lv_align_t align, int32_t x, int32_t y) {
  lv_obj_t* l = lv_label_create(parent);
  lv_label_set_text(l, text);
  lv_obj_set_style_text_font(l, font, LV_PART_MAIN);
  lv_obj_set_style_text_color(l, color, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(l, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(l, 0, LV_PART_MAIN);
  lv_obj_align(l, align, x, y);
  return l;
}

lv_obj_t* block(lv_obj_t* parent, int32_t w, int32_t h, lv_align_t align,
                int32_t x, int32_t y, lv_color_t color, lv_coord_t radius,
                lv_opa_t opa) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_set_size(o, w, h);
  lv_obj_align(o, align, x, y);
  lv_obj_set_style_bg_color(o, color, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(o, opa, LV_PART_MAIN);
  lv_obj_set_style_radius(o, radius, LV_PART_MAIN);
  lv_obj_set_style_border_width(o, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(o, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(o, 0, LV_PART_MAIN);
  return o;
}

void rule(lv_obj_t* parent, int32_t y, int32_t w, lv_color_t color,
          int32_t x) {
  (void)block(parent, w, 1, LV_ALIGN_TOP_LEFT, x, y, color, 0);
}

}  // namespace ui
}  // namespace sunisle
