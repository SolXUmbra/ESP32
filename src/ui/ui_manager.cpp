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
// Mirrors the Windows desktop lock state. The panel never decides this itself:
// see main.cpp applySessionLock() and the SESSION= serial hook.
bool      s_sessionLocked = false;

}  // namespace

void begin() {
  uiBridge::begin();
  uiBridge::showBoot();        // splash only; privacy is applied when it ends

  s_veil = block(lv_scr_act(), SCREEN_W, SCREEN_H, LV_ALIGN_TOP_LEFT, 0, 0,
                 bg(), 0, LV_OPA_COVER);
  lv_obj_move_foreground(s_veil);
  s_bootStart = millis();
  s_lastPct = 0xFF;

  setState(SystemState::LOCAL_INIT);
  setPage(Page::GUEST);
  uiBridge::tick();            // public clock + quote only; private setters drop
  SUN_LOG("ui", "UI 2.0 BOOT (%lu ms); lock state mirrors the Windows session",
          static_cast<unsigned long>(BOOT_TOTAL_MS));
}

void tickFrame() {
  if (!s_veil) return;
  const uint32_t e = millis() - s_bootStart;

  // Three-phase veil opacity: fade IN (255 -> 0), hold (0), fade OUT (0 -> 255).
  // The veil is rice-paper coloured, so the fade-out melts the boot into the
  // same ground the HOME page uses - the handoff at the end is seamless.
  lv_opa_t opa;
  if (e < BOOT_FADE_MS) {
    opa = static_cast<lv_opa_t>(255 - (e * 255 / BOOT_FADE_MS));
  } else if (e < BOOT_FADE_MS + BOOT_HOLD_MS) {
    opa = LV_OPA_TRANSP;
  } else if (e < BOOT_TOTAL_MS) {
    const uint32_t o = e - BOOT_FADE_MS - BOOT_HOLD_MS;
    opa = static_cast<lv_opa_t>((o * 255 / BOOT_FADE_OUT_MS));
  } else {
    opa = LV_OPA_COVER;
  }
  lv_obj_set_style_bg_opa(s_veil, opa, LV_PART_MAIN);

  // One linear progress sweep across fade-in + hold; it stays full during the
  // fade-out. Only touch the widget when the integer percent changes.
  const uint32_t ptotal = BOOT_FADE_MS + BOOT_HOLD_MS;
  uint8_t pct = static_cast<uint8_t>(e >= ptotal ? 100 : (e * 100 / ptotal));
  if (pct != s_lastPct) { s_lastPct = pct; uiBridge::setBootProgress(pct); }

  if (e >= BOOT_TOTAL_MS) {
    lv_obj_del(s_veil);
    s_veil = nullptr;
    // Mirror the Windows desktop. Booting into GUEST with no session link is a
    // dead end: GUEST drops every tap, so the panel would look frozen. Default
    // to UNLOCKED and let an explicit lock event (SESSION=lock, later the
    // Bridge) put it away.
    if (s_sessionLocked) {
      uiBridge::setPrivacy(false, true);
    } else {
      uiBridge::setPrivacy(true, true);
    }
    setState(SystemState::HOME_READY);
    s_page = s_sessionLocked ? Page::GUEST : Page::HOME;
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
    s_sessionLocked = true;
    uiBridge::setPrivacy(false, false);
  } else {
    s_sessionLocked = false;
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
bool sessionLocked() { return s_sessionLocked; }
bool bootActive() { return s_veil != nullptr; }

}  // namespace ui
}  // namespace sunisle
