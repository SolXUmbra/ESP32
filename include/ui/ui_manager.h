// UI orchestration. LVGL is touched ONLY from the core-1 loop.
#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include "sunisle/state.h"

namespace sunisle {
namespace ui {

void begin();
void tickFrame();          // every LVGL loop iteration: boot fade / transition
void tick1s();             // refresh labels of the active page
void showPage(Page p);
Page currentPage();

// True when the panel mirrors a LOCKED Windows desktop (GUEST, taps dropped).
bool sessionLocked();
bool bootActive();

}  // namespace ui
}  // namespace sunisle
