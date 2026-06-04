#include "input.h"
#include <lvgl.h>

void input_init(void)
{
    lv_indev_t *mouse = lv_sdl_mouse_create();
    (void)mouse;
}
