/**
 * @file app_rfid_test.cpp
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2023-08-04
 *
 * @copyright Copyright (c) 2023
 *
 * M5DialDemo port: the esp-idf-rc522 task + event callback replaced by polling M5Dial.Rfid
 * (MFRC522 API) in onRunning(); the reader is initialised once by m5dd::begin() (bounded,
 * checked) instead of per open (docs/PORTING_NOTES.md, "RFID test"); M5Dial.update() in the
 * button wait (m5dd::buttonClicked), M5_LOGI log lines.
 */
#include "app_rfid_test.h"

#include <M5Dial.h>

#include "../../m5dd_begin.h"
#include "uid_format.h"


namespace {
constexpr uint16_t SCAN_TONE_HZ = 4000;  ///< original buzz.tone(4000, ...)
constexpr uint32_t SCAN_TONE_MS = 30;    ///< plan value (original 100 ms)
}  // namespace


using namespace MOONCAKE::USER_APP;


void RFID_Test::_poll_card()
{
    if (!_data.poll.due(millis()))
        return;

    if (!M5Dial.Rfid.PICC_IsNewCardPresent() || !M5Dial.Rfid.PICC_ReadCardSerial())
        return;

    // why the clamp: uid.size comes from the card's answer; never read past uidByte[]
    uint8_t len = M5Dial.Rfid.uid.size;
    if (len > sizeof(M5Dial.Rfid.uid.uidByte))
        len = sizeof(M5Dial.Rfid.uid.uidByte);
    std::string uid = uid_format::formatUid(M5Dial.Rfid.uid.uidByte, len);

    /* Updata page */
    _gui.renderPage(uid);

    /* Bi */
    M5Dial.Speaker.tone(SCAN_TONE_HZ, SCAN_TONE_MS);

    M5_LOGI("rfid: uid=%s", uid.c_str());

    // why: a halted card ignores REQA, so it is reported once until it leaves the field and
    // comes back (the original library's task did the same per scan)
    M5Dial.Rfid.PICC_HaltA();
}


void RFID_Test::onSetup()
{
    setAppName("RFID_Test");
    setAllowBgRunning(false);

    /* Copy default value */
    APP_RFID_TEST::Data_t default_data;
    _data = default_data;

    _data.ctx = static_cast<AppContext*>(getUserData());
}


/* Life cycle */
void RFID_Test::onCreate()
{
    M5_LOGI("app: open %s", getAppName().c_str());

    // why no init here (the original created and started the rc522 driver per open):
    // m5dd::begin() already initialised the reader with a bounded reset; PCD_Init() could hang
    // on a missing reader (docs/PORTING_NOTES.md, "RFID presence probe")
    _data.reader = m5dd::rfidPresent();
    if (!_data.reader)
    {
        M5_LOGW("rfid: missing");
        _gui.renderMissing();
        return;
    }
    M5_LOGI("rfid: ready");
}


void RFID_Test::onRunning()
{
    /* If button pressed: hold until button release, then bye */
    if (m5dd::buttonClicked())
    {
        destroyApp();
        return;
    }

    if (_data.reader)
        _poll_card();
}


void RFID_Test::onDestroy()
{
    // why no deinit: the reader stays initialised for the next open (owned by m5dd::begin)
    M5_LOGI("app: close %s", getAppName().c_str());
}
