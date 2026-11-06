#include "sunisle/state.h"
#include "sunisle/config.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>

namespace sunisle {
namespace {

SemaphoreHandle_t s_mux = nullptr;
Snapshot          s_state;

}  // namespace

const char* stateName(SystemState s) {
  switch (s) {
    case SystemState::BOOT:            return "BOOT";
    case SystemState::LOCAL_INIT:      return "LOCAL_INIT";
    case SystemState::HOME_READY:      return "HOME_READY";
    case SystemState::WIFI_CONNECTING: return "WIFI_CONNECTING";
    case SystemState::SYNCING:         return "SYNCING";
    case SystemState::WEATHER_UPDATING:return "WEATHER_UPDATING";
    case SystemState::OFFLINE:         return "OFFLINE";
    case SystemState::ERROR_RECOVERY:  return "ERROR_RECOVERY";
    default:                           return "UNKNOWN";
  }
}

void stateInit() {
  memset(&s_state, 0, sizeof(s_state));
  s_state.state = SystemState::BOOT;
  s_state.page  = Page::HOME;
  s_state.brightness   = 220;
  s_state.slideshowMs  = SLIDESHOW_DEFAULT_MS;
  strcpy(s_state.timeHHMM, "--:--");
  strcpy(s_state.dateLine, "----.--.--");
  strcpy(s_state.weekday,  "---");
  strcpy(s_state.city,     "未配置");
  strcpy(s_state.weatherLine, "天气未配置");
  // Honest stage-1 status: no network, no NTP, no weather, no photos yet.
  strcpy(s_state.statusLine, "陪伴模式 / 网络未连接");
  if (!s_mux) s_mux = xSemaphoreCreateMutex();
}

Snapshot snapshot() {
  Snapshot out;
  memset(&out, 0, sizeof(out));
  if (s_mux && xSemaphoreTake(s_mux, pdMS_TO_TICKS(20)) == pdTRUE) {
    out = s_state;
    xSemaphoreGive(s_mux);
  }
  return out;
}

void publish(const Snapshot& s) {
  if (s_mux && xSemaphoreTake(s_mux, pdMS_TO_TICKS(20)) == pdTRUE) {
    s_state = s;
    xSemaphoreGive(s_mux);
  }
}

void setState(SystemState s) {
  if (s_mux && xSemaphoreTake(s_mux, pdMS_TO_TICKS(20)) == pdTRUE) {
    s_state.state = s;
    xSemaphoreGive(s_mux);
  }
}

void setPage(Page p) {
  if (s_mux && xSemaphoreTake(s_mux, pdMS_TO_TICKS(20)) == pdTRUE) {
    s_state.page = p;
    xSemaphoreGive(s_mux);
  }
}

}  // namespace sunisle
