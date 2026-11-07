// Memo reminder service (B version).
//
// Local-first: memos live in /SUNISLE/memos/memos.tsv on the SD card, with a
// RAM copy so the UI never touches FAT from the LVGL thread. Deletion is a
// SOFT delete (state flag) and every rewrite first copies the old table to
// memos.bak. Network command delivery feeds the same add/done/snooze API,
// so there is exactly one source of truth.
#pragma once

#include <Arduino.h>
#include <stddef.h>

#include "sunisle/config.h"

namespace sunisle {
namespace memoService {

enum MemoState : uint8_t { ACTIVE = 0, DONE = 1, DELETED = 2 };
enum Repeat    : uint8_t { ONCE = 0, DAILY = 1, WEEKLY = 2 };

struct Memo {
  uint16_t id;
  uint8_t  state;
  uint8_t  repeat;
  uint32_t dueEpoch;      // 0 = untimed note, never fires
  char     text[MEMO_TEXT_MAX + 1];
};

void begin();

// All mutators persist immediately (with backup) and return false on a bad id
// or a full table. `outId` receives the id assigned to a new memo.
bool add(uint32_t dueEpoch, uint8_t repeat, const char* text, uint16_t& outId);
bool markDone(uint16_t id);     // repeating memos roll forward instead
bool snooze(uint16_t id, uint32_t ms);
bool softDelete(uint16_t id);   // moves to the recycle bin, never erases
bool setDue(uint16_t id, uint32_t dueEpoch);
bool restore(uint16_t id);      // bring a soft-deleted memo back

uint16_t activeCount();
uint16_t dueCount();
uint16_t size();                // rows in RAM, including deleted ones
const Memo* get(uint16_t index);
const Memo* find(uint16_t id);

void tick();                    // due-check + auto-dismiss of the alert
void publishState();            // refresh Snapshot counters / status line
void formatList(char* out, size_t n);   // multi-line text for the UI panel

bool alertActive();
const char* alertText();
uint16_t alertId();
void dismissAlert();

void dump();                    // serial listing, incl. recycle bin

}  // namespace memoService
}  // namespace sunisle
