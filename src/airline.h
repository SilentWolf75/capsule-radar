#pragma once
// Airline identity for the detail card: an offline callsign -> operator lookup.
//
// This deliberately does not fetch logos. The only free, keyless source (kiwi.com) has
// partial coverage and silently substitutes its own brand mark for airlines it lacks,
// so a Southwest flight rendered a Kiwi advert; airhex wants a paid key. A wrong logo
// is worse than none, and the embedded table below is instant and works offline.
#include <stddef.h>

// Offline: map an ADS-B callsign ("RYR4TG") to its operator via the 3-letter ICAO
// prefix. Any of the out params may be null. Returns false for an unknown prefix.
bool airline_lookup(const char *callsign,
                    char *iata, size_t in,
                    char *name, size_t nn);
