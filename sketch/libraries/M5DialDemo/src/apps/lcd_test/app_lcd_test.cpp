/**
 * @file app_lcd_test.cpp
 * @author Forairaaaaa
 * @brief 
 * @version 0.1
 * @date 2023-07-27
 * 
 * @copyright Copyright (c) 2023
 * 
 * M5DialDemo port: every HAL call replaced by the M5Dial API (docs/PORTING_NOTES.md,
 * "LCD test"), M5Dial.update() in every input wait, M5_LOGI log lines.
 */
#include "app_lcd_test.h"

#include <M5Dial.h>

#include "../../dial_encoder.h"
#include "../../m5dd_begin.h"


using namespace MOONCAKE::USER_APP;

namespace {
constexpr uint32_t TOUCH_POLL_MS = 5;  ///< original `while (isTouched()) delay(5)`
}


void LCD_Test::onSetup()
{
    setAppName("LCD_Test");
    setAllowBgRunning(false);

    /* Copy default value */
    LCD_TEST::Data_t default_data;
    _data = default_data;

    _data.ctx = static_cast<AppContext*>(getUserData());
}


/* Life cycle */
void LCD_Test::onCreate()
{
    M5_LOGI("app: open %s", getAppName().c_str());
    M5_LOGI("lcd: pattern %d", _gui.currentPage());
    _detent.reset(dialEncoder.read());

    /* Wait until released */
    // why: opened by a centre tap, the finger is still down - it must not draw a dot.
    // M5Dial.update() refreshes the touch state (Review Focus 1).
    while (M5Dial.Touch.getDetail().isPressed())
    {
        delay(TOUCH_POLL_MS);
        M5Dial.update();
    }
}


void LCD_Test::onRunning()
{
    /* If scrolled */
    // why no `delay(100)` ("lower frequency") as in the original: it throttled the per-count
    // wasMoved(); EncoderDetent already yields one step per detent, and the interrupt-driven
    // counter loses nothing in between.
    int32_t steps = m5dd::readDetents(_detent);
    if (steps != 0)
    {
        _gui.goPages(steps);
        M5_LOGI("lcd: pattern %d", _gui.currentPage());
    }

    /* If button pressed: hold until button release, then bye */
    if (m5dd::buttonClicked())
    {
        destroyApp();
        return;
    }

    /* If touched */
    auto touch = M5Dial.Touch.getDetail();
    if (touch.isPressed())
    {
        _gui.renderTp(touch.x, touch.y);
    }
}


void LCD_Test::onDestroy()
{
    M5_LOGI("app: close %s", getAppName().c_str());
}
