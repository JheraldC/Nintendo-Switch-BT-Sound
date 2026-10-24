/* SPDX-License-Identifier: GPL-2.0-only. All data synthetic. */
#include "nand_policy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(e) do{checks++;assert(e);}while(0)
static uint8_t data[BTD_GPT_BYTES];
static void put32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i));}
static void put64(uint8_t *p,uint64_t n){put32(p,(uint32_t)n);put32(p+4,(uint32_t)(n>>32));}
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
static bool parse(BtdWindow *w){return btd_parse_gpt(data,sizeof(data),1000000,w);}
static void reject(void){BtdWindow w={1,2,true};CHECK(!parse(&w));CHECK(!w.ready&&w.start==0&&w.end==0);}
int main(void) {
    BtdWindow w={0};fixture();CHECK(parse(&w)&&w.ready&&w.start==100000&&w.end==199999);
    CHECK(btd_crc32((const uint8_t *)"123456789",9)==0xcbf43926u);
    for(size_t i=0;i<sizeof(data);i+=37){if(i>=92&&i<512)continue;fixture();data[i]^=1;reject();}
    fixture();put32(data+80,129);checksum();reject();
    fixture();put64(data+72,3);checksum();reject();
    fixture();put32(data+12,512);checksum();reject();
    fixture();put64(data+512+9*128+40,100000);checksum();reject();
    fixture();put64(data+512+9*128+40,UINT64_MAX);checksum();reject();
    fixture();data[512+9*128+56]='U';checksum();reject();
    fixture();data[512+9*128+56+12]='X';checksum();reject();
    fixture();memcpy(data+512+10*128,data+512+9*128,128);put64(data+512+10*128+32,300000);put64(data+512+10*128+40,399999);checksum();reject();
    fixture();memcpy(data+512+10*128,data+512+9*128,128);memset(data+512+10*128+56,0,72);checksum();reject();
    fixture();memcpy(data+512+10*128,data+512+9*128,128);memset(data+512+9*128,0,128);checksum();reject();
    CHECK(!btd_parse_gpt(NULL,sizeof(data),1000000,&w));
    CHECK(!btd_parse_gpt(data,sizeof(data)-1,1000000,&w));
    CHECK(!btd_parse_gpt(data,sizeof(data),1000000,NULL));
    BtdWindow initial={0},ready={100000,199999,true};
    CHECK(btd_read_allowed(&initial,1,33));CHECK(!btd_read_allowed(&initial,0,33));
    CHECK(!btd_read_allowed(&initial,1,1)&&!btd_read_allowed(&initial,2,32));
    CHECK(btd_read_allowed(&ready,100000,100000));CHECK(btd_read_allowed(&ready,199999,1));
    CHECK(!btd_read_allowed(&ready,99999,1)&&!btd_read_allowed(&ready,200000,1));
    CHECK(!btd_read_allowed(&ready,199999,2)&&!btd_read_allowed(&ready,100000,UINT32_MAX));
    CHECK(!btd_read_allowed(&ready,100000,0)&&!btd_read_allowed(NULL,1,33));
    for(unsigned cmd=0;cmd<64;cmd++)for(unsigned emmc=0;emmc<2;emmc++) {
        CHECK(!btd_command_allowed(emmc,cmd,0,true,true,1,512,&ready));
    }
    const unsigned denied[]={24,25,26,27,28,29,32,33,35,36,38,42,56};
    for(unsigned i=0;i<sizeof(denied)/sizeof(*denied);i++) {
        CHECK(!btd_command_allowed(false,denied[i],0,false,false,0,0,&ready));
        CHECK(!btd_command_allowed(true,denied[i],0,false,false,0,0,&ready));
    }
    for(unsigned index=0;index<256;index++)for(unsigned value=0;value<256;value++) {
        uint32_t arg=3u<<24|index<<16|value<<8;
        bool expected=(index==183&&(value==0||value==1||value==2||value==5||value==6))||(index==185&&value<=3);
        CHECK(btd_command_allowed(true,6,arg,false,false,0,0,&ready)==expected);
    }
    CHECK(!btd_command_allowed(true,6,1u<<24|183<<16,false,false,0,0,&ready));
    CHECK(!btd_command_allowed(true,6,3u<<24|183<<16|1,false,false,0,0,&ready));
    CHECK(btd_command_allowed(true,18,1,true,false,33,512,&initial));
    CHECK(!btd_command_allowed(true,18,1,true,false,32,512,&initial));
    CHECK(btd_command_allowed(true,18,100000,true,false,32,512,&ready));
    CHECK(!btd_command_allowed(true,18,1,true,false,33,512,&ready));
    CHECK(!btd_command_allowed(true,18,100000,false,false,32,512,&ready));
    CHECK(!btd_command_allowed(true,18,100000,true,false,32,1024,&ready));
    CHECK(btd_command_allowed(true,8,0,true,false,1,512,&initial));
    CHECK(!btd_command_allowed(true,11,100000,true,false,32,512,&ready));
    CHECK(!btd_command_allowed(true,19,0,false,false,0,0,&ready));
    CHECK(!btd_command_allowed(false,60,0,false,false,0,0,&ready));
    printf("NAND policy: %u checks passed (synthetic GPT, transport and range gates)\n",checks);
    return 0;
}
