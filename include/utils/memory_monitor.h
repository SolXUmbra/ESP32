// Runtime heap high-water tracker for MEMORY_BUDGET.md.
#pragma once

#include <Arduino.h>

namespace sunisle {
namespace memory {

void sample();                 // call once per LVGL loop / before heavy ops
uint32_t minHeap();            // lowest free heap ever observed
uint32_t currentHeap();
void     resetMin();
void     dump();

}  // namespace memory
}  // namespace sunisle
