#include "coastline.h"
#include "coastline_data.h"
#include "geo.h"
#include <vector>
#include <math.h>

// Cached screen-space polylines for the current scope. Rebuilt only when the
// home position or range changes (a handful of times per session), so the per-
// frame render cost is zero — the chrome layer just repaints what's here.
static std::vector<std::vector<lv_point_t>> s_lines;

void coastline_project(double homeLat, double homeLon, double rangeKm,
                       float cx, float cy, float rOuterPx) {
    s_lines.clear();
    if (rangeKm <= 0) return;

    const float EDGE = 1.08f;                       // include a touch past the rim, then clip
    const float rangeDeg  = (float)rangeKm / 111.0f;
    const float latMargin = rangeDeg * 1.20f;
    const float cosLat    = cosf((float)(homeLat * M_PI / 180.0));
    const float lonMargin = latMargin / (cosLat < 0.15f ? 0.15f : cosLat);

    const int16_t *p = COAST_PTS;
    for (int poly = 0; poly < COAST_NUM_POLYS; ++poly) {
        const int n = COAST_POLY_LEN[poly];
        std::vector<lv_point_t> run;
        for (int i = 0; i < n; ++i) {
            const float lat = (float)p[i * 2]     / (float)COAST_SCALE;
            const float lon = (float)p[i * 2 + 1] / (float)COAST_SCALE;
            const float dlon = lon - (float)homeLon;
            // cheap bounding-box reject (no trig) discards ~99% of the planet instantly;
            // the second dlon test wraps the antimeridian so e.g. home near 179E still works.
            const bool out = (fabsf(lat - (float)homeLat) > latMargin) ||
                             (fabsf(dlon) > lonMargin && fabsf(fabsf(dlon) - 360.0f) > lonMargin);
            if (!out) {
                const float dist = geo::haversineKmf((float)homeLat, (float)homeLon, lat, lon);
                if (dist <= (float)rangeKm * EDGE) {
                    const float brg = geo::bearingDegf((float)homeLat, (float)homeLon, lat, lon);
                    const float rPx = (dist / (float)rangeKm) * rOuterPx;
                    const float a   = brg * (float)M_PI / 180.0f;
                    lv_point_t sp;
                    sp.x = (lv_coord_t)lroundf(cx + rPx * sinf(a));
                    sp.y = (lv_coord_t)lroundf(cy - rPx * cosf(a));
                    run.push_back(sp);
                    continue;
                }
            }
            if (run.size() >= 2) s_lines.push_back(std::move(run));   // flush the in-range run
            run.clear();
        }
        if (run.size() >= 2) s_lines.push_back(std::move(run));
        p += n * 2;
    }
}

void coastline_draw(lv_draw_ctx_t *ctx, lv_color_t color, lv_opa_t opa, lv_coord_t width) {
    if (s_lines.empty()) return;
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.color = color;
    d.width = width;
    d.opa   = opa;
    d.round_start = d.round_end = 1;   // smooth the joints on the thicker line
    for (const auto &line : s_lines) {
        for (size_t i = 1; i < line.size(); ++i) {
            lv_point_t a = line[i - 1];
            lv_point_t b = line[i];
            lv_draw_line(ctx, &d, &a, &b);
        }
    }
}

// --- rectangular projection (continental fire map) --------------------------------
// Separate cache from the radar projection above: both can be live at once, and this
// one only changes when the map view changes (a tap), not per frame.
static std::vector<std::vector<lv_point_t>> s_rectLines;

void coastline_project_rect(double west, double south, double east, double north,
                            lv_coord_t cx, lv_coord_t cy,
                            lv_coord_t halfW, lv_coord_t halfH) {
    s_rectLines.clear();
    if (east <= west || north <= south) return;

    // Longitude compressed by cos(mid-latitude), and the whole box fitted inside the
    // draw area preserving aspect - otherwise the continent renders as a wide smear.
    const float midLat = (float)((south + north) * 0.5), midLon = (float)((west + east) * 0.5);
    float k = cosf(midLat * (float)M_PI / 180.0f);
    if (k < 0.2f) k = 0.2f;
    const float wDeg = (float)(east - west) * k, hDeg = (float)(north - south);
    const float sx = 2.0f * halfW / wDeg, sy = 2.0f * halfH / hDeg;
    const float s = (sx < sy) ? sx : sy;

    const int16_t *p = COAST_PTS;
    for (int poly = 0; poly < COAST_NUM_POLYS; ++poly) {
        const int n = COAST_POLY_LEN[poly];
        std::vector<lv_point_t> run;
        for (int i = 0; i < n; ++i) {
            const float lat = (float)p[i * 2]     / (float)COAST_SCALE;
            const float lon = (float)p[i * 2 + 1] / (float)COAST_SCALE;
            // Margin so coastlines entering the view are not clipped mid-stroke.
            if (lat < (float)south - 4.0f || lat > (float)north + 4.0f || lon < (float)west - 6.0f || lon > (float)east + 6.0f) {
                if (run.size() > 1) s_rectLines.push_back(run);
                run.clear();
                continue;
            }
            lv_point_t sp;
            sp.x = (lv_coord_t)lroundf(cx + (lon - midLon) * k * s);
            sp.y = (lv_coord_t)lroundf(cy - (lat - midLat) * s);
            // Decimate. At continental scale one pixel spans a third of a degree, so
            // consecutive source points land on top of each other: keeping them all
            // produced ~100k segments per frame, which stalled the render entirely.
            // Dropping anything within 2 px of the last kept point is invisible here and
            // cuts the work by more than an order of magnitude.
            if (!run.empty()) {
                const lv_coord_t dx = (lv_coord_t)(sp.x - run.back().x);
                const lv_coord_t dy = (lv_coord_t)(sp.y - run.back().y);
                if (dx * dx + dy * dy < 4) continue;
            }
            run.push_back(sp);
        }
        if (run.size() > 1) s_rectLines.push_back(run);
        p += n * 2;
    }
}

void coastline_draw_rect(lv_draw_ctx_t *ctx, lv_color_t color, lv_opa_t opa, lv_coord_t width) {
    if (s_rectLines.empty()) return;
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.color = color; d.opa = opa; d.width = width;
    for (const auto &line : s_rectLines) {
        for (size_t i = 1; i < line.size(); ++i) {
            lv_point_t a = line[i - 1], b = line[i];
            lv_draw_line(ctx, &d, &a, &b);
        }
    }
}

#include "borders_data.h"

static std::vector<std::vector<lv_point_t>> s_borderLines;

void borders_project_rect(double west, double south, double east, double north,
                          lv_coord_t cx, lv_coord_t cy,
                          lv_coord_t halfW, lv_coord_t halfH) {
    s_borderLines.clear();
    if (east <= west || north <= south) return;

    const float midLat = (float)((south + north) * 0.5), midLon = (float)((west + east) * 0.5);
    float k = cosf(midLat * (float)M_PI / 180.0f);
    if (k < 0.2f) k = 0.2f;
    const float wDeg = (float)(east - west) * k, hDeg = (float)(north - south);
    const float sx = 2.0f * halfW / wDeg, sy = 2.0f * halfH / hDeg;
    const float s = (sx < sy) ? sx : sy;

    const int16_t *p = BORDER_PTS;
    for (int poly = 0; poly < BORDER_NUM_POLYS; ++poly) {
        const int n = BORDER_POLY_LEN[poly];
        std::vector<lv_point_t> run;
        for (int i = 0; i < n; ++i) {
            const float lat = (float)p[i * 2]     / (float)BORDER_SCALE;
            const float lon = (float)p[i * 2 + 1] / (float)BORDER_SCALE;
            if (lat < (float)south - 4.0f || lat > (float)north + 4.0f || lon < (float)west - 6.0f || lon > (float)east + 6.0f) {
                if (run.size() > 1) s_borderLines.push_back(run);
                run.clear();
                continue;
            }
            lv_point_t sp;
            sp.x = (lv_coord_t)lroundf(cx + (lon - midLon) * k * s);
            sp.y = (lv_coord_t)lroundf(cy - (lat - midLat) * s);
            if (!run.empty()) {
                const lv_coord_t dx = (lv_coord_t)(sp.x - run.back().x);
                const lv_coord_t dy = (lv_coord_t)(sp.y - run.back().y);
                if (dx * dx + dy * dy < 4) continue;
            }
            run.push_back(sp);
        }
        if (run.size() > 1) s_borderLines.push_back(run);
        p += n * 2;
    }
}

void borders_draw_rect(lv_draw_ctx_t *ctx, lv_color_t color, lv_opa_t opa, lv_coord_t width) {
    if (s_borderLines.empty()) return;
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.color = color; d.opa = opa; d.width = width;
    for (const auto &line : s_borderLines) {
        for (size_t i = 1; i < line.size(); ++i) {
            lv_point_t a = line[i - 1], b = line[i];
            lv_draw_line(ctx, &d, &a, &b);
        }
    }
}
