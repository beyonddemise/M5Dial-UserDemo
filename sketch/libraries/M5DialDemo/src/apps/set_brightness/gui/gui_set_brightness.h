/**
 * @file gui_set_brightness.h
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



/** @brief GUI of Set_Brightness: the brightness value, large, in a white bubble. */
class GUI_SetBrightness : public GUI_Base
{
    private:

    public:
        /** @brief Draws nothing (as upstream): the app renders its first page in onCreate() */
        void init() override;
        /** @brief Draw `brightness` (0..255) and push it */
        void renderPage(uint8_t brightness);
        
};
