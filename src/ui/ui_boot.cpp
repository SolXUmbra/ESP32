#include "ui/ui_boot.h"

#include "sunisle/config.h"
#include "ui/ui_theme.h"

namespace sunisle {
namespace ui {
namespace boot {

lv_obj_t* create(lv_obj_t* parent) {
  lv_obj_t* c = lv_obj_create(parent);
  lv_obj_set_size(c, SCREEN_W, SCREEN_H);
  lv_obj_set_pos(c, 0, 0);
  lv_obj_set_style_bg_color(c, bg(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(c, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(c, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(c, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(c, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(c, 0, LV_PART_MAIN);

  label(c, PRODUCT_CN, brand(), ink(), LV_ALIGN_TOP_MID, 0, 96);
  label(c, PRODUCT_EN, latinBody(), muted(), LV_ALIGN_TOP_MID, 0, 142);

  // A single quiet hairline instead of a decorative card.
  rule(c, 178, 44, divider(), (SCREEN_W - 44) / 2);

  label(c, SLOGAN_CN, cn(), muted(), LV_ALIGN_TOP_MID, 0, 198);
  label(c, SLOGAN_EN, latinSmall(), muted(), LV_ALIGN_TOP_MID, 0, 228);

  lv_obj_move_foreground(c);
  // Fade is implemented by a cream veil on top whose bg_opa ramps 255->0.
  // This avoids relying on style-opa inheritance for child objects.
  return c;
}

}  // namespace boot
}  // namespace ui
}  // namespace sunisle
