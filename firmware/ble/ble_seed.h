/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FELUCCA_BLE_SEED_H
#define FELUCCA_BLE_SEED_H
#include <stdint.h>
enum { BLE_SEED_FAILED, BLE_SEED_FIRST_BOOT, BLE_SEED_CHAINED, BLE_SEED_PROVISIONED, BLE_SEED_UNPERSISTED };
enum { BLE_PERSIST_STORED, BLE_PERSIST_NOT_STORED, BLE_PERSIST_UNVERIFIED };
enum { BLE_FLASH_OK, BLE_FLASH_REFUSED, BLE_FLASH_NOT_ISSUED, BLE_FLASH_LATE_READY };
typedef struct {
    uint32_t sequence,malformed,conflicts,rand_calls,bytes,reseed_attempts,flash_failures;
    uint8_t state,origin,persistence,flags; uint16_t credit;
} ble_seed_diag_t;
int ble_port_flash_read(uint32_t off,uint8_t *out,uint32_t n);
int ble_port_flash_erase(uint32_t off,uint32_t *ticks);
int ble_port_flash_program(uint32_t off,const uint8_t *p,uint32_t n,uint32_t *ticks);
int ble_port_storage_idle(void);
uint32_t ble_crc32(const uint8_t *p,unsigned n);
int ble_seed_record_valid(const uint8_t record[64]);
void ble_seed_boot(const uint8_t address[6],uint32_t reset,uint32_t r64lo,uint32_t r64hi);
int ble_entropy(uint8_t *out,unsigned n);
void ble_entropy_absorb(uint8_t label,const void *data,unsigned n);
void ble_entropy_connection(void);
void ble_entropy_sample(uint32_t ticks,uint32_t rssi,int event);
void ble_entropy_poll(void);
void ble_seed_rand_called(void);
void ble_seed_reset_stats(void);
void ble_seed_get_diag(ble_seed_diag_t *out);
#endif
