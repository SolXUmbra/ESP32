// UI 2.0 orchestration. Owns the BOOT fade + the single linear progress
// sweep, then hands the device to GUEST. Page / privacy / action requests go
// through the Bridge adapter. LVGL is touched exclusively from the core-1 loop.
#include "ui/ui_manager.h"

#include "ui/ui_bridge.h"
#include "ui/ui_theme.h"
#include "sunisle/config.h"
#include "sunisle/state.h"
#include "utils/logger.h"

namespace sunisle {
namespace ui {
namespace {

lv_obj_t* s_veil = nullptr;
uint32_t  s_bootStart = 0;
uint8_t   s_lastPct = 0xFF;
Page      s_page = Page::GUEST;

}  // namespace

void begin() {
  uiBridge::begin();
  uiBridge::showBoot();        // power-on default: BOOT, never Owner

  s_veil = block(lv_scr_act(), SCREEN_W, SCREEN_H, LV_ALIGN_TOP_LEFT, 0, 0,
                 bg(), 0, LV_OPA_COVER);
  lv_obj_move_foreground(s_veil);
  s_bootStart = millis();
  s_lastPct = 0xFF;

  setState(SystemState::LOCAL_INIT);
  setPage(Page::GUEST);
  uiBridge::tick();            // public clock + quote only; private setters drop
  SUN_LOG("ui", "UI 2.0 BOOT (%lu ms) -> GUEST; power-on privacy = GUEST",
          static_cast<unsigned long>(BOOT_TOTAL_MS));
}

void tickFrame() {
  if (!s_veil) return;
  const uint32_t e = millis() - s_bootStart;
  const lv_opa_t opa = static_cast<lv_opa_t>(
      e >= BOOT_FADE_MS ? 0 : (255 - (e * 255 / BOOT_FADE_MS)));
  lv_obj_set_style_bg_opa(s_veil, opa, LV_PART_MAIN);

  // One linear progress sweep; only touch the widget when the integer percent
  // changes, so the 5 ms loop does not invalidate the whole bar every frame.
  uint8_t pct = static_cast<uint8_t>(
      e >= BOOT_TOTAL_MS ? 100 : (e * 100 / BOOT_TOTAL_MS));
  if (pct != s_lastPct) { s_lastPct = pct; uiBridge::setBootProgress(pct); }

  if (e >= BOOT_TOTAL_MS) {
    lv_obj_del(s_veil);
    s_veil = nullptr;
    // Task book: power-on default is GUEST. BUT if the user already unlocked
    // (BOOT long-press or PRIVACY=owner) during the splash, respect that
    // explicit session instead of forcing GUEST on top of it.
    if (!uiBridge::owner()) {
      uiBridge::setPrivacy(false, false);
    }
    setState(SystemState::HOME_READY);
    s_page = uiBridge::owner() ? Page::HOME : Page::GUEST;
    setPage(s_page);
    uiBridge::tick();
    lv_obj_invalidate(lv_scr_act());
    SUN_LOG("ui", "BOOT done -> %s",
            uiBridge::owner() ? "HOME (unlocked during boot)" : "GUEST (default)");
  }
}

void showPage(Page p) {
  if (p == Page::BOOT) {
    uiBridge::showBoot();
  } else if (p == Page::GUEST) {
    uiBridge::setPrivacy(false, false);
  } else {
    // HOME / MEMOS map 1:1 onto qy_page_t 0..1.
    const uint8_t idx = static_cast<uint8_t>(p);
    if (!uiBridge::owner()) {
      SUN_LOG("ui", "PAGE=%u blocked: GUEST active (PRIVACY=OWNER first)",
              static_cast<unsigned>(idx));
      return;
    }
    uiBridge::showPage(idx);
  }
  s_page = p;
  setPage(p);
  uiBridge::tick();
  SUN_LOG("ui", "page -> %u", static_cast<unsigned>(p));
}

void tick1s() { uiBridge::tick(); }

Page currentPage() { return s_page; }
bool bootActive() { return s_veil != nullptr; }

}  // namespace ui
}  // namespace sunisle
