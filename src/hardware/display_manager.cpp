#include "hardware/display_manager.h"

#include "sunisle/config.h"
#include "hardware/backlight_manager.h"
#include "utils/logger.h"

#include <LovyanGFX.hpp>
#include <lgfx/v1/touch/Touch_XPT2046.hpp>

namespace sunisle {
namespace display {
namespace {

// Only ONE panel driver is compiled in. The audited project needed four for
// its diagnostic scanner; SUNISLE uses the visually VERIFIED ST7789 profile.

// Declared before the class so the constructor can flag touch readiness.
static bool s_touchReady = false;

class SunisleLGFX : public lgfx::LGFX_Device {
  lgfx::Bus_SPI       _bus;
  lgfx::Panel_ST7789  _panel;
  lgfx::Touch_XPT2046 _touch;   // rides the panel's HSPI bus (bus_shared)

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

    // Touch: XPT2046 rides the SAME HSPI bus as the LCD (bus_shared=true).
    // This is the bus-safe path the audit called for: LovyanGFX arbitrates SPI
    // transactions between panel writes and touch reads, so reading the touch
    // can never blank the panel the way the old software-bitbang probe did.
    // Only the touch CS pin differs from the LCD; the SPI pins are identical.
    auto tc = _touch.config();
    tc.spi_host  = HSPI_HOST;     // same host as the panel bus
    tc.pin_sclk  = LCD_SCLK;      // 14
    tc.pin_mosi  = LCD_MOSI;      // 13
    tc.pin_miso  = LCD_MISO;      // 12
    tc.pin_cs    = TOUCH_CS;      // 33 (best-guess CYD; verify on hardware)
    tc.pin_int   = TOUCH_IRQ;     // 36 (optional; polled if -1)
    tc.freq      = 1000000;
    tc.bus_shared = true;
    tc.x_min = 300; tc.x_max = 3900;   // XPT2046 raw calibration (tune on hw)
    tc.y_min = 400; tc.y_max = 3900;
    tc.offset_rotation = 0;
    _touch.config(tc);
    if (_touch.init()) {
      _panel.setTouch(&_touch);
      s_touchReady = true;
      SUN_LOG("display", "TOUCH ready (XPT2046, shared HSPI, cs=%d)",
              static_cast<int>(TOUCH_CS));
    } else {
      SUN_LOG("display", "TOUCH init FAILED (check TOUCH_CS=%d wiring)",
              static_cast<int>(TOUCH_CS));
    }
  }

  // Runtime CS re-probe for TOUCHPROBE (used to find the correct pin).
  bool probeTouchCS(int8_t cs) {
    auto tc = _touch.config();
    tc.pin_cs = cs;
    _touch.config(tc);
    s_touchReady = _touch.init();
    if (s_touchReady) _panel.setTouch(&_touch);
    return s_touchReady;
  }
};

inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

SunisleLGFX* s_gfx = nullptr;
bool         s_ready = false;
uint32_t     s_flushCount = 0;

static lv_indev_drv_t s_indevDrv;
static lv_indev_t*    s_indev = nullptr;

// LVGL pointer input callback: forwards the XPT2046 read into LVGL.
static void touchReadCb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
  (void)drv;
  uint16_t x = 0, y = 0;
  if (s_ready && s_gfx && s_touchReady) {
    if (s_gfx->getTouch(&x, &y) > 0) {
      data->point.x = static_cast<lv_coord_t>(x);
      data->point.y = static_cast<lv_coord_t>(y);
      data->state = LV_INDEV_STATE_PRESSED;
      return;
    }
  }
  data->state = LV_INDEV_STATE_RELEASED;
}

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

  // Register the touch pointer device. Harmless if touch init failed
  // (s_touchReady stays false and no indev is created).
  if (s_touchReady) {
    lv_indev_drv_init(&s_indevDrv);
    s_indevDrv.type = LV_INDEV_TYPE_POINTER;
    s_indevDrv.read_cb = &touchReadCb;
    s_indevDrv.disp = lv_disp_get_default();
    s_indev = lv_indev_drv_register(&s_indevDrv);
    SUN_LOG("display", "LVGL pointer indev registered");
  }

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

bool touchReady() { return s_touchReady; }

bool touchRead(uint16_t* x, uint16_t* y) {
  if (!s_ready || !s_gfx || !s_touchReady) return false;
  return s_gfx->getTouch(x, y) > 0;
}

bool touchProbe(int8_t cs) {
  if (!s_gfx) return false;
  return s_gfx->probeTouchCS(cs);
}

}  // namespace display
}  // namespace sunisle
