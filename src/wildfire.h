#pragma once
// Active-fire markers for the radar scope (NASA FIRMS).
// Portable half: holds the detection list (written by the network task, read by the
// UI under a mutex), projects it to screen like airports.*, and draws the markers.
// Projection is recomputed only when the scope geometry changes, never per frame.
#include <lvgl.h>
#include <stddef.h>

#define WILDFIRE_MAX 64

struct WildfirePoint {
    float lat, lon;
    float frp;        // fire radiative power, MW — marker size/intensity
    uint8_t conf;     // confidence 0..100 (FIRMS nominal/high mapped to a number)
};

// Network task: replace the detection set (n is clamped to WILDFIRE_MAX).
void wildfire_store(const WildfirePoint *pts, int n);
int  wildfire_count();

// UI thread. Cheap to call every update: returns false (and does nothing) unless the
// scope geometry moved or fresh detections arrived, so the caller can skip repainting.
bool wildfire_project(double homeLat, double homeLon, double rangeKm,
                      float cx, float cy, float rOuterPx);
void wildfire_draw(lv_draw_ctx_t *ctx, lv_opa_t opa);

// Nearest detection to a tap, within `maxPx`. Returns false if none is close enough.
bool wildfire_hit_test(int x, int y, int maxPx, WildfirePoint *out, float *distKm);
