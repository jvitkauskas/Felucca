/* SPDX-License-Identifier: GPL-3.0-only */
/* r4 04 §8.1. Hash counter generator; all physical inputs carry zero credit.
 * Seed flash is accessed only by boot. Neither HCI Reset nor reseed writes it. */
#include "ble_seed.h"
#include "ble_crypto.h"
#include "ble_link.h"
#include "ble_memory.h"
#include "tinycrypt/sha256.h"
static struct {
    uint8_t pool[32],key[32]; uint32_t counter,since_reseed;
    volatile uint8_t connection_reseed;
    ble_seed_diag_t diag;
    struct { uint32_t ticks,rssi; } samples[8];
    volatile uint32_t w,r;
    uint32_t previous_tick;
} rng __attribute__((section(".pool.ble")));
uint32_t ble_crc32(const uint8_t *p,unsigned n)
{
    uint32_t crc=0xffffffffu; unsigned j;
    while(n--) { crc^=*p++; for(j=0;j<8;j++) crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u); }
    return ~crc;
}
static int blank(const uint8_t *p,unsigned n)
{
    while(n--) if(*p++!=255) return 0;
    return 1;
}
int ble_seed_record_valid(const uint8_t p[64])
{
    unsigned i;
    if(memcmp(p,"WLSD",4) || p[4]!=1 || p[5]>1 || ble_get16(p+6)>256 ||
        (!p[5] && ble_get16(p+6)) || ble_get32(p+60)!=ble_crc32(p,60)) return 0;
    for(i=44;i<60;i++) if(p[i]) return 0;
    return 1;
}
static void hash_parts(uint8_t out[32],uint8_t label,const uint8_t *a,unsigned an,const uint8_t *b,unsigned bn)
{
    struct tc_sha256_state_struct h; uint8_t header[3]={label,0,0};
    tc_sha256_init(&h); ble_put16(header+1,(uint16_t)an); tc_sha256_update(&h,header,3);
    if(an) tc_sha256_update(&h,a,an);
    header[0]=(uint8_t)(label^0x80u); ble_put16(header+1,(uint16_t)bn); tc_sha256_update(&h,header,3);
    if(bn) tc_sha256_update(&h,b,bn);
    tc_sha256_final(out,&h); ble_secure_zero(&h,sizeof h);
}
void ble_entropy_absorb(uint8_t label,const void *p,unsigned n)
{
    hash_parts(rng.pool,label,rng.pool,32,p,n);
}
static int hash_selftest(void)
{
    static const uint8_t answer[32]={0xba,0x78,0x16,0xbf,0x8f,1,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,
        0xb0,3,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0,0x15,0xad};
    struct tc_sha256_state_struct h; uint8_t digest[32];
    tc_sha256_init(&h); tc_sha256_update(&h,(const uint8_t *)"abc",3); tc_sha256_final(digest,&h);
    return !memcmp(digest,answer,32);
}
static int drbg_selftest(void)
{
    static const uint8_t expected[32]={0x34,4,0x47,0xb9,0xbe,0x2a,0xcb,0xfe,0xb8,0x98,0xcd,0xf3,0x85,0x93,0x3d,0x14,
        0x8c,0x1c,0xfb,5,0x3a,0xe3,0xfa,0xb8,0x90,0x41,0xbc,0x41,0xa3,0xc5,0x5f,0x80};
    uint8_t key[32],counter[4]={1,0,0,0},out[32]; unsigned i;
    for(i=0;i<32;i++) key[i]=(uint8_t)i;
    hash_parts(out,0x31,key,32,counter,4); return !memcmp(out,expected,32);
}
void ble_seed_boot(const uint8_t address[6],uint32_t reset,uint32_t r64lo,uint32_t r64hi)
{
    uint8_t candidate[64],record[64],old[64]={0},words[12];
    int selected=-1,last[2]={-1,-1},any[2]={0,0},target,slot,erase=0,rc;
    unsigned area,i; uint32_t duration=0,off; uint8_t retry=0;
    memset(&rng,0,sizeof rng);
    if(!hash_selftest() || !drbg_selftest() || !ble_crypto_selftest()) { rng.diag.state=BLE_SEED_FAILED; return; }
    for(area=0;area<2;area++) for(i=0;i<60;i++) {
        if(ble_port_flash_read(0x94000u+area*4096u+i*64u,candidate,64)) continue;
        if(blank(candidate,64)) continue;
        last[area]=(int)i; any[area]=1;
        if(!ble_seed_record_valid(candidate)) { rng.diag.malformed++; continue; }
        if(selected<0) { memcpy(old,candidate,64); selected=(int)area; }
        else {
            uint32_t delta=ble_get32(candidate+8)-ble_get32(old+8);
            if(!delta || delta==0x80000000u) rng.diag.conflicts++;
            else if(delta<0x80000000u) { memcpy(old,candidate,64); selected=(int)area; }
        }
    }
    rng.diag.origin=selected<0 ? BLE_SEED_FIRST_BOOT :
        old[5] && ble_get16(old+6)==256 ? BLE_SEED_PROVISIONED : BLE_SEED_CHAINED;
    if(selected>=0) {
        ble_entropy_absorb(1,old+12,32); ble_entropy_absorb(2,old+8,4);
        rng.diag.flags=old[5]; rng.diag.credit=ble_get16(old+6);
        rng.diag.sequence=ble_get32(old+8)+1u;
    } else rng.diag.sequence=1;
    ble_entropy_absorb(3,address,6); ble_put32(words,reset); ble_entropy_absorb(4,words,4);
    /* Optional power-on RAM is omitted: no Felucca window has been qualified.
     * One actual boot-time reading is a diversifier, never a timing sample loop. */
    ble_put32(words,ble_port_ticks()); ble_entropy_absorb(5,words,4);
    ble_put32(words,r64lo); ble_put32(words+4,r64hi); ble_entropy_absorb(6,words,8);
    memset(record,0,64); memcpy(record,"WLSD",4); record[4]=1; record[5]=rng.diag.flags;
    ble_put16(record+6,rng.diag.credit); ble_put32(record+8,rng.diag.sequence);
    do { hash_parts(record+12,0x20,rng.pool,32,&retry,1); retry++; }
    while(selected>=0 && !memcmp(record+12,old+12,32) && retry);
    hash_parts(rng.key,0x21,rng.pool,32,0,0); rng.counter=1;
    ble_put32(record+60,ble_crc32(record,60));
    if(selected>=0) {
        target=selected; slot=last[target]+1;
        if(slot==60) { target^=1; slot=0; erase=1; }
    } else { target=any[0] && !any[1] ? 1 : 0; slot=0; erase=any[target]; }
    off=0x94000u+(uint32_t)target*4096u+(uint32_t)slot*64u;
    rc=erase ? ble_port_flash_erase(off&~4095u,&duration) : BLE_FLASH_OK;
    if(rc==BLE_FLASH_OK) rc=ble_port_flash_program(off,record,64,&duration);
    rng.diag.persistence=rc==BLE_FLASH_OK ? BLE_PERSIST_STORED :
        (rc==BLE_FLASH_REFUSED || rc==BLE_FLASH_NOT_ISSUED) ? BLE_PERSIST_NOT_STORED : BLE_PERSIST_UNVERIFIED;
    if(rc==BLE_FLASH_OK && (ble_port_flash_read(off,candidate,64) || memcmp(candidate,record,64)))
        rng.diag.persistence=BLE_PERSIST_UNVERIFIED;
    rng.diag.state=rng.diag.persistence==BLE_PERSIST_STORED ? rng.diag.origin : BLE_SEED_UNPERSISTED;
    if(rng.diag.persistence!=BLE_PERSIST_STORED) rng.diag.flash_failures++;
    ble_put32(words,duration); ble_entropy_absorb(7,words,4);
    ble_secure_zero(old,sizeof old); ble_secure_zero(record,sizeof record); ble_secure_zero(candidate,sizeof candidate);
}
void ble_entropy_connection(void) { rng.connection_reseed=1; }
void ble_entropy_sample(uint32_t ticks,uint32_t rssi,int event)
{
    uint32_t w=rng.w;
    if(w-rng.r==8) return;
    rng.samples[w%8u].ticks=event ? ticks-rng.previous_tick : 0; rng.samples[w%8u].rssi=rssi;
    if(event) rng.previous_tick=ticks;
    __asm__ volatile("" ::: "memory"); rng.w=w+1u;
}
void ble_entropy_poll(void)
{
    if(rng.r!=rng.w) {
        uint32_t words[2],r=rng.r;
        words[0]=rng.samples[r%8u].ticks; words[1]=rng.samples[r%8u].rssi;
        rng.r=r+1u; ble_entropy_absorb(8,words,sizeof words);
    }
}
int ble_entropy(uint8_t *out,unsigned n)
{
    uint8_t block[32],counter[4]; unsigned take;
    if(rng.diag.state==BLE_SEED_FAILED) return 0;
    if(rng.connection_reseed || rng.since_reseed>=65536u) {
        hash_parts(rng.key,0x30,rng.key,32,rng.pool,32);
        rng.connection_reseed=0; rng.since_reseed=0; rng.diag.reseed_attempts++;
    }
    rng.diag.bytes+=n; rng.since_reseed+=n;
    while(n) {
        ble_put32(counter,rng.counter++); hash_parts(block,0x31,rng.key,32,counter,4);
        take=n>32 ? 32 : n; memcpy(out,block,take); out+=take; n-=take;
        hash_parts(rng.key,0x32,rng.key,32,block,32);
        if(!rng.counter) rng.counter=1; /* a freshly rekeyed generator, no nonce reuse */
    }
    ble_secure_zero(block,sizeof block); return 1;
}
void ble_seed_rand_called(void) { rng.diag.rand_calls++; }
void ble_seed_reset_stats(void)
{
    rng.diag.rand_calls=rng.diag.bytes=rng.diag.reseed_attempts=0;
    /* Boot parsing and persistence results describe the surviving seed state. */
}
void ble_seed_get_diag(ble_seed_diag_t *out)
{
    uint32_t key=ble_port_lock(); *out=rng.diag; ble_port_unlock(key);
}
