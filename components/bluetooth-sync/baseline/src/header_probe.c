/* SPDX-License-Identifier: GPL-2.0-only */
#include "header_probe.h"
#include "bt_only.h"
#include <stdlib.h>
#include <string.h>
/* Publicly defined by the frozen SHA source; no modification of that core. */
void bsr_sha256(const void *,size_t,uint8_t out[32]);
static uint32_t le32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void layout_summary(const uint8_t *h,BtdHeaderSummary *out) {
    static const unsigned offsets[BTD_LAYOUT_PARTS]={0x300,0x344,0x408,0x608,0x650,0x690,0xAD8};
    static const uint32_t magic[BTD_LAYOUT_PARTS]={0x53465044,0x43465649,0x4C474E4A,0x45564153,0x50414D52,0x50414D52,0x43465649};
    static const uint32_t version[BTD_LAYOUT_PARTS]={0x10000,0x20000,0x10000,0x60000,0x10000,0x10000,0x20000};
    out->layout_checked=true;
    for(unsigned i=0;i<BTD_LAYOUT_PARTS;i++) {
        if(i==BTD_FAT_IVFC&&out->header[0].version!=0x50000)continue;
        out->layout_mask|=1u<<i;
        const uint8_t *p=h+offsets[i];
        if(le32(p)!=magic[i])continue;
        out->layout_version[i]=le32(p+4);
        if(out->layout_version[i]==version[i])out->layout_valid|=1u<<i;
        if(i==BTD_MAIN_RMAP||i==BTD_META_RMAP)for(unsigned j=0;j<3;j++)out->remap_geometry[i-BTD_MAIN_RMAP][j]=le32(p+8+j*4);
        if(i==BTD_DATA_IVFC||i==BTD_FAT_IVFC)out->ivfc_levels[i==BTD_FAT_IVFC]=le32(p+12);
    }
}
BsrResult btd_probe_headers(const BsrStorage *s,BtdComputeCmac compute,void *ctx,BtdHeaderSummary *out) {
    if(!out)return BSR_ARGUMENT;
    memset(out,0,sizeof(*out));
    if(!s||!s->ReadExact||!s->GetSize||!compute)return BSR_ARGUMENT;
    if(!s->GetSize(s->context,&out->container_size))return BSR_IO;
    uint8_t *bytes=malloc(0x8000);if(!bytes)return BSR_MEMORY;
    memset(bytes,0,0x8000);
    for(unsigned i=0;i<2;i++) {
        BtdHeaderCheck *check=&out->header[i];uint8_t *h=bytes+i*0x4000;
        if(out->container_size<(uint64_t)(i+1)*0x4000)continue;
        check->read=s->ReadExact(s->context,i*0x4000,h,0x4000);if(!check->read)continue;
        _Alignas(16) uint8_t cmac[16]={0},digest[32]={0};
        check->cmac_engine=compute(ctx,h,0x4000,cmac);
        uint8_t diff=0;for(unsigned j=0;j<16;j++)diff|=cmac[j]^h[j];
        check->cmac_match=check->cmac_engine&&diff==0;
        check->magic=le32(h+0x100)==UINT32_C(0x46534944);
        if(check->magic)check->version=le32(h+0x104);
        check->version_ok=check->magic&&(check->version==0x40000||check->version==0x50000);
        bsr_sha256(h+0x300,0x3D00,digest);check->sha_match=memcmp(digest,h+0x108,32)==0;
        bto_wipe(cmac,sizeof(cmac));bto_wipe(digest,sizeof(digest));bto_wipe(&diff,sizeof(diff));
    }
    out->identical=out->header[0].read&&out->header[1].read&&memcmp(bytes,bytes+0x4000,0x4000)==0;
    if(out->identical&&out->header[0].cmac_match&&out->header[1].cmac_match&&
       out->header[0].version_ok&&out->header[1].version_ok&&out->header[0].sha_match&&out->header[1].sha_match)
        layout_summary(bytes,out);
    bto_wipe(bytes,0x8000);free(bytes);
    for(unsigned i=0;i<2;i++)if(!out->header[i].read)return BSR_IO;
    for(unsigned i=0;i<2;i++)if(!out->header[i].cmac_match)return BSR_AUTH;
    for(unsigned i=0;i<2;i++)if(!out->header[i].version_ok)return BSR_UNSUPPORTED;
    for(unsigned i=0;i<2;i++)if(!out->header[i].sha_match)return BSR_INTEGRITY;
    return out->identical?BSR_OK:BSR_AMBIGUOUS;
}
