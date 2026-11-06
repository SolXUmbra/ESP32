// Time module - independent of network and UI.
// Stage 1: runs honestly in an UNSYNCED state (shows --:--, never a fake
// clock). Stage 3 feeds it a real NTP epoch through syncFromEpoch().
// Once synced it keeps ticking from the internal RTC, so losing Wi-Fi does
// not stop the clock. Uses Beijing time UTC+8, seconds are NOT displayed.
#pragma once

#include <Arduino.h>

namespace sunisle {
namespace timeService {

void begin();
bool isSynced();
uint32_t epoch();                 // 0 when unsynced
void syncFromEpoch(uint32_t e);   // stage 3 hook (NTP)
void tick();                      // refresh the shared Snapshot strings
void dump();

}  // namespace timeService
}  // namespace sunisle
