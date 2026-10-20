/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef BLE_S2_SIM_H
#define BLE_S2_SIM_H
extern int sim_executor_enabled;
extern void (*sim_lock_hook)(void);
#include "../firmware/ble/ble_link.h"
extern uint32_t sim_ticks,sim_starts,sim_stops,sim_windows,sim_applies;
extern int sim_stop_result,sim_slot[2],sim_latency_enabled;
extern ble_adv_t sim_advertising;
#if FELUCCA_BLE_STAGE >= 3
extern uint8_t sim_air[2][31],sim_air_len[2];
#endif
void sim_reset(void);
void sim_advance_us(uint32_t us);
void sim_ack(unsigned slot);
ble_conn_t sim_connection(void);
#endif
