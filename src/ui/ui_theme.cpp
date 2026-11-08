#include "ui/ui_theme.h"

#include "fonts/sunisle_cn.h"
#include "sunisle/config.h"

namespace sunisle {
namespace ui {

// UI 2.2 简约中式 - keep in lockstep with qingyu_ui.cpp's token block.
lv_color_t bg()      { return lv_color_hex(0xF5F3ED); }  // 宣纸
lv_color_t surface() { return lv_color_hex(0xFFFFFF); }
lv_color_t ink()     { return lv_color_hex(0x222A27); }  // 墨
lv_color_t muted()   { return lv_color_hex(0x717A74); } // 淡墨
lv_color_t brand()    { return lv_color_hex(0xB23A2E); } // 朱砂
lv_color_t tech()    { return lv_color_hex(0x6E8B7E); }  // 青瓷
lv_color_t divider() { return lv_color_hex(0xE2DFD6); }

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
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  return o;
}

}  // namespace ui
}  // namespace sunisle
