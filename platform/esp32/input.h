#pragma once

/* Initialise the GT911 capacitive touch controller and register it
 * with LVGL. Call after display_init(). */
void input_init(void);

/* The shared I2C0 bus (touch + camera SCCB + RTC live on it).
 * NULL until input_init() has run. */
typedef struct i2c_master_bus_t *i2c_master_bus_handle_t;
i2c_master_bus_handle_t input_get_i2c_bus(void);
