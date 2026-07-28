#pragma once
// Map-tile background for the radar scope.
//
// The scope is an azimuthal-equidistant plot (screen radius is linear in distance),
// while web tiles are Web Mercator, so the client resamples a stitched tile mosaic
// through the inverse of the scope projection. The result is a full-screen RGB565
// image, masked to the scope circle, that the radar draws underneath its chrome.
//
// Portable half: double-buffered pixel store (same front/back pattern as wx_radar).
#include <stdint.h>
#include "config.h"

// Matches the panel; the circle is masked inside it. This was hardcoded to 466, which
// on a larger panel drew the basemap as a 466-pixel square parked in the middle of the
// screen instead of filling the scope.
#define MAP_BG_SIZE SCREEN_W

void map_bg_begin(void);
uint16_t *map_bg_back_buffer(void);          // client renders here, then commits
void map_bg_commit(double lat, double lon, float rangeKm);
// Front buffer plus the geometry it was rendered for, so the UI can tell whether the
// image still matches the current scope (a zoom invalidates it until the next build).
bool map_bg_front(const uint16_t **pixels, double *lat, double *lon,
                  float *rangeKm, uint32_t *version);
void map_bg_clear(void);                     // drop the image (feature turned off)
