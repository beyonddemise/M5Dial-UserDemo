/**
 * @file test_encoder_detent.cpp
 * @brief EncoderDetent: one detent = exactly one step, with a window centred on the rest
 *        position (raw = multiple of 4), so +-1 chatter at rest never steps and detents around
 *        raw 0 are as wide as any other (Review Focus 2); sign configurable for clockwise.
 *        Index = floor((raw + 2) / 4): detent 0 covers raw -2..+1.
 */
#include "encoder_detent.h"
#include "host_test.h"

HOST_TEST(rest_chatter_never_steps) {
    EncoderDetent d(4, +1);
    d.reset(0);
    HT_EQ(0, d.update(1));
    HT_EQ(0, d.update(0));
    HT_EQ(0, d.update(-1));
    HT_EQ(0, d.update(0));
    HT_EQ(0, d.update(-2));
    HT_EQ(0, d.update(1));
}

HOST_TEST(half_detent_steps_once_positive) {
    EncoderDetent d(4, +1);
    d.reset(0);
    HT_EQ(1, d.update(2));
    HT_EQ(0, d.update(3));
    HT_EQ(0, d.update(4));  // next rest position: no second step
    HT_EQ(0, d.update(5));
    HT_EQ(1, d.update(6));
}

HOST_TEST(back_below_half_point_undoes_step) {
    // why: the window boundary sits mid-detent (raw 2 mod 4), where the knob never rests
    EncoderDetent d(4, +1);
    d.reset(0);
    HT_EQ(1, d.update(2));
    HT_EQ(-1, d.update(1));
}

HOST_TEST(negative_side_symmetric) {
    EncoderDetent d(4, +1);
    d.reset(0);
    HT_EQ(0, d.update(-2));  // floor(0 / 4) = 0: still detent 0
    HT_EQ(-1, d.update(-3));
    HT_EQ(0, d.update(-4));  // next rest position: no second step
    HT_EQ(0, d.update(-6));
    HT_EQ(-1, d.update(-7));
    HT_EQ(1, d.update(-6));
}

HOST_TEST(full_detents_count_one_each) {
    EncoderDetent d(4, +1);
    d.reset(0);
    HT_EQ(1, d.update(4));
    HT_EQ(-1, d.update(0));
    HT_EQ(-1, d.update(-4));
    HT_EQ(1, d.update(0));
}

HOST_TEST(jump_counts_all_detents) {
    EncoderDetent d(4, +1);
    d.reset(0);
    HT_EQ(3, d.update(12));
    HT_EQ(-6, d.update(-12));
}

HOST_TEST(cw_sign_minus_inverts) {
    EncoderDetent d(4, -1);
    d.reset(0);
    HT_EQ(0, d.update(1));
    HT_EQ(-1, d.update(2));
    HT_EQ(2, d.update(-4));
    HT_EQ(1, d.update(-7));
    HT_EQ(-4, d.update(8));
}

HOST_TEST(no_change_is_zero) {
    EncoderDetent d(4, +1);
    d.reset(5);
    HT_EQ(0, d.update(5));
    HT_EQ(0, d.update(4));
}

HOST_TEST(reset_rebases_without_step) {
    EncoderDetent d(4, +1);
    d.reset(0);
    d.reset(100);
    HT_EQ(0, d.update(100));
    HT_EQ(1, d.update(104));
}

HOST_TEST(defaults_come_from_config) {
    EncoderDetent d;
    d.reset(0);
    HT_EQ(M5DD_ENCODER_CW_SIGN, d.update(M5DD_ENCODER_COUNTS_PER_DETENT));
}
