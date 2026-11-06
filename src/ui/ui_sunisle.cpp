#include "ui_sunisle.h"
#include <string.h>
#include <stdio.h>

/* ----------------------------- Palette ---------------------------------- */
#define C_BG          lv_color_hex(0xF7F4EC)  // warm ivory
#define C_PAPER       lv_color_hex(0xFFFEFA)
#define C_INK         lv_color_hex(0x353A36)
#define C_MUTED       lv_color_hex(0x858477)
#define C_SAND        lv_color_hex(0xEEE8DC)
#define C_ORANGE      lv_color_hex(0xE9A86D)
#define C_SAGE        lv_color_hex(0x9EAA91)
#define C_BLUE        lv_color_hex(0xB9CBD0)
#define C_LINE        lv_color_hex(0xE5E0D5)
#define C_WHITE       lv_color_hex(0xFFFFFF)

/* Deeper accents for small text on the ivory ground. The task-book palette
   tints (orange/sage/mist) are too light to read at 10 px, so these darker
   siblings carry the same hue at legible contrast. */
#define C_ORANGE_INK  lv_color_hex(0xC97F3F)  // burnt orange
#define C_SAGE_INK    lv_color_hex(0x74815F)  // deep sage
#define C_BLUE_INK    lv_color_hex(0x6F8A93)  // slate blue
#define C_GOLD        lv_color_hex(0xD9A441)  // warm gold, time accent

/* ------------------------------ Layout ---------------------------------- */
#define SCREEN_W      240
#define SCREEN_H      320

#define PHOTO_X       12
#define PHOTO_Y       69
#define PHOTO_W       216
#define PHOTO_H       164

static lv_obj_t *scr;
static lv_obj_t *date_label;
static lv_obj_t *time_label;
static lv_obj_t *temp_label;
static lv_obj_t *weather_desc_label;
static lv_obj_t *city_label;
static lv_obj_t *caption_label;
static lv_obj_t *status_label;
static lv_obj_t *brand_footer;
static lv_obj_t *photo_viewport;
static lv_obj_t *photo_img;
static lv_obj_t *photo_placeholder;
static lv_obj_t *weather_icon;
static lv_obj_t *home_group;
static lv_obj_t *photo_group;

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

static void set_photo_cover(const lv_img_dsc_t *image)
{
    if (!photo_viewport || !photo_img || !image) return;

    /* The adapter already decoded the JPEG into a VIEWPORT-SIZED sprite
       (216x164, contain-fit + sand letterbox). So here we show it 1:1: the
       image IS the viewport. No second zoom, no re-fit, no clip math. */
    lv_img_set_src(photo_img, image);
    lv_img_set_zoom(photo_img, 256);          /* 1.0x (8.8 fixed) */
    lv_img_set_antialias(photo_img, false);   /* 1:1 needs no filtering */
    lv_obj_set_pos(photo_img, 0, 0);
    lv_obj_set_size(photo_img, PHOTO_W, PHOTO_H);

    if (photo_placeholder)
        lv_obj_add_flag(photo_placeholder, LV_OBJ_FLAG_HIDDEN);
}

static void create_home(void)
{
    /* Root canvas */
    scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, C_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Brand micro-header */
    lv_obj_t *brand = make_label(scr, "SUNISLE", C_INK, &lv_font_montserrat_12);
    lv_obj_set_pos(brand, 14, 12);
    lv_obj_set_style_text_letter_space(brand, 2, 0);

    lv_obj_t *brand_dot = lv_obj_create(scr);
    lv_obj_remove_style_all(brand_dot);
    lv_obj_set_size(brand_dot, 5, 5);
    lv_obj_set_pos(brand_dot, 91, 17);
    lv_obj_set_style_radius(brand_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(brand_dot, C_ORANGE, 0);
    lv_obj_set_style_bg_opa(brand_dot, LV_OPA_COVER, 0);

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

    /* Photo viewport: soft paper frame, no dashboard card feeling. */
    photo_viewport = lv_obj_create(scr);
    lv_obj_set_pos(photo_viewport, PHOTO_X, PHOTO_Y);
    lv_obj_set_size(photo_viewport, PHOTO_W, PHOTO_H);
    lv_obj_set_style_radius(photo_viewport, 13, 0);
    lv_obj_set_style_bg_color(photo_viewport, C_SAND, 0);
    lv_obj_set_style_bg_opa(photo_viewport, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(photo_viewport, 1, 0);
    lv_obj_set_style_border_color(photo_viewport, C_LINE, 0);
    lv_obj_set_style_border_opa(photo_viewport, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(photo_viewport, 0, 0);
    lv_obj_set_style_clip_corner(photo_viewport, true, 0);
    lv_obj_clear_flag(photo_viewport, LV_OBJ_FLAG_SCROLLABLE);

    /* Placeholder artwork: calm sun and horizon until first local photo loads. */
    photo_placeholder = lv_obj_create(photo_viewport);
    lv_obj_remove_style_all(photo_placeholder);
    lv_obj_set_size(photo_placeholder, PHOTO_W, PHOTO_H);
    lv_obj_set_style_bg_color(photo_placeholder, lv_color_hex(0xEDE4D5), 0);
    lv_obj_set_style_bg_opa(photo_placeholder, LV_OPA_COVER, 0);

    lv_obj_t *sun = lv_obj_create(photo_placeholder);
    lv_obj_remove_style_all(sun);
    lv_obj_set_size(sun, 43, 43);
    lv_obj_set_pos(sun, 148, 34);
    lv_obj_set_style_radius(sun, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(sun, lv_color_hex(0xE9B982), 0);
    lv_obj_set_style_bg_opa(sun, LV_OPA_COVER, 0);

    lv_obj_t *horizon = lv_obj_create(photo_placeholder);
    lv_obj_remove_style_all(horizon);
    lv_obj_set_size(horizon, PHOTO_W, 70);
    lv_obj_set_pos(horizon, 0, 112);
    lv_obj_set_style_bg_color(horizon, lv_color_hex(0xC5C8B1), 0);
    lv_obj_set_style_bg_opa(horizon, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(horizon, 0, 0);

    lv_obj_t *horizon2 = lv_obj_create(photo_placeholder);
    lv_obj_remove_style_all(horizon2);
    lv_obj_set_size(horizon2, PHOTO_W, 43);
    lv_obj_set_pos(horizon2, 0, 139);
    lv_obj_set_style_bg_color(horizon2, lv_color_hex(0xA8B49B), 0);
    lv_obj_set_style_bg_opa(horizon2, LV_OPA_COVER, 0);

    lv_obj_t *placeholder_title = make_label(photo_placeholder, "A LITTLE MOMENT", C_INK,
                                              &lv_font_montserrat_12);
    lv_obj_set_pos(placeholder_title, 15, 16);
    lv_obj_set_style_text_letter_space(placeholder_title, 1, 0);

    lv_obj_t *placeholder_sub = make_label(photo_placeholder, "YOUR PHOTOS, CLOSE BY", C_MUTED,
                                            &lv_font_montserrat_10);
    lv_obj_set_pos(placeholder_sub, 15, 33);
    lv_obj_set_style_text_letter_space(placeholder_sub, 1, 0);

    photo_img = lv_img_create(photo_viewport);
    lv_obj_add_flag(photo_img, LV_OBJ_FLAG_HIDDEN);

    /* Caption */
    caption_label = make_label(scr, "", C_BLUE_INK, &lv_font_montserrat_10);
    lv_obj_set_pos(caption_label, 14, 239);
    lv_obj_set_width(caption_label, 212);
    lv_obj_set_style_text_align(caption_label, LV_TEXT_ALIGN_LEFT, 0);

    /* Time is the visual anchor at the bottom. */
    time_label = make_label(scr, "--:--", C_INK, &lv_font_montserrat_32);
    lv_obj_set_pos(time_label, 12, 255);
    lv_obj_set_width(time_label, 142);
    lv_obj_set_style_text_letter_space(time_label, 1, 0);

    city_label = make_label(scr, "YOUR PLACE", C_BLUE_INK, &lv_font_montserrat_10);
    lv_obj_set_pos(city_label, 151, 267);
    lv_obj_set_width(city_label, 76);
    lv_obj_set_style_text_align(city_label, LV_TEXT_ALIGN_RIGHT, 0);

    /* Bottom strip: brand logo (left, two-tone) + daily quote (right, scroll). */
    brand_footer = make_label(scr, "晴屿", C_ORANGE_INK, &lv_font_montserrat_12);
    lv_obj_set_pos(brand_footer, 14, 302);

    lv_obj_t *brand_footer_en = make_label(scr, "SUNISLE", C_INK,
                                            &lv_font_montserrat_10);
    lv_obj_set_pos(brand_footer_en, 46, 304);
    lv_obj_set_style_text_letter_space(brand_footer_en, 1, 0);

    lv_obj_t *brand_footer_dot = lv_obj_create(scr);
    lv_obj_remove_style_all(brand_footer_dot);
    lv_obj_set_size(brand_footer_dot, 4, 4);
    lv_obj_set_pos(brand_footer_dot, 41, 308);
    lv_obj_set_style_radius(brand_footer_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(brand_footer_dot, C_ORANGE, 0);
    lv_obj_set_style_bg_opa(brand_footer_dot, LV_OPA_COVER, 0);

    status_label = make_label(scr, "", C_SAGE_INK, &lv_font_montserrat_10);
    lv_obj_set_pos(status_label, 92, 303);
    lv_obj_set_width(status_label, 134);
    lv_obj_set_style_text_align(status_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(status_label, LV_LABEL_LONG_SCROLL_CIRCULAR);

    make_line(scr, 14, 292, 212, C_LINE);

    /* These handles are reserved for future page switching. */
    home_group = scr;
    photo_group = photo_viewport;
}

extern "C" void ui_init(void)
{
    create_home();
}

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

extern "C" void ui_show_photo(const lv_img_dsc_t *image)
{
    if (!image || !photo_img) return;
    lv_obj_clear_flag(photo_img, LV_OBJ_FLAG_HIDDEN);
    set_photo_cover(image);
}

extern "C" void ui_set_caption(const char *caption)
{
    if (!caption_label) return;
    if (!caption || caption[0] == '\0') {
        lv_label_set_text(caption_label, "");
        lv_obj_add_flag(caption_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(caption_label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(caption_label, caption);
    }
}

extern "C" void ui_set_status(const char *status)
{
    if (status_label && status) lv_label_set_text(status_label, status);
}

extern "C" void ui_show_home(void)
{
    if (scr) lv_scr_load(scr);
}

extern "C" void ui_show_photo_view(void)
{
    /* Phase 1 intentionally keeps one calm home screen.
       This entry point is reserved for a later photo-browser page. */
    if (scr) lv_scr_load(scr);
}

/* ---------------------------------------------------------------------------
 * SUNISLE firmware hook - APPENDED BY THE FIRMWARE PROJECT.
 *
 * create_home() above is UNTOUCHED: every coordinate, size, colour and
 * rounding value is exactly as delivered. The UI header documents that
 * Chinese strings ("11月06日  星期五", "晴朗", "多云") are supported as long as
 * the integrator supplies a Chinese font - this is that hand-off point.
 *
 * Call once after ui_init(). Passing NULL for a slot keeps the original
 * Montserrat font for that label.
 * ------------------------------------------------------------------------- */
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
    if (brand_footer && date_font)       /* "晴屿 SUNISLE" logo, cn 12 px */
        lv_obj_set_style_text_font(brand_footer, date_font, 0);
    if (status_label && quote_font)      /* daily quote text */
        lv_obj_set_style_text_font(status_label, quote_font, 0);
}
