#include "services/time_service.h"

#include "sunisle/config.h"
#include "sunisle/state.h"
#include "utils/logger.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

namespace sunisle {
namespace timeService {
namespace {

// Survives a software restart (ESP.restart) but not a real power loss.
// RTC_DATA_ATTR would be zeroed on every restart - that was a real bug in
// the audited project, so RTC_NOINIT_ATTR + magic is mandatory here.
struct RtcTime {
  uint32_t magic;
  uint32_t epochBase;
  uint32_t millisBase;
};
constexpr uint32_t RTC_MAGIC = 0x51A15A01u;
RTC_NOINIT_ATTR RtcTime s_rtc;

const char* kWday[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
const char* kWdayCn[7] = {"日", "一", "二", "三", "四", "五", "六"};
bool s_synced = false;

}  // namespace

void begin() {
  // Beijing time, UTC+8, no DST. No network access in stage 1.
  setenv("TZ", "CST-8", 1);
  tzset();
  if (s_rtc.magic == RTC_MAGIC && s_rtc.epochBase > 1700000000UL) {
    s_synced = true;
    SUN_LOG("time", "restored RTC time base epoch=%lu",
            static_cast<unsigned long>(s_rtc.epochBase));
  } else {
    memset(&s_rtc, 0, sizeof(s_rtc));
    s_rtc.magic = RTC_MAGIC;
    s_synced = false;
    SUN_LOG("time", "UNSYNCED - showing --:-- until a real NTP sync (stage 3)");
  }
}

bool isSynced() { return s_synced; }

uint32_t epoch() {
  if (!s_synced) return 0;
  return s_rtc.epochBase + (millis() - s_rtc.millisBase) / 1000UL;
}

void syncFromEpoch(uint32_t e) {
  if (e < 1700000000UL) return;
  s_rtc.epochBase  = e;
  s_rtc.millisBase = millis();
  s_rtc.magic      = RTC_MAGIC;
  s_synced = true;
  SUN_LOG("time", "SYNCED epoch=%lu", static_cast<unsigned long>(e));
  Snapshot s = snapshot();
  s.timeSynced = true;
  publish(s);
}

void tick() {
  Snapshot s = snapshot();
  s.timeSynced = s_synced;
  if (s_synced) {
    time_t raw = static_cast<time_t>(epoch());
    struct tm* lt = localtime(&raw);
    if (lt) {
      strftime(s.timeHHMM, sizeof(s.timeHHMM), "%H:%M", lt);
      strftime(s.dateLine, sizeof(s.dateLine), "%Y.%m.%d", lt);
      // Chinese header line for the delivered UI (needs a CJK subset font).
      snprintf(s.dateCn, sizeof(s.dateCn), "%d月%02d日",
               lt->tm_mon + 1, lt->tm_mday);
      int wd = (lt->tm_wday >= 0 && lt->tm_wday < 7) ? lt->tm_wday : 0;
      strncpy(s.weekday, kWday[wd], sizeof(s.weekday) - 1);
      s.weekday[sizeof(s.weekday) - 1] = 0;
      snprintf(s.weekdayCn, sizeof(s.weekdayCn), "星期%s", kWdayCn[wd]);
    }
  } else {
    strcpy(s.timeHHMM, "--:--");
    strcpy(s.dateLine, "----.--.--");
    strcpy(s.weekday, "---");
    strcpy(s.dateCn, "未校时");
    strcpy(s.weekdayCn, "");
  }
  publish(s);
}

void dump() {
  SUN_LOG("time", "synced=%s epoch=%lu hhmm=%s date=%s",
          s_synced ? "YES" : "NO", static_cast<unsigned long>(epoch()),
          snapshot().timeHHMM, snapshot().dateLine);
}

}  // namespace timeService
}  // namespace sunisle
