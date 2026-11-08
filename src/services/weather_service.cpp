// SUNISLE weather service - REAL data only.
//
// Pipeline (runs on a core-0 task, never on the LVGL thread):
//   Wi-Fi (SYNC window) -> NTP -> Open-Meteo /v1/forecast -> publish Snapshot
//   -> radio OFF again (COMPANION mode: the frame is offline by default).
//
// Honesty rules that this file obeys:
//   * weatherValid becomes true ONLY after a real HTTP 200 body with a real
//     numeric temperature parsed out of `current`.
//   * Failure keeps the previous value but marks it weatherCached=true, which
//     the UI renders as "离线". Nothing is invented.
//   * No HTTPClient / WiFiClientSecure: that pulls mbedTLS into the image
//     (~1.4 MB > the 1.28 MB app slot) even for plain http://. A raw
//     WiFiClient with HTTP/1.0 + Connection: close returns a non-chunked body
//     and keeps the firmware inside its partition.
#include "services/weather_service.h"

#include "sunisle/config.h"
#include "sunisle/state.h"
#include "hardware/storage_manager.h"
#include "services/time_service.h"
#include "utils/logger.h"

#include <WiFi.h>
#include <WiFiMulti.h>
#include <SD.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "secrets.h"

namespace sunisle {
namespace weatherService {
namespace {

char     s_status[64] = "天气未配置";
bool     s_started = false;
uint32_t s_okEpoch  = 0;      // last REAL successful fetch
uint32_t s_failCount = 0;

constexpr uint32_t kRefreshMs   = 30UL * 60UL * 1000UL;  // 30 min
constexpr uint32_t kRetryMs     = 5UL * 60UL * 1000UL;   // 5 min
constexpr uint32_t kWifiWaitMs  = 30000;
constexpr uint32_t kNtpWaitMs   = 12000;
constexpr uint8_t  WMO_UNKNOWN  = 255;

// ---- WMO 4677 -> Chinese wording -----------------------------------------
// Chars covered by tools/gen_cn_font.py (WEATHER_STRINGS). Adding a case here
// REQUIRES adding the glyphs there, otherwise the label renders as boxes.
const char* wmoCn(uint8_t code) {
  switch (code) {
    case 0:              return "晴";
    case 1:              return "晴间多云";
    case 2:              return "多云";
    case 3:              return "阴";
    case 45: case 48:    return "雾";
    case 51: case 53:
    case 55:             return "毛毛雨";
    case 56: case 57:
    case 66: case 67:    return "冻雨";
    case 61:             return "小雨";
    case 63:             return "中雨";
    case 65:             return "大雨";
    case 71:             return "小雪";
    case 73:             return "中雪";
    case 75:             return "大雪";
    case 77:             return "雪粒";
    case 80: case 81:
    case 82:             return "阵雨";
    case 85: case 86:    return "阵雪";
    case 95:             return "雷阵雨";
    case 96: case 99:    return "雷暴冰雹";
    default:             return "未知";
  }
}

const char* findAfter(const char* hay, const char* key) {
  char pat[40];
  snprintf(pat, sizeof(pat), "\"%s\":", key);
  const char* p = strstr(hay, pat);
  return p ? p + strlen(pat) : nullptr;
}

long jsonLong(const String& s, const char* key, long dflt) {
  String pat = String("\"") + key + "\":";
  int i = s.indexOf(pat);
  if (i < 0) return dflt;
  return strtol(s.c_str() + i + pat.length(), nullptr, 10);
}

// ---- last-good cache: /SUNISLE/weather/cache.json -------------------------
void loadCache() {
  if (!storage::isReady()) return;
  String txt;
  if (!storage::readText(FILE_WEATHER, txt) || txt.length() == 0) return;
  long temp = jsonLong(txt, "temp", 999);
  long wmo  = jsonLong(txt, "wmo", -1);
  long ep   = jsonLong(txt, "epoch", 0);
  if (temp == 999 || wmo < 0 || wmo > 99) return;

  Snapshot s = snapshot();
  s.tempC          = static_cast<int16_t>(temp);
  s.wmoCode        = static_cast<uint8_t>(wmo);
  s.weatherValid   = true;      // it WAS real when it was fetched
  s.weatherCached  = true;      // ... but it is not live now
  s.lastWeatherEpoch = static_cast<uint32_t>(ep);
  strncpy(s.weatherLine, wmoCn(static_cast<uint8_t>(wmo)),
          sizeof(s.weatherLine) - 1);
  strncpy(s.weatherDesc, wmoCn(static_cast<uint8_t>(wmo)),
          sizeof(s.weatherDesc) - 1);
  publish(s);
  SUN_LOG("weather", "cache restored: %d C wmo=%ld (%s, offline)",
          static_cast<int>(temp), wmo, wmoCn(static_cast<uint8_t>(wmo)));
}

void saveCache(int16_t temp, uint8_t wmo, uint32_t epoch) {
  if (!storage::isReady()) return;
  char buf[96];
  snprintf(buf, sizeof(buf),
           "{\"temp\":%d,\"wmo\":%u,\"epoch\":%lu,\"city\":\"%s\"}\n",
           static_cast<int>(temp), static_cast<unsigned>(wmo),
           static_cast<unsigned long>(epoch), SUNISLE_CITY_NAME);
  if (!storage::atomicReplace(FILE_WEATHER, buf))
    SUN_LOG("weather", "cache write FAILED (kept live value in RAM)");
}

void publishWeather(int16_t temp, uint8_t wmo, bool cached) {
  Snapshot s = snapshot();
  s.weatherConfigured = true;
  s.weatherValid      = true;
  s.weatherCached     = cached;
  s.tempC             = temp;
  s.wmoCode           = wmo;
  s.lastWeatherEpoch  = s_okEpoch;
  strncpy(s.city, SUNISLE_CITY_NAME, sizeof(s.city) - 1);
  s.city[sizeof(s.city) - 1] = 0;
  strncpy(s.weatherLine, wmoCn(wmo), sizeof(s.weatherLine) - 1);
  strncpy(s.weatherDesc, wmoCn(wmo), sizeof(s.weatherDesc) - 1);
  publish(s);

  snprintf(s_status, sizeof(s_status), "%s %d° / %s",
           cached ? "离线数据" : "实时", static_cast<int>(temp), wmoCn(wmo));
  SUN_LOG("weather", "%s temp=%d C wmo=%u -> %s",
          cached ? "CACHED" : "LIVE", static_cast<int>(temp),
          static_cast<unsigned>(wmo), wmoCn(wmo));
}

// ---- Open-Meteo over plain HTTP/1.0 --------------------------------------
bool fetchOpenMeteo(int16_t& tempOut, uint8_t& wmoOut) {
  WiFiClient c;
  c.setTimeout(8);

  if (!c.connect("api.open-meteo.com", 80)) {
    SUN_LOG("weather", "connect api.open-meteo.com:80 FAILED");
    return false;
  }

  char req[256];
  snprintf(req, sizeof(req),
           "GET /v1/forecast?latitude=%s&longitude=%s"
           "&current=temperature_2m,relative_humidity_2m,weather_code"
           "&timezone=Asia%%2FShanghai HTTP/1.0\r\n"
           "Host: api.open-meteo.com\r\n"
           "Connection: close\r\n\r\n",
           SUNISLE_CITY_LAT, SUNISLE_CITY_LON);
  c.print(req);

  static char buf[1600];
  size_t n = 0;
  uint32_t t0 = millis();
  while (n < sizeof(buf) - 1 && millis() - t0 < 10000) {
    if (c.available()) {
      buf[n++] = static_cast<char>(c.read());
      t0 = millis();
    } else if (!c.connected()) {
      break;
    } else {
      delay(10);
    }
  }
  buf[n] = 0;
  c.stop();

  const char* body = strstr(buf, "\r\n\r\n");
  if (!body) {
    SUN_LOG("weather", "no HTTP body (%u B)", static_cast<unsigned>(n));
    return false;
  }
  body += 4;
  if (!strstr(buf, "200 OK")) {
    SUN_LOG("weather", "HTTP status not 200: %.24s", buf);
    return false;
  }

  // Only search inside `current` so "temperature_2m" from current_units
  // ("°C") can never be mistaken for the number.
  const char* cur = strstr(body, "\"current\":");
  const char* scope = cur ? cur : body;

  const char* tp = findAfter(scope, "temperature_2m");
  const char* wp = findAfter(scope, "weather_code");
  if (!tp || !wp) {
    SUN_LOG("weather", "JSON missing current.temperature_2m/weather_code");
    return false;
  }

  const float t = strtof(tp, nullptr);
  const int   w = atoi(wp);
  if (t < -60.0f || t > 60.0f || w < 0 || w > 99) {
    SUN_LOG("weather", "implausible values t=%.1f wmo=%d", t, w);
    return false;
  }

  tempOut = static_cast<int16_t>(lroundf(t));
  wmoOut  = static_cast<uint8_t>(w);
  SUN_LOG("weather", "HTTP 200, %u B body, current.temperature_2m=%.1f "
                     "weather_code=%d", static_cast<unsigned>(n), t, w);
  return true;
}

bool syncClock() {
  configTime(8 * 3600, 0, "ntp.aliyun.com", "cn.pool.ntp.org",
             "pool.ntp.org");
  const uint32_t t0 = millis();
  while (millis() - t0 < kNtpWaitMs) {
    const time_t now = time(nullptr);
    if (now > 1700000000) {
      timeService::syncFromEpoch(static_cast<uint32_t>(now));
      return true;
    }
    delay(200);
  }
  SUN_LOG("time", "NTP timeout (%lu ms) - clock stays unsynced",
          static_cast<unsigned long>(kNtpWaitMs));
  return false;
}

void radioOff() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Snapshot s = snapshot();
  s.wifiOn = false;
  publish(s);
  SUN_LOG("net", "radio OFF -> COMPANION mode (offline frame)");
}

// One full sync window. Always leaves the radio off.
void netCycle() {
  Snapshot s = snapshot();
  s.wifiOn = true;
  publish(s);
  setState(SystemState::WIFI_CONNECTING);

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);

  // 支持多个 Wi-Fi（主网络 + 备用网络），设备依次尝试直到连上其中一个
  WiFiMulti wifiMulti;
  if (SUNISLE_WIFI_SSID[0])  wifiMulti.addAP(SUNISLE_WIFI_SSID, SUNISLE_WIFI_PASSWORD);
#ifdef SUNISLE_WIFI_SSID_2
  if (SUNISLE_WIFI_SSID_2[0])  wifiMulti.addAP(SUNISLE_WIFI_SSID_2, SUNISLE_WIFI_PASSWORD_2);
#endif
  WiFi.setSleep(true);

  const uint32_t t0 = millis();
  while (wifiMulti.run() != WL_CONNECTED && millis() - t0 < kWifiWaitMs)
    delay(250);

  if (WiFi.status() != WL_CONNECTED) {
    SUN_LOG("net", "Wi-Fi connect FAILED within %lu ms (status=%d)",
            static_cast<unsigned long>(kWifiWaitMs),
            static_cast<int>(WiFi.status()));
    radioOff();
    setState(SystemState::OFFLINE);
    s_failCount++;
    return;
  }
  SUN_LOG("net", "Wi-Fi up: ssid=%s ip=%s rssi=%d dBm",
          WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(),
          static_cast<int>(WiFi.RSSI()));

  syncClock();
  setState(SystemState::WEATHER_UPDATING);

  int16_t temp = 0;
  uint8_t wmo = WMO_UNKNOWN;
  if (fetchOpenMeteo(temp, wmo)) {
    s_okEpoch = timeService::isSynced() ? timeService::epoch() : millis() / 1000;
    publishWeather(temp, wmo, false);
    saveCache(temp, wmo, s_okEpoch);
    s_failCount = 0;
    setState(SystemState::HOME_READY);
  } else {
    s_failCount++;
    SUN_LOG("weather", "fetch FAILED (%lu in a row) - keeping cached value",
            static_cast<unsigned long>(s_failCount));
    if (!snapshot().weatherValid) {
      s_status[0] = 0;
      strcpy(s_status, "天气获取失败");
      Snapshot f = snapshot();
      strcpy(f.weatherLine, "获取失败");
      strcpy(f.weatherDesc, "获取失败");
      publish(f);
    }
    setState(SystemState::OFFLINE);
  }

  radioOff();
}

void netTask(void*) {
  vTaskDelay(pdMS_TO_TICKS(2500));   // let the UI finish its first frame
  for (;;) {
    netCycle();
    const uint32_t wait = s_failCount ? kRetryMs : kRefreshMs;
    SUN_LOG("net", "next sync window in %lu min",
            static_cast<unsigned long>(wait / 60000UL));
    vTaskDelay(pdMS_TO_TICKS(wait));
  }
}

}  // namespace

void begin() {
  const bool haveCreds = (SUNISLE_WIFI_SSID[0] != 0)
#ifdef SUNISLE_WIFI_SSID_2
                      || (SUNISLE_WIFI_SSID_2[0] != 0)
#endif
                      ;
  const bool havePlace = SUNISLE_CITY_LAT[0] != 0 && SUNISLE_CITY_LON[0] != 0;

  Snapshot s = snapshot();
  s.weatherConfigured = havePlace;
  s.weatherValid   = false;
  s.weatherCached  = false;
  s.tempC          = 0;
  s.wmoCode        = WMO_UNKNOWN;
  s.lastWeatherEpoch = 0;
  if (havePlace) {
    strncpy(s.city, SUNISLE_CITY_NAME, sizeof(s.city) - 1);
    strcpy(s.weatherLine, "更新中");
  } else {
    strcpy(s.city, "未配置");
    strcpy(s.weatherLine, "天气未配置");
  }
  publish(s);

  if (!havePlace) {
    strcpy(s_status, "天气未配置");
    SUN_LOG("weather", "NO location configured -> no request is made");
    return;
  }

  loadCache();   // last REAL value first, explicitly labelled 离线

  SUN_LOG("weather", "location=%s lat=%s lon=%s (Open-Meteo, plain HTTP)",
          SUNISLE_CITY_NAME, SUNISLE_CITY_LAT, SUNISLE_CITY_LON);

  if (!haveCreds) {
    strcpy(s_status, "无网络配置");
    SUN_LOG("net", "no Wi-Fi credentials in include/secrets.h -> offline only");
    return;
  }

  s_started = true;
  xTaskCreatePinnedToCore(netTask, "sunisle_net", 8192, nullptr, 1, nullptr, 0);
  SUN_LOG("net", "network task started on core 0 (LVGL stays on core 1)");
}

void tick() {
  Snapshot s = snapshot();
  if (!s.weatherConfigured) {
    strcpy(s_status, "天气未配置");
    return;
  }
  if (s.weatherValid) {
    snprintf(s_status, sizeof(s_status), "%s %d° / %s",
             s.weatherCached ? "离线数据" : "实时", static_cast<int>(s.tempC),
             wmoCn(s.wmoCode));
  } else if (s_started) {
    strcpy(s_status, "天气更新中");
  }
}

bool isConfigured() { return snapshot().weatherConfigured; }
bool hasRealData() { return snapshot().weatherValid; }
const char* statusLine() { return s_status; }

void dump() {
  Snapshot s = snapshot();
  SUN_LOG("weather", "configured=%s valid=%s cached=%s temp=%d wmo=%u "
                     "line=%s status=%s",
          s.weatherConfigured ? "YES" : "NO", s.weatherValid ? "YES" : "NO",
          s.weatherCached ? "YES" : "NO", static_cast<int>(s.tempC),
          static_cast<unsigned>(s.wmoCode), s.weatherLine, s_status);
}

}  // namespace weatherService
}  // namespace sunisle
