/* Guition JC1060P470C touch: GT911 on I2C0 (SDA GPIO7, SCL GPIO8),
 * INT GPIO21, RST GPIO22. GT911 answers on 0x5D or 0x14 depending on
 * the INT level latched during reset, so try both.
 *
 * Touch failure is non-fatal: the display still works without it. */

#include "input.h"

#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"

#define PIN_TOUCH_SDA 7
#define PIN_TOUCH_SCL 8
#define PIN_TOUCH_INT 21
#define PIN_TOUCH_RST 22

#define TOUCH_H_RES 1024
#define TOUCH_V_RES 600

static const char *TAG = "input";

static void scan_bus(i2c_master_bus_handle_t bus)
{
    int found = 0;
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        if (i2c_master_probe(bus, addr, 50) == ESP_OK) {
            ESP_LOGI(TAG, "i2c scan: device at 0x%02x", addr);
            found++;
        }
    }
    if (!found) {
        ESP_LOGW(TAG, "i2c scan: no devices found");
    }
}

static esp_lcd_touch_handle_t try_gt911(i2c_master_bus_handle_t bus,
                                        uint8_t dev_addr)
{
    esp_lcd_panel_io_i2c_config_t io_cfg = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    io_cfg.dev_addr = dev_addr;
    io_cfg.scl_speed_hz = 400000;
    esp_lcd_panel_io_handle_t tp_io = NULL;
    if (esp_lcd_new_panel_io_i2c(bus, &io_cfg, &tp_io) != ESP_OK) {
        return NULL;
    }

    /* The chip is already out of reset and answering on the bus; letting
     * the driver toggle RST/INT (GPIO22/21) kills subsequent reads, so
     * leave both unmanaged and poll over I2C only. */
    esp_lcd_touch_config_t tp_cfg = {
        .x_max        = TOUCH_H_RES,
        .y_max        = TOUCH_V_RES,
        .rst_gpio_num = -1,
        .int_gpio_num = -1,
    };
    esp_lcd_touch_handle_t tp = NULL;
    if (esp_lcd_touch_new_i2c_gt911(tp_io, &tp_cfg, &tp) != ESP_OK) {
        esp_lcd_panel_io_del(tp_io);
        return NULL;
    }
    return tp;
}

void input_init(void)
{
    i2c_master_bus_handle_t i2c_bus = NULL;
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port          = 0,
        .sda_io_num        = PIN_TOUCH_SDA,
        .scl_io_num        = PIN_TOUCH_SCL,
        .clk_source        = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    if (i2c_new_master_bus(&bus_cfg, &i2c_bus) != ESP_OK) {
        ESP_LOGE(TAG, "i2c bus init failed; touch disabled");
        return;
    }

    scan_bus(i2c_bus);

    esp_lcd_touch_handle_t tp = try_gt911(i2c_bus, ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS);
    if (!tp) {
        ESP_LOGW(TAG, "GT911 not at 0x%02x, trying backup 0x%02x",
                 ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS,
                 ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP);
        tp = try_gt911(i2c_bus, ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP);
    }
    if (!tp) {
        ESP_LOGE(TAG, "GT911 init failed; touch disabled");
        return;
    }

    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp   = lv_display_get_default(),
        .handle = tp,
    };
    lvgl_port_add_touch(&touch_cfg);

    ESP_LOGI(TAG, "GT911 touch ready");
}
