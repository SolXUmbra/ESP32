// QINGYU UI 2.0 - four screens: BOOT / HOME / MEMOS / GUEST.
// Only HOME and MEMOS are navigable owner pages. GUEST is a privacy overlay,
// never part of the pager. Touch indev stays DISABLED (white-screen root
// cause); navigation and actions come from serial / Bridge commands.
//
// Palette, typography and 320x240 coordinates follow
// UI2_DESIGN/晴屿_UI2.0_视觉设计规范.md exactly.
#pragma once

#include <lvgl.h>
#include <stdint.h>

typedef enum {
    QY_PAGE_HOME = 0,
    QY_PAGE_MEMOS = 1,
    QY_PAGE_COUNT
} qy_page_t;

typedef enum {
    QY_PRIVACY_GUEST = 0,
    QY_PRIVACY_OWNER
} qy_privacy_t;

// §8.2 hardware-gated equivalents of the designed touch gestures.
typedef enum {
    QY_ACT_ACK = 0,      // tap 确认   on the smart reminder
    QY_ACT_LATER,         // tap 稍后   on the smart reminder
    QY_ACT_DETAIL,        // tap 查看详情 / memo card
    QY_ACT_TASK           // tap the task card / next-task row
} qy_action_t;

typedef struct {
    const char *clock;        // "09:41"  (public)
    const char *date_cn;      // "11月08日 星期日" (public)
    const char *status;      // "OFFLINE" / "DEMO" (public)
    const char *task_done;    // "03"
    const char *task_total;   // "08"
    const char *done_line;    // "已完成 3 项"
    uint8_t     progress_pct; // 0..100
    const char *summary1;
    const char *summary2;
    const char *next_time;    // "09:30"
    const char *next_title;
} qy_dashboard_t;

typedef struct {
    const char *pending;      // "2 待处理"
    const char *remind_time;  // "14:30"
    const char *remind_title;
    const char *remind_state; // "待处理" / "已确认" / "稍后处理"
    const char *remind_index; // "1 / 3"
    const char *memo_time;    // "08:12 更新"
    const char *memo_title;
    const char *memo_summary;
} qy_work_t;

typedef void (*qy_page_changed_cb_t)(qy_page_t page, void *user_data);
typedef void (*qy_action_cb_t)(qy_action_t action, void *user_data);

typedef struct {
    qy_page_changed_cb_t on_page_changed;
    qy_action_cb_t       on_action;
    void *user_data;
} qy_ui_callbacks_t;

void qy_ui_init(const qy_ui_callbacks_t *callbacks);

/* BOOT -> GUEST is the ONLY power-on path. Owner requires a verified
 * Bridge session; serial PRIVACY=OWNER is LOCAL DEBUG ONLY. */
void qy_ui_show_boot(void);
void qy_ui_set_privacy(qy_privacy_t mode);
qy_privacy_t qy_ui_get_privacy(void);
bool qy_ui_private_visible(void);

void qy_ui_show_page(qy_page_t page);
void qy_ui_next_page(void);

/* BOOT progress bar: one linear sweep over BOOT_TOTAL_MS. */
void qy_ui_set_boot_progress(uint8_t pct);

/* PUBLIC channels - always writable, even in Guest. */
void qy_ui_set_clock(const char *hhmm);
void qy_ui_set_date(const char *date_cn);
void qy_ui_set_quote(const char *text);
void qy_ui_set_status(const char *text, bool online);

/* PRIVATE channels - silently DROPPED while Guest/Boot (no text cached). */
void qy_ui_set_dashboard(const qy_dashboard_t *data);
void qy_ui_set_work(const qy_work_t *data);

/* Serial/Bridge equivalent of the designed tap gestures. */
void qy_ui_action(qy_action_t action);

/* TODO(stage D): Bridge-side rasterised dynamic text channel. A fixed CJK
 * subset can never cover arbitrary Bridge strings; until this exists the
 * receiver sanitises out-of-subset glyphs (see ui_bridge.cpp). */
// void qy_ui_set_dynamic_bitmap(uint8_t widget_id, const uint8_t *rgba,
//                               uint16_t w, uint16_t h);
