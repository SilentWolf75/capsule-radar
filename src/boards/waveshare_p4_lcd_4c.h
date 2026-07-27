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
// MIPI-DSI needs no databus GPIOs (the DSI PHY is dedicated), but reset/backlight and
// the touch controller's INT/RST are board wiring and still have to come from the demo.
#define PIN_LCD_RST         -1
#define PIN_LCD_BL          -1
#define PIN_TP_INT          -1
#define PIN_TP_RST          -1
#define TP_MIRROR_X         false          // unverified: confirm against the touch demo
#define TP_MIRROR_Y         false
#define I2C_ADDR_TOUCH      -1             // controller not yet identified (GT911 likely)

// ---------- ES8311 codec over I2S ----------
#define PIN_I2S_MCLK        -1
#define PIN_I2S_BCLK        -1
#define PIN_I2S_LRCLK       -1
#define PIN_I2S_DOUT        -1
#define PIN_I2S_DIN         -1
#define PIN_AUDIO_PA        -1
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
