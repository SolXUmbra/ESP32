// Daily quote service - see quote_service.h for the rationale.
#include "services/quote_service.h"

#include "sunisle/state.h"
#include "services/time_service.h"
#include "utils/logger.h"

#include <string.h>
#include <time.h>

namespace sunisle {
namespace quoteService {
namespace {

// MUST stay in sync with tools/gen_cn_font.py QUOTES. Each entry's glyphs are
// baked into the sunisle_cn_quote subset font; adding a sentence here without
// regenerating the font would render it as tofu boxes.
static const char* const kQuotes[] = {
    "把喜欢的时光，留在身边。",
    "种一棵树最好的时间是十年前，其次是现在。",
    "愿你遍历山河，仍觉人间值得。",
    "慢一点，也没关系，风景都在路上。",
    "每一次日落，都是为下一次日出作序。",
    "认真生活的人，运气都不会太差。",
    "心有山海，静而不争。",
    "日子很滚烫，又暖又明亮。",
    "你只管努力，时间会给你答案。",
    "生活明朗，万物可爱。",
    "眼里有光，心中有暖。",
    "山高路远，看世界也找自己。",
    "把日子过成自己喜欢的样子。",
    "今天也要好好吃饭，好好生活。",
    "所有的美好，都在赶来的路上。",
    "温柔半两，从容一生。",
    "心若向阳，无畏悲伤。",
    "愿你三冬暖，愿你春不寒。",
    "世界很大，带着微笑去看看。",
    "疲惫的时候，就抱抱自己。",
    "慢慢来，比较快。",
    "你若盛开，清风自来。",
    "平凡的日子，也要闪着光。",
    "今日份的快乐，正在加载。",
    "走过的路，都会算数。",
    "与其仰望别人，不如点亮自己。",
    "好好睡一觉，明天会更好。",
    "愿你眼里有星辰，脚下有远方。",
    "把平凡的事做好，就是不平凡。",
    "留一点温柔给自己。",
};

constexpr uint32_t kCount = sizeof(kQuotes) / sizeof(kQuotes[0]);
uint32_t s_index = 0;   // currently shown quote index
bool     s_shown = false;

// Deterministic day number: local calendar date collapsed to an integer, so
// the quote is stable for a whole day and advances with the date.
uint32_t dayNumber() {
  if (timeService::isSynced()) {
    time_t raw = static_cast<time_t>(timeService::epoch());
    struct tm* lt = localtime(&raw);
    if (lt)
      return static_cast<uint32_t>(lt->tm_year * 372 + lt->tm_mon * 31 +
                                   lt->tm_mday);
  }
  // Not synced yet: stable fallback (just show the first quote until NTP).
  return 0;
}

void publishQuote() {
  const uint32_t idx = dayNumber() % kCount;
  Snapshot s = snapshot();
  strncpy(s.quote, kQuotes[idx], sizeof(s.quote) - 1);
  s.quote[sizeof(s.quote) - 1] = 0;
  publish(s);
  s_index = idx;
  s_shown = true;
  SUN_LOG("quote", "daily quote #%lu/%lu: %s",
          static_cast<unsigned long>(idx + 1),
          static_cast<unsigned long>(kCount), kQuotes[idx]);
}

}  // namespace

void begin() {
  publishQuote();
}

void tick() {
  const uint32_t idx = dayNumber() % kCount;
  if (!s_shown || idx != s_index) publishQuote();
}

void dump() {
  SUN_LOG("quote", "index=%lu/%lu shown=%s",
          static_cast<unsigned long>(s_index + 1),
          static_cast<unsigned long>(kCount), s_shown ? "YES" : "NO");
}

}  // namespace quoteService
}  // namespace sunisle
