/**
 * @file gui_base.h
 * @author Forairaaaaa
 * @brief 
 * @version 0.1
 * @date 2023-07-26
 * 
 * @copyright Copyright (c) 2023
 * 
 * Ported to Arduino for M5DialDemo from main/apps/utilities/gui_base (M5Stack M5Dial-UserDemo,
 * MIT): sprite type -> M5Canvas, <LovyanGFX.hpp> -> <M5GFX.h>; API otherwise unchanged.
 */
#pragma once
#include <stdint.h>
#include <string>
#include <M5GFX.h>


/** @brief Position and size of a GUI object (upstream spelling kept) */
struct BasicObeject_t
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};


/**
 * @brief Base class for app gui
 * 
 */
class GUI_Base
{
    protected:
        M5Canvas* _canvas;
        M5Canvas* _icon;
        uint32_t _theme_color;

        int _canvas_half_width;
        int _canvas_half_height;
    
    public:
        /** @brief Construct without canvas; call setCanvas() or init(canvas, icon) before drawing */
        // why: _icon and the half sizes are left uninitialised upstream; zero them here
        GUI_Base() :
            _canvas(nullptr),
            _icon(nullptr),
            _theme_color(0xFFFFFF),
            _canvas_half_width(0),
            _canvas_half_height(0)
            {}
        virtual ~GUI_Base() = default;

        /* Basic settings */
        /** @brief Set the full-screen canvas and the App icon sprite drawn at the top */
        inline void setCanvas(M5Canvas* canvas, M5Canvas* icon)
        {
            _canvas = canvas;
            _icon = icon;
            _canvas_half_width = _canvas->width() / 2;
            _canvas_half_height = _canvas->height() / 2;
        }
        /** @brief Set the background (theme) colour, RGB888 */
        inline void setThemeColor(const uint32_t& color) { _theme_color = color; }
        
        /* Entry for override */
        /** @brief Draw the App's initial screen and push it to the display */
        virtual void init();
        /** @brief setCanvas() then init() */
        void init(M5Canvas* canvas, M5Canvas* icon)
        {
            setCanvas(canvas, icon);
            init();
        }


    /* Common drawing api */
    protected:
        void _draw_quit_button(const int& buttonColor = TFT_WHITE);
        void _draw_banner(const std::string& label, int x, int y, const int& bannerColor = TFT_WHITE);
        void _draw_top_banner(const std::string& label, const int& bannerColor = TFT_WHITE);
        void _draw_top_icon();
};
