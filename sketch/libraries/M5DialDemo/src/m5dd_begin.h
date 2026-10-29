/**
 * @file m5dd_begin.h
 * @brief Common bootstrap of every M5DialDemo sketch and the standalone app runner.
 */
#pragma once
#include <M5Dial.h>

#include "app_context.h"
#include "mooncake/app.h"

/** M5DialDemo library functions. */
namespace m5dd {

/**
 * Brings up the M5Dial the same way in every sketch.
 *
 * Calls `M5Dial.begin(M5.config(), false, false)` (M5Dial's polled encoder off), then
 * `dialEncoder.begin()`; sets the serial log level to INFO and logs `boot: <sketchName>`;
 * probes the RFID reader (VersionReg) and initialises it only if present, logging
 * `init: rfid ok|missing`; creates `canvas` as a 240x240 16 bpp sprite. With -DM5DD_SIM it
 * first applies the virtual-M5Dial workaround (see m5dd_begin.cpp).
 *
 * @param sketchName name logged in the boot line (tests match on it)
 * @param canvas sprite to allocate (usually `M5Canvas canvas(&M5Dial.Display)`)
 */
void begin(const char* sketchName, M5Canvas& canvas);

/** @return true if begin() found the RFID reader and initialised it (M5Dial.Rfid usable). */
bool rfidPresent();

/**
 * Runs one app on its own, as a standalone sketch: setUserData(&ctx), onSetup(), onCreate(),
 * then forever M5Dial.update() + onRunning(). When the app asks to close or be destroyed,
 * calls onDestroy(), logs `app: restart` and restarts the chip. Never returns.
 */
[[noreturn]] void runStandalone(MOONCAKE::APP_BASE& app, AppContext& ctx);

}  // namespace m5dd
