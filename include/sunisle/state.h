// Shared application state.
// LVGL is NOT thread-safe. Any future network/sync task (stage 3+) writes a
// plain Snapshot under a mutex; the UI loop on core 1 only ever reads a copy.
#pragma once

#include <Arduino.h>

namespace sunisle {

// Task book section 9: explicit, observable states.
enum class SystemState : uint8_t {
  BOOT,
  LOCAL_INIT,
  HOME_READY,
  WIFI_CONNECTING,   // stage 3+
  SYNCING,           // stage 4+
  WEATHER_UPDATING,  // stage 3+
  OFFLINE,
  ERROR_RECOVERY,
};

enum class Page : uint8_t { HOME = 0, PHOTOS = 1, SETTINGS = 2 };

struct Snapshot {
  SystemState state;
  Page        page;
  bool        sdReady;
  bool        timeSynced;        // stage 3 NTP; false in stage 1
  bool        wifiOn;            // COMPANION mode keeps this false
  bool        weatherConfigured; // false until the user fills city/lat/lon
  bool        weatherValid;      // true only after a REAL successful fetch
  bool        weatherCached;     // last-good data, explicitly labelled
  uint16_t    photoCount;        // real files found on SD
  uint32_t    lastWeatherEpoch;
  uint8_t     brightness;
  uint32_t    slideshowMs;
  int16_t     tempC;             // REAL Open-Meteo temperature, deg C
  uint8_t     wmoCode;           // REAL WMO weather code (255 = unknown)
  char timeHHMM[8];
  char dateLine[24];             // 2026.11.06
  char weekday[8];               // FRI
  char dateCn[24];               // 11月06日
  char weekdayCn[10];            // 星期五
  char city[24];                 // 渑池
  char weatherLine[36];          // 晴 / 多云 / 小雨 ...
  char weatherDesc[24];          // same as weatherLine, Chinese, UI-sized
  char statusLine[48];
  char quote[48];                // daily quote ("每日一言"), rotated by date
};

void     stateInit();
Snapshot snapshot();             // mutex-protected copy
void     publish(const Snapshot& s);
void     setState(SystemState s);
void     setPage(Page p);
const char* stateName(SystemState s);

}  // namespace sunisle
