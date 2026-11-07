#!/usr/bin/env python3
"""Generate size-optimised Latin subsets of LVGL's built-in Montserrat fonts.

Why: each stock lv_font_montserrat_NN.c ships 95 ASCII glyphs + 62 FontAwesome
icon glyphs (0xEFxx/0xF0xx) plus a ~3 KB kerning table. SUNISLE uses none of
the icons and its UI text is pure ASCII (Chinese goes through the CJK subset
fonts), so the icons+kerning are dead weight in the 1.25 MB app partition.

Method: parse the *stock* LVGL font C file and copy the exact bitmap bytes of
the glyphs we keep. Glyph pixels, metrics, line height and baseline are therefore
bit-identical to the original - only unused glyphs disappear.

Output defines lv_font_montserrat_{10,12,14,16,18,32} with the SAME symbol names.
Because the symbols are already resolved by our object file, the linker never
pulls the corresponding members out of liblvgl.a (static-archive semantics), so
lv_conf.h keeps LV_FONT_MONTSERRAT_* = 1 and no vendor UI file needs editing.

To add a future character: add its code point to BASE_CPS (or TIME_CPS if it
must appear in the 32 px clock), then re-run this script and verify_fonts.py.
The script fails loudly if a code point is missing from the stock font.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
FONT_DIR = ROOT / ".pio" / "libdeps" / "esp32dev" / "lvgl" / "src" / "font"
OUT_CPP = ROOT / "src" / "ui" / "sunisle_latin.cpp"
OUT_H = ROOT / "include" / "fonts" / "sunisle_latin.h"

SIZES = [10, 12, 14, 16, 18, 32]

# Every printable ASCII the UI can render, plus the degree sign used by "%d°".
# Chinese characters are never drawn by these fonts (CJK subsets own them).
BASE_CPS = list(range(0x20, 0x7F)) + [0x00B0]

# The 32 px font is used ONLY by time_label ("--:--" / "HH:MM").
TIME_CPS = [ord(c) for c in "0123456789:- "] + [0x00B0]

CHARSETS = {s: (set(TIME_CPS) if s == 32 else set(BASE_CPS)) for s in SIZES}


def parse_font(path):
    t = path.read_text(encoding="utf-8")

    m = re.search(r"static\s+(?:LV_ATTRIBUTE_LARGE_CONST\s+)?const uint8_t glyph_bitmap\[\] = \{(.*?)\n\};", t, re.S)
    bmp = bytes(int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{2})", m.group(1)))

    m = re.search(r"static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc\[\] = \{(.*?)\n\};", t, re.S)
    ds = []
    for line in m.group(1).splitlines():
        mm = re.search(
            r"\.bitmap_index\s*=\s*(\d+).*?\.adv_w\s*=\s*(\d+).*?"
            r"\.box_w\s*=\s*(\d+).*?\.box_h\s*=\s*(\d+).*?"
            r"\.ofs_x\s*=\s*(-?\d+).*?\.ofs_y\s*=\s*(-?\d+)", line)
        if mm:
            ds.append(tuple(int(x) for x in mm.groups()))
    assert len(ds) == 158, (path, len(ds))

    m = re.search(r"static const uint16_t unicode_list_1\[\] = \{(.*?)\n\};", t, re.S)
    sparse = [int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]+)", m.group(1))]
    assert len(sparse) == 62, (path, len(sparse))

    cmap0 = re.search(r"\.range_start = (\d+), \.range_length = (\d+), \.glyph_id_start = (\d+)", t)
    cmap1 = re.findall(r"\.range_start = (\d+), \.range_length = (\d+), \.glyph_id_start = (\d+)", t)
    assert len(cmap1) >= 2, path
    a_start, a_len, a_gid = (int(v) for v in cmap1[0])
    s_start, s_len, s_gid = (int(v) for v in cmap1[1])
    assert (a_start, a_len, a_gid) == (32, 95, 1), (path, cmap1)
    assert s_gid == 96, (path, cmap1)

    lh = int(re.search(r"\.line_height = (\d+)", t).group(1))
    bl = int(re.search(r"\.base_line = (\d+)", t).group(1))
    up = int(re.search(r"\.underline_position = (-?\d+)", t).group(1))
    ut = int(re.search(r"\.underline_thickness = (\d+)", t).group(1))
    bpp = int(re.search(r"\.bpp = (\d+)", t).group(1))
    assert bpp == 4, (path, bpp)

    cp2gid = {}
    for cp in range(a_start, a_start + a_len):
        cp2gid[cp] = a_gid + (cp - a_start)
    for i, off in enumerate(sparse):
        cp2gid[s_start + off] = s_gid + i
    return dict(bitmap=bmp, dsc=ds, cp2gid=cp2gid,
                line_height=lh, base_line=bl, underline_position=up,
                underline_thickness=ut, bpp=bpp)


def glyph_bytes(d):
    _, _, w, h, _, _ = d
    return (w * h + 1) // 2  # 4bpp: two pixels per byte


def c_array(name, data, per_line=16, hexfmt="0x%02X"):
    lines = ["static const uint8_t %s[] = {" % name]
    for i in range(0, len(data), per_line):
        lines.append("    " + ", ".join(hexfmt % b for b in data[i:i + per_line]) + ",")
    lines.append("};")
    return "\n".join(lines)


def build_subset(size, cps, stock):
    missing = sorted(cp for cp in cps if cp not in stock["cp2gid"])
    assert not missing, "size %d: code points missing from stock font: %s" % (
        size, [hex(c) for c in missing])

    cp2gid = stock["cp2gid"]
    ordered = sorted(cps)
    bitmap = bytearray()
    glyphs = [(0, 0, 0, 0, 0, 0)]  # id 0 is reserved by LVGL
    for cp in ordered:
        d = stock["dsc"][cp2gid[cp]]
        bi = d[0]
        n = glyph_bytes(d)
        raw = stock["bitmap"][bi:bi + n]
        assert len(raw) == n, (size, cp, bi, n, len(stock["bitmap"]))
        new_index = len(bitmap)
        bitmap += raw
        glyphs.append((new_index, d[1], d[2], d[3], d[4], d[5]))

    lo, hi = min(ordered), max(ordered)
    return dict(bitmap=bytes(bitmap), glyphs=glyphs, cps=ordered,
                range_start=lo, range_length=hi - lo + 1,
                line_height=stock["line_height"], base_line=stock["base_line"],
                up=stock["underline_position"], ut=stock["underline_thickness"],
                bpp=stock["bpp"])


def emit(s):
    nm = "latin_%d" % s["size"]
    out = []
    out.append("/* ---------- lv_font_montserrat_%d : %d glyphs ---------- */" %
               (s["size"], len(s["cps"])))
    out.append(c_array("bmp_" + nm, s["bitmap"]))
    out.append("")
    out.append("static const lv_font_fmt_txt_glyph_dsc_t dsc_%s[] = {" % nm)
    for g in s["glyphs"]:
        out.append("    {%u, %u, %u, %u, %d, %d}," % g)
    out.append("};")
    out.append("")
    vals = [cp - s["range_start"] for cp in s["cps"]]
    out.append("static const uint16_t uni_%s[] = {" % nm)
    for i in range(0, len(vals), 12):
        out.append("    " + ", ".join(str(v) for v in vals[i:i + 12]) + ",")
    out.append("};")
    out.append("")
    out.append("static const lv_font_fmt_txt_cmap_t cmap_%s[] = {" % nm)
    out.append("    {0x%X, %u, 1, uni_%s, nullptr, %u," %
               (s["range_start"], s["range_length"], nm, len(vals)))
    out.append("     LV_FONT_FMT_TXT_CMAP_SPARSE_TINY},")
    out.append("};")
    out.append("")
    out.append("static lv_font_fmt_txt_glyph_cache_t cache_%s;" % nm)
    out.append("static const lv_font_fmt_txt_dsc_t fdsc_%s = {" % nm)
    out.append("    bmp_%s, dsc_%s, cmap_%s, nullptr, 0, 1, %d, 0, 0, &cache_%s" %
               (nm, nm, nm, s["bpp"], nm))
    out.append("};")
    out.append("")
    out.append("const lv_font_t lv_font_montserrat_%d = {" % s["size"])
    out.append("    lv_font_get_glyph_dsc_fmt_txt, lv_font_get_bitmap_fmt_txt,")
    out.append("    %d, %d, LV_FONT_SUBPX_NONE, %d, %d," %
               (s["line_height"], s["base_line"], s["up"], s["ut"]))
    out.append("    &fdsc_%s, nullptr, nullptr" % nm)
    out.append("};")
    out.append("")
    return "\n".join(out)


def verify_coverage():
    """Static check: no ASCII/degree char in project strings falls outside BASE_CPS."""
    need = set()
    for p in list((ROOT / "src").rglob("*.cpp")) + list((ROOT / "include").rglob("*.h")):
        if "sunisle_latin.cpp" in p.name:
            continue
        text = p.read_text(encoding="utf-8", errors="replace")
        for lit in re.findall(r'"((?:[^"\\]|\\.)*)"', text):
            for ch in lit:
                cp = ord(ch)
                if 0x20 <= cp <= 0x7E or cp == 0xB0:
                    need.add(cp)
    # format specs and dynamic values are ASCII too
    missing = sorted(need - set(BASE_CPS))
    if missing:
        print("WARNING: source chars outside subset: %s" % [hex(c) for c in missing])
    else:
        print("coverage: OK, %d distinct ASCII/degree chars in project strings" % len(need))
    return not missing


def generate(stats_only=False):
    if not FONT_DIR.is_dir():
        sys.exit("LVGL sources not found at %s\n"
                 "Run 'pio run' once so PlatformIO fetches lib_deps, then retry."
                 % FONT_DIR)
    subsets = {}
    for s in SIZES:
        path = FONT_DIR / ("lv_font_montserrat_%d.c" % s)
        stock = parse_font(path)
        sub = build_subset(s, CHARSETS[s], stock)
        sub["size"] = s
        subsets[s] = sub
        print("  montserrat_%-2d : 158 -> %3d glyphs | bitmap %6d -> %5d B | "
              "kern+dsc+cmap trim ~%5d B" %
              (s, len(sub["cps"]), len(stock["bitmap"]), len(sub["bitmap"]),
               len(stock["bitmap"]) - len(sub["bitmap"])))
    if stats_only:
        return

    ok = verify_coverage()
    if not ok:
        print("Refusing to generate: charset coverage failed", file=sys.stderr)
        sys.exit(1)

    hdr = [
        "// AUTO-GENERATED by tools/gen_latin_font.py - DO NOT EDIT BY HAND.",
        "// Size-optimised Montserrat subsets (same symbols as LVGL built-ins).",
        "#pragma once",
        '#include "lvgl.h"',
        "",
    ]
    for s in SIZES:
        hdr.append("extern const lv_font_t lv_font_montserrat_%d;" % s)
    hdr.append("")
    OUT_H.parent.mkdir(parents=True, exist_ok=True)
    OUT_H.write_text("\n".join(hdr), encoding="utf-8")

    body = [
        "// AUTO-GENERATED by tools/gen_latin_font.py - DO NOT EDIT BY HAND.",
        "// Exact-pixel copies of LVGL Montserrat glyphs, minus unused FontAwesome",
        "// icons and kerning tables. Regenerate after changing EXTRA characters.",
        '#include "fonts/sunisle_latin.h"',
        "",
    ]
    for s in SIZES:
        body.append(emit(subsets[s]))
    OUT_CPP.write_text("\n".join(body), encoding="utf-8")
    print("wrote", OUT_CPP)
    print("wrote", OUT_H)


if __name__ == "__main__":
    generate(stats_only="--stats" in sys.argv)
