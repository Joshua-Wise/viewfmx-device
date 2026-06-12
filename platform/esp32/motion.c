#include "motion.h"
#include "display.h"
#include "input.h"

#include "esp_cache.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include "esp_video_device.h"
#include "esp_video_init.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "linux/videodev2.h"
#include "sdkconfig.h"

#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

static const char *TAG = "motion";

#define CAM_DEV       ESP_VIDEO_MIPI_CSI_DEVICE_NAME
#define CAM_BUF_COUNT 2

/* Coarse luma grid for frame differencing. */
#define GRID_W      32
#define GRID_H      24
#define GRID_CELLS  (GRID_W * GRID_H)
#define CELL_DELTA  14                    /* per-cell luma change to count  */
#define MIN_CELLS   (GRID_CELLS * 3 / 200)  /* >=1.5% of cells -> motion    */
#define MAX_CELLS   (GRID_CELLS * 70 / 100) /* >70% is exposure flicker     */

#define FRAME_INTERVAL_MS 500

static struct {
    void  *start;
    size_t len;
} s_bufs[CAM_BUF_COUNT];

static uint8_t s_grid_prev[GRID_CELLS];
static bool    s_grid_valid;
static int64_t s_last_motion_us;

/* Builds a GRID_WxGRID_H mean-luma grid from an RGB565 frame, using the
 * green channel as the luma proxy, averaging a 4x4 pixel block per cell
 * to suppress sensor noise. */
static void build_grid(const uint16_t *px, uint32_t w, uint32_t h, uint8_t *grid)
{
    for (int gy = 0; gy < GRID_H; gy++) {
        uint32_t y0 = (uint64_t)h * (2 * gy + 1) / (2 * GRID_H);
        for (int gx = 0; gx < GRID_W; gx++) {
            uint32_t x0 = (uint64_t)w * (2 * gx + 1) / (2 * GRID_W);
            uint32_t sum = 0;
            for (int dy = 0; dy < 4; dy++) {
                const uint16_t *row = px + (y0 + dy) * w + x0;
                for (int dx = 0; dx < 4; dx++) {
                    sum += (row[dx] >> 3) & 0xFC;   /* G6 -> 8-bit */
                }
            }
            grid[gy * GRID_W + gx] = sum / 16;
        }
    }
}

static bool detect_motion(const uint8_t *grid)
{
    if (!s_grid_valid) return false;

    int changed = 0;
    for (int i = 0; i < GRID_CELLS; i++) {
        int d = (int)grid[i] - (int)s_grid_prev[i];
        if (d < 0) d = -d;
        if (d > CELL_DELTA) changed++;
    }
    static int dbg;
    if (++dbg % 120 == 0) {
        ESP_LOGD(TAG, "changed cells: %d / %d", changed, GRID_CELLS);
    }
    return changed >= MIN_CELLS && changed <= MAX_CELLS;
}

static int64_t touch_idle_us(void)
{
    uint32_t ms = UINT32_MAX;
    if (lvgl_port_lock(50)) {
        ms = lv_display_get_inactive_time(NULL);
        lvgl_port_unlock();
    }
    return (int64_t)ms * 1000;
}

static void motion_task(void *arg)
{
    int fd = (int)(intptr_t)arg;
    struct v4l2_format fmt = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE };
    ioctl(fd, VIDIOC_G_FMT, &fmt);
    const uint32_t w = fmt.fmt.pix.width, h = fmt.fmt.pix.height;
    ESP_LOGI(TAG, "camera streaming %lux%lu, dim after %ds to %d%%",
             (unsigned long)w, (unsigned long)h,
             CONFIG_VIEWFMX_DIM_TIMEOUT_S, CONFIG_VIEWFMX_DIM_LEVEL);

    uint8_t grid[GRID_CELLS];
    int current_brightness = 100;
    s_last_motion_us = esp_timer_get_time();

    int starved = 0;
    while (1) {
        struct v4l2_buffer buf = {
            .type   = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .memory = V4L2_MEMORY_MMAP,
        };
        if (ioctl(fd, VIDIOC_DQBUF, &buf) != 0) {
            /* Non-blocking: no frame ready (or a transient driver error).
             * Never park forever and never die — after ~30 s without
             * frames, bounce the stream and carry on. */
            if (++starved >= 60) {
                ESP_LOGW(TAG, "no camera frames for 30s; restarting stream");
                int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                ioctl(fd, VIDIOC_STREAMOFF, &type);
                for (int i = 0; i < CAM_BUF_COUNT; i++) {
                    struct v4l2_buffer rb = {
                        .type   = V4L2_BUF_TYPE_VIDEO_CAPTURE,
                        .memory = V4L2_MEMORY_MMAP,
                        .index  = i,
                    };
                    ioctl(fd, VIDIOC_QBUF, &rb);
                }
                ioctl(fd, VIDIOC_STREAMON, &type);
                display_set_brightness(100);
                starved = 0;
            }
            vTaskDelay(pdMS_TO_TICKS(FRAME_INTERVAL_MS));
            continue;
        }
        starved = 0;

        /* Frame is DMA-written PSRAM; drop stale cache lines first. */
        esp_cache_msync(s_bufs[buf.index].start, s_bufs[buf.index].len,
                        ESP_CACHE_MSYNC_FLAG_DIR_M2C);
        build_grid(s_bufs[buf.index].start, w, h, grid);
        if (detect_motion(grid)) {
            s_last_motion_us = esp_timer_get_time();
        }
        memcpy(s_grid_prev, grid, sizeof(s_grid_prev));
        s_grid_valid = true;

        ioctl(fd, VIDIOC_QBUF, &buf);

        int64_t idle_us = esp_timer_get_time() - s_last_motion_us;
        int64_t t_idle = touch_idle_us();
        if (t_idle < idle_us) idle_us = t_idle;

        int target = (idle_us > (int64_t)CONFIG_VIEWFMX_DIM_TIMEOUT_S * 1000000)
                         ? CONFIG_VIEWFMX_DIM_LEVEL : 100;
        if (target != current_brightness) {
            ESP_LOGI(TAG, "%s (idle %llds)",
                     target == 100 ? "wake" : "dim", idle_us / 1000000);
            display_set_brightness(target);
            current_brightness = target;
        }

        vTaskDelay(pdMS_TO_TICKS(FRAME_INTERVAL_MS));
    }
}

void motion_init(void)
{
    const esp_video_init_csi_config_t csi_cfg = {
        .sccb_config = {
            .init_sccb  = false,
            .i2c_handle = input_get_i2c_bus(),   /* shared with touch */
            .freq       = 100000,
        },
        .reset_pin = -1,
        .pwdn_pin  = -1,
    };
    const esp_video_init_config_t cfg = { .csi = &csi_cfg };

    if (!csi_cfg.sccb_config.i2c_handle || esp_video_init(&cfg) != ESP_OK) {
        ESP_LOGW(TAG, "no camera detected; dimming disabled");
        return;
    }

    int fd = open(CAM_DEV, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        ESP_LOGW(TAG, "camera device open failed; dimming disabled");
        return;
    }

    struct v4l2_format fmt = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE };
    if (ioctl(fd, VIDIOC_G_FMT, &fmt) != 0) goto fail;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
    if (ioctl(fd, VIDIOC_S_FMT, &fmt) != 0) goto fail;

    struct v4l2_requestbuffers req = {
        .count  = CAM_BUF_COUNT,
        .type   = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = V4L2_MEMORY_MMAP,
    };
    if (ioctl(fd, VIDIOC_REQBUFS, &req) != 0) goto fail;

    for (int i = 0; i < CAM_BUF_COUNT; i++) {
        struct v4l2_buffer buf = {
            .type   = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .memory = V4L2_MEMORY_MMAP,
            .index  = i,
        };
        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) != 0) goto fail;
        s_bufs[i].len   = buf.length;
        s_bufs[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                               MAP_SHARED, fd, buf.m.offset);
        if (!s_bufs[i].start) goto fail;
        if (ioctl(fd, VIDIOC_QBUF, &buf) != 0) goto fail;
    }

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) != 0) goto fail;

    xTaskCreate(motion_task, "motion", 6144, (void *)(intptr_t)fd, 3, NULL);
    return;

fail:
    ESP_LOGW(TAG, "camera setup failed; dimming disabled");
    close(fd);
}
