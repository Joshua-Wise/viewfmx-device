/* viewfmx-device — ESP32-P4 entry point (Guition JC1060P470C).
 *
 * Wired Ethernet (IP101) + esp_http_client provider, or MockProvider
 * when CONFIG_VIEWFMX_USE_MOCK is set. */

#include "esp_lvgl_port.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "gui/gui.h"
#include "data/data_provider.h"
#include "data/http_provider.h"
#include "data/mock_provider.h"
#include "platform/esp32/display.h"
#include "platform/esp32/input.h"
#include "platform/esp32/net.h"
#include "platform/esp32/motion.h"
#include "platform/sysinfo.h"

#define DISPLAY_WIDTH  1024
#define DISPLAY_HEIGHT 600

/* gui_init() keeps the pointer, so the provider must outlive app_main. */
static ViewFMX_DataProvider g_provider;

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    /* Meeting times arrive as UTC; render them in local time. */
    setenv("TZ", CONFIG_VIEWFMX_TZ, 1);
    tzset();

    /* Bring the panel up first so the user sees something while
     * net_init() blocks on DHCP. */
    display_init(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    input_init();

    const char *resource_id = CONFIG_VIEWFMX_RESOURCE_ID;
    const char *building_id = CONFIG_VIEWFMX_BUILDING_ID;

#if CONFIG_VIEWFMX_USE_MOCK
    net_init();
    mock_provider_init(&g_provider);
#else
    net_init();
    static HttpProviderConfig http_cfg;
    strncpy(http_cfg.base_url,    CONFIG_VIEWFMX_BASE_URL,    sizeof(http_cfg.base_url) - 1);
    strncpy(http_cfg.resource_id, CONFIG_VIEWFMX_RESOURCE_ID, sizeof(http_cfg.resource_id) - 1);
    strncpy(http_cfg.building_id, CONFIG_VIEWFMX_BUILDING_ID, sizeof(http_cfg.building_id) - 1);
    /* IDs set from the on-device settings overlay override Kconfig. */
    roomcfg_load(http_cfg.resource_id, sizeof(http_cfg.resource_id),
                 http_cfg.building_id, sizeof(http_cfg.building_id));
    http_provider_init(&g_provider, &http_cfg);
    resource_id = http_cfg.resource_id;
    building_id = http_cfg.building_id;
#endif

    /* The LVGL port runs its own task; take the lock around all LVGL calls. */
    lvgl_port_lock(0);
    gui_init(&g_provider, resource_id, building_id);
    lvgl_port_unlock();

#if CONFIG_VIEWFMX_MOTION_DIM
    motion_init();
#endif
}
