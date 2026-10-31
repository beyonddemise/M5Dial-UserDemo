/**
 * @file gui_rfid_test.cpp
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2023-08-03
 *
 * @copyright Copyright (c) 2023
 *
 * M5DialDemo port: the unused SDL `delay` macro dropped; the scanned page shows the UID text
 * (`UID` + hex bytes) instead of `SN: <decimal>`; renderMissing() added. Layout and colours as
 * the original.
 */
#include "gui_rfid_test.h"

#include "../../../config.h"


namespace {
constexpr int BUBBLE_X = 120;  ///< original info bubble: centred, 240x140, radius 36
constexpr int BUBBLE_Y = 120;
constexpr int BUBBLE_W = 240;
constexpr int BUBBLE_H = 140;
constexpr int BUBBLE_R = 36;
}  // namespace


void GUI_RFID_Test::_draw_bubble_page()
{
    _canvas->fillScreen(_theme_color);

    /* Icon */
    _draw_top_icon();

    /* Info */
    _canvas->fillSmoothRoundRect(BUBBLE_X - BUBBLE_W / 2, BUBBLE_Y - BUBBLE_H / 2, BUBBLE_W,
                                 BUBBLE_H, BUBBLE_R, TFT_WHITE);
    _canvas->setTextColor(_theme_color);
}


void GUI_RFID_Test::init()
{
    _draw_bubble_page();

    _canvas->drawCenterString("RFID TEST", BUBBLE_X, BUBBLE_Y - 55);
    _canvas->drawCenterString("Place Your RFID", BUBBLE_X, BUBBLE_Y - 12);
    _canvas->drawCenterString("Card Above Screen", BUBBLE_X, BUBBLE_Y + 14);
    _canvas->drawCenterString("To Read", BUBBLE_X, BUBBLE_Y + 14 + 26);

    /* Quit button */
    _draw_quit_button();

    _canvas->pushSprite(0, 0);
}


void GUI_RFID_Test::renderPage(const std::string& uid)
{
    _canvas->fillScreen(TFT_DARKGRAY);
    _canvas->fillSmoothCircle(120, 120, 120, _theme_color);

    /* Icon */
    _draw_top_icon();

    /* Info */
    _canvas->fillSmoothRoundRect(BUBBLE_X - BUBBLE_W / 2, BUBBLE_Y - BUBBLE_H / 2, BUBBLE_W,
                                 BUBBLE_H, BUBBLE_R, TFT_WHITE);
    _canvas->setTextColor(_theme_color);
    _canvas->drawCenterString("TAG SCANNED", BUBBLE_X, BUBBLE_Y - 55);
    _canvas->drawCenterString("UID", BUBBLE_X, BUBBLE_Y - 20);

    // why the small font: a 7-byte UID is 20 characters, 240 px in efontCN_24 (12 px per ASCII
    // glyph) - wider than the round panel at that height; efontCN_16_b (8 px) fits up to 10 bytes
    _canvas->setFont(GUI_FONT_CN_SMALL);
    _canvas->drawCenterString(uid.c_str(), BUBBLE_X, BUBBLE_Y + 10);
    _canvas->setFont(GUI_FONT_CN_BIG);

    /* Quit button */
    _draw_quit_button();

    _canvas->drawCircle(120, 120, 120, TFT_DARKGRAY);
    _canvas->pushSprite(0, 0);
}


void GUI_RFID_Test::renderMissing()
{
    _draw_bubble_page();

    _canvas->drawCenterString("RFID TEST", BUBBLE_X, BUBBLE_Y - 55);
    _canvas->drawCenterString("Reader", BUBBLE_X, BUBBLE_Y - 12);
    _canvas->drawCenterString("Not Found", BUBBLE_X, BUBBLE_Y + 14);

    /* Quit button */
    _draw_quit_button();

    _canvas->pushSprite(0, 0);
}
