#pragma once
// Weather glyphs drawn as vectors, from the WMO code Open-Meteo returns.
//
// No bitmaps and no icon font: the shapes are built from LVGL primitives, so they cost
// nothing in flash, scale to whatever box they are given, and pick up the theme tint.
// Attach weather_icon_attach() to a small object and set its code with
// weather_icon_set(); the object repaints itself.
#include <lvgl.h>

void weather_icon_attach(lv_obj_t *obj);        // install the draw callback
void weather_icon_set(lv_obj_t *obj, int wmoCode, bool night);
