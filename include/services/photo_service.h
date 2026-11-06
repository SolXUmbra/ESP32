// Photo service.
// Stage 1 manages ONLY the real file index found on SD and the slideshow
// cursor. JPEG decoding is deliberately NOT implemented yet (stage 2):
// the Home page renders an explicitly labelled empty-state illustration,
// never a fake photograph.
#pragma once

#include <Arduino.h>

namespace sunisle {
namespace photoService {

void begin();
uint16_t count();
uint16_t cursor();
const char* currentPath();     // nullptr when there are no photos
void next();
void advanceIfDue();           // automatic rotation in COMPANION mode
void rescan();
const char* statusLine();      // honest status, no invented data
void dump();

}  // namespace photoService
}  // namespace sunisle
