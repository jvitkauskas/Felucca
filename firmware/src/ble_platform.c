/* SPDX-License-Identifier: GPL-3.0-only */
#pragma clang diagnostic push
#pragma clang diagnostic error "-Wall"
#pragma clang diagnostic error "-Wextra"
/* Single-unit Felucca adapter. Register access stays in hal/fm1_ble*.h.
 * Audio/TIMER5 accounting counts a BLE preemption inside TIMER5 only once. */
#include "../ble/ble_s1.h"
static ble_cal_t ble_cal;
static uint8_t ble_random_config[6],ble_started;
#include "../ble/ble_host.h"
#if FELUCCA_BLE_STAGE >= 3
#include "../ble/ble_seed.h"
#include "../ble/ble_bonds.h"
#endif
void ble_port_pend(void) { fm1_ble_worker_pend(); }
uint32_t ble_port_lock(void) { return fm1_ble_irq_save(); }
void ble_port_unlock(uint32_t key) { fm1_ble_irq_restore(key); }
void ble_port_refill(void) { bb_refill(); }
void ble_port_refill_one(void) { bb_refill_limit(1); }
void ble_port_rx_recover(void) { if(bb.state==4) bb_connected_rx(fm1_ticks(),1); }
void ble_port_window(uint32_t us) { bb_window(us); }
void ble_port_priority(unsigned p) { bb_priority(p); }
void ble_port_latency(uint16_t latency,int enabled) { bb_latency(latency,enabled); }
void ble_port_instant(uint8_t kind,uint16_t instant,const ble_conn_t *c)
{
    (void)kind; (void)c; bb_anchor(0,5,instant); bb_priority(30);
}
void ble_port_apply(uint8_t kind,const ble_conn_t *c,uint16_t widening,uint32_t window) { bb_apply(kind,c,widening,window); }
void ble_port_host_idle(void) { fm1_wdt_feed(); }
void fm1_ble_worker_irq(void)
{
    static uint32_t chain_start;
    uint32_t key=fm1_ble_irq_save(),start=fm1_ticks(),gap=start-br_diag.worker_timer_last_ticks;
    fm1_ble_worker_timer_ack(); ble_llx_running=1;
    if(ble_llx_pending) {
        ble_time_dispatch(ble_llx_request_ticks,start);
    }
    ble_llx_pending=0;
    ble_llx_request_ticks=0;
    if(br_diag.worker_timer_calls && gap>br_diag.worker_timer_gap_ticks) br_diag.worker_timer_gap_ticks=gap;
    if(br_diag.worker_timer_calls && gap>7500u*24u) br_diag.worker_timer_over_gap++;
    br_diag.worker_timer_calls++; br_diag.worker_timer_last_ticks=start;
    ble_time_pass_begin(start);
    fm1_ble_irq_restore(key);
    if(!ble_llx_testing) {
        bb_mark_event(BLE_STAGE_WORKER);
        uint32_t item=ble_time_item_begin();
        fm1_ble_service(); ble_time_item_end(item,BLE_WORK_SERVICE,0);
        if(ble_time_budget()) ble_ll_worker(); else ble_port_pend();
        if(ble_time_budget()) ble_worker(); else ble_port_pend();
    }
    key=fm1_ble_irq_save();
    ble_time_pass_end();
    ble_llx_running=0;
    if(!ble_llx_testing) {
        uint32_t delay=ble_llx_pending ? 6u : 6000u;
        if(!ble_llx_pending) chain_start=0;
        else if(!chain_start) chain_start=start;
        else if((uint32_t)(start-chain_start)>=24000000u) {
            delay=60000u; chain_start=0; br_diag.executor_starvation++;
        }
        fm1_ble_worker_timer_arm(delay);
    }
    fm1_ble_irq_restore(key);
}

uint32_t ble_port_ticks(void) { return fm1_ticks(); }
uint32_t ble_port_ms(void) { return fm1_ms; }
int ble_port_start(const ble_adv_t *adv) { return fm1_ble_adv_start(adv); }
void ble_port_update(const ble_adv_t *adv) { fm1_ble_adv_update(adv); }
void ble_port_stop_begin(void) { bb.restart=0; fm1_ble_stop_begin(); }
int ble_port_stop_poll(void) { return fm1_ble_stop_poll(); }
int ble_port_failed(void) { return br_diag.error!=0; }
void ble_port_reset_stats(void)
{
    uint32_t k=fm1_ble_irq_save();
    br_diag.adv_events=br_diag.rx_irqs=br_diag.event_irqs=0;
    br_diag.scan_req=br_diag.connect_ignored=br_diag.bad_rx=0;
    br_diag.rx_max_ticks=br_diag.event_max_ticks=br_diag.irq_over_budget=0;
    br_diag.event_work_max_ticks=br_diag.event_over_150us=0;
    br_diag.worker_gap_ticks=0; bb.last_worker=0;
    br_diag.worker_timer_calls=br_diag.worker_timer_last_ticks=0;
    br_diag.worker_timer_gap_ticks=br_diag.worker_timer_over_gap=0;
    ble_time_reset();
    fm1_ble_irq_restore(k);
}
static void ble_irq_account(uint32_t t0,int rx)
{
    uint32_t dt=fm1_ticks()-t0;
    uint32_t *max=rx ? &br_diag.rx_max_ticks : &br_diag.event_max_ticks;
    if (dt>*max) *max=dt;
    /* Event total includes optional inline RX; its separate post-RX work is
     * bounded to 150 us in the HAL. RX entry/exit has its own 380 us bound. */
    if (rx && dt>380u*24u) br_diag.irq_over_budget++;
    if (ble_audio_measuring && !ble_t5_active && !ble_worker_active) t5_nested_ticks+=dt;
}
void fm1_ble_rx_irq(void)
{
    uint32_t t=fm1_ticks(); fm1_ble_rx_body(); ble_port_pend(); ble_irq_account(t,1);
}
void fm1_ble_event_irq(void)
{
    uint32_t t=fm1_ticks(); fm1_ble_event_body(); ble_port_pend(); ble_irq_account(t,0);
}
static int ble_flash_plain(uint32_t off,uint8_t *out,uint32_t n)
{
    return flash_ok ? fm1_ble_flash_read(off,out,n) : -1;
}
#if FELUCCA_BLE_STAGE >= 3
int ble_port_storage_idle(void) { return bb.state==0 && !fm1_ble_link_active(); }
int ble_port_flash_read(uint32_t off,uint8_t *out,uint32_t n)
{
    if(!flash_ok || off<0x94000u || off>0x97000u || n>0x97000u-off) return -1;
    return fm1_ble_flash_read(off,out,n);
}
static int ble_flash_result(int rc)
{
    return rc==0 ? BLE_FLASH_OK : rc==-1 || rc==-8 ? BLE_FLASH_REFUSED :
        rc==-2 || rc==-3 ? BLE_FLASH_NOT_ISSUED : BLE_FLASH_LATE_READY;
}
int ble_port_flash_erase(uint32_t off,uint32_t *ticks)
{
    uint32_t us=0,key; int rc;
    *ticks=0;
    if(off<0x94000u || off>=0x97000u || (off&4095u)) return BLE_FLASH_REFUSED;
    if(!flash_ok) return BLE_FLASH_NOT_ISSUED;
    fm1_wdt_feed(); key=fm1_ble_icfg_read(); fm1_ble_icfg_write(key&~(1u<<8));
    if(!ble_port_storage_idle()) { fm1_ble_icfg_write(key); return BLE_FLASH_REFUSED; }
    audio_silence(); rc=FL_FAR(fl_erase4k_ram)(off,&us);
    fm1_ble_icfg_write(key); fm1_wdt_feed(); *ticks=us*24u; return ble_flash_result(rc);
}
int ble_port_flash_program(uint32_t off,const uint8_t *data,uint32_t n,uint32_t *ticks)
{
    uint32_t us=0,key; int rc;
    *ticks=0;
    if(!n || n>256 || (off&255u)+n>256u || off<0x94000u || off>0x97000u || n>0x97000u-off)
        return BLE_FLASH_REFUSED;
    if(!flash_ok) return BLE_FLASH_NOT_ISSUED;
    fm1_wdt_feed(); key=fm1_ble_icfg_read(); fm1_ble_icfg_write(key&~(1u<<8));
    if(!ble_port_storage_idle()) { fm1_ble_icfg_write(key); return BLE_FLASH_REFUSED; }
    audio_silence(); rc=FL_FAR(fl_prog_ram)(off,data,n,&us);
    fm1_ble_icfg_write(key); fm1_wdt_feed(); *ticks=us*24u; return ble_flash_result(rc);
}
#endif
static void ble_platform_irq_probe(void)
{
    uint32_t outer=fm1_ble_irq_save(),before,inner;
    br_diag.irq_probe[0]=fm1_ble_icfg_read();
    inner=fm1_ble_irq_save(); br_diag.irq_probe[1]=fm1_ble_icfg_read();
    fm1_ble_irq_restore(inner); br_diag.irq_probe[2]=fm1_ble_icfg_read();
    before=*(volatile uint32_t *)&felucca_dbg.timer_irqs;
    fm1_delay_us(250); /* two TIMER5 periods, before the first RF event */
    br_diag.irq_ticks_while_masked=*(volatile uint32_t *)&felucca_dbg.timer_irqs-before;
    fm1_ble_irq_restore(outer); br_diag.irq_probe[3]=fm1_ble_icfg_read();
    if (br_diag.irq_ticks_while_masked) fm1_ble_fail(9);
}
/* P2: persist_boot has installed the plain mapping; no audio/USB yet. */
static void ble_platform_prepare(void)
{
    ble_breadcrumb_init(&fm1_ble_breadcrumb); bb_mark(BLE_STAGE_PREPARE,0);
#ifdef FELUCCA_BLE_RANDOM_BYTES
    static const uint8_t configured[6]={FELUCCA_BLE_RANDOM_BYTES};
    memcpy(ble_random_config,configured,6);
#endif
    ble_read_cal(ble_flash_plain,&ble_cal);
#if FELUCCA_BLE_STAGE >= 3
    { uint32_t r64[2]; fm1_ble_r64_snapshot(r64);
      ble_seed_boot(ble_cal.public_addr,(uint32_t)fm1_boot.p3_rst|(fm1_boot.rst_src<<8),r64[0],r64[1]); }
#endif
    br_clock_probe();
}
/* P4, immediately after P5: both BLE CPU lines are still masked. */
static void ble_platform_start(void)
{
    ble_platform_irq_probe();
    if (!ble_port_failed()) fm1_ble_startup(&ble_cal);
    ble_init(ble_cal.public_addr,0);
    ble_host_start();
    fm1_ble_worker_timer_start(); ble_started=1; bb_mark(BLE_STAGE_READY,0);
}
static void ble_platform_worker(void)
{
    uint32_t packet,stamp;
    bb_mark_event(BLE_STAGE_HOST);
    ble_host_worker();
    for(;;) {
        uint32_t k=fm1_ble_irq_save(),cin;
        /* Keep eight input slots for USB/UART. Retain decoded BLE messages
         * for the next main yield when the shared audio-consumed ring is full. */
        if(midi_in_overflow || mi_w-mi_r>=MQ-8u || !ble_host_midi_take(&packet,&stamp)) {
            fm1_ble_irq_restore(k); break;
        }
        cin=packet&15u;
        (void)stamp; /* Felucca consumes in its next audio half, as USB/TRS do. */
        if(cin>=4u && cin<=7u) {
            unsigned n=cin==4u || cin==7u ? 3u : cin==6u ? 2u : 1u,i;
            for(i=0;i<n;i++) sysex_byte((uint8_t)(packet>>(8u*(i+1u))));
        } else midi_enqueue(packet,3u);
        fm1_ble_irq_restore(k);
    }
}

/* Main-context yield points during long panel redraws and the boot panel
 * wait. The guard forbids re-entering the host; no UI callback runs in NimBLE. */
static void ble_platform_yield(void)
{
    static uint32_t last;
    static uint8_t active;
    uint32_t now;
    if(!ble_started || active) return;
    now=fm1_ticks(); if((uint32_t)(now-last)<24000u) return;
    last=now; active=1; ble_platform_worker(); active=0;
}
static void ble_platform_delay(uint32_t ms)
{
    uint32_t start=fm1_ticks();
    while((uint32_t)(fm1_ticks()-start)<ms*24000u) ble_platform_yield();
}
#pragma clang diagnostic pop
