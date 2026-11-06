#include "services/photo_service.h"

#include "sunisle/config.h"
#include "sunisle/state.h"
#include "hardware/storage_manager.h"
#include "utils/logger.h"

namespace sunisle {
namespace photoService {
namespace {

uint16_t s_cursor = 0;
uint32_t s_lastSwitch = 0;
char     s_status[48];

void refreshStatus() {
  uint16_t n = storage::photoCount();
  if (!storage::isReady()) {
    snprintf(s_status, sizeof(s_status), "存储卡未就绪");
  } else if (n == 0) {
    snprintf(s_status, sizeof(s_status), "暂无照片 / 等待首张照片");
  } else {
    snprintf(s_status, sizeof(s_status), "第 %u / %u 张", s_cursor + 1, n);
  }
}

}  // namespace

void begin() {
  s_cursor = 0;
  s_lastSwitch = millis();
  refreshStatus();
  Snapshot s = snapshot();
  s.photoCount = storage::photoCount();
  publish(s);
  SUN_LOG("photo", "stage 1: %u real JPEG(s), decoder arrives in stage 2", s.photoCount);
}

uint16_t count() { return storage::photoCount(); }
uint16_t cursor() { return count() ? s_cursor : 0; }

const char* currentPath() {
  return count() ? storage::photoPath(s_cursor) : nullptr;
}

void next() {
  uint16_t n = count();
  if (n == 0) return;
  s_cursor = static_cast<uint16_t>((s_cursor + 1) % n);
  s_lastSwitch = millis();
  refreshStatus();
  SUN_LOG("photo", "-> %u/%u %s", s_cursor + 1, n, currentPath());
}

void advanceIfDue() {
  if (count() < 2) return;
  if (millis() - s_lastSwitch >= snapshot().slideshowMs) next();
}

void rescan() {
  storage::rescanPhotos();
  if (s_cursor >= count()) s_cursor = 0;
  refreshStatus();
  Snapshot s = snapshot();
  s.photoCount = count();
  publish(s);
}

const char* statusLine() { return s_status; }

void dump() {
  SUN_LOG("photo", "count=%u cursor=%u interval=%lus status=%s", count(),
          s_cursor, static_cast<unsigned long>(snapshot().slideshowMs / 1000),
          s_status);
}

}  // namespace photoService
}  // namespace sunisle
