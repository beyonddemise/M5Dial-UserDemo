// SPDX-FileCopyrightText: © 2026 Stephan Wissel
// SPDX-License-Identifier: MIT
//
// Scriptable incremental rotary encoder - Wokwi custom chip.
//
// Outputs the two-channel quadrature signal of a mechanical detent encoder (one
// full A/B cycle per detent, both channels high at rest). Unlike the stock KY-040
// part it can be turned from automation scenarios: setting the `steps` control to
// n emits |n| detents, clockwise for n > 0 (A leads B), with a configurable time
// between edges so fast turns can be simulated.
//
// See README.md for pins, controls and limitations.

#include "wokwi-api.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define POLL_US 1000
#define MIN_EDGE_US 1

/** Complete chip state (one instance per part). */
typedef struct {
  pin_t a, b;
  uint32_t attr_steps, attr_interval;
  bool debug;
  int32_t last_steps; /* control value seen by the last poll */
  int32_t pending;    /* detents still to emit; sign = direction */
  uint8_t phase;      /* edge within the current detent, 0..3 */
  int8_t dir;         /* direction of the detent in progress: +1 cw, -1 ccw */
  bool running;
  timer_t poll, edge;
} chip_t;

/** Read a signed control value (controls are numeric; negative values need the float accessor). */
static int32_t read_int(uint32_t attr) {
  float v = attr_read_float(attr);
  return (int32_t)(v < 0 ? v - 0.5f : v + 0.5f); /* round half away from zero, no libm */
}

/** Time between two edges in microseconds (control `edgeIntervalUs`). */
static uint32_t edge_interval_us(chip_t *c) {
  int32_t v = read_int(c->attr_interval);
  return v < MIN_EDGE_US ? MIN_EDGE_US : (uint32_t)v;
}

/**
 * Emit the next edge. Clockwise (A leads B): A falls, B falls, A rises, B rises.
 * Counter-clockwise (B leads A): B falls, A falls, B rises, A rises.
 */
static void on_edge(void *user_data) {
  chip_t *c = user_data;
  if (c->phase == 0) {
    if (c->pending == 0) {
      c->running = false;
      return;
    }
    c->dir = c->pending > 0 ? 1 : -1;
  }
  bool cw = c->dir > 0;
  pin_t lead = cw ? c->a : c->b, lag = cw ? c->b : c->a;
  static const struct { bool lead_pin; uint32_t level; } seq[4] = {{true, LOW}, {false, LOW}, {true, HIGH}, {false, HIGH}};
  pin_write(seq[c->phase].lead_pin ? lead : lag, seq[c->phase].level);
  if (++c->phase == 4) {
    c->phase = 0;
    c->pending -= c->dir;
    if (c->debug) printf("encoder: detent %s, %d left\n", cw ? "cw" : "ccw", (int)c->pending);
  }
  timer_start(c->edge, edge_interval_us(c), false);
}

/** Poll the `steps` control: every change to a non-zero value queues that many detents. */
static void on_poll(void *user_data) {
  chip_t *c = user_data;
  int32_t v = read_int(c->attr_steps);
  if (v == c->last_steps) return;
  c->last_steps = v;
  if (v == 0) return; /* re-arm: lets a scenario repeat the same value */
  // why: a detent in progress finishes in its own direction before a reversal is emitted
  c->pending += v;
  if (c->debug) printf("encoder: steps %d queued\n", (int)v);
  if (!c->running) {
    c->running = true;
    timer_start(c->edge, edge_interval_us(c), false);
  }
}

/** Wokwi entry point: outputs at rest (both high), control poll timer. */
void chip_init(void) {
  chip_t *c = calloc(1, sizeof(chip_t));
  c->attr_steps = attr_init_float("steps", 0);
  c->attr_interval = attr_init_float("edgeIntervalUs", 1000);
  c->debug = attr_read(attr_init("debug", 0)) != 0;
  c->a = pin_init("A", OUTPUT_HIGH);
  c->b = pin_init("B", OUTPUT_HIGH);
  c->last_steps = read_int(c->attr_steps);

  const timer_config_t e = {.user_data = c, .callback = on_edge};
  c->edge = timer_init(&e);
  const timer_config_t p = {.user_data = c, .callback = on_poll};
  c->poll = timer_init(&p);
  timer_start(c->poll, POLL_US, true);
  printf("encoder: ready\n");
}
