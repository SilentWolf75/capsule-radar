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

    const double EDGE = 1.08;                       // include a touch past the rim, then clip
    const double rangeDeg  = rangeKm / 111.0;
    const double latMargin = rangeDeg * 1.20;
    const double cosLat    = cos(homeLat * M_PI / 180.0);
    const double lonMargin = latMargin / (cosLat < 0.15 ? 0.15 : cosLat);

    const int16_t *p = COAST_PTS;
    for (int poly = 0; poly < COAST_NUM_POLYS; ++poly) {
        const int n = COAST_POLY_LEN[poly];
        std::vector<lv_point_t> run;
        for (int i = 0; i < n; ++i) {
            const double lat = p[i * 2]     / (double)COAST_SCALE;
            const double lon = p[i * 2 + 1] / (double)COAST_SCALE;
            const double dlon = lon - homeLon;
            // cheap bounding-box reject (no trig) discards ~99% of the planet instantly;
            // the second dlon test wraps the antimeridian so e.g. home near 179E still works.
            const bool out = (fabs(lat - homeLat) > latMargin) ||
                             (fabs(dlon) > lonMargin && fabs(fabs(dlon) - 360.0) > lonMargin);
            if (!out) {
                const double dist = geo::haversineKm(homeLat, homeLon, lat, lon);
                if (dist <= rangeKm * EDGE) {
                    const double brg = geo::bearingDeg(homeLat, homeLon, lat, lon);
                    const double rPx = (dist / rangeKm) * rOuterPx;
                    const double a   = brg * M_PI / 180.0;
                    lv_point_t sp;
                    sp.x = (lv_coord_t)lroundf((float)(cx + rPx * sin(a)));
                    sp.y = (lv_coord_t)lroundf((float)(cy - rPx * cos(a)));
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
    const double midLat = (south + north) * 0.5, midLon = (west + east) * 0.5;
    double k = cos(midLat * M_PI / 180.0);
    if (k < 0.2) k = 0.2;
    const double wDeg = (east - west) * k, hDeg = north - south;
    const double sx = 2.0 * halfW / wDeg, sy = 2.0 * halfH / hDeg;
    const double s = (sx < sy) ? sx : sy;

    const int16_t *p = COAST_PTS;
    for (int poly = 0; poly < COAST_NUM_POLYS; ++poly) {
        const int n = COAST_POLY_LEN[poly];
        std::vector<lv_point_t> run;
        for (int i = 0; i < n; ++i) {
            const double lat = p[i * 2]     / (double)COAST_SCALE;
            const double lon = p[i * 2 + 1] / (double)COAST_SCALE;
            // Margin so coastlines entering the view are not clipped mid-stroke.
            if (lat < south - 4 || lat > north + 4 || lon < west - 6 || lon > east + 6) {
                if (run.size() > 1) s_rectLines.push_back(run);
                run.clear();
                continue;
            }
            lv_point_t sp;
            sp.x = (lv_coord_t)lround(cx + (lon - midLon) * k * s);
            sp.y = (lv_coord_t)lround(cy - (lat - midLat) * s);
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
