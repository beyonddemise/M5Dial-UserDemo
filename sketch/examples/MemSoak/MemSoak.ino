/**
 * @file MemSoak.ino
 * @brief Device-only heap soak (Review Focus 5): opens and closes the Wi-Fi scan and the BLE
 *        server apps in turn, three times, without restarting, and logs the internal heap
 *        after each cycle. No input needed; the knob and button are not used.
 *
 * Each cycle: WiFi_Scan runs until its scan has completed (`wifi: ap_count=<n>`, n may be 0)
 * or 20 s pass, then is destroyed; BLE_Server runs for 5 s, then is destroyed; then
 * `heap: free=<n> min=<n>` (MALLOC_CAP_INTERNAL, current and lowest since boot).
 * After three good cycles it logs `soak: done`, a success-only line. Any failure logs
 * `soak: FAIL <reason>` (`wifi timeout`, `wifi quit`, `ble quit`, `alloc wifi`, `alloc ble`)
 * and stops; the sketch then idles and never restarts by itself.
 * Sequencing: mem_soak.h (host-tested). Device check (no scenario: Wokwi has no BLE):
 *   tools/device_test.py examples/MemSoak --expect 'heap: free=\d+ min=\d+' \
 *     'heap: free=\d+ min=\d+' 'heap: free=\d+ min=\d+' 'soak: done' \
 *     --forbid 'boot: MemSoak.*boot: MemSoak' --forbid 'soak: FAIL' --timeout 180
 */
#include <M5DialDemo.h>
#include <apps/ble_server/app_ble_server.h>
#include <apps/launcher/launcher_layout.h>
#include <apps/wifi_scan/app_wifi_scan.h>
#include <assets/launcher_icons/icon_ble.h>
#include <assets/launcher_icons/icon_wifi.h>
#include <esp_heap_caps.h>
#include <mem_soak.h>

#include <new>

// why: explicit prototypes - arduino-cli's prototype generator is unreliable on this setup
// (it inserted calls inside setup() in DialProbe, see docs/PORTING_NOTES.md)
void setup();
void loop();

namespace {

using MOONCAKE::APP_BASE;
using MOONCAKE::USER_APP::BLE_Server;
using MOONCAKE::USER_APP::WiFi_Scan;
using mem_soak::Phase;
using mem_soak::Step;

constexpr uint32_t IDLE_MS = 100;  ///< loop() period once the soak is over

M5Canvas canvas(&M5Dial.Display);
// why one icon sprite for both apps: createSprite() at the same size reuses the buffer, so
// re-initialising it per app does not allocate (and cannot skew the heap figures)
M5Canvas icon(&canvas);
AppContext ctx{&canvas};
mem_soak::Sequencer soak;

/**
 * Creates and opens the app of `phase` as the launcher would: GUI init, setUserData(&ctx),
 * onSetup(), onCreate(); then starts its clock.
 * @return the app, or nullptr if it could not be allocated (soak failed)
 */
APP_BASE* open_app(Phase phase) {
    APP_BASE* app = nullptr;
    if (phase == Phase::Wifi) {
        app = new (std::nothrow) WiFi_Scan;
        if (app == nullptr) return nullptr;
        m5dd::initGui(*app, ctx, icon, launcher_layout::appColor(launcher_layout::SLOT_WIFI_SCAN),
                      image_data_icon_wifi);
    } else {
        app = new (std::nothrow) BLE_Server;
        if (app == nullptr) return nullptr;
        m5dd::initGui(*app, ctx, icon,
                      launcher_layout::appColor(launcher_layout::SLOT_BLE_SERVER),
                      image_data_icon_ble);
    }
    app->setUserData(&ctx);
    app->onSetup();
    app->onCreate();
    soak.appStarted(millis());
    return app;
}

/** Closes and frees an app (onDestroy(), delete); null-safe. */
void close_app(APP_BASE*& app) {
    if (app == nullptr) return;
    app->onDestroy();
    delete app;
    app = nullptr;
}

/** Logs `soak: FAIL <reason>`. */
void log_fail() { M5_LOGE("soak: FAIL %s", soak.failReason()); }

/**
 * The soak driver: runs the Sequencer to completion, then returns.
 * why its own loop, not m5dd::runStandalone(): runStandalone() restarts the chip when an app
 * closes, which would reset the heap the soak is measuring.
 */
void run_soak() {
    M5_LOGI("soak: start cycles=%d", soak.cycles());
    APP_BASE* app = open_app(soak.phase());
    if (app == nullptr) {
        soak.fail("alloc wifi");
        log_fail();
        return;
    }

    while (!soak.finished()) {
        M5Dial.update();
        app->onRunning();

        bool quit = app->isGoingDestroy() || app->isGoingClose();
        // why the static_cast is safe: in the Wi-Fi phase `app` is always the WiFi_Scan that
        // open_app() made for it; the BLE phase has no done query (the Sequencer times it)
        bool done = soak.phase() == Phase::Wifi && static_cast<WiFi_Scan*>(app)->scanDone();

        switch (soak.poll(millis(), done, quit)) {
            case Step::Run:
            case Step::Idle:
                break;
            case Step::Fail:
                close_app(app);
                log_fail();
                return;
            case Step::CycleEnd:
                close_app(app);
                M5_LOGI("heap: free=%u min=%u",
                        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                        (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
                if (soak.succeeded()) {
                    M5_LOGI("soak: done");
                    return;
                }
                [[fallthrough]];
            case Step::Switch:
                close_app(app);  // no-op after CycleEnd
                app = open_app(soak.phase());
                if (app == nullptr) {
                    soak.fail(soak.phase() == Phase::Wifi ? "alloc wifi" : "alloc ble");
                    log_fail();
                    return;
                }
                break;
        }
    }
    close_app(app);
}

}  // namespace

/** Bring up the Dial, run the soak once. */
void setup() {
    m5dd::begin("MemSoak", canvas);
    run_soak();
}

/** The soak is over: idle (no restart, so the result stays on the serial log). */
void loop() {
    M5Dial.update();
    delay(IDLE_MS);
}
