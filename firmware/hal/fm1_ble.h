/* SPDX-License-Identifier: GPL-3.0-only */
#pragma clang diagnostic push
#pragma clang diagnostic error "-Wall"
#pragma clang diagnostic error "-Wextra"
/* BLE DMA/baseband HAL: frozen r2/r2b 02 §§2-7,9 and 04 §0. */
#pragma once
#include "fm1_ble_radio.h"

/* Measure only the controller's own outer critical sections. BSP blackouts
 * and the cumulative 390 us/920 us limit still require the PB3.1 bench probe. */
static uint32_t ble_critical_depth,ble_critical_start;
static uint32_t fm1_ble_irq_save(void)
{
    uint32_t state=fm1_icfg();
    fm1_icfg_set(state&~0x100u); br_sync();
    if (!ble_critical_depth++) ble_critical_start=fm1_ticks();
    return state;
}
static void fm1_ble_irq_restore(uint32_t state)
{
    if (!--ble_critical_depth) {
        uint32_t dt=fm1_ticks()-ble_critical_start;
        if (dt>br_diag.critical_max_ticks) br_diag.critical_max_ticks=dt;
        if (dt>390u*24u) br_diag.critical_over_390us++;
    }
    br_sync(); fm1_icfg_set(state); br_sync();
}
static uint32_t fm1_ble_icfg_read(void) { return fm1_icfg(); }
static void fm1_ble_fail(uint32_t code) { br_error(code); }
static void fm1_ble_line_enable(unsigned n)
{
    FM1_ICFG(n)|=1u<<((n&7u)*4u);
}
static struct {
    uint8_t bb[BLE_BB_SIZE];
    uint32_t modem[243],canary[4];
} ble_dma __attribute__((section(".ble_dma"),aligned(4)));
static struct {
    ble_adv_t adv;
    uint8_t frame_template[BLE_FRAME_SIZE];
    uint32_t stop_start,stop_bound,last_worker,last_canary;
    volatile uint8_t state,restart;
} bb;
enum { BB_FRAME=16, BB_RX0=340, BB_RX1=596, BB_TX0=852, BB_TX1=892 };
#define BB_F16(off) (*(volatile uint16_t *)(void *)(ble_dma.bb+BB_FRAME+(off)))
#define BB_F8(off) (*(volatile uint8_t *)(void *)(ble_dma.bb+BB_FRAME+(off)))
#define BB_F32(off) (*(volatile uint32_t *)(void *)(ble_dma.bb+BB_FRAME+(off)))

static void bb_anchor(unsigned id,unsigned index,uint32_t value)
{
    uint32_t k=fm1_ble_irq_save();
    BR_R(0x28020u)=value; br_sync(); BR_R(0x2801cu)=(index<<10)|(id<<4)|5;
    fm1_ble_irq_restore(k);
}
static void bb_zero_anchors(void)
{
    unsigned id,i;
    for (id=0;id<8;id++) for (i=0;i<17;i++) bb_anchor(id,i,0);
}
static void bb_remap(void)
{
    unsigned first=BB_F16(BF_TX_TOGGLE)&1u,j;
    for (j=0;j<2;j++) {
        unsigned slot=first^j;
        uint8_t n=(uint8_t)((j ? bb.adv.scan_len : bb.adv.data_len)+6);
        uint8_t type=(uint8_t)(j ? 4 : bb.adv.params.type==3 ? 2 : 0);
        BB_F16(BF_TX_PTR+2*slot)=(uint16_t)(j ? BB_TX1 : BB_TX0);
        BB_F16(BF_TX_ADV+2*slot)=ble_adv_header(type,bb.adv.params.own_type);
        BB_F16(BF_TX_DATA+2*slot)=(uint16_t)n<<8;
        br_sync(); BB_F8(BF_TX_CTL+slot)&=(uint8_t)~1u; br_sync();
    }
}
static void bb_frame_publish(const ble_adv_t *adv)
{
    unsigned i;
    /* Build software bytes off-DMA, then store each hardware field at its
     * specified width while stopped. The instance-table entry is still zero. */
    ble_frame_init(bb.frame_template,adv);
    for (i=0;i<100;i+=2) BB_F16(i)=ble_get16(bb.frame_template+i);
    for (i=100;i<264;i++) BB_F8(i)=bb.frame_template[i];
    for (i=264;i<288;i+=2) BB_F16(i)=ble_get16(bb.frame_template+i);
    for (i=288;i<312;i+=4) BB_F32(i)=ble_get32(bb.frame_template+i);
    for (i=312;i<320;i+=2) BB_F16(i)=ble_get16(bb.frame_template+i);
    BB_F32(320)=ble_get32(bb.frame_template+320);
}
static void bb_payloads(void)
{
    ble_adv_payload(ble_dma.bb+BB_TX0,bb.adv.addr,bb.adv.data,bb.adv.data_len);
    ble_adv_payload(ble_dma.bb+BB_TX1,bb.adv.addr,bb.adv.scan,bb.adv.scan_len);
    BB_F32(BF_POWER)=16u+bb.adv.power; BB_F32(BF_TX_SET)=16u+bb.adv.power;
    br_sync();
}
static void fm1_ble_stop_begin(void);
static void bb_rx_pending(void)
{
    unsigned slot,code,type,n;
    uint8_t *payload;
    if (!(BR_R(0x28028u)&BR_R(0x28030u)&256u)) return;
    BR_R(0x2802cu)=256; br_ssync();
    /* Closing handlers acknowledge but never publish a new payload. */
    if (bb.state!=1) return;
    slot=(BB_F16(BF_RX_TOGGLE)&1u)^1u;
    code=BB_F16(BF_RX_STAT+2*slot)&15u;
    type=BB_F16(BF_RX_ADV+2*slot)&15u;
    n=BB_F16(BF_RX_DATA+2*slot)>>8;
    payload=ble_dma.bb+(slot ? BB_RX1 : BB_RX0);
    if (code!=1) { br_diag.bad_rx++; return; }
    if (type==3 && n==12 && bb.adv.params.type==0 &&
        ((BB_F16(BF_RX_ADV+2*slot)>>7)&1u)==bb.adv.params.own_type &&
        !memcmp(payload+6,bb.adv.addr,6)) {
        unsigned i;
        br_diag.scan_req++;
        for (i=0;i<3;i++) BB_F16(BF_PEER+2*i)=ble_get16(payload+2*i);
    } else if (type==5) {
        /* S1 has no connection state. Stop future anchors immediately, then
         * quiesce/reopen from the worker, even if the request was malformed.
         * The hardware may end this advertising event on CONNECT_IND (A6). */
        br_diag.connect_ignored++;
        if (bb.state==1) { bb.restart=1; fm1_ble_stop_begin(); }
    } else if (type!=3) br_diag.bad_rx++;
    /* Advertising slots use no full flag; fixed payload buffers are retained.
     * S1 only copies the six-byte scanner address inside this interrupt. */
}
static void fm1_ble_rx_body(void)
{
    br_diag.rx_irqs++;
    bb_rx_pending();
}
static void fm1_ble_event_body(void)
{
    uint32_t pending=BR_R(0x28028u)&BR_R(0x28030u),work_start,dt;
    br_diag.event_irqs++;
    /* Equal-level arbitration must not matter: drain RX first. */
    if (pending&256u) {
        uint32_t rx_start=fm1_ticks();
        bb_rx_pending();
        dt=fm1_ticks()-rx_start;
        if (dt>br_diag.rx_max_ticks) br_diag.rx_max_ticks=dt;
        if (dt>380u*24u) br_diag.irq_over_budget++;
    }
    work_start=fm1_ticks(); /* step 1a belongs to RX, not E_ev (02 §7.3a) */
    if (pending&1u) {
        BR_R(0x2802cu)=1;
        if (bb.state==1) {
            br_diag.adv_events++;
            if (!br_diag.first_event_ticks) br_diag.first_event_ticks=fm1_ticks();
            if (!bb.restart) {
                bb_payloads(); bb_remap();
                BB_F16(BF_PRIO_CTRL)=26; BB_F16(BF_PRIO_STAT)=26;
            }
        }
    }
    if ((BR_R(0x2804cu)&BR_R(0x28050u))&1u)
        BR_R(0x2804cu)=(BR_R(0x2804cu)&255u)|256u;
    dt=fm1_ticks()-work_start;
    if (dt>br_diag.event_work_max_ticks) br_diag.event_work_max_ticks=dt;
    if (dt>150u*24u) br_diag.event_over_150us++;
    /* Sleep is never entered; do not touch its request/handshake bits. */
}
extern void isr_ble_rx(void),isr_ble_event(void);
static void fm1_ble_attach(void)
{
    uint32_t k=fm1_ble_irq_save();
    fm1_irq_attach(29,isr_ble_rx,5); fm1_irq_mask(29);
    fm1_irq_attach(45,isr_ble_event,5); fm1_irq_mask(45);
    fm1_ble_irq_restore(k);

}
static void fm1_ble_startup(ble_cal_t *cal)
{
    unsigned i;
    memset(&ble_dma,0,sizeof ble_dma);
    for (i=0;i<4;i++) ble_dma.canary[i]=0x4252444du;
    br_diag.dma_base=(uint32_t)(uintptr_t)ble_dma.bb;
    br_diag.dma_end=br_diag.dma_base+BLE_BB_SIZE;
    br_diag.modem_base=(uint32_t)(uintptr_t)ble_dma.modem;
    br_radio_init(cal,ble_dma.modem);
    br_diag.bringup_end_ticks=fm1_ticks();
    if (br_diag.error) return;
    bb_zero_anchors();
    BR_R(0x2fc80u)|=1; BR_R(0x28000u)=1; BR_R(0x28034u)=0xa01;
    BR_R(0x2fc84u)=(uint32_t)(uintptr_t)ble_dma.bb;
    BR_R(0x2fcbcu)=(uint32_t)(uintptr_t)(ble_dma.bb+BLE_BB_SIZE);
    br_field(0x28000u,0x6010,0x6000);
    br_field(0x28008u,0xf3ff,0xf046); br_field(0x2800cu,1023,70);
    br_field(0x28010u,65535,0x3e3e); br_field(0x28014u,255,30); br_field(0x28018u,65535,0x1414);
}
static int fm1_ble_adv_start(const ble_adv_t *adv)
{
    uint32_t k;
    if (br_diag.error || bb.state) return -1;
    if (BR_R(0x28038u)&2u) { br_error(6); return -1; }
    /* Closed instance: no DMA owner or enabled peripheral source. Prepare
     * the large window outside the short publication critical section. */
    memset(ble_dma.bb,0,sizeof ble_dma.bb); bb.adv=*adv; bb.restart=0;
    bb_frame_publish(adv); bb_payloads();
    BB_F16(BF_RX_PTR)=BB_RX0; BB_F16(BF_RX_PTR+2)=BB_RX1;
    BB_F8(BF_RX_CTL)&=(uint8_t)~1u; BB_F8(BF_RX_CTL+1)&=(uint8_t)~1u;
    bb_remap();
    k=fm1_ble_irq_save();
    /* P6: attach left these CPU lines masked throughout P2/P3/P4. */
    fm1_ble_line_enable(29); fm1_ble_line_enable(45);
    br_sync(); *(volatile uint16_t *)(void *)ble_dma.bb=BB_FRAME|0x8000u; br_sync();
    bb_anchor(0,1,adv->params.max); bb_anchor(0,15,0x8000);
    bb_anchor(0,2,0x2000); bb_anchor(0,8,0xc000);
    bb_anchor(0,6,ble_adv_channel_word(adv->params.channels));
    BR_R(0x2802cu)=257;
    BR_R(0x2804cu)=(BR_R(0x2804cu)&255u)|256u;
    BR_R(0x28028u)|=257; BR_R(0x2804cu)=(BR_R(0x2804cu)&255u)|1u;
    bb.state=1;
    bb_anchor(0,7,0); bb_anchor(0,0,0); bb_anchor(0,14,0);
    bb_anchor(0,0,0);
    br_diag.enable_ticks=fm1_ticks(); br_diag.first_event_ticks=0;
    bb_anchor(0,14,0x8000); BB_F16(BF_MODE)|=4;
    br_sync(); fm1_ble_irq_restore(k);
    return 0;
}
static void fm1_ble_adv_update(const ble_adv_t *adv)
{
    uint32_t k=fm1_ble_irq_save();
    bb.adv=*adv; /* shadow only; payload and power stores at event close */
    fm1_ble_irq_restore(k);
}
static void fm1_ble_stop_begin(void)
{
    uint32_t k=fm1_ble_irq_save();
    if (bb.state!=1) { fm1_ble_irq_restore(k); return; }
    bb.state=2; /* closing and refill exclusion under the same lock */
    bb_anchor(0,14,0); bb.stop_start=fm1_ticks();
    bb.stop_bound=(uint32_t)bb.adv.params.max*1250u;
    if (bb.stop_bound<20000u) bb.stop_bound=20000;
    if (bb.stop_bound>100000u) bb.stop_bound=100000;
    bb.stop_bound*=24;
    fm1_ble_irq_restore(k);
}
static int fm1_ble_stop_poll(void)
{
    uint32_t k;
    unsigned i;
    if (bb.state==3) return -1;
    if (!bb.state) return 1;
    if (bb.state!=2) return 0;
    if (BR_R(0x28038u)&2u) {
        if ((uint32_t)(fm1_ticks()-bb.stop_start)<bb.stop_bound) return 0;
        /* Quarantine retains the frame, window and every buffer. No abort. */
        bb.state=3; br_diag.stop_timeouts++; br_error(7); return -1;
    }
    k=fm1_ble_irq_save();
    BR_R(0x28028u)&=~257u;
    BR_R(0x2804cu)=(BR_R(0x2804cu)&254u)|256u; BR_R(0x2802cu)=257;
    for (i=0;i<17;i++) bb_anchor(0,i,0);
    *(volatile uint16_t *)(void *)ble_dma.bb=0; br_sync();
    fm1_ble_irq_restore(k);
    /* Reclaim only after busy cleared and the source was masked/ACKed. */
    memset(ble_dma.bb,0,sizeof ble_dma.bb); bb.state=0;
    return 1;
}
static void fm1_ble_service(void)
{
    uint32_t t=fm1_ticks(),gap=t-bb.last_worker;
    if (bb.last_worker && gap>br_diag.worker_gap_ticks) br_diag.worker_gap_ticks=gap;
    bb.last_worker=t;
    if (bb.restart && bb.state!=3) {
        if (bb.state==1) fm1_ble_stop_begin();
        if (fm1_ble_stop_poll()==1) {
            ble_adv_t adv=bb.adv;
            bb.restart=0; br_diag.restarts++;
            fm1_ble_adv_start(&adv);
        }
    }
    if ((uint32_t)(t-bb.last_canary)>=24000000u) {
        unsigned i;
        bb.last_canary=t;
        for (i=0;i<4;i++) if (ble_dma.canary[i]!=0x4252444du) {
            br_diag.canary_errors++; br_error(8);
            if (bb.state==1) fm1_ble_stop_begin();
        }
    }
}
static int fm1_ble_flash_read(uint32_t off,uint8_t *out,uint32_t n)
{
    uint32_t i;
    /* Plain XIP mapping was installed by persist_boot. No raw read, IRQ-off
     * flash window, item write, or pointer outside the 1 MiB flash. */
    if (off<0x93000u || off>0x100000u || n>0x100000u-off) return -1;
    for (i=0;i<n;i++) out[i]=*(const volatile uint8_t *)(uintptr_t)FL_XIP(off+i);
    return 0;
}
static void fm1_ble_readback(uint32_t *out)
{
    unsigned i;
    for (i=0;i<31;i++) out[i]=BR_A(i);
    for (i=0;i<8;i++) out[31+i]=BR_R(0x14000u+4*i);
    for (i=0;i<10;i++) out[39+i]=BR_R(0x14040u+4*i);
    out[49]=BR_R(0x20000u);
}
#undef BB_F16
#undef BB_F8
#undef BB_F32

#pragma clang diagnostic pop
