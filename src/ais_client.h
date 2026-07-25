#pragma once
// aisstream.io WebSocket client (device-only). Opens one persistent WSS connection,
// subscribes to a bounding box around home, and feeds position reports into vessel.*.
// Needs a free API key from https://aisstream.io — the feature stays off without one.
#include <stddef.h>

void ais_set_key(const char *apiKey);      // "" disables and disconnects
bool ais_has_key(void);
void ais_configure(double lat, double lon, float rangeKm);  // (re)subscribe on change
void ais_loop(void);                        // pump the socket; call from the network task
bool ais_connected(void);
void ais_stop(void);
