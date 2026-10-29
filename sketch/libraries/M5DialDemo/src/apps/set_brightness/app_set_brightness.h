/**
 * @file app_set_brightness.h
 * @author Forairaaaaa
 * @brief 
 * @version 0.1
 * @date 2023-08-04
 * 
 * @copyright Copyright (c) 2023
 * 
 * M5DialDemo port: HAL -> AppContext + M5Dial API (docs/PORTING_NOTES.md, "Brightness").
 */
#pragma once
#include <stdint.h>

#include "../../app_context.h"
#include "../../encoder_detent.h"
#include "../../mooncake/app.h"
#include "../../smooth_menu/lv_anim/lv_anim.h"
#include "gui/gui_set_brightness.h"


namespace MOONCAKE
{
    namespace USER_APP
    {
        namespace SET_BRIGHTNESS
        {
            /** @brief Set_Brightness runtime state */
            struct Data_t
            {
                AppContext* ctx = nullptr;  ///< host context (canvas)
                
                int16_t brightness = 128;              ///< target brightness 0..255
                int16_t applied = -1;                  ///< last value written to the backlight
                uint32_t scroll_speed_time_count = 0;  ///< millis() of the last detent

                LVGL::Anim_Path brightness_anim;  ///< eases the backlight to the target
            };
        }

        /**
         * @brief Backlight setting: the knob changes the brightness (clockwise = brighter,
         * faster turns take bigger steps, 0..255), the backlight eases to it in 600 ms; a button
         * click quits and keeps the value.
         *
         * Log lines: `app: open Set_Brightness`, `brightness: <n>` (on open and every change),
         * `app: close Set_Brightness`.
         * User data: AppContext* (set with setUserData() before onSetup()).
         */
        class Set_Brightness : public APP_BASE
        {
            private:
                EncoderDetent _detent;  ///< encoder raw count -> detent steps (cw positive)

            public:
                SET_BRIGHTNESS::Data_t _data;
                GUI_SetBrightness _gui;


                /**
                 * @brief Get gui pointer for basic settings 
                 * 
                 * @return GUI_Base* 
                 */
                GUI_Base* getGui() override { return &_gui; }


                /**
                 * @brief Lifecycle callbacks for derived to override
                 * 
                 */
                /** @brief Setup App configs: name, reads the AppContext from user data */
                void onSetup() override;

                /* Life cycle */
                /** @brief Read the current brightness, log and draw it */
                void onCreate() override;
                /** @brief One frame: knob -> new target, ease the backlight, button click -> quit */
                void onRunning() override;
                /** @brief Log `app: close Set_Brightness` */
                void onDestroy() override;
        };

    }
}
