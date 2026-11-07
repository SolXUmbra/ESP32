// Shared application state.
// LVGL is NOT thread-safe. Any future network/sync task writes a plain
// Snapshot under a mutex; the UI loop only ever reads a copy.
#pragma once

#include <Arduino.h>

namespace sunisle {

// Task book section 9: explicit, observable states.
enum class SystemState : uint8_t {
  BOOT,
  LOCAL_INIT,
  HOME_READY,
  WIFI_CONNECTING,
  SYNCING,
  WEATHER_UPDATING,
  OFFLINE,
  ERROR_RECOVERY,
};

enum class Page : uint8_t { HOME = 0, MEMOS = 1, SETTINGS = 2 };

struct Snapshot {
  SystemState state;
  Page        page;
  bool        sdReady;
  bool        timeSynced;
  bool        wifiOn;
  bool        weatherConfigured;
  bool        weatherValid;
  bool        weatherCached;
  uint16_t    memoCount;         // active memos (soft-deleted excluded)
  uint16_t    dueCount;          // memos currently due / reminding
  uint16_t    taskCount;         // composite tasks
  uint32_t    lastWeatherEpoch;
  uint8_t     brightness;
  int16_t     tempC;
  uint8_t     wmoCode;
  char timeHHMM[8];
  char dateLine[24];
  char weekday[8];
  char dateCn[24];
  char weekdayCn[10];
  char city[24];
  char weatherLine[36];
  char weatherDesc[24];
  char statusLine[48];
  char quote[48];
  char activeMemo[48];           // current on-screen reminder, "" when idle
};

void     stateInit();
Snapshot snapshot();             // mutex-protected copy
void     publish(const Snapshot& s);
void     setState(SystemState s);
void     setPage(Page p);
const char* stateName(SystemState s);

}  // namespace sunisle
