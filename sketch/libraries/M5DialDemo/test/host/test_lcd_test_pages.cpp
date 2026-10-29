/**
 * @file test_lcd_test_pages.cpp
 * @brief lcd_test_pages: page stepping of the LCD test (original GUI_LCD_TEST::goNextPage /
 *        goLastPage: pages 0..6, wrapping both ways; page 6 is the touch test page).
 */
#include "apps/lcd_test/lcd_test_pages.h"
#include "host_test.h"

using namespace lcd_test_pages;

HOST_TEST(lcd_has_seven_pages) {
    HT_EQ(7, PAGE_COUNT);
    HT_EQ(6, TEST_PAGE);
}

HOST_TEST(lcd_next_advances_and_wraps_to_0) {
    HT_EQ(1, next(0));
    HT_EQ(6, next(5));
    HT_EQ(0, next(6));
}

HOST_TEST(lcd_last_goes_back_and_wraps_to_6) {
    HT_EQ(4, last(5));
    HT_EQ(0, last(1));
    HT_EQ(6, last(0));
}

HOST_TEST(lcd_step_applies_signed_detents) {
    HT_EQ(0, step(0, 0));
    HT_EQ(3, step(0, 3));    // three clockwise detents
    HT_EQ(1, step(6, 2));    // wraps forwards
    HT_EQ(5, step(0, -2));   // wraps backwards
    HT_EQ(0, step(0, 14));   // whole turns
    HT_EQ(0, step(0, -14));
}
