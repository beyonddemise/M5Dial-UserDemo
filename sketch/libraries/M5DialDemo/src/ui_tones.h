/**
 * @file ui_tones.h
 * @brief The original HAL's click tones (main/hal/hal.cpp callbacks), shared by the launcher
 *        and the m5dd app helpers so every input sounds the same everywhere.
 *
 * Arduino-free: plain constants for `M5Dial.Speaker.tone(hz, ms)`.
 */
#pragma once
#include <stdint.h>

/** Click tones of the original HAL. */
namespace ui_tones {

constexpr uint32_t TONE_MS = 20;       ///< length of every click
constexpr float TONE_PRESS_HZ = 2000;  ///< button-pressed callback
constexpr float TONE_NEXT_HZ = 6000;   ///< encoder-moved callback, count up = next / clockwise
constexpr float TONE_LAST_HZ = 7000;   ///< encoder-moved callback, count down = previous

}  // namespace ui_tones
