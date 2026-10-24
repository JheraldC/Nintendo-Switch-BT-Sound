/* SPDX-License-Identifier: GPL-2.0-only
 * Adapter/denial behavior only; crypto and frozen reader are mocks here. */
#include "tegra_reader.h"
#include "tegra_keys.h"
#include "bt_only.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct BsrReader { BsrStorage storage; };
static unsigned checks,opens,closes,key_sets,key_clears,real_writes,partition_calls;
static unsigned fail_open,short_read,bad_cmac;static uint8_t medium[0x8000];
static unsigned connections,disconnections,bis_sets,bis_clears,key_opens,key_closes;
static unsigned fail_connect,fail_mount,fail_keys,key_short,key_close_fail;
static const char valid_keys[]="bis_key_02 = 000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f\nsave_mac_key = 202122232425262728292a2b2c2d2e2f\n";
static const char *key_text=valid_keys;
static unsigned core_calls;
#ifdef BTO_DIAGNOSTIC
static unsigned header_a_reports,header_b_reports;
#endif
void bsr_sha256(const void *,size_t,uint8_t out[32]);
#define CHECK(expr) do{checks++;assert(expr);}while(0)
#ifdef BTO_DIAGNOSTIC
void btd_status(const char *label,int code){CHECK(label!=NULL);(void)code;if(!strcmp(label,"HDR_A_CMAC"))header_a_reports++;if(!strcmp(label,"HDR_B_CMAC"))header_b_reports++;}
void btd_number(const char *label,uint32_t value){CHECK(label!=NULL);(void)value;}
void btd_skip(const char *label){CHECK(label!=NULL);}
#endif
int connectMMC(uint8_t mode){CHECK(mode==MMC_CONN_EMMC);connections++;return fail_connect?1:0;}
void disconnectMMC(void){disconnections++;}
ErrCode_t mountMMCPart(const char *part){CHECK(strcmp(part,"SYSTEM")==0);return (ErrCode_t){fail_mount?1:0};}
FRESULT f_open(FIL *f,const char *path,BYTE mode) {
    if(strcmp(path,"sd:/switch/prod.keys")==0) {
        CHECK(mode==FA_READ);key_opens++;
        if(fail_keys)return FR_DENIED;
        *f=(FIL){strlen(key_text),0,1};return FR_OK;
    }
    CHECK(strcmp(path,"bis:/save/8000000000000050")==0&&mode==FA_READ);
    opens++;if(fail_open)return FR_DENIED;*f=(FIL){sizeof(medium),0,0};return FR_OK;
}
FRESULT f_lseek(FIL *f,FSIZE_t pos){if(pos>f->size)return FR_DENIED;f->position=pos;return FR_OK;}
FRESULT f_read(FIL *f,void *out,UINT n,UINT *actual) {
    if(f->kind==1) {
        CHECK(n<=128&&f->position<=f->size&&n<=f->size-f->position);
        *actual=key_short&&n?n-1:n;memcpy(out,key_text+f->position,*actual);f->position+=*actual;return FR_OK;
    }
    CHECK(f->position<=sizeof(medium)&&n<=sizeof(medium)-f->position);
    *actual=short_read&&n?n-1:n;memcpy(out,medium+f->position,*actual);f->position+=*actual;return FR_OK;
}
FRESULT f_close(FIL *f){if(f->kind==1){key_closes++;return key_close_fail?FR_DENIED:FR_OK;}closes++;return FR_OK;}
FRESULT f_write(FIL *f,const void *p,UINT n,UINT *w){(void)f;(void)p;(void)n;(void)w;real_writes++;return FR_OK;}
FRESULT f_unlink(const char *p){(void)p;real_writes++;return FR_OK;}
FRESULT f_mkdir(const char *p){(void)p;real_writes++;return FR_OK;}
FRESULT f_rename(const char *a,const char *b){(void)a;(void)b;real_writes++;return FR_OK;}
int emummc_storage_write(sdmmc_storage_t *s,u32 a,u32 b,void *p){(void)s;(void)a;(void)b;(void)p;real_writes++;return 1;}
int sdmmc_storage_write(sdmmc_storage_t *s,u32 a,u32 b,void *p){(void)s;(void)a;(void)b;(void)p;real_writes++;return 1;}
int nx_emmc_bis_write(u32 a,u32 b,void *p){(void)a;(void)b;(void)p;real_writes++;return 0;}
int emummc_storage_set_mmc_partition(sdmmc_storage_t *s,u32 p){(void)s;CHECK(p==0);partition_calls++;return 1;}
void se_aes_key_set(u32 slot,const void *key,u32 n){
    CHECK(key!=NULL&&n==16);
    if(slot==4||slot==5){bis_sets++;CHECK(((const uint8_t *)key)[0]==(slot==4?0:16));}
    else {CHECK(slot==10);key_sets++;}
}
void se_aes_key_clear(u32 slot){if(slot==4||slot==5)bis_clears++;else{CHECK(slot==10);key_clears++;}}
int se_aes_cmac(u32 slot,void *out,u32 n,const void *in,u32 size) {
    CHECK(slot==10&&n==16&&in!=NULL&&size==0x200);memset(out,bad_cmac?1:0,16);return 1;
}
BsrResult bsr_open(const BsrStorage *s,BsrValidateCmac auth,void *ctx,BsrReader **out,BsrError *error) {
    core_calls++;
    uint64_t size=0;uint8_t header[0x4000];*out=NULL;(void)error;
    CHECK(s->GetSize(s->context,&size)&&size==sizeof(medium));
    CHECK(!s->ReadExact(s->context,UINT64_MAX,header,1));
    for(unsigned i=0;i<2;i++) {
        memset(header,0xA5,sizeof(header));
        if(!s->ReadExact(s->context,i*sizeof(header),header,sizeof(header))) {
            CHECK(header[0]==0&&header[sizeof(header)-1]==0);return BSR_IO;
        }
        if(!auth(ctx,header,sizeof(header)))return BSR_AUTH;
    }
    *out=malloc(sizeof(**out));CHECK(*out!=NULL);(*out)->storage=*s;return BSR_OK;
}
void bsr_close(BsrReader **p){if(p){free(*p);*p=NULL;}}
static bool zeroed(const void *p,size_t n){const uint8_t *b=p;while(n--)if(*b++)return false;return true;}
/* Calls within the TU that defines a real backend are not rewritten by ld.
 * Exercise denial bodies explicitly here; calls from tegra_reader.c still
 * pass through the actual linker wraps and reach these real mock backends. */
FRESULT __wrap_f_open(FIL *,const TCHAR *,BYTE);
FRESULT __wrap_f_write(FIL *,const void *,UINT,UINT *);
FRESULT __wrap_f_unlink(const TCHAR *);FRESULT __wrap_f_mkdir(const TCHAR *);
FRESULT __wrap_f_rename(const TCHAR *,const TCHAR *);
int __wrap_emummc_storage_write(sdmmc_storage_t *,u32,u32,void *);
int __wrap_sdmmc_storage_write(sdmmc_storage_t *,u32,u32,void *);
int __wrap_nx_emmc_bis_write(u32,u32,void *);
int __wrap_emummc_storage_set_mmc_partition(sdmmc_storage_t *,u32);
#define f_open __wrap_f_open
#define f_write __wrap_f_write
#define f_unlink __wrap_f_unlink
#define f_mkdir __wrap_f_mkdir
#define f_rename __wrap_f_rename
#define emummc_storage_write __wrap_emummc_storage_write
#define sdmmc_storage_write __wrap_sdmmc_storage_write
#define nx_emmc_bis_write __wrap_nx_emmc_bis_write
#define emummc_storage_set_mmc_partition __wrap_emummc_storage_set_mmc_partition
int main(void) {
    medium[0x100]='D';medium[0x101]='I';medium[0x102]='S';medium[0x103]='F';medium[0x106]=5;
    const unsigned offsets[]={0x300,0x344,0x408,0x608,0x650,0x690,0xAD8};
    const uint32_t magic[]={0x53465044,0x43465649,0x4C474E4A,0x45564153,0x50414D52,0x50414D52,0x43465649};
    const uint32_t version[]={0x10000,0x20000,0x10000,0x60000,0x10000,0x10000,0x20000};
    for(unsigned i=0;i<7;i++)for(unsigned j=0;j<4;j++){medium[offsets[i]+j]=(uint8_t)(magic[i]>>(8*j));medium[offsets[i]+4+j]=(uint8_t)(version[i]>>(8*j));}
    bsr_sha256(medium+0x300,0x3D00,medium+0x108);memcpy(medium+0x4000,medium,0x4000);
    FIL file={0};sdmmc_storage_t dev={0};UINT actual=99;uint8_t byte=0,key[16]={1};
    CHECK(f_open(&file,"sd:/switch/prod.keys",FA_READ|FA_WRITE)==FR_DENIED);
    CHECK(f_open(&file,"sd:/switch/title.keys",FA_READ)==FR_DENIED);
    CHECK(f_open(&file,"sd:/switch/../prod.keys",FA_READ)==FR_DENIED);
    CHECK(f_open(&file,"bis:/save/8000000000000050",FA_READ|FA_WRITE)==FR_DENIED);
    CHECK(f_open(&file,"bis:/save/8000000000000051",FA_READ)==FR_DENIED);
    CHECK(f_open(&file,"bis:/save/8000000000000050/../8000000000000051",FA_READ)==FR_DENIED);
    CHECK(f_write(&file,&byte,1,&actual)==FR_WRITE_PROTECTED&&actual==0);
    CHECK(f_unlink("any")==FR_WRITE_PROTECTED&&f_mkdir("any")==FR_WRITE_PROTECTED);
    CHECK(f_rename("a","b")==FR_WRITE_PROTECTED);
    CHECK(emummc_storage_write(&dev,0,1,&byte)==0&&sdmmc_storage_write(&dev,0,1,&byte)==0);
    CHECK(nx_emmc_bis_write(0,1,&byte)==RES_WRPRT&&real_writes==0);
    CHECK(emummc_storage_set_mmc_partition(&dev,1)==0&&emummc_storage_set_mmc_partition(&dev,2)==0);
    CHECK(partition_calls==0&&emummc_storage_set_mmc_partition(&dev,0)==1&&partition_calls==1);
    BtoTegraReader reader={0};BsrError error={0};uint8_t empty_key[16]={0};
    CHECK(bto_tegra_open(&reader,empty_key,&error)==BSR_AUTH&&opens==0&&key_sets==0);
    CHECK(bto_tegra_open(&reader,key,&error)==BSR_OK&&opens==1&&key_sets==1);
    CHECK(reader.file_open&&reader.reader!=NULL);
    bto_tegra_close(&reader);CHECK(zeroed(&reader,sizeof(reader))&&closes==1);
    bad_cmac=1;CHECK(bto_tegra_open(&reader,key,&error)==BSR_AUTH);bad_cmac=0;
#ifdef BTO_DIAGNOSTIC
    CHECK(header_a_reports==2&&header_b_reports==2&&core_calls==1);
#endif
    CHECK(zeroed(&reader,sizeof(reader))&&closes==2);
    short_read=1;CHECK(bto_tegra_open(&reader,key,&error)==BSR_IO);short_read=0;
    CHECK(zeroed(&reader,sizeof(reader))&&closes==3);
    fail_open=1;CHECK(bto_tegra_open(&reader,key,&error)==BSR_IO);
    CHECK(zeroed(&reader,sizeof(reader))&&closes==3&&real_writes==0&&key_clears>0);
    fail_open=0;
    BtoKeys keys;memset(&keys,0xA5,sizeof(keys));
    fail_keys=1;CHECK(bto_tegra_keys_load(&keys)==BTO_KEYS_IO&&zeroed(&keys,sizeof(keys)));fail_keys=0;
    key_short=1;CHECK(bto_tegra_keys_load(&keys)==BTO_KEYS_IO&&zeroed(&keys,sizeof(keys)));key_short=0;
    key_close_fail=1;CHECK(bto_tegra_keys_load(&keys)==BTO_KEYS_IO&&zeroed(&keys,sizeof(keys)));key_close_fail=0;
    key_text="bis_key_02 = 0011\n";
    unsigned old_connections=connections;
    CHECK(bto_tegra_open_from_sd(&reader,&error)==BSR_AUTH&&connections==old_connections&&bis_sets==0);
    key_text=valid_keys;
    CHECK(bto_tegra_open_from_sd(&reader,&error)==BSR_OK&&reader.owns_bis_slots&&bis_sets==2&&bis_clears==0);
    bto_tegra_close(&reader);CHECK(zeroed(&reader,sizeof(reader))&&bis_clears==2);
    bad_cmac=1;CHECK(bto_tegra_open_from_sd(&reader,&error)==BSR_AUTH&&bis_sets==4&&bis_clears==4);bad_cmac=0;
    CHECK(zeroed(&reader,sizeof(reader)));
    fail_mount=1;CHECK(bto_tegra_open_from_sd(&reader,&error)==BSR_IO&&bis_sets==6&&bis_clears==6);fail_mount=0;
    CHECK(zeroed(&reader,sizeof(reader)));
    fail_connect=1;CHECK(bto_tegra_open_from_sd(&reader,&error)==BSR_IO&&bis_sets==8&&bis_clears==8);fail_connect=0;
    CHECK(zeroed(&reader,sizeof(reader))&&real_writes==0&&connections==disconnections+1);
    CHECK(key_opens==key_closes+1);
    puts("tegra adapter: read-only path, short IO, authentication rejection and write denials passed (mocks)");
    printf("tegra adapter: %u checks passed\n",checks);return 0;
}

int __wrap_sdmmc_storage_write(sdmmc_storage_t *s,u32 first,u32 n,void *p){(void)s;(void)first;(void)n;(void)p;return 0;}
