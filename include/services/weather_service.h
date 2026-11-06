// Weather service.
// Stage 1 performs NO network request. The task book says the city/coords
// are not configured yet, so the honest state is "天气未配置".
// Stage 3 will fetch Open-Meteo asynchronously, store the last successful
// REAL response in /SUNISLE/weather/cache.json and label offline data CACHED.
#pragma once

#include <Arduino.h>

namespace sunisle {
namespace weatherService {

void begin();
void tick();
bool isConfigured();
bool hasRealData();
const char* statusLine();
void dump();

}  // namespace weatherService
}  // namespace sunisle
