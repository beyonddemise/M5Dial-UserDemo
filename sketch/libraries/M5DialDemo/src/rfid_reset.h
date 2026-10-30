/**
 * @file rfid_reset.h
 * @brief Bounded MFRC522/WS1850S soft reset (Arduino-free, host-tested).
 *
 * why: M5Dial 1.0.3's MFRC522::PCD_Reset() polls CommandReg's PowerDown bit with no timeout,
 * so a reader that answers VersionReg but never leaves power-down hangs boot (PR #30 review).
 */
#pragma once
#include <stdint.h>

constexpr uint8_t RFID_REG_COMMAND = 0x01;        ///< MFRC522 CommandReg
constexpr uint8_t RFID_CMD_SOFT_RESET = 0x0F;     ///< CommandReg: SoftReset
constexpr uint8_t RFID_POWER_DOWN = 1 << 4;       ///< CommandReg bit 4: soft power-down
constexpr uint32_t RFID_RESET_SETTLE_MS = 50;     ///< oscillator start-up, as PCD_Reset()
constexpr uint32_t RFID_RESET_POLL_MS = 5;        ///< PowerDown poll interval

/**
 * Soft-resets the reader and waits, at most `timeoutMs`, for it to leave power-down.
 * @param write   write(reg, value): one register write
 * @param read    read(reg) -> value: one register read
 * @param nowMs   monotonic milliseconds
 * @param sleepMs sleepMs(ms): blocking delay
 * @return true once CommandReg's PowerDown bit reads clear; false on timeout
 */
template <class Write, class Read, class Now, class Sleep>
bool rfidSoftReset(Write write, Read read, Now nowMs, Sleep sleepMs, uint32_t timeoutMs) {
    const uint32_t start = nowMs();
    write(RFID_REG_COMMAND, RFID_CMD_SOFT_RESET);
    sleepMs(RFID_RESET_SETTLE_MS);
    for (;;) {
        if (!(read(RFID_REG_COMMAND) & RFID_POWER_DOWN)) return true;
        // why: unsigned subtraction stays correct across a millis() wrap
        if (nowMs() - start >= timeoutMs) return false;
        sleepMs(RFID_RESET_POLL_MS);
    }
}
