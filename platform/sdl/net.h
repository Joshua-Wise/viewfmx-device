#pragma once

/* One-time network init/cleanup (libcurl global state).
 * Call net_init() before any DataProvider use;
 * net_cleanup() at shutdown.                          */
void net_init(void);
void net_cleanup(void);
