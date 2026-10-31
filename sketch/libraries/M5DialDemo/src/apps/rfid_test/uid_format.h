/**
 * @file uid_format.h
 * @brief Arduino-free text form of an RFID card UID, as the RFID test logs and draws it.
 *
 * Kept free of Arduino/M5 includes so it can be unit-tested on the host
 * (test/host/test_uid_format.cpp). Replaces the original app's decimal 64-bit serial number
 * (gui_rfid_test.cpp `SN: %llu`, Forairaaaaa, M5Stack M5Dial-UserDemo, MIT) with the UID bytes
 * the MFRC522 API returns (`M5Dial.Rfid.uid.uidByte` / `.size`).
 */
#pragma once
#include <stdint.h>

#include <string>

/** RFID test UID text. */
namespace uid_format {

/**
 * Formats a card UID as uppercase hex bytes joined by ':' (e.g. "04:A1:B2:C3"), the form of the
 * `rfid: uid=` log line (same as DialProbe's).
 * @param bytes UID bytes, most significant (first transmitted) first; may be nullptr if len is 0
 * @param len number of bytes (4, 7 or 10 for ISO 14443-3 type A cards)
 * @return the text; empty for len 0 or a null pointer
 */
inline std::string formatUid(const uint8_t* bytes, uint8_t len) {
    static const char HEX_DIGITS[] = "0123456789ABCDEF";
    std::string out;
    if (bytes == nullptr) return out;
    out.reserve(len * 3u);
    for (uint8_t i = 0; i < len; ++i) {
        if (i) out += ':';
        out += HEX_DIGITS[bytes[i] >> 4];
        out += HEX_DIGITS[bytes[i] & 0x0F];
    }
    return out;
}

}  // namespace uid_format
