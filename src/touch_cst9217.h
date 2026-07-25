#pragma once
// Minimal Arduino driver for the CST9217 capacitive touch (I2C). Device-only.
// Protocol ported from Waveshare's esp_lcd_touch_cst9217 component.
#include <stdint.h>

bool touch_begin();                       // I2C + hardware reset; logs comms status
bool touch_read(uint16_t *x, uint16_t *y); // true if currently pressed (x,y in screen px)
int  touch_read_points(uint16_t x[2], uint16_t y[2]); // 0/1/2 active points (pinch support)
