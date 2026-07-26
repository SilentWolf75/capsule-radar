// Fetch nearby aircraft from airplanes.live (fallback adsb.lol) and parse the
// readsb JSON into a vector<Aircraft>.
//
// Memory safety (important on the ESP32): we parse straight from the HTTP stream
// (no full-body String), use an ArduinoJson field filter so only the ~12 fields we
// need are kept, and hard-cap the number of aircraft (ADSB_MAX_AIRCRAFT). The radar
// then keeps only the nearest ~20 for display.
#include "adsb_client.h"
#include "config.h"
#include "geo.h"           // haversineKm — keep the nearest N aircraft
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>   // v7
#include <esp_heap_caps.h>

// Parse the JSON in PSRAM, not internal RAM. Otherwise the per-poll JSON alloc/free
// churn fragments the internal heap and, after a while, mbedTLS can't find a large
// enough contiguous block for the TLS handshake (-32512), freezing the feed.
struct PsramJsonAllocator : ArduinoJson::Allocator {
    void* allocate(size_t n) override { return heap_caps_malloc(n, MALLOC_CAP_SPIRAM); }
    void  deallocate(void* p) override { heap_caps_free(p); }
    void* reallocate(void* p, size_t n) override { return heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM); }
};
static PsramJsonAllocator s_jsonPsram;

// NetworkClient::readBytes() treats a transient negative TLS read as end-of-input,
// which makes ArduinoJson intermittently report IncompleteInput. Deliberately wrap
// the client without overriding readBytes(): Stream's timed byte reader retries
// temporary no-data reads until the configured timeout.
class ReliableJsonStream : public Stream {
public:
    explicit ReliableJsonStream(Stream& source) : _source(source) {}
    int available() override { return _source.available(); }
    int read() override {
        const int value = _source.read();
        if (value >= 0) ++_bytesRead;
        return value;
    }
    int peek() override { return _source.peek(); }
    void flush() override { _source.flush(); }
    size_t write(uint8_t) override { return 0; }
    size_t bytesRead() const { return _bytesRead; }

private:
    Stream& _source;
    size_t _bytesRead = 0;
};

void AdsbClient::begin(double homeLat, double homeLon, float rangeKm) {
    _lat = homeLat; _lon = homeLon; _rangeKm = rangeKm;
}

bool AdsbClient::poll(std::vector<Aircraft>& out) {
    if (WiFi.status() != WL_CONNECTED) return false;
    std::vector<Aircraft> primaryList;
    bool pOk = fetchFrom(ADSB_PRIMARY_HOST, primaryList);
    
    // If primary returned aircraft, use them. If primary failed or returned 0, query fallback feed.
    if (pOk && !primaryList.empty()) {
        out.swap(primaryList);
        return true;
    }
    
    std::vector<Aircraft> fallbackList;
    bool fOk = fetchFrom(ADSB_FALLBACK_HOST, fallbackList);
    if (fOk && !fallbackList.empty()) {
        out.swap(fallbackList);
        return true;
    }
    
    if (pOk) { out.swap(primaryList); return true; }
    if (fOk) { out.swap(fallbackList); return true; }
    return false;
}

bool AdsbClient::fetchFrom(const char* host, std::vector<Aircraft>& out) {
    const double nm = _rangeKm * 0.539957;            // km -> nautical miles (API radius unit)
    char url[160];
    snprintf(url, sizeof(url), "https://%s/v2/point/%.4f/%.4f/%.0f", host, _lat, _lon, nm);

    bool hostChanged = (_lastHost == nullptr || strcmp(_lastHost, host) != 0);
    if (hostChanged) {
        _http.end();
        _client.setInsecure();
        _http.setReuse(true);
        _http.setConnectTimeout(6000);
        _http.setTimeout(8000);
        _lastHost = host;
    }

    if (!_http.begin(_client, url)) { Serial.printf("[adsb] begin failed (%s)\n", host); return false; }
    _http.addHeader("User-Agent", ADSB_USER_AGENT);
    _http.addHeader("Accept", "application/json");

    const int code = _http.GET();
    if (code != 200) {
        char tls[128] = "";
        const int tlsCode = _client.lastError(tls, sizeof(tls));
        Serial.printf("[adsb] HTTP %d (%s) tls=%d '%s' heap=%u largest=%u psram=%u\n",
                      code, host, tlsCode, tls,
                      (unsigned)ESP.getFreeHeap(),
                      (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
                      (unsigned)ESP.getFreePsram());
        _http.end();
        _client.stop();
        return false;
    }

    // Only keep the fields we use -> much smaller parsed document.
    JsonDocument filter(&s_jsonPsram);
    const char* keys[] = { "ac", "aircraft" };
    const char* flds[] = { "hex", "flight", "t", "lat", "lon", "alt_baro", "alt_geom",
                           "track", "true_heading", "gs", "baro_rate",
                           "squawk", "seen_pos", "dbFlags" };
    for (const char* k : keys)
        for (const char* f : flds)
            filter[k][0][f] = true;

    JsonDocument doc(&s_jsonPsram);
    const int expectedBytes = _http.getSize();
    NetworkClient& responseStream = _http.getStream();
    ReliableJsonStream jsonStream(responseStream);
    DeserializationError err = deserializeJson(doc, jsonStream,
                                               DeserializationOption::Filter(filter));
    if (err) {
        Serial.printf("[adsb] JSON parse failed (%s): %s; expected=%d read=%u available=%d connected=%d\n",
                      host, err.c_str(), expectedBytes, (unsigned)jsonStream.bytesRead(),
                      responseStream.available(), responseStream.connected());
        _http.end();
        _client.stop();
        return false;
    }
    _http.end();
    _client.stop();

    JsonArrayConst arr = doc["ac"].as<JsonArrayConst>();
    if (arr.isNull()) arr = doc["aircraft"].as<JsonArrayConst>();
    if (arr.isNull()) return false;

    // Keep the ADSB_MAX_AIRCRAFT *nearest* aircraft
    std::vector<Aircraft> tmp;
    std::vector<float>     dist;             // parallel array: km from home for each kept aircraft
    tmp.reserve(ADSB_MAX_AIRCRAFT);
    dist.reserve(ADSB_MAX_AIRCRAFT);
    const uint32_t now = millis();
    for (JsonObjectConst a : arr) {
        if (a["lat"].isNull() || a["lon"].isNull()) continue;   // need a position
        const double lat = a["lat"].as<double>();
        const double lon = a["lon"].as<double>();

        // Robust altitude parsing: preference alt_baro, fallback to alt_geom
        bool onGround = false;
        float altFt = 0.0f;
        if (a["alt_baro"].is<const char*>()) {
            const char* s = a["alt_baro"].as<const char*>();
            if (s && (strcmp(s, "ground") == 0 || strcmp(s, "GROUND") == 0)) onGround = true;
        } else if (a["alt_baro"].is<float>()) {
            altFt = a["alt_baro"].as<float>();
        } else if (a["alt_baro"].is<int>()) {
            altFt = (float)a["alt_baro"].as<int>();
        } else if (a["alt_geom"].is<float>()) {
            altFt = a["alt_geom"].as<float>();
        } else if (a["alt_geom"].is<int>()) {
            altFt = (float)a["alt_geom"].as<int>();
        }

        if (_hideGround && onGround) continue;
        // optional filters (applied before the cap, so slots only go to matching aircraft)
        if (_minAltFt > 0.0f && (onGround || altFt < _minAltFt)) continue;
        if (_milOnly && (((a["dbFlags"] | 0u) & 0x1) == 0)) continue;

        const float d = (float)geo::haversineKm(_lat, _lon, lat, lon);

        // nearest-N gate: if the buffer is full and this one isn't closer than the farthest kept,
        // drop it now — before any string allocation.
        int farIdx = -1;
        if ((int)tmp.size() >= ADSB_MAX_AIRCRAFT) {
            farIdx = 0;
            for (int i = 1; i < (int)dist.size(); ++i) if (dist[i] > dist[farIdx]) farIdx = i;
            if (d >= dist[farIdx]) continue;
        }

        Aircraft ac;
        ac.hex = (const char*)(a["hex"] | "");
        if (ac.hex.length() == 0) continue;
        ac.flight = String((const char*)(a["flight"] | "")); ac.flight.trim();
        ac.type   = (const char*)(a["t"] | "");
        ac.lat = lat; ac.lon = lon;
        ac.onGround = onGround;
        ac.altBaro  = altFt;
        ac.track    = a["track"].is<float>() ? a["track"].as<float>() : (a["true_heading"] | NAN);
        ac.gs       = a["gs"] | NAN;
        ac.baroRate = a["baro_rate"] | NAN;
        ac.squawk   = a["squawk"].is<const char*>() ? atoi(a["squawk"]) : (a["squawk"] | -1);
        ac.seenPos  = a["seen_pos"] | 0;
        ac.military = ((a["dbFlags"] | 0u) & 0x1) != 0;
        ac.lastUpdateMs = now;

        if (farIdx >= 0) { tmp[farIdx] = std::move(ac); dist[farIdx] = d; }   // replace the farthest kept
        else             { tmp.push_back(std::move(ac)); dist.push_back(d); }
    }

    out.swap(tmp);
    _lastOkMs = now;
    return true;
}
