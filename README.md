# viewfmx-device

Native LVGL UI for conference-room signs, targeting the
**Guition JC1060P470C** (ESP32-P4, 7″ 1024×600 IPS, capacitive touch,
wired Ethernet). Developed and tested on macOS using the LVGL SDL simulator.

The device displays the same per-room schedule data as the viewFMX iPad
web app, fetched from the same Docker container via two JSON endpoints.
No GoFMX credentials live on the device.

---

## macOS Build (Apple Silicon)

### Prerequisites

```bash
brew install cmake sdl2 curl
```

> **Apple Silicon arch-mismatch gotcha**: Homebrew on M-series Macs installs
> `arm64` libraries. Do **not** pass `-DCMAKE_OSX_ARCHITECTURES=x86_64` to
> CMake — linking will fail because SDL2 and curl will be arm64 but your
> binary would target x86_64. Leave the arch flag unset and CMake picks the
> native arm64 target automatically.

### Build (MockProvider — no server needed)

```bash
cd /path/to/viewfmx-device
mkdir build && cd build
cmake -DUSE_MOCK=ON ..
make -j$(sysctl -n hw.logicalcpu)
```

### Run

```bash
./viewfmx_device
```

A 1024×600 SDL window opens showing canned room data. Click **Refresh**
or **Book 30/60 min** to exercise the UI without network traffic.

---

### Build (HttpProvider — live viewfmx server)

Start the viewFMX Docker stack first:

```bash
cd ../viewFMX/docker
docker compose --env-file ../.env up -d
```

Then build with your room's IDs:

```bash
cmake \
  -DUSE_MOCK=OFF \
  -DVIEWFMX_BASE_URL="http://localhost:3000" \
  -DVIEWFMX_RESOURCE_ID="<your-resource-id>" \
  -DVIEWFMX_BUILDING_ID="<your-building-id>" \
  ..
make -j$(sysctl -n hw.logicalcpu)
./viewfmx_device
```

The room IDs are the same integers you configure in the iPad Settings UI
(`?resourceId=X&buildingId=Y`).

---

## Project layout

```
viewfmx-device/
├── CMakeLists.txt          Main build (macOS SDL)
├── lv_conf.h               LVGL v9 config: 1024×600, 16-bit color
├── main.c                  macOS SDL entry point
├── gui/                    Portable LVGL layer (compiles unchanged on ESP32)
│   ├── gui.h / gui.c       Init + 60-second refresh timer
│   └── screens/
│       └── room_screen.c   Widgets: name, status, current mtg, upcoming, buttons
├── data/                   Data contract + provider implementations
│   ├── data_provider.h     C interface (struct of fn pointers)
│   ├── mock_provider.*     Canned offline data
│   └── http_provider.*     libcurl → viewfmx /device/v1/ endpoints
├── platform/
│   ├── sdl/                macOS SDL display + input + curl init
│   └── esp32/              PORT LAYER STUB — see platform/esp32/README.md
└── vendor/cjson/           cJSON v1.7.18 (MIT, two files)
```

---

## viewfmx JSON endpoints (served by viewFMX Docker container)

### `GET /device/v1/rooms/:resourceId/status?buildingId=<id>[&count=<n>]`

Returns current room state. `count` defaults to 5 upcoming meetings.

```json
{
  "room": { "resource_id": "456", "building_id": "123", "name": "Lakeview A" },
  "as_of": "2026-06-03T19:45:00Z",
  "status": "busy",
  "current_meeting": {
    "id": "9900", "title": "Q3 Planning",
    "start_time": "2026-06-03T19:00:00Z", "end_time": "2026-06-03T20:00:00Z",
    "is_private": false, "minutes_remaining": 15
  },
  "upcoming_meetings": [
    { "id": "9901", "title": "1:1 — Josh",
      "start_time": "2026-06-03T20:30:00Z", "end_time": "2026-06-03T21:00:00Z",
      "is_private": false }
  ]
}
```

### `POST /device/v1/rooms/:resourceId/book`

```json
// request
{ "building_id": "123", "duration_minutes": 30 }

// success
{ "success": true, "meeting_id": "9910",
  "start_time": "...", "end_time": "..." }

// conflict
{ "success": false, "error": "Room is not available for the requested duration" }
```

---

## ESP32-P4 port (future)

See `platform/esp32/README.md`. The `gui/` and `data/` layers compile
unchanged. Only `platform/esp32/` and the ESP-IDF CMake wrapper need to
be written.
