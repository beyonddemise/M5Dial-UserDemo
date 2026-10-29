// SPDX-FileCopyrightText: © 2026 Stephan Wissel
// SPDX-License-Identifier: MIT
//
// Host-side unit test for gc9a01.chip.c: the chip source is compiled natively
// against a stubbed Wokwi API, then driven with the byte/edge sequences a host
// MCU produces (init, window + RAMWR fill, RDDID read with a dummy clock).
// Run: make test

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../gc9a01.chip.c"

/* ---- stubbed Wokwi API ------------------------------------------------------ */
enum { P_CS, P_DC, P_RST, P_SCL, P_SDA, P_BL, NPINS };
static const char *pin_names[NPINS] = {"CS", "DC", "RST", "SCL", "SDA", "BL"};
static uint32_t level[NPINS];
static uint32_t mode[NPINS];
static pin_watch_config_t watch[NPINS];
static bool watched[NPINS];
static spi_config_t spi_cfg;
static uint8_t *spi_buf;
static uint32_t spi_cap, spi_len;
static bool spi_active;
static uint8_t fb[WIDTH * HEIGHT * 4];

pin_t pin_init(const char *name, uint32_t m) {
  for (int i = 0; i < NPINS; i++)
    if (!strcmp(name, pin_names[i])) { mode[i] = m; return i; }
  assert(!"unknown pin");
  return NO_PIN;
}
uint32_t pin_read(pin_t p) { return level[p]; }
void pin_write(pin_t p, uint32_t v) { level[p] = v; }
void pin_mode(pin_t p, uint32_t m) { mode[p] = m; }
bool pin_watch(pin_t p, const pin_watch_config_t *c) {
  if (watched[p]) return false;
  watch[p] = *c; watched[p] = true; return true;
}
void pin_watch_stop(pin_t p) { watched[p] = false; }
uint32_t attr_init(const char *name, uint32_t def) { (void)name; return def; }
uint32_t attr_read(uint32_t v) { return v; }
spi_dev_t spi_init(const spi_config_t *c) { spi_cfg = *c; return 1; }
void spi_start(spi_dev_t d, uint8_t *b, uint32_t n) { (void)d; spi_buf = b; spi_cap = n; spi_len = 0; spi_active = true; }
void spi_stop(spi_dev_t d) {
  (void)d;
  if (!spi_active) return;
  spi_active = false;
  spi_cfg.done(spi_cfg.user_data, spi_buf, spi_len);
}
/* referenced by static helpers in wokwi-api.h; unused by the chip */
void timer_start_ns_d(const timer_t t, double ns, bool rep) { (void)t; (void)ns; (void)rep; }
double get_sim_nanos_d(void) { return 0; }
buffer_t framebuffer_init(uint32_t *w, uint32_t *h) { *w = WIDTH; *h = HEIGHT; return 1; }
void buffer_write(buffer_t b, uint32_t off, void *d, uint32_t n) { (void)b; memcpy(fb + off, d, n); }

/* ---- host MCU model --------------------------------------------------------- */
static void set_pin(int p, uint32_t v) {
  if (level[p] == v) return;
  level[p] = v;
  if (watched[p]) watch[p].pin_change(watch[p].user_data, p, v);
}
static void spi_byte(uint8_t b) {
  assert(spi_active && "chip must be listening while CS is low");
  spi_buf[spi_len++] = b;
  if (spi_len == spi_cap) { spi_active = false; spi_cfg.done(spi_cfg.user_data, spi_buf, spi_len); }
}
static void cmd(uint8_t c) { set_pin(P_DC, LOW); spi_byte(c); }
static void dat(uint8_t d) { set_pin(P_DC, HIGH); spi_byte(d); }
/** One SCL clock in mode 0; returns SDA as sampled on the rising edge. */
static int clock_in(void) {
  set_pin(P_SCL, HIGH);
  int v = (int)level[P_SDA];
  set_pin(P_SCL, LOW);
  return v;
}

static void test_rddid(void) {
  // M5GFX _read_panel_id(): CS low, cmd 04h, DC high, 1 dummy clock, 32 data clocks.
  set_pin(P_CS, LOW);
  cmd(0x04);
  set_pin(P_DC, HIGH);
  assert(mode[P_SDA] == OUTPUT && "SDA turned around for the reply");
  clock_in(); /* dummy */
  uint32_t id = 0;
  for (int byte = 0; byte < 4; byte++) {
    uint32_t v = 0;
    for (int i = 0; i < 8; i++) v = v << 1 | (uint32_t)clock_in();
    id |= v << (8 * byte); /* ESP32 SPI stores the first byte in the low byte */
  }
  set_pin(P_CS, HIGH);
  assert(mode[P_SDA] == INPUT && "SDA released after CS high");
  printf("RDDID -> %08x\n", id);
  assert((id & 0xFFFFFF) == 0x019a00);
}

static void test_fill_green(void) {
  set_pin(P_RST, LOW);
  set_pin(P_RST, HIGH);
  set_pin(P_CS, LOW);
  cmd(0x11);           /* SLPOUT */
  cmd(0x21);           /* INVON (IPS glass) */
  cmd(0x36); dat(0x08); /* MADCTL: BGR */
  cmd(0x3A); dat(0x55); /* 16 bpp */
  cmd(0x29);           /* DISPON */
  cmd(0x2A); dat(0); dat(0); dat(0); dat(239);
  cmd(0x2B); dat(0); dat(0); dat(0); dat(239);
  cmd(0x2C);
  set_pin(P_DC, HIGH);
  for (int i = 0; i < WIDTH * HEIGHT; i++) { spi_byte(0x07); spi_byte(0xE0); }
  set_pin(P_CS, HIGH);
  for (int i = 0; i < WIDTH * HEIGHT; i++) {
    const uint8_t *px = &fb[i * 4];
    if (px[0] != 0 || px[1] != 255 || px[2] != 0 || px[3] != 255) {
      printf("pixel %d = %u,%u,%u,%u\n", i, px[0], px[1], px[2], px[3]);
      assert(!"framebuffer not green");
    }
  }
  printf("fill green ok\n");
}

static void test_window(void) {
  /* 2x2 red block at (10,20) without touching anything else */
  set_pin(P_CS, LOW);
  cmd(0x2A); dat(0); dat(10); dat(0); dat(11);
  cmd(0x2B); dat(0); dat(20); dat(0); dat(21);
  cmd(0x2C);
  for (int i = 0; i < 4; i++) { dat(0xF8); dat(0x00); }
  set_pin(P_CS, HIGH);
  const uint8_t *in = &fb[(21 * WIDTH + 11) * 4], *out = &fb[(22 * WIDTH + 11) * 4];
  assert(in[0] == 255 && in[1] == 0 && in[2] == 0);
  assert(out[0] == 0 && out[1] == 255 && out[2] == 0);
  printf("window ok\n");
}

int main(void) {
  level[P_CS] = HIGH;
  level[P_RST] = HIGH;
  chip_init();
  test_rddid();
  test_fill_green();
  test_window();
  test_rddid(); /* reads still work after pixel traffic */
  printf("all gc9a01 host tests passed\n");
  return 0;
}
