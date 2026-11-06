#include "ui/ui_photos.h"

#include "sunisle/config.h"
#include "sunisle/state.h"
#include "hardware/storage_manager.h"
#include "hardware/touch_manager.h"
#include "services/photo_service.h"
#include "ui/ui_theme.h"

#include <stdio.h>

namespace sunisle {
namespace ui {
namespace photos {
namespace {

lv_obj_t* s_countLbl = nullptr;
lv_obj_t* s_fileLbl  = nullptr;
lv_obj_t* s_storeLbl = nullptr;
lv_obj_t* s_syncLbl  = nullptr;
lv_obj_t* s_touchLbl = nullptr;

}  // namespace

void create(lv_obj_t* parent) {
  label(parent, "照片", cn(), ink(), LV_ALIGN_TOP_LEFT, 14, 10);
  label(parent, "PHOTO LIBRARY", latinSmall(), muted(),
        LV_ALIGN_TOP_LEFT, 14, 30);
  rule(parent, 46, 216, divider(), 12);

  label(parent, "共", cn(), muted(), LV_ALIGN_TOP_LEFT, 14, 64);
  s_countLbl = label(parent, "0", latinTime(), ink(),
                     LV_ALIGN_TOP_LEFT, 40, 56);
  label(parent, "张", cn(), muted(), LV_ALIGN_TOP_LEFT, 152, 64);

  rule(parent, 120, 216, divider(), 12);
  s_fileLbl = label(parent, "暂无照片 / 等待首张照片", cn(), muted(),
                    LV_ALIGN_TOP_LEFT, 14, 134);
  s_storeLbl = label(parent, "存储卡未就绪", cn(), muted(),
                     LV_ALIGN_TOP_LEFT, 14, 160);
  s_syncLbl = label(parent, "同步 / 服务未接入", cn(), muted(),
                    LV_ALIGN_TOP_LEFT, 14, 186);
  s_touchLbl = label(parent, "触摸未启用", cn(), muted(),
                     LV_ALIGN_TOP_LEFT, 14, 212);

  rule(parent, 244, 216, divider(), 12);
  label(parent, "使用串口命令", cn(), muted(), LV_ALIGN_TOP_LEFT, 14, 258);
  label(parent, "PAGE=photos  /  RESCAN  /  NEXT", latinSmall(), muted(),
        LV_ALIGN_TOP_LEFT, 14, 282);
}

void refresh() {
  const Snapshot s = snapshot();
  lv_label_set_text(s_countLbl, String(photoService::count()).c_str());
  lv_label_set_text(s_fileLbl, photoService::statusLine());
  lv_label_set_text(s_storeLbl, storage::isReady()
                                    ? "存储卡已就绪" : "存储卡未就绪");
  lv_label_set_text(s_syncLbl, "同步 / 服务未接入");
  lv_label_set_text(s_touchLbl, touch::isReady()
                                    ? "触摸已启用" : "触摸未启用");
  (void)s;
}

}  // namespace photos
}  // namespace ui
}  // namespace sunisle
