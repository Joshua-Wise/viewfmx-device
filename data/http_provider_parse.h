#pragma once
#include "data_provider.h"

/* Shared JSON parsing for the viewfmx /device/v1/ endpoints, used by both
 * the libcurl (SDL) and esp_http_client (ESP32) transports.
 * Both return 0 on success, non-zero on parse error / failure. */
int viewfmx_parse_status_json(const char *json, ViewFMX_RoomData *out);
int viewfmx_parse_book_json(const char *json);
