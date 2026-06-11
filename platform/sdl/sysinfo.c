#include "../sysinfo.h"
#include <stdio.h>
#include <string.h>

void sysinfo_get_network(char *ip, size_t ip_sz, char *mac, size_t mac_sz)
{
    snprintf(ip, ip_sz, "simulator");
    snprintf(mac, mac_sz, "--");
}

bool roomcfg_load(char *resource, size_t rsz, char *building, size_t bsz)
{
    (void)resource; (void)rsz; (void)building; (void)bsz;
    return false;
}

void roomcfg_save_and_restart(const char *resource, const char *building)
{
    printf("[viewfmx-device] roomcfg_save_and_restart(%s, %s): "
           "no-op on simulator (set via CMake defines)\n", resource, building);
}
