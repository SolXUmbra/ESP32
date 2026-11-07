// Adapter: real SUNISLE state -> memo UI setters (B version).
// The A-version JPEG sprite / TJpgD path is entirely gone: no pixel buffer,
// no lazy sprite, no photo decoder is linked into this build.
#include "ui/ui_bridge.h"

#include "ui_sunisle.h"
#include "fonts/sunisle_cn.h"
#include "sunisle/config.h"
#include "sunisle/state.h"
#include "services/memo_service.h"
#include "utils/logger.h"

#include <stdio.h>
#include <string.h>

extern "C" void ui_apply_cn_fonts(const lv_font_t* date_font,
                                  const lv_font_t* weather_font,
                                  const lv_font_t* city_font,
                                  const lv_font_t* status_font,
                                  const lv_font_t* quote_font);

namespace sunisle {
namespace uiBridge {
namespace {

// REAL WMO 4677 code -> the hand-drawn weather icon set.
ui_weather_t condFromWmo(uint8_t c) {
  if (c == 0) return UI_WEATHER_SUNNY;
  if (c == 1 || c == 2) return UI_WEATHER_PARTLY_CLOUDY;
  if (c == 3 || c == 45 || c == 48) return UI_WEATHER_CLOUDY;
  if ((c >= 51 && c <= 67) || (c >= 80 && c <= 82) || c >= 95)
    return UI_WEATHER_RAIN;
  if ((c >= 71 && c <= 77) || c == 85 || c == 86) return UI_WEATHER_SNOW;
  return UI_WEATHER_UNKNOWN;
}

}  // namespace

void begin() {
  ui_init();
  // 12 px date/status, 16 px weather + city. Memo text uses the dedicated
  // sunisle_cn_memo subset directly inside the UI file.
  ui_apply_cn_fonts(&sunisle_cn_12, &sunisle_cn_16, &sunisle_cn_16,
                    &sunisle_cn_quote, &sunisle_cn_quote);
  SUN_LOG("uibridge", "B-version memo UI initialised (240x320)");
}

void tick() {
  const Snapshot s = snapshot();

  char date[32];
  if (s.timeSynced)
    snprintf(date, sizeof(date), "%s  %s", s.dateCn, s.weekdayCn);
  else
    snprintf(date, sizeof(date), "未校时");
  ui_set_date(date);
  ui_set_time(s.timeSynced ? s.timeHHMM : "--:--");

  ui_weather_t cond = UI_WEATHER_UNKNOWN;
  char temp[12];
  const char* desc;
  if (s.weatherValid) {
    cond = condFromWmo(s.wmoCode);
    snprintf(temp, sizeof(temp), "%d°", static_cast<int>(s.tempC));
    desc = s.weatherDesc;
  } else {
    snprintf(temp, sizeof(temp), "--°");
    desc = s.weatherConfigured ? "更新中" : "天气未配置";
  }
  ui_set_weather(cond, temp, desc, DISPLAY_CITY);

  // ---- Memo panel + due alert ----
  char list[400];
  memoService::formatList(list, sizeof(list));
  ui_set_memos(list);

  char count[40];
  snprintf(count, sizeof(count), "%u 条待办 / %u 条到期",
           static_cast<unsigned>(s.memoCount),
           static_cast<unsigned>(s.dueCount));
  ui_set_memo_count(count);

  if (s.activeMemo[0]) ui_show_alert(s.activeMemo);
  else                 ui_clear_alert();

  ui_set_status(s.quote[0] ? s.quote : SLOGAN_CN);
}

}  // namespace uiBridge
}  // namespace sunisle
