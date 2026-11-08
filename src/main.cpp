// 晴屿 QINGYU - UI 2.0 entry point.
// Orchestration ONLY. Four screens: BOOT / HOME / MEMOS / GUEST.
// NO LVGL pointer indev is registered (white-screen root cause, see
// B方案_白屏二分记录.md). Navigation comes from serial, plus an optional,
// boot-disabled low-rate XPT2046 polling layer (TOUCH=on) that reports taps
// through a callback instead of an LVGL input device.
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
#include "services/weather_service.h"
#include "services/quote_service.h"
#include "ui/ui_manager.h"
#include "ui/ui_bridge.h"

using namespace sunisle;

namespace {

String   s_cmd;
uint32_t s_lastTick = 0;
uint32_t s_lastHeartbeat = 0;
bool     s_diagMode = false;   // RAW display diagnostic holds the framebuffer

// A tap is dispatched to the SAME UI actions as ACT=/NEXT. GUEST deliberately
// ignores every tap: the privacy overlay must never be touch-dismissable.
static void onTouchTap(uint16_t sx, uint16_t sy,
                        uint16_t rx, uint16_t ry, uint16_t z) {
  Serial.printf("[touch] tap raw=%u,%u z=%u -> screen=%u,%u\n",
                rx, ry, z, sx, sy);
  if (!uiBridge::owner()) {
    Serial.println(F("[touch] GUEST ignored (privacy)"));
    return;
  }
  const Page page = snapshot().page;
  if (page == Page::HOME) {
    uiBridge::nextPage();
    Serial.println(F("[touch] HOME -> MEMOS"));
  } else if (page == Page::MEMOS) {
    // Hit boxes follow the 320x240 design sheet (qingyu_ui.cpp).
    if (sx >= 24 && sx <= 100 && sy >= 108 && sy <= 142) {
      uiBridge::action(0);
      Serial.println(F("[touch] MEMOS -> ACK"));
    } else if (sx >= 96 && sx <= 172 && sy >= 108 && sy <= 142) {
      uiBridge::action(1);
      Serial.println(F("[touch] MEMOS -> LATER"));
    } else if (sx >= 218 && sx <= 304 && sy >= 166 && sy <= 198) {
      uiBridge::action(2);
      Serial.println(F("[touch] MEMOS -> DETAIL"));
    } else {
      uiBridge::nextPage();
      Serial.println(F("[touch] MEMOS -> HOME"));
    }
  }
}

void printBanner() {
  Serial.println();
  Serial.println(F("====================================="));
  Serial.println(F("   QINGYU  /  UI 2.0"));
  Serial.printf("   %s  build %s %s\n", FW_VERSION, __DATE__, __TIME__);
  Serial.println(F("   A Little Island of Sunshine."));
  Serial.println(F("====================================="));
}

void handleCommand(const String& raw) {
  String c = raw;
  c.trim();
  if (c.length() == 0) return;
  c.toUpperCase();

  if (c == "HELP") {
    Serial.println(F("HELP | STATE | MEM | SD | TIME | WX"));
    Serial.println(F("PAGE=home|memos|guest|boot | NEXT"));
    Serial.println(F("PRIVACY=owner|guest  (LOCAL DEBUG only)"));
    Serial.println(F("DEMO=on|off | ACT=ack|later|detail|task"));
    Serial.println(F("BL=<0..255> | DIAG | UI | SDPROBE[=FULL] | REBOOT"));
    Serial.println(F("TOUCH | TOUCH=on|off | TOUCHPROBE[=pin] | TOUCHSCAN[=ms]"));
    Serial.println(F("TOUCHCS=<pin> | TOUCHMAP=0..3 | I2CPROBE  (touch held OFF at boot)"));
    Serial.println(F("TOUCHSPI[=<cs>,<ms>] | TOUCHMISO[=<cs>] | TOUCHMATRIX | TOUCHIRQ[=<ms>]"));
  } else if (c == "WX") {
    weatherService::dump();
  } else if (c == "STATE") {
    Snapshot s = snapshot();
    Serial.printf("[state] state=%s page=%u sd=%s time=%s wifi=%s "
                  "weatherCfg=%s weatherValid=%s privacy=%s\n",
                  stateName(s.state), static_cast<unsigned>(s.page),
                  s.sdReady ? "YES" : "NO",
                  s.timeSynced ? s.timeHHMM : "UNSYNCED",
                  s.wifiOn ? "ON" : "OFF",
                  s.weatherConfigured ? "YES" : "NO",
                  s.weatherValid ? "YES" : "NO",
                  uiBridge::owner() ? "OWNER" : "GUEST");
    Serial.printf("[state] demo=%s\n",
                  uiBridge::demo() ? "ON(sample)" : "OFF(real)");
    Serial.printf("[state] date=%s %s city=%s weather=%s quote=%s\n",
                  s.dateLine, s.weekday, s.city, s.weatherLine, s.quote);
  } else if (c == "MEM") {
    memory::dump();
  } else if (c == "SD") {
    storage::dump();
  } else if (c == "TIME") {
    timeService::dump();
  } else if (c.startsWith("PAGE=")) {
    const String p = c.substring(5);
    Page page = Page::GUEST;
    bool ok = true;
    if      (p == "HOME")  page = Page::HOME;
    else if (p == "MEMOS") page = Page::MEMOS;
    else if (p == "GUEST") page = Page::GUEST;
    else if (p == "BOOT")  page = Page::BOOT;
    else ok = false;
    if (!ok) {
      Serial.println(F("[cmd] PAGE=home|memos|guest|boot"));
      return;
    }
    s_diagMode = false;
    ui::showPage(page);
  } else if (c == "NEXT") {
    s_diagMode = false;
    uiBridge::nextPage();
  } else if (c.startsWith("PRIVACY=")) {
    const bool on = c.endsWith("OWNER");
    uiBridge::setPrivacy(on, false);   // false = local debug, not Bridge auth
    s_diagMode = false;
    ui::showPage(on ? Page::HOME : Page::GUEST);
  } else if (c == "DEMO=ON" || c == "DEMO=OFF") {
    uiBridge::setDemo(c == "DEMO=ON");
    uiBridge::tick();
  } else if (c.startsWith("ACT=")) {
    const String a = c.substring(4);
    int8_t idx = -1;
    if      (a == "ACK")    idx = 0;
    else if (a == "LATER")  idx = 1;
    else if (a == "DETAIL") idx = 2;
    else if (a == "TASK")   idx = 3;
    if (idx < 0) {
      Serial.println(F("[cmd] ACT=ack|later|detail|task"));
      return;
    }
    ui::showPage(Page::MEMOS);   // actions live on the MEMOS screen
    uiBridge::action(static_cast<uint8_t>(idx));
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
  } else if (c == "DIAG") {
    // Raw LovyanGFX pattern: proves the panel/rotation independent of LVGL.
    s_diagMode = true;
    display::drawRawDiagnostic();
    Serial.println(F("[cmd] RAW diagnostic held. Send UI to resume LVGL."));
  } else if (c == "UI") {
    s_diagMode = false;
    lv_obj_invalidate(lv_scr_act());
    Serial.println(F("[cmd] LVGL resumed, full redraw requested"));
  } else if (c == "TOUCH") {
    touch::report();
  } else if (c == "TOUCH=ON" || c == "TOUCH=OFF") {
    touch::setEnabled(c == "TOUCH=ON");
    touch::report();
  } else if (c == "TOUCHPROBE") {
    touch::probe();
  } else if (c.startsWith("TOUCHPROBE=")) {
    touch::probe(static_cast<int8_t>(c.substring(11).toInt()));
  } else if (c.startsWith("TOUCHCS=")) {
    touch::setCs(static_cast<int8_t>(c.substring(8).toInt()));
  } else if (c.startsWith("TOUCHMAP=")) {
    touch::setMap(static_cast<uint8_t>(c.substring(9).toInt()));
  } else if (c == "TOUCHSCAN") {
    touch::scan(12000);
  } else if (c.startsWith("TOUCHSCAN=")) {
    uint32_t ms = static_cast<uint32_t>(c.substring(10).toInt());
    if (ms > 30000) ms = 30000;
    touch::scan(ms);
  } else if (c == "I2CPROBE") {
    touch::i2cProbe();
  } else if (c == "TOUCHSPI") {
    touch::spiProbe(-1, 0);
  } else if (c.startsWith("TOUCHSPI=")) {
    // TOUCHSPI=<cs>[,<ms>] : independent Arduino SPI full-duplex read.
    const String a = c.substring(9);
    const int comma = a.indexOf(',');
    const int8_t cs = static_cast<int8_t>(
        (comma < 0 ? a : a.substring(0, comma)).toInt());
    uint32_t ms = (comma < 0) ? 12000UL
                              : static_cast<uint32_t>(a.substring(comma + 1).toInt());
    if (ms > 30000UL) ms = 30000UL;
    touch::spiProbe(cs, ms);
  } else if (c == "TOUCHMATRIX") {
    touch::matrixProbe();
  } else if (c == "TOUCHMISO") {
    touch::misoProbe(33);
  } else if (c.startsWith("TOUCHMISO=")) {
    touch::misoProbe(static_cast<int8_t>(c.substring(10).toInt()));
  } else if (c == "TOUCHIRQ") {
    touch::irqWatch(15000);
  } else if (c.startsWith("TOUCHIRQ=")) {
    uint32_t ms = static_cast<uint32_t>(c.substring(9).toInt());
    if (ms > 30000UL) ms = 30000UL;
    touch::irqWatch(ms);
  } else if (c == "REBOOT") {
    Serial.println(F("[cmd] restarting..."));
    Serial.flush();
    delay(100);
    ESP.restart();
  } else {
    Serial.printf("[cmd] unknown: %s\n", c.c_str());
  }
}

}  // namespace

void setup() {
  Serial.setRxBufferSize(2048);
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
  touch::begin();           // intentionally HELD OFF (touch_manager.h)
  storage::begin();         // settings / weather cache only, no photo use
  timeService::begin();
  weatherService::begin();
  quoteService::begin();
  ui::begin();
  lv_refr_now(nullptr);     // first full frame

  Serial.println(F("[boot] HELP for commands; power-on privacy = GUEST"));
  Serial.printf("[boot] UI 2.0 READY heap=%u B flush=%u\n",
                static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(display::flushCount()));
  s_lastTick = s_lastHeartbeat = millis();
}

void loop() {
  if (display::isReady() && !s_diagMode) {
    lv_timer_handler();
    ui::tickFrame();          // boot fade -> Guest
  }
  memory::sample();

  const uint32_t now = millis();
  touch::tick(now, onTouchTap);   // no-op unless TOUCH=on was issued
  if (now - s_lastTick >= UI_TICK_MS) {
    s_lastTick = now;
    timeService::tick();
    weatherService::tick();
    quoteService::tick();
    ui::tick1s();
  }

  while (Serial.available()) {
    char ch = static_cast<char>(Serial.read());
    if (ch == '\n' || ch == '\r') {
      if (s_cmd.length()) { handleCommand(s_cmd); s_cmd.clear(); }
    } else if (s_cmd.length() < 63) {
      s_cmd += ch;
    }
  }

  if (now - s_lastHeartbeat >= HEARTBEAT_MS) {
    s_lastHeartbeat = now;
    Snapshot s = snapshot();
    Serial.printf("[hb] up=%lus heap=%uKB min=%uKB state=%s page=%u "
                  "privacy=%s flush=%u\n",
                  static_cast<unsigned long>(millis() / 1000),
                  static_cast<unsigned>(ESP.getFreeHeap() / 1024),
                  static_cast<unsigned>(memory::minHeap() / 1024),
                  stateName(s.state), static_cast<unsigned>(s.page),
                  uiBridge::owner() ? "OWNER" : "GUEST",
                  static_cast<unsigned>(display::flushCount()));
  }

  delay(LVGL_LOOP_MS);
}
