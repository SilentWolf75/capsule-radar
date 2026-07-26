#pragma once
#include <stdint.h>

// Vector polylines for North American US state boundaries and country borders.
// Stored as int16 lat/lon pairs scaled by BORDER_SCALE = 100.
#define BORDER_SCALE 100

static const int16_t BORDER_PTS[] = {
    // US-Canada Border (West to East: 49th parallel to Great Lakes / Maine)
    4900, -12300,  4900, -11700,  4900, -11000,  4900, -10400,  4900, -9600,  4900, -8900,
    4800, -8900,   4650, -8450,   4500, -8250,   4300, -8250,   4250, -7900,  4500, -7480,
    4500, -7150,   4700, -6900,   4500, -6700,

    // US-Mexico Border (West to East: San Diego to Gulf of Mexico / Rio Grande)
    3250, -11710,  3250, -11470,  3130, -11100,  3130, -10820,  3180, -10650,  2900, -10350,
    2600, -9900,   2600, -9720,

    // Pacific States (WA / OR / CA / NV / AZ / ID)
    // WA-OR Border (Columbia River / 46th parallel)
    4600, -12400,  4600, -11900,  4600, -11700,
    // OR-CA Border (42nd parallel)
    4200, -12420,  4200, -12000,
    // CA-NV Border (Diagonal from Lake Tahoe to Colorado River)
    4200, -12000,  3900, -12000,  3500, -11460,
    // NV-UT Border (114th meridian)
    4200, -11400,  3700, -11400,
    // AZ-NM Border (109th meridian)
    3700, -10900,  3130, -10900,
    // UT-AZ Border (37th parallel)
    3700, -11400,  3700, -10900,
    // ID-MT Border (Bitterroot Ridge)
    4900, -11600,  4700, -11500,  4450, -11300,  4450, -11100,

    // Mountain / Central Plains States (MT / WY / CO / NM / ND / SD / NE / KS / OK / TX)
    // MT-WY Border (45th parallel)
    4500, -11100,  4500, -10400,
    // WY-CO Border (41st parallel)
    4100, -11100,  4100, -10400,
    // CO-NM Border (37th parallel)
    3700, -10900,  3700, -10300,
    // CO-KS Border (102nd meridian)
    4100, -10200,  3700, -10200,
    // WY-NE Border (104th meridian)
    4300, -10400,  4100, -10400,
    // ND-SD Border (45.9th parallel)
    4590, -10400,  4590, -9650,
    // SD-NE Border (43rd parallel)
    4300, -10400,  4300, -9800,
    // NE-KS Border (40th parallel)
    4000, -10200,  4000, -9530,
    // KS-OK Border (37th parallel)
    3700, -10200,  3700, -9460,
    // OK Panhandle & TX Border (36.5th / 100th / Red River)
    3650, -10300,  3650, -10000,  3450, -10000,  3400, -9450,  3200, -9400,
    // TX-NM Border (32nd parallel & 103rd meridian)
    3650, -10300,  3200, -10300,  3200, -10650,

    // Midwest & Great Lakes (MN / WI / IL / IA / MO / IN / MI / OH)
    // MN-WI Border (Mississippi / St Croix)
    4700, -9200,   4500, -9280,   4350, -9100,
    // IA-MO Border (40.6th parallel)
    4060, -9570,   4060, -9170,
    // MO-AR Border (36.5th parallel)
    3650, -9460,   3650, -8960,
    // IL-IN Border (87.5th meridian)
    4170, -8750,   3800, -8750,
    // IN-OH Border (84.8th meridian)
    4170, -8480,   3900, -8480,
    // MI Upper Peninsula Border
    4600, -9000,   4550, -8750,

    // South & East Coast (AR / LA / MS / AL / GA / FL / NC / VA / PA / NY)
    // LA-MS Border (31st parallel / Mississippi)
    3100, -9150,   3100, -8970,   3020, -8970,
    // MS-AL Border (88.4th meridian)
    3500, -8840,   3030, -8840,
    // AL-GA Border (85th meridian)
    3500, -8560,   3220, -8500,   3070, -8500,
    // GA-FL Border (30.4th parallel)
    3040, -8500,   3040, -8200,
    // TN-NC/AL/GA Borders (35th parallel)
    3500, -9000,   3500, -8400,   3500, -8170,
    // NC-VA Border (36.5th parallel)
    3650, -8170,   3650, -7580,
    // PA-NY Border (42nd parallel)
    4200, -7970,   4200, -7470
};

// Polyline lengths (number of points per segment)
static const uint8_t BORDER_POLY_LEN[] = {
    13, // US-Canada
    8,  // US-Mexico
    3,  // WA-OR
    2,  // OR-CA
    3,  // CA-NV
    2,  // NV-UT
    2,  // AZ-NM
    2,  // UT-AZ
    4,  // ID-MT
    2,  // MT-WY
    2,  // WY-CO
    2,  // CO-NM
    2,  // CO-KS
    2,  // WY-NE
    2,  // ND-SD
    2,  // SD-NE
    2,  // NE-KS
    2,  // KS-OK
    5,  // OK-TX
    3,  // TX-NM
    3,  // MN-WI
    2,  // IA-MO
    2,  // MO-AR
    2,  // IL-IN
    2,  // IN-OH
    2,  // MI UP
    3,  // LA-MS
    2,  // MS-AL
    3,  // AL-GA
    2,  // GA-FL
    3,  // TN
    2,  // NC-VA
    2   // PA-NY
};

static const int BORDER_NUM_POLYS = sizeof(BORDER_POLY_LEN) / sizeof(BORDER_POLY_LEN[0]);
