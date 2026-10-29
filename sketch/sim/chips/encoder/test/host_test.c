// SPDX-FileCopyrightText: © 2026 Stephan Wissel
// SPDX-License-Identifier: MIT
//
// Host-side unit test for encoder.chip.c: the chip source is compiled natively
// against a stubbed Wokwi API; the emitted A/B edges are decoded with a standard
// x4 quadrature state table (as in PJRC's Encoder library).
// Run: make test

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../encoder.chip.c"

/* ---- stubbed Wokwi API ------------------------------------------------------ */
enum { P_A, P_B, NPINS };
static const char *pin_names[NPINS] = {"A", "B"};
static uint32_t level[NPINS];
static float attr_f[2]; /* steps, edgeIntervalUs */
static timer_config_t tcfg[2];
static bool trunning[2];
static uint32_t tperiod[2];
static int ntimers;
static int32_t position; /* decoded x4 count, + when A leads B */
static int edges;

pin_t pin_init(const char *name, uint32_t m) {
  for (int i = 0; i < NPINS; i++)
    if (!strcmp(name, pin_names[i])) { level[i] = (m == OUTPUT_HIGH); return i; }
  assert(!"unknown pin");
  return NO_PIN;
}
/** Decode each edge: Gray-code transition (prev AB -> new AB) to +-1. */
void pin_write(pin_t p, uint32_t v) {
  uint32_t prev = (level[P_A] << 1) | level[P_B];
  level[p] = v;
  uint32_t now = (level[P_A] << 1) | level[P_B];
  static const int8_t table[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
  int8_t d = table[(prev << 2) | now];
  assert(d != 0 && "every edge must be a valid single-channel transition");
  position += d; /* with state = A<<1 | B, A leading B counts up */
  edges++;
}
uint32_t attr_init(const char *name, uint32_t def) { (void)name; (void)def; return 10; } /* debug */
uint32_t attr_read(uint32_t a) { (void)a; return 0; }
uint32_t attr_init_float(const char *name, float def) {
  int i = !strcmp(name, "steps") ? 0 : 1;
  attr_f[i] = def;
  return (uint32_t)i;
}
float attr_read_float(uint32_t a) { return attr_f[a]; }
timer_t timer_init(const timer_config_t *c) { tcfg[ntimers] = *c; return (timer_t)ntimers++; }
void timer_start(const timer_t t, uint32_t us, bool rep) { (void)rep; trunning[t] = true; tperiod[t] = us; }
void timer_start_ns_d(const timer_t t, double ns, bool rep) { (void)t; (void)ns; (void)rep; }
double get_sim_nanos_d(void) { return 0; }

enum { T_EDGE = 0, T_POLL = 1 };
static void poll(void) { tcfg[T_POLL].callback(tcfg[T_POLL].user_data); }
/** Fire the edge timer until it stops re-arming itself. */
static void run_edges(void) {
  while (trunning[T_EDGE]) {
    trunning[T_EDGE] = false;
    tcfg[T_EDGE].callback(tcfg[T_EDGE].user_data);
  }
}

static void boot(void) {
  ntimers = 0;
  position = 0;
  edges = 0;
  memset(trunning, 0, sizeof(trunning));
  chip_init();
  assert(level[P_A] == HIGH && level[P_B] == HIGH);
}

static void test_one_detent_each_way(void) {
  boot();
  attr_f[0] = 1;
  poll();
  run_edges();
  assert(edges == 4 && position == 4);
  assert(level[P_A] == HIGH && level[P_B] == HIGH);
  attr_f[0] = 0;
  poll();
  run_edges();
  assert(edges == 4);
  attr_f[0] = -1;
  poll();
  run_edges();
  assert(edges == 8 && position == 0);
}

static void test_cw_order_a_leads(void) {
  boot();
  attr_f[0] = 1;
  poll();
  tcfg[T_EDGE].callback(tcfg[T_EDGE].user_data);
  assert(level[P_A] == LOW && level[P_B] == HIGH); /* A falls first (KY-040: CLK first) */
  run_edges();
}

static void test_multi_and_interval(void) {
  boot();
  attr_f[1] = 250;
  attr_f[0] = -3;
  poll();
  assert(tperiod[T_EDGE] == 250);
  run_edges();
  assert(edges == 12 && position == -12);
}

static void test_reversal_mid_detent(void) {
  boot();
  attr_f[0] = 1;
  poll();
  tcfg[T_EDGE].callback(tcfg[T_EDGE].user_data); /* one edge into the cw detent */
  trunning[T_EDGE] = false;
  attr_f[0] = -1;
  poll();
  trunning[T_EDGE] = true;
  run_edges();
  assert(edges == 8 && position == 0); /* cw detent completed, then one ccw */
  assert(level[P_A] == HIGH && level[P_B] == HIGH);
}

int main(void) {
  test_one_detent_each_way();
  test_cw_order_a_leads();
  test_multi_and_interval();
  test_reversal_mid_detent();
  printf("all encoder host tests passed\n");
  return 0;
}
