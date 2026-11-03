/**
 * @file UserDemo.ino
 * @brief The M5Dial UserDemo: the launcher with all demo apps (port of main/main.cpp).
 *
 * Boot: hold the button for more than 1 s while powering on to enter the factory test
 * (ENABLE_FACTORY_TEST, config.h; double tone, then MOONCAKE::USER_APP::FactoryTest, which
 * ends with a power-off - docs/FACTORY_TEST_JIG.md). Otherwise the launcher runs forever.
 *
 * Optional secrets.h (git-ignored, `cp secrets.example.h secrets.h`): Wi-Fi credentials for the
 * factory test, picked up by M5DialDemo's config.h if present.
 * Partition table: partitions.csv in this folder (4 MB app, no OTA, no FAT; tools/build.sh
 * size-checks against it).
 * Scenarios: sim/scenarios/UserDemo/{launcher,more_menu_quit,power_off,enter,skip_io,
 * io_leds}.test.yaml.
 * Log lines: `boot: UserDemo`,
 * `launcher: ready`, `launcher: select <index> <name>`, `app: open <Class>` / `app: close <Class>`
 * (ported apps), `app: <name> not ported`; factory test: `factory: ...` (app_factory_test.h).
 */
#include <M5DialDemo.h>
#include <apps/factory_test/app_factory_test.h>
#include <apps/launcher/launcher.h>

// why: explicit prototypes - arduino-cli's prototype generator is unreliable on this setup
// (it inserted calls inside setup() in DialProbe, see docs/PORTING_NOTES.md)
void setup();
void loop();

namespace {

constexpr uint32_t FACTORY_HOLD_MS = 1000;  ///< original: time_count * 10 > 1000
constexpr uint32_t FACTORY_POLL_MS = 10;
constexpr float FACTORY_TONE_HZ = 4000;
constexpr uint32_t FACTORY_TONE_MS = 30;

M5Canvas canvas(&M5Dial.Display);
AppContext ctx{&canvas};
MOONCAKE::USER_APP::Launcher launcher;

#if ENABLE_FACTORY_TEST
/**
 * @return true if the button is pressed now and stays pressed for more than FACTORY_HOLD_MS.
 * why M5Dial.update() each poll: BtnA only changes state inside update() (Review Focus 1).
 */
bool factory_hold_at_boot() {
    M5Dial.update();
    uint32_t start = millis();
    while (M5Dial.BtnA.isPressed()) {
        if (millis() - start > FACTORY_HOLD_MS) return true;
        delay(FACTORY_POLL_MS);
        M5Dial.update();
    }
    return false;
}
#endif

}  // namespace

/** Bring up the Dial, check for the factory-test hold, then start the launcher. */
void setup() {
    m5dd::begin("UserDemo", canvas);

#if ENABLE_FACTORY_TEST
    if (factory_hold_at_boot()) {
        M5Dial.Speaker.tone(FACTORY_TONE_HZ, FACTORY_TONE_MS);
        delay(50);
        M5Dial.Speaker.tone(FACTORY_TONE_HZ, FACTORY_TONE_MS);
        // why a local (as upstream main.cpp): onCreate() runs every step and ends with the
        // power-off, so the launcher below is never reached in factory mode
        MOONCAKE::USER_APP::FactoryTest factory_test;
        factory_test.setUserData(&ctx);
        factory_test.onSetup();
        factory_test.onCreate();
    }
#endif

    launcher.setUserData(&ctx);
    launcher.onSetup();
    launcher.onCreate();
}

/** One launcher frame; the launcher reads input that update() refreshes. */
void loop() {
    M5Dial.update();
    launcher.onRunning();
}
