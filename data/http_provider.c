#include "http_provider.h"
#include "../vendor/cjson/cJSON.h"
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
/* JSON helpers                                                         */
/* ------------------------------------------------------------------ */

static void copy_str(char *dst, size_t dsz, const cJSON *obj, const char *key)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsString(item) && item->valuestring) {
        strncpy(dst, item->valuestring, dsz - 1);
        dst[dsz - 1] = '\0';
    }
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

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) return -1;

    memset(out, 0, sizeof(*out));

    const cJSON *room = cJSON_GetObjectItemCaseSensitive(root, "room");
    if (room) copy_str(out->room_name, sizeof(out->room_name), room, "name");

    const cJSON *status = cJSON_GetObjectItemCaseSensitive(root, "status");
    out->is_busy = cJSON_IsString(status) && strcmp(status->valuestring, "busy") == 0;

    const cJSON *cur = cJSON_GetObjectItemCaseSensitive(root, "current_meeting");
    if (!cJSON_IsNull(cur) && cur) {
        out->has_current = true;
        copy_str(out->current.title,      sizeof(out->current.title),      cur, "title");
        copy_str(out->current.start_time, sizeof(out->current.start_time), cur, "start_time");
        copy_str(out->current.end_time,   sizeof(out->current.end_time),   cur, "end_time");
        const cJSON *priv = cJSON_GetObjectItemCaseSensitive(cur, "is_private");
        out->current.is_private = cJSON_IsTrue(priv);
        const cJSON *mins = cJSON_GetObjectItemCaseSensitive(cur, "minutes_remaining");
        if (cJSON_IsNumber(mins)) out->current.minutes_remaining = (int)mins->valuedouble;
    }

    const cJSON *upcoming = cJSON_GetObjectItemCaseSensitive(root, "upcoming_meetings");
    if (cJSON_IsArray(upcoming)) {
        int i = 0;
        const cJSON *item;
        cJSON_ArrayForEach(item, upcoming) {
            if (i >= VIEWFMX_MAX_UPCOMING) break;
            copy_str(out->upcoming[i].title,      sizeof(out->upcoming[i].title),      item, "title");
            copy_str(out->upcoming[i].start_time, sizeof(out->upcoming[i].start_time), item, "start_time");
            copy_str(out->upcoming[i].end_time,   sizeof(out->upcoming[i].end_time),   item, "end_time");
            const cJSON *priv = cJSON_GetObjectItemCaseSensitive(item, "is_private");
            out->upcoming[i].is_private = cJSON_IsTrue(priv);
            i++;
        }
        out->upcoming_count = i;
    }

    cJSON_Delete(root);
    return 0;
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

    cJSON *root = cJSON_Parse(resp);
    free(resp);
    if (!root) return -1;

    const cJSON *success = cJSON_GetObjectItemCaseSensitive(root, "success");
    int ok = cJSON_IsTrue(success) ? 0 : -1;
    cJSON_Delete(root);
    return ok;
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
