/**
 * @file lv_conf.h
 * @brief LVGL 9.6 configuration of examples/LvglDemo (only the options that differ from, or
 *        are pinned against, LVGL's defaults).
 *
 * why a short file instead of a full copy of lv_conf_template.h: lv_conf_internal.h supplies
 * the default for every option not defined here, so this file lists exactly the choices this
 * sketch makes and keeps working when a newer LVGL adds options.
 *
 * Found through tools/build.sh, which adds `-DLV_CONF_INCLUDE_SIMPLE -I<sketch dir>` to the C
 * and C++ flags of any sketch whose directory contains an lv_conf.h (docs/PORTING_NOTES.md,
 * "LvglDemo"). Included from LVGL's C sources too, so plain C preprocessor only.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

/** RGB565, the panel's native depth; the flush hands it to M5GFX as lgfx::rgb565_t. */
#define LV_COLOR_DEPTH 16

/**
 * LVGL's own heap (builtin TLSF allocator over a static array, i.e. .bss, not malloc).
 * why 64 KB (LVGL's default, pinned here): widgets, styles and draw tasks of this small UI
 * need a fraction of it, and the ESP32-S3 without PSRAM still has well over 200 KB free.
 */
#define LV_USE_STDLIB_MALLOC LV_STDLIB_BUILTIN
#define LV_MEM_SIZE (64 * 1024U)

/** No OS integration: lv_timer_handler() runs from loop() on the Arduino task only. */
#define LV_USE_OS LV_OS_NONE

/** Logging goes through M5_LOGI in the sketch; LVGL's own logger stays off. */
#define LV_USE_LOG 0

/** Value label font (14 px is LVGL's default and stays the default font). */
#define LV_FONT_MONTSERRAT_28 1

/** Dark default theme: the round panel's bezel is black. */
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1

#endif /* LV_CONF_H */
