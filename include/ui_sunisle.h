#pragma once

#include "lvgl.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * SUNISLE LVGL v8 UI
 * Target: 240x320 portrait display.
 *
 * Call ui_init() after lv_init() and after the display driver has been registered.
 * Call all ui_* functions from the LVGL task/thread, or protect them with the
 * same LVGL mutex used by your project.
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

/* Date example: "FRIDAY  NOV 06" or "11月06日  星期五" (requires Chinese font). */
void ui_set_date(const char *date_text);

/* Time example: "14:08". */
void ui_set_time(const char *time_text);

/* Weather text example: "晴朗", "多云", "小雨"; ASCII text is safe with default font. */
void ui_set_weather(ui_weather_t condition,
                    const char *temperature,
                    const char *description,
                    const char *city);

/*
 * Set a decoded LVGL image descriptor as the current photo.
 * The image is scaled with "cover" behavior and clipped to the photo viewport.
 * The descriptor and its pixel data must remain valid while displayed.
 *
 * Important: this function expects an lv_img_dsc_t, not a JPEG filename.
 * If your JPEG decoder produces another format, adapt it before calling here.
 */
void ui_show_photo(const lv_img_dsc_t *image);

/* Show a short caption below the photo. Pass NULL or "" to hide it. */
void ui_set_caption(const char *caption);

/* Optional status line: e.g. "已同步  ·  128 张" or "OFFLINE". */
void ui_set_status(const char *status);

/* Optional: switch between home and simple photo view. */
void ui_show_home(void);
void ui_show_photo_view(void);

#ifdef __cplusplus
}
#endif
