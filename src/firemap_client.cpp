#include "firemap_client.h"
#include "firemap.h"
#include "wildfire_client.h"    // wildfire_has_key() / wildfire_map_key()
#include "net_fetch.h"
#include "config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <stdlib.h>
#include <string.h>

// Rows parsed per call. A continent-wide MODIS query is a few thousand rows; doing them
// all in one pass would hold CPU 0 long enough for the task watchdog to reboot the
// device - the same failure the map basemap's compose() hit. Yield often.
#define FM_ROWS_PER_STEP 400

enum FmState { FM_IDLE, FM_PARSE };
static FmState  s_state = FM_IDLE;
static uint8_t *s_body = nullptr;
static size_t   s_len = 0, s_pos = 0;
static int      s_cLat = -1, s_cLon = -1, s_cFrp = -1, s_cConf = -1;
static int      s_rows = 0;
static uint32_t s_nextAt = 0;

// --- tiny CSV helpers (same shape as wildfire_client) ---
static int csv_col(const char *h, size_t len, const char *name) {
    int col = 0; size_t i = 0;
    while (i <= len) {
        size_t j = i;
        while (j < len && h[j] != ',') ++j;
        size_t flen = j - i;
        while (flen && (h[i + flen - 1] == '\r' || h[i + flen - 1] == ' ')) --flen;
        if (flen == strlen(name) && strncasecmp(h + i, name, flen) == 0) return col;
        if (j >= len) break;
        i = j + 1; ++col;
    }
    return -1;
}

static bool csv_field(const char *line, size_t len, int col, char *out, size_t on) {
    int c = 0; size_t i = 0;
    while (i <= len) {
        size_t j = i;
        while (j < len && line[j] != ',') ++j;
        if (c == col) {
            size_t flen = j - i;
            if (flen >= on) flen = on - 1;
            memcpy(out, line + i, flen);
            out[flen] = 0;
            return true;
        }
        if (j >= len) break;
        i = j + 1; ++c;
    }
    return false;
}

static void cleanup(void) {
    if (s_body) { heap_caps_free(s_body); s_body = nullptr; }
    s_len = s_pos = 0;
    s_state = FM_IDLE;
}

static bool start_fetch(void) {
    FireView v; firemap_get_view(&v);

    // MODIS over the continent, VIIRS once zoomed in. MODIS returns roughly a sixth of
    // the rows for the same area - plenty for an overview, and far kinder to a 3 MB
    // fetch budget. VIIRS has the resolution that matters when looking at one fire.
    const bool zoomed = firemap_is_zoomed();
    const char *src = zoomed ? "VIIRS_SNPP_NRT" : "MODIS_NRT";

    char url[288];
    snprintf(url, sizeof(url),
             "https://firms.modaps.eosdis.nasa.gov/api/area/csv/%s/%s/"
             "%.2f,%.2f,%.2f,%.2f/%d",
             wildfire_map_key(), src, v.west, v.south, v.east, v.north, FIRE_DAY_RANGE);

    firemap_set_status("fetching...");
    if (!net_fetch_psram(url, ADSB_USER_AGENT, &s_body, &s_len, 3145728, 5000, 20000)) {
        firemap_set_status("fetch failed");
        Serial.println("[firemap] fetch failed");
        return false;
    }

    // Header row: column order is not guaranteed, so look the fields up by name.
    const char *txt = (const char *)s_body;
    size_t p = 0;
    while (p < s_len && txt[p] != '\n') ++p;
    if (p == 0 || p >= s_len) {
        firemap_set_status("empty response");
        cleanup();
        return false;
    }
    s_cLat  = csv_col(txt, p, "latitude");
    s_cLon  = csv_col(txt, p, "longitude");
    s_cFrp  = csv_col(txt, p, "frp");
    s_cConf = csv_col(txt, p, "confidence");
    if (s_cLat < 0 || s_cLon < 0) {
        firemap_set_status("unexpected CSV");
        cleanup();
        return false;
    }

    s_pos = p + 1;
    s_rows = 0;
    firemap_bin_reset();
    s_state = FM_PARSE;
    Serial.printf("[firemap] %s %.0fx%.0f deg, %u KB\n", src,
                  v.east - v.west, v.north - v.south, (unsigned)(s_len / 1024));
    return true;
}

static void parse_band(void) {
    const char *txt = (const char *)s_body;
    int done = 0;
    while (s_pos < s_len && done < FM_ROWS_PER_STEP) {
        size_t e = s_pos;
        while (e < s_len && txt[e] != '\n') ++e;
        const size_t len = (e > s_pos && txt[e - 1] == '\r') ? (e - s_pos - 1) : (e - s_pos);
        if (len > 4) {
            char f[24];
            float lat = 0, lon = 0, frp = 0;
            int conf = 100;
            bool ok = csv_field(txt + s_pos, len, s_cLat, f, sizeof(f));
            if (ok) { lat = atof(f); ok = csv_field(txt + s_pos, len, s_cLon, f, sizeof(f)); }
            if (ok) {
                lon = atof(f);
                if (s_cFrp >= 0 && csv_field(txt + s_pos, len, s_cFrp, f, sizeof(f))) frp = atof(f);
                if (s_cConf >= 0 && csv_field(txt + s_pos, len, s_cConf, f, sizeof(f))) {
                    if (f[0] == 'h' || f[0] == 'H')      conf = 90;
                    else if (f[0] == 'n' || f[0] == 'N') conf = 60;
                    else if (f[0] == 'l' || f[0] == 'L') conf = 25;
                    else                                 conf = atoi(f);
                }
                if (conf >= 25 && (lat != 0.0f || lon != 0.0f)) {
                    firemap_bin_add(lat, lon, frp);
                    ++s_rows;
                }
            }
        }
        s_pos = e + 1;
        ++done;
    }

    if (s_pos < s_len) return;                    // more bands to go

    firemap_bin_commit(s_rows);
    const int cells = firemap_cell_count();
    char st[64];
    snprintf(st, sizeof(st), "%d detections in %d areas", s_rows, cells);
    firemap_set_status(st);
    Serial.printf("[firemap] %d detections -> %d cells\n", s_rows, cells);
    firemap_mark_fetched();
    cleanup();
}

bool firemap_client_step(void) {
    if (WiFi.status() != WL_CONNECTED) return false;
    if (!wildfire_has_key()) return false;

    if (s_state == FM_PARSE) { parse_band(); return true; }

    if (firemap_needs_fetch() && (int32_t)(millis() - s_nextAt) >= 0) {
        s_nextAt = millis() + FIREMAP_REFRESH_MS;
        if (!start_fetch()) firemap_mark_fetched();   // don't spin on a failure
        return true;
    }
    return false;
}
