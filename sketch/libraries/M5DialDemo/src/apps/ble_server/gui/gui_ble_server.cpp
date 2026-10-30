/**
 * @file gui_ble_server.cpp
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2023-08-07
 *
 * @copyright Copyright (c) 2023
 *
 * M5DialDemo port: the unused SDL `delay` macro and commented-out code dropped; bubble
 * geometry as named constants. Layout and colours as the original.
 */
#include "gui_ble_server.h"

#include "../../../config.h"


namespace {
constexpr int BUBBLE_X = 120;  ///< original info bubble: centred, 240x140, radius 36
constexpr int BUBBLE_Y = 120;
constexpr int BUBBLE_W = 240;
constexpr int BUBBLE_H = 140;
constexpr int BUBBLE_R = 36;
constexpr int LABEL_X = 30;    ///< left edge of the `[Name]` / `[UUID]` / `[Info]` lines
}  // namespace


void GUI_BLE_Server::init()
{
    _canvas->fillScreen(TFT_DARKGRAY);
    _canvas->fillSmoothCircle(120, 120, 120, _theme_color);

    /* Icon */
    _draw_top_icon();

    /* Quit button */
    _draw_quit_button();

    _canvas->drawCircle(120, 120, 120, TFT_DARKGRAY);
    _canvas->pushSprite(0, 0);
}


void GUI_BLE_Server::renderPage(BLEStatus_t status, const std::string& name,
                                const std::string& info)
{
    _canvas->fillScreen(TFT_DARKGRAY);
    _canvas->fillSmoothCircle(120, 120, 120, _theme_color);

    /* Info */
    _canvas->fillSmoothRoundRect(BUBBLE_X - BUBBLE_W / 2, BUBBLE_Y - BUBBLE_H / 2, BUBBLE_W,
                                 BUBBLE_H, BUBBLE_R, TFT_WHITE);

    _canvas->setTextColor(_theme_color);
    _canvas->drawCenterString("BLE SERVER", BUBBLE_X, BUBBLE_Y - 65);

    _canvas->setFont(GUI_FONT_CN_SMALL);

    _canvas->setCursor(LABEL_X, BUBBLE_Y - 26);
    _canvas->printf("[Name]:    %s", name.c_str());

    _canvas->setCursor(LABEL_X, BUBBLE_Y - 6);
    _canvas->printf("[UUID]:    0x180D");

    if (status == disconnet)
    {
        _canvas->setCursor(LABEL_X, BUBBLE_Y + 14);

        _canvas->setTextColor(_theme_color);
        _canvas->printf("[Info]:");

        _canvas->setTextColor(TFT_ORANGE, TFT_WHITE);
        _canvas->printf("    disconnect");
    }
    else if (status == connected)
    {
        _canvas->setCursor(LABEL_X, BUBBLE_Y + 14);
        _canvas->printf("[Info]:");

        _canvas->setTextColor(TFT_DARKGREEN);
        _canvas->printf("    connected");

        _canvas->setTextColor(TFT_BLUE, TFT_WHITE);
        _canvas->setCursor(LABEL_X, BUBBLE_Y + 34);
        _canvas->printf("           %s", info.c_str());
    }

    _canvas->setFont(GUI_FONT_CN_BIG);

    /* Icon */
    _draw_top_icon();

    /* Quit button */
    _draw_quit_button();

    _canvas->drawCircle(120, 120, 120, TFT_DARKGRAY);
    _canvas->pushSprite(0, 0);
}
