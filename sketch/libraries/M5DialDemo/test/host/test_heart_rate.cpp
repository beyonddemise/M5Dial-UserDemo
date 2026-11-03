/**
 * @file test_heart_rate.cpp
 * @brief heart_rate: the BLE server's simulated counter (90..159, wrap to 90), the 0x2A37
 *        payload, the upstream device name `M5-<hex>`, the GUI info line, and the 1 s
 *        notification schedule that starts on subscribe and resets on unsubscribe.
 */
#include <cstring>
#include <string>

#include "apps/ble_server/heart_rate.h"
#include "host_test.h"

using namespace heart_rate;

HOST_TEST(hr_counter_counts_up_and_wraps_159_to_90_as_upstream) {
    HT_EQ(91, next(90));
    HT_EQ(159, next(158));
    HT_EQ(90, next(159));
}

HOST_TEST(hr_measurement_is_flags_0x06_then_uint8_value) {
    Measurement m = encode(123);
    HT_EQ(2, (int)sizeof(m.bytes));
    HT_EQ(0x06, m.bytes[0]);
    HT_EQ(123, m.bytes[1]);
}

HOST_TEST(hr_device_name_is_upper_two_mac_bytes_unpadded_hex) {
    // upstream: snprintf("M5-%lX", (uint32_t)(mac_as_le_uint64 >> 32)) = mac[5] << 8 | mac[4]
    const uint8_t mac[6] = {0x24, 0x58, 0x7C, 0x11, 0xAB, 0xCD};
    HT_EQ(0, std::strcmp("M5-CDAB", deviceName(mac).c_str()));
    const uint8_t small[6] = {0, 0, 0, 0, 0x05, 0x00};
    HT_EQ(0, std::strcmp("M5-5", deviceName(small).c_str()));
}

HOST_TEST(hr_info_text_states) {
    HT_EQ(0, std::strcmp("no subscribe", infoText(false, 0).c_str()));
    HT_EQ(0, std::strcmp("no subscribe", infoText(false, 97).c_str()));
    HT_EQ(0, std::strcmp("subscribed", infoText(true, 0).c_str()));
    HT_EQ(0, std::strcmp("send 97", infoText(true, 97).c_str()));
}

HOST_TEST(hr_notifier_idle_until_subscribed) {
    Notifier n;
    uint8_t v = 0;
    HT_EQ(false, n.subscribed());
    HT_EQ(false, n.poll(5000, v));
    HT_EQ(0, n.lastValue());
}

HOST_TEST(hr_notifier_first_value_90_one_interval_after_subscribe) {
    Notifier n;
    uint8_t v = 0;
    n.setSubscribed(true, 1000);
    HT_EQ(true, n.subscribed());
    HT_EQ(false, n.poll(1000 + NOTIFY_INTERVAL_MS - 1, v));
    HT_EQ(true, n.poll(1000 + NOTIFY_INTERVAL_MS, v));
    HT_EQ(90, v);
    HT_EQ(90, n.lastValue());
    HT_EQ(false, n.poll(1000 + NOTIFY_INTERVAL_MS + 10, v));
    HT_EQ(true, n.poll(1000 + 2 * NOTIFY_INTERVAL_MS, v));
    HT_EQ(91, v);
}

HOST_TEST(hr_notifier_one_value_per_poll_even_after_a_long_stall) {
    // why: a slow frame must not burst several notifications (upstream timer re-arms per send)
    Notifier n;
    uint8_t v = 0;
    n.setSubscribed(true, 0);
    HT_EQ(true, n.poll(10 * NOTIFY_INTERVAL_MS, v));
    HT_EQ(90, v);
    HT_EQ(false, n.poll(10 * NOTIFY_INTERVAL_MS + 1, v));
}

HOST_TEST(hr_notifier_unsubscribe_stops_and_restarts_at_90) {
    Notifier n;
    uint8_t v = 0;
    n.setSubscribed(true, 0);
    n.poll(NOTIFY_INTERVAL_MS, v);
    n.poll(2 * NOTIFY_INTERVAL_MS, v);
    HT_EQ(91, v);
    n.setSubscribed(false, 2500);
    HT_EQ(false, n.subscribed());
    HT_EQ(false, n.poll(10000, v));
    HT_EQ(0, n.lastValue());
    n.setSubscribed(true, 20000);
    HT_EQ(true, n.poll(20000 + NOTIFY_INTERVAL_MS, v));
    HT_EQ(90, v);
}

HOST_TEST(hr_notifier_repeated_subscribe_does_not_restart_the_counter) {
    Notifier n;
    uint8_t v = 0;
    n.setSubscribed(true, 0);
    n.poll(NOTIFY_INTERVAL_MS, v);
    n.setSubscribed(true, NOTIFY_INTERVAL_MS + 1);  // no edge
    HT_EQ(true, n.poll(2 * NOTIFY_INTERVAL_MS, v));
    HT_EQ(91, v);
}

HOST_TEST(hr_notifier_survives_millis_wraparound) {
    Notifier n;
    uint8_t v = 0;
    n.setSubscribed(true, 0xFFFFFF00u);
    HT_EQ(false, n.poll(0xFFFFFFFFu, v));
    HT_EQ(true, n.poll(0xFFFFFF00u + NOTIFY_INTERVAL_MS, v));  // wraps past 0
    HT_EQ(90, v);
}

HOST_TEST(hr_notifier_full_cycle_wraps_after_70_values) {
    Notifier n;
    uint8_t v = 0;
    n.setSubscribed(true, 0);
    for (uint32_t i = 1; i <= 70; ++i) n.poll(i * NOTIFY_INTERVAL_MS, v);
    HT_EQ(159, v);
    n.poll(71 * NOTIFY_INTERVAL_MS, v);
    HT_EQ(90, v);
}
