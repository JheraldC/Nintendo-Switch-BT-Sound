#ifndef MOCK_EXPORT_H
#define MOCK_EXPORT_H
#include "mock_native.h"
typedef struct {FATFS *fs;size_t size;} MockObj;
typedef struct {MockObj obj;size_t fptr;u32 sect,clust;unsigned kind;} FIL;
enum {FA_READ=1,FS_FAT32=3,FR_DENIED=7};
#define f_size(f) ((f)->obj.size)
FRESULT f_open(FIL *,const char *,BYTE);
FRESULT f_read(FIL *,void *,UINT,UINT *);
FRESULT f_close(FIL *);
int sdmmc_storage_write(sdmmc_storage_t *,u32,u32,void *);
extern FATFS sd_fs;
#endif
