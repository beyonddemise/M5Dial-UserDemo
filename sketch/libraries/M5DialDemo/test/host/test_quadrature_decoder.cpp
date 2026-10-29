/**
 * @file test_quadrature_decoder.cpp
 * @brief QuadratureDecoder must count exactly like M5Dial's (PJRC) Encoder.h state table.
 *        `ab` = (pinA << 1) | pinB, pin A = PJRC pin1 (GPIO41), pin B = PJRC pin2 (GPIO40).
 */
#include "host_test.h"
#include "quadrature_decoder.h"

namespace {

/** Feeds a sequence of AB states (after reset to the first) and returns the summed delta. */
int feed(std::initializer_list<uint8_t> seq) {
    QuadratureDecoder d;
    auto it = seq.begin();
    d.reset(*it++);
    int sum = 0;
    for (; it != seq.end(); ++it) sum += d.update(*it);
    return sum;
}

}  // namespace

// why: B leads A here (A stays low while B rises); PJRC counts that +1 per edge, so the
// opposite (A leads B = M5Dial clockwise) is -4 per cycle - the reason CW_SIGN is -1.
HOST_TEST(full_cycle_b_leads_is_plus_four) { HT_EQ(4, feed({0b00, 0b01, 0b11, 0b10, 0b00})); }

HOST_TEST(full_cycle_a_leads_is_minus_four) { HT_EQ(-4, feed({0b00, 0b10, 0b11, 0b01, 0b00})); }

HOST_TEST(each_edge_counts_one) {
    QuadratureDecoder d;
    d.reset(0b00);
    HT_EQ(1, d.update(0b01));
    HT_EQ(1, d.update(0b11));
    HT_EQ(1, d.update(0b10));
    HT_EQ(1, d.update(0b00));
}

HOST_TEST(bounce_on_one_pin_nets_zero) {
    HT_EQ(0, feed({0b00, 0b01, 0b00}));
    HT_EQ(0, feed({0b00, 0b10, 0b00, 0b10, 0b00}));
}

HOST_TEST(skipped_state_counts_two) {
    // PJRC: old 00 -> new 11 is +2 ("assume pin1 edges only"), old 01 -> new 10 (ab) is -2.
    HT_EQ(2, feed({0b00, 0b11}));
    HT_EQ(2, feed({0b11, 0b00}));
    HT_EQ(-2, feed({0b10, 0b01}));
    HT_EQ(-2, feed({0b01, 0b10}));
}

HOST_TEST(same_state_is_zero) {
    for (uint8_t ab = 0; ab < 4; ++ab) HT_EQ(0, feed({ab, ab, ab}));
}

HOST_TEST(reset_sets_the_reference_state) {
    QuadratureDecoder d;
    d.reset(0b11);
    HT_EQ(0, d.update(0b11));
    HT_EQ(1, d.update(0b10));
}

// Exhaustive check against PJRC's documented table (Encoder.h, "new pin2 new pin1 old pin2
// old pin1 -> Result").
HOST_TEST(matches_pjrc_table_exhaustively) {
    const int pjrc[16] = {0, +1, -1, +2, -1, 0, -2, +1, +1, -2, 0, -1, +2, -1, +1, 0};
    for (int s = 0; s < 16; ++s) {
        int oldP1 = s & 1, oldP2 = (s >> 1) & 1, newP1 = (s >> 2) & 1, newP2 = (s >> 3) & 1;
        QuadratureDecoder d;
        d.reset((uint8_t)((oldP1 << 1) | oldP2));
        HT_EQ(pjrc[s], d.update((uint8_t)((newP1 << 1) | newP2)));
    }
}
