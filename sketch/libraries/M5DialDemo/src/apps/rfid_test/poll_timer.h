/**
 * @file poll_timer.h
 * @brief Arduino-free throttle for the RFID test's card poll (test/host/test_poll_timer.cpp).
 */
#pragma once
#include <stdint.h>

/** RFID test poll throttle. */
namespace poll_timer {

/** Says when a periodic poll is due, given the current millis(). */
class PollTimer {
public:
    /** @param period_ms minimum time between two due() == true */
    explicit PollTimer(uint32_t period_ms) : _period_ms(period_ms) {}

    /**
     * @param now_ms current millis()
     * @return true on the first call and then once at least period_ms after the last true
     */
    bool due(uint32_t now_ms) {
        // why elapsed time since the last poll, not a deadline: unsigned subtraction is correct
        // at any uptime and across the millis() wrap; a zero-initialised signed deadline blocked
        // every poll after 2^31 ms (~24.9 days) of uptime
        if (_polled && now_ms - _last_ms < _period_ms) return false;
        _polled = true;
        _last_ms = now_ms;
        return true;
    }

private:
    uint32_t _period_ms;
    uint32_t _last_ms = 0;
    bool _polled = false;  ///< false until the first poll, so it is never delayed
};

}  // namespace poll_timer
