/**
 * @file test_io_led_level.cpp
 * @brief io_led_level: the factory IO step's pot-to-LED mapping, as the original io_test.cpp:
 *        `3000 - mv`, above 2900 -> 3000, then `/ 500`; pixel i lit if i <= level or
 *        13 - i <= level (the ring fills from both ends).
 */
#include "apps/factory_test/steps/io_led_level.h"
#include "host_test.h"

using namespace io_led_level;

HOST_TEST(led_level_pot_at_zero_volt_is_six) {
    HT_EQ(6, level(0));
    HT_EQ(6, level(99));   // 2901 > 2900 -> 3000
    HT_EQ(5, level(100));  // 2900 is not > 2900 -> 2900 / 500
}

HOST_TEST(led_level_steps_of_500_mv) {
    HT_EQ(5, level(500));
    HT_EQ(4, level(501));  // 2499 / 500
    HT_EQ(4, level(1000));
    HT_EQ(3, level(1500));
    HT_EQ(2, level(2000));  // 1000 / 500
    HT_EQ(1, level(2001));  // 999 / 500
}

HOST_TEST(led_level_pot_at_3v3_is_zero) {
    HT_EQ(0, level(2600));  // 400 / 500
    HT_EQ(0, level(3000));
    HT_EQ(0, level(3100));  // ADC full scale (11 dB): negative, truncates to 0 as upstream
    HT_EQ(0, level(3300));
}

HOST_TEST(led_level_never_negative) {
    HT_EQ(0, level(4000));  // upstream would give -1 (all off); clamped to 0
    HT_EQ(6, level(-1000));  // below 0 V (never on the jig): 4000 > 2900 -> 3000
}

HOST_TEST(led_lit_fills_from_both_ends) {
    HT_EQ(14, PIXELS);
    // level 0: the two end pixels
    HT_EQ(true, lit(0, 0));
    HT_EQ(true, lit(13, 0));
    HT_EQ(false, lit(1, 0));
    HT_EQ(false, lit(12, 0));
    HT_EQ(2, litCount(0));
    // level 6: 0..6 and 7..13 = all 14
    HT_EQ(14, litCount(6));
    // level 3: 0..3 and 10..13
    HT_EQ(8, litCount(3));
    HT_EQ(false, lit(4, 3));
    HT_EQ(true, lit(10, 3));
}
