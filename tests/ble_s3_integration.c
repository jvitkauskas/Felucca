/* SPDX-License-Identifier: GPL-3.0-only */
/* Actual pinned host SMP (legacy and SC), actual HCI/LL/CCM, deterministic
 * test-only central and NOR model. No hardware or network I/O. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "host/ble_hs.h"
#include "host/ble_store.h"
#include "tinycrypt/ecc.h"
#include "../firmware/third_party/nimble/nimble/host/src/ble_sm_priv.h"
#include "ble_s3_sim.h"
#include "../firmware/ble/ble_host.h"
#include "../firmware/ble/ble_crypto.h"
#include "../firmware/ble/ble_security.h"
#include "../firmware/ble/ble_seed.h"
#include "../firmware/ble/ble_bonds.h"
static const uint8_t pub[6]={0x58,0x65,0x67,0x8b,0x40,0x62},central[6]={1,0,0,0,0,0};
static uint8_t smp[32][70],smp_len[32],l2[256],att[256];
static unsigned smp_n,l2_n,l2_want,att_n,fragments65;
static uint8_t session_ltk[16],sk[16],iv[8],request[23];
static uint64_t tx_counter,rx_counter;
static int tx_encrypted,started,start_req_seen;
static void control_rx(const uint8_t *p,unsigned n);
static void central_rx(const uint8_t *p,unsigned n,uint8_t llid)
{
    uint8_t air[31]; assert(n<=27); memcpy(air,p,n);
    if(tx_encrypted && n) { assert(ble_ccm(sk,iv,tx_counter++,1,llid,air,(uint8_t)n,0)); n+=4; }
    assert(ble_ll_rx(1,(uint16_t)((n<<8)|llid),air,sim_ticks,0));
}
static void take_slot(unsigned slot)
{
    unsigned id=(unsigned)sim_slot[slot],n=sim_air_len[slot],llid=ble_ll_tx_llid(id),i; uint8_t p[31];
    memcpy(p,sim_air[slot],n);
    if(n==ble_ll_tx_len(id)+4u) { assert(ble_ccm(sk,iv,rx_counter++,0,(uint8_t)llid,p,(uint8_t)(n-4u),1)); n-=4; }
    if(llid==3) {
        if(p[0]==4 && n==13) {
            uint8_t skd[16],keyle[16]; memcpy(skd,request+11,8); memcpy(skd+8,p+1,8);
            memcpy(iv,request+19,4); memcpy(iv+4,p+9,4); assert(ble_aes_le(session_ltk,skd,keyle));
            for(i=0;i<16;i++) sk[i]=keyle[15u-i];
        } else if(p[0]==5) start_req_seen=1;
        else if(p[0]==6) started=1;
        else if(p[0]==13 || p[0]==2) { fprintf(stderr,"unexpected LL reject/terminate %u reason %u\n",p[0],p[1]); assert(0); }
    } else {
        if(llid==2) { l2_n=0; l2_want=ble_get16(p)+4u; assert(l2_want<=sizeof l2); }
        assert(l2_n+n<=sizeof l2); memcpy(l2+l2_n,p,n); l2_n+=n;
        if(l2_n==l2_want) {
            if(ble_get16(l2+2)==6) {
                assert(smp_n<32 && l2_n-4u<=70); smp_len[smp_n]=(uint8_t)(l2_n-4u);
                memcpy(smp[smp_n++],l2+4,l2_n-4u); if(l2_n==69) fragments65++;
            } else if(ble_get16(l2+2)==4) { att_n=l2_n-4u; memcpy(att,l2+4,att_n); }
            l2_n=l2_want=0;
        }
    }
    sim_ack(slot);
}
static void pump(void)
{
    unsigned i,j;
    for(i=0;i<32;i++) {
        int any=0;
        sim_advance_us(0); ble_host_worker(); ble_port_refill();
        for(j=0;j<2;j++) if(sim_slot[j]>=0) { take_slot(j); any=1; }
        if(!any && !ble_ll_pending()) break;
    }
}
static void l2send(uint16_t cid,const uint8_t *p,unsigned n)
{
    uint8_t data[256]; unsigned off=0;
    ble_put16(data,(uint16_t)n); ble_put16(data+2,cid); memcpy(data+4,p,n); n+=4;
    while(off<n) { unsigned take=n-off>27 ? 27 : n-off; central_rx(data+off,take,off ? 1 : 2); off+=take; }
    pump();
}
static unsigned smp_get(uint8_t op,uint8_t *out)
{
    unsigned i; pump();
    for(i=0;i<smp_n;i++) if(smp_len[i] && smp[i][0]==op) { unsigned n=smp_len[i]; memcpy(out,smp[i],n); smp_len[i]=0; return n; }
    { ble_host_diag_t d; ble_host_get_diag(&d); fprintf(stderr,"host req %u err %u conn %u ms %u state %u pairfail %u acldrop %u procs %d; ",d.security_requests,d.error,d.connection,ble_port_ms(),ble_link.state,d.pair_failures,d.acl_drop,ble_sm_num_procs()); }
    { ble_seed_diag_t seed; ble_host_diag_t d; ble_seed_get_diag(&seed); ble_host_get_diag(&d); fprintf(stderr,"seed %u Rand %u status %u asserts %u; ",seed.state,seed.rand_calls,d.pair_status,d.assertions); }
    fprintf(stderr,"missing SMP %02x; received:",op); for(i=0;i<smp_n;i++) fprintf(stderr," %02x",smp[i][0]); fputc('\n',stderr); assert(0); return 0;
}
static void smp_send(uint8_t op,const uint8_t *p,unsigned n)
{
    uint8_t data[70]; data[0]=op; memcpy(data+1,p,n); l2send(6,data,n+1);
}
static void control_rx(const uint8_t *p,unsigned n) { central_rx(p,n,3); pump(); }
static void connect_link(void)
{
    ble_conn_t c=sim_connection();
    tx_encrypted=started=start_req_seen=0; tx_counter=rx_counter=0; smp_n=l2_n=l2_want=0; sim_slot[0]=sim_slot[1]=-1;
    ble_ll_create(&c,sim_ticks); assert(ble_ll_rx(1,1,0,sim_ticks,0)); pump();
    control_rx((uint8_t[9]){8,1,0,0,0,0,0,0,0},9);
    control_rx((uint8_t[6]){12,6,1,0,2,0},6);
}
static void encryption_start(const uint8_t key[16],const uint8_t rand_ediv[10])
{
    unsigned i; memcpy(session_ltk,key,16); memset(request,0,sizeof request); request[0]=3;
    memcpy(request+1,rand_ediv,10); for(i=0;i<12;i++) request[11+i]=(uint8_t)(i+33);
    control_rx(request,23); pump(); assert(start_req_seen);
    tx_encrypted=1; tx_counter=rx_counter=0; control_rx((uint8_t[]){6},1);
    assert(started);
}
int main(int argc,char **argv)
{
    uint8_t preq[7]={1,3,0,1,16,1,1},pres[70],nc[16],np[16],confirm[70],packet[70],key[16],zero[16]={0};
    uint8_t pubc[64],privc[32]={2},pubp[64],dhkey[32],mackey[16],expected[16],saved_ltk[16],rand_ediv[10]={0};
    unsigned i; int sc=argc>1 && !strcmp(argv[1],"sc"); ble_host_diag_t d; ble_seed_diag_t seed; ble_bond_diag_t bond;
    sim_reset(); sim_storage_reset(); sim_seed(); sim_executor_enabled=1; ble_init(pub,0); ble_host_start(); pump(); sim_advance_us(10000); pump();
    ble_host_get_diag(&d); assert(d.synced && !d.error); connect_link();
    sim_advance_us(101000); assert(ble_ll_rx(1,1,0,sim_ticks,0)); pump();
    assert(smp_get(11,packet)==2); /* Application's Security Request, open MIDI unchanged. */
    for(i=0;i<16;i++) nc[i]=(uint8_t)(i+80);
    preq[3]=(uint8_t)(sc ? 9 : 1); l2send(6,preq,7); assert(smp_get(2,pres)==7 && pres[4]==16);
    if(sc) {
        uint8_t privbe[32]={0},pubbe[64]; privbe[31]=2;
        /* Test-only central private scalar 2. Production host still generates its
         * key through LE Rand; no host debug keys or RNG hook are enabled. */
        assert(uECC_compute_public_key(privbe,pubbe,&curve_secp256r1));
        for(i=0;i<32;i++) { pubc[i]=pubbe[31-i]; pubc[32+i]=pubbe[63-i]; }
        smp_send(12,pubc,64); assert(smp_get(12,packet)==65); memcpy(pubp,packet+1,64);
        assert(smp_get(3,confirm)==17); smp_send(4,nc,16); assert(smp_get(4,packet)==17); memcpy(np,packet+1,16);
        assert(!ble_sm_alg_f4(pubp,pubc,np,0,expected) && !memcmp(expected,confirm+1,16));
        assert(!ble_sm_alg_gen_dhkey(pubp,pubp+32,privc,dhkey));
        assert(!ble_sm_alg_f5(dhkey,nc,np,0,central,0,pub,mackey,key));
        assert(!ble_sm_alg_f6(mackey,nc,np,zero,preq+1,0,central,0,pub,expected));
        smp_send(13,expected,16); assert(smp_get(13,packet)==17);
        assert(!ble_sm_alg_f6(mackey,np,nc,zero,pres+1,0,pub,0,central,expected) && !memcmp(expected,packet+1,16));
        assert(fragments65==1);
    } else {
        assert(!ble_sm_alg_c1(zero,nc,preq,pres,0,0,central,pub,expected)); smp_send(3,expected,16);
        assert(smp_get(3,confirm)==17); smp_send(4,nc,16); assert(smp_get(4,packet)==17); memcpy(np,packet+1,16);
        assert(!ble_sm_alg_c1(zero,np,preq,pres,0,0,central,pub,expected) && !memcmp(expected,confirm+1,16));
        assert(!ble_sm_alg_s1(zero,np,nc,key));
    }
    encryption_start(key,rand_ediv);
    if(!sc) {
        assert(smp_get(6,packet)==17); memcpy(saved_ltk,packet+1,16);
        assert(smp_get(7,packet)==11); memcpy(rand_ediv,packet+3,8); memcpy(rand_ediv+8,packet+1,2);
        memset(packet,0x5a,16); smp_send(6,packet,16); memset(packet,0x6b,10); smp_send(7,packet,10);
    } else memcpy(saved_ltk,key,16);
    pump(); ble_host_get_diag(&d);
    assert(d.encrypted && d.key_size==16 && d.bonded && d.pair_method==(sc ? 2 : 1) && !d.pair_failures && !d.resets);
    /* Open ATT read still works encrypted; timestamped write flows to MIDI. */
    l2send(4,(uint8_t[]){2,200,0},3); assert(att_n==3 && att[0]==3 && ble_get16(att+1)==200);
    ble_seed_get_diag(&seed); assert(seed.rand_calls==(sc ? 10u : 6u));
    {
        static const uint8_t uuid[16]={0x00,0xc7,0xc4,0x4e,0xe3,0x6c,0x51,0xa7,0x33,0x4b,0xe8,0xed,0x5a,0x0e,0xb8,0x03};
        uint8_t q[32]={6,1,0,255,255,0,0x28}; uint16_t start,end,value,cccd; uint32_t pkt,ts;
        memcpy(q+7,uuid,16); l2send(4,q,23); assert(att[0]==7); start=ble_get16(att+1); end=ble_get16(att+3);
        q[0]=8; ble_put16(q+1,start); ble_put16(q+3,end); ble_put16(q+5,0x2803); l2send(4,q,7);
        assert(att[0]==9); value=ble_get16(att+5);
        q[0]=4; ble_put16(q+1,(uint16_t)(value+1u)); ble_put16(q+3,end); l2send(4,q,5); assert(att[0]==5); cccd=ble_get16(att+2);
        q[0]=18; ble_put16(q+1,cccd); q[3]=1; q[4]=0; l2send(4,q,5); assert(att[0]==19);
        q[0]=0x52; ble_put16(q+1,value); memcpy(q+3,(uint8_t[]){0x80,0x80,0x90,60,100},5); l2send(4,q,8);
        assert(ble_host_midi_take(&pkt,&ts) && pkt==0x643c9009);
        ble_host_midi_out(pkt,8191); pump(); assert(att[0]==0x1b && ble_get16(att+1)==value && att[3]==0xbf && att[4]==0xff);
    }
    control_rx((uint8_t[]){2,0x13},2); ble_ll_event(0,sim_ticks); pump(); assert(ble_link.state==BLE_LINK_IDLE);
    ble_bonds_get_diag(&bond); assert(!bond.pending && bond.writes && !bond.failures);
    /* Reconstruct the RAM cache from the NOR journal (simulated reboot of
     * store, not a second NimBLE init) and encrypt with its persistent LTK. */
    ble_bonds_init(); sim_advance_us(101000); pump(); connect_link(); encryption_start(saved_ltk,rand_ediv);
    ble_host_get_diag(&d); assert(d.encrypted && d.bonded && d.key_size==16 && !d.pair_failures);
    /* A corrupted tag ends the physical link before Disconnection Complete. */
    { uint8_t bad[6]={2,0x13}; assert(ble_ccm(sk,iv,tx_counter,1,3,bad,2,0)); bad[5]^=1;
      assert(ble_ll_rx(1,0x603,bad,sim_ticks,0)); pump(); assert(ble_link.reason==0x3d && ble_link.state==BLE_LINK_IDLE); }
    ble_host_get_diag(&d); assert(!d.resets && !d.assertions && !d.heap_fail && !d.event_drop && !d.acl_drop);
    printf("NimBLE %s Just Works: real SMP, HCI LTK, CCM link, key size 16, persistent bond reconnect, MTU 200, encrypted MIDI both directions, MIC failure: PASS; LE Rand %u, heap peak %u\n",sc ? "SC" : "legacy",seed.rand_calls,d.heap_peak);
    return 0;
}
