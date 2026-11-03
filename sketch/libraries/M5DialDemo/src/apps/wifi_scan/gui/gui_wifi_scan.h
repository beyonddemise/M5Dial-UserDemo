/**
 * @file gui_wifi_scan.h
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2023-08-04
 *
 * @copyright Copyright (c) 2023
 *
 * M5DialDemo port: GUI_Base include path, doc comments; renderScanning() split out of init()
 * so a rescan can show the "Scanning..." page again.
 */
#pragma once
#include <string>

#include "../../../mooncake/gui_base.h"


/** @brief GUI of the Wi-Fi scan: a "Scanning..." page, then the scrollable list of networks. */
class GUI_WiFi_Scan : public GUI_Base
{
    public:
        /** @brief Draw the "Scanning..." page and push it (the host calls this before onCreate) */
        void init() override;

        /** @brief Draw the "WIFI SCAN / Scanning..." page and push it */
        void renderScanning();

        /**
         * @brief Draw the "WIFI LIST" page and push it
         * @param menuY_offet vertical offset of the list in px (knob scrolling, upstream spelling)
         * @param menu list text, one `wifi_list::formatLine()` per network
         */
        void renderPage(int menuY_offet, const std::string& menu);
};
