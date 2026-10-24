/* SPDX-License-Identifier: GPL-2.0-only */
#include "tegra_keys.h"
#include "bt_only.h"
#ifdef BTO_DIAGNOSTIC
extern void btd_status(const char *,int);
#endif
extern bool bto_tegra_io_denials_linked(void);
static bool key_read(void *context,void *out,size_t count) {
    FIL *file=context;UINT actual=0;
    if(count>128)return false;
    return f_read(file,out,(UINT)count,&actual)==FR_OK&&actual==count;
}
BtoKeyResult bto_tegra_keys_load(BtoKeys *out) {
    if(!out)return BTO_KEYS_ARGUMENT;
    bto_wipe(out,sizeof(*out));
    if(!bto_tegra_io_denials_linked())return BTO_KEYS_IO;
    FIL file={0};
    if(f_open(&file,"sd:/switch/prod.keys",FA_READ)!=FR_OK) {
#ifdef BTO_DIAGNOSTIC
        btd_status("KEYS",BTO_KEYS_IO);
#endif
        return BTO_KEYS_IO;
    }
    FSIZE_t size=f_size(&file);BtoKeyResult result=BTO_KEYS_ARGUMENT;
    if(size&&size<=BTO_KEYS_FILE_MAX)result=bto_keys_parse(key_read,&file,(size_t)size,out);
    if(f_close(&file)!=FR_OK)result=BTO_KEYS_IO;
    bto_wipe(&file,sizeof(file));
    if(result!=BTO_KEYS_OK)bto_wipe(out,sizeof(*out));
#ifdef BTO_DIAGNOSTIC
    btd_status("KEYS",(int)result);
#endif
    return result;
}
