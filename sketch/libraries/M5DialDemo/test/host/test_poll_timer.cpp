/**
 * @file test_poll_timer.cpp
 * @brief poll_timer: the RFID test's card-poll throttle must fire on the first call at any
 *        uptime (also past 2^31 ms, ~24.9 days) and stay periodic across the millis() wrap.
 */
#include "apps/rfid_test/poll_timer.h"
#include "host_test.h"

using poll_timer::PollTimer;

HOST_TEST(poll_first_call_due_after_long_uptime) {
    PollTimer t(100);
    HT_EQ(true, t.due(0x80000010u));  // past 2^31 ms: a deadline of 0 would block here
}

HOST_TEST(poll_waits_one_period_between_polls) {
    PollTimer t(100);
    HT_EQ(true, t.due(5000));
    HT_EQ(false, t.due(5050));
    HT_EQ(false, t.due(5099));
    HT_EQ(true, t.due(5100));
}

HOST_TEST(poll_periodic_across_millis_wrap) {
    PollTimer t(100);
    HT_EQ(true, t.due(0xFFFFFFC0u));
    HT_EQ(false, t.due(0x00000010u));  // 80 ms later, after the wrap
    HT_EQ(true, t.due(0x00000024u));   // 100 ms later
}
