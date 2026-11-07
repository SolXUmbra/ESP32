// Memo reminder service (B version). See header for the data-safety contract.
#include "services/memo_service.h"

#include "sunisle/config.h"
#include "sunisle/state.h"
#include "services/time_service.h"
#include "hardware/storage_manager.h"
#include "utils/logger.h"

#include <time.h>
#include <stdlib.h>
#include <string.h>

namespace sunisle {
namespace memoService {
namespace {

Memo     s_memos[MEMO_MAX_ENTRIES];
uint16_t s_count = 0;
uint16_t s_nextId = 1;

uint16_t  s_alertId = 0;          // 0 = no alert on screen
uint32_t  s_alertSinceMs = 0;
uint32_t  s_lastFired[MEMO_MAX_ENTRIES];  // epoch of the last alert per row

// TSV has only five columns, so these characters must never appear in text.
void sanitise(char* s) {
  for (char* p = s; *p; ++p) {
    if (*p == '\t' || *p == '\n' || *p == '\r' || *p == '|') *p = ' ';
  }
}

Memo* findMutable(uint16_t id) {
  for (uint16_t i = 0; i < s_count; ++i)
    if (s_memos[i].id == id) return &s_memos[i];
  return nullptr;
}

uint16_t slotOf(uint16_t id) {
  for (uint16_t i = 0; i < s_count; ++i)
    if (s_memos[i].id == id) return i;
  return 0xFFFF;
}

bool parseRow(const String& line, Memo& out) {
  // id \t state \t repeat \t dueEpoch \t text
  int a = 0, b = line.indexOf('\t', a);
  if (b < 0) return false;
  long id = line.substring(a, b).toInt();
  a = b + 1; b = line.indexOf('\t', a);
  if (b < 0) return false;
  long st = line.substring(a, b).toInt();
  a = b + 1; b = line.indexOf('\t', a);
  if (b < 0) return false;
  long rep = line.substring(a, b).toInt();
  a = b + 1; b = line.indexOf('\t', a);
  if (b < 0) return false;
  unsigned long due = line.substring(a, b).toInt();
  String text = line.substring(b + 1);
  text.trim();
  if (id <= 0 || id > 65535 || text.length() == 0) return false;

  memset(&out, 0, sizeof(out));
  out.id = static_cast<uint16_t>(id);
  out.state = static_cast<uint8_t>(st);
  out.repeat = static_cast<uint8_t>(rep);
  out.dueEpoch = static_cast<uint32_t>(due);
  strncpy(out.text, text.c_str(), MEMO_TEXT_MAX);
  out.text[MEMO_TEXT_MAX] = 0;
  return true;
}

void load() {
  s_count = 0;
  s_nextId = 1;
  String txt;
  if (!storage::readText(FILE_MEMOS, txt)) {
    SUN_LOG("memo", "no memos.tsv yet - starting with an empty table");
    return;
  }
  int from = 0;
  while (from < (int)txt.length() && s_count < MEMO_MAX_ENTRIES) {
    int to = txt.indexOf('\n', from);
    if (to < 0) to = txt.length();
    String line = txt.substring(from, to);
    line.trim();
    from = to + 1;
    if (line.length() == 0 || line[0] == '#') continue;
    Memo m;
    if (!parseRow(line, m)) continue;
    s_memos[s_count++] = m;
    if (m.id >= s_nextId) s_nextId = m.id + 1;
    uint16_t slot = s_count - 1;
    s_lastFired[slot] = 0;
  }
  SUN_LOG("memo", "loaded %u row(s) from SD", static_cast<unsigned>(s_count));
}

bool save() {
  if (!storage::isReady()) {
    SUN_LOG("memo", "WARN SD offline - changes are RAM-only this session");
    return false;
  }
  // DATA SAFETY: keep the previous table before rewriting ours.
  String old;
  if (storage::readText(FILE_MEMOS, old) && old.length())
    storage::atomicReplace(FILE_MEMOS_BAK, old.c_str());

  String out = "# id\tstate\trepeat\tdueEpoch\ttext\n";
  for (uint16_t i = 0; i < s_count; ++i) {
    const Memo& m = s_memos[i];
    out += String(m.id) + "\t" + String(m.state) + "\t" +
           String(m.repeat) + "\t" + String((unsigned long)m.dueEpoch) +
           "\t" + String(m.text) + "\n";
  }
  const bool ok = storage::atomicReplace(FILE_MEMOS, out.c_str());
  if (!ok) SUN_LOG("memo", "ERROR saving memos.tsv (RAM copy still valid)");
  return ok;
}

void hhmm(uint32_t epoch, char* out, size_t n) {
  if (!epoch) { snprintf(out, n, "  -  "); return; }
  time_t t = static_cast<time_t>(epoch);
  struct tm lt;
  if (!localtime_r(&t, &lt)) { snprintf(out, n, "??:??"); return; }
  snprintf(out, n, "%02d:%02d", lt.tm_hour, lt.tm_min);
}

}  // namespace

void begin() {
  memset(s_memos, 0, sizeof(s_memos));
  memset(s_lastFired, 0, sizeof(s_lastFired));
  s_alertId = 0;
  s_alertSinceMs = 0;
  load();
  publishState();
  SUN_LOG("memo", "ready: %u active, %u due (clock %s)",
          static_cast<unsigned>(activeCount()),
          static_cast<unsigned>(dueCount()),
          timeService::isSynced() ? "SYNCED" : "UNSYNCED");
}

bool add(uint32_t dueEpoch, uint8_t repeat, const char* text, uint16_t& outId) {
  if (!text || !*text || s_count >= MEMO_MAX_ENTRIES) return false;
  Memo& m = s_memos[s_count];
  memset(&m, 0, sizeof(m));
  m.id = s_nextId++;
  m.state = ACTIVE;
  m.repeat = repeat > WEEKLY ? ONCE : repeat;
  m.dueEpoch = dueEpoch;
  strncpy(m.text, text, MEMO_TEXT_MAX);
  m.text[MEMO_TEXT_MAX] = 0;
  sanitise(m.text);
  s_lastFired[s_count] = 0;
  ++s_count;
  outId = m.id;
  save();
  publishState();
  SUN_LOG("memo", "ADD id=%u due=%lu repeat=%u text=%s",
          static_cast<unsigned>(m.id),
          static_cast<unsigned long>(m.dueEpoch),
          static_cast<unsigned>(m.repeat), m.text);
  return true;
}

bool markDone(uint16_t id) {
  Memo* m = findMutable(id);
  if (!m || m->state == DELETED) return false;
  if (m->repeat != ONCE && m->dueEpoch) {
    m->dueEpoch += (m->repeat == DAILY ? 86400UL : 604800UL);
    uint16_t slot = slotOf(id);
    if (slot != 0xFFFF) s_lastFired[slot] = 0;
    SUN_LOG("memo", "DONE id=%u -> rolled to %lu",
            static_cast<unsigned>(id),
            static_cast<unsigned long>(m->dueEpoch));
  } else {
    m->state = DONE;
  }
  if (s_alertId == id) dismissAlert();
  save();
  publishState();
  return true;
}

bool snooze(uint16_t id, uint32_t ms) {
  Memo* m = findMutable(id);
  if (!m || m->state != ACTIVE) return false;
  const uint32_t now = timeService::epoch();
  if (!now) return false;
  m->dueEpoch = now + (ms ? ms / 1000UL : SNOOZE_DEFAULT_MS / 1000UL);
  uint16_t slot = slotOf(id);
  if (slot != 0xFFFF) s_lastFired[slot] = 0;
  if (s_alertId == id) dismissAlert();
  save();
  publishState();
  SUN_LOG("memo", "SNOOZE id=%u -> %lu", static_cast<unsigned>(id),
          static_cast<unsigned long>(m->dueEpoch));
  return true;
}

bool softDelete(uint16_t id) {
  Memo* m = findMutable(id);
  if (!m) return false;
  m->state = DELETED;      // stays on the card = recycle bin
  if (s_alertId == id) dismissAlert();
  save();
  publishState();
  SUN_LOG("memo", "SOFT-DELETE id=%u (row kept, restore with MEMO RESTORE)",
          static_cast<unsigned>(id));
  return true;
}

bool setDue(uint16_t id, uint32_t dueEpoch) {
  Memo* m = findMutable(id);
  if (!m || m->state == DELETED) return false;
  m->dueEpoch = dueEpoch;
  uint16_t slot = slotOf(id);
  if (slot != 0xFFFF) s_lastFired[slot] = 0;
  save();
  publishState();
  return true;
}

bool restore(uint16_t id) {
  Memo* m = findMutable(id);
  if (!m) return false;
  m->state = ACTIVE;
  save();
  publishState();
  return true;
}

uint16_t activeCount() {
  uint16_t n = 0;
  for (uint16_t i = 0; i < s_count; ++i)
    if (s_memos[i].state == ACTIVE) ++n;
  return n;
}

uint16_t dueCount() {
  const uint32_t now = timeService::epoch();
  if (!now) return 0;
  uint16_t n = 0;
  for (uint16_t i = 0; i < s_count; ++i)
    if (s_memos[i].state == ACTIVE && s_memos[i].dueEpoch &&
        s_memos[i].dueEpoch <= now) ++n;
  return n;
}

uint16_t size() { return s_count; }
const Memo* get(uint16_t index) {
  return index < s_count ? &s_memos[index] : nullptr;
}
const Memo* find(uint16_t id) { return findMutable(id); }

void tick() {
  // Auto-dismiss an alert the user never acknowledged.
  if (s_alertId && millis() - s_alertSinceMs >= ALERT_HOLD_MS) {
    SUN_LOG("memo", "alert id=%u auto-dismissed", static_cast<unsigned>(s_alertId));
    dismissAlert();
    return;
  }
  if (s_alertId) return;      // one alert at a time

  const uint32_t now = timeService::epoch();
  if (!now) return;           // UNSYNCED clock: never fire a false reminder
  for (uint16_t i = 0; i < s_count; ++i) {
    Memo& m = s_memos[i];
    if (m.state != ACTIVE || !m.dueEpoch || m.dueEpoch > now) continue;
    if (s_lastFired[i] && now - s_lastFired[i] < 60) continue;
    s_lastFired[i] = now;
    s_alertId = m.id;
    s_alertSinceMs = millis();
    SUN_LOG("memo", "ALERT id=%u text=%s", static_cast<unsigned>(m.id), m.text);
    publishState();
    return;
  }
}

void publishState() {
  Snapshot s = snapshot();
  s.memoCount = activeCount();
  s.dueCount = dueCount();
  if (s_alertId) {
    const Memo* m = find(s_alertId);
    snprintf(s.activeMemo, sizeof(s.activeMemo), "%s",
             m ? m->text : "");
  } else {
    s.activeMemo[0] = 0;
  }
  snprintf(s.statusLine, sizeof(s.statusLine), "备忘 %u 条 / 到期 %u 条",
           static_cast<unsigned>(s.memoCount),
           static_cast<unsigned>(s.dueCount));
  publish(s);
}

void formatList(char* out, size_t n) {
  out[0] = 0;
  if (!s_count) {
    snprintf(out, n, "暂无备忘录\n等待 WorkBuddy 下发指令");
    return;
  }
  size_t used = 0;
  uint8_t shown = 0;
  for (uint16_t i = 0; i < s_count && shown < 5; ++i) {
    const Memo& m = s_memos[i];
    if (m.state != ACTIVE) continue;
    char t[8];
    hhmm(m.dueEpoch, t, sizeof(t));
    int w = snprintf(out + used, n - used, "%s%s %s\n",
                     shown ? "" : "", t, m.text);
    if (w < 0 || (size_t)w >= n - used) break;
    used += (size_t)w;
    ++shown;
  }
  if (!shown) snprintf(out, n, "暂无待办\n全部已完成");
  // trim trailing newline
  if (used && out[used - 1] == '\n') out[used - 1] = 0;
}

bool alertActive() { return s_alertId != 0; }
uint16_t alertId() { return s_alertId; }
const char* alertText() {
  const Memo* m = find(s_alertId);
  return m ? m->text : "";
}

void dismissAlert() {
  s_alertId = 0;
  s_alertSinceMs = 0;
  publishState();
}

void dump() {
  SUN_LOG("memo", "--- %u row(s), %u active, %u due ---",
          static_cast<unsigned>(s_count),
          static_cast<unsigned>(activeCount()),
          static_cast<unsigned>(dueCount()));
  for (uint16_t i = 0; i < s_count; ++i) {
    const Memo& m = s_memos[i];
    char t[8];
    hhmm(m.dueEpoch, t, sizeof(t));
    SUN_LOG("memo", "  #%-3u %-7s rep=%u at=%s  %s",
            static_cast<unsigned>(m.id),
            m.state == ACTIVE ? "ACTIVE" :
            (m.state == DONE ? "DONE" : "DELETED"),
            static_cast<unsigned>(m.repeat), t, m.text);
  }
}

}  // namespace memoService
}  // namespace sunisle
