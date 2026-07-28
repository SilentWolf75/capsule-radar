// M3 UI: tileview (radar / list / stats) + tap-to-inspect detail card.
// Pure LVGL, portable. Taps hit-test via radar::hitTest; selection lives in radar.
#include "ui.h"
#include "radar_view.h"
#include "route.h"
#include "photo.h"
#include "airline.h"
#include "weather_icons.h"
#include "firemap.h"
#include "coastline.h"
#include "vessel.h"
#include "ais_client.h"
#include "wildfire.h"
#include "weather.h"
#include "wx_radar.h"
#include "cloud_image.h"
#include "airports.h"
#include "geo.h"
#include "config.h"
#include <lvgl.h>
#include <WiFi.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define UI_GREEN lv_color_hex(0x1DFF86)
#define UI_INK   lv_color_hex(0xEAFFF3)
#define UI_SOFT  lv_color_hex(0x9AFFC8)
#define UI_DIM   lv_color_hex(0x5F7A6C)
#define UI_PANEL lv_color_hex(0x0C160F)
#define UI_EMERG lv_color_hex(0xFF5A3C)
#define UI_MIL   lv_color_hex(0xC77DFF)   // matches the scope's military brackets
#define UI_AMBER lv_color_hex(0xFF9E0B)
#define UI_CYAN  lv_color_hex(0x00E5FF)

static lv_obj_t *s_tv = nullptr;
static lv_obj_t *s_tileRadar = nullptr, *s_tileList = nullptr, *s_tileStats = nullptr, *s_tileWeather = nullptr;
static lv_obj_t *s_tileClock = nullptr, *s_tileTracked = nullptr;
static lv_obj_t *s_trkTitle = nullptr, *s_trkRoute = nullptr, *s_trkBar = nullptr;
static lv_obj_t *s_trkFrom = nullptr, *s_trkTo = nullptr, *s_trkPct = nullptr;
static lv_obj_t *s_trkStats = nullptr, *s_trkEta = nullptr, *s_trkHint = nullptr;
static lv_obj_t *s_cardTrackBtn = nullptr, *s_cardTrackLbl = nullptr;
static char s_trackHex[8] = "";      // tracked contact (empty = nothing tracked)
static char s_trackCall[12] = "";
static lv_obj_t *s_clockTime = nullptr, *s_clockDate = nullptr, *s_clockSec = nullptr;
static lv_obj_t *s_clockArc = nullptr, *s_clockRing = nullptr, *s_clockRule = nullptr;
static lv_obj_t *s_clockTemp = nullptr, *s_clockCond = nullptr;
static lv_obj_t *s_clockDay[3] = { nullptr, nullptr, nullptr };
static lv_obj_t *s_clockDayTemp[3] = { nullptr, nullptr, nullptr };
static lv_obj_t *s_clockIcon = nullptr, *s_clockDayIcon[3] = { nullptr, nullptr, nullptr };
static lv_obj_t *s_fcIcon = nullptr, *s_fcDayIcon[3] = { nullptr, nullptr, nullptr };
static int s_iconPreview = -1;    // >=0 pins every weather glyph to this WMO code

// All weather glyphs go through here so the diagnostic override applies uniformly.
static void build_clock_weather(void);   // defined with the clock tile, below
static void set_wx_icon(lv_obj_t *o, int code, bool night) {
    weather_icon_set(o, s_iconPreview >= 0 ? s_iconPreview : code, night);
}
static lv_obj_t *s_card = nullptr, *s_cardTitle = nullptr, *s_cardL = nullptr, *s_cardR = nullptr;
static lv_obj_t *s_cardRoute = nullptr;
static lv_obj_t *s_cardAirline = nullptr;
static lv_obj_t *s_vesselCard = nullptr, *s_vesselTitle = nullptr, *s_vesselBody = nullptr;
static bool      s_vesselShown = false;
static lv_obj_t *s_photo = nullptr, *s_photoCredit = nullptr;   // aircraft photo above the card
static char s_lastRouteReq[12] = "";
static lv_obj_t *s_hudWifi = nullptr, *s_hudCount = nullptr, *s_hudClock = nullptr, *s_hudBatt = nullptr, *s_hudDate = nullptr;
static lv_obj_t *s_hudBars[4] = { nullptr, nullptr, nullptr, nullptr };   // WiFi signal-strength bars
static lv_obj_t *s_list = nullptr, *s_listTitle = nullptr;
static lv_obj_t *s_statsLbl = nullptr;
static lv_obj_t *s_statsNet = nullptr;
static lv_obj_t *s_hudGps   = nullptr;   // HUD satellite icon (hidden unless GPS auto-location is on)
static lv_obj_t *s_statsGps = nullptr;   // Stats view GPS status line
static lv_obj_t *s_weatherNow = nullptr, *s_weatherMeta = nullptr, *s_weatherDays = nullptr;
static lv_obj_t *s_wxCanvas = nullptr, *s_wxStatus = nullptr, *s_wxAirport = nullptr;
static lv_obj_t *s_wxFooter = nullptr, *s_wxMeta = nullptr, *s_wxAttrib = nullptr;
static lv_obj_t *s_wxRings[3] = { nullptr, nullptr, nullptr };
static lv_obj_t *s_wxNorth = nullptr, *s_wxCenter = nullptr, *s_wxRange = nullptr;
static lv_obj_t *s_weatherModeBtn = nullptr, *s_weatherModeLbl = nullptr;
static lv_obj_t *s_weatherTitle = nullptr;
enum WeatherViewMode { WEATHER_RADAR, WEATHER_CLOUDS, WEATHER_FORECAST };
static WeatherViewMode s_weatherMode = WEATHER_RADAR;
static lv_obj_t *s_fcCurrent = nullptr, *s_fcCondition = nullptr, *s_fcUpdated = nullptr;
static lv_obj_t *s_fcMetricName[3] = { nullptr, nullptr, nullptr };
static lv_obj_t *s_fcMetricValue[3] = { nullptr, nullptr, nullptr };
static lv_obj_t *s_fcDay[3] = { nullptr, nullptr, nullptr };
static lv_obj_t *s_fcDayCondition[3] = { nullptr, nullptr, nullptr };
static lv_obj_t *s_fcDayTemp[3] = { nullptr, nullptr, nullptr };
static lv_obj_t *s_fcDayRain[3] = { nullptr, nullptr, nullptr };

// --------------------------------------------------------------------- units
// 0 = Aviation (ft, kt, km) · 1 = Metric (m, km/h, km) · 2 = Imperial (ft, mph, mi).
// The feed gives altitude in ft, speed in kt, vertical speed in fpm, distance in km.
static int s_units = 0;
void ui_set_units(int u) { s_units = (u < 0 || u > 2) ? 0 : u; }

// ----------------------------------------------------------------- clock format
static bool s_time24 = false;
void ui_set_time_24h(bool on) { s_time24 = on; }
bool ui_time_24h(void) { return s_time24; }

// Rough day/night for icon choice: the crescent only appears outside daylight hours.
// Sunrise/sunset would be better, but the forecast feed doesn't carry them and a weather
// glyph isn't worth a solar-position calculation.
static bool is_night(void) {
    const time_t now = time(nullptr);
    if (now < 1000000000) return false;      // clock not set yet
    struct tm ti;
#if defined(ARDUINO) || defined(ESP_PLATFORM)
    localtime_r(&now, &ti);
#else
    ti = *localtime(&now);
#endif
    return ti.tm_hour < 6 || ti.tm_hour >= 20;
}

void ui_format_clock(char *buf, size_t n, int hour, int min, bool withSuffix) {
    if (s_time24) { snprintf(buf, n, "%02d:%02d", hour, min); return; }
    int h12 = hour % 12;
    if (h12 == 0) h12 = 12;                    // midnight and noon are both "12"
    if (withSuffix) snprintf(buf, n, "%d:%02d %s", h12, min, hour < 12 ? "AM" : "PM");
    else            snprintf(buf, n, "%d:%02d", h12, min);
}

// Accessibility: "large text" swaps every font one-or-two steps up. The flag must be
// set BEFORE ui_create() — fonts are baked into the widgets at creation time (the web
// toggle saves to NVS and reboots, so it always takes effect through this path).
static bool s_bigText = false;
void ui_set_large_text(bool on) { s_bigText = on; }
// UI_S() scales positions, but LVGL fonts are fixed sizes and cannot follow it. On a
// noticeably larger panel the layout would otherwise grow around text that stayed the
// same size, which reads as sparse and hard to see from a distance. Step the tier up
// once past a threshold rather than trying to interpolate: the available Montserrat
// sizes are discrete anyway.
#define UI_BIG_PANEL (SCREEN_W >= 600)
static const lv_font_t *F12() {
    if (UI_BIG_PANEL) return s_bigText ? &lv_font_montserrat_20 : &lv_font_montserrat_18;
    return s_bigText ? &lv_font_montserrat_16 : &lv_font_montserrat_12;
}
static const lv_font_t *F14() {
    if (UI_BIG_PANEL) return &lv_font_montserrat_20;
    return s_bigText ? &lv_font_montserrat_18 : &lv_font_montserrat_14;
}
static const lv_font_t *F16() {
    if (UI_BIG_PANEL) return s_bigText ? &lv_font_montserrat_28 : &lv_font_montserrat_20;
    return s_bigText ? &lv_font_montserrat_20 : &lv_font_montserrat_16;
}

static void fmt_alt(char *b, size_t n, float ft, bool gnd) {
    if (gnd)            snprintf(b, n, "GND");
    else if (s_units == 1) snprintf(b, n, "%.0f m",  ft * 0.3048f);
    else                snprintf(b, n, "%.0f ft", ft);
}
static void fmt_spd(char *b, size_t n, float kt) {
    if (kt != kt)          snprintf(b, n, "-");
    else if (s_units == 1) snprintf(b, n, "%.0f km/h", kt * 1.852f);
    else if (s_units == 2) snprintf(b, n, "%.0f mph",  kt * 1.15078f);
    else                   snprintf(b, n, "%.0f kt",   kt);
}
static void fmt_vs(char *b, size_t n, float fpm) {
    if (fpm != fpm)        snprintf(b, n, "-");
    else if (s_units == 1) snprintf(b, n, "%+.1f m/s", fpm * 0.00508f);
    else                   snprintf(b, n, "%+.0f fpm", fpm);
}
static float dist_val(float km) {
    if (s_units == 0) return km * 0.539957f;   // Aviation -> nautical miles
    if (s_units == 2) return km * 0.621371f;   // Imperial -> miles
    return km;                                   // Metric   -> km
}
static const char *dist_unit(void) { return s_units == 0 ? "nm" : (s_units == 2 ? "mi" : "km"); }
// The weather view's overlays are set in caps to match the aviation-style chrome.
static const char *dist_unit_caps(void) { return s_units == 0 ? "NM" : (s_units == 2 ? "MI" : "KM"); }

// Coverage radius of each imagery product. These are properties of the source data
// (RainViewer tile span, EUMETSAT crop), so they live in km and convert for display.
#define WX_RADAR_RANGE_KM   75.0f
#define WX_CLOUD_RANGE_KM  200.0f

static float weather_temp(float c) { return s_units == 2 ? c * 1.8f + 32.0f : c; }
static const char *weather_temp_unit(void) { return s_units == 2 ? "F" : "C"; }
static float weather_wind(float kmh) {
    if (s_units == 0) return kmh * 0.539957f;
    if (s_units == 2) return kmh * 0.621371f;
    return kmh;
}
static const char *weather_wind_unit(void) { return s_units == 0 ? "kt" : (s_units == 2 ? "mph" : "km/h"); }
static const char *cardinal(float deg) {
    static const char *p[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    int i = ((int)(deg + 22.5f) / 45) & 7;
    return p[i];
}

// Fold Latin-1 accents / drop any other non-ASCII so the Montserrat font never hits a
// missing glyph (which renders as an empty box). Belt-and-suspenders for card text.
static void fold_ascii(char *s) {
    char *o = s;
    for (unsigned char *p = (unsigned char *)s; *p; ) {
        if (*p < 0x80) { *o++ = (char)*p++; continue; }
        if (*p == 0xC3 && p[1]) {                       // Latin-1 Supplement (U+00C0..U+00FF)
            const unsigned char d = p[1];
            char r;
            if      (d >= 0x80 && d <= 0x85) r = 'A';
            else if (d >= 0xA0 && d <= 0xA5) r = 'a';
            else if (d == 0x87)              r = 'C';
            else if (d == 0xA7)              r = 'c';
            else if (d >= 0x88 && d <= 0x8B) r = 'E';
            else if (d >= 0xA8 && d <= 0xAB) r = 'e';
            else if (d >= 0x8C && d <= 0x8F) r = 'I';
            else if (d >= 0xAC && d <= 0xAF) r = 'i';
            else if (d == 0x91)              r = 'N';
            else if (d == 0xB1)              r = 'n';
            else if (d >= 0x92 && d <= 0x96) r = 'O';
            else if (d >= 0xB2 && d <= 0xB6) r = 'o';
            else if (d >= 0x99 && d <= 0x9C) r = 'U';
            else if (d >= 0xB9 && d <= 0xBC) r = 'u';
            else                             r = '?';
            *o++ = r; p += 2; continue;
        }
        ++p;                                            // skip other multibyte lead + continuation
        while (*p >= 0x80 && *p < 0xC0) ++p;
    }
    *o = 0;
}

// ----------------------------------------------------------------- detail card
static void refresh_card(void) {
    AcInfo in;
    if (s_vesselCard && !s_vesselShown) lv_obj_add_flag(s_vesselCard, LV_OBJ_FLAG_HIDDEN);
    if (!radar::selected(in)) {
        lv_obj_add_flag(s_card, LV_OBJ_FLAG_HIDDEN);
        if (s_photo)       lv_obj_add_flag(s_photo, LV_OBJ_FLAG_HIDDEN);
        if (s_photoCredit) lv_obj_add_flag(s_photoCredit, LV_OBJ_FLAG_HIDDEN);
        s_lastRouteReq[0] = 0;
        return;
    }

    // Operator identity, resolved offline from the callsign's ICAO prefix.
    //
    // This used to download the airline's logo. It no longer does: the only free,
    // keyless source (kiwi.com) has partial coverage and quietly substitutes its OWN
    // brand mark for airlines it lacks, so a Southwest flight displayed a Kiwi advert.
    // airhex wants a paid key. A wrong logo is worse than no logo, and the embedded
    // name table is instant, works offline, and saves a TLS handshake per tap.
    char alIata[4] = "", alName[32] = "";
    const bool haveAirline = airline_lookup(in.call, alIata, sizeof(alIata), alName, sizeof(alName));
    if (s_cardAirline) {
        if (haveAirline && alName[0]) {
            char an[36];
            snprintf(an, sizeof(an), "%s", alName);
            fold_ascii(an);
            lv_label_set_text(s_cardAirline, an);
            lv_obj_clear_flag(s_cardAirline, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_cardAirline, LV_OBJ_FLAG_HIDDEN);
        }
    }
    lv_obj_clear_flag(s_card, LV_OBJ_FLAG_HIDDEN);
    if (s_vesselCard) {                     // an aircraft selection replaces the vessel card
        lv_obj_add_flag(s_vesselCard, LV_OBJ_FLAG_HIDDEN);
        s_vesselShown = false;
    }

    // Registration is worth more than the squawk for spotting, so it goes in the title
    // row next to the callsign once the lookup lands.
    if (in.hex[0]) reg_request(in.hex);
    char reg[12] = "", regType[24] = "";
    const bool haveReg = in.hex[0] && reg_get(in.hex, reg, sizeof(reg), regType, sizeof(regType));

    char title[64];
    const char *typeStr = in.type[0] ? in.type : (haveReg && regType[0] ? regType : "");
    if (haveReg && reg[0] && typeStr[0])
        snprintf(title, sizeof(title), "%s  %s  %s", in.call[0] ? in.call : "-", reg, typeStr);
    else if (haveReg && reg[0])
        snprintf(title, sizeof(title), "%s  %s", in.call[0] ? in.call : "-", reg);
    else if (typeStr[0])
        snprintf(title, sizeof(title), "%s  %s", in.call[0] ? in.call : "-", typeStr);
    else
        snprintf(title, sizeof(title), "%s", in.call[0] ? in.call : "-");
    fold_ascii(title);
    lv_label_set_text(s_cardTitle, title);
    lv_obj_set_style_text_color(s_cardTitle,
        in.emergency ? UI_EMERG : (in.military ? UI_MIL : UI_INK), 0);

    char altS[16], vsS[24], spdS[16], sqS[16];
    fmt_alt(altS, sizeof(altS), in.altFt, in.onGround);
    fmt_vs (vsS,  sizeof(vsS),  in.vsFpm);
    fmt_spd(spdS, sizeof(spdS), in.gsKt);
    if (in.squawk < 0)          snprintf(sqS, sizeof(sqS), "-");
    else                        snprintf(sqS, sizeof(sqS), "%04d", in.squawk);

    char left[96], right[96];
    snprintf(left,  sizeof(left),  "ALT  %s\nSPD  %s\nDIST %.1f %s", altS, spdS, dist_val(in.distKm), dist_unit());
    snprintf(right, sizeof(right), "V/S  %s\nHDG  %03.0f\nSQK  %s%s", vsS, in.bearingDeg, sqS,
             in.military ? "  MIL" : "");
    lv_label_set_text(s_cardL, left);
    lv_label_set_text(s_cardR, right);

    if (s_cardTrackLbl) {
        const bool isTracked = s_trackHex[0] && strcmp(s_trackHex, in.hex) == 0;
        lv_label_set_text(s_cardTrackLbl, isTracked ? "TRACKING" : "TRACK");
        lv_obj_set_style_text_color(s_cardTrackLbl, isTracked ? UI_INK : UI_GREEN, 0);
        lv_obj_set_style_border_opa(s_cardTrackBtn, isTracked ? 255 : 120, 0);
    }

    // route (origin -> destination), looked up asynchronously by callsign
    if (in.call[0] && strcmp(in.call, s_lastRouteReq) != 0) {
        snprintf(s_lastRouteReq, sizeof(s_lastRouteReq), "%s", in.call);
        route_request(in.call);
    }
    char rfrom[40], rto[40];
    if (!in.call[0]) {
        lv_label_set_text(s_cardRoute, "Route -");                 // no callsign -> nothing to look up
    } else if (route_get(in.call, rfrom, sizeof(rfrom), rto, sizeof(rto))) {
        char rt[96];
        if (rfrom[0] || rto[0]) snprintf(rt, sizeof(rt), "%s -> %s", rfrom[0] ? rfrom : "?", rto[0] ? rto : "?");
        else                    snprintf(rt, sizeof(rt), "Route unavailable");
        fold_ascii(rt);
        lv_label_set_text(s_cardRoute, rt);
    } else {
        lv_label_set_text(s_cardRoute, "Looking up route...");     // pending: lookup in flight
    }

    // aircraft photo (planespotters), shown above the card when one is available
    if (in.hex[0]) photo_request(in.hex);
    int pw = 0, ph = 0; char pcred[40];
    if (s_photo && in.hex[0] && photo_get(in.hex, &pw, &ph, pcred, sizeof(pcred)) && pw > 0 && ph > 0) {
        int mw, mh;
        lv_color_t *pbuf = photo_buffer(&mw, &mh);
        lv_canvas_set_buffer(s_photo, pbuf, pw, ph, LV_IMG_CF_TRUE_COLOR);
        lv_obj_set_size(s_photo, pw, ph);
        lv_obj_align(s_photo, LV_ALIGN_CENTER, 0, -28 - ph / 2);   // sit lower: fill the band down to the card
        lv_obj_clear_flag(s_photo, LV_OBJ_FLAG_HIDDEN);
        lv_obj_invalidate(s_photo);
        if (s_photoCredit) {
            char c[52];
            snprintf(c, sizeof(c), "Photo: %s", pcred[0] ? pcred : "planespotters.net");
            lv_label_set_text(s_photoCredit, c);
            lv_obj_align_to(s_photoCredit, s_photo, LV_ALIGN_OUT_BOTTOM_MID, UI_S(0), UI_S(1));
            lv_obj_clear_flag(s_photoCredit, LV_OBJ_FLAG_HIDDEN);
        }
    } else if (s_photo) {
        // No image to show yet: hide the canvas, but use the caption line to tell the
        // user what's happening — "Loading..." while the fetch is in flight, or a quiet
        // "No photo" once it finished without one. Unobtrusive (small, dim) but informative.
        lv_obj_add_flag(s_photo, LV_OBJ_FLAG_HIDDEN);
        if (s_photoCredit) {
            const bool done = in.hex[0] && photo_done(in.hex);
            lv_label_set_text(s_photoCredit, done ? "No photo available" : "Loading photo...");
            lv_obj_align(s_photoCredit, LV_ALIGN_CENTER, UI_S(0), UI_S(-104));   // where the photo would sit
            lv_obj_clear_flag(s_photoCredit, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// ----------------------------------------------------------------- vessel card
// AIS contacts get their own compact card: a ship has no altitude, squawk, route or
// photo, so reusing the aircraft card would leave most of it blank.
static void show_vessel_card(const VesselInfo &vi) {
    if (!s_vesselCard) return;
    char title[28];
    if (vi.name[0]) snprintf(title, sizeof(title), "%s", vi.name);
    else            snprintf(title, sizeof(title), "MMSI %lu", (unsigned long)vi.mmsi);
    fold_ascii(title);
    lv_label_set_text(s_vesselTitle, title);

    char sog[20], cog[16];
    if (vi.sogKt == vi.sogKt) {
        if      (s_units == 1) snprintf(sog, sizeof(sog), "%.1f km/h", vi.sogKt * 1.852f);
        else if (s_units == 2) snprintf(sog, sizeof(sog), "%.1f mph",  vi.sogKt * 1.15078f);
        else                   snprintf(sog, sizeof(sog), "%.1f kt",   vi.sogKt);
    } else snprintf(sog, sizeof(sog), "-");
    if (vi.cogDeg == vi.cogDeg) snprintf(cog, sizeof(cog), "%03.0f", vi.cogDeg);
    else                        snprintf(cog, sizeof(cog), "-");

    char body[112];
    snprintf(body, sizeof(body), "MMSI  %lu\nSOG   %s    COG  %s\nDIST  %.1f %s   BRG  %03.0f",
             (unsigned long)vi.mmsi, sog, cog, dist_val(vi.distKm), dist_unit(), vi.bearingDeg);
    lv_label_set_text(s_vesselBody, body);
    lv_obj_clear_flag(s_vesselCard, LV_OBJ_FLAG_HIDDEN);
    s_vesselShown = true;
}

// --------------------------------------------------------------------- input
static bool s_longPressed = false;
static int s_rangeIdx = -1;
static float s_rangeKm = RANGE_KM_DEFAULT;   // current display range (km), for the stats view
static void (*s_rangeCb)(float) = nullptr;
static lv_obj_t *s_zoomBtn = nullptr, *s_zoomLbl = nullptr;

void ui_set_range_cb(void (*cb)(float)) { s_rangeCb = cb; }

static void zoom_cb(lv_event_t *e) {   // fires on PRESS (robust vs scroll-cancel on the tileview)
    (void)e;
    static uint32_t last = 0;
    const uint32_t now = lv_tick_get();
    if (now - last < 250) return;      // debounce repeated/held presses
    last = now;
    if (!s_rangeCb) return;
    const int n = (int)(sizeof(RANGE_STEPS_KM) / sizeof(RANGE_STEPS_KM[0]));
    s_rangeIdx = (s_rangeIdx + 1) % n;
    s_rangeCb(RANGE_STEPS_KM[s_rangeIdx]);
}

void ui_set_range_km(float km) {
    s_rangeKm = km;
    if (s_zoomLbl) {
        char b[20];
        float val = dist_val(km);
        if (fabsf(val - roundf(val)) < 0.15f) {
            snprintf(b, sizeof(b), LV_SYMBOL_LOOP " %.0f %s", (double)roundf(val), dist_unit());
        } else {
            snprintf(b, sizeof(b), LV_SYMBOL_LOOP " %.1f %s", (double)val, dist_unit());
        }
        lv_label_set_text(s_zoomLbl, b);
    }
    int best = 0; float bd = 1e9f;                 // sync the cycle index to the shown range
    const int n = (int)(sizeof(RANGE_STEPS_KM) / sizeof(RANGE_STEPS_KM[0]));
    for (int i = 0; i < n; ++i) { float d = km - RANGE_STEPS_KM[i]; if (d < 0) d = -d; if (d < bd) { bd = d; best = i; } }
    s_rangeIdx = best;
}

// ------------------------------------------------------------------ pinch zoom
// Two-finger pinch on the radar continuously scales the display range. While the
// gesture runs only the visual projection changes (preview cb); the feed re-query +
// NVS persist (full range cb) happen once, when the last finger lifts.
#define PINCH_RANGE_MIN_KM   1.0f
#define PINCH_RANGE_MAX_KM 150.0f
static void (*s_rangePreviewCb)(float) = nullptr;
static bool     s_pinchActive = false;
static float    s_pinchStartDist = 0.0f;
static float    s_pinchStartRange = 0.0f;
static float    s_pinchRange = 0.0f;
static uint32_t s_pinchLastApply = 0;
static uint32_t s_pinchEndTick = 0;          // suppress the ghost tap right after a pinch

void ui_set_range_preview_cb(void (*cb)(float)) { s_rangePreviewCb = cb; }

void ui_pinch_touch(int nPoints, int x0, int y0, int x1, int y1) {
    if (nPoints >= 2) {
        const float dx = (float)(x1 - x0), dy = (float)(y1 - y0);
        const float dist = sqrtf(dx * dx + dy * dy);
        if (!s_pinchActive) {
            if (dist < 30.0f) return;              // ignore degenerate two-point reads
            s_pinchActive = true;
            s_pinchStartDist = dist;
            s_pinchStartRange = s_rangeKm;
            s_pinchRange = s_rangeKm;
            return;
        }
        if (dist < 20.0f) return;
        // fingers moving apart = zoom in = smaller range
        float km = s_pinchStartRange * (s_pinchStartDist / dist);
        if (km < PINCH_RANGE_MIN_KM) km = PINCH_RANGE_MIN_KM;
        if (km > PINCH_RANGE_MAX_KM) km = PINCH_RANGE_MAX_KM;
        const uint32_t now = lv_tick_get();
        // Each preview re-projects the coastline (a full sweep of the embedded point
        // set) and rebuilds the flow layer, so keep the cadence modest and ignore
        // changes too small to see.
        if (now - s_pinchLastApply < 200) return;
        if (fabsf(km - s_pinchRange) < s_pinchRange * 0.03f) return;
        s_pinchLastApply = now;
        s_pinchRange = km;
        if (s_rangePreviewCb) s_rangePreviewCb(km);
    } else if (s_pinchActive) {
        s_pinchActive = false;
        s_pinchEndTick = lv_tick_get();
        // round the committed range to a whole km so labels/NVS stay tidy
        const float km = floorf(s_pinchRange + 0.5f);
        // A brief two-finger touch that never scaled anything shouldn't cost an NVS
        // write and a feed re-query.
        if (s_rangeCb && fabsf(km - s_pinchStartRange) >= 0.5f) s_rangeCb(km);
    }
}

static void radar_press_cb(lv_event_t *e) { (void)e; s_longPressed = false; }

static void radar_longpress_cb(lv_event_t *e) {   // long-press cycles the visual theme
    (void)e;
    radar::cycleTheme();
    s_longPressed = true;
}

static void radar_clicked_cb(lv_event_t *e) {
    (void)e;
    if (s_longPressed) { s_longPressed = false; return; }   // ignore the click after a long-press
    if (s_pinchActive || lv_tick_get() - s_pinchEndTick < 400) return;   // pinch, not a tap
    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return;
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    const int hit = radar::hitTest(p.x, p.y);
    if (hit >= 0) {                            // aircraft win ties: they are the main event
        s_vesselShown = false;
        radar::select(hit);
        refresh_card();
        return;
    }
    // no aircraft under the finger -> try the AIS contacts
    VesselInfo vi;
    if (radar::trafficMode() == radar::TRAFFIC_MARINE && vessel_hit_test(p.x, p.y, 30, &vi)) {
        radar::select(-1);
        show_vessel_card(vi);
        refresh_card();
        return;
    }
    s_vesselShown = false;
    radar::select(-1);
    refresh_card();
}

static void list_btn_cb(lv_event_t *e) {
    lv_obj_t *b = lv_event_get_target(e);
    const int idx = (int)(intptr_t)lv_obj_get_user_data(b);
    radar::select(idx);
    refresh_card();
    lv_obj_set_tile_id(s_tv, 0, 0, LV_ANIM_ON);   // jump back to the radar
}

// ----------------------------------------------------------------- list/stats
void ui_set_status(bool wifiUp, bool feedOk, int rssi, const char *clock) {
    // bar count from RSSI (dBm): the weaker the signal, the fewer lit bars
    int level;
    if      (!wifiUp)     level = 0;
    else if (rssi >= -55) level = 4;   // excellent
    else if (rssi >= -67) level = 3;   // good
    else if (rssi >= -75) level = 2;   // ok
    else                  level = 1;   // weak (connected but marginal)
    // colour: red = no WiFi, amber = connected but feed stale (no fresh data), white = healthy
    const lv_color_t col = !wifiUp ? UI_EMERG : (feedOk ? UI_INK : lv_color_hex(0xFFB23C));
    for (int i = 0; i < 4; ++i) {
        if (!s_hudBars[i]) continue;
        lv_obj_set_style_bg_color(s_hudBars[i], col, 0);
        lv_obj_set_style_bg_opa(s_hudBars[i], (i < level) ? LV_OPA_COVER : 45, 0);
    }
    if (s_hudClock && clock) lv_label_set_text(s_hudClock, clock);
}

void ui_set_battery(int pct, bool charging, bool present) {
    if (!s_hudBatt) return;
    if (!present || pct < 0) { lv_label_set_text(s_hudBatt, ""); return; }   // USB-only -> hide
    const char *sym = pct > 80 ? LV_SYMBOL_BATTERY_FULL :
                      pct > 55 ? LV_SYMBOL_BATTERY_3 :
                      pct > 35 ? LV_SYMBOL_BATTERY_2 :
                      pct > 12 ? LV_SYMBOL_BATTERY_1 : LV_SYMBOL_BATTERY_EMPTY;
    char buf[24];
    snprintf(buf, sizeof(buf), "%s%s%d", charging ? LV_SYMBOL_CHARGE : "", sym, pct);
    lv_label_set_text(s_hudBatt, buf);
    lv_obj_set_style_text_color(s_hudBatt, (pct <= 15 && !charging) ? UI_EMERG : UI_INK, 0);
}

void ui_set_date(const char *date) {
    if (s_hudDate && date) lv_label_set_text(s_hudDate, date);
}

void ui_set_netinfo(const char *line) {
    if (s_statsNet && line) lv_label_set_text(s_statsNet, line);
}

// GPS indicator. state: 0 = off / no module (hidden), 1 = acquiring (amber), 2 = fix (green).
void ui_set_gps(int state, int sats) {
    if (state <= 0) {                                 // hidden when GPS auto-location is off
        if (s_hudGps)   lv_label_set_text(s_hudGps, "");
        if (s_statsGps) lv_label_set_text(s_statsGps, "");
        return;
    }
    const bool fix = (state >= 2);
    const lv_color_t col = fix ? UI_GREEN : lv_color_hex(0xFFB23C);   // amber while acquiring
    if (s_hudGps) {
        char b[16];
        snprintf(b, sizeof(b), LV_SYMBOL_GPS "%d", sats);
        lv_label_set_text(s_hudGps, b);
        lv_obj_set_style_text_color(s_hudGps, col, 0);
    }
    if (s_statsGps) {
        char s[40];
        if (fix) snprintf(s, sizeof(s), LV_SYMBOL_GPS " fix  " LV_SYMBOL_BULLET "  %d sats", sats);
        else     snprintf(s, sizeof(s), LV_SYMBOL_GPS " acquiring  (%d sats)", sats);
        lv_label_set_text(s_statsGps, s);
        lv_obj_set_style_text_color(s_statsGps, col, 0);
    }
}

// Rebuild the scrollable contact list. Costly (deletes+recreates LVGL buttons), so we
// only call it when the list tile is actually visible — not on every 2 s poll.
static void vessel_list_btn_cb(lv_event_t *e) {
    lv_obj_t *b = lv_event_get_target(e);
    const int idx = (int)(intptr_t)lv_obj_get_user_data(b);
    VesselInfo vi;
    if (!vessel_visible_info(idx, &vi)) return;
    show_vessel_card(vi);
    lv_obj_set_tile_id(s_tv, 0, 0, LV_ANIM_ON);   // jump back to the scope
}

static void build_list(void) {
    if (!s_list) return;
    lv_obj_clean(s_list);
    const bool marine = radar::trafficMode() == radar::TRAFFIC_MARINE;
    if (s_listTitle) lv_label_set_text(s_listTitle, marine ? "VESSELS" : "AIRCRAFT");

    // The list follows the scope: in marine mode it enumerates ships, not aircraft.
    if (marine) {
        const int vn = vessel_visible_count();
        // Say why it is empty. Without this, switching to Marine inland -- or before a
        // key is entered -- shows a blank screen indistinguishable from a broken feed.
        if (vn == 0) {
            lv_obj_t *e = lv_label_create(s_list);
            lv_label_set_long_mode(e, LV_LABEL_LONG_WRAP);
            lv_obj_set_width(e, UI_S(320));
            lv_obj_set_style_text_align(e, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_text_font(e, F14(), 0);
            lv_obj_set_style_text_color(e, UI_DIM, 0);
            // Explicit line breaks: the list's inner width clips before the round bezel
            // does, so auto-wrap loses the last character of a long line.
            lv_label_set_text(e, ais_has_key()
                                     ? "No vessels in range\n\nAIS reception is\ncrowd-sourced from coastal\nreceivers, so inland\nlocations often see nothing."
                                     : "No aisstream.io key\n\nAdd a free key on the\nconfig page to plot\nmarine traffic.");
            lv_obj_align(e, LV_ALIGN_CENTER, UI_S(0), UI_S(0));
            return;
        }
        for (int i = 0; i < vn; ++i) {
            VesselInfo vi;
            if (!vessel_visible_info(i, &vi)) continue;
            char sog[12], txt[64];
            if (vi.sogKt == vi.sogKt) snprintf(sog, sizeof(sog), "%.0fkt", vi.sogKt);
            else                      snprintf(sog, sizeof(sog), "-");
            char nm[22];
            if (vi.name[0]) snprintf(nm, sizeof(nm), "%s", vi.name);
            else            snprintf(nm, sizeof(nm), "%lu", (unsigned long)vi.mmsi);
            fold_ascii(nm);
            snprintf(txt, sizeof(txt), "%-12.12s %-6s %4.1f %s",
                     nm, sog, dist_val(vi.distKm), dist_unit());
            lv_obj_t *b = lv_list_add_btn(s_list, NULL, txt);
            lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
            lv_obj_set_style_text_color(b, lv_color_hex(0x9BE9FF), 0);
            lv_obj_set_style_text_font(b, &lv_font_montserrat_16, 0);
            lv_obj_set_user_data(b, (void *)(intptr_t)i);
            lv_obj_add_event_cb(b, vessel_list_btn_cb, LV_EVENT_CLICKED, NULL);
        }
        return;
    }

    const int n = radar::count();
    for (int i = 0; i < n; ++i) {
        AcInfo in;
        radar::info(i, in);
        char altS[16], txt[64];
        fmt_alt(altS, sizeof(altS), in.altFt, in.onGround);
        snprintf(txt, sizeof(txt), "%-8.8s  %-8s %4.1f %s",
                 in.call[0] ? in.call : in.hex, altS, dist_val(in.distKm), dist_unit());
        lv_obj_t *b = lv_list_add_btn(s_list, NULL, txt);
        lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(b, in.emergency ? UI_EMERG : (in.military ? UI_MIL : UI_SOFT), 0);
        lv_obj_set_style_text_font(b, F16(), 0);
        lv_obj_set_user_data(b, (void *)(intptr_t)i);
        lv_obj_add_event_cb(b, list_btn_cb, LV_EVENT_CLICKED, NULL);
    }
}

static void build_stats(void) {
    if (!s_statsLbl) return;
    const int n = radar::count();
    int emg = 0;
    float nearest = 1e9f, highest = -1e9f;
    char nearestCall[12] = "-";
    for (int i = 0; i < n; ++i) {
        AcInfo in;
        radar::info(i, in);
        if (in.emergency) emg++;
        if (in.distKm < nearest) { nearest = in.distKm; snprintf(nearestCall, sizeof(nearestCall), "%s", in.call[0] ? in.call : in.hex); }
        if (!in.onGround && in.altFt > highest) highest = in.altFt;
    }
    char altH[16];
    fmt_alt(altH, sizeof(altH), (highest > -1e8f) ? highest : 0.0f, false);
    const int fires = wildfire_count();
    const int ships = vessel_count();
    char extra[64] = "";
    // Only mention the optional layers once they actually have data, so the panel stays
    // uncluttered for users who never enable them.
    if (ships) snprintf(extra + strlen(extra), sizeof(extra) - strlen(extra), "\nVessels    %d", ships);
    if (fires) snprintf(extra + strlen(extra), sizeof(extra) - strlen(extra), "\nFires      %d", fires);

    const int rssi = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0;
    const char *q = (rssi > -60) ? "EXCELLENT" : ((rssi > -75) ? "GOOD" : ((rssi != 0) ? "WEAK" : "OFFLINE"));

    char st[350];
    snprintf(st, sizeof(st),
             "Aircraft   %d\n"
             "Emergency  %d\n"
             "Nearest    %s (%.1f %s)\n"
             "Highest    %s\n"
             "Range      %.0f %s%s\n"
             "WiFi RSSI  %d dBm (%s)",
             n, emg, n ? nearestCall : "-", dist_val(n ? nearest : 0.0f), dist_unit(),
             altH, dist_val(s_rangeKm), dist_unit(), extra,
             rssi, q);
    lv_label_set_text(s_statsLbl, st);
}

static void build_weather(void) {
    if (!s_weatherNow || !s_weatherMeta || !s_weatherDays || !s_wxFooter) return;
    WeatherSnapshot w;
    if (!weather_get(w)) {
        lv_label_set_text(s_weatherNow, "Forecast unavailable");
        lv_label_set_text(s_weatherMeta, "Waiting for WiFi data...");
        lv_label_set_text(s_wxFooter, "WEATHER DATA PENDING");
        lv_label_set_text(s_weatherDays, "");
    } else {
        char now[96];
        snprintf(now, sizeof(now), "%.0f %s\n%s", weather_temp(w.tempC),
                 weather_temp_unit(), weather_condition(w.code));
        lv_label_set_text(s_weatherNow, now);
        char meta[128];
        snprintf(meta, sizeof(meta), "Feels %.0f %s   Humidity %d%%\nWind %.0f %s  %s   Updated %s",
                 weather_temp(w.feelsC), weather_temp_unit(), w.humidity,
                 weather_wind(w.windKmh), weather_wind_unit(), cardinal((float)w.windDeg), w.updated);
        lv_label_set_text(s_weatherMeta, meta);

        char footer[96];
        snprintf(footer, sizeof(footer), "%.0f %s   %s", weather_temp(w.tempC),
                 weather_temp_unit(), weather_condition(w.code));
        lv_label_set_text(s_wxFooter, footer);
        char wxmeta[96];
        snprintf(wxmeta, sizeof(wxmeta), "WIND %s %.0f %s   HUM %d%%",
                 cardinal((float)w.windDeg), weather_wind(w.windKmh), weather_wind_unit(), w.humidity);
        lv_label_set_text(s_wxMeta, wxmeta);

        char current[24];
        snprintf(current, sizeof(current), "%.0f %s", weather_temp(w.tempC), weather_temp_unit());
        lv_label_set_text(s_fcCurrent, current);
        lv_label_set_text(s_fcCondition, weather_condition(w.code));
        set_wx_icon(s_fcIcon, w.code, is_night());
        lv_label_set_text(s_fcMetricValue[0], current);
        char hum[16]; snprintf(hum, sizeof(hum), "%d%%", w.humidity);
        lv_label_set_text(s_fcMetricValue[1], hum);
        char wind[28]; snprintf(wind, sizeof(wind), "%s %.0f %s", cardinal((float)w.windDeg),
                                weather_wind(w.windKmh), weather_wind_unit());
        lv_label_set_text(s_fcMetricValue[2], wind);
        // The feed hands back "HH:MM" in 24-hour form; re-format it so this line agrees
        // with the clock preference instead of showing 23:30 next to a 12-hour clock.
        char stampU[16] = "", updated[28];
        {
            int uh = 0, um = 0;
            if (sscanf(w.updated, "%d:%d", &uh, &um) == 2)
                ui_format_clock(stampU, sizeof(stampU), uh, um, true);
            else
                snprintf(stampU, sizeof(stampU), "%s", w.updated);
        }
        snprintf(updated, sizeof(updated), "UPDATED %s", stampU);
        lv_label_set_text(s_fcUpdated, updated);

        for (int col = 0; col < 3; ++col) {
            const int i = col + 1;
            if (i < w.dayCount) {
                lv_label_set_text(s_fcDay[col], weather_day_name(w.days[i].date));
                lv_label_set_text(s_fcDayCondition[col], weather_condition(w.days[i].code));
                set_wx_icon(s_fcDayIcon[col], w.days[i].code, false);
                char temps[28];
                snprintf(temps, sizeof(temps), "%.0f / %.0f %s",
                         weather_temp(w.days[i].tempMaxC), weather_temp(w.days[i].tempMinC), weather_temp_unit());
                lv_label_set_text(s_fcDayTemp[col], temps);
                char chance[20]; snprintf(chance, sizeof(chance), "RAIN %d%%", w.days[i].rainChance);
                lv_label_set_text(s_fcDayRain[col], chance);
            } else {
                lv_label_set_text(s_fcDay[col], "-");
                lv_label_set_text(s_fcDayCondition[col], "");
                lv_label_set_text(s_fcDayTemp[col], "");
                lv_label_set_text(s_fcDayRain[col], "");
                weather_icon_set(s_fcDayIcon[col], -1, false);
            }
        }

        char days[320] = "";
        for (int i = 1; i < w.dayCount && i < 4; ++i) {
            char row[104];
            snprintf(row, sizeof(row), "%-3s  %-14s  %2.0f/%2.0f %s  %3d%%\n",
                     weather_day_name(w.days[i].date), weather_condition(w.days[i].code),
                     weather_temp(w.days[i].tempMaxC), weather_temp(w.days[i].tempMinC),
                     weather_temp_unit(), w.days[i].rainChance);
            strncat(days, row, sizeof(days) - strlen(days) - 1);
        }
        lv_label_set_text(s_weatherDays, days);
    }

    const uint16_t *radarPixels = nullptr, *cloudPixels = nullptr;
    uint32_t frameTime = 0, version = 0;
    double rlat = 0, rlon = 0;
    const bool cloudMode = s_weatherMode == WEATHER_CLOUDS;
    const bool forecastMode = s_weatherMode == WEATHER_FORECAST;
    bool haveImage = false;
    if (cloudMode)
        haveImage = cloud_image_front(&cloudPixels, &frameTime, &rlat, &rlon, &version);
    else
        haveImage = wx_radar_front(&radarPixels, &frameTime, &rlat, &rlon, &version);
    const uint16_t *pixels = cloudMode ? cloudPixels : radarPixels;
    if (haveImage && pixels && s_wxCanvas) {
        lv_canvas_set_buffer(s_wxCanvas, (void *)pixels, WX_RADAR_SIZE, WX_RADAR_SIZE, LV_IMG_CF_TRUE_COLOR);
        lv_obj_clear_flag(s_wxCanvas, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_wxStatus, LV_OBJ_FLAG_HIDDEN);
        lv_obj_invalidate(s_wxCanvas);
        char iata[6]; float d = 0, b = 0;
        if (airports_nearest_iata(rlat, rlon, 200.0f, iata, &d, &b)) {
            char apt[64];
            snprintf(apt, sizeof(apt), "O  %s   %.0f %s %s", iata, dist_val(d), dist_unit(), cardinal(b));
            lv_label_set_text(s_wxAirport, apt);
        } else lv_label_set_text(s_wxAirport, "RADAR CENTRE");
        char stamp[12] = "--:--";
        time_t ft = (time_t)frameTime; struct tm ti;
        if (frameTime && localtime_r(&ft, &ti))
            ui_format_clock(stamp, sizeof(stamp), ti.tm_hour, ti.tm_min, true);
        char attr[64];
        snprintf(attr, sizeof(attr), cloudMode ? "SAT %s  |  EUMETSAT" : "RADAR %s  |  RAINVIEWER", stamp);
        lv_label_set_text(s_wxAttrib, attr);
    } else {
        if (s_wxCanvas) lv_obj_add_flag(s_wxCanvas, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_wxStatus, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_wxAirport, "RADAR CENTRE");
        lv_label_set_text(s_wxAttrib, cloudMode ? "WAITING FOR SATELLITE DATA" : "WAITING FOR RADAR DATA");
    }

    lv_obj_t *forecastObjs[] = {
        s_fcIcon, s_fcDayIcon[0], s_fcDayIcon[1], s_fcDayIcon[2],
        s_fcCurrent, s_fcCondition, s_fcUpdated,
        s_fcMetricName[0], s_fcMetricName[1], s_fcMetricName[2],
        s_fcMetricValue[0], s_fcMetricValue[1], s_fcMetricValue[2],
        s_fcDay[0], s_fcDay[1], s_fcDay[2],
        s_fcDayCondition[0], s_fcDayCondition[1], s_fcDayCondition[2],
        s_fcDayTemp[0], s_fcDayTemp[1], s_fcDayTemp[2],
        s_fcDayRain[0], s_fcDayRain[1], s_fcDayRain[2]
    };
    lv_obj_t *radarObjs[] = { s_wxCanvas, s_wxStatus, s_wxAirport, s_wxFooter, s_wxMeta,
                              s_wxAttrib, s_wxNorth, s_wxCenter, s_wxRange,
                              s_wxRings[0], s_wxRings[1], s_wxRings[2] };
    for (lv_obj_t *o : forecastObjs) if (o) {
        if (forecastMode) lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    }
    for (lv_obj_t *o : radarObjs) if (o) {
        if (forecastMode) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN); else lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
    }
    if (!forecastMode && haveImage) lv_obj_add_flag(s_wxStatus, LV_OBJ_FLAG_HIDDEN);
    if (!forecastMode && !haveImage) lv_obj_add_flag(s_wxCanvas, LV_OBJ_FLAG_HIDDEN);
    char wxRng[16];
    snprintf(wxRng, sizeof(wxRng), "%.0f %s",
             dist_val(cloudMode ? WX_CLOUD_RANGE_KM : WX_RADAR_RANGE_KM), dist_unit_caps());
    lv_label_set_text(s_wxRange, wxRng);
    lv_label_set_text(s_weatherModeLbl,
        s_weatherMode == WEATHER_RADAR ? "CLOUDS" :
        s_weatherMode == WEATHER_CLOUDS ? "3-DAY FORECAST" : "WX RADAR");
    if (s_weatherTitle) lv_label_set_text(s_weatherTitle,
        s_weatherMode == WEATHER_RADAR ? "WX RADAR" :
        s_weatherMode == WEATHER_CLOUDS ? "SAT CLOUDS" : "WEATHER");
}

static void weather_mode_cb(lv_event_t *) {
    s_weatherMode = (WeatherViewMode)(((int)s_weatherMode + 1) % 3);
    build_weather();
}

void ui_set_weather_forecast(bool forecast) {
    s_weatherMode = forecast ? WEATHER_FORECAST : WEATHER_RADAR;
    build_weather();
}

void ui_set_weather_mode(int mode) {
    if (mode < 0 || mode > 2) return;
    s_weatherMode = (WeatherViewMode)mode;
    build_weather();
}

// Force every weather glyph to one WMO code, so the whole icon set can be eyeballed
// without waiting for the weather to oblige (a Kansas summer is not going to supply
// snow on demand). It has to be a latch rather than a one-shot: build_weather() runs on
// every poll and would otherwise paint the real code back within two seconds.
// /view?icon=-1 returns to live data.
void ui_preview_weather_icon(int code) {
    s_iconPreview = (code < 0) ? -1 : code;
    build_weather();
    build_clock_weather();
}

// ---------------------------------------------------------------- tracked tile
// Follows one flight: route, progress along the great circle, ETA and live numbers.
// Progress uses flown/(flown+remaining) rather than flown/total so a diversion or a
// non-direct routing still yields a sane bar instead of pinning at 100%.
static void build_tracked(void) {
    if (!s_trkTitle) return;

    if (!s_trackHex[0]) {
        lv_label_set_text(s_trkTitle, "No flight tracked");
        lv_label_set_text(s_trkRoute, "");
        lv_label_set_text(s_trkStats, "");
        lv_label_set_text(s_trkEta, "");
        lv_label_set_text(s_trkFrom, "");
        lv_label_set_text(s_trkTo, "");
        lv_label_set_text(s_trkPct, "");
        lv_obj_add_flag(s_trkBar, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_trkHint, "Tap an aircraft on the radar,\nthen press TRACK on its card.");
        return;
    }
    lv_obj_clear_flag(s_trkBar, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_trkHint, "");

    AcInfo in;
    const bool live = radar::infoByHex(s_trackHex, in);
    char title[40];
    if (live && in.type[0]) snprintf(title, sizeof(title), "%s  %s", in.call[0] ? in.call : s_trackHex, in.type);
    else                    snprintf(title, sizeof(title), "%s", s_trackCall[0] ? s_trackCall : s_trackHex);
    fold_ascii(title);
    lv_label_set_text(s_trkTitle, title);
    lv_obj_set_style_text_color(s_trkTitle, (live && in.emergency) ? UI_EMERG : UI_INK, 0);

    const char *call = (live && in.call[0]) ? in.call : s_trackCall;
    char rfrom[40] = "", rto[40] = "";
    const bool haveRoute = call[0] && route_get(call, rfrom, sizeof(rfrom), rto, sizeof(rto));
    if (haveRoute && (rfrom[0] || rto[0])) {
        char rt[96];
        snprintf(rt, sizeof(rt), "%s  " LV_SYMBOL_RIGHT "  %s", rfrom[0] ? rfrom : "?", rto[0] ? rto : "?");
        fold_ascii(rt);
        lv_label_set_text(s_trkRoute, rt);
        char f[40], t[40];
        snprintf(f, sizeof(f), "%s", rfrom[0] ? rfrom : "?");
        snprintf(t, sizeof(t), "%s", rto[0] ? rto : "?");
        fold_ascii(f); fold_ascii(t);
        lv_label_set_text(s_trkFrom, f);
        lv_label_set_text(s_trkTo, t);
    } else {
        lv_label_set_text(s_trkRoute, call[0] ? "Looking up route..." : "Route unavailable");
        lv_label_set_text(s_trkFrom, "");
        lv_label_set_text(s_trkTo, "");
    }

    if (!live) {
        lv_label_set_text(s_trkStats, "Contact lost\n(out of range or feed gap)");
        lv_label_set_text(s_trkEta, "");
        return;
    }

    char altS[16], spdS[16], vsS[24];
    fmt_alt(altS, sizeof(altS), in.altFt, in.onGround);
    fmt_spd(spdS, sizeof(spdS), in.gsKt);
    fmt_vs(vsS, sizeof(vsS), in.vsFpm);
    char st[128];
    snprintf(st, sizeof(st), "ALT  %-10s  SPD %s\nDIST %.1f %s   BRG %03.0f   V/S %s",
             altS, spdS, dist_val(in.distKm), dist_unit(), in.bearingDeg, vsS);
    lv_label_set_text(s_trkStats, st);

    // progress + ETA need the route endpoints and the aircraft's live position
    RouteCoords rc;
    double aclat = 0, aclon = 0;
    int pct = -1;
    double remainKm = 0;
    if (call[0] && route_get_coords(call, rc) && radar::positionByHex(s_trackHex, &aclat, &aclon)) {
        const double flown  = geo::haversineKm(rc.fromLat, rc.fromLon, aclat, aclon);
        remainKm            = geo::haversineKm(aclat, aclon, rc.toLat, rc.toLon);
        const double denom  = flown + remainKm;
        if (denom > 1.0) {
            pct = (int)((flown / denom) * 100.0 + 0.5);
            if (pct < 0) pct = 0;
            if (pct > 100) pct = 100;
        }
    }

    if (pct >= 0) {
        lv_bar_set_value(s_trkBar, pct, LV_ANIM_ON);
        char p[8];
        snprintf(p, sizeof(p), "%d%%", pct);
        lv_label_set_text(s_trkPct, p);
        char eta[64];
        if (in.gsKt == in.gsKt && in.gsKt > 40.0f) {          // NaN-safe; ignore taxi speeds
            const double hours = remainKm / (in.gsKt * 1.852);
            const int mins = (int)(hours * 60.0 + 0.5);
            if (mins < 60) snprintf(eta, sizeof(eta), "%.0f %s to run  " LV_SYMBOL_BULLET "  ETA %d min",
                                    dist_val((float)remainKm), dist_unit(), mins);
            else           snprintf(eta, sizeof(eta), "%.0f %s to run  " LV_SYMBOL_BULLET "  ETA %dh %02dm",
                                    dist_val((float)remainKm), dist_unit(), mins / 60, mins % 60);
        } else {
            snprintf(eta, sizeof(eta), "%.0f %s to run", dist_val((float)remainKm), dist_unit());
        }
        lv_label_set_text(s_trkEta, eta);
    } else {
        lv_bar_set_value(s_trkBar, 0, LV_ANIM_OFF);
        lv_label_set_text(s_trkPct, "");
        lv_label_set_text(s_trkEta, haveRoute ? "No airport coordinates for this route" : "");
    }
}

static void track_btn_cb(lv_event_t *) {
    AcInfo in;
    if (!radar::selected(in)) return;
    if (s_trackHex[0] && strcmp(s_trackHex, in.hex) == 0) {   // pressing it again untracks
        s_trackHex[0] = 0;
        s_trackCall[0] = 0;
        radar::setTracked("");
    } else {
        snprintf(s_trackHex, sizeof(s_trackHex), "%s", in.hex);
        snprintf(s_trackCall, sizeof(s_trackCall), "%s", in.call);
        radar::setTracked(in.hex);
        if (in.call[0]) route_request(in.call);   // make sure the route is on its way
    }
    refresh_card();
    build_tracked();
}

// Same effect as pressing TRACK on the detail card, minus the finger. The UI keeps its
// own tracked hex (the radar layer's copy only pins the contact against the on-screen
// cap), so a remote capture has to go through here or the Tracked view stays empty.
void ui_track_selected(bool on) {
    if (!on) {
        s_trackHex[0] = 0;
        s_trackCall[0] = 0;
        radar::setTracked("");
    } else {
        AcInfo in;
        if (!radar::selected(in)) return;
        snprintf(s_trackHex,  sizeof(s_trackHex),  "%s", in.hex);
        snprintf(s_trackCall, sizeof(s_trackCall), "%s", in.call);
        radar::setTracked(in.hex);
        if (in.call[0]) route_request(in.call);
    }
    refresh_card();
    build_tracked();
}

// ------------------------------------------------------------------- fires tile
// A continental fire map. Deliberately not the radar scope: wildfires are a regional
// phenomenon and the scope's range would have to be absurd to show any. Coastline comes
// from the same embedded dataset the radar uses, so the map needs no network at all.
static lv_obj_t *s_tileFires = nullptr, *s_fireLayer = nullptr;
static lv_obj_t *s_fireTitle = nullptr, *s_fireBadge = nullptr, *s_fireInfo = nullptr, *s_fireBack = nullptr;
static uint32_t  s_fireSeen = 0;

static void fire_draw_cb(lv_event_t *e) {
    lv_draw_ctx_t *d = lv_event_get_draw_ctx(e);
    const lv_coord_t cx = SCREEN_CX, cy = SCREEN_CY + 8;

    // Get current view bounds to draw dynamic lat/lon grid lines
    FireView v;
    firemap_get_view(&v);

    if (v.east > v.west && v.north > v.south) {
        // 1. Draw a high-tech lat/lon coordinate grid
        lv_draw_line_dsc_t grid;
        lv_draw_line_dsc_init(&grid);
        grid.color = lv_color_hex(0x1B2C3A);   // dark slate-blue
        grid.width = 1;
        grid.opa = 65;

        // Draw longitude lines every 10 degrees
        for (double lon = ceil(v.west / 10.0) * 10.0; lon <= floor(v.east / 10.0) * 10.0; lon += 10.0) {
            lv_point_t p1, p2;
            firemap_project(v.south, lon, cx, cy, FIREMAP_HALF_W, FIREMAP_HALF_H, &p1);
            firemap_project(v.north, lon, cx, cy, FIREMAP_HALF_W, FIREMAP_HALF_H, &p2);
            lv_draw_line(d, &grid, &p1, &p2);
        }
        // Draw latitude lines every 10 degrees
        for (double lat = ceil(v.south / 10.0) * 10.0; lat <= floor(v.north / 10.0) * 10.0; lat += 10.0) {
            lv_point_t p1, p2;
            firemap_project(lat, v.west, cx, cy, FIREMAP_HALF_W, FIREMAP_HALF_H, &p1);
            firemap_project(lat, v.east, cx, cy, FIREMAP_HALF_W, FIREMAP_HALF_H, &p2);
            lv_draw_line(d, &grid, &p1, &p2);
        }
    }

    // 2. Draw a subtle telemetry crosshair in the center
    lv_draw_line_dsc_t ch;
    lv_draw_line_dsc_init(&ch);
    ch.color = lv_color_hex(0x2E5C86);
    ch.width = 1;
    ch.opa = 80;
    lv_point_t h1 = { (lv_coord_t)(cx - 10), cy }, h2 = { (lv_coord_t)(cx + 10), cy };
    lv_point_t v1 = { cx, (lv_coord_t)(cy - 10) }, v2 = { cx, (lv_coord_t)(cy + 10) };
    lv_draw_line(d, &ch, &h1, &h2);
    lv_draw_line(d, &ch, &v1, &v2);

    // 3. Draw US State Lines & Borders (subtle dark slate blue)
    borders_draw_rect(d, lv_color_hex(0x1E3A52), 160, 1);

    // 4. Draw Ocean Coastlines (vibrant cyan/teal)
    coastline_draw_rect(d, lv_color_hex(0x38BDF8), 200, 2);

    // 5. Draw active hot spots with pulsing radial heat glows
    const int n = firemap_cell_count();
    const float pulse = 0.8f + 0.2f * sinf((float)lv_tick_get() * 0.003f); // soft breathing pulse

    for (int pass = 1; pass >= 0; --pass) {
        for (int i = 0; i < n; ++i) {
            FireCell c;
            if (!firemap_cell(i, &c)) continue;
            const bool big = (c.frp >= 200.0f || c.count >= 12);
            if ((pass == 1) != big) continue;

            lv_point_t p;
            firemap_project(c.lat, c.lon, cx, cy, FIREMAP_HALF_W, FIREMAP_HALF_H, &p);

            const bool mid = (c.frp >= 50.0f || c.count >= 4);
            const lv_coord_t r = big ? 6 : (mid ? 4 : 2);
            
            // Draw a soft heat glow under the core dot
            lv_draw_arc_dsc_t glow;
            lv_draw_arc_dsc_init(&glow);
            glow.color = big ? lv_color_hex(0xFF3B1F)
                             : (mid ? lv_color_hex(0xFF7A18) : lv_color_hex(0xFFC24D));
            glow.width = 2;
            glow.opa = (lv_opa_t)(40.0f * pulse);
            lv_draw_arc(d, &glow, &p, (uint16_t)(r + 3), 0, 360);

            // Major hotspots have a wider secondary pulsing ring
            if (big) {
                glow.opa = (lv_opa_t)(20.0f * pulse);
                lv_draw_arc(d, &glow, &p, (uint16_t)(r + 7), 0, 360);
            }

            // Core dot
            lv_draw_rect_dsc_t dot;
            lv_draw_rect_dsc_init(&dot);
            dot.radius = LV_RADIUS_CIRCLE;
            dot.bg_opa = LV_OPA_COVER;
            dot.bg_color = glow.color;
            lv_area_t a = { (lv_coord_t)(p.x - r), (lv_coord_t)(p.y - r),
                            (lv_coord_t)(p.x + r), (lv_coord_t)(p.y + r) };
            lv_draw_rect(d, &dot, &a);
        }
    }
}

static void build_fires(void) {
    if (!s_fireTitle) return;
    const bool zoomed = firemap_is_zoomed();
    lv_label_set_text(s_fireTitle, "FIRES");
    if (s_fireBadge) {
        lv_label_set_text(s_fireBadge, zoomed ? "VIIRS HIGH-RES" : "MODIS OVERVIEW");
        lv_obj_set_style_text_color(s_fireBadge, zoomed ? UI_AMBER : UI_CYAN, 0);
    }
    if (zoomed) lv_obj_clear_flag(s_fireBack, LV_OBJ_FLAG_HIDDEN);
    else        lv_obj_add_flag(s_fireBack, LV_OBJ_FLAG_HIDDEN);

    const int total = firemap_total(), cells = firemap_cell_count();
    char info[72];
    if (cells == 0) snprintf(info, sizeof(info), "%s", firemap_status());
    else            snprintf(info, sizeof(info), "%d detections  %s  %d areas",
                             total, LV_SYMBOL_BULLET, cells);
    lv_label_set_text(s_fireInfo, info);

    // Reproject the coastline and borders only when the view actually changed
    static double pw = 1e9, ps = 1e9, pe = 1e9, pn = 1e9;
    FireView v; firemap_get_view(&v);
    if (v.west != pw || v.south != ps || v.east != pe || v.north != pn) {
        pw = v.west; ps = v.south; pe = v.east; pn = v.north;
        coastline_project_rect(v.west, v.south, v.east, v.north,
                               SCREEN_CX, SCREEN_CY + 8, FIREMAP_HALF_W, FIREMAP_HALF_H);
        borders_project_rect(v.west, v.south, v.east, v.north,
                             SCREEN_CX, SCREEN_CY + 8, FIREMAP_HALF_W, FIREMAP_HALF_H);
    }
    if (s_fireLayer) lv_obj_invalidate(s_fireLayer);
}

// Zoom the fire map around a screen point. Split out of the touch handler so the same
// path can be driven over HTTP (/view?fire=x,y) for capture and verification, exactly
// like sel/trk -- otherwise tap-to-zoom is only reachable with a finger on the glass.
void ui_fire_tap(int x, int y) {
    double lat, lon;
    firemap_unproject(x, y, SCREEN_CX, SCREEN_CY + 8,
                      FIREMAP_HALF_W, FIREMAP_HALF_H, &lat, &lon);

    // Clamp coordinates to valid North American bounds
    if (lat < 15.0)  lat = 15.0;
    if (lat > 70.0)  lat = 70.0;
    if (lon < -165.0) lon = -165.0;
    if (lon > -55.0)  lon = -55.0;

    // Zoom or re-center the high-resolution VIIRS view around the tap
    FireView v;
    v.west  = lon - 6.0; v.east  = lon + 6.0;
    v.south = lat - 4.5; v.north = lat + 4.5;
    firemap_set_view(&v);
    build_fires();
}

// Back to the continental overview -- the WHOLE MAP button, without the finger.
void ui_fire_reset(void) {
    FireView v; firemap_default_view(&v);
    firemap_set_view(&v);
    build_fires();
}

static void fire_tap_cb(lv_event_t *e) {
    (void)e;
    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return;
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    ui_fire_tap(p.x, p.y);
}

static void fire_back_cb(lv_event_t *e) {
    (void)e;
    FireView v; firemap_default_view(&v);
    firemap_set_view(&v);
    build_fires();
}

// ------------------------------------------------------------------ clock tile
// Big watch-face view: time + date, current conditions, and a compact 3-day strip.
// The time/date refresh runs on its own 1 s LVGL timer; the weather part is rebuilt
// with the other tiles whenever data arrives or the tile slides into view.
static void build_clock_weather(void) {
    if (!s_clockTemp) return;
    WeatherSnapshot w;
    if (!weather_get(w)) {
        lv_label_set_text(s_clockTemp, "");
        lv_label_set_text(s_clockCond, "");
        weather_icon_set(s_clockIcon, -1, false);
        for (int i = 0; i < 3; ++i) {
            lv_label_set_text(s_clockDay[i], "");
            lv_label_set_text(s_clockDayTemp[i], "");
            weather_icon_set(s_clockDayIcon[i], -1, false);
        }
        return;
    }
    char t[24];
    snprintf(t, sizeof(t), "%.0f %s", weather_temp(w.tempC), weather_temp_unit());
    lv_label_set_text(s_clockTemp, t);
    lv_label_set_text(s_clockCond, weather_condition(w.code));
    set_wx_icon(s_clockIcon, w.code, is_night());
    for (int col = 0; col < 3; ++col) {
        const int i = col + 1;
        if (i < w.dayCount) {
            lv_label_set_text(s_clockDay[col], weather_day_name(w.days[i].date));
            char d[24];
            snprintf(d, sizeof(d), "%.0f/%.0f", weather_temp(w.days[i].tempMaxC),
                     weather_temp(w.days[i].tempMinC));
            lv_label_set_text(s_clockDayTemp[col], d);
            set_wx_icon(s_clockDayIcon[col], w.days[i].code, false);   // daily = daytime
        } else {
            lv_label_set_text(s_clockDay[col], "");
            lv_label_set_text(s_clockDayTemp[col], "");
            weather_icon_set(s_clockDayIcon[col], -1, false);
        }
    }
}

static void clock_tick_cb(lv_timer_t *) {
    if (!s_clockTime || !s_tv) return;
    if (lv_tileview_get_tile_act(s_tv) != s_tileClock) return;   // only pay when visible
    const time_t now = time(nullptr);
    struct tm ti;
#if defined(ARDUINO) || defined(ESP_PLATFORM)
    localtime_r(&now, &ti);
#else
    ti = *localtime(&now);
#endif
    if (now < 1000000000) {                        // clock not set yet (no RTC/NTP)
        lv_label_set_text(s_clockTime, "--:--");
        lv_label_set_text(s_clockSec, "");
        lv_label_set_text(s_clockDate, "");
        return;
    }
    char hm[12], date[40];
    ui_format_clock(hm, sizeof(hm), ti.tm_hour, ti.tm_min, false);
    strftime(date, sizeof(date), "%A  %d %b %Y", &ti);
    lv_label_set_text(s_clockTime, hm);
    lv_label_set_text(s_clockDate, date);
    // Seconds read off the rim arc instead of a number: it echoes the radar sweep and
    // removes the "25 PM" ambiguity that came from sharing one label with the meridiem.
    if (s_clockArc) lv_arc_set_value(s_clockArc, ti.tm_sec);
    lv_label_set_text(s_clockSec, s_time24 ? "" : (ti.tm_hour < 12 ? "AM" : "PM"));
}

// ------------------------------------------------------------------- about tile
static lv_obj_t *s_tileAbout = nullptr, *s_aboutBody = nullptr;

static void build_about(void) {
    if (!s_aboutBody) return;
    const uint32_t up = millis() / 1000UL;
    char ip[24] = "offline";
    if (WiFi.status() == WL_CONNECTED) snprintf(ip, sizeof(ip), "%s", WiFi.localIP().toString().c_str());

    // Board and chip read from the build and the runtime, not hardcoded. This screen
    // claimed "ESP32-S3 : 8MB PSRAM : dual 240MHz" on a P4 with 32 MB and a different
    // architecture entirely -- an About screen that lies is worse than none.
    char body[560];
    snprintf(body, sizeof(body),
             "#63D8FF BUILD#   %s\n"
             "#63D8FF BOARD#   %s\n"
             "#63D8FF CHIP#    %s : %uMB PSRAM\n"
             "         %uMB flash : %u MHz\n"
             "#63D8FF HOST#    skyglass.local\n"
             "#63D8FF IP#      %s\n"
             "#63D8FF UPTIME#  %luh %lum\n"
             "#63D8FF FEED#    airplanes.live / adsb.lol\n"
             "#9AFFC8 github.com/SilentWolf75/skyglass#\n"
             "#5F7A6C MIT : fork of socquique/capsule-radar#",
             __DATE__, BOARD_NAME, ESP.getChipModel(),
             (unsigned)(ESP.getPsramSize() / (1024 * 1024)),
             (unsigned)(ESP.getFlashChipSize() / (1024 * 1024)),
             (unsigned)ESP.getCpuFreqMHz(),
             ip, (unsigned long)(up / 3600UL), (unsigned long)((up / 60UL) % 60UL));
    lv_label_set_text(s_aboutBody, body);
}

// Rebuild whichever of list/stats is currently on screen (called on poll and on swipe).
static void refresh_active_tile(void) {
    if (!s_tv) return;
    lv_obj_t *act = lv_tileview_get_tile_act(s_tv);
    if (act == s_tileList)  build_list();
    else if (act == s_tileStats) build_stats();
    else if (act == s_tileWeather) build_weather();
    else if (act == s_tileClock) build_clock_weather();
    else if (act == s_tileFires) build_fires();
    else if (act == s_tileTracked) build_tracked();
    else if (act == s_tileAbout) build_about();
}

void ui_on_data_updated(void) {
    refresh_card();
    if (s_hudCount) {
        const bool marine = radar::trafficMode() == radar::TRAFFIC_MARINE;
        char cbuf[8];
        snprintf(cbuf, sizeof(cbuf), "%d", marine ? vessel_visible_count() : radar::countInRange());
        lv_label_set_text(s_hudCount, cbuf);
    }
    refresh_active_tile();   // only the visible tile pays the rebuild cost
}

// ------------------------------------------------------------------- building
static lv_obj_t *make_tile_title(lv_obj_t *tile, const char *txt) {
    lv_obj_t *l = lv_label_create(tile);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, F16(), 0);
    lv_obj_set_style_text_color(l, UI_GREEN, 0);
    lv_obj_align(l, LV_ALIGN_TOP_MID, UI_S(0), UI_S(22));
    return l;
}

// A full-screen round panel that clips its content to the circle (for list/stats views).
static lv_obj_t *make_round_panel(lv_obj_t *parent) {
    lv_obj_t *p = lv_obj_create(parent);
    lv_obj_remove_style_all(p);
    lv_obj_set_size(p, UI_S(462), UI_S(462));
    lv_obj_center(p);
    lv_obj_set_style_radius(p, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(p, lv_color_hex(0x05100A), 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(p, UI_GREEN, 0);
    lv_obj_set_style_border_opa(p, 50, 0);
    lv_obj_set_style_border_width(p, 2, 0);
    lv_obj_set_style_clip_corner(p, true, 0);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    return p;
}

static void build_card(void) {
    s_card = lv_obj_create(s_tileRadar);
    lv_obj_remove_style_all(s_card);
    // large text needs a taller card (three 18px data lines + the route line below them)
    lv_obj_set_size(s_card, UI_S(s_bigText ? 316 : 300), UI_S(s_bigText ? 148 : 118));
    lv_obj_align(s_card, LV_ALIGN_CENTER, 0, s_bigText ? 56 : 66);
    lv_obj_set_style_bg_color(s_card, UI_PANEL, 0);
    lv_obj_set_style_bg_opa(s_card, 235, 0);
    lv_obj_set_style_radius(s_card, 14, 0);
    lv_obj_set_style_border_color(s_card, UI_GREEN, 0);
    lv_obj_set_style_border_opa(s_card, 90, 0);
    lv_obj_set_style_border_width(s_card, 1, 0);
    lv_obj_set_style_pad_all(s_card, 12, 0);
    lv_obj_add_flag(s_card, LV_OBJ_FLAG_CLICKABLE);   // consume taps (don't deselect)
    lv_obj_clear_flag(s_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_card, LV_OBJ_FLAG_HIDDEN);

    s_cardTitle = lv_label_create(s_card);
    lv_obj_set_style_text_font(s_cardTitle, F16(), 0);
    lv_obj_set_style_text_color(s_cardTitle, UI_INK, 0);
    lv_obj_align(s_cardTitle, LV_ALIGN_TOP_LEFT, UI_S(0), UI_S(0));

    s_cardL = lv_label_create(s_card);
    lv_obj_set_style_text_font(s_cardL, F14(), 0);
    lv_obj_set_style_text_color(s_cardL, UI_SOFT, 0);
    lv_obj_align(s_cardL, LV_ALIGN_TOP_LEFT, 0, s_bigText ? 30 : 26);

    s_cardR = lv_label_create(s_card);
    lv_obj_set_style_text_font(s_cardR, F14(), 0);
    lv_obj_set_style_text_color(s_cardR, UI_SOFT, 0);
    lv_obj_align(s_cardR, LV_ALIGN_TOP_LEFT, s_bigText ? 160 : 150, s_bigText ? 30 : 26);

    s_cardRoute = lv_label_create(s_card);
    lv_obj_set_style_text_font(s_cardRoute, F14(), 0);
    lv_obj_set_style_text_color(s_cardRoute, UI_GREEN, 0);
    lv_obj_align(s_cardRoute, LV_ALIGN_TOP_LEFT, 0, s_bigText ? 100 : 76);


    s_cardAirline = lv_label_create(s_card);
    lv_obj_set_style_text_font(s_cardAirline, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_cardAirline, UI_SOFT, 0);
    lv_obj_set_style_text_align(s_cardAirline, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(s_cardAirline, "");
    lv_obj_align(s_cardAirline, LV_ALIGN_TOP_RIGHT, UI_S(0), UI_S(2));
    lv_obj_add_flag(s_cardAirline, LV_OBJ_FLAG_HIDDEN);

    // TRACK toggle: pins this contact to the tracked view (progress bar + ETA)
    s_cardTrackBtn = lv_btn_create(s_card);
    lv_obj_set_size(s_cardTrackBtn, UI_S(84), UI_S(26));
    lv_obj_align(s_cardTrackBtn, LV_ALIGN_BOTTOM_RIGHT, UI_S(0), UI_S(2));
    lv_obj_set_style_radius(s_cardTrackBtn, 13, 0);
    lv_obj_set_style_bg_color(s_cardTrackBtn, UI_PANEL, 0);
    lv_obj_set_style_border_color(s_cardTrackBtn, UI_GREEN, 0);
    lv_obj_set_style_border_width(s_cardTrackBtn, 1, 0);
    lv_obj_clear_flag(s_cardTrackBtn, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_add_event_cb(s_cardTrackBtn, track_btn_cb, LV_EVENT_CLICKED, nullptr);
    s_cardTrackLbl = lv_label_create(s_cardTrackBtn);
    lv_obj_set_style_text_font(s_cardTrackLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_cardTrackLbl, UI_GREEN, 0);
    lv_label_set_text(s_cardTrackLbl, "TRACK");
    lv_obj_center(s_cardTrackLbl);

    // aircraft photo + credit, floating above the card (hidden until one loads)
    s_photo = lv_canvas_create(s_tileRadar);
    lv_obj_set_style_radius(s_photo, 6, 0);
    lv_obj_set_style_clip_corner(s_photo, true, 0);
    lv_obj_set_style_border_color(s_photo, UI_GREEN, 0);
    lv_obj_set_style_border_opa(s_photo, 170, 0);
    lv_obj_set_style_border_width(s_photo, 1, 0);
    lv_obj_add_flag(s_photo, LV_OBJ_FLAG_HIDDEN);

    s_photoCredit = lv_label_create(s_tileRadar);
    lv_obj_set_style_text_font(s_photoCredit, F12(), 0);
    lv_obj_set_style_text_color(s_photoCredit, UI_DIM, 0);
    lv_label_set_text(s_photoCredit, "");
    lv_obj_add_flag(s_photoCredit, LV_OBJ_FLAG_HIDDEN);

    // --- AIS vessel card (compact; shares the aircraft card's slot) ---
    s_vesselCard = lv_obj_create(s_tileRadar);
    lv_obj_remove_style_all(s_vesselCard);
    lv_obj_set_size(s_vesselCard, UI_S(300), UI_S(96));
    lv_obj_align(s_vesselCard, LV_ALIGN_CENTER, UI_S(0), UI_S(76));
    lv_obj_set_style_bg_color(s_vesselCard, UI_PANEL, 0);
    lv_obj_set_style_bg_opa(s_vesselCard, 235, 0);
    lv_obj_set_style_radius(s_vesselCard, 14, 0);
    lv_obj_set_style_border_color(s_vesselCard, lv_color_hex(0x35D6FF), 0);
    lv_obj_set_style_border_opa(s_vesselCard, 120, 0);
    lv_obj_set_style_border_width(s_vesselCard, 1, 0);
    lv_obj_set_style_pad_all(s_vesselCard, 12, 0);
    lv_obj_add_flag(s_vesselCard, LV_OBJ_FLAG_CLICKABLE);   // consume taps (don't deselect)
    lv_obj_clear_flag(s_vesselCard, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_vesselCard, LV_OBJ_FLAG_HIDDEN);

    s_vesselTitle = lv_label_create(s_vesselCard);
    lv_obj_set_style_text_font(s_vesselTitle, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_vesselTitle, lv_color_hex(0x9BE9FF), 0);
    lv_label_set_text(s_vesselTitle, "");
    lv_obj_align(s_vesselTitle, LV_ALIGN_TOP_LEFT, UI_S(0), UI_S(0));

    s_vesselBody = lv_label_create(s_vesselCard);
    lv_obj_set_style_text_font(s_vesselBody, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_vesselBody, UI_SOFT, 0);
    lv_label_set_text(s_vesselBody, "");
    lv_obj_align(s_vesselBody, LV_ALIGN_TOP_LEFT, UI_S(0), UI_S(24));
}

void ui_show_view(int idx) {
    if (s_tv && idx >= 0 && idx <= 7) lv_obj_set_tile_id(s_tv, (uint32_t)idx, 0, LV_ANIM_OFF);
}

// ------------------------------------------------------------------- splash
// Painted into a PSRAM canvas rather than stacked out of LVGL widgets: a gradient sky,
// soft-edged clouds and a rotated aircraft silhouette are all per-pixel work that the
// widget set cannot express. 466x466 RGB565 is ~434 KB, which PSRAM has to spare, and
// it is freed the moment the splash fades out.
static lv_color_t *s_splashBuf = nullptr;

static inline void sp_blend(lv_color_t *b, int x, int y, lv_color_t c, int a) {
    if (a <= 0 || x < 0 || y < 0 || x >= SCREEN_W || y >= SCREEN_H) return;
    lv_color_t *p = &b[y * SCREEN_W + x];
    *p = (a >= 255) ? c : lv_color_mix(c, *p, (uint8_t)a);
}

// Filled disc with a feathered edge — the building block for clouds and glows.
static void sp_disc(lv_color_t *b, float cx, float cy, float r, lv_color_t c, int a, float feather) {
    const int x0 = (int)(cx - r - feather), x1 = (int)(cx + r + feather);
    const int y0 = (int)(cy - r - feather), y1 = (int)(cy + r + feather);
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            const float d  = sqrtf(dx * dx + dy * dy);
            if (d > r + feather) continue;
            int aa = a;
            if (d > r && feather > 0.0f) aa = (int)(a * (1.0f - (d - r) / feather));
            sp_blend(b, x, y, c, aa);
        }
    }
}

static void sp_ring(lv_color_t *b, float cx, float cy, float r, float th, lv_color_t c, int a) {
    const int x0 = (int)(cx - r - th), x1 = (int)(cx + r + th);
    const int y0 = (int)(cy - r - th), y1 = (int)(cy + r + th);
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            const float e  = fabsf(sqrtf(dx * dx + dy * dy) - r);
            if (e > th) continue;
            sp_blend(b, x, y, c, (int)(a * (1.0f - e / th)));
        }
    }
}

// Convex polygon scanline fill. The aircraft is assembled from a few of these.
static void sp_poly(lv_color_t *b, const float *px, const float *py, int n, lv_color_t c, int a) {
    float miny = 1e9f, maxy = -1e9f;
    for (int i = 0; i < n; ++i) { if (py[i] < miny) miny = py[i]; if (py[i] > maxy) maxy = py[i]; }
    int y0 = (int)floorf(miny), y1 = (int)ceilf(maxy);
    if (y0 < 0) y0 = 0;
    if (y1 > SCREEN_H - 1) y1 = SCREEN_H - 1;
    for (int y = y0; y <= y1; ++y) {
        const float fy = y + 0.5f;
        float xs[10]; int cnt = 0;
        for (int i = 0; i < n && cnt < 10; ++i) {
            const int j = (i + 1) % n;
            const float ya = py[i], yb = py[j];
            if ((ya <= fy && yb > fy) || (yb <= fy && ya > fy))
                xs[cnt++] = px[i] + (fy - ya) / (yb - ya) * (px[j] - px[i]);
        }
        if (cnt < 2) continue;
        for (int i = 0; i < cnt - 1; ++i)
            for (int k = i + 1; k < cnt; ++k)
                if (xs[k] < xs[i]) { const float t = xs[i]; xs[i] = xs[k]; xs[k] = t; }
        for (int i = 0; i + 1 < cnt; i += 2) {
            const int xa = (int)ceilf(xs[i] - 0.5f), xb = (int)floorf(xs[i + 1] - 0.5f);
            for (int x = xa; x <= xb; ++x) sp_blend(b, x, y, c, a);
        }
    }
}

// Rotate a local-space shape about (0,0), scale, translate, then fill.
static void sp_shape(lv_color_t *b, const float *lx, const float *ly, int n,
                     float ox, float oy, float rot, float sc, lv_color_t c, int a) {
    float px[10], py[10];
    const float s = sinf(rot), co = cosf(rot);
    for (int i = 0; i < n; ++i) {
        px[i] = ox + (lx[i] * co - ly[i] * s) * sc;
        py[i] = oy + (lx[i] * s  + ly[i] * co) * sc;
    }
    sp_poly(b, px, py, n, c, a);
}

static void splash_paint(lv_color_t *b) {
    // Every coordinate below was picked against the 466 px panel. K maps them onto
    // whatever this board actually has; without it the whole painting bunches into
    // the top-left corner of a larger screen.
    const float K = (float)SCREEN_W / (float)UI_DESIGN_W;
    // --- sky: vertical gradient from deep night down to a warm horizon ---
    struct { int y; uint32_t rgb; } stop[] = {
        {           0, 0x050A1E }, { (int)(120*K), 0x0C2350 }, { (int)(230*K), 0x1B4E8C },
        { (int)(320*K), 0x3E82B8 }, { (int)(384*K), 0xE79A55 }, { (int)(430*K), 0xF8C87C },
        {    SCREEN_H, 0xFFDCA8 },
    };
    const int nstop = (int)(sizeof(stop) / sizeof(stop[0]));
    for (int y = 0; y < SCREEN_H; ++y) {
        int k = 0;
        while (k < nstop - 2 && y > stop[k + 1].y) ++k;
        const float t = (float)(y - stop[k].y) / (float)(stop[k + 1].y - stop[k].y);
        const lv_color_t c = lv_color_mix(lv_color_hex(stop[k + 1].rgb),
                                          lv_color_hex(stop[k].rgb),
                                          (uint8_t)(t * 255.0f));
        for (int x = 0; x < SCREEN_W; ++x) b[y * SCREEN_W + x] = c;
    }

    // --- stars, fading out as the sky brightens toward the horizon ---
    uint32_t rnd = 0x5EED1234;
    for (int i = 0; i < 90; ++i) {
        rnd = rnd * 1664525u + 1013904223u;
        const int sx = (int)((rnd >> 8) % SCREEN_W);
        rnd = rnd * 1664525u + 1013904223u;
        const int starBand = (int)(250 * K);
        const int sy = (int)((rnd >> 8) % starBand);
        const int a  = 40 + (int)((rnd >> 4) % 150);
        sp_blend(b, sx, sy, lv_color_hex(0xFFFFFF), a * (starBand - sy) / starBand);
    }

    // --- clouds: overlapping feathered discs, lit warm from below ---
    const struct { float x, y, r; int a; uint32_t c; } cloud[] = {
        {  70, 352, 30, 150, 0xFFE2C0 }, { 104, 344, 38, 160, 0xFFEBD2 }, { 140, 354, 26, 145, 0xFFDDB8 },
        { 330, 336, 34, 150, 0xFFE6C8 }, { 366, 330, 26, 140, 0xFFF0DC }, { 300, 344, 22, 130, 0xFFDCB4 },
        { 210, 392, 30, 120, 0xFFD9AE }, { 250, 398, 24, 110, 0xFFE4C4 }, { 172, 398, 22, 110, 0xFFD2A4 },
    };
    for (unsigned i = 0; i < sizeof(cloud) / sizeof(cloud[0]); ++i)
        sp_disc(b, cloud[i].x * K, cloud[i].y * K, cloud[i].r * K,
                lv_color_hex(cloud[i].c), cloud[i].a, 16.0f * K);

    // --- radar scope, sitting in the darker upper sky for contrast ---
    const float scx = SCREEN_CX, scy = 196.0f * K;
    const float scR = 132.0f * K;
    const lv_color_t cyan = lv_color_hex(0x35E8FF);
    sp_disc(b, scx, scy, scR, lv_color_hex(0x03121F), 120, 10.0f * K);   // scope glass
    for (int i = 1; i <= 3; ++i) sp_ring(b, scx, scy, 44.0f * K * i, 1.6f * K, cyan, 150);
    sp_ring(b, scx, scy, scR, 2.4f * K, cyan, 200);
    for (int i = 0; i < 4; ++i) {                                        // crosshair
        const float a = (float)i * (float)M_PI / 2.0f;
        for (float d = 6 * K; d < scR; d += 1.0f)
            sp_blend(b, (int)(scx + cosf(a) * d), (int)(scy + sinf(a) * d), cyan, 60);
    }

    // --- sweep wedge, trailing behind the leading edge ---
    const float lead = -0.62f, span = 1.15f;
    const float scRo = scR + 1.0f;
    for (int y = (int)(scy - scRo); y <= (int)(scy + scRo); ++y) {
        for (int x = (int)(scx - scRo); x <= (int)(scx + scRo); ++x) {
            const float dx = x + 0.5f - scx, dy = y + 0.5f - scy;
            const float d = sqrtf(dx * dx + dy * dy);
            if (d > scR - 1.0f) continue;
            float da = lead - atan2f(dy, dx);
            while (da < 0) da += 2.0f * (float)M_PI;
            while (da > 2.0f * (float)M_PI) da -= 2.0f * (float)M_PI;
            if (da > span) continue;
            const int a = (int)(120.0f * (1.0f - da / span) * (1.0f - d / (scR - 1.0f) * 0.35f));
            sp_blend(b, x, y, lv_color_hex(0x2BFF9E), a);
        }
    }
    const float blip[][2] = { {-58, -34}, {40, -62}, {74, 30}, {-30, 66} };
    for (unsigned i = 0; i < sizeof(blip) / sizeof(blip[0]); ++i) {
        sp_disc(b, scx + blip[i][0] * K, scy + blip[i][1] * K, 6.0f * K,
                lv_color_hex(0x2BFF9E), 70, 5.0f * K);
        sp_disc(b, scx + blip[i][0] * K, scy + blip[i][1] * K, 2.4f * K,
                lv_color_hex(0xCFFFE6), 255, 1.2f * K);
    }

    // --- the aircraft: airliner planform, banking across the scope ---
    const float rot = -0.34f, sc = 1.32f * K, px_ = scx - 6 * K, py_ = scy + 4 * K;
    const lv_color_t body = lv_color_hex(0xF2F7FF), shade = lv_color_hex(0x9FB6D4);
    const float wingL[6] = {  8,  -6, -30, -22,  -2,   6 };
    const float wingLy[6]= { -4, -10, -54, -56, -12,  -4 };
    const float wingR[6] = {  8,  -6, -30, -22,  -2,   6 };
    const float wingRy[6]= {  4,  10,  54,  56,  12,   4 };
    sp_shape(b, wingL, wingLy, 6, px_, py_, rot, sc, shade, 250);
    sp_shape(b, wingR, wingRy, 6, px_, py_, rot, sc, body,  250);
    const float tailL[4] = { -36, -50, -44, -34 };
    const float tailLy[4]= {  -3, -22, -24,  -3 };
    const float tailR[4] = { -36, -50, -44, -34 };
    const float tailRy[4]= {   3,  22,  24,   3 };
    sp_shape(b, tailL, tailLy, 4, px_, py_, rot, sc, shade, 250);
    sp_shape(b, tailR, tailRy, 4, px_, py_, rot, sc, body,  250);
    // Slung just ahead of the swept leading edge. At this span the wing only occupies
    // x -17..-10, so anything further forward floats free of the aircraft.
    const float nacL[4] = { -24, -13, -13, -24 };
    const float nacLy[4]= { -28, -29, -35, -34 };
    const float nacR[4] = { -24, -13, -13, -24 };
    const float nacRy[4]= {  28,  29,  35,  34 };
    sp_shape(b, nacL, nacLy, 4, px_, py_, rot, sc, lv_color_hex(0x4E6B8C), 250);
    sp_shape(b, nacR, nacRy, 4, px_, py_, rot, sc, lv_color_hex(0x5E7EA4), 250);
    const float fus[8]  = {  54,  44,  10, -46, -52, -46,  10,  44 };
    const float fusy[8] = {   0,  -6,  -9,  -7,   0,   7,   9,   6 };
    sp_shape(b, fus, fusy, 8, px_, py_, rot, sc, body, 255);
    const float fin[4]  = { -34, -50, -44, -30 };
    const float finy[4] = {  -1, -12, -13,  -1 };
    sp_shape(b, fin, finy, 4, px_, py_, rot, sc, lv_color_hex(0x6FA8DC), 250);

    // --- vignette: the panel is round, so fade the unreachable corners to black ---
    for (int y = 0; y < SCREEN_H; ++y) {
        for (int x = 0; x < SCREEN_W; ++x) {
            const float dx = x - SCREEN_CX, dy = y - SCREEN_CY;
            const float d = sqrtf(dx * dx + dy * dy);
            const float vIn = 196.0f * K, vFade = 37.0f * K;
            if (d < vIn) continue;
            int a = (int)((d - vIn) / vFade * 255.0f);
            if (a > 255) a = 255;
            sp_blend(b, x, y, lv_color_black(), a);
        }
    }
}

static void splash_fade_cb(void *obj, int32_t v) { lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, 0); }
static void splash_del_cb(lv_anim_t *a) {
    lv_obj_del((lv_obj_t *)a->var);
    if (s_splashBuf) { heap_caps_free(s_splashBuf); s_splashBuf = nullptr; }
}

static void splash_dismiss_cb(lv_timer_t *t) {
    lv_obj_t *cont = (lv_obj_t *)t->user_data;
    lv_timer_del(t);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, cont);
    lv_anim_set_exec_cb(&a, splash_fade_cb);
    lv_anim_set_values(&a, 255, 0);
    lv_anim_set_time(&a, 800);
    lv_anim_set_ready_cb(&a, splash_del_cb);
    lv_anim_start(&a);
}

void ui_splash_show(void) {
    lv_obj_t *cont = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, SCREEN_W, SCREEN_H);
    lv_obj_center(cont);
    lv_obj_set_style_bg_color(cont, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    // Painted artwork. If PSRAM is momentarily unavailable the splash still shows its
    // text over black rather than failing to boot.
    s_splashBuf = (lv_color_t *)heap_caps_malloc((size_t)SCREEN_W * SCREEN_H * sizeof(lv_color_t),
                                                 MALLOC_CAP_SPIRAM);
    if (s_splashBuf) {
        splash_paint(s_splashBuf);
        lv_obj_t *cv = lv_canvas_create(cont);
        lv_canvas_set_buffer(cv, s_splashBuf, SCREEN_W, SCREEN_H, LV_IMG_CF_TRUE_COLOR);
        lv_obj_center(cv);
    }

    // Title, with a dark plate behind it so it stays legible over the horizon glow.
    lv_obj_t *plate = lv_obj_create(cont);
    lv_obj_remove_style_all(plate);
    lv_obj_set_size(plate, UI_S(306), UI_S(112));
    lv_obj_align(plate, LV_ALIGN_CENTER, UI_S(0), UI_S(132));
    lv_obj_set_style_radius(plate, 18, 0);
    lv_obj_set_style_bg_color(plate, lv_color_hex(0x02070F), 0);
    lv_obj_set_style_bg_opa(plate, 180, 0);
    lv_obj_set_style_border_color(plate, lv_color_hex(0x35E8FF), 0);
    lv_obj_set_style_border_opa(plate, 90, 0);
    lv_obj_set_style_border_width(plate, 1, 0);
    lv_obj_clear_flag(plate, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(cont);
    lv_label_set_text(title, "SKYGLASS");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xEAF8FF), 0);
    lv_obj_set_style_text_letter_space(title, 4, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, UI_S(0), UI_S(122));

    lv_obj_t *sub = lv_label_create(cont);
    lv_label_set_text(sub, "LIVE AIRSPACE MONITOR");
    lv_obj_set_style_text_font(sub, F12(), 0);
    lv_obj_set_style_text_color(sub, lv_color_hex(0x63D8FF), 0);
    lv_obj_set_style_text_letter_space(sub, 2, 0);
    lv_obj_align(sub, LV_ALIGN_CENTER, UI_S(0), UI_S(152));

    // On the plate, not the bezel: at the bottom of the screen this sat on the horizon
    // glow and was effectively invisible.
    lv_obj_t *ver = lv_label_create(cont);
    lv_label_set_text(ver, "v" FW_VERSION);
    lv_obj_set_style_text_font(ver, F12(), 0);
    lv_obj_set_style_text_color(ver, lv_color_hex(0xBFD8E8), 0);
    lv_obj_align(ver, LV_ALIGN_CENTER, UI_S(0), UI_S(176));

    // Hold splash screen visible for 6.0 seconds minimum before fading
    lv_timer_t *t = lv_timer_create(splash_dismiss_cb, 6000, cont);
    lv_timer_set_repeat_count(t, 1);
}

void ui_create(void) {
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    s_tv = lv_tileview_create(scr);
    lv_obj_set_size(s_tv, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(s_tv, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_tv, LV_OPA_COVER, 0);
    lv_obj_set_scrollbar_mode(s_tv, LV_SCROLLBAR_MODE_OFF);

    s_tileRadar = lv_tileview_add_tile(s_tv, 0, 0, LV_DIR_RIGHT);
    s_tileList  = lv_tileview_add_tile(s_tv, 1, 0, LV_DIR_HOR);
    s_tileStats = lv_tileview_add_tile(s_tv, 2, 0, LV_DIR_HOR);
    s_tileWeather = lv_tileview_add_tile(s_tv, 3, 0, LV_DIR_HOR);
    s_tileTracked = lv_tileview_add_tile(s_tv, 4, 0, LV_DIR_HOR);
    s_tileClock = lv_tileview_add_tile(s_tv, 5, 0, LV_DIR_HOR);
    s_tileFires = lv_tileview_add_tile(s_tv, 6, 0, LV_DIR_HOR);
    s_tileAbout = lv_tileview_add_tile(s_tv, 7, 0, LV_DIR_LEFT);
    // Rebuild the list/stats with the latest data the moment they slide into view
    // (between polls they'd otherwise show whatever was there when last visible).
    lv_obj_add_event_cb(s_tv, [](lv_event_t *) { refresh_active_tile(); }, LV_EVENT_VALUE_CHANGED, nullptr);

    // --- radar tile ---
    lv_obj_clear_flag(s_tileRadar, LV_OBJ_FLAG_SCROLLABLE);
    radar::init(s_tileRadar);
    radar::setRangeLabelVisible(false);                     // the zoom button shows the range instead
    lv_obj_add_flag(s_tileRadar, LV_OBJ_FLAG_CLICKABLE);     // receive taps (planes/empty)
    lv_obj_add_event_cb(s_tileRadar, radar_clicked_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(s_tileRadar, radar_press_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_tileRadar, radar_longpress_cb, LV_EVENT_LONG_PRESSED, NULL);
    build_card();

    // on-screen range/zoom button (reliable single tap; bottom, above the 'S' marker)
    s_zoomBtn = lv_btn_create(s_tileRadar);
    lv_obj_set_size(s_zoomBtn, UI_S(120), UI_S(44));
    lv_obj_set_ext_click_area(s_zoomBtn, 18);   // invisibly enlarge the tap target (easier to hit)
    lv_obj_align(s_zoomBtn, LV_ALIGN_BOTTOM_MID, UI_S(0), UI_S(-32));
    lv_obj_set_style_radius(s_zoomBtn, 18, 0);
    lv_obj_set_style_bg_color(s_zoomBtn, UI_PANEL, 0);
    lv_obj_set_style_bg_opa(s_zoomBtn, 225, 0);
    lv_obj_set_style_border_color(s_zoomBtn, UI_GREEN, 0);
    lv_obj_set_style_border_width(s_zoomBtn, 1, 0);
    lv_obj_set_style_border_opa(s_zoomBtn, 170, 0);
    lv_obj_clear_flag(s_zoomBtn, LV_OBJ_FLAG_SCROLL_CHAIN);  // tapping it must not swipe the tileview
    lv_obj_add_event_cb(s_zoomBtn, zoom_cb, LV_EVENT_PRESSED, NULL);  // fire on touch-down, not release
    s_zoomLbl = lv_label_create(s_zoomBtn);
    lv_label_set_text(s_zoomLbl, LV_SYMBOL_LOOP " --");   // replaced by ui_set_range_km()
    lv_obj_set_style_text_font(s_zoomLbl, F14(), 0);
    lv_obj_set_style_text_color(s_zoomLbl, UI_GREEN, 0);
    lv_obj_center(s_zoomLbl);

    // top status HUD (wifi / aircraft count / clock); white reads on both themes.
    // WiFi is a 4-bar signal meter: bar count = RSSI strength, colour = feed health.
    s_hudWifi = lv_obj_create(s_tileRadar);
    lv_obj_remove_style_all(s_hudWifi);
    lv_obj_set_size(s_hudWifi, UI_S(21), UI_S(14));
    lv_obj_align(s_hudWifi, LV_ALIGN_TOP_MID, UI_S(-94), UI_S(50));
    lv_obj_clear_flag(s_hudWifi, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    for (int i = 0; i < 4; ++i) {
        s_hudBars[i] = lv_obj_create(s_hudWifi);
        lv_obj_remove_style_all(s_hudBars[i]);
        lv_obj_set_size(s_hudBars[i], UI_S(3), (lv_coord_t)UI_S(4 + i * 3));   // 4, 7, 10, 13 px at 466
        lv_obj_align(s_hudBars[i], LV_ALIGN_BOTTOM_LEFT, (lv_coord_t)UI_S(i * 5), 0);
        lv_obj_set_style_radius(s_hudBars[i], 1, 0);
        lv_obj_set_style_bg_color(s_hudBars[i], UI_INK, 0);
        lv_obj_set_style_bg_opa(s_hudBars[i], LV_OPA_COVER, 0);
        lv_obj_clear_flag(s_hudBars[i], LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    }

    s_hudGps = lv_label_create(s_tileRadar);     // GPS satellite icon (between WiFi bars and count)
    lv_obj_set_style_text_font(s_hudGps, F14(), 0);
    lv_obj_set_style_text_color(s_hudGps, UI_GREEN, 0);
    lv_label_set_text(s_hudGps, "");             // hidden until ui_set_gps() says GPS is on
    lv_obj_align(s_hudGps, LV_ALIGN_TOP_MID, UI_S(-62), UI_S(50));

    s_hudCount = lv_label_create(s_tileRadar);
    lv_obj_set_style_text_font(s_hudCount, F14(), 0);
    lv_obj_set_style_text_color(s_hudCount, UI_INK, 0);
    lv_label_set_text(s_hudCount, "0");
    lv_obj_align(s_hudCount, LV_ALIGN_TOP_MID, UI_S(-34), UI_S(50));

    s_hudClock = lv_label_create(s_tileRadar);
    lv_obj_set_style_text_font(s_hudClock, F14(), 0);
    lv_obj_set_style_text_color(s_hudClock, UI_INK, 0);
    lv_label_set_text(s_hudClock, "--:--");
    lv_obj_align(s_hudClock, LV_ALIGN_TOP_MID, UI_S(30), UI_S(50));

    s_hudBatt = lv_label_create(s_tileRadar);
    lv_obj_set_style_text_font(s_hudBatt, F14(), 0);
    lv_obj_set_style_text_color(s_hudBatt, UI_INK, 0);
    lv_label_set_text(s_hudBatt, "");
    lv_obj_align(s_hudBatt, LV_ALIGN_TOP_MID, UI_S(92), UI_S(50));

    s_hudDate = lv_label_create(s_tileRadar);
    lv_obj_set_style_text_font(s_hudDate, F12(), 0);
    lv_obj_set_style_text_color(s_hudDate, UI_INK, 0);
    lv_obj_set_style_text_opa(s_hudDate, 140, 0);
    lv_label_set_text(s_hudDate, "");
    lv_obj_align(s_hudDate, LV_ALIGN_TOP_MID, UI_S(0), UI_S(70));

    // --- list tile (circular panel, clipped to the round screen) ---
    lv_obj_t *lp = make_round_panel(s_tileList);
    s_listTitle = make_tile_title(lp, "AIRCRAFT");
    s_list = lv_list_create(lp);
    // Scaled: this stayed 300 px wide on a larger panel while the font stepped up a
    // tier, so the distance column clipped. The UI_S() pass missed it because the width
    // is a ternary rather than a plain literal.
    lv_obj_set_size(s_list, UI_S(s_bigText ? 340 : 300), UI_S(372));
    lv_obj_align(s_list, LV_ALIGN_CENTER, UI_S(0), UI_S(22));
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_pad_row(s_list, 2, 0);

    // --- stats tile (circular panel) ---
    lv_obj_t *sp = make_round_panel(s_tileStats);
    make_tile_title(sp, "STATS");
    s_statsLbl = lv_label_create(sp);
    lv_obj_set_style_text_font(s_statsLbl, F16(), 0);
    lv_obj_set_style_text_color(s_statsLbl, UI_SOFT, 0);
    lv_label_set_text(s_statsLbl, "Aircraft   0");
    lv_obj_align(s_statsLbl, LV_ALIGN_CENTER, UI_S(0), UI_S(-16));

    s_statsGps = lv_label_create(sp);               // GPS status line (hidden unless GPS is on)
    lv_obj_set_style_text_font(s_statsGps, F14(), 0);
    lv_obj_set_style_text_color(s_statsGps, UI_SOFT, 0);
    lv_obj_set_style_text_align(s_statsGps, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_statsGps, "");
    lv_obj_align(s_statsGps, LV_ALIGN_CENTER, UI_S(0), UI_S(90));

    // footer: where to reach the configuration page (IP / hostname / setup AP)
    s_statsNet = lv_label_create(sp);
    lv_obj_set_width(s_statsNet, UI_S(320));
    lv_obj_set_style_text_font(s_statsNet, F14(), 0);
    lv_obj_set_style_text_color(s_statsNet, UI_GREEN, 0);
    lv_obj_set_style_text_align(s_statsNet, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_statsNet, "");
    lv_obj_align(s_statsNet, LV_ALIGN_CENTER, UI_S(0), UI_S(132));

    lv_obj_t *ver = lv_label_create(sp);            // firmware version (so users can tell what's flashed)
    lv_obj_set_style_text_font(ver, F12(), 0);
    lv_obj_set_style_text_color(ver, UI_DIM, 0);
    lv_label_set_text(ver, "SkyGlass v" FW_VERSION);
    lv_obj_align(ver, LV_ALIGN_CENTER, UI_S(0), UI_S(170));

    // --- weather tile (current conditions + next three days) ---
    lv_obj_t *wp = make_round_panel(s_tileWeather);
    lv_obj_set_style_bg_color(wp, lv_color_black(), 0); // hide square radar-tile bounds on AMOLED
    s_weatherTitle = make_tile_title(wp, "WX RADAR");
    lv_obj_set_style_bg_color(s_weatherTitle, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_weatherTitle, 170, 0);
    lv_obj_set_style_pad_left(s_weatherTitle, 8, 0);
    lv_obj_set_style_pad_right(s_weatherTitle, 8, 0);
    lv_obj_set_style_pad_top(s_weatherTitle, 2, 0);
    lv_obj_set_style_pad_bottom(s_weatherTitle, 2, 0);
    lv_obj_set_style_radius(s_weatherTitle, 8, 0);
    s_weatherNow = lv_label_create(wp);
    lv_obj_set_width(s_weatherNow, UI_S(330));
    lv_obj_set_style_text_font(s_weatherNow, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(s_weatherNow, UI_INK, 0);
    lv_obj_set_style_text_align(s_weatherNow, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_weatherNow, "Forecast unavailable");
    lv_obj_align(s_weatherNow, LV_ALIGN_TOP_MID, UI_S(0), UI_S(64));

    s_weatherMeta = lv_label_create(wp);
    lv_obj_set_width(s_weatherMeta, UI_S(380));
    lv_obj_set_style_text_font(s_weatherMeta, F14(), 0);
    lv_obj_set_style_text_color(s_weatherMeta, UI_SOFT, 0);
    lv_obj_set_style_text_align(s_weatherMeta, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_weatherMeta, "Waiting for WiFi data...");
    lv_obj_align(s_weatherMeta, LV_ALIGN_TOP_MID, UI_S(0), UI_S(150));

    s_weatherDays = lv_label_create(wp);
    lv_obj_set_width(s_weatherDays, UI_S(390));
    lv_obj_set_style_text_font(s_weatherDays, F16(), 0);
    lv_obj_set_style_text_color(s_weatherDays, UI_GREEN, 0);
    lv_obj_set_style_text_align(s_weatherDays, LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_text(s_weatherDays, "");
    lv_obj_align(s_weatherDays, LV_ALIGN_TOP_LEFT, UI_S(42), UI_S(234));
    // Legacy formatted labels are retained only to avoid touching older data-update
    // plumbing; the redesigned forecast uses independent aligned objects below.
    lv_obj_add_flag(s_weatherNow, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_weatherMeta, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_weatherDays, LV_OBJ_FLAG_HIDDEN);

    // Default mode: genuine precipitation radar with aviation-style overlays.
    s_wxAirport = lv_label_create(wp);
    lv_obj_set_style_text_font(s_wxAirport, F14(), 0);
    lv_obj_set_style_text_color(s_wxAirport, UI_SOFT, 0);
    lv_obj_set_style_bg_color(s_wxAirport, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_wxAirport, 160, 0);
    lv_obj_set_style_pad_left(s_wxAirport, 6, 0);
    lv_obj_set_style_pad_right(s_wxAirport, 6, 0);
    lv_obj_set_style_radius(s_wxAirport, 6, 0);
    lv_label_set_text(s_wxAirport, "RADAR CENTRE");
    lv_obj_align(s_wxAirport, LV_ALIGN_TOP_MID, UI_S(0), UI_S(46));

    s_wxCanvas = lv_canvas_create(wp);
    lv_obj_set_size(s_wxCanvas, WX_RADAR_SIZE, WX_RADAR_SIZE);
    lv_obj_align(s_wxCanvas, LV_ALIGN_TOP_MID, UI_S(0), UI_S(52));
    lv_obj_add_flag(s_wxCanvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_wxCanvas, weather_mode_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(s_wxCanvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_background(s_wxCanvas);

    s_wxStatus = lv_label_create(wp);
    lv_obj_set_style_text_font(s_wxStatus, F14(), 0);
    lv_obj_set_style_text_color(s_wxStatus, UI_DIM, 0);
    lv_label_set_text(s_wxStatus, "ACQUIRING WX RADAR...");
    lv_obj_align(s_wxStatus, LV_ALIGN_TOP_MID, UI_S(0), UI_S(222));

    const int ringSize[3] = { 360, 240, 120 };
    for (int i = 0; i < 3; ++i) {
        s_wxRings[i] = lv_obj_create(wp);
        lv_obj_remove_style_all(s_wxRings[i]);
        lv_obj_set_size(s_wxRings[i], ringSize[i], ringSize[i]);
        lv_obj_align(s_wxRings[i], LV_ALIGN_TOP_MID, 0, 52 + (360 - ringSize[i]) / 2);
        lv_obj_set_style_radius(s_wxRings[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_color(s_wxRings[i], UI_GREEN, 0);
        lv_obj_set_style_border_opa(s_wxRings[i], i == 0 ? 180 : 90, 0);
        lv_obj_set_style_border_width(s_wxRings[i], 1, 0);
        lv_obj_clear_flag(s_wxRings[i], LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    }
    s_wxNorth = lv_label_create(wp);
    lv_obj_set_style_text_font(s_wxNorth, F12(), 0);
    lv_obj_set_style_text_color(s_wxNorth, UI_GREEN, 0);
    lv_label_set_text(s_wxNorth, "N");
    lv_obj_align(s_wxNorth, LV_ALIGN_TOP_MID, UI_S(0), UI_S(58));
    s_wxCenter = lv_label_create(wp);
    lv_obj_set_style_text_font(s_wxCenter, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(s_wxCenter, UI_INK, 0);
    lv_label_set_text(s_wxCenter, "+");
    lv_obj_align(s_wxCenter, LV_ALIGN_TOP_MID, UI_S(0), UI_S(219));
    s_wxRange = lv_label_create(wp);
    lv_obj_set_style_text_font(s_wxRange, F12(), 0);
    lv_obj_set_style_text_color(s_wxRange, UI_GREEN, 0);
    lv_label_set_text(s_wxRange, "");            // filled by build_weather() in the chosen unit
    lv_obj_align(s_wxRange, LV_ALIGN_TOP_MID, UI_S(128), UI_S(225));

    s_wxFooter = lv_label_create(wp);
    lv_obj_set_width(s_wxFooter, UI_S(360));
    lv_obj_set_style_text_font(s_wxFooter, F16(), 0);
    lv_obj_set_style_text_color(s_wxFooter, UI_INK, 0);
    lv_obj_set_style_text_align(s_wxFooter, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_wxFooter, "WEATHER DATA PENDING");
    lv_obj_align(s_wxFooter, LV_ALIGN_TOP_MID, UI_S(0), UI_S(326));
    lv_obj_set_style_bg_color(s_wxFooter, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_wxFooter, 185, 0);
    lv_obj_set_style_pad_hor(s_wxFooter, 8, 0);
    lv_obj_set_style_radius(s_wxFooter, 7, 0);
    s_wxMeta = lv_label_create(wp);
    lv_obj_set_width(s_wxMeta, UI_S(360));
    lv_obj_set_style_text_font(s_wxMeta, F14(), 0);
    lv_obj_set_style_text_color(s_wxMeta, UI_SOFT, 0);
    lv_obj_set_style_text_align(s_wxMeta, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_wxMeta, "");
    lv_obj_align(s_wxMeta, LV_ALIGN_TOP_MID, UI_S(0), UI_S(351));
    lv_obj_set_style_bg_color(s_wxMeta, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_wxMeta, 185, 0);
    lv_obj_set_style_pad_hor(s_wxMeta, 8, 0);
    lv_obj_set_style_radius(s_wxMeta, 7, 0);
    s_wxAttrib = lv_label_create(wp);
    lv_obj_set_style_text_font(s_wxAttrib, F12(), 0);
    lv_obj_set_style_text_color(s_wxAttrib, UI_DIM, 0);
    lv_label_set_text(s_wxAttrib, "WAITING FOR RADAR DATA");
    lv_obj_align(s_wxAttrib, LV_ALIGN_TOP_MID, UI_S(0), UI_S(376));
    lv_obj_set_style_bg_color(s_wxAttrib, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_wxAttrib, 170, 0);
    lv_obj_set_style_pad_hor(s_wxAttrib, 6, 0);
    lv_obj_set_style_radius(s_wxAttrib, 6, 0);

    // Forecast mode: independent, aligned objects instead of a tiny text table.
    s_fcIcon = lv_obj_create(wp);                 // current conditions, beside the temperature
    weather_icon_attach(s_fcIcon);
    lv_obj_set_size(s_fcIcon, UI_S(58), UI_S(58));
    lv_obj_align(s_fcIcon, LV_ALIGN_TOP_MID, UI_S(-78), UI_S(62));

    s_fcCurrent = lv_label_create(wp);
    lv_obj_set_style_text_font(s_fcCurrent, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(s_fcCurrent, UI_INK, 0);
    lv_label_set_text(s_fcCurrent, "-- C");
    lv_obj_align(s_fcCurrent, LV_ALIGN_TOP_MID, UI_S(22), UI_S(68));
    s_fcCondition = lv_label_create(wp);
    lv_obj_set_style_text_font(s_fcCondition, F16(), 0);
    lv_obj_set_style_text_color(s_fcCondition, UI_SOFT, 0);
    lv_label_set_text(s_fcCondition, "Waiting for data");
    lv_obj_align(s_fcCondition, LV_ALIGN_TOP_MID, UI_S(0), UI_S(105));

    const char *metricNames[3] = { "FEELS", "HUMIDITY", "WIND" };
    const int colX[3] = { UI_S(-122), 0, UI_S(122) };
    for (int i = 0; i < 3; ++i) {
        s_fcMetricName[i] = lv_label_create(wp);
        lv_obj_set_style_text_font(s_fcMetricName[i], F12(), 0);
        lv_obj_set_style_text_color(s_fcMetricName[i], UI_DIM, 0);
        lv_label_set_text(s_fcMetricName[i], metricNames[i]);
        lv_obj_align(s_fcMetricName[i], LV_ALIGN_TOP_MID, colX[i], UI_S(150));
        s_fcMetricValue[i] = lv_label_create(wp);
        lv_obj_set_style_text_font(s_fcMetricValue[i], F16(), 0);
        lv_obj_set_style_text_color(s_fcMetricValue[i], UI_INK, 0);
        lv_label_set_text(s_fcMetricValue[i], "-");
        lv_obj_align(s_fcMetricValue[i], LV_ALIGN_TOP_MID, colX[i], UI_S(170));

        s_fcDay[i] = lv_label_create(wp);
        lv_obj_set_style_text_font(s_fcDay[i], F16(), 0);
        lv_obj_set_style_text_color(s_fcDay[i], UI_GREEN, 0);
        lv_label_set_text(s_fcDay[i], "---");
        lv_obj_align(s_fcDay[i], LV_ALIGN_TOP_MID, colX[i], UI_S(212));

        s_fcDayIcon[i] = lv_obj_create(wp);
        weather_icon_attach(s_fcDayIcon[i]);
        lv_obj_set_size(s_fcDayIcon[i], UI_S(44), UI_S(44));
        lv_obj_align(s_fcDayIcon[i], LV_ALIGN_TOP_MID, colX[i], UI_S(230));
        s_fcDayCondition[i] = lv_label_create(wp);
        lv_obj_set_width(s_fcDayCondition[i], UI_S(116));
        lv_obj_set_style_text_font(s_fcDayCondition[i], F12(), 0);
        lv_obj_set_style_text_color(s_fcDayCondition[i], UI_SOFT, 0);
        lv_obj_set_style_text_align(s_fcDayCondition[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(s_fcDayCondition[i], LV_LABEL_LONG_WRAP);
        lv_label_set_text(s_fcDayCondition[i], "");
        lv_obj_align(s_fcDayCondition[i], LV_ALIGN_TOP_MID, colX[i], UI_S(276));
        s_fcDayTemp[i] = lv_label_create(wp);
        lv_obj_set_style_text_font(s_fcDayTemp[i], F14(), 0);
        lv_obj_set_style_text_color(s_fcDayTemp[i], UI_INK, 0);
        lv_label_set_text(s_fcDayTemp[i], "");
        lv_obj_align(s_fcDayTemp[i], LV_ALIGN_TOP_MID, colX[i], UI_S(304));
        s_fcDayRain[i] = lv_label_create(wp);
        lv_obj_set_style_text_font(s_fcDayRain[i], F12(), 0);
        lv_obj_set_style_text_color(s_fcDayRain[i], lv_color_hex(0x4DDCFF), 0);
        lv_label_set_text(s_fcDayRain[i], "");
        lv_obj_align(s_fcDayRain[i], LV_ALIGN_TOP_MID, colX[i], UI_S(328));
    }
    s_fcUpdated = lv_label_create(wp);
    lv_obj_set_style_text_font(s_fcUpdated, F12(), 0);
    lv_obj_set_style_text_color(s_fcUpdated, UI_DIM, 0);
    lv_label_set_text(s_fcUpdated, "");
    lv_obj_align(s_fcUpdated, LV_ALIGN_TOP_MID, UI_S(0), UI_S(365));

    s_weatherModeBtn = lv_btn_create(wp);
    lv_obj_set_size(s_weatherModeBtn, UI_S(164), UI_S(34));
    lv_obj_align(s_weatherModeBtn, LV_ALIGN_BOTTOM_MID, UI_S(0), UI_S(-18));
    lv_obj_set_style_radius(s_weatherModeBtn, 17, 0);
    lv_obj_set_style_bg_color(s_weatherModeBtn, UI_PANEL, 0);
    lv_obj_set_style_border_color(s_weatherModeBtn, UI_GREEN, 0);
    lv_obj_set_style_border_width(s_weatherModeBtn, 1, 0);
    lv_obj_clear_flag(s_weatherModeBtn, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_add_event_cb(s_weatherModeBtn, weather_mode_cb, LV_EVENT_CLICKED, nullptr);
    s_weatherModeLbl = lv_label_create(s_weatherModeBtn);
    lv_obj_set_style_text_font(s_weatherModeLbl, F12(), 0);
    lv_obj_set_style_text_color(s_weatherModeLbl, UI_GREEN, 0);
    lv_label_set_text(s_weatherModeLbl, "3-DAY FORECAST");
    lv_obj_center(s_weatherModeLbl);

    // --- tracked tile (one flight: route, progress, ETA, live numbers) ---
    lv_obj_t *tp = make_round_panel(s_tileTracked);
    make_tile_title(tp, "TRACKED");

    s_trkTitle = lv_label_create(tp);
    lv_obj_set_width(s_trkTitle, UI_S(330));
    lv_obj_set_style_text_font(s_trkTitle, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(s_trkTitle, UI_INK, 0);
    lv_obj_set_style_text_align(s_trkTitle, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_trkTitle, "No flight tracked");
    lv_obj_align(s_trkTitle, LV_ALIGN_TOP_MID, UI_S(0), UI_S(74));

    s_trkRoute = lv_label_create(tp);
    lv_obj_set_width(s_trkRoute, UI_S(350));
    lv_obj_set_style_text_font(s_trkRoute, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_trkRoute, UI_GREEN, 0);
    lv_obj_set_style_text_align(s_trkRoute, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_trkRoute, "");
    lv_obj_align(s_trkRoute, LV_ALIGN_TOP_MID, UI_S(0), UI_S(116));

    s_trkBar = lv_bar_create(tp);
    lv_obj_set_size(s_trkBar, UI_S(300), UI_S(10));
    lv_obj_align(s_trkBar, LV_ALIGN_CENTER, UI_S(0), UI_S(-6));
    lv_obj_set_style_radius(s_trkBar, 5, 0);
    lv_obj_set_style_bg_color(s_trkBar, lv_color_hex(0x14301F), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_trkBar, UI_GREEN, LV_PART_INDICATOR);
    lv_bar_set_range(s_trkBar, 0, 100);
    lv_bar_set_value(s_trkBar, 0, LV_ANIM_OFF);
    lv_obj_add_flag(s_trkBar, LV_OBJ_FLAG_HIDDEN);

    s_trkFrom = lv_label_create(tp);
    lv_obj_set_style_text_font(s_trkFrom, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_trkFrom, UI_SOFT, 0);
    lv_label_set_text(s_trkFrom, "");
    lv_obj_align(s_trkFrom, LV_ALIGN_CENTER, UI_S(-150), UI_S(12));

    s_trkTo = lv_label_create(tp);
    lv_obj_set_style_text_font(s_trkTo, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_trkTo, UI_SOFT, 0);
    lv_obj_set_style_text_align(s_trkTo, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(s_trkTo, "");
    lv_obj_align(s_trkTo, LV_ALIGN_CENTER, UI_S(150), UI_S(12));

    s_trkPct = lv_label_create(tp);
    lv_obj_set_style_text_font(s_trkPct, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_trkPct, UI_INK, 0);
    lv_label_set_text(s_trkPct, "");
    lv_obj_align(s_trkPct, LV_ALIGN_CENTER, UI_S(0), UI_S(12));

    s_trkEta = lv_label_create(tp);
    lv_obj_set_width(s_trkEta, UI_S(340));
    lv_obj_set_style_text_font(s_trkEta, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_trkEta, UI_INK, 0);
    lv_obj_set_style_text_align(s_trkEta, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_trkEta, "");
    lv_obj_align(s_trkEta, LV_ALIGN_CENTER, UI_S(0), UI_S(44));

    s_trkStats = lv_label_create(tp);
    lv_obj_set_width(s_trkStats, UI_S(340));
    lv_obj_set_style_text_font(s_trkStats, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_trkStats, UI_SOFT, 0);
    lv_obj_set_style_text_align(s_trkStats, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_trkStats, "");
    lv_obj_align(s_trkStats, LV_ALIGN_CENTER, UI_S(0), UI_S(84));

    s_trkHint = lv_label_create(tp);
    lv_obj_set_width(s_trkHint, UI_S(320));
    lv_obj_set_style_text_font(s_trkHint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_trkHint, UI_DIM, 0);
    lv_obj_set_style_text_align(s_trkHint, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_trkHint, "Tap an aircraft on the radar,\nthen press TRACK on its card.");
    lv_obj_align(s_trkHint, LV_ALIGN_CENTER, UI_S(0), UI_S(20));

    // --- clock tile (watch face + current weather + 3-day strip) ---
    // Laid out as a watch face rather than a column of text: a seconds arc at the rim
    // (echoing the radar sweep), the time on the centre line, and the weather sitting
    // below a hairline rule so the two halves read as separate information.
    lv_obj_t *cp = make_round_panel(s_tileClock);
    lv_obj_set_style_bg_color(cp, lv_color_black(), 0);   // true black: kind to the AMOLED at night

    s_clockRing = lv_obj_create(cp);                      // static outer ring, radar-like
    lv_obj_remove_style_all(s_clockRing);
    lv_obj_set_size(s_clockRing, UI_S(442), UI_S(442));
    lv_obj_center(s_clockRing);
    lv_obj_set_style_radius(s_clockRing, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(s_clockRing, UI_GREEN, 0);
    lv_obj_set_style_border_opa(s_clockRing, 45, 0);
    lv_obj_set_style_border_width(s_clockRing, 1, 0);
    lv_obj_clear_flag(s_clockRing, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    s_clockArc = lv_arc_create(cp);                       // seconds, sweeping the rim
    lv_obj_set_size(s_clockArc, UI_S(424), UI_S(424));
    lv_obj_center(s_clockArc);
    lv_arc_set_rotation(s_clockArc, 270);                 // start at 12 o'clock
    lv_arc_set_bg_angles(s_clockArc, 0, 360);
    lv_arc_set_range(s_clockArc, 0, 59);
    lv_arc_set_value(s_clockArc, 0);
    lv_obj_remove_style(s_clockArc, NULL, LV_PART_KNOB);  // no drag handle on a clock
    lv_obj_clear_flag(s_clockArc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(s_clockArc, UI_GREEN, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(s_clockArc, 30, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_clockArc, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_clockArc, UI_GREEN, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(s_clockArc, 235, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_clockArc, 4, LV_PART_INDICATOR);

    s_clockTime = lv_label_create(cp);
    lv_obj_set_style_text_font(s_clockTime, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_clockTime, UI_INK, 0);
    lv_label_set_text(s_clockTime, "--:--");
    lv_obj_align(s_clockTime, LV_ALIGN_CENTER, UI_S(-14), UI_S(-74));

    s_clockSec = lv_label_create(cp);                     // meridiem only; seconds are the arc
    lv_obj_set_style_text_font(s_clockSec, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_clockSec, UI_GREEN, 0);
    lv_label_set_text(s_clockSec, "");
    // Anchored to the time text, not the panel. montserrat_48 is LVGL's largest built-in
    // font so the digits cannot grow with the screen; scaling this offset instead left the
    // meridiem stranded out to the right on a bigger panel.
    lv_obj_align_to(s_clockSec, s_clockTime, LV_ALIGN_OUT_RIGHT_BOTTOM, UI_S(8), UI_S(-6));

    s_clockDate = lv_label_create(cp);
    lv_obj_set_style_text_font(s_clockDate, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_clockDate, UI_SOFT, 0);
    lv_obj_set_style_text_align(s_clockDate, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_clockDate, "");
    lv_obj_align(s_clockDate, LV_ALIGN_CENTER, UI_S(0), UI_S(-34));

    s_clockRule = lv_obj_create(cp);                      // hairline between time and weather
    lv_obj_remove_style_all(s_clockRule);
    lv_obj_set_size(s_clockRule, UI_S(150), UI_S(1));
    lv_obj_align(s_clockRule, LV_ALIGN_CENTER, UI_S(0), UI_S(-8));
    lv_obj_set_style_bg_color(s_clockRule, UI_GREEN, 0);
    lv_obj_set_style_bg_opa(s_clockRule, 70, 0);
    lv_obj_clear_flag(s_clockRule, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    s_clockIcon = lv_obj_create(cp);                      // current conditions, left of the temp
    weather_icon_attach(s_clockIcon);
    lv_obj_set_size(s_clockIcon, UI_S(54), UI_S(54));
    lv_obj_align(s_clockIcon, LV_ALIGN_CENTER, UI_S(-66), UI_S(30));

    s_clockTemp = lv_label_create(cp);
    lv_obj_set_style_text_font(s_clockTemp, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(s_clockTemp, UI_INK, 0);
    lv_label_set_text(s_clockTemp, "");
    lv_obj_align(s_clockTemp, LV_ALIGN_CENTER, UI_S(24), UI_S(20));

    s_clockCond = lv_label_create(cp);
    lv_obj_set_style_text_font(s_clockCond, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_clockCond, UI_SOFT, 0);
    lv_label_set_text(s_clockCond, "");
    lv_obj_align(s_clockCond, LV_ALIGN_CENTER, UI_S(24), UI_S(46));

    // Scaled: the icons grow with UI_S() but these offsets did not, so on a larger
    // panel the rows collided. The regex pass that added UI_S() only matched calls
    // with two literal coordinates, and these pass a variable.
    const int clkColX[3] = { UI_S(-104), 0, UI_S(104) };
    for (int i = 0; i < 3; ++i) {
        s_clockDay[i] = lv_label_create(cp);
        lv_obj_set_style_text_font(s_clockDay[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(s_clockDay[i], UI_GREEN, 0);
        lv_label_set_text(s_clockDay[i], "");
        lv_obj_align(s_clockDay[i], LV_ALIGN_CENTER, clkColX[i], UI_S(90));
        s_clockDayIcon[i] = lv_obj_create(cp);
        weather_icon_attach(s_clockDayIcon[i]);
        lv_obj_set_size(s_clockDayIcon[i], UI_S(38), UI_S(38));
        lv_obj_align(s_clockDayIcon[i], LV_ALIGN_CENTER, clkColX[i], UI_S(119));
        s_clockDayTemp[i] = lv_label_create(cp);
        lv_obj_set_style_text_font(s_clockDayTemp[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(s_clockDayTemp[i], UI_SOFT, 0);
        lv_label_set_text(s_clockDayTemp[i], "");
        lv_obj_align(s_clockDayTemp[i], LV_ALIGN_CENTER, clkColX[i], UI_S(146));
    }
    lv_timer_create(clock_tick_cb, 1000, nullptr);

    // --- fires tile (continental map) ---
    lv_obj_t *fp = make_round_panel(s_tileFires);
    lv_obj_set_style_bg_color(fp, lv_color_black(), 0);
    s_fireTitle = make_tile_title(fp, "FIRES");

    s_fireBadge = lv_label_create(fp);
    lv_obj_set_style_text_font(s_fireBadge, F12(), 0);
    lv_obj_set_style_text_color(s_fireBadge, UI_CYAN, 0);
    lv_obj_set_style_bg_color(s_fireBadge, lv_color_hex(0x0A1926), 0);
    lv_obj_set_style_bg_opa(s_fireBadge, 200, 0);
    lv_obj_set_style_pad_left(s_fireBadge, 8, 0);
    lv_obj_set_style_pad_right(s_fireBadge, 8, 0);
    lv_obj_set_style_pad_top(s_fireBadge, 2, 0);
    lv_obj_set_style_pad_bottom(s_fireBadge, 2, 0);
    lv_obj_set_style_radius(s_fireBadge, 10, 0);
    lv_obj_align(s_fireBadge, LV_ALIGN_TOP_MID, UI_S(0), UI_S(48));
    lv_label_set_text(s_fireBadge, "MODIS OVERVIEW");

    s_fireLayer = lv_obj_create(fp);
    lv_obj_remove_style_all(s_fireLayer);
    lv_obj_set_size(s_fireLayer, SCREEN_W, SCREEN_H);
    lv_obj_center(s_fireLayer);
    lv_obj_add_flag(s_fireLayer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_fireLayer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_fireLayer, fire_draw_cb, LV_EVENT_DRAW_MAIN, nullptr);
    lv_obj_add_event_cb(s_fireLayer, fire_tap_cb, LV_EVENT_CLICKED, nullptr);

    s_fireInfo = lv_label_create(fp);
    lv_obj_set_width(s_fireInfo, UI_S(380));
    lv_obj_set_style_text_font(s_fireInfo, F14(), 0);
    lv_obj_set_style_text_color(s_fireInfo, UI_SOFT, 0);
    lv_obj_set_style_text_align(s_fireInfo, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_fireInfo, "waiting for data...");
    lv_obj_align(s_fireInfo, LV_ALIGN_BOTTOM_MID, UI_S(0), UI_S(-54));

    s_fireBack = lv_btn_create(fp);
    lv_obj_set_size(s_fireBack, UI_S(130), UI_S(34));
    lv_obj_align(s_fireBack, LV_ALIGN_BOTTOM_MID, UI_S(0), UI_S(-16));
    lv_obj_set_style_radius(s_fireBack, 17, 0);
    lv_obj_set_style_bg_color(s_fireBack, UI_PANEL, 0);
    lv_obj_set_style_border_color(s_fireBack, UI_GREEN, 0);
    lv_obj_set_style_border_width(s_fireBack, 1, 0);
    lv_obj_clear_flag(s_fireBack, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_add_event_cb(s_fireBack, fire_back_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(s_fireBack, LV_OBJ_FLAG_HIDDEN);

    // --- about tile (last screen: what this is, what it is running on) ---
    lv_obj_t *ap = make_round_panel(s_tileAbout);
    make_tile_title(ap, "ABOUT");

    lv_obj_t *aName = lv_label_create(ap);
    lv_label_set_text(aName, "SKYGLASS");
    lv_obj_set_style_text_font(aName, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(aName, UI_INK, 0);
    lv_obj_set_style_text_letter_space(aName, 3, 0);
    lv_obj_align(aName, LV_ALIGN_TOP_MID, UI_S(0), UI_S(54));

    lv_obj_t *aVer = lv_label_create(ap);
    lv_label_set_text(aVer, "v" FW_VERSION);
    lv_obj_set_style_text_font(aVer, F14(), 0);
    lv_obj_set_style_text_color(aVer, UI_CYAN, 0);
    lv_obj_align(aVer, LV_ALIGN_TOP_MID, UI_S(0), UI_S(90));

    // Recolour mode so the field labels can be dimmer than their values in one label.
    s_aboutBody = lv_label_create(ap);
    lv_label_set_recolor(s_aboutBody, true);
    lv_obj_set_width(s_aboutBody, UI_S(360));
    lv_obj_set_style_text_font(s_aboutBody, F12(), 0);
    lv_obj_set_style_text_color(s_aboutBody, UI_SOFT, 0);
    lv_obj_set_style_text_align(s_aboutBody, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(s_aboutBody, 2, 0);
    // Kept high enough that the last line clears the round bezel: at this width the
    // usable half-chord shrinks fast below y=370.
    lv_obj_align(s_aboutBody, LV_ALIGN_CENTER, UI_S(0), UI_S(14));
    build_about();
    lv_obj_t *bl = lv_label_create(s_fireBack);
    lv_obj_set_style_text_font(bl, F12(), 0);
    lv_obj_set_style_text_color(bl, UI_GREEN, 0);
    lv_label_set_text(bl, "WHOLE MAP");
    lv_obj_center(bl);

    lv_obj_set_tile_id(s_tv, 0, 0, LV_ANIM_OFF);

    ui_splash_show();   // branded boot splash on top (auto-fades)
}
