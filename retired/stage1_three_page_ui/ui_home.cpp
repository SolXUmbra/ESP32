#include "ui/ui_home.h"

#include "sunisle/config.h"
#include "sunisle/state.h"
#include "services/photo_service.h"
#include "services/weather_service.h"
#include "ui/ui_theme.h"
#include "ui/ui_icons.h"

#include <stdio.h>

namespace sunisle {
namespace ui {
namespace home {
namespace {

lv_obj_t* s_dateLbl = nullptr;
lv_obj_t* s_wdayLbl = nullptr;
lv_obj_t* s_modeLbl = nullptr;
lv_obj_t* s_tempLbl = nullptr;
lv_obj_t* s_heroTxt = nullptr;
lv_obj_t* s_timeLbl = nullptr;
lv_obj_t* s_infoLbl = nullptr;
lv_obj_t* s_hero = nullptr;

}  // namespace

void create(lv_obj_t* parent) {
  // ---------- top zone (0..44) : date / weekday / mode / weather ----------
  s_dateLbl = label(parent, "----.--.--", latinSmall(), muted(),
                    LV_ALIGN_TOP_LEFT, 14, 10);
  s_wdayLbl = label(parent, "---", latinSmall(), orange(),
                    LV_ALIGN_TOP_LEFT, 92, 10);
  s_modeLbl = label(parent, "陪伴模式", cn(), muted(),
                    LV_ALIGN_TOP_LEFT, 14, 26);
  // No weather data in stage 1 -> a quiet UNSET ring, never a fake condition.
  weatherGlyph(parent, Glyph::UNSET, 188, 8);
  s_tempLbl = label(parent, "--", latinBody(), muted(),
                    LV_ALIGN_TOP_LEFT, 212, 11);
  rule(parent, 44, 216, divider(), 12);

  // ---------- hero zone (48..230) : the photograph is the subject ----------
  s_hero = block(parent, 216, 182, LV_ALIGN_TOP_MID, 0, 48, sand(), 6);
  emptyScene(s_hero, 0, 0, 216, 182);
  s_heroTxt = label(s_hero, "暂无照片", cn(), muted(),
                    LV_ALIGN_TOP_MID, 0, 134);
  label(s_hero, SLOGAN_EN, latinSmall(), muted(),
        LV_ALIGN_TOP_MID, 0, 156);

  // ---------- bottom zone (236..320) : time / city / weather ----------
  rule(parent, 236, 216, divider(), 12);
  s_timeLbl = label(parent, "--:--", latinTime(), ink(),
                    LV_ALIGN_TOP_LEFT, 12, 242);
  s_infoLbl = label(parent, "未配置 / 天气未配置", cn(), muted(),
                    LV_ALIGN_TOP_LEFT, 14, 298);
}

void refresh() {
  const Snapshot s = snapshot();
  lv_label_set_text(s_dateLbl, s.dateLine);
  lv_label_set_text(s_wdayLbl, s.weekday);
  lv_label_set_text(s_modeLbl, s.wifiOn ? "同步模式" : "陪伴模式");
  lv_label_set_text(s_tempLbl, s.weatherValid ? s.weatherLine : "--");
  lv_label_set_text(s_timeLbl, s.timeSynced ? s.timeHHMM : "--:--");

  char info[64];
  snprintf(info, sizeof(info), "%s / %s", s.city,
           s.weatherValid ? s.weatherLine : "天气未配置");
  lv_label_set_text(s_infoLbl, info);

  // Stage 1 has no decoder, so the hero always shows an explicit empty state
  // or the real file count - never a fake photograph.
  char hero[32];
  if (photoService::count() > 0)
    snprintf(hero, sizeof(hero), "共 %u 张", photoService::count());
  else
    snprintf(hero, sizeof(hero), "暂无照片");
  lv_label_set_text(s_heroTxt, hero);
}

}  // namespace home
}  // namespace ui
}  // namespace sunisle
