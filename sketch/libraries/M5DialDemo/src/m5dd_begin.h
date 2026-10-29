/**
 * @file m5dd_begin.h
 * @brief Common bootstrap of every M5DialDemo sketch, the standalone app runner and the small
 *        input/GUI helpers every ported app shares.
 */
#pragma once
#include <M5Dial.h>

#include "app_context.h"
#include "encoder_detent.h"
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

/**
 * Prepares an app's GUI in a standalone sketch the way the UserDemo launcher does before it
 * opens the app: canvas font GUI_FONT_CN_BIG (the original HAL set it at boot), the 42x42
 * launcher icon in `icon`, theme colour, then `getGui()->init(ctx.canvas, &icon)`.
 * Call after begin() and before runStandalone(). Does nothing for an app without a GUI.
 *
 * @param app the app whose GUI to initialise
 * @param ctx host context (its canvas must already be allocated by begin())
 * @param icon sprite to hold the app icon (kept alive by the caller, e.g. a global)
 * @param themeColor RGB888 background colour (launcher_layout::appColor())
 * @param iconPixels 42x42 RGB565 launcher icon (assets/launcher_icons/icon_*.h)
 */
void initGui(MOONCAKE::APP_BASE& app, AppContext& ctx, M5Canvas& icon, uint32_t themeColor,
             const uint16_t* iconPixels);

/**
 * Detent steps since the last call, with the original HAL's knob click (ui_tones.h): 6 kHz for
 * clockwise (next), 7 kHz for counter-clockwise, 20 ms, once per call that moved. Only apps
 * that read the knob call it, matching upstream, where the click fired from wasMoved().
 * @param detent the app's detent tracker (reset it with dialEncoder.read() in onCreate())
 * @return detent steps, clockwise positive (0 = not moved)
 */
int32_t readDetentsWithClick(EncoderDetent& detent);

/**
 * Front-button "click" as the ported apps use it to quit: true once per press, after the
 * button has been released again. Plays the original HAL's 2 kHz press tone on the press,
 * then blocks until the release, calling M5Dial.update() (Review Focus 1).
 * The host must call M5Dial.update() before (runStandalone and the launcher do).
 * A press that begins and ends inside another blocking wait of the app (one that also calls
 * M5Dial.update(), e.g. waiting for a touch release) is consumed there and not reported.
 * @return true if the button was pressed (and is released now)
 */
bool buttonClicked();

}  // namespace m5dd
