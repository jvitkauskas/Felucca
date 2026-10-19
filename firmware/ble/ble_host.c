/* SPDX-License-Identifier: GPL-3.0-only */
/* NimBLE host and application: all functions except the two SPSC ring entries
 * execute exclusively in Felucca's main loop, never in a timer/radio ISR. */
#include <string.h>
#include "os/os.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_store.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "nimble/nimble_port.h"
#include "nimble/transport.h"
#include "ble_s1.h"
#include "ble_link.h"
#include "ble_host.h"
#include "ble_midi.h"
#define MIDI_Q 128u
static struct {
    uint32_t pkt[MIDI_Q],ms[MIDI_Q];
    volatile uint32_t w,r;
} midi_rx __attribute__((section(".pool.ble"))),midi_tx __attribute__((section(".pool.ble")));
static ble_host_diag_t diag;
static ble_midi_decoder_t decoder;
static ble_midi_usb_t usb_decoder;
static uint16_t midi_handle;
static uint32_t last_main,restart_at,update_at;
static uint8_t update_due;
static uint8_t advertise_pending;
static const ble_uuid128_t midi_service=BLE_UUID128_INIT(0x00,0xc7,0xc4,0x4e,0xe3,0x6c,0x51,0xa7,0x33,0x4b,0xe8,0xed,0x5a,0x0e,0xb8,0x03);
static const ble_uuid128_t midi_characteristic=BLE_UUID128_INIT(0xf3,0x6b,0x10,0x9d,0x66,0xf2,0xa9,0xa1,0x12,0x41,0x68,0x38,0xdb,0xe5,0x72,0x77);
extern void os_msys_init(void);
extern void os_mempool_module_init(void);
extern void ble_npl_poll(void);
extern void ble_heap_diag(uint32_t *out);
void ble_port_host_idle(void);
static void usb_received(uint32_t pkt,uint16_t ts,void *arg)
{
    uint32_t i=midi_rx.w%MIDI_Q;
    (void)arg;
    if(midi_rx.w-midi_rx.r==MIDI_Q) { diag.midi_drop++; return; }
    midi_rx.pkt[i]=pkt; midi_rx.ms[i]=ts;
    __asm__ volatile("" ::: "memory"); midi_rx.w++;
    if(diag.echo) ble_host_midi_out(pkt,ts);
}
static void byte_received(uint8_t byte,uint16_t ts,void *arg) { ble_midi_usb_byte(&usb_decoder,byte,ts,usb_received,arg); }
static int midi_access(uint16_t conn,uint16_t attr,struct ble_gatt_access_ctxt *ctxt,void *arg)
{
    uint8_t packet[200]; uint16_t n;
    (void)conn; (void)attr; (void)arg;
    if(ctxt->op==BLE_GATT_ACCESS_OP_READ_CHR) return 0;
    if(ctxt->op!=BLE_GATT_ACCESS_OP_WRITE_CHR) return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
    n=OS_MBUF_PKTLEN(ctxt->om);
    if(n>sizeof packet || os_mbuf_copydata(ctxt->om,0,n,packet)) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    if(!ble_midi_decode(&decoder,packet,n,byte_received,0)) { diag.midi_bad++; return BLE_ATT_ERR_UNLIKELY; }
    diag.midi_in++; return 0;
}
static const struct ble_gatt_chr_def midi_chars[]={
    { .uuid=&midi_characteristic.u,.access_cb=midi_access,.val_handle=&midi_handle,
      .flags=BLE_GATT_CHR_F_READ|BLE_GATT_CHR_F_WRITE_NO_RSP|BLE_GATT_CHR_F_NOTIFY },
    { .uuid=0 }
};
static const struct ble_gatt_svc_def services[]={
    { .type=BLE_GATT_SVC_TYPE_PRIMARY,.uuid=&midi_service.u,.characteristics=midi_chars }, { .type=0 }
};
static int gap_event(struct ble_gap_event *event,void *arg)
{
    (void)arg;
    switch(event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if(!event->connect.status) { diag.connection=event->connect.conn_handle; diag.mtu=23; update_due=1; update_at=ble_port_ms()+1000u; }
        else { advertise_pending=1; restart_at=ble_port_ms()+100u; }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        diag.connection=BLE_HS_CONN_HANDLE_NONE; diag.subscribed=0; update_due=0;
        memset(&decoder,0,sizeof decoder); memset(&usb_decoder,0,sizeof usb_decoder);
        midi_tx.r=midi_tx.w; advertise_pending=1; restart_at=ble_port_ms()+100u; break;
    case BLE_GAP_EVENT_SUBSCRIBE:
        if(event->subscribe.attr_handle==midi_handle) diag.subscribed=event->subscribe.cur_notify;
        break;
    case BLE_GAP_EVENT_MTU: diag.mtu=event->mtu.value; break;
    default: break;
    }
    return 0;
}
static void advertise(void)
{
    struct ble_gap_adv_params params;
    static const uint8_t adv[]={2,1,6,17,7,0x00,0xc7,0xc4,0x4e,0xe3,0x6c,0x51,0xa7,0x33,0x4b,0xe8,0xed,0x5a,0x0e,0xb8,0x03};
    static const uint8_t scan[]={12,9,'F','e','l','u','c','c','a',' ','B','L','E'};
    int rc=ble_gap_adv_set_data(adv,sizeof adv);
    if(!rc) rc=ble_gap_adv_rsp_set_data(scan,sizeof scan);
    memset(&params,0,sizeof params); params.conn_mode=BLE_GAP_CONN_MODE_UND;
    params.disc_mode=BLE_GAP_DISC_MODE_GEN; params.itvl_min=params.itvl_max=160;
    if(!rc) rc=ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC,0,BLE_HS_FOREVER,&params,gap_event,0);
    if(rc) { diag.error=(uint32_t)rc; restart_at=ble_port_ms()+1000u; }
    else advertise_pending=0;
}
static void synced(void) { diag.synced=1; diag.syncs++; advertise_pending=1; restart_at=ble_port_ms(); }
static void reset(int reason) { diag.resets++; diag.error=(uint32_t)reason; diag.synced=diag.subscribed=0; }
static int gen_key(uint8_t key,struct ble_store_gen_key *out,uint16_t conn)
{
    (void)conn;
    if(key!=BLE_STORE_GEN_KEY_IRK) return BLE_HS_ENOTSUP;
    memset(out,0,sizeof *out); return 0;
}
static void receive(uint8_t type,const uint8_t *packet,uint16_t n)
{
    if(type==4) {
        void *event=ble_transport_alloc_evt(0);
        if(!event) { diag.event_drop++; return; }
        memcpy(event,packet,n); ble_transport_to_hs_evt(event);
    } else if(type==2) {
        struct os_mbuf *om=ble_transport_alloc_acl_from_ll();
        if(!om || os_mbuf_append(om,packet,n)) { if(om) os_mbuf_free_chain(om); diag.acl_drop++; return; }
        ble_transport_to_hs_acl(om);
    }
}
void ble_transport_ll_init(void) { }
int ble_transport_to_ll_cmd_impl(void *buf)
{
    uint8_t *p=buf; int rc=ble_hci_send(1,p,(uint16_t)(p[2]+3u));
    ble_transport_free(buf); return rc ? BLE_HS_ECONTROLLER : 0;
}
int ble_transport_to_ll_acl_impl(struct os_mbuf *om)
{
    uint8_t packet[31]; uint16_t n=OS_MBUF_PKTLEN(om); int rc=1;
    if(n<=sizeof packet && !os_mbuf_copydata(om,0,n,packet)) rc=ble_hci_send(2,packet,n);
    os_mbuf_free_chain(om); return rc ? BLE_HS_EAGAIN : 0;
}
int ble_transport_to_ll_iso_impl(struct os_mbuf *om) { os_mbuf_free_chain(om); return BLE_HS_ENOTSUP; }
void ble_host_pump_controller(void) { ble_worker(); ble_port_host_idle(); }
void ble_host_start(void)
{
    int rc;
    ble_set_host(receive); os_mempool_module_init(); os_msys_init();
    ble_transport_init(); ble_hs_init();
    ble_hs_cfg.sync_cb=synced; ble_hs_cfg.reset_cb=reset; ble_hs_cfg.store_gen_key_cb=gen_key;
    ble_svc_gap_init(); ble_svc_gatt_init();
    rc=ble_gatts_count_cfg(services); if(!rc) rc=ble_gatts_add_svcs(services);
    diag.connection=BLE_HS_CONN_HANDLE_NONE;
    if(rc) diag.error=(uint32_t)rc; else diag.running=1;
}
void ble_host_worker(void)
{
    uint32_t now=ble_port_ms(),gap=now-last_main; unsigned budget=32;
    if(last_main && gap>diag.main_gap_ms) diag.main_gap_ms=gap;
    last_main=now;
    if(!diag.running) return;
    ble_npl_poll(); ble_worker();
    while(budget--) {
        struct ble_npl_event *event=ble_npl_eventq_get(nimble_port_get_dflt_eventq(),0);
        if(!event) break;
        ble_npl_event_run(event); ble_worker();
    }
    if(diag.synced && advertise_pending && (int32_t)(now-restart_at)>=0) advertise();
    if(update_due && (int32_t)(now-update_at)>=0 && diag.connection!=BLE_HS_CONN_HANDLE_NONE) {
        struct ble_gap_upd_params params;
        memset(&params,0,sizeof params); params.itvl_min=6; params.itvl_max=12;
        params.supervision_timeout=400;
        /* Negotiated octet 0 has no connection-parameter-request bit: NimBLE
         * sends an L2CAP signalling request, never peripheral HCI 2013. */
        (void)ble_gap_update_params(diag.connection,&params); update_due=0;
    }
    budget=16;
    while(budget-- && midi_tx.r!=midi_tx.w) {
        uint32_t i=midi_tx.r%MIDI_Q; uint8_t packet[8]; int n,rc;
        struct os_mbuf *om;
        if(!diag.subscribed) { midi_tx.r=midi_tx.w; break; }
        n=ble_midi_encode_usb(midi_tx.pkt[i],midi_tx.ms[i],packet,sizeof packet);
        if(!n) { midi_tx.r++; diag.midi_bad++; continue; }
        om=ble_hs_mbuf_from_flat(packet,(uint16_t)n);
        if(!om) break;
        rc=ble_gatts_notify_custom(diag.connection,midi_handle,om);
        if(rc==BLE_HS_ENOMEM || rc==BLE_HS_EAGAIN) break;
        midi_tx.r++; if(rc) diag.midi_drop++; else diag.midi_out++;
    }
}
void ble_host_midi_out(uint32_t pkt,uint32_t ms)
{
    uint32_t key=ble_port_lock(),i=midi_tx.w%MIDI_Q;
    if(!diag.subscribed) { ble_port_unlock(key); return; }
    if(midi_tx.w-midi_tx.r==MIDI_Q) { diag.midi_drop++; ble_port_unlock(key); return; }
    midi_tx.pkt[i]=pkt; midi_tx.ms[i]=ms;
    __asm__ volatile("" ::: "memory"); midi_tx.w++; ble_port_unlock(key);
}
void ble_host_echo(int enabled) { diag.echo=(uint8_t)!!enabled; }

int ble_host_midi_take(uint32_t *pkt,uint32_t *ms)
{
    uint32_t i=midi_rx.r%MIDI_Q;
    if(midi_rx.r==midi_rx.w) return 0;
    *pkt=midi_rx.pkt[i]; *ms=midi_rx.ms[i]; midi_rx.r++; return 1;
}
void ble_host_get_diag(ble_host_diag_t *out)
{
    uint32_t heap[3]; ble_heap_diag(heap); *out=diag;
    out->heap_used=heap[0]; out->heap_peak=heap[1]; out->heap_fail=heap[2];
}
void ble_host_assert(const char *file,int line)
{
    (void)file; diag.assertions++; diag.error=(uint32_t)line; diag.running=0;
}

void ble_host_manual(void)
{
    diag.running=0; diag.subscribed=0; ble_set_host(0); midi_tx.r=midi_tx.w;
}
void ble_host_resume(void)
{
    ble_set_host(receive); diag.running=1; advertise_pending=0;
    ble_hs_sched_reset(BLE_HS_ECONTROLLER);
}
