/* SPDX-License-Identifier: GPL-2.0-only */
#include "bt_probe.h"
#include "bt_only.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
BsrResult btd_v6_bt_probe(BsrReader *,BtdBtSummary *);
static unsigned checks,calls,fail_call,wipes;static bool size_fail;
static uint8_t file[0x80000];static uint64_t file_size=sizeof(file);
static BsrReader *reader=(BsrReader *)(void *)file;
#define CHECK(e) do {checks++;assert(e);} while(0)
static void put32(size_t off,uint32_t n){for(unsigned i=0;i<4;i++)file[off+i]=(uint8_t)(n>>(8*i));}
static size_t slot(unsigned i){return i<10?0x8070+i*512:0x2AAA0+(i-10)*512;}
static void record(unsigned i,unsigned type) {
    uint8_t *p=file+slot(i);memset(p,0,512);p[0]=2;p[5]=(uint8_t)(i+1);
    p[0x29]=0x41;p[0x39]=1;p[0x40]=0x7E;p[0x41]=5;
    p[0x42]=type==1?6:type==2?7:9;p[0x43]=0x20;
    strcpy((char *)p+0xD6,type==1?"Joy-Con (L)":type==2?"Joy-Con (R)":"Pro Controller");
    if(type==0){p[0x40]=0x12;p[0x41]=0x34;strcpy((char *)p+0xD6,"SYNTHETIC OTHER BT");}
}
static void reset(unsigned count) {
    memset(file,0xA5,sizeof(file));memset(file,0,16);put32(0,0x220000);put32(4,7);
    put32(0x8060,count);for(unsigned i=0;i<count&&i<24;i++)record(i,i%4);
    calls=fail_call=wipes=0;size_fail=false;file_size=sizeof(file);
}
BsrResult bsr_file_size(const BsrReader *r,uint64_t *size) {
    CHECK(r==reader);if(size_fail)return BSR_IO;*size=file_size;return BSR_OK;
}
BsrResult bsr_file_read(BsrReader *r,uint64_t off,void *out,size_t size) {
    CHECK(r==reader);calls++;
    bool allowed=(off==0&&size==16)||(off==0x8060&&size==4);
    for(unsigned i=0;i<24;i++)if(off==slot(i)&&size==512)allowed=true;
    CHECK(allowed);CHECK(off<=sizeof(file)&&size<=sizeof(file)-off);
    if(fail_call==calls){memset(out,0xCC,size);return BSR_IO;}
    memcpy(out,file+(size_t)off,size);return BSR_OK;
}
void __real_bto_wipe(void *,size_t);
void __wrap_bto_wipe(void *p,size_t n) {
    __real_bto_wipe(p,n);if(n==512)wipes++;
    for(size_t i=0;i<n;i++)CHECK(((uint8_t *)p)[i]==0);
}
static BtdBtSummary probe(BsrResult expected) {
    BtdBtSummary s;memset(&s,0xCC,sizeof(s));CHECK(btd_bt_probe(reader,&s)==expected);
    CHECK(s.bytes_read<=20+24*512);CHECK(wipes>=1);
    if(expected!=BSR_OK)CHECK(!s.complete&&!s.validated&&!s.joy_l&&!s.joy_r&&!s.pro&&!s.other);
    return s;
}
int main(void) {
    BtdBtSummary s;BtoNintendoType type;
    CHECK(btd_bt_probe(reader,NULL)==BSR_ARGUMENT);
    memset(&s,0xCC,sizeof(s));CHECK(btd_bt_probe(NULL,&s)==BSR_ARGUMENT);
    CHECK(s.stage==0&&s.complete==0);
    CHECK(bto_nintendo_type(NULL,&type)==BTO_ARGUMENT&&type==BTO_NINTENDO_NONE);
    BtoRecord r={{0}};CHECK(bto_nintendo_type(&r,NULL)==BTO_ARGUMENT);
    reset(6);CHECK(btd_v6_bt_probe(reader,&s)==BSR_UNSUPPORTED);
    CHECK(s.stage==BTD_BT_PROFILE&&s.version==2228224&&s.flags==7&&s.bytes_read==16&&calls==1&&!s.complete);
    reset(6);s=probe(BSR_OK);CHECK(s.validated==6&&s.count==6);
    unsigned counts[]={0,1,9,10,11,20,24};
    for(unsigned k=0;k<sizeof(counts)/sizeof(*counts);k++) {
        unsigned n=counts[k];reset(n);s=probe(BSR_OK);
        CHECK(s.stage==BTD_BT_DONE&&s.complete&&s.layout_known&&s.validated==n&&s.count==n);
        CHECK(calls==2+n&&s.bytes_read==20+n*512);
        CHECK(s.other+s.joy_l+s.joy_r+s.pro==n);CHECK(wipes>=n+1);
    }
    reset(11);record(10,3);s=probe(BSR_OK);CHECK(s.pro==3); /* split slot 10 */
    reset(24);record(23,1);s=probe(BSR_OK);CHECK(s.joy_l==7); /* last allowed slot */
    uint64_t sizes[]={0,15,0x336A0,0x80000-1,0x80000+1,UINT64_MAX};
    for(unsigned i=0;i<sizeof(sizes)/sizeof(*sizes);i++) {
        reset(1);file_size=sizes[i];s=probe(BSR_UNSUPPORTED);CHECK(calls==0&&s.stage==BTD_BT_SIZE);
    }
    reset(1);size_fail=true;s=probe(BSR_IO);CHECK(calls==0);
    uint32_t versions[]={0,0x140000,0x130000,0x150000,0x170000,UINT32_MAX};
    for(unsigned i=0;i<sizeof(versions)/sizeof(*versions);i++) {
        reset(1);put32(0,versions[i]);s=probe(BSR_UNSUPPORTED);
        CHECK(calls==1&&!s.layout_known&&s.version==versions[i]&&s.stage==BTD_BT_PROFILE);
    }
    for(unsigned i=0;i<16;i++)if(i!=7) {
        reset(1);put32(4,i);s=probe(BSR_UNSUPPORTED);CHECK(calls==1&&!s.layout_known);
    }
    for(unsigned i=8;i<16;i++){reset(1);file[i]=1;s=probe(BSR_UNSUPPORTED);CHECK(calls==1);}
    uint32_t too_many[]={25,32,0x80000000,UINT32_MAX};
    for(unsigned i=0;i<sizeof(too_many)/sizeof(*too_many);i++) {
        reset(0);put32(0x8060,too_many[i]);s=probe(BSR_LIMIT);CHECK(calls==2&&s.stage==BTD_BT_COUNT);
    }
    for(unsigned n=1;n<=24;n++) {
        reset(n);memcpy(file+slot(n-1),file+slot(0),6);
        if(n>1){s=probe(BSR_AMBIGUOUS);CHECK(s.stage==BTD_BT_DUPLICATE&&s.failed_slot==n);}
    }
    for(unsigned k=0;k<7;k++) {
        reset(1);record(0,1);uint8_t *p=file+slot(0);
        if(k==0)memset(p,0,6);
        if(k==1)memset(p,0xFF,6);
        if(k==2)memset(p+0xD6,'X',0xF9);
        if(k==3)p[0x46]=0x81;
        if(k==4)p[0x47]=1;
        if(k==5)p[0xCA]=10;
        if(k==6)p[0x39]=2;
        s=probe(BSR_FORMAT);CHECK(s.stage==BTD_BT_RECORD&&s.failed_slot==1);
    }
    for(unsigned i=0;i<2;i++) {
        reset(4);if(i==0)file[slot(1)+0x39]=0;else memset(file+slot(1)+0x29,0,16);
        s=probe(BSR_AUTH);CHECK(s.stage==BTD_BT_KEY&&s.failed_slot==2);
    }
    reset(1);record(0,1);strcpy((char *)file+slot(0)+0xD6,"Joy-Con (L) X");
    s=probe(BSR_OK);CHECK(s.other==1&&!s.joy_l); /* spoofed name never selected */
    for(unsigned i=1;i<=26;i++) {
        reset(24);fail_call=i;s=probe(BSR_IO);CHECK(calls==i&&!s.complete);
    }
    /* Deterministic malformed inputs: no reading gaps, Wifi, accounts or scanning. */
    uint32_t state=0x41424344;
    for(unsigned k=0;k<2000;k++) {
        reset(24);state=state*1664525u+1013904223u;
        unsigned index=(state>>8)%24;size_t off=slot(index)+(state%512);
        file[off]^=(uint8_t)(1u<<(state%8));
        BsrResult got=btd_bt_probe(reader,&s);
        CHECK(got==BSR_OK||got==BSR_FORMAT||got==BSR_AUTH||got==BSR_AMBIGUOUS);
        CHECK(s.bytes_read<=12308&&calls<=26);
        if(got!=BSR_OK)CHECK(!s.complete&&!s.validated&&!s.joy_l&&!s.joy_r&&!s.pro&&!s.other);
        else CHECK(s.complete&&s.validated==24&&s.other+s.joy_l+s.joy_r+s.pro==24);
    }
    printf("BT probe: %u checks passed (strict profile, split table, malformed, I/O faults, wipes, read allowlist)\n",checks);
    return 0;
}
