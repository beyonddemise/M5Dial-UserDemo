/**
 * @file wifi_list.h
 * @brief Arduino-free parts of the Wi-Fi scan app: the list text it draws, the 10-entry cap,
 *        SSID truncation, and the knob scroll acceleration.
 *
 * Kept free of Arduino/M5 includes so it can be unit-tested on the host
 * (test/host/test_wifi_list.cpp). Extracted from the original app_wifi_scan.cpp onRunning() and
 * wifi_common_test.cpp SCAN::scan() (Forairaaaaa, M5Stack M5Dial-UserDemo, MIT).
 */
#pragma once
#include <stddef.h>
#include <stdint.h>

#include <string>

/** Wi-Fi scan list text and scrolling. */
namespace wifi_list {

constexpr int MAX_LISTED = 10;         ///< original DEFAULT_SCAN_LIST_SIZE
constexpr size_t MAX_SSID_BYTES = 20;  ///< drawn SSID length limit, ellipsis included
constexpr int MAX_SCROLL_INCREMENT = 25;  ///< original `25 - delta_time / 8` at delta 0
constexpr uint32_t MS_PER_INCREMENT = 8;  ///< every 8 ms between detents costs one pixel

/**
 * @param scanResult WiFi.scanComplete() value: AP count, or WIFI_SCAN_RUNNING (-1) /
 *        WIFI_SCAN_FAILED (-2)
 * @return number of entries to list: 0 for an error, at most MAX_LISTED
 */
inline int listedCount(int scanResult) {
    if (scanResult < 0) return 0;
    return scanResult > MAX_LISTED ? MAX_LISTED : scanResult;
}

/**
 * SSID as the list draws it: "(hidden)" for an empty one; one longer than MAX_SSID_BYTES is
 * cut and ends in "...", so the line fits the round panel in efontCN_16_b.
 * why bytes, cut on a character boundary: SSIDs are up to 32 bytes of (usually) UTF-8; cutting
 * inside a multi-byte sequence would draw a broken glyph.
 * @param ssid NUL-terminated SSID; may be nullptr
 */
inline std::string displaySsid(const char* ssid) {
    if (ssid == nullptr || ssid[0] == '\0') return "(hidden)";
    std::string s(ssid);
    if (s.size() <= MAX_SSID_BYTES) return s;
    static const char ELLIPSIS[] = "...";
    size_t cut = MAX_SSID_BYTES - (sizeof(ELLIPSIS) - 1);
    // why: 10xxxxxx is a UTF-8 continuation byte; back up to the start of its character
    while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80) --cut;
    return s.substr(0, cut) + ELLIPSIS;
}

/**
 * One list line, as the original builds it: `<index>. (<rssi>) <ssid>\n`.
 * @param index 0-based position in the scan result
 * @param rssi signal strength in dBm
 * @param ssid raw SSID (displaySsid() is applied)
 */
inline std::string formatLine(int index, int rssi, const char* ssid) {
    return std::to_string(index) + ". (" + std::to_string(rssi) + ") " + displaySsid(ssid) +
           "\n";
}

/**
 * Pixels the list moves per detent, from the time since the previous detent: the original
 * `y = -(1/8) x + 25`, at least 1 - a fast turn moves 25 px per detent, a slow one 1 px.
 * why the explicit compare: the original subtracted in int; with an unsigned time the
 * subtraction must not wrap.
 */
inline int scrollIncrement(uint32_t msSinceLastDetent) {
    uint32_t slowdown = msSinceLastDetent / MS_PER_INCREMENT;
    if (slowdown >= static_cast<uint32_t>(MAX_SCROLL_INCREMENT)) return 1;
    return MAX_SCROLL_INCREMENT - static_cast<int>(slowdown);  // 1..25
}

/**
 * @return the list's y offset moved by `detents`; clockwise (positive) scrolls towards later
 * entries, so the content moves up and the offset decreases (Review Focus 3).
 * Unbounded, as upstream.
 */
inline int scrollOffset(int offset, int32_t detents, uint32_t msSinceLastDetent) {
    return offset - static_cast<int>(detents) * scrollIncrement(msSinceLastDetent);
}

}  // namespace wifi_list
