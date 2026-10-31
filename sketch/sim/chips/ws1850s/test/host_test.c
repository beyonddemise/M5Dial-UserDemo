// SPDX-FileCopyrightText: © 2026 Stephan Wissel
// SPDX-License-Identifier: MIT
//
// Host-side unit test for ws1850s.chip.c: the chip source is compiled natively
// against a stubbed Wokwi API and driven by a C transcription of the MFRC522
// Arduino driver flow (PCD_Init, PICC_RequestA, PICC_Select CL1, PICC_HaltA via
// PCD_CommunicateWithPICC / PCD_CalculateCRC) over the I2C register interface.
// Run: make test

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../ws1850s.chip.c"

/* ---- stubbed Wokwi API ------------------------------------------------------ */
enum { P_IRQ, P_RST, P_SCL, P_SDA, NPINS };
static const char *pin_names[NPINS] = {"IRQ", "RST", "SCL", "SDA"};
static uint32_t level[NPINS] = {0, HIGH, 0, 0};
static uint32_t mode[NPINS];
static i2c_config_t i2c_cfg;
static const char *attr_names[] = {"version", "address", "sak", "atqa", "debug", "cardPresent", "uid0", "uid1", "uid2", "uid3"};
enum { NATTR = 10, A_PRESENT = 5, A_UID0 = 6 };
static uint32_t attr_val[NATTR];

#define MAX_TIMERS 4
static struct { timer_config_t cfg; bool running; double due; } timers[MAX_TIMERS];
static int ntimers;
static double sim_ns;

pin_t pin_init(const char *name, uint32_t m) {
  for (int i = 0; i < NPINS; i++)
    if (!strcmp(name, pin_names[i])) { mode[i] = m; return i; }
  assert(!"unknown pin");
  return NO_PIN;
}
uint32_t pin_read(pin_t p) { return level[p]; }
void pin_mode(pin_t p, uint32_t m) { mode[p] = m; }
bool pin_watch(pin_t p, const pin_watch_config_t *c) { (void)p; (void)c; return true; }
uint32_t attr_init(const char *name, uint32_t def) {
  for (int i = 0; i < NATTR; i++)
    if (!strcmp(name, attr_names[i])) { attr_val[i] = def; return (uint32_t)i; }
  assert(!"unknown attr");
  return 0;
}
uint32_t attr_read(uint32_t a) { return attr_val[a]; }
i2c_dev_t i2c_init(const i2c_config_t *c) { i2c_cfg = *c; return 1; }
timer_t timer_init(const timer_config_t *c) { timers[ntimers].cfg = *c; return (timer_t)ntimers++; }
void timer_start(const timer_t t, uint32_t us, bool rep) { (void)rep; (void)us; (void)t; /* poll timer: not needed */ }
void timer_start_ns_d(const timer_t t, double ns, bool rep) { (void)rep; timers[t].running = true; timers[t].due = sim_ns + ns; }
void timer_stop(const timer_t t) { timers[t].running = false; }
double get_sim_nanos_d(void) { return sim_ns; }

/** Each register access takes ~100 us at 100 kHz; fire timers that become due. */
static void tick(void) {
  sim_ns += 100e3;
  for (int i = 0; i < ntimers; i++)
    if (timers[i].running && timers[i].due <= sim_ns) {
      timers[i].running = false;
      timers[i].cfg.callback(timers[i].cfg.user_data);
    }
}

/* ---- MFRC522 driver transcription (M5Dial utility/MFRC522.cpp) --------------- */
enum { STATUS_OK, STATUS_ERROR, STATUS_COLLISION, STATUS_TIMEOUT, STATUS_NO_ROOM, STATUS_CRC_WRONG = 7 };

static void wreg_n(uint8_t reg, const uint8_t *v, int n) {
  tick();
  assert(i2c_cfg.connect(i2c_cfg.user_data, 0x28, false));
  assert(i2c_cfg.write(i2c_cfg.user_data, reg));
  for (int i = 0; i < n; i++) assert(i2c_cfg.write(i2c_cfg.user_data, v[i]));
  i2c_cfg.disconnect(i2c_cfg.user_data);
}
static void wreg(uint8_t reg, uint8_t v) { wreg_n(reg, &v, 1); }
static void rreg_n(uint8_t reg, uint8_t *v, int n) {
  tick();
  assert(i2c_cfg.connect(i2c_cfg.user_data, 0x28, false));
  assert(i2c_cfg.write(i2c_cfg.user_data, reg));
  assert(i2c_cfg.connect(i2c_cfg.user_data, 0x28, true));
  for (int i = 0; i < n; i++) v[i] = i2c_cfg.read(i2c_cfg.user_data);
  i2c_cfg.disconnect(i2c_cfg.user_data);
}
static uint8_t rreg(uint8_t reg) { uint8_t v; rreg_n(reg, &v, 1); return v; }
static void set_bits(uint8_t reg, uint8_t m) { wreg(reg, rreg(reg) | m); }
static void clr_bits(uint8_t reg, uint8_t m) { wreg(reg, rreg(reg) & (uint8_t)~m); }

static int calc_crc(const uint8_t *d, int n, uint8_t *out) {
  wreg(CommandReg, PCD_Idle);
  wreg(DivIrqReg, 0x04);
  set_bits(FIFOLevelReg, 0x80);
  wreg_n(FIFODataReg, d, n);
  wreg(CommandReg, PCD_CalcCRC);
  for (int i = 5000; !(rreg(DivIrqReg) & 0x04);)
    if (--i == 0) return STATUS_TIMEOUT;
  wreg(CommandReg, PCD_Idle);
  out[0] = rreg(CRCResultRegL);
  out[1] = rreg(CRCResultRegH);
  return STATUS_OK;
}

static int transceive(const uint8_t *send, int n, uint8_t *back, uint8_t *back_len, uint8_t *valid_bits) {
  uint8_t tx_last = valid_bits ? *valid_bits : 0;
  wreg(CommandReg, PCD_Idle);
  wreg(ComIrqReg, 0x7F);
  set_bits(FIFOLevelReg, 0x80);
  wreg_n(FIFODataReg, send, n);
  wreg(BitFramingReg, tx_last);
  wreg(CommandReg, PCD_Transceive);
  set_bits(BitFramingReg, 0x80);
  for (int i = 2000;;) {
    uint8_t irq = rreg(ComIrqReg);
    if (irq & 0x30) break;
    if (irq & 0x01) return STATUS_TIMEOUT;
    if (--i == 0) return STATUS_TIMEOUT;
  }
  uint8_t err = rreg(ErrorReg);
  if (err & 0x13) return STATUS_ERROR;
  if (back && back_len) {
    uint8_t lvl = rreg(FIFOLevelReg);
    if (lvl > *back_len) return STATUS_NO_ROOM;
    *back_len = lvl;
    rreg_n(FIFODataReg, back, lvl);
    uint8_t vb = rreg(ControlReg) & 0x07;
    if (valid_bits) *valid_bits = vb;
  }
  return (err & 0x08) ? STATUS_COLLISION : STATUS_OK;
}

static void pcd_init(void) {
  wreg(CommandReg, PCD_SoftReset);
  while (rreg(CommandReg) & 0x10) {}
  wreg(TModeReg, 0x80);
  wreg(TPrescalerReg, 0xA9);
  wreg(TReloadRegH, 0x03);
  wreg(TReloadRegL, 0xE8);
  wreg(0x15, 0x40);
  wreg(ModeReg, 0x3D);
  uint8_t v = rreg(TxControlReg);
  if ((v & 0x03) != 0x03) wreg(TxControlReg, v | 0x03);
}

static int request_a(uint8_t cmd) {
  uint8_t atqa[2], len = 2, vb = 7;
  clr_bits(CollReg, 0x80);
  int st = transceive(&cmd, 1, atqa, &len, &vb);
  if (st != STATUS_OK) return st;
  if (len != 2 || vb != 0) return STATUS_ERROR;
  assert(atqa[0] == 0x04 && atqa[1] == 0x00);
  return STATUS_OK;
}

/** PICC_Select for a 4-byte UID (cascade level 1: ANTICOLLISION, then SELECT). */
static int select_cl1(uint8_t uid[4], uint8_t *sak) {
  uint8_t buf[9] = {PICC_SEL_CL1, 0x20};
  clr_bits(CollReg, 0x80);
  wreg(BitFramingReg, 0);
  uint8_t rlen = 7, vb = 0;
  int st = transceive(buf, 2, &buf[2], &rlen, &vb);
  if (st != STATUS_OK) return st;
  assert(rlen == 5);
  buf[1] = 0x70;
  buf[6] = buf[2] ^ buf[3] ^ buf[4] ^ buf[5];
  if (calc_crc(buf, 7, &buf[7]) != STATUS_OK) return STATUS_TIMEOUT;
  wreg(BitFramingReg, 0);
  uint8_t *resp = &buf[6];
  rlen = 3;
  vb = 0;
  st = transceive(buf, 9, resp, &rlen, &vb);
  if (st != STATUS_OK) return st;
  if (rlen != 3 || vb != 0) return STATUS_ERROR;
  memcpy(uid, &buf[2], 4);
  uint8_t crc[2];
  calc_crc(resp, 1, crc);
  if (crc[0] != resp[1] || crc[1] != resp[2]) return STATUS_CRC_WRONG;
  *sak = resp[0];
  return STATUS_OK;
}

static int halt_a(void) {
  uint8_t buf[4] = {PICC_HLTA, 0};
  calc_crc(buf, 2, &buf[2]);
  int st = transceive(buf, 4, NULL, NULL, NULL);
  return st == STATUS_TIMEOUT ? STATUS_OK : STATUS_ERROR;
}

/* ---- tests ------------------------------------------------------------------- */
static void boot(void) {
  memset(timers, 0, sizeof(timers));
  ntimers = 0;
  sim_ns = 0;
  chip_init();
  pcd_init();
}
static void card(bool present, uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
  attr_val[A_UID0] = a; attr_val[A_UID0 + 1] = b; attr_val[A_UID0 + 2] = c; attr_val[A_UID0 + 3] = d;
  attr_val[A_PRESENT] = present;
}

static void test_crc_a_vectors(void) {
  // ISO/IEC 14443-3: HLTA 50 00 -> CRC 57 CD; SAK 08 -> B6 DD
  const uint8_t hlta[2] = {0x50, 0x00}, sak[1] = {0x08};
  assert(crc_a(hlta, 2, 0x6363) == 0xCD57);
  assert(crc_a(sak, 1, 0x6363) == 0xDDB6);
}

static void test_version_and_reset(void) {
  boot();
  assert(rreg(VersionReg) == 0x15);
  assert((rreg(CommandReg) & 0x10) == 0);
  assert((rreg(TxControlReg) & 0x03) == 0x03);
}

static void test_no_card_times_out(void) {
  boot();
  card(false, 0, 0, 0, 0);
  double t0 = sim_ns;
  assert(request_a(PICC_REQA) == STATUS_TIMEOUT);
  // why: TPrescaler 0xA9, TReload 0x3E8 -> 1001 * 339 / 13.56 MHz = 25 ms
  assert(sim_ns - t0 > 24e6 && sim_ns - t0 < 30e6);
}

static void test_read_uid_then_halt(void) {
  boot();
  card(true, 0xDE, 0xAD, 0xBE, 0xEF);
  assert(request_a(PICC_REQA) == STATUS_OK);
  uint8_t uid[4], sak = 0;
  assert(select_cl1(uid, &sak) == STATUS_OK);
  const uint8_t want[4] = {0xDE, 0xAD, 0xBE, 0xEF};
  assert(!memcmp(uid, want, 4) && sak == 0x08);
  assert(halt_a() == STATUS_OK);
  assert(request_a(PICC_REQA) == STATUS_TIMEOUT); /* halted: ignores REQA */
  assert(request_a(PICC_WUPA) == STATUS_OK);      /* WUPA wakes it */
}

static void test_reinsert_after_halt(void) {
  boot();
  card(true, 1, 2, 3, 4);
  uint8_t uid[4], sak;
  assert(request_a(PICC_REQA) == STATUS_OK && select_cl1(uid, &sak) == STATUS_OK && halt_a() == STATUS_OK);
  card(false, 1, 2, 3, 4);
  on_poll(i2c_cfg.user_data);
  card(true, 1, 2, 3, 4);
  assert(request_a(PICC_REQA) == STATUS_OK);
}

static void test_active_card_drops_to_idle_on_reqa(void) {
  boot();
  card(true, 1, 2, 3, 4);
  uint8_t uid[4], sak;
  assert(request_a(PICC_REQA) == STATUS_OK && select_cl1(uid, &sak) == STATUS_OK);
  assert(request_a(PICC_REQA) == STATUS_TIMEOUT); /* ACTIVE: unexpected frame -> IDLE, no answer */
  assert(request_a(PICC_REQA) == STATUS_OK);
}

static void test_antenna_off_no_answer(void) {
  boot();
  card(true, 1, 2, 3, 4);
  clr_bits(TxControlReg, 0x03);
  assert(request_a(PICC_REQA) == STATUS_TIMEOUT);
}

static void test_irq_pin(void) {
  boot();
  assert(mode[P_IRQ] == INPUT); /* IRqInv=1 default: inactive = high (released) */
  wreg(ComIEnReg, 0x80 | 0x20);  /* RxIEn */
  card(true, 1, 2, 3, 4);
  assert(request_a(PICC_REQA) == STATUS_OK);
  assert(mode[P_IRQ] == OUTPUT_LOW);
  wreg(ComIrqReg, 0x7F);
  assert(mode[P_IRQ] == INPUT);
}

int main(void) {
  test_crc_a_vectors();
  test_version_and_reset();
  test_no_card_times_out();
  test_read_uid_then_halt();
  test_reinsert_after_halt();
  test_active_card_drops_to_idle_on_reqa();
  test_antenna_off_no_answer();
  test_irq_pin();
  printf("all ws1850s host tests passed\n");
  return 0;
}
