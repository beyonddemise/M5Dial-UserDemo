/**
 * @file RtcTest.ino
 * @brief Standalone RTC test (MOONCAKE::USER_APP::RTC_Test): shows the RTC time, refreshed once
 *        per second; click the button to quit (restarts the sketch).
 *
 * Scenario: sim/scenarios/RtcTest/rtc_test.test.yaml. Log lines: `boot: RtcTest`,
 * `app: open RTC_Test`, `rtc: YYYY-MM-DD HH:MM:SS` (1 Hz), `app: close RTC_Test`,
 * `app: restart`.
 */
#include <M5DialDemo.h>
#include <apps/launcher/launcher_layout.h>
#include <apps/rtc_test/app_rtc_test.h>
#include <assets/launcher_icons/icon_rtc.h>

// why: explicit prototypes - arduino-cli's prototype generator is unreliable on this setup
// (it inserted calls inside setup() in DialProbe, see docs/PORTING_NOTES.md)
void setup();
void loop();

namespace {
M5Canvas canvas(&M5Dial.Display);
M5Canvas icon(&canvas);
AppContext ctx{&canvas};
MOONCAKE::USER_APP::RTC_Test app;
}  // namespace

/** Bring up the Dial, draw the app's first page and run it until it quits. */
void setup() {
    m5dd::begin("RtcTest", canvas);
    m5dd::initGui(app, ctx, icon, launcher_layout::appColor(launcher_layout::SLOT_RTC_TEST),
                  image_data_icon_rtc);
    m5dd::runStandalone(app, ctx);
}

/** Never reached: runStandalone() does not return. */
void loop() {}
