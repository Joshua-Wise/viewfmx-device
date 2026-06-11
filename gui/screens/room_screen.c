#include "room_screen.h"
#include "settings_overlay.h"
#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef ESP_PLATFORM
/* newlib has no timegm(); see platform/esp32/compat.c */
time_t timegm(struct tm *tm);
#endif

/* ------------------------------------------------------------------ */
/* Widget handles                                                       */
/* ------------------------------------------------------------------ */

static lv_obj_t *g_screen;

/* Header */
static lv_obj_t *lbl_room_name;

LV_IMAGE_DECLARE(img_district_logo);

/* Left panel: current status */
static lv_obj_t *panel_current;
static lv_obj_t *lbl_cur_hdr;        /* "Current meeting" (hidden when free) */
static lv_obj_t *lbl_cur_title;      /* meeting title, or "Available"        */
static lv_obj_t *lbl_cur_time;       /* "Wed Jun 11  1:00 PM - 4:00 PM"      */
static lv_obj_t *btn_book;           /* floating "+", shown when free        */
static lv_obj_t *g_book_modal;

/* Right panel: next meeting */
static lv_obj_t *panel_next;
static lv_obj_t *lbl_next_title;
static lv_obj_t *lbl_next_date;
static lv_obj_t *lbl_next_time;

/* Bottom strip: meetings after the next one */
#define BOTTOM_COLS 4
static lv_obj_t *panel_bottom;
static lv_obj_t *lbl_bot_title[BOTTOM_COLS];
static lv_obj_t *lbl_bot_date[BOTTOM_COLS];
static lv_obj_t *lbl_bot_time[BOTTOM_COLS];

/* Provider reference kept for button callbacks */
static ViewFMX_DataProvider *g_provider;
static char g_resource_id[32];
static char g_building_id[32];

/* ------------------------------------------------------------------ */
/* Colour palette (mirrors the viewFMX web app)                         */
/* ------------------------------------------------------------------ */

#define CLR_LIGHT       lv_color_hex(0xD1D5DB)   /* header / bottom strip */
#define CLR_PANEL_DARK  lv_color_hex(0x475569)   /* current-status panel  */
#define CLR_PANEL_MED   lv_color_hex(0x6B7280)   /* next-meeting panel    */
#define CLR_TEXT_DARK   lv_color_hex(0x111827)
#define CLR_TEXT_MUTED  lv_color_hex(0x4B5563)   /* on light backgrounds  */
#define CLR_TEXT_SOFT   lv_color_hex(0xD1D5DB)   /* on dark backgrounds   */
#define CLR_WHITE       lv_color_hex(0xFFFFFF)
#define CLR_BLUE        lv_color_hex(0x3B82F6)
#define CLR_ORANGE      lv_color_hex(0xF59E0B)
#define CLR_CANCEL_BG   lv_color_hex(0xE5E7EB)

#define HEADER_H  96
#define BOTTOM_H  150
#define PANEL_H   (600 - HEADER_H - BOTTOM_H)

/* ------------------------------------------------------------------ */
/* Time helpers                                                         */
/* ------------------------------------------------------------------ */

/* Parses an ISO 8601 UTC string into local broken-down time.
 * On the ESP32 the local timezone is set via ESP-IDF's POSIX TZ env var
 * before gui_init() is called; on macOS it follows the system timezone.
 * Returns false on parse failure. */
static bool iso_to_local(const char *iso, struct tm *local)
{
    if (!iso || !iso[0]) return false;

    struct tm utc = {0};
    if (sscanf(iso, "%d-%d-%dT%d:%d:%d",
               &utc.tm_year, &utc.tm_mon, &utc.tm_mday,
               &utc.tm_hour, &utc.tm_min, &utc.tm_sec) != 6) {
        return false;
    }
    utc.tm_year -= 1900;
    utc.tm_mon  -= 1;

    /* timegm() treats the struct as UTC and returns a UTC epoch.
     * It is available on macOS and glibc; on ESP-IDF use a shim if needed. */
    time_t t = timegm(&utc);
    if (t == (time_t)-1) return false;

    localtime_r(&t, local);
    return true;
}

/* "3:30 PM" */
static void fmt_time(char *buf, size_t sz, const char *iso)
{
    struct tm local;
    if (!iso_to_local(iso, &local)) { snprintf(buf, sz, "--:--"); return; }

    strftime(buf, sz, "%I:%M %p", &local);

    /* Strip leading zero from hour (e.g. "09:30 AM" -> "9:30 AM") */
    if (buf[0] == '0') memmove(buf, buf + 1, sz - 1);
}

/* "Wed Jun 11" */
static void fmt_date(char *buf, size_t sz, const char *iso)
{
    struct tm local;
    if (!iso_to_local(iso, &local)) { snprintf(buf, sz, "--"); return; }

    char dow_mon[16];
    strftime(dow_mon, sizeof(dow_mon), "%a %b", &local);
    snprintf(buf, sz, "%s %d", dow_mon, local.tm_mday);
}

/* "1:00 PM - 4:00 PM" */
static void fmt_range(char *buf, size_t sz, const char *start_iso, const char *end_iso)
{
    char ts[16], te[16];
    fmt_time(ts, sizeof(ts), start_iso);
    fmt_time(te, sizeof(te), end_iso);
    snprintf(buf, sz, "%s - %s", ts, te);
}

static const char *meeting_title(const ViewFMX_Meeting *m)
{
    return m->title[0] ? m->title : "Reserved";
}

/* ------------------------------------------------------------------ */
/* Button event callbacks                                               */
/* ------------------------------------------------------------------ */

static void logo_cb(lv_event_t *e)
{
    (void)e;
    settings_overlay_open(g_provider, g_resource_id, g_building_id);
}

static void book_modal_close(void)
{
    if (g_book_modal) {
        lv_obj_delete(g_book_modal);
        g_book_modal = NULL;
    }
}

static void book_cancel_cb(lv_event_t *e)
{
    (void)e;
    book_modal_close();
}

static void book_cb(lv_event_t *e)
{
    int minutes = (int)(intptr_t)lv_event_get_user_data(e);
    book_modal_close();
    if (g_provider->book_room(g_provider->ctx, minutes) == 0) {
        /* Refresh to show new booking */
        ViewFMX_RoomData data = {0};
        if (g_provider->fetch_status(g_provider->ctx, &data) == 0) {
            room_screen_update(&data);
        }
    }
}

static lv_obj_t *make_modal_btn(lv_obj_t *parent, const char *txt,
                                lv_color_t bg, lv_color_t fg)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_style_bg_color(btn, bg, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_t *l = lv_label_create(btn);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(l, fg, 0);
    lv_label_set_text(l, txt);
    lv_obj_center(l);
    return btn;
}

static void book_open_cb(lv_event_t *e)
{
    (void)e;
    if (g_book_modal) return;

    g_book_modal = lv_obj_create(lv_layer_top());
    lv_obj_set_size(g_book_modal, 1024, 600);
    lv_obj_set_style_bg_color(g_book_modal, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(g_book_modal, LV_OPA_50, 0);
    lv_obj_set_style_border_width(g_book_modal, 0, 0);
    lv_obj_set_style_radius(g_book_modal, 0, 0);
    lv_obj_clear_flag(g_book_modal, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *card = lv_obj_create(g_book_modal);
    lv_obj_set_size(card, 560, 236);
    lv_obj_center(card);
    lv_obj_set_style_bg_color(card, CLR_WHITE, 0);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 24, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(card);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, CLR_TEXT_DARK, 0);
    lv_label_set_text(title, "Schedule a Quick Meeting:");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *b30 = make_modal_btn(card, "30 Minutes", CLR_BLUE, CLR_WHITE);
    lv_obj_set_size(b30, 246, 56);
    lv_obj_align(b30, LV_ALIGN_TOP_LEFT, 0, 48);
    lv_obj_add_event_cb(b30, book_cb, LV_EVENT_CLICKED, (void *)(intptr_t)30);

    lv_obj_t *b60 = make_modal_btn(card, "1 Hour", CLR_BLUE, CLR_WHITE);
    lv_obj_set_size(b60, 246, 56);
    lv_obj_align(b60, LV_ALIGN_TOP_RIGHT, 0, 48);
    lv_obj_add_event_cb(b60, book_cb, LV_EVENT_CLICKED, (void *)(intptr_t)60);

    lv_obj_t *cancel = make_modal_btn(card, "Cancel", CLR_CANCEL_BG, CLR_TEXT_MUTED);
    lv_obj_set_size(cancel, 512, 52);
    lv_obj_align(cancel, LV_ALIGN_TOP_LEFT, 0, 120);
    lv_obj_add_event_cb(cancel, book_cancel_cb, LV_EVENT_CLICKED, NULL);
}

/* ------------------------------------------------------------------ */
/* Layout creation                                                      */
/* ------------------------------------------------------------------ */

/*
 * Layout (1024 x 600), mirroring the viewFMX web app:
 *
 *  ┌──────────────────────── HEADER (light, h=96) ──────────────────────┐
 *  │  Room Name                                                  (⟳)    │
 *  ├──────────────── LEFT (dark, 512) ──┬───────── RIGHT (med, 512) ────┤
 *  │  Current meeting                   │  Next meeting                 │
 *  │  Title                             │  Title                        │
 *  │  Wed Jun 11  1:00 PM - 4:00 PM     │  Wed Jun 11                   │
 *  │  85 min left                       │  2:45 PM - 3:45 PM            │
 *  │  [Book 30] [Book 60] (when free)   │                               │
 *  ├──────────────────── BOTTOM STRIP (light, h=150) ───────────────────┤
 *  │  Title         Title         Title         Title                   │
 *  │  date/time     date/time     date/time     date/time               │
 *  └─────────────────────────────────────────────────────────────────────┘
 */

static lv_obj_t *make_panel(lv_obj_t *parent, lv_color_t color)
{
    lv_obj_t *p = lv_obj_create(parent);
    lv_obj_set_style_bg_color(p, color, 0);
    lv_obj_set_style_border_width(p, 0, 0);
    lv_obj_set_style_radius(p, 0, 0);
    lv_obj_set_style_pad_all(p, 24, 0);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    return p;
}

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_label_set_text(l, "");
    return l;
}

void room_screen_create(ViewFMX_DataProvider *provider,
                        const char *resource_id,
                        const char *building_id)
{
    g_provider = provider;
    strncpy(g_resource_id, resource_id, sizeof(g_resource_id) - 1);
    strncpy(g_building_id, building_id, sizeof(g_building_id) - 1);

    g_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_screen, CLR_PANEL_DARK, 0);

    /* ---- Header ---- */
    lv_obj_t *header = make_panel(g_screen, CLR_LIGHT);
    lv_obj_set_size(header, 1024, HEADER_H);
    lv_obj_align(header, LV_ALIGN_TOP_LEFT, 0, 0);

    lbl_room_name = make_label(header, &lv_font_montserrat_40, CLR_TEXT_DARK);
    lv_label_set_text(lbl_room_name, "Loading...");
    lv_obj_align(lbl_room_name, LV_ALIGN_LEFT_MID, 4, 2);

    lv_obj_t *logo = lv_image_create(header);
    lv_image_set_src(logo, &img_district_logo);
    lv_obj_align(logo, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_flag(logo, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(logo, 16);
    lv_obj_add_event_cb(logo, logo_cb, LV_EVENT_CLICKED, NULL);

    /* ---- Left: current status ---- */
    panel_current = make_panel(g_screen, CLR_PANEL_DARK);
    lv_obj_set_size(panel_current, 512, PANEL_H);
    lv_obj_align(panel_current, LV_ALIGN_TOP_LEFT, 0, HEADER_H);
    lv_obj_set_flex_flow(panel_current, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(panel_current, 10, 0);

    lbl_cur_hdr = make_label(panel_current, &lv_font_montserrat_24, CLR_TEXT_SOFT);
    lbl_cur_title = make_label(panel_current, &lv_font_montserrat_32, CLR_WHITE);
    lv_label_set_long_mode(lbl_cur_title, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl_cur_title, 464);
    lbl_cur_time = make_label(panel_current, &lv_font_montserrat_20, CLR_TEXT_SOFT);

    /* Floating "+" booking button (web-app style), bottom-right of the
     * current panel; ignored by the flex layout. */
    btn_book = lv_btn_create(panel_current);
    lv_obj_set_size(btn_book, 72, 72);
    lv_obj_set_style_radius(btn_book, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn_book, CLR_WHITE, 0);
    lv_obj_set_style_border_color(btn_book, CLR_ORANGE, 0);
    lv_obj_set_style_border_width(btn_book, 3, 0);
    lv_obj_set_style_shadow_width(btn_book, 0, 0);
    lv_obj_add_flag(btn_book, LV_OBJ_FLAG_FLOATING);
    lv_obj_align(btn_book, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_add_event_cb(btn_book, book_open_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_plus = make_label(btn_book, &lv_font_montserrat_32, CLR_TEXT_DARK);
    lv_label_set_text(lbl_plus, LV_SYMBOL_PLUS);
    lv_obj_center(lbl_plus);

    /* ---- Right: next meeting ---- */
    panel_next = make_panel(g_screen, CLR_PANEL_MED);
    lv_obj_set_size(panel_next, 512, PANEL_H);
    lv_obj_align(panel_next, LV_ALIGN_TOP_LEFT, 512, HEADER_H);
    lv_obj_set_flex_flow(panel_next, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(panel_next, 10, 0);

    lv_obj_t *lbl_next_hdr = make_label(panel_next, &lv_font_montserrat_24, CLR_TEXT_SOFT);
    lv_label_set_text(lbl_next_hdr, "Next meeting");
    lbl_next_title = make_label(panel_next, &lv_font_montserrat_32, CLR_WHITE);
    lv_label_set_long_mode(lbl_next_title, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl_next_title, 464);
    lbl_next_date = make_label(panel_next, &lv_font_montserrat_20, CLR_TEXT_SOFT);
    lbl_next_time = make_label(panel_next, &lv_font_montserrat_24, CLR_WHITE);

    /* ---- Bottom strip: later meetings ---- */
    panel_bottom = make_panel(g_screen, CLR_LIGHT);
    lv_obj_set_size(panel_bottom, 1024, BOTTOM_H);
    lv_obj_align(panel_bottom, LV_ALIGN_TOP_LEFT, 0, HEADER_H + PANEL_H);

    for (int i = 0; i < BOTTOM_COLS; i++) {
        int x = i * 244;
        lbl_bot_title[i] = make_label(panel_bottom, &lv_font_montserrat_16, CLR_TEXT_DARK);
        lv_label_set_long_mode(lbl_bot_title[i], LV_LABEL_LONG_DOT);
        lv_obj_set_width(lbl_bot_title[i], 220);
        lv_obj_align(lbl_bot_title[i], LV_ALIGN_TOP_LEFT, x, 4);

        lbl_bot_date[i] = make_label(panel_bottom, &lv_font_montserrat_14, CLR_TEXT_MUTED);
        lv_obj_align(lbl_bot_date[i], LV_ALIGN_TOP_LEFT, x, 36);

        lbl_bot_time[i] = make_label(panel_bottom, &lv_font_montserrat_14, CLR_TEXT_MUTED);
        lv_obj_align(lbl_bot_time[i], LV_ALIGN_TOP_LEFT, x, 60);
    }

    lv_screen_load(g_screen);
}

/* ------------------------------------------------------------------ */
/* Data update                                                          */
/* ------------------------------------------------------------------ */

void room_screen_update(const ViewFMX_RoomData *data)
{
    lv_label_set_text(lbl_room_name, data->room_name[0] ? data->room_name : "Unknown Room");

    /* Left panel: current meeting, or availability + booking */
    if (data->has_current) {
        const ViewFMX_Meeting *m = &data->current;

        lv_label_set_text(lbl_cur_hdr, "Current meeting");
        lv_label_set_text(lbl_cur_title, meeting_title(m));

        char date[24], range[40], buf[72];
        fmt_date(date, sizeof(date), m->start_time);
        fmt_range(range, sizeof(range), m->start_time, m->end_time);
        snprintf(buf, sizeof(buf), "%s   %s", date, range);
        lv_label_set_text(lbl_cur_time, buf);

        lv_obj_add_flag(btn_book, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(lbl_cur_hdr, "");
        lv_label_set_text(lbl_cur_title, "Available");
        lv_label_set_text(lbl_cur_time, "");
        lv_obj_remove_flag(btn_book, LV_OBJ_FLAG_HIDDEN);
    }

    /* Right panel: first upcoming meeting */
    if (data->upcoming_count > 0) {
        const ViewFMX_Meeting *m = &data->upcoming[0];
        char date[24], range[40];
        fmt_date(date, sizeof(date), m->start_time);
        fmt_range(range, sizeof(range), m->start_time, m->end_time);

        lv_label_set_text(lbl_next_title, meeting_title(m));
        lv_label_set_text(lbl_next_date, date);
        lv_label_set_text(lbl_next_time, range);
    } else {
        lv_label_set_text(lbl_next_title, "Nothing scheduled");
        lv_label_set_text(lbl_next_date, "");
        lv_label_set_text(lbl_next_time, "");
    }

    /* Bottom strip: meetings after the next one */
    for (int i = 0; i < BOTTOM_COLS; i++) {
        int idx = i + 1;
        if (idx < data->upcoming_count) {
            const ViewFMX_Meeting *m = &data->upcoming[idx];
            char date[24], range[40];
            fmt_date(date, sizeof(date), m->start_time);
            fmt_range(range, sizeof(range), m->start_time, m->end_time);

            lv_label_set_text(lbl_bot_title[i], meeting_title(m));
            lv_label_set_text(lbl_bot_date[i], date);
            lv_label_set_text(lbl_bot_time[i], range);
        } else {
            lv_label_set_text(lbl_bot_title[i], "");
            lv_label_set_text(lbl_bot_date[i], "");
            lv_label_set_text(lbl_bot_time[i], "");
        }
    }
}
