#include "room_screen.h"
#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef ESP_PLATFORM
/* newlib has no timegm(); see platform/esp32/compat.c */
time_t timegm(struct tm *tm);
#endif

/* ------------------------------------------------------------------ */
/* Widget handles                                                        */
/* ------------------------------------------------------------------ */

static lv_obj_t *g_screen;

/* Header */
static lv_obj_t *lbl_room_name;
static lv_obj_t *lbl_status_badge;

/* Current meeting panel */
static lv_obj_t *panel_current;
static lv_obj_t *lbl_cur_title;
static lv_obj_t *lbl_cur_time;
static lv_obj_t *lbl_cur_remaining;

/* Upcoming panel */
static lv_obj_t *panel_upcoming;
static lv_obj_t *lbl_upcoming[VIEWFMX_MAX_UPCOMING];
static lv_obj_t *lbl_upcoming_time[VIEWFMX_MAX_UPCOMING];

/* Buttons */
static lv_obj_t *btn_refresh;
static lv_obj_t *btn_book_30;
static lv_obj_t *btn_book_60;

/* Provider reference kept for button callbacks */
static ViewFMX_DataProvider *g_provider;
static char g_resource_id[32];
static char g_building_id[32];

/* ------------------------------------------------------------------ */
/* Colour palette (mirrors iPad grey theme)                             */
/* ------------------------------------------------------------------ */

#define CLR_BG          lv_color_hex(0x4B5563)   /* gray-600 */
#define CLR_PANEL_BUSY  lv_color_hex(0x374151)   /* gray-700 */
#define CLR_PANEL_FREE  lv_color_hex(0x6B7280)   /* gray-500 */
#define CLR_HEADER      lv_color_hex(0xD1D5DB)   /* gray-300 */
#define CLR_WHITE       lv_color_hex(0xFFFFFF)
#define CLR_GREEN       lv_color_hex(0x22C55E)
#define CLR_RED         lv_color_hex(0xEF4444)

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
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

/* "h:MM AM/PM" */
static void fmt_time(char *buf, size_t sz, const char *iso)
{
    struct tm local;
    if (!iso_to_local(iso, &local)) { snprintf(buf, sz, "--:--"); return; }

    strftime(buf, sz, "%I:%M %p", &local);

    /* Strip leading zero from hour (e.g. "09:30 AM" -> "9:30 AM") */
    if (buf[0] == '0') memmove(buf, buf + 1, sz - 1);
}

/* "Wed Jun 11, 3:30 PM" — for upcoming meetings, which may be days out. */
static void fmt_datetime(char *buf, size_t sz, const char *iso)
{
    struct tm local;
    if (!iso_to_local(iso, &local)) { snprintf(buf, sz, "--:--"); return; }

    char dow_mon[16];
    strftime(dow_mon, sizeof(dow_mon), "%a %b", &local);

    char t[16];
    strftime(t, sizeof(t), "%I:%M %p", &local);
    const char *tp = (t[0] == '0') ? t + 1 : t;

    snprintf(buf, sz, "%s %d, %s", dow_mon, local.tm_mday, tp);
}

/* ------------------------------------------------------------------ */
/* Button event callbacks                                               */
/* ------------------------------------------------------------------ */

static void refresh_cb(lv_event_t *e)
{
    (void)e;
    ViewFMX_RoomData data = {0};
    if (g_provider->fetch_status(g_provider->ctx, &data) == 0) {
        room_screen_update(&data);
    }
}

static void book_cb(lv_event_t *e)
{
    int minutes = (int)(intptr_t)lv_event_get_user_data(e);
    if (g_provider->book_room(g_provider->ctx, minutes) == 0) {
        /* Refresh to show new booking */
        ViewFMX_RoomData data = {0};
        if (g_provider->fetch_status(g_provider->ctx, &data) == 0) {
            room_screen_update(&data);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Layout creation                                                      */
/* ------------------------------------------------------------------ */

/*
 * Layout (1024 x 600):
 *
 *  ┌─────────────────────────── HEADER (h=80) ──────────────────────────┐
 *  │  [Room Name]                              [Refresh]                 │
 *  │  [● FREE / ● BUSY]                                                  │
 *  ├────────────────────────────────────────────────────────────────────┤
 *  │ CURRENT PANEL (w=420, full height below header)                     │
 *  │   "Current meeting" / "Available"                                   │
 *  │   Title                                                             │
 *  │   HH:MM – HH:MM          (X min remaining)                         │
 *  │                                                                     │
 *  │   [Book 30 min]  [Book 60 min]   (shown only when free)            │
 *  ├────────────────────────────────────────────────────────────────────┤
 *  │ UPCOMING PANEL (w=604, right side)                                  │
 *  │   "Upcoming"                                                        │
 *  │   Title1   HH:MM                                                    │
 *  │   Title2   HH:MM                                                    │
 *  │   …                                                                 │
 *  └────────────────────────────────────────────────────────────────────┘
 */

void room_screen_create(ViewFMX_DataProvider *provider,
                        const char *resource_id,
                        const char *building_id)
{
    g_provider = provider;
    strncpy(g_resource_id, resource_id, sizeof(g_resource_id) - 1);
    strncpy(g_building_id, building_id, sizeof(g_building_id) - 1);

    g_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_screen, CLR_BG, 0);

    /* ---- Header ---- */
    lv_obj_t *header = lv_obj_create(g_screen);
    lv_obj_set_size(header, 1024, 80);
    lv_obj_align(header, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(header, CLR_HEADER, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);

    lbl_room_name = lv_label_create(header);
    lv_obj_set_style_text_font(lbl_room_name, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(lbl_room_name, lv_color_hex(0x111827), 0);
    lv_label_set_text(lbl_room_name, "Loading...");
    lv_obj_align(lbl_room_name, LV_ALIGN_LEFT_MID, 12, -12);

    lbl_status_badge = lv_label_create(header);
    lv_obj_set_style_text_font(lbl_status_badge, &lv_font_montserrat_16, 0);
    lv_label_set_text(lbl_status_badge, "");
    lv_obj_align(lbl_status_badge, LV_ALIGN_LEFT_MID, 12, 22);

    btn_refresh = lv_btn_create(header);
    lv_obj_set_size(btn_refresh, 100, 40);
    lv_obj_align(btn_refresh, LV_ALIGN_RIGHT_MID, -12, 0);
    lv_obj_add_event_cb(btn_refresh, refresh_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_ref = lv_label_create(btn_refresh);
    lv_label_set_text(lbl_ref, "Refresh");
    lv_obj_center(lbl_ref);

    /* ---- Current meeting panel (left, 420 wide) ---- */
    panel_current = lv_obj_create(g_screen);
    lv_obj_set_size(panel_current, 420, 520);
    lv_obj_align(panel_current, LV_ALIGN_TOP_LEFT, 0, 80);
    lv_obj_set_style_bg_color(panel_current, CLR_PANEL_BUSY, 0);
    lv_obj_set_style_border_width(panel_current, 0, 0);
    lv_obj_set_style_radius(panel_current, 0, 0);
    lv_obj_set_style_pad_all(panel_current, 24, 0);
    lv_obj_set_flex_flow(panel_current, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *lbl_cur_hdr = lv_label_create(panel_current);
    lv_obj_set_style_text_font(lbl_cur_hdr, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_cur_hdr, lv_color_hex(0xD1D5DB), 0);
    lv_label_set_text(lbl_cur_hdr, "Current meeting");

    lbl_cur_title = lv_label_create(panel_current);
    lv_obj_set_style_text_font(lbl_cur_title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_cur_title, CLR_WHITE, 0);
    lv_label_set_long_mode(lbl_cur_title, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl_cur_title, 370);
    lv_label_set_text(lbl_cur_title, "");

    lbl_cur_time = lv_label_create(panel_current);
    lv_obj_set_style_text_font(lbl_cur_time, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_cur_time, lv_color_hex(0xD1D5DB), 0);
    lv_label_set_text(lbl_cur_time, "");

    lbl_cur_remaining = lv_label_create(panel_current);
    lv_obj_set_style_text_font(lbl_cur_remaining, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lbl_cur_remaining, CLR_WHITE, 0);
    lv_label_set_text(lbl_cur_remaining, "");

    /* Book Now buttons (shown when free) */
    btn_book_30 = lv_btn_create(panel_current);
    lv_obj_set_size(btn_book_30, 160, 48);
    lv_obj_set_style_bg_color(btn_book_30, lv_color_hex(0x3B82F6), 0);
    lv_obj_add_event_cb(btn_book_30, book_cb, LV_EVENT_CLICKED, (void *)(intptr_t)30);
    lv_obj_t *lbl30 = lv_label_create(btn_book_30);
    lv_label_set_text(lbl30, "Book 30 min");
    lv_obj_center(lbl30);

    btn_book_60 = lv_btn_create(panel_current);
    lv_obj_set_size(btn_book_60, 160, 48);
    lv_obj_set_style_bg_color(btn_book_60, lv_color_hex(0x3B82F6), 0);
    lv_obj_add_event_cb(btn_book_60, book_cb, LV_EVENT_CLICKED, (void *)(intptr_t)60);
    lv_obj_t *lbl60 = lv_label_create(btn_book_60);
    lv_label_set_text(lbl60, "Book 60 min");
    lv_obj_center(lbl60);

    /* ---- Upcoming panel (right, 604 wide) ---- */
    panel_upcoming = lv_obj_create(g_screen);
    lv_obj_set_size(panel_upcoming, 604, 520);
    lv_obj_align(panel_upcoming, LV_ALIGN_TOP_RIGHT, 0, 80);
    lv_obj_set_style_bg_color(panel_upcoming, CLR_PANEL_FREE, 0);
    lv_obj_set_style_border_width(panel_upcoming, 0, 0);
    lv_obj_set_style_radius(panel_upcoming, 0, 0);
    lv_obj_set_style_pad_all(panel_upcoming, 24, 0);
    lv_obj_set_flex_flow(panel_upcoming, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *lbl_up_hdr = lv_label_create(panel_upcoming);
    lv_obj_set_style_text_font(lbl_up_hdr, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_up_hdr, lv_color_hex(0xE5E7EB), 0);
    lv_label_set_text(lbl_up_hdr, "Upcoming");

    for (int i = 0; i < VIEWFMX_MAX_UPCOMING; i++) {
        lbl_upcoming[i] = lv_label_create(panel_upcoming);
        lv_obj_set_style_text_font(lbl_upcoming[i], &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(lbl_upcoming[i], CLR_WHITE, 0);
        lv_label_set_long_mode(lbl_upcoming[i], LV_LABEL_LONG_DOT);
        lv_obj_set_width(lbl_upcoming[i], 400);
        lv_label_set_text(lbl_upcoming[i], "");

        lbl_upcoming_time[i] = lv_label_create(panel_upcoming);
        lv_obj_set_style_text_font(lbl_upcoming_time[i], &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(lbl_upcoming_time[i], lv_color_hex(0xD1D5DB), 0);
        lv_label_set_text(lbl_upcoming_time[i], "");
    }

    lv_screen_load(g_screen);
}

/* ------------------------------------------------------------------ */
/* Data update                                                          */
/* ------------------------------------------------------------------ */

void room_screen_update(const ViewFMX_RoomData *data)
{
    /* Room name */
    lv_label_set_text(lbl_room_name, data->room_name[0] ? data->room_name : "Unknown Room");

    /* Status badge: only shown when free — when busy, the current
     * meeting panel already says it. */
    if (data->is_busy) {
        lv_label_set_text(lbl_status_badge, "");
        lv_obj_set_style_bg_color(panel_current, CLR_PANEL_BUSY, 0);
    } else {
        lv_label_set_text(lbl_status_badge, "* AVAILABLE");
        lv_obj_set_style_text_color(lbl_status_badge, CLR_GREEN, 0);
        lv_obj_set_style_bg_color(panel_current, CLR_PANEL_FREE, 0);
    }

    /* Current meeting */
    if (data->has_current) {
        const ViewFMX_Meeting *m = &data->current;
        lv_label_set_text(lbl_cur_title, m->title[0] ? m->title : "Reserved");

        char ts[16], te[16];
        fmt_time(ts, sizeof(ts), m->start_time);
        fmt_time(te, sizeof(te), m->end_time);
        char timebuf[40];
        snprintf(timebuf, sizeof(timebuf), "%s - %s", ts, te);
        lv_label_set_text(lbl_cur_time, timebuf);

        char rembuf[32];
        snprintf(rembuf, sizeof(rembuf), "%d min left", m->minutes_remaining);
        lv_label_set_text(lbl_cur_remaining, rembuf);

        lv_obj_add_flag(btn_book_30, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(btn_book_60, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(lbl_cur_title, "Available");
        lv_label_set_text(lbl_cur_time, "");
        lv_label_set_text(lbl_cur_remaining, "");
        lv_obj_remove_flag(btn_book_30, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(btn_book_60, LV_OBJ_FLAG_HIDDEN);
    }

    /* Upcoming */
    for (int i = 0; i < VIEWFMX_MAX_UPCOMING; i++) {
        if (i < data->upcoming_count) {
            const ViewFMX_Meeting *m = &data->upcoming[i];
            lv_label_set_text(lbl_upcoming[i], m->title[0] ? m->title : "Reserved");

            char ts[40];
            fmt_datetime(ts, sizeof(ts), m->start_time);
            lv_label_set_text(lbl_upcoming_time[i], ts);
        } else {
            lv_label_set_text(lbl_upcoming[i], "");
            lv_label_set_text(lbl_upcoming_time[i], "");
        }
    }
}
