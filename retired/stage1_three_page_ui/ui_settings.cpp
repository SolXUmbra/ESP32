#include "ui/ui_settings.h"

#include "sunisle/config.h"
#include "sunisle/state.h"
#include "hardware/storage_manager.h"
#include "hardware/touch_manager.h"
#include "ui/ui_theme.h"

#include <stdio.h>

namespace sunisle {
namespace ui {
namespace settings {
namespace {

lv_obj_t* s_bright = nullptr;
lv_obj_t* s_interv = nullptr;
lv_obj_t* s_city   = nullptr;
lv_obj_t* s_weath  = nullptr;
lv_obj_t* s_sync   = nullptr;
lv_obj_t* s_touch  = nullptr;
lv_obj_t* s_sd     = nullptr;

void row(lv_obj_t* parent, int32_t y, const char* name, lv_obj_t** value) {
  label(parent, name, cn(), ink(), LV_ALIGN_TOP_LEFT, 14, y);
  *value = label(parent, "--", cn(), muted(), LV_ALIGN_TOP_LEFT, 132, y);
  rule(parent, y + 26, 216, divider(), 12);
}

}  // namespace

void create(lv_obj_t* parent) {
  label(parent, "设置", cn(), ink(), LV_ALIGN_TOP_LEFT, 14, 10);
  label(parent, "SETTINGS", latinSmall(), muted(),
        LV_ALIGN_TOP_LEFT, 14, 30);
  rule(parent, 46, 216, divider(), 12);

  row(parent, 54,  "亮度",     &s_bright);
  row(parent, 82,  "轮播间隔", &s_interv);
  row(parent, 110, "城市",     &s_city);
  row(parent, 138, "天气",     &s_weath);
  row(parent, 166, "同步",     &s_sync);
  row(parent, 194, "触摸",     &s_touch);
  row(parent, 222, "存储卡",   &s_sd);

  label(parent, "版本", cn(), ink(), LV_ALIGN_TOP_LEFT, 14, 250);
  label(parent, FW_VERSION, latinSmall(), muted(),
        LV_ALIGN_TOP_LEFT, 132, 252);

  rule(parent, 282, 216, divider(), 12);
  label(parent, "使用串口命令", cn(), muted(),
        LV_ALIGN_TOP_LEFT, 14, 290);
  label(parent, "BL= / INTERVAL= / PAGE=", latinSmall(), muted(),
        LV_ALIGN_TOP_LEFT, 14, 310);
}

void refresh() {
  const Snapshot s = snapshot();
  char buf[32];
  snprintf(buf, sizeof(buf), "%u / 255", s.brightness);
  lv_label_set_text(s_bright, buf);
  snprintf(buf, sizeof(buf), "%lu s",
           static_cast<unsigned long>(s.slideshowMs / 1000));
  lv_label_set_text(s_interv, buf);
  lv_label_set_text(s_city, s.weatherConfigured ? s.city : "未配置");
  lv_label_set_text(s_weath, s.weatherValid ? "已配置" : "天气未配置");
  lv_label_set_text(s_sync, "服务未接入");
  lv_label_set_text(s_touch, touch::isReady() ? "触摸已启用" : "触摸未启用");
  lv_label_set_text(s_sd, storage::isReady()
                              ? "存储卡已就绪" : "存储卡未就绪");
}

}  // namespace settings
}  // namespace ui
}  // namespace sunisle
