/**
 * @file verdict_choice.h
 * @brief Arduino-free highlight logic of the factory test's PASS/FAIL/SKIP verdict prompt.
 *
 * New in the M5DialDemo port (the original had no verdicts). Kept free of Arduino/M5 includes
 * so it can be unit-tested on the host (test/host/test_verdict_choice.cpp); verdict_prompt.cpp
 * draws and reads the knob/button around it.
 */
#pragma once
#include <stdint.h>

#include "report.h"

/** Verdict prompt highlight. */
namespace verdict_choice {

constexpr int COUNT = 3;  ///< offered options: PASS, FAIL, SKIP (in this order)

/** @return `index` limited to 0..COUNT-1. */
inline int clampIndex(int index) { return index < 0 ? 0 : (index >= COUNT ? COUNT - 1 : index); }

/** @return the verdict of option `index` (clamped): 0 PASS, 1 FAIL, 2 SKIP. */
inline factory_test::Verdict at(int index) {
    static const factory_test::Verdict ORDER[COUNT] = {
        factory_test::Verdict::Pass, factory_test::Verdict::Fail, factory_test::Verdict::Skip};
    return ORDER[clampIndex(index)];
}

/** @return the option index of `v`; Pending (not offered) falls back to 0 = PASS. */
inline int indexOf(factory_test::Verdict v) {
    for (int i = 0; i < COUNT; ++i)
        if (at(i) == v) return i;
    return 0;
}

/**
 * Moves the highlight by knob detents, clockwise positive = next option (Review Focus 3).
 * why clamped, not wrapping: an overshooting turn past SKIP must not land on PASS - the
 * operator would confirm the opposite of what they aimed for.
 * @return the new index, 0..COUNT-1
 */
inline int move(int index, int32_t detents) {
    // why int64: a huge detent count must not overflow before the clamp
    int64_t next = static_cast<int64_t>(clampIndex(index)) + detents;
    return next < 0 ? 0 : (next >= COUNT ? COUNT - 1 : static_cast<int>(next));
}

}  // namespace verdict_choice
