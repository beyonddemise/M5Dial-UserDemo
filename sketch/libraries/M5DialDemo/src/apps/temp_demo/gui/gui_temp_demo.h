/**
 * @file gui_temp_demo.h
 * @author Forairaaaaa
 * @brief 
 * @version 0.1
 * @date 2023-08-10
 * 
 * @copyright Copyright (c) 2023
 * 
 * M5DialDemo port: GUI_VideoShit -> GUI_TempDemo, the images come from the library's
 * assets/temp_demo_icons (their tables moved to the .cpp, so they exist once), tempUp/tempDown
 * folded into stepTarget() (temp_demo_target.h, host-tested); drawing unchanged.
 */
#pragma once
#include "../../../mooncake/gui_base.h"
#include "../temp_demo_target.h"



/**
 * @brief GUI of the thermostat demo: the set-point as a large image (19..27 C) above a "SET"
 * button image that changes while the screen is touched.
 */
class GUI_TempDemo : public GUI_Base
{
    private:
        int _temp;
        bool _button_state;
        

    public:
        GUI_TempDemo() :
            _temp(temp_demo_target::DEFAULT_INDEX),
            _button_state(false)
            {}

        /** @brief Draw the whole page (theme 0x008CD6, set-point, button) and push it */
        void init() override;

        /** @brief Redraw the whole page */
        void renderPage();
        /** @brief Move the set-point by `detents` (positive = warmer, clamped) and redraw */
        void stepTarget(int detents);
        /** @brief Show the button as pressed and redraw */
        void buttonPressed();
        /** @brief Show the button as released and redraw */
        void buttonReleased();
        /** @brief Current set-point image index (temp_demo_target::celsius() gives degrees) */
        int target() const { return _temp; }

};
