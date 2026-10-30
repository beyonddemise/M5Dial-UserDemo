/**
 * @file app_ble_server.h
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2023-08-07
 *
 * @copyright Copyright (c) 2023
 *
 * M5DialDemo port: HAL + C ble_demo_start/stop -> AppContext + BleHeartRateServer on
 * NimBLE-Arduino, polled in onRunning() (docs/PORTING_NOTES.md, "BLE server").
 */
#pragma once
#include <stdint.h>

#include "../../app_context.h"
#include "../../mooncake/app.h"
#include "ble_heart_rate.h"
#include "gui/gui_ble_server.h"


namespace MOONCAKE
{
    namespace USER_APP
    {
        namespace BLE_SERVER
        {
            /** @brief BLE_Server runtime state */
            struct Data_t
            {
                AppContext* ctx = nullptr;  ///< host context (canvas)

                uint32_t ble_page_update_interval = 1000;  ///< page redraw period, ms
                uint32_t ble_page_update_time_count = 0;   ///< millis() of the last redraw
            };
        }

        /**
         * @brief Simulated BLE heart-rate sensor: advertises as `M5-<hex>` (from the MAC),
         * serves Heart Rate 0x180D and Device Information 0x180A, and notifies a counting
         * heart rate every second to a subscribed client. The page shows name, UUID and
         * connection state, redrawn once a second; a button click quits.
         *
         * BLE is started on open and fully released on close (NimBLEDevice::deinit(true)),
         * so the heap is returned for the next app.
         *
         * Log lines: `app: open BLE_Server`, `ble: advertising <name>`, `ble: connected=0|1`,
         * `ble: subscribed=0|1`, `ble: heap=<free internal bytes>` after BLE is off,
         * `app: close BLE_Server`.
         * User data: AppContext* (set with setUserData() before onSetup()).
         */
        class BLE_Server : public APP_BASE
        {
            private:
                BleHeartRateServer _ble;  ///< the GATT server

            public:
                BLE_SERVER::Data_t _data;
                GUI_BLE_Server _gui;


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
                /** @brief Log open, start BLE under the MAC-derived name */
                void onCreate() override;
                /** @brief One frame: BLE poll, page redraw once a second, button click -> quit */
                void onRunning() override;
                /** @brief BLE off (end()), log `ble: heap=` and `app: close BLE_Server` */
                void onDestroy() override;
        };

    }
}
