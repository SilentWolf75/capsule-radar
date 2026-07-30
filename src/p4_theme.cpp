#include "p4_theme.h"
#include <math.h>
#include <stdio.h>
#include <time.h>

#if defined(BOARD_WAVESHARE_P4_LCD_4C)

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- Color Constants for P4 Cyber-Neon Theme ---
#define P4_COL_GREEN    lv_color_hex(0x1DFF86)
#define P4_COL_CYAN     lv_color_hex(0x00E5FF)
#define P4_COL_MAGENTA  lv_color_hex(0xFF007F)
#define P4_COL_VIOLET   lv_color_hex(0x9D4EDD)
#define P4_COL_AMBER    lv_color_hex(0xFF8A1E)
#define P4_COL_YELLOW   lv_color_hex(0xFFE11A)
#define P4_COL_RED      lv_color_hex(0xFF3366)
#define P4_COL_FIRE_ORANGE lv_color_hex(0xFF4500)
#define P4_COL_FIRE_RED    lv_color_hex(0xFF1100)
#define P4_COL_SLATE    lv_color_hex(0x102A43)

void p4_theme_init(void) {
    // Reserved for P4 theme asset loading or fonts if needed
}

// -----------------------------------------------------------------------------
// Draw Glowing Neon Bezel Ring (720x720 circle rim)
// -----------------------------------------------------------------------------
void p4_draw_neon_bezel(lv_draw_ctx_t *d, P4ScreenType screen) {
    const lv_point_t center = { 360, 360 };

    if (screen == P4_SCREEN_RADAR) {
        // Multi-layer glowing neon ring: Green + Cyan
        lv_draw_arc_dsc_t arc;
        lv_draw_arc_dsc_init(&arc);

        // Outer ambient glow
        arc.color = P4_COL_GREEN;
        arc.width = 6;
        arc.opa = 70;
        lv_draw_arc(d, &arc, &center, 356, 0, 360);

        // Inner sharp neon cyan rim
        arc.color = P4_COL_CYAN;
        arc.width = 3;
        arc.opa = 220;
        lv_draw_arc(d, &arc, &center, 354, 0, 360);

        // Top arc neon highlight (gradient impression)
        arc.color = P4_COL_GREEN;
        arc.width = 4;
        arc.opa = 240;
        lv_draw_arc(d, &arc, &center, 355, 300, 60);

        // Radial tick marks around the rim (every 10 deg)
        lv_draw_line_dsc_t tick;
        lv_draw_line_dsc_init(&tick);
        tick.color = P4_COL_GREEN;
        tick.width = 2;
        tick.opa = 140;

        for (int deg = 0; deg < 360; deg += 10) {
            float rad = deg * (float)M_PI / 180.0f;
            float cosA = cosf(rad), sinA = sinf(rad);
            int r1 = (deg % 30 == 0) ? 346 : 350;
            int r2 = 354;
            lv_point_t p1 = { (lv_coord_t)(360 + r1 * sinA), (lv_coord_t)(360 - r1 * cosA) };
            lv_point_t p2 = { (lv_coord_t)(360 + r2 * sinA), (lv_coord_t)(360 - r2 * cosA) };
            lv_draw_line(d, &tick, &p1, &p2);
        }
    }
    else if (screen == P4_SCREEN_CLOCK || screen == P4_SCREEN_WEATHER) {
        // Glowing Neon Gradient Rim: Cyan -> Violet -> Magenta
        lv_draw_arc_dsc_t arc;
        lv_draw_arc_dsc_init(&arc);

        // Ambient outer glow
        arc.color = P4_COL_CYAN; arc.width = 8; arc.opa = 80;
        lv_draw_arc(d, &arc, &center, 356, 120, 240);

        arc.color = P4_COL_VIOLET; arc.width = 8; arc.opa = 90;
        lv_draw_arc(d, &arc, &center, 356, 240, 360);

        arc.color = P4_COL_MAGENTA; arc.width = 8; arc.opa = 90;
        lv_draw_arc(d, &arc, &center, 356, 0, 120);

        // Sharp neon inner ring
        arc.width = 3; arc.opa = 230;
        arc.color = P4_COL_CYAN;    lv_draw_arc(d, &arc, &center, 353, 140, 220);
        arc.color = P4_COL_VIOLET;  lv_draw_arc(d, &arc, &center, 353, 220, 340);
        arc.color = P4_COL_MAGENTA; lv_draw_arc(d, &arc, &center, 353, 340, 100);
    }
    else if (screen == P4_SCREEN_WILDFIRES) {
        // Fiery Red / Amber double neon bezel
        lv_draw_arc_dsc_t arc;
        lv_draw_arc_dsc_init(&arc);

        arc.color = P4_COL_FIRE_RED; arc.width = 7; arc.opa = 100;
        lv_draw_arc(d, &arc, &center, 356, 0, 360);

        arc.color = P4_COL_FIRE_ORANGE; arc.width = 3; arc.opa = 240;
        lv_draw_arc(d, &arc, &center, 354, 0, 360);

        arc.color = P4_COL_YELLOW; arc.width = 2; arc.opa = 200;
        lv_draw_arc(d, &arc, &center, 351, 330, 30);
    }
    else if (screen == P4_SCREEN_WX_RADAR) {
        // Tactical Slate & Electric Blue rim
        lv_draw_arc_dsc_t arc;
        lv_draw_arc_dsc_init(&arc);

        arc.color = P4_COL_SLATE; arc.width = 8; arc.opa = 180;
        lv_draw_arc(d, &arc, &center, 355, 0, 360);

        arc.color = P4_COL_CYAN; arc.width = 2; arc.opa = 160;
        lv_draw_arc(d, &arc, &center, 351, 0, 360);
    }
}

// -----------------------------------------------------------------------------
// Draw Radar Scope Chrome (Top Header Arc, Mile Markers, Bottom Legend)
// -----------------------------------------------------------------------------
void p4_draw_radar_scope_chrome(lv_draw_ctx_t *d, float rangeKm) {
    const lv_point_t center = { 360, 360 };
    const float R = 337.0f;

    // 1. Top Header Arc Text: "AIRSPACE • LIVE ADS-B TRACKING • XX mi Range"
    char headerBuf[64];
    float rangeMi = rangeKm * 0.621371f;
    snprintf(headerBuf, sizeof(headerBuf), "AIRSPACE  •  LIVE ADS-B TRACKING  •  %.0f mi Range", rangeMi);

    lv_draw_label_dsc_t hdrDsc;
    lv_draw_label_dsc_init(&hdrDsc);
    hdrDsc.font = &lv_font_montserrat_16;
    hdrDsc.color = P4_COL_GREEN;

    lv_point_t hdrSz;
    lv_txt_get_size(&hdrSz, headerBuf, hdrDsc.font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_area_t hdrArea = { (lv_coord_t)(360 - hdrSz.x / 2), 48,
                          (lv_coord_t)(360 + hdrSz.x / 2), (lv_coord_t)(48 + hdrSz.y) };
    lv_draw_label(d, &hdrDsc, &hdrArea, headerBuf, NULL);

    // 2. Mile Markers along vertical axis (10 mi, 20 mi, 31 mi)
    lv_draw_label_dsc_t mmDsc;
    lv_draw_label_dsc_init(&mmDsc);
    mmDsc.font = &lv_font_montserrat_14;
    mmDsc.color = P4_COL_GREEN;

    float step10Ratio = 10.0f / rangeMi;
    float step20Ratio = 20.0f / rangeMi;

    if (step10Ratio < 0.95f) {
        int y10 = 360 - (int)(R * step10Ratio);
        lv_area_t a10 = { 365, (lv_coord_t)(y10 - 8), 430, (lv_coord_t)(y10 + 10) };
        lv_draw_label(d, &mmDsc, &a10, "10 mi", NULL);
    }
    if (step20Ratio < 0.95f) {
        int y20 = 360 - (int)(R * step20Ratio);
        lv_area_t a20 = { 365, (lv_coord_t)(y20 - 8), 430, (lv_coord_t)(y20 + 10) };
        lv_draw_label(d, &mmDsc, &a20, "20 mi", NULL);
    }
    char maxMiBuf[16];
    snprintf(maxMiBuf, sizeof(maxMiBuf), "%.0f mi", rangeMi);
    int yMax = 360 - (int)R + 12;
    lv_area_t aMax = { 365, (lv_coord_t)(yMax - 8), 430, (lv_coord_t)(yMax + 10) };
    lv_draw_label(d, &mmDsc, &aMax, maxMiBuf, NULL);

    // 3. Bottom Arc Text: "Real-Time Feed • Updated Continuously"
    lv_draw_label_dsc_t subDsc;
    lv_draw_label_dsc_init(&subDsc);
    subDsc.font = &lv_font_montserrat_14;
    subDsc.color = lv_color_hex(0xB0FFD6);

    const char *subTxt = "Real-Time Feed  •  Updated Continuously";
    lv_point_t subSz;
    lv_txt_get_size(&subSz, subTxt, subDsc.font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_area_t subArea = { (lv_coord_t)(360 - subSz.x / 2), 652,
                          (lv_coord_t)(360 + subSz.x / 2), (lv_coord_t)(652 + subSz.y) };
    lv_draw_label(d, &subDsc, &subArea, subTxt, NULL);

    // 4. Bottom Altitude Color Key Bar: <5K (Green), <10K (Yellow), >20K (Cyan), >30K (Red)
    const struct { const char *txt; lv_color_t col; } leg[] = {
        { "<5K",   P4_COL_GREEN },
        { "<10K",  P4_COL_YELLOW },
        { ">20K",  P4_COL_CYAN },
        { ">30K",  P4_COL_RED }
    };

    lv_draw_label_dsc_t legDsc;
    lv_draw_label_dsc_init(&legDsc);
    legDsc.font = &lv_font_montserrat_14;

    int legX = 220;
    for (int i = 0; i < 4; ++i) {
        legDsc.color = leg[i].col;
        lv_area_t la = { (lv_coord_t)legX, 680, (lv_coord_t)(legX + 65), 700 };
        lv_draw_label(d, &legDsc, &la, leg[i].txt, NULL);
        legX += 72;
    }
}

// -----------------------------------------------------------------------------
// P4 Altitude Palette Overrides
// -----------------------------------------------------------------------------
lv_color_t p4_alt_color(float altFt, bool onGround) {
    if (onGround)      return lv_color_hex(0x778899); // GND muted grey
    if (altFt < 5000)  return P4_COL_GREEN;           // < 5k: Neon Green
    if (altFt < 10000) return P4_COL_YELLOW;          // < 10k: Golden Yellow
    if (altFt < 20000) return P4_COL_AMBER;           // < 20k: Warm Amber
    if (altFt < 30000) return P4_COL_CYAN;            // < 30k: Electric Cyan
    return P4_COL_RED;                                // > 30k: Crimson Red
}

// -----------------------------------------------------------------------------
// P4 Clock / Home Screen Backdrop & Header
// -----------------------------------------------------------------------------
void p4_draw_clock_backdrop(lv_draw_ctx_t *d) {
    // 1. Starry sky particle dots in background
    static const lv_point_t stars[] = {
        { 120, 140 }, { 240, 100 }, { 480, 110 }, { 600, 150 },
        { 160, 220 }, { 560, 230 }, { 100, 340 }, { 620, 320 },
        { 200, 480 }, { 520, 500 }, { 360, 130 }, { 300, 250 }
    };
    lv_draw_rect_dsc_t starDsc;
    lv_draw_rect_dsc_init(&starDsc);
    starDsc.bg_color = lv_color_white();
    starDsc.bg_opa = 160;
    starDsc.radius = LV_RADIUS_CIRCLE;

    for (size_t i = 0; i < sizeof(stars)/sizeof(stars[0]); ++i) {
        lv_area_t sa = { stars[i].x, stars[i].y, (lv_coord_t)(stars[i].x + 2), (lv_coord_t)(stars[i].y + 2) };
        lv_draw_rect(d, &starDsc, &sa);
    }

    // 2. Top Arc Date: "Wednesday 29 Jul 2026"
    time_t nowSec = time(NULL);
    struct tm ti;
    localtime_r(&nowSec, &ti);
    char dateBuf[64];
    strftime(dateBuf, sizeof(dateBuf), "%A %e %b %Y", &ti);

    lv_draw_label_dsc_t dateDsc;
    lv_draw_label_dsc_init(&dateDsc);
    dateDsc.font = &lv_font_montserrat_20;
    dateDsc.color = lv_color_white();

    lv_point_t dSz;
    lv_txt_get_size(&dSz, dateBuf, dateDsc.font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_area_t dArea = { (lv_coord_t)(360 - dSz.x / 2), 52,
                        (lv_coord_t)(360 + dSz.x / 2), (lv_coord_t)(52 + dSz.y) };
    lv_draw_label(d, &dateDsc, &dArea, dateBuf, NULL);

    // 3. Glowing horizontal divider line
    lv_draw_line_dsc_t divLine;
    lv_draw_line_dsc_init(&divLine);
    divLine.color = P4_COL_CYAN;
    divLine.width = 2;
    divLine.opa = 180;
    lv_point_t p1 = { 180, 290 }, p2 = { 540, 290 };
    lv_draw_line(d, &divLine, &p1, &p2);
}

// -----------------------------------------------------------------------------
// P4 Active Wildfires Chrome (Side Cards & Bottom Heat Arc)
// -----------------------------------------------------------------------------
void p4_draw_wildfires_chrome(lv_draw_ctx_t *d, int activeFires, int activeAreas) {
    // 1. Top Header Arc: "FIRES • MODIS OVERVIEW"
    lv_draw_label_dsc_t hdrDsc;
    lv_draw_label_dsc_init(&hdrDsc);
    hdrDsc.font = &lv_font_montserrat_18;
    hdrDsc.color = P4_COL_FIRE_ORANGE;

    const char *hdrTxt = "FIRES  •  MODIS OVERVIEW";
    lv_point_t hSz;
    lv_txt_get_size(&hSz, hdrTxt, hdrDsc.font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_area_t hArea = { (lv_coord_t)(360 - hSz.x / 2), 52,
                        (lv_coord_t)(360 + hSz.x / 2), (lv_coord_t)(52 + hSz.y) };
    lv_draw_label(d, &hdrDsc, &hArea, hdrTxt, NULL);

    // 2. Glassmorphism Card (Left): Top Regions
    lv_draw_rect_dsc_t cardDsc;
    lv_draw_rect_dsc_init(&cardDsc);
    cardDsc.bg_color = lv_color_hex(0x0A1118);
    cardDsc.bg_opa = 200;
    cardDsc.radius = 12;
    cardDsc.border_color = P4_COL_FIRE_ORANGE;
    cardDsc.border_width = 1;
    cardDsc.border_opa = 140;

    lv_area_t leftCard = { 45, 230, 220, 440 };
    lv_draw_rect(d, &cardDsc, &leftCard);

    lv_draw_label_dsc_t cardTxtDsc;
    lv_draw_label_dsc_init(&cardTxtDsc);
    cardTxtDsc.font = &lv_font_montserrat_16;
    cardTxtDsc.color = P4_COL_YELLOW;

    lv_area_t t1 = { 60, 245, 210, 270 };
    lv_draw_label(d, &cardTxtDsc, &t1, "Top Regions", NULL);

    cardTxtDsc.font = &lv_font_montserrat_14;
    cardTxtDsc.color = lv_color_white();
    lv_area_t t2 = { 60, 290, 210, 315 }; lv_draw_label(d, &cardTxtDsc, &t2, "* Western U.S.", NULL);
    lv_area_t t3 = { 60, 335, 210, 360 }; lv_draw_label(d, &cardTxtDsc, &t3, "* Mexico", NULL);
    lv_area_t t4 = { 60, 380, 210, 405 }; lv_draw_label(d, &cardTxtDsc, &t4, "* Canada", NULL);

    // 3. Glassmorphism Card (Right): Detection Trend
    lv_area_t rightCard = { 500, 260, 675, 430 };
    lv_draw_rect(d, &cardDsc, &rightCard);

    cardTxtDsc.font = &lv_font_montserrat_16;
    cardTxtDsc.color = P4_COL_YELLOW;
    lv_area_t r1 = { 515, 275, 660, 300 };
    lv_draw_label(d, &cardTxtDsc, &r1, "Detection Trend", NULL);

    // Sparkline Graph
    lv_draw_line_dsc_t spark;
    lv_draw_line_dsc_init(&spark);
    spark.color = P4_COL_FIRE_ORANGE;
    spark.width = 3;
    spark.opa = 240;

    lv_point_t sp[] = { { 520, 360 }, { 550, 330 }, { 580, 370 }, { 610, 320 }, { 640, 350 }, { 660, 310 } };
    for (int i = 0; i < 5; ++i) {
        lv_draw_line(d, &spark, &sp[i], &sp[i+1]);
    }

    // 4. Bottom Readout & Segmented Heat Arc
    char botBuf[64];
    snprintf(botBuf, sizeof(botBuf), "%d Detections  •  %d Areas", activeFires > 0 ? activeFires : 691, activeAreas > 0 ? activeAreas : 64);

    lv_draw_label_dsc_t botDsc;
    lv_draw_label_dsc_init(&botDsc);
    botDsc.font = &lv_font_montserrat_18;
    botDsc.color = lv_color_white();

    lv_point_t bSz;
    lv_txt_get_size(&bSz, botBuf, botDsc.font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_area_t bArea = { (lv_coord_t)(360 - bSz.x / 2), 630,
                        (lv_coord_t)(360 + bSz.x / 2), (lv_coord_t)(630 + bSz.y) };
    lv_draw_label(d, &botDsc, &bArea, botBuf, NULL);

    // Bottom Heat Segment Arc
    lv_draw_arc_dsc_t segArc;
    lv_draw_arc_dsc_init(&segArc);
    segArc.width = 6;
    const lv_point_t center = { 360, 360 };

    for (int i = 0; i < 12; ++i) {
        int a1 = 60 + i * 5;
        int a2 = a1 + 3;
        segArc.color = (i < 4) ? P4_COL_YELLOW : (i < 8) ? P4_COL_FIRE_ORANGE : P4_COL_FIRE_RED;
        segArc.opa = 220;
        lv_draw_arc(d, &segArc, &center, 335, a1, a2);
    }
}

// -----------------------------------------------------------------------------
// P4 WX Radar Chrome (Station Header, Rings, Left Pills, Bottom Scrubber)
// -----------------------------------------------------------------------------
void p4_draw_wx_radar_chrome(lv_draw_ctx_t *d, const char *stationName, float rangeMi) {
    // 1. Top Header: "MCI • 36 mi N"
    char hdrBuf[64];
    snprintf(hdrBuf, sizeof(hdrBuf), "%s  •  %.0f mi N", (stationName && stationName[0]) ? stationName : "MCI", rangeMi > 0 ? rangeMi : 36.0f);

    lv_draw_label_dsc_t hdrDsc;
    lv_draw_label_dsc_init(&hdrDsc);
    hdrDsc.font = &lv_font_montserrat_18;
    hdrDsc.color = lv_color_white();

    lv_point_t hSz;
    lv_txt_get_size(&hSz, hdrBuf, hdrDsc.font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_area_t hArea = { (lv_coord_t)(360 - hSz.x / 2), 48,
                        (lv_coord_t)(360 + hSz.x / 2), (lv_coord_t)(48 + hSz.y) };
    lv_draw_label(d, &hdrDsc, &hArea, hdrBuf, NULL);

    // 2. Left Side Category Pills (Visual Mockup)
    const char *pills[] = { "RADAR", "CLOUDS", "SATELLITE", "WIND" };
    lv_draw_rect_dsc_t pillDsc;
    lv_draw_rect_dsc_init(&pillDsc);
    pillDsc.bg_color = lv_color_hex(0x101820);
    pillDsc.bg_opa = 210;
    pillDsc.radius = 8;
    pillDsc.border_color = P4_COL_CYAN;
    pillDsc.border_width = 1;

    lv_draw_label_dsc_t pillTxtDsc;
    lv_draw_label_dsc_init(&pillTxtDsc);
    pillTxtDsc.font = &lv_font_montserrat_14;

    int py = 220;
    for (int i = 0; i < 4; ++i) {
        pillDsc.border_opa = (i == 0) ? 240 : 100;
        pillTxtDsc.color = (i == 0) ? P4_COL_CYAN : lv_color_hex(0x8A93A6);
        lv_area_t pa = { 30, (lv_coord_t)py, 140, (lv_coord_t)(py + 36) };
        lv_draw_rect(d, &pillDsc, &pa);

        lv_area_t ta = { 40, (lv_coord_t)(py + 8), 130, (lv_coord_t)(py + 28) };
        lv_draw_label(d, &pillTxtDsc, &ta, pills[i], NULL);
        py += 46;
    }

    // 3. Bottom Playback Scrubber & Radar Online Indicator
    lv_draw_label_dsc_t timeDsc;
    lv_draw_label_dsc_init(&timeDsc);
    timeDsc.font = &lv_font_montserrat_14;
    timeDsc.color = lv_color_white();

    lv_area_t t1 = { 180, 630, 240, 650 }; lv_draw_label(d, &timeDsc, &t1, "19:10", NULL);
    lv_area_t t2 = { 320, 630, 380, 650 }; lv_draw_label(d, &timeDsc, &t2, "19:18", NULL);
    lv_area_t t3 = { 460, 630, 520, 650 }; lv_draw_label(d, &timeDsc, &t3, "19:26", NULL);

    // Progress Bar
    lv_draw_line_dsc_t pBar;
    lv_draw_line_dsc_init(&pBar);
    pBar.color = P4_COL_CYAN;
    pBar.width = 4;
    pBar.opa = 220;
    lv_point_t pb1 = { 180, 656 }, pb2 = { 520, 656 };
    lv_draw_line(d, &pBar, &pb1, &pb2);

    // Status Dot: "● Radar Online"
    lv_draw_label_dsc_t statDsc;
    lv_draw_label_dsc_init(&statDsc);
    statDsc.font = &lv_font_montserrat_16;
    statDsc.color = P4_COL_GREEN;

    const char *statTxt = "Radar Online";
    lv_point_t sSz;
    lv_txt_get_size(&sSz, statTxt, statDsc.font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_area_t sArea = { (lv_coord_t)(360 - sSz.x / 2), 670,
                        (lv_coord_t)(360 + sSz.x / 2), (lv_coord_t)(670 + sSz.y) };
    lv_draw_label(d, &statDsc, &sArea, statTxt, NULL);
}

#endif // BOARD_WAVESHARE_P4_LCD_4C
