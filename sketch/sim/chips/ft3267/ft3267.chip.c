// SPDX-FileCopyrightText: © 2026 Stephan Wissel
// SPDX-License-Identifier: MIT
//
// FT3267 capacitive touch controller - Wokwi custom chip.
//
// Models the FocalTech FT3267 / FT6x36 family I2C register map: one touch point
// (TD_STATUS, P1_XH..P1_MISC), identification registers (CIPHER, FIRMID,
// FOCALTECH_ID), the interrupt mode / power mode registers and the INT output.
// The touch itself comes from the part's controls `x`, `y` and `touched`.
//
// See README.md for the register model and limitations.

#include "wokwi-api.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FT3267_ADDR 0x38

/* --- registers (FT6x36 / FT3267 application note) ----------------------------- */
#define REG_DEV_MODE 0x00
#define REG_GEST_ID 0x01
#define REG_TD_STATUS 0x02
#define REG_P1_XH 0x03 /* [7:6] event flag, [3:0] X[11:8] */
#define REG_P1_XL 0x04
#define REG_P1_YH 0x05 /* [7:4] touch id, [3:0] Y[11:8] */
#define REG_P1_YL 0x06
#define REG_P1_WEIGHT 0x07
#define REG_P1_MISC 0x08
#define REG_P2_XH 0x09
#define REG_P2_LAST 0x0E
#define REG_TH_GROUP 0x80
#define REG_PERIODACTIVE 0x88
#define REG_LIB_VER_H 0xA1
#define REG_LIB_VER_L 0xA2
#define REG_CIPHER 0xA3
#define REG_G_MODE 0xA4 /* 0 = INT low while touched (polling), 1 = trigger */
#define REG_PWR_MODE 0xA5
#define REG_FIRMID 0xA6
#define REG_FOCALTECH_ID 0xA8
#define REG_RELEASE_CODE_ID 0xAF
#define REG_STATE 0xBC

#define EVENT_PRESS_DOWN 0x00
#define EVENT_LIFT_UP 0x40
#define EVENT_CONTACT 0x80
#define EVENT_NONE 0xC0

#define PWR_HIBERNATE 0x03
#define POLL_US 5000 /* control polling / report period */

/** Complete chip state (one instance per part). */
typedef struct {
  pin_t intn, rst;
  uint8_t regs[256];
  uint32_t attr_x, attr_y, attr_touched;
  uint8_t chip_id, vendor_id, firmware_id;
  bool debug;

  bool touched;       /* touch state seen by the last poll */
  bool down_reported; /* the press-down event has been read out */
  bool hibernating;
  uint16_t last_x, last_y;

  uint8_t ptr;
  bool ptr_pending;
  timer_t poll;
} chip_t;

/** Register values after power-on / reset. */
static void reset_registers(chip_t *c) {
  memset(c->regs, 0, sizeof(c->regs));
  for (int r = REG_P1_XH; r <= REG_P2_LAST; r++) c->regs[r] = 0xFF;
  c->regs[REG_TH_GROUP] = 0x16;
  c->regs[REG_PERIODACTIVE] = 0x0E;
  c->regs[REG_LIB_VER_H] = 0x30;
  c->regs[REG_LIB_VER_L] = 0x03;
  c->regs[REG_CIPHER] = c->chip_id;
  c->regs[REG_G_MODE] = 0x01;
  c->regs[REG_FIRMID] = c->firmware_id;
  c->regs[REG_FOCALTECH_ID] = c->vendor_id;
  c->regs[REG_RELEASE_CODE_ID] = 0x01;
  c->regs[REG_STATE] = 0x01; /* working */
  c->down_reported = false;
  c->hibernating = false;
}

/** Clamp a control value to the 12-bit coordinate range. */
static uint16_t coord(uint32_t attr) {
  uint32_t v = attr_read(attr);
  return (uint16_t)(v > 0x0FFF ? 0x0FFF : v);
}

/**
 * Refresh the touch data registers from the controls (once per transaction, so a
 * multi-byte read sees one consistent report).
 */
static void latch_report(chip_t *c) {
  uint8_t *r = c->regs;
  if (c->touched) {
    c->last_x = coord(c->attr_x);
    c->last_y = coord(c->attr_y);
    uint8_t event = c->down_reported ? EVENT_CONTACT : EVENT_PRESS_DOWN;
    c->down_reported = true;
    r[REG_TD_STATUS] = 1;
    r[REG_P1_XH] = (uint8_t)(event | (c->last_x >> 8));
    r[REG_P1_XL] = (uint8_t)c->last_x;
    r[REG_P1_YH] = (uint8_t)(0x00 /* id 0 */ | (c->last_y >> 8));
    r[REG_P1_YL] = (uint8_t)c->last_y;
    r[REG_P1_WEIGHT] = 0x10;
    r[REG_P1_MISC] = 0x10;
  } else {
    r[REG_TD_STATUS] = 0;
    // why: after a release the part keeps the last point with a lift-up event
    r[REG_P1_XH] = (uint8_t)(c->down_reported ? (EVENT_LIFT_UP | (c->last_x >> 8)) : EVENT_NONE | 0x0F);
  }
}

/** Drive INT: low while a touch is present (both interrupt modes), released otherwise. */
static void update_int(chip_t *c) {
  pin_mode(c->intn, (c->touched && !c->hibernating) ? OUTPUT_LOW : INPUT);
}

/** Poll the `touched` control; a new touch restarts the press-down / contact sequence. */
static void on_poll(void *user_data) {
  chip_t *c = user_data;
  bool t = attr_read(c->attr_touched) != 0;
  if (t == c->touched) return;
  c->touched = t;
  if (t) c->down_reported = false;
  if (c->debug) printf("ft3267: %s\n", t ? "touch" : "release");
  update_int(c);
}

/** RST low resets the part; INT pulled low by the host wakes it from hibernate. */
static void on_pin(void *user_data, pin_t pin, uint32_t value) {
  chip_t *c = user_data;
  if (pin == c->rst && value == LOW) {
    reset_registers(c);
    update_int(c);
  } else if (pin == c->intn && value == LOW && c->hibernating) {
    c->hibernating = false;
    c->regs[REG_PWR_MODE] = 0;
    if (c->debug) printf("ft3267: wake\n");
  }
}

/** I2C START addressed to the part: NACK while hibernating, else latch a report for reads. */
static bool on_i2c_connect(void *user_data, uint32_t address, bool read) {
  (void)address;
  chip_t *c = user_data;
  if (c->hibernating) return false;
  if (read) {
    if (c->ptr <= REG_P1_MISC) latch_report(c);
  } else {
    c->ptr_pending = true;
  }
  return true;
}

/** One byte read: register at the pointer, then auto-increment. */
static uint8_t on_i2c_read(void *user_data) {
  chip_t *c = user_data;
  return c->regs[c->ptr++];
}

/** One byte written: the first is the register address, the rest are data. */
static bool on_i2c_write(void *user_data, uint8_t data) {
  chip_t *c = user_data;
  if (c->ptr_pending) {
    c->ptr = data;
    c->ptr_pending = false;
    return true;
  }
  uint8_t reg = c->ptr++;
  switch (reg) {
  case REG_TD_STATUS: case REG_P1_XH: case REG_P1_XL: case REG_P1_YH: case REG_P1_YL:
  case REG_CIPHER: case REG_FIRMID: case REG_FOCALTECH_ID: case REG_LIB_VER_H: case REG_LIB_VER_L:
    break; /* read-only */
  case REG_PWR_MODE:
    c->regs[reg] = data;
    if (data == PWR_HIBERNATE) {
      c->hibernating = true;
      update_int(c);
      if (c->debug) printf("ft3267: hibernate\n");
    }
    break;
  default:
    c->regs[reg] = data;
    break;
  }
  return true;
}

/** I2C STOP: nothing to commit. */
static void on_i2c_disconnect(void *user_data) { (void)user_data; }

/** Wokwi entry point: attributes, controls, pins, I2C device and the control poll timer. */
void chip_init(void) {
  chip_t *c = calloc(1, sizeof(chip_t));
  c->chip_id = (uint8_t)attr_read(attr_init("chipId", 0x33));
  c->vendor_id = (uint8_t)attr_read(attr_init("vendorId", 0x11));
  c->firmware_id = (uint8_t)attr_read(attr_init("firmwareId", 0x01));
  c->debug = attr_read(attr_init("debug", 0)) != 0;
  c->attr_x = attr_init("x", 120);
  c->attr_y = attr_init("y", 120);
  c->attr_touched = attr_init("touched", 0);

  c->intn = pin_init("INT", INPUT);
  c->rst = pin_init("RST", INPUT_PULLUP);
  const pin_watch_config_t w = {.user_data = c, .edge = FALLING, .pin_change = on_pin};
  pin_watch(c->rst, &w);
  pin_watch(c->intn, &w);
  reset_registers(c);

  const i2c_config_t i2c = {.user_data = c,
                            .address = FT3267_ADDR,
                            .scl = pin_init("SCL", INPUT),
                            .sda = pin_init("SDA", INPUT),
                            .connect = on_i2c_connect,
                            .read = on_i2c_read,
                            .write = on_i2c_write,
                            .disconnect = on_i2c_disconnect};
  i2c_init(&i2c);

  const timer_config_t t = {.user_data = c, .callback = on_poll};
  c->poll = timer_init(&t);
  timer_start(c->poll, POLL_US, true);
  on_poll(c);
  printf("ft3267: ready\n");
}
