// SUNISLE B - WorkBuddy device command protocol (version 1).
//
// ONE LINE = ONE MESSAGE. Fields are separated by '|', parameters are
// key=value. UTF-8 is allowed in values (but not '\t', '\n', '\r' or '|').
// Every message returns exactly one ACK/ERR line, so a host can pipeline
// commands over serial OR over the optional HTTP poll link.
//
//   CMD|<action>|key=value|...            one-shot directive
//   SKILL|SET|<skill>.<key>=<value>|...  configure a device skill
//   SKILL|GET
//   TASK|ADD|name=<n>|steps=a;b;c|at=<time>
//   TASK|LIST | TASK|NEXT|id=<n> | TASK|DONE|id=<n> | TASK|DEL|id=<n>
//
// Replies:  ACK|<ref>|<message>
//           ERR|<ref>|code=<n>|<message>
//
// The same line format is used by the serial console (bare lines) and by the
// network link (one command per response line), so there is a single parser.
#pragma once

#include "sunisle/config.h"

namespace sunisle {

constexpr uint8_t  PROTO_VERSION   = 1;
constexpr uint8_t  PROTO_MAX_FIELDS = 10;
constexpr uint8_t  TASK_MAX        = 12;
constexpr uint8_t  TASK_MAX_STEPS  = 6;
constexpr uint8_t  TASK_NAME_MAX   = 32;
constexpr uint8_t  TASK_STEP_MAX   = MEMO_TEXT_MAX;
constexpr uint8_t  SKILL_MAX_ROWS  = 24;

enum CmdError : uint8_t {
  ERR_NONE       = 0,
  ERR_BAD_FORMAT = 1,
  ERR_BAD_ACTION = 2,
  ERR_BAD_PARAM  = 3,
  ERR_NOT_FOUND  = 4,
  ERR_FULL       = 5,
  ERR_NO_CLOCK   = 6,
  ERR_STORAGE    = 7,
  ERR_DISABLED   = 8,
};

}  // namespace sunisle
