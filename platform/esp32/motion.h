#pragma once

/* Camera-based presence dimming: captures low-rate frames from the bezel
 * camera (MIPI-CSI via esp_video), detects motion by frame differencing,
 * and fades the backlight down after a period with no motion or touch.
 * Call after display_init(), input_init(), and gui_init(). Non-fatal:
 * if no camera is found the display simply stays at full brightness. */
void motion_init(void);
