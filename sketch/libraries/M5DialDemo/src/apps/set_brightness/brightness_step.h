/**
 * @file brightness_step.h
 * @brief Arduino-free brightness stepping of the Set_Brightness app: knob acceleration and the
 *        0..255 clamp.
 *
 * Extracted from the original app_set_brightness.cpp onRunning() (Forairaaaaa, M5Stack
 * M5Dial-UserDemo, MIT) so it can be unit-tested on the host
 * (test/host/test_brightness_step.cpp).
 */
#pragma once
#include <stdint.h>

/** Brightness stepping. */
namespace brightness_step {

constexpr int MIN = 0;     ///< panel backlight off
constexpr int MAX = 255;   ///< original clamp 0xFF (M5GFX setBrightness takes uint8_t)
constexpr int MAX_INCREMENT = 10;  ///< original `10 - delta_time / 8` at delta 0
constexpr uint32_t MS_PER_INCREMENT = 8;  ///< every 8 ms between detents costs one unit

/** @return `value` limited to MIN..MAX. */
inline int clamp(int value) { return value < MIN ? MIN : (value > MAX ? MAX : value); }

/**
 * Brightness change per detent, from the time since the previous detent: the original
 * `y = -(1/8) x + 10`, at least 1 - a fast turn moves 10 per detent, a slow one 1.
 * why the explicit compare: the original subtracted in int and let it go negative; with an
 * unsigned time the subtraction must not wrap.
 */
inline int increment(uint32_t msSinceLastDetent) {
    uint32_t slowdown = msSinceLastDetent / MS_PER_INCREMENT;
    if (slowdown >= static_cast<uint32_t>(MAX_INCREMENT)) return 1;
    return MAX_INCREMENT - static_cast<int>(slowdown);  // 1..10
}

/**
 * @return `current` moved by `detents` (clockwise positive = brighter), each worth
 * increment(msSinceLastDetent), clamped to MIN..MAX.
 */
inline int apply(int current, int32_t detents, uint32_t msSinceLastDetent) {
    // why int64: a large detent count times 10 must not overflow before the clamp
    int64_t next = static_cast<int64_t>(current) +
                   static_cast<int64_t>(detents) * increment(msSinceLastDetent);
    return next < MIN ? MIN : (next > MAX ? MAX : static_cast<int>(next));
}

}  // namespace brightness_step
