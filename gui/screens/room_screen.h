#pragma once
#include "../../data/data_provider.h"

/* Create and load the room display screen.
 * Must be called once after lv_init() and display init.           */
void room_screen_create(ViewFMX_DataProvider *provider,
                        const char *resource_id,
                        const char *building_id);

/* Push fresh data into the widgets. Safe to call from the LVGL task. */
void room_screen_update(const ViewFMX_RoomData *data);

/* Shows/hides the amber OFFLINE pill in the header. The last known
 * schedule stays on screen while offline. */
void room_screen_set_offline(bool offline);
