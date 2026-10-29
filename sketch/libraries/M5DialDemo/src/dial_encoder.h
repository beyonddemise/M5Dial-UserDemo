/**
 * @file dial_encoder.h
 * @brief Interrupt-driven counter for the M5Dial's rotary encoder, with the API of M5Dial's
 *        `ENCODER` (begin/read/readAndReset/write).
 */
#pragma once
#include <Arduino.h>
#include <M5Dial.h>

#include "quadrature_decoder.h"

/**
 * Counts encoder edges in a GPIO interrupt, exactly like M5Dial.Encoder would if its
 * interrupts attached (same PJRC state table via QuadratureDecoder, x4, same sign).
 *
 * why: M5Dial 1.0.3's utility/Encoder.h defines CORE_INTxx_PIN only up to GPIO39
 * (CORE_NUM_INTERRUPT 40), so on the Dial's GPIO40/41 no interrupt attaches and edges are
 * only sampled inside read() - a 20 ms canvas push loses them (m5stack/M5Dial#8, D12).
 *
 * Use the global `dialEncoder`; m5dd::begin() calls begin(). read() etc. are safe from any
 * task; the position is guarded by a portMUX critical section shared with the ISR.
 */
class DialEncoder {
   public:
    /**
     * Configures both pins as INPUT_PULLUP, samples the rest state and attaches a CHANGE
     * interrupt to each pin. Resets the position to 0.
     * @param pinA PJRC pin1 (M5Dial: GPIO41)
     * @param pinB PJRC pin2 (M5Dial: GPIO40)
     */
    void begin(uint8_t pinA = DIAL_ENCODER_PIN_A, uint8_t pinB = DIAL_ENCODER_PIN_B);

    /** @return raw x4 count since begin()/write()/readAndReset() */
    int32_t read();

    /** @return raw count, then sets it to 0 (atomically) */
    int32_t readAndReset();

    /** Sets the raw count. */
    void write(int32_t position);

   private:
    /** GPIO interrupt handler for both pins (IRAM_ATTR at the definition); `arg` is the DialEncoder. */
    static void onEdge(void* arg);

    /** @return current pin levels as (A << 1) | B; direct register read (ISR-safe). */
    uint8_t sample() const;

    uint8_t _pinA = 0;
    uint8_t _pinB = 0;
    bool _attached = false;  ///< begin() attached the ISRs to _pinA/_pinB
    volatile uint32_t* _regA = nullptr;
    volatile uint32_t* _regB = nullptr;
    uint32_t _maskA = 0;
    uint32_t _maskB = 0;
    QuadratureDecoder _decoder;
    int32_t _position = 0;
    portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
};

/** The M5Dial's encoder (GPIO41/40). */
extern DialEncoder dialEncoder;
