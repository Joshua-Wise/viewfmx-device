#pragma once
#include "../../data/data_provider.h"

/* Background schedule refresh: fetches every 60 s from its own task and
 * posts results to the UI under the LVGL lock, so a hung connection can
 * never stall the LVGL task. Also arms a task watchdog on the LVGL task
 * (panic + reboot if the UI stalls). Call after gui_init(). */
void fetcher_init(ViewFMX_DataProvider *provider);
