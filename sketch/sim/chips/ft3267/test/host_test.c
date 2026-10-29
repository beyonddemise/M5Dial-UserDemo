// SPDX-FileCopyrightText: © 2026 Stephan Wissel
// SPDX-License-Identifier: MIT
//
// Host-side unit test for ft3267.chip.c: the chip source is compiled natively
// against a stubbed Wokwi API and driven with the register accesses of a FocalTech
// driver (the sequence of M5GFX's Touch_FT5x06: init check, then point reads).
// Run: make test

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../ft3267.chip.c"

/* ---- stubbed Wokwi API ------------------------------------------------------ */
enum { P_INT, P_RST, P_SCL, P_SDA, NPINS };
static const char *pin_names[NPINS] = {"INT", "RST", "SCL", "SDA"};
static uint32_t mode[NPINS];
static pin_watch_config_t watch[NPINS];
static i2c_config_t i2c_cfg;
static timer_config_t poll_cfg;
enum { A_CHIPID = 100, A_VENDID, A_FWID, A_DEBUG, A_X, A_Y, A_TOUCHED };
static uint32_t attr_val[A_TOUCHED + 1];

pin_t pin_init(const char *name, uint32_t m) {
  for (int i = 0; i < NPINS; i++)
    if (!strcmp(name, pin_names[i])) { mode[i] = m; return i; }
  assert(!"unknown pin");
  return NO_PIN;
}
void pin_mode(pin_t p, uint32_t m) { mode[p] = m; }
bool pin_watch(pin_t p, const pin_watch_config_t *c) { watch[p] = *c; return true; }
uint32_t attr_init(const char *name, uint32_t def) {
  static const char *names[] = {"chipId", "vendorId", "firmwareId", "debug", "x", "y", "touched"};
  for (int i = 0; i < 7; i++)
    if (!strcmp(name, names[i])) { attr_val[A_CHIPID + i] = def; return A_CHIPID + i; }
  assert(!"unknown attr");
  return 0;
}
uint32_t attr_read(uint32_t a) { return attr_val[a]; }
i2c_dev_t i2c_init(const i2c_config_t *c) { i2c_cfg = *c; return 1; }
timer_t timer_init(const timer_config_t *c) { poll_cfg = *c; return 1; }
void timer_start(const timer_t t, uint32_t us, bool rep) { (void)t; (void)us; (void)rep; }
void timer_start_ns_d(const timer_t t, double ns, bool rep) { (void)t; (void)ns; (void)rep; }
double get_sim_nanos_d(void) { return 0; }

static void poll(void) { poll_cfg.callback(poll_cfg.user_data); }
static void host_drives_int_low(void) { watch[P_INT].pin_change(watch[P_INT].user_data, P_INT, LOW); }

/* ---- host driver model ------------------------------------------------------ */
static bool wr8(uint8_t reg, uint8_t v) {
  if (!i2c_cfg.connect(i2c_cfg.user_data, FT3267_ADDR, false)) return false;
  assert(i2c_cfg.write(i2c_cfg.user_data, reg));
  assert(i2c_cfg.write(i2c_cfg.user_data, v));
  i2c_cfg.disconnect(i2c_cfg.user_data);
  return true;
}
static bool rd(uint8_t reg, uint8_t *out, int n) {
  if (!i2c_cfg.connect(i2c_cfg.user_data, FT3267_ADDR, false)) return false;
  assert(i2c_cfg.write(i2c_cfg.user_data, reg));
  assert(i2c_cfg.connect(i2c_cfg.user_data, FT3267_ADDR, true));
  for (int i = 0; i < n; i++) out[i] = i2c_cfg.read(i2c_cfg.user_data);
  i2c_cfg.disconnect(i2c_cfg.user_data);
  return true;
}
/** Touch_FT5x06::_read_data: TD_STATUS, then (same read) points * 6 - 2 more bytes. */
static int read_points(uint8_t *data) {
  assert(i2c_cfg.connect(i2c_cfg.user_data, FT3267_ADDR, false));
  assert(i2c_cfg.write(i2c_cfg.user_data, REG_TD_STATUS));
  assert(i2c_cfg.connect(i2c_cfg.user_data, FT3267_ADDR, true));
  data[0] = i2c_cfg.read(i2c_cfg.user_data);
  int points = data[0] & 0x0F;
  for (int i = 1; i < points * 6 - 1; i++) data[i] = i2c_cfg.read(i2c_cfg.user_data);
  i2c_cfg.disconnect(i2c_cfg.user_data);
  return points;
}

static void boot(void) {
  memset(attr_val, 0, sizeof(attr_val));
  chip_init();
}

static void test_init_check(void) {
  boot();
  // Touch_FT5x06::_check_init (Touch_FT5x06.inl:58-61)
  uint8_t tmp[6];
  assert(wr8(REG_DEV_MODE, 0x00));
  assert(rd(REG_CIPHER, tmp, 6));
  assert(wr8(REG_G_MODE, 0x00));
  assert(tmp[0] == 0x33 && tmp[3] == 0x01 && tmp[5] == 0x11); /* CIPHER, FIRMID, VENDID */
  assert(mode[P_INT] == INPUT);
}

static void test_touch_report(void) {
  boot();
  attr_val[A_X] = 120;
  attr_val[A_Y] = 60;
  uint8_t d[30];
  assert(read_points(d) == 0);
  attr_val[A_TOUCHED] = 1;
  poll();
  assert(mode[P_INT] == OUTPUT_LOW);
  assert(read_points(d) == 1);
  assert((d[1] & 0xC0) == EVENT_PRESS_DOWN);
  assert((((d[1] & 0x0F) << 8) | d[2]) == 120);
  assert((((d[3] & 0x0F) << 8) | d[4]) == 60);
  assert((d[3] >> 4) == 0); /* touch id */
  assert(read_points(d) == 1);
  assert((d[1] & 0xC0) == EVENT_CONTACT);
  attr_val[A_TOUCHED] = 0;
  poll();
  assert(mode[P_INT] == INPUT);
  assert(read_points(d) == 0);
  uint8_t xh;
  rd(REG_P1_XH, &xh, 1);
  assert((xh & 0xC0) == EVENT_LIFT_UP);
}

static void test_hibernate_and_wake(void) {
  boot();
  assert(wr8(REG_PWR_MODE, PWR_HIBERNATE));
  uint8_t v;
  assert(!rd(REG_FOCALTECH_ID, &v, 1)); /* NACK while hibernating */
  host_drives_int_low();               /* Touch_FT5x06::wakeup: INT pulled low */
  assert(rd(REG_FOCALTECH_ID, &v, 1) && v == 0x11);
}

static void test_id_registers_read_only(void) {
  boot();
  assert(wr8(REG_CIPHER, 0x55));
  uint8_t v;
  rd(REG_CIPHER, &v, 1);
  assert(v == 0x33);
}

int main(void) {
  test_init_check();
  test_touch_report();
  test_hibernate_and_wake();
  test_id_registers_read_only();
  printf("all ft3267 host tests passed\n");
  return 0;
}
