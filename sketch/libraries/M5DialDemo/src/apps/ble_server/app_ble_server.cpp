/**
 * @file app_ble_server.cpp
 * @author Forairaaaaa
 * @brief https://github.com/espressif/arduino-esp32/blob/esp-idf-v5.2/libraries/BLE/examples/BLE_server/BLE_server.ino
 * @version 0.1
 * @date 2023-08-07
 *
 * @copyright Copyright (c) 2023
 *
 * M5DialDemo port: ble_demo_start()/ble_demo_stop() (raw NimBLE host + FreeRTOS timer) ->
 * BleHeartRateServer begin()/loop()/end(); the info text in heart_rate.h (host-tested);
 * M5Dial.update() in the button wait; M5_LOGI log lines (docs/PORTING_NOTES.md, "BLE server").
 */
#include "app_ble_server.h"

#include <M5Dial.h>
#include <esp_heap_caps.h>
#include <esp_mac.h>

#include "../../m5dd_begin.h"
#include "heart_rate.h"


using namespace MOONCAKE::USER_APP;


void BLE_Server::onSetup()
{
    setAppName("BLE_Server");
    setAllowBgRunning(false);

    /* Copy default value */
    BLE_SERVER::Data_t default_data;
    _data = default_data;

    _data.ctx = static_cast<AppContext*>(getUserData());
}


/* Life cycle */
void BLE_Server::onCreate()
{
    M5_LOGI("app: open %s", getAppName().c_str());

    /* Get a name: M5-<last two MAC bytes> (upstream) */
    uint8_t mac[6] = {0};
    esp_efuse_mac_get_default(mac);
    _ble.begin(heart_rate::deviceName(mac).c_str());

    // why: draw the page at once, not one interval after open
    _data.ble_page_update_time_count = millis() - _data.ble_page_update_interval;
}


void BLE_Server::onRunning()
{
    /* If button pressed: hold until button release, then bye */
    if (m5dd::buttonClicked())
    {
        destroyApp();
        return;
    }

    _ble.loop();

    uint32_t now = millis();
    if (now - _data.ble_page_update_time_count >= _data.ble_page_update_interval)
    {
        /* Update page */
        _gui.renderPage(_ble.isConnected() ? GUI_BLE_Server::connected : GUI_BLE_Server::disconnet,
                        _ble.deviceName(),
                        heart_rate::infoText(_ble.isSubscribed(), _ble.lastValue()));

        /* Reset */
        _data.ble_page_update_time_count = now;
    }
}


void BLE_Server::onDestroy()
{
    /* Close ble demo */
    _ble.end();
    M5_LOGI("ble: heap=%u", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    M5_LOGI("app: close %s", getAppName().c_str());
}
