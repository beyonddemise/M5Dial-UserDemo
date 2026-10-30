/**
 * @file test_rfid_reset.cpp
 * @brief rfidSoftReset(): issues SoftReset, waits the 50 ms oscillator start-up, then polls the
 *        CommandReg PowerDown bit, giving up after the timeout instead of spinning forever
 *        like M5Dial 1.0.3's MFRC522::PCD_Reset().
 */
#include "rfid_reset.h"
#include "host_test.h"

#include <vector>

namespace {

/** Fake reader + clock: CommandReg reports PowerDown for the first `busyReads` reads. */
struct Fake {
    explicit Fake(int busy) : busyReads(busy) {}
    int busyReads;
    uint32_t now = 0;
    int reads = 0;
    std::vector<std::pair<uint8_t, uint8_t>> writes;

    bool run(uint32_t timeoutMs) {
        return rfidSoftReset(
            [&](uint8_t reg, uint8_t v) { writes.push_back({reg, v}); },
            [&](uint8_t) -> uint8_t { return reads++ < busyReads ? 0x30 : 0x20; },
            [&]() { return now; }, [&](uint32_t ms) { now += ms; }, timeoutMs);
    }
};

}  // namespace

HOST_TEST(rfid_reset_issues_soft_reset_then_waits_50ms) {
    Fake f(0);
    HT_EQ(true, f.run(200));
    HT_EQ(1, (int)f.writes.size());
    HT_EQ(RFID_REG_COMMAND, f.writes[0].first);
    HT_EQ(RFID_CMD_SOFT_RESET, f.writes[0].second);
    HT_EQ(true, f.now >= 50u);
}

HOST_TEST(rfid_reset_succeeds_once_power_down_clears) {
    Fake f(3);
    HT_EQ(true, f.run(200));
    HT_EQ(4, f.reads);
}

HOST_TEST(rfid_reset_times_out_when_power_down_never_clears) {
    Fake f(1 << 30);
    HT_EQ(false, f.run(200));
    HT_EQ(true, f.now >= 200u);
    HT_EQ(true, f.now < 300u);  // bounded: gives up promptly after the timeout
}
