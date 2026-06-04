#include "net.h"
#include <curl/curl.h>

void net_init(void)
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

void net_cleanup(void)
{
    curl_global_cleanup();
}
