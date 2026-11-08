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

// Physical BOOT button (GPIO 0) as a reliable navigation fallback. The
// integrated board exposes it and it is free (unused by LCD / SD / touch).
constexpr int8_t BTN_PIN = 0;
bool     s_btnDown = false;
uint32_t s_btnPressStart = 0;
uint32_t s_lastInteraction = 0;        // owner-activity watchdog for auto-lock
constexpr uint32_t AUTOLOCK_MS = 60000; // OWNER idle -> GUEST after 60 s

// A tap is dispatched to the SAME UI actions as ACT=/NEXT. GUEST deliberately
// ignores every tap: the privacy overlay must never be touch-dismissable.
// `dur` is the press duration in ms (long press in OWNER = manual lock).
static void onTouchTap(uint16_t sx, uint16_t sy,
                        uint16_t rx, uint16_t ry, uint16_t z, uint32_t dur) {
  Serial.printf("[touch] tap raw=%u,%u z=%u dur=%lums -> screen=%u,%u\n",
                rx, ry, z, static_cast<unsigned long>(dur), sx, sy);
  if (!uiBridge::owner()) {
    Serial.println(F("[touch] GUEST ignored (privacy; BOOT btn unlocks)"));
    return;
  }
  s_lastInteraction = millis();
  // Long press while OWNER = manual lock back to GUEST.
  if (dur >= 2000) {
    uiBridge::setPrivacy(false, false);
    ui::showPage(Page::GUEST);
    Serial.println(F("[touch] OWNER -> GUEST (long-press lock)"));
    return;
  }
  const Page page = snapshot().page;
  if (page == Page::HOME) {
    uiBridge::nextPage();
    Serial.println(F("[touch] HOME -> MEMOS"));
  } else if (page == Page::MEMOS) {
    // Hit boxes follow the 320x240 design sheet (qingyu_ui.cpp): ACK/LATER
    // pills at y=116..136, DETAIL row at the bottom-right of the memo card.
    if (sx >= 24 && sx <= 100 && sy >= 108 && sy <= 142) {
      uiBridge::action(0);
      Serial.println(F("[touch] MEMOS -> ACK"));
    } else if (sx >= 96 && sx <= 172 && sy >= 108 && sy <= 142) {
      uiBridge::action(1);
      Serial.println(F("[touch] MEMOS -> LATER"));
    } else if (sx >= 210 && sx <= 304 && sy >= 164 && sy <= 200) {
      uiBridge::action(2);
      Serial.println(F("[touch] MEMOS -> DETAIL"));
    } else {
      uiBridge::nextPage();
      Serial.println(F("[touch] MEMOS -> HOME"));
    }
  }
}

// BOOT button (GPIO 0, active-low): short press = next page (owner only);
// long press = toggle lock. This is the deliberate local unlock gate, keeping
// "touch can never enter Owner" intact - only the physical button can.
static void handleButton(uint32_t now) {
  const bool down = (digitalRead(BTN_PIN) == LOW);
  if (down && !s_btnDown) {
    s_btnDown = true;
    s_btnPressStart = now;
  } else if (!down && s_btnDown) {
    s_btnDown = false;
    const uint32_t d = now - s_btnPressStart;
    if (d < 40) return;                       // debounce glitch
    if (d >= 2000) {                          // long press: toggle lock
      if (uiBridge::owner()) {
        uiBridge::setPrivacy(false, false);
        ui::showPage(Page::GUEST);
        Serial.println(F("[btn] OWNER -> GUEST (long-press lock)"));
      } else {
        uiBridge::setPrivacy(true, false);     // LOCAL unlock, not Bridge auth
        ui::showPage(Page::HOME);
        s_lastInteraction = now;
        Serial.println(F("[btn] GUEST -> HOME (local unlock)"));
      }
    } else if (uiBridge::owner()) {            // short press: next page
      uiBridge::nextPage();
      s_lastInteraction = now;
      Serial.println(F("[btn] NEXT"));
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
    Serial.println(F("NAV: BOOT btn short=NEXT, long(2s)=lock/unlock; touch HOME->MEMOS"));
    Serial.println(F("GUEST long-press BOOT to unlock (touch cannot enter Owner)"));
    Serial.println(F("TOUCH | TOUCH=on|off | TOUCHPROBE[=pin] | TOUCHSCAN[=ms]"));
    Serial.println(F("TOUCHCS=<pin> | TOUCHMAP=0..3 | I2CPROBE | I2CFIND=sda,scl"));
    Serial.println(F("TOUCHSPI[=<cs>,<ms>] | TOUCHIRQ[=<ms>]"));
    Serial.println(F("BRING-UP: GPIOSCAN[=ms] finds PENIRQ; TOUCHBUS=clk,mosi,miso;"));
    Serial.println(F("          TOUCHPINS=clk,mosi,miso,cs,irq; TOUCHMODE=hw|bb"));
    Serial.println(F("          TOUCHRAW[=idleMs,pressMs] untouched-vs-pressed ADC"));
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
    if (on) s_lastInteraction = millis();
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
  } else if (c.startsWith("I2CFIND=")) {
    // I2CFIND=sda,scl  (targeted capacitive-controller scan on one pair)
    const String a = c.substring(8);
    const int comma = a.indexOf(',');
    if (comma < 0) {
      Serial.println(F("[cmd] I2CFIND=sda,scl"));
      return;
    }
    const int8_t sda = static_cast<int8_t>(a.substring(0, comma).toInt());
    const int8_t scl = static_cast<int8_t>(a.substring(comma + 1).toInt());
    touch::i2cFind(sda, scl);
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
  } else if (c.startsWith("TOUCHBUS=")) {
    // TOUCHBUS=clk,mosi,miso  (reconfigure the SPI trio at runtime)
    const String a = c.substring(9);
    const int c1 = a.indexOf(','), c2 = a.indexOf(',', c1 + 1);
    if (c1 < 0 || c2 < 0) {
      Serial.println(F("[cmd] TOUCHBUS=clk,mosi,miso"));
      return;
    }
    const int8_t clk  = static_cast<int8_t>(a.substring(0, c1).toInt());
    const int8_t mosi = static_cast<int8_t>(a.substring(c1 + 1, c2).toInt());
    const int8_t miso = static_cast<int8_t>(a.substring(c2 + 1).toInt());
    touch::setBus(clk, mosi, miso);
  } else if (c.startsWith("TOUCHPINS=")) {
    // TOUCHPINS=clk,mosi,miso,cs,irq  (full reconfigure at runtime)
    const String a = c.substring(10);
    int idx[4]; idx[0] = a.indexOf(',');
    for (int i = 1; i < 4; ++i) idx[i] = a.indexOf(',', idx[i - 1] + 1);
    if (idx[3] < 0) {
      Serial.println(F("[cmd] TOUCHPINS=clk,mosi,miso,cs,irq"));
      return;
    }
    const int8_t clk  = static_cast<int8_t>(a.substring(0, idx[0]).toInt());
    const int8_t mosi = static_cast<int8_t>(a.substring(idx[0] + 1, idx[1]).toInt());
    const int8_t miso = static_cast<int8_t>(a.substring(idx[1] + 1, idx[2]).toInt());
    const int8_t cs   = static_cast<int8_t>(a.substring(idx[2] + 1, idx[3]).toInt());
    const int8_t irq  = static_cast<int8_t>(a.substring(idx[3] + 1).toInt());
    touch::setPins(clk, mosi, miso, cs, irq);
  } else if (c.startsWith("TOUCHMODE=")) {
    // TOUCHMODE=hw|bb  (hardware VSPI transfer vs software bit-bang)
    const String a = c.substring(10);
    touch::setMode(!(a == "bb" || a == "BB" || a == "bitbang"));
  } else if (c == "TOUCHRAW") {
    touch::rawDump(4000, 10000);
  } else if (c.startsWith("TOUCHRAW=")) {
    const String a = c.substring(9);
    const int comma = a.indexOf(',');
    uint32_t idleMs  = static_cast<uint32_t>(a.substring(0, comma < 0 ? a.length() : comma).toInt());
    uint32_t pressMs = (comma < 0) ? 8000UL
                                   : static_cast<uint32_t>(a.substring(comma + 1).toInt());
    if (idleMs > 15000UL) idleMs = 15000UL;
    if (pressMs > 30000UL) pressMs = 30000UL;
    touch::rawDump(idleMs, pressMs);
  } else if (c == "TOUCHLOG") {
    touch::logDump(20000);
  } else if (c.startsWith("TOUCHLOG=")) {
    uint32_t ms = static_cast<uint32_t>(c.substring(9).toInt());
    if (ms > 60000UL) ms = 60000UL;
    touch::logDump(ms);
  } else if (c == "TOUCHDBG") {
    touch::debugFor(15000);
  } else if (c.startsWith("TOUCHDBG=")) {
    uint32_t ms = static_cast<uint32_t>(c.substring(9).toInt());
    if (ms > 60000UL) ms = 60000UL;
    touch::debugFor(ms);
  } else if (c == "TOUCHCAL") {
    touch::calRun(10000);
  } else if (c.startsWith("TOUCHCAL=")) {
    uint32_t per = static_cast<uint32_t>(c.substring(9).toInt());
    if (per < 3000UL) per = 3000UL;
    if (per > 20000UL) per = 20000UL;
    touch::calRun(per);
  } else if (c.startsWith("TOUCHSETCAL=")) {
    // TOUCHSETCAL=x0,x1,y0,y1[,swap]
    const String a = c.substring(12);
    int idx[4]; idx[0] = a.indexOf(',');
    for (int i = 1; i < 4; ++i) idx[i] = a.indexOf(',', idx[i - 1] + 1);
    if (idx[3] < 0) {
      Serial.println(F("[cmd] TOUCHSETCAL=x0,x1,y0,y1[,swap]"));
      return;
    }
    const int32_t x0 = a.substring(0, idx[0]).toInt();
    const int32_t x1 = a.substring(idx[0] + 1, idx[1]).toInt();
    const int32_t y0 = a.substring(idx[1] + 1, idx[2]).toInt();
    const int32_t y1 = a.substring(idx[2] + 1, idx[3]).toInt();
    const String tail = a.substring(idx[3] + 1);
    touch::setCal(x0, x1, y0, y1, tail.toInt() != 0);
  } else if (c == "GPIOSCAN") {
    touch::gpioScan(10000);
  } else if (c.startsWith("GPIOSCAN=")) {
    uint32_t ms = static_cast<uint32_t>(c.substring(9).toInt());
    if (ms > 30000UL) ms = 30000UL;
    touch::gpioScan(ms);
  } else if (c == "LGFXTOUCH" || c.startsWith("LGFXTOUCH=")) {
    int8_t cs = -2, ip = -2;
    if (c.startsWith("LGFXTOUCH=")) {
      const String a = c.substring(10);
      const int comma = a.indexOf(',');
      if (comma < 0) {
        cs = static_cast<int8_t>(a.toInt());
      } else {
        cs = static_cast<int8_t>(a.substring(0, comma).toInt());
        ip = static_cast<int8_t>(a.substring(comma + 1).toInt());
      }
    }
    const bool ok = display::touchInit(cs, ip);
    Serial.printf("[lgfxtouch] init=%s cs=%d intPin=%d (shared HSPI)\n",
                  ok ? "OK" : "FAILED",
                  static_cast<int>(display::touchCsPin()),
                  static_cast<int>(display::touchIntPin()));
  } else if (c == "LGFXSCAN" || c.startsWith("LGFXSCAN=")) {
    uint32_t ms = (c == "LGFXSCAN") ? 15000UL
                                    : static_cast<uint32_t>(c.substring(9).toInt());
    if (ms > 30000UL) ms = 30000UL;
    Serial.printf("[lgfxscan] %lu ms - PRESS THE PANEL NOW\n",
                  static_cast<unsigned long>(ms));
    const uint32_t t0 = millis();
    uint32_t hits = 0, lastPrint = 0;
    uint16_t px = 0xFFFF, py = 0xFFFF;
    while (millis() - t0 < ms) {
      uint16_t rx = 0, ry = 0, gx = 0, gy = 0;
      const bool rawOk = display::touchRaw(&rx, &ry);
      const bool pressed = display::touchRead(&gx, &gy);
      if (pressed) ++hits;
      if (rawOk && (rx != px || ry != py || millis() - lastPrint > 500)) {
        Serial.printf("[lgfxraw] t=%lu raw=%u,%u pressed=%d gx=%u gy=%u\n",
                      static_cast<unsigned long>(millis() - t0), rx, ry,
                      pressed ? 1 : 0, gx, gy);
        px = rx; py = ry; lastPrint = millis();
      }
      delay(20);
    }
    Serial.printf("[lgfxscan] DONE hits=%lu\n", static_cast<unsigned long>(hits));
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
  touch::begin();           // logs; bus pins configured by autoDetect()
  pinMode(BTN_PIN, INPUT_PULLUP);     // BOOT button = nav fallback
  touch::autoDetect();      // enable CYD XPT2046 on its dedicated bus
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
  touch::tick(now, onTouchTap);   // PENIRQ-gated CYD taps (enabled at boot)
  handleButton(now);              // BOOT button nav fallback + unlock
  // Auto-lock: OWNER idle too long -> back to GUEST (privacy).
  if (uiBridge::owner() && (now - s_lastInteraction > AUTOLOCK_MS)) {
    uiBridge::setPrivacy(false, false);
    ui::showPage(Page::GUEST);
    Serial.println(F("[auto-lock] OWNER idle -> GUEST"));
  }
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
