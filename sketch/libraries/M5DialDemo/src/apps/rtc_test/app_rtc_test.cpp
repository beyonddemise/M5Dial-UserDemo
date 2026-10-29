/**
 * @file app_rtc_test.cpp
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2023-08-03
 *
 * @copyright Copyright (c) 2023
 *
 * M5DialDemo port: every HAL call replaced by the M5Dial API (docs/PORTING_NOTES.md,
 * "RTC test"), M5Dial.update() in the button wait (m5dd::buttonClicked), M5_LOGI log lines.
 */
#include "app_rtc_test.h"

#include <M5Dial.h>

#include "../../m5dd_begin.h"
#include "rtc_format.h"

using namespace MOONCAKE::USER_APP;

void RTC_Test::onSetup()
{
    setAppName("RTC_Test");
    setAllowBgRunning(false);

    /* Copy default value */
    RTC_TEST::Data_t default_data;
    _data = default_data;

    _data.ctx = static_cast<AppContext*>(getUserData());
}

/* Life cycle */
void RTC_Test::onCreate() { M5_LOGI("app: open %s", getAppName().c_str()); }

void RTC_Test::onRunning()
{
    /* If button pressed: hold until button release, then bye */
    if (m5dd::buttonClicked())
    {
        destroyApp();
        return;
    }

    /* Update page */
    if (rtc_format::due(millis(), _data.time_count, rtc_format::REFRESH_MS))
    {
        // why before the read (the original set it after drawing): a failed read then also
        // waits a full period instead of retrying, and logging a warning, every frame
        _data.time_count = millis();

        m5::rtc_datetime_t dt;
        if (!M5Dial.Rtc.getDateTime(&dt))
        {
            M5_LOGW("rtc: read failed");
            return;
        }
        char line[rtc_format::DATE_TIME_LEN];
        rtc_format::formatDateTime(line, sizeof(line), dt.date.year, dt.date.month, dt.date.date,
                                   dt.time.hours, dt.time.minutes, dt.time.seconds);
        M5_LOGI("rtc: %s", line);

        _gui.renderPage(dt.time.hours, dt.time.minutes, dt.time.seconds);
    }
}

void RTC_Test::onDestroy() { M5_LOGI("app: close %s", getAppName().c_str()); }
