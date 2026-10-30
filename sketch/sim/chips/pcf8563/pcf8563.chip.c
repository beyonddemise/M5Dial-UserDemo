// SPDX-FileCopyrightText: © 2026 Stephan Wissel
// SPDX-License-Identifier: MIT
//
// PCF8563 real-time clock / calendar - Wokwi custom chip.
//
// Models the NXP PCF8563 I2C register file (00h-0Fh): BCD time and date with
// century flag, alarm (minute/hour/day/weekday), countdown timer and the
// open-drain INT output. Time runs on simulation time; it starts from the host's
// UTC wall clock (or the `startTime` attribute).
//
// See README.md for the register model and limitations.

#include <time.h>
// why: some hosts' <time.h> declare a POSIX timer_t; rename the Wokwi API's timer_t so both coexist
#define timer_t wokwi_timer_t
#include "wokwi-api.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PCF8563_ADDR 0x51
#define NREGS 16

/* --- registers (PCF8563 datasheet rev. 11, section 8) ------------------------ */
#define REG_CTRL1 0x00
#define REG_CTRL2 0x01
#define REG_SECONDS 0x02 /* bit 7 = VL (clock integrity not guaranteed) */
#define REG_MINUTES 0x03
#define REG_HOURS 0x04
#define REG_DAYS 0x05
#define REG_WEEKDAYS 0x06
#define REG_MONTHS 0x07 /* bit 7 = century */
#define REG_YEARS 0x08
#define REG_ALARM_MIN 0x09 /* bit 7 = AE (1 = this alarm field disabled) */
#define REG_ALARM_WDAY 0x0C
#define REG_CLKOUT 0x0D
#define REG_TIMER_CTRL 0x0E
#define REG_TIMER 0x0F

#define CTRL1_STOP 0x20
#define CTRL1_MASK 0xA8 /* TEST1, STOP, TESTC */
#define CTRL2_TI_TP 0x10
#define CTRL2_AF 0x08
#define CTRL2_TF 0x04
#define CTRL2_AIE 0x02
#define CTRL2_TIE 0x01
#define TIMER_TE 0x80
#define AE 0x80

#define NS_PER_S 1000000000ull

/** Complete chip state (one instance per part). */
typedef struct {
  pin_t intn;
  i2c_dev_t i2c;
  uint8_t regs[NREGS]; /* control/alarm/timer registers; 02h-08h hold the latched time */
  bool debug;

  /* time keeping: the clock shows base_epoch at simulation time base_ns (+ elapsed if running) */
  int64_t base_epoch;
  uint64_t base_ns;
  uint8_t base_wday; /* weekday register value at base_epoch (the part just counts it) */
  bool vl;           /* VL flag */
  bool stopped;

  /* I2C transaction */
  uint8_t ptr;
  bool ptr_pending; /* next written byte is the register address */
  bool time_dirty;  /* time registers written; committed at STOP / repeated START */

  wokwi_timer_t tick;      /* 1 Hz, aligned to the seconds counter */
  wokwi_timer_t countdown; /* countdown timer period */
} chip_t;

/* ---------------------------------------------------------------------------
 * Calendar helpers (proleptic Gregorian, days since 1970-01-01)
 * ------------------------------------------------------------------------- */

/** Days since 1970-01-01 for a civil date (H. Hinnant's algorithm, valid for any year). */
static int64_t days_from_civil(int64_t y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int64_t)doe - 719468;
}

/** Civil date for a day count since 1970-01-01 (inverse of days_from_civil). */
static void civil_from_days(int64_t z, int *y, unsigned *m, unsigned *d) {
  z += 719468;
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = (unsigned)(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  *d = doy - (153 * mp + 2) / 5 + 1;
  *m = mp < 10 ? mp + 3 : mp - 9;
  *y = (int)(yoe + era * 400 + (*m <= 2));
}

/** Floor division (seconds -> days also for times before 1970). */
static int64_t floor_div(int64_t a, int64_t b) { return a / b - ((a % b != 0) && ((a < 0) != (b < 0))); }

/** Binary (0-99) to packed BCD. */
static uint8_t to_bcd(unsigned v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }
/** Packed BCD to binary. */
static unsigned from_bcd(uint8_t v) { return (unsigned)((v >> 4) * 10 + (v & 0x0f)); }

/* ---------------------------------------------------------------------------
 * Clock
 * ------------------------------------------------------------------------- */

/** Current clock value in seconds since 1970 (frozen while STOP is set). */
static int64_t now_epoch(const chip_t *c) {
  if (c->stopped) return c->base_epoch;
  return c->base_epoch + (int64_t)((get_sim_nanos() - c->base_ns) / NS_PER_S);
}

/** Weekday register value for `epoch`: the stored weekday advanced by elapsed days. */
static uint8_t weekday_at(const chip_t *c, int64_t epoch) {
  int64_t delta = floor_div(epoch, 86400) - floor_div(c->base_epoch, 86400);
  return (uint8_t)(((int64_t)c->base_wday + delta % 7 + 7) % 7);
}

/** Encode clock value `t` as the register values of 02h-08h (out[0] = seconds .. out[6] = years). */
static void encode_time(const chip_t *c, int64_t t, uint8_t out[7]) {
  int64_t days = floor_div(t, 86400);
  int64_t sod = t - days * 86400;
  int y;
  unsigned m, d;
  civil_from_days(days, &y, &m, &d);
  out[0] = (uint8_t)((c->vl ? 0x80 : 0) | to_bcd((unsigned)(sod % 60)));
  out[1] = to_bcd((unsigned)(sod / 60 % 60));
  out[2] = to_bcd((unsigned)(sod / 3600));
  out[3] = to_bcd(d);
  out[4] = weekday_at(c, t);
  // why: century bit set = 19xx, clear = 20xx - the convention M5Unified and most drivers use
  // (the datasheet only says the bit toggles when the years wrap 99 -> 00). The model keeps the
  // full year, so the bit toggles at 1999 -> 2000; see README "Limitations" for the range.
  out[5] = (uint8_t)((y < 2000 ? 0x80 : 0) | to_bcd(m));
  out[6] = to_bcd((unsigned)(y % 100));
}

/** Write clock value `t` into registers 02h-08h. */
static void latch_time_at(chip_t *c, int64_t t) { encode_time(c, t, &c->regs[REG_SECONDS]); }

/** Latch the current time into registers 02h-08h (the part freezes them during an access). */
static void latch_time(chip_t *c) { latch_time_at(c, now_epoch(c)); }

/** Restart the 1 Hz tick so it fires on the next whole second of the clock. */
static void restart_tick(chip_t *c) {
  timer_stop(c->tick);
  if (!c->stopped) timer_start_ns(c->tick, NS_PER_S, true);
}

/** Set the clock to `epoch` with weekday `wday`, resetting the sub-second prescaler. */
static void set_clock(chip_t *c, int64_t epoch, uint8_t wday) {
  c->base_epoch = epoch;
  c->base_ns = get_sim_nanos();
  c->base_wday = wday & 0x07;
  restart_tick(c);
}

/** Apply written time registers 02h-08h to the clock. */
static void commit_time(chip_t *c) {
  c->time_dirty = false;
  const uint8_t *r = c->regs;
  int year = (int)from_bcd(r[REG_YEARS]) + ((r[REG_MONTHS] & 0x80) ? 1900 : 2000);
  int64_t days = days_from_civil(year, from_bcd(r[REG_MONTHS] & 0x1f), from_bcd(r[REG_DAYS] & 0x3f));
  int64_t epoch = days * 86400 + from_bcd(r[REG_HOURS] & 0x3f) * 3600 + from_bcd(r[REG_MINUTES] & 0x7f) * 60 +
                  from_bcd(r[REG_SECONDS] & 0x7f);
  c->vl = (r[REG_SECONDS] & 0x80) != 0;
  set_clock(c, epoch, r[REG_WEEKDAYS]);
  if (c->debug) printf("pcf8563: time set, epoch %lld\n", (long long)epoch);
}

/* ---------------------------------------------------------------------------
 * Interrupts: alarm, countdown timer, INT pin
 * ------------------------------------------------------------------------- */

/** Drive the open-drain INT output: low while an enabled flag is set, released otherwise. */
static void update_int(chip_t *c) {
  uint8_t s = c->regs[REG_CTRL2];
  bool active = ((s & CTRL2_AF) && (s & CTRL2_AIE)) || ((s & CTRL2_TF) && (s & CTRL2_TIE));
  pin_mode(c->intn, active ? OUTPUT_LOW : INPUT);
}

/** True if every enabled alarm field matches clock value `t` (and at least one is enabled). */
static bool alarm_matches(const chip_t *c, int64_t t) {
  // why: local copy - the tick may fire between two bytes of a host transaction, and the
  // latched registers must not change under the host's burst read or partial write
  uint8_t now[7];
  encode_time(c, t, now);
  static const uint8_t time_idx[4] = {REG_MINUTES - REG_SECONDS, REG_HOURS - REG_SECONDS, REG_DAYS - REG_SECONDS,
                                      REG_WEEKDAYS - REG_SECONDS};
  static const uint8_t mask[4] = {0x7f, 0x3f, 0x3f, 0x07};
  bool any = false;
  for (int i = 0; i < 4; i++) {
    uint8_t a = c->regs[REG_ALARM_MIN + i];
    if (a & AE) continue;
    any = true;
    if ((a & mask[i]) != (now[time_idx[i]] & mask[i])) return false;
  }
  return any;
}

/** 1 Hz tick: on each new minute, raise AF if the alarm matches. */
static void on_tick(void *user_data) {
  chip_t *c = user_data;
  if (c->stopped) return;
  // why: the tick fires exactly on a second boundary; round so float jitter in the
  // simulation clock cannot land it on the previous second
  uint64_t elapsed_ns = get_sim_nanos() - c->base_ns;
  int64_t t = c->base_epoch + (int64_t)((elapsed_ns + NS_PER_S / 2) / NS_PER_S);
  if (t - floor_div(t, 60) * 60 != 0) return;
  if (alarm_matches(c, t)) {
    c->regs[REG_CTRL2] |= CTRL2_AF;
    if (c->debug) printf("pcf8563: alarm\n");
    update_int(c);
  }
}

/** Countdown timer period elapsed: raise TF (the counter reloads and keeps running). */
static void on_countdown(void *user_data) {
  chip_t *c = user_data;
  c->regs[REG_CTRL2] |= CTRL2_TF;
  if (c->debug) printf("pcf8563: timer\n");
  update_int(c);
}

/** (Re)start the countdown timer from TE, TD[1:0] and the timer value. */
static void restart_countdown(chip_t *c) {
  timer_stop(c->countdown);
  uint8_t ctrl = c->regs[REG_TIMER_CTRL];
  uint8_t n = c->regs[REG_TIMER];
  if (!(ctrl & TIMER_TE) || n == 0) return;
  static const uint64_t period_ns[4] = {NS_PER_S / 4096, NS_PER_S / 64, NS_PER_S, 60 * NS_PER_S};
  timer_start_ns(c->countdown, period_ns[ctrl & 0x03] * n, true);
}

/* ---------------------------------------------------------------------------
 * Register file
 * ------------------------------------------------------------------------- */

/** Store one written register byte, honouring the flag/STOP semantics of the part. */
static void write_reg(chip_t *c, uint8_t reg, uint8_t v) {
  switch (reg) {
  case REG_CTRL1: {
    bool stop = (v & CTRL1_STOP) != 0;
    if (stop && !c->stopped) {
      /* why: weekday_at() counts days from base_epoch, so rebase the weekday together with it
         (otherwise a STOP after a midnight rollover keeps the previous day) */
      int64_t now = now_epoch(c);
      c->base_wday = weekday_at(c, now);
      c->base_epoch = now;
      c->stopped = true;
      timer_stop(c->tick);
    } else if (!stop && c->stopped) {
      c->stopped = false;
      set_clock(c, c->base_epoch, weekday_at(c, c->base_epoch));
    }
    c->regs[reg] = v & CTRL1_MASK;
    break;
  }
  case REG_CTRL2: {
    // why: AF and TF can only be cleared by the host (writing 1 leaves them unchanged)
    uint8_t flags = c->regs[reg] & v & (CTRL2_AF | CTRL2_TF);
    c->regs[reg] = (uint8_t)((v & (CTRL2_TI_TP | CTRL2_AIE | CTRL2_TIE)) | flags);
    update_int(c);
    break;
  }
  case REG_SECONDS: case REG_MINUTES: case REG_HOURS: case REG_DAYS:
  case REG_WEEKDAYS: case REG_MONTHS: case REG_YEARS:
    c->regs[reg] = v;
    c->time_dirty = true;
    break;
  case REG_TIMER_CTRL:
  case REG_TIMER:
    c->regs[reg] = (reg == REG_TIMER_CTRL) ? (v & 0x83) : v;
    restart_countdown(c);
    break;
  default:
    c->regs[reg] = v;
    break;
  }
}

/** I2C START/repeated START addressed to the part: commit pending time writes, latch for reads. */
static bool on_i2c_connect(void *user_data, uint32_t address, bool read) {
  (void)address;
  chip_t *c = user_data;
  if (c->time_dirty) commit_time(c);
  if (read) latch_time(c);
  else c->ptr_pending = true;
  return true;
}

/** One byte read by the host: register at the pointer, then auto-increment. */
static uint8_t on_i2c_read(void *user_data) {
  chip_t *c = user_data;
  uint8_t v = c->regs[c->ptr];
  c->ptr = (c->ptr + 1) % NREGS; /* auto-increment, wraps after 0Fh */
  return v;
}

/** One byte written by the host: the first is the register address, the rest are data. */
static bool on_i2c_write(void *user_data, uint8_t data) {
  chip_t *c = user_data;
  if (c->ptr_pending) {
    c->ptr = data % NREGS;
    c->ptr_pending = false;
    if (!c->time_dirty) latch_time(c); /* a partial time write keeps the other fields */
    return true;
  }
  write_reg(c, c->ptr, data);
  c->ptr = (c->ptr + 1) % NREGS;
  return true;
}

/** I2C STOP: a written time takes effect now. */
static void on_i2c_disconnect(void *user_data) {
  chip_t *c = user_data;
  if (c->time_dirty) commit_time(c);
}

/**
 * Initial clock value: the `startTime` attribute ("YYYY-MM-DD HH:MM:SS", UTC) if set,
 * else the host's UTC wall clock.
 */
static int64_t initial_epoch(void) {
  char buf[32] = {0};
  string_t s = attr_string_init("startTime");
  if (s != STRING_NULL && string_get_length(s) > 0) {
    string_read(s, buf, sizeof(buf));
    int y, mo, d, h, mi, sec;
    if (sscanf(buf, "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &sec) == 6)
      return days_from_civil(y, (unsigned)mo, (unsigned)d) * 86400 + h * 3600 + mi * 60 + sec;
    printf("pcf8563: bad startTime '%s', using host time\n", buf);
  }
  return (int64_t)time(NULL);
}

/** Wokwi entry point: register file at power-on values, clock from the host, I2C device. */
void chip_init(void) {
  chip_t *c = calloc(1, sizeof(chip_t));
  c->debug = attr_read(attr_init("debug", 0)) != 0;
  c->intn = pin_init("INT", INPUT);
  pin_init("CLKOUT", INPUT); /* accepted for wiring, not modelled */

  /* power-on reset values (datasheet table 7) */
  c->regs[REG_CTRL1] = 0x08;
  for (int i = 0; i < 4; i++) c->regs[REG_ALARM_MIN + i] = AE;
  c->regs[REG_CLKOUT] = 0x80;
  c->regs[REG_TIMER_CTRL] = 0x03;

  const timer_config_t tick = {.user_data = c, .callback = on_tick};
  c->tick = timer_init(&tick);
  const timer_config_t cd = {.user_data = c, .callback = on_countdown};
  c->countdown = timer_init(&cd);

  int64_t epoch = initial_epoch();
  set_clock(c, epoch, (uint8_t)((floor_div(epoch, 86400) + 4) % 7)); /* 1970-01-01 = Thursday */

  const i2c_config_t i2c = {.user_data = c,
                            .address = PCF8563_ADDR,
                            .scl = pin_init("SCL", INPUT),
                            .sda = pin_init("SDA", INPUT),
                            .connect = on_i2c_connect,
                            .read = on_i2c_read,
                            .write = on_i2c_write,
                            .disconnect = on_i2c_disconnect};
  c->i2c = i2c_init(&i2c);
  latch_time(c);
  printf("pcf8563: ready, %s%02x-%02x-%02x %02x:%02x:%02x UTC\n", (c->regs[REG_MONTHS] & 0x80) ? "19" : "20",
         c->regs[REG_YEARS], c->regs[REG_MONTHS] & 0x1f,
         c->regs[REG_DAYS], c->regs[REG_HOURS], c->regs[REG_MINUTES], c->regs[REG_SECONDS] & 0x7f);
}
