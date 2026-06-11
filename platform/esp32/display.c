/* Guition JC1060P470C display: 7" 1024x600 IPS, JD9165 controller,
 * 2-lane MIPI-DSI. Backlight on GPIO23, LCD reset on GPIO5. */

#include "display.h"

#include "driver/gpio.h"
#include "esp_lcd_jd9165.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_ldo_regulator.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"

#define PIN_BACKLIGHT     23

/* The MIPI D-PHY is powered by internal LDO channel 3 at 2.5 V on ESP32-P4. */
#define MIPI_PHY_LDO_CHAN 3
#define MIPI_PHY_LDO_MV   2500

static const char *TAG = "display";

static lv_display_t *g_disp;

void display_init(int width, int height)
{
    esp_ldo_channel_handle_t ldo_mipi_phy = NULL;
    esp_ldo_channel_config_t ldo_cfg = {
        .chan_id    = MIPI_PHY_LDO_CHAN,
        .voltage_mv = MIPI_PHY_LDO_MV,
    };
    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_cfg, &ldo_mipi_phy));

    /* Guition's official BSP runs the lanes at 550 Mbps; the component
     * default of 750 Mbps makes this panel flicker/lose sync. */
    esp_lcd_dsi_bus_handle_t dsi_bus = NULL;
    esp_lcd_dsi_bus_config_t bus_cfg = {
        .bus_id            = 0,
        .num_data_lanes    = 2,
        .phy_clk_src       = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bit_rate_mbps = 550,
    };
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_cfg, &dsi_bus));

    esp_lcd_panel_io_handle_t dbi_io = NULL;
    esp_lcd_dbi_io_config_t dbi_cfg = JD9165_PANEL_IO_DBI_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(dsi_bus, &dbi_cfg, &dbi_io));

    esp_lcd_dpi_panel_config_t dpi_cfg =
        JD9165_1024_600_PANEL_60HZ_DPI_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB565);
    jd9165_vendor_config_t vendor_cfg = {
        .mipi_config = {
            .dsi_bus    = dsi_bus,
            .dpi_config = &dpi_cfg,
        },
    };
    /* No LCD reset line on this board (Guition BSP uses GPIO_NUM_NC). */
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = -1,
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config  = &vendor_cfg,
    };
    esp_lcd_panel_handle_t panel = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_jd9165(dbi_io, &panel_cfg, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));

    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    /* The refresh timer and button callbacks do HTTP fetches from the
     * LVGL task; the default stack is too small for esp_http_client. */
    port_cfg.task_stack = 12288;
    ESP_ERROR_CHECK(lvgl_port_init(&port_cfg));

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle     = dbi_io,
        .panel_handle  = panel,
        .buffer_size   = (uint32_t)width * 50,
        .double_buffer = false,
        .hres          = width,
        .vres          = height,
        .color_format  = LV_COLOR_FORMAT_RGB565,
        .flags = {
            /* Internal-RAM DMA buffer per Guition BSP; a PSRAM draw
             * buffer contends with the DPI framebuffer and flickers. */
            .buff_dma    = true,
            .buff_spiram = false,
        },
    };
    const lvgl_port_display_dsi_cfg_t dsi_disp_cfg = {
        .flags = { .avoid_tearing = false },
    };
    g_disp = lvgl_port_add_disp_dsi(&disp_cfg, &dsi_disp_cfg);

    gpio_config_t bk_cfg = {
        .pin_bit_mask = 1ULL << PIN_BACKLIGHT,
        .mode         = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&bk_cfg));
    gpio_set_level(PIN_BACKLIGHT, 1);

    ESP_LOGI(TAG, "display ready (%dx%d)", width, height);
}
