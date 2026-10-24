/* SPDX-License-Identifier: GPL-2.0-only
 * Synthetic host-only API, excluded from the ARM build. */
#ifndef BTO_MOCK_TEGRA_H
#define BTO_MOCK_TEGRA_H
#include <stdint.h>
#include <stddef.h>
typedef uint8_t BYTE;typedef unsigned UINT;typedef uint32_t u32;
typedef uint64_t FSIZE_t;typedef char TCHAR;typedef int FRESULT;
typedef struct { FSIZE_t size,position; unsigned kind; } FIL;
typedef struct { unsigned unused; } sdmmc_storage_t;
typedef struct { int err; } ErrCode_t;
enum { FR_OK=0,FR_DENIED=7,FR_WRITE_PROTECTED=10,FA_READ=1,FA_WRITE=2,RES_WRPRT=2,MMC_CONN_EMMC=1 };
#define f_size(f) ((f)->size)
FRESULT f_open(FIL *,const TCHAR *,BYTE);
FRESULT f_lseek(FIL *,FSIZE_t);
FRESULT f_read(FIL *,void *,UINT,UINT *);
FRESULT f_close(FIL *);
FRESULT f_write(FIL *,const void *,UINT,UINT *);
FRESULT f_unlink(const TCHAR *);FRESULT f_mkdir(const TCHAR *);
FRESULT f_rename(const TCHAR *,const TCHAR *);
int emummc_storage_write(sdmmc_storage_t *,u32,u32,void *);
int sdmmc_storage_write(sdmmc_storage_t *,u32,u32,void *);
int nx_emmc_bis_write(u32,u32,void *);
int emummc_storage_set_mmc_partition(sdmmc_storage_t *,u32);
int connectMMC(uint8_t);ErrCode_t mountMMCPart(const char *);
void disconnectMMC(void);
int se_aes_cmac(u32,void *,u32,const void *,u32);
void se_aes_key_set(u32,const void *,u32);void se_aes_key_clear(u32);
#endif
