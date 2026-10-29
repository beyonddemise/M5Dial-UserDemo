/**
 * @file gui_rtc_test.h
 * @author Forairaaaaa
 * @brief 
 * @version 0.1
 * @date 2023-08-03
 * 
 * @copyright Copyright (c) 2023
 * 
 * M5DialDemo port: GUI_Base include path, doc comments; API unchanged.
 */
#pragma once
#include <stdint.h>

#include "../../../mooncake/gui_base.h"



/** @brief GUI of the RTC test: the current time, large, in a white bubble. */
class GUI_RTC_Test : public GUI_Base
{
    private:

    public:
        /** @brief Draw the empty page (icon + quit mark) and push it */
        void init() override;
        /** @brief Draw "HH:MM:SS" and push it */
        void renderPage(const uint8_t& hour, const uint8_t& minute, const uint8_t& second);
        
};
