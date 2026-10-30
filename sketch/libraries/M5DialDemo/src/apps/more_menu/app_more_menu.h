/**
 * @file app_more_menu.h
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2023-08-07
 *
 * @copyright Copyright (c) 2023
 *
 * M5DialDemo port: HAL -> AppContext + M5Dial API, the three LVGL entries (and lv_demos.h)
 * dropped (LVGL lives in the LvglDemo sketch, D3), entries and drawing maths in
 * more_menu_items.h (host-tested); docs/PORTING_NOTES.md, "More menu / power-off".
 */
#pragma once
#include <stdint.h>

#include "../../app_context.h"
#include "../../encoder_detent.h"
#include "../../mooncake/app.h"
#include "../../smooth_menu/simple_menu/simple_menu.h"

struct MoreMenuRender_CB_t;


namespace MOONCAKE
{
    namespace USER_APP
    {
        namespace MORE_MENU
        {
            /** @brief MoreMenu runtime state */
            struct Data_t
            {
                AppContext* ctx = nullptr;                    ///< host context (canvas)
                SMOOTH_MENU::Simple_Menu* menu = nullptr;      ///< vertical list + selector anim (owned)
                MoreMenuRender_CB_t* menu_render_cb = nullptr; ///< draws each menu frame (owned)
            };
        }

        /**
         * @brief The launcher's "MORE" slot: a curved vertical list navigated with the knob
         * (clockwise = next entry), chosen with a button click. "Quit" returns to the
         * launcher, "Power Off" switches the M5Dial off; the remaining rows are upstream's
         * filler entries and do nothing.
         *
         * Log lines: `app: open MoreMenu`, `more: select <index> <tag>` per detent,
         * `more: selected <tag>` on a click, `power: off` (then the power goes),
         * `app: close MoreMenu`.
         * User data: AppContext* (set with setUserData() before onSetup()).
         */
        class MoreMenu : public APP_BASE
        {
            private:
                EncoderDetent _detent;  ///< encoder raw count -> detent steps (cw positive)
                void _create_menu();
                void _menu_loop();
                void _log_select();


                /* Selected callback */
                void _item_selected_callback(uint8_t selectedNum);
                /** @brief Black screen, RTC alarm IRQ off, log `power: off`, power off */
                void _power_off();


            public:
                MORE_MENU::Data_t _data;

                ~MoreMenu();


                /**
                 * @brief Lifecycle callbacks for derived to override
                 *
                 */
                /** @brief Setup App configs: name, reads the AppContext from user data */
                void onSetup() override;

                /* Life cycle */
                /** @brief Log open, build the menu (selector on "Quit") */
                void onCreate() override;
                /** @brief One frame: draw, knob -> move the selector, click -> run the entry.
                 *  The host calls M5Dial.update() before each call. */
                void onRunning() override;
                /** @brief Free the menu, log `app: close MoreMenu` */
                void onDestroy() override;
        };
    }
}
