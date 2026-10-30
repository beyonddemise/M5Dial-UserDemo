/**
 * @file app_rfid_test.h
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2023-08-04
 *
 * @copyright Copyright (c) 2023
 *
 * M5DialDemo port: HAL + esp-idf-rc522 events -> AppContext + polled M5Dial.Rfid (MFRC522 API)
 * (docs/PORTING_NOTES.md, "RFID test").
 */
#pragma once
#include <stdint.h>

#include "../../app_context.h"
#include "../../mooncake/app.h"
#include "gui/gui_rfid_test.h"


namespace MOONCAKE
{
    namespace USER_APP
    {
        namespace APP_RFID_TEST
        {
            /** @brief RFID test runtime state */
            struct Data_t
            {
                AppContext* ctx = nullptr;  ///< host context (canvas)
                bool reader = false;        ///< m5dd::rfidPresent() at open: M5Dial.Rfid usable
                uint32_t next_poll_ms = 0;  ///< millis() of the next card poll
            };
        }

        /**
         * @brief Reads RFID cards (ISO 14443-3 type A) with the M5Dial's WS1850S reader and shows
         * the UID of each card presented; a button click quits.
         *
         * The reader is initialised by m5dd::begin(); if it is missing the app shows and logs
         * that and never touches M5Dial.Rfid. Each card is halted after the read, so it is
         * reported once per presentation (remove it and present it again for a new line).
         *
         * Log lines: `app: open RFID_Test`, `rfid: ready` or `rfid: missing`,
         * `rfid: uid=<HEX:HEX:..>` per card, `app: close RFID_Test`.
         * User data: AppContext* (set with setUserData() before onSetup()).
         */
        class RFID_Test : public APP_BASE
        {
            private:
                /** @brief Poll for a new card; on one: draw, beep, log its UID, halt it */
                void _poll_card();

            public:
                APP_RFID_TEST::Data_t _data;
                GUI_RFID_Test _gui;


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
                /** @brief Log open; log `rfid: ready`, or `rfid: missing` and show it */
                void onCreate() override;
                /** @brief One frame: button click -> quit; if the reader is present, poll for a card */
                void onRunning() override;
                /** @brief Log `app: close RFID_Test` */
                void onDestroy() override;
        };

    }
}
