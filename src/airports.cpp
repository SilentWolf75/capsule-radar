#include "airports.h"
#include "airports_data.h"
#include "geo.h"
#include <vector>
#include <math.h>
#include <string.h>

struct Apt { lv_point_t pos; char iata[6]; uint8_t large; };
static std::vector<Apt> s_apts;

// Label everything when the scope isn't crowded. Zoomed in, the nearby GA field is
// exactly what you want named; zoomed out, labelling 40 airports is unreadable, so
// only the large ones keep their tag.
#define APT_LABEL_ALL_MAX 14

void airports_project(double homeLat, double homeLon, double rangeKm,
                      float cx, float cy, float rOuterPx) {
    s_apts.clear();
    if (rangeKm <= 0) return;

    const double rangeDeg  = rangeKm / 111.0;
    const double latMargin = rangeDeg * 1.05;
    const double cosLat    = cos(homeLat * M_PI / 180.0);
    const double lonMargin = latMargin / (cosLat < 0.15 ? 0.15 : cosLat);

    for (int i = 0; i < AIRPORT_NUM; ++i) {
        const double lat = AIRPORT_LAT[i] / (double)AIRPORT_SCALE;
        const double lon = AIRPORT_LON[i] / (double)AIRPORT_SCALE;
        const double dlon = lon - homeLon;
        if (fabs(lat - homeLat) > latMargin) continue;                      // cheap bbox reject
        if (fabs(dlon) > lonMargin && fabs(fabs(dlon) - 360.0) > lonMargin) continue;
        const double dist = geo::haversineKm(homeLat, homeLon, lat, lon);
        if (dist > rangeKm) continue;                                       // only inside the scope
        const double brg = geo::bearingDeg(homeLat, homeLon, lat, lon);
        const double rPx = (dist / rangeKm) * rOuterPx;
        const double a   = brg * M_PI / 180.0;
        Apt ap;
        ap.pos.x = (lv_coord_t)lroundf((float)(cx + rPx * sin(a)));
        ap.pos.y = (lv_coord_t)lroundf((float)(cy - rPx * cos(a)));
        memcpy(ap.iata, AIRPORT_IATA[i], sizeof(ap.iata));
        ap.iata[sizeof(ap.iata) - 1] = 0;
        ap.large = AIRPORT_LARGE[i];
        s_apts.push_back(ap);
    }
}

void airports_draw(lv_draw_ctx_t *ctx, lv_color_t color, lv_opa_t opa,
                   lv_color_t labelColor, lv_opa_t labelOpa) {
    if (s_apts.empty()) return;

    lv_draw_arc_dsc_t ring;
    lv_draw_arc_dsc_init(&ring);
    ring.color = color; ring.width = 2; ring.opa = opa;

    lv_draw_rect_dsc_t dot;
    lv_draw_rect_dsc_init(&dot);
    dot.bg_color = color; dot.bg_opa = opa; dot.radius = LV_RADIUS_CIRCLE;

    lv_draw_label_dsc_t lbl;
    lv_draw_label_dsc_init(&lbl);
    lbl.color = labelColor; lbl.opa = labelOpa; lbl.font = &lv_font_montserrat_14;

    const bool labelAll = (s_apts.size() <= APT_LABEL_ALL_MAX);
    for (const Apt &ap : s_apts) {
        if (ap.large) {
            lv_draw_arc(ctx, &ring, &ap.pos, 3, 0, 360);                    // small hollow ring
        } else {
            lv_area_t d = { (lv_coord_t)(ap.pos.x - 2), (lv_coord_t)(ap.pos.y - 2),
                            (lv_coord_t)(ap.pos.x + 2), (lv_coord_t)(ap.pos.y + 2) };
            lv_draw_rect(ctx, &dot, &d);                                    // small marker
        }
        if (ap.iata[0] && (ap.large || labelAll)) {
            lv_area_t la = { (lv_coord_t)(ap.pos.x + 5), (lv_coord_t)(ap.pos.y - 7),
                             (lv_coord_t)(ap.pos.x + 52), (lv_coord_t)(ap.pos.y + 7) };
            lv_draw_label(ctx, &lbl, &la, ap.iata, NULL);
        }
    }
}

bool airports_nearest_iata(double lat, double lon, float maxKm,
                           char iata[6], float *distKm, float *bearingDeg) {
    if (iata) iata[0] = 0;
    // Two passes: a large airport is the useful landmark for the weather view's
    // "nearest field" line. Now that small GA strips are in the dataset, a single
    // nearest-any search would answer with whatever grass runway is down the road.
    double best = maxKm;
    int bestIdx = -1;
    for (int pass = 0; pass < 2 && bestIdx < 0; ++pass) {
        best = maxKm;
        for (int i = 0; i < AIRPORT_NUM; ++i) {
            if (pass == 0 && !AIRPORT_LARGE[i]) continue;
            if (!AIRPORT_IATA[i][0]) continue;
            const double alat = AIRPORT_LAT[i] / (double)AIRPORT_SCALE;
            const double alon = AIRPORT_LON[i] / (double)AIRPORT_SCALE;
            const double d = geo::haversineKm(lat, lon, alat, alon);
            if (d < best) { best = d; bestIdx = i; }
        }
    }
    if (bestIdx < 0) return false;
    if (iata) { memcpy(iata, AIRPORT_IATA[bestIdx], 6); iata[5] = 0; }
    if (distKm) *distKm = (float)best;
    if (bearingDeg) {
        const double alat = AIRPORT_LAT[bestIdx] / (double)AIRPORT_SCALE;
        const double alon = AIRPORT_LON[bestIdx] / (double)AIRPORT_SCALE;
        *bearingDeg = (float)geo::bearingDeg(lat, lon, alat, alon);
    }
    return true;
}
