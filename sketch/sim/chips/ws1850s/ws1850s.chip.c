// SPDX-FileCopyrightText: © 2026 Stephan Wissel
// SPDX-License-Identifier: MIT
//
// WS1850S 13.56 MHz RFID reader (MFRC522-compatible, I2C) - Wokwi custom chip.
//
// Models the MFRC522 register interface as seen over I2C (FIFO, command
// register, interrupt/error/status registers, CRC coprocessor, timer, IRQ pin)
// plus one ISO/IEC 14443-3 type A card in the field: REQA/WUPA, cascade level 1
// anticollision, SELECT and HLTA, with the IDLE/READY/ACTIVE/HALT state machine.
// The card is controlled by the part's controls `cardPresent` and `uid0..uid3`.
//
// See README.md for the supported commands and limitations.

#include "wokwi-api.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FIFO_SIZE 64
#define POLL_US 10000
#define FC_HZ 13560000.0 /* carrier frequency, clocks the timer prescaler */

/* --- MFRC522 registers (datasheet rev. 3.9, chapter 9) ------------------------ */
#define CommandReg 0x01
#define ComIEnReg 0x02
#define DivIEnReg 0x03
#define ComIrqReg 0x04
#define DivIrqReg 0x05
#define ErrorReg 0x06
#define Status1Reg 0x07
#define Status2Reg 0x08
#define FIFODataReg 0x09
#define FIFOLevelReg 0x0A
#define WaterLevelReg 0x0B
#define ControlReg 0x0C
#define BitFramingReg 0x0D
#define CollReg 0x0E
#define ModeReg 0x11
#define TxModeReg 0x12
#define RxModeReg 0x13
#define TxControlReg 0x14
#define DemodReg 0x19
#define CRCResultRegH 0x21
#define CRCResultRegL 0x22
#define TModeReg 0x2A
#define TPrescalerReg 0x2B
#define TReloadRegH 0x2C
#define TReloadRegL 0x2D
#define VersionReg 0x37

/* commands (CommandReg[3:0]) */
#define PCD_Idle 0x0
#define PCD_Mem 0x1
#define PCD_GenerateRandomID 0x2
#define PCD_CalcCRC 0x3
#define PCD_Transmit 0x4
#define PCD_NoCmdChange 0x7
#define PCD_Receive 0x8
#define PCD_Transceive 0xC
#define PCD_MFAuthent 0xE
#define PCD_SoftReset 0xF

/* ComIrqReg bits */
#define IRQ_TX 0x40
#define IRQ_RX 0x20
#define IRQ_IDLE 0x10
#define IRQ_ERR 0x02
#define IRQ_TIMER 0x01
/* DivIrqReg bits */
#define DIVIRQ_CRC 0x04
#define DIVIRQ_MASK 0x14
/* ErrorReg bits */
#define ERR_BUFFER_OVFL 0x10
#define ERR_CRC 0x04
#define ERR_PROTOCOL 0x01

/* ISO/IEC 14443-3 type A */
#define PICC_REQA 0x26
#define PICC_WUPA 0x52
#define PICC_SEL_CL1 0x93
#define PICC_HLTA 0x50

/** Card state (ISO/IEC 14443-3 figure 7). */
typedef enum { PICC_IDLE, PICC_READY, PICC_ACTIVE, PICC_HALT } picc_state_t;

/** Complete chip state (one instance per part). */
typedef struct {
  pin_t irq, rst;
  uint8_t regs[64];
  uint8_t fifo[FIFO_SIZE];
  uint8_t fifo_len;
  uint8_t mem[25]; /* internal buffer of the Mem command */
  uint8_t rx_last_bits;
  bool crc_ready;
  bool timer_running;
  bool debug;

  /* card */
  uint32_t attr_present, attr_uid[4];
  uint8_t sak;
  uint16_t atqa;
  picc_state_t picc;
  bool present;
  uint8_t uid[4];

  /* I2C */
  uint8_t ptr;
  bool ptr_pending;

  timer_t poll, rf_timer;
} chip_t;

/* ---------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------- */

/** CRC_A (ISO/IEC 14443-3 annex B) with the given preset; result low byte first on air. */
static uint16_t crc_a(const uint8_t *data, uint32_t len, uint16_t preset) {
  uint16_t crc = preset;
  for (uint32_t i = 0; i < len; i++) {
    uint8_t b = data[i] ^ (uint8_t)(crc & 0xff);
    b ^= (uint8_t)(b << 4);
    crc = (uint16_t)((crc >> 8) ^ ((uint16_t)b << 8) ^ ((uint16_t)b << 3) ^ (b >> 4));
  }
  return crc;
}

/** CRC preset selected by ModeReg CRCPreset[1:0]. */
static uint16_t crc_preset(const chip_t *c) {
  static const uint16_t presets[4] = {0x0000, 0x6363, 0xA671, 0xFFFF};
  return presets[c->regs[ModeReg] & 0x03];
}

/** True if the last two bytes of `frame` are its CRC_A (preset 6363h, as on air). */
static bool crc_ok(const uint8_t *frame, uint32_t len) {
  if (len < 3) return false;
  uint16_t crc = crc_a(frame, len - 2, 0x6363);
  return frame[len - 2] == (crc & 0xff) && frame[len - 1] == (crc >> 8);
}

/** Append bytes to the FIFO; overflow sets ErrorReg.BufferOvfl. */
static void fifo_push(chip_t *c, const uint8_t *data, uint32_t len) {
  for (uint32_t i = 0; i < len; i++) {
    if (c->fifo_len == FIFO_SIZE) {
      c->regs[ErrorReg] |= ERR_BUFFER_OVFL;
      return;
    }
    c->fifo[c->fifo_len++] = data[i];
  }
}

/** Remove and return the oldest FIFO byte (0 if empty). */
static uint8_t fifo_pop(chip_t *c) {
  if (c->fifo_len == 0) return 0;
  uint8_t v = c->fifo[0];
  memmove(c->fifo, c->fifo + 1, --c->fifo_len);
  return v;
}

/** Drive the IRQ pin from the enabled interrupt requests (polarity and driver per ComIEn/DivIEn bit 7). */
static void update_irq(chip_t *c) {
  bool active = (c->regs[ComIrqReg] & c->regs[ComIEnReg] & 0x7f) || (c->regs[DivIrqReg] & c->regs[DivIEnReg] & DIVIRQ_MASK);
  bool inverted = (c->regs[ComIEnReg] & 0x80) != 0; /* IRqInv: active low */
  bool push_pull = (c->regs[DivIEnReg] & 0x80) != 0;
  bool level = inverted ? !active : active;
  if (push_pull) pin_mode(c->irq, level ? OUTPUT_HIGH : OUTPUT_LOW);
  else pin_mode(c->irq, level ? INPUT : OUTPUT_LOW); /* open drain */
}

/** Set ComIrqReg bits and refresh the IRQ pin. */
static void raise(chip_t *c, uint8_t bits) {
  c->regs[ComIrqReg] |= bits;
  update_irq(c);
}

/** Register values after power-on, hard reset (NRSTPD) or SoftReset. */
static void reset_registers(chip_t *c, uint8_t version) {
  static const struct { uint8_t reg, val; } defaults[] = {
      {CommandReg, 0x20}, {ComIEnReg, 0x80}, {ComIrqReg, 0x14}, {Status1Reg, 0x21}, {WaterLevelReg, 0x08},
      {ControlReg, 0x10}, {CollReg, 0xA0}, {ModeReg, 0x3F}, {TxControlReg, 0x80}, {0x16, 0x10},
      {0x17, 0x84}, {0x18, 0x84}, {DemodReg, 0x4D}, {0x1C, 0x62}, {0x1F, 0xEB},
      {CRCResultRegH, 0xFF}, {CRCResultRegL, 0xFF}, {0x24, 0x26}, {0x26, 0x48}, {0x27, 0x88},
      {0x28, 0x20}, {0x29, 0x20}, {0x36, 0x40}};
  memset(c->regs, 0, sizeof(c->regs));
  for (size_t i = 0; i < sizeof(defaults) / sizeof(defaults[0]); i++) c->regs[defaults[i].reg] = defaults[i].val;
  c->regs[VersionReg] = version;
  c->fifo_len = 0;
  c->rx_last_bits = 0;
  c->crc_ready = false;
  c->timer_running = false;
  timer_stop(c->rf_timer);
  update_irq(c);
}

/* ---------------------------------------------------------------------------
 * Timer
 * ------------------------------------------------------------------------- */

/** Start the reader's timer: period = (TReload + 1) * (2 * TPrescaler + 1 [+1 if TPrescalEven]) / 13.56 MHz. */
static void timer_begin(chip_t *c) {
  uint32_t pre = ((uint32_t)(c->regs[TModeReg] & 0x0F) << 8) | c->regs[TPrescalerReg];
  uint32_t reload = ((uint32_t)c->regs[TReloadRegH] << 8) | c->regs[TReloadRegL];
  double div = 2.0 * pre + ((c->regs[DemodReg] & 0x10) ? 2.0 : 1.0);
  double ns = (reload + 1.0) * div / FC_HZ * 1e9;
  c->timer_running = true;
  timer_start_ns(c->rf_timer, (uint64_t)ns, false);
}

/** Timer underflow: TimerIRq; restarts if TAutoRestart is set. */
static void on_rf_timer(void *user_data) {
  chip_t *c = user_data;
  c->timer_running = false;
  if (c->debug) printf("ws1850s: timer irq\n");
  raise(c, IRQ_TIMER);
  if (c->regs[TModeReg] & 0x10) timer_begin(c);
}

/* ---------------------------------------------------------------------------
 * Card (PICC) model
 * ------------------------------------------------------------------------- */

/** Read the card controls; removal powers the card down, a new UID is a different card. */
static void refresh_card(chip_t *c) {
  bool present = attr_read(c->attr_present) != 0;
  uint8_t uid[4];
  for (int i = 0; i < 4; i++) uid[i] = (uint8_t)attr_read(c->attr_uid[i]);
  if (present != c->present || memcmp(uid, c->uid, 4) != 0) {
    c->picc = PICC_IDLE;
    if (c->debug) printf("ws1850s: card %s %02X%02X%02X%02X\n", present ? "in field" : "removed", uid[0], uid[1], uid[2], uid[3]);
  }
  c->present = present;
  memcpy(c->uid, uid, 4);
}

/** Periodic control poll (card insertion/removal/UID changes between transactions). */
static void on_poll(void *user_data) { refresh_card(user_data); }

/**
 * Process one frame sent to the card. Returns the response length in bytes (0 = no
 * response) and sets *resp_last_bits (valid bits in the last byte, 0 = all 8).
 */
static uint32_t picc_process(chip_t *c, const uint8_t *f, uint32_t len, uint8_t last_bits, uint8_t *resp,
                             uint8_t *resp_last_bits) {
  refresh_card(c);
  *resp_last_bits = 0;
  if (!c->present || len == 0) return 0;

  if (last_bits == 7 && len == 1) { /* short frame */
    bool wake = f[0] == PICC_WUPA && (c->picc == PICC_IDLE || c->picc == PICC_HALT);
    bool req = f[0] == PICC_REQA && c->picc == PICC_IDLE;
    if (wake || req) {
      c->picc = PICC_READY;
      resp[0] = (uint8_t)(c->atqa & 0xff);
      resp[1] = (uint8_t)(c->atqa >> 8);
      return 2;
    }
    if (c->picc != PICC_HALT) c->picc = PICC_IDLE;
    return 0;
  }

  if (c->picc == PICC_READY && f[0] == PICC_SEL_CL1) {
    uint8_t bcc = c->uid[0] ^ c->uid[1] ^ c->uid[2] ^ c->uid[3];
    if (len == 2 && f[1] == 0x20 && last_bits == 0) { /* ANTICOLLISION, no known bits */
      memcpy(resp, c->uid, 4);
      resp[4] = bcc;
      return 5;
    }
    if (len == 9 && f[1] == 0x70 && !memcmp(&f[2], c->uid, 4) && f[6] == bcc && crc_ok(f, 9)) { /* SELECT */
      c->picc = PICC_ACTIVE;
      resp[0] = c->sak;
      uint16_t crc = crc_a(resp, 1, 0x6363);
      resp[1] = (uint8_t)(crc & 0xff);
      resp[2] = (uint8_t)(crc >> 8);
      if (c->debug) printf("ws1850s: card selected\n");
      return 3;
    }
  } else if (c->picc == PICC_ACTIVE && len == 4 && f[0] == PICC_HLTA && f[1] == 0x00 && crc_ok(f, 4)) {
    c->picc = PICC_HALT;
    if (c->debug) printf("ws1850s: card halted\n");
    return 0;
  }
  // why: ISO/IEC 14443-3 - any unexpected frame sends a READY/ACTIVE card back to IDLE silently
  if (c->picc == PICC_READY || c->picc == PICC_ACTIVE) c->picc = PICC_IDLE;
  return 0;
}

/* ---------------------------------------------------------------------------
 * Reader (PCD) commands
 * ------------------------------------------------------------------------- */

/** Send the FIFO content to the card; with `receive`, place the answer in the FIFO or start the timeout. */
static void transmit(chip_t *c, bool receive) {
  uint8_t frame[FIFO_SIZE + 2];
  uint32_t len = c->fifo_len;
  memcpy(frame, c->fifo, len);
  c->fifo_len = 0;
  uint8_t last_bits = c->regs[BitFramingReg] & 0x07;
  if ((c->regs[TxModeReg] & 0x80) && len > 0) { /* TxCRCEn: append CRC_A */
    uint16_t crc = crc_a(frame, len, crc_preset(c));
    frame[len++] = (uint8_t)(crc & 0xff);
    frame[len++] = (uint8_t)(crc >> 8);
  }
  c->regs[ErrorReg] = 0;
  raise(c, IRQ_TX);

  uint8_t resp[16];
  uint8_t resp_bits = 0;
  bool field_on = (c->regs[TxControlReg] & 0x03) != 0;
  uint32_t n = field_on ? picc_process(c, frame, len, last_bits, resp, &resp_bits) : 0;
  if (c->debug) printf("ws1850s: tx %u byte(s) %02X.., rx %u\n", (unsigned)len, len ? frame[0] : 0, (unsigned)n);

  if (!receive) {
    c->regs[CommandReg] &= 0xF0;
    raise(c, IRQ_IDLE);
    return;
  }
  if (n == 0) {
    if (c->regs[TModeReg] & 0x80) timer_begin(c); /* TAuto: timer starts at the end of transmission */
    return;
  }
  if (c->regs[RxModeReg] & 0x80) { /* RxCRCEn: check and strip CRC_A */
    if (n < 3 || crc_a(resp, n - 2, crc_preset(c)) != (uint16_t)(resp[n - 2] | (resp[n - 1] << 8))) {
      c->regs[ErrorReg] |= ERR_CRC;
    }
    if (n >= 2) n -= 2;
  }
  fifo_push(c, resp, n);
  c->rx_last_bits = resp_bits;
  raise(c, IRQ_RX | (c->regs[ErrorReg] ? IRQ_ERR : 0));
}

/** Execute a command written to CommandReg. */
static void run_command(chip_t *c, uint8_t cmd) {
  switch (cmd) {
  case PCD_Idle:
  case PCD_Receive:     /* nothing will arrive unprompted */
  case PCD_Transceive:  /* waits for BitFramingReg.StartSend */
  case PCD_NoCmdChange:
    break;
  case PCD_Mem: {
    for (int i = 0; i < 25; i++) c->mem[i] = fifo_pop(c);
    c->regs[CommandReg] &= 0xF0;
    raise(c, IRQ_IDLE);
    break;
  }
  case PCD_GenerateRandomID:
    c->regs[CommandReg] &= 0xF0;
    raise(c, IRQ_IDLE);
    break;
  case PCD_CalcCRC: {
    // why: the CRC coprocessor consumes the FIFO; the command stays active until Idle
    uint16_t crc = crc_a(c->fifo, c->fifo_len, crc_preset(c));
    c->fifo_len = 0;
    c->regs[CRCResultRegL] = (uint8_t)(crc & 0xff);
    c->regs[CRCResultRegH] = (uint8_t)(crc >> 8);
    c->crc_ready = true;
    c->regs[DivIrqReg] |= DIVIRQ_CRC;
    update_irq(c);
    break;
  }
  case PCD_Transmit:
    transmit(c, false);
    break;
  case PCD_MFAuthent: /* MIFARE Crypto1 is not modelled */
    c->regs[ErrorReg] |= ERR_PROTOCOL;
    c->regs[CommandReg] &= 0xF0;
    raise(c, IRQ_ERR | IRQ_IDLE);
    break;
  case PCD_SoftReset:
    reset_registers(c, c->regs[VersionReg]);
    break;
  default:
    break;
  }
}

/** Store one written register byte with the side effects of the real register. */
static void write_reg(chip_t *c, uint8_t reg, uint8_t v) {
  switch (reg) {
  case CommandReg: {
    uint8_t cmd = v & 0x0F;
    if (cmd == PCD_NoCmdChange) cmd = c->regs[CommandReg] & 0x0F;
    c->regs[CommandReg] = (uint8_t)((v & 0x30) | cmd);
    if (cmd != PCD_CalcCRC) c->crc_ready = false;
    if ((v & 0x0F) != PCD_NoCmdChange) run_command(c, cmd);
    break;
  }
  case ComIrqReg: /* Set1: 1 sets the marked bits, 0 clears them */
    if (v & 0x80) c->regs[reg] |= v & 0x7f;
    else c->regs[reg] &= (uint8_t)~(v & 0x7f);
    update_irq(c);
    break;
  case DivIrqReg: /* Set2 */
    if (v & 0x80) c->regs[reg] |= v & DIVIRQ_MASK;
    else c->regs[reg] &= (uint8_t)~(v & DIVIRQ_MASK);
    update_irq(c);
    break;
  case ComIEnReg:
  case DivIEnReg:
    c->regs[reg] = v;
    update_irq(c);
    break;
  case ErrorReg: case Status1Reg: case VersionReg: case CRCResultRegH: case CRCResultRegL:
    break; /* read-only */
  case FIFODataReg:
    fifo_push(c, &v, 1);
    break;
  case FIFOLevelReg:
    if (v & 0x80) { /* FlushBuffer */
      c->fifo_len = 0;
      c->regs[ErrorReg] &= (uint8_t)~ERR_BUFFER_OVFL;
    }
    break;
  case ControlReg:
    if (v & 0x80) { /* TStopNow */
      timer_stop(c->rf_timer);
      c->timer_running = false;
    }
    if (v & 0x40) timer_begin(c); /* TStartNow */
    c->regs[reg] = v & 0x30;
    break;
  case BitFramingReg:
    c->regs[reg] = v & 0x7f;
    if ((v & 0x80) && (c->regs[CommandReg] & 0x0F) == PCD_Transceive) transmit(c, true); /* StartSend */
    break;
  default:
    c->regs[reg] = v;
    break;
  }
}

/** Read one register with the side effects of the real register (FIFO pop, live status). */
static uint8_t read_reg(chip_t *c, uint8_t reg) {
  switch (reg) {
  case FIFODataReg:
    return fifo_pop(c);
  case FIFOLevelReg:
    return c->fifo_len;
  case ControlReg:
    return (uint8_t)(c->regs[ControlReg] | c->rx_last_bits);
  case Status1Reg: {
    uint8_t water = c->regs[WaterLevelReg] & 0x3f;
    uint8_t s = 0;
    if (c->regs[ComIrqReg] & c->regs[ComIEnReg] & 0x7f) s |= 0x10; /* IRq */
    if (c->crc_ready) s |= 0x20 | 0x40;                           /* CRCReady, CRCOk */
    if (c->timer_running) s |= 0x08;                              /* TRunning */
    if (c->fifo_len >= FIFO_SIZE - water) s |= 0x02;              /* HiAlert */
    if (c->fifo_len <= water) s |= 0x01;                          /* LoAlert */
    return s;
  }
  default:
    return c->regs[reg & 0x3f];
  }
}

/* ---------------------------------------------------------------------------
 * I2C device (no address auto-increment: multi-byte accesses stay on one register)
 * ------------------------------------------------------------------------- */

/** I2C START/repeated START addressed to the part. */
static bool on_i2c_connect(void *user_data, uint32_t address, bool read) {
  (void)address;
  chip_t *c = user_data;
  if (pin_read(c->rst) == LOW) return false; /* hard power-down */
  if (!read) c->ptr_pending = true;
  return true;
}

/** One byte read from the addressed register. */
static uint8_t on_i2c_read(void *user_data) {
  chip_t *c = user_data;
  return read_reg(c, c->ptr);
}

/** One byte written: the first is the register address, the rest go to that register. */
static bool on_i2c_write(void *user_data, uint8_t data) {
  chip_t *c = user_data;
  if (c->ptr_pending) {
    c->ptr = data & 0x3f;
    c->ptr_pending = false;
    return true;
  }
  write_reg(c, c->ptr, data);
  return true;
}

/** I2C STOP. */
static void on_i2c_disconnect(void *user_data) { (void)user_data; }

/** NRSTPD (RST) low powers the part down; the rising edge is a hard reset. */
static void on_rst(void *user_data, pin_t pin, uint32_t value) {
  (void)pin;
  chip_t *c = user_data;
  if (value == HIGH) reset_registers(c, c->regs[VersionReg]);
}

/** Wokwi entry point: attributes, card controls, pins, I2C device, timers. */
void chip_init(void) {
  chip_t *c = calloc(1, sizeof(chip_t));
  uint8_t version = (uint8_t)attr_read(attr_init("version", 0x15));
  uint32_t address = attr_read(attr_init("address", 0x28));
  c->sak = (uint8_t)attr_read(attr_init("sak", 0x08));
  c->atqa = (uint16_t)attr_read(attr_init("atqa", 0x0004));
  c->debug = attr_read(attr_init("debug", 0)) != 0;
  c->attr_present = attr_init("cardPresent", 0);
  static const char *uid_names[4] = {"uid0", "uid1", "uid2", "uid3"};
  for (int i = 0; i < 4; i++) c->attr_uid[i] = attr_init(uid_names[i], 0);

  c->irq = pin_init("IRQ", INPUT);
  c->rst = pin_init("RST", INPUT_PULLUP);
  const pin_watch_config_t w = {.user_data = c, .edge = BOTH, .pin_change = on_rst};
  pin_watch(c->rst, &w);

  const timer_config_t rt = {.user_data = c, .callback = on_rf_timer};
  c->rf_timer = timer_init(&rt);
  reset_registers(c, version);
  refresh_card(c);

  const i2c_config_t i2c = {.user_data = c,
                            .address = address,
                            .scl = pin_init("SCL", INPUT),
                            .sda = pin_init("SDA", INPUT),
                            .connect = on_i2c_connect,
                            .read = on_i2c_read,
                            .write = on_i2c_write,
                            .disconnect = on_i2c_disconnect};
  i2c_init(&i2c);

  const timer_config_t pt = {.user_data = c, .callback = on_poll};
  c->poll = timer_init(&pt);
  timer_start(c->poll, POLL_US, true);
  printf("ws1850s: ready (I2C 0x%02X, version 0x%02X)\n", (unsigned)address, version);
}
