/**
 * @file heart_rate.h
 * @brief Arduino-free parts of the BLE heart-rate server: the GATT UUIDs and values, the
 *        simulated heart-rate counter, the 0x2A37 payload, the device name, the GUI info line
 *        and the 1 s notification schedule.
 *
 * Kept free of Arduino/M5/NimBLE includes so it can be unit-tested on the host
 * (test/host/test_heart_rate.cpp). Values and behaviour from the original
 * main/apps/utilities/ble_demo/{ble_demo.c,gatt_svr.c,blehr_sens.h} (Forairaaaaa, M5Stack
 * M5Dial-UserDemo, MIT; derived from the Apache-2.0 ESP-IDF NimBLE `blehr` example) and
 * app_ble_server.cpp onRunning().
 */
#pragma once
#include <stdint.h>
#include <stdio.h>

#include <string>

/** BLE heart-rate server logic. */
namespace heart_rate {

/* GATT layout (blehr_sens.h) */
constexpr uint16_t HRS_UUID = 0x180D;                  ///< Heart Rate service
constexpr uint16_t HRS_MEASUREMENT_UUID = 0x2A37;      ///< notify
constexpr uint16_t HRS_BODY_SENSOR_LOC_UUID = 0x2A38;  ///< read
constexpr uint16_t DEVICE_INFO_UUID = 0x180A;          ///< Device Information service
constexpr uint16_t MANUFACTURER_NAME_UUID = 0x2A29;    ///< read
constexpr uint16_t MODEL_NUMBER_UUID = 0x2A24;         ///< read

/* Values (gatt_svr.c) */
constexpr uint8_t BODY_SENSOR_LOC_CHEST = 0x01;        ///< 0x2A38 value, "Chest"
constexpr const char* MANUFACTURER_NAME = "Apache Mynewt ESP32 devkitC";
constexpr const char* MODEL_NUMBER = "Mynewt HR Sensor demo";

/* Simulation (ble_demo.c) */
constexpr uint8_t HR_START = 90;               ///< first value after a (re)subscribe
constexpr uint8_t HR_WRAP = 160;               ///< reaching this goes back to HR_START
constexpr uint8_t MEASUREMENT_FLAGS = 0x06;    ///< sensor contact supported + detected, uint8
constexpr uint32_t NOTIFY_INTERVAL_MS = 1000;  ///< blehr_tx_timer period

/** @return the counter value after `hr`: +1, 159 -> 90 */
inline uint8_t next(uint8_t hr) {
    uint8_t n = static_cast<uint8_t>(hr + 1);
    return n == HR_WRAP ? HR_START : n;
}

/** Heart Rate Measurement (0x2A37) payload as the original sends it. */
struct Measurement {
    uint8_t bytes[2];  ///< flags, heart rate (uint8 format)
};

/** @return the 0x2A37 payload for heart rate `hr` */
inline Measurement encode(uint8_t hr) { return Measurement{{MEASUREMENT_FLAGS, hr}}; }

/**
 * Advertised / GAP device name, as the original builds it from the factory MAC:
 * `snprintf("M5-%lX", (uint32_t)(chip_mac_id >> 32))` with the 6 MAC bytes loaded into a
 * little-endian uint64 - i.e. the last two MAC bytes, mac[5] high, unpadded upper-case hex.
 * @param mac base MAC address (esp_efuse_mac_get_default() byte order)
 */
inline std::string deviceName(const uint8_t mac[6]) {
    unsigned id = (static_cast<unsigned>(mac[5]) << 8) | mac[4];
    char buf[12];
    snprintf(buf, sizeof(buf), "M5-%X", id);
    return buf;
}

/**
 * Info line under "connected" on the page.
 * @param subscribed a client has notifications on 0x2A37 enabled
 * @param lastValue last notified value, 0 if none yet since subscribing
 * @return "no subscribe" (as upstream), "subscribed" before the first value, else "send <n>"
 * why last sent, not the next value: upstream printed its counter after the increment, so the
 * page was one ahead of what the client had received.
 */
inline std::string infoText(bool subscribed, uint8_t lastValue) {
    if (!subscribed) return "no subscribe";
    if (lastValue == 0) return "subscribed";
    return "send " + std::to_string(lastValue);
}

/**
 * Notification schedule of the simulated sensor, polled from the app loop.
 *
 * Upstream's FreeRTOS timer started (re-armed) on subscribe, fired every second, notified the
 * current value and advanced the counter; a tick after an unsubscribe stopped it and reset the
 * counter to 90. Here: setSubscribed(true) arms the first notification one interval later,
 * poll() reports at most one due value per call, setSubscribed(false) stops and resets.
 */
class Notifier {
   public:
    /**
     * Record the subscription state; only a change has an effect.
     * @param on notifications enabled by the client
     * @param nowMs millis()
     */
    void setSubscribed(bool on, uint32_t nowMs) {
        if (on == _subscribed) return;
        _subscribed = on;
        _next = HR_START;
        _last = 0;
        _since = nowMs;
    }

    /** @return whether notifications are enabled */
    bool subscribed() const { return _subscribed; }

    /**
     * @param nowMs millis()
     * @param[out] value the value to notify, when due
     * @return true if a notification of `value` is due now (the counter has advanced)
     * why `now - since` and re-arm at now: unsigned subtraction survives the millis() wrap,
     * and re-arming from the send time (as upstream's xTimerReset) means a stalled frame
     * sends one value, not a burst.
     */
    bool poll(uint32_t nowMs, uint8_t& value) {
        if (!_subscribed || nowMs - _since < NOTIFY_INTERVAL_MS) return false;
        _since = nowMs;
        value = _last = _next;
        _next = next(_next);
        return true;
    }

    /** @return the last value poll() reported since subscribing, 0 if none */
    uint8_t lastValue() const { return _last; }

   private:
    bool _subscribed = false;
    uint8_t _next = HR_START;  ///< value of the next notification
    uint8_t _last = 0;         ///< last notified value, 0 = none yet
    uint32_t _since = 0;       ///< millis() of the subscribe or the last notification
};

}  // namespace heart_rate
