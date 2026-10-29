/**
 * @file gui_temp_demo.cpp
 * @author Forairaaaaa
 * @brief 
 * @version 0.1
 * @date 2023-08-10
 * 
 * @copyright Copyright (c) 2023
 * 
 * M5DialDemo port: GUI_VideoShit -> GUI_TempDemo, image tables from the library assets, the
 * debug printf and unused SDL `delay` macro dropped; drawing unchanged.
 */
#include "gui_temp_demo.h"

#include <array>

#include "../../../assets/temp_demo_icons/temp_demo_button_pressed.h"
#include "../../../assets/temp_demo_icons/temp_demo_button_released.h"
#include "../../../assets/temp_demo_icons/temp_demo_1.h"
#include "../../../assets/temp_demo_icons/temp_demo_2.h"
#include "../../../assets/temp_demo_icons/temp_demo_3.h"
#include "../../../assets/temp_demo_icons/temp_demo_4.h"
#include "../../../assets/temp_demo_icons/temp_demo_5.h"
#include "../../../assets/temp_demo_icons/temp_demo_6.h"
#include "../../../assets/temp_demo_icons/temp_demo_7.h"
#include "../../../assets/temp_demo_icons/temp_demo_8.h"
#include "../../../assets/temp_demo_icons/temp_demo_9.h"


namespace {

constexpr uint32_t THEME_COLOR = 0x008CD6;  ///< the images' blue background

const std::array<const uint16_t*, temp_demo_target::IMAGE_COUNT> _pic_temp_list =
{
    image_data_temp_demo_1,
    image_data_temp_demo_2,
    image_data_temp_demo_3,
    image_data_temp_demo_4,
    image_data_temp_demo_5,
    image_data_temp_demo_6,
    image_data_temp_demo_7,
    image_data_temp_demo_8,
    image_data_temp_demo_9
};
const std::array<const uint16_t*, 2> _pic_button_list =
{
    image_data_temp_demo_button_released,
    image_data_temp_demo_button_pressed
};

}  // namespace


void GUI_TempDemo::init()
{
    _theme_color = THEME_COLOR;

    _canvas->fillScreen(_theme_color);
    

    // why no setSwapBytes: byte-swapped RGB565 as the launcher icons (see launcher.cpp)
    _canvas->pushImage(0, 50 - 10, 240, 110, _pic_temp_list[_temp]);

    // why [0] while pressed: kept from upstream (the two images differ only slightly)
    if (_button_state)
        _canvas->pushImage(0, 160, 240, 82, _pic_button_list[0]);
    else
        _canvas->pushImage(0, 160, 240, 82, _pic_button_list[1]);

    /* Cover shit */
    _canvas->fillRect(0, 120, 10, 40, _theme_color);
    _canvas->fillRect(240 - 10, 120, 10, 40, _theme_color);



    _canvas->pushSprite(0, 0);
}


void GUI_TempDemo::renderPage()
{
    init();
}


void GUI_TempDemo::stepTarget(int detents)
{
    _temp = temp_demo_target::step(_temp, detents);
    renderPage();
}


void GUI_TempDemo::buttonPressed()
{
    _button_state = true;
    renderPage();
}


void GUI_TempDemo::buttonReleased()
{
    _button_state = false;
    renderPage();
}
