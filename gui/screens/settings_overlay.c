#include "settings_overlay.h"
#include "room_screen.h"
#include "../../platform/sysinfo.h"

#include <lvgl.h>
#include <stdio.h>
#include <string.h>

static lv_obj_t *g_backdrop;
static lv_obj_t *g_ta_resource;
static lv_obj_t *g_ta_building;
static lv_obj_t *g_keyboard;

static ViewFMX_DataProvider *g_provider;

#define CLR_LIGHT      lv_color_hex(0xE5E7EB)
#define CLR_TEXT_DARK  lv_color_hex(0x111827)
#define CLR_TEXT_MUTED lv_color_hex(0x4B5563)
#define CLR_BLUE       lv_color_hex(0x3B82F6)
#define CLR_GREEN      lv_color_hex(0x22C55E)

static void close_cb(lv_event_t *e)
{
    (void)e;
    lv_obj_delete(g_backdrop);
    g_backdrop = NULL;
}

static void refresh_cb(lv_event_t *e)
{
    ViewFMX_RoomData data = {0};
    if (g_provider->fetch_status(g_provider->ctx, &data) == 0) {
        room_screen_update(&data);
    }
    close_cb(e);
}

static void save_cb(lv_event_t *e)
{
    (void)e;
    roomcfg_save_and_restart(lv_textarea_get_text(g_ta_resource),
                             lv_textarea_get_text(g_ta_building));
    /* Reached only on the simulator, where saving is a no-op. */
    close_cb(e);
}

static void ta_focus_cb(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_FOCUSED) {
        lv_keyboard_set_textarea(g_keyboard, ta);
        lv_obj_remove_flag(g_keyboard, LV_OBJ_FLAG_HIDDEN);
    } else if (code == LV_EVENT_DEFOCUSED || code == LV_EVENT_READY ||
               code == LV_EVENT_CANCEL) {
        lv_obj_add_flag(g_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

static lv_obj_t *make_text(lv_obj_t *parent, const lv_font_t *font,
                           lv_color_t color, const char *txt)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_label_set_text(l, txt);
    return l;
}

static lv_obj_t *make_id_input(lv_obj_t *parent, const char *value)
{
    lv_obj_t *ta = lv_textarea_create(parent);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_accepted_chars(ta, "0123456789");
    lv_textarea_set_max_length(ta, 12);
    lv_textarea_set_text(ta, value);
    lv_obj_set_size(ta, 180, 48);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_20, 0);
    lv_obj_add_event_cb(ta, ta_focus_cb, LV_EVENT_ALL, NULL);
    return ta;
}

static lv_obj_t *make_button(lv_obj_t *parent, const char *txt,
                             lv_color_t color, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 160, 52);
    lv_obj_set_style_bg_color(btn, color, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l = make_text(btn, &lv_font_montserrat_20, lv_color_white(), txt);
    lv_obj_center(l);
    return btn;
}

void settings_overlay_open(ViewFMX_DataProvider *provider,
                           const char *resource_id,
                           const char *building_id)
{
    if (g_backdrop) return;   /* already open */
    g_provider = provider;

    g_backdrop = lv_obj_create(lv_layer_top());
    lv_obj_set_size(g_backdrop, 1024, 600);
    lv_obj_set_style_bg_color(g_backdrop, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(g_backdrop, LV_OPA_50, 0);
    lv_obj_set_style_border_width(g_backdrop, 0, 0);
    lv_obj_set_style_radius(g_backdrop, 0, 0);
    lv_obj_clear_flag(g_backdrop, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *panel = lv_obj_create(g_backdrop);
    lv_obj_set_size(panel, 640, 460);
    lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 24);
    lv_obj_set_style_bg_color(panel, CLR_LIGHT, 0);
    lv_obj_set_style_radius(panel, 16, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 24, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    make_text(panel, &lv_font_montserrat_24, CLR_TEXT_DARK, "Settings");

    /* Room config */
    lv_obj_t *lbl_room = make_text(panel, &lv_font_montserrat_16, CLR_TEXT_MUTED,
                                   "Room  (resource ID / building ID)");
    lv_obj_align(lbl_room, LV_ALIGN_TOP_LEFT, 0, 48);

    g_ta_resource = make_id_input(panel, resource_id);
    lv_obj_align(g_ta_resource, LV_ALIGN_TOP_LEFT, 0, 76);
    g_ta_building = make_id_input(panel, building_id);
    lv_obj_align(g_ta_building, LV_ALIGN_TOP_LEFT, 200, 76);

    lv_obj_t *btn_save = make_button(panel, "Save + Restart", CLR_GREEN, save_cb);
    lv_obj_align(btn_save, LV_ALIGN_TOP_LEFT, 400, 74);

    /* Network details */
    char ip[20], mac[20], buf[96];
    sysinfo_get_network(ip, sizeof(ip), mac, sizeof(mac));
    snprintf(buf, sizeof(buf), "IP  %s        MAC  %s", ip, mac);

    lv_obj_t *lbl_net_hdr = make_text(panel, &lv_font_montserrat_16, CLR_TEXT_MUTED,
                                      "Network");
    lv_obj_align(lbl_net_hdr, LV_ALIGN_TOP_LEFT, 0, 152);
    lv_obj_t *lbl_net = make_text(panel, &lv_font_montserrat_20, CLR_TEXT_DARK, buf);
    lv_obj_align(lbl_net, LV_ALIGN_TOP_LEFT, 0, 180);

    /* Actions */
    lv_obj_t *btn_refresh = make_button(panel, LV_SYMBOL_REFRESH "  Refresh",
                                        CLR_BLUE, refresh_cb);
    lv_obj_align(btn_refresh, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_obj_t *btn_close = make_button(panel, "Close",
                                      lv_color_hex(0x6B7280), close_cb);
    lv_obj_align(btn_close, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    /* Numeric keyboard, shown while an ID field is focused */
    g_keyboard = lv_keyboard_create(g_backdrop);
    lv_keyboard_set_mode(g_keyboard, LV_KEYBOARD_MODE_NUMBER);
    lv_obj_set_size(g_keyboard, 640, 180);
    lv_obj_align(g_keyboard, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_add_flag(g_keyboard, LV_OBJ_FLAG_HIDDEN);
}
