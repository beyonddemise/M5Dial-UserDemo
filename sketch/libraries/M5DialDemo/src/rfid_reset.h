/**
 * @file rfid_reset.h
 * @brief Checked, bounded MFRC522/WS1850S initialisation (Arduino-free, host-tested).
 *
 * why: M5Dial 1.0.3's MFRC522::PCD_Init() cannot report failure - PCD_Reset() polls CommandReg's
 * PowerDown bit with no timeout, and PCD_Read/WriteRegister() ignore the I2C status (a failed read
 * returns an uninitialised byte). A reader that answers VersionReg can still hang boot or be
 * reported ready while unusable (PR #30 review). These helpers take status-returning register
 * callbacks so every transfer is checked.
 */
#pragma once
#include <stdint.h>

constexpr uint8_t RFID_REG_COMMAND = 0x01;        ///< MFRC522 CommandReg
constexpr uint8_t RFID_REG_TX_CONTROL = 0x14;     ///< TxControlReg (antenna drivers TX1/TX2)
constexpr uint8_t RFID_CMD_SOFT_RESET = 0x0F;     ///< CommandReg: SoftReset
constexpr uint8_t RFID_POWER_DOWN = 1 << 4;       ///< CommandReg bit 4: soft power-down
constexpr uint8_t RFID_ANTENNA_ON = 0x03;         ///< TxControlReg Tx1RFEn | Tx2RFEn
constexpr uint32_t RFID_RESET_SETTLE_MS = 50;     ///< oscillator start-up, as PCD_Reset()
constexpr uint32_t RFID_RESET_POLL_MS = 5;        ///< PowerDown poll interval

/**
 * Soft-resets the reader and waits, at most `timeoutMs`, for it to leave power-down.
 * @param write   write(reg, value) -> bool: one register write, true if the transfer succeeded
 * @param read    read(reg, uint8_t& value) -> bool: one register read, true if it succeeded
 * @param nowMs   monotonic milliseconds
 * @param sleepMs sleepMs(ms): blocking delay
 * @return true once CommandReg's PowerDown bit reads clear; false on a failed transfer or timeout
 */
template <class Write, class Read, class Now, class Sleep>
bool rfidSoftReset(Write write, Read read, Now nowMs, Sleep sleepMs, uint32_t timeoutMs) {
    const uint32_t start = nowMs();
    if (!write(RFID_REG_COMMAND, RFID_CMD_SOFT_RESET)) return false;
    sleepMs(RFID_RESET_SETTLE_MS);
    for (;;) {
        uint8_t cmd = 0;
        if (!read(RFID_REG_COMMAND, cmd)) return false;
        if (!(cmd & RFID_POWER_DOWN)) return true;
        // why: unsigned subtraction stays correct across a millis() wrap
        if (nowMs() - start >= timeoutMs) return false;
        sleepMs(RFID_RESET_POLL_MS);
    }
}

/**
 * PCD_Init() equivalent: bounded soft reset, then the same timer/ASK/CRC setup and antenna-on
 * as M5Dial 1.0.3's MFRC522::PCD_Init() (MFRC522.cpp:197-224), with every transfer checked.
 * Parameters as rfidSoftReset().
 * @return true only if the reset and all setup transfers succeeded
 */
template <class Write, class Read, class Now, class Sleep>
bool rfidInit(Write write, Read read, Now nowMs, Sleep sleepMs, uint32_t timeoutMs) {
    static constexpr uint8_t SETUP[][2] = {
        {0x2A, 0x80},  // TModeReg: TAuto, timer starts after each transmission
        {0x2B, 0xA9},  // TPrescalerReg: 40 kHz timer (25 us period)
        {0x2C, 0x03},  // TReloadRegH/L: reload 1000 = 25 ms timeout
        {0x2D, 0xE8},
        {0x15, 0x40},  // TxASKReg: force 100 % ASK modulation
        {0x11, 0x3D},  // ModeReg: CRC preset 6363h (ISO 14443-3)
    };
    if (!rfidSoftReset(write, read, nowMs, sleepMs, timeoutMs)) return false;
    for (const auto& rv : SETUP) {
        if (!write(rv[0], rv[1])) return false;
    }
    uint8_t tx = 0;
    if (!read(RFID_REG_TX_CONTROL, tx)) return false;
    // why: unlike PCD_AntennaOn() always write, so the final transfer is checked too
    return write(RFID_REG_TX_CONTROL, tx | RFID_ANTENNA_ON);
}
