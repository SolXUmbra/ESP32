// Daily quote ("每日一言") service.
//
// Offline by design: a live quote API returns ARBITRARY Chinese that a subset
// font can never cover, and the app partition is only ~1.25 MB. The quote is
// instead a fixed, curated list, rotated once per BOOT:
//   * the font ships ONLY the glyphs these sentences use (see gen_cn_font.py),
//   * a boot counter persisted in NVS advances on every start, so two
//     consecutive boots always show different sentences,
//   * once chosen it stays for the whole session (no mid-boot jump after NTP),
//   * it needs no network and no SD card.
#pragma once

#include <Arduino.h>

namespace sunisle {
namespace quoteService {

void begin();           // advance the NVS boot counter, pick & publish once
void tick();            // no-op unless the quote was never published
void dump();

}  // namespace quoteService
}  // namespace sunisle
