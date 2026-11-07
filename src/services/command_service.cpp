// WorkBuddy directive dispatcher (B version) - protocol v1.
//
// Three message families share ONE parser and ONE reply format:
//   CMD   one-shot directive        (memo add/done/snooze/del, status, sync...)
//   SKILL device capability config  (memo.lead_min, display.brightness, ...)
//   TASK  composite multi-step task (each step raises a memo reminder)
//
// Serial and the optional network poll both call execute(), so a directive
// behaves identically regardless of transport. Persistence is TSV on SD via
// storage::atomicReplace(); the RAM copy stays authoritative when SD is away.
#include "services/command_service.h"

#include "sunisle/config.h"
#include "sunisle/protocol.h"
#include "sunisle/state.h"
#include "services/memo_service.h"
#include "services/time_service.h"
#include "hardware/backlight_manager.h"
#include "hardware/storage_manager.h"
#include "utils/logger.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

namespace sunisle {
namespace commandService {
namespace {

// ------------------------------ line helpers ------------------------------
uint8_t splitFields(const String& line, String* out, uint8_t maxFields) {
  uint8_t n = 0;
  int from = 0;
  const int len = (int)line.length();
  while (n < maxFields && from <= len) {
    int to = line.indexOf('|', from);
    if (to < 0) to = len;
    out[n] = line.substring(from, to);
    out[n].trim();
    ++n;
    if (to == len) break;
    from = to + 1;
  }
  return n;
}

bool kv(const String* f, uint8_t n, const char* key, String& out) {
  String pat = String(key) + "=";
  for (uint8_t i = 1; i < n; ++i) {
    if (f[i].startsWith(pat)) {
      out = f[i].substring(pat.length());
      out.trim();
      return out.length() > 0;
    }
  }
  return false;
}

uint16_t kvUint(const String* f, uint8_t n, const char* key, uint16_t dflt) {
  String v;
  if (!kv(f, n, key, v)) return dflt;
  return static_cast<uint16_t>(strtoul(v.c_str(), nullptr, 10));
}

void ack(String& reply, const char* ref, const char* msg) {
  reply = String("ACK|") + ref + "|" + msg;
}
void err(String& reply, const char* ref, CmdError code, const char* msg) {
  reply = String("ERR|") + ref + "|code=" + String((int)code) + "|" + msg;
}

bool digits2(const char* s, int& v) {
  if (!isdigit((unsigned char)s[0]) || !isdigit((unsigned char)s[1])) return false;
  v = (s[0] - '0') * 10 + (s[1] - '0');
  return true;
}

// at=YYYY-MM-DDTHH:MM | at=HH:MM (today, or tomorrow if already past)
// Uses mktime() with the device's configured TZ (CST-8). No sscanf: that
// would pull the whole scanf family into the 1.25 MB app partition.
bool parseAt(const String& v, uint32_t& out) {
  const char* s = v.c_str();
  struct tm t;
  memset(&t, 0, sizeof(t));
  t.tm_isdst = -1;
  const uint32_t now = timeService::epoch();

  if (v.length() == 16 && s[4] == '-' && s[7] == '-' &&
      (s[10] == 'T' || s[10] == ' ') && s[13] == ':') {
    int y = atoi(v.substring(0, 4).c_str());
    int mo, d, h, mi;
    if (!digits2(s + 5, mo) || !digits2(s + 8, d) ||
        !digits2(s + 11, h) || !digits2(s + 14, mi)) return false;
    t.tm_year = y - 1900; t.tm_mon = mo - 1; t.tm_mday = d;
    t.tm_hour = h; t.tm_min = mi;
  } else if (v.length() == 5 && s[2] == ':') {
    if (!now) return false;
    int h, mi;
    if (!digits2(s, h) || !digits2(s + 3, mi)) return false;
    time_t nt = (time_t)now;
    struct tm lt;
    if (!localtime_r(&nt, &lt)) return false;
    t = lt;
    t.tm_hour = h; t.tm_min = mi; t.tm_sec = 0; t.tm_isdst = -1;
    time_t candidate = mktime(&t);
    if (candidate <= nt) candidate += 86400;
    out = (uint32_t)candidate;
    return true;
  } else {
    return false;
  }
  time_t e = mktime(&t);
  if (e <= 0) return false;
  out = (uint32_t)e;
  return true;
}

// ------------------------------ SKILL config ------------------------------
struct SkillRow { char key[24]; char value[24]; };
SkillRow s_skills[SKILL_MAX_ROWS];
uint8_t  s_skillCount = 0;

const char* skillGet(const char* key, const char* dflt) {
  for (uint8_t i = 0; i < s_skillCount; ++i)
    if (strcmp(s_skills[i].key, key) == 0) return s_skills[i].value;
  return dflt;
}
uint16_t skillGetU(const char* key, uint16_t dflt) {
  return static_cast<uint16_t>(strtoul(skillGet(key, ""), nullptr, 10)) ?
         static_cast<uint16_t>(strtoul(skillGet(key, ""), nullptr, 10)) : dflt;
}

bool skillSet(const char* key, const char* value) {
  for (uint8_t i = 0; i < s_skillCount; ++i) {
    if (strcmp(s_skills[i].key, key) == 0) {
      strncpy(s_skills[i].value, value, sizeof(s_skills[i].value) - 1);
      s_skills[i].value[sizeof(s_skills[i].value) - 1] = 0;
      return true;
    }
  }
  if (s_skillCount >= SKILL_MAX_ROWS) return false;
  SkillRow& r = s_skills[s_skillCount++];
  strncpy(r.key, key, sizeof(r.key) - 1);   r.key[sizeof(r.key) - 1] = 0;
  strncpy(r.value, value, sizeof(r.value) - 1);
  r.value[sizeof(r.value) - 1] = 0;
  return true;
}

void loadSkills() {
  s_skillCount = 0;
  String txt;
  if (!storage::readText(FILE_SKILLS, txt)) return;
  int from = 0;
  while (from < (int)txt.length()) {
    int to = txt.indexOf('\n', from);
    if (to < 0) to = txt.length();
    String line = txt.substring(from, to); line.trim();
    from = to + 1;
    if (!line.length() || line[0] == '#') continue;
    int tab = line.indexOf('\t');
    if (tab < 0) continue;
    skillSet(line.substring(0, tab).c_str(),
             line.substring(tab + 1).c_str());
  }
}

bool saveSkills() {
  if (!storage::isReady()) return false;
  String out = "# skill.key\tvalue\n";
  for (uint8_t i = 0; i < s_skillCount; ++i)
    out += String(s_skills[i].key) + "\t" + String(s_skills[i].value) + "\n";
  return storage::atomicReplace(FILE_SKILLS, out.c_str());
}

// Some skills take effect immediately; the rest are read at use sites.
void applySkill(const char* key, const char* value) {
  if (strcmp(key, "display.brightness") == 0) {
    uint8_t v = static_cast<uint8_t>(constrain(atoi(value), 0, 255));
    backlight::set(v);
    Snapshot s = snapshot(); s.brightness = v; publish(s);
  }
}

// ------------------------------ composite TASK ---------------------------
struct Task {
  uint16_t id;
  uint8_t  state;          // 0 active, 1 done, 2 deleted
  uint8_t  step;           // 0-based index of the next step
  uint8_t  total;
  char     name[TASK_NAME_MAX + 1];
  char     steps[TASK_MAX_STEPS][TASK_STEP_MAX + 1];
};
Task     s_tasks[TASK_MAX];
uint8_t  s_taskCount = 0;
uint16_t s_nextTaskId = 1;
bool     s_rebootPending = false;

Task* findTask(uint16_t id) {
  for (uint8_t i = 0; i < s_taskCount; ++i)
    if (s_tasks[i].id == id) return &s_tasks[i];
  return nullptr;
}

uint8_t splitSteps(const String& src, char dest[][TASK_STEP_MAX + 1],
                   uint8_t maxSteps) {
  uint8_t n = 0;
  int from = 0;
  const int len = (int)src.length();
  while (n < maxSteps && from <= len) {
    int to = src.indexOf(';', from);
    if (to < 0) to = len;
    String one = src.substring(from, to); one.trim();
    if (one.length()) {
      strncpy(dest[n], one.c_str(), TASK_STEP_MAX);
      dest[n][TASK_STEP_MAX] = 0;
      ++n;
    }
    if (to == len) break;
    from = to + 1;
  }
  return n;
}

void loadTasks() {
  s_taskCount = 0;
  s_nextTaskId = 1;
  String txt;
  if (!storage::readText(FILE_TASKS, txt)) return;
  int from = 0;
  while (from < (int)txt.length() && s_taskCount < TASK_MAX) {
    int to = txt.indexOf('\n', from);
    if (to < 0) to = txt.length();
    String line = txt.substring(from, to); line.trim();
    from = to + 1;
    if (!line.length() || line[0] == '#') continue;
    // id \t state \t step \t total \t name \t steps(semicolon joined)
    int p[6]; p[0] = 0;
    int idx = 1;
    for (int i = 0; i < (int)line.length() && idx < 6; ++i)
      if (line[i] == '\t') p[idx++] = i + 1;
    if (idx < 6) continue;
    Task& t = s_tasks[s_taskCount];
    memset(&t, 0, sizeof(t));
    t.id = (uint16_t)line.substring(p[0], p[1] - 1).toInt();
    t.state = (uint8_t)line.substring(p[1], p[2] - 1).toInt();
    t.step = (uint8_t)line.substring(p[2], p[3] - 1).toInt();
    t.total = (uint8_t)line.substring(p[3], p[4] - 1).toInt();
    strncpy(t.name, line.substring(p[4], p[5] - 1).c_str(), TASK_NAME_MAX);
    t.name[TASK_NAME_MAX] = 0;
    char tmp[TASK_MAX_STEPS][TASK_STEP_MAX + 1];
    uint8_t n = splitSteps(line.substring(p[5]), tmp, TASK_MAX_STEPS);
    for (uint8_t k = 0; k < n; ++k) strcpy(t.steps[k], tmp[k]);
    t.total = n ? n : t.total;
    ++s_taskCount;
    if (t.id >= s_nextTaskId) s_nextTaskId = t.id + 1;
  }
}

bool saveTasks() {
  if (!storage::isReady()) return false;
  String out = "# id\tstate\tstep\ttotal\tname\tsteps\n";
  for (uint8_t i = 0; i < s_taskCount; ++i) {
    const Task& t = s_tasks[i];
    out += String(t.id) + "\t" + String(t.state) + "\t" +
           String(t.step) + "\t" + String(t.total) + "\t" +
           String(t.name) + "\t";
    for (uint8_t k = 0; k < t.total; ++k) {
      if (k) out += ";";
      out += String(t.steps[k]);
    }
    out += "\n";
  }
  return storage::atomicReplace(FILE_TASKS, out.c_str());
}

// Raising the current step as a memo is what makes a "composite task"
// observable on a device that has no task-list screen of its own.
void raiseStep(Task& t) {
  if (t.step >= t.total) return;
  char text[MEMO_TEXT_MAX + 1];
  snprintf(text, sizeof(text), "%s (%u/%u): %s", t.name,
           (unsigned)(t.step + 1), (unsigned)t.total, t.steps[t.step]);
  uint16_t dummy = 0;
  memoService::add(0, memoService::ONCE, text, dummy);
}

// ------------------------------ CMD actions ------------------------------
void cmdStatus(String& reply) {
  char buf[160];
  Snapshot s = snapshot();
  snprintf(buf, sizeof(buf),
           "proto=%u state=%s clock=%s memos=%u due=%u tasks=%u skills=%u sd=%s",
           (unsigned)PROTO_VERSION, stateName(s.state),
           timeService::isSynced() ? "SYNCED" : "UNSYNCED",
           (unsigned)s.memoCount, (unsigned)s.dueCount,
           (unsigned)taskCount(), (unsigned)s_skillCount,
           storage::isReady() ? "YES" : "NO");
  ack(reply, "CMD", buf);
}

void cmdAdd(const String* f, uint8_t n, String& reply) {
  String text, at, in, rep;
  if (!kv(f, n, "text", text)) {
    err(reply, "CMD", ERR_BAD_PARAM, "text= is required"); return;
  }
  uint32_t due = 0;
  if (kv(f, n, "at", at)) {
    if (!parseAt(at, due)) {
      err(reply, "CMD", ERR_BAD_PARAM, "at= expects HH:MM or YYYY-MM-DDTHH:MM");
      return;
    }
    // SKILL: memo.lead_min fires the reminder early.
    uint16_t lead = skillGetU("memo.lead_min", 0);
    if (lead && due > lead * 60UL) due -= lead * 60UL;
  } else if (kv(f, n, "in", in)) {
    uint32_t now = timeService::epoch();
    if (!now) { err(reply, "CMD", ERR_NO_CLOCK, "clock unsynced, use at="); return; }
    uint32_t mins = strtoul(in.c_str(), nullptr, 10);
    if (!mins || mins > 10080) {
      err(reply, "CMD", ERR_BAD_PARAM, "in= expects 1..10080 minutes"); return;
    }
    due = now + mins * 60UL;
  }
  uint8_t repeat = memoService::ONCE;
  if (kv(f, n, "repeat", rep)) {
    if (rep == "daily") repeat = memoService::DAILY;
    else if (rep == "weekly") repeat = memoService::WEEKLY;
  }
  uint16_t id = 0;
  if (!memoService::add(due, repeat, text.c_str(), id)) {
    err(reply, "CMD", ERR_FULL, "memo table full"); return;
  }
  char buf[64];
  snprintf(buf, sizeof(buf), "id=%u due=%lu repeat=%u", (unsigned)id,
           (unsigned long)due, (unsigned)repeat);
  ack(reply, "CMD", buf);
}

void cmdById(const String* f, uint8_t n, String& reply, const char* action) {
  uint16_t id = kvUint(f, n, "id", 0);
  if (!id) { err(reply, "CMD", ERR_BAD_PARAM, "id= is required"); return; }
  bool ok = false;
  if (strcmp(action, "done") == 0)         ok = memoService::markDone(id);
  else if (strcmp(action, "del") == 0)     ok = memoService::softDelete(id);
  else if (strcmp(action, "restore") == 0) ok = memoService::restore(id);
  else if (strcmp(action, "snooze") == 0) {
    uint32_t ms = (uint32_t)kvUint(f, n, "min", 10) * 60000UL;
    ok = memoService::snooze(id, ms);
  }
  if (!ok) { err(reply, "CMD", ERR_NOT_FOUND, "no active memo with that id"); return; }
  char buf[48];
  snprintf(buf, sizeof(buf), "%s id=%u OK", action, (unsigned)id);
  ack(reply, "CMD", buf);
}

void handleCmd(const String* f, uint8_t n, String& reply) {
  if (n < 2) { err(reply, "CMD", ERR_BAD_FORMAT, "CMD|<action>|..."); return; }
  const String& a = f[1];
  if (a == "status")       cmdStatus(reply);
  else if (a == "list")    { memoService::dump(); cmdStatus(reply); }
  else if (a == "add")     cmdAdd(f, n, reply);
  else if (a == "done" || a == "del" || a == "restore" || a == "snooze")
                            cmdById(f, n, reply, a.c_str());
  else if (a == "dismiss") { memoService::dismissAlert(); ack(reply, "CMD", "alert dismissed"); }
  else if (a == "sync")    ack(reply, "CMD", "sync=queued (poll link reads same execute())");
  else if (a == "reboot")  { s_rebootPending = true; ack(reply, "CMD", "reboot scheduled"); }
  else if (a == "help")    ack(reply, "CMD",
      "status|list|add text=.. at=HH:MM|in=N repeat=daily|done id=|"
      "snooze id=|del id=|restore id=|dismiss|sync|reboot");
  else err(reply, "CMD", ERR_BAD_ACTION, "unknown action (CMD|help)");
}

// ------------------------------ SKILL handler ----------------------------
void handleSkill(const String* f, uint8_t n, String& reply) {
  if (n < 2) { err(reply, "SKILL", ERR_BAD_FORMAT, "SKILL|SET|k=v... or SKILL|GET"); return; }
  if (f[1] == "GET") {
    if (!s_skillCount) { ack(reply, "SKILL", "(no overrides)"); return; }
    String out;
    for (uint8_t i = 0; i < s_skillCount; ++i) {
      if (i) out += ";";
      out += String(s_skills[i].key) + "=" + String(s_skills[i].value);
    }
    ack(reply, "SKILL", out.c_str());
    return;
  }
  if (f[1] != "SET") { err(reply, "SKILL", ERR_BAD_ACTION, "use SET or GET"); return; }
  uint8_t changed = 0;
  for (uint8_t i = 2; i < n; ++i) {
    int eq = f[i].indexOf('=');
    if (eq <= 0) continue;
    String k = f[i].substring(0, eq);   k.trim();
    String v = f[i].substring(eq + 1); v.trim();
    if (!k.length() || !v.length()) continue;
    if (!skillSet(k.c_str(), v.c_str())) {
      err(reply, "SKILL", ERR_FULL, "skill table full"); return;
    }
    applySkill(k.c_str(), v.c_str());
    ++changed;
  }
  if (!changed) { err(reply, "SKILL", ERR_BAD_PARAM, "no key=value pairs"); return; }
  saveSkills();
  char buf[48];
  snprintf(buf, sizeof(buf), "%u key(s) updated", (unsigned)changed);
  ack(reply, "SKILL", buf);
}

// ------------------------------ TASK handler -----------------------------
void listTasks(String& reply) {
  if (!s_taskCount) { ack(reply, "TASK", "(no tasks)"); return; }
  String out;
  for (uint8_t i = 0; i < s_taskCount; ++i) {
    const Task& t = s_tasks[i];
    if (t.state == 2) continue;
    if (out.length()) out += ";";
    out += String(t.id) + ":" + String(t.name) + ":" +
           String((unsigned)t.step + 1) + "/" + String((unsigned)t.total) +
           ":" + (t.state == 1 ? "DONE" : "ACTIVE");
  }
  ack(reply, "TASK", out.length() ? out.c_str() : "(no tasks)");
}

void handleTask(const String* f, uint8_t n, String& reply) {
  if (n < 2) { err(reply, "TASK", ERR_BAD_FORMAT, "TASK|ADD|LIST|NEXT|DONE|DEL"); return; }
  const String& a = f[1];
  if (a == "LIST") { listTasks(reply); return; }

  if (a == "ADD") {
    String name, stepsSrc, at;
    if (!kv(f, n, "name", name) || !kv(f, n, "steps", stepsSrc)) {
      err(reply, "TASK", ERR_BAD_PARAM, "name= and steps=a;b;c are required"); return;
    }
    if (s_taskCount >= TASK_MAX) { err(reply, "TASK", ERR_FULL, "task table full"); return; }
    Task& t = s_tasks[s_taskCount];
    memset(&t, 0, sizeof(t));
    t.id = s_nextTaskId++;
    strncpy(t.name, name.c_str(), TASK_NAME_MAX); t.name[TASK_NAME_MAX] = 0;
    t.total = splitSteps(stepsSrc, t.steps, TASK_MAX_STEPS);
    if (!t.total) { err(reply, "TASK", ERR_BAD_PARAM, "steps= is empty"); return; }
    t.state = 0; t.step = 0;
    ++s_taskCount;
    if (kv(f, n, "at", at)) {
      uint32_t due = 0;
      if (parseAt(at, due)) {
        uint16_t dummy = 0;
        char first[MEMO_TEXT_MAX + 1];
        snprintf(first, sizeof(first), "%s (1/%u): %s", t.name,
                 (unsigned)t.total, t.steps[0]);
        memoService::add(due, memoService::ONCE, first, dummy);
      }
    } else {
      raiseStep(t);      // untimed first step still shows up in the memo list
    }
    saveTasks();
    char buf[64];
    snprintf(buf, sizeof(buf), "id=%u steps=%u name=%s", (unsigned)t.id,
             (unsigned)t.total, t.name);
    ack(reply, "TASK", buf);
    return;
  }

  uint16_t id = kvUint(f, n, "id", 0);
  if (!id) { err(reply, "TASK", ERR_BAD_PARAM, "id= is required"); return; }
  Task* t = findTask(id);
  if (!t || t->state == 2) { err(reply, "TASK", ERR_NOT_FOUND, "unknown task id"); return; }

  if (a == "NEXT") {
    if (t->state == 1) { err(reply, "TASK", ERR_BAD_ACTION, "task already done"); return; }
    ++t->step;
    if (t->step >= t->total) {
      t->state = 1;
      ack(reply, "TASK", "all steps complete");
    } else {
      raiseStep(*t);
      char buf[48];
      snprintf(buf, sizeof(buf), "step %u/%u raised", (unsigned)(t->step + 1),
               (unsigned)t->total);
      ack(reply, "TASK", buf);
    }
  } else if (a == "DONE") {
    t->state = 1; t->step = t->total;
    ack(reply, "TASK", "done");
  } else if (a == "DEL") {
    t->state = 2;                  // soft delete, row stays on the card
    ack(reply, "TASK", "soft-deleted");
  } else {
    err(reply, "TASK", ERR_BAD_ACTION, "unknown action"); return;
  }
  saveTasks();
}

}  // namespace

void begin() {
  s_rebootPending = false;
  loadSkills();
  loadTasks();
  // Defaults live here so a fresh card is usable without any SKILL|SET.
  if (!skillGet("memo.lead_min", nullptr)) skillSet("memo.lead_min", "0");
  if (!skillGet("memo.quiet_start", nullptr)) skillSet("memo.quiet_start", "22:00");
  if (!skillGet("memo.quiet_end", nullptr)) skillSet("memo.quiet_end", "07:00");
  if (!skillGet("net.poll_sec", nullptr)) skillSet("net.poll_sec", "60");
  SUN_LOG("cmd", "protocol v%u ready: %u skill(s), %u task(s)",
          (unsigned)PROTO_VERSION, (unsigned)s_skillCount, (unsigned)s_taskCount);
}

void execute(const String& line, String& reply) {
  String f[PROTO_MAX_FIELDS];
  String trimmed = line; trimmed.trim();
  if (!trimmed.length()) { err(reply, "-", ERR_BAD_FORMAT, "empty line"); return; }
  uint8_t n = splitFields(trimmed, f, PROTO_MAX_FIELDS);
  f[0].toUpperCase();
  if (f[0] == "CMD")        handleCmd(f, n, reply);
  else if (f[0] == "SKILL") handleSkill(f, n, reply);
  else if (f[0] == "TASK")  handleTask(f, n, reply);
  else err(reply, f[0].c_str(), ERR_BAD_FORMAT, "expected CMD| / SKILL| / TASK|");
}

uint16_t taskCount() {
  uint16_t n = 0;
  for (uint8_t i = 0; i < s_taskCount; ++i)
    if (s_tasks[i].state != 2) ++n;
  return n;
}

bool rebootPending() { return s_rebootPending; }

void dump() {
  SUN_LOG("cmd", "--- protocol v%u, %u task(s), %u skill(s) ---",
          (unsigned)PROTO_VERSION, (unsigned)taskCount(),
          (unsigned)s_skillCount);
  for (uint8_t i = 0; i < s_taskCount; ++i) {
    const Task& t = s_tasks[i];
    SUN_LOG("cmd", "  TASK #%-3u %-7s step=%u/%u %s", (unsigned)t.id,
            t.state == 0 ? "ACTIVE" : (t.state == 1 ? "DONE" : "DELETED"),
            (unsigned)t.step, (unsigned)t.total, t.name);
  }
  for (uint8_t i = 0; i < s_skillCount; ++i)
    SUN_LOG("cmd", "  SKILL %s = %s", s_skills[i].key, s_skills[i].value);
}

}  // namespace commandService
}  // namespace sunisle
