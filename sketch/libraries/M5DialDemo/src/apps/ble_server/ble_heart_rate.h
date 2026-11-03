/**
 * @file ble_heart_rate.h
 * @brief BLE heart-rate peripheral on NimBLE-Arduino: the GATT server of the original
 *        ble_demo.c / gatt_svr.c (Forairaaaaa, M5Stack M5Dial-UserDemo, MIT; from the
 *        Apache-2.0 ESP-IDF NimBLE `blehr` example), as a class the app polls.
 *
 * M5DialDemo port: raw NimBLE host (nimble_port_*, ble_gatts_*, a FreeRTOS notify timer) ->
 * NimBLEDevice / NimBLEServer with the counter and schedule in heart_rate.h (host-tested),
 * notified from loop() (docs/PORTING_NOTES.md, "BLE server").
 */
#pragma once
#include <NimBLEDevice.h>
#include <stdint.h>

#include <atomic>
#include <string>

#include "heart_rate.h"

/**
 * @brief Simulated BLE heart-rate sensor: Heart Rate service 0x180D (0x2A37 notify, 0x2A38
 * read = Chest) and Device Information 0x180A (0x2A29, 0x2A24), advertised under a given name.
 *
 * Once a client enables notifications on 0x2A37, loop() sends `{0x06, hr}` every second, hr
 * counting 90..159 and wrapping to 90; an unsubscribe or disconnect resets it. Advertising
 * resumes after a disconnect.
 *
 * Threading: NimBLE calls the callbacks on its host task; they only set atomics. loop(), on
 * the app task, turns those into log lines, the schedule and the notifications.
 * Log lines: `ble: advertising <name>`, `ble: connected=0|1`, `ble: subscribed=0|1`.
 * One instance at a time (NimBLEDevice is a singleton).
 */
class BleHeartRateServer
{
    public:
        /**
         * @brief Init NimBLE, build the GATT services, start advertising `deviceName`.
         * Does nothing if already started. If NimBLE init or the GATT server start fails it
         * logs `ble: init failed` / `ble: server start failed`, releases what it took and
         * stays stopped: loop() and end() then do nothing, and the next begin() retries.
         * @param deviceName GAP / advertised name (upstream: heart_rate::deviceName(mac))
         */
        void begin(const char* deviceName);

        /** @return a client is connected (as last seen by loop()) */
        bool isConnected() const { return _connected; }

        /** @return a client has notifications on 0x2A37 enabled (as last seen by loop()) */
        bool isSubscribed() const { return _notifier.subscribed(); }

        /** @return last notified heart-rate value, 0 if none since subscribing */
        uint8_t lastValue() const { return _notifier.lastValue(); }

        /** @return the name passed to begin() */
        const std::string& deviceName() const { return _name; }

        /**
         * @brief Poll: log connection / subscription changes, send a notification when due.
         * Call every frame.
         */
        void loop();

        /**
         * @brief Stop advertising, disconnect clients, NimBLEDevice::deinit(true) (frees the
         * NimBLE host and controller heap and the server objects). Safe to call twice; begin()
         * may be called again afterwards.
         */
        void end();

    private:
        /** Connection callbacks (NimBLE host task): count clients, flag changes. */
        class ServerCallbacks : public NimBLEServerCallbacks
        {
            public:
                explicit ServerCallbacks(BleHeartRateServer& owner) : _owner(owner) {}
                /** @brief A client connected */
                void onConnect(NimBLEServer* server, NimBLEConnInfo& info) override;
                /** @brief A client disconnected (its CCCD subscription ends with it) */
                void onDisconnect(NimBLEServer* server, NimBLEConnInfo& info, int reason) override;

            private:
                BleHeartRateServer& _owner;
        };

        /** 0x2A37 CCCD callback (NimBLE host task): flag the notify bit. */
        class MeasurementCallbacks : public NimBLECharacteristicCallbacks
        {
            public:
                explicit MeasurementCallbacks(BleHeartRateServer& owner) : _owner(owner) {}
                /** @brief CCCD written: bit 0 = notifications, bit 1 = indications */
                void onSubscribe(NimBLECharacteristic* chr, NimBLEConnInfo& info,
                                 uint16_t subValue) override;

            private:
                BleHeartRateServer& _owner;
        };

        ServerCallbacks _server_callbacks{*this};
        MeasurementCallbacks _measurement_callbacks{*this};

        NimBLEServer* _server = nullptr;             ///< owned by NimBLEDevice
        NimBLECharacteristic* _measurement = nullptr;  ///< 0x2A37, owned by _server
        std::string _name;

        std::atomic<int> _peer_count{0};          ///< written by the host task
        std::atomic<bool> _peer_subscribed{false};  ///< written by the host task

        bool _connected = false;          ///< _peer_count > 0 as last logged
        heart_rate::Notifier _notifier;  ///< subscription as last logged + schedule
};
