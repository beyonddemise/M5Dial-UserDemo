/**
 * @file RfidTest.ino
 * @brief Standalone RFID test (MOONCAKE::USER_APP::RFID_Test): hold a card above the screen to
 *        read its UID (shown, beeped, logged once per presentation); click the button to quit
 *        (restarts the sketch). Without a reader it shows "Reader Not Found" and still quits.
 *
 * Scenario: sim/scenarios/RfidTest/card.test.yaml. Log lines: `boot: RfidTest`,
 * `init: rfid ok|missing`, `app: open RFID_Test`, `rfid: ready|missing`,
 * `rfid: uid=<HEX:HEX:..>`, `app: close RFID_Test`, `app: restart`.
 */
#include <M5DialDemo.h>
#include <apps/launcher/launcher_layout.h>
#include <apps/rfid_test/app_rfid_test.h>
#include <assets/launcher_icons/icon_rfid.h>

// why: explicit prototypes - arduino-cli's prototype generator is unreliable on this setup
// (it inserted calls inside setup() in DialProbe, see docs/PORTING_NOTES.md)
void setup();
void loop();

namespace {
M5Canvas canvas(&M5Dial.Display);
M5Canvas icon(&canvas);
AppContext ctx{&canvas};
MOONCAKE::USER_APP::RFID_Test app;
}  // namespace

/** Bring up the Dial (initialises the reader if present), draw the first page, run the app. */
void setup() {
    m5dd::begin("RfidTest", canvas);
    m5dd::initGui(app, ctx, icon, launcher_layout::appColor(launcher_layout::SLOT_RFID_TEST),
                  image_data_icon_rfid);
    m5dd::runStandalone(app, ctx);
}

/** Never reached: runStandalone() does not return. */
void loop() {}
