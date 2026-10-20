/* SPDX-License-Identifier: GPL-3.0-only */
/* r4 02 §8: CCM with 13-byte nonce, L=2, one AAD octet and four-byte MIC. */
#include "ble_crypto.h"
#include "ble_s1.h"
#include "ble_memory.h"
void ble_secure_zero(void *p,unsigned n) { volatile uint8_t *v=p; while(n--) *v++=0; }
int ble_aes_le(const uint8_t key[16],const uint8_t in[16],uint8_t out[16])
{
    uint8_t k[16],p[16],result[16]; unsigned i; int ok;
    for(i=0;i<16;i++) { k[i]=key[15-i]; p[i]=in[15-i]; }
    ok=ble_port_aes_begin(k);
    if(ok) ok=ble_port_aes_block(p,result);
    ble_port_aes_end();
    if(ok) for(i=0;i<16;i++) out[i]=result[15-i];
    ble_secure_zero(k,sizeof k); ble_secure_zero(p,sizeof p); ble_secure_zero(result,sizeof result);
    return ok;
}
static int mac_block(uint8_t mac[16],const uint8_t block[16])
{
    uint8_t in[16]; unsigned i;
    for(i=0;i<16;i++) in[i]=mac[i]^block[i];
    return ble_port_aes_block(in,mac);
}
int ble_ccm(const uint8_t key[16],const uint8_t iv[8],uint64_t counter,
    int central,uint8_t llid,uint8_t *data,uint8_t n,int decrypt)
{
    uint8_t ctr[16]={1},block[16]={0x49},mac[16]={0},stream[16],plain[27],tag[4];
    unsigned i,off; uint8_t difference=0; int ok=0;
    if(!n || n>27 || counter>=((uint64_t)1<<39)) return 0;
    for(i=0;i<5;i++) ctr[1+i]=(uint8_t)(counter>>(8u*i));
    ctr[5]|=central ? 0x80u : 0;
    memcpy(ctr+6,iv,8); memcpy(block+1,ctr+1,13); block[15]=n;
    if(!ble_port_aes_begin(key)) goto done;
    if(!mac_block(mac,block)) goto done;
    memset(block,0,16); block[1]=1; block[2]=llid&3u;
    if(!mac_block(mac,block)) goto done;
    /* CTR decryption first; CBC-MAC always authenticates plaintext. */
    for(off=0;off<n;off+=16) {
        ctr[15]=(uint8_t)(off/16u+1u);
        if(!ble_port_aes_block(ctr,stream)) goto done;
        memset(block,0,16);
        for(i=0;i<16 && off+i<n;i++) {
            plain[off+i]=decrypt ? data[off+i]^stream[i] : data[off+i];
            block[i]=plain[off+i];
            if(!decrypt) data[off+i]=plain[off+i]^stream[i];
        }
        if(!mac_block(mac,block)) goto done;
    }
    ctr[15]=0;
    if(!ble_port_aes_block(ctr,stream)) goto done;
    for(i=0;i<4;i++) tag[i]=mac[i]^stream[i];
    if(decrypt) {
        for(i=0;i<4;i++) difference|=tag[i]^data[n+i];
        if(difference) goto done;
        memcpy(data,plain,n);
    } else memcpy(data+n,tag,4);
    ok=1;
done:
    ble_port_aes_end(); ble_secure_zero(plain,sizeof plain); ble_secure_zero(mac,sizeof mac);
    ble_secure_zero(stream,sizeof stream); ble_secure_zero(block,sizeof block); return ok;
}
int ble_crypto_selftest(void)
{
    static const uint8_t key[16]={0xbf,1,0xfb,0x9d,0x4e,0xf3,0xbc,0x36,0xd8,0x74,0xf5,0x39,0x41,0x38,0x68,0x4c};
    static const uint8_t text[16]={0x13,2,0xf1,0xe0,0xdf,0xce,0xbd,0xac,0x79,0x68,0x57,0x46,0x35,0x24,0x13,2};
    static const uint8_t answer[16]={0x66,0xc6,0xc2,0x27,0x8e,0x3b,0x8e,5,0x3e,0x7e,0xa3,0x26,0x52,0x1b,0xad,0x99};
    uint8_t out[16];
    return ble_aes_le(key,text,out) && !memcmp(out,answer,16);
}
