/**
 * @file sim_shims.h
 * @brief Sim-only workarounds for the virtual M5Dial (Wokwi). Compiled only with
 *        -DM5DD_SIM (set by tools/sim.sh). Task 4 moves this into m5dd::begin.
 */
#pragma once
#ifdef M5DD_SIM
#include <esp_rom_gpio.h>
#include <soc/gpio_sig_map.h>

/**
 * Route the panel's shared data line (GPIO5) to the SPI2 MISO input (FSPIQ) as well.
 * Call before M5Dial.begin().
 *
 * why: M5GFX reads the M5Dial panel ID over 3-wire SPI (SPI_SIO; data comes back on
 * MOSI/FSPID, pin_miso = -1). Wokwi's ESP32-S3 SPI samples FSPIQ even in SIO mode,
 * so the RDDID read returns 00000000 and the Dial autodetect fails. Evidence (CI):
 * run 40946628850 (probe: 3-wire 00000000, 4-wire with MISO=GPIO5 00019a00),
 * run 40948213333 (with this route: board_M5Dial detected). Real hardware ignores
 * FSPIQ in SIO mode; see docs/PORTING_NOTES.md.
 */
inline void m5dd_sim_route_panel_miso() {
    esp_rom_gpio_connect_in_signal(GPIO_NUM_5, FSPIQ_IN_IDX, false);
}
#endif
