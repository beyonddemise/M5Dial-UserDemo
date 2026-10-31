/**
 * @file test_uid_format.cpp
 * @brief uid_format: the RFID test's card UID text (`rfid: uid=04:A1:B2:C3`), uppercase hex
 *        bytes joined by ':' for the 4/7/10-byte UIDs an MFRC522 reads.
 */
#include <cstring>

#include "apps/rfid_test/uid_format.h"
#include "host_test.h"

using namespace uid_format;

HOST_TEST(uid_four_bytes_uppercase_colon_joined) {
    const uint8_t uid[] = {0x04, 0xA1, 0xB2, 0xC3};
    HT_EQ(0, std::strcmp("04:A1:B2:C3", formatUid(uid, sizeof(uid)).c_str()));
}

HOST_TEST(uid_seven_bytes) {
    const uint8_t uid[] = {0x04, 0x5f, 0x3a, 0x9c, 0x2b, 0x81, 0xff};
    HT_EQ(0, std::strcmp("04:5F:3A:9C:2B:81:FF", formatUid(uid, sizeof(uid)).c_str()));
}

HOST_TEST(uid_leading_zero_byte_keeps_both_digits) {
    const uint8_t uid[] = {0x00, 0x0A, 0x01, 0x10};
    HT_EQ(0, std::strcmp("00:0A:01:10", formatUid(uid, sizeof(uid)).c_str()));
}

HOST_TEST(uid_empty_or_null_is_empty_string) {
    const uint8_t uid[] = {0x12};
    HT_EQ(0, std::strcmp("", formatUid(uid, 0).c_str()));
    HT_EQ(0, std::strcmp("", formatUid(nullptr, 4).c_str()));
    HT_EQ(0, std::strcmp("12", formatUid(uid, 1).c_str()));
}
