/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef BLE_S3_SIM_H
#define BLE_S3_SIM_H
#include "ble_s2_sim.h"
extern uint8_t sim_flash[12288];
extern int sim_flash_fault,sim_flash_idle,sim_flash_mismatch,sim_aes_fault;
extern unsigned sim_erases[3],sim_programs[3];
extern uint32_t sim_sha_compressions,sim_sha_cost_ticks;
void sim_storage_reset(void);
void sim_seed(void);
#endif
