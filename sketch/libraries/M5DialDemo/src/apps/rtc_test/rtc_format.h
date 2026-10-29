/**
 * @file rtc_format.h
 * @brief Arduino-free text formatting and refresh tick of the RTC test app.
 *
 * Kept free of Arduino/M5 includes so it can be unit-tested on the host
 * (test/host/test_rtc_format.cpp). The clock format and the 1 s tick come from the original
 * app_rtc_test.cpp / gui_rtc_test.cpp (Forairaaaaa, M5Stack M5Dial-UserDemo, MIT).
 */
#pragma once
#include <stdint.h>
#include <stdio.h>

/** RTC test text formats. */
namespace rtc_format {

// why larger than "YYYY-MM-DD HH:MM:SS" (20) / "HH:MM:SS" (9): room for any value of the
// rtc_datetime_t field types (int16_t year, int8_t fields, uint8_t clock), so a garbage RTC
// register cannot truncate the text and gcc's -Wformat-truncation stays quiet.
constexpr size_t DATE_TIME_LEN = 32;  ///< fits "YYYY-MM-DD HH:MM:SS" for any int16/int8 input
constexpr size_t CLOCK_LEN = 12;      ///< fits "HH:MM:SS" for any uint8_t input
constexpr uint32_t REFRESH_MS = 1000; ///< original refresh period

/**
 * Writes "YYYY-MM-DD HH:MM:SS" (the `rtc:` log line, same form as DialProbe's), truncated to
 * `size` (always NUL-terminated when size > 0).
 */
inline void formatDateTime(char* buf, size_t size, int year, int month, int day, int hour,
                           int minute, int second) {
    snprintf(buf, size, "%04d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, minute,
             second);
}

/** Writes "HH:MM:SS", the clock drawn on screen (original gui_rtc_test.cpp format). */
inline void formatClock(char* buf, size_t size, int hour, int minute, int second) {
    snprintf(buf, size, "%02d:%02d:%02d", hour, minute, second);
}

/**
 * @return true once more than `periodMs` passed since `lastMs` (original
 * `(millis() - time_count) > 1000`); unsigned subtraction keeps it right across the
 * millis() wrap.
 */
inline bool due(uint32_t nowMs, uint32_t lastMs, uint32_t periodMs) {
    return nowMs - lastMs > periodMs;
}

}  // namespace rtc_format
