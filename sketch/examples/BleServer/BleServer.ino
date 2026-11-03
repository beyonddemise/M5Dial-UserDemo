/**
 * @file BleServer.ino
 * @brief Standalone BLE heart-rate server (MOONCAKE::USER_APP::BLE_Server): advertises as
 *        `M5-<hex>`, serves Heart Rate 0x180D (0x2A37 notify, 0x2A38 read) and Device
 *        Information 0x180A, and notifies a counting heart rate (90..159) every second to a
 *        subscribed client. A button click quits (restarts the sketch).
 *
 * Device-only: Wokwi has no BLE, so there is no sim scenario. Check from a host with
 * `tools/.venv/bin/python tools/ble_check.py --name M5-<hex>`. Log lines: `boot: BleServer`,
 * `app: open BLE_Server`, `ble: advertising <name>`, `ble: connected=0|1`,
 * `ble: subscribed=0|1`, `ble: heap=<free>`, `app: close BLE_Server`, `app: restart`.
 */
#include <M5DialDemo.h>
#include <apps/ble_server/app_ble_server.h>
#include <apps/launcher/launcher_layout.h>
#include <assets/launcher_icons/icon_ble.h>

// why: explicit prototypes - arduino-cli's prototype generator is unreliable on this setup
// (it inserted calls inside setup() in DialProbe, see docs/PORTING_NOTES.md)
void setup();
void loop();

namespace {
M5Canvas canvas(&M5Dial.Display);
M5Canvas icon(&canvas);
AppContext ctx{&canvas};
MOONCAKE::USER_APP::BLE_Server app;
}  // namespace

/** Bring up the Dial, draw the empty page, run the app. */
void setup() {
    m5dd::begin("BleServer", canvas);
    m5dd::initGui(app, ctx, icon, launcher_layout::appColor(launcher_layout::SLOT_BLE_SERVER),
                  image_data_icon_ble);
    m5dd::runStandalone(app, ctx);
}

/** Never reached: runStandalone() does not return. */
void loop() {}
