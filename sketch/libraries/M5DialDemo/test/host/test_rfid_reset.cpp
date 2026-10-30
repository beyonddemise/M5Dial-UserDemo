/**
 * @file test_rfid_reset.cpp
 * @brief rfidSoftReset() / rfidInit(): bounded MFRC522 reset and PCD_Init()-equivalent setup that
 *        fail on any failed register transfer instead of spinning forever (PCD_Reset()) or
 *        trusting an unchecked read, as M5Dial 1.0.3's MFRC522 does.
 */
#include "rfid_reset.h"
#include "host_test.h"

#include <map>
#include <vector>

namespace {

/** Fake reader + clock: CommandReg reports PowerDown for the first `busyReads` reads. */
struct Fake {
    explicit Fake(int busy) : busyReads(busy) {}
    int busyReads;
    int failReadAt = -1;   ///< index of the register read that fails (-1 = none)
    int failWriteAt = -1;  ///< index of the register write that fails (-1 = none)
    uint32_t now = 0;
    int reads = 0;
    std::vector<std::pair<uint8_t, uint8_t>> writes;
    std::map<uint8_t, uint8_t> regs{{0x14, 0x80}};  // TxControlReg reset value

    bool write(uint8_t reg, uint8_t v) {
        if ((int)writes.size() == failWriteAt) return false;
        writes.push_back({reg, v});
        regs[reg] = v;
        return true;
    }
    bool read(uint8_t reg, uint8_t& v) {
        if (reads++ == failReadAt) return false;
        v = reg == RFID_REG_COMMAND ? (reads <= busyReads ? 0x30 : 0x20) : regs[reg];
        return true;
    }
    auto w() { return [this](uint8_t r, uint8_t v) { return write(r, v); }; }
    auto r() { return [this](uint8_t reg, uint8_t& v) { return read(reg, v); }; }
    auto clock() { return [this]() { return now; }; }
    auto sleep() { return [this](uint32_t ms) { now += ms; }; }

    bool reset(uint32_t timeoutMs) { return rfidSoftReset(w(), r(), clock(), sleep(), timeoutMs); }
    bool init() { return rfidInit(w(), r(), clock(), sleep(), 200); }
};

}  // namespace

HOST_TEST(rfid_reset_issues_soft_reset_then_waits_50ms) {
    Fake f(0);
    HT_EQ(true, f.reset(200));
    HT_EQ(1, (int)f.writes.size());
    HT_EQ(RFID_REG_COMMAND, f.writes[0].first);
    HT_EQ(RFID_CMD_SOFT_RESET, f.writes[0].second);
    HT_EQ(true, f.now >= 50u);
}

HOST_TEST(rfid_reset_succeeds_once_power_down_clears) {
    Fake f(3);
    HT_EQ(true, f.reset(200));
    HT_EQ(4, f.reads);
}

HOST_TEST(rfid_reset_times_out_when_power_down_never_clears) {
    Fake f(1 << 30);
    HT_EQ(false, f.reset(200));
    HT_EQ(true, f.now >= 200u);
    HT_EQ(true, f.now < 300u);  // bounded: gives up promptly after the timeout
}

HOST_TEST(rfid_reset_fails_on_failed_command_read) {
    Fake f(0);
    f.failReadAt = 0;  // the byte of a failed read is garbage: must not count as "powered up"
    HT_EQ(false, f.reset(200));
}

HOST_TEST(rfid_reset_fails_on_failed_reset_write) {
    Fake f(0);
    f.failWriteAt = 0;
    HT_EQ(false, f.reset(200));
}

HOST_TEST(rfid_init_writes_pcd_init_setup_and_antenna_on) {
    Fake f(0);
    HT_EQ(true, f.init());
    std::vector<std::pair<uint8_t, uint8_t>> want{
        {0x01, 0x0F}, {0x2A, 0x80}, {0x2B, 0xA9}, {0x2C, 0x03},
        {0x2D, 0xE8}, {0x15, 0x40}, {0x11, 0x3D}, {0x14, 0x83}};
    HT_EQ(true, f.writes == want);
}

HOST_TEST(rfid_init_fails_on_any_failed_setup_write) {
    for (int i = 1; i < 8; ++i) {
        Fake f(0);
        f.failWriteAt = i;
        HT_EQ(false, f.init());
    }
}

HOST_TEST(rfid_init_fails_on_failed_antenna_read) {
    Fake f(0);
    f.failReadAt = 1;  // read 0 = CommandReg, read 1 = TxControlReg
    HT_EQ(false, f.init());
}
