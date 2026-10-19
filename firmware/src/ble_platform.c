/* SPDX-License-Identifier: GPL-3.0-only */
#pragma clang diagnostic push
#pragma clang diagnostic error "-Wall"
#pragma clang diagnostic error "-Wextra"
/* Single-unit Felucca adapter. Register access stays in hal/fm1_ble*.h.
 * Audio/TIMER5 accounting counts a BLE preemption inside TIMER5 only once. */
#include "../ble/ble_s1.h"
static ble_cal_t ble_cal;
static uint8_t ble_random_config[6],ble_start_pending;

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
    if (ble_audio_measuring && !ble_t5_active) t5_nested_ticks+=dt;
}
void fm1_ble_rx_irq(void)
{
    uint32_t t=fm1_ticks(); fm1_ble_rx_body(); ble_irq_account(t,1);
}
void fm1_ble_event_irq(void)
{
    uint32_t t=fm1_ticks(); fm1_ble_event_body(); ble_irq_account(t,0);
}
static int ble_flash_plain(uint32_t off,uint8_t *out,uint32_t n)
{
    return flash_ok ? fm1_ble_flash_read(off,out,n) : -1;
}
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
#ifdef FELUCCA_BLE_RANDOM_BYTES
    static const uint8_t configured[6]={FELUCCA_BLE_RANDOM_BYTES};
    memcpy(ble_random_config,configured,6);
#endif
    ble_read_cal(ble_flash_plain,&ble_cal);
    br_clock_probe();
}
/* P4, immediately after P5: both BLE CPU lines are still masked. */
static void ble_platform_start(void)
{
    ble_platform_irq_probe();
    if (!ble_port_failed()) fm1_ble_startup(&ble_cal);
    ble_init(ble_cal.public_addr,0);
    ble_driver_start(0,ble_random_config);
    ble_start_pending=1;
}
static void ble_platform_worker(void)
{
    unsigned i,n=ble_start_pending ? 16u : 1u;
    ble_start_pending=0;
    /* P6 first main pass: prime the finite cold-start sequence before the
     * panel's boot delay/redraw. Each pass processes at most one HCI command. */
    for (i=0;i<n;i++) { fm1_ble_service(); ble_worker(); }
}

#pragma clang diagnostic pop
