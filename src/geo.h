#pragma once
// Pure geo math — complete, framework-independent. See docs/DATA_SOURCE.md.
#include <math.h>

namespace geo {

inline double deg2rad(double d) { return d * M_PI / 180.0; }
inline double rad2deg(double r) { return r * 180.0 / M_PI; }

inline float deg2radf(float d) { return d * (float)M_PI / 180.0f; }
inline float rad2degf(float r) { return r * 180.0f / (float)M_PI; }

// Great-circle distance in kilometers.
inline double haversineKm(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371.0088;
    double dlat = deg2rad(lat2 - lat1);
    double dlon = deg2rad(lon2 - lon1);
    double a = sin(dlat / 2) * sin(dlat / 2) +
               cos(deg2rad(lat1)) * cos(deg2rad(lat2)) *
               sin(dlon / 2) * sin(dlon / 2);
    return 2 * R * atan2(sqrt(a), sqrt(1 - a));
}

// Great-circle distance in kilometers (single precision, FPU-accelerated)
inline float haversineKmf(float lat1, float lon1, float lat2, float lon2) {
    const float R = 6371.0088f;
    float dlat = deg2radf(lat2 - lat1);
    float dlon = deg2radf(lon2 - lon1);
    float a = sinf(dlat / 2.0f) * sinf(dlat / 2.0f) +
              cosf(deg2radf(lat1)) * cosf(deg2radf(lat2)) *
              sinf(dlon / 2.0f) * sinf(dlon / 2.0f);
    return 2.0f * R * atan2f(sqrtf(a), sqrtf(1.0f - a));
}

// Initial bearing from point 1 to point 2, degrees clockwise from true north [0,360).
inline double bearingDeg(double lat1, double lon1, double lat2, double lon2) {
    double y = sin(deg2rad(lon2 - lon1)) * cos(deg2rad(lat2));
    double x = cos(deg2rad(lat1)) * sin(deg2rad(lat2)) -
               sin(deg2rad(lat1)) * cos(deg2rad(lat2)) * cos(deg2rad(lon2 - lon1));
    double b = rad2deg(atan2(y, x));
    return fmod(b + 360.0, 360.0);
}

// Initial bearing from point 1 to point 2, degrees clockwise from true north [0,360) (single precision, FPU-accelerated)
inline float bearingDegf(float lat1, float lon1, float lat2, float lon2) {
    float y = sinf(deg2radf(lon2 - lon1)) * cosf(deg2radf(lat2));
    float x = cosf(deg2radf(lat1)) * sinf(deg2radf(lat2)) -
              sinf(deg2radf(lat1)) * cosf(deg2radf(lat2)) * cosf(deg2radf(lon2 - lon1));
    float b = rad2degf(atan2f(y, x));
    return fmodf(b + 360.0f, 360.0f);
}

inline double kmToNm(double km) { return km * 0.539957; }

// Project a target to screen pixels (internally single-precision, FPU-accelerated)
struct Point { float x; float y; bool inRange; };
inline Point projectToScreen(double distKm, double bearingDeg, double rangeKm,
                             float cx, float cy, float rOuterPx, double rotationDeg = 0.0) {
    float rPx = (float)((distKm / rangeKm) * rOuterPx);
    bool inRange = (distKm <= rangeKm);
    if (!inRange) rPx = rOuterPx;                 // clamp to rim
    float ang = deg2radf((float)(bearingDeg - rotationDeg));
    Point p;
    p.x = cx + rPx * sinf(ang);
    p.y = cy - rPx * cosf(ang);           // screen Y grows downward; north is up
    p.inRange = inRange;
    return p;
}

} // namespace geo
