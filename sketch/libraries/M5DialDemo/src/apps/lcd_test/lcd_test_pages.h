/**
 * @file lcd_test_pages.h
 * @brief Arduino-free page stepping of the LCD test app.
 *
 * Extracted from the original GUI_LCD_TEST::goNextPage / goLastPage (Forairaaaaa, M5Stack
 * M5Dial-UserDemo, MIT) so it can be unit-tested on the host
 * (test/host/test_lcd_test_pages.cpp).
 */
#pragma once

/** Pages of the LCD test: 0 = info, 1..5 = colour fills, 6 = touch/geometry test page. */
namespace lcd_test_pages {

constexpr int PAGE_COUNT = 7;  ///< original _max_page_num = 6, pages 0..6
constexpr int TEST_PAGE = 6;   ///< the page with the circles/cross and touch coordinates

/** @return page after `page`, wrapping 6 -> 0 (original goNextPage). */
inline int next(int page) { return page + 1 >= PAGE_COUNT ? 0 : page + 1; }

/** @return page before `page`, wrapping 0 -> 6 (original goLastPage). */
inline int last(int page) { return page - 1 < 0 ? PAGE_COUNT - 1 : page - 1; }

/**
 * Moves `page` by `detents` (positive = clockwise = next), wrapping both ways.
 * why: several detents can arrive between two frames; each one must change the page.
 */
inline int step(int page, int detents) {
    int p = (page + detents) % PAGE_COUNT;
    return p < 0 ? p + PAGE_COUNT : p;
}

}  // namespace lcd_test_pages
