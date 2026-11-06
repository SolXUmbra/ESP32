// Adapter: real SUNISLE state  ->  delivered ui_sunisle.cpp setters
//          LovyanGFX TJpgD     ->  ui_show_photo(lv_img_dsc_t*)
//
// DATA SAFETY / MEMORY CONTRACT
//  * The delivered UI file is used byte-for-byte; no layout is redefined here.
//  * ESP32 has NO PSRAM. A JPEG is NEVER decoded at native resolution.
//    LovyanGFX/TJpgD streams MCU blocks (3,900 B work pool + 512 B input)
//    into a single VIEWPORT-sized 216x164 RGB565 sprite (70,848 B), which is
//    the only pixel buffer kept alive. LVGL then owns the image completely,
//    so normal LVGL redraws cannot wipe it (unlike drawing straight to LCD).
//  * The sprite is allocated lazily: with zero photos (current stage 1) it is
//    never created, so the shipped firmware uses 0 extra bytes of heap.
//  * No photo is invented. Without a real JPEG the vendor placeholder stays.
#include "ui/ui_bridge.h"

#include "ui_sunisle.h"
#include "fonts/sunisle_cn.h"
#include "sunisle/config.h"
#include "sunisle/state.h"
#include "services/photo_service.h"
#include "hardware/storage_manager.h"
#include "utils/logger.h"

// Implemented at the END of the delivered ui_sunisle.cpp (appended, layout
// untouched). It only swaps the font object of the four CJK-capable labels.
extern "C" void ui_apply_cn_fonts(const lv_font_t* date_font,
                                  const lv_font_t* weather_font,
                                  const lv_font_t* city_font,
                                  const lv_font_t* status_font,
                                  const lv_font_t* quote_font);

// FS_H / fs::FS must be visible BEFORE LovyanGFX.hpp so that the include-
// order-dependent drawJpgFile(fs::FS&, ...) extension methods are generated.
// LGFX_FILESYSTEM_Support adds NO data members, so the class layout stays
// identical to display_manager.cpp's translation unit.
#include <SD.h>
#include <LovyanGFX.hpp>

#include <stdio.h>
#include <string.h>

namespace sunisle {
namespace uiBridge {
namespace {

// MUST match PHOTO_W / PHOTO_H in the delivered ui_sunisle.cpp (lines 23-24).
// Kept as constants here so the vendor file stays byte-identical.
constexpr int32_t kPhotoW = 216;
constexpr int32_t kPhotoH = 164;

lgfx::LGFX_Sprite* s_sprite = nullptr;
lv_img_dsc_t       s_dsc;
char               s_shownPath[64] = {0};
const char*        s_mode = "NONE";
bool               s_photoEnabled = true;
int8_t             s_pixelSwap = -1;   // -1 = not probed yet

// REAL WMO 4677 code -> the vendor's hand-drawn weather icon set.
ui_weather_t condFromWmo(uint8_t c) {
  if (c == 0) return UI_WEATHER_SUNNY;
  if (c == 1 || c == 2) return UI_WEATHER_PARTLY_CLOUDY;
  if (c == 3 || c == 45 || c == 48) return UI_WEATHER_CLOUDY;
  if ((c >= 51 && c <= 67) || (c >= 80 && c <= 82) || c >= 95)
    return UI_WEATHER_RAIN;
  if ((c >= 71 && c <= 77) || c == 85 || c == 86) return UI_WEATHER_SNOW;
  return UI_WEATHER_UNKNOWN;
}

// Minimal JPEG SOF reader: only reads marker headers, never image data.
// Used to pick a sane decode zoom without loading pixels into RAM.
bool jpegSize(const char* path, uint16_t& w, uint16_t& h) {
  w = h = 0;
  File f = SD.open(path, FILE_READ);
  if (!f) return false;
  uint8_t b[4];
  if (f.read(b, 2) != 2 || b[0] != 0xFF || b[1] != 0xD8) {  // SOI
    f.close();
    return false;
  }
  for (int guard = 0; guard < 32; ++guard) {
    uint8_t c = 0;
    do { if (f.read(&c, 1) != 1) { f.close(); return false; } } while (c != 0xFF);
    do { if (f.read(&c, 1) != 1) { f.close(); return false; } } while (c == 0xFF);
    const uint8_t marker = c;
    if (marker == 0xD9 || marker == 0xDA) break;             // EOI / SOS
    if (f.read(b, 2) != 2) { f.close(); return false; }
    const uint16_t len = (uint16_t(b[0]) << 8) | b[1];
    if (len < 2) break;
    if (marker >= 0xC0 && marker <= 0xCF &&
        marker != 0xC4 && marker != 0xC8 && marker != 0xCC) {
      uint8_t sof[6];
      if (f.read(sof, 6) != 6) { f.close(); return false; }
      h = (uint16_t(sof[1]) << 8) | sof[2];
      w = (uint16_t(sof[3]) << 8) | sof[4];
      f.close();
      return w > 0 && h > 0;
    }
    f.seek(len - 2, SeekCur);
  }
  f.close();
  return false;
}

bool ensureSprite() {
  if (s_sprite) return true;
  s_sprite = new lgfx::LGFX_Sprite();        // standalone: no parent device
  if (!s_sprite) return false;
  s_sprite->setPsram(false);                 // this board has NO PSRAM
  s_sprite->setColorDepth(16);               // RGB565 == lv_color_t (LE)
  void* buf = s_sprite->createSprite(kPhotoW, kPhotoH);
  if (!buf) {
    delete s_sprite;
    s_sprite = nullptr;
    SUN_LOG("uibridge", "sprite alloc FAILED (%ld B), photo stays hidden",
            static_cast<long>(kPhotoW * kPhotoH * 2));
    return false;
  }
  SUN_LOG("uibridge", "viewport sprite %ldx%ld = %ld B (no PSRAM, lazy)",
          static_cast<long>(kPhotoW), static_cast<long>(kPhotoH),
          static_cast<long>(kPhotoW * kPhotoH * 2));
  return true;
}

// WHY THE PHOTO WAS GARBLED (花屏)
// --------------------------------
// LovyanGFX stores 16-bit sprites as swap565_t: RGB565 in SPI byte order
// (the two bytes of every pixel are reversed). LVGL here is configured with
// LV_COLOR_16_SWAP=0, i.e. it expects native little-endian RGB565. The rest
// of the UI is drawn by LVGL itself, so only the photo - whose pixels came
// straight out of the LovyanGFX sprite - looked like coloured noise.
//
// Probe once with a known colour: write it, read the first two bytes back.
// If the little-endian interpretation does not match, the sprite is in SPI
// byte order and every pixel must be byte-swapped before LVGL sees it.
void probePixelOrder() {
  if (s_pixelSwap >= 0 || !s_sprite) return;
  const uint16_t probe = lgfx::color565(0x12, 0x34, 0x56);
  s_sprite->fillScreen(probe);   // immediately overwritten by the sand fill
  const uint8_t* p = static_cast<const uint8_t*>(s_sprite->getBuffer());
  const uint16_t asLE = static_cast<uint16_t>(p[0]) |
                        (static_cast<uint16_t>(p[1]) << 8);
  bool need = (asLE != probe);      // sprite bytes reversed vs LVGL
#if LV_COLOR_16_SWAP
  need = !need;                     // LVGL itself wants SPI byte order
#endif
  s_pixelSwap = need ? 1 : 0;
  SUN_LOG("uibridge", "pixel order: probe=%04X asLE=%04X -> swap=%d",
          static_cast<unsigned>(probe), static_cast<unsigned>(asLE),
          static_cast<int>(s_pixelSwap));
}

// Stream-decode one JPEG into the viewport sprite, then hand LVGL a descriptor.
bool streamPhoto(const char* path) {
  if (!storage::isReady()) return false;
  uint16_t jw = 0, jh = 0;
  if (!jpegSize(path, jw, jh)) {
    SUN_LOG("uibridge", "SOF read failed (not a JPEG?): %s", path);
    return false;
  }
  if (!ensureSprite()) return false;
  probePixelOrder();

  // "Contain" fit keeps the whole real photo visible and centered; the frame
  // keeps the vendor's exact 216x164 geometry and rounded corners.
  const float fx = static_cast<float>(kPhotoW) / static_cast<float>(jw);
  const float fy = static_cast<float>(kPhotoH) / static_cast<float>(jh);
  const float zoom = fx < fy ? fx : fy;
  const int32_t dw = static_cast<int32_t>(jw * zoom);
  const int32_t dh = static_cast<int32_t>(jh * zoom);
  const int32_t x = (kPhotoW - dw) / 2;
  const int32_t y = (kPhotoH - dh) / 2;

  // Sand background = vendor C_SAND, so letterboxing looks intentional.
  s_sprite->fillScreen(lgfx::color565(0xEE, 0xE8, 0xDC));
  const bool ok = s_sprite->drawJpgFile(SD, path, x, y, dw, dh,
                                        0, 0, zoom);
  if (!ok) {
    SUN_LOG("uibridge", "TJpgD stream FAILED for %s", path);
    return false;
  }

  // Normalise the pixel byte order in place: no second buffer needed.
  if (s_pixelSwap == 1) {
    uint8_t* px = static_cast<uint8_t*>(s_sprite->getBuffer());
    for (size_t i = 0; i < kPhotoW * kPhotoH * 2; i += 2) {
      const uint8_t t = px[i];
      px[i] = px[i + 1];
      px[i + 1] = t;
    }
  }

  memset(&s_dsc, 0, sizeof(s_dsc));
  s_dsc.header.always_zero = 0;
  s_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
  s_dsc.header.w = kPhotoW;
  s_dsc.header.h = kPhotoH;
  s_dsc.data_size = kPhotoW * kPhotoH * 2;
  s_dsc.data = static_cast<const uint8_t*>(s_sprite->getBuffer());

  // Pixel proof: a decode that "succeeds" but writes nothing would leave the
  // buffer at the sand fill colour. Sampling is cheap and answers the one
  // question the serial log cannot: did real photo pixels land in the sprite?
  {
    const uint16_t* px = reinterpret_cast<const uint16_t*>(s_dsc.data);
    const uint16_t sand = lgfx::color565(0xEE, 0xE8, 0xDC);   // same fill as above
    uint32_t sampled = 0, nonSand = 0, sum = 0;
    for (int32_t i = 0; i < kPhotoW * kPhotoH; i += 11) {
      const uint16_t v = px[i];
      ++sampled;
      sum += v;
      if (v != sand) ++nonSand;
    }
    SUN_LOG("uibridge", "pixels sampled=%lu nonSand=%lu%% checksum=%08lX",
            static_cast<unsigned long>(sampled),
            static_cast<unsigned long>(nonSand * 100 / (sampled ? sampled : 1)),
            static_cast<unsigned long>(sum));
  }

  ui_show_photo(&s_dsc);   // vendor cover() sees 216x164 -> zoom 256 = 1:1
  strncpy(s_shownPath, path, sizeof(s_shownPath) - 1);
  s_shownPath[sizeof(s_shownPath) - 1] = 0;
  s_mode = "STREAMING";
  // Integer milliscale: printf float support is not guaranteed on this toolchain.
  SUN_LOG("uibridge", "photo %ux%u -> zoom=%lu/1000 (%s)",
          static_cast<unsigned>(jw), static_cast<unsigned>(jh),
          static_cast<unsigned long>(zoom * 1000.0f), path);
  return true;
}

}  // namespace

void begin() {
  ui_init();       // the delivered UI, after lv_init() + display registration
  // Chinese hand-off: 12 px date/status, 16 px weather + city. Layout, colours
  // and coordinates inside create_home() are untouched.
  ui_apply_cn_fonts(&sunisle_cn_12, &sunisle_cn_16, &sunisle_cn_16,
                    &sunisle_cn_quote, &sunisle_cn_quote);
  s_mode = "NONE";
  SUN_LOG("uibridge", "vendor ui_sunisle.cpp initialised (240x320 single screen)");
  SUN_LOG("uibridge", "CJK fonts bound: cn_12 date/status, cn_16 weather/city");
}

void tick() {
  const Snapshot s = snapshot();

  // ---- Real date, Chinese ("11月06日  星期五") ----
  char date[32];
  if (s.timeSynced)
    snprintf(date, sizeof(date), "%s  %s", s.dateCn, s.weekdayCn);
  else
    snprintf(date, sizeof(date), "未校时");
  ui_set_date(date);

  // ---- Real time ----
  ui_set_time(s.timeSynced ? s.timeHHMM : "--:--");

  // ---- Real weather / city (Open-Meteo, Chinese WMO wording) ----
  ui_weather_t cond = UI_WEATHER_UNKNOWN;
  char temp[12];
  const char* desc;
  if (s.weatherValid) {
    cond = condFromWmo(s.wmoCode);
    snprintf(temp, sizeof(temp), "%d°", static_cast<int>(s.tempC));
    desc = s.weatherDesc;
  } else {
    snprintf(temp, sizeof(temp), "--°");
    desc = s.weatherConfigured ? "更新中" : "天气未配置";
  }
  // Place label is always the Chinese 地名「渑池」, independent of whether
  // Open-Meteo is configured. The CJK font ships both glyphs.
  ui_set_weather(cond, temp, desc, DISPLAY_CITY);

  // ---- Real photo state ----
  const char* path = s_photoEnabled ? photoService::currentPath() : nullptr;
  if (path && strcmp(path, s_shownPath) != 0) {
    if (!streamPhoto(path)) s_mode = "FAILED";
  }
  char caption[32];
  caption[0] = 0;
  if (path) {
    const char* slash = strrchr(path, '/');
    const char* name = slash ? slash + 1 : path;
    snprintf(caption, sizeof(caption), "%s", name);
    char* dot = strrchr(caption, '.');
    if (dot) *dot = 0;
    for (char* p = caption; *p; ++p) if (*p == '_') *p = ' ';
  }
  ui_set_caption(caption);

  // ---- Bottom strip: the brand moved to the top-left, so the full-width
  //      strip now carries only the daily quote. ----
  ui_set_status(s.quote[0] ? s.quote : "把喜欢的时光，留在身边。");
}

void setPhotoEnabled(bool on) {
  s_photoEnabled = on;
  s_shownPath[0] = 0;          // force a re-decode on the next tick
  if (!on) {
    ui_hide_photo();           // back to the calm placeholder, never garble
    s_mode = "OFF";
    SUN_LOG("uibridge", "photo display disabled (PHOTO=OFF)");
  } else {
    SUN_LOG("uibridge", "photo display enabled (PHOTO=ON)");
  }
}

const char* photoMode() { return s_mode; }

}  // namespace uiBridge
}  // namespace sunisle
