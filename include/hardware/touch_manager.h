// Safe, reversible XPT2046 probe/tap layer for the QINGYU B board.
//
// Hard constraints from B方案_白屏二分记录.md / HARDWARE_SPEC.md:
//   * NEVER register an LVGL pointer indev (30 ms getTouch() polling is the
//     prime white-screen suspect).
//   * NEVER call lgfx::spi::init() for the touch (it reconfigures the HSPI
//     peripheral the panel is using).
//   * NEVER touch ANY touch GPIO at boot. Power-on display behaviour is
//     bit-for-bit identical to the touch-disabled firmware.
//   * pin_int is NOT used: the old pin_int=36 guess made
//     Touch_XPT2046::getTouchRaw() return early whenever that line was high,
//     so a real press could never be reported.
//
// This layer only issues low-frequency SPI transactions on the already
// initialised HSPI bus, reads the XPT2046 raw channels itself, and reports
// taps through a callback. Everything is gated behind TOUCH=on and can be
// switched off (or rebooted) at any time.
//
// 2026-11-08 09:40 - user confirmed the integrated board IS touch-capable
// and that presses really happened. lgfx::spi::readBytes() is a panel-ID
// helper, not a guaranteed full-duplex controller read, so two independent
// zero-GPIO-reconfiguration probes were added:
//   spiProbe()  - Arduino SPIClass full-duplex transfer() on the SAME HSPI
//                 pins (14/12/13). No pinMode on clk/mosi, no matrix detach.
//   irqWatch()  - read-only GPIO level monitor to catch XPT2046 PENIRQ.
#pragma once

#include <stdint.h>

namespace sunisle {
namespace touch {

typedef void (*TapCallback)(uint16_t sx, uint16_t sy,
                            uint16_t raw_x, uint16_t raw_y, uint16_t z);

void begin();                       // log status only, zero GPIO activity
bool isReady();
const char* reason();

void setEnabled(bool on);
bool enabled();
void setCs(int8_t cs);              // default 33 (best guess, unverified)
void setMap(uint8_t m);             // 0..3 rotation mapping, default 1
void probe(int8_t cs = -1);         // -1 = scan 33 / 32 / 34
void scan(uint32_t duration_ms);    // continuous raw dump (for press tests)
void i2cProbe();                    // rule out I2C capacitive touch panels
void spiProbe(int8_t cs, uint32_t window_ms);  // Arduino SPI full-duplex
void misoProbe(int8_t cs);             // re-route HSPI MISO input, scan pins
void matrixProbe();                    // MISO x CS cross-product, no press needed
void irqWatch(uint32_t window_ms);  // read-only PENIRQ / GPIO level monitor
void tick(uint32_t now_ms, TapCallback cb);
void report();

}  // namespace touch
}  // namespace sunisle
