#include "http_provider_parse.h"

#ifdef ESP_PLATFORM
#include "cJSON.h"            /* ESP-IDF built-in json component */
#else
#include "../vendor/cjson/cJSON.h"
#endif

#include <string.h>

static void copy_str(char *dst, size_t dsz, const cJSON *obj, const char *key)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsString(item) && item->valuestring) {
        strncpy(dst, item->valuestring, dsz - 1);
        dst[dsz - 1] = '\0';
    }
}

int viewfmx_parse_status_json(const char *json, ViewFMX_RoomData *out)
{
    cJSON *root = cJSON_Parse(json);
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

int viewfmx_parse_book_json(const char *json)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) return -1;

    const cJSON *success = cJSON_GetObjectItemCaseSensitive(root, "success");
    int ok = cJSON_IsTrue(success) ? 0 : -1;
    cJSON_Delete(root);
    return ok;
}
