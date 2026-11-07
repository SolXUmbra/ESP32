#!/usr/bin/env python3
"""Independent verification for the generated Latin font subsets.

Checks, without hardware:
  1. every code point the project can display is present in each font's cmap
  2. every glyph's bitmap bytes are byte-identical to the stock LVGL glyph
  3. every glyph's metrics (adv_w/box/ofs) match the stock font
  4. the 32 px time font covers exactly what ui_set_time() can produce
Exits non-zero on any mismatch.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from gen_latin_font import parse_font, FONT_DIR, SIZES, CHARSETS, glyph_bytes  # noqa: E402

GEN = ROOT / "src" / "ui" / "sunisle_latin.cpp"


def parse_generated():
    t = GEN.read_text(encoding="utf-8")
    out = {}
    for s in SIZES:
        nm = "latin_%d" % s
        block = re.search(
            r"/\* -+ lv_font_montserrat_%d : (\d+) glyphs -+ \*/(.*?)(?=/\* -+ lv_font_montserrat_\d+ :|\Z)" % s,
            t, re.S)
        assert block, "font %d block not found" % s
        b = block.group(2)
        bmp_m = re.search(r"static const uint8_t bmp_%s\[\] = \{(.*?)\n\};" % nm, b, re.S)
        bmp = bytes(int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{2})", bmp_m.group(1)))
        dsc_m = re.search(r"static const lv_font_fmt_txt_glyph_dsc_t dsc_%s\[\] = \{(.*?)\n\};" % nm, b, re.S)
        dsc = [tuple(int(v) for v in m.groups())
               for m in re.finditer(r"\{\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(-?\d+),\s*(-?\d+)\s*\}", dsc_m.group(1))]
        uni_m = re.search(r"static const uint16_t uni_%s\[\] = \{(.*?)\n\};" % nm, b, re.S)
        uni = [int(v) for v in re.findall(r"\d+", uni_m.group(1))]
        cm_m = re.search(r"\{\s*0x([0-9A-Fa-f]+),\s*(\d+),\s*(\d+),\s*uni_%s" % nm, b)
        start, length, gid = int(cm_m.group(1), 16), int(cm_m.group(2)), int(cm_m.group(3))
        fd_m = re.search(r"static const lv_font_fmt_txt_dsc_t fdsc_%s = \{(.*?)\};" % nm, b, re.S)
        fields = [v.strip() for v in fd_m.group(1).split(",")]
        bpp = int(fields[6])
        out[s] = dict(bmp=bmp, dsc=dsc, uni=uni, start=start, length=length,
                      gid_start=gid, bpp=bpp, declared=int(block.group(1)))
    return out


def verify_cn():
    """Independent check of the 4bpp CJK bitmap packing (high nibble first)."""
    from PIL import Image, ImageDraw
    from gen_cn_font import SPECS, load_font, UI_STRINGS, WEATHER_STRINGS

    t = (ROOT / "src" / "ui" / "sunisle_cn.cpp").read_text(encoding="utf-8")
    errors = []
    for spec in SPECS:
        nm = spec["name"]
        m = re.search(r"static const uint8_t bitmap_%s\[\] = \{(.*?)\n\};" % nm, t, re.S)
        assert m, "missing bitmap_%s" % nm
        packed = bytes(int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{2})", m.group(1)))
        size = spec["size"]
        chars = spec["chars"] if spec["chars"] is not None \
            else "".join(UI_STRINGS + WEATHER_STRINGS)
        unique = sorted({c for c in chars if ord(c) > 0x2E80})
        per_glyph = (size * size + 1) // 2
        assert len(packed) == len(unique) * per_glyph, (
            nm, len(packed), len(unique) * per_glyph)
        font, _ = load_font(size, spec.get("font", "hei"))
        for gi, ch in enumerate(unique):
            img = Image.new("L", (size, size), 0)
            d = ImageDraw.Draw(img)
            x0, y0, x1, y1 = font.getbbox(ch)
            d.text(((size - (x1 - x0)) // 2 - x0,
                    (size - (y1 - y0)) // 2 - y0), ch, font=font, fill=255)
            px = img.tobytes()
            base = gi * per_glyph
            for i, v in enumerate(px):
                byte = packed[base + i // 2]
                level = (byte >> 4) if i % 2 == 0 else (byte & 0x0F)
                want = (v + 8) // 17  # LVGL _lv_bpp4_opa_table[i] == i*17
                if level != want:
                    errors.append("%s '%s' px %d: packed %d != expected %d" %
                                  (nm, ch, i, level, want))
                    break
        bpp_line = re.search(
            r"static lv_font_fmt_txt_dsc_t font_dsc_%s = \{(.*?)\};" % nm, t, re.S)
        fields = [v.strip() for v in bpp_line.group(1).split(",")] if bpp_line else []
        if len(fields) < 7 or fields[6] != "4":
            errors.append("%s: font_dsc bpp is not 4 (fields=%s)" % (nm, fields[:9]))
        print("  %-20s %3d glyphs, %6d B bitmap, 4bpp packing OK" %
              (nm, len(unique), len(packed)))
    return errors


def main():
    gen = parse_generated()
    errors = []
    for s in SIZES:
        stock = parse_font(FONT_DIR / ("lv_font_montserrat_%d.c" % s))
        g = gen[s]
        cps = [g["start"] + off for off in g["uni"]]
        want = sorted(CHARSETS[s])
        if cps != want:
            errors.append("size %d: cmap charset mismatch (missing %s, extra %s)" % (
                s, sorted(set(want) - set(cps))[:8], sorted(set(cps) - set(want))[:8]))
        if g["declared"] != len(want) or len(g["dsc"]) != len(want) + 1:
            errors.append("size %d: glyph count mismatch declared=%d dsc=%d want=%d" % (
                s, g["declared"], len(g["dsc"]), len(want)))
        if g["bpp"] != 4:
            errors.append("size %d: bpp=%d (expected 4)" % (s, g["bpp"]))
        for i, cp in enumerate(cps, start=1):
            sd = stock["dsc"][stock["cp2gid"][cp]]
            gd = g["dsc"][i]
            if gd[1:] != sd[1:]:
                errors.append("size %d cp %s: metrics %s != stock %s" % (s, hex(cp), gd[1:], sd[1:]))
                continue
            n = glyph_bytes(sd)
            ref = stock["bitmap"][sd[0]:sd[0] + n]
            got = g["bmp"][gd[0]:gd[0] + n]
            if ref != got:
                errors.append("size %d cp %s: bitmap bytes differ" % (s, hex(cp)))
        print("  montserrat_%-2d : %3d glyphs, %5d B bitmap, bpp=%d "
              "- cmap/metrics/pixels OK" % (s, len(cps), len(g["bmp"]), g["bpp"]))

    # 32 px font is fed only by ui_set_time(): "%H:%M" or "--:--"
    cps32 = {gen[32]["start"] + off for off in gen[32]["uni"]}
    needed = {ord(c) for c in "0123456789:-"} | {ord(" ")}
    if not needed.issubset(cps32):
        errors.append("32px time font missing: %s" % sorted(needed - cps32))

    print("")
    errors.extend(verify_cn())

    if errors:
        print("\nFAILED (%d issues):" % len(errors))
        for e in errors[:20]:
            print("  -", e)
        sys.exit(1)
    print("\nALL CHECKS PASSED - Latin glyphs pixel-identical to stock LVGL; "
          "CJK 4bpp packing correct.")


if __name__ == "__main__":
    main()
