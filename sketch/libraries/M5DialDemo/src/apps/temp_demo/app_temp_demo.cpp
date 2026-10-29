/**
 * @file app_temp_demo.cpp
 * @author Forairaaaaa
 * @brief 
 * @version 0.1
 * @date 2023-08-10
 * 
 * @copyright Copyright (c) 2023
 * 
 * M5DialDemo port: VideoShit -> TempDemo, every HAL call replaced by the M5Dial API
 * (docs/PORTING_NOTES.md, "Temp demo"), M5Dial.update() in every input wait, M5_LOGI log
 * lines; Arduino delay()/millis() instead of the FreeRTOS/esp_timer macros.
 */
#include "app_temp_demo.h"

#include <M5Dial.h>

#include "../../dial_encoder.h"
#include "../../m5dd_begin.h"
#include "temp_demo_target.h"


namespace {
constexpr uint32_t TOUCH_POLL_MS = 5;  ///< original `while (isTouched()) delay(5)`
}


namespace MOONCAKE
{
    namespace USER_APP
    {
        void TempDemo::_log_frame()
        {
            M5_LOGI("temp: frame");
        }


        void TempDemo::onSetup()
        {
            // why the class name, not upstream's "Video Shit": the `app: open/close` log lines
            // use the class name in every app
            setAppName("TempDemo");
            setAllowBgRunning(false);

            /* Init with default value */
            TEMP_DEMO::Data_t default_data;
            _data = default_data;

            _data.ctx = static_cast<AppContext*>(getUserData());
        }


        /* Life cycle */
        void TempDemo::onCreate()
        {
            M5_LOGI("app: open %s", getAppName().c_str());
            M5_LOGI("temp: target=%d", temp_demo_target::celsius(_gui.target()));
            _detent.reset(dialEncoder.read());

            // why redraw here: the host drew the page with getGui()->init() before onCreate(),
            // so this is the first frame the log can show after `app: open`
            _gui.renderPage();
            _log_frame();
        }


        void TempDemo::onRunning()
        {
            /* If scrolled */
            // why no `delay(100)` ("lower frequency") as upstream: see LCD_Test - one step per
            // detent already, and the interrupt-driven counter loses nothing meanwhile
            int32_t steps = m5dd::readDetentsWithClick(_detent);
            if (steps != 0)
            {
                int before = _gui.target();
                _gui.stepTarget(steps);
                _log_frame();
                if (_gui.target() != before)
                {
                    M5_LOGI("temp: target=%d", temp_demo_target::celsius(_gui.target()));
                }
            }

            /* If button pressed: hold until button release, then bye */
            if (m5dd::buttonClicked())
            {
                destroyApp();
                return;
            }

            /* If touched */
            if (M5Dial.Touch.getDetail().isPressed())
            {
                _gui.buttonPressed();
                _log_frame();
                // why M5Dial.update(): the touch state only changes inside update(); a button
                // press made and released entirely inside this wait is consumed here (no quit)
                while (M5Dial.Touch.getDetail().isPressed())
                {
                    delay(TOUCH_POLL_MS);
                    M5Dial.update();
                }
                _gui.buttonReleased();
                _log_frame();
            }
        }


        void TempDemo::onDestroy()
        {
            M5_LOGI("app: close %s", getAppName().c_str());
        }
    }
}
