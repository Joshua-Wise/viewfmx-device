#pragma once

/* Initialise the LVGL SDL display driver.
 * Must be called after lv_init() and before gui_init().   */
void display_init(int width, int height);
