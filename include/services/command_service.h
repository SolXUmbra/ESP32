// Unified WorkBuddy directive dispatcher (B version).
//
// Everything that can "give the device an order" goes through execute():
// the serial console, and (when enabled) the network command poll. Keeping a
// single entry point means the same CMD / SKILL / TASK messages behave
// identically no matter which transport delivered them.
#pragma once

#include <Arduino.h>

namespace sunisle {
namespace commandService {

void begin();

// Parse and run one protocol line. `reply` is always set to exactly one
// ACK/... or ERR/... line; the caller decides where to print/send it.
void execute(const String& line, String& reply);

// Convenience for the network link: number of composite tasks currently held.
uint16_t taskCount();
bool rebootPending();        // CMD|reboot is deferred until the reply is sent
void dump();                 // list skills + tasks over serial

}  // namespace commandService
}  // namespace sunisle
