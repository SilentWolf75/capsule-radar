#pragma once

bool wx_radar_fetch(double lat, double lon);

// Frames still to download before the loop is complete (0 = done).
int wx_radar_backlog(void);
