/* SPDX-License-Identifier: GPL-3.0-only */
/* In-memory H4 S1 controller, r2 03 §§1-5 and 04 §10. Main context only.
 * No NimBLE, connections, encryption, heap, MMIO, or host callbacks from ISR. */
#include "ble_s1.h"
#include "ble_memory.h"

typedef struct { uint8_t data[BLE_HCI_MAX], cancel; uint16_t n; } command_t;
static struct {
    command_t q[4];
    unsigned r,w,count;
    ble_host_fn host;
    uint8_t pub[6],random[6],random_set,masks[2][8];
    ble_adv_t adv;
    ble_diag_t diag;
    uint16_t deferred;
    uint8_t reply[70],overflow_reply[6];
} ctl;
_Static_assert(sizeof(ble_trace_t)==40,"trace wire/storage layout");
static ble_trace_t trace[BLE_TRACE_N] __attribute__((section(".ble_trace"),aligned(4)));
static uint32_t trace_w,trace_used;
static uint8_t trace_frozen;
static struct { uint8_t active,waiting,type,own_type,random[6]; uint16_t expected; } driver;

static void trace_add(uint8_t type,const uint8_t *p,uint16_t n,uint8_t status)
{
    ble_trace_t *r;
    if (trace_frozen) { ctl.diag.trace_frozen_drops++; return; }
    r=&trace[trace_w++%BLE_TRACE_N];
    r->ms=ble_port_ms(); r->len=n; r->dir_type=type; r->intake=status;
    memset(r->data,0,sizeof r->data); memcpy(r->data,p,n<32 ? n : 32);
    if (trace_used<BLE_TRACE_N) trace_used++;
    ctl.diag.trace_count++;
}
static void driver_receive(uint8_t type,const uint8_t *p,uint16_t n)
{
    if (type!=4 || n<6 || p[0]!=14 || !driver.waiting) return;
    if (ble_get16(p+3)!=driver.expected || p[5]) {
        ctl.diag.driver_error=1; driver.active=0;
    } else ctl.diag.driver_step++;
    driver.waiting=0;
}
static void complete(uint16_t opcode,uint8_t status,const uint8_t *data,uint8_t n,int immediate)
{
    uint8_t *b=immediate ? ctl.overflow_reply : ctl.reply;
    /* All error replies contain status only. No uninitialised success payload. */
    if (status) n=0;
    b[0]=14; b[1]=(uint8_t)(4+n); b[2]=1;
    ble_put16(b+3,opcode); b[5]=status;
    if (n) memcpy(b+6,data,n);
    ctl.diag.replies++; ctl.diag.last_opcode=opcode; ctl.diag.last_status=status;
    trace_add(4,b,(uint16_t)(6+n),0);
    if (ctl.host) ctl.host(4,b,(uint16_t)(6+n));
}
static void defaults(void)
{
    memset(&ctl.adv,0,sizeof ctl.adv);
    ctl.adv.params.min=ctl.adv.params.max=2048;
    ctl.adv.params.channels=7; ctl.adv.power=6;
    memset(ctl.random,0,6); ctl.random_set=0;
    memset(ctl.masks,0,sizeof ctl.masks);
    memset(ctl.masks[0],255,5); ctl.masks[0][5]=31;
    ctl.masks[1][0]=31;
    ctl.diag.enabled=0; ctl.diag.stopping=0;
}
void ble_init(const uint8_t *public_addr,ble_host_fn host)
{
    memset(&ctl,0,sizeof ctl); memset(&driver,0,sizeof driver);
    trace_w=trace_used=0; trace_frozen=0;
    defaults();
    if (ble_public_valid(public_addr)) memcpy(ctl.pub,public_addr,6);
    ctl.host=host;
}
void ble_set_host(ble_host_fn host) { driver.active=driver.waiting=0; ctl.host=host; }
int ble_hci_send(uint8_t type,const uint8_t *packet,uint16_t n)
{
    if (!packet || type!=1 || n<3 || n>BLE_HCI_MAX || n!=3u+packet[2]) {
        ctl.diag.rejected++;
        if (packet) trace_add((uint8_t)(128u|type),packet,n,1);
        return 1;
    }
    if (!ctl.diag.commands && !ctl.diag.replies) ctl.diag.first_command_ticks=ble_port_ticks();
    trace_add(129,packet,n,0);
    if (ctl.count==4) {
        ctl.diag.overflow++;
        complete(ble_get16(packet),0x0c,0,0,1);
        return 0; /* answered, not enqueued; exactly one response */
    }
    memcpy(ctl.q[ctl.w].data,packet,n); ctl.q[ctl.w].n=n; ctl.q[ctl.w].cancel=0;
    ctl.w=(ctl.w+1u)%4; ctl.count++; ctl.diag.commands++;
    return 0;
}
static int command_length(uint16_t op)
{
    switch (op) {
    case 0x0c03: case 0x1001: case 0x1002: case 0x1003: case 0x1009:
    case 0x2002: case 0x2003: case 0x201c: return 0;
    case 0x0c01: case 0x2001: return 8;
    case 0x2005: return 6;
    case 0x2006: return 15;
    case 0x2008: case 0x2009: return 32;
    case 0x200a: return 1;
#if FELUCCA_BLE_TX_DBM != 127
    case 0x2007: return 0;
#endif
    default: return -1;
    }
}
static void supported(uint8_t *out)
{
    memset(out,0,64);
    out[5]=192; out[14]=40; out[15]=2;
    out[25]=0xb7; out[26]=3; out[28]=8;
#if FELUCCA_BLE_TX_DBM != 127
    out[25]|=64;
#endif
}
static void reset_finish(void)
{
    uint8_t power=ctl.adv.power;
    defaults(); ctl.adv.power=power;
    ble_port_reset_stats();
}
static void execute(const command_t *cmd)
{
    uint16_t op=ble_get16(cmd->data);
    const uint8_t *p=cmd->data+3;
    uint8_t data[64],n=0,status=0;
    int expected=command_length(op);
    if (cmd->cancel) status=0x0c;
    else if (expected<0) status=1;
    else if (cmd->data[2]!=expected) status=0x12;
    else switch (op) {
    case 0x0c03: {
        unsigned i;
        /* Cancel commands already queued behind Reset; still answer each one. */
        for (i=0;i<ctl.count;i++) ctl.q[(ctl.r+i)%4].cancel=1;
        if (ctl.diag.enabled || ctl.diag.stopping) {
            ctl.deferred=op; ctl.diag.stopping=1; ble_port_stop_begin(); return;
        }
        reset_finish(); break;
    }
    case 0x1001:
        data[0]=6; ble_put16(data+1,1); data[3]=6;
        ble_put16(data+4,0xffff); ble_put16(data+6,1); n=8; break;
    case 0x1002: supported(data); n=64; break;
    case 0x1003: memset(data,0,8); data[4]=0x60; n=8; break;
    case 0x1009: memcpy(data,ctl.pub,6); n=6; break;
    case 0x0c01: memcpy(ctl.masks[0],p,8); break;
    case 0x2001: memcpy(ctl.masks[1],p,8); break;
    case 0x2002: ble_put16(data,27); data[2]=5; n=3; break;
    case 0x2003: memset(data,0,8); n=8; break; /* S1 has no encryption */
    case 0x201c: memset(data,0,8); data[0]=5; n=8; break; /* advertising only */
    case 0x2005:
        if (ctl.diag.enabled) status=0x0c;
        else if (!ble_random_valid(p)) status=0x12;
        else { memcpy(ctl.random,p,6); ctl.random_set=1; }
        break;
    case 0x2006:
        status=ctl.diag.enabled ? 0x0c : ble_adv_validate(p,&ctl.adv.params); break;
    case 0x2008: case 0x2009:
        if (p[0]>31) status=0x12;
        else {
            uint8_t *dst=op==0x2008 ? ctl.adv.data : ctl.adv.scan;
            memset(dst,0,31); memcpy(dst,p+1,p[0]);
            if (op==0x2008) ctl.adv.data_len=p[0]; else ctl.adv.scan_len=p[0];
            if (ctl.diag.enabled) ble_port_update(&ctl.adv);
        }
        break;
    case 0x200a:
        if (p[0]>1) status=0x12;
        else if (!p[0]) {
            if (ctl.diag.enabled) {
                ctl.deferred=op; ctl.diag.stopping=1; ble_port_stop_begin(); return;
            }
        } else if (ble_port_failed()) status=3;
        else if (!ctl.diag.enabled) {
            const uint8_t *addr=ctl.adv.params.own_type ? ctl.random : ctl.pub;
            if (ctl.adv.params.own_type ? !ctl.random_set : !ble_public_valid(addr)) status=0x12;
            else if (ble_port_failed()) status=3;
            else {
                memcpy(ctl.adv.addr,addr,6);
                status=ble_port_start(&ctl.adv) ? 3 : 0;
                if (!status) ctl.diag.enabled=1;
            }
        }
        break;
#if FELUCCA_BLE_TX_DBM != 127
    case 0x2007:
        if (ctl.adv.power!=6) status=3;
        else { data[0]=(uint8_t)FELUCCA_BLE_TX_DBM; n=1; }
        break;
#endif
    default: status=1; break;
    }
    complete(op,status,data,n,0);
}
static void driver_pass(void)
{
    static const uint16_t ops[]={0x0c03,0x1001,0x1002,0x1003,0x0c01,0x2001,
                                0x2002,0x2003,0x1009,0x2005,0x2008,0x2009,0x2006,0x200a};
    static const uint8_t mask[8]={0x90,0x80,0,2,0,0x80,0,0x20};
    uint8_t cmd[35]={0},step=ctl.diag.driver_step;
    uint16_t op;
    if (!driver.active || driver.waiting || ctl.count || ctl.deferred) return;
    if (step==9 && !driver.own_type) step=++ctl.diag.driver_step;
    if (step>=sizeof ops/sizeof ops[0]) { driver.active=0; return; }
    op=ops[step]; ble_put16(cmd,op);
    if (step==4) { cmd[2]=8; memcpy(cmd+3,mask,8); }
    if (step==5) { cmd[2]=8; cmd[3]=31; }
    if (step==9) { cmd[2]=6; memcpy(cmd+3,driver.random,6); }
    if (step==10) {
        cmd[2]=32; cmd[3]=3; cmd[4]=2; cmd[5]=1; cmd[6]=6;
        /* NONCONN has no scan response: repeat complete name in ADV data. */
        if (driver.type==3) { cmd[3]=16; cmd[7]=12; cmd[8]=9; memcpy(cmd+9,"Felucca BLE",11); }
    }
    if (step==11) { cmd[2]=32; cmd[3]=13; cmd[4]=12; cmd[5]=9; memcpy(cmd+6,"Felucca BLE",11); }
    if (step==12) {
        cmd[2]=15; ble_put16(cmd+3,160); ble_put16(cmd+5,160);
        cmd[7]=driver.type; cmd[8]=driver.own_type; cmd[16]=7;
    }
    if (step==13) { cmd[2]=1; cmd[3]=1; }
    driver.waiting=1; driver.expected=op;
    if (ble_hci_send(1,cmd,(uint16_t)(3+cmd[2]))) {
        driver.waiting=driver.active=0; ctl.diag.driver_error=2;
    }
}
void ble_worker(void)
{
    if (ctl.deferred) {
        int rc=ble_port_stop_poll();
        uint16_t op;
        if (!rc) return;
        op=ctl.deferred; ctl.deferred=0; ctl.diag.stopping=0; ctl.diag.enabled=0;
        if (op==0x0c03 && rc>0) reset_finish();
        complete(op,rc<0 ? 3 : 0,0,0,0);
        return;
    }
    driver_pass();
    if (ctl.count) {
        command_t cmd=ctl.q[ctl.r];
        ctl.r=(ctl.r+1u)%4; ctl.count--;
        execute(&cmd);
    }
}
void ble_driver_start(uint8_t type,const uint8_t *random_addr)
{
    memset(&driver,0,sizeof driver); driver.active=1; driver.type=type;
    ctl.diag.driver_step=ctl.diag.driver_error=0; ctl.host=driver_receive;
    driver.own_type=(uint8_t)!ble_public_valid(ctl.pub);
    if (random_addr) memcpy(driver.random,random_addr,6);
}
void ble_driver_stop(void) { driver.active=driver.waiting=0; }
void ble_get_diag(ble_diag_t *out) { *out=ctl.diag; }
void ble_trace_freeze(int freeze) { trace_frozen=(uint8_t)!!freeze; }
uint32_t ble_trace_size(void) { return trace_used; }
const ble_trace_t *ble_trace_record(uint32_t oldest_index)
{
    if (!trace_frozen || oldest_index>=trace_used) return 0;
    return &trace[(trace_w-trace_used+oldest_index)%BLE_TRACE_N];
}
void ble_set_power(uint8_t level)
{
    ctl.adv.power=level>11 ? 11 : level;
    if (ctl.diag.enabled) ble_port_update(&ctl.adv);
}
