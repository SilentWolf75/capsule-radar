#pragma once
// NASA FIRMS active-fire fetch (device-only; runs on the core-0 network task).
// Needs a free MAP_KEY from https://firms.modaps.eosdis.nasa.gov/api/map_key/
#include <stddef.h>

void wildfire_set_key(const char *mapKey);   // "" disables the feature
bool wildfire_has_key(void);
bool wildfire_fetch(double lat, double lon, float radiusKm);
