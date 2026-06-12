#include "gui.h"
#include "screens/room_screen.h"
#include <lvgl.h>
#include <string.h>

static ViewFMX_DataProvider *g_provider;
static char g_resource_id[32];
static char g_building_id[32];

static lv_timer_t *g_refresh_timer;

static void refresh_timer_cb(lv_timer_t *t)
{
    (void)t;
    ViewFMX_RoomData data = {0};
    if (g_provider->fetch_status(g_provider->ctx, &data) == 0) {
        room_screen_update(&data);
        room_screen_set_offline(false);
    } else {
        room_screen_set_offline(true);
    }
}

void gui_init(ViewFMX_DataProvider *provider,
              const char *resource_id,
              const char *building_id)
{
    g_provider = provider;
    strncpy(g_resource_id, resource_id, sizeof(g_resource_id) - 1);
    strncpy(g_building_id, building_id, sizeof(g_building_id) - 1);

    room_screen_create(provider, resource_id, building_id);

    /* Initial data load */
    ViewFMX_RoomData data = {0};
    if (provider->fetch_status(provider->ctx, &data) == 0) {
        room_screen_update(&data);
    } else {
        room_screen_set_offline(true);
    }

#ifndef ESP_PLATFORM
    /* Refresh every 60 seconds. On the ESP32 the platform fetcher task
     * does this instead — network I/O must never run in the LVGL task,
     * or a hung connection freezes the whole UI. */
    g_refresh_timer = lv_timer_create(refresh_timer_cb, 60000, NULL);
#endif
}

void gui_tick(void)
{
    /* nothing extra needed; LVGL timer system drives refresh_timer_cb */
}
