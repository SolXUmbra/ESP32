// 晴屿 · 开机画面
//
// 文案沿用上一版，一字未改：PRODUCT_CN / PRODUCT_EN / SLOGAN_CN / SLOGAN_EN。
// 设计只做减法——宣纸留白、楷体题字、一条细线，不添任何图形装饰。
#include "ui/ui_boot.h"

#include "sunisle/config.h"
#include "ui/ui_theme.h"
#include "fonts/sunisle_cn.h"

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

  // 题字「晴屿」：楷体 28，墨色。位置与上一版一致。
  label(c, PRODUCT_CN, brand(), ink(), LV_ALIGN_TOP_MID, 0, 96);

  // 「SUNISLE」：字距略拉开，更斯文。
  lv_obj_t* en = label(c, PRODUCT_EN, latinBody(), muted(),
                       LV_ALIGN_TOP_MID, 0, 142);
  lv_obj_set_style_text_letter_space(en, 4, 0);

  // 唯一装饰：一条极细短线，替代任何卡片或图形。
  rule(c, 178, 44, divider(), (SCREEN_W - 44) / 2);

  // 原有中英标语；中文改用楷体，文字内容不变。
  label(c, SLOGAN_CN, &sunisle_cn_boot, muted(), LV_ALIGN_TOP_MID, 0, 198);
  label(c, SLOGAN_EN, latinSmall(), muted(), LV_ALIGN_TOP_MID, 0, 228);

  lv_obj_move_foreground(c);
  // Fade is implemented by a cream veil on top whose bg_opa ramps 255->0.
  // This avoids relying on style-opa inheritance for child objects.
  return c;
}

}  // namespace boot
}  // namespace ui
}  // namespace sunisle
