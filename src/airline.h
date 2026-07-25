#pragma once
// Airline identity for the detail card: an offline callsign -> operator lookup, plus
// shared state for the downloaded logo image (same request/commit handshake as photo.*).
// Portable: the UI thread requests, the network task fulfils, the UI reads.
#include <lvgl.h>
#include <stddef.h>

// Offline: map an ADS-B callsign ("RYR4TG") to its operator via the 3-letter ICAO
// prefix. Any of the out params may be null. Returns false for an unknown prefix.
bool airline_lookup(const char *callsign,
                    char *iata, size_t in,
                    char *name, size_t nn);

void airline_logo_request(const char *iata);            // UI: I want this airline's logo
bool airline_logo_pending(char *iataOut, size_t n);     // network task: code to fetch (deduped)
lv_color_t *airline_logo_buffer(int *maxW, int *maxH);  // PSRAM RGB565 buffer to decode into
void airline_logo_commit(int w, int h, const char *iata);  // ready (w=0 => no logo)
bool airline_logo_get(const char *iata, int *w, int *h);   // UI: ready & matches this code?
