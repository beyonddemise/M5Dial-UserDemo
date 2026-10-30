/**
 * @file more_menu_selected_callback.cpp
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2023-08-10
 *
 * @copyright Copyright (c) 2023
 *
 * M5DialDemo port: the LVGL demo branch (_callback_lvgl, lv_demos.h, esp_restart) dropped -
 * LVGL lives only in the standalone LvglDemo sketch (D3); HAL power/RTC calls -> M5Dial.Power /
 * M5Dial.Rtc; `power: off` logged and flushed before the power goes.
 */
#include <M5Dial.h>

#include "app_more_menu.h"
#include "more_menu_items.h"


using namespace MOONCAKE::USER_APP;

namespace {
constexpr uint32_t POWER_OFF_SETTLE_MS = 500;  ///< original delay between RTC IRQ off and power off
}


void MoreMenu::_item_selected_callback(uint8_t selectedNum)
{
    M5_LOGI("more: selected %s", more_menu_items::tag(selectedNum));

    switch (selectedNum)
    {
        case more_menu_items::ITEM_QUIT:
            destroyApp();
            return;
        case more_menu_items::ITEM_POWER_OFF:
            _power_off();
            return;
        default:
            // why: upstream's filler rows ("???" ... "List") do nothing either
            return;
    }
}


void MoreMenu::_power_off()
{
    // why first, and flushed: the line must reach the host before the power (and USB-CDC) goes
    M5_LOGI("power: off");
    Serial.flush();

    _data.ctx->canvas->fillScreen(TFT_BLACK);
    _data.ctx->canvas->pushSprite(0, 0);

    // why: the PCF8563 INT line can switch the M5Dial back on; a pending or armed alarm would
    // restart it right away (upstream did the same)
    M5Dial.Rtc.clearIRQ();
    M5Dial.Rtc.disableIRQ();

    delay(POWER_OFF_SETTLE_MS);

    // why no upstream `delay(4000); powerOn()` after it: M5Unified's powerOff() pulses the hold
    // pin (GPIO46) low and then calls esp_deep_sleep_start(), so it does not return; on USB
    // power, where the hold pin cannot cut the supply, the chip stays in deep sleep instead
    M5Dial.Power.powerOff();
}
