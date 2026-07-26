#pragma once
// Continental fire fetch for the FIRES screen (device-only).
//
// Kept separate from wildfire_client because the two have opposite shapes: that one
// pulls a handful of detections around the scope, this one pulls a continent and bins
// thousands. Returns true while work is in flight; call once per network-task pass.
bool firemap_client_step(void);
