/**
 * @file M5DialDemo.h
 * @brief Umbrella header of the M5DialDemo library: the shared core of the M5Dial UserDemo
 *        Arduino port (app framework, encoder input, bootstrap).
 *
 * Typical standalone sketch:
 * @code
 * #include <M5DialDemo.h>
 * M5Canvas canvas(&M5Dial.Display);
 * AppContext ctx{&canvas};
 * MyApp app;
 * void setup() { m5dd::begin("MyApp", canvas); m5dd::runStandalone(app, ctx); }
 * void loop() {}
 * @endcode
 */
#pragma once
#include <M5Dial.h>

#include "config.h"
#include "app_context.h"
#include "quadrature_decoder.h"
#include "dial_encoder.h"
#include "encoder_detent.h"
#include "m5dd_begin.h"
#include "mooncake/app.h"
#include "mooncake/gui_base.h"
