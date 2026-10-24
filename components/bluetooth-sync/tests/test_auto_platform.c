#define BTA_PLATFORM_HOST_TEST 1
#include "../src/auto_platform.c"
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
static unsigned checks,write_calls,fail_write,denied,keep_size;
static bool nand_connected,card_removed,power_cut;
static const char marker[]="UNRELATED FILE MUST REMAIN BYTE EXACT";
static uint8_t *medium,*fixture;static const size_t medium_size=70000u*512;
sdmmc_t sd_sdmmc={SDMMC_1};sdmmc_storage_t sd_storage={&sd_sdmmc,BTA_SD_END,1};FATFS sd_fs;
#define CHECK(e) do{checks++;assert(e);}while(0)
void gfx_printf(const char *s,...){(void)s;}
void *ff_memalloc(UINT n){return malloc(n);}void ff_memfree(void *p){free(p);}
int btd_nand_connected(void){return nand_connected?1:0;}
int sd_get_card_removed(void){return card_removed?1:0;}
DSTATUS disk_initialize(BYTE d){return d==DRIVE_SD?0:STA_NOINIT;}
DSTATUS disk_status(BYTE d){return disk_initialize(d);}
DRESULT disk_read(BYTE d,BYTE *p,DWORD sector,UINT n){
    CHECK(d==DRIVE_SD&&n>0);if(power_cut)return RES_ERROR;
    if(sector==0){
        CHECK(n==1);memset(p,0,512);p[510]=0x55;p[511]=0xAA;p[0x1BE + 4]=0x0C;
        u32 start=BTA_SD_FIRST,total=70000;for(unsigned i=0;i<4;i++){p[0x1BE + 8+i]=(BYTE)(start>>(8*i));p[0x1BE + 12+i]=(BYTE)(total>>(8*i));}return RES_OK;
    }
    CHECK(sector>=BTA_SD_FIRST&&(uint64_t)sector+n<=BTA_SD_FIRST+70000u);
    memcpy(p,medium+(size_t)(sector-BTA_SD_FIRST)*512,n*512);return RES_OK;
}
int sdmmc_storage_write(sdmmc_storage_t *s,u32 sector,u32 n,void *p){return __wrap_sdmmc_storage_write(s,sector,n,p);}
int __real_sdmmc_storage_write(sdmmc_storage_t *s,u32 sector,u32 n,void *p){
    CHECK(s==&sd_storage&&!nand_connected&&btx_transport_allowed(25,sector,1,n,512));write_calls++;
    if(power_cut||write_calls==fail_write){power_cut=true;return 0;}
    CHECK(sector>=BTA_SD_FIRST&&(uint64_t)sector+n<=BTA_SD_FIRST+70000u);memcpy(medium+(size_t)(sector-BTA_SD_FIRST)*512,p,n*512);return 1;
}
DRESULT disk_write(BYTE d,const BYTE *p,DWORD sector,UINT n){if(d!=DRIVE_SD){denied++;return RES_WRPRT;}return (DRESULT)bta_sd_disk_write(p,sector,n);}
DRESULT disk_ioctl(BYTE d,BYTE cmd,void *p){(void)p;return d==DRIVE_SD&&cmd==CTRL_SYNC?(DRESULT)bta_sd_sync():RES_PARERR;}
static bool seed(const char *path,const uint8_t *p,size_t n){
    FIL f;UINT got=0;session=true;
    FRESULT result=__real_f_open(&f,path,FA_WRITE|FA_CREATE_NEW);
    if(result==FR_OK){result=__real_f_write(&f,p,(UINT)n,&got);if(f_close(&f)!=FR_OK)result=FR_DISK_ERR;}
    session=false;return result==FR_OK&&got==n;
}
static void assert_keep(void){
    FIL f;UINT got=0;uint8_t data[512]={0};CHECK(__real_f_open(&f,"sd:/keep.txt",FA_READ)==FR_OK);
    CHECK(f_read(&f,data,sizeof(data),&got)==FR_OK);CHECK(f_close(&f)==FR_OK);CHECK(got==keep_size);
    CHECK(bound_public(public_paths[5],bta_hekate_hash,0x30000));
    for(unsigned i=0;i<8;i++)CHECK(!memcmp(data+i*(sizeof(marker)-1),marker,sizeof(marker)-1));
}
int main(void){
    FILE *f=fopen("output/auto-fatfs.bin","rb");CHECK(f!=NULL);fixture=malloc(medium_size);medium=malloc(medium_size);CHECK(fixture&&medium);
    CHECK(fread(fixture,1,medium_size,f)==medium_size);CHECK(!fclose(f));memcpy(medium,fixture,medium_size);keep_size=(sizeof(marker)-1)*8;
    CHECK(f_mount(&sd_fs,"sd:",1)==FR_OK);CHECK(media_ok());
    BthRecords dest={0},src={0},merged={0};BtaState before,after;BtaStats stats;dest.count=src.count=1;
    uint8_t *r=src.record[0];r[0]=2;r[5]=1;r[0x27]=0x25;r[0x29]=42;r[0x39]=1;strcpy((char *)r+0xD6,"Joy-Con (L)");memcpy(dest.record[0],r,512);
    CHECK(bta_state_from_source(&src,&before));src.count=2;memcpy(src.record[1],r,512);src.record[1][5]=2;CHECK(bta_merge(&dest,&src,&before,&merged,&after,&stats));
    uint8_t old_db[BTA_DB_MAX],new_db[BTA_DB_MAX],old_state[BTA_STATE_MAX],new_state[BTA_STATE_MAX];size_t a,b,c,d;
    CHECK(bta_database_encode(&dest,old_db,sizeof(old_db),&a));CHECK(bta_database_encode(&merged,new_db,sizeof(new_db),&b));
    CHECK(bta_state_encode(&before,bta_config_hash,old_state,sizeof(old_state),&c));CHECK(bta_state_encode(&after,bta_config_hash,new_state,sizeof(new_state),&d));
    CHECK(seed(paths[BTA_DB],old_db,a));CHECK(seed(paths[BTA_STATE],old_state,c));CHECK(f_mount(NULL,"sd:",0)==FR_OK);
    memcpy(fixture,medium,medium_size);const BtaIo *io=bta_platform_io();
    unsigned normal_writes=0,recovered=0,stopped_safely=0;
    for(unsigned failure=0;failure<=(normal_writes?normal_writes+2:40);failure++){
        session=false;owner=-1;raw_armed=false;power_cut=false;write_calls=0;fail_write=failure;
        memcpy(medium,fixture,medium_size);memset(&sd_fs,0,sizeof(sd_fs));CHECK(f_mount(&sd_fs,"sd:",1)==FR_OK);CHECK(bta_recover(io,bta_config_hash));
        bool success=bta_transaction(io,bta_config_hash,old_db,a,old_state,c,new_db,b,new_state,d);
        if(!failure){CHECK(success);normal_writes=write_calls;}
        power_cut=false;fail_write=0;session=false;owner=-1;raw_armed=false;
        CHECK(f_mount(NULL,"sd:",0)==FR_OK);memset(&sd_fs,0,sizeof(sd_fs));CHECK(f_mount(&sd_fs,"sd:",1)==FR_OK);
        bool safe=bta_recover(io,bta_config_hash);if(safe){
            uint8_t data[BTA_DB_MAX];size_t n=0;CHECK(io->read(NULL,BTA_DB,data,sizeof(data),&n)==1);
            CHECK((n==a&&!memcmp(data,old_db,a))||(n==b&&!memcmp(data,new_db,b)));recovered++;
        } else stopped_safely++;
        assert_keep();CHECK(f_mount(NULL,"sd:",0)==FR_OK);
    }
    memcpy(medium,fixture,medium_size);memset(&sd_fs,0,sizeof(sd_fs));power_cut=false;fail_write=0;CHECK(f_mount(&sd_fs,"sd:",1)==FR_OK);
    FIL arbitrary;UINT written=123;CHECK(__wrap_f_open(&arbitrary,"bis:/save/8000000000000050",FA_WRITE)==FR_DENIED);
    CHECK(__wrap_f_open(&arbitrary,"sd:/keep.txt",FA_WRITE|FA_CREATE_NEW)==FR_DENIED);
    CHECK(__wrap_f_write(&arbitrary,old_db,(UINT)a,&written)==FR_DENIED&&written==0);
    session=true;CHECK(__wrap_f_unlink(paths[BTA_DB])==FR_DENIED);CHECK(__wrap_f_rename(paths[BTA_DB],"sd:/keep.txt")==FR_DENIED);
    nand_connected=true;CHECK(bta_sd_disk_write(old_db,BTA_SD_FIRST+5000,1)==RES_WRPRT);nand_connected=false;
    card_removed=true;CHECK(bta_sd_disk_write(old_db,BTA_SD_FIRST+5000,1)==RES_WRPRT);card_removed=false;
    CHECK(bta_sd_disk_write(old_db,BTA_SD_FIRST-1,1)==RES_WRPRT);CHECK(bta_sd_disk_write(old_db,BTA_SD_END,1)==RES_WRPRT);
    CHECK(bta_sd_disk_write(old_db,BTA_SD_FIRST+5000,129)==RES_WRPRT);session=false;
    CHECK(!btx_transport_allowed(25,BTA_SD_FIRST+5000,1,1,512));CHECK(disk_write(DRIVE_BIS,old_db,0,1)==RES_WRPRT&&denied==1);
    assert_keep();free(medium);free(fixture);
    printf("Actual SD platform: %u checks passed; %u normal sector writes; fault-prefix recovery=%u, safe-stop=%u; unrelated file preserved; NAND/path/media guards passed\n",checks,normal_writes,recovered,stopped_safely);return 0;
}
