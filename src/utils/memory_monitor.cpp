#include "utils/memory_monitor.h"

#include <limits.h>

namespace sunisle {
namespace memory {
namespace {

uint32_t s_min = ULONG_MAX;

}  // namespace

void sample() {
  uint32_t h = ESP.getFreeHeap();
  if (h < s_min) s_min = h;
}

uint32_t currentHeap() { return ESP.getFreeHeap(); }

uint32_t minHeap() {
  return s_min == ULONG_MAX ? ESP.getFreeHeap() : s_min;
}

void resetMin() { s_min = ULONG_MAX; }

void dump() {
  Serial.printf("[mem] heap=%u B min=%u B maxAlloc=%u B psram=%u B\n",
                static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(minHeap()),
                static_cast<unsigned>(ESP.getMaxAllocHeap()),
                static_cast<unsigned>(ESP.getPsramSize()));
}

}  // namespace memory
}  // namespace sunisle
