/* SPDX-License-Identifier: GPL-3.0-only */
#include <string.h>
#include "ble_s2_sim.h"
uint32_t sim_ticks,sim_starts,sim_stops,sim_windows,sim_applies;
int sim_stop_result=1,sim_slot[2]={-1,-1},sim_latency_enabled;
ble_adv_t sim_advertising;
static int refilling;
static uint32_t clock_last,clock_ms,clock_fraction;
uint32_t ble_port_ticks(void) { return sim_ticks; }
uint32_t ble_port_ms(void)
{
    uint32_t delta=sim_ticks-clock_last;
    clock_last=sim_ticks; clock_ms+=delta/24000u;
    clock_fraction+=delta%24000u;
    if(clock_fraction>=24000u) { clock_fraction-=24000u; clock_ms++; }
    return clock_ms;
}
int ble_port_start(const ble_adv_t *adv) { sim_advertising=*adv; sim_starts++; return 0; }
void ble_port_update(const ble_adv_t *adv) { sim_advertising=*adv; }
void ble_port_stop_begin(void) { sim_stops++; }
int ble_port_stop_poll(void) { return sim_stop_result; }
int ble_port_failed(void) { return sim_stop_result<0; }
void ble_port_reset_stats(void) { }
uint32_t ble_port_lock(void) { return 0; }
void ble_port_unlock(uint32_t key) { (void)key; }
void ble_port_refill(void)
{
    unsigned i;
    if(refilling) return;
    refilling=1;
    for(i=0;i<2;i++) if(sim_slot[i]<0) sim_slot[i]=ble_ll_tx_pick();
    refilling=0;
}
void ble_port_rx_recover(void) { }
void ble_port_window(uint32_t us) { sim_windows=us; }
void ble_port_instant(uint8_t kind,uint16_t instant,const ble_conn_t *c) { (void)kind; (void)instant; (void)c; }
void ble_port_apply(uint8_t kind,const ble_conn_t *c,uint16_t w,uint32_t win) { (void)kind; (void)c; (void)w; sim_windows=win; sim_applies++; }
void ble_port_latency(uint16_t latency,int enabled) { sim_latency_enabled=enabled && latency; }
void ble_port_priority(unsigned priority) { (void)priority; }
void ble_port_host_idle(void) { sim_advance_us(100); }
void sim_advance_us(uint32_t us) { sim_ticks+=us*24u; ble_ll_worker(); }
void sim_ack(unsigned slot)
{
    int id=sim_slot[slot]; sim_slot[slot]=-1;
    if(id>=0) ble_ll_tx_ack((unsigned)id);
    ble_port_refill();
}
void sim_reset(void)
{
    sim_ticks=24000; clock_last=sim_ticks; clock_ms=1; clock_fraction=0; sim_starts=sim_stops=sim_windows=sim_applies=0;
    sim_stop_result=1; sim_latency_enabled=0; sim_slot[0]=sim_slot[1]=-1; refilling=0; ble_ll_init();
}
ble_conn_t sim_connection(void)
{
    ble_conn_t c;
    memset(&c,0,sizeof c); c.interval=24; c.timeout=500; c.win=1; c.hop=5;
    c.aa=0x12345678; c.crc=0x123456; memset(c.map,255,4); c.map[4]=31;
    c.peer[0]=1; return c;
}
