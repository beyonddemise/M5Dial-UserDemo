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
 * M5Dial.Rtc; `power: off` logged and flushed before the power goes; a 4 s timer wake so that
 * on USB power the Dial comes back (upstream: hold pin re-taken after 4 s).
 */
#include <M5Dial.h>
#include <esp_sleep.h>

#include "../../m5dd_begin.h"
#include "app_more_menu.h"
#include "more_menu_items.h"


using namespace MOONCAKE::USER_APP;

namespace {
constexpr uint32_t POWER_OFF_SETTLE_MS = 500;  ///< original delay between RTC IRQ off and power off
constexpr uint64_t USB_POWER_WAKE_US = 4000000;  ///< upstream's 4 s before it re-took the hold pin
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
    m5dd::flushLog();

    _data.ctx->canvas->fillScreen(TFT_BLACK);
    _data.ctx->canvas->pushSprite(0, 0);

    // why: the PCF8563 INT line can switch the M5Dial back on; a pending or armed alarm would
    // restart it right away (upstream did the same)
    M5Dial.Rtc.clearIRQ();
    M5Dial.Rtc.disableIRQ();

    delay(POWER_OFF_SETTLE_MS);

    // why the timer wake instead of upstream's `delay(4000); powerOn()`: M5Unified's powerOff()
    // pulses the hold pin (GPIO46) low and then calls esp_deep_sleep_start(), so it does not
    // return. On battery the supply is cut at the first pulse and the timer never fires. On USB
    // power the hold pin cannot cut the supply, and nothing else wakes the Dial (M5Unified arms
    // no wake source for it; the front button, GPIO42, is not an RTC GPIO): the timer reboots
    // it to the launcher after 4 s instead of leaving it dark until RST or a replug.
    esp_sleep_enable_timer_wakeup(USB_POWER_WAKE_US);
    M5Dial.Power.powerOff();
}
