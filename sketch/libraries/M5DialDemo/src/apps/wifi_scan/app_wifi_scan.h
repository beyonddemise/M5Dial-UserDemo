/**
 * @file app_wifi_scan.h
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2023-08-04
 *
 * @copyright Copyright (c) 2023
 *
 * M5DialDemo port: HAL + blocking ESP-IDF scan -> AppContext + asynchronous WiFi.h scan polled
 * in onRunning(); touch rescans (docs/PORTING_NOTES.md, "Wi-Fi scan").
 */
#pragma once
#include <stdint.h>

#include <string>

#include "../../app_context.h"
#include "../../encoder_detent.h"
#include "../../mooncake/app.h"
#include "../../smooth_menu/lv_anim/lv_anim.h"
#include "gui/gui_wifi_scan.h"


namespace MOONCAKE
{
    namespace USER_APP
    {
        namespace WIFI_SCAN
        {
            /** @brief WiFi_Scan runtime state */
            struct Data_t
            {
                AppContext* ctx = nullptr;  ///< host context (canvas)

                bool scanning = false;                 ///< a WiFi.scanNetworks(true) is in flight
                uint32_t scroll_speed_time_count = 0;  ///< millis() of the last detent

                int menu_y_offset = 0;         ///< scroll target of the list, px
                std::string wifi_scan_result;  ///< list text, one line per network

                LVGL::Anim_Path menu_y_offset_anim;  ///< eases the list to menu_y_offset
            };
        }

        /**
         * @brief Scans for Wi-Fi networks (station mode, no connection, no credentials) and
         * lists up to 10 as `<n>. (<rssi>) <ssid>`; the knob scrolls the list, a touch on the
         * screen rescans, a button click quits.
         *
         * The scan is asynchronous (WiFi.scanNetworks(true)), so the button stays responsive
         * while it runs. The scan results are freed (WiFi.scanDelete()) as soon as the list
         * text is built, and Wi-Fi is switched off on close, so repeated opens do not leak.
         *
         * Log lines: `app: open WiFi_Scan`, `wifi: scan`, `wifi: ap_count=<n>` and
         * `wifi: <rssi> <ssid>` per listed network (or `wifi: scan failed`),
         * `wifi: heap=<free internal bytes>` after Wi-Fi is off, `app: close WiFi_Scan`.
         * User data: AppContext* (set with setUserData() before onSetup()).
         */
        class WiFi_Scan : public APP_BASE
        {
            private:
                EncoderDetent _detent;  ///< encoder raw count -> detent steps (cw positive)

                /** @brief Show "Scanning..." and start an asynchronous scan */
                void _start_scan();
                /** @brief If the scan has finished: log, build the list text, free the results */
                void _poll_scan();
                /** @brief Show the new list text from the top (slide-in), dropping stale knob turns */
                void _show_list();

            public:
                WIFI_SCAN::Data_t _data;
                GUI_WiFi_Scan _gui;


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
                /** @brief Log open, Wi-Fi to station mode, start the first scan */
                void onCreate() override;
                /**
                 * @brief One frame: button click -> quit; while scanning poll the scan;
                 * otherwise knob -> scroll, touch -> rescan, draw the list
                 */
                void onRunning() override;
                /** @brief Free scan results, Wi-Fi off, log `wifi: heap=` and `app: close WiFi_Scan` */
                void onDestroy() override;
        };

    }
}
