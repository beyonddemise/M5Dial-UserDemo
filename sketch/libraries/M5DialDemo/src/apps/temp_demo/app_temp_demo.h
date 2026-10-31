/**
 * @file app_temp_demo.h
 * @author Forairaaaaa
 * @brief 
 * @version 0.1
 * @date 2023-08-10
 * 
 * @copyright Copyright (c) 2023
 * 
 * M5DialDemo port: VideoShit -> TempDemo (VIDEO_SHIT -> TEMP_DEMO), HAL -> AppContext + M5Dial
 * API (docs/PORTING_NOTES.md, "Temp demo").
 */
#pragma once
#include "../../app_context.h"
#include "../../encoder_detent.h"
#include "../../mooncake/app.h"
#include "gui/gui_temp_demo.h"


namespace MOONCAKE
{
    namespace USER_APP
    {

        namespace TEMP_DEMO
        {
            /** @brief TempDemo runtime state */
            struct Data_t
            {
                AppContext* ctx = nullptr;  ///< host context (canvas)
            };
        }

        /**
         * @brief Thermostat UI demo (upstream class `VideoShit`): the knob sets the target
         * temperature (clockwise = warmer, 19..27 C), touching the screen presses the "SET"
         * button, a button click quits.
         *
         * Log lines: `app: open TempDemo`, `temp: target=<degrees C>` (on open and every
         * change), `temp: frame` (every redraw; only on input, so a few per second at most),
         * `app: close TempDemo`.
         * User data: AppContext* (set with setUserData() before onSetup()).
         */
        class TempDemo : public APP_BASE
        {
            private:
                EncoderDetent _detent;  ///< encoder raw count -> detent steps (cw positive)
                /** @brief Log one `temp: frame` (call after each GUI redraw) */
                void _log_frame();

            public:
                TempDemo() = default;
                ~TempDemo() = default;

                TEMP_DEMO::Data_t _data;
                GUI_TempDemo _gui;


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
                /** @brief Log open and the start target, draw the first frame */
                void onCreate() override;
                /** @brief One frame: knob -> target, button click -> quit, touch -> SET button */
                void onRunning() override;
                /** @brief Log `app: close TempDemo` */
                void onDestroy() override;
        };

    }
}
