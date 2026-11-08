// XPT2046 driver for the QINGYU B board (ESP32-2432S028R "CYD2USB", the
// two-USB / ST7789 revision).
//
// The touch controller sits on its OWN bus, separate from the LCD (HSPI
// 14/13/12) and the SD card (VSPI 18/19/23):
//   T_CLK=25  T_MOSI(DIN)=32  T_MISO(DOUT)=39  T_CS=33  (PENIRQ=36, unused)
//
// Two facts were established by measurement on this unit and matter:
//   1. Transfers use HARDWARE SPI (VSPI host driving GPIO 25/39/32) - the same
//      recipe the upstream CYD2USB driver uses. A bit-bang path is kept as a
//      fallback but is not the default.
//   2. PENIRQ is NOT used. This revision does not give a trustworthy IRQ line,
//      so a press is detected purely from the pressure channel: z = Z1 + 4095
//      - Z2, press above 400, release below 250 (hysteresis). An untouched
//      resistive panel reads random raw X/Y - only Z means anything.
//
// Channel commands are X=0x90, Y=0xD0, Z1=0xB0, Z2=0xC0 (+0x01 to keep the
// reference on). Getting X/Y or the Z formula wrong makes every read look like
// floating noise, which is exactly what caused the earlier "touch is dead"
// misdiagnosis.
//
// Screen coordinates come from a 4-corner calibration stored in NVS
// (`qytouch`/`cal`); TOUCHCAL re-runs it, TOUCHSETCAL injects known points.
//
// No LVGL pointer indev is registered (per B方案_白屏二分记录.md the 30 ms
// getTouch() polling on a mis-configured bus was the prime white-screen
// suspect). Taps are reported through the callback only.
#include "hardware/touch_manager.h"

#include <Arduino.h>
#include <Preferences.h>
#include <SPI.h>
#include <Wire.h>
#include <string.h>

#include "sunisle/config.h"
#include "utils/logger.h"

namespace sunisle {
namespace touch {
namespace {

// ---- Touch bus (runtime-reconfigurable for bring-up) --------------------
// Defaults follow the CYD (ESP32-2432S028) wiring, but every pin can be
// overridden at runtime via TOUCHBUS=/TOUCHPINS= so we can locate the real
// controller on this specific board WITHOUT recompiling.
int8_t   s_clk  = 25;
int8_t   s_mosi = 32;   // DIN
int8_t   s_miso = 39;   // DOUT (input-only)
constexpr int8_t TOUCH_CS_DEFAULT = 33;  // CYD default; reconfigurable at runtime
int8_t   s_irq  = 36;   // PENIRQ, active-low (input-only)

constexpr uint32_t POLL_MS  = 30;   // ~33 Hz; no LVGL indev involved
constexpr int      RAW_X_MIN = 300, RAW_X_MAX = 3900;
constexpr int      RAW_Y_MIN = 400, RAW_Y_MAX = 3900;

// XPT2046 channel commands (start bit + addr + 12-bit + keep-Vref-on).
// Verified against the CYD2USB upstream driver and XPT2046_Touchscreen:
//   X = 0x90, Y = 0xD0, Z1 = 0xB0, Z2 = 0xC0  (+0x01 keeps the reference on)
constexpr uint8_t CMD_X  = 0x91;
constexpr uint8_t CMD_Y  = 0xD1;
constexpr uint8_t CMD_Z1 = 0xB1;
constexpr uint8_t CMD_Z2 = 0xC1;

// Pressure thresholds (XPT2046_Touchscreen / Cyd2UsbTouch defaults) in raw Z
// units. Hysteresis: press above Z_PRESS, release below Z_RELEASE.
constexpr uint16_t Z_PRESS   = 400;
constexpr uint16_t Z_RELEASE = 250;

bool     s_enabled   = false;
bool     s_initialized = false;
int8_t   s_csPin     = TOUCH_CS_DEFAULT;  // active CS, reconfigurable
uint8_t  s_map       = 1;           // rotation 1 -> logical 320x240
uint32_t s_lastPoll  = 0;

// Transfer path. HARDWARE SPI (VSPI) is the default: the upstream CYD2USB
// driver talks to the XPT2046 this way (SPIClass bus, sck=25 miso=39 mosi=32
// cs=33). Bit-bang stays available as a bring-up fallback.
bool      s_useHw = true;
SPIClass* s_spi   = nullptr;

bool     s_pressed   = false;
uint32_t s_pressStart = 0;
uint8_t  s_pressStreak  = 0, s_releaseStreak = 0;
uint16_t s_rx = 0, s_ry = 0, s_rz = 0;

// Configure the ACTIVE CS pin. The previous CS (if any) is released to INPUT
// so it never drives the bus; the new one is an active-low OUTPUT left HIGH
// (deselected). Only OUTPUT-capable GPIOs may be used as CS (34/35/36/39 are
// input-only and would fault, so they are excluded from probe candidates).
void setCsPin(int8_t pin) {
  if (s_csPin >= 0 && s_csPin != pin) pinMode(s_csPin, INPUT);  // release old
  s_csPin = pin;
  if (s_csPin >= 0) {
    pinMode(s_csPin, OUTPUT);
    digitalWrite(s_csPin, HIGH);
  }
}

void hwInit() {
  setCsPin(s_csPin);            // CS is a plain GPIO, driven manually (active low)
  if (s_irq >= 0) pinMode(s_irq, INPUT);
  if (s_useHw) {
    if (!s_spi) s_spi = new SPIClass(VSPI);
    if (s_clk >= 0 && s_mosi >= 0 && s_miso >= 0) {
      s_spi->begin(s_clk, s_miso, s_mosi, -1);   // SCK, MISO, MOSI, no hw SS
      s_spi->setFrequency(1000000);
    }
  } else {
    if (s_clk  >= 0) { pinMode(s_clk,  OUTPUT); digitalWrite(s_clk,  LOW); }
    if (s_mosi >= 0) { pinMode(s_mosi, OUTPUT); digitalWrite(s_mosi, LOW); }
    if (s_miso >= 0) pinMode(s_miso, INPUT);
  }
  s_initialized = true;
  Serial.printf("[touch] bus init: mode=%s clk=%d mosi=%d miso=%d irq=%d cs=%d\n",
                s_useHw ? "hw(VSPI)" : "bitbang",
                static_cast<int>(s_clk), static_cast<int>(s_mosi),
                static_cast<int>(s_miso), static_cast<int>(s_irq),
                static_cast<int>(s_csPin));
}

// Hardware-SPI transfer: 8-bit command, 16-bit reply, 12-bit value in [14:3].
uint16_t readChannelHw(uint8_t cmd) {
  if (!s_spi) return 0;
  s_spi->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  digitalWrite(s_csPin, LOW);
  delayMicroseconds(2);
  s_spi->transfer(cmd);
  const uint16_t data = s_spi->transfer16(0x0000);
  digitalWrite(s_csPin, HIGH);
  s_spi->endTransaction();
  return static_cast<uint16_t>((data >> 3) & 0x0FFF);
}

// Software SPI: send 8-bit cmd MSB-first, read 16-bit result MSB-first.
// The 12-bit ADC value lands in bits [14:3] of the 16-bit word.
uint16_t readChannelBB(uint8_t cmd) {
  digitalWrite(s_csPin, LOW);
  for (int i = 7; i >= 0; --i) {
    digitalWrite(s_mosi, (cmd >> i) & 1 ? HIGH : LOW);
    digitalWrite(s_clk, HIGH);
    digitalWrite(s_clk, LOW);
  }
  uint16_t data = 0;
  for (int i = 0; i < 16; ++i) {
    digitalWrite(s_clk, HIGH);
    data <<= 1;
    if (digitalRead(s_miso)) data |= 1;
    digitalWrite(s_clk, LOW);
  }
  digitalWrite(s_csPin, HIGH);
  return static_cast<uint16_t>((data >> 3) & 0x0FFF);
}

uint16_t readChannel(uint8_t cmd) {
  return s_useHw ? readChannelHw(cmd) : readChannelBB(cmd);
}

// Side-by-side raw dump: one window with the panel untouched, one with it
// pressed. If the X/Y/Z ADC values do not move between the two, the chip is
// not answering on this bus - no amount of threshold tuning will help.
void rawDumpImpl(uint32_t idle_ms, uint32_t press_ms) {
  hwInit();
  auto window = [](uint32_t ms, const char* tag) {
    int xmin = 4095, xmax = 0, ymin = 4095, ymax = 0;
    int z1min = 4095, z1max = 0, z2min = 4095, z2max = 0;
    int32_t sx = 0, sy = 0;
    uint32_t n = 0;
    const uint32_t t0 = millis();
    while (millis() - t0 < ms) {
      const uint16_t x  = readChannel(CMD_X);
      const uint16_t y  = readChannel(CMD_Y);
      const uint16_t z1 = readChannel(CMD_Z1);
      const uint16_t z2 = readChannel(CMD_Z2);
      if (x < xmin) xmin = x;   if (x > xmax) xmax = x;
      if (y < ymin) ymin = y;   if (y > ymax) ymax = y;
      if (z1 < z1min) z1min = z1; if (z1 > z1max) z1max = z1;
      if (z2 < z2min) z2min = z2; if (z2 > z2max) z2max = z2;
      sx += x; sy += y; ++n;
      delay(25);
    }
    const int ax = n ? static_cast<int>(sx / static_cast<int32_t>(n)) : 0;
    const int ay = n ? static_cast<int>(sy / static_cast<int32_t>(n)) : 0;
    Serial.printf("[touchraw] %-22s n=%lu  x=%d..%d avg%d  y=%d..%d avg%d  "
                  "z1=%d..%d  z2=%d..%d\n",
                  tag, static_cast<unsigned long>(n),
                  xmin, xmax, ax, ymin, ymax, ay,
                  z1min, z1max, z2min, z2max);
  };
  Serial.printf("[touchraw] cs=%d mode=%s - window 1 = DO NOT TOUCH\n",
                static_cast<int>(s_csPin), s_useHw ? "hw" : "bitbang");
  window(idle_ms, "IDLE (untouched)");
  Serial.println(F("[touchraw] >>> PRESS THE PANEL REPEATEDLY NOW <<<"));
  window(press_ms, "PRESSED");
  Serial.println(F("[touchraw] done - x/y/z1/z2 must MOVE in window 2"));
}

// Continuous per-sample log: z is the ONLY trustworthy press indicator, so it
// is printed alongside x/y for free-form press/release testing.
bool sample(uint16_t* x, uint16_t* y, uint16_t* z);   // fwd: defined below
void logDumpImpl(uint32_t ms) {
  hwInit();
  Serial.printf("[touchlog] %lu ms, cs=%d mode=%s - press and release freely\n",
                static_cast<unsigned long>(ms), static_cast<int>(s_csPin),
                s_useHw ? "hw" : "bitbang");
  const uint32_t t0 = millis();
  uint16_t zmin = 4095, zmax = 0;
  while (millis() - t0 < ms) {
    uint16_t x, y, z;
    sample(&x, &y, &z);
    if (z < zmin) zmin = z;
    if (z > zmax) zmax = z;
    Serial.printf("[touchlog] t=%5lu z=%4u %s x=%4u y=%4u\n",
                  static_cast<unsigned long>(millis() - t0), z,
                  z > Z_PRESS ? "DOWN" : "    ", x, y);
    delay(70);
  }
  Serial.printf("[touchlog] done z=%u..%u %s\n", zmin, zmax,
                zmax > Z_PRESS ? "(press DETECTED)" : "(no press seen)");
}

bool sample(uint16_t* x, uint16_t* y, uint16_t* z) {
  readChannel(CMD_X);              // first conversion after idle is unreliable
  const uint16_t xr = readChannel(CMD_X);
  const uint16_t yr = readChannel(CMD_Y);
  const uint16_t z1 = readChannel(CMD_Z1);
  const uint16_t z2 = readChannel(CMD_Z2);
  // Canonical XPT2046 pressure: Z1 + 4095 - Z2, with the upstream clamp that
  // treats an overflow as "no pressure" (0).
  int32_t zv = static_cast<int32_t>(z1) + 4095 - static_cast<int32_t>(z2);
  if (zv < 0) zv = 0;
  if (zv > 4095) zv = 0;
  *x = xr; *y = yr; *z = static_cast<uint16_t>(zv);
  return true;
}

bool validXY(uint16_t x, uint16_t y) {
  return x > 128 && x < 3968 && y > 128 && y < 3968;
}

// ---- Calibration (4 corner taps in SCREEN space, 320x240) ---------------
// Raw corner readings define the linear mapping; store in NVS so a re-flash
// does not need another calibration round.
int32_t s_cx0 = 235, s_cx1 = 3633;   // raw value at screen x=0 / x=319
int32_t s_cy0 = 445, s_cy1 = 3663;   // raw value at screen y=0 / y=239
bool    s_cswap = false;             // raw axes swapped vs screen axes

void loadCal() {
  Preferences p;
  if (p.begin("qytouch", true)) {
    int32_t v[4] = {0, 0, 0, 0};
    const size_t n = p.getBytes("cal", v, sizeof(v));
    const bool sw = p.getBool("swap", false);
    p.end();
    if (n == sizeof(v) && v[1] != v[0] && v[3] != v[2]) {
      s_cx0 = v[0]; s_cx1 = v[1]; s_cy0 = v[2]; s_cy1 = v[3]; s_cswap = sw;
      Serial.printf("[touch] cal loaded: x %ld..%ld y %ld..%ld swap=%d\n",
                    static_cast<long>(s_cx0), static_cast<long>(s_cx1),
                    static_cast<long>(s_cy0), static_cast<long>(s_cy1),
                    s_cswap ? 1 : 0);
      return;
    }
  }
  Serial.println(F("[touch] cal = built-in default (not calibrated yet)"));
}

void saveCal() {
  Preferences p;
  if (!p.begin("qytouch", false)) return;
  const int32_t v[4] = {s_cx0, s_cx1, s_cy0, s_cy1};
  p.putBytes("cal", v, sizeof(v));
  p.putBool("swap", s_cswap);
  p.end();
}

void mapRaw(uint16_t rx, uint16_t ry, uint16_t* sx, uint16_t* sy) {
  const int32_t X = s_cswap ? static_cast<int32_t>(ry) : static_cast<int32_t>(rx);
  const int32_t Y = s_cswap ? static_cast<int32_t>(rx) : static_cast<int32_t>(ry);
  const int32_t dx = s_cx1 - s_cx0, dy = s_cy1 - s_cy0;
  int32_t px = (dx != 0) ? (X - s_cx0) * 319 / dx : 0;
  int32_t py = (dy != 0) ? (Y - s_cy0) * 239 / dy : 0;
  if (px < 0) px = 0; if (px > 319) px = 319;
  if (py < 0) py = 0; if (py > 239) py = 239;
  *sx = static_cast<uint16_t>(px);
  *sy = static_cast<uint16_t>(py);
}

int32_t iabs32(int32_t v) { return v < 0 ? -v : v; }

// Four-corner routine: hold the requested corner while its window runs.
void calRunImpl(uint32_t per_spot_ms) {
  hwInit();
  s_enabled = false;
  static const char* kName[4] = {"TOP-LEFT", "TOP-RIGHT", "BOTTOM-LEFT", "BOTTOM-RIGHT"};
  int32_t r[4][2] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
  bool ok[4] = {false, false, false, false};
  Serial.println(F("[touchcal] 4 corner taps, screen space 320x240"));
  for (int i = 0; i < 4; ++i) {
    Serial.printf("[touchcal] %d/4 >>> HOLD %s NOW (%lu s)\n", i + 1, kName[i],
                  static_cast<unsigned long>(per_spot_ms / 1000));
    const uint32_t t0 = millis();
    int32_t sx = 0, sy = 0;
    uint32_t n = 0;
    while (millis() - t0 < per_spot_ms) {
      uint16_t x, y, z;
      sample(&x, &y, &z);
      if (z > Z_PRESS) { sx += x; sy += y; ++n; }
      delay(15);
    }
    if (n >= 5) {
      r[i][0] = sx / static_cast<int32_t>(n);
      r[i][1] = sy / static_cast<int32_t>(n);
      ok[i] = true;
      Serial.printf("[touchcal] %s raw=%ld,%ld (n=%lu)\n", kName[i],
                    static_cast<long>(r[i][0]), static_cast<long>(r[i][1]),
                    static_cast<unsigned long>(n));
    } else {
      Serial.printf("[touchcal] %s NOT DETECTED (n=%lu) - hold longer/harder\n",
                    kName[i], static_cast<unsigned long>(n));
    }
  }
  if (!(ok[0] && ok[1] && ok[2] && ok[3])) {
    Serial.println(F("[touchcal] INCOMPLETE - calibration kept as-is"));
    return;
  }
  // Which raw axis runs left->right on screen?
  const int32_t dxa = iabs32((r[0][0] + r[2][0]) / 2 - (r[1][0] + r[3][0]) / 2);
  const int32_t dxb = iabs32((r[0][1] + r[2][1]) / 2 - (r[1][1] + r[3][1]) / 2);
  s_cswap = (dxb > dxa);
  if (!s_cswap) {
    s_cx0 = (r[0][0] + r[2][0]) / 2;  s_cx1 = (r[1][0] + r[3][0]) / 2;
    s_cy0 = (r[0][1] + r[1][1]) / 2;  s_cy1 = (r[2][1] + r[3][1]) / 2;
  } else {
    s_cx0 = (r[0][1] + r[2][1]) / 2;  s_cx1 = (r[1][1] + r[3][1]) / 2;
    s_cy0 = (r[0][0] + r[1][0]) / 2;  s_cy1 = (r[2][0] + r[3][0]) / 2;
  }
  saveCal();
  Serial.printf("[touchcal] OK x %ld..%ld y %ld..%ld swap=%d (saved to NVS)\n",
                static_cast<long>(s_cx0), static_cast<long>(s_cx1),
                static_cast<long>(s_cy0), static_cast<long>(s_cy1),
                s_cswap ? 1 : 0);
}

}  // namespace

void begin() {
  Serial.println(F("[touch] layer ready - bus pins configured lazily by autoDetect()"));
}

bool isReady() { return s_enabled; }
const char* reason() {
  static char buf[96];
  if (!s_enabled) return "disabled (enable with TOUCH=on or autoDetect)";
  snprintf(buf, sizeof(buf),
           "XPT2046 hw-SPI (clk=%d mosi=%d miso=%d cs=%d), z-threshold detect",
           static_cast<int>(s_clk), static_cast<int>(s_mosi),
           static_cast<int>(s_miso), static_cast<int>(s_csPin));
  return buf;
}

void setEnabled(bool on) {
  hwInit();
  s_enabled = on;
  s_pressed = false;
  s_lastPoll = millis();
  Serial.printf("[touch] %s\n", on ? "ENABLED" : "DISABLED");
}
bool enabled() { return s_enabled; }

void setCs(int8_t cs) {
  if (cs < 0) return;
  setCsPin(cs);
  Serial.printf("[touch] cs=%d\n", static_cast<int>(s_csPin));
}
void setMap(uint8_t m) {
  s_map = m & 3;
  Serial.printf("[touch] map=%u\n", static_cast<unsigned>(s_map));
}

void setMode(bool hw) {
  s_useHw = hw;
  if (s_spi) { s_spi->end(); }
  hwInit();
  Serial.printf("[touch] transfer mode = %s\n",
                hw ? "hardware SPI (VSPI, sck=25 miso=39 mosi=32)" : "bit-bang");
}

void rawDump(uint32_t idle_ms, uint32_t press_ms) { rawDumpImpl(idle_ms, press_ms); }
void logDump(uint32_t ms) { logDumpImpl(ms); }
void calRun(uint32_t per_spot_ms) { calRunImpl(per_spot_ms); }

// Inject a calibration measured elsewhere (e.g. from four verified corner taps)
// and persist it. x0/x1 = raw at screen x=0/319, y0/y1 = raw at screen y=0/239.
void setCal(int32_t x0, int32_t x1, int32_t y0, int32_t y1, bool swap) {
  if (x0 == x1 || y0 == y1) {
    Serial.println(F("[touch] setCal rejected (zero span)"));
    return;
  }
  s_cx0 = x0; s_cx1 = x1; s_cy0 = y0; s_cy1 = y1; s_cswap = swap;
  saveCal();
  Serial.printf("[touch] setCal x %ld..%ld y %ld..%ld swap=%d (saved)\n",
                static_cast<long>(x0), static_cast<long>(x1),
                static_cast<long>(y0), static_cast<long>(y1), swap ? 1 : 0);
}

// ---- Runtime bus reconfiguration (bring-up only) -----------------------
void setBus(int8_t clk, int8_t mosi, int8_t miso) {
  if (s_clk  >= 0) pinMode(s_clk,  INPUT);   // release old bus pins
  if (s_mosi >= 0) pinMode(s_mosi, INPUT);
  if (s_miso >= 0) pinMode(s_miso, INPUT);
  s_clk = clk; s_mosi = mosi; s_miso = miso;
  hwInit();
  Serial.printf("[touch] bus set -> clk=%d mosi=%d miso=%d (cs=%d irq=%d unchanged)\n",
                static_cast<int>(s_clk), static_cast<int>(s_mosi),
                static_cast<int>(s_miso), static_cast<int>(s_csPin),
                static_cast<int>(s_irq));
}

void setPins(int8_t clk, int8_t mosi, int8_t miso, int8_t cs, int8_t irq) {
  if (s_clk  >= 0) pinMode(s_clk,  INPUT);
  if (s_mosi >= 0) pinMode(s_mosi, INPUT);
  if (s_miso >= 0) pinMode(s_miso, INPUT);
  if (s_irq  >= 0) pinMode(s_irq,  INPUT);
  s_clk = clk; s_mosi = mosi; s_miso = miso; s_irq = irq;
  hwInit();                 // reconfigures bus + re-asserts current CS
  setCsPin(cs);             // set the new CS
  Serial.printf("[touch] pins set -> clk=%d mosi=%d miso=%d cs=%d irq=%d\n",
                static_cast<int>(s_clk), static_cast<int>(s_mosi),
                static_cast<int>(s_miso), static_cast<int>(s_csPin),
                static_cast<int>(s_irq));
}

// Scan a safe set of GPIOs and report any that CHANGE while the user presses
// the panel. A pin that goes LOW on press is the PENIRQ (active-low) of a
// resistive XPT2046 -> proves the panel is electrically connected and tells us
// exactly which IRQ pin to use. No XPT2046 SPI bus is involved here.
void gpioScan(uint32_t window_ms) {
  s_enabled = false;        // stop the main loop from bit-banging these pins
  static const int8_t kPins[] = {4, 16, 17, 22, 25, 26, 27, 32, 33, 34, 35, 36, 39};
  const uint8_t n = static_cast<uint8_t>(sizeof(kPins) / sizeof(kPins[0]));
  int base[20] = {0}, lo[20] = {0}, hi[20] = {0};
  for (uint8_t i = 0; i < n; ++i) {
    pinMode(kPins[i], INPUT_PULLUP);
    base[i] = digitalRead(kPins[i]);
    lo[i] = hi[i] = base[i];
  }
  Serial.printf("[gpioscan] baseline: ");
  for (uint8_t i = 0; i < n; ++i) Serial.printf("%d=%d ", kPins[i], base[i]);
  Serial.println();
  Serial.printf("[gpioscan] PRESS THE PANEL repeatedly for %lu ms\n",
                static_cast<unsigned long>(window_ms));
  const uint32_t t0 = millis();
  while (millis() - t0 < window_ms) {
    for (uint8_t i = 0; i < n; ++i) {
      const int v = digitalRead(kPins[i]);
      if (v < lo[i]) lo[i] = v;
      if (v > hi[i]) hi[i] = v;
    }
    delay(10);
  }
  Serial.println(F("[gpioscan] CHANGED pins (min..max over window):"));
  for (uint8_t i = 0; i < n; ++i) {
    if (lo[i] != hi[i] || lo[i] != base[i]) {
      const bool wentLow = (base[i] == 1 && lo[i] == 0);
      Serial.printf("  gpio%d: base=%d min=%d max=%d%s\n", kPins[i], base[i],
                    lo[i], hi[i],
                    wentLow ? "  <-- WENT LOW (likely PENIRQ active-low!)" : "");
    }
  }
  Serial.println(F("[gpioscan] done (touch left DISABLED; send TOUCH=on to resume)"));
}

void autoDetect() {
  hwInit();
  loadCal();                     // restore the 4-corner mapping from NVS
  s_enabled = true;
  s_pressed = false;
  s_lastPoll = millis();
  uint16_t x, y, z;
  sample(&x, &y, &z);
  Serial.printf("[touch] autoDetect: enabled, idle z=%u (press -> z>%u), "
                "cal x %ld..%ld y %ld..%ld\n",
                z, static_cast<unsigned>(Z_PRESS),
                static_cast<long>(s_cx0), static_cast<long>(s_cx1),
                static_cast<long>(s_cy0), static_cast<long>(s_cy1));
}

void probe(int8_t cs) {
  hwInit();
  // REAL CS scan: each candidate is actually SELECTED (setCsPin) so we can
  // tell which one drives MISO. A floating line reads full-range RANDOM X/Y;
  // a connected XPT2046 returns a bounded (low-variance) value even when the
  // panel is NOT pressed. Candidates are OUTPUT-capable GPIOs only (34/35/36
  // are input-only and can never be CS).
  static const int8_t kCs[] = {33, 25, 26, 27, 22, 16, 17, 4};
  const uint8_t n = cs >= 0 ? 1 : static_cast<uint8_t>(sizeof(kCs) / sizeof(kCs[0]));
  for (uint8_t i = 0; i < n; ++i) {
    const int8_t c = cs >= 0 ? cs : kCs[i];
    setCsPin(c);
    uint16_t x, y, z; sample(&x, &y, &z);   // discard first (acquisition)
    int vmin = 4095, vmax = 0, ymin = 4095, ymax = 0, sumx = 0, sumy = 0;
    for (uint8_t k = 0; k < 8; ++k) {
      if (!sample(&x, &y, &z)) continue;
      if (x < vmin) vmin = x; if (x > vmax) vmax = x;
      if (y < ymin) ymin = y; if (y > ymax) ymax = y;
      sumx += x; sumy += y;
      delay(20);
    }
    const bool stable = (vmax - vmin) < 600 && (ymax - ymin) < 600;
    Serial.printf("[touchprobe] cs=%2d  x=%u..%u(avg%d)  y=%u..%u(avg%d)  %s\n",
                  static_cast<int>(c), vmin, vmax, sumx / 8,
                  ymin, ymax, sumy / 8,
                  stable ? "STABLE (controller on this CS?)" : "random/noise");
  }
  setCsPin(TOUCH_CS_DEFAULT);   // restore default CS before returning
}

void scan(uint32_t duration_ms) {
  hwInit();
  const uint32_t t0 = millis();
  uint32_t frames = 0, hits = 0;
  uint16_t minx = 4095, maxx = 0, miny = 4095, maxy = 0;
  Serial.printf("[touchscan] START cs=%d for %lu ms - PRESS THE PANEL NOW\n",
                static_cast<int>(s_csPin), static_cast<unsigned long>(duration_ms));
  while (millis() - t0 < duration_ms) {
    uint16_t x, y, z;
    if (!sample(&x, &y, &z)) { delay(20); continue; }
    ++frames;
    if (validXY(x, y)) {                    // real touch -> report coordinates
      uint16_t sx, sy; mapRaw(x, y, &sx, &sy);
      Serial.printf("[touchscan] t=%lu raw=%u,%u z=%u -> screen=%u,%u\n",
                    static_cast<unsigned long>(millis() - t0), x, y, z, sx, sy);
      ++hits;
      if (x < minx) minx = x; if (x > maxx) maxx = x;
      if (y < miny) miny = y; if (y > maxy) maxy = y;
    }
    delay(20);
  }
  Serial.printf("[touchscan] DONE frames=%lu hits=%lu x=%u..%u y=%u..%u\n",
                static_cast<unsigned long>(frames), static_cast<unsigned long>(hits),
                minx, maxx, miny, maxy);
}

void i2cProbe() {
  // Some ST7789 boards ship an I2C capacitive controller (GT911 / CST816 /
  // FT6x36) instead of the resistive XPT2046. Scan a wide set of SDA/SCL
  // candidate pairs (excluding LCD HSPI 14/13/12/15/2, SD VSPI 18/19/23/5,
  // UART 1/3). A device found here means the touch is I2C, not SPI.
  static const int8_t kPairs[][2] = {
      {21, 22}, {22, 21}, {25, 26}, {26, 25}, {26, 27}, {27, 26},
      {25, 27}, {27, 25}, {32, 33}, {33, 32}, {16, 17}, {17, 16},
      {4, 16}, {16, 4}, {4, 17}, {17, 4}, {22, 26}, {26, 22},
      {27, 22}, {22, 27}, {32, 26}, {26, 32}, {25, 32}, {32, 25},
      {4, 22}, {22, 4}, {21, 25}, {25, 21}};
  Serial.println(F("[i2cprobe] scanning candidate SDA/SCL pairs for any device"));
  for (const auto& p : kPairs) {
    Wire.begin(p[0], p[1]);
    Wire.setClock(100000);
    delay(10);
    uint8_t found = 0, addrs[8] = {0};
    for (uint8_t a = 0x08; a <= 0x77; ++a) {
      Wire.beginTransmission(a);
      if (Wire.endTransmission() == 0 && found < sizeof(addrs)) addrs[found++] = a;
    }
    Serial.printf("[i2cprobe] sda=%d scl=%d devices=%u", p[0], p[1], found);
    for (uint8_t i = 0; i < found; ++i) Serial.printf(" 0x%02X", addrs[i]);
    Serial.println();
    Wire.end();
    delay(5);
  }
  Serial.println(F("[i2cprobe] done"));
}

// Targeted I2C touch-controller finder. Clears a possibly-stuck bus first so a
// single bad pair cannot hang the whole scan, then probes only the addresses
// used by common capacitive controllers (GT911 / CST816 / FT6x36 / etc.).
void i2cFind(int8_t sda, int8_t scl) {
  // Release both pins, then check the idle level. A healthy I2C bus idles
  // HIGH (pull-ups). If SDA is stuck LOW we do the 9-clock recovery; if it is
  // STILL low we skip this pair entirely (probing it would hang the I2C driver).
  pinMode(sda, INPUT_PULLUP);
  pinMode(scl, INPUT_PULLUP);
  delayMicroseconds(200);
  if (digitalRead(sda) == LOW) {
    // manual recovery: drive SCL as output while SDA floats high
    pinMode(scl, OUTPUT);
    for (int i = 0; i < 9; ++i) {
      digitalWrite(scl, LOW);  delayMicroseconds(5);
      digitalWrite(scl, HIGH); delayMicroseconds(5);
    }
    pinMode(scl, INPUT_PULLUP);
    delayMicroseconds(200);
  }
  if (digitalRead(sda) == LOW) {
    Serial.printf("[i2cfind] sda=%d scl=%d: SKIP (SDA stuck low)\n", sda, scl);
    return;
  }
  Wire.begin(sda, scl);
  Wire.setClock(100000);
  static const uint8_t kAddrs[] = {
      0x15, 0x1A, 0x14, 0x5D, 0x38, 0x48, 0x2E, 0x36,
      0x21, 0x20, 0x2D, 0x39, 0x40, 0x44, 0x5A, 0xBB};
  Serial.printf("[i2cfind] sda=%d scl=%d: ", sda, scl);
  uint8_t found = 0;
  for (uint8_t a : kAddrs) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { Serial.printf("0x%02X ", a); ++found; }
    delay(1);
  }
  Serial.printf("(found %u)\n", found);
  Wire.end();
}

void spiProbe(int8_t cs, uint32_t window_ms) {
  hwInit();
  const int8_t c = cs >= 0 ? cs : s_csPin;
  setCsPin(c);
  const uint32_t t0 = millis();
  Serial.printf("[spiprobe] bus fixed (clk=25 mosi=32 miso=39). window=%lu ms, "
                "PRESS THE PANEL. cs=%d\n", static_cast<unsigned long>(window_ms), c);
  while (millis() - t0 < window_ms) {
    uint16_t x, y, z;
    if (!sample(&x, &y, &z)) { delay(20); continue; }
    if (validXY(x, y)) {
      uint16_t sx, sy; mapRaw(x, y, &sx, &sy);
      Serial.printf("[spiprobe] raw=%u,%u z=%u -> screen=%u,%u\n", x, y, z, sx, sy);
    }
    delay(20);
  }
  Serial.println(F("[spiprobe] done"));
}

void misoProbe(int8_t) {
  Serial.println(F("[misoprobe] bus is now fixed to clk=25 mosi=32 miso=39 cs=33. "
                   "No MISO re-routing needed; use TOUCHSCAN to verify."));
}
void matrixProbe() {
  Serial.println(F("[matrix] bus is now fixed to clk=25 mosi=32 miso=39 cs=33. "
                   "No MISO x CS scan needed; use TOUCHSCAN to verify."));
}

void irqWatch(uint32_t window_ms) {
  hwInit();
  const uint32_t t0 = millis();
  int last = digitalRead(s_irq);
  uint32_t flips = 0;
  Serial.printf("[irqwatch] %lu ms - PRESS THE PANEL. PENIRQ(idle)=%d "
                "(expect 1 up / 0 down on press)\n",
                static_cast<unsigned long>(window_ms), last);
  while (millis() - t0 < window_ms) {
    const int v = digitalRead(s_irq);
    if (v != last) {
      last = v;
      ++flips;
      Serial.printf("[irqwatch] t=%lu PENIRQ -> %d\n",
                    static_cast<unsigned long>(millis() - t0), v);
    }
    delay(20);
  }
  Serial.printf("[irqwatch] DONE flips=%lu (a flip on press = PENIRQ wired)\n",
                static_cast<unsigned long>(flips));
}

void tick(uint32_t now_ms, TapCallback cb) {
  if (!s_enabled || !cb) return;
  if (now_ms - s_lastPoll < POLL_MS) return;
  s_lastPoll = now_ms;

  // Pressure-based detection with hysteresis, exactly like the upstream CYD2USB
  // driver: PENIRQ (GPIO36) is NOT used (it is not reliably wired on this
  // revision) and there is no X/Y window test - an untouched resistive panel
  // legitimately reads random raw X/Y, only Z tells us whether a finger is on it.
  uint16_t x, y, z;
  if (!sample(&x, &y, &z)) return;
  const bool down = s_pressed ? (z > Z_RELEASE) : (z > Z_PRESS);
  if (down) {
    s_rx = x; s_ry = y; s_rz = z;          // latest raw sample
    if (!s_pressed) { s_pressed = true; s_pressStart = now_ms; }
  } else if (s_pressed) {
    s_pressed = false;
    const uint32_t dur = (now_ms > s_pressStart) ? (now_ms - s_pressStart) : 0;
    uint16_t sx, sy;
    mapRaw(s_rx, s_ry, &sx, &sy);
    cb(sx, sy, s_rx, s_ry, s_rz, dur);
  }
}

void report() {
  Serial.printf("[touch] enabled=%s cs=%d map=%u pressed=%s reason=%s\n",
                s_enabled ? "ON" : "OFF", static_cast<int>(s_csPin),
                static_cast<unsigned>(s_map), s_pressed ? "YES" : "NO", reason());
}

}  // namespace touch
}  // namespace sunisle
