/**
 * @file test_encoder_detent.cpp
 * @brief EncoderDetent: one detent = exactly one step, floor-based so detents around raw 0
 *        are as wide as any other (Review Focus 2), sign configurable for clockwise.
 */
#include "encoder_detent.h"
#include "host_test.h"

HOST_TEST(detent_sequence_cw_sign_plus) {
    EncoderDetent d(4, +1);
    d.reset(0);
    HT_EQ(0, d.update(3));
    HT_EQ(1, d.update(4));
    HT_EQ(-1, d.update(0));
    // why: floor(-1 / 4) = -1, a truncating division would give 0 (double-wide dead zone)
    HT_EQ(-1, d.update(-1));
    HT_EQ(0, d.update(-4));
}

HOST_TEST(jump_counts_all_detents) {
    EncoderDetent d(4, +1);
    d.reset(0);
    HT_EQ(3, d.update(12));
}

HOST_TEST(cw_sign_minus_inverts) {
    EncoderDetent d(4, -1);
    d.reset(0);
    HT_EQ(0, d.update(3));
    HT_EQ(-1, d.update(4));
    HT_EQ(1, d.update(0));
    HT_EQ(1, d.update(-1));
    HT_EQ(0, d.update(-4));
    HT_EQ(-3, d.update(8));
}

HOST_TEST(no_change_is_zero) {
    EncoderDetent d(4, +1);
    d.reset(5);
    HT_EQ(0, d.update(5));
    HT_EQ(0, d.update(7));
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
