#pragma once
// Shared route state (origin -> destination by callsign). Portable: the UI thread
// requests a lookup, a network task fulfils it, the UI reads the result.
#include <stddef.h>

// Endpoint coordinates that come with a route. Tracked mode uses them for the
// progress bar and ETA; they are optional, since not every lookup resolves an airport.
struct RouteCoords {
    double fromLat = 0, fromLon = 0;
    double toLat = 0, toLon = 0;
    bool   valid = false;   // true only when both endpoints resolved
};

void route_request(const char *callsign);                     // UI: want a route for this callsign
bool route_pending(char *callOut, size_t n);                  // task: is a lookup needed? returns callsign
void route_store(const char *callsign, const char *from, const char *to);  // task/sim: store result
void route_store_full(const char *callsign, const char *from, const char *to,
                      const RouteCoords &coords);             // ...with endpoint coordinates
bool route_get(const char *callsign, char *from, size_t fn, char *to, size_t tn); // UI: read result
bool route_get_coords(const char *callsign, RouteCoords &out);                    // UI: endpoint coords
