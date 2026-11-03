/**
 * @file step_apps.cpp
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2023-08-11
 *
 * @copyright Copyright (c) 2023
 *
 * M5DialDemo port of test/lcd_test.cpp, rtc_test.cpp and rfid_test.cpp: the ported apps run
 * embedded (as upstream) with M5Dial.update() each frame; the per-frame Chinese overlay text of
 * the RTC/RFID steps dropped (the step name is on the verdict prompt instead); the RFID step
 * skips a missing reader (docs/PORTING_NOTES.md, "Factory test").
 */
#include <M5Dial.h>

#include "../../../config.h"
#include "../../../m5dd_begin.h"
#include "../../../assets/launcher_icons/icon_lcd.h"
#include "../../../assets/launcher_icons/icon_rfid.h"
#include "../../../assets/launcher_icons/icon_rtc.h"
#include "../../lcd_test/app_lcd_test.h"
#include "../../rfid_test/app_rfid_test.h"
#include "../../rtc_test/app_rtc_test.h"
#include "steps.h"

namespace factory_test {

namespace {

constexpr const char* FW_VERSION = "V1.0";  ///< upstream FW_VERISON (app_factory_test.h)

constexpr uint32_t RELEASE_POLL_MS = 10;

/**
 * Runs `app` embedded until it asks to close: GUI init (black theme as upstream, the app's
 * launcher icon), onSetup/onCreate, then M5Dial.update() + onRunning() per frame, onDestroy().
 * why not the launcher's runner: it is private to the launcher and also plays its animations.
 */
void runEmbedded(MOONCAKE::APP_BASE& app, AppContext& ctx, const uint16_t* iconPixels) {
    M5Canvas icon(ctx.canvas);
    // why TFT_BLACK: upstream set every embedded app's theme to black
    m5dd::initGui(app, ctx, icon, TFT_BLACK, iconPixels);
    app.setUserData(&ctx);
    app.onSetup();
    app.onCreate();
    while (!app.isGoingDestroy() && !app.isGoingClose()) {
        // why: BtnA/touch only change state inside update() (Review Focus 1)
        M5Dial.update();
        app.onRunning();
    }
    app.onDestroy();
    icon.deleteSprite();
}

}  // namespace

void runLcdStep(AppContext& ctx) {
    M5_LOGI("factory: step LCD");
    M5Canvas& c = *ctx.canvas;
    c.fillScreen(TFT_BLACK);
    c.setFont(GUI_FONT_CN_BIG);
    c.setTextSize(1);
    c.setTextColor(TFT_YELLOW);
    c.setCursor(20, 20);
    c.printf("[M5Dial测试]\n  %s\n  %s\n\n  松开按键\n  进入测试模式", FW_VERSION, __DATE__);
    c.pushSprite(0, 0);

    // why M5Dial.update(): the button is usually still held from the boot check; BtnA only
    // changes state inside update() (Review Focus 1)
    M5Dial.update();
    while (M5Dial.BtnA.isPressed()) {
        delay(RELEASE_POLL_MS);
        M5Dial.update();
    }

    MOONCAKE::USER_APP::LCD_Test app;
    runEmbedded(app, ctx, image_data_icon_lcd);
}

void runRtcStep(AppContext& ctx) {
    M5_LOGI("factory: step RTC");
    MOONCAKE::USER_APP::RTC_Test app;
    runEmbedded(app, ctx, image_data_icon_rtc);
}

bool runRfidStep(AppContext& ctx) {
    M5_LOGI("factory: step RFID");
    // why before the app: RFID_Test would show "Reader Not Found" and wait for a click; the
    // verdict is already clear, so go straight to the prompt (default FAIL)
    if (!m5dd::rfidPresent()) {
        M5_LOGW("factory: rfid missing");
        return false;
    }
    MOONCAKE::USER_APP::RFID_Test app;
    runEmbedded(app, ctx, image_data_icon_rfid);
    return true;
}

}  // namespace factory_test
