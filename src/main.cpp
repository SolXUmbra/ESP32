// 晴屿 SUNISLE - stage 1 entry point.
// Orchestration ONLY. All real work lives in the hardware/ services/ ui/
// modules. LVGL is touched exclusively from this file's loop() / ui_manager.
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
#include "services/photo_service.h"
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

void printBanner() {
  Serial.println();
  Serial.println(F("====================================="));
  Serial.println(F("   晴屿 SUNISLE  /  STAGE 1"));
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
    Serial.println(F("HELP | STATE | MEM | SD | TIME | WX | PAGE=home|photos|settings"));
    Serial.println(F("BL=<0..255> | INTERVAL=<seconds> | RESCAN | NEXT | PHOTO=ON|OFF"));
    Serial.println(F("DEL /SUNISLE/photos/NAME.JPG  (remove one photo before re-uploading)"));
    Serial.println(F("DIAG (RAW GFX, LVGL bypassed) | UI (resume) | REBOOT"));
    Serial.println(F("SDPROBE | SDPROBE=FULL (read-only SD bus matrix)"));
    Serial.println(F("PUT /SUNISLE/photos/NAME.JPG <bytes>  (raw bytes follow)"));
  } else if (c.startsWith("PUT ")) {
    // The microSD lives inside the frame and this PC has no card reader, so
    // the device itself is the writer. Protocol:
    //   host -> "PUT /SUNISLE/photos/x.jpg 12345\n"
    //   dev  -> "[put] READY 12345 /SUNISLE/photos/x.jpg"
    //   host -> exactly 12345 raw bytes
    //   dev  -> "[put] OK ..." / "[put] FAIL ..."
    // LVGL pauses for the length of the transfer (a few seconds for a photo);
    // the UI resumes on the next loop and the new file is rescanned at once.
    String t = raw;
    t.trim();
    const int sp = t.indexOf(' ', 4);
    if (sp < 0) {
      Serial.println(F("[put] usage: PUT /SUNISLE/photos/name.jpg <bytes>"));
      return;
    }
    String path = t.substring(4, sp);
    const uint32_t size = static_cast<uint32_t>(t.substring(sp + 1).toInt());

    if (!storage::isReady()) {
      Serial.println(F("[put] ERR SD not ready"));
      return;
    }
    if (!path.startsWith("/SUNISLE/photos/") || path.length() < 20) {
      Serial.println(F("[put] ERR path must be /SUNISLE/photos/<name>.jpg"));
      return;
    }
    if (size < 128 || size > 2UL * 1024UL * 1024UL) {
      Serial.println(F("[put] ERR size out of range"));
      return;
    }
    if (storage::fileExists(path.c_str())) {
      Serial.println(F("[put] ERR file exists (never overwritten)"));
      return;
    }

    while (Serial.available()) Serial.read();     // drop CR/LF remnants
    Serial.printf("[put] READY %lu %s\n", static_cast<unsigned long>(size),
                  path.c_str());
    Serial.flush();

    uint32_t written = 0;
    const bool ok = storage::writeFromStream(path.c_str(), Serial, size, written);
    Serial.printf("[put] %s %lu/%lu %s\n", ok ? "OK" : "FAIL",
                  static_cast<unsigned long>(written),
                  static_cast<unsigned long>(size), path.c_str());
    if (ok) {
      photoService::rescan();     // real rescan, no optimistic count
      ui::tick1s();               // decode + show immediately
    }
  } else if (c.startsWith("DEL ")) {
    // The PUT protocol never overwrites: to replace a photo, remove the old
    // one first, then PUT the same name. Storage layer enforces that only a
    // JPEG directly inside /SUNISLE/photos/ can be deleted.
    String t = raw;
    t.trim();
    String path = t.substring(4);
    path.trim();
    const bool ok = storage::deletePhoto(path.c_str());
    Serial.printf("[del] %s %s\n", ok ? "OK" : "FAILED", path.c_str());
    if (ok) photoService::rescan();   // keep cursor + path list consistent
  } else if (c == "WX") {
    weatherService::dump();
  } else if (c == "STATE") {
    Snapshot s = snapshot();
    Serial.printf("[state] state=%s page=%u sd=%s time=%s wifi=%s "
                  "weatherCfg=%s weatherValid=%s photos=%u\n",
                  stateName(s.state), static_cast<unsigned>(s.page),
                  s.sdReady ? "YES" : "NO",
                  s.timeSynced ? s.timeHHMM : "UNSYNCED",
                  s.wifiOn ? "ON" : "OFF",
                  s.weatherConfigured ? "YES" : "NO",
                  s.weatherValid ? "YES" : "NO", s.photoCount);
    Serial.printf("[state] date=%s %s city=%s weather=%s\n",
                  s.dateLine, s.weekday, s.city, s.weatherLine);
  } else if (c == "MEM") {
    memory::dump();
  } else if (c == "SD") {
    storage::dump();
  } else if (c == "TIME") {
    timeService::dump();
  } else if (c.startsWith("PAGE=")) {
    String p = c.substring(5);
    Page page = Page::HOME;
    if (p == "PHOTOS") page = Page::PHOTOS;
    else if (p == "SETTINGS") page = Page::SETTINGS;
    else if (p != "HOME") {
      Serial.println(F("[cmd] usage: PAGE=home|photos|settings"));
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
  } else if (c.startsWith("INTERVAL=")) {
    uint32_t sec = c.substring(9).toInt();
    if (sec < 3 || sec > 300) {
      Serial.println(F("[cmd] INTERVAL=<3..300 seconds>"));
      return;
    }
    Snapshot s = snapshot();
    s.slideshowMs = sec * 1000UL;
    publish(s);
    storage::saveSettings(s);
    Serial.printf("[cmd] slideshow interval=%lus\n",
                  static_cast<unsigned long>(sec));
    ui::tick1s();
  } else if (c == "RESCAN") {
    if (!storage::isReady()) storage::remount();
    photoService::rescan();
    ui::tick1s();
  } else if (c == "NEXT") {
    photoService::next();
    ui::tick1s();
  } else if (c.startsWith("PHOTO=")) {
    const bool on = !c.endsWith("OFF");
    uiBridge::setPhotoEnabled(on);
    Serial.printf("[cmd] photo display %s\n", on ? "ON" : "OFF");
    ui::tick1s();
  } else if (c == "SDPROBE" || c == "SDPROBE=FULL") {
    storage::runProbe(c == "SDPROBE=FULL");
  } else if (c == "DIAG") {
    // Raw LovyanGFX pattern: if this is visible but the UI is not, the fault
    // is in LVGL; if it is blank too, it is hardware/SPI/backlight.
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
    Serial.printf("[cmd] unknown: %s\n", c.c_str());
  }
}

}  // namespace

void setup() {
  // 8 KB RX buffer: a photo upload streams ~30 KB at 115200 while the network
  // task may be logging at the same time. With the default 256 B FIFO the
  // host overruns it and the transfer ends short (observed: 30361/32167 B).
  Serial.setRxBufferSize(8192);
  Serial.begin(115200);
  delay(300);
  printBanner();

  stateInit();
  memory::sample();

  // Diagnostic only: is SD I/O healthy BEFORE LovyanGFX claims the HSPI bus?
  // The audited project never tested a real FAT read, only cardSize().
  storage::preDisplayProbe();

  if (!display::begin()) {
    Serial.println(F("[boot] DISPLAY FAILED - serial-only diagnostic mode"));
    setState(SystemState::ERROR_RECOVERY);
    return;
  }
  touch::begin();           // intentionally HELD OFF, see touch_manager.h
  storage::begin();         // degrades gracefully when no card is present
  timeService::begin();
  photoService::begin();
  weatherService::begin();
  quoteService::begin();
  ui::begin();
  lv_refr_now(nullptr);     // first full frame

  Serial.println(F("[boot] HELP for commands"));
  Serial.printf("[boot] STAGE 1 READY heap=%u B flush=%u\n",
                static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(display::flushCount()));
  s_lastTick = s_lastHeartbeat = millis();
}

void loop() {
  if (display::isReady() && !s_diagMode) {
    lv_timer_handler();
    ui::tickFrame();          // boot fade / automatic transition to Home
  }
  memory::sample();

  const uint32_t now = millis();
  if (now - s_lastTick >= UI_TICK_MS) {
    s_lastTick = now;
    timeService::tick();
    weatherService::tick();
    quoteService::tick();      // re-pick if the local day rolled over
    if (!ui::bootActive()) photoService::advanceIfDue();
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
                  "sd=%s photos=%u flush=%u\n",
                  static_cast<unsigned long>(millis() / 1000),
                  static_cast<unsigned>(ESP.getFreeHeap() / 1024),
                  static_cast<unsigned>(memory::minHeap() / 1024),
                  stateName(s.state), static_cast<unsigned>(s.page),
                  s.sdReady ? "YES" : "NO", s.photoCount,
                  static_cast<unsigned>(display::flushCount()));
  }

  delay(LVGL_LOOP_MS);
}
