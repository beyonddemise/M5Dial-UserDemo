/**
 * @file app_context.h
 * @brief Shared state handed to every app via APP_BASE::setUserData().
 */
#pragma once
#include <M5GFX.h>

/**
 * What an app needs from its host (standalone sketch or the UserDemo launcher).
 * Apps read it with `static_cast<AppContext*>(getUserData())`.
 */
struct AppContext {
    M5Canvas* canvas;  ///< full-screen 240x240 16 bpp canvas, created by m5dd::begin()
};
