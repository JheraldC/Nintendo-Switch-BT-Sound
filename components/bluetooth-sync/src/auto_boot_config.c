#include "auto_boot_config.h"
#include "auto_binding.h"
#include "sha256.h"
#include "transfer.h"
#include <string.h>
bool bta_patch_boot_config(uint8_t *image,size_t n,bool hack){
    uint8_t digest[32]={0};
    if(!image||n!=BTA_HEKATE_BYTES||n<0x130||n>0x30000)return false;
    bsr_sha256(image,n,digest);bool ok=!memcmp(digest,bta_hekate_hash,32);bth_wipe(digest,sizeof(digest));if(!ok)return false;
    memset(image+0x94,0,0x84);
    image[0x94]=hack?5:1;image[0x95]=hack?1:0;image[0x96]=hack?1:0;
    if(hack)memcpy(image+0x98,"BTREAL",7);
    return true;
}
