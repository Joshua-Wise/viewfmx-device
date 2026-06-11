#pragma once
#include <stdbool.h>
#include <stddef.h>

/* Platform info/config used by the settings overlay. Implemented in
 * platform/sdl/sysinfo.c (stubs) and platform/esp32/sysinfo.c (NVS +
 * esp_netif). */

/* Fills ip/mac with printable strings ("10.0.0.5", "80:f1:..."). */
void sysinfo_get_network(char *ip, size_t ip_sz, char *mac, size_t mac_sz);

/* Loads a persisted room config. Returns false when none is stored
 * (caller falls back to build-time defaults). */
bool roomcfg_load(char *resource, size_t rsz, char *building, size_t bsz);

/* Persists the room config and restarts the device to apply it.
 * On the simulator this just logs. */
void roomcfg_save_and_restart(const char *resource, const char *building);
