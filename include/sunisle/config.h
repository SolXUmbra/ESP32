// SUNISLE non-secret build-time constants.
// Hardware values below are FROZEN: see HARDWARE_CONFIG.md and AUDIT_REPORT.md.
// Secrets (Wi-Fi, device token) live in include/secrets.h (gitignored).
#pragma once

#include <Arduino.h>

namespace sunisle {

// ---- Verified display (panel-scan cards 03 / 07 / 16) ----
constexpr uint16_t SCREEN_W = 240;
constexpr uint16_t SCREEN_H = 320;

constexpr int8_t  LCD_SCLK = 14;
constexpr int8_t  LCD_MOSI = 13;
constexpr int8_t  LCD_MISO = 12;
constexpr int8_t  LCD_DC   = 2;
constexpr int8_t  LCD_CS   = 15;
constexpr int8_t  LCD_RST  = -1;
constexpr uint32_t LCD_FREQ_WRITE = 20000000UL;  // 20 MHz verified
constexpr uint32_t LCD_FREQ_READ  = 10000000UL;
constexpr bool    LCD_INVERT      = false;
constexpr bool    LCD_RGB_ORDER   = false;
constexpr bool    LCD_PIXEL_SWAP  = true;       // LVGL LE RGB565 -> SPI byte order
constexpr uint8_t LCD_OFFSET_X    = 0;
constexpr uint8_t LCD_OFFSET_Y    = 0;

// ---- Backlight: confirmed from atlas-moments/src/hal/display.cpp:50-55 ----
constexpr int8_t  BL_PIN       = 21;
constexpr uint8_t BL_CHANNEL   = 0;
constexpr uint32_t BL_PWM_FREQ = 5000;
constexpr uint8_t BL_PWM_RES   = 8;
constexpr uint8_t BL_DEFAULT   = 220;

// ---- SD: VSPI, separate host from the LCD HSPI bus ----
constexpr int8_t SD_CS = 5;

// ---- LVGL ----
constexpr uint16_t LVGL_BUF_LINES = 12;         // 240*12 px partial buffer

// ---- Timing ----
constexpr uint32_t LVGL_LOOP_MS      = 5;
constexpr uint32_t UI_TICK_MS        = 1000;
constexpr uint32_t HEARTBEAT_MS      = 10000;
constexpr uint32_t BOOT_FADE_MS      = 400;    // 米色淡入 0.4s
constexpr uint32_t BOOT_HOLD_MS      = 5000;   // 画面清晰停留 5s
constexpr uint32_t BOOT_TOTAL_MS     = 5400;   // 400 + 5000
constexpr uint32_t SLIDESHOW_DEFAULT_MS = 5000;    // 5 秒随机轮换
constexpr uint32_t SLIDESHOW_MIN_MS      = 3000;
constexpr uint32_t SLIDESHOW_MAX_MS      = 300000;

// ---- SD layout (task book section 8) ----
constexpr const char* SD_ROOT        = "/SUNISLE";
constexpr const char* DIR_CONFIG     = "/SUNISLE/config";
constexpr const char* DIR_PHOTOS     = "/SUNISLE/photos";
constexpr const char* DIR_THUMBS     = "/SUNISLE/photos/thumbnails";
constexpr const char* DIR_WEATHER    = "/SUNISLE/weather";
constexpr const char* DIR_SYSTEM     = "/SUNISLE/system";
constexpr const char* FILE_DEVICE    = "/SUNISLE/config/device.json";
constexpr const char* FILE_SETTINGS  = "/SUNISLE/config/settings.json";
constexpr const char* FILE_INDEX     = "/SUNISLE/photos/index.json";
constexpr const char* FILE_WEATHER   = "/SUNISLE/weather/cache.json";
constexpr const char* FILE_SYNC      = "/SUNISLE/system/sync.json";
constexpr const char* FILE_SYNC_LOG  = "/SUNISLE/system/sync.log";

// Reserve space; never fill the card.
constexpr uint64_t SD_MIN_FREE_BYTES = 64ULL * 1024ULL * 1024ULL;

// ---- Product identity ----
// Home screen place label: always 渑池 in Chinese, independent of whether
// the weather service is configured. The CJK font ships these two glyphs.
constexpr const char* DISPLAY_CITY = "渑池";
constexpr const char* PRODUCT_CN   = "晴屿";
constexpr const char* PRODUCT_EN   = "SUNISLE";
constexpr const char* SLOGAN_CN    = "把喜欢的时光，留在身边。";
constexpr const char* SLOGAN_EN    = "A Little Island of Sunshine.";
constexpr const char* FW_VERSION   = "SUNISLE 1.0.0-stage1";

// Max number of photo paths kept in RAM (path strings only, never pixels).
constexpr uint16_t MAX_PHOTO_ENTRIES = 200;

}  // namespace sunisle
