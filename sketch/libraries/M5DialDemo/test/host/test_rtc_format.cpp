/**
 * @file test_rtc_format.cpp
 * @brief rtc_format: the RTC test's log line (`rtc: YYYY-MM-DD HH:MM:SS`, same form as
 *        DialProbe), its on-screen clock (original "%02d:%02d:%02d") and the 1 s refresh tick.
 */
#include <cstring>

#include "apps/rtc_test/rtc_format.h"
#include "host_test.h"

using namespace rtc_format;

HOST_TEST(rtc_date_time_is_zero_padded) {
    char buf[DATE_TIME_LEN];
    formatDateTime(buf, sizeof(buf), 2026, 1, 2, 3, 4, 5);
    HT_EQ(0, std::strcmp("2026-01-02 03:04:05", buf));
    formatDateTime(buf, sizeof(buf), 2099, 12, 31, 23, 59, 59);
    HT_EQ(0, std::strcmp("2099-12-31 23:59:59", buf));
}

HOST_TEST(rtc_date_time_truncates_to_buffer) {
    char small[5];
    formatDateTime(small, sizeof(small), 2026, 1, 2, 3, 4, 5);
    HT_EQ(0, std::strcmp("2026", small));
}

HOST_TEST(rtc_clock_is_original_format) {
    char buf[CLOCK_LEN];
    formatClock(buf, sizeof(buf), 7, 8, 9);
    HT_EQ(0, std::strcmp("07:08:09", buf));
    formatClock(buf, sizeof(buf), 23, 59, 0);
    HT_EQ(0, std::strcmp("23:59:00", buf));
}

HOST_TEST(rtc_tick_due_after_period) {
    // original: `(millis() - time_count) > 1000`
    HT_EQ(false, due(1000, 0, 1000));
    HT_EQ(true, due(1001, 0, 1000));
    HT_EQ(false, due(5500, 5000, 1000));
    HT_EQ(true, due(6001, 5000, 1000));
}

HOST_TEST(rtc_tick_survives_millis_wrap) {
    HT_EQ(false, due(100u, 0xFFFFFF00u, 1000));  // 356 ms elapsed across the wrap
    HT_EQ(true, due(1000u, 0xFFFFFF00u, 1000));  // 1256 ms elapsed
}
