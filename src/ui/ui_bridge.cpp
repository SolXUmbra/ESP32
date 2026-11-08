// UI 2.0 adapter. Feeds the four screens with REAL local state only:
// time / date / weekday / daily quote from the existing services, plus an
// explicitly labelled DEMO sample set for visual verification.
// There is NO Bridge session yet, so the honest live state is empty; no
// fabricated WorkBuddy payload is ever presented as real data.
#include "ui/ui_bridge.h"

#include "ui/qingyu_ui.h"
#include "fonts/sunisle_cn_chars.h"
#include "sunisle/config.h"
#include "sunisle/state.h"
#include "utils/logger.h"

#include <stdio.h>
#include <string.h>

namespace sunisle {
namespace uiBridge {
namespace {

static char s_buf[4][192];
static uint16_t s_lastDropped = 0xFFFF;
static bool s_demo = true;   // visual verification; GUEST never shows demo data

void onPageChanged(qy_page_t page, void*) {
  SUN_LOG("qyui", "owner page -> %u", static_cast<unsigned>(page));
}
void onAction(qy_action_t a, void*) {
  SUN_LOG("qyui", "action -> %u (serial/Bridge equivalent)",
          static_cast<unsigned>(a));
}

bool codepointAllowed(uint32_t cp) {
  if (cp < 0x2E80) return true;            // Latin / digits / punctuation via Montserrat
  for (size_t i = 0; i < kSunisleCnCount; ++i)
    if (kSunisleCnCodepoints[i] == cp) return true;
  return false;
}

}  // namespace

// UTF-8 walk: every CJK glyph must exist in sunisle_cn_14, otherwise replace
// with '?' and log the count. Stage D replaces this with rasterised bitmaps
// from the Windows Bridge (TODO in qingyu_ui.h).
const char* sanitizeDynamic(const char* src) {
  static uint8_t slot = 0;
  char* out = s_buf[slot++ & 3];
  out[0] = 0;
  if (!src) return out;

  size_t o = 0;
  uint16_t dropped = 0;
  const uint8_t* p = reinterpret_cast<const uint8_t*>(src);
  while (*p && o + 5 < sizeof(s_buf[0])) {
    const uint8_t b = *p;
    if (b < 0x80) { out[o++] = static_cast<char>(b); ++p; continue; }

    int n = 0;
    uint32_t cp = 0;
    if      ((b & 0xE0) == 0xC0) { n = 2; cp = b & 0x1F; }
    else if ((b & 0xF0) == 0xE0) { n = 3; cp = b & 0x0F; }
    else if ((b & 0xF8) == 0xF0) { n = 4; cp = b & 0x07; }
    else { ++p; ++dropped; continue; }

    bool valid = true;
    for (int i = 1; i < n; ++i) {
      if ((p[i] & 0xC0) != 0x80) { valid = false; break; }
      cp = (cp << 6) | (p[i] & 0x3F);
    }
    if (!valid) { ++p; ++dropped; continue; }

    if (codepointAllowed(cp)) {
      for (int i = 0; i < n; ++i) out[o++] = static_cast<char>(p[i]);
    } else {
      out[o++] = '?';
      ++dropped;
    }
    p += n;
  }
  out[o] = 0;

  if (dropped != s_lastDropped) {
    s_lastDropped = dropped;
    if (dropped)
      SUN_LOG("bridge", "dynamic text: %u glyph(s) outside CJK subset -> '?'",
              static_cast<unsigned>(dropped));
  }
  return out;
}

void begin() {
  qy_ui_callbacks_t cb = {};
  cb.on_page_changed = onPageChanged;
  cb.on_action = onAction;
  qy_ui_init(&cb);
  SUN_LOG("bridge", "UI 2.0 ready: BOOT/HOME/MEMOS/GUEST, 320x240, "
                    "14/16px 8bpp CJK + 34/56px Light digits");
  SUN_LOG("bridge", "DEMO sample data ON (visual check only); DEMO=OFF for real state");
}

void tick() {
  const Snapshot s = snapshot();

  char date[48];
  if (s.dateCn[0] && s.weekdayCn[0])
    snprintf(date, sizeof(date), "%s %s", s.dateCn, s.weekdayCn);
  else
    snprintf(date, sizeof(date), "未校时");

  // PUBLIC channels - always writable, even in Guest.
  qy_ui_set_clock(s.timeSynced ? s.timeHHMM : "--:--");
  qy_ui_set_date(date);
  qy_ui_set_quote(sanitizeDynamic(s.quote[0] ? s.quote : SLOGAN_CN));
  qy_ui_set_status(s_demo ? "DEMO" : (s.wifiOn ? "USB ON" : "USB OFF"), s.wifiOn);

  // PRIVATE channels - dropped by qy_ui while Guest/Boot is active.
  qy_dashboard_t d = {};
  qy_work_t w = {};
  if (s_demo) {
    // Exact sample copy from the four 320x240 design sheets. Shown ONLY on
    // the two Owner pages; the top bar says DEMO so it cannot be mistaken
    // for a real WorkBuddy payload.
    d.task_done = "03";
    d.task_total = "/ 08";
    d.done_line = "已完成 3 项";
    d.progress_pct = 38;
    d.summary1 = "今天优先处理快递财务对账，";
    d.summary2 = "下午检查场地设备。";
    d.next_time = "09:30";
    d.next_title = "核对昨日入库数据";
    w.pending = "2 待处理";
    w.remind_time = "14:30";
    w.remind_title = "检查场地设备";
    w.remind_state = "待处理";
    w.remind_index = "1 / 3";
    w.memo_time = "08:12 更新";
    w.memo_title = "待办与摘要";
    w.memo_summary = "检查场地设备，更新入库数据";
  } else {
    d.task_done = "00";
    d.task_total = "/ 00";
    d.done_line = "已完成 0 项";
    d.progress_pct = 0;
    d.summary1 = "等待 Bridge 接入";
    d.summary2 = "暂无摘要";
    d.next_time = "--:--";
    d.next_title = "暂无待办";
    w.pending = "0 待处理";
    w.remind_time = "--:--";
    w.remind_title = "暂无提醒";
    w.remind_state = "--";
    w.remind_index = "0 / 0";
    w.memo_time = "--:--";
    w.memo_title = "暂无备忘";
    w.memo_summary = "暂无数据";
  }
  qy_ui_set_dashboard(&d);
  qy_ui_set_work(&w);
}

void showBoot() { qy_ui_show_boot(); }
void setBootProgress(uint8_t pct) { qy_ui_set_boot_progress(pct); }

void setPrivacy(bool isOwner, bool bridgeVerified) {
  if (isOwner && !bridgeVerified)
    SUN_LOG("bridge", "OWNER via LOCAL DEBUG - NOT a verified Bridge session");
  qy_ui_set_privacy(isOwner ? QY_PRIVACY_OWNER : QY_PRIVACY_GUEST);
  SUN_LOG("bridge", "privacy -> %s", isOwner ? "OWNER" : "GUEST");
}

void showPage(uint8_t idx) {
  if (idx >= QY_PAGE_COUNT) {
    SUN_LOG("bridge", "PAGE index out of range: %u", static_cast<unsigned>(idx));
    return;
  }
  if (!owner()) {
    SUN_LOG("bridge", "PAGE blocked: GUEST is active (PRIVACY=OWNER first)");
    return;
  }
  qy_ui_show_page(static_cast<qy_page_t>(idx));
}
void nextPage() {
  if (!owner()) {
    SUN_LOG("bridge", "NEXT blocked: GUEST is active");
    return;
  }
  qy_ui_next_page();
}
void action(uint8_t a) {
  if (!owner()) { SUN_LOG("bridge", "ACT blocked: GUEST is active"); return; }
  qy_ui_action(static_cast<qy_action_t>(a));
}
void setDemo(bool on) {
  s_demo = on;
  SUN_LOG("bridge", "DEMO -> %s", on ? "ON (sample data)" : "OFF (real state)");
}
bool demo() { return s_demo; }
bool owner() { return qy_ui_private_visible(); }

}  // namespace uiBridge
}  // namespace sunisle
