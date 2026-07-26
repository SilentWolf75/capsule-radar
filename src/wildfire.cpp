#include "wildfire.h"
#include "geo.h"
#include <mutex>
#include <vector>
#include <math.h>
#include <string.h>

static std::mutex s_m;
static WildfirePoint s_pts[WILDFIRE_MAX];
static int  s_n = 0;
static bool s_dirty = false;          // new data since the last projection

// Projected markers (UI thread only).
struct FireMark { lv_point_t pos; float frp; float distKm; int src; };
static std::vector<FireMark> s_marks;
static double s_pLat = 1e9, s_pLon = 1e9;
static float  s_pRange = -1.0f;

void wildfire_store(const WildfirePoint *pts, int n) {
    std::lock_guard<std::mutex> g(s_m);
    if (n < 0) n = 0;
    if (n > WILDFIRE_MAX) n = WILDFIRE_MAX;
    if (pts && n) memcpy(s_pts, pts, (size_t)n * sizeof(WildfirePoint));
    s_n = n;
    s_dirty = true;
}

int wildfire_count() {
    std::lock_guard<std::mutex> g(s_m);
    return s_n;
}

bool wildfire_project(double homeLat, double homeLon, double rangeKm,
                      float cx, float cy, float rOuterPx) {
    WildfirePoint local[WILDFIRE_MAX];
    int n;
    {
        std::lock_guard<std::mutex> g(s_m);
        // Re-project when the scope moved OR fresh detections arrived; otherwise the
        // cached marker list is still correct and this is a no-op.
        if (!s_dirty && homeLat == s_pLat && homeLon == s_pLon && (float)rangeKm == s_pRange)
            return false;
        n = s_n;
        memcpy(local, s_pts, (size_t)n * sizeof(WildfirePoint));
        s_dirty = false;
    }
    s_pLat = homeLat; s_pLon = homeLon; s_pRange = (float)rangeKm;

    s_marks.clear();
    if (rangeKm <= 0) return true;
    for (int i = 0; i < n; ++i) {
        const float d = geo::haversineKmf((float)homeLat, (float)homeLon, (float)local[i].lat, (float)local[i].lon);
        if (d > (float)rangeKm) continue;                          // only inside the scope
        const float brg = geo::bearingDegf((float)homeLat, (float)homeLon, (float)local[i].lat, (float)local[i].lon);
        const float rPx = (d / (float)rangeKm) * rOuterPx;
        const float a = brg * (float)M_PI / 180.0f;
        FireMark m;
        m.pos.x = (lv_coord_t)lroundf(cx + rPx * sinf(a));
        m.pos.y = (lv_coord_t)lroundf(cy - rPx * cosf(a));
        m.frp = local[i].frp;
        m.distKm = d;
        m.src = i;
        s_marks.push_back(m);
    }
    return true;
}

void wildfire_draw(lv_draw_ctx_t *ctx, lv_opa_t opa) {
    if (s_marks.empty()) return;

    lv_draw_rect_dsc_t dot;
    lv_draw_rect_dsc_init(&dot);
    dot.radius = LV_RADIUS_CIRCLE;
    dot.bg_opa = opa;

    lv_draw_arc_dsc_t halo;
    lv_draw_arc_dsc_init(&halo);
    halo.width = 2;

    for (const FireMark &m : s_marks) {
        // Size and colour track fire radiative power: small yellow flickers up to
        // large deep-red hotspots. Deliberately off the altitude palette's greens.
        const bool big = m.frp >= 50.0f;
        const bool mid = m.frp >= 15.0f;
        const lv_coord_t r = big ? 5 : (mid ? 4 : 3);
        dot.bg_color = big ? lv_color_hex(0xFF3B1F)
                           : (mid ? lv_color_hex(0xFF7A18) : lv_color_hex(0xFFC24D));
        lv_area_t a = { (lv_coord_t)(m.pos.x - r), (lv_coord_t)(m.pos.y - r),
                        (lv_coord_t)(m.pos.x + r), (lv_coord_t)(m.pos.y + r) };
        lv_draw_rect(ctx, &dot, &a);

        halo.color = dot.bg_color;
        halo.opa = (lv_opa_t)(opa / 3);
        lv_draw_arc(ctx, &halo, &m.pos, (uint16_t)(r + 4), 0, 360);
    }
}

bool wildfire_hit_test(int x, int y, int maxPx, WildfirePoint *out, float *distKm) {
    int best = -1;
    long bestD = (long)maxPx * maxPx;
    for (size_t i = 0; i < s_marks.size(); ++i) {
        const long dx = (long)s_marks[i].pos.x - x;
        const long dy = (long)s_marks[i].pos.y - y;
        const long dd = dx * dx + dy * dy;
        if (dd <= bestD) { bestD = dd; best = (int)i; }
    }
    if (best < 0) return false;
    if (distKm) *distKm = s_marks[best].distKm;
    if (out) {
        std::lock_guard<std::mutex> g(s_m);
        const int src = s_marks[best].src;
        if (src >= 0 && src < s_n) *out = s_pts[src];
        else return false;
    }
    return true;
}
