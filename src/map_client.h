#pragma once
// Map-tile background builder (device-only; runs on the core-0 network task).
//
// Building a background means downloading up to ~16 tiles, which would stall the live
// ADS-B poll if done in one go. Instead the work is a state machine: map_client_step()
// does one tile (or the final resample) per call, so the feed keeps polling in between.
#include <stdint.h>

void map_client_enable(bool on);
bool map_client_enabled(void);
void map_client_set_style(int style);      // 0 = dark, 1 = light
int  map_client_style(void);
void map_client_set_opacity(int percent);  // 0..100 (%)
int  map_client_opacity(void);

// Ask for a background for this scope. A no-op if one is already built or in flight
// for the same geometry; any change restarts the build.
void map_client_request(double lat, double lon, float rangeKm);

// Perform one unit of work. Returns true while a build is still in progress.
bool map_client_step(void);
