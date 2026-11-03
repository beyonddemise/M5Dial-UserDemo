/**
 * @file more_menu_render_callback.hpp
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2023-08-07
 *
 * @copyright Copyright (c) 2023
 *
 * M5DialDemo port: LGFX_Sprite -> M5Canvas; approxSin, row offset and shade in
 * more_menu_items.h (host-tested); the selector's fillSmoothRoundRectInDifference (a method
 * the original added to its vendored LovyanGFX, not in M5GFX) redone as
 * _invert_round_rect(). Included by app_more_menu.cpp only.
 */
#pragma once
#include <M5GFX.h>

#include "../../config.h"
#include "../../smooth_menu/simple_menu/simple_menu.h"
#include "more_menu_items.h"


/* Render setting */
#define MORE_MENU_COLOR_SELECTOR           (uint32_t)0xfa7000
#define MORE_MENU_COLOR_BG                  TFT_BLACK
#define MORE_MENU_SELECTOR_RADIUS           5


/** Draws one More menu frame: "MORE" title, curved rows, the selector inverting its row. */
struct MoreMenuRender_CB_t : public SMOOTH_MENU::SimpleMenuCallback_t
{
    private:
        M5Canvas* _canvas = nullptr;

        /**
         * Inside the rounded rect: black pixels become `color`, all others black, so the row
         * under the selector reads as black text on orange.
         * why per pixel: the original's fillSmoothRoundRectInDifference did the same on its
         * own LovyanGFX fork (black -> colour, else black); M5GFX has no such call. The rect is
         * one row (~100 x 24 px) and the canvas is RAM, so readPixel per pixel is cheap.
         * The upstream anti-aliased corner edge is not reproduced (exact mask instead).
         */
        void _invert_round_rect(int x, int y, int w, int h, int r, uint32_t color)
        {
            uint16_t fill = _canvas->color565((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
            for (int py = y; py < y + h; py++)
            {
                for (int px = x; px < x + w; px++)
                {
                    if (!more_menu_items::inRoundRect(px, py, x, y, w, h, r))
                        continue;
                    bool black = _canvas->readPixel(px, py) == 0;
                    _canvas->drawPixel(px, py, black ? fill : (uint16_t)0);
                }
            }
        }


    public:
        /** @brief Canvas to draw on and push (AppContext::canvas) */
        inline void setCanvas(M5Canvas* canvasPtr) { _canvas = canvasPtr; }

        /* Override render callback */
        /** @brief Draw and push one frame (called by Simple_Menu::update()) */
        void renderCallback(
            const std::vector<SMOOTH_MENU::Item_t*>& menuItemList,
            const SMOOTH_MENU::RenderAttribute_t& selector,
            const SMOOTH_MENU::RenderAttribute_t& /*camera*/
        ) override
        {
            _canvas->fillScreen(MORE_MENU_COLOR_BG);

            int menu_start_y_offset = 108;
            float row_dy = 0;


            /* Draw menu */
            _canvas->setFont(GUI_FONT_CN_BIG);

            for (size_t i = 0; i < menuItemList.size(); i++)
            {
                /* Draw menu label */
                if (i == 0)
                {
                    _canvas->setTextSize(1);
                    _canvas->setTextColor(MORE_MENU_COLOR_SELECTOR);
                    _canvas->drawCenterString("MORE", 120, menuItemList[0]->y - selector.y + 20);
                }


                /* Calculate item x offset and text color */
                row_dy = menuItemList[i]->y - selector.y;
                int shade = more_menu_items::textShade(row_dy);
                _canvas->setTextColor(lgfx::color565(shade, shade, shade));


                /* Draw tags */
                _canvas->setTextSize(menuItemList[i]->height / 24);
                _canvas->drawString(
                    menuItemList[i]->tag.c_str(),
                    menuItemList[i]->x + more_menu_items::curveOffsetX(row_dy),
                    menuItemList[i]->y - selector.y + menu_start_y_offset
                );
            }


            /* Draw selector in difference stack */
            _invert_round_rect(
                selector.x,
                menu_start_y_offset + (menuItemList[selector.targetItem]->height - selector.height) / 2,
                selector.width,
                selector.height,
                MORE_MENU_SELECTOR_RADIUS,
                MORE_MENU_COLOR_SELECTOR
            );


            _canvas->pushSprite(0, 0);
        }
};
