/**
 * @file encoder_detent.h
 * @brief Arduino-free conversion of a raw encoder count into detent steps.
 */
#pragma once
#include <stdint.h>

#include "config.h"

/**
 * Converts the raw (x4) encoder count into whole detent steps, positive = clockwise.
 *
 * The detent index is floor(raw / countsPerDetent); update() returns how many detents the
 * index moved since the previous call (or reset), multiplied by cwSign.
 * why floor: truncating division maps raw -3..+3 to detent 0, a double-wide dead zone around
 * count 0 (Review Focus 2); floor keeps every detent exactly countsPerDetent wide.
 */
class EncoderDetent {
   public:
    /**
     * @param countsPerDetent raw counts per mechanical detent (> 0)
     * @param cwSign sign that makes a clockwise raw delta positive (+1 or -1)
     */
    explicit EncoderDetent(int countsPerDetent = M5DD_ENCODER_COUNTS_PER_DETENT,
                           int cwSign = M5DD_ENCODER_CW_SIGN)
        : _counts(countsPerDetent), _sign(cwSign) {}

    /**
     * Feeds the current raw count.
     * @return detent steps since the last update()/reset(), clockwise positive
     */
    int32_t update(int32_t rawCount) {
        int32_t idx = floorDiv(rawCount, _counts);
        int32_t steps = idx - _index;
        _index = idx;
        return steps * _sign;
    }

    /** Sets the reference raw count without producing steps (e.g. on app start). */
    void reset(int32_t rawCount) { _index = floorDiv(rawCount, _counts); }

   private:
    /** Integer division rounding towards negative infinity (C++ `/` truncates). */
    static int32_t floorDiv(int32_t a, int32_t b) {
        int32_t q = a / b;
        return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
    }

    int32_t _counts;
    int32_t _sign;
    int32_t _index = 0;  ///< detent index at the last update()/reset()
};
