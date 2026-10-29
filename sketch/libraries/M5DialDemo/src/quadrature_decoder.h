/**
 * @file quadrature_decoder.h
 * @brief Arduino-free x4 quadrature decoder with the exact state table of PJRC's Encoder
 *        library (as shipped in M5Dial 1.0.3 `utility/Encoder.h`, Copyright (c) 2011,2013
 *        PJRC.COM, LLC - Paul Stoffregen, MIT licence).
 */
#pragma once
#include <stdint.h>

/**
 * Turns successive A/B pin samples into count deltas.
 *
 * `ab` = (pinA << 1) | pinB, where pin A is PJRC's pin1 and pin B its pin2. Each call to
 * update() returns +-1 per valid edge, +-2 for a skipped state (PJRC "assume pin1 edges
 * only"), 0 for no change - so DialEncoder counts exactly like M5Dial.Encoder would if its
 * interrupts attached. Not thread-safe; the caller serialises access (DialEncoder's ISR).
 */
class QuadratureDecoder {
   public:
    /** Sets the reference state (the current pin levels) without counting. */
    void reset(uint8_t ab) { _state = toPjrc(ab); }

    /**
     * Feeds the current pin levels.
     * @param ab (pinA << 1) | pinB
     * @return count delta for the transition from the previous state
     */
    int8_t update(uint8_t ab) {
        uint8_t now = toPjrc(ab);
        // why: same index as PJRC's update(): old pin1/pin2 in bits 0/1, new in bits 2/3
        uint8_t s = (uint8_t)(_state | (now << 2));
        _state = now;
        return delta(s);
    }

   private:
    /** Converts (A<<1)|B into PJRC's 2-bit state pin1 | (pin2 << 1). */
    static uint8_t toPjrc(uint8_t ab) { return (uint8_t)(((ab >> 1) & 1) | ((ab & 1) << 1)); }

    /**
     * PJRC's transition table ("new pin2, new pin1, old pin2, old pin1 -> Result").
     * why: the same switch as PJRC's update(), case for case, so it can be checked against
     * the reference by eye (and test_quadrature_decoder.cpp checks all 16 transitions).
     */
    static int8_t delta(uint8_t s) {
        switch (s) {
            case 1: case 7: case 8: case 14: return 1;
            case 2: case 4: case 11: case 13: return -1;
            case 3: case 12: return 2;
            case 6: case 9: return -2;
            default: return 0;  // 0, 5, 10, 15: no movement
        }
    }

    uint8_t _state = 0;  ///< previous state, PJRC encoding
};
