#!/usr/bin/env python3
# SUNISLE Chinese subset font generator.
#
# Produces a genuine LVGL v8 `lv_font_fmt_txt` font containing ONLY the
# Chinese glyphs actually used by the UI (plus a small brand-only size).
# ASCII/digits keep LVGL's Montserrat via the font fallback pointer, so the
# Latin typography stays high quality without shipping a full CJK font.
#
# Rasteriser: Pillow + a local Windows CJK TTF. Nothing is downloaded.
# Output: include/fonts/sunisle_cn.h  +  src/ui/sunisle_cn.cpp
"""Generate the SUNISLE subset font from a real system CJK font."""

import os
import pathlib
from PIL import Image, ImageDraw, ImageFont

ROOT = pathlib.Path(__file__).resolve().parent.parent
HDR = ROOT / "include" / "fonts" / "sunisle_cn.h"
SRC = ROOT / "src" / "ui" / "sunisle_cn.cpp"

FONT_SETS = {
    # Sans: crisp for the live home screen (date / weather / quotes).
    "hei": [("C:/Windows/Fonts/msyh.ttc", 0),
            ("C:/Windows/Fonts/simhei.ttf", 0)],
    # Kai: brush-like, used only by the boot splash (brand + poem + seal).
    "kai": [("C:/Windows/Fonts/simkai.ttf", 0),
            ("C:/Windows/Fonts/simsun.ttc", 0)],
    # UI 2.0: thin modern digits for the 34 px clock.
    "latin_light": [("C:/Windows/Fonts/segoeuil.ttf", 0),
                    ("C:/Windows/Fonts/segoeui.ttf", 0),
                    ("C:/Windows/Fonts/arial.ttf", 0)],
}

# Boot splash slogan: FROZEN copy, identical to SLOGAN_CN in config.h.
# Rendered in Kai; the live UI keeps its sans subset untouched.
BOOT_STRINGS = "把喜欢的时光，留在身边。"

# Weather vocabulary for the REAL Open-Meteo WMO codes (see weather_service.cpp).
# Kept beside UI_STRINGS so a new WMO wording never renders as tofu boxes.
WEATHER_STRINGS = [
    "渑池", "池", "间", "毛", "冻", "阵", "雷", "暴", "冰", "雹", "粒",
    "夹", "局", "部", "微", "霾", "沙", "尘", "未", "知", "转", "到",
    "晴", "多云", "阴", "雾", "小雨", "中雨", "大雨", "阵雨", "雷阵雨",
    "小雪", "中雪", "大雪", "阵雪", "雨夹雪", "冻雨", "毛毛雨",
    "霾", "浮尘", "扬沙", "大风", "晴间多云", "阴有小雨", "强雷暴",
    "摄氏度", "度", "体感", "湿度",
    "更新中", "获取失败", "天气更新中", "无网络配置",
]

# Chinese date/time words for the header line (12 px subset, tiny).
DATE_CHARS = "年月日星期一二三四五六天"

# Every Chinese string used by the UI. The generator keeps the unique set, so
# adding a UI string here is all that is needed to make it renderable.
UI_STRINGS = [
    "把喜欢的时光，留在身边。",              # boot splash slogan (16 px)
    # date / weather placeholder line
    "未校时", "未配置", "天气未配置", "更新中", "无网络配置", "获取失败",
    "天气更新中",
    # weather wording (WMO 4677)
    "晴", "晴间多云", "多云", "阴", "雾", "毛毛雨", "冻雨", "雨",
    "小雨", "中雨", "大雨", "小雪", "中雪", "大雪", "雪粒",
    "阵雨", "阵雪", "雷阵雨", "雷暴冰雹", "未知",
    # city
    "渑池",
]

BRAND_CHARS = "晴屿"

# ---------------------------------------------------------------------------
# Stage C (B plan): every Chinese string that the delivered qingyu_ui six-page
# UI can render statically. Extracted from the delivery package + the local
# empty-state / placeholder copy added during integration. Dynamic Bridge
# text is NOT assumed to be covered: see B_DYNAMIC_FALLBACK and
# ui_bridge.cpp::sanitizeDynamic(), which downgrades out-of-subset glyphs
# until the Bridge-side rasterisation channel (stage D, TODO) is available.
# ---------------------------------------------------------------------------
B_UI_STRINGS = [
    "晴屿",
    "任务中心", "AI 备忘", "智能提醒",
    "早上好，今天也从容一点。", "下午好，今天也从容一点。",
    "晚上好，今天也从容一点。",
    "11月08日  星期日",
    "项待办", "项提醒", "条重点",
    "WORKBUDDY 摘要",
    "今天优先处理快递财务对账，", "下午检查场地设备。",
    "下一项", "核对昨日入库数据",
    "任务", "备忘", "提醒", "首页",
    "欢迎来访", "主人暂时离开工位，请稍候。", "私人工作内容已隐藏",
    "暂无任务", "暂无备忘", "暂无提醒", "暂无摘要", "暂无待办",
    "等待 Bridge 接入", "暂无数据", "未连接", "本地调试",
    "天气", "渑池", "每日一言",
]
# Placeholder vocabulary for Bridge-driven dynamic text before rasterisation.
B_DYNAMIC_FALLBACK = "等待接入暂无数据未连接更新中已同步离线"

# ---------------------------------------------------------------------------
# Typography variety (2026-11-08 07:57): a SMALL Kai face for section headings,
# the greetings and the vertical "每日一言" column label, so the six pages are
# not one uniform sans-serif. Deliberately tiny: every 16 px CJK glyph costs
# 256 B of app0 bitmap, and only ~75 KB of the partition is left.
# NOTE: "私人工作内容已隐藏" is no longer DISPLAYED, but stays in B_UI_STRINGS
# so the 288-glyph whitelist (and therefore sanitizeDynamic coverage) is
# unchanged. No runtime text regresses because of a copy edit.
# ---------------------------------------------------------------------------
KAI_TITLE = (
    "任务中心备忘智能提醒首页每日一言欢迎来访"
    "早上好今天也从容一点下午晚上"
    "主人暂时离开工位请稍候"
    "晴屿"
    "，。、！？；：（）"   # CJK punctuation must be IN the Kai face, not fallback
)


# ---------------------------------------------------------------------------
# Daily quotes ("每日一言"). Offline by design: the app partition is ~1.25 MB
# and a live quote API returns ARBITRARY Chinese that a subset font can never
# cover, so quotes are a fixed, curated list rotated by date (see quote
# service). The font ships ONLY the glyphs these 30 sentences actually use.
# ---------------------------------------------------------------------------
QUOTES = [
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
]

# ---------------------------------------------------------------------------
# UI 2.0 (2026-11-08). Every static Chinese string on the four new screens.
# The 11 glyphs that UI 1.0's 288-glyph whitelist did NOT cover are explicitly
# appended (see 晴屿_UI2.0_视觉设计规范.md §1.2), so no static caption can
# ever fall back to a tofu box.
# ---------------------------------------------------------------------------
UI2_MISSING = "伴侣动后启完情桌确详面"
UI2_STRINGS = [
    "晴屿", "AI 桌面工作伴侣", "渑池高中AI桌面助理", "正在启动",
    "今日任务", "已完成 3 项", "已完成 0 项", "已完成",
    "WORKBUDDY 摘要", "等待 Bridge 接入", "暂无摘要",
    "今天优先处理快递财务对账，", "下午检查场地设备。",
    "下一项", "暂无待办", "核对昨日入库数据",
    "备忘与提醒", "工作首页", "待处理", "已确认", "稍后处理", "已展开",
    "智能提醒", "检查场地设备", "确认", "稍后",
    "AI 备忘", "更新", "待办与摘要", "查看详情",
    "检查场地设备，更新入库数据",
    "欢迎来访", "主人暂时离开工位，请稍候。", "每日一言",
    "把喜欢的时光，留在身边。", "在线", "离线",
    "等待接入", "暂无备忘", "暂无提醒", "暂无数据",
    # GUEST 2.2 (简约中式): enlarged time / welcome layer, kept from 2.1.
    "访客模式", "现在时间", "稍候",
]

# GUEST 2.2 static-only 16 px glyphs. The guest date line ("11月08日 星期日")
# and the enlarged welcome hint are 16 px, so their CJK glyphs must live HERE,
# not just in the 14 px dynamic whitelist. Latin digits still fall back to
# Montserrat 16, whose baseline H=15 matches this face, so the mixed line
# stays straight.
UI2_16_GUEST = ("访客模式年月日星期一二三四五六天欢迎来访主人暂时离开工位请稍候"
                "每日一言未校时现在时间")

# 16 px sans face is now a TIGHT static-only face: title, card captions and
# buttons. Every dynamic/changing label (date, quote, summary, task/memo/
# reminder titles) is bound to the 14 px face instead. This is the §9.4
# slimming step: 288 -> ~60 glyphs, freeing ~60 KB of app0.
UI2_16 = (
    "晴屿"   # brand wordmark, now in the sans face (was only in the Kai faces)
    "备忘与提醒工作首页今日任务已完成项智能提醒待处理确认稍后摘要"
    "下一项每日一言欢迎来访查看详情已确认稍后处理已展开在线离线"
    "暂无备忘提醒数据主人暂时离开工位请稍候"
    "，。、！？；：（）"
) + UI2_16_GUEST

# 34 px Light numeric face for the clock. ASCII glyphs are kept INSIDE this
# face (build() normally filters them out). Every glyph now carries its REAL
# bounding box and its TTF advance, so "09:41" is typeset instead of being
# pasted as a row of square tiles.
NUM_CHARS = "0123456789:-/. "

# Baseline-to-line-top distance. A face that falls back to a built-in
# Montserrat face MUST use the same baseline offset, otherwise a mixed
# CJK/Latin line zig-zags vertically.
#   Montserrat 14: line_height 16, base_line 3  -> H = 13
#   Montserrat 16: line_height 18, base_line 3  -> H = 15
# The 28 px Kai brand and 34 px Light digits never mix with Latin, so they
# use the height their own glyphs need.
# 56 px Segoe UI Light: measured ink bbox y0=-40/-39, y1=0, so H=41 puts
# the digit top at line_top+1 and the bottom exactly on the baseline.
FONT_H = {14: 13, 16: 15, 28: 26, 34: 27, 56: 41}
FONT_BL = {14: 3, 16: 3, 28: 5, 34: 3, 56: 3}

# 8bpp alpha is kept deliberately: the 4bpp repacking in upstream 1ab7ea3
# caused visible missing glyphs on real hardware (B方案_白屏二分记录.md).
# specs[0] MUST stay the 14 px face: ui_bridge.cpp::sanitizeDynamic() derives
# its CJK whitelist from specs[0]["unique"].
ALL_CN = "".join([
    "".join(UI_STRINGS), "".join(WEATHER_STRINGS), DATE_CHARS, BRAND_CHARS,
    BOOT_STRINGS, "".join(QUOTES),
    "".join(B_UI_STRINGS), B_DYNAMIC_FALLBACK,
    "".join(UI2_STRINGS), UI2_MISSING,
])
SPECS = [
    {"name": "sunisle_cn_14", "size": 14, "chars": ALL_CN,
     "fallback": "lv_font_montserrat_14",
     "comment": "BODY/CAPTION + all dynamic text; whitelist source"},
    {"name": "sunisle_cn_16", "size": 16, "chars": UI2_16,
     "fallback": "lv_font_montserrat_16",
     "comment": "TITLE / card captions / buttons (static only, slimmed)"},
    {"name": "sunisle_cn_28", "size": 28, "chars": BRAND_CHARS + "欢迎来访",
     "fallback": "lv_font_montserrat_14", "font": "kai",
     "comment": "brand + GUEST oversized welcome heading, Kai 28 px"},
    {"name": "sunisle_cn_kai_16", "size": 16, "chars": KAI_TITLE,
     "fallback": "lv_font_montserrat_16", "font": "kai",
     "comment": "HEADING / greeting (Kai 16 px)"},
    {"name": "sunisle_num_34", "size": 34, "chars": NUM_CHARS,
     "fallback": "lv_font_montserrat_14", "font": "latin_light",
     "allow_ascii": True,
     "comment": "NUM_XL clock / task count, proportional Light digits"},
    {"name": "sunisle_num_56", "size": 56, "chars": NUM_CHARS,
     "fallback": "lv_font_montserrat_14", "font": "latin_light",
     "allow_ascii": True,
     "comment": "GUEST hero clock, 56 px proportional Light digits"},
]


def load_font(size, which="hei"):
    for path, index in FONT_SETS.get(which, FONT_SETS["hei"]):
        if os.path.exists(path):
            try:
                return ImageFont.truetype(path, size, index=index), path
            except Exception:
                continue
    raise RuntimeError("No usable CJK TTF found on this machine: " + which)


def render_glyph(font, ch):
    """Rasterise ONE glyph on the TTF's SHARED baseline, true bbox.

    The previous version centred each character's own ink box inside a
    size x size square. That destroys the baseline: a horizontal stroke (一),
    punctuation (，。), ':' and '1' all end up vertically centred while a
    full-height ideograph does not, so every line visibly zig-zags. This is
    what was reported as "歪歪扭扭" on real hardware.

    With Pillow's 'ls' (left-baseline) anchor the y argument IS the baseline
    for every character. We store exactly the ink bounding box plus its
    measured offset, which is what lv_font_conv emits and what LVGL expects.
    """
    x0, y0, x1, y1 = font.getbbox(ch, anchor="ls")
    w = max(1, x1 - x0)
    h = max(1, y1 - y0)
    img = Image.new("L", (w, h), 0)
    d = ImageDraw.Draw(img)
    # Shift so the ink box's own top-left is (0,0) of the bitmap.
    d.text((-x0, -y0), ch, font=font, anchor="ls", fill=255)
    # LVGL: box_bottom = line_top + H - ofs_y; ink_baseline = line_top + H.
    # Hence ofs_y = -y1 (negative when the ink descends below the baseline),
    # ofs_x = x0 (side bearing).
    return bytes(img.getdata()), w, h, x0, -y1


def emit_c_array(name, data, per_line=16):
    lines = ["static const uint8_t %s[] = {" % name]
    for i in range(0, len(data), per_line):
        chunk = ", ".join("0x%02X" % b for b in data[i:i + per_line])
        lines.append("    " + chunk + ",")
    lines.append("};")
    return "\n".join(lines)


def build(spec):
    size = spec["size"]
    chars = spec["chars"] if spec["chars"] is not None \
        else "".join(UI_STRINGS + WEATHER_STRINGS)
    allow_ascii = bool(spec.get("allow_ascii"))
    unique = sorted({c for c in chars if ord(c) > 0x2E80 or allow_ascii})
    font, font_path = load_font(size, spec.get("font", "hei"))
    spec["font_path"] = font_path

    bitmap = bytearray()
    glyphs = [(0, 0, 0, 0, 0, 0)]  # glyph id 0 is RESERVED by LVGL
    for ch in unique:
        px, bw, bh, ofs_x, ofs_y = render_glyph(font, ch)
        index = len(bitmap)
        bitmap += px
        # adv_w is 8.4 fixed point (value * 16). Use the TTF's REAL advance:
        # CJK is full-width, Latin digits are proportional.
        adv = int(round(font.getlength(ch) * 16))
        # LV_FONT_FMT_TXT_LARGE == 0 => adv_w is a 12-bit field, box_w/box_h
        # uint8_t, ofs_x/ofs_y int8_t, bitmap_index 20 bits. Guard all of it.
        assert 0 < adv < 4096, (ch, adv)
        assert 0 < bw < 256 and 0 < bh < 256, (ch, bw, bh)
        assert -128 <= ofs_x <= 127 and -128 <= ofs_y <= 127, (ch, ofs_x, ofs_y)
        glyphs.append((index, adv, bw, bh, ofs_x, ofs_y))
    assert len(bitmap) < (1 << 20), "bitmap_index overflow (20-bit field)"

    cps = [ord(c) for c in unique]
    start, end = min(cps), max(cps)
    span = end - start + 1
    assert span <= 65535, "cmap range too wide for uint16 rcp"
    assert len(unique) <= 65534
    h = FONT_H[size]
    bl = FONT_BL[size]
    # Verify the shared baseline actually fits the declared line box.
    spec.update(bitmap=bytes(bitmap), glyphs=glyphs, unique=unique,
                range_start=start, range_length=span,
                line_height=h + bl, base_line=bl, baseline_h=h)
    return spec


def generate():
    specs = [build(s) for s in SPECS]
    out = []
    out.append("// AUTO-GENERATED by tools/gen_cn_font.py - DO NOT EDIT BY HAND.")
    out.append("// Source font: %s" % specs[0]["font_path"])
    out.append("// " + ", ".join("%s=%d glyphs/%dB" %
                                 (s["name"], len(s["unique"]), len(s["bitmap"]))
                                 for s in specs))
    out.append("// 8bpp alpha, TRUE ink bounding boxes on a shared TTF baseline,")
    out.append("// Montserrat-compatible line metrics (CJK/Latin mixed lines align).")
    out.append("")
    out.append('#include "fonts/sunisle_cn.h"')
    out.append('#include "lvgl.h"')
    out.append("")

    for s in specs:
        nm = s["name"]
        out.append("/* ---------- %s (%s) ---------- */" % (nm, s["comment"]))
        out.append(emit_c_array("bitmap_" + nm, s["bitmap"]))
        out.append("")
        out.append("static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc_%s[] = {" % nm)
        for g in s["glyphs"]:
            out.append("    {%u, %u, %u, %u, %d, %d}," % g)
        out.append("};")
        out.append("")
        out.append("static const uint16_t unicode_%s[] = {" % nm)
        vals = [cp - s["range_start"] for cp in [ord(c) for c in s["unique"]]]
        for i in range(0, len(vals), 12):
            out.append("    " + ", ".join(str(v) for v in vals[i:i + 12]) + ",")
        out.append("};")
        out.append("")
        out.append("static const lv_font_fmt_txt_cmap_t cmap_%s[] = {" % nm)
        out.append("    {0x%X, %u, 1, unicode_%s, nullptr, %u," %
                   (s["range_start"], s["range_length"], nm, len(vals)))
        out.append("     LV_FONT_FMT_TXT_CMAP_SPARSE_TINY},")
        out.append("};")
        out.append("")
        out.append("static lv_font_fmt_txt_dsc_t font_dsc_%s = {" % nm)
        out.append("    bitmap_%s, glyph_dsc_%s, cmap_%s, nullptr, 0, 1, 8, 0, 0, nullptr" %
                   (nm, nm, nm))
        out.append("};")
        out.append("")
        out.append("const lv_font_t %s = {" % nm)
        out.append("    lv_font_get_glyph_dsc_fmt_txt, lv_font_get_bitmap_fmt_txt,")
        out.append("    %d, %d, LV_FONT_SUBPX_NONE, -1, 1," %
                   (s["line_height"], s["base_line"]))
        out.append("    &font_dsc_%s, &%s, nullptr" % (nm, s["fallback"]))
        out.append("};")
        out.append("")

    SRC.parent.mkdir(parents=True, exist_ok=True)
    SRC.write_text("\n".join(out), encoding="utf-8")

    hdr = [
        "// AUTO-GENERATED by tools/gen_cn_font.py - DO NOT EDIT BY HAND.",
        "// Minimal CJK subset fonts for SUNISLE. No full Chinese font is linked.",
        "#pragma once",
        "",
        '#include "lvgl.h"',
        "",
    ]
    for s in specs:
        hdr.append("extern const lv_font_t %s;  // %d px, %s" %
                   (s["name"], s["size"], s["comment"]))
    hdr.append("")
    HDR.parent.mkdir(parents=True, exist_ok=True)
    HDR.write_text("\n".join(hdr), encoding="utf-8")

    # Whitelist consumed by ui_bridge.cpp::sanitizeDynamic(): Bridge text with
    # CJK outside this set is downgraded instead of showing tofu boxes.
    cps = sorted({ord(c) for c in specs[0]["unique"]})
    wl = HDR.parent / "sunisle_cn_chars.h"
    rows = []
    for i in range(0, len(cps), 12):
        rows.append("    " + ", ".join("0x%04X" % c for c in cps[i:i + 12]) + ",")
    wl.write_text(
        "// AUTO-GENERATED by tools/gen_cn_font.py - DO NOT EDIT BY HAND.\n"
        "// Codepoints of the CJK glyphs present in sunisle_cn_14.\n"
        "#pragma once\n\n"
        "#include <stdint.h>\n"
        "#include <stddef.h>\n\n"
        "static const uint32_t kSunisleCnCodepoints[] = {\n" +
        "\n".join(rows) + "\n};\n"
        "static const size_t kSunisleCnCount = %d;\n" % len(cps),
        encoding="utf-8")
    print("wrote", wl)

    print("wrote", HDR)
    print("wrote", SRC)
    for s in specs:
        print("  %s: %d glyphs, %d bytes bitmap, line_height=%d" %
              (s["name"], len(s["unique"]), len(s["bitmap"]), s["line_height"]))


if __name__ == "__main__":
    generate()
