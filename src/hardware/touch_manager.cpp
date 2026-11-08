// Safe XPT2046 probe/tap layer. See include/hardware/touch_manager.h for the
// hard constraints (no indev, no spi::init(), no GPIO activity at boot).
#include "hardware/touch_manager.h"

#include <Arduino.h>
#include <string.h>
#include <Wire.h>
#include <SPI.h>
#include <esp32-hal-spi.h>
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/common.hpp>
#include <soc/spi_reg.h>

#include "sunisle/config.h"
#include "utils/logger.h"

namespace sunisle {
namespace touch {

namespace {

// Raw XPT2046 channel commands (start bit + address + 12-bit + PD0=1).
constexpr uint8_t CMD_X  = 0xD1;
constexpr uint8_t CMD_Y  = 0x91;
constexpr uint8_t CMD_Z1 = 0xB1;
constexpr uint8_t CMD_Z2 = 0xC1;

constexpr uint32_t TOUCH_SPI_HZ = 1000000;   // XPT2046 max ~2 MHz
constexpr uint32_t POLL_MS      = 40;       // low rate, no LVGL indev involved
constexpr uint16_t Z_PRESS      = 400;       // same z metric as LGFX's driver
constexpr uint16_t RAW_MIN      = 128;
constexpr uint16_t RAW_MAX      = 3968;

// Calibration is UNVERIFIED on this board; these are the LGFX defaults.
constexpr int RAW_X_MIN = 300, RAW_X_MAX = 3900;
constexpr int RAW_Y_MIN = 400, RAW_Y_MAX = 3900;

int8_t   s_cs      = 33;      // best guess, never confirmed
uint8_t  s_map     = 1;       // rotation=1 (320x240 landscape) guess
bool     s_enabled = false;
uint32_t s_lastPoll = 0;

uint8_t  s_pressStreak = 0, s_releaseStreak = 0;
bool     s_pressed = false;
uint16_t s_rx = 0, s_ry = 0, s_rz = 0;
uint8_t  s_lastRaw[3] = {0, 0, 0};

bool channel(uint8_t cmd, uint16_t* out) {
  if (s_cs < 0) return false;
  uint8_t b[3] = {cmd, 0, 0};
  lgfx::spi::beginTransaction(HSPI_HOST, TOUCH_SPI_HZ, 0);
  // lgfx::spi::readBytes() sets SPI_MOSI_DLEN_REG only. In full-duplex mode
  // the ESP32 ALSO needs SPI_MISO_DLEN_REG set, otherwise MISO shifts in a
  // single bit and every sample comes back as zeros. This is the reason the
  // first probe run looked like "no touch controller" on all CS pins.
  *reinterpret_cast<volatile uint32_t*>(SPI_MISO_DLEN_REG(HSPI_HOST + 1)) = 23;
  digitalWrite(s_cs, LOW);
  lgfx::spi::readBytes(HSPI_HOST, b, 3);   // transmits b, overwrites with MISO
  digitalWrite(s_cs, HIGH);
  lgfx::spi::endTransaction(HSPI_HOST);
  memcpy(s_lastRaw, b, 3);
  *out = static_cast<uint16_t>(((static_cast<uint16_t>(b[1]) << 8) | b[2]) >> 3);
  return true;
}

bool sample(uint16_t* x, uint16_t* y, uint16_t* z,
            uint16_t* z1_out = nullptr, uint16_t* z2_out = nullptr) {
  uint16_t z1, z2;
  if (!channel(CMD_X, x))  return false;
  if (!channel(CMD_Y, y))  return false;
  if (!channel(CMD_Z1, &z1)) return false;
  if (!channel(CMD_Z2, &z2)) return false;
  if (z1_out) *z1_out = z1;
  if (z2_out) *z2_out = z2;
  // Same pressure estimator as LovyanGFX Touch_XPT2046::getTouchRaw().
  int32_t zv = 0x3200 + static_cast<int32_t>(*y) - static_cast<int32_t>(*x)
             + ((static_cast<int32_t>(z1) - static_cast<int32_t>(z2)) >> 1);
  *z = zv > 0 ? static_cast<uint16_t>(zv) : 0;
  return true;
}

bool validPress(uint16_t x, uint16_t y, uint16_t z) {
  return x > RAW_MIN && x < RAW_MAX && y > RAW_MIN && y < RAW_MAX && z > Z_PRESS;
}

void acquire() {
  if (s_cs < 0) return;
  pinMode(s_cs, OUTPUT);
  digitalWrite(s_cs, HIGH);
}
void release() {
  if (s_cs < 0) return;
  pinMode(s_cs, INPUT);
}

void mapRaw(uint16_t rx, uint16_t ry, uint16_t* sx, uint16_t* sy) {
  // Physical panel is 240x320; setRotation(1) gives a 320x240 logical screen.
  int px = (static_cast<int>(rx) - RAW_X_MIN) * 240 / (RAW_X_MAX - RAW_X_MIN);
  int py = (static_cast<int>(ry) - RAW_Y_MIN) * 320 / (RAW_Y_MAX - RAW_Y_MIN);
  if (px < 0) px = 0; if (px > 239) px = 239;
  if (py < 0) py = 0; if (py > 319) py = 319;
  switch (s_map & 3) {
    case 0: *sx = px;        *sy = py;        break;  // portrait
    case 1: *sx = py;        *sy = 239 - px;  break;  // rotation 1 (A)
    case 2: *sx = 319 - py;  *sy = px;        break;  // rotation 1 (B)
    default: *sx = 239 - px; *sy = 319 - py;  break;
  }
}

}  // namespace

void begin() {
  // Deliberately NO pinMode / digitalWrite / spi::init() / setTouch() / indev.
  SUN_LOG("touch", "HELD OFF at boot - no GPIO touched (panel stays safe)");
  SUN_LOG("touch", "TOUCHPROBE / TOUCH=on / TOUCHSCAN / TOUCH=off available");
}

bool isReady()     { return s_enabled; }
const char* reason() {
  return s_enabled ? "SAFE POLL (no LVGL indev, no IRQ pin)"
                   : "HELD OFF (enable with TOUCH=on)";
}

void setEnabled(bool on) {
  s_enabled = on;
  s_pressed = false;
  s_pressStreak = s_releaseStreak = 0;
  s_lastPoll = millis();
  if (on) {
    acquire();
    SUN_LOG("touch", "ENABLED cs=%d map=%u 1MHz shared HSPI, NO indev, IRQ disabled",
            static_cast<int>(s_cs), static_cast<unsigned>(s_map));
  } else {
    release();
    SUN_LOG("touch", "DISABLED - CS released to INPUT");
  }
}
bool enabled() { return s_enabled; }

void setCs(int8_t cs) {
  if (s_enabled) release();
  s_cs = cs;
  if (s_enabled) acquire();
  Serial.printf("[touch] cs=%d\n", static_cast<int>(s_cs));
}
void setMap(uint8_t m) {
  s_map = m & 3;
  Serial.printf("[touch] map=%u\n", static_cast<unsigned>(s_map));
}

void probe(int8_t cs) {
  // GPIO34/35/36/39 are input-only on the ESP32 and must never be used as CS.
  const int8_t candidates[] = {33, 32, 25, 26, 27};
  const uint8_t n = cs >= 0 ? 1 : sizeof(candidates);
  const bool wasEnabled = s_enabled;
  const int8_t origCs = s_cs;
  for (uint8_t i = 0; i < n; ++i) {
    int8_t c = cs >= 0 ? cs : candidates[i];
    s_cs = c;
    acquire();
    uint16_t minx = 4095, maxx = 0, miny = 4095, maxy = 0, maxz = 0, valid = 0;
    uint16_t fx = 0, fy = 0, fz = 0, fz1 = 0, fz2 = 0;
    for (uint8_t k = 0; k < 6; ++k) {
      uint16_t x, y, z, z1, z2;
      if (!sample(&x, &y, &z, &z1, &z2)) continue;
      if (k == 0) { fx = x; fy = y; fz = z; fz1 = z1; fz2 = z2; }
      if (x < minx) minx = x; if (x > maxx) maxx = x;
      if (y < miny) miny = y; if (y > maxy) maxy = y;
      if (z > maxz) maxz = z;
      if (validPress(x, y, z)) ++valid;
      delay(12);
    }
    Serial.printf("[touchprobe] cs=%d first x=%u y=%u z1=%u z2=%u z=%u | "
                  "x=%u..%u y=%u..%u zmax=%u valid=%u/6 | lastSPI=%02X %02X %02X\n",
                  static_cast<int>(c), fx, fy, fz1, fz2, fz,
                  minx, maxx, miny, maxy, maxz, valid,
                  s_lastRaw[0], s_lastRaw[1], s_lastRaw[2]);
    release();
  }
  s_cs = origCs;
  if (wasEnabled) acquire();
  Serial.println(F("[touchprobe] IRQ pin intentionally NOT used; press during TOUCHSCAN to verify"));
}

void scan(uint32_t duration_ms) {
  if (s_cs < 0) { Serial.println(F("[touchscan] no CS selected")); return; }
  const bool wasEnabled = s_enabled;
  acquire();
  const uint32_t t0 = millis();
  uint32_t frames = 0, hits = 0;
  uint16_t minx = 4095, maxx = 0, miny = 4095, maxy = 0, maxz = 0;
  Serial.printf("[touchscan] START cs=%d for %lu ms - PRESS THE PANEL NOW\n",
                static_cast<int>(s_cs), static_cast<unsigned long>(duration_ms));
  while (millis() - t0 < duration_ms) {
    uint16_t x, y, z;
    if (sample(&x, &y, &z)) {
      ++frames;
      if (x < minx) minx = x; if (x > maxx) maxx = x;
      if (y < miny) miny = y; if (y > maxy) maxy = y;
      if (z > maxz) maxz = z;
      if (validPress(x, y, z)) {
        ++hits;
        uint16_t sx, sy;
        mapRaw(x, y, &sx, &sy);
        Serial.printf("[touchscan] t=%lu raw=%u,%u z=%u -> screen=%u,%u\n",
                      static_cast<unsigned long>(millis() - t0), x, y, z, sx, sy);
      }
    }
    delay(20);
  }
  Serial.printf("[touchscan] DONE frames=%lu hits=%lu x=%u..%u y=%u..%u zmax=%u\n",
                static_cast<unsigned long>(frames), static_cast<unsigned long>(hits),
                minx, maxx, miny, maxy, maxz);
  if (!wasEnabled) release();
}

void i2cProbe() {
  // Some ST7789 boards ship an I2C capacitive touch controller (GT911 0x5D /
  // 0x14, FT6236 0x38, CST816 0x15) instead of an XPT2046. Only pins that are
  // NOT used by LCD / SD / backlight / boot strapping are probed.
  static const int8_t kPairs[][2] = {
      {33, 32}, {32, 33}, {25, 26}, {26, 25}, {16, 17}, {17, 16}, {27, 26}};
  for (const auto& p : kPairs) {
    Wire.begin(p[0], p[1]);
    Wire.setClock(100000);
    uint8_t found = 0;
    uint8_t addrs[8] = {0};
    for (uint8_t a = 0x08; a <= 0x77; ++a) {
      Wire.beginTransmission(a);
      if (Wire.endTransmission() == 0) {
        if (found < sizeof(addrs)) addrs[found] = a;
        ++found;
      }
    }
    Serial.printf("[i2cprobe] sda=%d scl=%d devices=%u", p[0], p[1], found);
    for (uint8_t i = 0; i < found && i < sizeof(addrs); ++i)
      Serial.printf(" 0x%02X", addrs[i]);
    Serial.println();
    Wire.end();
  }
  Serial.println(F("[i2cprobe] done - no address means no capacitive controller on these pins"));
}

// ---- second-generation probes (user confirmed the panel IS touch-capable) --
// The first three probe rounds all used lgfx::spi::readBytes(). That function
// is a PANEL-ID helper: it copies `data` into SPI_W0 as MOSI and reads W0 back,
// but it relies on the LGFX bus init having enabled SPI_USR_MISO/DLEN. On a
// write-only panel bus that is not guaranteed, which is a plausible reason for
// the "00 00 00 on every CS" result. Arduino's SPIClass is a completely
// independent full-duplex path on the SAME HSPI pins (14/12/13), so it neither
// detaches the GPIO matrix nor re-runs LGFX's panel init.
SPIClass* s_arduinoSpi = nullptr;

SPIClass* arduinoSpi() {
  if (!s_arduinoSpi) {
    s_arduinoSpi = new SPIClass(HSPI);
    s_arduinoSpi->begin(LCD_SCLK, LCD_MISO, LCD_MOSI, 33);
    Serial.println(F("[spiprobe] Arduino SPIClass(HSPI) ready clk=14 miso=12 mosi=13 @1MHz"));
  }
  return s_arduinoSpi;
}

uint16_t spiRead(int8_t cs, uint8_t cmd, uint8_t raw[3]) {
  SPIClass* spi = arduinoSpi();
  pinMode(cs, OUTPUT);
  digitalWrite(cs, HIGH);
  delayMicroseconds(2);
  spi->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  digitalWrite(cs, LOW);
  raw[0] = spi->transfer(cmd);
  raw[1] = spi->transfer(0x00);
  raw[2] = spi->transfer(0x00);
  digitalWrite(cs, HIGH);
  spi->endTransaction();
  return static_cast<uint16_t>(((raw[1] << 8) | raw[2]) >> 3);
}

void spiProbe(int8_t cs, uint32_t window_ms) {
  static const int8_t kCs[] = {33, 32, 25, 26, 27, 16, 17, 4, 22, 21};
  if (cs < 0) {
    Serial.println(F("[spiprobe] full-duplex CS scan (Arduino SPI, MISO=12)"));
    for (int8_t c : kCs) {
      uint8_t b[3];
      uint16_t v = spiRead(c, 0xD1, b);
      Serial.printf("[spiprobe] cs=%d D1 -> %02X %02X %02X  x=%u\n",
                     static_cast<int>(c), b[0], b[1], b[2], v);
      pinMode(c, INPUT);
    }
    Serial.println(F("[spiprobe] scan done; TOUCHSPI=<cs>,<ms> for a press window"));
    return;
  }
  // Single CS, continuous window: only print CHANGES so a 12 s press test
  // does not flood the serial log.
  const uint32_t t0 = millis();
  uint32_t frames = 0, nonzero = 0, changes = 0;
  uint16_t minv = 4095, maxv = 0;
  uint8_t prev[3] = {0xFF, 0xFF, 0xFF};
  Serial.printf("[spiprobe] cs=%d window=%lu ms - PRESS THE PANEL NOW\n",
                static_cast<int>(cs), static_cast<unsigned long>(window_ms));
  while (millis() - t0 < window_ms) {
    uint8_t b[3];
    uint16_t v = spiRead(cs, 0xD1, b);
    ++frames;
    if (b[0] || b[1] || b[2]) ++nonzero;
    if (v < minv) minv = v;
    if (v > maxv) maxv = v;
    if (memcmp(b, prev, 3) != 0) {
      memcpy(prev, b, 3);
      ++changes;
      if (changes <= 40)
        Serial.printf("[spiprobe] t=%lu %02X %02X %02X x=%u\n",
                      static_cast<unsigned long>(millis() - t0), b[0], b[1], b[2], v);
    }
    delay(30);
  }
  Serial.printf("[spiprobe] DONE frames=%lu nonzero=%lu changes=%lu x=%u..%u\n",
                static_cast<unsigned long>(frames), static_cast<unsigned long>(nonzero),
                static_cast<unsigned long>(changes), minv, maxv);
  pinMode(cs, INPUT);
}

void misoProbe(int8_t cs) {
  // Every earlier probe assumed touch DOUT shares GPIO12 with the LCD. Many
  // integrated ST7789 boards instead wire the touch controller to a separate
  // input-only pin (34/35/36/39) to avoid bus contention. Re-route the HSPI
  // Q input through the GPIO matrix; the peripheral and the panel's
  // CLK/MOSI are never re-initialised, and 12 is restored at the end.
  // 19/23 are the SD card's VSPI MISO/MOSI. Routing them to HSPI Q would
  // force them to INPUT and break the SD card until the next reboot, so they
  // are deliberately EXCLUDED from every pin scan.
  static const int8_t kMiso[] = {12, 22, 25, 26, 27, 32, 33,
                                  34, 35, 36, 39, 4, 16, 17};
  SPIClass* spi = arduinoSpi();
  int8_t prev = LCD_MISO;
  Serial.printf("[misoprobe] cs=%d clk=14 mosi=13; %u MISO candidates\n",
                static_cast<int>(cs),
                static_cast<unsigned>(sizeof(kMiso) / sizeof(kMiso[0])));
  for (int8_t m : kMiso) {
    spiDetachMISO(spi->bus(), prev);
    spiAttachMISO(spi->bus(), m);
    prev = m;
    if (m == cs) {
      Serial.printf("[misoprobe] miso=%2d SKIP (same pin as CS)\n", cs);
      continue;
    }
    uint8_t b[3];
    uint16_t v = spiRead(cs, 0xD1, b);
    Serial.printf("[misoprobe] miso=%2d -> %02X %02X %02X x=%u\n",
                  static_cast<int>(m), b[0], b[1], b[2], v);
  }
  spiDetachMISO(spi->bus(), prev);
  spiAttachMISO(spi->bus(), LCD_MISO);
  pinMode(cs, INPUT);
  Serial.println(F("[misoprobe] MISO restored to 12 (panel bus untouched)"));
}

void matrixProbe() {
  // Last autonomous probe: cross-product of MISO x CS. A real XPT2046 that
  // gets selected will drive the MISO line with 12-bit ADC data instead of
  // leaving it floating (constant 00 or FF). 4 reads per cell also reveal a
  // line that is actually alive. No press required.
  static const int8_t kMiso[] = {12, 22, 25, 26, 27, 32, 33,
                                  34, 35, 36, 39, 4, 16, 17};
  // LCD_CS(15), LCD_DC(2), SD_CS(5) and BL(21) are NEVER toggled: pulling
  // them low would inject our 0xD1 into the panel / SD / backlight.
  static const int8_t kCs[]   = {33, 32, 25, 26, 27, 16, 17, 4, 22};
  SPIClass* spi = arduinoSpi();
  int8_t prev = LCD_MISO;
  uint32_t cells = 0, interesting = 0, printed = 0;
  Serial.println(F("[matrix] MISO x CS full-duplex scan, 4 reads/cell"));
  for (int8_t m : kMiso) {
    spiDetachMISO(spi->bus(), prev);
    spiAttachMISO(spi->bus(), m);
    prev = m;
    for (int8_t c : kCs) {
      if (c == m) continue;
      uint8_t first[3] = {0, 0, 0}, b[3];
      uint16_t vmin = 4095, vmax = 0;
      for (uint8_t k = 0; k < 4; ++k) {
        const uint16_t v = spiRead(c, 0xD1, b);
        if (k == 0) memcpy(first, b, 3);
        if (v < vmin) vmin = v;
        if (v > vmax) vmax = v;
      }
      ++cells;
      const bool allZero = !first[0] && !first[1] && !first[2];
      const bool allOne  = first[0] == 0xFF && first[1] == 0xFF && first[2] == 0xFF;
      if ((!allZero && !allOne) || vmin != vmax) {
        ++interesting;
        if (printed < 40) {
          ++printed;
          Serial.printf("[matrix] miso=%2d cs=%2d -> %02X %02X %02X x=%u..%u\n",
                        static_cast<int>(m), static_cast<int>(c),
                        first[0], first[1], first[2], vmin, vmax);
        }
      }
      pinMode(c, INPUT);
    }
  }
  spiDetachMISO(spi->bus(), prev);
  spiAttachMISO(spi->bus(), LCD_MISO);
  Serial.printf("[matrix] DONE cells=%lu interesting=%lu (printed=%lu); MISO restored to 12\n",
                static_cast<unsigned long>(cells),
                static_cast<unsigned long>(interesting),
                static_cast<unsigned long>(printed));
}

void irqWatch(uint32_t window_ms) {
  // XPT2046 PENIRQ is open-drain and goes LOW on press. Watching READ-ONLY
  // GPIO levels needs no CS/MISO guess: a pin that flips exactly during a
  // press proves the touch panel is wired to this ESP32 at all.
  // GPIO33 is driven as the XPT2046 CS by the SPI read inside this same
  // window, so it cannot also be an IRQ candidate.
  static const int8_t kPins[] = {4, 16, 17, 22, 25, 26, 27, 32,
                                  34, 35, 36, 39};
  constexpr uint8_t N = sizeof(kPins) / sizeof(kPins[0]);
  int last[N];
  uint32_t flips[N] = {0};
  uint32_t spiFrames = 0, spiChanges = 0;
  uint8_t spiPrev[3] = {0xFF, 0xFF, 0xFF};
  uint32_t lastSpi = 0;
  for (uint8_t i = 0; i < N; ++i) {
    pinMode(kPins[i], INPUT);
    last[i] = digitalRead(kPins[i]);
  }
  Serial.printf("[irqwatch] %lu ms - PRESS THE PANEL NOW; baseline:",
                 static_cast<unsigned long>(window_ms));
  for (uint8_t i = 0; i < N; ++i)
    Serial.printf(" %d=%d", kPins[i], last[i]);
  Serial.println();
  const uint32_t t0 = millis();
  uint32_t samples = 0;
  while (millis() - t0 < window_ms) {
    for (uint8_t i = 0; i < N; ++i) {
      const int v = digitalRead(kPins[i]);
      if (v != last[i]) {
        last[i] = v;
        ++flips[i];
        if (flips[i] <= 6)
          Serial.printf("[irqwatch] t=%lu gpio%d -> %d\n",
                        static_cast<unsigned long>(millis() - t0), kPins[i], v);
      }
    }
    // Same press window also samples the controller through Arduino SPI, so
    // ONE 15 s press yields both the IRQ evidence and the raw X/Y evidence.
    if (millis() - lastSpi >= 100) {
      lastSpi = millis();
      uint8_t b[3];
      uint16_t v = spiRead(33, 0xD1, b);
      ++spiFrames;
      if (memcmp(b, spiPrev, 3) != 0) {
        memcpy(spiPrev, b, 3);
        ++spiChanges;
        if (spiChanges <= 20)
          Serial.printf("[irqwatch] spi33 t=%lu %02X %02X %02X x=%u\n",
                        static_cast<unsigned long>(millis() - t0), b[0], b[1], b[2], v);
      }
    }
    ++samples;
    delay(20);
  }
  pinMode(33, INPUT);
  Serial.printf("[irqwatch] DONE samples=%lu changes:", static_cast<unsigned long>(samples));
  for (uint8_t i = 0; i < N; ++i)
    if (flips[i]) Serial.printf(" gpio%d=%lu", kPins[i],
                                static_cast<unsigned long>(flips[i]));
  Serial.printf(" | spi33 frames=%lu changes=%lu\n",
                static_cast<unsigned long>(spiFrames),
                static_cast<unsigned long>(spiChanges));
  Serial.println(F("  (a line flipping on press = PENIRQ candidate)"));
}

void tick(uint32_t now_ms, TapCallback cb) {
  if (!s_enabled || !cb) return;
  if (now_ms - s_lastPoll < POLL_MS) return;
  s_lastPoll = now_ms;
  uint16_t x, y, z;
  if (!sample(&x, &y, &z)) return;
  if (validPress(x, y, z)) {
    s_releaseStreak = 0;
    if (s_pressStreak < 250) ++s_pressStreak;
    if (s_pressStreak >= 2 && !s_pressed) {
      s_pressed = true;
      s_rx = x; s_ry = y; s_rz = z;
    }
  } else {
    s_pressStreak = 0;
    if (s_releaseStreak < 250) ++s_releaseStreak;
    if (s_releaseStreak >= 2 && s_pressed) {
      s_pressed = false;
      uint16_t sx, sy;
      mapRaw(s_rx, s_ry, &sx, &sy);
      cb(sx, sy, s_rx, s_ry, s_rz);
    }
  }
}

void report() {
  Serial.printf("[touch] enabled=%s cs=%d map=%u pressed=%s reason=%s\n",
                s_enabled ? "ON" : "OFF", static_cast<int>(s_cs),
                static_cast<unsigned>(s_map), s_pressed ? "YES" : "NO", reason());
}

}  // namespace touch
}  // namespace sunisle
