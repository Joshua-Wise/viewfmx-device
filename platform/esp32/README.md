# ESP32-P4 Port Layer

Port layer for the Guition JC1060P470C (ESP32-P4, 7″ 1024×600 MIPI-DSI,
capacitive touch). The ESP-IDF project that builds this lives in
`/esp32` at the repo root; this directory holds only the platform
driver sources, mirroring `platform/sdl/`.

## Hardware map (JC1060P470C)

| Function        | Detail                                        |
|-----------------|-----------------------------------------------|
| Display         | 7″ 1024×600 IPS, JD9165 controller, 2-lane MIPI-DSI |
| LCD reset       | GPIO5                                         |
| Backlight       | GPIO23 (driven as plain GPIO; PWM optional)   |
| MIPI D-PHY power| Internal LDO channel 3 @ 2.5 V                |
| Touch           | GT911, I2C addr 0x5D                          |
| Touch I2C       | SDA GPIO7, SCL GPIO8 (400 kHz)                |
| Touch INT / RST | GPIO21 / GPIO22                               |
| PSRAM / Flash   | 32 MB hex PSRAM @ 200 MHz / 16 MB flash       |
| Ethernet        | 100M RJ45, IP101 PHY addr 1, RST GPIO51, REF_CLK in GPIO50, MDC/MDIO GPIO31/52 (= P4 EMAC defaults) |
| Wi-Fi (unused)  | Wi-Fi 6 via onboard ESP32-C6 over SDIO (`esp_wifi_remote`) |

## Files

- `display.c` — JD9165 DSI panel via `espressif/esp_lcd_jd9165`, wired
  into LVGL with `espressif/esp_lvgl_port` (`lvgl_port_init()` here also
  calls `lv_init()`).
- `input.c` — GT911 via `espressif/esp_lcd_touch_gt911`, registered with
  `lvgl_port_add_touch()`.
- `net.c` — wired Ethernet (internal EMAC + IP101) with DHCP; blocks up
  to 15 s for an IP at boot, then the 60 s refresh timer retries forever.
  The matching HTTP transport is `data/http_provider_esp.c`
  (`esp_http_client`); JSON parsing is shared with the SDL/curl build via
  `data/http_provider_parse.c`.

## No GoFMX credentials here

Room identity (`resource_id`, `building_id`) and the viewfmx server URL
are set via Kconfig (`idf.py menuconfig` → "viewFMX Device") — never
hardcoded. The GoFMX token lives exclusively in the viewfmx Docker
container.
