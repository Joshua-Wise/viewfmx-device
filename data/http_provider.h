#pragma once
#include "data_provider.h"

typedef struct {
    char base_url[256];     /* e.g. "http://192.168.1.10:3000" */
    char resource_id[32];
    char building_id[32];
} HttpProviderConfig;

/* Initialises *provider to call viewfmx /device/v1/ endpoints via libcurl.
 * net_init() must have been called before any fetch/book calls.           */
void http_provider_init(ViewFMX_DataProvider *provider, const HttpProviderConfig *cfg);
void http_provider_destroy(ViewFMX_DataProvider *provider);
