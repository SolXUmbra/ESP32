// SUNISLE B - memo/reminder home screen.
// Layout inherits A version's calm geometry (240x320, warm ivory); the photo
// viewport is replaced by a memo panel plus a tap-to-complete alert card.
#include "ui_sunisle.h"
#include <string.h>
#include <stdio.h>

#include "fonts/sunisle_cn.h"
#include "services/memo_service.h"

/* ----------------------------- Palette ---------------------------------- */
#define C_BG          lv_color_hex(0xF7F4EC)
#define C_PAPER       lv_color_hex(0xFFFEFA)
#define C_INK         lv_color_hex(0x353A36)
#define C_MUTED       lv_color_hex(0x858477)
#define C_SAND        lv_color_hex(0xEEE8DC)
#define C_ORANGE      lv_color_hex(0xE9A86D)
#define C_SAGE        lv_color_hex(0x9EAA91)
#define C_BLUE        lv_color_hex(0xB9CBD0)
#define C_LINE        lv_color_hex(0xE5E0D5)
#define C_ORANGE_INK  lv_color_hex(0xC97F3F)
#define C_SAGE_INK    lv_color_hex(0x74815F)
#define C_BLUE_INK    lv_color_hex(0x6F8A93)
#define C_GOLD        lv_color_hex(0xD9A441)

/* ------------------------------ Layout ---------------------------------- */
#define SCREEN_W      240
#define SCREEN_H      320
#define MEMO_X        12
#define MEMO_Y        69
#define MEMO_W        216
#define MEMO_H        164

static lv_obj_t *scr;
static lv_obj_t *date_label;
static lv_obj_t *time_label;
static lv_obj_t *temp_label;
static lv_obj_t *weather_desc_label;
static lv_obj_t *city_label;
static lv_obj_t *caption_label;
static lv_obj_t *status_label;
static lv_obj_t *brand_footer;
static lv_obj_t *memo_panel;
static lv_obj_t *memo_list;
static lv_obj_t *memo_count_label;
static lv_obj_t *alert_box;
static lv_obj_t *alert_label;
static lv_obj_t *weather_icon;
static lv_obj_t *home_group;
static lv_obj_t *memo_group;

static ui_weather_t current_weather = UI_WEATHER_UNKNOWN;

/* ------------------------------ Helpers --------------------------------- */
static void style_label(lv_obj_t *obj, lv_color_t color, const lv_font_t *font)
{
    lv_obj_set_style_text_color(obj, color, 0);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_opa(obj, LV_OPA_COVER, 0);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_CLIP);
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text,
                            lv_color_t color, const lv_font_t *font)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text ? text : "");
    style_label(label, color, font);
    return label;
}

static void make_line(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                      lv_coord_t w, lv_color_t color)
{
    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_remove_style_all(line);
    lv_obj_set_pos(line, x, y);
    lv_obj_set_size(line, w, 1);
    lv_obj_set_style_bg_color(line, color, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
}

/* Small hand-drawn sun/cloud icon. No emoji or external icon font required. */
static void draw_weather_icon(ui_weather_t condition)
{
    if (!weather_icon) return;
    lv_obj_clean(weather_icon);

    if (condition == UI_WEATHER_SUNNY || condition == UI_WEATHER_PARTLY_CLOUDY ||
        condition == UI_WEATHER_UNKNOWN) {
        lv_obj_t *sun = lv_obj_create(weather_icon);
        lv_obj_remove_style_all(sun);
        lv_obj_set_size(sun, 15, 15);
        lv_obj_set_pos(sun, 5, 4);
        lv_obj_set_style_radius(sun, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(sun, C_ORANGE, 0);
        lv_obj_set_style_bg_opa(sun, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(sun, 0, 0);
    }
    if (condition == UI_WEATHER_CLOUDY || condition == UI_WEATHER_PARTLY_CLOUDY ||
        condition == UI_WEATHER_RAIN || condition == UI_WEATHER_SNOW) {
        lv_obj_t *cloud_a = lv_obj_create(weather_icon);
        lv_obj_remove_style_all(cloud_a);
        lv_obj_set_size(cloud_a, 23, 10);
        lv_obj_set_pos(cloud_a, 8, 13);
        lv_obj_set_style_radius(cloud_a, 6, 0);
        lv_obj_set_style_bg_color(cloud_a, C_BLUE, 0);
        lv_obj_set_style_bg_opa(cloud_a, LV_OPA_COVER, 0);

        lv_obj_t *cloud_b = lv_obj_create(weather_icon);
        lv_obj_remove_style_all(cloud_b);
        lv_obj_set_size(cloud_b, 13, 13);
        lv_obj_set_pos(cloud_b, 13, 8);
        lv_obj_set_style_radius(cloud_b, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(cloud_b, C_BLUE, 0);
        lv_obj_set_style_bg_opa(cloud_b, LV_OPA_COVER, 0);
    }
    if (condition == UI_WEATHER_RAIN) {
        for (uint8_t i = 0; i < 3; i++) {
            lv_obj_t *drop = lv_obj_create(weather_icon);
            lv_obj_remove_style_all(drop);
            lv_obj_set_size(drop, 2, 6);
            lv_obj_set_pos(drop, 12 + i * 7, 25);
            lv_obj_set_style_radius(drop, 2, 0);
            lv_obj_set_style_bg_color(drop, C_BLUE, 0);
            lv_obj_set_style_bg_opa(drop, LV_OPA_COVER, 0);
        }
    }
}

static void create_home(void)
{
    scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, C_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Brand micro-header */
    brand_footer = make_label(scr, "晴屿", C_ORANGE_INK, &lv_font_montserrat_12);
    lv_obj_set_pos(brand_footer, 14, 10);

    lv_obj_t *brand_dot = lv_obj_create(scr);
    lv_obj_remove_style_all(brand_dot);
    lv_obj_set_size(brand_dot, 4, 4);
    lv_obj_set_pos(brand_dot, 39, 16);
    lv_obj_set_style_radius(brand_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(brand_dot, C_ORANGE, 0);
    lv_obj_set_style_bg_opa(brand_dot, LV_OPA_COVER, 0);

    lv_obj_t *brand_en = make_label(scr, "SUNISLE B", C_INK, &lv_font_montserrat_10);
    lv_obj_set_pos(brand_en, 46, 13);
    lv_obj_set_style_text_letter_space(brand_en, 1, 0);

    date_label = make_label(scr, "FRIDAY  NOV 06", C_MUTED, &lv_font_montserrat_10);
    lv_obj_set_pos(date_label, 14, 34);
    lv_obj_set_style_text_letter_space(date_label, 1, 0);

    /* Weather cluster */
    weather_icon = lv_obj_create(scr);
    lv_obj_remove_style_all(weather_icon);
    lv_obj_set_size(weather_icon, 38, 36);
    lv_obj_set_pos(weather_icon, 154, 11);
    draw_weather_icon(UI_WEATHER_UNKNOWN);

    temp_label = make_label(scr, "--°", C_ORANGE_INK, &lv_font_montserrat_18);
    lv_obj_set_width(temp_label, 48);
    lv_obj_set_style_text_align(temp_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(temp_label, 178, 12);

    weather_desc_label = make_label(scr, "WEATHER", C_SAGE_INK, &lv_font_montserrat_10);
    lv_obj_set_width(weather_desc_label, 72);
    lv_obj_set_style_text_align(weather_desc_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(weather_desc_label, 154, 35);

    make_line(scr, 14, 55, 212, C_LINE);

    /* ---- Memo panel replaces the A-version photo viewport ---- */
    memo_panel = lv_obj_create(scr);
    lv_obj_set_pos(memo_panel, MEMO_X, MEMO_Y);
    lv_obj_set_size(memo_panel, MEMO_W, MEMO_H);
    lv_obj_set_style_radius(memo_panel, 13, 0);
    lv_obj_set_style_bg_color(memo_panel, C_PAPER, 0);
    lv_obj_set_style_bg_opa(memo_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(memo_panel, 1, 0);
    lv_obj_set_style_border_color(memo_panel, C_LINE, 0);
    lv_obj_set_style_border_opa(memo_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(memo_panel, 0, 0);
    lv_obj_set_style_clip_corner(memo_panel, true, 0);
    lv_obj_clear_flag(memo_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *memo_title = make_label(memo_panel, "备忘录  /  MEMO", C_ORANGE_INK,
                                      &sunisle_cn_memo);
    lv_obj_set_pos(memo_title, 12, 9);
    make_line(memo_panel, 12, 30, 192, C_LINE);

    memo_list = make_label(memo_panel, "暂无备忘录\n等待 WorkBuddy 下发指令",
                           C_INK, &sunisle_cn_memo);
    lv_obj_set_pos(memo_list, 12, 39);
    lv_obj_set_width(memo_list, 192);
    lv_obj_set_height(memo_list, 100);
    lv_label_set_long_mode(memo_list, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_line_space(memo_list, 3, 0);

    memo_count_label = make_label(memo_panel, "", C_MUTED, &lv_font_montserrat_10);
    lv_obj_set_pos(memo_count_label, 12, 143);
    lv_obj_set_width(memo_count_label, 192);

    /* Alert card sits on top of the list while a reminder is due. */
    alert_box = lv_obj_create(scr);
    lv_obj_remove_style_all(alert_box);
    lv_obj_set_pos(alert_box, MEMO_X + 6, MEMO_Y + 34);
    lv_obj_set_size(alert_box, MEMO_W - 12, 96);
    lv_obj_set_style_radius(alert_box, 10, 0);
    lv_obj_set_style_bg_color(alert_box, C_SAND, 0);
    lv_obj_set_style_bg_opa(alert_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(alert_box, 2, 0);
    lv_obj_set_style_border_color(alert_box, C_ORANGE, 0);
    lv_obj_set_style_border_opa(alert_box, LV_OPA_COVER, 0);
    lv_obj_add_flag(alert_box, LV_OBJ_FLAG_HIDDEN);

    alert_label = make_label(alert_box, "", C_INK, &sunisle_cn_memo);
    lv_obj_set_pos(alert_label, 12, 12);
    lv_obj_set_width(alert_label, MEMO_W - 36);
    lv_obj_set_height(alert_label, 48);
    lv_label_set_long_mode(alert_label, LV_LABEL_LONG_CLIP);

    lv_obj_t *alert_hint = make_label(alert_box, "轻触屏幕完成", C_ORANGE_INK,
                                      &sunisle_cn_memo);
    lv_obj_set_pos(alert_hint, 12, 68);

    /* Caption / time / city / quote strip (unchanged geometry) */
    caption_label = make_label(scr, "", C_BLUE_INK, &lv_font_montserrat_10);
    lv_obj_set_pos(caption_label, 14, 239);
    lv_obj_set_width(caption_label, 212);
    lv_obj_set_style_text_align(caption_label, LV_TEXT_ALIGN_LEFT, 0);

    time_label = make_label(scr, "--:--", C_INK, &lv_font_montserrat_32);
    lv_obj_set_pos(time_label, 12, 255);
    lv_obj_set_width(time_label, 142);
    lv_obj_set_style_text_letter_space(time_label, 1, 0);

    city_label = make_label(scr, "渑池", C_BLUE_INK, &lv_font_montserrat_10);
    lv_obj_set_pos(city_label, 151, 267);
    lv_obj_set_width(city_label, 76);
    lv_obj_set_style_text_align(city_label, LV_TEXT_ALIGN_RIGHT, 0);

    status_label = make_label(scr, "", C_SAGE_INK, &lv_font_montserrat_10);
    lv_obj_set_pos(status_label, 14, 300);
    lv_obj_set_width(status_label, 212);
    lv_obj_set_style_text_align(status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(status_label, LV_LABEL_LONG_SCROLL_CIRCULAR);

    make_line(scr, 14, 292, 212, C_LINE);

    /* One transparent top-most click layer: tapping during an alert marks the
       memo done; otherwise it is a no-op (page switching is serial-driven). */
    lv_obj_t *touch_catcher = lv_obj_create(scr);
    lv_obj_remove_style_all(touch_catcher);
    lv_obj_set_size(touch_catcher, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(touch_catcher, 0, 0);
    lv_obj_add_flag(touch_catcher, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(touch_catcher, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(touch_catcher, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(touch_catcher,
                        [](lv_event_t* e) {
                          if (e->code != LV_EVENT_CLICKED) return;
                          if (sunisle::memoService::alertActive()) {
                            sunisle::memoService::markDone(
                                sunisle::memoService::alertId());
                            ui_clear_alert();
                          }
                        },
                        LV_EVENT_CLICKED, nullptr);

    home_group = scr;
    memo_group = memo_panel;
}

extern "C" void ui_init(void) { create_home(); }

extern "C" void ui_set_date(const char *date_text)
{
    if (date_label && date_text) lv_label_set_text(date_label, date_text);
}

extern "C" void ui_set_time(const char *time_text)
{
    if (time_label && time_text) lv_label_set_text(time_label, time_text);
}

extern "C" void ui_set_weather(ui_weather_t condition,
                               const char *temperature,
                               const char *description,
                               const char *city)
{
    current_weather = condition;
    draw_weather_icon(condition);
    if (temp_label && temperature) lv_label_set_text(temp_label, temperature);
    if (weather_desc_label && description) lv_label_set_text(weather_desc_label, description);
    if (city_label && city) lv_label_set_text(city_label, city);
}

extern "C" void ui_set_memos(const char *text)
{
    if (memo_list) lv_label_set_text(memo_list, text ? text : "");
}

extern "C" void ui_set_memo_count(const char *text)
{
    if (memo_count_label) lv_label_set_text(memo_count_label, text ? text : "");
}

extern "C" void ui_show_alert(const char *text)
{
    if (!alert_box) return;
    if (alert_label) lv_label_set_text(alert_label, text ? text : "");
    lv_obj_clear_flag(alert_box, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(alert_box);
}

extern "C" void ui_clear_alert(void)
{
    if (alert_box) lv_obj_add_flag(alert_box, LV_OBJ_FLAG_HIDDEN);
}

extern "C" void ui_set_status(const char *status)
{
    if (status_label && status) lv_label_set_text(status_label, status);
}

extern "C" void ui_show_home(void)
{
    if (scr) lv_scr_load(scr);
}

extern "C" void ui_show_memo_view(void)
{
    if (scr) lv_scr_load(scr);
}

/* Chinese font hand-off. The memo panel uses sunisle_cn_memo directly. */
extern "C" void ui_apply_cn_fonts(const lv_font_t *date_font,
                                  const lv_font_t *weather_font,
                                  const lv_font_t *city_font,
                                  const lv_font_t *status_font,
                                  const lv_font_t *quote_font)
{
    if (date_label && date_font)
        lv_obj_set_style_text_font(date_label, date_font, 0);
    if (weather_desc_label && weather_font)
        lv_obj_set_style_text_font(weather_desc_label, weather_font, 0);
    if (city_label && city_font)
        lv_obj_set_style_text_font(city_label, city_font, 0);
    if (brand_footer && date_font)
        lv_obj_set_style_text_font(brand_footer, date_font, 0);
    if (status_label && quote_font)
        lv_obj_set_style_text_font(status_label, quote_font, 0);
}
