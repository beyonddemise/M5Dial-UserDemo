/**
 * @file test_more_menu_items.cpp
 * @brief more_menu_items: the More menu's entries (upstream list minus the three LVGL demos),
 *        their row geometry, the curved x offset, the text shade and the selector mask.
 */
#include <cstring>

#include "apps/more_menu/more_menu_items.h"
#include "host_test.h"

using namespace more_menu_items;

HOST_TEST(more_quit_and_power_off_are_the_first_two_entries) {
    HT_EQ(0, ITEM_QUIT);
    HT_EQ(1, ITEM_POWER_OFF);
    HT_EQ(0, std::strcmp("Quit", tag(ITEM_QUIT)));
    HT_EQ(0, std::strcmp("Power Off", tag(ITEM_POWER_OFF)));
}

HOST_TEST(more_list_is_upstream_without_the_lvgl_entries) {
    // upstream: 16 entries, of which "LVGL Widgets", "LVGL Benchmark", "LVGL Stress" are dropped
    HT_EQ(13, ITEM_COUNT);
    for (int i = 0; i < ITEM_COUNT; ++i) {
        HT_EQ(true, std::strstr(tag(i), "LVGL") == nullptr);
    }
    HT_EQ(0, std::strcmp("???", tag(2)));
    HT_EQ(0, std::strcmp("List", tag(ITEM_COUNT - 1)));
}

HOST_TEST(more_tag_out_of_range_is_empty) {
    HT_EQ(0, std::strcmp("", tag(-1)));
    HT_EQ(0, std::strcmp("", tag(ITEM_COUNT)));
}

HOST_TEST(more_rows_are_upstream_x24_y_10_plus_26i_12px_per_char_24_high) {
    Rect r0 = itemRect(0);
    HT_EQ(24, r0.x);
    HT_EQ(10, r0.y);
    HT_EQ(4 * 12, r0.w);  // "Quit"
    HT_EQ(24, r0.h);
    Rect r1 = itemRect(1);
    HT_EQ(36, r1.y);
    HT_EQ(9 * 12, r1.w);  // "Power Off"
}

HOST_TEST(more_selected_row_has_no_curve_offset) {
    HT_EQ(0, curveOffsetX(0.0f));
}

HOST_TEST(more_curve_offset_grows_symmetrically_away_from_the_selector) {
    int up = curveOffsetX(-52.0f);
    int down = curveOffsetX(52.0f);
    HT_EQ(up, down);
    HT_EQ(true, down > curveOffsetX(26.0f));
    HT_EQ(true, curveOffsetX(26.0f) > 0);
}

HOST_TEST(more_text_shade_fades_with_distance_and_clamps_at_black) {
    HT_EQ(255, textShade(0.0f));
    HT_EQ(203, textShade(26.0f));
    HT_EQ(203, textShade(-26.0f));
    HT_EQ(1, textShade(127.0f));
    // why the clamp: upstream assigned the negative float to uint8_t (undefined behaviour)
    HT_EQ(0, textShade(128.0f));
    HT_EQ(0, textShade(500.0f));
}

HOST_TEST(more_round_rect_mask_covers_inside_and_cuts_the_corners) {
    // 20x10 at (5, 5), radius 4
    HT_EQ(true, inRoundRect(15, 10, 5, 5, 20, 10, 4));   // centre
    HT_EQ(true, inRoundRect(9, 5, 5, 5, 20, 10, 4));     // top edge, past the corner
    HT_EQ(true, inRoundRect(5, 9, 5, 5, 20, 10, 4));     // left edge, past the corner
    HT_EQ(false, inRoundRect(5, 5, 5, 5, 20, 10, 4));    // top-left corner pixel
    HT_EQ(false, inRoundRect(24, 14, 5, 5, 20, 10, 4));  // bottom-right corner pixel
    HT_EQ(true, inRoundRect(24, 9, 5, 5, 20, 10, 4));    // right edge, middle
    HT_EQ(false, inRoundRect(25, 9, 5, 5, 20, 10, 4));   // just outside the right edge
    HT_EQ(false, inRoundRect(4, 9, 5, 5, 20, 10, 4));    // just outside the left edge
}

HOST_TEST(more_round_rect_radius_is_limited_to_half_the_smaller_side) {
    // r 50 on a 10x6 rect behaves as r 3: the middle row is fully inside
    HT_EQ(true, inRoundRect(0, 3, 0, 0, 10, 6, 50));
    HT_EQ(true, inRoundRect(9, 3, 0, 0, 10, 6, 50));
    HT_EQ(false, inRoundRect(0, 0, 0, 0, 10, 6, 50));
}

HOST_TEST(more_round_rect_with_zero_size_is_empty) {
    HT_EQ(false, inRoundRect(0, 0, 0, 0, 0, 10, 2));
    HT_EQ(false, inRoundRect(0, 0, 0, 0, 10, 0, 2));
}
