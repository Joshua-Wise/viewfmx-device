#pragma once

/* Initialise the MIPI-DSI panel (JD9165) and the LVGL port layer.
 * Calls lvgl_port_init() internally, which also calls lv_init() —
 * do not call lv_init() separately on this target. */
void display_init(int width, int height);
