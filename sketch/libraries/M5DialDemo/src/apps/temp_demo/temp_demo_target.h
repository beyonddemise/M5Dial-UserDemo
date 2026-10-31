/**
 * @file temp_demo_target.h
 * @brief Arduino-free set-point stepping of the TempDemo app (upstream "VideoShit").
 *
 * Extracted from the original GUI_VideoShit::tempUp / tempDown (Forairaaaaa, M5Stack
 * M5Dial-UserDemo, MIT) so it can be unit-tested on the host
 * (test/host/test_temp_demo_target.cpp).
 */
#pragma once

/** TempDemo set-point: an index into the nine temperature images (temp_demo_1..9). */
namespace temp_demo_target {

constexpr int IMAGE_COUNT = 9;    ///< assets/temp_demo_icons/temp_demo_1..9
constexpr int DEFAULT_INDEX = 3;  ///< original `_temp(3)`
constexpr int FIRST_CELSIUS = 19; ///< temp_demo_1 shows 19 C, each next image one degree more

/** @return the temperature (degrees C) image `index` shows. */
inline int celsius(int index) { return FIRST_CELSIUS + index; }

/**
 * Moves the set-point by `detents` (positive = clockwise = warmer), clamped to the images.
 * why clamp, not wrap: as the original tempUp/tempDown - a thermostat stops at its limits.
 */
inline int step(int index, int detents) {
    int i = index + detents;
    return i < 0 ? 0 : (i >= IMAGE_COUNT ? IMAGE_COUNT - 1 : i);
}

}  // namespace temp_demo_target
