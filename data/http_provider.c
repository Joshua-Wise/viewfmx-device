#include "http_provider.h"
#include "http_provider_parse.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Internal response buffer                                             */
/* ------------------------------------------------------------------ */

typedef struct {
    char  *data;
    size_t len;
} ResponseBuf;

static size_t write_cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    ResponseBuf *buf = userdata;
    size_t total = size * nmemb;
    char *tmp = realloc(buf->data, buf->len + total + 1);
    if (!tmp) return 0;
    buf->data = tmp;
    memcpy(buf->data + buf->len, ptr, total);
    buf->len += total;
    buf->data[buf->len] = '\0';
    return total;
}

static char *http_get(const char *url)
{
    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    ResponseBuf buf = {0};
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode rc = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK) {
        free(buf.data);
        return NULL;
    }
    return buf.data; /* caller must free */
}

static char *http_post_json(const char *url, const char *body)
{
    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    struct curl_slist *hdrs = NULL;
    hdrs = curl_slist_append(hdrs, "Content-Type: application/json");

    ResponseBuf buf = {0};
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode rc = curl_easy_perform(curl);
    curl_slist_free_all(hdrs);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK) {
        free(buf.data);
        return NULL;
    }
    return buf.data;
}

/* ------------------------------------------------------------------ */
/* Provider implementation                                              */
/* ------------------------------------------------------------------ */

static int http_fetch(void *ctx, ViewFMX_RoomData *out)
{
    const HttpProviderConfig *cfg = ctx;
    char url[512];
    snprintf(url, sizeof(url),
             "%s/device/v1/rooms/%s/status?buildingId=%s&count=%d",
             cfg->base_url, cfg->resource_id, cfg->building_id, VIEWFMX_MAX_UPCOMING);

    char *body = http_get(url);
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

    char *resp = http_post_json(url, req_body);
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
