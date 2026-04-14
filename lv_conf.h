/**
 * LVGL 9.x configuration for sensesp-cockpit-display.
 *
 * This file must be found by LVGL's build system. It's included via
 * -DLV_CONF_INCLUDE_SIMPLE in platformio.ini build_flags.
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* --- Color --- */
#define LV_COLOR_DEPTH 16  /* RGB565 for MIPI-DSI panels */

/* --- Memory --- */
#define LV_MEM_CUSTOM 1  /* Use stdlib malloc (PSRAM-capable) */

/* --- OS --- */
#define LV_USE_OS LV_OS_NONE  /* We call lv_timer_handler() from ReactESP loop */

/* --- Display --- */
#define LV_DEF_REFR_PERIOD 33  /* ~30fps refresh */

/* --- Input device --- */
#define LV_INDEV_DEF_READ_PERIOD 10  /* 100Hz touch read */

/* --- Drawing --- */
#define LV_USE_DRAW_SW 1
#define LV_DRAW_SW_COMPLEX 1
#define LV_USE_DRAW_ARM2D 0
#define LV_USE_DRAW_SW_ASM LV_DRAW_SW_ASM_NONE  /* No ARM asm on RISC-V */

/* --- Logging --- */
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

/* --- Fonts --- */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_36 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_20

/* --- Widgets --- */
#define LV_USE_LABEL 1
#define LV_USE_BTN 1
#define LV_USE_SWITCH 1
#define LV_USE_ARC 1
#define LV_USE_BAR 1
#define LV_USE_SLIDER 0
#define LV_USE_TABLE 0
#define LV_USE_TEXTAREA 0
#define LV_USE_KEYBOARD 0
#define LV_USE_DROPDOWN 0
#define LV_USE_ROLLER 0
#define LV_USE_CHECKBOX 0
#define LV_USE_BTNMATRIX 1
#define LV_USE_LINE 1
#define LV_USE_IMAGE 1
#define LV_USE_CANVAS 0
/* LV_USE_ANIMIMAGE removed — LVGL 9 uses LV_USE_ANIMIMG internally */
#define LV_USE_SPAN 0
#define LV_USE_SCALE 1

/* --- Layouts --- */
#define LV_USE_FLEX 1
#define LV_USE_GRID 1

/* --- Extras --- */
#define LV_USE_TABVIEW 1
#define LV_USE_CHART 0
#define LV_USE_METER 0
#define LV_USE_CALENDAR 0
#define LV_USE_SPINBOX 0
#define LV_USE_COLORWHEEL 0
#define LV_USE_LED 1
#define LV_USE_LIST 0
#define LV_USE_MENU 0
#define LV_USE_MSGBOX 0
#define LV_USE_SPAN 0
#define LV_USE_TILEVIEW 0
#define LV_USE_WIN 0

/* --- Theme --- */
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1  /* Dark theme base for marine use */

/* --- Performance --- */
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0

#endif /* LV_CONF_H */
