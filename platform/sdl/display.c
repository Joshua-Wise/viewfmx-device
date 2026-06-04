#include "display.h"
#include <lvgl.h>

void display_init(int width, int height)
{
    /* LVGL v9 SDL driver: creates an SDL window sized to width x height.
     * lv_sdl_window_create() registers the display with LVGL automatically. */
    lv_display_t *disp = lv_sdl_window_create(width, height);
    (void)disp;
}
