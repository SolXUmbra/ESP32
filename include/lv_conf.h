// LVGL 8 configuration for 晴屿 SUNISLE.
// LV_CONF_H guard is REQUIRED: without it lv_conf_internal.h silently falls
// back to library defaults (this was a real bug in the audited project).
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH      16
#define LV_COLOR_16_SWAP    0

#define LV_HOR_RES_MAX      320
#define LV_VER_RES_MAX      240

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
#define LV_USE_BTN     1   // stage D pointer-driven nav (currently no indev)
#define LV_USE_CONT    1
// IMG stays 1: LVGL 8's lv_canvas.h / lv_animimg.h reference lv_img_t
// unconditionally, so disabling it breaks the header set (measured above).
#define LV_USE_IMG     1
#define LV_USE_LINE    1
// Widget gates below are the upstream 1ab7ea3 slimming set, which compiled
// and booted on this exact board. B six-page UI uses obj/label/btn only.
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

// Montserrat for Latin/digits. UI 2.0 keeps ONLY the CJK fallback sizes
// (12/14/16); the 34 px clock and the big task count come from our own
// proportional Light digit face sunisle_num_34. 18/28 were the UI 1.0 page
// index and guest clock and are now off, freeing ~25 KB of app0 (§9.4).
#define LV_FONT_MONTSERRAT_10 0
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 0
#define LV_FONT_MONTSERRAT_28 0
#define LV_FONT_MONTSERRAT_32 0
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
