/* SPDX-License-Identifier: GPL-3.0-only */
/* Pure r3 02 §§3.4,5.4-5.8,6; 03 §6.2 arithmetic. */
#include "ble_link.h"
#include "ble_memory.h"
uint8_t ble_map_count(const uint8_t map[5])
{
    unsigned i,n=0;
    for(i=0;i<37;i++) n+=(map[i/8]>>(i%8))&1u;
    return (uint8_t)n;
}
uint8_t ble_csa1(uint8_t *unmapped,uint8_t hop,const uint8_t map[5])
{
    unsigned i,n=ble_map_count(map),r;
    *unmapped=(uint8_t)((*unmapped+hop)%37u);
    if(map[*unmapped/8]&(1u<<(*unmapped%8))) return *unmapped;
    if(!n) return 0xff;
    r=*unmapped%n;
    for(i=0;i<37;i++) if(map[i/8]&(1u<<(i%8))) { if(!r--) return (uint8_t)i; }
    return 0xff;
}
uint8_t ble_rx_class(uint16_t status,uint8_t n)
{
    unsigned code=status&15u;
    if(code&4u) return 1; /* B, including 13 */
    if(n>27) return 3;
    if(code==1) return 0;
    if(code==9) return 2;
    return 3;
}
int ble_conn_params_valid(const ble_conn_t *c)
{
    uint32_t effective=(uint32_t)(c->latency+1u)*c->interval;
    return c->interval>=6 && c->interval<=3200 && c->latency<=499 &&
        c->timeout>=10 && c->timeout<=3200 && (uint32_t)c->timeout*4u>effective &&
        c->win>=1 && c->win<=8 && c->win<c->interval && c->offset<=c->interval && effective<=3435;
}
int ble_connect_decode(ble_conn_t *c,const uint8_t *p,uint8_t n,uint8_t header)
{
    if(n!=34) return 0;
    memset(c,0,sizeof *c);
    memcpy(c->peer,p,6); c->peer_type=(header>>6)&1u;
    p+=12; c->aa=ble_get32(p); c->crc=ble_get16(p+4)|((uint32_t)p[6]<<16);
    c->win=p[7]; c->offset=ble_get16(p+8); c->interval=ble_get16(p+10);
    c->latency=ble_get16(p+12); c->timeout=ble_get16(p+14);
    memcpy(c->map,p+16,5); c->map[4]&=31; c->hop=p[21]&31u; c->sca=p[21]>>5;
    return ble_conn_params_valid(c) && c->hop>=5 && c->hop<=16 &&
        c->aa!=0x8e89bed6u && ble_map_count(c->map)>=2;
}
uint16_t ble_widen(const ble_conn_t *c)
{
    static const uint16_t ppm[]={500,250,150,100,75,50,30,20};
    uint32_t v=(uint32_t)(ppm[c->sca&7u]+BLE_LOCAL_SCA_PPM)*c->interval*1250u*(c->latency+1u);
    return (uint16_t)((v/1000000u+1u)*2u);
}
int ble_instant_valid(uint16_t instant,uint16_t closed)
{
    uint16_t diff=(uint16_t)(instant-1u-closed);
    return diff>=1 && diff<=0x7ffe;
}
void ble_timeline_create(ble_timeline_t *t,const ble_conn_t *c,uint32_t now)
{
    memset(t,0,sizeof *t); t->sup=now+(uint32_t)c->timeout*240000u;
    t->first=now+(uint32_t)(c->offset+1u)*30000u;
    t->est_end=t->first+(uint32_t)(5u*c->interval+c->win)*30000u+24000u;
    t->cutoff=t->first+(uint32_t)6u*c->interval*30000u;
}
uint8_t ble_timeline_rx(ble_timeline_t *t,const ble_conn_t *c,uint8_t cls,uint32_t now)
{
    if(!t->established && cls<3) {
        if((int32_t)(now-t->cutoff)>=0) return 0x3e;
        t->established=1;
    }
    if(cls==0 || cls==2) t->sup=now+(uint32_t)c->timeout*240000u;
    return 0;
}
uint8_t ble_timeline_check(ble_timeline_t *t,const ble_conn_t *c,uint32_t now)
{
    if(t->win_pending && (int32_t)(now-t->win_reset)>=0) {
        uint32_t d=t->win_reset+(uint32_t)c->timeout*240000u;
        if((int32_t)(d-t->sup)>0) t->sup=d;
        t->win_pending=0;
    }
    if(!t->established) return (int32_t)(now-t->est_end)>=0 ? 0x3e : 0;
    return (int32_t)(now-t->sup)>=0 ? 8 : 0;
}
int ble_missed_step(int fine,uint32_t raw,unsigned phase,int *next,unsigned *drift)
{
    int q=(int)phase+fine,v=fine-(int)BLE_MISS_STEP_US;
    unsigned d=0;
    if((q>=625 && q<625+(int)BLE_MISS_STEP_US) || (q>=0 && q<(int)BLE_MISS_STEP_US)) d=1;
    else if(q < (int)BLE_MISS_STEP_US-625) { d=1; v=fine+625-(int)BLE_MISS_STEP_US; }
    if(d>=raw || v < -2048 || v>2047) return 0;
    *next=v; *drift=d; return 1;
}
