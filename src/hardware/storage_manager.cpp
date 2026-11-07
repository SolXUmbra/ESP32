#include "hardware/storage_manager.h"

#include "sunisle/config.h"
#include "utils/logger.h"

#include <SD.h>
#include <SPI.h>
#include <string.h>
#include <stdlib.h>

namespace sunisle {
namespace storage {
namespace {

bool     s_ready = false;
uint64_t s_total = 0;

// B version: no photo index. Memos/tasks/skills are plain TSV files owned
// by us under /SUNISLE/ (soft delete + .bak on every write).
const char* kDirs[] = {SD_ROOT, DIR_CONFIG, DIR_MEMOS,
                       DIR_WEATHER, DIR_SYSTEM};

// ---------------------------------------------------------------------------
// WHY THIS FILE HAS A MOUNT LADDER
//
// The audited project claimed "SD works" from SD.begin(5) -> cardSize().
// That is NOT a real I/O test: SDFS::cardSize() returns the CACHED CSD value
// (sectors * sectorSize), see SD.cpp:68-76. The first real FatFs call after
// mount (SD.exists / mkdir / open) failed on this board with
//   [E][sd_diskio.cpp:126] sdSelectCard(): Select Failed
// i.e. sdWait() saw MISO stuck at 0x00 for 500 ms.
//
// So mount success proves only that the low-speed (400 kHz) init sequence
// worked. We now require a real read-only FAT probe before declaring ready,
// and try a small ladder of host/pin/clock combinations. The first option is
// the audited default; no hardware parameter is silently changed unless a
// probe proves the default cannot sustain I/O.
// ---------------------------------------------------------------------------

struct MountOpt {
  const char* label;
  uint8_t  host;              // VSPI or HSPI (Arduino constants)
  int8_t   sck, miso, mosi, ss;
  uint32_t hz;
};

// The framework's global SPI object is VSPI (SPI.cpp:348) with the esp32
// variant defaults SS=5, MOSI=23, MISO=19, SCK=18. LovyanGFX uses HSPI with
// 14/13/12 - a DIFFERENT peripheral - so there is no host collision.
// Boot ladder: ALL options stay on the audited VSPI pins 18/19/23/5.
// A failed probe must NEVER re-route VSPI onto 14/12/13: those are the LCD's
// HSPI pins, and two SPI peripherals driving the same GPIOs would corrupt the
// panel. Alternate wiring is only tested by the explicit SDPROBE command.
static const MountOpt kMountOpts[] = {
  {"VSPI/4M",   VSPI, 18, 19, 23, SD_CS, 4000000},   // audited default
  {"VSPI/1M",   VSPI, 18, 19, 23, SD_CS, 1000000},
  {"VSPI/400K", VSPI, 18, 19, 23, SD_CS,  400000},
  {"VSPI/8M",   VSPI, 18, 19, 23, SD_CS, 8000000},
};
static constexpr size_t kMountOptCount =
    sizeof(kMountOpts) / sizeof(kMountOpts[0]);

// Only allocated if an HSPI-host variant is explicitly probed.
SPIClass* s_altSpi = nullptr;

SPIClass& busFor(uint8_t host) {
  if (host == VSPI) return SPI;
  if (!s_altSpi) s_altSpi = new SPIClass(HSPI);
  return *s_altSpi;
}

bool isManaged(const char* path) {
  return path && strncmp(path, "/SUNISLE/", 9) == 0;
}

// Tiny JSON readers: settings.json is a flat object we own. No JSON library
// is linked (saves flash). Missing keys fall back to defaults.
long jsonNumber(const String& s, const char* key, long dflt) {
  String pat = String("\"") + key + "\"";
  int i = s.indexOf(pat);
  if (i < 0) return dflt;
  i = s.indexOf(':', i);
  if (i < 0) return dflt;
  ++i;
  while (i < (int)s.length() && (s[i] == ' ' || s[i] == '\t')) ++i;
  return strtol(s.c_str() + i, nullptr, 10);
}

String jsonString(const String& s, const char* key, const char* dflt) {
  String pat = String("\"") + key + "\"";
  int i = s.indexOf(pat);
  if (i < 0) return String(dflt);
  i = s.indexOf(':', i);
  if (i < 0) return String(dflt);
  i = s.indexOf('"', i);
  if (i < 0) return String(dflt);
  int e = s.indexOf('"', i + 1);
  if (e < 0) return String(dflt);
  return s.substring(i + 1, e);
}

// READ-ONLY probe. Opening "/" forces FatFs find_volume -> disk_status ->
// ff_sd_status -> sdTransaction(SEND_STATUS) -> sdSelectCard(), which is the
// exact call that failed. No writes, no directory creation, no deletions.
// A successful cardSize() alone is NOT sufficient.
bool ioProbe(uint32_t& ms) {
  const uint32_t t0 = millis();
  File root = SD.open("/");
  if (!root) {
    ms = millis() - t0;
    return false;
  }
  const bool isDir = root.isDirectory();
  root.close();
  ms = millis() - t0;
  return isDir;
}

// Returns true ONLY when both the mount AND a real FAT read succeed.
bool tryMount(const MountOpt& o, uint32_t& mountMs, uint32_t& probeMs,
              bool& io) {
  SD.end();
  delay(30);
  SPIClass& bus = busFor(o.host);
  bus.end();                       // allows re-routing the GPIO matrix
  delay(30);
  bus.begin(o.sck, o.miso, o.mosi, o.ss);

  const uint32_t t0 = millis();
  const bool mounted = SD.begin(o.ss, bus, o.hz);
  mountMs = millis() - t0;
  io = false;
  probeMs = 0;
  if (mounted) io = ioProbe(probeMs);

  SUN_LOG("sd", "probe %-11s host=%u pins=%d/%d/%d/%d hz=%7lu "
                "mount=%s(%4lums) IO=%s(%4lums)",
          o.label, static_cast<unsigned>(o.host),
          o.sck, o.miso, o.mosi, o.ss,
          static_cast<unsigned long>(o.hz),
          mounted ? "OK  " : "FAIL", static_cast<unsigned long>(mountMs),
          io ? "PASS" : "FAIL", static_cast<unsigned long>(probeMs));
  return mounted && io;
}

bool mountBest() {
  for (size_t i = 0; i < kMountOptCount; ++i) {
    uint32_t mm = 0, pm = 0;
    bool io = false;
    if (tryMount(kMountOpts[i], mm, pm, io)) {
      s_total = SD.cardSize();
      s_ready = true;
      SUN_LOG("sd", "MOUNT OK %s cs=%d card=%lu MB (real FAT read passed)",
              kMountOpts[i].label, SD_CS,
              static_cast<unsigned long>(s_total / (1024ULL * 1024ULL)));
      return true;
    }
  }
  SD.end();
  s_ready = false;
  // Detach VSPI from the GPIO matrix. Leaving it attached would be harmless
  // on its own default pins, but after any re-routing attempt it could fight
  // the LCD HSPI bus. A failed mount must leave the display untouched.
  SPI.end();
  return false;
}

// Aborts on the FIRST failure: a broken card otherwise burns ~2.5 s per
// operation (5 x 500 ms sdWait retries) and would stall boot for 20+ seconds.
bool ensureDirectories() {
  for (const char* d : kDirs) {
    if (SD.exists(d)) continue;
    if (!SD.mkdir(d)) {
      SUN_LOG("sd", "IO probe failed at %s", d);
      return false;
    }
    SUN_LOG("sd", "created dir %s", d);
  }
  return true;
}

String defaultSettings() {
  char buf[256];
  snprintf(buf, sizeof(buf),
           "{\"brightness\":%u,\"city\":\"\","
           "\"lat\":\"\",\"lon\":\"\",\"timezone\":\"CST-8\","
           "\"sync_daily\":1}\n",
           BL_DEFAULT);
  return String(buf);
}

String defaultDevice() {
  char buf[192];
  snprintf(buf, sizeof(buf),
           "{\"product\":\"SUNISLE\",\"firmware\":\"%s\",\"schema\":1}\n",
           FW_VERSION);
  return String(buf);
}

bool readAll(const char* path, String& out) {
  File f = SD.open(path, FILE_READ);
  if (!f) return false;
  out = "";
  while (f.available()) out += static_cast<char>(f.read());
  f.close();
  return out.length() > 0;
}

void publishOffline() {
  s_ready = false;
  Snapshot s = snapshot();
  s.sdReady = false;
  snprintf(s.statusLine, sizeof(s.statusLine), "存储卡不可用 / 备忘仅存内存");
  publish(s);
}

}  // namespace

bool isReady() { return s_ready; }
uint64_t totalBytes() { return s_total; }
uint64_t usedBytes() { return s_ready ? SD.usedBytes() : 0; }
uint64_t freeBytes() {
  if (!s_ready || SD.usedBytes() >= s_total) return 0;
  return s_total - SD.usedBytes();
}
bool writeIfAbsent(const char* path, const char* content) {
  if (!s_ready || !isManaged(path)) return false;
  if (SD.exists(path)) return false;   // never overwrite user data
  File f = SD.open(path, FILE_WRITE);
  if (!f) return false;
  size_t n = strlen(content);
  size_t written = f.write(reinterpret_cast<const uint8_t*>(content), n);
  f.close();
  if (written != n || !SD.exists(path)) return false;
  SUN_LOG("sd", "created %s (%u B)", path, static_cast<unsigned>(n));
  return true;
}

bool atomicReplace(const char* path, const char* content) {
  if (!s_ready || !isManaged(path)) return false;
  String tmp = String(path) + ".tmp";
  File f = SD.open(tmp.c_str(), FILE_WRITE);
  if (!f) return false;
  size_t n = strlen(content);
  size_t w = f.write(reinterpret_cast<const uint8_t*>(content), n);
  f.close();
  File chk = SD.open(tmp.c_str(), FILE_READ);
  if (!chk || chk.size() != n) {
    if (chk) chk.close();
    SUN_LOG("sd", "atomic write size mismatch, kept original %s", path);
    return false;
  }
  chk.close();
  // Only our own managed config/memo files may be replaced. The caller
  // must keep a .bak copy before invoking this on user data.
  if (SD.exists(path)) SD.remove(path);
  if (!SD.rename(tmp.c_str(), path)) {
    SUN_LOG("sd", "rename failed for %s", path);
    return false;
  }
  return true;
}

bool appendLine(const char* path, const char* line) {
  if (!s_ready || !isManaged(path)) return false;
  File f = SD.open(path, FILE_APPEND);
  if (!f) return false;
  f.println(line);
  f.close();
  return true;
}

bool readText(const char* path, String& out) {
  out = "";
  if (!s_ready || !isManaged(path)) return false;
  if (!SD.exists(path)) return false;    // quiet: VFS logs an error otherwise
  File f = SD.open(path, FILE_READ);
  if (!f) return false;
  while (f.available()) out += static_cast<char>(f.read());
  f.close();
  return out.length() > 0;
}

bool fileExists(const char* path) {
  return s_ready && isManaged(path) && SD.exists(path);
}

// B version: the serial photo-upload/delete path is gone. Memo writes use
// atomicReplace()/readText() below, which are restricted to /SUNISLE/ files.

bool loadSettings(Snapshot& s) {
  String txt;
  if (!readAll(FILE_SETTINGS, txt)) return false;
  long b = jsonNumber(txt, "brightness", BL_DEFAULT);
  s.brightness = static_cast<uint8_t>(constrain(b, 0, 255));
  String city = jsonString(txt, "city", "");
  if (city.length() > 0) city.toCharArray(s.city, sizeof(s.city));
  String lat = jsonString(txt, "lat", "");
  s.weatherConfigured = (lat.length() > 0 && city.length() > 0);
  return true;
}

bool saveSettings(const Snapshot& s) {
  // Preserve fields this stage does not own (user-entered lat/lon/timezone).
  // Overwriting them with empty strings would silently destroy user data.
  String old;
  readAll(FILE_SETTINGS, old);
  String lat = jsonString(old, "lat", "");
  String lon = jsonString(old, "lon", "");
  String tz  = jsonString(old, "timezone", "CST-8");
  char buf[320];
  snprintf(buf, sizeof(buf),
           "{\"brightness\":%u,\"city\":\"%s\","
           "\"lat\":\"%s\",\"lon\":\"%s\",\"timezone\":\"%s\","
           "\"sync_daily\":1}\n",
           s.brightness, s.city, lat.c_str(), lon.c_str(), tz.c_str());
  bool ok = atomicReplace(FILE_SETTINGS, buf);
  SUN_LOG("sd", "settings %s (brightness=%u)",
          ok ? "saved" : "save FAILED", s.brightness);
  return ok;
}

bool remount() {
  if (!mountBest()) {
    SUN_LOG("sd", "remount FAILED - no option sustained a real FAT read");
    publishOffline();
    return false;
  }
  if (!ensureDirectories()) {
    SUN_LOG("sd", "remount degraded: directory IO failed");
    SD.end();
    publishOffline();
    return false;
  }
  Snapshot s = snapshot();
  s.sdReady = true;
  publish(s);
  return true;
}

bool begin() {
  s_ready = false;

  if (!mountBest()) {
    SUN_LOG("sd", "UNAVAILABLE - no mount option sustained a real FAT read");
    SUN_LOG("sd", "degraded mode: UI / clock / backlight still run");
    publishOffline();
    return false;
  }

  if (!ensureDirectories()) {
    // Honest state: s_ready stays true ONLY when IO is proven working.
    SUN_LOG("sd", "IO failed AFTER mount - marking card OFFLINE");
    SD.end();
    publishOffline();
    return false;
  }

  writeIfAbsent(FILE_DEVICE, defaultDevice().c_str());
  // Deliberately DOES NOT overwrite an existing settings.json.
  writeIfAbsent(FILE_SETTINGS, defaultSettings().c_str());
  writeIfAbsent(FILE_SYNC, "{\"last_sync\":0,\"result\":\"NEVER\"}\n");

  Snapshot s = snapshot();
  s.sdReady = true;
  loadSettings(s);
  publish(s);
  writeIfAbsent(FILE_MEMOS, "# id\tstate\tat\trepeat\ttext\n");
  writeIfAbsent(FILE_TASKS, "# id\tstate\tname\tstep\ttotal\n");
  writeIfAbsent(FILE_SKILLS, "# skill\tkey\tvalue\n");
  return true;
}

bool preDisplayProbe() {
  uint32_t mm = 0, pm = 0;
  bool io = false;
  const bool ok = tryMount(kMountOpts[0], mm, pm, io);
  SUN_LOG("sd", "PRE-DISPLAY SD I/O (LCD HSPI not yet up): %s",
          ok ? "PASS" : "FAIL");
  SD.end();
  delay(30);
  return ok;
}

void runProbe(bool includeSharedBus) {
  SUN_LOG("sd", "--- SD PROBE MATRIX (READ-ONLY, no writes) ---");
  SUN_LOG("sd", "ALT variants briefly re-route VSPI onto LCD pins; "
                "bus is restored at the end of this command.");
  static const MountOpt kMatrix[] = {
    {"VSPI/4M",    VSPI, 18, 19, 23, SD_CS, 4000000},
    {"VSPI/1M",    VSPI, 18, 19, 23, SD_CS, 1000000},
    {"VSPI/400K",  VSPI, 18, 19, 23, SD_CS,  400000},
    {"VSPI/2M",    VSPI, 18, 19, 23, SD_CS, 2000000},
    {"VSPI/8M",    VSPI, 18, 19, 23, SD_CS, 8000000},
    {"VSPI/16M",   VSPI, 18, 19, 23, SD_CS, 16000000},
    {"ALT/1M",     VSPI, LCD_SCLK, LCD_MISO, LCD_MOSI, SD_CS, 1000000},
    {"ALT/4M",     VSPI, LCD_SCLK, LCD_MISO, LCD_MOSI, SD_CS, 4000000},
  };
  for (const MountOpt& o : kMatrix) {
    uint32_t mm = 0, pm = 0;
    bool io = false;
    tryMount(o, mm, pm, io);
  }
  if (includeSharedBus) {
    SUN_LOG("sd", "WARNING: HSPI-host variants share the LCD peripheral; "
                  "screen may glitch. Send UI to redraw.");
    static const MountOpt kShared[] = {
      {"HSPI/1M", HSPI, LCD_SCLK, LCD_MISO, LCD_MOSI, SD_CS, 1000000},
      {"HSPI/4M", HSPI, LCD_SCLK, LCD_MISO, LCD_MOSI, SD_CS, 4000000},
    };
    for (const MountOpt& o : kShared) {
      uint32_t mm = 0, pm = 0;
      bool io = false;
      tryMount(o, mm, pm, io);
    }
  }
  SD.end();
  // Restore a clean GPIO matrix: neither VSPI nor the temporary HSPI object
  // may keep driving the LCD's 14/13/12 pins after the diagnostic.
  SPI.end();
  if (s_altSpi) s_altSpi->end();
  SPI.begin();                 // back to variant defaults 18/19/23/5, idle
  publishOffline();
  SUN_LOG("sd", "--- END MATRIX. Bus restored. Send REBOOT for a clean boot. ---");
}

void dump() {
  SUN_LOG("sd", "ready=%s total=%lu MB used=%lu MB free=%lu MB",
          s_ready ? "YES" : "NO",
          static_cast<unsigned long>(s_total / (1024ULL * 1024ULL)),
          static_cast<unsigned long>(usedBytes() / (1024ULL * 1024ULL)),
          static_cast<unsigned long>(freeBytes() / (1024ULL * 1024ULL)));
}

}  // namespace storage
}  // namespace sunisle
