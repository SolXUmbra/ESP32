// Daily quote ("每日一言") service.
//
// Offline by design: a live quote API returns ARBITRARY Chinese that a subset
// font can never cover, and the app partition is only ~1.25 MB. The quote is
// instead a fixed, curated list rotated deterministically by local date, so:
//   * the font ships ONLY the glyphs these sentences use (see gen_cn_font.py),
//   * the same sentence stays up for one full day (a "daily" quote),
//   * it needs no network and no storage.
//
// The date used is the local (CST-8) day; before NTP sync it falls back to a
// fixed seed so there is always exactly one quote shown.
#pragma once

#include <Arduino.h>

namespace sunisle {
namespace quoteService {

void begin();           // pick today's quote (date-aware) and publish it
void tick();            // re-pick if the local day rolled over
void dump();

}  // namespace quoteService
}  // namespace sunisle
