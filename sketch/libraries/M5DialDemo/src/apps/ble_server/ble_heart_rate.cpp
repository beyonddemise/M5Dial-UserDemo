/**
 * @file ble_heart_rate.cpp
 * @brief BleHeartRateServer: the original ble_demo.c / gatt_svr.c heart-rate server on
 *        NimBLE-Arduino (see ble_heart_rate.h).
 *
 * Original: Forairaaaaa, M5Stack M5Dial-UserDemo (MIT), itself from the ESP-IDF NimBLE
 * `blehr` example, licensed to the Apache Software Foundation under the Apache License 2.0.
 */
#include "ble_heart_rate.h"

#include <Arduino.h>
#include <M5Unified.h>
#include <esp_bt.h>


using namespace heart_rate;


void BleHeartRateServer::ServerCallbacks::onConnect(NimBLEServer*, NimBLEConnInfo&)
{
    _owner._peer_count++;
}


void BleHeartRateServer::ServerCallbacks::onDisconnect(NimBLEServer*, NimBLEConnInfo&, int)
{
    // why clear here: NimBLE only forwards the unsubscribe-on-disconnect event to
    // onSubscribe() while the connection can still be found, which it no longer can
    if (--_owner._peer_count <= 0)
    {
        _owner._peer_count = 0;
        _owner._peer_subscribed = false;
    }
}


void BleHeartRateServer::MeasurementCallbacks::onSubscribe(NimBLECharacteristic*,
                                                           NimBLEConnInfo&, uint16_t subValue)
{
    _owner._peer_subscribed = (subValue & 0x01) != 0;
}


void BleHeartRateServer::begin(const char* deviceName)
{
    if (_server != nullptr)
        return;

    _name = deviceName;
    _peer_count = 0;
    _peer_subscribed = false;
    _connected = false;
    _notifier.setSubscribed(false, millis());

    // why no nvs_flash_init() as upstream: NimBLEDevice::init() does it (PHY calibration data)
    if (!NimBLEDevice::init(_name))
    {
        // why release the controller here: init() returns false before it marks itself
        // initialised (NVS, controller init or enable failed), so deinit() skips the teardown.
        // A still-initialised controller would make every later open fail as well, and
        // building a server on a host that never started locks a mutex deinit() freed.
        if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED)
            esp_bt_controller_disable();
        if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED)
            esp_bt_controller_deinit();
        M5_LOGE("ble: init failed");
        return;
    }

    _server = NimBLEDevice::createServer();
    // why false: the callback objects are members, NimBLE must not delete them
    _server->setCallbacks(&_server_callbacks, false);
    // why: upstream restarted advertising on every BLE_GAP_EVENT_DISCONNECT
    _server->advertiseOnDisconnect(true);

    /* Service: Heart-rate */
    NimBLEService* hrs = _server->createService(NimBLEUUID(HRS_UUID));
    _measurement = hrs->createCharacteristic(NimBLEUUID(HRS_MEASUREMENT_UUID),
                                             NIMBLE_PROPERTY::NOTIFY);
    _measurement->setCallbacks(&_measurement_callbacks);
    NimBLECharacteristic* location = hrs->createCharacteristic(
        NimBLEUUID(HRS_BODY_SENSOR_LOC_UUID), NIMBLE_PROPERTY::READ);
    location->setValue(BODY_SENSOR_LOC_CHEST);

    /* Service: Device Information */
    NimBLEService* dis = _server->createService(NimBLEUUID(DEVICE_INFO_UUID));
    dis->createCharacteristic(NimBLEUUID(MANUFACTURER_NAME_UUID), NIMBLE_PROPERTY::READ)
        ->setValue(MANUFACTURER_NAME);
    dis->createCharacteristic(NimBLEUUID(MODEL_NUMBER_UUID), NIMBLE_PROPERTY::READ)
        ->setValue(MODEL_NUMBER);

    if (!_server->start())
    {
        M5_LOGE("ble: server start failed");
        // why: the same teardown as end(), so the next open starts from a clean NimBLE
        NimBLEDevice::deinit(true);
        _server = nullptr;
        _measurement = nullptr;
        return;
    }

    /* Advertising: flags (general discoverable, BLE only), TX power, complete name (upstream) */
    NimBLEAdvertisementData adv;
    adv.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
    adv.addTxPower();
    adv.setName(_name);
    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    advertising->setAdvertisementData(adv);
    if (advertising->start())
        M5_LOGI("ble: advertising %s", _name.c_str());
    else
        M5_LOGW("ble: advertising failed");
}


void BleHeartRateServer::loop()
{
    if (_server == nullptr)
        return;

    uint32_t now = millis();

    bool connected = _peer_count > 0;
    if (connected != _connected)
    {
        _connected = connected;
        M5_LOGI("ble: connected=%d", connected ? 1 : 0);
    }

    bool subscribed = _peer_subscribed;
    if (subscribed != _notifier.subscribed())
    {
        _notifier.setSubscribed(subscribed, now);
        M5_LOGI("ble: subscribed=%d", subscribed ? 1 : 0);
    }

    uint8_t hr = 0;
    if (_notifier.poll(now, hr))
    {
        Measurement m = encode(hr);
        // why setValue + notify(): 0x2A37 then also holds the last value for a reader, and
        // notify() with no handle sends to every subscribed client
        _measurement->setValue(m.bytes, sizeof(m.bytes));
        _measurement->notify();
    }
}


void BleHeartRateServer::end()
{
    if (_server == nullptr)
        return;

    NimBLEDevice::stopAdvertising();
    for (uint16_t handle : _server->getPeerDevices())
        _server->disconnect(handle);

    // why true: also deletes the server, services and characteristics (no leak per open)
    NimBLEDevice::deinit(true);
    _server = nullptr;
    _measurement = nullptr;

    if (_connected)
        M5_LOGI("ble: connected=0");
    if (_notifier.subscribed())
        M5_LOGI("ble: subscribed=0");
    _connected = false;
    _notifier.setSubscribed(false, millis());
    _peer_count = 0;
    _peer_subscribed = false;
}
