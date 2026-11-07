// Optional network transport for WorkBuddy directives (B version).
//
// The device has no WebServer (that would pull a large HTTP/TLS stack into the
// 1.25 MB app partition). Instead it polls a plain-HTTP endpoint during the
// Wi-Fi window that weather_service already opens, then feeds every response
// line into commandService::execute() - the SAME parser the serial console
// uses. Enable it by defining SUNISLE_CMD_HOST in the gitignored secrets.h.
#pragma once

#include <Arduino.h>

namespace sunisle {
namespace cmdLink {

bool enabled();
void begin();
void onNetworkWindow();   // call only while Wi-Fi is connected

}  // namespace cmdLink
}  // namespace sunisle
