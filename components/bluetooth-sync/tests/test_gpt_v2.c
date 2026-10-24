/* SPDX-License-Identifier: GPL-2.0-only. Synthetic media only. */
#include "nand_policy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;static uint8_t data[BTD_GPT_BYTES];
#define CHECK(e) do{checks++;assert(e);}while(0)
static void put32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i));}
static void put64(uint8_t *p,uint64_t n){put32(p,(uint32_t)n);put32(p+4,(uint32_t)(n>>32));}
static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void checksum(void){unsigned n=get32(data+80);if(n<=128)put32(data+88,btd_crc32(data+512,n*128));put32(data+16,0);put32(data+16,btd_crc32(data,92));}
static void fixture(unsigned count,unsigned index) {
    memset(data,0,sizeof(data));memcpy(data,"EFI PART",8);put32(data+8,0x10000);put32(data+12,92);
    put64(data+24,1);put64(data+32,999999);put64(data+40,34);put64(data+48,999966);
    put64(data+72,2);put32(data+80,count);put32(data+84,128);
    uint8_t *p=data+512+index*128;p[0]=1;put64(p+32,100000);put64(p+40,199999);
    for(unsigned i=0;i<6;i++)p[56+2*i]=(uint8_t)"SYSTEM"[i];
    checksum();
}
static BtdGptResult check(BtdGptInfo *info) {
    BtdWindow w={1,2,true};BtdGptResult rc=btd_check_gpt(data,sizeof(data),1000000,&w,info);
    CHECK((rc==BTD_GPT_OK)==w.ready);
    if(rc==BTD_GPT_OK)CHECK(w.start==100000&&w.end==199999);
    else CHECK(w.start==0&&w.end==0);
    return rc;
}
int main(void) {
    BtdGptInfo info;
    CHECK(btd_crc32((const uint8_t *)"123456789",9)==0xcbf43926u);
    /* Regression: this console declares 24, not 128. CRC must use 3072 bytes. */
    fixture(24,9);CHECK(check(&info)==BTD_GPT_OK&&info.entries==24&&info.system_index==9);
    data[512+24*128+7]^=0xFF;CHECK(check(&info)==BTD_GPT_OK);
    data[512+24*128-1]^=1;CHECK(check(&info)==BTD_GPT_TABLE_CRC);
    for(unsigned count=1;count<=128;count++)for(unsigned index=0;index<count;index++) {
        fixture(count,index);CHECK(check(&info)==BTD_GPT_OK&&info.system_index==index);
    }
    fixture(128,9);put32(data+80,129);checksum();CHECK(check(&info)==BTD_GPT_ENTRY_COUNT);
    fixture(24,9);put32(data+80,0);checksum();CHECK(check(&info)==BTD_GPT_ENTRY_COUNT);
    fixture(24,24);CHECK(check(&info)==BTD_GPT_SYSTEM_MISSING);
    fixture(24,9);data[512+9*128+56+14]=0x7F;checksum();CHECK(check(&info)==BTD_GPT_OK);
    fixture(24,9);data[512+9*128+56+12]='X';checksum();CHECK(check(&info)==BTD_GPT_SYSTEM_MISSING);
    fixture(24,9);memcpy(data+512+10*128,data+512+9*128,128);put64(data+512+10*128+32,300000);put64(data+512+10*128+40,399999);checksum();CHECK(check(&info)==BTD_GPT_DUPLICATE_SYSTEM);
    fixture(24,9);memcpy(data+512+10*128,data+512+9*128,128);checksum();CHECK(check(&info)==BTD_GPT_OVERLAP);
    fixture(24,9);put64(data+512+9*128+40,100000);checksum();CHECK(check(&info)==BTD_GPT_SYSTEM_ALIGNMENT);
    fixture(24,9);put64(data+512+9*128+40,UINT64_MAX);checksum();CHECK(check(&info)==BTD_GPT_PART_RANGE);
    /* Multiple independent problems are retained in one pass. */
    fixture(24,9);put32(data+8,0);put32(data+20,1);data[512+88]^=1;
    CHECK(check(&info)==BTD_GPT_REVISION);
    CHECK(info.problems&(1u<<(BTD_GPT_RESERVED-BTD_GPT_ARGUMENT)));
    CHECK(info.problems&(1u<<(BTD_GPT_HEADER_CRC-BTD_GPT_ARGUMENT)));
    CHECK(info.problems&(1u<<(BTD_GPT_TABLE_CRC-BTD_GPT_ARGUMENT)));
    fixture(24,9);data[0]=0;CHECK(check(&info)==BTD_GPT_SIGNATURE&&!info.signature_valid&&info.entries==0);
    /* Every corruption inside a declared, CRC-covered region is rejected. */
    for(size_t i=0;i<512+24*128;i+=19) {
        if(i>=92&&i<512)continue;
        fixture(24,9);data[i]^=1;CHECK(check(&info)!=BTD_GPT_OK);
    }
    BtdWindow initial={0},ready={100000,199999,true};
    CHECK(btd_read_allowed(&initial,1,33)&&!btd_read_allowed(&initial,1,32));
    CHECK(!btd_read_allowed(&ready,99999,1)&&!btd_read_allowed(&ready,199999,2));
    CHECK(!btd_read_allowed(&ready,UINT32_MAX,2));
    for(unsigned cmd=0;cmd<64;cmd++)for(unsigned emmc=0;emmc<2;emmc++)CHECK(!btd_command_allowed(emmc,cmd,0,true,true,1,512,&ready));
    for(unsigned index=0;index<256;index++)for(unsigned value=0;value<256;value++) {
        bool allowed=(index==183&&(value==0||value==1||value==2||value==5||value==6))||(index==185&&value<=3);
        CHECK(btd_command_allowed(true,6,3u<<24|index<<16|value<<8,false,false,0,0,&ready)==allowed);
    }
    printf("GPT v2: %u checks passed (24-entry regression, declared CRC, bounds, all errors)\n",checks);return 0;
}
