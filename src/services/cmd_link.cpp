// Plain-HTTP command poll. Compiled to (almost) nothing unless the user puts
// SUNISLE_CMD_HOST in include/secrets.h (gitignored).
#include "services/cmd_link.h"

#include "services/command_service.h"
#include "utils/logger.h"

#include <WiFi.h>
#include <string.h>

#include "secrets.h"   // must come first: SUNISLE_CMD_HOST is defined here

#ifdef SUNISLE_CMD_HOST
#ifndef SUNISLE_CMD_PORT
#define SUNISLE_CMD_PORT "80"
#endif
#ifndef SUNISLE_CMD_PATH
#define SUNISLE_CMD_PATH "/api/commands"
#endif
#ifndef SUNISLE_CMD_TOKEN
#define SUNISLE_CMD_TOKEN ""
#endif
#endif

namespace sunisle {
namespace cmdLink {

bool enabled() {
#ifdef SUNISLE_CMD_HOST
  return SUNISLE_CMD_HOST[0] != 0;
#else
  return false;
#endif
}

void begin() {
  SUN_LOG("cmdlink", "network poll %s",
          enabled() ? "ENABLED (reads during Wi-Fi window)" :
                      "disabled (set SUNISLE_CMD_HOST in secrets.h)");
}

void onNetworkWindow() {
#ifdef SUNISLE_CMD_HOST
  if (!enabled()) return;

  WiFiClient c;
  c.setTimeout(6);
  const uint16_t port = static_cast<uint16_t>(atoi(SUNISLE_CMD_PORT));
  if (!c.connect(SUNISLE_CMD_HOST, port)) {
    SUN_LOG("cmdlink", "connect %s:%u FAILED (directives stay queued server-side)",
            SUNISLE_CMD_HOST, static_cast<unsigned>(port));
    return;
  }

  char req[320];
  snprintf(req, sizeof(req),
           "GET %s?device=sunisle-b&proto=1 HTTP/1.0\r\n"
           "Host: %s\r\n"
           "X-Device-Token: %s\r\n"
           "Connection: close\r\n\r\n",
           SUNISLE_CMD_PATH, SUNISLE_CMD_HOST, SUNISLE_CMD_TOKEN);
  c.print(req);

  static char buf[2048];
  size_t n = 0;
  uint32_t t0 = millis();
  while (n < sizeof(buf) - 1 && millis() - t0 < 8000) {
    if (c.available()) {
      buf[n++] = static_cast<char>(c.read());
      t0 = millis();
    } else if (!c.connected()) {
      break;
    } else {
      delay(10);
    }
  }
  buf[n] = 0;
  c.stop();

  const char* body = strstr(buf, "\r\n\r\n");
  if (!body || !strstr(buf, "200 OK")) {
    SUN_LOG("cmdlink", "no usable 200 response (%u B)", static_cast<unsigned>(n));
    return;
  }
  body += 4;

  // One directive per line. Replies go to serial now; a future POST-back
  // channel can carry them to the server without touching the parser.
  uint16_t ran = 0;
  const char* p = body;
  while (*p) {
    const char* e = strchr(p, '\n');
    if (!e) e = p + strlen(p);
    String line(p, (size_t)(e - p));
    line.trim();
    if (line.length() && line[0] != '#' && line[0] != '{') {
      String reply;
      commandService::execute(line, reply);
      Serial.println(reply);
      ++ran;
    }
    if (!*e) break;
    p = e + 1;
  }
  SUN_LOG("cmdlink", "executed %u directive(s) from network",
          static_cast<unsigned>(ran));
#endif
}

}  // namespace cmdLink
}  // namespace sunisle
