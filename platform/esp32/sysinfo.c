#include "../sysinfo.h"

#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "nvs.h"

#include <stdio.h>
#include <string.h>

#define NVS_NS       "viewfmx"
#define KEY_RESOURCE "res_id"
#define KEY_BUILDING "bld_id"

void sysinfo_get_network(char *ip, size_t ip_sz, char *mac, size_t mac_sz)
{
    snprintf(ip, ip_sz, "no IP");
    esp_netif_t *netif = esp_netif_next_unsafe(NULL);
    if (netif) {
        esp_netif_ip_info_t info;
        if (esp_netif_get_ip_info(netif, &info) == ESP_OK && info.ip.addr) {
            snprintf(ip, ip_sz, IPSTR, IP2STR(&info.ip));
        }
    }

    uint8_t m[6] = {0};
    esp_read_mac(m, ESP_MAC_ETH);
    snprintf(mac, mac_sz, "%02x:%02x:%02x:%02x:%02x:%02x",
             m[0], m[1], m[2], m[3], m[4], m[5]);
}

bool roomcfg_load(char *resource, size_t rsz, char *building, size_t bsz)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return false;

    size_t rlen = rsz, blen = bsz;
    bool ok = nvs_get_str(h, KEY_RESOURCE, resource, &rlen) == ESP_OK &&
              nvs_get_str(h, KEY_BUILDING, building, &blen) == ESP_OK;
    nvs_close(h);
    return ok && resource[0] && building[0];
}

void roomcfg_save_and_restart(const char *resource, const char *building)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, KEY_RESOURCE, resource);
        nvs_set_str(h, KEY_BUILDING, building);
        nvs_commit(h);
        nvs_close(h);
    }
    esp_restart();
}
