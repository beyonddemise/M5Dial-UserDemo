/**
 * @file dial_encoder.cpp
 * @brief DialEncoder implementation (see dial_encoder.h for why it exists).
 */
#include "dial_encoder.h"

DialEncoder dialEncoder;

void DialEncoder::begin(uint8_t pinA, uint8_t pinB) {
    // why: begin() may run again (other pins, or a second m5dd::begin); detach only what
    // this instance attached - detaching before the GPIO ISR service exists logs an error
    if (_attached) {
        detachInterrupt(_pinA);
        detachInterrupt(_pinB);
    }
    _pinA = pinA;
    _pinB = pinB;
    pinMode(pinA, INPUT_PULLUP);
    pinMode(pinB, INPUT_PULLUP);
    // why: same register/bitmask access as PJRC's DIRECT_PIN_READ - no driver call in the ISR
    _regA = portInputRegister(digitalPinToPort(pinA));
    _regB = portInputRegister(digitalPinToPort(pinB));
    _maskA = digitalPinToBitMask(pinA);
    _maskB = digitalPinToBitMask(pinB);
    // why: as in PJRC's begin(), let a passive R-C filter charge through the pull-ups before
    // sampling the rest state
    delayMicroseconds(2000);
    portENTER_CRITICAL(&_mux);
    _decoder.reset(sample());
    _position = 0;
    portEXIT_CRITICAL(&_mux);
    attachInterruptArg(pinA, onEdge, this, CHANGE);
    attachInterruptArg(pinB, onEdge, this, CHANGE);
    _attached = true;
}

int32_t DialEncoder::read() {
    portENTER_CRITICAL(&_mux);
    int32_t p = _position;
    portEXIT_CRITICAL(&_mux);
    return p;
}

int32_t DialEncoder::readAndReset() {
    portENTER_CRITICAL(&_mux);
    int32_t p = _position;
    _position = 0;
    portEXIT_CRITICAL(&_mux);
    return p;
}

void DialEncoder::write(int32_t position) {
    portENTER_CRITICAL(&_mux);
    _position = position;
    portEXIT_CRITICAL(&_mux);
}

uint8_t IRAM_ATTR DialEncoder::sample() const {
    return (uint8_t)((((*_regA & _maskA) ? 1 : 0) << 1) | ((*_regB & _maskB) ? 1 : 0));
}

void IRAM_ATTR DialEncoder::onEdge(void* arg) {
    auto* self = static_cast<DialEncoder*>(arg);
    // why: both pins are sampled inside the lock, so an edge on the other pin that fires
    // its own ISR right after is decoded against this state, never against a stale one
    portENTER_CRITICAL_ISR(&self->_mux);
    self->_position += self->_decoder.update(self->sample());
    portEXIT_CRITICAL_ISR(&self->_mux);
}
