// 每日一言 service - 每次开机换一句。
// 开机计数持久化在 NVS (Preferences)，每次启动 +1 后取模选句，
// 因此连续两次开机必然不同；30 句循环一轮后才会重复。
#include "services/quote_service.h"

#include "sunisle/state.h"
#include "utils/logger.h"

#include <Preferences.h>
#include <esp_system.h>
#include <string.h>

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
uint32_t s_index = 0;        // currently shown quote index
uint32_t s_bootCount = 0;    // persisted across reboots
bool     s_shown = false;

void publishQuote(uint32_t idx) {
  Snapshot s = snapshot();
  strncpy(s.quote, kQuotes[idx], sizeof(s.quote) - 1);
  s.quote[sizeof(s.quote) - 1] = 0;
  publish(s);
  s_index = idx;
  s_shown = true;
  SUN_LOG("quote", "boot #%lu -> #%lu/%lu: %s",
          static_cast<unsigned long>(s_bootCount),
          static_cast<unsigned long>(idx + 1),
          static_cast<unsigned long>(kCount), kQuotes[idx]);
}

}  // namespace

void begin() {
  // NVS lives at 0x9000 and is never touched by our app0-only flashing, so
  // the counter survives every reflash as well as every power cycle.
  Preferences prefs;
  if (prefs.begin("sunisle", false)) {
    s_bootCount = prefs.getUInt("bootCount", 0) + 1;
    prefs.putUInt("bootCount", s_bootCount);
    prefs.end();
  } else {
    s_bootCount = esp_random();   // NVS unavailable: still varies per boot
    SUN_LOG("quote", "NVS unavailable, using random seed");
  }
  publishQuote(s_bootCount % kCount);
}

void tick() {
  // The quote is chosen once at boot and stays for the whole session.
  // (Previously it re-picked after NTP, which made it jump mid-boot.)
  if (!s_shown) publishQuote(s_bootCount % kCount);
}

void dump() {
  SUN_LOG("quote", "boot=%lu index=%lu/%lu shown=%s",
          static_cast<unsigned long>(s_bootCount),
          static_cast<unsigned long>(s_index + 1),
          static_cast<unsigned long>(kCount), s_shown ? "YES" : "NO");
}

}  // namespace quoteService
}  // namespace sunisle
