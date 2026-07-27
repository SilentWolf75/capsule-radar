// GT911 capacitive touch for the ESP32-P4-WIFI6-Touch-LCD-4C.
//
// The controller was reported as GT911 by a community ESPHome config for this exact
// board, where touch was working. The register map below is GT911's documented one, so
// the protocol is not guesswork -- only the board wiring (which I2C address the panel
// straps to, and whether the axes need mirroring) needs confirming on hardware.
//
// GT911 has two possible addresses, 0x5D and 0x14, selected by the state of INT during
// reset. Rather than hard-code one, probe both: that is cheap and removes a whole class
// of "nothing responds" bring-up confusion.
#include "config.h"
#if defined(BOARD_WAVESHARE_P4_LCD_4C)

#include <Arduino.h>
#include <Wire.h>
#include "touch_gt911.h"

#define GT911_ADDR_A        I2C_ADDR_TOUCH
#define GT911_ADDR_B        I2C_ADDR_TOUCH_ALT
#define GT911_REG_STATUS    0x814E   // bit7 = data ready, bits3..0 = touch count
#define GT911_REG_POINT1    0x8150   // x_lo, x_hi, y_lo, y_hi, size_lo, size_hi, reserved
#define GT911_REG_PRODUCT   0x8140   // 4 ASCII bytes, "911" for a GT911

static uint8_t s_addr = 0;

static bool rd(uint16_t reg, uint8_t *buf, size_t n) {
    Wire.beginTransmission(s_addr);
    Wire.write((uint8_t)(reg >> 8));
    Wire.write((uint8_t)(reg & 0xFF));
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)s_addr, (int)n) != (int)n) return false;
    for (size_t i = 0; i < n; ++i) buf[i] = Wire.read();
    return true;
}

static bool wr8(uint16_t reg, uint8_t v) {
    Wire.beginTransmission(s_addr);
    Wire.write((uint8_t)(reg >> 8));
    Wire.write((uint8_t)(reg & 0xFF));
    Wire.write(v);
    return Wire.endTransmission() == 0;
}

bool touch_gt911_begin(void) {
    // Hold the controller in reset briefly so it latches a known address. INT is not
    // connected on this board (vendor: GPIO_NUM_NC), so it strays to the 0x5D default.
    if (PIN_TP_RST >= 0) {
        pinMode(PIN_TP_RST, OUTPUT);
        digitalWrite(PIN_TP_RST, LOW);
        delay(10);
        digitalWrite(PIN_TP_RST, HIGH);
        delay(60);
    }
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(I2C_CLOCK_HZ);   // [VENDOR] 100 kHz; 400 k is not what the demo uses

    const uint8_t candidates[2] = { GT911_ADDR_A, GT911_ADDR_B };
    uint8_t id[4] = {0};
    for (int i = 0; i < 2; ++i) {
        s_addr = candidates[i];
        if (rd(GT911_REG_PRODUCT, id, 4) && id[0] == '9' && id[1] == '1' && id[2] == '1') {
            Serial.printf("[gt911] found at 0x%02X\n", s_addr);
            return true;
        }
    }
    s_addr = 0;
    Serial.println("[gt911] not found at 0x5D or 0x14");
    return false;
}

// Returns true while a finger is down, with the coordinate in *x/*y.
bool touch_gt911_read(int *x, int *y) {
    if (!s_addr) return false;
    uint8_t st = 0;
    if (!rd(GT911_REG_STATUS, &st, 1)) return false;
    if (!(st & 0x80)) return false;                 // no new data

    const int count = st & 0x0F;
    bool down = false;
    if (count > 0) {
        uint8_t p[4];
        if (rd(GT911_REG_POINT1, p, 4)) {
            int px = (int)(p[0] | (p[1] << 8));
            int py = (int)(p[2] | (p[3] << 8));
            if (TP_MIRROR_X) px = SCREEN_W - 1 - px;
            if (TP_MIRROR_Y) py = SCREEN_H - 1 - py;
            if (x) *x = px;
            if (y) *y = py;
            down = true;
        }
    }
    // The status register must be cleared or the controller stops reporting.
    wr8(GT911_REG_STATUS, 0);
    return down;
}

// LVGL input device callback.
void touch_gt911_lvgl_read(lv_indev_drv_t *, lv_indev_data_t *data) {
    static int lx = 0, ly = 0;
    int x = lx, y = ly;
    if (touch_gt911_read(&x, &y)) {
        lx = x; ly = y;
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->point.x = lx;
        data->point.y = ly;
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

#endif // BOARD_WAVESHARE_P4_LCD_4C
