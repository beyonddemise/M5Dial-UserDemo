/**
 * @file test_wifi_list.cpp
 * @brief wifi_list: the Wi-Fi scan's list text (`0. (-40) Wokwi-GUEST`), the 10-entry cap,
 *        SSID truncation on a UTF-8 boundary, and the knob scroll acceleration / direction.
 */
#include <cstring>
#include <string>

#include "apps/wifi_scan/wifi_list.h"
#include "host_test.h"

using namespace wifi_list;

HOST_TEST(wifi_listed_count_caps_at_ten_and_maps_errors_to_zero) {
    HT_EQ(0, listedCount(-1));  // WIFI_SCAN_RUNNING
    HT_EQ(0, listedCount(-2));  // WIFI_SCAN_FAILED
    HT_EQ(0, listedCount(0));
    HT_EQ(3, listedCount(3));
    HT_EQ(10, listedCount(10));
    HT_EQ(10, listedCount(37));
}

HOST_TEST(wifi_line_is_index_rssi_ssid_newline_as_upstream) {
    HT_EQ(0, std::strcmp("0. (-40) Wokwi-GUEST\n", formatLine(0, -40, "Wokwi-GUEST").c_str()));
    HT_EQ(0, std::strcmp("9. (-100) x\n", formatLine(9, -100, "x").c_str()));
}

HOST_TEST(wifi_hidden_ssid_has_a_placeholder) {
    HT_EQ(0, std::strcmp("(hidden)", displaySsid("").c_str()));
    HT_EQ(0, std::strcmp("(hidden)", displaySsid(nullptr).c_str()));
}

HOST_TEST(wifi_ssid_up_to_limit_is_unchanged) {
    std::string twenty(MAX_SSID_BYTES, 'a');
    HT_EQ(0, std::strcmp(twenty.c_str(), displaySsid(twenty.c_str()).c_str()));
}

HOST_TEST(wifi_long_ssid_is_truncated_with_ellipsis_within_limit) {
    // 32 bytes: the longest SSID 802.11 allows
    std::string out = displaySsid("ABCDEFGHIJKLMNOPQRSTUVWXYZ012345");
    HT_EQ(0, std::strcmp("ABCDEFGHIJKLMNOPQ...", out.c_str()));
    HT_EQ(true, out.size() <= MAX_SSID_BYTES);
}

HOST_TEST(wifi_truncation_never_splits_a_utf8_character) {
    // 16 ASCII bytes, then U+00E9 (2 bytes) straddling the 17-byte cut, then more text
    std::string out = displaySsid("abcdefghijklmnop\xC3\xA9xyzxyzxyz");
    HT_EQ(0, std::strcmp("abcdefghijklmnop...", out.c_str()));
    // 3-byte CJK characters (U+4E2D): 7 of them = 21 bytes; 5 fit in 17 bytes (15)
    std::string cjk;
    for (int i = 0; i < 7; ++i) cjk += "\xE4\xB8\xAD";
    out = displaySsid(cjk.c_str());
    HT_EQ(15u + 3u, out.size());
    HT_EQ(0, std::strcmp("...", out.c_str() + 15));
}

HOST_TEST(wifi_scroll_increment_is_upstream_25_minus_ms_over_8_at_least_1) {
    HT_EQ(25, scrollIncrement(0));
    HT_EQ(24, scrollIncrement(8));
    HT_EQ(1, scrollIncrement(192));
    HT_EQ(1, scrollIncrement(5000));
    HT_EQ(1, scrollIncrement(0xFFFFFFFFu));
}

HOST_TEST(wifi_clockwise_scrolls_the_list_up_to_later_entries) {
    // Review Focus 3: clockwise = next item = content moves up = offset decreases
    HT_EQ(-25, scrollOffset(0, 1, 0));
    HT_EQ(10 + 2, scrollOffset(10, -2, 1000));
    HT_EQ(0, scrollOffset(0, 0, 0));
}

HOST_TEST(wifi_ssid_control_characters_are_replaced) {
    // an AP can broadcast any 32 bytes: a newline would split the log line and the list row
    HT_EQ(0, std::strcmp("evil?0. (-1) fake", sanitizeSsid("evil\n0. (-1) fake").c_str()));
    HT_EQ(0, std::strcmp("a?b?c?", sanitizeSsid("a\x1b" "b\x7f" "c\r").c_str()));
    HT_EQ(0, std::strcmp("caf\xC3\xA9", sanitizeSsid("caf\xC3\xA9").c_str()));  // UTF-8 kept
    HT_EQ(0, std::strcmp("", sanitizeSsid(nullptr).c_str()));
}

HOST_TEST(wifi_list_line_never_contains_an_extra_newline) {
    std::string line = formatLine(0, -50, "two\nrows");
    HT_EQ(0, std::strcmp("0. (-50) two?rows\n", line.c_str()));
}
