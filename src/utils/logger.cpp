#include "utils/logger.h"

#include "sunisle/config.h"
#include "hardware/storage_manager.h"

#include <SD.h>

namespace sunisle {

void logToFile(const char* tag, const char* message) {
  if (!storage::isReady()) return;
  File f = SD.open(FILE_SYNC_LOG, FILE_APPEND);
  if (!f) return;
  f.printf("[%lu] [%s] %s\n", static_cast<unsigned long>(millis()),
           tag ? tag : "log", message ? message : "");
  f.close();
}

}  // namespace sunisle
