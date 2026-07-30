#pragma once
#include <lvgl.h>
#include "config.h"

// P4 Cyber-Neon Theme System for Waveshare ESP32-P4-WIFI6-Touch-LCD-4C (720x720)

enum P4ScreenType {
    P4_SCREEN_RADAR = 0,
    P4_SCREEN_CLOCK,
    P4_SCREEN_WEATHER,
    P4_SCREEN_WX_RADAR,
    P4_SCREEN_WILDFIRES,
    P4_SCREEN_LIST,
    P4_SCREEN_STATS,
    P4_SCREEN_SETTINGS
};

#if defined(BOARD_WAVESHARE_P4_LCD_4C)

// Initialize P4 Theme resources
void p4_theme_init(void);

// Draw glowing neon bezel ring around 720x720 circle viewport
void p4_draw_neon_bezel(lv_draw_ctx_t *d, P4ScreenType screen);

// Draw P4 Radar Scope elements: top header arc, range mile markers, bottom legend bar & status arc
void p4_draw_radar_scope_chrome(lv_draw_ctx_t *d, float rangeKm);

// P4 Altitude palette overrides
lv_color_t p4_alt_color(float altFt, bool onGround);

// Draw P4 Clock / Home screen neon backdrop, top date arc & WX RADAR quick-switch pill
void p4_draw_clock_backdrop(lv_draw_ctx_t *d);

// Draw P4 Active Wildfires top header arc, side info cards & bottom heat meter
void p4_draw_wildfires_chrome(lv_draw_ctx_t *d, int activeFires, int activeAreas);

// Draw P4 WX Radar (RainViewer) header arc, distance rings & left side mode pills
void p4_draw_wx_radar_chrome(lv_draw_ctx_t *d, const char *stationName, float rangeMi);

#else

// Stubs for non-P4 builds (S3, Desktop Sim) so S3 build remains 100% unaffected and untouched!
inline void p4_theme_init(void) {}
inline void p4_draw_neon_bezel(lv_draw_ctx_t *, P4ScreenType) {}
inline void p4_draw_radar_scope_chrome(lv_draw_ctx_t *, float) {}
inline lv_color_t p4_alt_color(float altFt, bool onGround) { return lv_color_hex(0x888888); }
inline void p4_draw_clock_backdrop(lv_draw_ctx_t *) {}
inline void p4_draw_wildfires_chrome(lv_draw_ctx_t *, int, int) {}
inline void p4_draw_wx_radar_chrome(lv_draw_ctx_t *, const char *, float) {}

#endif
