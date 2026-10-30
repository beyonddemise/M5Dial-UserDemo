/**
 * @file app_lcd_test.h
 * @author Forairaaaaa
 * @brief 
 * @version 0.1
 * @date 2023-07-27
 * 
 * @copyright Copyright (c) 2023
 * 
 * M5DialDemo port: HAL -> AppContext + M5Dial API (docs/PORTING_NOTES.md, "LCD test").
 */
#pragma once
#include "../../app_context.h"
#include "../../encoder_detent.h"
#include "../../mooncake/app.h"
#include "gui/gui_lcd_test.h"


namespace MOONCAKE
{
    namespace USER_APP
    {
        namespace LCD_TEST
        {
            /** @brief LCD test runtime state */
            struct Data_t
            {
                AppContext* ctx = nullptr;  ///< host context (canvas)
            };
        }

        /**
         * @brief Display test: the knob steps through colour pages (clockwise = next), touches
         * draw dots, a button click quits.
         *
         * Log lines: `app: open LCD_Test`, `lcd: pattern <n>` (0..6, on open and every page
         * change), `app: close LCD_Test`.
         * User data: AppContext* (set with setUserData() before onSetup()).
         */
        class LCD_Test : public APP_BASE
        {
            private:
                LCD_TEST::Data_t _data;
                GUI_LCD_TEST _gui;
                EncoderDetent _detent;  ///< encoder raw count -> detent steps (cw positive)
                

            public:
                
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
                /** @brief Log open + page 0, wait until the opening touch is released */
                void onCreate() override;
                /** @brief One frame: knob -> page, button click -> quit, touch -> dot */
                void onRunning() override;
                /** @brief Log `app: close LCD_Test` */
                void onDestroy() override;
        };

    }
}
