// MIPI-DSI panel bring-up for the Waveshare ESP32-P4-WIFI6-Touch-LCD-4C.
//
// Drives the panel through ESP-IDF's esp_lcd_mipi_dsi, which Arduino-ESP32 3.x exposes.
// (An earlier note here claimed Arduino_GFX has no DSI path. That was wrong -- the
// vendor demo ships a fork with Arduino_ESP32DSIPanel. Going straight to esp_lcd keeps
// this independent of that fork.) Structure follows the standard
// IDF sequence: power the PHY's LDO, create the DSI bus, create a DBI channel to send
// the panel's init commands, create the DPI video panel, then hand frames to LVGL.
//
// UNVERIFIED AGAINST HARDWARE (the board is not here yet), but the panel-specific data
// is no longer guesswork: the init sequence and video timings are transcribed from the
// vendor demo's own 4INCH-DSI configuration. See docs/PORT_P4.md.
#include "config.h"
#if defined(BOARD_WAVESHARE_P4_LCD_4C)

#include <Arduino.h>
#include <lvgl.h>
#include <esp_lcd_mipi_dsi.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_ldo_regulator.h>
#include <driver/ledc.h>
#include <esp_heap_caps.h>
#include "display_p4.h"

static esp_lcd_dsi_bus_handle_t  s_bus   = nullptr;
static esp_lcd_panel_io_handle_t s_io    = nullptr;
static esp_lcd_panel_handle_t    s_panel = nullptr;
static esp_ldo_channel_handle_t  s_ldo   = nullptr;

#include "boards/p4_panel_init.h"   // 197 vendor init registers, transcribed verbatim

bool display_p4_begin(void) {
    // 1) The DSI PHY is fed by an internal LDO. Without this the whole chain reports
    //    success and the screen stays black -- the exact symptom reported by someone
    //    running this board under ESPHome.
    esp_ldo_channel_config_t ldo_cfg = {};
    ldo_cfg.chan_id    = DSI_LDO_CHANNEL;
    ldo_cfg.voltage_mv = DSI_LDO_MV;
    if (esp_ldo_acquire_channel(&ldo_cfg, &s_ldo) != ESP_OK) {
        Serial.println("[p4lcd] DSI PHY LDO acquire failed");
        return false;
    }

    // 2) DSI bus
    esp_lcd_dsi_bus_config_t bus_cfg = {};
    bus_cfg.bus_id             = 0;
    bus_cfg.num_data_lanes     = DSI_LANES;
    bus_cfg.phy_clk_src        = MIPI_DSI_PHY_CLK_SRC_DEFAULT;
    bus_cfg.lane_bit_rate_mbps = (uint32_t)(DSI_LANE_BITRATE_HZ / 1000000UL);
    if (esp_lcd_new_dsi_bus(&bus_cfg, &s_bus) != ESP_OK) {
        Serial.println("[p4lcd] dsi bus create failed");
        return false;
    }

    // 3) DBI channel: the low-speed path used only to push the init registers.
    esp_lcd_dbi_io_config_t dbi_cfg = {};
    dbi_cfg.virtual_channel = 0;
    dbi_cfg.lcd_cmd_bits    = 8;
    dbi_cfg.lcd_param_bits  = 8;
    if (esp_lcd_new_panel_io_dbi(s_bus, &dbi_cfg, &s_io) != ESP_OK) {
        Serial.println("[p4lcd] dbi io create failed");
        return false;
    }

    for (size_t i = 0; i < sizeof(kPanelInit) / sizeof(kPanelInit[0]); ++i) {
        const uint8_t v = kPanelInit[i].val;
        esp_lcd_panel_io_tx_param(s_io, kPanelInit[i].reg, &v, 1);
        if (kPanelInit[i].delay_ms) delay(kPanelInit[i].delay_ms);
    }

    // 4) DPI video panel, timings from the board header (the vendor's own values).
    esp_lcd_dpi_panel_config_t dpi_cfg = {};
    dpi_cfg.virtual_channel     = 0;
    dpi_cfg.dpi_clk_src         = MIPI_DSI_DPI_CLK_SRC_DEFAULT;
    dpi_cfg.dpi_clock_freq_mhz  = (uint32_t)(DSI_PCLK_HZ / 1000000UL);
    dpi_cfg.pixel_format        = LCD_COLOR_PIXEL_FORMAT_RGB565;
    dpi_cfg.num_fbs             = 1;
    dpi_cfg.video_timing.h_size = SCREEN_W;
    dpi_cfg.video_timing.v_size = SCREEN_H;
    dpi_cfg.video_timing.hsync_pulse_width = DSI_HSYNC_PULSE;
    dpi_cfg.video_timing.hsync_front_porch = DSI_HSYNC_FRONT;
    dpi_cfg.video_timing.hsync_back_porch  = DSI_HSYNC_BACK;
    dpi_cfg.video_timing.vsync_pulse_width = DSI_VSYNC_PULSE;
    dpi_cfg.video_timing.vsync_front_porch = DSI_VSYNC_FRONT;
    dpi_cfg.video_timing.vsync_back_porch  = DSI_VSYNC_BACK;
    dpi_cfg.flags.use_dma2d = true;    // the P4's 2D accelerator does the blit
    if (esp_lcd_new_panel_dpi(s_bus, &dpi_cfg, &s_panel) != ESP_OK) {
        Serial.println("[p4lcd] dpi panel create failed");
        return false;
    }
    if (esp_lcd_panel_init(s_panel) != ESP_OK) {
        Serial.println("[p4lcd] panel init failed");
        return false;
    }

    display_p4_backlight(true);
    Serial.printf("[p4lcd] DSI up: %dx%d, %u lanes, %u Mbps\n",
                  SCREEN_W, SCREEN_H, (unsigned)DSI_LANES, (unsigned)bus_cfg.lane_bit_rate_mbps);
    return true;
}

void display_p4_backlight(bool on) {
    if (PIN_LCD_BL < 0) return;
    static bool inited = false;
    if (!inited) {
        ledc_timer_config_t t = {};
        t.speed_mode      = LEDC_LOW_SPEED_MODE;
        t.duty_resolution = LEDC_TIMER_8_BIT;
        t.timer_num       = LEDC_TIMER_0;
        t.freq_hz         = 5000;
        t.clk_cfg         = LEDC_AUTO_CLK;
        ledc_timer_config(&t);
        ledc_channel_config_t c = {};
        c.gpio_num   = PIN_LCD_BL;
        c.speed_mode = LEDC_LOW_SPEED_MODE;
        c.channel    = LEDC_CHANNEL_0;
        c.timer_sel  = LEDC_TIMER_0;
        c.duty       = 0;
        ledc_channel_config(&c);
        inited = true;
    }
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, on ? 255 : 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

// LVGL flush. The DPI panel owns its framebuffer, so this is a straight blit; the
// area is inclusive on both ends in LVGL and exclusive at the end in esp_lcd.
void display_p4_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *px) {
    if (s_panel) {
        esp_lcd_panel_draw_bitmap(s_panel, area->x1, area->y1,
                                  area->x2 + 1, area->y2 + 1, px);
    }
    lv_disp_flush_ready(drv);
}

#endif // BOARD_WAVESHARE_P4_LCD_4C
