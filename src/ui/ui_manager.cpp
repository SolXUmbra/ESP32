// UI orchestration. The screen itself is the DELIVERED vendor UI
// (ui_sunisle.cpp); this file only owns the boot splash fade and forwards
// page/data requests to ui_bridge / ui_sunisle setters.
// LVGL is touched ONLY from the core-1 loop.
#include "ui/ui_manager.h"

#include "ui/ui_bridge.h"
#include "ui_sunisle.h"
#include "ui/ui_theme.h"
#include "ui/ui_boot.h"
#include "sunisle/config.h"
#include "sunisle/state.h"
#include "utils/logger.h"

namespace sunisle {
namespace ui {
namespace {

lv_obj_t* s_boot = nullptr;
lv_obj_t* s_veil = nullptr;
uint32_t  s_bootStart = 0;
Page      s_page = Page::HOME;

}  // namespace

void begin() {
  // lv_init() + display driver registration already happened in display::begin().
  uiBridge::begin();          // -> ui_init() of the delivered UI

  lv_obj_t* scr = lv_scr_act();
  s_boot = boot::create(scr);
  s_veil = block(s_boot, SCREEN_W, SCREEN_H, LV_ALIGN_TOP_LEFT, 0, 0,
                 bg(), 0, LV_OPA_COVER);
  lv_obj_move_foreground(s_veil);
  s_bootStart = millis();

  setState(SystemState::LOCAL_INIT);
  uiBridge::tick();           // push real (unsynced/unconfigured) state at once
  SUN_LOG("ui", "vendor UI + brand splash (%lu ms); compact CJK subset only",
          static_cast<unsigned long>(BOOT_TOTAL_MS));
}

void tickFrame() {
  if (!s_boot) return;
  const uint32_t e = millis() - s_bootStart;
  const lv_opa_t opa = static_cast<lv_opa_t>(
      e >= BOOT_FADE_MS ? 0 : (255 - (e * 255 / BOOT_FADE_MS)));
  lv_obj_set_style_bg_opa(s_veil, opa, LV_PART_MAIN);
  if (e >= BOOT_TOTAL_MS) {
    lv_obj_del(s_boot);       // also deletes the veil child
    s_boot = nullptr;
    s_veil = nullptr;
    setState(SystemState::HOME_READY);
    showPage(Page::HOME);
    uiBridge::tick();
    lv_obj_invalidate(lv_scr_act());
    SUN_LOG("ui", "splash done -> HOME_READY (vendor single screen)");
  }
}

void showPage(Page p) {
  s_page = p;
  // The B-version UI keeps ONE calm screen with a memo panel; the memo view
  // hook currently loads the same screen (page switching stays serial-driven).
  if (p == Page::MEMOS) ui_show_memo_view();
  else                  ui_show_home();
  setPage(p);
  uiBridge::tick();
  const char* n = p == Page::HOME ? "HOME"
                  : (p == Page::MEMOS ? "MEMOS" : "SETTINGS");
  SUN_LOG("ui", "page -> %s (single-screen memo UI)", n);
}

void tick1s() {
  if (!s_boot) uiBridge::tick();
}

Page currentPage() { return s_page; }
bool bootActive() { return s_boot != nullptr; }

}  // namespace ui
}  // namespace sunisle
