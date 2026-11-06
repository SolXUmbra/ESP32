// SD card manager: mount with graceful degradation, directory bootstrap,
// non-destructive config handling and atomic replacement for OUR OWN files.
//
// DATA SAFETY: only ever creates files inside /SUNISLE/. Never deletes or
// overwrites anything the user put on the card. Photo cleanup (stage 4) must
// pass the four conditions in the task book and prefers a soft-delete bin.
#pragma once

#include <Arduino.h>

#include "sunisle/state.h"

namespace sunisle {
namespace storage {

bool begin();
bool isReady();
bool remount();                 // recovery after a mount failure

// --- Diagnostics (read-only; never creates/modifies card data) ---
// Boot-time check performed BEFORE the LCD HSPI bus is initialized, so we can
// tell whether display init is what breaks SD I/O. Mounts and unmounts again.
bool preDisplayProbe();
// Serial "SDPROBE" matrix. Tries bus/pin/clock combinations, logs which ones
// sustain a real FAT read. Leaves the card unmounted; send RESCAN to remount.
void runProbe(bool includeSharedBus);

uint64_t totalBytes();
uint64_t usedBytes();
uint64_t freeBytes();

uint16_t photoCount();          // real .jpg/.jpeg files found on SD
const char* photoPath(uint16_t index);
bool rescanPhotos();

// NEVER overwrites an existing file. Returns false if it already exists.
bool writeIfAbsent(const char* path, const char* content);
// tmp file + size check + replace. Allowed ONLY for SUNISLE-managed files.
bool atomicReplace(const char* path, const char* content);
bool appendLine(const char* path, const char* line);

// Read a whole small text file (settings / cache). False when absent.
bool readText(const char* path, String& out);

// --- Photo upload over serial (the card sits inside the frame) ---
// Writes exactly `expected` bytes taken from `src` into `path`.
// Refuses to overwrite an existing photo: the user's card is never clobbered.
bool fileExists(const char* path);
bool writeFromStream(const char* path, Stream& src, uint32_t expected,
                     uint32_t& written);
// Replace-a-photo helper for the serial DEL command. Hard-restricted to
// .jpg/.jpeg files directly inside /SUNISLE/photos/; nothing else on the
// card can be removed (config, system files and subfolders are refused).
bool deletePhoto(const char* path);

bool loadSettings(Snapshot& s);
bool saveSettings(const Snapshot& s);
void dump();

}  // namespace storage
}  // namespace sunisle
