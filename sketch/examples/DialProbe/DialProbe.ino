/**
 * @file DialProbe.ino
 * @brief Permanent M5Dial diagnostic sketch: probes every on-board peripheral once and then
 *        logs each input event over USB serial (115200), one line per event.
 *
 * The same lines are checked on the virtual M5Dial (sim/scenarios/DialProbe/*.test.yaml)
 * and on the device (tools/device_test.py), so sim and hardware can be compared directly.
 *
 * Log lines (tag: message):
 *   init: display <w>x<h> | init: rtc ok|fail | init: touch ok|fail | init: rfid ok|fail
 *   probe: ready
 *   enc: steps=<+-n> raw=<n>      (clockwise = positive steps)
 *   btn: pressed|released
 *   touch: x=<n> y=<n>
 *   rtc: YYYY-MM-DD HH:MM:SS      (1 Hz)
 *   rfid: uid=<HEX:HEX:..>
 */
#include <M5Dial.h>
#include "sim_shims.h"

/**
 * Sign that turns a raw clockwise encoder count into a positive step.
 * why: M5Dial 1.0.3's ENCODER(pin1 = GPIO41/A, pin2 = GPIO40/B) counts down when A leads B;
 * value taken from the DialProbe encoder scenario, see docs/PORTING_NOTES.md.
 */
#ifndef M5DD_ENCODER_CW_SIGN
#define M5DD_ENCODER_CW_SIGN (-1)
#endif

namespace {

constexpr uint8_t TOUCH_I2C_ADDR = 0x38;  ///< FT3267, M5GFX.cpp:1987
constexpr uint8_t TOUCH_REG_VENDID = 0xA8; ///< FT5x06 vendor id, Touch_FT5x06.inl:34
constexpr uint8_t RFID_REG_VERSION = 0x37; ///< MFRC522 VersionReg
constexpr int32_t COUNTS_PER_DETENT = 4;   ///< x4 quadrature decoding, one cycle per detent
constexpr uint32_t RTC_PERIOD_MS = 1000;
constexpr uint32_t RFID_PERIOD_MS = 250;

bool rfid_ok = false;
int32_t enc_base = 0;   ///< raw count of the last whole detent
uint32_t rtc_next_ms = 0;
uint32_t rfid_next_ms = 0;

/** Read one register of an internal-bus I2C device; false if the device did not ACK. */
bool probe_reg(uint8_t addr, uint8_t reg, uint8_t* value, uint32_t freq) {
    return M5.In_I2C.readRegister(addr, reg, value, 1, freq);
}

/** Touch controller answers and reports a vendor id (the same check M5GFX's driver makes). */
bool probe_touch() {
    uint8_t v = 0;
    return probe_reg(TOUCH_I2C_ADDR, TOUCH_REG_VENDID, &v, 400000) && v != 0;
}

/** RFID reader answers with a plausible VersionReg value. */
bool probe_rfid() {
    uint8_t v = 0;
    return probe_reg(RC522_I2C_ADDRESS, RFID_REG_VERSION, &v, 100000) && v != 0x00 && v != 0xFF;
}

/**
 * Turn the raw x4 count into whole detents relative to the last logged detent and log them.
 * why: relative to the last detent (not raw / 4), so detents around count 0 are as wide as
 * any other (truncating division has a double-wide dead zone there).
 */
void poll_encoder() {
    int32_t raw = M5Dial.Encoder.read();
    int32_t steps = 0;
    while (raw - enc_base >= COUNTS_PER_DETENT) { enc_base += COUNTS_PER_DETENT; ++steps; }
    while (raw - enc_base <= -COUNTS_PER_DETENT) { enc_base -= COUNTS_PER_DETENT; --steps; }
    if (steps != 0) M5_LOGI("enc: steps=%d raw=%d", (int)(steps * M5DD_ENCODER_CW_SIGN), (int)raw);
}

/** Log front-button edges (M5Unified BtnA, GPIO42). */
void poll_button() {
    if (M5Dial.BtnA.wasPressed()) M5_LOGI("btn: pressed");
    if (M5Dial.BtnA.wasReleased()) M5_LOGI("btn: released");
}

/** Log the position of each new touch. */
void poll_touch() {
    auto t = M5Dial.Touch.getDetail();
    if (t.wasPressed()) M5_LOGI("touch: x=%d y=%d", (int)t.x, (int)t.y);
}

/** Log the RTC date/time once per second. */
void poll_rtc(uint32_t now) {
    if ((int32_t)(now - rtc_next_ms) < 0) return;
    rtc_next_ms = now + RTC_PERIOD_MS;
    m5::rtc_datetime_t dt;
    if (!M5Dial.Rtc.getDateTime(&dt)) { M5_LOGW("rtc: read failed"); return; }
    M5_LOGI("rtc: %04d-%02d-%02d %02d:%02d:%02d", dt.date.year, dt.date.month, dt.date.date,
            dt.time.hours, dt.time.minutes, dt.time.seconds);
}

/**
 * Log the UID of a newly presented card, then halt it.
 * why: a halted card ignores REQA, so it is logged once until it leaves the field and comes back.
 */
void poll_rfid(uint32_t now) {
    if (!rfid_ok || (int32_t)(now - rfid_next_ms) < 0) return;
    rfid_next_ms = now + RFID_PERIOD_MS;
    if (!M5Dial.Rfid.PICC_IsNewCardPresent() || !M5Dial.Rfid.PICC_ReadCardSerial()) return;
    char hex[3 * sizeof(M5Dial.Rfid.uid.uidByte)] = {0};
    size_t n = 0;
    for (uint8_t i = 0; i < M5Dial.Rfid.uid.size && i < sizeof(M5Dial.Rfid.uid.uidByte); ++i) {
        n += snprintf(hex + n, sizeof(hex) - n, i ? ":%02X" : "%02X", M5Dial.Rfid.uid.uidByte[i]);
    }
    M5_LOGI("rfid: uid=%s", hex);
    M5Dial.Rfid.PICC_HaltA();
}

}  // namespace

/** Bring up the M5Dial, probe each peripheral and log the result. */
void setup() {
#ifdef M5DD_SIM
    m5dd_sim_route_panel_miso();  // sim-only, see sim_shims.h
#endif
    auto cfg = M5.config();
    // why: RFID is initialised below only if the reader answers; MFRC522::PCD_Reset() spins
    // forever on an unanswered (uninitialised) CommandReg read.
    M5Dial.begin(cfg, /*enableEncoder*/ true, /*enableRFID*/ false);
    // why: M5Unified's default serial log level comes from CORE_DEBUG_LEVEL (0 in this core's
    // default build), so INFO must be enabled explicitly.
    M5.Log.setLogLevel(m5::log_target_serial, ESP_LOG_INFO);

    M5_LOGI("init: display %dx%d", (int)M5Dial.Display.width(), (int)M5Dial.Display.height());
    M5_LOGI("init: rtc %s", M5Dial.Rtc.isEnabled() ? "ok" : "fail");
    M5_LOGI("init: touch %s", probe_touch() ? "ok" : "fail");
    rfid_ok = probe_rfid();
    if (rfid_ok) M5Dial.Rfid.PCD_Init();
    M5_LOGI("init: rfid %s", rfid_ok ? "ok" : "fail");

    M5Dial.Display.fillScreen(TFT_BLACK);
    M5Dial.Display.drawCenterString("DialProbe", 120, 110);
    enc_base = M5Dial.Encoder.read();
    M5_LOGI("probe: ready");
}

/**
 * Poll every input. The encoder is read in a tight loop without redraws.
 * why: M5Dial 1.0.3's ENCODER attaches no interrupts on GPIO40/41, so read() polls the pins;
 * Task 4 replaces it with the interrupt-driven DialEncoder.
 */
void loop() {
    M5Dial.update();
    uint32_t now = millis();
    poll_encoder();
    poll_button();
    poll_touch();
    poll_rtc(now);
    poll_rfid(now);
}
