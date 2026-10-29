/**
 * @file config.h
 * @brief Build-time configuration of the M5DialDemo library. Every value can be overridden
 *        with a compiler define (-D...) before this header is included.
 *
 * Kept free of Arduino/M5 includes: the host unit tests include it (tools/test_host.sh).
 */
#pragma once

/**
 * Raw encoder counts per mechanical detent.
 * why: M5Dial's encoder (PJRC Encoder.h state table, mirrored by QuadratureDecoder) counts
 * every edge of both channels (x4), and one detent is one full quadrature cycle.
 */
#ifndef M5DD_ENCODER_COUNTS_PER_DETENT
#define M5DD_ENCODER_COUNTS_PER_DETENT 4
#endif

/**
 * Sign that turns a clockwise raw count into a positive detent step (+1 = next item).
 * why: with pin A = GPIO41 (PJRC pin1) and B = GPIO40, clockwise means A leads B, which the
 * PJRC table counts down (-4 per detent); see docs/PORTING_NOTES.md "CW sign" (Task 3).
 * TODO(Task 3/19): derived from library code only - confirm with the DialProbe encoder
 * scenario once the Wokwi quota is back, and physically on the device (Task 19).
 */
#ifndef M5DD_ENCODER_CW_SIGN
#define M5DD_ENCODER_CW_SIGN (-1)
#endif

/**
 * Factory-test entry check in the UserDemo sketch (hold the button > 1 s at boot).
 * Defaults on, as the original build (CMakeLists.txt: add_definitions(-DENABLE_FACTORY_TEST));
 * build with -DENABLE_FACTORY_TEST=0 to drop it.
 */
#ifndef ENABLE_FACTORY_TEST
#define ENABLE_FACTORY_TEST 1
#endif

/**
 * GUI fonts of the original (CMakeLists.txt add_definitions). Plain text macros: they only
 * need M5GFX where they are used, so this header stays Arduino-free.
 */
#ifndef GUI_FONT_CN_BIG
#define GUI_FONT_CN_BIG (&fonts::efontCN_24)
#endif
#ifndef GUI_FONT_CN_SMALL
#define GUI_FONT_CN_SMALL (&fonts::efontCN_16_b)
#endif
