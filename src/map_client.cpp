// Map-tile background: download CARTO basemap tiles, stitch them, then resample the
// mosaic through the inverse of the radar's azimuthal-equidistant projection.
//
// Tiles are Web Mercator; the scope plots screen radius linearly in ground distance.
// Sampling per destination pixel (rather than pasting tiles straight through) keeps the
// map aligned with the range rings at every zoom, which is the whole point of having it.
//
// The download is spread across calls (one tile per map_client_step) so the live ADS-B
// poll keeps running; only the final resample is done in one pass, and that is pure CPU.
#include "map_client.h"
#include "map_bg.h"
#include "net_fetch.h"
#include "config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <PNGdec.h>
#include <esp_heap_caps.h>
#include <math.h>
#include <string.h>
#include <new>

#define MAP_TILE_PX     256
#define MAP_MAX_TILES   4          // per axis (4x4 = 2 MB mosaic worst case)
#define MAP_ZOOM_MIN    4
#define MAP_ZOOM_MAX    13
#define MAP_UA "SkyGlass/1.0 (+https://github.com/socquique/capsule-radar)"

// CARTO basemaps: no API key, dark/light variants without labels (labels are unreadable
// at this scale and fight with the callsign text drawn above them).
#define MAP_HOST_FMT "https://basemaps.cartocdn.com/%s/%d/%d/%d.png"

enum MapState { MAP_IDLE, MAP_FETCH, MAP_COMPOSE };

static bool     s_enabled = false;
static int      s_style = 0;                 // 0 = dark, 1 = light
static MapState s_state = MAP_IDLE;

// geometry of the build in flight
static double   s_lat = 0, s_lon = 0;
static float    s_rangeKm = 0;
static int      s_zoom = 0;
static int      s_tx0 = 0, s_ty0 = 0, s_nx = 0, s_ny = 0;
static int      s_tileIdx = 0;
static int      s_failed = 0;
static int      s_composeRow = 0;

// geometry of the last committed (or attempted) build, to avoid rebuilding needlessly
static double   s_haveLat = 1e9, s_haveLon = 1e9;
static float    s_haveRange = -1.0f;

static uint16_t *s_mosaic = nullptr;
static int       s_mosaicW = 0, s_mosaicH = 0;
static PNG      *s_png = nullptr;
static int       s_dstX = 0, s_dstY = 0;     // where the tile being decoded lands

static bool ensure_decoder(void) {
    if (s_png) return true;
    void *mem = heap_caps_malloc(sizeof(PNG), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!mem) { Serial.println("[map] PSRAM decoder allocation failed"); return false; }
    s_png = new (mem) PNG();
    return true;
}

static void free_mosaic(void) {
    if (s_mosaic) { heap_caps_free(s_mosaic); s_mosaic = nullptr; }
    s_mosaicW = s_mosaicH = 0;
}

void map_client_enable(bool on) {
    s_enabled = on;
    if (!on) {
        s_state = MAP_IDLE;
        free_mosaic();
        map_bg_clear();
        s_haveRange = -1.0f;          // force a rebuild if it is switched back on
    }
}
bool map_client_enabled(void) { return s_enabled; }

void map_client_set_style(int style) {
    const int st = (style == 1) ? 1 : 0;
    if (st == s_style) return;
    s_style = st;
    s_haveRange = -1.0f;              // restyle = rebuild
    s_state = MAP_IDLE;
    free_mosaic();
}
int map_client_style(void) { return s_style; }

// Basemap visibility deliberately does NOT live here any more. It is a display
// property, applied as LVGL image alpha in radar_view, so it is instant, costs no
// refetch, and is reversible. See compose() above.

// --- Web Mercator helpers (world pixel space at the active zoom) ---------------
static inline double world_size(int z) { return (double)MAP_TILE_PX * (double)(1 << z); }

static inline double lon_to_wx(double lon, int z) {
    return (lon + 180.0) / 360.0 * world_size(z);
}
static inline double lat_to_wy(double lat, int z) {
    const double s = sin(lat * M_PI / 180.0);
    const double y = 0.5 - log((1.0 + s) / (1.0 - s)) / (4.0 * M_PI);
    return y * world_size(z);
}

// Pick the zoom whose ground resolution best matches the scope's pixel scale, then
// step down until the tile mosaic fits the per-axis cap.
static int pick_zoom(double lat, float rangeKm) {
    const double mppNeeded = (double)rangeKm * 1000.0 / (double)RADAR_R_OUTER_PX;
    const double base = 156543.03392 * cos(lat * M_PI / 180.0);
    int z = (int)lround(log(base / mppNeeded) / log(2.0));
    if (z < MAP_ZOOM_MIN) z = MAP_ZOOM_MIN;
    if (z > MAP_ZOOM_MAX) z = MAP_ZOOM_MAX;
    return z;
}

// Tile bounds covering the scope's bounding box at zoom z. Returns false if it needs
// more tiles per axis than the cap allows.
static bool tile_bounds(double lat, double lon, float rangeKm, int z,
                        int *tx0, int *ty0, int *nx, int *ny) {
    const double dLat = (double)rangeKm / 111.0;
    const double cosLat = cos(lat * M_PI / 180.0);
    const double dLon = dLat / (cosLat < 0.15 ? 0.15 : cosLat);
    double latN = lat + dLat, latS = lat - dLat;
    if (latN > 85.0) latN = 85.0;
    if (latS < -85.0) latS = -85.0;

    const double wx0 = lon_to_wx(lon - dLon, z), wx1 = lon_to_wx(lon + dLon, z);
    const double wy0 = lat_to_wy(latN, z),       wy1 = lat_to_wy(latS, z);
    const int x0 = (int)floor(wx0 / MAP_TILE_PX), x1 = (int)floor(wx1 / MAP_TILE_PX);
    const int y0 = (int)floor(wy0 / MAP_TILE_PX), y1 = (int)floor(wy1 / MAP_TILE_PX);
    const int cx = x1 - x0 + 1, cy = y1 - y0 + 1;
    if (cx > MAP_MAX_TILES || cy > MAP_MAX_TILES || cx < 1 || cy < 1) return false;
    *tx0 = x0; *ty0 = y0; *nx = cx; *ny = cy;
    return true;
}

void map_client_request(double lat, double lon, float rangeKm) {
    if (!s_enabled) return;
    // Ignore sub-kilometre drift and tiny range changes: rebuilding on every GPS
    // jitter would keep the network task permanently busy.
    if (s_state != MAP_IDLE) {
        if (fabs(lat - s_lat) < 0.01 && fabs(lon - s_lon) < 0.01 &&
            fabsf(rangeKm - s_rangeKm) < 0.5f) return;      // same build already running
    } else if (fabs(lat - s_haveLat) < 0.01 && fabs(lon - s_haveLon) < 0.01 &&
               fabsf(rangeKm - s_haveRange) < 0.5f) {
        return;                                             // current image still valid
    }

    int z = pick_zoom(lat, rangeKm);
    int tx0 = 0, ty0 = 0, nx = 0, ny = 0;
    while (z >= MAP_ZOOM_MIN && !tile_bounds(lat, lon, rangeKm, z, &tx0, &ty0, &nx, &ny)) --z;
    if (z < MAP_ZOOM_MIN) { Serial.println("[map] no usable zoom for this range"); return; }

    free_mosaic();
    s_mosaicW = nx * MAP_TILE_PX;
    s_mosaicH = ny * MAP_TILE_PX;
    s_mosaic = (uint16_t *)heap_caps_malloc((size_t)s_mosaicW * s_mosaicH * sizeof(uint16_t),
                                            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_mosaic) {
        Serial.println("[map] mosaic allocation failed");
        s_mosaicW = s_mosaicH = 0;
        return;
    }
    memset(s_mosaic, 0, (size_t)s_mosaicW * s_mosaicH * sizeof(uint16_t));

    s_lat = lat; s_lon = lon; s_rangeKm = rangeKm;
    s_zoom = z; s_tx0 = tx0; s_ty0 = ty0; s_nx = nx; s_ny = ny;
    s_tileIdx = 0;
    s_failed = 0;
    s_state = MAP_FETCH;
    Serial.printf("[map] build z=%d %dx%d tiles for %.0f km\n", z, nx, ny, (double)rangeKm);
}

// --- tile decode ---------------------------------------------------------------
static int map_png_line(PNGDRAW *draw) {
    if (!s_mosaic) return 0;
    const int y = s_dstY + draw->y;
    if (y < 0 || y >= s_mosaicH) return 1;
    uint16_t line[MAP_TILE_PX];
    const int w = (draw->iWidth > MAP_TILE_PX) ? MAP_TILE_PX : draw->iWidth;
    s_png->getLineAsRGB565(draw, line, PNG_RGB565_LITTLE_ENDIAN, 0xFFFFFFFF);
    for (int x = 0; x < w; ++x) {
        const int dx = s_dstX + x;
        if (dx < 0 || dx >= s_mosaicW) continue;
        s_mosaic[(size_t)y * s_mosaicW + dx] = line[x];
    }
    return 1;
}

static bool fetch_one_tile(int idx) {
    const int ix = idx % s_nx, iy = idx / s_nx;
    const int tx = s_tx0 + ix, ty = s_ty0 + iy;
    const int span = 1 << s_zoom;
    if (ty < 0 || ty >= span) return true;             // above/below the map: leave black
    const int wrapX = ((tx % span) + span) % span;     // wrap across the date line

    char url[160];
    snprintf(url, sizeof(url), MAP_HOST_FMT,
             s_style == 1 ? "light_nolabels" : "dark_nolabels", s_zoom, wrapX, ty);

    uint8_t *png = nullptr; size_t len = 0;
    if (!net_fetch_psram(url, MAP_UA, &png, &len, 131072, 3500, 8000)) {
        Serial.printf("[map] tile %d/%d fetch failed\n", idx + 1, s_nx * s_ny);
        return false;
    }
    if (!ensure_decoder()) { heap_caps_free(png); return false; }

    s_dstX = ix * MAP_TILE_PX;
    s_dstY = iy * MAP_TILE_PX;
    bool ok = false;
    if (s_png->openRAM(png, len, map_png_line) == PNG_SUCCESS) {
        ok = (s_png->decode(nullptr, 0) == PNG_SUCCESS);
        s_png->close();
    }
    heap_caps_free(png);
    if (!ok) Serial.printf("[map] tile %d decode failed\n", idx + 1);
    return ok;
}

// --- resample: scope pixel -> lat/lon -> mercator world pixel -> mosaic sample ---
//
// Done a band of rows at a time. Composing all 466x466 pixels in one call ran for
// several seconds without yielding - the ESP32-S3 has no double-precision FPU, so every
// sqrt/asin/atan2/log here is emulated in software - which starved IDLE0 and let the
// task watchdog reboot the device. That is a hard reboot loop the moment the basemap is
// switched on, so the loop must hand the CPU back regularly.
#define MAP_COMPOSE_ROWS 24        // ~11k pixels per call; comfortably inside the 5 s WDT

static void compose_band(int y0, int rows) {
    uint16_t *dst = map_bg_back_buffer();
    if (!dst || !s_mosaic) return;

    const float R = 6371.0088f;
    const float lat1 = (float)(s_lat * M_PI / 180.0);
    const float lon1 = (float)(s_lon * M_PI / 180.0);
    const float sinLat1 = sinf(lat1), cosLat1 = cosf(lat1);
    const float ws = (float)world_size(s_zoom);
    const float originX = (float)s_tx0 * MAP_TILE_PX;
    const float originY = (float)s_ty0 * MAP_TILE_PX;
    const float kmPerPx = s_rangeKm / RADAR_R_OUTER_PX;
    const int cx = SCREEN_CX, cy = SCREEN_CY;
    const long rMax = (long)RADAR_R_OUTER_PX * RADAR_R_OUTER_PX;

    const int yEnd = (y0 + rows > MAP_BG_SIZE) ? MAP_BG_SIZE : (y0 + rows);
    for (int py = y0; py < yEnd; ++py) {
        const float dy = (float)(py - cy);
        uint16_t *row = dst + (size_t)py * MAP_BG_SIZE;
        for (int px = 0; px < MAP_BG_SIZE; ++px) {
            const float dx = (float)(px - cx);
            const float rr = dx * dx + dy * dy;
            if (rr > (float)rMax) { row[px] = 0; continue; }      // outside the scope circle

            const float rPx = sqrtf(rr);
            const float d = (rPx * kmPerPx) / R;          // angular distance
            const float brg = atan2f(dx, -dy);   // screen up = north

            const float sinD = sinf(d), cosD = cosf(d);
            const float lat2 = asinf(sinLat1 * cosD + cosLat1 * sinD * cosf(brg));
            const float lon2 = lon1 + atan2f(sinf(brg) * sinD * cosLat1,
                                             cosD - sinLat1 * sinf(lat2));

            float wx = (lon2 * 180.0f / (float)M_PI + 180.0f) / 360.0f * ws;
            const float sl = sinf(lat2);
            float wy = (0.5f - logf((1.0f + sl) / (1.0f - sl)) / (4.0f * (float)M_PI)) * ws;

            int mx = (int)(wx - originX);
            int my = (int)(wy - originY);
            if (mx < 0 || mx >= s_mosaicW || my < 0 || my >= s_mosaicH) { row[px] = 0; continue; }

            // Store the tile at full brightness. Visibility is applied later as LVGL
            // image alpha, NOT baked in here.
            //
            // Baking it was destructive and could not be undone: composing at 0% wrote a
            // black image, and raising the slider afterwards could not restore it because
            // the source mosaic is freed once a build completes. That is exactly why the
            // slider "worked" at 0% and then left the map dark at 100%.
            row[px] = s_mosaic[(size_t)my * s_mosaicW + mx];
        }
    }
}

bool map_client_step(void) {
    if (!s_enabled || s_state == MAP_IDLE) return false;
    if (WiFi.status() != WL_CONNECTED) return true;       // wait for the network

    if (s_state == MAP_FETCH) {
        const int total = s_nx * s_ny;
        if (s_tileIdx >= total) { s_state = MAP_COMPOSE; s_composeRow = 0; return true; }
        if (!fetch_one_tile(s_tileIdx)) ++s_failed;       // missing tile stays black
        ++s_tileIdx;
        if (s_tileIdx >= total) { s_state = MAP_COMPOSE; s_composeRow = 0; }
        return true;
    }

    // MAP_COMPOSE
    if (s_failed >= s_nx * s_ny) {                        // nothing downloaded at all
        Serial.println("[map] all tiles failed; keeping the previous background");
        s_state = MAP_IDLE;
        free_mosaic();
        return false;
    }

    // One band per call, so the task returns to its loop (and the scheduler) between
    // bands instead of monopolising CPU 0 until the watchdog fires.
    compose_band(s_composeRow, MAP_COMPOSE_ROWS);
    s_composeRow += MAP_COMPOSE_ROWS;
    if (s_composeRow < MAP_BG_SIZE) return true;          // more bands to go

    map_bg_commit(s_lat, s_lon, s_rangeKm);
    s_haveLat = s_lat; s_haveLon = s_lon; s_haveRange = s_rangeKm;
    s_state = MAP_IDLE;
    free_mosaic();                                        // the composed image is all we keep
    Serial.printf("[map] background ready (z=%d, %d tile(s) missing)\n", s_zoom, s_failed);
    return false;
}
