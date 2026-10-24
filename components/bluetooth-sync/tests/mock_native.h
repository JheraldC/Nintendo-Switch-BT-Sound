/* SPDX-License-Identifier: GPL-2.0-only. Host-only test interface. */
#ifndef BTD_MOCK_NATIVE_H
#define BTD_MOCK_NATIVE_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
typedef uint8_t u8,BYTE;typedef uint32_t u32,DWORD,UINT;
typedef int DRESULT,DSTATUS,FRESULT;
typedef struct { unsigned id; } sdmmc_t;
#ifdef BTX_EXPORT_HOST_TEST
typedef struct { sdmmc_t *sdmmc;u32 sec_cnt;bool initialized,has_sector_access; } sdmmc_storage_t;
#else
typedef struct { sdmmc_t *sdmmc;u32 sec_cnt;bool initialized; } sdmmc_storage_t;
#endif
#ifdef BTX_EXPORT_HOST_TEST
typedef struct {u32 pdrv,fs_type,database,n_fatent,csize;} FATFS;
#else
typedef struct { u8 win[512]; } FATFS;
#endif
typedef struct { uint16_t err,loc;char *file; } ErrCode_t;
typedef struct { uint16_t cmd;u32 arg,rsp_type,check_busy; } sdmmc_cmd_t;
typedef struct { void *buf;u32 blksize,num_sectors;int is_write; } sdmmc_req_t;
enum { SDMMC_1=0,SDMMC_4=3,SDMMC_BUS_WIDTH_8=2,SDHCI_TIMING_MMC_HS200=3,
       DRIVE_SD=0,DRIVE_BIS=3,STA_NOINIT=1,FR_OK=0,RES_OK=0,RES_ERROR=1,RES_WRPRT=2,RES_PARERR=4,
       MMC_CONN_EMMC=1 };
extern sdmmc_storage_t sd_storage;
int sdmmc_storage_init_mmc(sdmmc_storage_t *,sdmmc_t *,u32,u32);
int sdmmc_storage_end(sdmmc_storage_t *);
int sdmmc_storage_set_mmc_partition(sdmmc_storage_t *,u32);
int sdmmc_storage_read(sdmmc_storage_t *,u32,u32,void *);
FRESULT f_mount(FATFS *,const char *,BYTE);
int se_aes_xts_crypt_sec(u32,u32,u32,uint64_t,void *,const void *,u32);
int connectMMC(u8);void disconnectMMC(void);ErrCode_t mountMMCPart(const char *);
DSTATUS disk_status(BYTE);DSTATUS disk_initialize(BYTE);
DRESULT disk_read(BYTE,BYTE *,DWORD,UINT);DRESULT disk_write(BYTE,const BYTE *,DWORD,UINT);
DRESULT disk_ioctl(BYTE,BYTE,void *);
#endif
