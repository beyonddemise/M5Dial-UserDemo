/**
 * @file DialProbe.ino
 * @brief Permanent M5Dial diagnostic sketch: probes every on-board peripheral once and then
 *        logs each input event over USB serial (115200), one line per event.
 *
 * The same lines are checked on the virtual M5Dial (sim/scenarios/DialProbe/probe.test.yaml)
 * and on the device (tools/device_test.py), so sim and hardware can be compared directly.
 *
 * Log lines (tag: message):
 *   boot: DialProbe | init: rfid ok|missing   (both from m5dd::begin)
 *   init: board <n> | init: display <w>x<h> | init: rtc ok|fail | init: touch ok|fail
 *   probe: ready
 *   enc: steps=<+-n> total=<+-n> raw=<n>  (clockwise = positive; total = sum of all steps)
 *   enc: idle raw=<n> total=<+-n>         (once, 100 ms after the encoder stops moving)
 *   btn: pressed|released
 *   touch: x=<n> y=<n>
 *   rtc: YYYY-MM-DD HH:MM:SS      (1 Hz)
 *   rfid: uid=<HEX:HEX:..>
 */
#include <M5DialDemo.h>

// why: explicit prototypes. Without them arduino-cli 1.5.1's prototype generator (confused by
// this sketch's layout) inserted `setup(); loop();` *inside* setup()'s body, so setup()
// recursed forever (gcc -Winfinite-recursion; .build/<Name>/sketch/<Name>.ino.cpp shows it).
void setup();
void loop();

namespace {

constexpr uint8_t TOUCH_I2C_ADDR = 0x38;  ///< FT3267, M5GFX.cpp:1987
constexpr uint8_t TOUCH_REG_VENDID = 0xA8; ///< FT5x06 vendor id, Touch_FT5x06.inl:34
constexpr uint32_t RTC_PERIOD_MS = 1000;
constexpr uint32_t RFID_PERIOD_MS = 250;
constexpr uint32_t ENC_IDLE_MS = 100;      ///< no edge for this long = knob at rest

M5Canvas canvas(&M5Dial.Display);
EncoderDetent detent;  ///< M5DD_ENCODER_COUNTS_PER_DETENT / M5DD_ENCODER_CW_SIGN from config.h
int32_t enc_total = 0;  ///< sum of all logged steps (probe.test.yaml "encoder fast" checks it)
int32_t enc_last_raw = 0;      ///< raw count at the previous poll
uint32_t enc_moved_ms = 0;     ///< millis() of the last raw change
bool enc_idle_logged = true;   ///< the idle line for the current rest position was printed
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

/**
 * Log the detent steps (clockwise positive) the interrupt-counted encoder moved since the
 * last poll, and the raw count once the knob has come to rest.
 * why the idle line: steps fire at the half-detent point, so a step line's raw value depends
 * on poll timing; the raw count at rest is exact and shows every lost or extra edge.
 */
void poll_encoder(uint32_t now) {
    int32_t raw = dialEncoder.read();
    if (raw != enc_last_raw) {
        enc_last_raw = raw;
        enc_moved_ms = now;
        enc_idle_logged = false;
    } else if (!enc_idle_logged && now - enc_moved_ms >= ENC_IDLE_MS) {
        enc_idle_logged = true;
        M5_LOGI("enc: idle raw=%d total=%d", (int)raw, (int)enc_total);
    }
    int32_t steps = detent.update(raw);
    if (steps == 0) return;
    enc_total += steps;
    M5_LOGI("enc: steps=%d total=%d raw=%d", (int)steps, (int)enc_total, (int)raw);
}

/**
 * Redraw the canvas and push it to the panel.
 * why: called every loop on purpose - a full 240x240 push takes ~10-20 ms, the load under
 * which a polled encoder loses edges (Review Focus 2); the "encoder fast" steps of
 * probe.test.yaml check that DialEncoder does not.
 */
void draw() {
    canvas.fillSprite(TFT_BLACK);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawCenterString("DialProbe", 120, 100);
    canvas.drawCenterString(String(enc_total), 120, 124);
    canvas.pushSprite(0, 0);
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
    if (!m5dd::rfidPresent() || (int32_t)(now - rfid_next_ms) < 0) return;
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

/**
 * Bring up the M5Dial (m5dd::begin, which also probes and initialises RFID and logs
 * `init: rfid ok|missing`), probe the other peripherals and log the result.
 */
void setup() {
    m5dd::begin("DialProbe", canvas);

    // why: board 12 = board_M5Dial, i.e. M5GFX autodetected the (virtual) panel
    M5_LOGI("init: board %d", (int)M5.getBoard());
    M5_LOGI("init: display %dx%d", (int)M5Dial.Display.width(), (int)M5Dial.Display.height());
    M5_LOGI("init: rtc %s", M5Dial.Rtc.isEnabled() ? "ok" : "fail");
    M5_LOGI("init: touch %s", probe_touch() ? "ok" : "fail");

    draw();
    enc_last_raw = dialEncoder.read();
    detent.reset(enc_last_raw);
    M5_LOGI("probe: ready");
}

/** Poll every input, then redraw (see draw() for why every loop). */
void loop() {
    M5Dial.update();
    uint32_t now = millis();
    poll_encoder(now);
    poll_button();
    poll_touch();
    poll_rtc(now);
    poll_rfid(now);
    draw();
}
