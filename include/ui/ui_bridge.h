// Adapter between SUNISLE services and the B-version memo UI.
// This file owns NO layout: it only feeds real data into ui_set_*.
// All calls must come from the LVGL thread (core-1 loop).
#pragma once

namespace sunisle {
namespace uiBridge {

void begin();   // ui_init() + Chinese font hand-off, once after LVGL is ready
void tick();    // push time / weather / memo list / alert state once per second

}  // namespace uiBridge
}  // namespace sunisle
