/* SPDX-License-Identifier: GPL-2.0-only. Synthetic save headers; fake CMAC. */
#include "header_probe.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
void bsr_sha256(const void *,size_t,uint8_t out[32]);
static unsigned checks,calls,fail_read,fail_mac,engine_fail;
static uint8_t medium[0x8000];static uint64_t medium_size=sizeof(medium);
#define CHECK(e) do{checks++;assert(e);}while(0)
static void put32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static void fixture(void) {
    memset(medium,0,sizeof(medium));put32(medium+0x100,0x46534944);put32(medium+0x104,0x50000);
    const unsigned offsets[]={0x300,0x344,0x408,0x608,0x650,0x690,0xAD8};
    const uint32_t magic[]={0x53465044,0x43465649,0x4C474E4A,0x45564153,0x50414D52,0x50414D52,0x43465649};
    const uint32_t version[]={0x10000,0x20000,0x10000,0x60000,0x10000,0x10000,0x20000};
    for(unsigned i=0;i<7;i++){put32(medium+offsets[i],magic[i]);put32(medium+offsets[i]+4,version[i]);}
    bsr_sha256(medium+0x300,0x3D00,medium+0x108);memcpy(medium+0x4000,medium,0x4000);
    calls=0;fail_read=0;fail_mac=0;engine_fail=0;medium_size=sizeof(medium);
}
static bool size(void *p,uint64_t *out){(void)p;*out=medium_size;return true;}
static bool read_exact(void *p,uint64_t off,void *out,size_t n) {
    (void)p;CHECK(n==0x4000&&(off==0||off==0x4000));
    if(fail_read&&(off==0x4000))return false;
    memcpy(out,medium+off,n);return true;
}
static bool cmac(void *p,const uint8_t *h,size_t n,uint8_t out[16]) {
    (void)p;CHECK(h&&n==0x4000);calls++;memset(out,fail_mac&&calls==1?1:0,16);return !engine_fail;
}
int main(void) {
    BsrStorage s={NULL,read_exact,size};BtdHeaderSummary out;
    fixture();CHECK(btd_probe_headers(&s,cmac,NULL,&out)==BSR_OK&&calls==2&&out.identical);
    CHECK(out.header[0].cmac_match&&out.header[1].sha_match&&out.header[0].version==0x50000);
    CHECK(out.layout_checked&&out.layout_mask==127&&out.layout_valid==127);
    fixture();fail_mac=1;CHECK(btd_probe_headers(&s,cmac,NULL,&out)==BSR_AUTH&&calls==2);
    CHECK(!out.header[0].cmac_match&&out.header[1].cmac_match&&out.header[1].sha_match);
    CHECK(!out.layout_checked&&out.layout_valid==0);
    fixture();engine_fail=1;CHECK(btd_probe_headers(&s,cmac,NULL,&out)==BSR_AUTH&&calls==2&&!out.header[0].cmac_engine);
    fixture();fail_read=1;CHECK(btd_probe_headers(&s,cmac,NULL,&out)==BSR_IO&&calls==1&&out.header[0].read&&!out.header[1].read);
    fixture();medium_size=0x4000;CHECK(btd_probe_headers(&s,cmac,NULL,&out)==BSR_IO&&calls==1);
    fixture();medium[0x100]=0;CHECK(btd_probe_headers(&s,cmac,NULL,&out)==BSR_UNSUPPORTED&&calls==2);
    CHECK(!out.header[0].magic&&out.header[0].version==0&&out.header[1].version_ok);
    fixture();put32(medium+0x104,0x60000);CHECK(btd_probe_headers(&s,cmac,NULL,&out)==BSR_UNSUPPORTED&&out.header[0].version==0x60000);
    fixture();medium[0x300]^=1;CHECK(btd_probe_headers(&s,cmac,NULL,&out)==BSR_INTEGRITY&&!out.header[0].sha_match&&out.header[1].sha_match);
    fixture();medium[0x20]=1;CHECK(btd_probe_headers(&s,cmac,NULL,&out)==BSR_AMBIGUOUS&&out.header[0].sha_match&&!out.identical);
    CHECK(!out.layout_checked);
    fixture();put32(medium+0x104,0x40000);memcpy(medium+0x4000,medium,0x4000);
    CHECK(btd_probe_headers(&s,cmac,NULL,&out)==BSR_OK&&out.layout_checked&&out.layout_mask==63);
    fixture();put32(medium+0x650+4,0x70000);bsr_sha256(medium+0x300,0x3D00,medium+0x108);memcpy(medium+0x4000,medium,0x4000);
    CHECK(btd_probe_headers(&s,cmac,NULL,&out)==BSR_OK&&!(out.layout_valid&(1u<<BTD_MAIN_RMAP))&&out.layout_version[BTD_MAIN_RMAP]==0x70000);
    CHECK(btd_probe_headers(NULL,cmac,NULL,&out)==BSR_ARGUMENT);
    printf("Header probe: %u checks passed (A/B independently, real SHA, fake CMAC, failures)\n",checks);return 0;
}
