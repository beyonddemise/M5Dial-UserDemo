/**
 * @file app_template.cpp
 * @author Forairaaaaa
 * @brief 
 * @version 0.1
 * @date 2023-07-28
 * 
 * @copyright Copyright (c) 2023
 * 
 * M5DialDemo port (app_set_brightness.cpp): every HAL call replaced by the M5Dial API
 * (docs/PORTING_NOTES.md, "Brightness"), stepping and clamp in brightness_step.h
 * (host-tested), M5Dial.update() in the button wait, M5_LOGI log lines.
 */
#include "app_set_brightness.h"

#include <M5Dial.h>

#include "../../dial_encoder.h"
#include "../../m5dd_begin.h"
#include "brightness_step.h"


using namespace MOONCAKE::USER_APP;

namespace {
constexpr int32_t ANIM_MS = 600;  ///< original ease-out time to the new brightness
}


void Set_Brightness::onSetup()
{
    setAppName("Set_Brightness");
    setAllowBgRunning(false);

    /* Copy default value */
    SET_BRIGHTNESS::Data_t default_data;
    _data = default_data;

    _data.ctx = static_cast<AppContext*>(getUserData());
}


/* Life cycle */
void Set_Brightness::onCreate()
{
    M5_LOGI("app: open %s", getAppName().c_str());
    _detent.reset(dialEncoder.read());

    /* Get current brightness */
    _data.brightness = M5Dial.Display.getBrightness();
    _data.applied = _data.brightness;
    M5_LOGI("brightness: %d", _data.brightness);

    /* Update page for the first time */
    _gui.renderPage(_data.brightness);

    /* Anim init */
    _data.brightness_anim.setAnim(LVGL::ease_out, _data.brightness, _data.brightness, 0);
}


void Set_Brightness::onRunning()
{
    /* If scrolled */
    int32_t steps = m5dd::readDetentsWithClick(_detent);
    if (steps != 0)
    {
        /* Get increment from the scroll speed, step, clamp 0..255 */
        uint32_t now = millis();
        _data.brightness = brightness_step::apply(_data.brightness, steps,
                                                  now - _data.scroll_speed_time_count);

        /* Set brightness */
        M5_LOGI("brightness: %d", _data.brightness);
        _gui.renderPage(_data.brightness);


        /* Reset anim */
        _data.brightness_anim.setAnim(LVGL::ease_out, _data.brightness_anim.getValue(now), _data.brightness, ANIM_MS);
        _data.brightness_anim.resetTime(now);
        

        /* Reset time count */
        _data.scroll_speed_time_count = now;
    }


    /* Update brightness */
    // why not the original `if (!anim.isFinished()) setBrightness(anim.getValue())`: its last
    // frame inside the 600 ms can stop short of the target (e.g. 2 instead of 0). getValue()
    // returns the end value once finished, so writing only changes also lands exactly on it.
    int value = brightness_step::clamp(_data.brightness_anim.getValue(millis()));
    if (value != _data.applied)
    {
        M5Dial.Display.setBrightness(value);
        _data.applied = value;
    }


    /* If button pressed: hold until button release, then bye */
    if (m5dd::buttonClicked())
    {
        destroyApp();
    }
}


void Set_Brightness::onDestroy()
{
    // why: a click during the 600 ms ease-out ends onRunning() before the animation lands, which
    // would leave the backlight at an intermediate value while page and log show the target
    int target = brightness_step::clamp(_data.brightness);
    if (target != _data.applied)
    {
        M5Dial.Display.setBrightness(target);
        _data.applied = target;
    }
    M5_LOGI("app: close %s", getAppName().c_str());
}
