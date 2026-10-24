#ifndef BTA_MOCK_AUTO_NATIVE_H
#define BTA_MOCK_AUTO_NATIVE_H
#include <libs/fatfs/ff.h>
#include <libs/fatfs/diskio.h>
typedef struct {unsigned id;} sdmmc_t;
typedef struct {sdmmc_t *sdmmc;u32 sec_cnt;int has_sector_access;} sdmmc_storage_t;
typedef struct {unsigned cmd,arg;} sdmmc_cmd_t;
typedef struct {unsigned is_write,num_sectors,blksize;} sdmmc_req_t;
enum {SDMMC_1=0,SDMMC_4=3};
extern sdmmc_t sd_sdmmc;
extern sdmmc_storage_t sd_storage;
extern FATFS sd_fs;
int sd_get_card_removed(void);
int sdmmc_storage_write(sdmmc_storage_t *,u32,u32,void *);
#endif
