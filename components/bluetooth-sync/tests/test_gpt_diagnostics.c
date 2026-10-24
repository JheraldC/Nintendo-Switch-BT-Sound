/* SPDX-License-Identifier: GPL-2.0-only. All media synthetic. */
#include "nand_policy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(e) do{checks++;assert(e);}while(0)
bool btd_v1_parse_gpt(const uint8_t *,size_t,uint32_t,BtdWindow *);
static uint8_t data[BTD_GPT_BYTES];
static void put32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static void put64(uint8_t *p,uint64_t v){put32(p,(uint32_t)v);put32(p+4,(uint32_t)(v>>32));}
static void checksum(void) {
    put32(data+88,btd_crc32(data+512,128*128));put32(data+16,0);put32(data+16,btd_crc32(data,92));
}
static void fixture(void) {
    memset(data,0,sizeof(data));memcpy(data,"EFI PART",8);put32(data+8,0x10000);put32(data+12,92);
    put64(data+24,1);put64(data+32,999999);put64(data+40,34);put64(data+48,999966);
    put64(data+72,2);put32(data+80,128);put32(data+84,128);
    uint8_t *p=data+512+9*128;p[0]=1;put64(p+32,100000);put64(p+40,199999);
    for(unsigned i=0;i<6;i++)p[56+2*i]=(uint8_t)"SYSTEM"[i];
    checksum();
}
static void compare(BtdGptResult expected) {
    BtdWindow old={0},new={0};BtdGptInfo info={0};
    bool accepted=btd_v1_parse_gpt(data,sizeof(data),1000000,&old);
    BtdGptResult result=btd_check_gpt(data,sizeof(data),1000000,&new,&info);
    CHECK(result==expected);CHECK(accepted==(result==BTD_GPT_OK));
    CHECK(old.ready==new.ready&&old.start==new.start&&old.end==new.end);
    CHECK(strcmp(btd_gpt_result_stage(result),"G_UNKNOWN")!=0);
}
int main(void) {
    fixture();compare(BTD_GPT_OK);
    fixture();data[0]^=1;compare(BTD_GPT_SIGNATURE);
    BtdWindow window={0};BtdGptInfo info;
    CHECK(btd_check_gpt(data,sizeof(data),1000000,&window,&info)==BTD_GPT_SIGNATURE);
    CHECK(!info.signature_valid&&info.entries==0&&info.backup_lba==0);
    fixture();put32(data+8,0);compare(BTD_GPT_REVISION);
    fixture();put32(data+12,512);checksum();compare(BTD_GPT_HEADER_SIZE);
    fixture();put32(data+20,1);compare(BTD_GPT_RESERVED);
    fixture();put64(data+24,2);compare(BTD_GPT_PRIMARY_LBA);
    fixture();put64(data+32,999998);checksum();compare(BTD_GPT_BACKUP_LBA);
    fixture();put64(data+40,33);compare(BTD_GPT_USABLE_RANGE);
    fixture();put64(data+72,3);compare(BTD_GPT_TABLE_LBA);
    fixture();put32(data+80,11);checksum();compare(BTD_GPT_ENTRY_COUNT);
    CHECK(btd_check_gpt(data,sizeof(data),1000000,&window,&info)==BTD_GPT_ENTRY_COUNT&&info.entries==11);
    fixture();put32(data+84,256);compare(BTD_GPT_ENTRY_SIZE);
    fixture();data[16]^=1;compare(BTD_GPT_HEADER_CRC);
    fixture();data[512+100]^=1;compare(BTD_GPT_TABLE_CRC);
    fixture();put64(data+512+9*128+40,UINT64_MAX);checksum();compare(BTD_GPT_PART_RANGE);
    fixture();memcpy(data+512+10*128,data+512+9*128,128);checksum();compare(BTD_GPT_OVERLAP);
    fixture();memcpy(data+512+10*128,data+512+9*128,128);memset(data+512+9*128,0,128);checksum();compare(BTD_GPT_SYSTEM_INDEX);
    fixture();put64(data+512+9*128+40,100000);checksum();compare(BTD_GPT_SYSTEM_ALIGNMENT);
    fixture();memcpy(data+512+10*128,data+512+9*128,128);put64(data+512+10*128+32,300000);put64(data+512+10*128+40,399999);checksum();compare(BTD_GPT_DUPLICATE_SYSTEM);
    fixture();data[512+9*128+56]='U';checksum();compare(BTD_GPT_SYSTEM_MISSING);
    CHECK(btd_check_gpt(data,sizeof(data)-1,1000000,&window,&info)==BTD_GPT_ARGUMENT);
    /* Differential checks preserve v1 acceptance exactly; only reporting changes. */
    uint32_t random=0x12345678;
    for(unsigned i=0;i<800;i++) {
        fixture();random=random*1664525u+1013904223u;
        size_t offset=random%sizeof(data);data[offset]^=(uint8_t)((random>>24)|1);
        if(i&1)checksum();
        BtdWindow a={0},b={0};bool old=btd_v1_parse_gpt(data,sizeof(data),1000000,&a);
        bool new=btd_parse_gpt(data,sizeof(data),1000000,&b);
        CHECK(old==new);CHECK(a.ready==b.ready&&a.start==b.start&&a.end==b.end);
    }
    printf("GPT diagnostics: %u checks passed (exact errors and v1 acceptance comparison)\n",checks);
    return 0;
}
