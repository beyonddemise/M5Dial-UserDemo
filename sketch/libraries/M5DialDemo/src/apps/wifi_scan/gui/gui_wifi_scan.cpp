/**
 * @file gui_wifi_scan.cpp
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2023-08-04
 *
 * @copyright Copyright (c) 2023
 *
 * M5DialDemo port: the unused SDL `delay` macro dropped; init() draws via renderScanning();
 * `printf("%s", menu)` -> `print(menu)`.
 * Layout and colours as the original.
 */
#include "gui_wifi_scan.h"

#include "../../../config.h"


namespace {
constexpr int BUBBLE_X = 120;  ///< original info bubble: centred, 240x140, radius 36
constexpr int BUBBLE_Y = 120;
constexpr int BUBBLE_W = 240;
constexpr int BUBBLE_H = 140;
constexpr int BUBBLE_R = 36;
constexpr int TITLE_LINE_W = 90;  ///< underline of "WIFI LIST"
}  // namespace


void GUI_WiFi_Scan::init()
{
    renderScanning();
}


void GUI_WiFi_Scan::renderScanning()
{
    _canvas->fillScreen(TFT_DARKGRAY);
    _canvas->fillSmoothCircle(120, 120, 120, _theme_color);

    /* Icon */
    _draw_top_icon();

    /* Info */
    _canvas->fillSmoothRoundRect(BUBBLE_X - BUBBLE_W / 2, BUBBLE_Y - BUBBLE_H / 2, BUBBLE_W,
                                 BUBBLE_H, BUBBLE_R, TFT_WHITE);

    _canvas->setTextColor(_theme_color);
    _canvas->drawCenterString("WIFI SCAN", BUBBLE_X, BUBBLE_Y - 65);
    _canvas->drawCenterString("Scanning...", BUBBLE_X, BUBBLE_Y - 12);

    /* Quit button */
    _draw_quit_button();

    _canvas->drawCircle(120, 120, 120, TFT_DARKGRAY);
    _canvas->pushSprite(0, 0);
}


void GUI_WiFi_Scan::renderPage(int menuY_offet, const std::string& menu)
{
    _canvas->fillScreen(_theme_color);

    /* Info */
    _canvas->fillSmoothRoundRect(BUBBLE_X - BUBBLE_W / 2, BUBBLE_Y - BUBBLE_H / 2, BUBBLE_W,
                                 BUBBLE_H, BUBBLE_R, TFT_WHITE);

    _canvas->setTextColor(_theme_color);
    _canvas->drawCenterString("WIFI LIST", BUBBLE_X, BUBBLE_Y - 65 + menuY_offet);
    _canvas->drawFastHLine(BUBBLE_X - TITLE_LINE_W / 2, BUBBLE_Y - 34 + menuY_offet,
                           TITLE_LINE_W, _theme_color);

    _canvas->setFont(GUI_FONT_CN_SMALL);
    _canvas->setCursor(0, BUBBLE_Y - 20 + menuY_offet);
    _canvas->print(menu.c_str());
    _canvas->setFont(GUI_FONT_CN_BIG);

    /* Icon */
    _draw_top_icon();

    /* Quit button */
    _draw_quit_button();

    _canvas->pushSprite(0, 0);
}
