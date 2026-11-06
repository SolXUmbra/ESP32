#include "hardware/display_manager.h"

#include "sunisle/config.h"
#include "hardware/backlight_manager.h"
#include "utils/logger.h"

#include <LovyanGFX.hpp>

namespace sunisle {
namespace display {
namespace {

// Only ONE panel driver is compiled in. The audited project needed four for
// its diagnostic scanner; SUNISLE uses the visually VERIFIED ST7789 profile.
class SunisleLGFX : public lgfx::LGFX_Device {
  lgfx::Bus_SPI      _bus;
  lgfx::Panel_ST7789 _panel;

 public:
  SunisleLGFX() {
    auto bc = _bus.config();
    bc.spi_host    = HSPI_HOST;
    bc.spi_mode    = 0;
    bc.freq_write  = LCD_FREQ_WRITE;
    bc.freq_read   = LCD_FREQ_READ;
    bc.spi_3wire   = false;
    bc.use_lock    = true;
    bc.dma_channel = 1;
    bc.pin_sclk    = LCD_SCLK;
    bc.pin_mosi    = LCD_MOSI;
    bc.pin_miso    = LCD_MISO;
    bc.pin_dc      = LCD_DC;
    _bus.config(bc);

    _panel.setBus(&_bus);
    auto pc = _panel.config();
    pc.pin_cs       = LCD_CS;
    pc.pin_rst      = LCD_RST;
    pc.pin_busy     = -1;
    pc.panel_width  = SCREEN_W;
    pc.panel_height = SCREEN_H;
    pc.offset_x     = LCD_OFFSET_X;
    pc.offset_y     = LCD_OFFSET_Y;
    pc.invert       = LCD_INVERT;
    pc.rgb_order    = LCD_RGB_ORDER;
    _panel.config(pc);
    setPanel(&_panel);
  }
};

inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

SunisleLGFX* s_gfx = nullptr;
bool         s_ready = false;
uint32_t     s_flushCount = 0;

static lv_disp_draw_buf_t s_drawBuf;
static lv_disp_drv_t      s_dispDrv;
static lv_color_t         s_buf1[SCREEN_W * LVGL_BUF_LINES];

void flush(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
  if (s_ready && s_gfx) {
    const int32_t w = area->x2 - area->x1 + 1;
    const int32_t h = area->y2 - area->y1 + 1;
    ++s_flushCount;
    s_gfx->startWrite();
    s_gfx->setAddrWindow(area->x1, area->y1, w, h);
    // swap=true converts LVGL native little-endian RGB565 to SPI byte order.
    // This exact call was visually verified with 16 numbered test cards.
    s_gfx->writePixels(reinterpret_cast<uint16_t*>(color_p), w * h,
                       LCD_PIXEL_SWAP);
    s_gfx->endWrite();
  }
  lv_disp_flush_ready(drv);
}

}  // namespace

bool begin() {
  s_gfx = new SunisleLGFX();
  if (!s_gfx->init()) {
    SUN_LOG("display", "LovyanGFX init FAILED");
    s_ready = false;
    return false;
  }
  s_gfx->setRotation(0);

  backlight::begin();
  s_gfx->fillScreen(rgb565(0xF7, 0xF4, 0xEC));  // cream white #F7F4EC
  delay(20);

  lv_init();
  lv_disp_draw_buf_init(&s_drawBuf, s_buf1, nullptr,
                        SCREEN_W * LVGL_BUF_LINES);
  lv_disp_drv_init(&s_dispDrv);
  s_dispDrv.hor_res  = SCREEN_W;
  s_dispDrv.ver_res  = SCREEN_H;
  s_dispDrv.flush_cb = &flush;
  s_dispDrv.draw_buf = &s_drawBuf;
  lv_disp_drv_register(&s_dispDrv);

  s_ready = true;
  SUN_LOG("display", "READY ST7789 %dx%d HSPI 20MHz swap=%d buf=%uB",
          SCREEN_W, SCREEN_H, LCD_PIXEL_SWAP ? 1 : 0,
          static_cast<unsigned>(sizeof(s_buf1)));
  return true;
}

void drawRawDiagnostic() {
  if (!s_ready || !s_gfx) return;
  const uint16_t bars[6] = {
      rgb565(0xE9, 0xA8, 0x6D), rgb565(0x9E, 0xAA, 0x91),
      rgb565(0xB9, 0xCB, 0xD0), rgb565(0xEE, 0xE8, 0xDC),
      rgb565(0x85, 0x84, 0x77), rgb565(0x35, 0x3A, 0x36)};
  s_gfx->fillScreen(rgb565(0xF7, 0xF4, 0xEC));
  for (uint8_t i = 0; i < 6; ++i) s_gfx->fillRect(i * 40, 0, 40, 24, bars[i]);
  s_gfx->setTextColor(rgb565(0x35, 0x3A, 0x36));
  s_gfx->setTextSize(2);
  s_gfx->setCursor(16, 50);
  s_gfx->print("SUNISLE RAW OK");
  s_gfx->setTextSize(1);
  s_gfx->setCursor(16, 80);
  s_gfx->print("LVGL BYPASSED - DIAG ONLY");
  for (int y = 110; y < 300; y += 20)
    for (int x = 0; x < SCREEN_W; x += 20)
      if (((x / 20) + (y / 20)) & 1)
        s_gfx->fillRect(x, y, 20, 20, rgb565(0xE5, 0xE0, 0xD5));
  SUN_LOG("display", "RAW diagnostic pattern drawn (LVGL bypassed)");
}

void setBrightness(uint8_t v) { backlight::set(v); }
bool isReady() { return s_ready; }
uint32_t flushCount() { return s_flushCount; }

void pushRect(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t* px) {
  if (!s_ready || !s_gfx) return;
  s_gfx->startWrite();
  s_gfx->setAddrWindow(x, y, w, h);
  s_gfx->writePixels(px, w * h, LCD_PIXEL_SWAP);
  s_gfx->endWrite();
}

}  // namespace display
}  // namespace sunisle
