#pragma once
// Pulled in by LV_ASSERT_HANDLER_INCLUDE (see lv_conf.h). Kept as its own header because
// LVGL allows exactly one include there, and the handler needs both a flush and a reset.
//
// This is included from C as well as C++ -- the generated font tables are .c files -- so
// nothing here may be C++-only. That rules out Serial/ESP.restart(); the ESP-IDF calls
// below do the same job and compile in both languages.
#include <stdio.h>
#include <esp_system.h>
