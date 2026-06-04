#pragma once
#include "../data/data_provider.h"

/* Call once after LVGL and the display driver are initialised. */
void gui_init(ViewFMX_DataProvider *provider,
              const char *resource_id,
              const char *building_id);

/* Call periodically from the main loop (same thread as LVGL). */
void gui_tick(void);
