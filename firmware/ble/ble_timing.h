/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FELUCCA_BLE_TIMING_H
#define FELUCCA_BLE_TIMING_H
#include <stdint.h>
typedef struct {
    uint32_t dispatch_max,dispatch_live_max,dispatch_negative,wall_max,own_max,item_max,item_over_100us;
    uint32_t own_over_250us,pass_over_budget,refill_max,refill_ccm_max,refill_other_max,refill_over_80us;
} ble_timing_diag_t;
void ble_time_preempt_enter(void);
void ble_time_preempt_leave(void);
uint32_t ble_time_own(void);
void ble_time_dispatch(uint32_t request,uint32_t entry);
void ble_time_pass_begin(uint32_t wall);
void ble_time_pass_end(void);
int ble_time_budget(void);
uint32_t ble_time_item_begin(void);
void ble_time_item_end(uint32_t start);
void ble_time_ccm_add(uint32_t ticks);
uint32_t ble_time_ccm_total(void);
void ble_time_refill_end(uint32_t start,uint32_t ccm_before);
void ble_time_get_diag(ble_timing_diag_t *out);
void ble_time_reset(void);
#endif
