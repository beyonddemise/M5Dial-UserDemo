/**
 * @file step_io.cpp
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2023-08-11
 *
 * @copyright Copyright (c) 2023
 *
 * M5DialDemo port of test/io_test.cpp: esp-idf led_strip (RMT) -> Adafruit NeoPixel, the
 * adc_oneshot + calibration helper -> analogReadMilliVolts(1), the hand-made I2C_NUM_1 driver
 * + address probe -> M5Dial.Ex_I2C.scanID(); LED mapping in io_led_level.h (host-tested);
 * a `factory: io` log line per scan; button click (M5Dial.update() in the wait) ends the step
 * and the ring is turned off (docs/PORTING_NOTES.md, "Factory test").
 */
#include <Adafruit_NeoPixel.h>
#include <M5Dial.h>

#include <string>

#include "../../../config.h"
#include "../../../m5dd_begin.h"
#include "io_led_level.h"
#include "steps.h"

namespace factory_test {

namespace {

constexpr uint8_t RING_PIN = 2;     ///< Port B, original LED_STRIP_BLINK_GPIO
constexpr uint8_t POT_PIN = 1;      ///< Port B, original ADC1 channel 0 = GPIO1
constexpr uint32_t SCAN_MS = 800;   ///< original Port A scan period
constexpr uint8_t LED_R = 1, LED_G = 23, LED_B = 1;  ///< original dim green
constexpr uint8_t I2C_FIRST = 0x08, I2C_LAST = 0x77;  ///< scanID() probes 0x08..0x77

/** @return the addresses scanID() found, as "0x3C 0x50" (empty if none). */
std::string scanPortA() {
    bool found[0x80] = {};
    M5Dial.Ex_I2C.scanID(found);
    std::string out;
    char buf[6];
    for (int a = I2C_FIRST; a <= I2C_LAST; ++a) {
        if (!found[a]) continue;
        snprintf(buf, sizeof(buf), "0x%02X", a);
        if (!out.empty()) out += ' ';
        out += buf;
    }
    return out;
}

/** Draws the IO page (upstream layout, cursor text) and pushes it. */
void draw(M5Canvas& c, int mv, int level, const std::string& addrs) {
    c.fillScreen(TFT_BLACK);
    c.setTextSize(1);
    c.setCursor(0, 40);
    c.setTextColor(TFT_YELLOW, TFT_BLACK);
    c.printf("  [IO 测试]\n\n");
    c.setTextColor(TFT_WHITE, TFT_BLACK);
    c.printf("  [PORT-B] ");
    c.setTextColor(TFT_YELLOW, TFT_BLACK);
    c.printf("%dmV %d\n", mv, level);
    c.setTextColor(TFT_WHITE, TFT_BLACK);
    c.printf("  [PORT-A] ");
    if (addrs.empty()) {
        c.setTextColor(TFT_RED, TFT_BLACK);
        c.printf("无");
    } else {
        c.setTextColor(TFT_GREEN, TFT_BLACK);
        c.printf("\n  %s", addrs.c_str());
    }
    c.pushSprite(0, 0);
}

}  // namespace

void runIoStep(AppContext& ctx) {
    M5_LOGI("factory: step IO");
    M5Canvas& c = *ctx.canvas;
    c.setFont(GUI_FONT_CN_BIG);

    Adafruit_NeoPixel ring(io_led_level::PIXELS, RING_PIN, NEO_GRB + NEO_KHZ800);
    ring.begin();
    // why begin here: M5Unified only sets the Port A pins (G13/G15) for Ex_I2C at boot
    M5Dial.Ex_I2C.begin();

    std::string addrs;
    // why "now - SCAN_MS": scan (and log) on the first frame, as upstream's time_count = 0
    uint32_t lastScan = millis() - SCAN_MS - 1;
    int lastLevel = -1;
    while (true) {
        // why M5Dial.update(): BtnA only changes state inside update() (Review Focus 1)
        M5Dial.update();
        int mv = analogReadMilliVolts(POT_PIN);
        int level = io_led_level::level(mv);
        // why only on change: show() blocks interrupts for ~0.5 ms; no need every frame
        if (level != lastLevel) {
            for (int i = 0; i < io_led_level::PIXELS; ++i) {
                bool on = io_led_level::lit(i, level);
                ring.setPixelColor(i, on ? ring.Color(LED_R, LED_G, LED_B) : 0);
            }
            ring.show();
            lastLevel = level;
        }

        if (millis() - lastScan > SCAN_MS) {
            addrs = scanPortA();
            lastScan = millis();
            M5_LOGI("factory: io adc=%d leds=%d i2c=[%s]", mv, level, addrs.c_str());
        }

        draw(c, mv, level, addrs);

        if (m5dd::buttonClicked()) break;
    }

    ring.clear();
    ring.show();
}

}  // namespace factory_test
