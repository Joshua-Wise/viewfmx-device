/**
 * viewfmx-device — macOS SDL simulator entry point.
 *
 * Build/run: see README.md.
 *
 * To use MockProvider (offline, no network):
 *   cmake -DUSE_MOCK=ON ..
 *
 * To use HttpProvider (live viewfmx server):
 *   cmake -DUSE_MOCK=OFF -DVIEWFMX_BASE_URL="http://192.168.1.x:3000" \
 *         -DVIEWFMX_RESOURCE_ID="456" -DVIEWFMX_BUILDING_ID="123" ..
 */

#include <lvgl.h>
#include <SDL.h>
#include <stdio.h>
#include <stdbool.h>

#include "gui/gui.h"
#include "data/data_provider.h"
#include "data/mock_provider.h"
#include "data/http_provider.h"
#include "platform/sdl/display.h"
#include "platform/sdl/input.h"
#include "platform/sdl/net.h"

#define DISPLAY_WIDTH  1024
#define DISPLAY_HEIGHT 600

/* These are substituted by CMake; see CMakeLists.txt */
#ifndef VIEWFMX_USE_MOCK
#  define VIEWFMX_USE_MOCK 1
#endif
#ifndef VIEWFMX_BASE_URL
#  define VIEWFMX_BASE_URL "http://localhost:3000"
#endif
#ifndef VIEWFMX_RESOURCE_ID
#  define VIEWFMX_RESOURCE_ID "0"
#endif
#ifndef VIEWFMX_BUILDING_ID
#  define VIEWFMX_BUILDING_ID "0"
#endif

int main(void)
{
    /* Platform init */
    net_init();
    lv_init();
    display_init(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    input_init();

    /* Choose provider */
    ViewFMX_DataProvider provider = {0};
    HttpProviderConfig   http_cfg = {0};

#if VIEWFMX_USE_MOCK
    printf("[viewfmx-device] Using MockProvider (offline)\n");
    mock_provider_init(&provider);
#else
    printf("[viewfmx-device] Using HttpProvider → %s\n", VIEWFMX_BASE_URL);
    strncpy(http_cfg.base_url,    VIEWFMX_BASE_URL,     sizeof(http_cfg.base_url) - 1);
    strncpy(http_cfg.resource_id, VIEWFMX_RESOURCE_ID,  sizeof(http_cfg.resource_id) - 1);
    strncpy(http_cfg.building_id, VIEWFMX_BUILDING_ID,  sizeof(http_cfg.building_id) - 1);
    http_provider_init(&provider, &http_cfg);
#endif

    gui_init(&provider, VIEWFMX_RESOURCE_ID, VIEWFMX_BUILDING_ID);

    /* Main loop */
    bool running = true;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = false;
        }

        lv_timer_handler();
        SDL_Delay(5);
    }

#if !VIEWFMX_USE_MOCK
    http_provider_destroy(&provider);
#endif
    net_cleanup();
    return 0;
}
