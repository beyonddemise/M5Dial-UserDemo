/**
 * @file gui_lcd_test.h
 * @author Forairaaaaa
 * @brief 
 * @version 0.1
 * @date 2023-07-26
 * 
 * @copyright Copyright (c) 2023
 * 
 * M5DialDemo port: GUI_Base include path, page stepping in lcd_test_pages.h (host-tested),
 * no printf (the app logs `lcd: pattern <n>`); drawing unchanged.
 */
#pragma once
#include <array>

#include "../../../mooncake/gui_base.h"
#include "../lcd_test_pages.h"



/**
 * @brief GUI of the LCD test: page 0 = instructions, 1..5 = colour fills, 6 = geometry/touch
 * test page; touches draw dots in a per-page colour.
 */
class GUI_LCD_TEST : public GUI_Base
{
    private:
        /* 0~6 */
        int _current_page_num;
        std::array<int, lcd_test_pages::PAGE_COUNT> _bg_color_list = {TFT_BLACK, TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE, TFT_BLACK, TFT_BLACK};
        std::array<int, lcd_test_pages::PAGE_COUNT> _tp_color_list = {TFT_BROWN, TFT_WHITE, TFT_BLUE, TFT_WHITE, TFT_BLUE, TFT_YELLOW, TFT_BLUE};
           

    public:
        GUI_LCD_TEST() :
            _current_page_num(0)
            {}

        /** @brief Draw page 0 (instructions) and push it */
        void init() override;

        /** @brief Draw page `currentPageNum` (0..6) and push it */
        void renderPage(const int& currentPageNum);
        /** @brief Move `detents` pages (positive = next), wrapping, and draw the new page */
        void goPages(int detents);
        /** @brief Draw a touch dot at (tpX, tpY); on the test page also print the coordinates */
        void renderTp(const int& tpX, const int& tpY);
        /** @brief Page currently shown (0..6) */
        int currentPage() const { return _current_page_num; }

};
