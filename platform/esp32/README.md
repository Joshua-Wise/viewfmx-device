# ESP32-P4 Port Layer — STUB (NOT YET IMPLEMENTED)

This directory is intentionally empty. It is the port layer for the
Guition JC1060P470C (ESP32-P4, 1024×600, capacitive touch, wired Ethernet).

When the hardware arrives, implement the following, keeping the same
function signatures as the SDL equivalents in `platform/sdl/`:

## Files to create

### `display.c` / `display.h`
Register an `lv_display_t` backed by the ESP32-P4 RGB panel driver
(`esp_lcd_rgb_panel`). Match the SDL signature:
```c
void display_init(int width, int height);
```

### `input.c` / `input.h`
Register an `lv_indev_t` for the capacitive touch controller (GT911
or similar, via I2C). Match:
```c
void input_init(void);
```

### `net.c` / `net.h`
Initialise the W5500/built-in Ethernet MAC and lwIP stack, or the
ESP-IDF `esp_http_client` wrapper used by `http_provider.c`. Match:
```c
void net_init(void);
void net_cleanup(void);
```

Note: `http_provider.c` currently uses **libcurl**. On ESP-IDF you
will either port it to `esp_http_client`, or pull in a libcurl ESP-IDF
component. The `data/` and `gui/` layers are not aware of which HTTP
library is used — only `platform/` files change.

## Build system
The ESP32 target uses ESP-IDF's CMake build, not this project's top-level
`CMakeLists.txt`. Create an `esp32/` ESP-IDF component that pulls in the
`gui/` and `data/` sources and links the platform drivers.

## No GoFMX credentials here
Room identity (`resource_id`, `building_id`) and the viewfmx server URL
are set at build time via Kconfig or NVS — never hardcoded. The GoFMX
token lives exclusively in the viewfmx Docker container.
