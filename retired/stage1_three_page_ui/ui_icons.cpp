#include "ui/ui_icons.h"

#include "ui/ui_theme.h"

namespace sunisle {
namespace ui {
namespace {

lv_obj_t* ring(lv_obj_t* parent, int32_t x, int32_t y, int32_t size,
               lv_color_t color, lv_opa_t opa, int32_t border) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_set_size(o, size, size);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(o, border, LV_PART_MAIN);
  lv_obj_set_style_border_color(o, color, LV_PART_MAIN);
  lv_obj_set_style_border_opa(o, opa, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(o, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(o, 0, LV_PART_MAIN);
  return o;
}

lv_obj_t* disc(lv_obj_t* parent, int32_t x, int32_t y, int32_t size,
               lv_color_t color, lv_opa_t opa) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_set_size(o, size, size);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_bg_color(o, color, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(o, opa, LV_PART_MAIN);
  lv_obj_set_style_border_width(o, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(o, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(o, 0, LV_PART_MAIN);
  return o;
}

lv_obj_t* bar(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h,
              lv_color_t color, lv_opa_t opa, int32_t radius = 0) {
  return block(parent, w, h, LV_ALIGN_TOP_LEFT, x, y, color, radius, opa);
}

}  // namespace

lv_obj_t* weatherGlyph(lv_obj_t* parent, Glyph g, int32_t x, int32_t y) {
  const lv_color_t c = (g == Glyph::UNSET) ? muted() : orange();
  const lv_opa_t opa = (g == Glyph::UNSET) ? LV_OPA_40 : LV_OPA_COVER;
  lv_obj_t* root = ring(parent, x, y, 20, c, opa, 2);

  switch (g) {
    case Glyph::SUN:
      disc(parent, x + 6, y + 6, 8, orange(), LV_OPA_70);
      break;
    case Glyph::CLOUD:
      bar(parent, x + 4, y + 11, 14, 4, mist(), LV_OPA_COVER, 2);
      break;
    case Glyph::RAIN:
      bar(parent, x + 4, y + 11, 14, 3, mist(), LV_OPA_COVER, 1);
      bar(parent, x + 6, y + 17, 2, 4, mist(), LV_OPA_70, 1);
      bar(parent, x + 11, y + 17, 2, 4, mist(), LV_OPA_70, 1);
      break;
    case Glyph::SNOW:
      bar(parent, x + 4, y + 11, 14, 3, mist(), LV_OPA_COVER, 1);
      disc(parent, x + 6, y + 17, 3, mist(), LV_OPA_80);
      disc(parent, x + 12, y + 18, 3, mist(), LV_OPA_80);
      break;
    case Glyph::FOG:
      bar(parent, x + 3, y + 12, 15, 2, muted(), LV_OPA_50, 1);
      bar(parent, x + 5, y + 16, 12, 2, muted(), LV_OPA_50, 1);
      break;
    case Glyph::UNSET:
    default:
      break;  // quiet ring only - no invented weather condition
  }
  return root;
}

void emptyScene(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h) {
  // A pale sun, a horizon, and a small island. Deliberately abstract so it
  // can never be mistaken for a real photograph.
  const int32_t cx = x + w / 2;
  ring(parent, cx - 27, y + 24, 54, orange(), LV_OPA_30, 2);
  disc(parent, cx - 19, y + 32, 38, orange(), LV_OPA_40);

  const int32_t horizon = y + h - 66;
  bar(parent, x + 20, horizon, w - 40, 1, divider(), LV_OPA_COVER);
  bar(parent, x + 36, horizon + 7, w - 72, 1, divider(), LV_OPA_60);

  // Island: a low, soft sage mound sitting on the horizon.
  bar(parent, cx - 30, horizon - 8, 60, 9, sage(), LV_OPA_50, 4);
  bar(parent, cx - 18, horizon - 13, 36, 6, sage(), LV_OPA_30, 3);
  bar(parent, cx - 1, horizon - 20, 2, 8, sage(), LV_OPA_60, 1);
}

}  // namespace ui
}  // namespace sunisle
