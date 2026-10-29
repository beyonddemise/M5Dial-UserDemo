// SPDX-FileCopyrightText: © 2026 Stephan Wissel
// SPDX-License-Identifier: MIT
//
// Host-side unit test for pcf8563.chip.c: the chip source is compiled natively
// against a stubbed Wokwi API (I2C device, timers, simulation clock) and driven
// with the register accesses a host driver (e.g. M5Unified's PCF8563_Class) makes.
// Run: make test

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../pcf8563.chip.c"

/* ---- stubbed Wokwi API ------------------------------------------------------ */
enum { P_INT, P_SCL, P_SDA, P_CLKOUT, NPINS };
static const char *pin_names[NPINS] = {"INT", "SCL", "SDA", "CLKOUT"};
static uint32_t mode[NPINS];
static i2c_config_t i2c_cfg;
static double sim_ns;
static const char *start_time = "";

#define MAX_TIMERS 4
static struct { timer_config_t cfg; bool running, repeat; double due, period; } timers[MAX_TIMERS];
static int ntimers;

pin_t pin_init(const char *name, uint32_t m) {
  for (int i = 0; i < NPINS; i++)
    if (!strcmp(name, pin_names[i])) { mode[i] = m; return i; }
  assert(!"unknown pin");
  return NO_PIN;
}
void pin_mode(pin_t p, uint32_t m) { mode[p] = m; }
uint32_t attr_init(const char *name, uint32_t def) { (void)name; return def; }
uint32_t attr_read(uint32_t v) { return v; }
string_t attr_string_init(const char *name) { (void)name; return 1; }
uint32_t string_get_length(string_t s) { (void)s; return (uint32_t)strlen(start_time); }
uint32_t string_read(string_t s, char *buf, uint32_t n) { (void)s; snprintf(buf, n, "%s", start_time); return (uint32_t)strlen(buf); }
i2c_dev_t i2c_init(const i2c_config_t *c) { i2c_cfg = *c; return 1; }
wokwi_timer_t timer_init(const timer_config_t *c) { timers[ntimers].cfg = *c; return (wokwi_timer_t)ntimers++; }
void timer_start_ns_d(const wokwi_timer_t t, double ns, bool rep) {
  timers[t].running = true; timers[t].repeat = rep; timers[t].period = ns; timers[t].due = sim_ns + ns;
}
void timer_stop(const wokwi_timer_t t) { timers[t].running = false; }
double get_sim_nanos_d(void) { return sim_ns; }

/** Advance simulation time by `ns`, firing due timers in time order. */
static void advance(double ns) {
  double end = sim_ns + ns;
  for (;;) {
    int next = -1;
    for (int i = 0; i < ntimers; i++)
      if (timers[i].running && timers[i].due <= end && (next < 0 || timers[i].due < timers[next].due)) next = i;
    if (next < 0) break;
    sim_ns = timers[next].due;
    if (timers[next].repeat) timers[next].due += timers[next].period;
    else timers[next].running = false;
    timers[next].cfg.callback(timers[next].cfg.user_data);
  }
  sim_ns = end;
}

/* ---- host driver model (one I2C transaction each) --------------------------- */
static void wr(uint8_t reg, const uint8_t *data, int n) {
  assert(i2c_cfg.connect(i2c_cfg.user_data, PCF8563_ADDR, false));
  assert(i2c_cfg.write(i2c_cfg.user_data, reg));
  for (int i = 0; i < n; i++) assert(i2c_cfg.write(i2c_cfg.user_data, data[i]));
  i2c_cfg.disconnect(i2c_cfg.user_data);
}
static void wr8(uint8_t reg, uint8_t v) { wr(reg, &v, 1); }
/** Register pointer write, repeated START, read (M5Unified readRegister). */
static void rd(uint8_t reg, uint8_t *out, int n) {
  assert(i2c_cfg.connect(i2c_cfg.user_data, PCF8563_ADDR, false));
  assert(i2c_cfg.write(i2c_cfg.user_data, reg));
  assert(i2c_cfg.connect(i2c_cfg.user_data, PCF8563_ADDR, true));
  for (int i = 0; i < n; i++) out[i] = i2c_cfg.read(i2c_cfg.user_data);
  i2c_cfg.disconnect(i2c_cfg.user_data);
}
static uint8_t rd8(uint8_t reg) { uint8_t v; rd(reg, &v, 1); return v; }

static void boot(const char *t) {
  memset(timers, 0, sizeof(timers));
  ntimers = 0;
  sim_ns = 0;
  start_time = t;
  chip_init();
}

static void test_start_time_and_ticking(void) {
  boot("2026-10-30 12:34:56");
  // M5Unified PCF8563_Class::begin (PCF8563_Class.inl:21-22)
  wr8(0x00, 0x00);
  wr8(0x00, 0x00);
  wr8(0x0E, 0x03);
  uint8_t b[7];
  rd(0x02, b, 7);
  const uint8_t want[7] = {0x56, 0x34, 0x12, 0x30, 5 /* Friday */, 0x10, 0x26};
  assert(!memcmp(b, want, 7));
  advance(1.5e9);
  assert(rd8(0x02) == 0x57);
  advance(64e9); /* 12:36:01.5 */
  rd(0x02, b, 3);
  assert(b[0] == 0x01 && b[1] == 0x36 && b[2] == 0x12);
}

static void test_set_date_time_and_century(void) {
  boot("2026-01-01 00:00:00");
  // 1999-12-31 23:59:59, weekday 5 - M5Unified setDateTime: century bit set for < 2000
  const uint8_t t[7] = {0x59, 0x59, 0x23, 0x31, 5, 0x80 | 0x12, 0x99};
  wr(0x02, t, 7);
  advance(1e9);
  uint8_t b[7];
  rd(0x02, b, 7);
  const uint8_t want[7] = {0x00, 0x00, 0x00, 0x01, 6, 0x01 /* century flips to 20xx */, 0x00};
  assert(!memcmp(b, want, 7));
}

static void test_partial_write_keeps_time(void) {
  boot("2026-10-30 12:34:56");
  const uint8_t date[4] = {0x01, 3, 0x02, 0x27}; /* 2027-02-01, date registers only */
  wr(0x05, date, 4);
  uint8_t b[7];
  rd(0x02, b, 7);
  assert(b[0] == 0x56 && b[1] == 0x34 && b[2] == 0x12 && b[3] == 0x01 && b[4] == 3 && b[5] == 0x02 && b[6] == 0x27);
}

static void test_alarm_irq(void) {
  boot("2026-10-30 12:34:58");
  const uint8_t alarm[4] = {0x35, 0x80, 0x80, 0x80}; /* minute 35, other fields disabled */
  wr(0x09, alarm, 4);
  wr8(0x01, CTRL2_AIE);
  assert(mode[P_INT] == INPUT);
  advance(1.2e9);
  assert(!(rd8(0x01) & CTRL2_AF));
  advance(1e9); /* 12:35:00 */
  assert(rd8(0x01) & CTRL2_AF);
  assert(mode[P_INT] == OUTPUT_LOW);
  wr8(0x01, CTRL2_AIE | CTRL2_AF); /* writing 1 leaves AF set */
  assert(rd8(0x01) & CTRL2_AF);
  wr8(0x01, CTRL2_AIE); /* M5Unified clearIRQ: bitOff(0x01, 0x0C) */
  assert(!(rd8(0x01) & CTRL2_AF));
  assert(mode[P_INT] == INPUT);
}

static void test_alarm_does_not_touch_latched_registers(void) {
  boot("2026-10-30 12:34:59");
  const uint8_t alarm[4] = {0x35, 0x80, 0x80, 0x80};
  wr(0x09, alarm, 4);
  wr8(0x01, CTRL2_AIE);
  // burst read 02h.. latched at 12:34:59; the alarm tick fires between two bytes
  assert(i2c_cfg.connect(i2c_cfg.user_data, PCF8563_ADDR, false));
  assert(i2c_cfg.write(i2c_cfg.user_data, 0x02));
  assert(i2c_cfg.connect(i2c_cfg.user_data, PCF8563_ADDR, true));
  uint8_t sec = i2c_cfg.read(i2c_cfg.user_data);
  advance(1.2e9);
  uint8_t min = i2c_cfg.read(i2c_cfg.user_data);
  i2c_cfg.disconnect(i2c_cfg.user_data);
  assert(sec == 0x59 && min == 0x34);
  assert(rd8(0x01) & CTRL2_AF);
}

static void test_countdown_timer_irq(void) {
  boot("2026-10-30 12:00:00");
  // M5Unified setTimerIRQ(3000): 0E = 0x82 (TE, 1 Hz), 0F = 3, 01 |= TIE
  wr8(0x0E, 0x82);
  wr8(0x0F, 3);
  wr8(0x01, CTRL2_TIE);
  advance(2.9e9);
  assert(!(rd8(0x01) & CTRL2_TF));
  advance(0.2e9);
  assert(rd8(0x01) & CTRL2_TF);
  assert(mode[P_INT] == OUTPUT_LOW);
  // M5Unified disableIRQ: alarms off, timer off, flags and enables cleared
  wr8(0x0E, 0);
  wr8(0x01, 0);
  assert(mode[P_INT] == INPUT);
  advance(10e9);
  assert(!(rd8(0x01) & CTRL2_TF));
}

static void test_stop_freezes_clock(void) {
  boot("2026-10-30 12:00:00");
  wr8(0x00, CTRL1_STOP);
  advance(5e9);
  assert(rd8(0x02) == 0x00);
  wr8(0x00, 0x00);
  advance(2e9);
  assert(rd8(0x02) == 0x02);
}

static void test_pointer_wraps(void) {
  boot("2026-10-30 12:00:00");
  uint8_t b[3];
  rd(0x0F, b, 3); /* 0Fh, then wraps to 00h, 01h */
  assert(b[1] == 0x08 && b[2] == 0x00);
}

int main(void) {
  test_start_time_and_ticking();
  test_set_date_time_and_century();
  test_partial_write_keeps_time();
  test_alarm_irq();
  test_alarm_does_not_touch_latched_registers();
  test_countdown_timer_irq();
  test_stop_freezes_clock();
  test_pointer_wraps();
  printf("all pcf8563 host tests passed\n");
  return 0;
}
