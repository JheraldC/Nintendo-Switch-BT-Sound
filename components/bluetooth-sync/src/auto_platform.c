/* SPDX-License-Identifier: GPL-2.0-only */
#include "diag_native.h"
#ifdef BTA_PLATFORM_HOST_TEST
#include "mock_auto_platform.h"
#else
#include <storage/nx_sd.h>
#include <libs/fatfs/ff.h>
#include <libs/fatfs/diskio.h>
#endif
#include "auto_platform.h"
#include "auto_binding.h"
#include "sha256.h"
#include <string.h>
#include <stdlib.h>
static const char *const paths[BTA_PATHS]={
    "sd:/atmosphere/bluetooth_devices.db","sd:/switch/bt-autosync-v1/history.bin",
    "sd:/atmosphere/bluetooth_devices.bt-next","sd:/switch/bt-autosync-v1/history.next",
    "sd:/atmosphere/bluetooth_devices.bt-prev","sd:/switch/bt-autosync-v1/history.prev",
    "sd:/switch/bt-autosync-v1/transaction.bin"
};
static const char *const public_paths[]={"sd:/switch/prod.keys","bis:/save/8000000000000050",
    "sd:/emuMMC/emummc.ini","sd:/atmosphere/config/system_settings.ini","sd:/bootloader/hekate_ipl.ini",
    "sd:/switch/bt-autosync-v1/hekate-continue.bin"};
static FIL owned;
static int owner=-1;
static bool session,raw_armed;
static uint32_t raw_first,raw_count;
static const void *raw_buffer;
static bool media_ok(void){
    if(btd_nand_connected()||sd_get_card_removed()||!sd_storage.has_sector_access||sd_storage.sdmmc!=&sd_sdmmc||sd_sdmmc.id!=SDMMC_1)return false;
    if(sd_fs.fs_type!=FS_FAT32||sd_fs.pdrv!=DRIVE_SD||sd_fs.volbase!=BTA_SD_FIRST||sd_fs.n_fatent<3||!sd_fs.csize)return false;
    uint64_t end=(uint64_t)sd_fs.database+(uint64_t)(sd_fs.n_fatent-2)*sd_fs.csize;
    return end<=BTA_SD_END&&BTA_SD_END<=sd_storage.sec_cnt&&sd_fs.database>=BTA_SD_FIRST;
}
static int path_id(const char *p){if(p)for(int i=0;i<BTA_PATHS;i++)if(!strcmp(p,paths[i]))return i;return -1;}
static size_t limit(int id){return id==BTA_TXN?BTA_TXN_SIZE:id==BTA_STATE_NEXT?BTA_STATE_MAX:BTA_DB_MAX;}
bool bto_tegra_io_denials_linked(void){return true;}
FRESULT __real_f_open(FIL *,const TCHAR *,BYTE);
FRESULT __wrap_f_open(FIL *file,const TCHAR *path,BYTE mode){
    if(!path||!file)return FR_DENIED;
    int id=path_id(path);
    if(mode==FA_READ){
        bool allowed=id>=0;
        for(size_t i=0;i<sizeof(public_paths)/sizeof(*public_paths);i++)if(!strcmp(path,public_paths[i]))allowed=true;
        return allowed?__real_f_open(file,path,mode):FR_DENIED;
    }
    if(!session||!media_ok()||file!=&owned||owner!=-1||mode!=(FA_WRITE|FA_CREATE_NEW)||(id!=BTA_DB_NEXT&&id!=BTA_STATE_NEXT&&id!=BTA_TXN))return FR_DENIED;
    FRESULT result=__real_f_open(file,path,mode);
    if(result==FR_OK){
        if(file->obj.fs!=&sd_fs||file->obj.fs->pdrv!=DRIVE_SD){f_close(file);return FR_DENIED;}
        owner=id;
    }
    return result;
}
FRESULT __real_f_write(FIL *,const void *,UINT,UINT *);
FRESULT __wrap_f_write(FIL *file,const void *p,UINT n,UINT *written){
    if(written)*written=0;
    if(!p||!written||!session||!media_ok()||file!=&owned||owner<0||file->obj.fs!=&sd_fs||file->fptr>limit(owner)||n>limit(owner)-file->fptr)return FR_DENIED;
    return __real_f_write(file,p,n,written);
}
static bool aliases_main(const char *path){
    FIL target;FRESULT rc=f_open(&target,path,FA_READ);
    if(rc!=FR_OK)return true;
    DWORD cluster=target.obj.sclust;f_close(&target);bth_wipe(&target,sizeof(target));
    if(!cluster)return false;
    for(unsigned i=0;i<2;i++){
        FIL main;rc=f_open(&main,paths[i],FA_READ);
        if(rc==FR_NO_FILE||rc==FR_NO_PATH)continue;
        if(rc!=FR_OK)return true;
        bool alias=main.obj.sclust==cluster;f_close(&main);bth_wipe(&main,sizeof(main));if(alias)return true;
    }
    return false;
}
FRESULT __real_f_unlink(const TCHAR *);
FRESULT __wrap_f_unlink(const TCHAR *path){
    int id=path_id(path);
    if(!session||!media_ok()||id<BTA_DB_NEXT||id>=BTA_PATHS||aliases_main(path))return FR_DENIED;
    return __real_f_unlink(path);
}
FRESULT __real_f_rename(const TCHAR *,const TCHAR *);
FRESULT __wrap_f_rename(const TCHAR *from,const TCHAR *to){
    int a=path_id(from),b=path_id(to);
    bool allowed=(a==BTA_DB&&(b==BTA_DB_PREV||b==BTA_DB_NEXT))||(a==BTA_STATE&&(b==BTA_STATE_PREV||b==BTA_STATE_NEXT))
        ||((a==BTA_DB_NEXT||a==BTA_DB_PREV)&&b==BTA_DB)||((a==BTA_STATE_NEXT||a==BTA_STATE_PREV)&&b==BTA_STATE);
    if(!session||!media_ok()||!allowed)return FR_DENIED;
    return __real_f_rename(from,to);
}
int btx_transport_allowed(unsigned cmd,unsigned arg,int write,unsigned count,unsigned size){
    return session&&raw_armed&&media_ok()&&cmd==25&&write&&size==512&&count>0&&count<=128
        &&arg>=raw_first&&(uint64_t)arg+count<=(uint64_t)raw_first+raw_count;
}
int __real_sdmmc_storage_write(sdmmc_storage_t *,u32,u32,void *);
int __wrap_sdmmc_storage_write(sdmmc_storage_t *s,u32 sector,u32 count,void *buffer){
    if(s!=&sd_storage||s->sdmmc!=&sd_sdmmc||buffer!=raw_buffer||sector!=raw_first||count!=raw_count||!btx_transport_allowed(25,sector,1,count,512))return 0;
    return __real_sdmmc_storage_write(s,sector,count,buffer);
}
int bta_sd_disk_write(const void *p,unsigned sector,unsigned n){
    if(!session||!p||!n||n>128||!media_ok()||sector<BTA_SD_FIRST||(uint64_t)sector+n>(uint64_t)sd_fs.database+(uint64_t)(sd_fs.n_fatent-2)*sd_fs.csize)return RES_WRPRT;
    raw_first=sector;raw_count=n;raw_buffer=p;raw_armed=true;
    int result=sdmmc_storage_write(&sd_storage,sector,n,(void *)p);
    raw_armed=false;raw_first=raw_count=0;raw_buffer=NULL;return result?RES_OK:RES_ERROR;
}
int bta_sd_sync(void){return session&&media_ok()?RES_OK:RES_WRPRT;}
static int read_file(void *ctx,enum BtaPath id,uint8_t *out,size_t cap,size_t *n){
    (void)ctx;if(n)*n=0;if((unsigned)id>=BTA_PATHS||!out||!n)return -1;
    FIL file;FRESULT rc=f_open(&file,paths[id],FA_READ);if(rc==FR_NO_FILE||rc==FR_NO_PATH)return 0;if(rc!=FR_OK)return -1;
    size_t size=f_size(&file);UINT got=0;bool ok=size<=cap;
    if(ok)ok=f_read(&file,out,(UINT)size,&got)==FR_OK&&got==size;
    if(f_close(&file)!=FR_OK)ok=false;
    bth_wipe(&file,sizeof(file));if(!ok){bth_wipe(out,cap);return -1;}*n=size;return 1;
}
static bool write_file(void *ctx,enum BtaPath id,const uint8_t *p,size_t n){
    (void)ctx;if((id!=BTA_DB_NEXT&&id!=BTA_STATE_NEXT&&id!=BTA_TXN)||!p||!n||n>limit(id)||!media_ok())return false;
    session=true;UINT written=0;bool opened=false;FRESULT rc=f_open(&owned,paths[id],FA_WRITE|FA_CREATE_NEW);
    if(rc==FR_OK){opened=true;rc=f_write(&owned,p,(UINT)n,&written);}
    bool ok=rc==FR_OK&&written==n;
    if(opened){if(f_sync(&owned)!=FR_OK)ok=false;if(f_close(&owned)!=FR_OK)ok=false;}
    owner=-1;session=false;bth_wipe(&owned,sizeof(owned));
    if(ok){uint8_t *copy=malloc(n);size_t got=0;if(!copy)return false;ok=read_file(NULL,id,copy,n,&got)==1&&got==n&&!memcmp(p,copy,n);bth_wipe(copy,n);free(copy);}
    return ok;
}
static bool rename_file(void *ctx,enum BtaPath from,enum BtaPath to){
    (void)ctx;if((unsigned)from>=BTA_PATHS||(unsigned)to>=BTA_PATHS||!media_ok())return false;
    session=true;FRESULT result=f_rename(paths[from],paths[to]);session=false;return result==FR_OK;
}
static bool remove_file(void *ctx,enum BtaPath id){
    (void)ctx;if(id<BTA_DB_NEXT||id>=BTA_PATHS||!media_ok())return false;
    session=true;FRESULT result=f_unlink(paths[id]);session=false;return result==FR_OK;
}
const BtaIo *bta_platform_io(void){static const BtaIo io={NULL,read_file,write_file,rename_file,remove_file};return &io;}
static bool bound_public(const char *path,const uint8_t expected[32],size_t cap){
    FIL file;if(f_open(&file,path,FA_READ)!=FR_OK)return false;
    size_t n=f_size(&file);uint8_t *data=n&&n<=cap?malloc(n):NULL;UINT got=0;uint8_t h[32]={0};bool ok=data!=NULL;
    if(ok)ok=f_read(&file,data,(UINT)n,&got)==FR_OK&&got==n;
    if(ok){bsr_sha256(data,n,h);ok=!memcmp(h,expected,32);}
    f_close(&file);bth_wipe(&file,sizeof(file));if(data){bth_wipe(data,n);free(data);}bth_wipe(h,sizeof(h));return ok;
}
int bta_platform_guards(void){
    return media_ok()&&bound_public(public_paths[2],bta_config_hash,4096)&&bound_public(public_paths[3],bta_settings_hash,65536)
        &&bound_public(public_paths[4],bta_main_ini_hash,65536);
}

