/**
 * @file m5dd_begin.cpp
 * @brief m5dd::begin() / m5dd::runStandalone() and the sim-only shim they need.
 */
#include "m5dd_begin.h"

#include "dial_encoder.h"
#include "rfid_reset.h"

#ifdef M5DD_SIM
#include <esp_rom_gpio.h>
#include <soc/gpio_sig_map.h>
#endif

namespace {

constexpr int CANVAS_SIZE = 240;  ///< the round panel is 240x240
constexpr int CANVAS_BPP = 16;    ///< RGB565: 115 KB, fits without PSRAM
constexpr uint8_t RFID_REG_VERSION = 0x37;   ///< MFRC522 VersionReg
constexpr uint32_t RFID_I2C_FREQ = 100000;   ///< as M5Dial's MFRC522 register access
constexpr uint32_t RFID_RESET_TIMEOUT_MS = 200;  ///< bound on the soft reset (PCD_Reset has none)

bool rfid_present = false;

/**
 * Probes the WS1850S/MFRC522 on the internal I2C bus.
 * @return true if it ACKs and reports a plausible VersionReg (not 00h/FFh)
 */
bool probe_rfid() {
    uint8_t v = 0;
    return M5.In_I2C.readRegister(RC522_I2C_ADDRESS, RFID_REG_VERSION, &v, 1, RFID_I2C_FREQ) &&
           v != 0x00 && v != 0xFF;
}

/**
 * PCD_Init() with a bounded soft reset: rfidSoftReset() replaces PCD_Reset(), then the same
 * timer/ASK/CRC register setup and antenna-on as M5Dial 1.0.3's MFRC522::PCD_Init().
 * @return false if the reader never left power-down (it is then treated as missing)
 *
 * why not M5Dial.Rfid.begin(): PCD_Init() calls PCD_Reset(), which spins forever on the
 * PowerDown bit; a VersionReg answer does not prove the reset will complete.
 */
bool init_rfid() {
    MFRC522& r = M5Dial.Rfid;
    if (!rfidSoftReset([&](uint8_t reg, uint8_t v) { r.PCD_WriteRegister(reg, v); },
                       [&](uint8_t reg) { return r.PCD_ReadRegister(reg); },
                       [] { return (uint32_t)millis(); }, [](uint32_t ms) { delay(ms); },
                       RFID_RESET_TIMEOUT_MS)) {
        return false;
    }
    r.PCD_WriteRegister(MFRC522::TModeReg, 0x80);       // timer starts after each transmission
    r.PCD_WriteRegister(MFRC522::TPrescalerReg, 0xA9);  // 40 kHz timer (25 us period)
    r.PCD_WriteRegister(MFRC522::TReloadRegH, 0x03);    // reload 1000 = 25 ms timeout
    r.PCD_WriteRegister(MFRC522::TReloadRegL, 0xE8);
    r.PCD_WriteRegister(MFRC522::TxASKReg, 0x40);       // force 100 % ASK modulation
    r.PCD_WriteRegister(MFRC522::ModeReg, 0x3D);        // CRC preset 6363h (ISO 14443-3)
    r.PCD_AntennaOn();
    return true;
}

#ifdef M5DD_SIM
/**
 * Route the panel's shared data line (GPIO5) to the SPI2 MISO input (FSPIQ) as well.
 * Sim-only (-DM5DD_SIM, set by tools/sim.sh); must run before M5Dial.begin().
 *
 * why: M5GFX reads the M5Dial panel ID over 3-wire SPI (SPI_SIO; data comes back on
 * MOSI/FSPID, pin_miso = -1). Wokwi's ESP32-S3 SPI samples FSPIQ even in SIO mode,
 * so the RDDID read returns 00000000 and the Dial autodetect fails. Evidence (CI):
 * run 40946628850 (probe: 3-wire 00000000, 4-wire with MISO=GPIO5 00019a00),
 * run 40948213333 (with this route: board_M5Dial detected). Real hardware ignores
 * FSPIQ in SIO mode; see docs/PORTING_NOTES.md.
 */
void sim_route_panel_miso() {
    esp_rom_gpio_connect_in_signal(GPIO_NUM_5, FSPIQ_IN_IDX, false);
}
#endif

}  // namespace

namespace m5dd {

void begin(const char* sketchName, M5Canvas& canvas) {
#ifdef M5DD_SIM
    sim_route_panel_miso();
#endif
    // why: enableEncoder=false - M5Dial's ENCODER attaches no interrupts on GPIO40/41
    // (m5stack/M5Dial#8); dialEncoder counts in an ISR instead (D12).
    // why enableRFID=false (deviation from the plan's M5Dial.begin(cfg, false, true)):
    // MFRC522::PCD_Reset() busy-waits forever on CommandReg, so a missing or stuck reader
    // would hang every sketch at boot; RFID is initialised below only if the reader answers,
    // with a bounded reset (see docs/PORTING_NOTES.md "RFID presence probe").
    M5Dial.begin(M5.config(), /*enableEncoder*/ false, /*enableRFID*/ false);
    dialEncoder.begin();
    // why: M5Unified's default serial log level comes from CORE_DEBUG_LEVEL (0 in this core's
    // default build), so INFO must be enabled explicitly
    M5.Log.setLogLevel(m5::log_target_serial, ESP_LOG_INFO);
    // why: HWCDC drops output until the USB host reconnects after a reset, so wait (bounded)
    // for it or the one-shot boot:/init: lines are lost
    while (!Serial && millis() < 3000) delay(10);
    M5_LOGI("boot: %s", sketchName);

    rfid_present = probe_rfid() && init_rfid();
    M5_LOGI("init: rfid %s", rfid_present ? "ok" : "missing");

    canvas.setColorDepth(CANVAS_BPP);
    if (!canvas.createSprite(CANVAS_SIZE, CANVAS_SIZE)) {
        M5_LOGE("boot: canvas alloc failed (%d free)", (int)ESP.getFreeHeap());
    }
}

bool rfidPresent() { return rfid_present; }

void runStandalone(MOONCAKE::APP_BASE& app, AppContext& ctx) {
    app.setUserData(&ctx);
    app.onSetup();
    app.onCreate();
    for (;;) {
        M5Dial.update();
        app.onRunning();
        if (app.isGoingClose() || app.isGoingDestroy()) {
            app.onDestroy();
            M5_LOGI("app: restart");
            // why: a standalone sketch has no launcher to return to; restarting re-opens the
            // app. Flush first so the log line reaches the host before USB-CDC drops.
            Serial.flush();
            ESP.restart();
        }
    }
}

}  // namespace m5dd
