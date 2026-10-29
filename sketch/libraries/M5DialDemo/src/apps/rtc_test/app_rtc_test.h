/**
 * @file app_rtc_test.h
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2023-08-03
 *
 * @copyright Copyright (c) 2023
 *
 * M5DialDemo port: HAL -> AppContext + M5Dial API (docs/PORTING_NOTES.md, "RTC test").
 */
#pragma once
#include <stdint.h>

#include "../../app_context.h"
#include "../../mooncake/app.h"
#include "gui/gui_rtc_test.h"

namespace MOONCAKE
{
    namespace USER_APP
    {
        namespace RTC_TEST
        {
            /** @brief RTC test runtime state */
            struct Data_t
            {
                AppContext* ctx = nullptr;  ///< host context (canvas)
                uint32_t time_count = 0;    ///< millis() of the last refresh (0 = refresh at once)
            };
        } // namespace RTC_TEST

        /**
         * @brief Shows the PCF8563 RTC time, refreshed once per second; a button click quits.
         *
         * Log lines: `app: open RTC_Test`, `rtc: YYYY-MM-DD HH:MM:SS` (1 Hz),
         * `rtc: read failed` (warning), `app: close RTC_Test`.
         * User data: AppContext* (set with setUserData() before onSetup()).
         */
        class RTC_Test : public APP_BASE
        {
        private:

        public:
            RTC_TEST::Data_t _data;
            GUI_RTC_Test _gui;

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
            /** @brief Log `app: open RTC_Test` */
            void onCreate() override;
            /** @brief One frame: button click -> quit; once per second read, log and draw the time */
            void onRunning() override;
            /** @brief Log `app: close RTC_Test` */
            void onDestroy() override;
        };

    } // namespace USER_APP
} // namespace MOONCAKE
