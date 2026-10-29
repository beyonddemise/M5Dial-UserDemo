/**
 * @file TempDemo.ino
 * @brief Standalone thermostat UI demo (MOONCAKE::USER_APP::TempDemo, upstream "VideoShit"):
 *        turn the knob to set 19..27 C, touch the screen to press SET, click the button to
 *        quit (restarts the sketch).
 *
 * Scenario: sim/scenarios/TempDemo/temp_demo.test.yaml. Log lines: `boot: TempDemo`,
 * `app: open TempDemo`, `temp: target=<n>`, `temp: frame`, `app: close TempDemo`,
 * `app: restart`.
 */
#include <M5DialDemo.h>
#include <apps/launcher/launcher_layout.h>
#include <apps/temp_demo/app_temp_demo.h>
#include <assets/launcher_icons/icon_temp.h>

// why: explicit prototypes - arduino-cli's prototype generator is unreliable on this setup
// (it inserted calls inside setup() in DialProbe, see docs/PORTING_NOTES.md)
void setup();
void loop();

namespace {
M5Canvas canvas(&M5Dial.Display);
M5Canvas icon(&canvas);
AppContext ctx{&canvas};
MOONCAKE::USER_APP::TempDemo app;
}  // namespace

/** Bring up the Dial, draw the app's first page and run it until it quits. */
void setup() {
    m5dd::begin("TempDemo", canvas);
    m5dd::initGui(app, ctx, icon, launcher_layout::appColor(launcher_layout::SLOT_TEMP_DEMO),
                  image_data_icon_temp);
    m5dd::runStandalone(app, ctx);
}

/** Never reached: runStandalone() does not return. */
void loop() {}
