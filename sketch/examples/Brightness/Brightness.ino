/**
 * @file Brightness.ino
 * @brief Standalone backlight setting (MOONCAKE::USER_APP::Set_Brightness): turn the knob to
 *        change the brightness (0..255, clockwise = brighter); click the button to quit
 *        (restarts the sketch).
 *
 * Scenario: sim/scenarios/Brightness/brightness.test.yaml. Log lines: `boot: Brightness`,
 * `app: open Set_Brightness`, `brightness: <n>`, `app: close Set_Brightness`, `app: restart`.
 */
#include <M5DialDemo.h>
#include <apps/launcher/launcher_layout.h>
#include <apps/set_brightness/app_set_brightness.h>
#include <assets/launcher_icons/icon_brigntness.h>

// why: explicit prototypes - arduino-cli's prototype generator is unreliable on this setup
// (it inserted calls inside setup() in DialProbe, see docs/PORTING_NOTES.md)
void setup();
void loop();

namespace {
M5Canvas canvas(&M5Dial.Display);
M5Canvas icon(&canvas);
AppContext ctx{&canvas};
MOONCAKE::USER_APP::Set_Brightness app;
}  // namespace

/** Bring up the Dial, prepare the app's GUI and run it until it quits. */
void setup() {
    m5dd::begin("Brightness", canvas);
    m5dd::initGui(app, ctx, icon, launcher_layout::appColor(launcher_layout::SLOT_BRIGHTNESS),
                  image_data_icon_brigntness);
    m5dd::runStandalone(app, ctx);
}

/** Never reached: runStandalone() does not return. */
void loop() {}
