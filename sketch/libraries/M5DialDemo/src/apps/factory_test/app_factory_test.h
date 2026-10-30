/**
 * @file app_factory_test.h
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2023-08-11
 *
 * @copyright Copyright (c) 2023
 *
 * M5DialDemo port: HAL -> AppContext + M5Dial API; operator verdicts (PASS/FAIL/SKIP) after
 * every step, a summary screen and serial summary line; the Wi-Fi and BLE steps (commented
 * out upstream) not ported (docs/PORTING_NOTES.md, "Factory test").
 */
#pragma once

#include "../../app_context.h"
#include "../../mooncake/app.h"
#include "report.h"


namespace MOONCAKE
{
    namespace USER_APP
    {
        namespace FACTORY_TEST
        {
            /** @brief FactoryTest runtime state */
            struct Data_t
            {
                AppContext* ctx = nullptr;      ///< host context (canvas)
                factory_test::FactoryReport report;  ///< verdict of every step
            };
        }

        /**
         * @brief The factory test: LCD -> RTC -> RFID -> IO, each followed by the operator's
         * PASS/FAIL/SKIP verdict, then a summary; a click there arms the RTC alarm (4 s) and
         * powers off, so on battery the Dial switches itself back on (RTC wake-up test).
         *
         * onCreate() runs the whole sequence and does not return (the power goes off).
         * UserDemo starts it when the button is held for more than 1 s at boot.
         *
         * Log lines: `app: open FactoryTest`, `factory: step <LCD|RTC|RFID|IO>`, the step apps'
         * lines, `factory: rfid missing`, `factory: io adc=.. leds=.. i2c=[..]`,
         * `factory: prompt <step>`, `factory: verdict <step>=<v>`,
         * `factory: LCD=.. RTC=.. RFID=.. IO=..` (summary), `factory: rtc wakeup in 4 s`.
         * User data: AppContext* (set with setUserData() before onSetup()).
         */
        class FactoryTest : public APP_BASE
        {
            private:
                /** @brief Show the summary of every verdict and log FactoryReport::serial() */
                void _show_summary();

                /** @brief Wait for a click, then RTC alarm in 4 s and power off (never returns) */
                [[noreturn]] void _rtc_wakeup_test();

            public:
                FACTORY_TEST::Data_t _data;


                /** @brief Setup App configs: name, reads the AppContext from user data */
                void onSetup() override;

                /* Life cycle */
                /** @brief Run every step with its verdict, the summary and the power-off */
                void onCreate() override;
                /** @brief Never reached: onCreate() ends with the power-off */
                void onRunning() override {}
                /** @brief Never reached: onCreate() ends with the power-off */
                void onDestroy() override {}
        };

    }
}
