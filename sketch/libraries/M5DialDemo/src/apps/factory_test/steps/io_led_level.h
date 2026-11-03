/**
 * @file io_led_level.h
 * @brief Arduino-free pot-voltage-to-LED-ring mapping of the factory test's IO step.
 *
 * Extracted from the original io_test.cpp `_io_test()` loop (Forairaaaaa, M5Stack
 * M5Dial-UserDemo, MIT) so it can be unit-tested on the host (test/host/test_io_led_level.cpp).
 */
#pragma once

/** IO step LED mapping. */
namespace io_led_level {

constexpr int PIXELS = 14;           ///< original LED_STRIP_LED_NUMBERS (jig ring)
constexpr int MAX_LEVEL = 6;         ///< 3000 / 500
constexpr int FULL_MV = 3000;        ///< original `3000 - adc`
constexpr int SNAP_ABOVE_MV = 2900;  ///< original: above 2900 counts as 3000
constexpr int MV_PER_LEVEL = 500;    ///< original `/ 500`

/**
 * LED level of a pot reading, as the original: `3000 - mv`, above 2900 -> 3000, then `/ 500`
 * (0 V -> 6, 3.3 V -> 0; the pot is wired so that turning it up empties the ring).
 * why the clamp at 0: C++ `/` truncates, so readings up to 3499 mV already give 0 as upstream;
 * above that upstream gave -1 (ring dark). 0 keeps the log value in the documented 0..6.
 * @param mv calibrated ADC reading in millivolts (analogReadMilliVolts)
 * @return 0..MAX_LEVEL
 */
inline int level(int mv) {
    int v = FULL_MV - mv;
    if (v > SNAP_ABOVE_MV) v = FULL_MV;
    int n = v / MV_PER_LEVEL;
    return n < 0 ? 0 : n;
}

/** @return true if ring pixel `i` (0..PIXELS-1) is lit at `lvl`: filled from both ends. */
inline bool lit(int i, int lvl) { return i <= lvl || (PIXELS - 1 - i) <= lvl; }

/** @return number of lit pixels at `lvl` (2 at level 0, all 14 at level 6). */
inline int litCount(int lvl) {
    int n = 0;
    for (int i = 0; i < PIXELS; ++i)
        if (lit(i, lvl)) ++n;
    return n;
}

}  // namespace io_led_level
