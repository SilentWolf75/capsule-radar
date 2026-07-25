#include "airline.h"
#include "airlines_data.h"
#include <mutex>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#if defined(ESP_PLATFORM)
#include <esp_heap_caps.h>
#endif

// Logo canvas. Wide and short: airline marks are banner-shaped, and this sits in the
// detail card's title row, so it must not crowd the callsign.
#define AL_MAXW 108
#define AL_MAXH 30

static std::mutex  s_m;
static lv_color_t *s_buf = nullptr;
static char s_want[4] = "", s_doneIata[4] = "";
static int  s_w = 0, s_h = 0;
static bool s_ready = false;

bool airline_lookup(const char *callsign, char *iata, size_t in, char *name, size_t nn) {
    if (iata && in) iata[0] = 0;
    if (name && nn) name[0] = 0;
    if (!callsign) return false;

    // Take the leading 3 letters of the callsign — that is the ICAO operator code.
    char pre[4] = {0, 0, 0, 0};
    int j = 0;
    for (const char *p = callsign; *p && j < 3; ++p) {
        if (*p == ' ') continue;
        if (!isalpha((unsigned char)*p)) return false;   // registration, not an airline callsign
        pre[j++] = (char)toupper((unsigned char)*p);
    }
    if (j < 3) return false;

    for (int i = 0; i < AIRLINES_NUM; ++i) {
        if (memcmp(pre, AIRLINES[i].icao, 3) != 0) continue;
        if (iata && in) snprintf(iata, in, "%s", AIRLINES[i].iata);
        if (name && nn) snprintf(name, nn, "%s", AIRLINES[i].name);
        return true;
    }
    return false;
}

static lv_color_t *ensure_buf() {
    if (!s_buf) {
        const size_t sz = (size_t)AL_MAXW * AL_MAXH * sizeof(lv_color_t);
#if defined(ESP_PLATFORM)
        s_buf = (lv_color_t *)heap_caps_malloc(sz, MALLOC_CAP_SPIRAM);
#else
        s_buf = (lv_color_t *)malloc(sz);
#endif
    }
    return s_buf;
}

void airline_logo_request(const char *iata) {
    std::lock_guard<std::mutex> g(s_m);
    snprintf(s_want, sizeof(s_want), "%s", iata ? iata : "");
}

bool airline_logo_pending(char *o, size_t n) {
    std::lock_guard<std::mutex> g(s_m);
    if (s_want[0] && strcmp(s_want, s_doneIata) != 0) { snprintf(o, n, "%s", s_want); return true; }
    return false;
}

lv_color_t *airline_logo_buffer(int *mw, int *mh) {
    if (mw) *mw = AL_MAXW;
    if (mh) *mh = AL_MAXH;
    return ensure_buf();
}

void airline_logo_commit(int w, int h, const char *iata) {
    std::lock_guard<std::mutex> g(s_m);
    snprintf(s_doneIata, sizeof(s_doneIata), "%s", iata ? iata : "");
    s_w = w; s_h = h;
    s_ready = (w > 0 && h > 0);
}

bool airline_logo_get(const char *iata, int *w, int *h) {
    std::lock_guard<std::mutex> g(s_m);
    if (iata && s_ready && s_doneIata[0] && strcmp(iata, s_doneIata) == 0) {
        if (w) *w = s_w;
        if (h) *h = s_h;
        return true;
    }
    return false;
}
