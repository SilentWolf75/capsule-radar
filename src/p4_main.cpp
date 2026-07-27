// SkyGlass on the Waveshare ESP32-P4-WIFI6-Touch-LCD-4C — entry point.
//
// PORT IN PROGRESS. See docs/PORT_P4.md.
//
// This currently registers LVGL against a NULL flush: nothing reaches the panel, because
// the 4C is MIPI-DSI and Arduino_GFX has no DSI path. That is deliberate rather than
// idle -- it makes the whole portable UI compile and link for RISC-V at 720x720, so
// portability regressions surface here instead of on the bench. Replacing flush_cb with
// a real esp_lcd_mipi_dsi driver is the next step, and needs the panel's controller and
// reset/backlight pins read off the vendor demo.
#include "config.h"
#if defined(BOARD_WAVESHARE_P4_LCD_4C)

#include <Arduino.h>
#include <lvgl.h>
#include <esp_heap_caps.h>
#include "ui.h"
#include "radar_view.h"

static lv_disp_draw_buf_t s_dbuf;
static lv_disp_drv_t      s_drv;
static lv_color_t        *s_buf = nullptr;

// Discards the frame. The panel is not driven yet; see the file header.
static void flush_null(lv_disp_drv_t *drv, const lv_area_t *, lv_color_t *) {
    lv_disp_flush_ready(drv);
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.printf("[p4] SkyGlass %s on %s\n", FW_VERSION, BOARD_NAME);
    Serial.printf("[p4] psram %u free\n", (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    lv_init();
    const size_t px = (size_t)SCREEN_W * 40;
    s_buf = (lv_color_t *)heap_caps_malloc(px * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    if (!s_buf) { Serial.println("[p4] draw buffer alloc failed"); return; }
    lv_disp_draw_buf_init(&s_dbuf, s_buf, nullptr, px);

    lv_disp_drv_init(&s_drv);
    s_drv.hor_res  = SCREEN_W;
    s_drv.ver_res  = SCREEN_H;
    s_drv.flush_cb = flush_null;
    s_drv.draw_buf = &s_dbuf;
    lv_disp_drv_register(&s_drv);

    // No tick timer needed: lv_conf.h sets LV_TICK_CUSTOM to millis(), same as the S3 build.
    ui_create();
    Serial.printf("[p4] UI built at %dx%d\n", SCREEN_W, SCREEN_H);
}

void loop() {
    lv_timer_handler();
    delay(5);
}

#endif // BOARD_WAVESHARE_P4_LCD_4C
