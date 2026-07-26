#pragma once
// World coastline background for the radar scope.
// Project the embedded Natural Earth coastline (coastline_data.h) into screen
// polylines for the current scope, then draw them under the aircraft. Projection
// is done once per home/range change (cheap bbox cull + great-circle), never per
// frame; drawing happens inside the static chrome layer's DRAW_MAIN callback.
#include <lvgl.h>

void coastline_project(double homeLat, double homeLon, double rangeKm,
                       float cx, float cy, float rOuterPx);

void coastline_draw(lv_draw_ctx_t *ctx, lv_color_t color, lv_opa_t opa, lv_coord_t width);

// Second projection, for the continental fire map: a plain lat/lon box rather than a
// scope centred on the user. Same embedded data, so the map costs no flash and no
// network. Cached separately from the radar's projection - the two coexist.
void coastline_project_rect(double west, double south, double east, double north,
                            lv_coord_t cx, lv_coord_t cy,
                            lv_coord_t halfW, lv_coord_t halfH);
void coastline_draw_rect(lv_draw_ctx_t *ctx, lv_color_t color, lv_opa_t opa, lv_coord_t width);
