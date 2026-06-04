/**
 * LVGL v9 configuration for viewfmx-device.
 * Target panel: Guition JC1060P470C — 1024x600, 16-bit color.
 * Simulator target: macOS SDL.
 *
 * Strategy: enable everything by default; only override what we need.
 * Selectively disabling widgets causes transitive #error from lvgl.h.
 */

#if 1  /* enable this file */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR — 16-bit to match the panel
 *====================*/
#define LV_COLOR_DEPTH 16

/*====================
   MEMORY
 *====================*/
#define LV_MEM_SIZE (512 * 1024U)

/*====================
   HAL
 *====================*/
#define LV_TICK_CUSTOM 0
#define LV_DEF_REFR_PERIOD 16   /* ~60 fps */
#define LV_DPI_DEF 130

/*====================
   DRAW
 *====================*/
#define LV_DRAW_BUF_ALIGN 64

/*====================
   FONTS
 *====================*/
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_DEFAULT &lv_font_montserrat_16

/*====================
   SDL DRIVER (simulator)
 *====================*/
#define LV_USE_SDL 1
/* Homebrew SDL2 on Apple Silicon puts headers in include/SDL2/, so the
 * include dir is .../include/SDL2 and the header name is just <SDL.h>.   */
#define LV_SDL_INCLUDE_PATH <SDL.h>

/*====================
   LOGGING
 *====================*/
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

/*====================
   ASSERT
 *====================*/
#define LV_USE_ASSERT_NULL   1
#define LV_USE_ASSERT_MALLOC 1

/* All other settings take their defaults from lv_conf_internal.h,
 * which enables all widgets and standard features.               */

#endif /* LV_CONF_H */
#endif /* enable file */
