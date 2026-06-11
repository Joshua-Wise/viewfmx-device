/* HTTP provider transport for ESP-IDF: esp_http_client instead of libcurl.
 * Same interface (http_provider.h) and shared JSON parsing
 * (http_provider_parse.c) as the SDL/curl implementation. */

#include "http_provider.h"
#include "http_provider_parse.h"

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"

#include <stdlib.h>
#include <string.h>

static const char *TAG = "http_provider";

/* GoDaddy G2 intermediate CA (esp32/main/certs/gdig2.pem), used as the
 * trust anchor because viewfmx.celinaisd.com serves an incomplete chain
 * (leaf only) that the standard root bundle cannot verify. Safe to swap
 * back to esp_crt_bundle_attach once the server serves the full chain. */
extern const char gdig2_pem_start[] asm("_binary_gdig2_pem_start");
extern const char gdig2_pem_end[]   asm("_binary_gdig2_pem_end");

/* Perform a request and return the malloc'd, NUL-terminated response body
 * (caller frees), or NULL on failure. POST when post_body != NULL. */
static char *http_request(const char *url, const char *post_body)
{
    esp_http_client_config_t cfg = {
        .url = url,
        .method = post_body ? HTTP_METHOD_POST : HTTP_METHOD_GET,
        .timeout_ms = 10000,
        /* For https:// URLs (the live server redirects HTTP to HTTPS). */
        .cert_pem = gdig2_pem_start,
        .cert_len = (size_t)(gdig2_pem_end - gdig2_pem_start),
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return NULL;

    char *buf = NULL;
    size_t len = 0;
    int body_len = post_body ? (int)strlen(post_body) : 0;

    if (post_body) {
        esp_http_client_set_header(client, "Content-Type", "application/json");
    }
    if (esp_http_client_open(client, body_len) != ESP_OK) {
        ESP_LOGW(TAG, "open failed: %s", url);
        goto fail;
    }
    if (post_body &&
        esp_http_client_write(client, post_body, body_len) != body_len) {
        ESP_LOGW(TAG, "write failed");
        goto fail;
    }
    if (esp_http_client_fetch_headers(client) < 0) {
        ESP_LOGW(TAG, "fetch_headers failed");
        goto fail;
    }

    int status = esp_http_client_get_status_code(client);
    while (1) {
        char chunk[512];
        int n = esp_http_client_read(client, chunk, sizeof(chunk));
        if (n < 0) goto fail;
        if (n == 0) break;
        char *tmp = realloc(buf, len + n + 1);
        if (!tmp) goto fail;
        buf = tmp;
        memcpy(buf + len, chunk, n);
        len += n;
        buf[len] = '\0';
    }
    if (!buf) {
        /* Empty body still counts as a response; return an empty string. */
        buf = calloc(1, 1);
    }
    if (status < 200 || status >= 300) {
        ESP_LOGW(TAG, "HTTP %d for %s", status, url);
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return buf;

fail:
    free(buf);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return NULL;
}

static int http_fetch(void *ctx, ViewFMX_RoomData *out)
{
    const HttpProviderConfig *cfg = ctx;
    char url[512];
    snprintf(url, sizeof(url),
             "%s/device/v1/rooms/%s/status?buildingId=%s&count=%d",
             cfg->base_url, cfg->resource_id, cfg->building_id,
             VIEWFMX_MAX_UPCOMING);

    char *body = http_request(url, NULL);
    if (!body) return -1;

    int rc = viewfmx_parse_status_json(body, out);
    free(body);
    return rc;
}

static int http_book(void *ctx, int duration_minutes)
{
    const HttpProviderConfig *cfg = ctx;
    char url[512];
    snprintf(url, sizeof(url),
             "%s/device/v1/rooms/%s/book", cfg->base_url, cfg->resource_id);

    char req_body[256];
    snprintf(req_body, sizeof(req_body),
             "{\"building_id\":\"%s\",\"duration_minutes\":%d}",
             cfg->building_id, duration_minutes);

    char *resp = http_request(url, req_body);
    if (!resp) return -1;

    int rc = viewfmx_parse_book_json(resp);
    free(resp);
    return rc;
}

void http_provider_init(ViewFMX_DataProvider *provider, const HttpProviderConfig *cfg)
{
    HttpProviderConfig *stored = malloc(sizeof(HttpProviderConfig));
    if (!stored) return;
    *stored = *cfg;

    provider->fetch_status = http_fetch;
    provider->book_room    = http_book;
    provider->ctx          = stored;
}

void http_provider_destroy(ViewFMX_DataProvider *provider)
{
    free(provider->ctx);
    provider->ctx = NULL;
}
