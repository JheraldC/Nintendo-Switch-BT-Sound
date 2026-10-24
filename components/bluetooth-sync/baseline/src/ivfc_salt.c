/* SPDX-License-Identifier: GPL-2.0-only */
#include "ivfc_bridge.h"
#include "sha256.h"
#include <string.h>
void btd_ivfc_hmac(const uint8_t seed[32],const void *label,size_t label_size,uint8_t out[32]) {
    /* The frozen call sites use fixed public labels of 48/44 bytes. The
       seed is the 32-byte message, not the HMAC key. Support generic label
       length here using standard HMAC; production never accepts user labels. */
    uint8_t key[64]={0},pad[64],inner[32];BsrSha sha;
    if(label_size>sizeof(key))bsr_sha256(label,label_size,key);
    else memcpy(key,label,label_size);
    for(unsigned i=0;i<64;i++)pad[i]=key[i]^0x36;
    bsr_sha_init(&sha);bsr_sha_update(&sha,pad,64);bsr_sha_update(&sha,seed,32);bsr_sha_final(&sha,inner);
    for(unsigned i=0;i<64;i++)pad[i]=key[i]^0x5C;
    bsr_sha_init(&sha);bsr_sha_update(&sha,pad,64);bsr_sha_update(&sha,inner,32);bsr_sha_final(&sha,out);
    bsr_wipe(key,sizeof(key));bsr_wipe(pad,sizeof(pad));bsr_wipe(inner,sizeof(inner));
}
