#pragma once
// Waveshare ESP32-P4-WIFI6-Touch-LCD-4C — 4" round IPS, 720x720, MIPI-DSI.
//
// STATUS: PORT IN PROGRESS. This board is not yet supported end to end; see
// docs/PORT_P4.md for what is done, what is blocked, and why.
//
// Hardware, from the vendor wiki (waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-4C):
//   ESP32-P4NRW32   dual-core RISC-V (HP) + 40 MHz RISC-V (LP)
//   32 MB PSRAM in package, 32 MB NOR flash over QSPI
//   4" round IPS 720x720, MIPI-DSI 2-lane
//   ESP32-C6-MINI-1 Wi-Fi 6 / BT5 co-processor, attached over SDIO -- the P4 itself
//     has NO radio. Networking goes through esp_hosted, not the native Wi-Fi stack.
//   MIPI-CSI 2-lane camera header, SDIO 3.0 TF slot, ES8311 audio codec
//
// Anything below marked -1 is NOT yet confirmed from the vendor. Per CLAUDE.md these
// are never guessed: they come from the Waveshare demo or they stay -1 and the build
// refuses to pretend otherwise.

#define BOARD_NAME          "Waveshare ESP32-P4-WIFI6-Touch-LCD-4C"
#define BOARD_PANEL_QSPI    0
#define BOARD_PANEL_DSI     1        // esp_lcd_mipi_dsi, NOT Arduino_GFX

// ---------- Screen geometry ----------
// 720x720 is 2.4x the pixels of the 1.75" board. Radar radius keeps the same proportion
// of the panel (218/466 = 0.468) so the scope fills the round bezel identically.
#define SCREEN_W            720
#define SCREEN_H            720
#define SCREEN_CX           360
#define SCREEN_CY           360
#define RADAR_R_OUTER_PX    337
#define LCD_COL_OFFSET      0
#define LCD_ROW_OFFSET      0

// ---------- Shared I2C ----------
// Confirmed in the vendor wiki: "sets the GPIO number of the serial clock bus (SCL),
// which is 8" / "the serial data bus (SDA), which is 7". External pullups are fitted,
// so internal pullups stay off.
#define PIN_I2C_SDA         7
#define PIN_I2C_SCL         8

// ---------- Panel / touch ----------
// MIPI-DSI needs no databus GPIOs (the PHY is dedicated). The values below marked
// [COMMUNITY] come from a working ESPHome configuration for this exact board, not from
// Waveshare. They are far better than a guess but are NOT vendor-verified: confirm each
// against the vendor demo before trusting it. Audio and backlight were reported working;
// the display was not (see docs/PORT_P4.md).
#define PIN_LCD_RST         -1             // not in the community config
#define PIN_LCD_BL          26             // [COMMUNITY] LEDC PWM backlight
#define PIN_TP_INT          -1
#define PIN_TP_RST          -1
#define TP_MIRROR_X         false          // unverified: confirm against the touch demo
#define TP_MIRROR_Y         false
#define I2C_ADDR_TOUCH      0x14           // [COMMUNITY] GT911, its default address

// MIPI-DSI panel timing. [COMMUNITY] and SUSPECT: these were lifted from the 10.1"
// P4-NANO panel and the reporter never got output with them. 720+20+40+20 = 800 htotal
// and 720+4+24+12 = 760 vtotal at 80 MHz implies ~131 Hz, which is not a 4" panel
// figure. Treat as a starting point to replace with the vendor demo's own values.
#define DSI_LANES           2
#define DSI_LANE_BITRATE_HZ 1500000000UL
#define DSI_PCLK_HZ         80000000UL
#define DSI_HSYNC_PULSE     20
#define DSI_HSYNC_FRONT     40
#define DSI_HSYNC_BACK      20
#define DSI_VSYNC_PULSE     4
#define DSI_VSYNC_FRONT     24
#define DSI_VSYNC_BACK      12
// The DSI PHY is powered from an internal LDO that must be switched on explicitly.
// Forgetting it is the classic "everything logs fine, screen stays black" failure.
#define DSI_LDO_CHANNEL     3
#define DSI_LDO_MV          2500

// ---------- ES8311 codec over I2S ----------
// [COMMUNITY] reported working on this board.
#define PIN_I2S_MCLK        13
#define PIN_I2S_BCLK        12
#define PIN_I2S_LRCLK       10
#define PIN_I2S_DOUT        9              // ESP32 -> codec (speaker)
#define PIN_I2S_DIN         11             // codec -> ESP32 (mics)
#define PIN_AUDIO_PA        53             // BSP_POWER_AMP_IO
#define I2C_ADDR_AUDIO      0x18
#define PIN_BOOT_BUTTON     -1

// ---------- Peripherals present ----------
// No IMU, RTC or PMIC on this board: face-down sleep, battery reporting and the
// pre-WiFi clock all need to degrade gracefully rather than be assumed.
#define BOARD_HAS_IMU       0
#define BOARD_HAS_RTC       0
#define BOARD_HAS_PMIC      0
#define BOARD_HAS_AUDIO     1
#define I2C_ADDR_IMU        -1
#define I2C_ADDR_RTC        -1
#define I2C_ADDR_PMIC       -1

// Networking is via the C6 over SDIO (esp_hosted), so the Arduino WiFi objects are not
// a given. Flagged so code can branch instead of silently failing to connect.
#define BOARD_WIFI_HOSTED   1

#if (PIN_I2C_SDA < 0) || (PIN_I2C_SCL < 0)
#  error "board: I2C pins are placeholders (-1). Take them from the Waveshare demo."
#endif
