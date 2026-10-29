/**
 * @file Smoke.ino
 * @brief Toolchain smoke test: boots the M5Dial, logs one line over USB serial
 *        and fills the screen green. Removed in Task 3 once DialProbe exists.
 */
#include <M5Dial.h>

/** Initialise the M5Dial and emit the marker line checked by device_test.py. */
void setup() {
    M5Dial.begin();
    // why: M5Unified's serial log target defaults to ERROR-only; open it up to INFO
    M5.Log.setLogLevel(m5::log_target_serial, ESP_LOG_INFO);
    M5Dial.Display.fillScreen(TFT_GREEN);
    M5_LOGI("boot: smoke");
}

/** Keep the M5Dial state machine ticking; nothing else to do. */
void loop() {
    M5Dial.update();
    delay(10);
}
