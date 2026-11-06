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

FONT_CANDIDATES = [
    ("C:/Windows/Fonts/msyh.ttc", 0),   # Microsoft YaHei (regular)
    ("C:/Windows/Fonts/simhei.ttf", 0), # SimHei fallback
]

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

# Sizes actually needed by the delivered ui_sunisle.cpp:
#   12 px -> header date line ("11月06日  星期五")
#   16 px -> weather description ("晴间多云") and city ("渑池")
#   12 px -> daily quote line ("把喜欢的时光，留在身边。")  -- separate subset
#   28 px -> brand name on the boot splash
# Each spec declares its own Montserrat fallback so ASCII keeps the exact
# metrics the vendor layout was designed around.
# Each size ships ONLY the glyphs it can actually be asked to draw: the app
# partition is 1.25 MB and every CJK glyph at 16 px costs 256 B of bitmap.
SPECS = [
    # date ("11月06日  星期五") + "未校时" when unsynced + the two brand
    # chars "晴屿" for the bottom-left logo. The weather description and city
    # render at 16 px, so the 12 px set stays tiny.
    {"name": "sunisle_cn_12", "size": 12,
     "chars": "".join(["未校时", DATE_CHARS, BRAND_CHARS]),
     "fallback": "lv_font_montserrat_12",
     "comment": "date line + bottom logo"},
    # boot slogan + WMO wording + city
    {"name": "sunisle_cn_16", "size": 16,
     "chars": "".join(["把喜欢的时光，留在身边。"] + WEATHER_STRINGS),
     "fallback": "lv_font_montserrat_16",
     "comment": "weather description + city"},
    # daily quote line (its own subset so the main date font stays small)
    {"name": "sunisle_cn_quote", "size": 12,
     "chars": "".join(QUOTES),
     "fallback": "lv_font_montserrat_12",
     "comment": "daily quote"},
    {"name": "sunisle_cn_28", "size": 28, "chars": BRAND_CHARS,
     "fallback": "lv_font_montserrat_14",
     "comment": "brand only"},
]


def load_font(size):
    for path, index in FONT_CANDIDATES:
        if os.path.exists(path):
            try:
                return ImageFont.truetype(path, size, index=index), path
            except Exception:
                continue
    raise RuntimeError("No usable CJK TTF found on this machine")


def render_glyph(font, ch, size):
    """Render one char into a size x size 8bpp alpha box, visually centred."""
    img = Image.new("L", (size, size), 0)
    d = ImageDraw.Draw(img)
    x0, y0, x1, y1 = font.getbbox(ch)
    gw, gh = x1 - x0, y1 - y0
    ox = (size - gw) // 2 - x0
    oy = (size - gh) // 2 - y0
    d.text((ox, oy), ch, font=font, fill=255)
    return bytes(img.getdata())


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
    unique = sorted({c for c in chars if ord(c) > 0x2E80})
    font, font_path = load_font(size)
    spec["font_path"] = font_path

    bitmap = bytearray()
    glyphs = [(0, 0, 0, 0, 0, 0)]  # glyph id 0 is RESERVED by LVGL
    for ch in unique:
        px = render_glyph(font, ch, size)
        index = len(bitmap)
        bitmap += px
        # adv_w is 8.4 fixed point (value * 16). Full-width CJK advance.
        glyphs.append((index, size * 16, size, size, 0, 0))

    cps = [ord(c) for c in unique]
    start, end = min(cps), max(cps)
    span = end - start + 1
    assert span <= 65535, "cmap range too wide for uint16 rcp"
    assert len(unique) <= 65534
    spec.update(bitmap=bytes(bitmap), glyphs=glyphs, unique=unique,
                range_start=start, range_length=span,
                line_height=size + 2, base_line=2)
    return spec


def generate():
    specs = [build(s) for s in SPECS]
    out = []
    out.append("// AUTO-GENERATED by tools/gen_cn_font.py - DO NOT EDIT BY HAND.")
    out.append("// Source font: %s" % specs[0]["font_path"])
    out.append("// " + ", ".join("%s=%d glyphs/%dB" %
                                 (s["name"], len(s["unique"]), len(s["bitmap"]))
                                 for s in specs))
    out.append("// 8bpp alpha, uniform square boxes, Montserrat fallback for ASCII.")
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

    print("wrote", HDR)
    print("wrote", SRC)
    for s in specs:
        print("  %s: %d glyphs, %d bytes bitmap, line_height=%d" %
              (s["name"], len(s["unique"]), len(s["bitmap"]), s["line_height"]))


if __name__ == "__main__":
    generate()
