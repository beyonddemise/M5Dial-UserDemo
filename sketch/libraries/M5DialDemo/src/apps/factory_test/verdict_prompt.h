/**
 * @file verdict_prompt.h
 * @brief The factory test's operator verdict screen: PASS / FAIL / SKIP for one step.
 *
 * New in the M5DialDemo port (the original factory test recorded no verdicts); highlight
 * logic in verdict_choice.h (host-tested).
 */
#pragma once
#include <M5GFX.h>

#include "report.h"

namespace factory_test {

/**
 * Asks the operator for the verdict of one step and blocks until it is confirmed.
 *
 * Draws a round screen "<stepName>?" with the options PASS, FAIL, SKIP (top to bottom); the
 * knob moves the highlight (DialEncoder + EncoderDetent, clockwise = next, with the original
 * knob click), a button press (M5Dial.BtnA.wasPressed(), via m5dd::buttonClicked()) confirms
 * the highlighted option once the button is released again. Calls M5Dial.update() every
 * iteration (Review Focus 1).
 *
 * Log lines: `factory: prompt <step>` when the screen is up (scenarios wait for it before
 * turning the knob), `factory: verdict <step>=<PASS|FAIL|SKIP>` on confirm.
 *
 * @param canvas full-screen canvas to draw on (font, text size and datum are restored)
 * @param stepName step name as shown and logged, e.g. "LCD"
 * @param defaultVerdict initially highlighted option (Pass, Fail or Skip; Pending -> Pass)
 * @return the confirmed verdict (never Pending)
 */
Verdict promptVerdict(M5Canvas& canvas, const char* stepName,
                      Verdict defaultVerdict = Verdict::Pass);

}  // namespace factory_test
