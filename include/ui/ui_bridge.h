// Adapter between the SUNISLE services / LovyanGFX TJpgD and the delivered
// vendor UI (ui_sunisle.cpp). This file owns NO layout: it only feeds real
// data into ui_set_* and builds an lv_img_dsc_t for ui_show_photo().
//
// All functions must be called from the LVGL thread (core-1 loop).
#pragma once

namespace sunisle {
namespace uiBridge {

void begin();          // calls ui_init() once, after LVGL + display are ready
void tick();           // push real time/weather/city/photo state once per second
const char* photoMode();  // "NONE" | "STREAMING" | "FAILED" for diagnostics

}  // namespace uiBridge
}  // namespace sunisle
