// SPDX-FileCopyrightText: © 2026 Stephan Wissel
// SPDX-License-Identifier: MIT
//
// GC9A01 240x240 round TFT controller - Wokwi custom chip.
//
// Models the 4-line serial interface of the GalaxyCore GC9A01 as used in
// "3-wire + D/C" setups: one bidirectional data line (SDA), clock (SCL), chip
// select (CS), data/command (DC) and reset (RST). Writes are received through
// the Wokwi SPI device API (fast, buffered); register reads (RDDID etc.) are
// answered bit by bit on SDA, the way the real part turns its data line around.
//
// See README.md for the supported command set and limitations.

#include "wokwi-api.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 240
#define HEIGHT 240
#define SPI_BUF_SIZE 4096
#define MAX_PARAMS 16
#define MAX_READ_BYTES 8

/* --- GC9A01 commands (datasheet v1.0 section 6) ------------------------------ */
#define CMD_NOP 0x00
#define CMD_SWRESET 0x01
#define CMD_RDDID 0x04
#define CMD_RDDST 0x09
#define CMD_RDDPM 0x0A
#define CMD_RDDMADCTL 0x0B
#define CMD_RDDCOLMOD 0x0C
#define CMD_SLPIN 0x10
#define CMD_SLPOUT 0x11
#define CMD_INVOFF 0x20
#define CMD_INVON 0x21
#define CMD_DISPOFF 0x28
#define CMD_DISPON 0x29
#define CMD_CASET 0x2A
#define CMD_RASET 0x2B
#define CMD_RAMWR 0x2C
#define CMD_MADCTL 0x36
#define CMD_COLMOD 0x3A
#define CMD_RAMWRC 0x3C
#define CMD_RDID1 0xDA
#define CMD_RDID2 0xDB
#define CMD_RDID3 0xDC

/* MADCTL bits */
#define MAD_MY 0x80
#define MAD_MX 0x40
#define MAD_MV 0x20
#define MAD_BGR 0x08

/** Interface state: receiving bytes via the SPI device, or driving a read reply. */
typedef enum { IF_WRITE, IF_READ } if_state_t;

/** Complete chip state (one instance per part). */
typedef struct {
  pin_t cs, dc, scl, sda, rst;
  spi_dev_t spi;
  uint8_t spi_buf[SPI_BUF_SIZE];
  bool spi_running;
  bool stopping; /* why: spi_stop() calls done() synchronously; done must not restart then */
  bool dc_data;  /* DC level that applies to the bytes currently in spi_buf */

  /* attributes */
  bool ips;          /* glass inverts: INVON shows true colours (IPS GC9A01 modules) */
  bool glass_bgr;    /* glass is BGR ordered: MADCTL.BGR=1 shows true colours */
  uint32_t dummy_bits; /* dummy clocks before read data (datasheet: 1) */
  bool debug;
  bool frame_log;    /* print a frame summary after each RAMWR burst (for CI scenarios) */

  /* command decoder */
  uint8_t cmd;
  uint8_t params[MAX_PARAMS];
  uint32_t nparams;
  bool ram_write;
  bool ram_dirty; /* pixels written since the last frame summary */
  uint8_t pixel_hi;
  bool pixel_half;

  /* controller registers */
  uint8_t madctl, colmod;
  bool inverted, display_on, sleeping;
  uint16_t xs, xe, ys, ye; /* column / row (page) window */
  uint16_t cx, cy;         /* write pointer within the window */

  /* read reply (bit-banged on SDA) */
  if_state_t if_state;
  uint8_t reply[MAX_READ_BYTES];
  uint32_t reply_bits; /* total bits including dummy bits */
  uint32_t bit_pos;
  bool read_pending;

  /* memory + display */
  uint16_t gram[WIDTH * HEIGHT];
  uint32_t rgba[WIDTH * HEIGHT];
  buffer_t fb;
  uint32_t fb_w, fb_h;
  int dirty_lo, dirty_hi; /* dirty row range of rgba, -1 = clean */
} chip_t;

static void on_pin(void *user_data, pin_t pin, uint32_t value);
static void on_scl(void *user_data, pin_t pin, uint32_t value);
static void on_spi_done(void *user_data, uint8_t *buffer, uint32_t count);

/* ---------------------------------------------------------------------------
 * Display rendering
 * ------------------------------------------------------------------------- */

/** Convert one GRAM pixel to the RGBA value shown, honouring BGR/inversion/power state. */
static uint32_t render_pixel(const chip_t *c, uint16_t v) {
  if (!c->display_on || c->sleeping) return 0xff000000; /* panel dark */
  // why: what the eye sees depends on both the register state and the glass;
  // a mismatch between MADCTL.BGR and the glass order swaps red and blue, and
  // INVON on non-IPS glass (or INVOFF on IPS glass) shows the complement.
  if (c->inverted != c->ips) v = (uint16_t)~v;
  uint32_t r5 = (v >> 11) & 0x1f, g6 = (v >> 5) & 0x3f, b5 = v & 0x1f;
  if (((c->madctl & MAD_BGR) != 0) != c->glass_bgr) {
    uint32_t t = r5; r5 = b5; b5 = t;
  }
  uint32_t r = (r5 << 3) | (r5 >> 2), g = (g6 << 2) | (g6 >> 4), b = (b5 << 3) | (b5 >> 2);
  return 0xff000000u | (b << 16) | (g << 8) | r; /* bytes R,G,B,A in memory */
}

/** Extend the dirty row range of the RGBA mirror to include `row` (flushed by flush()). */
static void mark_dirty(chip_t *c, int row) {
  if (c->dirty_lo < 0 || row < c->dirty_lo) c->dirty_lo = row;
  if (row > c->dirty_hi) c->dirty_hi = row;
}

/** Push the dirty rows of the RGBA mirror to the Wokwi framebuffer (one call). */
static void flush(chip_t *c) {
  if (c->dirty_lo < 0) return;
  uint32_t off = (uint32_t)c->dirty_lo * WIDTH;
  uint32_t n = (uint32_t)(c->dirty_hi - c->dirty_lo + 1) * WIDTH;
  buffer_write(c->fb, off * 4, &c->rgba[off], n * 4);
  c->dirty_lo = c->dirty_hi = -1;
}

/** Re-render the whole panel (after an inversion / BGR / power change). */
static void render_all(chip_t *c) {
  for (uint32_t i = 0; i < WIDTH * HEIGHT; i++) c->rgba[i] = render_pixel(c, c->gram[i]);
  c->dirty_lo = 0;
  c->dirty_hi = HEIGHT - 1;
}

/** Store one RGB565 pixel at the write pointer and advance it inside the window. */
static void put_pixel(chip_t *c, uint16_t v) {
  // why: the write pointer walks the MCU-side (column, page) window; MADCTL
  // decides how that maps onto the physical glass (exchange first, then mirror).
  uint32_t col = c->cx, row = c->cy;
  if (c->madctl & MAD_MV) { uint32_t t = col; col = row; row = t; }
  if (c->madctl & MAD_MX) col = WIDTH - 1 - col;
  if (c->madctl & MAD_MY) row = HEIGHT - 1 - row;
  if (col < WIDTH && row < HEIGHT) {
    uint32_t i = row * WIDTH + col;
    c->gram[i] = v;
    c->rgba[i] = render_pixel(c, v);
    mark_dirty(c, (int)row);
    c->ram_dirty = true;
  }
  if (c->cx < c->xe) {
    c->cx++;
  } else {
    c->cx = c->xs;
    c->cy = (c->cy < c->ye) ? c->cy + 1 : c->ys;
  }
}

/**
 * Print a summary of what the panel shows: FNV-1a hash of the visible RGBA
 * pixels and, if every pixel has the same colour, that colour as RRGGBB.
 * Lets automation scenarios check the display where screenshots of custom
 * chips are not available (wokwi-cli: "Part does not have a valid framebuffer").
 */
static void frame_summary(chip_t *c) {
  if (!c->frame_log || !c->ram_dirty) return;
  c->ram_dirty = false;
  uint32_t h = 2166136261u;
  bool uniform = true;
  for (uint32_t i = 0; i < WIDTH * HEIGHT; i++) {
    uint32_t v = c->rgba[i];
    for (int k = 0; k < 4; k++) { h ^= (v >> (8 * k)) & 0xff; h *= 16777619u; }
    if (v != c->rgba[0]) uniform = false;
  }
  uint32_t p = c->rgba[0];
  if (uniform)
    printf("gc9a01: frame fnv=%08x uniform=%02x%02x%02x\n", (unsigned)h, (unsigned)(p & 0xff),
           (unsigned)((p >> 8) & 0xff), (unsigned)((p >> 16) & 0xff));
  else
    printf("gc9a01: frame fnv=%08x uniform=none\n", (unsigned)h);
}

/* ---------------------------------------------------------------------------
 * Command decoder
 * ------------------------------------------------------------------------- */

/** Power-on / hardware / software reset defaults (datasheet 'Reset Table'). */
static void reset_registers(chip_t *c) {
  c->madctl = 0;
  c->colmod = 0x66;
  c->inverted = false;
  c->display_on = false;
  c->sleeping = true;
  c->xs = 0; c->xe = WIDTH - 1; c->ys = 0; c->ye = HEIGHT - 1;
  c->cx = 0; c->cy = 0;
  c->cmd = CMD_NOP; c->nparams = 0; c->ram_write = false; c->pixel_half = false;
  c->read_pending = false;
  render_all(c);
}

/** Prepare a register read reply: `dummy_bits` dummy clocks, then `n` bytes MSB first. */
static void queue_reply(chip_t *c, const uint8_t *bytes, uint32_t n) {
  memset(c->reply, 0, sizeof(c->reply));
  memcpy(c->reply, bytes, n);
  c->reply_bits = c->dummy_bits + 8 * MAX_READ_BYTES;
  c->read_pending = true;
}

/** Handle a command byte (DC low). */
static void command(chip_t *c, uint8_t cmd) {
  if (c->debug) printf("gc9a01: cmd %02X\n", cmd);
  if (c->ram_write) frame_summary(c);
  // why: any new command cancels a read that was queued but never clocked out;
  // queue_reply() below sets it again for real read commands.
  c->read_pending = false;
  c->cmd = cmd;
  c->nparams = 0;
  c->ram_write = false;
  c->pixel_half = false;
  switch (cmd) {
    case CMD_SWRESET: reset_registers(c); break;
    case CMD_SLPIN: c->sleeping = true; render_all(c); break;
    case CMD_SLPOUT: c->sleeping = false; render_all(c); break;
    case CMD_INVOFF: c->inverted = false; render_all(c); break;
    case CMD_INVON: c->inverted = true; render_all(c); break;
    case CMD_DISPOFF: c->display_on = false; render_all(c); break;
    case CMD_DISPON: c->display_on = true; render_all(c); break;
    case CMD_RAMWR: c->cx = c->xs; c->cy = c->ys; c->ram_write = true; break;
    case CMD_RAMWRC: c->ram_write = true; break;
    case CMD_RDDID: case CMD_RDID1: case CMD_RDID2: case CMD_RDID3: {
      // why: GC9A01 ID = 00h (ID1, manufacturer) 9Ah (ID2, version) 01h (ID3, driver);
      // RDIDn returns the single byte, RDDID all three in a row.
      static const uint8_t id[3] = {0x00, 0x9A, 0x01};
      if (cmd == CMD_RDDID) queue_reply(c, id, 3);
      else queue_reply(c, &id[cmd - CMD_RDID1], 1);
      break;
    }
    case CMD_RDDST: {
      // why: approximate - only the bits a driver typically checks (power, MADCTL, COLMOD)
      uint8_t st[4] = {0, 0, 0, 0};
      st[0] = (uint8_t)((c->madctl & 0xF0) >> 3) | 0x80; /* booster on, MY/MX/MV/ML */
      st[1] = (uint8_t)(((c->madctl & MAD_BGR) ? 0x80 : 0) | (c->colmod & 0x77));
      st[2] = (uint8_t)((c->sleeping ? 0 : 0x08) | (c->display_on ? 0x04 : 0) | (c->inverted ? 0x20 : 0));
      queue_reply(c, st, 4);
      break;
    }
    case CMD_RDDPM: {
      uint8_t pm = (uint8_t)(0x80 | (c->sleeping ? 0 : 0x10) | (c->display_on ? 0x04 : 0));
      queue_reply(c, &pm, 1);
      break;
    }
    case CMD_RDDMADCTL: queue_reply(c, &c->madctl, 1); break;
    case CMD_RDDCOLMOD: queue_reply(c, &c->colmod, 1); break;
    default: break; /* vendor / gamma / power commands: accepted, no visible effect */
  }
}

/** Handle a parameter / pixel byte (DC high) for the current command. */
static void data(chip_t *c, uint8_t b) {
  if (c->ram_write) {
    // why: only 16-bit RGB565 (COLMOD 55h) is modelled; it is what M5GFX uses.
    if (!c->pixel_half) { c->pixel_hi = b; c->pixel_half = true; }
    else { put_pixel(c, (uint16_t)(c->pixel_hi << 8 | b)); c->pixel_half = false; }
    return;
  }
  if (c->nparams < MAX_PARAMS) c->params[c->nparams] = b;
  c->nparams++;
  const uint8_t *p = c->params;
  switch (c->cmd) {
    case CMD_CASET:
      if (c->nparams == 4) { c->xs = (uint16_t)(p[0] << 8 | p[1]); c->xe = (uint16_t)(p[2] << 8 | p[3]); }
      break;
    case CMD_RASET:
      if (c->nparams == 4) { c->ys = (uint16_t)(p[0] << 8 | p[1]); c->ye = (uint16_t)(p[2] << 8 | p[3]); }
      break;
    case CMD_MADCTL:
      if (c->nparams == 1) { c->madctl = b; render_all(c); }
      break;
    case CMD_COLMOD:
      if (c->nparams == 1) c->colmod = b;
      break;
    default: break;
  }
}

/* ---------------------------------------------------------------------------
 * Serial interface
 * ------------------------------------------------------------------------- */

/** Start (or keep) receiving bytes through the Wokwi SPI device into spi_buf. */
static void spi_begin(chip_t *c) {
  if (c->spi_running) return;
  c->spi_running = true;
  spi_start(c->spi, c->spi_buf, SPI_BUF_SIZE);
}

/** Stop the SPI device; its done() callback processes the partial buffer. */
static void spi_end(chip_t *c) {
  if (!c->spi_running) return;
  c->stopping = true;
  spi_stop(c->spi);
  c->stopping = false;
  c->spi_running = false;
}

/** Drive the next reply bit on SDA (called before each rising edge of SCL). */
static void drive_bit(chip_t *c) {
  uint32_t v = 0;
  if (c->bit_pos >= c->dummy_bits && c->bit_pos < c->reply_bits) {
    uint32_t i = c->bit_pos - c->dummy_bits;
    v = (c->reply[i >> 3] >> (7 - (i & 7))) & 1;
  }
  pin_write(c->sda, v);
}

/** Turn SDA around and answer the queued read, one bit per SCL clock (SPI mode 0). */
static void read_begin(chip_t *c) {
  c->read_pending = false;
  c->if_state = IF_READ;
  c->bit_pos = 0;
  pin_mode(c->sda, OUTPUT);
  drive_bit(c);
  const pin_watch_config_t w = {.user_data = c, .edge = BOTH, .pin_change = on_scl};
  if (!pin_watch(c->scl, &w) && c->debug) printf("gc9a01: pin_watch(SCL) failed\n");
  if (c->debug) printf("gc9a01: read reply for %02X\n", c->cmd);
}

/** Finish a read reply: stop watching SCL and release SDA back to the host. */
static void read_end(chip_t *c) {
  if (c->if_state != IF_READ) return;
  pin_watch_stop(c->scl);
  pin_mode(c->sda, INPUT);
  c->if_state = IF_WRITE;
  if (c->debug) printf("gc9a01: read done after %u clocks\n", (unsigned)c->bit_pos);
}

/** SCL edge while replying to a read: advance to the next reply bit on each falling edge. */
static void on_scl(void *user_data, pin_t pin, uint32_t value) {
  (void)pin;
  chip_t *c = user_data;
  if (c->if_state != IF_READ) return;
  // why: mode 0 - the host samples on the rising edge, the slave shifts on the falling edge
  if (value == LOW) {
    c->bit_pos++;
    drive_bit(c);
  }
}

/**
 * SPI device callback: decode the received bytes with the DC level they were sent
 * under, then keep listening while CS stays low (unless a read reply is queued).
 */
static void on_spi_done(void *user_data, uint8_t *buffer, uint32_t count) {
  chip_t *c = user_data;
  for (uint32_t i = 0; i < count; i++) {
    if (c->dc_data) data(c, buffer[i]);
    else command(c, buffer[i]);
  }
  flush(c);
  if (c->stopping) return;
  c->spi_running = false;
  if (pin_read(c->cs) == LOW && !c->read_pending) spi_begin(c);
}

/**
 * CS / DC / RST edge handler.
 * - CS low starts reception, CS high ends any transfer or read.
 * - DC edges flush the bytes received so far (they belong to the old DC level);
 *   the edge that follows a read command turns SDA around for the reply.
 * - RST low resets the registers; RST high resumes reception if CS is still low.
 */
static void on_pin(void *user_data, pin_t pin, uint32_t value) {
  chip_t *c = user_data;
  if (pin == c->rst) {
    if (value == LOW) { spi_end(c); read_end(c); reset_registers(c); flush(c); }
    else if (pin_read(c->cs) == LOW) spi_begin(c);
    return;
  }
  if (pin == c->cs) {
    if (value == LOW) {
      spi_begin(c);
    } else {
      spi_end(c);
      read_end(c);
      c->read_pending = false;
      flush(c);
      if (c->ram_write) frame_summary(c);
    }
    return;
  }
  if (pin == c->dc) {
    if (c->if_state == IF_READ) {
      // why: a DC high edge while replying changes nothing; DC low means the host has
      // moved on to the next command within the same CS assertion - end the reply.
      if (value == HIGH) return;
      read_end(c);
    }
    // why: bytes already shifted in belong to the old DC level - flush them first
    spi_end(c);
    c->dc_data = (value == HIGH);
    if (pin_read(c->cs) != LOW) return;
    if (c->read_pending) read_begin(c);
    else spi_begin(c);
  }
}

/** Wokwi entry point: read attributes, set up pins, SPI device and framebuffer. */
void chip_init(void) {
  chip_t *c = calloc(1, sizeof(chip_t));
  if (!c) { printf("gc9a01: out of memory (%u bytes)\n", (unsigned)sizeof(chip_t)); return; }
  c->ips = attr_read(attr_init("ips", 1)) != 0;
  c->glass_bgr = attr_read(attr_init("bgr", 1)) != 0;
  c->dummy_bits = attr_read(attr_init("readDummyBits", 1));
  c->debug = attr_read(attr_init("debug", 0)) != 0;
  c->frame_log = attr_read(attr_init("frameLog", 0)) != 0;

  c->cs = pin_init("CS", INPUT_PULLUP);
  c->dc = pin_init("DC", INPUT);
  c->rst = pin_init("RST", INPUT_PULLUP);
  c->scl = pin_init("SCL", INPUT);
  c->sda = pin_init("SDA", INPUT);
  pin_init("BL", INPUT);

  const pin_watch_config_t w = {.user_data = c, .edge = BOTH, .pin_change = on_pin};
  pin_watch(c->cs, &w);
  pin_watch(c->dc, &w);
  pin_watch(c->rst, &w);

  const spi_config_t spi = {
      .user_data = c, .sck = c->scl, .mosi = c->sda, .miso = NO_PIN, .mode = 0, .done = on_spi_done};
  c->spi = spi_init(&spi);

  c->fb = framebuffer_init(&c->fb_w, &c->fb_h);
  c->dirty_lo = c->dirty_hi = -1;
  c->dc_data = pin_read(c->dc) == HIGH;
  reset_registers(c);
  flush(c);
  if (pin_read(c->cs) == LOW) spi_begin(c);
  printf("gc9a01: ready (%ux%u)\n", (unsigned)c->fb_w, (unsigned)c->fb_h);
}
