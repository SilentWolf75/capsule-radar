#pragma once

#include <stdint.h>

#include "config.h"

// Displayed size of the radar image: a centre crop of the WX_RADAR_SOURCE_SIZE tile we
// fetch. Larger shows more of the same imagery at the same scale -- it is not a zoom.
// Boards with a big panel override it in their board header.
#ifndef WX_RADAR_SIZE
#  define WX_RADAR_SIZE 360
#endif
#define WX_RADAR_SOURCE_SIZE 512

// How many past frames to keep for the loop. RainViewer publishes 13 at ten-minute steps
// (two hours); a board keeps as many as its PSRAM can spare. Each frame costs
// WX_RADAR_SIZE^2 * 2 bytes -- 450 KB on a 480 px panel, 200 KB on a 320 px one.
// Boards override this; the default suits a small panel.
#ifndef WX_RADAR_FRAMES
#  define WX_RADAR_FRAMES 6
#endif

void wx_radar_begin(void);
uint16_t *wx_radar_back_buffer(void);                    // network/sim: decode here

// Publish the back buffer as the frame for `frameTime`. Frames are kept in time order and
// the oldest is evicted once the ring is full, so the loop always spans the most recent
// window regardless of the order they arrive in.
void wx_radar_commit(uint32_t frameTime, double lat, double lon);

// True if this frame time is already held -- the fetcher uses it to work out which of
// RainViewer's frames it still needs, so it never downloads the same one twice.
bool wx_radar_has_frame(uint32_t frameTime);
int  wx_radar_frame_count(void);
int  wx_radar_capacity(void);                            // frames actually allocated

// Frame `idx` in time order, 0 = oldest. Playback walks this.
bool wx_radar_frame(int idx, const uint16_t **pixels, uint32_t *frameTime);

// The newest frame, for callers that just want "now".
bool wx_radar_front(const uint16_t **pixels, uint32_t *frameTime,
                    double *lat, double *lon, uint32_t *version);
