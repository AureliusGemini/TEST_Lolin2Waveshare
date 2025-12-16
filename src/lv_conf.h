#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

// 1. ENABLE THE CONFIG
#if 1

// 2. COLOR SETTINGS (Sunton uses 16-bit color)
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

// 3. MEMORY SETTINGS (Use ESP32 Internal RAM)
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (64U * 1024U) // 64KB for LVGL Widgets

// 4. TICK SETTINGS (Use Arduino millis)
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

// 5. FONTS (Enable the ones we use in the manual UI)
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1 // <--- Essential for the code I gave you
#define LV_FONT_DEFAULT &lv_font_montserrat_14

// 6. PERFORMANCE
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0

#endif // End of #if 1
#endif // End of LV_CONF_H