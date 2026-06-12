#include "fetcher.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "../../gui/screens/room_screen.h"

#define FETCH_INTERVAL_MS  60000
#define HEAP_LOG_EVERY     30           /* fetch cycles (~30 min) */

static const char *TAG = "fetcher";

static ViewFMX_DataProvider *g_provider;

/* Runs in the LVGL task. First call subscribes that task to the task
 * watchdog; afterwards it feeds it. If the LVGL task ever stalls (i2c
 * deadlock, runaway callback, ...) the WDT panics and the sign reboots
 * itself instead of freezing. */
static void lvgl_wdt_timer_cb(lv_timer_t *t)
{
    (void)t;
    static bool subscribed;
    if (!subscribed) {
        subscribed = esp_task_wdt_add(NULL) == ESP_OK;
        return;
    }
    esp_task_wdt_reset();
}

static void fetcher_task(void *arg)
{
    (void)arg;
    int cycles = 0;
    bool offline = false;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(FETCH_INTERVAL_MS));

        ViewFMX_RoomData data = {0};
        bool ok = g_provider->fetch_status(g_provider->ctx, &data) == 0;

        if (ok != !offline) {
            ESP_LOGW(TAG, "server %s", ok ? "reachable again" : "unreachable");
        }
        offline = !ok;

        if (lvgl_port_lock(2000)) {
            if (ok) {
                room_screen_update(&data);
            }
            room_screen_set_offline(offline);
            lvgl_port_unlock();
        } else {
            ESP_LOGE(TAG, "LVGL lock timeout — UI task may be stuck");
        }

        if (++cycles % HEAP_LOG_EVERY == 0) {
            ESP_LOGI(TAG, "uptime %llu min, free heap %u KB (min %u KB)",
                     (unsigned long long)(esp_log_timestamp() / 60000),
                     heap_caps_get_free_size(MALLOC_CAP_DEFAULT) / 1024,
                     heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT) / 1024);
        }
    }
}

void fetcher_init(ViewFMX_DataProvider *provider)
{
    g_provider = provider;

    if (lvgl_port_lock(0)) {
        lv_timer_create(lvgl_wdt_timer_cb, 2000, NULL);
        lvgl_port_unlock();
    }

    /* Stack sized for esp_http_client + TLS. */
    xTaskCreate(fetcher_task, "fetcher", 10240, NULL, 4, NULL);
}
