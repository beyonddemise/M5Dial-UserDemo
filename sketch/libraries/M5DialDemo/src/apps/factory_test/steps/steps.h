/**
 * @file steps.h
 * @brief The factory test steps (ports of main/apps/app_factory_test/test/*.cpp,
 *        Forairaaaaa, M5Stack M5Dial-UserDemo, MIT). Each logs `factory: step <NAME>`,
 *        runs until the operator clicks the button, then returns; the caller asks for the
 *        verdict (promptVerdict).
 */
#pragma once
#include "../../../app_context.h"

namespace factory_test {

/**
 * LCD step (original `_lcd_test`): shows the "[M5Dial测试]" intro until the button (still
 * held from the boot check) is released, then runs the LCD_Test app (colour pages on the
 * knob, touch dots) until a click.
 */
void runLcdStep(AppContext& ctx);

/** RTC step (original `_rtc_test`): runs the RTC_Test app (time read-out) until a click. */
void runRtcStep(AppContext& ctx);

/**
 * RFID step (original `_rfid_test`): runs the RFID_Test app (card UIDs) until a click.
 * @return false if the reader is missing (m5dd::rfidPresent()): logs `factory: rfid missing`
 *         and returns at once without touching M5Dial.Rfid; true otherwise
 */
bool runRfidStep(AppContext& ctx);

/**
 * IO step (original `_io_test`), for the factory jig (docs/FACTORY_TEST_JIG.md): the pot on
 * GPIO1 (analogReadMilliVolts) sets how much of the 14-pixel NeoPixel ring on GPIO2 is lit
 * (io_led_level.h); Port A (M5Dial.Ex_I2C) is scanned every 800 ms. Runs until a click,
 * then turns the ring off.
 * Log line, after every scan: `factory: io adc=<mv> leds=<level> i2c=[<0xAA 0xBB>]`.
 */
void runIoStep(AppContext& ctx);

}  // namespace factory_test
