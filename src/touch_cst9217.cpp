// CST9217 capacitive touch over I2C (Arduino). Ported from Waveshare's
// esp_lcd_touch_cst9217: read 10 bytes from reg 0xD000, validate ACK 0xAB,
// unpack the 12-bit X/Y of the first touch point. Single-touch is enough here.
#include "touch_cst9217.h"
#include "config.h"
#include <Arduino.h>
#include <Wire.h>

#define CST9217_REG_DATA 0xD000
#define CST9217_ACK      0xAB
#define CST9217_DATA_LEN 10        // 1 point * 5 + 5
#define CST9217_DATA_LEN2 15       // 2 points * 5 + 5 (second point starts at byte 7)

// Read `len` bytes from a 16-bit register (address sent big-endian, then STOP).
static bool cst_read_reg(uint16_t reg, uint8_t *data, uint8_t len) {
    Wire.beginTransmission((uint8_t)I2C_ADDR_TOUCH);
    Wire.write((uint8_t)(reg >> 8));
    Wire.write((uint8_t)(reg & 0xFF));
    if (Wire.endTransmission(true) != 0) return false;
    delayMicroseconds(500);                       // chip needs a moment before the read
    if (Wire.requestFrom((uint8_t)I2C_ADDR_TOUCH, len) < len) return false;
    for (uint8_t i = 0; i < len; ++i) data[i] = Wire.read();
    return true;
}

bool touch_begin() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);

    // hardware reset (low 10 ms, high 50 ms)
    pinMode(PIN_TP_RST, OUTPUT);
    digitalWrite(PIN_TP_RST, LOW);
    delay(10);
    digitalWrite(PIN_TP_RST, HIGH);
    delay(50);
    pinMode(PIN_TP_INT, INPUT);

    // comms check: a valid (even untouched) data read carries ACK 0xAB at byte 6
    uint8_t d[CST9217_DATA_LEN] = {0};
    if (cst_read_reg(CST9217_REG_DATA, d, CST9217_DATA_LEN) && d[6] == CST9217_ACK) {
        Serial.println("[touch] CST9217 responding (ACK ok)");
    } else {
        Serial.println("[touch] CST9217 not responding yet (will keep polling)");
    }
    return true;
}

// Unpack one 5-byte point block (status/id, x_h, y_h, xy_l, pressure) into screen px.
// Returns false when the block's status nibble isn't "down".
static bool cst_unpack_point(const uint8_t *b, uint16_t *ox, uint16_t *oy) {
    if ((b[0] & 0x0F) != 0x06) return false;       // point status must be "down"
    uint16_t x = ((uint16_t)b[1] << 4) | (b[3] >> 4);
    uint16_t y = ((uint16_t)b[2] << 4) | (b[3] & 0x0F);
    if (x > SCREEN_W - 1) x = SCREEN_W - 1;
    if (y > SCREEN_H - 1) y = SCREEN_H - 1;
    if (TP_MIRROR_X) x = (SCREEN_W - 1) - x;
    if (TP_MIRROR_Y) y = (SCREEN_H - 1) - y;
    *ox = x;
    *oy = y;
    return true;
}

bool touch_read(uint16_t *ox, uint16_t *oy) {
    uint8_t d[CST9217_DATA_LEN];
    if (!cst_read_reg(CST9217_REG_DATA, d, CST9217_DATA_LEN)) return false;
    if (d[6] != CST9217_ACK) return false;

    const uint8_t points = d[5] & 0x7F;
    if (points == 0) return false;
    return cst_unpack_point(d, ox, oy);
}

// Read up to two active touch points (for pinch-zoom). Layout mirrors the vendor
// esp_lcd_touch_cst9217 report: point 0 in bytes 0..4, count in 5, ACK in 6, then
// each further point in the next 5 bytes (point 1 = bytes 7..11).
int touch_read_points(uint16_t x[2], uint16_t y[2]) {
    uint8_t d[CST9217_DATA_LEN2];
    if (!cst_read_reg(CST9217_REG_DATA, d, CST9217_DATA_LEN2)) return 0;
    if (d[6] != CST9217_ACK) return 0;

    const uint8_t points = d[5] & 0x7F;
    if (points == 0) return 0;

    int n = 0;
    if (cst_unpack_point(d, &x[n], &y[n])) ++n;
    if (points >= 2 && cst_unpack_point(d + 7, &x[n], &y[n])) ++n;
    return n;
}
