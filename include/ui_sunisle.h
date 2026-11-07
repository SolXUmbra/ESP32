#pragma once

#include "lvgl.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * SUNISLE B LVGL v8 UI - 240x320 portrait.
 * Call ui_init() after lv_init() and display driver registration.
 * All ui_* calls must come from the LVGL task/thread.
 */

typedef enum {
    UI_WEATHER_SUNNY = 0,
    UI_WEATHER_PARTLY_CLOUDY,
    UI_WEATHER_CLOUDY,
    UI_WEATHER_RAIN,
    UI_WEATHER_SNOW,
    UI_WEATHER_WIND,
    UI_WEATHER_UNKNOWN
} ui_weather_t;

void ui_init(void);

void ui_set_date(const char *date_text);
void ui_set_time(const char *time_text);
void ui_set_weather(ui_weather_t condition, const char *temperature,
                    const char *description, const char *city);

/* Memo panel: multi-line list + a counter line. */
void ui_set_memos(const char *text);
void ui_set_memo_count(const char *text);

/* Due reminder card. Tap anywhere to complete it (handled by the UI). */
void ui_show_alert(const char *text);
void ui_clear_alert(void);

void ui_set_status(const char *status);
void ui_show_home(void);
void ui_show_memo_view(void);

#ifdef __cplusplus
}
#endif
