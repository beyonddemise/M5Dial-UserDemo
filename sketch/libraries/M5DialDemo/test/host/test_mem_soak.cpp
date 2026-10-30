/**
 * @file test_mem_soak.cpp
 * @brief mem_soak::Sequencer: the MemSoak cycle state machine (Wi-Fi scan until done or 20 s,
 *        BLE for 5 s, three cycles), its timeouts and the success-only verdict.
 */
#include <cstring>

#include "host_test.h"
#include "mem_soak.h"

using mem_soak::Phase;
using mem_soak::Sequencer;
using mem_soak::Step;

namespace {

/** Drives one Wi-Fi + BLE cycle that succeeds; returns the step that ended the BLE phase. */
Step run_good_cycle(Sequencer& s, uint32_t& now) {
    HT_EQ((int)Phase::Wifi, (int)s.phase());
    s.appStarted(now);
    HT_EQ((int)Step::Run, (int)s.poll(now + 100, false, false));
    now += 3000;  // scan done after 3 s
    HT_EQ((int)Step::Switch, (int)s.poll(now, true, false));
    HT_EQ((int)Phase::Ble, (int)s.phase());
    s.appStarted(now);
    HT_EQ((int)Step::Run, (int)s.poll(now + mem_soak::BLE_RUN_MS - 1, false, false));
    now += mem_soak::BLE_RUN_MS;
    return s.poll(now, false, false);
}

}  // namespace

HOST_TEST(soak_defaults_are_three_cycles_20s_wifi_timeout_5s_ble) {
    HT_EQ(3, mem_soak::CYCLES);
    HT_EQ(20000, mem_soak::WIFI_TIMEOUT_MS);
    HT_EQ(5000, mem_soak::BLE_RUN_MS);
    Sequencer s;
    HT_EQ(3, s.cycles());
    HT_EQ(0, s.cyclesDone());
    HT_EQ((int)Phase::Wifi, (int)s.phase());
    HT_EQ(false, s.finished());
    HT_EQ(false, s.succeeded());
    HT_EQ(0, std::strcmp("", s.failReason()));
}

HOST_TEST(soak_three_good_cycles_end_in_done_with_a_cycle_end_each) {
    Sequencer s;
    uint32_t now = 1000;
    HT_EQ((int)Step::CycleEnd, (int)run_good_cycle(s, now));
    HT_EQ(1, s.cyclesDone());
    HT_EQ((int)Phase::Wifi, (int)s.phase());
    HT_EQ(false, s.finished());
    HT_EQ((int)Step::CycleEnd, (int)run_good_cycle(s, now));
    HT_EQ(2, s.cyclesDone());
    HT_EQ(false, s.succeeded());
    HT_EQ((int)Step::CycleEnd, (int)run_good_cycle(s, now));
    HT_EQ(3, s.cyclesDone());
    HT_EQ((int)Phase::Done, (int)s.phase());
    HT_EQ(true, s.finished());
    HT_EQ(true, s.succeeded());
    HT_EQ((int)Step::Idle, (int)s.poll(now + 1, true, false));
}

HOST_TEST(soak_wifi_scan_with_zero_aps_counts_as_done) {
    // the driver passes scanDone() as appDone; ap_count=0 still sets it - no special case here
    Sequencer s;
    s.appStarted(0);
    HT_EQ((int)Step::Switch, (int)s.poll(1, true, false));
}

HOST_TEST(soak_wifi_timeout_fails_with_reason_and_never_succeeds) {
    Sequencer s;
    s.appStarted(500);
    HT_EQ((int)Step::Run, (int)s.poll(500 + mem_soak::WIFI_TIMEOUT_MS - 1, false, false));
    HT_EQ((int)Step::Fail, (int)s.poll(500 + mem_soak::WIFI_TIMEOUT_MS, false, false));
    HT_EQ((int)Phase::Failed, (int)s.phase());
    HT_EQ(true, s.finished());
    HT_EQ(false, s.succeeded());
    HT_EQ(0, std::strcmp("wifi timeout", s.failReason()));
    // sticky: a late scan result does not revive the soak
    HT_EQ((int)Step::Idle, (int)s.poll(500 + mem_soak::WIFI_TIMEOUT_MS + 1, true, false));
    HT_EQ(false, s.succeeded());
}

HOST_TEST(soak_wifi_timeout_in_a_later_cycle_still_fails) {
    Sequencer s;
    uint32_t now = 0;
    HT_EQ((int)Step::CycleEnd, (int)run_good_cycle(s, now));
    HT_EQ((int)Step::CycleEnd, (int)run_good_cycle(s, now));
    s.appStarted(now);
    HT_EQ((int)Step::Fail, (int)s.poll(now + mem_soak::WIFI_TIMEOUT_MS, false, false));
    HT_EQ(2, s.cyclesDone());
    HT_EQ(false, s.succeeded());
}

HOST_TEST(soak_scan_done_wins_over_timeout_in_the_same_poll) {
    Sequencer s;
    s.appStarted(0);
    HT_EQ((int)Step::Switch, (int)s.poll(mem_soak::WIFI_TIMEOUT_MS + 50, true, false));
}

HOST_TEST(soak_app_that_quits_before_its_done_condition_fails) {
    Sequencer w;
    w.appStarted(0);
    HT_EQ((int)Step::Fail, (int)w.poll(10, false, true));
    HT_EQ(0, std::strcmp("wifi quit", w.failReason()));

    Sequencer b;
    b.appStarted(0);
    HT_EQ((int)Step::Switch, (int)b.poll(10, true, false));
    b.appStarted(20);
    HT_EQ((int)Step::Fail, (int)b.poll(30, false, true));
    HT_EQ(0, std::strcmp("ble quit", b.failReason()));
    HT_EQ(false, b.succeeded());
}

HOST_TEST(soak_ble_phase_ignores_app_done_and_runs_its_full_time) {
    Sequencer s;
    s.appStarted(0);
    HT_EQ((int)Step::Switch, (int)s.poll(10, true, false));
    s.appStarted(100);
    HT_EQ((int)Step::Run, (int)s.poll(200, true, false));
    HT_EQ((int)Step::CycleEnd, (int)s.poll(100 + mem_soak::BLE_RUN_MS, true, false));
}

HOST_TEST(soak_phase_clock_starts_at_app_started_not_at_the_switch) {
    // why: creating the BLE app (NimBLE init) takes time; it must not eat into its 5 s
    Sequencer s;
    s.appStarted(0);
    HT_EQ((int)Step::Switch, (int)s.poll(10, true, false));
    s.appStarted(2000);
    HT_EQ((int)Step::Run, (int)s.poll(10 + mem_soak::BLE_RUN_MS, false, false));
    HT_EQ((int)Step::CycleEnd, (int)s.poll(2000 + mem_soak::BLE_RUN_MS, false, false));
}

HOST_TEST(soak_timing_survives_millis_wraparound) {
    Sequencer s;
    uint32_t start = 0xFFFFFF00u;
    s.appStarted(start);
    HT_EQ((int)Step::Run, (int)s.poll(start + 1000, false, false));  // wrapped past 0
    HT_EQ((int)Step::Fail, (int)s.poll(start + mem_soak::WIFI_TIMEOUT_MS, false, false));
}

HOST_TEST(soak_external_failure_is_sticky_and_keeps_the_first_reason) {
    Sequencer s;
    HT_EQ((int)Step::Fail, (int)s.fail("alloc wifi"));
    HT_EQ(0, std::strcmp("alloc wifi", s.failReason()));
    HT_EQ((int)Step::Idle, (int)s.fail("later"));
    HT_EQ(0, std::strcmp("alloc wifi", s.failReason()));
    HT_EQ((int)Step::Idle, (int)s.poll(0, true, false));
    HT_EQ(false, s.succeeded());
}

HOST_TEST(soak_fail_after_done_does_not_turn_success_into_failure) {
    Sequencer s(1, 100, 100);
    s.appStarted(0);
    HT_EQ((int)Step::Switch, (int)s.poll(1, true, false));
    s.appStarted(1);
    HT_EQ((int)Step::CycleEnd, (int)s.poll(101, false, false));
    HT_EQ(true, s.succeeded());
    HT_EQ((int)Step::Idle, (int)s.fail("late"));
    HT_EQ(true, s.succeeded());
}

HOST_TEST(soak_custom_cycle_count_and_times) {
    Sequencer s(2, 50, 30);
    HT_EQ(2, s.cycles());
    s.appStarted(0);
    HT_EQ((int)Step::Fail, (int)s.poll(50, false, false));
}
