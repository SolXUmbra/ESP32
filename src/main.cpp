// 晴屿 SUNISLE B - memo/reminder edition entry point.
// Orchestration ONLY. Real work lives in hardware/ services/ ui/ modules.
// The serial console and the optional network poll both reach the SAME
// commandService::execute() parser (CMD| / SKILL| / TASK| protocol).
#include <Arduino.h>
#include <lvgl.h>

#include "sunisle/config.h"
#include "sunisle/state.h"
#include "utils/logger.h"
#include "utils/memory_monitor.h"
#include "hardware/display_manager.h"
#include "hardware/backlight_manager.h"
#include "hardware/storage_manager.h"
#include "hardware/touch_manager.h"
#include "services/time_service.h"
#include "services/memo_service.h"
#include "services/command_service.h"
#include "services/cmd_link.h"
#include "services/weather_service.h"
#include "services/quote_service.h"
#include "ui/ui_manager.h"

using namespace sunisle;

namespace {

String   s_cmd;
uint32_t s_lastTick = 0;
uint32_t s_lastHeartbeat = 0;
bool     s_diagMode = false;   // RAW display diagnostic holds the framebuffer

void printBanner() {
  Serial.println();
  Serial.println(F("====================================="));
  Serial.println(F("   晴屿 SUNISLE B / 备忘录提醒版"));
  Serial.printf("   %s  build %s %s\n", FW_VERSION, __DATE__, __TIME__);
  Serial.println(F("   CMD|help  for the WorkBuddy protocol"));
  Serial.println(F("====================================="));
}

void handleCommand(const String& raw) {
  String line = raw;
  line.trim();
  if (!line.length()) return;

  // Protocol lines keep their original case (values may contain UTF-8 text).
  String head = line;
  head.toUpperCase();
  if (head.startsWith("CMD|") || head.startsWith("SKILL|") ||
      head.startsWith("TASK|")) {
    String reply;
    commandService::execute(line, reply);
    Serial.println(reply);
    return;
  }

  String c = line;
  c.toUpperCase();

  if (c == "HELP") {
    Serial.println(F("HELP | STATE | MEM | SD | TIME | WX | CMOS | PAGE=home|memos|settings"));
    Serial.println(F("CMD|help            (WorkBuddy protocol v1)"));
    Serial.println(F("CMD|status | CMD|list | CMD|add|text=..|at=HH:MM"));
    Serial.println(F("SKILL|GET | SKILL|SET|memo.lead_min=10"));
    Serial.println(F("TASK|ADD|name=..|steps=a;b;c | TASK|LIST"));
    Serial.println(F("MEMO (dump) | BL=<0..255> | DIAG | UI | REBOOT"));
    Serial.println(F("SDPROBE | SDPROBE=FULL | TOUCH | TOUCHPROBE | TOUCHPROBE=34"));
  } else if (c == "WX") {
    weatherService::dump();
  } else if (c == "STATE") {
    Snapshot s = snapshot();
    Serial.printf("[state] state=%s page=%u sd=%s time=%s wifi=%s "
                  "weatherCfg=%s weatherValid=%s memos=%u due=%u tasks=%u\n",
                  stateName(s.state), static_cast<unsigned>(s.page),
                  s.sdReady ? "YES" : "NO",
                  s.timeSynced ? s.timeHHMM : "UNSYNCED",
                  s.wifiOn ? "ON" : "OFF",
                  s.weatherConfigured ? "YES" : "NO",
                  s.weatherValid ? "YES" : "NO",
                  static_cast<unsigned>(s.memoCount),
                  static_cast<unsigned>(s.dueCount),
                  static_cast<unsigned>(commandService::taskCount()));
    Serial.printf("[state] date=%s %s city=%s weather=%s alert=%s\n",
                  s.dateLine, s.weekday, s.city, s.weatherLine,
                  s.activeMemo[0] ? s.activeMemo : "none");
  } else if (c == "MEMO") {
    memoService::dump();
  } else if (c == "CMOS" || c == "PROTO") {
    commandService::dump();
  } else if (c == "MEM") {
    memory::dump();
  } else if (c == "SD") {
    storage::dump();
  } else if (c == "TIME") {
    timeService::dump();
  } else if (c.startsWith("PAGE=")) {
    String p = c.substring(5);
    Page page = Page::HOME;
    if (p == "MEMOS") page = Page::MEMOS;
    else if (p == "SETTINGS") page = Page::SETTINGS;
    else if (p != "HOME") {
      Serial.println(F("[cmd] usage: PAGE=home|memos|settings"));
      return;
    }
    s_diagMode = false;
    ui::showPage(page);
  } else if (c.startsWith("BL=")) {
    uint8_t v = static_cast<uint8_t>(c.substring(3).toInt());
    backlight::set(v);
    Snapshot s = snapshot();
    s.brightness = v;
    publish(s);
    storage::saveSettings(s);
    Serial.printf("[cmd] brightness=%u\n", v);
    ui::tick1s();
  } else if (c == "SDPROBE" || c == "SDPROBE=FULL") {
    storage::runProbe(c == "SDPROBE=FULL");
  } else if (c == "TOUCH") {
    uint16_t x = 0, y = 0;
    const bool pressed = display::touchRead(&x, &y);
    Serial.printf("[touch] ready=%s pressed=%s x=%u y=%u (cs=%d)\n",
                  display::touchReady() ? "YES" : "NO",
                  pressed ? "YES" : "no",
                  static_cast<unsigned>(x), static_cast<unsigned>(y),
                  static_cast<int>(TOUCH_CS));
  } else if (c.startsWith("TOUCHPROBE")) {
    int cand[3] = {33, 32, 34};   // candidate touch CS pins
    int n = 3;
    String t = raw; t.trim();
    if (t.indexOf('=') >= 0) {
      cand[0] = t.substring(t.indexOf('=') + 1).toInt();
      n = 1;
    }
    for (int i = 0; i < n; ++i) {
      const bool ok = display::touchProbe(static_cast<int8_t>(cand[i]));
      uint16_t x = 0, y = 0;
      const bool reads = ok && display::touchRead(&x, &y);
      Serial.printf("[touchprobe] cs=%d init=%s read=%s\n", cand[i],
                    ok ? "OK" : "fail", reads ? "YES" : "no");
    }
    Serial.println(F("[touchprobe] set TOUCH_CS in include/sunisle/config.h to the OK pin"));
  } else if (c == "DIAG") {
    s_diagMode = true;
    display::drawRawDiagnostic();
    Serial.println(F("[cmd] RAW diagnostic held. Send UI to resume LVGL."));
  } else if (c == "UI") {
    s_diagMode = false;
    lv_obj_invalidate(lv_scr_act());
    Serial.println(F("[cmd] LVGL resumed, full redraw requested"));
  } else if (c == "REBOOT") {
    Serial.println(F("[cmd] restarting..."));
    Serial.flush();
    delay(100);
    ESP.restart();
  } else {
    Serial.printf("[cmd] unknown: %s (try HELP or CMD|help)\n", c.c_str());
  }
}

}  // namespace

void setup() {
  // Protocol lines can carry UTF-8 memos; keep a generous RX buffer.
  Serial.setRxBufferSize(4096);
  Serial.begin(115200);
  delay(300);
  printBanner();

  stateInit();
  memory::sample();
  storage::preDisplayProbe();

  if (!display::begin()) {
    Serial.println(F("[boot] DISPLAY FAILED - serial-only diagnostic mode"));
    setState(SystemState::ERROR_RECOVERY);
    return;
  }
  touch::begin();
  storage::begin();         // degrades gracefully when no card is present
  timeService::begin();
  memoService::begin();     // loads memos.tsv (soft-delete table)
  commandService::begin();  // loads skills.tsv + tasks.tsv
  cmdLink::begin();         // network poll is opt-in via secrets.h
  weatherService::begin();
  quoteService::begin();
  ui::begin();
  lv_refr_now(nullptr);

  Serial.println(F("[boot] HELP / CMD|help for commands"));
  Serial.printf("[boot] B READY heap=%u B flush=%u\n",
                static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(display::flushCount()));
  s_lastTick = s_lastHeartbeat = millis();
}

void loop() {
  if (display::isReady() && !s_diagMode) {
    lv_timer_handler();
    ui::tickFrame();
  }
  memory::sample();

  const uint32_t now = millis();
  if (now - s_lastTick >= UI_TICK_MS) {
    s_lastTick = now;
    timeService::tick();
    weatherService::tick();
    quoteService::tick();
    memoService::tick();     // due-check + alert auto-dismiss
    ui::tick1s();            // pushes memo list / alert to LVGL
  }

  while (Serial.available()) {
    char ch = static_cast<char>(Serial.read());
    if (ch == '\n' || ch == '\r') {
      if (s_cmd.length()) { handleCommand(s_cmd); s_cmd.clear(); }
    } else if (s_cmd.length() < 255) {
      s_cmd += ch;
    }
  }

  if (commandService::rebootPending()) {
    Serial.flush();
    delay(100);
    ESP.restart();
  }

  if (now - s_lastHeartbeat >= HEARTBEAT_MS) {
    s_lastHeartbeat = now;
    Snapshot s = snapshot();
    Serial.printf("[hb] up=%lus heap=%uKB min=%uKB state=%s page=%u "
                  "sd=%s memos=%u due=%u tasks=%u flush=%u\n",
                  static_cast<unsigned long>(millis() / 1000),
                  static_cast<unsigned>(ESP.getFreeHeap() / 1024),
                  static_cast<unsigned>(memory::minHeap() / 1024),
                  stateName(s.state), static_cast<unsigned>(s.page),
                  s.sdReady ? "YES" : "NO",
                  static_cast<unsigned>(s.memoCount),
                  static_cast<unsigned>(s.dueCount),
                  static_cast<unsigned>(commandService::taskCount()),
                  static_cast<unsigned>(display::flushCount()));
  }

  delay(LVGL_LOOP_MS);
}
