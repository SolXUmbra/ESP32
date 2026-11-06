#pragma once
#include <Arduino.h>
#include <lvgl.h>

namespace sunisle { namespace ui { namespace boot {
// 1.8s brand splash, opacity ramped by ui_manager::tickFrame().
lv_obj_t* create(lv_obj_t* parent);
}}}
