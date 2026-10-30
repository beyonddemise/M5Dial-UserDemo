/**
 * @file Smoke.ino
 * @brief Toolchain smoke test: boots the M5Dial, logs one line over USB serial
 *        and fills the screen green. Removed in Task 3 once DialProbe exists.
 */
#include <M5Dial.h>
#include "sim_shims.h"

/** Initialise the M5Dial and emit the marker lines checked by device_test.py. */
void setup() {
#ifdef M5DD_SIM
    m5dd_sim_route_panel_miso();  // sim-only, see sim_shims.h
#endif
    M5Dial.begin();
    // why: M5Unified's default serial log level comes from CORE_DEBUG_LEVEL (0 = none
    // in this core's default build), so INFO must be enabled explicitly.
    M5.Log.setLogLevel(m5::log_target_serial, ESP_LOG_INFO);
    // why: HWCDC drops output until the USB host reconnects after a reset, so wait (bounded)
    // for it or the first markers are lost. Use the same idiom in m5dd::begin.
    while (!Serial && millis() < 3000) delay(10);
    M5_LOGI("boot: smoke");
    // why: the sim scenario checks that M5Unified autodetected the (virtual) Dial
    M5_LOGI("board: %d", (int)M5.getBoard());
    // why: after the logs - the scenario's chip-output check must be its last step
    M5Dial.Display.fillScreen(TFT_GREEN);
    // why: device_test.py checks this marker so a hung/crashed display update fails the
    // hardware test (the sim scenario ignores it; chip output is its last step)
    M5_LOGI("display: green");
}

/** Keep the M5Dial state machine ticking; nothing else to do. */
void loop() {
    M5Dial.update();
    delay(10);
}
