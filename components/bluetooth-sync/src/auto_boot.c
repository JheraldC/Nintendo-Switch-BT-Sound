/* SPDX-License-Identifier: GPL-2.0-only. Bound Hekate continuation, not a
 * general payload launcher. Boot configuration ABI is documented by Hekate. */
#include "diag_native.h"
#include <storage/nx_sd.h>
#include <libs/fatfs/ff.h>
#include <soc/t210.h>
#include <soc/hw_init.h>
#include "auto_platform.h"
#include "auto_binding.h"
#include "auto_boot_config.h"
#include "sha256.h"
#include <string.h>
static uint8_t *const image=(uint8_t *)0xC0000000;
static bool prepared;
int bta_prepare_boot(void){
    FIL file;UINT got=0;uint8_t h[32]={0};prepared=false;
    if(BTA_HEKATE_BYTES<0x130||BTA_HEKATE_BYTES>0x30000)return 0;
    if(f_open(&file,"sd:/switch/bt-autosync-v1/hekate-continue.bin",FA_READ)!=FR_OK)return 0;
    bool ok=f_size(&file)==BTA_HEKATE_BYTES;
    if(ok)ok=f_read(&file,image,BTA_HEKATE_BYTES,&got)==FR_OK&&got==BTA_HEKATE_BYTES;
    f_close(&file);bth_wipe(&file,sizeof(file));
    if(ok){bsr_sha256(image,BTA_HEKATE_BYTES,h);ok=!memcmp(h,bta_hekate_hash,32);}
    bth_wipe(h,sizeof(h));prepared=ok;return ok?1:0;
}
int bta_continue_boot(int hack){
    if(!prepared||btd_nand_connected())return -1;
    if(!bta_patch_boot_config(image,BTA_HEKATE_BYTES,hack!=0))return -1;
    sd_unmount();
    /* Same RAM-only flag used by Hekate's own update continuation. */
    EMC(EMC_SCRATCH0)|=EMC_HEKA_UPD;
    hw_reinit_workaround(false,0);
    sdmmc_storage_init_wait_sd();
    void (*entry)(void)=(void (*)(void))image;entry();return -1;
}
