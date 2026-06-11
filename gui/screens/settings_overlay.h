#pragma once
#include "../../data/data_provider.h"

/* Modal settings overlay (opened by tapping the district logo):
 * room IDs (editable, persisted via roomcfg_save_and_restart),
 * network details, and a refresh button. */
void settings_overlay_open(ViewFMX_DataProvider *provider,
                           const char *resource_id,
                           const char *building_id);
