/**
 * @file test_brightness_step.cpp
 * @brief brightness_step: the Set_Brightness knob acceleration (original
 *        `increment = 10 - delta_time / 8`, at least 1) and the 0..255 clamp.
 */
#include "apps/set_brightness/brightness_step.h"
#include "host_test.h"

using namespace brightness_step;

HOST_TEST(brightness_increment_accelerates_when_fast) {
    HT_EQ(10, increment(0));
    HT_EQ(10, increment(7));
    HT_EQ(9, increment(8));
    HT_EQ(2, increment(64));
    HT_EQ(1, increment(72));
}

HOST_TEST(brightness_increment_is_at_least_one_when_slow) {
    HT_EQ(1, increment(80));       // 10 - 10 = 0 -> 1
    HT_EQ(1, increment(1000));     // original int math goes negative -> 1
    HT_EQ(1, increment(0xFFFFFFFFu));
}

HOST_TEST(brightness_apply_slow_steps_by_one) {
    HT_EQ(129, apply(128, 1, 1000));
    HT_EQ(131, apply(128, 3, 1000));
    HT_EQ(126, apply(128, -2, 1000));
    HT_EQ(128, apply(128, 0, 0));
}

HOST_TEST(brightness_apply_fast_steps_by_ten) {
    HT_EQ(138, apply(128, 1, 0));
    HT_EQ(108, apply(128, -2, 0));
}

HOST_TEST(brightness_apply_clamps_to_0_255) {
    HT_EQ(255, apply(250, 1, 0));
    HT_EQ(255, apply(255, 3, 1000));
    HT_EQ(0, apply(5, -1, 0));
    HT_EQ(0, apply(0, -3, 1000));
    HT_EQ(255, apply(0, 1000, 0));
    HT_EQ(0, apply(255, -1000, 0));
}

HOST_TEST(brightness_clamp_range) {
    HT_EQ(0, clamp(-1));
    HT_EQ(0, clamp(0));
    HT_EQ(255, clamp(255));
    HT_EQ(255, clamp(256));
    HT_EQ(MIN, 0);
    HT_EQ(MAX, 255);
}
