// SD card manager (B version): mount with graceful degradation, directory
// bootstrap, non-destructive config handling and atomic replacement for OUR
// OWN files.
//
// DATA SAFETY: only ever creates files inside /SUNISLE/. Never deletes or
// overwrites anything the user put on the card. Memo/task removal is a SOFT
// delete (state flag in the TSV) and every rewrite keeps a .bak copy first.
#pragma once

#include <Arduino.h>

#include "sunisle/state.h"

namespace sunisle {
namespace storage {

bool begin();
bool isReady();
bool remount();                 // recovery after a mount failure

// --- Diagnostics (read-only; never creates/modifies card data) ---
bool preDisplayProbe();
void runProbe(bool includeSharedBus);

uint64_t totalBytes();
uint64_t usedBytes();
uint64_t freeBytes();

// NEVER overwrites an existing file. Returns false if it already exists.
bool writeIfAbsent(const char* path, const char* content);
// tmp file + size check + replace. Allowed ONLY for SUNISLE-managed files.
bool atomicReplace(const char* path, const char* content);
bool appendLine(const char* path, const char* line);

// Read a whole small text file (settings / memo / task / skill tables).
bool readText(const char* path, String& out);
bool fileExists(const char* path);

bool loadSettings(Snapshot& s);
bool saveSettings(const Snapshot& s);
void dump();

}  // namespace storage
}  // namespace sunisle
