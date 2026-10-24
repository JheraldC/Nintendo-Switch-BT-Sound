/* SPDX-License-Identifier: GPL-2.0-only */
/* Research integration: NAND is read-only; SD export uses a separate bounded data-sector gate.
 * Linker wrapping is required; an archive alone is not proof of enforcement. */
#include "tegra_platform.h"
#include <stdbool.h>
#include <string.h>
bool bto_tegra_io_denials_linked(void) { return true; }
FRESULT __real_f_open(FIL *file,const TCHAR *path,BYTE mode);
FRESULT __wrap_f_open(FIL *file,const TCHAR *path,BYTE mode) {
    if(!path||mode!=FA_READ)return FR_DENIED;
    if(strcmp(path,"bis:/save/8000000000000050")!=0&&
       strcmp(path,"sd:/switch/prod.keys")!=0&&
       strcmp(path,"sd:/emuMMC/emummc.ini")!=0&&
       strcmp(path,"sd:/switch/bt-transfer-v1/hack-pairings.bth")!=0&&
       strcmp(path,"sd:/switch/bt-origin-export-v1/source-pairings.bts")!=0)return FR_DENIED;
    return __real_f_open(file,path,mode);
}
FRESULT __wrap_f_write(FIL *file,const void *buffer,UINT size,UINT *written) {
    (void)file;(void)buffer;(void)size;if(written)*written=0;return FR_WRITE_PROTECTED;
}
FRESULT __wrap_f_unlink(const TCHAR *path) { (void)path;return FR_WRITE_PROTECTED; }
FRESULT __wrap_f_mkdir(const TCHAR *path) { (void)path;return FR_WRITE_PROTECTED; }
FRESULT __wrap_f_rename(const TCHAR *a,const TCHAR *b) { (void)a;(void)b;return FR_WRITE_PROTECTED; }
int __wrap_emummc_storage_write(sdmmc_storage_t *s,u32 sector,u32 count,void *buffer) {
    (void)s;(void)sector;(void)count;(void)buffer;return 0;
}
int __wrap_nx_emmc_bis_write(u32 sector,u32 count,void *buffer) {
    (void)sector;(void)count;(void)buffer;return RES_WRPRT;
}
int __real_emummc_storage_set_mmc_partition(sdmmc_storage_t *s,u32 partition);
int __wrap_emummc_storage_set_mmc_partition(sdmmc_storage_t *s,u32 partition) {
    /* Hardware selection only: reject BOOT0 and BOOT1 before their data is read. */
    if(partition!=0)return 0;
    return __real_emummc_storage_set_mmc_partition(s,partition);
}
