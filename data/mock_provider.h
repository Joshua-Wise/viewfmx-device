#pragma once
#include "data_provider.h"

/* Fills *provider with a MockProvider that returns static canned data.
 * Safe to call from any platform — no network required.         */
void mock_provider_init(ViewFMX_DataProvider *provider);
