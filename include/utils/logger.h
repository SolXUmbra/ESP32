// Minimal structured logger. Kept tiny on purpose (4MB flash, no PSRAM).
#pragma once

#include <Arduino.h>

#define SUN_LOG(tag, fmt, ...) Serial.printf("[" tag "] " fmt "\n", ##__VA_ARGS__)

// Append a line to /SUNISLE/system/sync.log. Additive only: never truncates
// or deletes user data. Silently does nothing when the SD card is absent.
namespace sunisle {
void logToFile(const char* tag, const char* message);
}
