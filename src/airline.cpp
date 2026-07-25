#include "airline.h"
#include "airlines_data.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>


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

