// QINGYU UI 2.0 implementation.
//
// Four screens, strict 320x240 landscape coordinates:
//   BOOT  1.5 s  -> GUEST (power-on default, rice-paper privacy screen)
//   HOME / MEMOS are the only two navigable OWNER pages.
//   UI 2.2: one 简约中式 token set + 8 px grid for all four screens.
//
// Frozen hardware: setRotation(1), 20 MHz HSPI, no touch indev. Navigation
// and the four tap actions are driven from serial / Bridge (§8.2).
// Every dynamic label is bound to sunisle_cn_14 (the 300-glyph whitelist);
// sunisle_cn_16 is a slim static-only face (UI 2.0 §9.4).
#include "ui/qingyu_ui.h"

#include "fonts/sunisle_cn.h"
#include <stdio.h>
#include <string.h>

// ---- UI 2.2 "简约中式" design system (sRGB intent; panel is RGB565) -------
// One token set for ALL FOUR screens. Rejected predecessors: the dark
// graphite GUEST (too dim) and the GUEST 2.1 dopamine block colours (too
// loud). The new system is a warm rice-paper ground, ink type, ONE vermilion
// accent, and a single celadon status hue. Big-tech layout rules: 8 px grid,
// 16 px page margin (288 px content), 14 px card inset, 8 px corner radius,
// 1 px hairline borders, four-level type hierarchy, right-aligned status.
#define C_BG        0xF5F3ED   // 宣纸 rice-paper ground
#define C_SURFACE   0xFFFFFF   // card / sheet surface
#define C_SOFT      0xFAF8F3   // pill / secondary surface
#define C_INK       0x222A27   // 墨 primary ink
#define C_MUTED     0x717A74   // 淡墨 secondary ink
#define C_BRAND     0xB23A2E   // 朱砂 vermilion - the ONLY accent
#define C_TECH      0x6E8B7E   // 青瓷 celadon - online / progress
#define C_SUCCESS   0x6E8B7E
#define C_ALERT     0xB23A2E
#define C_LINE      0xE2DFD6   // hairline divider / card border

static lv_obj_t *s_root, *s_boot, *s_guest;
static lv_obj_t *s_pages[QY_PAGE_COUNT];
static lv_obj_t *s_boot_fill, *s_progress_fill;
static lv_obj_t *s_home_clock, *s_home_date, *s_status_label, *s_status_dot;
static lv_obj_t *s_task_done, *s_task_total, *s_done_line;
static lv_obj_t *s_summary, *s_next_time, *s_next_title;
static lv_obj_t *s_pending_label, *s_pending_dot;
static lv_obj_t *s_remind_time, *s_remind_title, *s_remind_state;
static lv_obj_t *s_remind_index, *s_ack_label, *s_later_label;
static lv_obj_t *s_memo_time, *s_memo_title, *s_memo_summary;
static lv_obj_t *s_guest_clock, *s_guest_date, *s_guest_quote;

static qy_privacy_t s_privacy = QY_PRIVACY_GUEST;
static qy_page_t    s_page = QY_PAGE_HOME;
static bool         s_private_visible = false;
static qy_ui_callbacks_t s_cb;

// Every label holding private text is registered here so the privacy
// transition clears the widget tree itself, not just a covering screen.
static lv_obj_t *s_private[16];
static uint8_t   s_private_n = 0;

static lv_color_t col(uint32_t c) { return lv_color_hex(c); }

static void reg_private(lv_obj_t *o) {
    if (o && s_private_n < sizeof(s_private) / sizeof(s_private[0]))
        s_private[s_private_n++] = o;
}
static void set_label(lv_obj_t *o, const char *s) {
    if (o) lv_label_set_text(o, s ? s : "");
}
static void align_right(lv_obj_t *o, int right_x, int y) {
    lv_obj_align(o, LV_ALIGN_TOP_RIGHT, -(320 - right_x), y);
}

// All coordinates below are ABSOLUTE on the 320x240 page: every widget is a
// direct child of the page, card rectangles are pure backgrounds. This keeps
// the design-sheet coordinate table and the firmware in exact agreement.
static lv_obj_t *lbl(lv_obj_t *p, const char *txt, uint32_t color,
                     const lv_font_t *font, int x, int y,
                     int w = 0, bool wrap = false) {
    lv_obj_t *o = lv_label_create(p);
    lv_label_set_text(o, txt ? txt : "");
    lv_obj_set_style_text_font(o, font, 0);
    lv_obj_set_style_text_color(o, col(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    // WRAP is LVGL's default too; with no explicit width the label expands
    // to its content and stays on one line. Explicit width => real wrapping.
    lv_label_set_long_mode(o, LV_LABEL_LONG_WRAP);
    (void)wrap;   // kept in the signature for the two-line call sites
    if (w > 0) lv_obj_set_width(o, w);
    lv_obj_set_pos(o, x, y);
    return o;
}
static lv_obj_t *cn14(lv_obj_t *p, const char *t, uint32_t c, int x, int y,
                      int w = 0, bool wrap = false) {
    return lbl(p, t, c, &sunisle_cn_14, x, y, w, wrap);
}
static lv_obj_t *cn16(lv_obj_t *p, const char *t, uint32_t c, int x, int y) {
    return lbl(p, t, c, &sunisle_cn_16, x, y);
}
static lv_obj_t *kai16(lv_obj_t *p, const char *t, uint32_t c, int x, int y) {
    return lbl(p, t, c, &sunisle_cn_kai_16, x, y);
}
static lv_obj_t *lat(lv_obj_t *p, const char *t, uint32_t c, int x, int y,
                     uint8_t spacing = 0, const lv_font_t *font = &lv_font_montserrat_12) {
    lv_obj_t *o = lbl(p, t, c, font, x, y);
    if (spacing) lv_obj_set_style_text_letter_space(o, spacing, 0);
    return o;
}
static lv_obj_t *num34(lv_obj_t *p, const char *t, uint32_t c, int x, int y) {
    return lbl(p, t, c, &sunisle_num_34, x, y);
}
static lv_obj_t *num56(lv_obj_t *p, const char *t, uint32_t c, int x, int y) {
    return lbl(p, t, c, &sunisle_num_56, x, y);
}

static lv_obj_t *rect(lv_obj_t *p, int x, int y, int w, int h,
                      uint32_t color, int radius = 0) {
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, col(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_shadow_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}
// Big-tech component rule: every card is ONE surface + ONE hairline border,
// same 8 px radius everywhere. No coloured panel fills, no shadows.
static lv_obj_t *card(lv_obj_t *p, int x, int y, int w, int h) {
    lv_obj_t *o = rect(p, x, y, w, h, C_SURFACE, 8);
    lv_obj_set_style_border_width(o, 1, 0);
    lv_obj_set_style_border_color(o, col(C_LINE), 0);
    return o;
}
static lv_obj_t *pill(lv_obj_t *p, int x, int y, int w, int h,
                      uint32_t fill, uint32_t border) {
    lv_obj_t *o = rect(p, x, y, w, h, fill, h / 2);
    if (border) {
        lv_obj_set_style_border_width(o, 1, 0);
        lv_obj_set_style_border_color(o, col(border), 0);
    }
    return o;
}
static lv_obj_t *dot(lv_obj_t *p, int cx, int cy, uint32_t color, bool filled) {
    lv_obj_t *o = filled ? rect(p, cx - 3, cy - 3, 6, 6, color, 3)
                         : rect(p, cx - 3, cy - 3, 6, 6, C_BG, 3);
    if (!filled) {
        lv_obj_set_style_border_width(o, 1, 0);
        lv_obj_set_style_border_color(o, col(C_MUTED), 0);
    }
    return o;
}
static void style_page(lv_obj_t *p, uint32_t background) {
    lv_obj_set_size(p, 320, 240);
    lv_obj_set_style_bg_color(p, col(background), 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(p, 0, 0);
    lv_obj_set_style_pad_all(p, 0, 0);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
}

// ---- shared top bar -------------------------------------------------------
static void topbar(lv_obj_t *p, lv_obj_t **status_out, lv_obj_t **dot_out,
                   uint32_t dot_color) {
    // 8 px grid: 16 px margin, 36 px bar, hairline at y=34. A 6 px vermilion
    // seal is the only Chinese ornament; the rest is a standard product bar.
    rect(p, 16, 10, 6, 6, C_BRAND, 1);           // 朱砂小印
    kai16(p, "晴屿", C_INK, 28, 7);
    lat(p, "SUNISLE", C_BRAND, 66, 11, 3);   // cinnabar wordmark, not 淡墨
    // Status group is right-aligned as ONE unit: text ends at 296, dot at
    // 300. No overlap regardless of caption length (USB ON / 2 待处理).
    *status_out = cn14(p, "USB OFF", C_MUTED, 0, 9);
    align_right(*status_out, 296, 9);
    *dot_out = dot(p, 300, 15, dot_color, true);
    rect(p, 16, 34, 288, 1, C_LINE);
}
static void pager(lv_obj_t *p, int active) {
    dot(p, 292, 230, C_BRAND, active == 0);
    dot(p, 302, 230, C_BRAND, active == 1);
}

// ---- 01 BOOT --------------------------------------------------------------
// Brand logo (32x32) drawn once onto a canvas: a cinnabar sun over an ink
// islet on a celadon sea line. "晴屿" = 晴 (sun) + 屿 (islet), so the mark
// matches the word, not an arbitrary glyph.
static lv_color_t s_logo_buf[32 * 32];
static lv_obj_t *boot_logo(lv_obj_t *p, int x, int y) {
    lv_obj_t *cv = lv_canvas_create(p);
    lv_canvas_set_buffer(cv, s_logo_buf, 32, 32, LV_IMG_CF_TRUE_COLOR);
    lv_canvas_fill_bg(cv, col(C_BG), LV_OPA_COVER);   // page-colour = invisible bg
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    // sun (cinnabar, upper right)
    d.bg_color = col(C_BRAND); d.bg_opa = LV_OPA_COVER; d.radius = LV_RADIUS_CIRCLE;
    lv_canvas_draw_rect(cv, 17, 1, 14, 14, &d);
    // sea line (celadon)
    d.radius = 0;
    d.bg_color = col(C_TECH);
    lv_canvas_draw_rect(cv, 3, 28, 26, 2, &d);
    // islet (ink triangle sitting on the sea line)
    d.bg_color = col(C_INK);
    static lv_point_t tri[3] = {{16, 12}, {6, 28}, {26, 28}};
    lv_canvas_draw_polygon(cv, tri, 3, &d);
    lv_obj_set_pos(cv, x, y);
    return cv;
}

static void build_boot(void) {
    s_boot = lv_obj_create(s_root);
    style_page(s_boot, C_BG);
    // Horizontal brand lock-up: logo + 晴屿 + SUNISLE, optically centred on
    // the 320-wide panel. One cinnabar hairline + the linear progress sweep
    // stay; only the tagline copy and the new logo changed.
    boot_logo(s_boot, 62, 66);                                   // 32x32 mark
    lbl(s_boot, "晴屿", C_INK, &sunisle_cn_28, 106, 67);        // Chinese, ink
    lat(s_boot, "SUNISLE", C_BRAND, 172, 82, 5, &lv_font_montserrat_14);  // Latin, cinnabar
    rect(s_boot, 136, 138, 48, 2, C_BRAND);
    lv_obj_t *sub = cn14(s_boot, "渑池高中AI桌面助理", C_MUTED, 0, 158);
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 158);
    rect(s_boot, 100, 206, 120, 2, C_LINE);
    s_boot_fill = rect(s_boot, 100, 206, 1, 2, C_BRAND);
}

// ---- 02 HOME --------------------------------------------------------------
static void build_home(void) {
    lv_obj_t *p = s_pages[QY_PAGE_HOME];
    style_page(p, C_BG);
    topbar(p, &s_status_label, &s_status_dot, C_TECH);

    rect(p, 16, 42, 24, 2, C_BRAND);
    s_home_clock = num34(p, "--:--", C_INK, 16, 48);
    s_home_date  = cn14(p, "11月08日 星期日", C_MUTED, 16, 96);

    card(p, 176, 40, 128, 88);
    cn14(p, "今日任务", C_MUTED, 190, 49);
    s_task_done  = num34(p, "00", C_BRAND, 190, 62);
    s_task_total = lat(p, "/ 00", C_MUTED, 224, 76, 0, &lv_font_montserrat_14);
    rect(p, 190, 100, 100, 4, C_LINE);
    s_progress_fill = rect(p, 190, 100, 1, 4, C_SUCCESS);
    s_done_line = cn14(p, "已完成 0 项", C_MUTED, 190, 109);

    rect(p, 16, 138, 2, 49, C_BRAND);
    cn14(p, "WORKBUDDY 摘要", C_MUTED, 28, 137);
    s_summary = cn14(p, "", C_INK, 28, 153, 276, true);
    lv_obj_set_height(s_summary, 38);

    rect(p, 16, 192, 288, 1, C_LINE);
    cn14(p, "下一项", C_MUTED, 16, 197);
    s_next_time  = lat(p, "--:--", C_BRAND, 0, 197, 0, &lv_font_montserrat_14);
    align_right(s_next_time, 304, 197);
    s_next_title = cn14(p, "", C_INK, 16, 211, 288);
    pager(p, 0);

    reg_private(s_task_done); reg_private(s_task_total);
    reg_private(s_done_line); reg_private(s_summary);
    reg_private(s_next_time); reg_private(s_next_title);
}

// ---- 03 MEMOS -------------------------------------------------------------
static void build_memos(void) {
    lv_obj_t *p = s_pages[QY_PAGE_MEMOS];
    style_page(p, C_BG);
    topbar(p, &s_pending_label, &s_pending_dot, C_ALERT);
    // topbar() wrote "USB OFF" into the right-hand caption; MEMOS uses the
    // pending count there instead. It is private and registered below.
    set_label(s_pending_label, "");
    reg_private(s_pending_label);

    cn16(p, "备忘与提醒", C_INK, 16, 36);
    lat(p, "<  >", C_MUTED, 286, 40, 1, &lv_font_montserrat_14);

    card(p, 16, 62, 288, 84);
    rect(p, 16, 70, 3, 68, C_BRAND);
    cn14(p, "智能提醒", C_MUTED, 30, 68);
    s_remind_state = cn14(p, "待处理", C_ALERT, 0, 68);
    align_right(s_remind_state, 294, 68);
    s_remind_time  = lat(p, "--:--", C_INK, 30, 84, 0, &lv_font_montserrat_16);
    s_remind_title = cn14(p, "", C_INK, 112, 90, 170);

    pill(p, 30, 116, 64, 20, C_BRAND, 0);
    s_ack_label = cn14(p, "确认", C_SURFACE, 48, 118);
    pill(p, 102, 116, 64, 20, C_SURFACE, C_LINE);
    s_later_label = cn14(p, "稍后", C_INK, 120, 118);
    s_remind_index = lat(p, "0 / 0", C_MUTED, 0, 121);
    align_right(s_remind_index, 294, 121);

    card(p, 16, 154, 288, 66);
    cn14(p, "AI 备忘", C_MUTED, 30, 160);
    s_memo_time = cn14(p, "--:-- 更新", C_MUTED, 0, 160);
    align_right(s_memo_time, 294, 160);
    s_memo_title = cn14(p, "", C_INK, 30, 176, 200);
    lv_obj_t *detail = cn14(p, "查看详情 >", C_BRAND, 0, 178);
    align_right(detail, 294, 178);
    s_memo_summary = cn14(p, "", C_MUTED, 30, 194, 250);
    pager(p, 1);

    reg_private(s_remind_time); reg_private(s_remind_title);
    reg_private(s_remind_state); reg_private(s_remind_index);
    reg_private(s_memo_time); reg_private(s_memo_title);
    reg_private(s_memo_summary);
}

// ---- 04 GUEST 2.2 (简约中式) --------------------------------------------
// Spec D semantically unchanged: ONLY public clock / date / quote, no private
// text ever cached here, no tap can enter the OWNER pages. The dopamine
// block-colour composition is withdrawn. The 56 px hero clock and 16 px hint
// are kept but now sit on a rice-paper ground with a single vermilion hairline
// - the same design system as BOOT / HOME / MEMOS, not a separate guest
// "skin". All caption labels ("现在时间" / "每日一言" / "访客模式" /
// "欢迎来访") are removed: they restate what the screen already shows.
//
// Big-tech layout: centred hero stack on an 8 px vertical rhythm, one
// hairline-bordered card at the bottom, 16 px page margin, no colour panels.
static void build_guest(void) {
    s_guest = lv_obj_create(s_root);
    style_page(s_guest, C_BG);

    // (1) product bar 0..36, identical rhythm to topbar(). No state badge:
    // the screen content itself already says this is the guest view.
    rect(s_guest, 16, 10, 6, 6, C_BRAND, 1);
    kai16(s_guest, "晴屿", C_INK, 28, 7);
    lat(s_guest, "SUNISLE", C_BRAND, 66, 11, 3);   // cinnabar wordmark
    rect(s_guest, 16, 34, 288, 1, C_LINE);

    // (2) hero stack. No "现在时间" / "每日一言" / "访客模式" / "欢迎来访"
    // captions - a clock is self-evident, a quote reads as a quote, and the
    // screen needs no label telling the visitor it is a guest screen. With
    // the 28 px Kai line removed the stack is re-centred in the band between
    // the product bar (34) and the quote card (204): ink 69..109 | 116..131
    // | rule 144 | 156..172, i.e. 35 px above and 32 px below -> optically
    // centred on the 320x240 panel.
    s_guest_clock = num56(s_guest, "--:--", C_INK, 0, 68);
    lv_obj_align(s_guest_clock, LV_ALIGN_TOP_MID, 0, 68);
    s_guest_date = cn16(s_guest, "11月08日 星期日", C_INK, 0, 116);
    lv_obj_align(s_guest_date, LV_ALIGN_TOP_MID, 0, 116);
    rect(s_guest, 136, 144, 48, 2, C_BRAND);          // 一线朱砂
    lv_obj_t *hint = lbl(s_guest, "主人暂时离开工位，请稍候。", C_MUTED,
                         &sunisle_cn_16, 0, 156);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 156);

    // (3) public quote: ONE centred line in the standard hairline card, no
    // label. Fixed-width box + CENTER keeps dynamic quotes centred without
    // re-anchoring; DOT keeps a long curated quote on a single line.
    card(s_guest, 16, 204, 288, 30);
    s_guest_quote = cn14(s_guest, "", C_MUTED, 26, 212, 268);
    lv_label_set_long_mode(s_guest_quote, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(s_guest_quote, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_height(s_guest_quote, 18);
}

// ---- privacy --------------------------------------------------------------
static void clear_private(void) {
    for (uint8_t i = 0; i < s_private_n; ++i) set_label(s_private[i], "");
    if (s_progress_fill) lv_obj_set_width(s_progress_fill, 0);
    // clock / date / quote / status / brand are PUBLIC and stay visible.
}

void qy_ui_init(const qy_ui_callbacks_t *callbacks) {
    if (callbacks) memcpy(&s_cb, callbacks, sizeof(s_cb));
    else memset(&s_cb, 0, sizeof(s_cb));
    s_root = lv_scr_act();
    lv_obj_set_style_bg_color(s_root, col(C_BG), 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);
    for (int i = 0; i < QY_PAGE_COUNT; ++i) s_pages[i] = lv_obj_create(s_root);
    build_home();
    build_memos();
    build_guest();
    build_boot();
    s_privacy = QY_PRIVACY_GUEST;
    s_page = QY_PAGE_HOME;
    qy_ui_show_boot();
}

void qy_ui_set_privacy(qy_privacy_t mode) {
    s_privacy = mode;
    if (!s_root) return;
    if (s_boot) lv_obj_add_flag(s_boot, LV_OBJ_FLAG_HIDDEN);
    if (mode == QY_PRIVACY_GUEST) {
        s_private_visible = false;
        clear_private();
        for (int i = 0; i < QY_PAGE_COUNT; ++i)
            lv_obj_add_flag(s_pages[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_guest, LV_OBJ_FLAG_HIDDEN);
    } else {
        s_private_visible = true;
        lv_obj_add_flag(s_guest, LV_OBJ_FLAG_HIDDEN);
        qy_ui_show_page(s_page);
    }
}
qy_privacy_t qy_ui_get_privacy(void) { return s_privacy; }
bool qy_ui_private_visible(void) { return s_private_visible; }

void qy_ui_show_boot(void) {
    if (!s_root) return;
    s_privacy = QY_PRIVACY_GUEST;
    s_private_visible = false;
    clear_private();
    for (int i = 0; i < QY_PAGE_COUNT; ++i)
        lv_obj_add_flag(s_pages[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_guest, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_boot, LV_OBJ_FLAG_HIDDEN);
}
void qy_ui_set_boot_progress(uint8_t pct) {
    if (!s_boot_fill) return;
    if (pct > 100) pct = 100;
    lv_obj_set_width(s_boot_fill, 120 * pct / 100);
}
void qy_ui_show_page(qy_page_t page) {
    if (page >= QY_PAGE_COUNT || !s_private_visible) return;
    s_page = page;
    if (s_boot) lv_obj_add_flag(s_boot, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < QY_PAGE_COUNT; ++i) {
        if (i == page) lv_obj_clear_flag(s_pages[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_pages[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (s_cb.on_page_changed) s_cb.on_page_changed(page, s_cb.user_data);
}
qy_page_t qy_ui_next_page(void) {
    if (!s_private_visible) return s_page;
    s_page = (qy_page_t)((s_page + 1) % QY_PAGE_COUNT);
    qy_ui_show_page(s_page);
    return s_page;
}

// ---- public channels -----------------------------------------------------
void qy_ui_set_clock(const char *hhmm) {
    set_label(s_home_clock, hhmm);
    set_label(s_guest_clock, hhmm);
    // Centred labels change width when the digits change; re-anchor to the
    // SAME y the build placed them (68) so the hero never drifts on a tick.
    if (s_guest_clock) lv_obj_align(s_guest_clock, LV_ALIGN_TOP_MID, 0, 68);
}
void qy_ui_set_date(const char *date_cn) {
    set_label(s_home_date, date_cn);
    set_label(s_guest_date, date_cn);
    if (s_guest_date) lv_obj_align(s_guest_date, LV_ALIGN_TOP_MID, 0, 116);
}
void qy_ui_set_quote(const char *text) { set_label(s_guest_quote, text); }
void qy_ui_set_status(const char *text, bool online) {
    set_label(s_status_label, text);
    if (s_status_label) align_right(s_status_label, 296, 9);
    if (s_status_dot) {
        lv_obj_set_style_bg_color(s_status_dot,
                                  col(online ? C_TECH : C_LINE), 0);
    }
}

// ---- private channels -----------------------------------------------------
void qy_ui_set_dashboard(const qy_dashboard_t *d) {
    if (!d || !s_private_visible) return;   // drop; never cache private text
    set_label(s_task_done, d->task_done);
    set_label(s_task_total, d->task_total);
    set_label(s_done_line, d->done_line);
    static char sum[160];
    snprintf(sum, sizeof(sum), "%s\n%s",
             d->summary1 ? d->summary1 : "", d->summary2 ? d->summary2 : "");
    set_label(s_summary, sum);
    set_label(s_next_time, d->next_time);
    if (s_next_time) align_right(s_next_time, 304, 197);
    set_label(s_next_title, d->next_title);
    if (s_progress_fill) {
        uint8_t pct = d->progress_pct > 100 ? 100 : d->progress_pct;
        lv_obj_set_width(s_progress_fill, pct);
    }
}
void qy_ui_set_work(const qy_work_t *w) {
    if (!w || !s_private_visible) return;
    set_label(s_pending_label, w->pending);
    if (s_pending_label) align_right(s_pending_label, 296, 9);
    set_label(s_remind_time, w->remind_time);
    set_label(s_remind_title, w->remind_title);
    set_label(s_remind_state, w->remind_state);
    if (s_remind_state) align_right(s_remind_state, 294, 68);
    set_label(s_remind_index, w->remind_index);
    if (s_remind_index) align_right(s_remind_index, 294, 121);
    set_label(s_memo_time, w->memo_time);
    if (s_memo_time) align_right(s_memo_time, 294, 160);
    set_label(s_memo_title, w->memo_title);
    set_label(s_memo_summary, w->memo_summary);
}

// ---- §8.2 serial/Bridge equivalents of the designed taps ------------------
void qy_ui_action(qy_action_t action) {
    if (!s_private_visible) return;
    switch (action) {
        case QY_ACT_ACK:
            set_label(s_remind_state, "已确认");
            if (s_remind_state) align_right(s_remind_state, 294, 68);
            lv_obj_set_style_bg_color(s_pending_dot, col(C_SUCCESS), 0);
            break;
        case QY_ACT_LATER:
            set_label(s_remind_state, "稍后处理");
            if (s_remind_state) align_right(s_remind_state, 294, 68);
            break;
        case QY_ACT_DETAIL:
            set_label(s_memo_summary, "已展开 检查场地设备，更新入库数据");
            break;
        case QY_ACT_TASK:
            set_label(s_done_line, "已完成 3 项");
            break;
        default: break;
    }
    if (s_cb.on_action) s_cb.on_action(action, s_cb.user_data);
}
