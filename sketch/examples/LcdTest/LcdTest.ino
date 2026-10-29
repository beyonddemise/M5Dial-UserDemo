/**
 * @file LcdTest.ino
 * @brief Standalone LCD test (MOONCAKE::USER_APP::LCD_Test): turn the knob through the colour
 *        pages, touch to draw dots, click the button to quit (restarts the sketch).
 *
 * Scenario: sim/scenarios/LcdTest/lcd_test.test.yaml. Log lines: `boot: LcdTest`,
 * `app: open LCD_Test`, `lcd: pattern <n>`, `app: close LCD_Test`, `app: restart`.
 */
#include <M5DialDemo.h>
#include <apps/launcher/launcher_layout.h>
#include <apps/lcd_test/app_lcd_test.h>
#include <assets/launcher_icons/icon_lcd.h>

// why: explicit prototypes - arduino-cli's prototype generator is unreliable on this setup
// (it inserted calls inside setup() in DialProbe, see docs/PORTING_NOTES.md)
void setup();
void loop();

namespace {
M5Canvas canvas(&M5Dial.Display);
M5Canvas icon(&canvas);
AppContext ctx{&canvas};
MOONCAKE::USER_APP::LCD_Test app;
}  // namespace

/** Bring up the Dial, draw the app's first page and run it until it quits. */
void setup() {
    m5dd::begin("LcdTest", canvas);
    m5dd::initGui(app, ctx, icon, launcher_layout::appColor(launcher_layout::SLOT_LCD_TEST),
                  image_data_icon_lcd);
    m5dd::runStandalone(app, ctx);
}

/** Never reached: runStandalone() does not return. */
void loop() {}
