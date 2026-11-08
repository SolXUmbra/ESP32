// Safe XPT2046 touch layer for the QINGYU B board (ESP32-2432S028 / "CYD").
//
// The integrated board wires its resistive XPT2046 to a DEDICATED bus that is
// SEPARATE from the LCD (HSPI 14/13/12) and the SD (VSPI 18/19/23):
//     T_CLK = 25   T_MOSI(DIN) = 32   T_MISO(DOUT) = 39
//     T_CS  = 33   T_IRQ(PENIRQ) = 36
// Every earlier probe round failed because it reused the LCD bus and only
// toggled CS lines; the controller is simply not reachable on that bus.
//
// We bit-bang the XPT2046 on its own pins. No hardware SPI peripheral is free
// (HSPI drives the panel, VSPI drives the SD card), so software SPI is the only
// safe option and it leaves both hardware buses completely untouched. The pins
// are plain GPIO (39/36 are input-only) so configuring them at boot is safe and
// does not affect the display.
//
// Press detection is gated on PENIRQ (GPIO 36, active-low): when the pen is up
// the line is pulled HIGH and we sample nothing, which avoids false triggers
// from idle ADC noise. The tap callback also reports the press DURATION so the
// host can distinguish a tap from a long-press gesture.
//
// No LVGL pointer indev is registered (per B方案_白屏二分记录.md the 30 ms
// getTouch() polling on a mis-configured bus was the prime white-screen
// suspect). Taps are reported through the callback only.
#pragma once

#include <stdint.h>

namespace sunisle {
namespace touch {

// sx,sy = logical 320x240 coords; raw_x/raw_y = raw ADC; z = pressure proxy;
// duration_ms = how long the press lasted (0 for a quick tap).
typedef void (*TapCallback)(uint16_t sx, uint16_t sy,
                            uint16_t raw_x, uint16_t raw_y, uint16_t z,
                            uint32_t duration_ms);

void begin();                       // log status only, zero GPIO activity
bool isReady();
const char* reason();

void setEnabled(bool on);
bool enabled();
void setCs(int8_t cs);              // override CS (default 33 = CYD)
void setMap(uint8_t m);             // 0..3 rotation mapping, default 1
void setMode(bool hardwareSpi);     // true = HW SPI (default), false = bit-bang
void rawDump(uint32_t idle_ms, uint32_t press_ms);  // untouched vs pressed ADC
void logDump(uint32_t ms);          // per-sample z/x/y log (free-form press test)
void debugFor(uint32_t ms);         // non-blocking tick trace (main-loop view)
void calRun(uint32_t per_spot_ms);  // 4-corner calibration, saved to NVS
void setCal(int32_t x0, int32_t x1, int32_t y0, int32_t y1, bool swap);
void setBus(int8_t clk, int8_t mosi, int8_t miso);          // reconfigure SPI bus
void setPins(int8_t clk, int8_t mosi, int8_t miso,
             int8_t cs, int8_t irq);                        // full reconfigure
void gpioScan(uint32_t window_ms);  // find PENIRQ / any pin that toggles on press
void autoDetect();                  // init bus + enable if controller present

void probe(int8_t cs = -1);         // read a few samples on the touch bus
void scan(uint32_t duration_ms);    // continuous raw dump (for press tests)
void i2cProbe();                    // rule out I2C capacitive touch panels
void i2cFind(int8_t sda, int8_t scl);   // targeted scan of one SDA/SCL pair
void spiProbe(int8_t cs, uint32_t window_ms);  // alias: scan on given CS
void misoProbe(int8_t cs);          // NOTE: bus is fixed - prints guidance
void matrixProbe();                 // NOTE: bus is fixed - prints guidance
void irqWatch(uint32_t window_ms);  // monitor the real PENIRQ (GPIO 36)
void tick(uint32_t now_ms, TapCallback cb);
void report();

}  // namespace touch
}  // namespace sunisle
