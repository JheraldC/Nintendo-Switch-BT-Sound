/* SPDX-License-Identifier: GPL-2.0-only. Fake media and fake AES only. */
#include "diag_native.h"
#include "nand_policy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned checks,reads,decrypts,ends,mounts,fail_init,fail_partition,bad_gpt,fail_read,fail_crypto;
static unsigned force_bad_gpt_request;
static const char *error_stage,*last_number_label;static u32 last_number;
static uint8_t table[BTD_GPT_BYTES];static sdmmc_t sd_controller={SDMMC_1};
sdmmc_storage_t sd_storage={&sd_controller,1000000,true};
#define CHECK(e) do{checks++;assert(e);}while(0)
static void put32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i));}
static void put64(uint8_t *p,uint64_t n){put32(p,(uint32_t)n);put32(p+4,(uint32_t)(n>>32));}
static void fixture(void) {
    memset(table,0,sizeof(table));memcpy(table,"EFI PART",8);put32(table+8,0x10000);put32(table+12,92);
    put64(table+24,1);put64(table+32,999999);put64(table+40,34);put64(table+48,999966);
    put64(table+72,2);put32(table+80,128);put32(table+84,128);
    uint8_t *p=table+512+9*128;p[0]=1;put64(p+32,100000);put64(p+40,199999);
    for(unsigned i=0;i<6;i++)p[56+2*i]=(uint8_t)"SYSTEM"[i];
    put32(table+88,btd_crc32(table+512,128*128));put32(table+16,btd_crc32(table,92));
}
void btd_status(const char *stage,int result){CHECK(stage!=NULL);if(!strcmp(stage,"NAND_INIT")&&result==-1)error_stage=NULL;if(result>0&&!error_stage)error_stage=stage;}
void btd_number(const char *label,u32 value){CHECK(label!=NULL);last_number_label=label;last_number=value;}
void btd_skip(const char *label){CHECK(label!=NULL);}
int sdmmc_storage_init_mmc(sdmmc_storage_t *s,sdmmc_t *ctl,u32 width,u32 timing) {
    CHECK(width==SDMMC_BUS_WIDTH_8&&timing==SDHCI_TIMING_MMC_HS200);
    ctl->id=SDMMC_4;*s=(sdmmc_storage_t){ctl,1000000,true};
    uint8_t ext[512]={0};ext[179]=fail_partition?1:0xF8;
    return !fail_init&&btd_validate_ext_csd(ext);
}
int sdmmc_storage_end(sdmmc_storage_t *s){CHECK(s->sdmmc->id==SDMMC_4);ends++;return 1;}
int sdmmc_storage_set_mmc_partition(sdmmc_storage_t *s,u32 part){CHECK(s->sdmmc->id==SDMMC_4&&part==0);return !fail_partition;}
int sdmmc_storage_read(sdmmc_storage_t *s,u32 first,u32 count,void *out) {
    reads++;
    sdmmc_cmd_t cmd={count==1?17:18,first,0,0};sdmmc_req_t req={out,512,count,0};
    if(force_bad_gpt_request&&first==1){req.num_sectors--;CHECK(!btd_transport_guard(s->sdmmc,&cmd,&req));return 0;}
    CHECK(btd_transport_guard(s->sdmmc,&cmd,&req));
    if(fail_read)return 0;
    if(s->sdmmc->id==SDMMC_1){memset(out,0x55,count*512);return 1;}
    if(first==1){CHECK(count==33);memcpy(out,table,sizeof(table));if(bad_gpt)((uint8_t *)out)[0]^=1;return 1;}
    CHECK(first>=100000&&count==32&&first+count<=200000);
    for(unsigned i=0;i<count;i++)memset((uint8_t *)out+i*512,(first+i-100000)&255,512);
    if(first==100000){uint8_t *b=out;b[11]=0xA5;b[12]=2^0xA5;b[13]=32^0xA5;b[16]=2^0xA5;b[510]=0x55^0xA5;b[511]=0xAA^0xA5;}
    return 1;
}
FRESULT f_mount(FATFS *fs,const char *name,BYTE now){CHECK(strcmp(name,"bis:")==0);(void)now;if(fs)mounts++;return FR_OK;}
int se_aes_xts_crypt_sec(u32 tweak,u32 crypt,u32 enc,uint64_t index,void *out,const void *in,u32 size) {
    CHECK(tweak==5&&crypt==4&&enc==0&&size==32*512&&index<3125);decrypts++;
    if(fail_crypto)return 0;
    for(unsigned i=0;i<size;i++)((uint8_t *)out)[i]=((const uint8_t *)in)[i]^0xA5;
    return 1;
}
static bool zeroed(const uint8_t *p,size_t n){while(n--)if(*p++)return false;return true;}
int main(void) {
    fixture();uint8_t output[64*512];
    sdmmc_cmd_t app={55,0,0,0},detect={42,0,0,0};
    sdmmc_req_t write={output,512,1,1};
    CHECK(!btd_transport_guard(&sd_controller,&detect,NULL));
    CHECK(btd_transport_guard(&sd_controller,&app,NULL));
    CHECK(btd_transport_guard(&sd_controller,&detect,NULL));
    CHECK(!btd_transport_guard(&sd_controller,&detect,NULL));
    CHECK(btd_transport_guard(&sd_controller,&app,NULL));
    CHECK(!btd_transport_guard(&sd_controller,&detect,&write));
    sdmmc_t unknown={2};CHECK(!btd_transport_guard(&unknown,&app,NULL));
    CHECK(connectMMC(2)!=0&&reads==0);CHECK(mountMMCPart("SYSTEM").err!=0);
    uint8_t ext[512]={0};CHECK(!btd_validate_ext_csd(NULL));
    ext[179]=0xF8;CHECK(btd_validate_ext_csd(ext));
    for(unsigned i=1;i<8;i++){ext[179]=(uint8_t)i;CHECK(!btd_validate_ext_csd(ext));}
    CHECK(connectMMC(MMC_CONN_EMMC)==0&&reads==1);
    CHECK(mountMMCPart("USER").err!=0&&mountMMCPart("PRODINFO").err!=0);
    CHECK(mountMMCPart("SYSTEM").err==0&&mounts==1);
    unsigned before=reads;
    CHECK(disk_read(DRIVE_BIS,output,0,2)==RES_OK&&reads==before&&decrypts==1);
    CHECK(output[0]==0xA5&&output[512]==(1^0xA5));
    CHECK(disk_read(DRIVE_BIS,output,2,1)==RES_OK&&reads==before&&decrypts==1);
    CHECK(disk_read(DRIVE_BIS,output,31,3)==RES_OK&&decrypts==2);
    CHECK(output[0]==(31^0xA5)&&output[512]==(32^0xA5)&&output[1024]==(33^0xA5));
    before=reads;CHECK(disk_read(DRIVE_BIS,output,100000,1)==RES_PARERR&&reads==before);
    CHECK(disk_read(DRIVE_BIS,output,99999,2)==RES_PARERR&&reads==before);
    CHECK(disk_read(DRIVE_BIS,output,UINT32_MAX,1)==RES_PARERR&&reads==before);
    CHECK(disk_read(2,output,0,1)==RES_PARERR&&reads==before);
    CHECK(disk_write(DRIVE_BIS,output,0,1)==RES_WRPRT&&reads==before);
    CHECK(disk_write(DRIVE_SD,output,0,1)==RES_WRPRT&&reads==before);
    fail_read=1;memset(output,0xEE,sizeof(output));
    CHECK(disk_read(DRIVE_BIS,output,100,1)==RES_ERROR&&zeroed(output,512));fail_read=0;
    fail_crypto=1;memset(output,0xEE,sizeof(output));
    CHECK(disk_read(DRIVE_BIS,output,100,1)==RES_ERROR&&zeroed(output,512));fail_crypto=0;
    disconnectMMC();before=reads;CHECK(disk_read(DRIVE_BIS,output,0,1)==RES_PARERR&&reads==before&&ends==1);
    fail_init=1;CHECK(connectMMC(MMC_CONN_EMMC)!=0);fail_init=0;
    fail_partition=1;CHECK(connectMMC(MMC_CONN_EMMC)!=0);fail_partition=0;
    bad_gpt=1;CHECK(connectMMC(MMC_CONN_EMMC)!=0&&mountMMCPart("SYSTEM").err!=0);bad_gpt=0;
    CHECK(strcmp(error_stage,"G_SIGNATURE")==0);
    put32(table+80,129);put32(table+16,0);put32(table+16,btd_crc32(table,92));
    CHECK(connectMMC(MMC_CONN_EMMC)!=0&&strcmp(error_stage,"G_ENTRY_COUNT")==0);
    CHECK(strcmp(last_number_label,"ENTRY_COUNT")==0&&last_number==129);fixture();
    fail_read=1;CHECK(connectMMC(MMC_CONN_EMMC)!=0&&strcmp(error_stage,"GPT_READ")==0);fail_read=0;
    CHECK(strcmp(last_number_label,"MMC_READ_DENIED")==0&&last_number==0);
    force_bad_gpt_request=1;CHECK(connectMMC(MMC_CONN_EMMC)!=0);force_bad_gpt_request=0;
    CHECK(strcmp(last_number_label,"MMC_READ_DENIED")==0&&last_number==1);
    CHECK(connectMMC(MMC_CONN_EMMC)==0);before=decrypts;
    CHECK(disk_read(DRIVE_BIS,output,0,1)==RES_OK&&decrypts==before+1);
    disconnectMMC();
    printf("Native storage: %u checks passed (fake media and fake AES)\n",checks);return 0;
}

int btx_transport_allowed(unsigned cmd,unsigned arg,int write,unsigned n,unsigned size){(void)cmd;(void)arg;(void)write;(void)n;(void)size;return 0;}

int bta_sd_disk_write(const void *p,unsigned sector,unsigned n){(void)p;(void)sector;(void)n;return RES_WRPRT;}
int bta_sd_sync(void){return RES_PARERR;}
