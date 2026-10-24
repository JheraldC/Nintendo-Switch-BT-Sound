/* SPDX-License-Identifier: GPL-2.0-only */
#include "diag_native.h"
#ifndef BTD_HOST_TEST
#include <storage/nx_sd.h>
#include <libs/fatfs/ff.h>
#include <libs/fatfs/diskio.h>
#include <sec/se.h>
#endif
#include <string.h>
#include "bt_only.h"
#include "nand_policy.h"
#include "sd_export.h"
#include "auto_platform.h"
/* No emuMMC driver, general GPT explorer, BIS write cache, or save writer. */
static sdmmc_t nand_controller;
static sdmmc_storage_t nand_storage;
static FATFS system_fs;
static BtdWindow window;
static bool connected,mounted;
static bool sd_app_pending;
static bool user_area_verified;
static uint32_t denied_nand_reads;
static uint32_t nand_requests,bis_requests,cache_hits,crypto_failures,read_failures,last_bad_sector;
static DRESULT read_system(uint8_t *,uint32_t,uint32_t);
static void show_geometry(const char *name,uint64_t value) {
    btd_number(name,(u32)value);
    if(value>>32)btd_number("HIGH_WORD",(u32)(value>>32));
}
static void explain_gpt(BtdGptResult result,const BtdGptInfo *info) {
    if(!info->problems)btd_status(btd_gpt_result_stage(result),(int)result);
    for(unsigned i=0;i<19;i++)if(info->problems&(UINT32_C(1)<<i)) {
        BtdGptResult error=(BtdGptResult)(BTD_GPT_ARGUMENT+i);btd_status(btd_gpt_result_stage(error),(int)error);
    }
    btd_status("GPT_SYSTEM",(int)result);
    if(!info->signature_valid)return;
    switch(result) {
        case BTD_GPT_HEADER_SIZE:btd_number("HEADER_BYTES",info->header_size);break;
        case BTD_GPT_PRIMARY_LBA:show_geometry("PRIMARY_LBA",info->primary_lba);break;
        case BTD_GPT_BACKUP_LBA:
            btd_number("MEDIA_SECTORS",info->capacity);show_geometry("BACKUP_LBA",info->backup_lba);break;
        case BTD_GPT_USABLE_RANGE:
            btd_number("MEDIA_SECTORS",info->capacity);show_geometry("FIRST_USABLE",info->first_usable);
            show_geometry("LAST_USABLE",info->last_usable);break;
        case BTD_GPT_TABLE_LBA:show_geometry("TABLE_LBA",info->table_lba);break;
        case BTD_GPT_ENTRY_COUNT:btd_number("ENTRY_COUNT",info->entries);break;
        case BTD_GPT_ENTRY_SIZE:btd_number("ENTRY_BYTES",info->entry_size);break;
        case BTD_GPT_SYSTEM_ALIGNMENT:show_geometry("SYSTEM_SECTORS",info->system_sectors);break;
        case BTD_GPT_PART_RANGE:case BTD_GPT_OVERLAP:case BTD_GPT_SYSTEM_INDEX:
        case BTD_GPT_DUPLICATE_SYSTEM:btd_number("ENTRY_INDEX",info->entry_index);break;
        default:break;
    }
}
static uint32_t cached_cluster=UINT32_MAX;
static _Alignas(16) uint8_t cluster[32*512];
static _Alignas(16) uint8_t gpt[BTD_GPT_BYTES];
int btd_validate_ext_csd(const void *data) {
    user_area_verified=data&&((((const uint8_t *)data)[179]&7)==0);
    return user_area_verified?1:0;
}
int btd_transport_guard(const sdmmc_t *ctl,const sdmmc_cmd_t *cmd,const sdmmc_req_t *req) {
    if(!ctl||!cmd)return 0;
    if(ctl->id!=SDMMC_1&&ctl->id!=SDMMC_4)return 0;
    bool nand_read=ctl->id==SDMMC_4&&(cmd->cmd==17||cmd->cmd==18);
    if(nand_read)nand_requests++;
    if(nand_read&&!user_area_verified){denied_nand_reads++;return 0;}
    if(ctl->id==SDMMC_1) {
        /* SD ACMD42 disconnects the DAT3 pull-up and is required by this driver.
           It is distinct from plain CMD42 LOCK_UNLOCK, which stays denied. */
        if((req&&req->is_write)||cmd->cmd==24||cmd->cmd==25)
            return btx_transport_allowed(cmd->cmd,cmd->arg,req&&req->is_write,req?req->num_sectors:0,req?req->blksize:0);
        bool card_detect=sd_app_pending&&cmd->cmd==42&&cmd->arg==0&&req==NULL;
        sd_app_pending=cmd->cmd==55&&req==NULL;
        if(card_detect)return 1;
    }
    bool allowed=btd_command_allowed(ctl->id==SDMMC_4,cmd->cmd,cmd->arg,req!=NULL,
        req&&req->is_write,req?req->num_sectors:0,req?req->blksize:0,&window);
    if(nand_read&&!allowed)denied_nand_reads++;
    return allowed?1:0;
}
int btd_nand_connected(void) { return connected?1:0; }
void disconnectMMC(void) {
    if(mounted)f_mount(NULL,"bis:",0);
    mounted=false;
    if(connected)sdmmc_storage_end(&nand_storage);
    connected=false;user_area_verified=false;window=(BtdWindow){0};cached_cluster=UINT32_MAX;
    bto_wipe(cluster,sizeof(cluster));bto_wipe(gpt,sizeof(gpt));
    bto_wipe(&system_fs,sizeof(system_fs));bto_wipe(&nand_storage,sizeof(nand_storage));
}
int connectMMC(u8 type) {
    if(type!=MMC_CONN_EMMC)return 1;
    disconnectMMC();denied_nand_reads=0;nand_requests=0;bis_requests=0;cache_hits=0;
    crypto_failures=0;read_failures=0;last_bad_sector=0;btd_status("NAND_INIT",-1);
    if(!sdmmc_storage_init_mmc(&nand_storage,&nand_controller,SDMMC_BUS_WIDTH_8,SDHCI_TIMING_MMC_HS200)||!user_area_verified) {
        sdmmc_storage_end(&nand_storage);btd_status("NAND_INIT",1);return 1;
    }
    connected=true;
    btd_status("NAND_INIT",0);
    if(!sdmmc_storage_read(&nand_storage,BTD_GPT_SECTOR,BTD_GPT_SECTORS,gpt)) {
        btd_status("GPT_READ",1);btd_number("MMC_READ_DENIED",denied_nand_reads);
        disconnectMMC();return 1;
    }
    btd_status("GPT_READ",0);
    BtdGptInfo info={0};
    BtdGptResult result=btd_check_gpt(gpt,sizeof(gpt),nand_storage.sec_cnt,&window,&info);
    if(info.signature_valid) {
        btd_number("GPT_ENTRIES",info.entries);btd_number("GPT_ENTRY_BYTES",info.entry_size);
        btd_number("GPT_USED",info.used_entries);
        if(info.header_crc_checked)btd_status("GPT_HEADER_CRC",info.header_crc_ok?0:112);else btd_skip("GPT_HEADER_CRC");
        if(info.table_crc_checked)btd_status("GPT_TABLE_CRC",info.table_crc_ok?0:113);else btd_skip("GPT_TABLE_CRC");
        if(info.system_index!=UINT32_MAX)btd_number("SYSTEM_INDEX",info.system_index);
        if(info.system_sectors)show_geometry("SYSTEM_SECTORS",info.system_sectors);
    }
    if(result!=BTD_GPT_OK){explain_gpt(result,&info);disconnectMMC();return 1;}
    bto_wipe(gpt,sizeof(gpt));btd_status("GPT_SYSTEM",0);return 0;
}
ErrCode_t mountMMCPart(const char *name) {
    if(!connected||!window.ready||!name||strcmp(name,"SYSTEM"))return (ErrCode_t){.err=1};
    uint8_t boot[512]={0};DRESULT boot_result=read_system(boot,0,1);
    btd_status("FAT_BOOT_READ",(int)boot_result);
    if(boot_result==RES_OK) {
        bool signature=boot[510]==0x55&&boot[511]==0xAA;
        btd_status("FAT_BOOT_SIG",signature?0:13);
        if(signature) {
            btd_number("FAT_BYTES_SECTOR",boot[11]|((u32)boot[12]<<8));
            btd_number("FAT_SECTORS_CLUSTER",boot[13]);btd_number("FAT_COPIES",boot[16]);
        }
    }
    bto_wipe(boot,sizeof(boot));
    FRESULT result=f_mount(&system_fs,"bis:",1);
    btd_status("SYSTEM_FAT",(int)result);
    if(result!=FR_OK){f_mount(NULL,"bis:",0);return (ErrCode_t){.err=(u32)result};}
    mounted=true;return (ErrCode_t){.err=0};
}
static DRESULT read_system(uint8_t *out,uint32_t sector,uint32_t count) {
    uint64_t length=(uint64_t)window.end-window.start+1;
    if(!connected||!window.ready||!out||!count||count>UINT32_MAX/512||sector>=length||count>length-sector)return RES_PARERR;
    size_t total=(size_t)count*512;uint8_t *original=out;
    bis_requests++;
    while(count) {
        uint32_t index=sector/32,offset=sector%32,take=32-offset;if(take>count)take=count;
        if(cached_cluster!=index) {
            cached_cluster=UINT32_MAX;bto_wipe(cluster,sizeof(cluster));
            bool io=sdmmc_storage_read(&nand_storage,window.start+index*32,32,cluster)!=0;
            bool crypto=io&&se_aes_xts_crypt_sec(5,4,0,index,cluster,cluster,sizeof(cluster));
            if(!io||!crypto) {
                if(!io)read_failures++;else crypto_failures++;last_bad_sector=sector;
                bto_wipe(cluster,sizeof(cluster));bto_wipe(original,total);return RES_ERROR;
            }
            cached_cluster=index;
        } else cache_hits++;
        memcpy(out,cluster+offset*512,take*512);out+=take*512;sector+=take;count-=take;
    }
    return RES_OK;
}
void btd_storage_summary(void) {
    btd_number("MMC_REQUESTS",nand_requests);btd_number("MMC_DENIED",denied_nand_reads);
    btd_number("BIS_CALLS",bis_requests);btd_number("CACHE_HITS",cache_hits);
    btd_number("BIS_IO_FAILURES",read_failures);btd_number("AES_XTS_FAILURES",crypto_failures);
    if(read_failures||crypto_failures)btd_number("BAD_SYSTEM_SECTOR",last_bad_sector);
}
DSTATUS disk_status(BYTE drive) { return drive==DRIVE_SD||drive==DRIVE_BIS?0:STA_NOINIT; }
DSTATUS disk_initialize(BYTE drive) { return disk_status(drive); }
DRESULT disk_read(BYTE drive,BYTE *out,DWORD sector,UINT count) {
    if(!out||!count)return RES_PARERR;
    if(drive==DRIVE_SD)return sdmmc_storage_read(&sd_storage,sector,count,out)?RES_OK:RES_ERROR;
    if(drive==DRIVE_BIS)return read_system(out,sector,count);
    return RES_PARERR;
}
DRESULT disk_ioctl(BYTE drive,BYTE command,void *out) {
    (void)out;if(drive==DRIVE_SD&&command==0)return (DRESULT)bta_sd_sync();return RES_PARERR;
}
/* Defense at disk layer even though read-only FatFs never imports this symbol. */
DRESULT disk_write(BYTE d,const BYTE *p,DWORD sector,UINT n) {
    if(d!=DRIVE_SD)return RES_WRPRT;
    return (DRESULT)bta_sd_disk_write(p,sector,n);
}
