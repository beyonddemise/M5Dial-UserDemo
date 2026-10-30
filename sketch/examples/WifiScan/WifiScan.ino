/**
 * @file WifiScan.ino
 * @brief Standalone Wi-Fi scan (MOONCAKE::USER_APP::WiFi_Scan): lists up to 10 nearby networks
 *        as `<n>. (<rssi>) <ssid>`; the knob scrolls, a touch on the screen rescans, a button
 *        click quits (restarts the sketch). Station mode only: no connection, no credentials.
 *
 * Scenario: sim/scenarios/WifiScan/scan.test.yaml. Log lines: `boot: WifiScan`,
 * `app: open WiFi_Scan`, `wifi: scan`, `wifi: ap_count=<n>`, `wifi: <rssi> <ssid>`,
 * `wifi: heap=<free>`, `app: close WiFi_Scan`, `app: restart`.
 */
#include <M5DialDemo.h>
#include <apps/launcher/launcher_layout.h>
#include <apps/wifi_scan/app_wifi_scan.h>
#include <assets/launcher_icons/icon_wifi.h>

// why: explicit prototypes - arduino-cli's prototype generator is unreliable on this setup
// (it inserted calls inside setup() in DialProbe, see docs/PORTING_NOTES.md)
void setup();
void loop();

namespace {
M5Canvas canvas(&M5Dial.Display);
M5Canvas icon(&canvas);
AppContext ctx{&canvas};
MOONCAKE::USER_APP::WiFi_Scan app;
}  // namespace

/** Bring up the Dial, draw the "Scanning..." page, run the app. */
void setup() {
    m5dd::begin("WifiScan", canvas);
    m5dd::initGui(app, ctx, icon, launcher_layout::appColor(launcher_layout::SLOT_WIFI_SCAN),
                  image_data_icon_wifi);
    m5dd::runStandalone(app, ctx);
}

/** Never reached: runStandalone() does not return. */
void loop() {}
