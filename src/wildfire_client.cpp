// NASA FIRMS active-fire detections (VIIRS, near real time).
//   GET /api/area/csv/{MAP_KEY}/VIIRS_SNPP_NRT/{w},{s},{e},{n}/1
// Returns a small CSV; we parse the header row for the column positions we need
// (FIRMS has changed column order between products, so fixed indices are fragile).
// Device-only. Needs a free MAP_KEY, so the feature stays off until one is entered.
#include "wildfire_client.h"
#include "wildfire.h"
#include "net_fetch.h"
#include "config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

static char s_key[40] = "";

void wildfire_set_key(const char *mapKey) {
    snprintf(s_key, sizeof(s_key), "%s", mapKey ? mapKey : "");
}

bool wildfire_has_key(void) { return s_key[0] != 0; }

// Return the index of `name` among the comma-separated header fields, or -1.
static int csv_col(const char *header, size_t len, const char *name) {
    const size_t nlen = strlen(name);
    int col = 0;
    size_t i = 0;
    while (i <= len) {
        size_t j = i;
        while (j < len && header[j] != ',') ++j;
        size_t flen = j - i;
        while (flen && (header[i + flen - 1] == '\r' || header[i + flen - 1] == ' ')) --flen;
        if (flen == nlen && strncasecmp(header + i, name, nlen) == 0) return col;
        if (j >= len) break;
        i = j + 1;
        ++col;
    }
    return -1;
}

// Copy field `want` of a CSV line into `out`. Returns false if the line is too short.
static bool csv_field(const char *line, size_t len, int want, char *out, size_t on) {
    int col = 0;
    size_t i = 0;
    while (i <= len) {
        size_t j = i;
        while (j < len && line[j] != ',') ++j;
        if (col == want) {
            size_t flen = j - i;
            if (flen >= on) flen = on - 1;
            memcpy(out, line + i, flen);
            out[flen] = 0;
            return true;
        }
        if (j >= len) break;
        i = j + 1;
        ++col;
    }
    return false;
}

bool wildfire_fetch(double lat, double lon, float radiusKm) {
    if (!s_key[0] || WiFi.status() != WL_CONNECTED) return false;

    // Bounding box around the scope. FIRMS wants west,south,east,north in degrees.
    const double dLat = radiusKm / 111.0;
    const double cosLat = cos(lat * M_PI / 180.0);
    const double dLon = dLat / (cosLat < 0.15 ? 0.15 : cosLat);
    char url[224];
    snprintf(url, sizeof(url),
             "https://firms.modaps.eosdis.nasa.gov/api/area/csv/%s/VIIRS_SNPP_NRT/"
             "%.3f,%.3f,%.3f,%.3f/1",
             s_key, lon - dLon, lat - dLat, lon + dLon, lat + dLat);

    uint8_t *body = nullptr; size_t blen = 0;
    if (!net_fetch_psram(url, ADSB_USER_AGENT, &body, &blen, 49152, 3500, 8000)) {
        Serial.println("[fire] fetch failed");
        return false;
    }
    const char *txt = (const char *)body;

    // header row
    size_t p = 0;
    while (p < blen && txt[p] != '\n') ++p;
    if (p == 0 || p >= blen) { heap_caps_free(body); Serial.println("[fire] empty response"); return false; }
    const int cLat  = csv_col(txt, p, "latitude");
    const int cLon  = csv_col(txt, p, "longitude");
    const int cFrp  = csv_col(txt, p, "frp");
    const int cConf = csv_col(txt, p, "confidence");
    if (cLat < 0 || cLon < 0) {
        heap_caps_free(body);
        Serial.println("[fire] unexpected CSV header");
        return false;
    }

    WildfirePoint pts[WILDFIRE_MAX];
    int n = 0;
    ++p;                                   // past the newline
    while (p < blen && n < WILDFIRE_MAX) {
        size_t e = p;
        while (e < blen && txt[e] != '\n') ++e;
        const size_t len = (e > p && txt[e - 1] == '\r') ? (e - p - 1) : (e - p);
        if (len > 4) {
            char f[24];
            WildfirePoint w;
            w.frp = 0.0f;
            w.conf = 0;
            if (csv_field(txt + p, len, cLat, f, sizeof(f))) w.lat = atof(f); else { p = e + 1; continue; }
            if (csv_field(txt + p, len, cLon, f, sizeof(f))) w.lon = atof(f); else { p = e + 1; continue; }
            if (cFrp >= 0 && csv_field(txt + p, len, cFrp, f, sizeof(f))) w.frp = atof(f);
            if (cConf >= 0 && csv_field(txt + p, len, cConf, f, sizeof(f))) {
                // VIIRS reports l/n/h; MODIS reports a percentage.
                if (f[0] == 'h' || f[0] == 'H')      w.conf = 90;
                else if (f[0] == 'n' || f[0] == 'N') w.conf = 60;
                else if (f[0] == 'l' || f[0] == 'L') w.conf = 25;
                else                                 w.conf = (uint8_t)atoi(f);
            }
            if (w.conf >= 25 && (w.lat != 0.0f || w.lon != 0.0f)) pts[n++] = w;
        }
        p = e + 1;
    }
    heap_caps_free(body);

    wildfire_store(pts, n);
    Serial.printf("[fire] %d active detections within %.0f km\n", n, radiusKm);
    return true;
}
