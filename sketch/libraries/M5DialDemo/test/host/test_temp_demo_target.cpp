/**
 * @file test_temp_demo_target.cpp
 * @brief temp_demo_target: the TempDemo set-point (original GUI_VideoShit tempUp/tempDown:
 *        image index 0..8, start 3, clamped) and the temperature each image shows (19..27 C).
 */
#include "apps/temp_demo/temp_demo_target.h"
#include "host_test.h"

using namespace temp_demo_target;

HOST_TEST(temp_starts_at_22_degrees) {
    HT_EQ(9, IMAGE_COUNT);
    HT_EQ(3, DEFAULT_INDEX);
    HT_EQ(22, celsius(DEFAULT_INDEX));
}

HOST_TEST(temp_images_show_19_to_27) {
    HT_EQ(19, celsius(0));
    HT_EQ(27, celsius(IMAGE_COUNT - 1));
}

HOST_TEST(temp_step_moves_by_detents) {
    HT_EQ(4, step(3, 1));    // clockwise = warmer
    HT_EQ(2, step(3, -1));
    HT_EQ(6, step(3, 3));
    HT_EQ(3, step(3, 0));
}

HOST_TEST(temp_step_clamps_without_wrap) {
    HT_EQ(8, step(8, 1));
    HT_EQ(8, step(3, 100));
    HT_EQ(0, step(0, -1));
    HT_EQ(0, step(3, -100));
}
