/**
 * @file gui_ble_server.h
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2023-08-07
 *
 * @copyright Copyright (c) 2023
 *
 * M5DialDemo port: GUI_Base include path, doc comments.
 */
#pragma once
#include <string>

#include "../../../mooncake/gui_base.h"


/** @brief GUI of the BLE server: name, service UUID and connection / notification state. */
class GUI_BLE_Server : public GUI_Base
{
    public:
        /** @brief Connection state shown on the page (upstream spelling) */
        enum BLEStatus_t
        {
            disconnet = 0,
            connected,
        };

        /** @brief Draw the empty page (icon, quit button) and push it */
        void init() override;

        /**
         * @brief Draw the "BLE SERVER" page and push it
         * @param status disconnected: `[Info]: disconnect`; connected: `[Info]: connected` + info
         * @param name advertised device name
         * @param info second info line when connected (heart_rate::infoText())
         */
        void renderPage(BLEStatus_t status, const std::string& name, const std::string& info);
};
