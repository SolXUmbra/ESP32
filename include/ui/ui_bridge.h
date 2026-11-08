// UI 2.0 adapter: real SUNISLE state -> the four UI 2.0 screens.
// No Bridge session exists yet (stage D), so task/memo/reminder content is
// either honest empty state or an explicitly labelled DEMO sample set.
// All functions must run on the LVGL thread (core-1 loop).
#pragma once

#include <stdint.h>

namespace sunisle {
namespace uiBridge {

void begin();                // qy_ui_init(), after LVGL + display are ready
void tick();                  // push real time / date / quote once per second
void showBoot();
void setBootProgress(uint8_t pct);
void setPrivacy(bool owner, bool bridgeVerified);
void showPage(uint8_t qy_page_index);   // 0 = HOME, 1 = MEMOS
void nextPage();
void action(uint8_t act);      // §8.2 equivalent of the designed tap gestures
void setDemo(bool on);         // visual-verification sample data (OWNER only)
bool demo();
bool owner();                 // true only while an Owner screen is visible

// Dynamic Bridge text is arbitrary Chinese; a fixed subset cannot cover it.
// This downgrades out-of-subset CJK codepoints to '?' instead of tofu boxes.
// TODO(stage D): Bridge-side rasterised bitmap channel (see qingyu_ui.h).
const char* sanitizeDynamic(const char* utf8);

}  // namespace uiBridge
}  // namespace sunisle
