/**
 * @file gui_rfid_test.h
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2023-08-03
 *
 * @copyright Copyright (c) 2023
 *
 * M5DialDemo port: GUI_Base include path, doc comments; renderPage() takes the UID text
 * (uid_format::formatUid) instead of a 64-bit serial number; renderMissing() added for a
 * missing reader.
 */
#pragma once
#include <string>

#include "../../../mooncake/gui_base.h"


/** @brief GUI of the RFID test: an instruction page, the last card's UID, or "reader missing". */
class GUI_RFID_Test : public GUI_Base
{
    private:
        /** @brief Draw the theme background, top icon and the white info bubble (no push) */
        void _draw_bubble_page();

    public:
        /** @brief Draw the "place your card" page and push it */
        void init() override;

        /**
         * @brief Draw the "TAG SCANNED" page with the card UID and push it
         * @param uid UID text, e.g. "04:A1:B2:C3" (up to 10 bytes = 29 characters)
         */
        void renderPage(const std::string& uid);

        /** @brief Draw the "reader not found" page and push it (m5dd::rfidPresent() false) */
        void renderMissing();
};
