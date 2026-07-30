#pragma once

#include <stdint.h>

#include "config.h"

// Size of the displayed radar image. It is a centre crop of the WX_RADAR_SOURCE_SIZE
// tile we fetch, so a larger value shows more of the same imagery at the same scale --
// not a zoom. Boards with a big panel override it (see the board headers); anything
// above WX_RADAR_SOURCE_SIZE would need a larger tile.
#ifndef WX_RADAR_SIZE
#  define WX_RADAR_SIZE 360
#endif
#define WX_RADAR_SOURCE_SIZE 512
static_assert(WX_RADAR_SIZE <= WX_RADAR_SOURCE_SIZE, "radar crop exceeds the fetched tile");

void wx_radar_begin(void);
uint16_t *wx_radar_back_buffer(void);                    // network/sim: decode here
void wx_radar_commit(uint32_t frameTime, double lat, double lon);
bool wx_radar_front(const uint16_t **pixels, uint32_t *frameTime,
                    double *lat, double *lon, uint32_t *version);
