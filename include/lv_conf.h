// LVGL 8 configuration for 晴屿 SUNISLE.
// LV_CONF_H guard is REQUIRED: without it lv_conf_internal.h silently falls
// back to library defaults (this was a real bug in the audited project).
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH      16
#define LV_COLOR_16_SWAP    0

#define LV_HOR_RES_MAX      240
#define LV_VER_RES_MAX      320

#define LV_MEM_CUSTOM       0
#define LV_MEM_SIZE         (24U * 1024U)
#define LV_MEM_BUF_MAX_NUM  8
#define LV_DISP_DEF_REFR_PERIOD 33
#define LV_INDEV_DEF_READ_PERIOD 30

#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

#define LV_USE_LABEL   1
#define LV_USE_BTN     1
#define LV_USE_CONT    1
#define LV_USE_IMG     1
#define LV_USE_LINE    1
// Widgets actually used by ui_sunisle.cpp: obj/label/img/line only.
// LVGL 8 defaults EVERY other widget to 1, which pulled in calendar,
// keyboard, menu, meter, tabview, ... plus their theme/draw paths (~40 KB).
// Keep the vendor UI byte-identical: none of these are referenced by it.
#define LV_USE_ARC     0
#define LV_USE_METER   0
#define LV_USE_CALENDAR 0
#define LV_USE_KEYBOARD 0
#define LV_USE_MENU    0
#define LV_USE_TABVIEW 0
#define LV_USE_TILEVIEW 0
#define LV_USE_WIN     0
#define LV_USE_DROPDOWN 0
#define LV_USE_ROLLER  0
#define LV_USE_SPINBOX 0
#define LV_USE_SPINNER 0
#define LV_USE_SWITCH  0
#define LV_USE_CHECKBOX 0
#define LV_USE_BTNMATRIX 0
#define LV_USE_LIST    0
#define LV_USE_LED     0
#define LV_USE_COLORWHEEL 0
#define LV_USE_BAR     0
#define LV_USE_SLIDER  0
#define LV_USE_MSGBOX  0
#define LV_USE_CHART   0
#define LV_USE_TABLE   0
#define LV_USE_USER_DATA 1

// Montserrat for Latin/digits. 12/16/28 are the fallbacks of the generated
// CJK subset fonts (see tools/gen_cn_font.py).
// 10 / 18 / 32 are required by the user-provided ui_sunisle.cpp (date line,
// temperature, and the big time anchor). Enabled here rather than editing
// the vendor UI source, which stays byte-identical to the delivered zip.
// 20 and 48 were only used by the retired three-page mock-up; they are gone.
#define LV_FONT_MONTSERRAT_10 1
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_28 0
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 0
#define LV_USE_ANIM 1

#define LV_USE_FS_FATFS 0
#define LV_USE_FS_SPIFFS 0
#define LV_USE_PNG 0
#define LV_USE_BMP 0
#define LV_USE_GIF 0
#define LV_USE_QRCODE 0

#endif  // LV_CONF_H
