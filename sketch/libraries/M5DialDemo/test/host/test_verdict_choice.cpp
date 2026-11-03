/**
 * @file test_verdict_choice.cpp
 * @brief verdict_choice: the highlight of the factory test's PASS/FAIL/SKIP prompt - knob
 *        detents move it (clockwise = next, Review Focus 3), clamped at both ends.
 */
#include "apps/factory_test/verdict_choice.h"
#include <cstdint>

#include "host_test.h"

using factory_test::Verdict;
using namespace verdict_choice;

HOST_TEST(choice_order_is_pass_fail_skip) {
    HT_EQ(3, COUNT);
    HT_EQ((int)Verdict::Pass, (int)at(0));
    HT_EQ((int)Verdict::Fail, (int)at(1));
    HT_EQ((int)Verdict::Skip, (int)at(2));
}

HOST_TEST(choice_index_of_default) {
    HT_EQ(0, indexOf(Verdict::Pass));
    HT_EQ(1, indexOf(Verdict::Fail));
    HT_EQ(2, indexOf(Verdict::Skip));
    HT_EQ(0, indexOf(Verdict::Pending));  // not offered: falls back to PASS
}

HOST_TEST(choice_clockwise_is_next) {
    HT_EQ(1, move(0, 1));
    HT_EQ(2, move(0, 2));  // two CW detents from PASS = SKIP
    HT_EQ(2, move(1, 1));
}

HOST_TEST(choice_counter_clockwise_is_previous) {
    HT_EQ(1, move(2, -1));
    HT_EQ(0, move(2, -2));
}

HOST_TEST(choice_clamps_at_both_ends) {
    HT_EQ(2, move(2, 1));
    HT_EQ(2, move(0, 50));
    HT_EQ(0, move(0, -1));
    HT_EQ(0, move(1, -50));
    HT_EQ(1, move(1, 0));
}

HOST_TEST(choice_extreme_detents_clamp_without_overflow) {
    // why: int32 limits catch a narrowed sum, which would wrap and clamp to the wrong end
    HT_EQ(2, move(2, INT32_MAX));
    HT_EQ(0, move(0, INT32_MIN));
    HT_EQ(2, move(0, INT32_MAX));
    HT_EQ(0, move(2, INT32_MIN));
}

HOST_TEST(choice_out_of_range_index_is_clamped) {
    HT_EQ(0, move(-5, 0));
    HT_EQ(2, move(9, 0));
    HT_EQ((int)Verdict::Pass, (int)at(-1));
    HT_EQ((int)Verdict::Skip, (int)at(7));
}
