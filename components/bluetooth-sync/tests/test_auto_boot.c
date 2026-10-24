#include "auto_boot_config.h"
#include "auto_binding.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
static unsigned checks;
#define CHECK(e) do{checks++;assert(e);}while(0)
int main(void){
    uint8_t *original=malloc(BTA_HEKATE_BYTES),*copy=malloc(BTA_HEKATE_BYTES);CHECK(original&&copy);
    FILE *file=fopen("output/hekate-continue.bin","rb");CHECK(file!=NULL);CHECK(fread(original,1,BTA_HEKATE_BYTES,file)==BTA_HEKATE_BYTES);CHECK(!fclose(file));
    for(unsigned hack=0;hack<2;hack++){
        memcpy(copy,original,BTA_HEKATE_BYTES);CHECK(bta_patch_boot_config(copy,BTA_HEKATE_BYTES,hack!=0));
        CHECK(!memcmp(copy,original,0x94));CHECK(!memcmp(copy+0x118,original+0x118,BTA_HEKATE_BYTES-0x118));
        CHECK(copy[0x94]==(hack?5:1)&&copy[0x95]==hack&&copy[0x96]==hack&&copy[0x97]==0);
        for(size_t i=0x98;i<0x118;i++)CHECK(copy[i]==(hack&&i<0x98+6?(uint8_t)"BTREAL"[i-0x98]:0));
    }
    memcpy(copy,original,BTA_HEKATE_BYTES);CHECK(!bta_patch_boot_config(copy,BTA_HEKATE_BYTES-1,true));CHECK(!memcmp(copy,original,BTA_HEKATE_BYTES));
    CHECK(!bta_patch_boot_config(NULL,BTA_HEKATE_BYTES,true));
    for(size_t i=0;i<BTA_HEKATE_BYTES;i+=997){
        memcpy(copy,original,BTA_HEKATE_BYTES);copy[i]^=1;CHECK(!bta_patch_boot_config(copy,BTA_HEKATE_BYTES,true));CHECK(copy[i]==(uint8_t)(original[i]^1));
    }
    free(copy);free(original);printf("Hekate continuation: %u checks passed; pinned binary hash, fixed boot ID/menu config, unchanged code and invalid-image rejection\n",checks);return 0;
}
