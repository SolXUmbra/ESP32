// XPT2046 driver for the QINGYU B board (ESP32-2432S028 / "CYD").
//
// Resistive touch controller on its OWN bus:
//   T_CLK=25  T_MOSI(DIN)=32  T_MISO(DOUT)=39  T_CS=33  T_IRQ(PENIRQ)=36
//
// Software (bit-bang) SPI keeps the LCD HSPI and SD VSPI buses untouched.
// PENIRQ gates sampling so idle ADC noise never produces a false tap.
#include "hardware/touch_manager.h"

#include <Arduino.h>
#include <Wire.h>
#include <string.h>

#include "sunisle/config.h"
#include "utils/logger.h"

namespace sunisle {
namespace touch {
namespace {

// ---- CYD dedicated touch bus --------------------------------------------
constexpr int8_t TOUCH_CLK  = 25;
constexpr int8_t TOUCH_MOSI = 32;   // DIN
constexpr int8_t TOUCH_MISO = 39;   // DOUT (input-only)
constexpr int8_t TOUCH_CS_DEFAULT = 33;  // CYD default; reconfigurable at runtime
constexpr int8_t TOUCH_IRQ  = 36;   // PENIRQ, active-low (input-only)

constexpr uint32_t POLL_MS  = 30;   // ~33 Hz; no LVGL indev involved
constexpr int      RAW_X_MIN = 300, RAW_X_MAX = 3900;
constexpr int      RAW_Y_MIN = 400, RAW_Y_MAX = 3900;

// XPT2046 channel commands (start bit + addr + 12-bit + keep-Vref-on).
constexpr uint8_t CMD_X  = 0xD1;
constexpr uint8_t CMD_Y  = 0x91;
constexpr uint8_t CMD_Z1 = 0xB1;
constexpr uint8_t CMD_Z2 = 0xC1;

bool     s_enabled   = false;
bool     s_initialized = false;
int8_t   s_csPin     = TOUCH_CS_DEFAULT;  // active CS, reconfigurable
uint8_t  s_map       = 1;           // rotation 1 -> logical 320x240
uint32_t s_lastPoll  = 0;

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
  if (s_initialized) return;
  pinMode(TOUCH_CLK, OUTPUT);
  pinMode(TOUCH_MOSI, OUTPUT);
  pinMode(TOUCH_MISO, INPUT);
  pinMode(TOUCH_IRQ, INPUT);         // board has the pull-up; no internal needed
  setCsPin(TOUCH_CS_DEFAULT);        // CS=33 by default
  digitalWrite(TOUCH_CLK, LOW);
  digitalWrite(TOUCH_MOSI, LOW);
  s_initialized = true;
  Serial.println(F("[touch] bus initialised: clk=25 mosi=32 miso=39 cs=33 irq=36"));
}

// Software SPI: send 8-bit cmd MSB-first, read 16-bit result MSB-first.
// The 12-bit ADC value lands in bits [14:3] of the 16-bit word.
uint16_t readChannel(uint8_t cmd) {
  digitalWrite(s_csPin, LOW);
  for (int i = 7; i >= 0; --i) {
    digitalWrite(TOUCH_MOSI, (cmd >> i) & 1 ? HIGH : LOW);
    digitalWrite(TOUCH_CLK, HIGH);
    digitalWrite(TOUCH_CLK, LOW);
  }
  uint16_t data = 0;
  for (int i = 0; i < 16; ++i) {
    digitalWrite(TOUCH_CLK, HIGH);
    data <<= 1;
    if (digitalRead(TOUCH_MISO)) data |= 1;
    digitalWrite(TOUCH_CLK, LOW);
  }
  digitalWrite(s_csPin, HIGH);
  return static_cast<uint16_t>((data >> 3) & 0x0FFF);
}

bool sample(uint16_t* x, uint16_t* y, uint16_t* z) {
  const uint16_t xr = readChannel(CMD_X);
  const uint16_t yr = readChannel(CMD_Y);
  const uint16_t z1 = readChannel(CMD_Z1);
  const uint16_t z2 = readChannel(CMD_Z2);
  int32_t zv = 0x3200 + static_cast<int32_t>(yr) - static_cast<int32_t>(xr)
             + ((static_cast<int32_t>(z1) - static_cast<int32_t>(z2)) >> 1);
  *x = xr; *y = yr; *z = zv > 0 ? static_cast<uint16_t>(zv) : 0;
  return true;
}

bool validXY(uint16_t x, uint16_t y) {
  return x > 128 && x < 3968 && y > 128 && y < 3968;
}

void mapRaw(uint16_t rx, uint16_t ry, uint16_t* sx, uint16_t* sy) {
  // Physical panel 240x320; setRotation(1) -> logical 320x240.
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
  Serial.println(F("[touch] layer ready - bus pins configured lazily by autoDetect()"));
}

bool isReady() { return s_enabled; }
const char* reason() {
  return s_enabled ? "CYD XPT2046 (clk=25 mosi=32 miso=39 cs=33 irq=36), PENIRQ-gated"
                   : "disabled (enable with TOUCH=on or autoDetect)";
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

void autoDetect() {
  hwInit();
  s_enabled = true;
  s_pressed = false;
  s_lastPoll = millis();
  // Sanity: PENIRQ should be HIGH (pen up) at idle on a real CYD.
  const int irq = digitalRead(TOUCH_IRQ);
  Serial.printf("[touch] autoDetect: bus enabled, PENIRQ(idle)=%d "
                "(expect 1). Press the panel and watch TOUCHSCAN.\n", irq);
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
  // Some ST7789 boards ship an I2C capacitive controller instead; still worth
  // ruling out. Only pins NOT used by LCD / SD / backlight are scanned.
  static const int8_t kPairs[][2] = {
      {33, 32}, {32, 33}, {25, 26}, {26, 25}, {16, 17}, {17, 16}, {27, 26}};
  for (const auto& p : kPairs) {
    Wire.begin(p[0], p[1]);
    Wire.setClock(100000);
    uint8_t found = 0, addrs[8] = {0};
    for (uint8_t a = 0x08; a <= 0x77; ++a) {
      Wire.beginTransmission(a);
      if (Wire.endTransmission() == 0 && found < sizeof(addrs)) addrs[found++] = a;
    }
    Serial.printf("[i2cprobe] sda=%d scl=%d devices=%u", p[0], p[1], found);
    for (uint8_t i = 0; i < found; ++i) Serial.printf(" 0x%02X", addrs[i]);
    Serial.println();
    Wire.end();
  }
  Serial.println(F("[i2cprobe] done"));
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
  int last = digitalRead(TOUCH_IRQ);
  uint32_t flips = 0;
  Serial.printf("[irqwatch] %lu ms - PRESS THE PANEL. PENIRQ(idle)=%d "
                "(expect 1 up / 0 down on press)\n",
                static_cast<unsigned long>(window_ms), last);
  while (millis() - t0 < window_ms) {
    const int v = digitalRead(TOUCH_IRQ);
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

  // Coordinate-based detection (robust whether or not PENIRQ is wired): a real
  // touch yields X/Y inside the valid ADC window; an untouched panel reads ~0
  // or noise. A 2-sample debounce on each edge rejects idle glitches.
  uint16_t x, y, z;
  if (!sample(&x, &y, &z)) return;
  const bool pressed = validXY(x, y);
  if (pressed) {
    if (s_pressStreak < 250) ++s_pressStreak;
    s_releaseStreak = 0;
    if (s_pressStreak >= 2) {                 // debounced pen-down
      if (!s_pressed) { s_pressed = true; s_pressStart = now_ms; }
      s_rx = x; s_ry = y; s_rz = z;            // keep latest
    }
  } else {
    if (s_releaseStreak < 250) ++s_releaseStreak;
    s_pressStreak = 0;
    if (s_releaseStreak >= 2 && s_pressed) {   // debounced pen-up -> tap
      s_pressed = false;
      const uint32_t dur = (now_ms > s_pressStart) ? (now_ms - s_pressStart) : 0;
      uint16_t sx, sy;
      mapRaw(s_rx, s_ry, &sx, &sy);
      cb(sx, sy, s_rx, s_ry, s_rz, dur);
    }
  }
}

void report() {
  Serial.printf("[touch] enabled=%s cs=%d map=%u pressed=%s reason=%s\n",
                s_enabled ? "ON" : "OFF", static_cast<int>(s_csPin),
                static_cast<unsigned>(s_map), s_pressed ? "YES" : "NO", reason());
}

}  // namespace touch
}  // namespace sunisle
