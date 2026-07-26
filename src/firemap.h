#pragma once
// Continental wildfire map: shared state for the FIRES screen.
//
// Detections are binned into grid cells before they reach the UI. A continent-wide
// query returns thousands of points and the panel is 466 px across roughly 118 degrees
// of longitude - about a quarter degree per pixel - so individual markers would merge
// into noise. One marker per occupied cell, weighted by summed radiative power, is both
// far cheaper and far more readable: you see the Pacific Northwest alight rather than
// confetti.
//
// Portable (no network, no Arduino): the client fills this in, the UI reads it.
#include <lvgl.h>
#include <stddef.h>
#include <stdint.h>

#define FIREMAP_MAX_CELLS 400      // sparse: only occupied cells are stored
#define FIREMAP_GRID_X    72       // bins across the current view
#define FIREMAP_GRID_Y    48

struct FireCell {
    float    lat, lon;     // centre of the bin
    float    frp;          // summed fire radiative power, MW
    uint16_t count;        // detections in the bin
};

// The area currently displayed. Starts continental; a tap narrows it.
struct FireView { double west, south, east, north; };

void firemap_begin(void);
void firemap_default_view(FireView *v);          // USA + Canada + Mexico
void firemap_get_view(FireView *v);
void firemap_set_view(const FireView *v);        // triggers a refetch
bool firemap_is_zoomed(void);

// --- network task ---
void firemap_bin_reset(void);                    // start a new accumulation
void firemap_bin_add(float lat, float lon, float frp);
void firemap_bin_commit(int totalDetections);    // publish to the UI
bool firemap_needs_fetch(void);
void firemap_mark_fetched(void);
void firemap_request_fetch(void);

// --- UI ---
int  firemap_cell_count(void);
bool firemap_cell(int idx, FireCell *out);
int  firemap_total(void);                        // detections before binning
void firemap_set_status(const char *s);          // short line for the screen footer
const char *firemap_status(void);
uint32_t firemap_version(void);                  // bumps on every commit

// Project a lat/lon into the map rectangle, and back again for hit-testing. The
// horizontal scale is cosine-corrected at the view's mid-latitude so the continent is
// not stretched sideways.
void firemap_project(double lat, double lon, lv_coord_t cx, lv_coord_t cy,
                     lv_coord_t halfW, lv_coord_t halfH, lv_point_t *out);
void firemap_unproject(lv_coord_t x, lv_coord_t y, lv_coord_t cx, lv_coord_t cy,
                       lv_coord_t halfW, lv_coord_t halfH, double *lat, double *lon);
