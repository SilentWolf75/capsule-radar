// Route lookup via adsbdb.com (free, no API key): GET /v0/callsign/{callsign}.
// Returns origin/destination city names (English). Device-only.
#include "route_client.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_heap_caps.h>
#include <string.h>
#include <time.h>   // route-cache TTL

#define ROUTE_CACHE_MAX 200   // wrap the cache before it can crowd NVS

// strip spaces -> a valid NVS key (callsigns are <= 8 chars)
static void route_key(const char *callsign, char *out, size_t on) {
    size_t j = 0;
    for (const char *p = callsign; *p && j < on - 1; ++p)
        if (*p != ' ') out[j++] = *p;
    out[j] = 0;
}

#define ROUTE_FMT_VER 3   // bump to invalidate cached routes when the stored format changes

void route_cache_begin() {
    Preferences p;
    if (!p.begin("routes", false)) return;
    if (p.getUChar("__v", 0) != ROUTE_FMT_VER) { p.clear(); p.putUChar("__v", ROUTE_FMT_VER); }
    p.end();
}

bool route_cache_get(const char *callsign, char *from, size_t fn, char *to, size_t tn,
                     RouteCoords *coords) {
    if (fn) from[0] = 0;
    if (tn) to[0] = 0;
    if (coords) *coords = RouteCoords{};
    if (!callsign || !callsign[0]) return false;
    char key[12];
    route_key(callsign, key, sizeof(key));
    if (!key[0]) return false;
    Preferences p;
    if (!p.begin("routes", true)) return false;
    // stored as "epoch|from|to" plus, when the endpoints resolved, "|flat|flon|tlat|tlon"
    String v = p.getString(key, "");
    p.end();
    if (v.length() == 0) return false;
    const int b1 = v.indexOf('|');
    if (b1 < 0) return false;
    const uint32_t ts = (uint32_t)v.substring(0, b1).toInt();
    const String rest = v.substring(b1 + 1);
    const int b2 = rest.indexOf('|');
    if (b2 < 0) return false;
    const uint32_t now = (uint32_t)time(nullptr);    // expire stale routes (reused callsigns)
    if (now > 1700000000UL && ts > 1700000000UL && (now - ts) > 86400UL) return false;  // 24 h TTL
    snprintf(from, fn, "%s", rest.substring(0, b2).c_str());
    const String tail = rest.substring(b2 + 1);
    const int b3 = tail.indexOf('|');
    if (b3 < 0) {                       // labels only (no coordinates were resolved)
        snprintf(to, tn, "%s", tail.c_str());
        return true;
    }
    snprintf(to, tn, "%s", tail.substring(0, b3).c_str());
    if (coords) {
        String c = tail.substring(b3 + 1);
        double vals[4] = {0, 0, 0, 0};
        for (int i = 0; i < 4; ++i) {
            const int sep = c.indexOf('|');
            vals[i] = (sep < 0 ? c : c.substring(0, sep)).toDouble();
            if (sep < 0) { if (i < 3) return true; break; }   // truncated: labels still valid
            c = c.substring(sep + 1);
        }
        coords->fromLat = vals[0]; coords->fromLon = vals[1];
        coords->toLat   = vals[2]; coords->toLon   = vals[3];
        coords->valid = true;
    }
    return true;
}

void route_cache_put(const char *callsign, const char *from, const char *to,
                     const RouteCoords *coords) {
    if (!callsign || !callsign[0]) return;
    char key[12];
    route_key(callsign, key, sizeof(key));
    if (!key[0]) return;
    Preferences p;
    if (!p.begin("routes", false)) return;
    int n = p.getInt("__n", 0);
    if (n >= ROUTE_CACHE_MAX) { p.clear(); n = 0; }   // wrap to bound NVS usage
    String v = String((uint32_t)time(nullptr)) + "|" + String(from ? from : "") + "|" + String(to ? to : "");
    if (coords && coords->valid) {
        char c[64];
        snprintf(c, sizeof(c), "|%.4f|%.4f|%.4f|%.4f",
                 coords->fromLat, coords->fromLon, coords->toLat, coords->toLon);
        v += c;
    }
    if (p.putString(key, v) > 0) p.putInt("__n", n + 1);
    p.end();
}

// Most recognizable short airport label: a cleaned-up name ("Teesside", "Palma de
// Mallorca", "London Heathrow"), falling back to the municipality, then the IATA code.
static void pick_airport(JsonObjectConst ap, char *out, size_t n) {
    String s = (const char *)(ap["name"] | "");
    s.replace(" International Airport", "");
    s.replace(" Regional Airport", "");
    s.replace(" Airport", "");
    s.replace(" International", "");
    s.trim();
    if (s.length() == 0 || s.length() > 18) {           // name missing or too long -> municipality/IATA
        const char *muni = ap["municipality"] | "";
        const char *iata = ap["iata_code"] | "";
        snprintf(out, n, "%s", muni[0] ? muni : iata);
        return;
    }
    snprintf(out, n, "%s", s.c_str());
}

bool route_fetch(const char *callsign, char *from, size_t fn, char *to, size_t tn,
                 RouteCoords *coords) {
    if (fn) from[0] = 0;
    if (tn) to[0] = 0;
    if (coords) *coords = RouteCoords{};
    if (!callsign || !callsign[0] || WiFi.status() != WL_CONNECTED) return false;

    // strip spaces from the callsign
    char cs[12];
    size_t j = 0;
    for (const char *p = callsign; *p && j < sizeof(cs) - 1; ++p)
        if (*p != ' ') cs[j++] = *p;
    cs[j] = 0;
    if (j == 0) return false;

    char url[96];
    snprintf(url, sizeof(url), "https://api.adsbdb.com/v0/callsign/%s", cs);

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setReuse(false);
    http.setConnectTimeout(3000);   // short: runs on the feed task, don't stall the live poll
    http.setTimeout(6000);
    if (!http.begin(client, url)) return false;
    http.addHeader("User-Agent", ADSB_USER_AGENT);

    const int code = http.GET();
    if (code != 200) { http.end(); return false; }

    JsonDocument filter;
    filter["response"]["flightroute"]["origin"]["municipality"] = true;
    filter["response"]["flightroute"]["origin"]["iata_code"] = true;
    filter["response"]["flightroute"]["origin"]["name"] = true;
    filter["response"]["flightroute"]["origin"]["latitude"] = true;
    filter["response"]["flightroute"]["origin"]["longitude"] = true;
    filter["response"]["flightroute"]["destination"]["municipality"] = true;
    filter["response"]["flightroute"]["destination"]["iata_code"] = true;
    filter["response"]["flightroute"]["destination"]["name"] = true;
    filter["response"]["flightroute"]["destination"]["latitude"] = true;
    filter["response"]["flightroute"]["destination"]["longitude"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream(),
                                               DeserializationOption::Filter(filter));
    http.end();
    if (err) return false;

    JsonObjectConst fr = doc["response"]["flightroute"].as<JsonObjectConst>();
    if (fr.isNull()) return false;   // "unknown callsign" etc.

    JsonObjectConst org = fr["origin"].as<JsonObjectConst>();
    JsonObjectConst dst = fr["destination"].as<JsonObjectConst>();
    pick_airport(org, from, fn);
    pick_airport(dst, to, tn);
    if (coords) {
        // 0/0 means "not supplied" here — no airport sits on Null Island.
        const double flat = org["latitude"] | 0.0, flon = org["longitude"] | 0.0;
        const double tlat = dst["latitude"] | 0.0, tlon = dst["longitude"] | 0.0;
        if ((flat != 0.0 || flon != 0.0) && (tlat != 0.0 || tlon != 0.0)) {
            coords->fromLat = flat; coords->fromLon = flon;
            coords->toLat = tlat;   coords->toLon = tlon;
            coords->valid = true;
        }
    }
    return (from[0] || to[0]);
}

// Registration + type from adsbdb's aircraft endpoint: GET /v0/aircraft/{hex}.
// Same host and no API key, so it costs one more short request when a contact is tapped.
bool reg_fetch(const char *hex, char *reg, size_t rn, char *type, size_t tn) {
    if (rn) reg[0] = 0;
    if (tn) type[0] = 0;
    if (!hex || !hex[0] || WiFi.status() != WL_CONNECTED) return false;
    // Tapping a contact now fires four sequential HTTPS lookups (route, photo, logo,
    // registration) and each TLS handshake wants a contiguous internal block. This is
    // the least important of the four, so it yields first when memory is tight.
    if (heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) < 28000) return false;

    char url[96];
    snprintf(url, sizeof(url), "https://api.adsbdb.com/v0/aircraft/%s", hex);

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setReuse(false);
    http.setConnectTimeout(3000);      // shares the feed task; keep it short
    http.setTimeout(6000);
    if (!http.begin(client, url)) return false;
    http.addHeader("User-Agent", ADSB_USER_AGENT);

    const int code = http.GET();
    if (code != 200) { http.end(); return false; }

    JsonDocument filter;
    filter["response"]["aircraft"]["registration"] = true;
    filter["response"]["aircraft"]["type"] = true;
    filter["response"]["aircraft"]["icao_type"] = true;

    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, http.getStream(),
                                                     DeserializationOption::Filter(filter));
    http.end();
    if (err) return false;

    JsonObjectConst ac = doc["response"]["aircraft"].as<JsonObjectConst>();
    if (ac.isNull()) return false;                       // unknown airframe
    snprintf(reg, rn, "%s", (const char *)(ac["registration"] | ""));
    // Prefer the ICAO designator ("G650") over the long marketing name ("G650 ER"):
    // it matches what the live feed puts in `t`, and the card's title row is narrow.
    const char *icaoTy = ac["icao_type"] | "";
    const char *longTy = ac["type"] | "";
    snprintf(type, tn, "%s", icaoTy[0] ? icaoTy : longTy);
    return (reg[0] || type[0]);
}
