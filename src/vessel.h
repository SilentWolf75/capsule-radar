#pragma once
// Marine AIS contacts (aisstream.io) plotted alongside the aircraft.
//
// Portable half: an MMSI-keyed contact table written by the AIS client on the network
// core and read by the UI, plus projection and drawing for the scope (mirrors
// wildfire.*). Vessels move slowly, so entries persist until they go stale.
#include <lvgl.h>
#include <stddef.h>
#include <stdint.h>

#define VESSEL_MAX 40
#define VESSEL_STALE_MS 900000UL   // 15 min without an update -> drop the contact

struct Vessel {
    uint32_t mmsi;
    double   lat, lon;
    float    sogKt;      // speed over ground, knots (NaN if unknown)
    float    cogDeg;     // course over ground (NaN if unknown)
    char     name[21];   // AIS ShipName is up to 20 chars
    uint32_t updatedMs;
};

// Network task: insert or update one contact (keyed by MMSI).
void vessel_upsert(const Vessel &v);
void vessel_expire(uint32_t nowMs);   // drop contacts older than VESSEL_STALE_MS
void vessel_clear(void);
int  vessel_count(void);

// UI thread. Cheap to call every poll; only re-projects when something changed.
bool vessel_project(double homeLat, double homeLon, double rangeKm,
                    float cx, float cy, float rOuterPx);
void vessel_draw(lv_draw_ctx_t *ctx, lv_opa_t opa);

// Flattened contact for the detail card.
struct VesselInfo {
    char     name[21];
    uint32_t mmsi;
    float    sogKt, cogDeg;
    float    distKm, bearingDeg;
};
bool vessel_hit_test(int x, int y, int maxPx, VesselInfo *out);

// Contacts currently inside the scope, nearest first (for the list view).
int  vessel_visible_count(void);
bool vessel_visible_info(int idx, VesselInfo *out);
