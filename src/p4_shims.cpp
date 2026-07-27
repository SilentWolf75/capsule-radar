// Board shims for the ESP32-P4-WIFI6-Touch-LCD-4C.
//
// main.cpp is shared across boards and calls two things this board does not have in the
// same shape:
//
//   * the display:: namespace, which on the S3 wraps Arduino_GFX over QSPI. Here the
//     panel is MIPI-DSI and lives in display_p4.cpp, so these forward to it.
//   * QMI8658 IMU, PCF85063 RTC and AXP2101 PMIC, none of which are fitted. Rather than
//     scatter #ifdefs through main.cpp, they resolve to honest no-ops: no motion, no
//     battery, no RTC. The BOARD_HAS_* flags say which, so behaviour degrades visibly
//     (the HUD hides the battery, the clock waits for NTP) instead of reporting nonsense.
#include "config.h"
#if defined(BOARD_WAVESHARE_P4_LCD_4C)

#include <Arduino.h>
#include <lvgl.h>
#include <time.h>
#include "display.h"
#include "display_p4.h"

namespace display {

bool begin() { return display_p4_begin(); }

void loop() { lv_timer_handler(); }

// Backlight is a PWM output here rather than a panel command, so brightness maps onto
// the LEDC duty cycle. Anything above zero counts as on until a duty API is added.
void setBrightness(uint8_t v) { display_p4_backlight(v > 0); }

// Rotation is not implemented for DSI yet: the S3 path rotates in the flush callback by
// transposing blocks, which does not apply to a panel that owns its own framebuffer.
// Report 0 rather than pretend, so the config page shows the truth.
void setRotation(uint16_t) {}
uint16_t rotation() { return 0; }

// Screenshots need the flush path to mirror into a full framebuffer; not wired for DSI.
// Returning nullptr leaves /shot.bmp reporting failure rather than serving garbage.
const uint16_t *captureFrame() { return nullptr; }

uint32_t inactiveMs() { return lv_disp_get_inactive_time(nullptr); }

} // namespace display

// ---- peripherals this board does not carry -------------------------------------
#if !BOARD_HAS_IMU
bool imu_begin()    { return false; }
int  imu_facedown() { return -1; }   // -1 = unavailable, so the caller leaves state alone
#endif

#if !BOARD_HAS_PMIC
bool battery_begin()    { return false; }
bool battery_present()  { return false; }
bool battery_charging() { return false; }
int  battery_percent()  { return -1; }   // -1 = unknown; the HUD hides the indicator
// On the S3 this switches ALDO1 on for the ES8311's analog rail. The 4C has no PMIC,
// so the codec rail is either always on or switched elsewhere -- nothing to do here.
void battery_enable_codec_rail() {}
#endif

#if !BOARD_HAS_RTC
bool rtc_begin()             { return false; }
bool rtc_write(const tm *)   { return false; }
bool rtc_read(tm *)          { return false; }
#endif

#endif // BOARD_WAVESHARE_P4_LCD_4C
