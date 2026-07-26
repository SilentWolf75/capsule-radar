#include "firemap.h"
#include <math.h>
#include <mutex>
#include <string.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static std::mutex s_m;

// Published to the UI.
static FireCell s_cells[FIREMAP_MAX_CELLS];
static int      s_count = 0;
static int      s_total = 0;

// Accumulator, written only by the network task between reset() and commit().
struct Bin { int32_t key; float lat, lon, frp; uint16_t count; };
static Bin s_bins[FIREMAP_MAX_CELLS];
static int s_binCount = 0;

static char     s_status[64] = "waiting for data";
static FireView s_view;
static bool     s_zoomed = false;
static bool     s_needFetch = true;

void firemap_default_view(FireView *v) {
    if (!v) return;
    // Alaska through Newfoundland, southern Mexico to the Canadian arctic. Wide, but
    // clipping Alaska would drop one of the more fire-prone regions on the continent.
    v->west = -170.0; v->east = -52.0;
    v->south = 12.0;  v->north = 72.0;
}

void firemap_begin(void) {
    std::lock_guard<std::mutex> g(s_m);
    firemap_default_view(&s_view);
    s_zoomed = false;
    s_needFetch = true;
    s_count = s_total = 0;
}

void firemap_get_view(FireView *v) {
    std::lock_guard<std::mutex> g(s_m);
    if (v) *v = s_view;
}

void firemap_set_view(const FireView *v) {
    if (!v) return;
    std::lock_guard<std::mutex> g(s_m);
    s_view = *v;
    FireView d; firemap_default_view(&d);
    s_zoomed = (v->west > d.west + 0.5 || v->east < d.east - 0.5 ||
                v->south > d.south + 0.5 || v->north < d.north - 0.5);
    s_needFetch = true;
    s_count = 0;                       // old bins belong to the old view
}

bool firemap_is_zoomed(void)   { std::lock_guard<std::mutex> g(s_m); return s_zoomed; }
bool firemap_needs_fetch(void) { std::lock_guard<std::mutex> g(s_m); return s_needFetch; }
void firemap_mark_fetched(void){ std::lock_guard<std::mutex> g(s_m); s_needFetch = false; }

// --- binning ---------------------------------------------------------------------
void firemap_bin_reset(void) { s_binCount = 0; }

void firemap_bin_add(float lat, float lon, float frp) {
    FireView v;
    { std::lock_guard<std::mutex> g(s_m); v = s_view; }
    if (lat < v.south || lat > v.north || lon < v.west || lon > v.east) return;

    const int gx = (int)((lon - v.west) / (v.east - v.west) * FIREMAP_GRID_X);
    const int gy = (int)((lat - v.south) / (v.north - v.south) * FIREMAP_GRID_Y);
    const int32_t key = (int32_t)gy * FIREMAP_GRID_X + gx;

    // Linear scan. The occupied-cell count is small (hundreds at most) and this runs
    // once per detection on the network task, so a hash table would be false economy.
    for (int i = 0; i < s_binCount; ++i) {
        if (s_bins[i].key == key) {
            s_bins[i].frp += frp;
            if (s_bins[i].count < 0xFFFF) s_bins[i].count++;
            return;
        }
    }
    if (s_binCount >= FIREMAP_MAX_CELLS) return;      // saturated: densest areas already shown
    Bin &b = s_bins[s_binCount++];
    b.key = key;
    b.lon = (float)(v.west  + (gx + 0.5) * (v.east - v.west) / FIREMAP_GRID_X);
    b.lat = (float)(v.south + (gy + 0.5) * (v.north - v.south) / FIREMAP_GRID_Y);
    b.frp = frp;
    b.count = 1;
}

void firemap_bin_commit(int totalDetections) {
    std::lock_guard<std::mutex> g(s_m);
    s_count = (s_binCount > FIREMAP_MAX_CELLS) ? FIREMAP_MAX_CELLS : s_binCount;
    for (int i = 0; i < s_count; ++i) {
        s_cells[i].lat = s_bins[i].lat;
        s_cells[i].lon = s_bins[i].lon;
        s_cells[i].frp = s_bins[i].frp;
        s_cells[i].count = s_bins[i].count;
    }
    s_total = totalDetections;
}

void firemap_set_status(const char *s) {
    std::lock_guard<std::mutex> g(s_m);
    snprintf(s_status, sizeof(s_status), "%s", s ? s : "");
}
const char *firemap_status(void) { return s_status; }

int firemap_cell_count(void) { std::lock_guard<std::mutex> g(s_m); return s_count; }
int firemap_total(void)      { std::lock_guard<std::mutex> g(s_m); return s_total; }

bool firemap_cell(int idx, FireCell *out) {
    std::lock_guard<std::mutex> g(s_m);
    if (idx < 0 || idx >= s_count) return false;
    if (out) *out = s_cells[idx];
    return true;
}

// --- projection -------------------------------------------------------------------
// Equirectangular, with longitude scaled by cos(mid-latitude). Without that correction
// North America renders as a wide smear; with it, the landmass keeps its familiar shape.
static double lon_scale(const FireView &v) {
    const double midLat = (v.south + v.north) * 0.5;
    const double c = cos(midLat * M_PI / 180.0);
    return (c < 0.2) ? 0.2 : c;
}

void firemap_project(double lat, double lon, lv_coord_t cx, lv_coord_t cy,
                     lv_coord_t halfW, lv_coord_t halfH, lv_point_t *out) {
    FireView v; firemap_get_view(&v);
    const double k = lon_scale(v);
    const double wDeg = (v.east - v.west) * k;
    const double hDeg = (v.north - v.south);
    // Fit the view inside the box, preserving aspect, so the map is never distorted.
    const double sx = (wDeg > 0) ? (2.0 * halfW / wDeg) : 1.0;
    const double sy = (hDeg > 0) ? (2.0 * halfH / hDeg) : 1.0;
    const double s = (sx < sy) ? sx : sy;
    const double midLon = (v.west + v.east) * 0.5, midLat = (v.south + v.north) * 0.5;
    if (out) {
        out->x = (lv_coord_t)lround(cx + (lon - midLon) * k * s);
        out->y = (lv_coord_t)lround(cy - (lat - midLat) * s);   // screen y grows downward
    }
}

void firemap_unproject(lv_coord_t x, lv_coord_t y, lv_coord_t cx, lv_coord_t cy,
                       lv_coord_t halfW, lv_coord_t halfH, double *lat, double *lon) {
    FireView v; firemap_get_view(&v);
    const double k = lon_scale(v);
    const double wDeg = (v.east - v.west) * k;
    const double hDeg = (v.north - v.south);
    const double sx = (wDeg > 0) ? (2.0 * halfW / wDeg) : 1.0;
    const double sy = (hDeg > 0) ? (2.0 * halfH / hDeg) : 1.0;
    const double s = (sx < sy) ? sx : sy;
    const double midLon = (v.west + v.east) * 0.5, midLat = (v.south + v.north) * 0.5;
    if (lon) *lon = midLon + ((double)x - cx) / (k * s);
    if (lat) *lat = midLat - ((double)y - cy) / s;
}
