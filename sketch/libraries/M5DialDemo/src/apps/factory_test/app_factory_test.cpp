/**
 * @file app_factory_test.cpp
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2023-08-11
 *
 * @copyright Copyright (c) 2023
 *
 * M5DialDemo port: the step sequence of onCreate() kept (LCD, RTC, RFID, IO, RTC wake-up;
 * Wi-Fi/BLE were commented out upstream and are not ported); a verdict prompt after every
 * step and a summary before the wake-up test are new; M5Dial.update() in every button wait,
 * M5_LOGI log lines (docs/PORTING_NOTES.md, "Factory test").
 */
#include "app_factory_test.h"

#include <M5Dial.h>

#include "../../config.h"
#include "../../m5dd_begin.h"
#include "steps/steps.h"
#include "verdict_prompt.h"


using namespace MOONCAKE::USER_APP;
using factory_test::Step;
using factory_test::Verdict;

namespace {
constexpr int RTC_WAKEUP_S = 4;          ///< original setAlarmIRQ(4)
constexpr uint32_t POWER_OFF_SETTLE_MS = 500;  ///< original delay(500) before powerOff()
constexpr uint32_t POLL_MS = 10;
constexpr int CENTER = 120;              ///< centre of the 240x240 round panel
constexpr int SUMMARY_Y = 40;            ///< centre of the first verdict line
constexpr int SUMMARY_PITCH = 26;        ///< verdict line spacing (efontCN_24, 2 px gap)
constexpr int RESULT_Y = 148;            ///< "ALL OK" / "CHECK FAIL"
/// first of the three small hint lines
/// (why not lower: the round panel (r 119) is ~140 px wide at the last line's glyph bottom,
/// y~216; 8 CJK glyphs of efontCN_16_b need ~128 px, at 182 the corners were clipped)
constexpr int HINT_Y = 172;
constexpr int HINT_PITCH = 18;           ///< hint line spacing (efontCN_16_b)
}  // namespace


void FactoryTest::onSetup()
{
    setAppName("FactoryTest");
    setAllowBgRunning(false);

    /* Copy default value */
    FACTORY_TEST::Data_t default_data;
    _data = default_data;

    _data.ctx = static_cast<AppContext*>(getUserData());
}


/* Life cycle */
void FactoryTest::onCreate()
{
    M5_LOGI("app: open %s", getAppName().c_str());
    M5Canvas& canvas = *_data.ctx->canvas;
    factory_test::FactoryReport& report = _data.report;

    factory_test::runLcdStep(*_data.ctx);
    report.set(Step::Lcd, factory_test::promptVerdict(canvas, "LCD"));

    factory_test::runRtcStep(*_data.ctx);
    report.set(Step::Rtc, factory_test::promptVerdict(canvas, "RTC"));

    // why FAIL as default: without a reader the step cannot pass; the operator still decides
    bool reader = factory_test::runRfidStep(*_data.ctx);
    report.set(Step::Rfid, factory_test::promptVerdict(canvas, "RFID",
                                                       reader ? Verdict::Pass : Verdict::Fail));

    factory_test::runIoStep(*_data.ctx);
    report.set(Step::Io, factory_test::promptVerdict(canvas, "IO"));

    // _wifi_test(); _ble_test();  -- commented out upstream; not ported (PORTING_NOTES)
    _show_summary();
    _rtc_wakeup_test();
}


void FactoryTest::_show_summary()
{
    const factory_test::FactoryReport& report = _data.report;
    M5Canvas& c = *_data.ctx->canvas;
    c.fillScreen(TFT_BLACK);
    c.drawCircle(120, 120, 119, TFT_DARKGRAY);
    c.setFont(GUI_FONT_CN_BIG);
    c.setTextSize(1);
    // why centred: the panel is round; left-aligned text would be clipped near the bottom
    c.setTextDatum(middle_center);
    for (int i = 0; i < factory_test::STEP_COUNT; ++i)
    {
        Step s = static_cast<Step>(i);
        Verdict v = report.get(s);
        c.setTextColor(v == Verdict::Pass ? TFT_GREEN : (v == Verdict::Fail ? TFT_RED : TFT_ORANGE));
        c.drawString(report.line(s).c_str(), CENTER, SUMMARY_Y + i * SUMMARY_PITCH);
    }
    bool ok = report.allPassedOrSkipped();
    c.setTextColor(ok ? TFT_GREEN : TFT_RED);
    c.drawString(ok ? "ALL OK" : "CHECK FAIL", CENTER, RESULT_Y);
    // upstream wake-up screen text: remove the power, press to power off, restarts in seconds
    c.setTextColor(TFT_YELLOW);
    c.setFont(GUI_FONT_CN_SMALL);
    c.drawString("请移除电源", CENTER, HINT_Y);
    c.drawString("按下按键[关机]", CENTER, HINT_Y + HINT_PITCH);
    c.drawString("数秒后将自动启动", CENTER, HINT_Y + 2 * HINT_PITCH);
    c.setFont(GUI_FONT_CN_BIG);
    c.setTextDatum(top_left);
    c.pushSprite(0, 0);

    M5_LOGI("%s", report.serial().c_str());
}


void FactoryTest::_rtc_wakeup_test()
{
    /* Wait for a click */
    // why M5Dial.update(): BtnA only changes state inside update() (Review Focus 1)
    while (true)
    {
        M5Dial.update();
        if (m5dd::buttonClicked())
            break;
        delay(POLL_MS);
    }

    // why clearIRQ first (as upstream): a pending alarm flag would wake the Dial at once
    M5Dial.Rtc.clearIRQ();
    M5Dial.Rtc.setAlarmIRQ(RTC_WAKEUP_S);

    _data.ctx->canvas->fillScreen(TFT_BLACK);
    _data.ctx->canvas->pushSprite(0, 0);

    // why logged and flushed before the power goes: the line must reach the host first
    M5_LOGI("factory: rtc wakeup in %d s", RTC_WAKEUP_S);
    m5dd::flushLog();
    delay(POWER_OFF_SETTLE_MS);

    // why no return: M5Unified's powerOff() pulses the hold pin (GPIO46) low and then enters
    // deep sleep; on USB power (hold pin cannot cut the supply) the chip stays in deep sleep
    M5Dial.Power.powerOff();
    while (true)
        delay(1000);
}
