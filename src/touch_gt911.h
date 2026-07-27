#pragma once
// GT911 capacitive touch (ESP32-P4-WIFI6-Touch-LCD-4C). See touch_gt911.cpp.
#include "config.h"
#if defined(BOARD_WAVESHARE_P4_LCD_4C)
#include <lvgl.h>
bool touch_gt911_begin(void);                 // probes 0x5D then 0x14, verifies product id
bool touch_gt911_read(int *x, int *y);        // true while pressed
void touch_gt911_lvgl_read(lv_indev_drv_t *drv, lv_indev_data_t *data);
#endif
